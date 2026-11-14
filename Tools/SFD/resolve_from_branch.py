#!/usr/bin/env python3
"""Resolve refresh conflicts from the previously shipped branch.

Most conflicts in a refresh are the same shape: the PR was written against
master, 4.7 differs, and the branch we are replacing already carries a
resolution that built and passed tests. Re-deriving it by hand each time is
slow and is how a subtly different resolution gets shipped.

This takes the shipped branch's version of a conflicted file, but only when
that is provably safe. Two conditions, and both have to hold:

1. No commit still to be applied touches the file. Otherwise the shipped
   version is a state from the future and would silently skip those commits.
2. No PR that touches the file has moved since applied.lock. The shipped
   branch was built from the locked heads, so for a PR that has been pushed
   since, the branch's copy is STALE - taking it silently reverts whatever
   that PR changed. This is not hypothetical: #32232 renamed takeOffDetected
   to movedSinceArming after the lock, and taking the branch's
   AP_NavEKF3_VehicleStatus.cpp put the old name back against a header that
   only declares the new one.

Usage:
    resolve_from_branch.py [--branch REF] [--apply] [file ...]

With no files, acts on every conflicted file. Default is a dry run; --apply
writes and stages. Exit 1 if any file was left unresolved.
"""
import argparse
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
STATE = os.path.join(HERE, ".state")


def git(*args, **kw):
    return subprocess.run(["git", *args], capture_output=True, text=True,
                          check=kw.get("check", True)).stdout


def conflicted():
    out = git("diff", "--name-only", "--diff-filter=U")
    return [l for l in out.split("\n") if l]


def remaining_shas():
    """Commits after the one currently being resolved.

    progress.idx points AT the in-flight commit - refresh.sh only advances it
    once that commit is committed - so the commit being resolved is shas[idx]
    and anything genuinely later starts at idx+1. Counting the in-flight one
    would make every file look unsafe.
    """
    try:
        with open(os.path.join(STATE, "apply.txt")) as f:
            shas = [l.strip() for l in f if l.strip()]
        with open(os.path.join(STATE, "progress.idx")) as f:
            idx = int(f.read().strip() or 0)
    except OSError:
        return None
    return shas[idx + 1:]


def touched_by_remaining(paths):
    """Map path -> list of upcoming SHAs that modify it."""
    shas = remaining_shas()
    hits = {p: [] for p in paths}
    if shas is None:
        return None
    for sha in shas:
        files = git("show", "--pretty=", "--name-only", sha).split("\n")
        for p in paths:
            if p in files:
                hits[p].append(sha)
    return hits


def moved_prs():
    """PRs whose head has moved since applied.lock, so the branch is stale."""
    lock = os.path.join(HERE, "applied.lock")
    moved = set()
    try:
        rows = [l.split() for l in open(lock) if l.strip()]
    except OSError:
        return moved
    for row in rows:
        if len(row) < 2 or row[0] == "BASE":
            continue
        pr, locked = row[0], row[1]
        try:
            now = git("rev-parse", "refs/sfdpr/%s" % pr).strip()
        except subprocess.CalledProcessError:
            continue
        if now != locked:
            moved.add(pr)
    return moved


def touched_by_moved_prs(paths):
    """Map path -> sorted PRs that touch it and have moved since the lock.

    Walks the whole apply list, not just what is left: a PR earlier in the
    stack has already been applied to our tree, and if that PR moved since
    the lock then the branch does not have its current form.
    """
    moved = moved_prs()
    hits = {p: set() for p in paths}
    if not moved:
        return hits
    try:
        sha_pr = {}
        with open(os.path.join(STATE, "sha_pr.tsv")) as f:
            for line in f:
                parts = line.rstrip("\n").split("\t")
                if len(parts) >= 2:
                    sha_pr.setdefault(parts[0], parts[1])
    except OSError:
        return hits
    for sha, pr in sha_pr.items():
        if pr not in moved:
            continue
        try:
            files = git("show", "--pretty=", "--name-only", sha).split("\n")
        except subprocess.CalledProcessError:
            continue
        for p in paths:
            if p in files:
                hits[p].add(pr)
    return {p: sorted(v) for p, v in hits.items()}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--branch", default="SmallFastDrone-4.7.1-beta",
                    help="branch carrying the validated resolution")
    ap.add_argument("--apply", action="store_true", help="write and stage")
    ap.add_argument("files", nargs="*")
    a = ap.parse_args()

    paths = a.files or conflicted()
    if not paths:
        print("no conflicted files")
        return 0

    hits = touched_by_remaining(paths)
    if hits is None:
        print("no refresh state found; run 'refresh.sh plan' first", file=sys.stderr)
        return 1
    stale = touched_by_moved_prs(paths)

    left = 0
    for p in paths:
        if stale.get(p):
            print("SKIP  %s - touched by PR(s) %s, moved since applied.lock, so "
                  "%s's copy is stale" % (p, ", ".join("#" + x for x in stale[p]), a.branch))
            left += 1
            continue
        later = hits[p]
        if later:
            print("SKIP  %s - %d later commit(s) touch it, e.g. %s"
                  % (p, len(later), later[0][:10]))
            left += 1
            continue
        try:
            blob = git("show", "%s:%s" % (a.branch, p))
        except subprocess.CalledProcessError:
            print("SKIP  %s - not present on %s" % (p, a.branch))
            left += 1
            continue
        if a.apply:
            with open(p, "w") as f:
                f.write(blob)
            git("add", p)
            print("TAKE  %s <- %s" % (p, a.branch))
        else:
            print("WOULD TAKE  %s <- %s" % (p, a.branch))
    if left:
        print("\n%d file(s) need hand resolution" % left)
    return 1 if left else 0


if __name__ == "__main__":
    sys.exit(main())
