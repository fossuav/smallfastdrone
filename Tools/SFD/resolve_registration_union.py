#!/usr/bin/env python3
"""Resolve rebuild-tests registration-list conflicts by union.

`rebuild_testfile.sh` leaves diff3 conflicts in the vehicle test files when
two PRs add different entries to the same `tests()` registration list. The
documented resolution (REFRESH_NOTES, "Manual cases to expect in
rebuild-tests") is to union the entries and drop any whose `def` does not
exist in the file - master-only tests 4.7 does not carry, and tests that
belong to a merged PR and therefore vanish from the rebuild.

This only touches conflicts where EVERY line on all three sides is a bare
`self.Something,` registration. Anything else - a split method, a real code
conflict - is left alone and reported, because those need the recipes in
REFRESH_NOTES rather than a union.

Usage:
    resolve_registration_union.py [--apply] <file> [file ...]

Default is a dry run. Exit 1 if any conflict was left unresolved.
"""
import argparse
import re
import sys

ENTRY = re.compile(r"^\s*self\.(\w+),\s*$")

def defs_text(path):
    """All text a registered test's def could live in.

    A vehicle file's tests() registers methods defined in the vehicle class
    AND methods inherited from TestSuite in vehicle_test_suite.py (Button,
    CRSF, Gripper, the Fence* family, ...). Searching only the vehicle file
    reports every inherited test as dangling, which would delete 35 valid
    registrations from arducopter.py.
    """
    import os
    text = io.open(path, encoding="utf-8").read()
    suite = os.path.join(os.path.dirname(path) or ".", "vehicle_test_suite.py")
    if os.path.abspath(suite) != os.path.abspath(path) and os.path.exists(suite):
        text += "\n" + io.open(suite, encoding="utf-8").read()
    return text



def split_conflicts(lines):
    """Yield (start, mid, sep, end) indices for each diff3 conflict."""
    i = 0
    while i < len(lines):
        if lines[i].startswith("<<<<<<<"):
            start = i
            mid = sep = None
            for j in range(i + 1, len(lines)):
                if lines[j].startswith("|||||||") and mid is None:
                    mid = j
                elif lines[j].startswith("=======") and sep is None:
                    sep = j
                elif lines[j].startswith(">>>>>>>"):
                    yield (start, mid, sep, j)
                    i = j
                    break
            else:
                return
        i += 1


def resolve(path, apply_changes):
    with open(path) as f:
        lines = f.read().split("\n")
    base_text = defs_text(path)
    conflicts = list(split_conflicts(lines))
    if not conflicts:
        print("%s: no conflicts" % path)
        return 0

    text = base_text + "\n" + "\n".join(lines)
    out, last, left, done = [], 0, 0, 0
    for start, mid, sep, end in conflicts:
        cur = lines[start + 1:mid if mid is not None else sep]
        pr = lines[sep + 1:end]
        base = lines[mid + 1:sep] if mid is not None else []
        sides = [l for l in cur + base + pr if l.strip()]
        if not sides or not all(ENTRY.match(l) for l in sides):
            print("  SKIP conflict at line %d - not a pure registration list" % (start + 1))
            left += 1
            continue
        names, seen = [], set()
        for l in cur + base + pr:
            m = ENTRY.match(l)
            if m and m.group(1) not in seen:
                seen.add(m.group(1))
                names.append(m.group(1))
        kept = [n for n in names if re.search(r"\n    def %s\(" % re.escape(n), text)]
        dropped = [n for n in names if n not in kept]
        indent = ENTRY.match(sides[0]).group(0)[:len(sides[0]) - len(sides[0].lstrip())]
        out += lines[last:start]
        out += ["%sself.%s," % (indent, n) for n in kept]
        last = end + 1
        done += 1
        if dropped:
            print("  line %d: kept %d, dropped (no def): %s"
                  % (start + 1, len(kept), ", ".join(dropped)))
        else:
            print("  line %d: kept %d" % (start + 1, len(kept)))
    out += lines[last:]

    if apply_changes and done:
        with open(path, "w") as f:
            f.write("\n".join(out))
    print("%s: %d resolved, %d left%s"
          % (path, done, left, "" if apply_changes else " (dry run)"))
    return 1 if left else 0


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--apply", action="store_true")
    ap.add_argument("files", nargs="+")
    a = ap.parse_args()
    return max(resolve(p, a.apply) for p in a.files)


if __name__ == "__main__":
    sys.exit(main())
