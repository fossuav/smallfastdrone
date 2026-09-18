#!/usr/bin/env python3
"""Scan the autotest tree for master-only API used by tests taken from PR heads.

The hot-file rebuild copies test bodies verbatim from PR heads, which are
written against master. Where 4.7's harness differs the test still compiles,
loads and registers, then dies on the first call - so each one otherwise costs a
full test run to find. Run this after rebuild-tests and before testing.

Three checks, each diffed against vanilla 4.7 so pre-existing upstream quirks do
not drown the real findings:

  helpers   self.X(...) with no def X anywhere in the tree
  kwargs    keyword names a call passes that 4.7's own signature does not accept
  scripts   install_*_script*() naming a Lua file this tree does not ship

Usage: check_test_api.py [--base <git-ref>] [autotest-dir]
"""
import argparse
import ast
import os
import re
import subprocess
import sys

VEHICLE_FILES = ('arducopter.py', 'arduplane.py', 'quadplane.py',
                 'helicopter.py', 'ardusub.py', 'rover.py',
                 'vehicle_test_suite.py')


def parse(src, path):
    try:
        return ast.parse(src)
    except SyntaxError as e:
        print(f"{path}: SYNTAX ERROR {e}")
        return None


def collect(files):
    """defs, self-calls and their keywords across a set of (path, source)."""
    defs, calls, kwargs_used = set(), {}, []
    for path, src in files:
        tree = parse(src, path)
        if tree is None:
            continue
        for n in ast.walk(tree):
            if isinstance(n, (ast.FunctionDef, ast.AsyncFunctionDef)):
                defs.add(n.name)
            if isinstance(n, ast.Call) and isinstance(n.func, ast.Attribute) \
                    and isinstance(n.func.value, ast.Name) and n.func.value.id == 'self':
                calls.setdefault(n.func.attr, []).append(f"{path}:{n.lineno}")
                for kw in n.keywords:
                    if kw.arg:
                        kwargs_used.append((n.func.attr, kw.arg, f"{path}:{n.lineno}"))
    return defs, calls, kwargs_used


def signatures(files):
    """method name -> set of accepted keyword names, or None if it takes **kwargs.

    A name defined more than once (helper classes, vehicle subclasses) takes the
    union of what its definitions accept, so an overload never reads as a
    rejected keyword.
    """
    out = {}
    for path, src in files:
        tree = parse(src, path)
        if tree is None:
            continue
        for n in ast.walk(tree):
            if not isinstance(n, (ast.FunctionDef, ast.AsyncFunctionDef)):
                continue
            if n.args.kwarg is not None:
                out[n.name] = None
                continue
            if out.get(n.name, set()) is None:
                continue          # already known to take **kwargs
            names = {a.arg for a in n.args.args} | {a.arg for a in n.args.kwonlyargs}
            out[n.name] = out.get(n.name, set()) | names
    return out


def read_local(d):
    files = []
    for name in VEHICLE_FILES:
        p = os.path.join(d, name)
        if os.path.exists(p):
            files.append((p, open(p, encoding='utf-8', errors='replace').read()))
    return files


def read_ref(ref, d):
    files = []
    for name in VEHICLE_FILES:
        p = f"{d}/{name}"
        r = subprocess.run(['git', 'show', f'{ref}:{p}'], capture_output=True, text=True)
        if r.returncode == 0:
            files.append((p, r.stdout))
    return files


def script_files(files, roots):
    """install_*_script*() arguments, resolving simple local variables."""
    problems = []
    for path, src in files:
        tree = parse(src, path)
        if tree is None:
            continue
        for fn in [n for n in ast.walk(tree) if isinstance(n, (ast.FunctionDef, ast.AsyncFunctionDef))]:
            literals = {}
            for n in ast.walk(fn):
                if isinstance(n, ast.Assign) and isinstance(n.value, ast.Constant) \
                        and isinstance(n.value.value, str):
                    for t in n.targets:
                        if isinstance(t, ast.Name):
                            literals[t.id] = n.value.value
            if re.match(r'install_\w*script\w*$', fn.name):
                continue          # the helper's own definition passes its parameter
            for n in ast.walk(fn):
                if not (isinstance(n, ast.Call) and isinstance(n.func, ast.Attribute)):
                    continue
                if not re.match(r'install_\w*script\w*$', n.func.attr):
                    continue
                if not n.args:
                    continue
                a = n.args[0]
                if isinstance(a, ast.Constant) and isinstance(a.value, str):
                    name = a.value
                elif isinstance(a, ast.Name) and a.id in literals:
                    name = literals[a.id]
                else:
                    problems.append((f"{path}:{n.lineno}", f"{fn.name}: unresolved script argument"))
                    continue
                if not any(os.path.exists(os.path.join(r, name)) for r in roots):
                    problems.append((f"{path}:{n.lineno}", f"{fn.name}: missing script {name}"))
    return problems


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--base', default='upstream/ArduPilot-4.7')
    ap.add_argument('dir', nargs='?', default='Tools/autotest')
    args = ap.parse_args()

    cur = read_local(args.dir)
    base = read_ref(args.base, args.dir)
    if not cur:
        print(f"no autotest files under {args.dir}")
        return 2

    cur_defs, cur_calls, cur_kwargs = collect(cur)
    base_defs, base_calls, _ = collect(base)
    problems = 0

    missing_now = {k for k in cur_calls if k not in cur_defs}
    missing_base = {k for k in base_calls if k not in base_defs}
    for name in sorted(missing_now - missing_base):
        print(f"MISSING HELPER  self.{name}()  at {', '.join(cur_calls[name][:3])}")
        problems += 1

    cur_sigs = signatures(cur)
    _, base_calls_kw, base_kwargs = collect(base)
    base_sigs = signatures(base)
    base_bad = {(m, k) for m, k, _ in base_kwargs
                if base_sigs.get(m) is not None and m in base_sigs and k not in base_sigs[m]}
    for meth, kw, where in cur_kwargs:
        if meth not in cur_sigs:
            continue          # defined outside this tree
        accepted = cur_sigs[meth]
        if accepted is None or kw in accepted:
            continue          # **kwargs, or accepted
        if (meth, kw) in base_bad:
            continue          # pre-existing upstream quirk, not ours
        print(f"BAD KWARG       self.{meth}({kw}=...) at {where}; "
              f"this tree accepts {sorted(accepted)}")
        problems += 1

    roots = ['libraries/AP_Scripting/examples', 'libraries/AP_Scripting/applets',
             'libraries/AP_Scripting/tests', 'Tools/autotest/scripts']
    # diff against 4.7 by message, so scripts this tree never shipped (and the
    # unresolvable pass-throughs) do not drown the ones the refresh introduced
    base_msgs = {m for _, m in script_files(base, roots)}
    for where, msg in script_files(cur, roots):
        if msg in base_msgs:
            continue
        print(f"SCRIPT          {msg}  at {where}")
        problems += 1

    print(f"{len(cur)} files scanned, {problems} problem(s)")
    return 1 if problems else 0


if __name__ == '__main__':
    sys.exit(main())
