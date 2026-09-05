#!/bin/bash
#
# Run the SFD test set.
#
# The set is derived from the branch, not a hand-kept list: every test this
# branch registers that vanilla 4.7 does not have, plus the handful whose bodies
# this branch modifies. So it follows the PR stack automatically and a test that
# arrives with a PR head is picked up without anyone remembering to add it.
#
# Ordering puts the regression watch list first (the tests that were failing when
# last looked at, named in WATCH below) so a run reports the interesting answer in
# the first few minutes, and the throw tests last because a crashing test costs
# ~45 minutes of reconnect stall before the harness gives up.
#
# Usage:
#   Tools/SFD/run_sfd_tests.sh                 # run the whole set
#   Tools/SFD/run_sfd_tests.sh --list          # print the set and exit
#   Tools/SFD/run_sfd_tests.sh --resume        # skip steps that already passed
#   Tools/SFD/run_sfd_tests.sh --summary       # summarise the last run and exit
#   Tools/SFD/run_sfd_tests.sh test.Copter.X   # run only what is named
#
# DEBUGINFOD_URLS is cleared: on a crash the harness runs dumpstack.sh, and gdb
# otherwise stalls trying to download symbols until the run times out with no
# backtrace. Backtraces also need kernel.yama.ptrace_scope=0.
set -u
DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(git rev-parse --show-toplevel)"
cd "$ROOT" || exit 1
LOG="${SFD_TEST_LOG:-$ROOT/../buildlogs/sfd_tests.log}"
TIMEOUT="${SFD_TEST_TIMEOUT:-28800}"

# tests that were failing or crashing when this list was last touched; they run
# first so a run answers the open question before the slow bulk
WATCH="EK3NoAidAccelBiasXY EKFSourceSetFailsafe OpticalFlowGPSLossAiding
       LoiterNoCompassYawGPS EK3_OptflowAssumeFlatGnd OpticalFlowFocusHeight
       HeightDatumKeptOnMidairRearm"

list_steps() {
  WATCH="$WATCH" python3 - <<'PY'
import os, re, subprocess
def show(ref, p):
    r = subprocess.run(['git', 'show', f'{ref}:{p}'], capture_output=True, text=True)
    return r.stdout if r.returncode == 0 else ''
def regs(t):
    return (set(re.findall(r'^\s+self\.([A-Z][A-Za-z_0-9]*),\s*$', t, re.M)) |
            set(re.findall(r'Test\(self\.([A-Z][A-Za-z_0-9]*)', t)))
def meths(t):
    return {m.group(1): m.group(0) for m in
            re.finditer(r'^    def ([A-Za-z_0-9]+)\(.*?(?=^    def |\Z)', t, re.S | re.M)}
watch = os.environ.get('WATCH', '').split()
base_ref = os.environ.get('SFD_TEST_BASE', 'upstream/ArduPilot-4.7')
out = []
for veh, f in (('Copter', 'arducopter.py'), ('Plane', 'arduplane.py'),
               ('QuadPlane', 'quadplane.py'), ('Helicopter', 'helicopter.py')):
    p = f'Tools/autotest/{f}'
    try:
        src = open(p, encoding='utf-8', errors='replace').read()
    except OSError:
        continue
    base = show(base_ref, p)
    cm, bm = meths(src), meths(base)
    new = (regs(src) - regs(base)) & set(cm)
    mod = {n for n in cm if n in bm and cm[n] != bm[n] and n[0].isupper()} & regs(src)
    out += [(veh, n) for n in sorted(new | mod)]
def key(item):
    veh, n = item
    if n in watch:
        return (0, watch.index(n), '')
    if n.startswith('Throw'):
        return (2, 0, n)
    return (1, 0, f'{veh}.{n}')
out.sort(key=key)
for veh, n in out:
    print(f'test.{veh}.{n}')
PY
}

summarise() {
  [ -f "$LOG" ] || { echo "no log at $LOG"; return 1; }
  local p f
  p=$(grep -c '^>>>> PASSED STEP' "$LOG" 2>/dev/null || echo 0)
  f=$(grep -c '^>>>> FAILED STEP' "$LOG" 2>/dev/null || echo 0)
  echo "log: $LOG"
  echo "passed: $p   failed: $f   crashes: $(grep -c 'Floating point exception' "$LOG" 2>/dev/null || echo 0)"
  if [ "$f" -gt 0 ]; then
    echo "failures:"
    grep -E '^>>>> FAILED STEP' "$LOG" | sed -E 's/^>>>> FAILED STEP: ([^ ]+).*/  \1/'
    echo "reasons:"
    grep -hoE 'FAILED: "[^"]+": .*' "$LOG" | sed 's/^/  /' | cut -c1-160 | sort -u
  fi
  grep -q '^TIMEOUT' "$LOG" && echo "NOTE: the run hit its wall clock; a timeout is not a pass"
  return 0
}

case "${1:-}" in
  --list)    list_steps; exit 0;;
  --summary) summarise; exit $?;;
esac

RESUME=0
if [ "${1:-}" = "--resume" ]; then RESUME=1; shift; fi

if [ "$#" -gt 0 ]; then
  STEPS="$*"
else
  STEPS="$(list_steps | tr '\n' ' ')"
fi

if [ "$RESUME" = 1 ] && [ -f "$LOG" ]; then
  DONE="$(grep -E '^>>>> PASSED STEP' "$LOG" | sed -E 's/^>>>> PASSED STEP: ([^ ]+).*/\1/' | sort -u)"
  KEPT=""
  for s in $STEPS; do
    grep -qxF "$s" <<<"$DONE" || KEPT="$KEPT $s"
  done
  echo "resume: $(wc -w <<<"$DONE") already passed, $(wc -w <<<"$KEPT") to run"
  STEPS="$KEPT"
  cp "$LOG" "$LOG.prev"
fi

[ -n "${STEPS// /}" ] || { echo "nothing to run"; exit 0; }
echo "running $(wc -w <<<"$STEPS") steps -> $LOG"
mkdir -p "$(dirname "$LOG")"
DEBUGINFOD_URLS= python3 .claude/skills/autotest/run_autotest.py --timeout "$TIMEOUT" $STEPS > "$LOG" 2>&1
rc=$?
echo "run_autotest exit=$rc"
summarise
exit $rc
