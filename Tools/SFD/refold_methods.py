#!/usr/bin/env python3
"""Copy test methods verbatim from another ref into a test file.

The hot-file rebuild starts from 4.7, so every test that lives on the branch
rather than in a PR's own diff - branch-only tests, and tests of merged PRs
that 4.7 does not carry - has to be put back by hand. This copies each named
method from REF into the same class in FILE, placed before the method that
follows it in REF, and pulls in any self.<helper>() it calls that FILE's class
(or TestSuite in vehicle_test_suite.py) does not have but REF's class does.

Placement is by class, not by indentation: vehicle_test_suite.py has several
classes with four-space methods, and anchoring on the first "def __init__" in
the file once put two TestSuite helpers into Location.

Usage (from the repo root):
    Tools/SFD/refold_methods.py FILE REF NAME [NAME...]
Registrations are not touched; add them where REF has them.
"""

import ast
import subprocess
import sys


def class_methods(tree):
    """{class name: [(method name, first line, last line)]}, 1-based, decorators included"""
    out = {}
    for c in [n for n in tree.body if isinstance(n, ast.ClassDef)]:
        out[c.name] = [(f.name, (f.decorator_list[0].lineno if f.decorator_list else f.lineno), f.end_lineno)
                       for f in c.body if isinstance(f, (ast.FunctionDef, ast.AsyncFunctionDef))]
    return out


def main():
    path, ref, names = sys.argv[1], sys.argv[2], sys.argv[3:]
    if not names:
        sys.exit(__doc__)
    src = subprocess.run(['git', 'show', '%s:%s' % (ref, path)], capture_output=True, text=True, check=True).stdout
    src_lines = src.split('\n')
    src_classes = class_methods(ast.parse(src))
    suite = ''
    if not path.endswith('vehicle_test_suite.py'):
        suite = open('Tools/autotest/vehicle_test_suite.py').read()
    suite_names = {m[0] for m in class_methods(ast.parse(suite)).get('TestSuite', [])} if suite else set()

    def where(name):
        for cls, ms in src_classes.items():
            for i, m in enumerate(ms):
                if m[0] == name:
                    return cls, i
        return None, None

    queue, done = list(names), set()
    while queue:
        name = queue.pop(0)
        if name in done:
            continue
        done.add(name)
        cls, i = where(name)
        if cls is None:
            print('%s: not in %s' % (name, ref))
            continue
        cur = open(path).read()
        cur_classes = class_methods(ast.parse(cur))
        if cls not in cur_classes:
            print('%s: class %s not in %s' % (name, cls, path))
            continue
        have = {m[0] for m in cur_classes[cls]} | suite_names
        if name in have:
            if name in names:
                print('%s: already present' % name)
            continue
        ms = src_classes[cls]
        start, end = ms[i][1], ms[i][2]
        body = src_lines[start - 1:end]
        # carry the blank lines that separate it from the next method
        while end < len(src_lines) and src_lines[end].strip() == '':
            body.append(src_lines[end])
            end += 1
        lines = cur.split('\n')
        anchor = None
        for nxt in ms[i + 1:]:
            hit = [m for m in cur_classes[cls] if m[0] == nxt[0]]
            if hit:
                anchor = (hit[0][1] - 1, nxt[0])
                break
        if anchor is None:
            last = cur_classes[cls][-1]
            at = last[2]
            while at < len(lines) and lines[at].strip() == '':
                at += 1
            anchor = (at, 'end of %s' % cls)
            body = [''] + body
        lines[anchor[0]:anchor[0]] = body
        open(path, 'w').write('\n'.join(lines))
        print('%s: inserted in %s before %s%s' % (name, cls, anchor[1], '' if name in names else ' (helper)'))
        src_names = {m[0] for m in ms}
        for node in ast.walk(ast.parse('class _X:\n' + '\n'.join(body))):
            if (isinstance(node, ast.Attribute) and isinstance(node.value, ast.Name) and
                    node.value.id == 'self' and node.attr in src_names and node.attr not in done):
                queue.append(node.attr)


if __name__ == '__main__':
    main()
