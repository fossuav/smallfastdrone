#!/usr/bin/env python3
"""Auto-resolve the 'additive' diff3 conflicts left by rebuild_testfile.sh.

Autotest reconstruction merges each PR's net change to a test file onto a vanilla
4.7 base. Almost every conflict is additive - a new test method or a new entry in
a tests1* registration list - and differs from 4.7 only because master carries
extra tests 4.7 does not. Two shapes are safe to resolve mechanically:

    cur EMPTY, theirs == base + appended-new   -> keep the appended-new only
        (drop the master-only base entries 4.7 does not carry, keep our addition)

    base EMPTY (no common ancestor at this anchor) -> union cur + theirs
        (both sides added here independently; keep both additions)

Both use literal comparisons - no fuzzy matching - so a mid-insertion or any
conflict where cur AND base both have content (a genuine 3-way edit of existing
lines, or a diff3 mis-alignment that splits a method across markers) is LEFT in
place with generic <<< markers for a human to resolve. See REFRESH_NOTES.md
"Phase 2" for the manual recipe for the split-method case.

Usage: resolve_additive.py <file>   (edits in place; prints what it did)
Exit 0 always; check the printed "left N" and remaining markers to drive a loop.
"""
import sys


def parse(lines):
    out, i, n = [], 0, len(lines)
    while i < n:
        if lines[i].startswith('<<<<<<< '):
            i += 1
            cur = []
            while i < n and not lines[i].startswith('||||||| '):
                cur.append(lines[i]); i += 1
            if i >= n:
                raise SystemExit("unterminated conflict (no ||||||| base marker)")
            i += 1
            base = []
            while i < n and lines[i] != '=======':
                base.append(lines[i]); i += 1
            i += 1
            theirs = []
            while i < n and not lines[i].startswith('>>>>>>> '):
                theirs.append(lines[i]); i += 1
            i += 1
            out.append(('C', cur, base, theirs))
        else:
            out.append(('L', lines[i])); i += 1
    return out


def main():
    path = sys.argv[1]
    lines = open(path).read().split('\n')
    res, resolved, left = [], 0, 0
    for p in parse(lines):
        if p[0] == 'L':
            res.append(p[1]); continue
        _, cur, base, theirs = p
        cur_empty = all(s.strip() == '' for s in cur)
        base_empty = all(s.strip() == '' for s in base)
        prefix_ok = theirs[:len(base)] == base
        if base_empty:
            res.extend(cur); res.extend(theirs); resolved += 1
        elif cur_empty and prefix_ok:
            res.extend(theirs[len(base):]); resolved += 1
        else:
            res.append('<<<<<<< cur'); res.extend(cur)
            res.append('|||||||'); res.extend(base)
            res.append('======='); res.extend(theirs)
            res.append('>>>>>>> pr')
            left += 1
            print("  LEFT (manual): cur_empty=%s base_empty=%s near %r"
                  % (cur_empty, base_empty, (cur or theirs)[:1]))
    open(path, 'w').write('\n'.join(res))
    print("  auto-resolved %d, left %d" % (resolved, left))


if __name__ == "__main__":
    main()
