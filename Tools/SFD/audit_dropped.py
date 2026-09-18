#!/usr/bin/env python3
"""Find work a refresh dropped.

A refresh rebuilds the branch from the base plus prs.txt, so anything the old
shipping branch carried that is in neither is lost without any conflict or
build error to say so. refresh5 dropped six merged-upstream PRs that way.

For every commit on OLD that 4.7 does not have, this checks the commit against
the NEW tree:

  present   the diff reverse-applies (it is there)
  ABSENT    the diff forward-applies (it is not)
  modified  neither - the same code has since changed

then scores ABSENT and modified commits by how many of their added lines
survive in NEW, and matches each low scorer's subject against the current PR
heads (refs/sfdpr/*) and recent master. A low scorer found in a PR head is
usually PR evolution; one found only in master is a merged PR that needs to be
in prs.txt or the base; one found nowhere needs reading by hand. Added lines
are scored against OLD's tree too, and a commit OLD itself later reverted or
reworked (low in OLD as well) is left out: it was not on the branch to lose.

Usage: audit_dropped.py OLD NEW [--threshold 70]
"""

import argparse
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
SKIP = ('Tools/SFD/', 'README.md')


def git(*args, check=False, stdin=None):
    r = subprocess.run(('git',) + args, capture_output=True, text=True, errors='replace', input=stdin)
    if check and r.returncode != 0:
        sys.exit(r.stderr)
    return r


def index_env(ref):
    # a scratch index holding NEW's tree, so `apply --check --cached` tests against
    # NEW without it being checked out
    env = dict(os.environ, GIT_INDEX_FILE=os.path.join(git('rev-parse', '--git-dir').stdout.strip(), 'audit_dropped.index'))
    subprocess.run(['git', 'read-tree', ref], env=env, check=True)
    return env


def applies(diff, env, reverse):
    args = ['git', 'apply', '--check', '--cached'] + (['--reverse'] if reverse else [])
    args += [f'--exclude={p}*' if p.endswith('/') else f'--exclude={p}' for p in SKIP]
    return subprocess.run(args + ['-'], input=diff, capture_output=True, text=True, env=env).returncode == 0


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('old')
    ap.add_argument('new')
    ap.add_argument('--threshold', type=int, default=70)
    ap.add_argument('--base', default='upstream/ArduPilot-4.7')
    ap.add_argument('--master', default='upstream/master')
    a = ap.parse_args()

    mb = git('merge-base', a.old, a.base, check=True).stdout.strip()
    shas = git('rev-list', '--reverse', '--no-merges', f'{mb}..{a.old}', check=True).stdout.split()

    prs = [l.split('#')[0].split() for l in open(os.path.join(HERE, 'prs.txt'))]
    where = {}
    for n in [p[0] for p in prs if p]:
        head = f'refs/sfdpr/{n}'
        if git('rev-parse', '--verify', '-q', head).returncode:
            continue
        pmb = git('merge-base', head, a.master).stdout.strip()
        for s in git('log', '--format=%s', f'{pmb}..{head}').stdout.splitlines():
            where.setdefault(s, set()).add(f'#{n}')
    for s in git('log', '--since=6.months', '--format=%s', a.master).stdout.splitlines():
        where.setdefault(s, set()).add('master')

    env = index_env(a.new)
    cache = {}

    def text(ref, path):
        if (ref, path) not in cache:
            cache[(ref, path)] = git('show', f'{ref}:{path}').stdout
        return cache[(ref, path)]

    counts = {}
    rows = []
    for sha in shas:
        subj = git('log', '-1', '--format=%s', sha).stdout.strip()
        diff = git('show', '--format=', sha).stdout
        if applies(diff, env, reverse=True):
            st = 'present'
        elif applies(diff, env, reverse=False):
            st = 'ABSENT'
        else:
            st = 'modified'
        counts[st] = counts.get(st, 0) + 1
        if st == 'present':
            continue
        added = []
        path = None
        for d in git('show', '--format=', '-U0', sha).stdout.splitlines():
            if d.startswith('+++ '):
                path = d[6:] if d.startswith('+++ b/') else None
            elif path and not path.startswith(SKIP) and d.startswith('+') and len(d[1:].strip()) >= 15:
                added.append((path, d[1:].strip()))
        # score code lines; a commit that only adds comments or docs is scored on those
        code = [(p, s) for p, s in added if not s.startswith(('//', '#', '*', '/*'))]
        lines = code or added
        total = len(lines)
        hit = sum(s in text(a.new, p) for p, s in lines)
        old_hit = sum(s in text(a.old, p) for p, s in lines)
        if total and 100 * old_hit // total < a.threshold:
            continue
        # an ABSENT diff is reported whatever its line score: a small change can
        # score high only because its lines also occur elsewhere in the file
        pct = 100 * hit // total if total else 0
        if st == 'ABSENT' or pct < a.threshold:
            rows.append((pct, st, sha[:10], subj, ','.join(sorted(where.get(subj, ()))) or 'NOWHERE'))

    print(' '.join(f'{k}={v}' for k, v in sorted(counts.items())))
    for pct, st, sha, subj, w in sorted(rows):
        print(f'{pct:3d}% {st:8s} {sha} {subj[:70]:70s} {w}')


if __name__ == '__main__':
    main()
