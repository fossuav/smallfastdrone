#!/usr/bin/env python3
"""Scan AP_Param tables for what AP_Param's startup validation panics on.

A refresh stacks PRs that each picked the next free parameter index against
master, so two of them landing in one table collide.  Nothing catches that at
build time: it is a runtime PANIC("Bad parameter table") before SITL's first
heartbeat, so EVERY test fails with "Did not receive heartbeat" and none of the
failures is about the parameter.  The panic also names only the first offender
(and only with ENABLE_DEBUG set in AP_Param.cpp), so fixing them one boot at a
time is slow.  Run this after integrating PRs and before any test run.

GroupInfo tables (check_group_info): duplicate idx, idx >= 64, and a name that
would exceed AP_Param's 16-character limit.
Top-level Info tables (duplicate_key): the same k_param_ key or the same
parameter name used twice.

Usage: check_param_tables.py [path ...]      (default: the whole worktree)
"""
import os
import re
import sys

ENTRY = re.compile(
    r'\bAP_(?P<kind>GROUPINFO(?:_FLAGS|_FRAME|_FLAGS_DEFAULT_POINTER)?'
    r'|SUBGROUPINFO|SUBGROUPPTR|SUBGROUPVARPTR|SUBGROUPEXTENSION|NESTEDGROUPINFO)'
    r'\s*\((?P<args>[^;]*?)\)\s*,', re.S)
TABLE = re.compile(
    r'GroupInfo\s+(?P<cls>\w+)::(?P<tbl>\w+)\s*\[\s*\]\s*=\s*\{(?P<body>.*?)\n\};', re.S)
INFO_TABLE = re.compile(
    r'AP_Param::Info\s+(?P<cls>\w+)::(?P<tbl>\w+)\s*\[\s*\]\s*=\s*\{(?P<body>.*?)\n\};', re.S)
# GSCALAR(member, "NAME", default) / GGROUP / GOBJECT / GOBJECTN / GARRAY ...
INFO_ENTRY = re.compile(r'\b(?:G(?:SCALAR|GROUP|OBJECT|OBJECTN|OBJECTPTR|ARRAY)\w*)\s*\(\s*'
                        r'(?P<key>[A-Za-z_0-9]+)\s*,[^,]*?"(?P<name>[^"]*)"')


def entries(body):
    for m in ENTRY.finditer(body):
        kind, args = m.group('kind'), m.group('args')
        name = re.search(r'"([^"]*)"', args)
        idx = None
        if kind == 'NESTEDGROUPINFO':
            parts = [a.strip() for a in args.split(',')]
            if len(parts) >= 2 and parts[1].isdigit():
                idx = int(parts[1])
            yield ('<nested>', idx)
            continue
        if name is None:
            continue
        after = args[name.end():]
        num = re.search(r',\s*(\d+)\s*,', after)
        if num:
            idx = int(num.group(1))
        yield (name.group(1), idx)


def main(paths):
    files = []
    for p in paths:
        if os.path.isfile(p):
            files.append(p)
            continue
        for root, dirs, names in os.walk(p):
            dirs[:] = [d for d in dirs
                       if d not in ('.git', 'build', 'modules', 'buildlogs')]
            files += [os.path.join(root, n) for n in names if n.endswith('.cpp')]

    problems = 0
    for f in sorted(files):
        try:
            src = open(f, encoding='utf-8', errors='replace').read()
        except OSError:
            continue
        if 'GroupInfo' not in src:
            continue
        for t in TABLE.finditer(src):
            seen = {}
            for name, idx in entries(t.group('body')):
                if idx is None:
                    continue
                if idx >= 64:
                    print(f"{f}: {t.group('cls')}::{t.group('tbl')}: "
                          f"idx {idx} >= 64 for {name}")
                    problems += 1
                if idx in seen:
                    print(f"{f}: {t.group('cls')}::{t.group('tbl')}: "
                          f"duplicate idx {idx} for {name} and {seen[idx]}")
                    problems += 1
                else:
                    seen[idx] = name
                if len(name.lstrip('_')) and len(name) > 16:
                    print(f"{f}: {t.group('cls')}::{t.group('tbl')}: "
                          f"name too long ({len(name)}) {name}")
                    problems += 1
        for t in INFO_TABLE.finditer(src):
            body = t.group('body')
            keys = {}
            names = {}
            for m in INFO_ENTRY.finditer(body):
                key, name = m.group('key'), m.group('name')
                where = f"{f}: {t.group('cls')}::{t.group('tbl')}"
                if key in keys:
                    prev_name, prev_end = keys[key]
                    # the same member legitimately backs two names in mutually
                    # exclusive preprocessor branches (H_ vs MOT_ on Copter)
                    between = body[prev_end:m.start()]
                    if not re.search(r'^\s*#\s*(?:else|elif)\b', between, re.M):
                        print(f"{where}: duplicate key {key} for {name} and {prev_name}")
                        problems += 1
                else:
                    keys[key] = (name, m.end())
                if name in names:
                    print(f"{where}: duplicate parameter name {name}")
                    problems += 1
                else:
                    names[name] = key

    print(f"{len(files)} files scanned, {problems} problem(s)")
    return 1 if problems else 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:] or ['.']))
