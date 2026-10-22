#!/usr/bin/env python3
"""Compare every test method a PR owns against that PR's own head.

`check_pr_test_lines.py` counts how many of a PR's test lines are present, which
a rebuild that DUPLICATES content still passes: refresh7 shipped a
BaroDriftClearedAtArm with an extra `assert_reported_amsl_matches_gps()` in it,
asserting the reported height against a GPS the subtest had just killed, and the
file compiled and every other gate passed.

This works the other way round: for each PR, take the methods its head changes
against its own merge-base with master, and compare the branch's copy of each
with the head's. Differences are printed, not judged - several are legitimate:

  - registration lists (tests1c, tests2b, disabled_tests ...) merge every PR;
  - the documented 4.7 adaptations (takeoff() keywords, HeightDatumKept-
    OnMidairRearm's three, the Clamp bound);
  - a method two stacked PRs both change, where the branch holds both.

What it is for is the fourth kind: a method that is LARGER than both the PR
head's copy and the previous branch's, which is a rebuild that merged a version
into itself. Check any such method against the previous branch too.

Usage (from the refresh worktree):
    Tools/SFD/check_pr_methods.py [--file Tools/autotest/arducopter.py] [PR ...]
"""

import re
import subprocess
import sys


def show(ref, path):
    r = subprocess.run(['git', 'show', '%s:%s' % (ref, path)], capture_output=True, text=True)
    return r.stdout if r.returncode == 0 else ''


def methods(src):
    out = {}
    for m in re.finditer(r'\n    def ([A-Za-z_0-9]+)\(', src):
        i = m.start() + 1
        nxt = re.search(r'\n    def [A-Za-z_0-9]+\(', src[i + 10:])
        out[m.group(1)] = src[i:i + 10 + nxt.start() + 1] if nxt else src[i:]
    return out


def prs_from_manifest():
    out = []
    for line in open('Tools/SFD/prs.txt'):
        line = line.split('#', 1)[0].strip() if not line.lstrip().startswith('#') else ''
        if line:
            out.append(line.split()[0])
    return out


def main():
    args = sys.argv[1:]
    path = 'Tools/autotest/arducopter.py'
    if args and args[0] == '--file':
        path = args[1]
        args = args[2:]
    prs = args or prs_from_manifest()
    cur = methods(open(path).read())
    problems = 0
    for pr in prs:
        head_src = show('refs/sfdpr/%s' % pr, path)
        if not head_src:
            continue
        base = subprocess.run(['git', 'merge-base', 'refs/sfdpr/%s' % pr, 'upstream/master'],
                              capture_output=True, text=True).stdout.strip()
        old = methods(show(base, path))
        head = methods(head_src)
        for name, body in head.items():
            if old.get(name) == body:
                continue  # not this PR's work
            if name not in cur:
                print('#%-6s %-45s MISSING from the branch' % (pr, name))
                problems += 1
            elif cur[name] != body:
                flag = ''
                if len(cur[name]) > len(body):
                    flag = '  (branch copy is %d chars LARGER - check for duplicated content)' % (
                        len(cur[name]) - len(body))
                print('#%-6s %-45s differs from the PR head%s' % (pr, name, flag))
                problems += 1
    print('%s: %d method(s) differing from their PR head' % (path, problems))


if __name__ == '__main__':
    main()
