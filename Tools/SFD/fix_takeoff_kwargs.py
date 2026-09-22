#!/usr/bin/env python3
"""Rewrite master-style takeoff() calls for 4.7's Copter harness.

Master's takeoff() takes altitude_min and altitude_max, a ceiling. 4.7's takes
alt_min and max_err, a tolerance: the ceiling is alt_min + max_err. So
altitude_max=6 on takeoff(4, ...) becomes max_err=2, not a rename. With no
altitude_max, master applies no ceiling where 4.7 applies alt_min + 5; the
wait returns as the climb passes through that window, so it has not mattered.
check_test_api.py reports these as BAD KWARG; this fixes them.

Only literal values are converted; anything else is printed for hand fixing.

Usage (from the repo root):
    Tools/SFD/fix_takeoff_kwargs.py Tools/autotest/arducopter.py
"""
import ast, re, sys
path = sys.argv[1]
src = open(path).read()
tree = ast.parse(src)
lines = src.split('\n')
offs = [0]
for l in lines: offs.append(offs[-1] + len(l) + 1)
pos = lambda ln, col: offs[ln - 1] + col
edits = []
for node in ast.walk(tree):
    if not (isinstance(node, ast.Call) and isinstance(node.func, ast.Attribute) and node.func.attr == 'takeoff'
            and isinstance(node.func.value, ast.Name) and node.func.value.id == 'self'):
        continue
    kws = {k.arg: k for k in node.keywords}
    if 'altitude_min' not in kws and 'altitude_max' not in kws:
        continue
    amin = kws['altitude_min'].value if 'altitude_min' in kws else (node.args[0] if node.args else None)
    for k in node.keywords:
        s, e = pos(k.lineno, k.col_offset), pos(k.end_lineno, k.end_col_offset)
        if k.arg == 'altitude_min':
            edits.append((s, e, 'alt_min=' + ast.get_source_segment(src, k.value)))
        elif k.arg == 'altitude_max':
            try:
                err = ast.literal_eval(k.value) - ast.literal_eval(amin)
            except Exception:
                print(f'{path}:{node.lineno}: non-literal altitude_max, fix by hand'); continue
            err = int(err) if float(err).is_integer() else err
            edits.append((s, e, f'max_err={err}'))
    print(f'{path}:{node.lineno}: {ast.get_source_segment(src, node)}')
for s, e, t in sorted(edits, reverse=True):
    src = src[:s] + t + src[e:]
open(path, 'w').write(src)
