#!/usr/bin/env python3
"""Repair test methods that the hot-file rebuild left syntactically broken.

`rebuild-tests` merges each PR's own hunks into a file rebuilt from the 4.7
fork point. Where two PR heads carry different versions of the same method the
merge can interleave them: a truncated body, a duplicated block, a def with no
body. The file then does not compile and no other gate can run.

Every method it mangles belongs to a PR whose test the previous branch already
carried, so the previous branch is the reference. This replaces each broken
method with that branch's copy, repeating while the file fails to compile, and
prints what it touched so the tests of PRs whose heads moved this refresh can
be re-checked by hand (`check_pr_test_lines.py`).

Usage (from the refresh worktree):
    Tools/SFD/repair_test_methods.py Tools/autotest/arducopter.py SmallFastDrone-4.7.1-beta
"""
import re, subprocess, sys

path, ref = sys.argv[1], sys.argv[2]
ref_src = subprocess.run(['git', 'show', '%s:%s' % (ref, path)], capture_output=True, text=True).stdout
fixed = []
for _ in range(40):
    src = open(path).read()
    try:
        compile(src, path, 'exec')
        break
    except SyntaxError as e:
        lines = src.split('\n')
        # the method containing the reported line, or the one before the stray token
        anchor = None
        skip = fixed.count(fixed[-1]) if fixed else 0
        seen = 0
        for n in range(min(e.lineno, len(lines)), 0, -1):
            m = re.match(r'    def ([A-Za-z_0-9]+)\(', lines[n-1])
            if m:
                if seen < skip and fixed and m.group(1) == fixed[-1]:
                    seen += 1
                    continue
                anchor = m.group(1)
                break
        if anchor is None:
            sys.exit('no enclosing method for line %s' % e.lineno)
        a = ref_src.find('    def %s(' % anchor)
        if a < 0:
            sys.exit('%s not in %s (line %s: %s)' % (anchor, ref, e.lineno, e.msg))
        b = ref_src.index('\n    def ', a + 10) + 1
        x = src.index('    def %s(' % anchor)
        nxt = re.search(r'\n    def [A-Za-z_0-9]+\(', src[x+10:])
        y = x + 10 + nxt.start() + 1
        open(path, 'w').write(src[:x] + ref_src[a:b] + src[y:])
        fixed.append(anchor)
print('repaired: %s' % ', '.join(fixed) if fixed else 'nothing to repair')
compile(open(path).read(), path, 'exec')
print('file compiles')
