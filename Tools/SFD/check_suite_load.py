#!/usr/bin/env python3
"""Load every vehicle test suite and check its registrations.

py_compile accepts a registration whose def was lost in the hot-file rebuild;
it only fails as an AttributeError when tests() runs. This imports each
vehicle module and reports, for every self.X listed in a tests*() method:

  DANGLING       no attribute X on the class (nor inherited from TestSuite)
  DUP            X registered more than once
  DUPLICATE def  a method defined twice in one class (only tests() may repeat)

With --against REF it also lists the registrations REF has that this tree
lacks, and the reverse. That diff is a prompt, not a specification: REF is
stale wherever a PR moved, so a name missing here may have been replaced
upstream on purpose (EK3_PerCoreOptflowLogging -> EK3_PerCoreLogging).

Usage (from the repo root):
    Tools/SFD/check_suite_load.py [--against SmallFastDrone-4.7.1-beta]
"""

import argparse
import ast
import collections
import os
import subprocess
import sys

SUITES = {
    'arducopter': ['AutoTestCopter'],
    'helicopter': ['AutoTestHelicopter'],
    'arduplane': ['AutoTestPlane'],
    'quadplane': ['AutoTestQuadPlane'],
    'rover': ['AutoTestRover'],
    'ardusub': ['AutoTestSub'],
}


def registrations(src, classes):
    """{class: Counter(registered name)} and {class: Counter(def name)}, by AST"""
    regs, defs = {}, {}
    for cls in [n for n in ast.parse(src).body if isinstance(n, ast.ClassDef) and n.name in classes]:
        defs[cls.name] = collections.Counter(f.name for f in cls.body if isinstance(f, ast.FunctionDef))
        found = collections.Counter()
        for f in cls.body:
            if isinstance(f, ast.FunctionDef) and f.name.startswith('tests'):
                for node in ast.walk(f):
                    if isinstance(node, ast.List):
                        for e in node.elts:
                            # Test(self.X, ...) wraps a registration too
                            if isinstance(e, ast.Call) and e.args:
                                e = e.args[0]
                            if (isinstance(e, ast.Attribute) and isinstance(e.value, ast.Name) and
                                    e.value.id == 'self'):
                                found[e.attr] += 1
        regs[cls.name] = found
    return regs, defs


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--against', help='ref whose registrations to diff against')
    a = ap.parse_args()

    sys.path.insert(0, 'Tools/autotest')
    os.environ.setdefault('BUILDLOGS', '/tmp')
    bad = 0
    ours = set()
    for mod, classes in SUITES.items():
        path = 'Tools/autotest/%s.py' % mod
        regs, defs = registrations(open(path).read(), classes)
        module = __import__(mod)
        for cls in classes:
            klass = getattr(module, cls)
            for d, n in defs.get(cls, {}).items():
                if n > 1 and d != 'tests':
                    print('DUPLICATE def %s.%s.%s x%u' % (mod, cls, d, n))
                    bad += 1
            for r, n in sorted(regs.get(cls, {}).items()):
                ours.add((cls, r))
                if not hasattr(klass, r):
                    print('DANGLING %s.%s: %s' % (mod, cls, r))
                    bad += 1
                if n > 1:
                    print('DUP %s.%s: %s x%u' % (mod, cls, r, n))
                    bad += 1
            print('%s.%s: %u registrations' % (mod, cls, len(regs.get(cls, {}))))

    if a.against:
        theirs = set()
        for mod, classes in SUITES.items():
            src = subprocess.run(['git', 'show', '%s:Tools/autotest/%s.py' % (a.against, mod)],
                                 capture_output=True, text=True).stdout
            if src:
                for cls, names in registrations(src, classes)[0].items():
                    theirs |= {(cls, n) for n in names}
        for (cls, n) in sorted(theirs - ours):
            print('ONLY IN %s: %s.%s' % (a.against, cls, n))
        for (cls, n) in sorted(ours - theirs):
            print('ONLY HERE: %s.%s' % (cls, n))
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
