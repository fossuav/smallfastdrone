#!/usr/bin/env python3
"""Resolve one rebuild-tests stop in a hot vehicle test file.

Automates the three recipes in REFRESH_NOTES "Manual cases to expect in
rebuild-tests", which between them cover every stop seen so far:

1. A registration list where each side adds different entries -> union them.
2. Anything else -> resolve to OUR side. This is the diff3 mis-alignment
   case: a PR adds a method where the branch already has one with similar
   shape, and the merge splits the branch's method, leaving its def and
   docstring inside the markers and its body in the common region below.
   Taking our side reconnects the method to its body.
3. Recipe 2 can drop the PR's own new method, which then shows up as a
   registration with no def. For each of those, take the method verbatim
   from the PR head and insert it. If the PR head has no such def either,
   the entry is master-only (or belongs to a merged PR whose tests vanish
   from the rebuild) and the registration is dropped.

Usage:
    resolve_hotfile.py --pr N [--apply] <file>

Default is a dry run. Always finish a rebuild with py_compile, a
duplicate-def scan and a suite load - this script does not replace those.
"""
import argparse
import io
import re
import subprocess
import sys

ENTRY = re.compile(r"^(\s*)self\.(\w+),\s*$")

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



def git(*args):
    return subprocess.run(["git", *args], capture_output=True, text=True,
                          check=True).stdout


def parse_conflicts(lines):
    out, i = [], 0
    while i < len(lines):
        if lines[i].startswith("<<<<<<<"):
            mid = sep = end = None
            for j in range(i + 1, len(lines)):
                if lines[j].startswith("|||||||") and mid is None:
                    mid = j
                elif lines[j].startswith("=======") and sep is None:
                    sep = j
                elif lines[j].startswith(">>>>>>>"):
                    end = j
                    break
            if end is None:
                break
            out.append((i, mid, sep, end))
            i = end + 1
        else:
            i += 1
    return out


def resolve_conflicts(lines, a_file, pr_defs):
    """Apply recipes 1 and 2. Returns (new_lines, n_union, n_ours)."""
    text = defs_text(a_file) + "\n" + "\n".join(lines)
    conflicts = parse_conflicts(lines)
    out, last, n_union, n_ours, n_theirs = [], 0, 0, 0, 0
    pr_spans = []
    for start, mid, sep, end in conflicts:
        cur = lines[start + 1:mid if mid is not None else sep]
        base = lines[mid + 1:sep] if mid is not None else []
        pr = lines[sep + 1:end]
        sides = [l for l in cur + base + pr if l.strip()]
        out += lines[last:start]
        if sides and all(ENTRY.match(l) for l in sides):
            names, seen = [], set()
            for l in cur + base + pr:
                m = ENTRY.match(l)
                if m and m.group(2) not in seen:
                    seen.add(m.group(2))
                    names.append((m.group(1), m.group(2)))
            # keep an entry whose def exists here, and also one the PR head
            # defines: fix_dangling() inserts those. Dropping them here would
            # silently discard the PR's own new tests.
            kept = [(ind, n) for ind, n in names
                    if re.search(r"\n    def %s\(" % re.escape(n), text)
                    or n in pr_defs]
            out += ["%sself.%s," % (ind, n) for ind, n in kept]
            n_union += 1
        elif not [l for l in cur if l.strip()] and [l for l in pr if l.strip()]:
            # shape 1: our side is empty and the PR side is a superset of base,
            # so taking ours would delete the method outright. Take the PR side
            # and let the duplicate-def report below catch an orphaned copy.
            pr_spans.append((len(out), len(out) + len(pr)))
            out += pr
            n_theirs += 1
        else:
            out += cur
            n_ours += 1
        last = end + 1
    out += lines[last:]
    return out, n_union, n_ours, n_theirs, pr_spans


def in_tests_block(lines):
    """Index set of lines that sit inside a `def tests(` body.

    Outside one, `self.something,` is ordinary code - a tuple element, an
    argument - and must never be treated as a test registration.
    """
    inside, out, indent = False, set(), 0
    for i, l in enumerate(lines):
        if re.match(r"    def tests\(", l):
            inside, indent = True, len(l) - len(l.lstrip())
            continue
        if inside:
            if l.strip() and (len(l) - len(l.lstrip())) <= indent and not l.lstrip().startswith("#"):
                inside = False
            else:
                out.add(i)
    return out


def fix_dangling(lines, pr, a_file):
    """Apply recipe 3. Returns (new_lines, inserted, dropped)."""
    text = defs_text(a_file) + "\n" + "\n".join(lines)
    tb = in_tests_block(lines)
    registered = {m.group(2) for i, m in
                  ((i, ENTRY.match(l)) for i, l in enumerate(lines)) if m and i in tb}
    dangling = [n for n in sorted(registered)
                if not re.search(r"\n    def %s\(" % re.escape(n), text)]
    if not dangling:
        return lines, [], []

    try:
        src = git("show", "refs/sfdpr/%s:Tools/autotest/%s" % (pr, FILE_BASENAME)).split("\n")
    except subprocess.CalledProcessError:
        src = []

    inserted, dropped = [], []
    for name in dangling:
        s = next((i for i, l in enumerate(src)
                  if l.startswith("    def %s(" % name)), None)
        if s is None:
            dropped.append(name)
            continue
        e = next((i for i in range(s + 1, len(src)) if src[i].startswith("    def ")),
                 len(src))
        method = src[s:e]
        while method and not method[-1].strip():
            method.pop()
        anchor = next((i for i, l in enumerate(lines)
                       if l.startswith("    def ") and
                       not l.startswith("    def %s(" % name)), None)
        if anchor is None:
            dropped.append(name)
            continue
        lines = lines[:anchor] + method + [""] + lines[anchor:]
        inserted.append(name)

    if dropped:
        tb = in_tests_block(lines)
        lines = [l for i, l in enumerate(lines)
                 if not (i in tb and ENTRY.match(l)
                         and ENTRY.match(l).group(2) in dropped)]
    return lines, inserted, dropped


def method_spans(lines):
    """(qualified_name, start, end) for every `    def` in the file.

    Qualified by enclosing class, because a file like vehicle_test_suite.py
    holds dozens of classes and many legitimately share method names -
    __init__, update, close, flush. Treating those as duplicates deletes
    real code: an earlier version of this script removed 29 classes from
    vehicle_test_suite.py that way.
    """
    cls = "<module>"
    idx = []
    for i, l in enumerate(lines):
        m = re.match(r"class (\w+)", l)
        if m:
            cls = m.group(1)
        elif re.match(r"    def \w+\(", l):
            idx.append((i, "%s.%s" % (cls, re.match(r"    def (\w+)\(", l).group(1))))
    out = []
    for k, (i, name) in enumerate(idx):
        end = idx[k + 1][0] if k + 1 < len(idx) else len(lines)
        out.append((name, i, end))
    return out


def drop_orphan_dupes(lines, pr_spans):
    """Delete our copy of a method the PR side also brought in.

    Taking the PR side wholesale (shape 1) re-adds methods the branch already
    carries, because our copy matched into the common region above the
    conflict. REFRESH_NOTES says to delete the orphaned head, not the PR's
    copy - the PR's is the current one. Copies inside a PR span are kept.
    """
    dropped = []
    while True:
        spans = method_spans(lines)
        counts = {}
        for name, _, _ in spans:
            counts[name] = counts.get(name, 0) + 1
        dup = next((n for n, c in counts.items()
                    if c > 1 and not n.endswith(".tests")), None)
        if dup is None:
            return lines, sorted(set(dropped))
        cand = [(s_, e_) for n, s_, e_ in spans if n == dup]
        orphan = next(((s_, e_) for s_, e_ in cand
                       if not any(a <= s_ < b for a, b in pr_spans)), cand[0])
        lines = lines[:orphan[0]] + lines[orphan[1]:]
        dropped.append(dup)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--pr", required=True)
    ap.add_argument("--apply", action="store_true")
    ap.add_argument("file")
    a = ap.parse_args()

    global FILE_BASENAME
    FILE_BASENAME = a.file.rsplit("/", 1)[-1]

    try:
        pr_src = git("show", "refs/sfdpr/%s:Tools/autotest/%s"
                     % (a.pr, FILE_BASENAME)).split("\n")
    except subprocess.CalledProcessError:
        pr_src = []
    pr_defs = {m.group(1) for m in
               (re.match(r"    def (\w+)\(", l) for l in pr_src) if m}

    lines = io.open(a.file, encoding="utf-8").read().split("\n")
    lines, n_union, n_ours, n_theirs, pr_spans = resolve_conflicts(lines, a.file, pr_defs)
    lines, inserted, dropped = fix_dangling(lines, a.pr, a.file)

    print("conflicts: %d unioned, %d to ours, %d to the PR" % (n_union, n_ours, n_theirs))
    lines, orphans = drop_orphan_dupes(lines, pr_spans)
    if orphans:
        print("dropped orphaned duplicate def(s), PR copy kept: %s" % ", ".join(orphans))
    if inserted:
        print("inserted from #%s: %s" % (a.pr, ", ".join(inserted)))
    if dropped:
        print("dropped registrations (no def here or in #%s): %s"
              % (a.pr, ", ".join(dropped)))
    if a.apply:
        io.open(a.file, "w", encoding="utf-8").write("\n".join(lines))
        print("written")
    else:
        print("(dry run)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
