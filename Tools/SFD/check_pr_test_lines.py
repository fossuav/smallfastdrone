#!/usr/bin/env python3
"""Check each PR's own test additions survived the hot-file rebuild.

A rebuild-tests stop resolved to "ours" keeps the file compiling and every
registration resolving, and can still drop what the PR changed inside an
existing method. Nothing downstream notices until the test runs against the
old body. This lists, per hot file and per manifest PR, the lines the PR's
own diff adds that are not in the file.

Expected misses, each worth one look rather than a fix:
  - #31274, which the rebuild skips as superseded;
  - takeoff(altitude_min=/altitude_max=) lines fix_takeoff_kwargs.py rewrote;
  - a stacked PR's stale copies of its parent's methods, where the parent's
    current copy was kept on purpose;
  - changes to master-only code 4.7 does not have (#34363's DataFlashErase
    ceiling, for one).

Usage (from the repo root):
    Tools/SFD/check_pr_test_lines.py [--all]    # --all prints every missing line
"""

import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
HOT = ['arducopter.py', 'arduplane.py', 'quadplane.py', 'vehicle_test_suite.py']
MASTER = os.environ.get('SFD_MASTER', 'upstream/master')
BRANCH47 = os.environ.get('SFD_BRANCH47', 'upstream/ArduPilot-4.7')


def git(*args):
    return subprocess.run(['git', *args], capture_output=True, text=True).stdout


def fork_point(ref):
    """merge-base with master, or with the 4.7 branch for a PR raised against it"""
    m = git('merge-base', ref, MASTER).strip()
    b = git('merge-base', ref, BRANCH47).strip()
    if b and b != m and subprocess.run(['git', 'merge-base', '--is-ancestor', m, b]).returncode == 0:
        return b
    return m


def main():
    show_all = '--all' in sys.argv
    prs = [l.split('#')[0].split() for l in open(os.path.join(HERE, 'prs.txt'))]
    prs = [p[0] for p in prs if p]
    missing_any = False
    for f in HOT:
        path = 'Tools/autotest/' + f
        cur = open(path).read()
        for n in prs:
            ref = 'refs/sfdpr/%s' % n
            if subprocess.run(['git', 'rev-parse', '--verify', '-q', ref], capture_output=True).returncode:
                continue
            diff = git('diff', '-U0', fork_point(ref), ref, '--', path)
            added = [l[1:].strip() for l in diff.splitlines()
                     if l.startswith('+') and not l.startswith('+++') and len(l[1:].strip()) >= 12]
            miss = [a for a in added if a not in cur]
            if miss:
                missing_any = True
                print('%-22s #%s: %u/%u lines present' % (f, n, len(added) - len(miss), len(added)))
                for m in (miss if show_all else miss[:3]):
                    print('    - %s' % m[:110])
    return 1 if missing_any else 0


if __name__ == '__main__':
    sys.exit(main())
