#!/usr/bin/env python3
"""List the parameter and default changes a refresh brings in.

A PR that turns existing behaviour into an option, flips a default or renames
a setting changes what an SFD board does on the next refresh, with a clean
build. Each such change needs a value in hwdef/include/sfd_defaults.parm (or,
for a compile-time define, the SFD hwdefs) so users see no change -
#32473's ACC_ZBIAS_LEARN bit 3 and #32398's ARMING_DELAY_MS were two.

This diffs OLD..NEW over the flight code and prints, per file:
  - parameter table entries added, removed or changed (name, index, default)
  - @Bitmask and @Values documentation changes (a new bit or value that is
    off by default is how "behaviour becomes an option" usually looks)
  - compile-time #define defaults that changed
Read each one and decide; the script does not.

Usage (from the repo root):
    Tools/SFD/param_changes.py SmallFastDrone-4.7.1-beta SmallFastDrone-4.7.1-refreshN
"""

import re
import subprocess
import sys

PATHS = ['libraries', 'ArduCopter', 'ArduPlane']
SKIP = ('libraries/SITL/', 'libraries/AP_HAL_ChibiOS/hwdef/', '/tests/', '/examples/')
INTERESTING = re.compile(
    r'^[-+]\s*(AP_GROUPINFO\w*|AP_SUBGROUP\w*|// @Bitmask|// @Values|// @Param:|#\s*define \w+_(DEFAULT|MS|MSEC|SEC|ENABLED)\b)')


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    old, new = sys.argv[1:]
    diff = subprocess.run(['git', 'diff', '-U0', old, new, '--', *PATHS],
                          capture_output=True, text=True, check=True).stdout
    path = None
    printed = None
    for line in diff.splitlines():
        if line.startswith('+++ '):
            path = line[6:] if line.startswith('+++ b/') else None
            continue
        if not path or any(s in path for s in SKIP) or not path.endswith(('.cpp', '.h')):
            continue
        if line.startswith(('+++', '---')) or not INTERESTING.match(line):
            continue
        if printed != path:
            print('== %s' % path)
            printed = path
        print('   %s' % line[:200])


if __name__ == '__main__':
    main()
