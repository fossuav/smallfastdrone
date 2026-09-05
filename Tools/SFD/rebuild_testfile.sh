#!/bin/bash
#
# Rebuild ONE Tools/autotest file from a vanilla 4.7 base plus each PR's net
# change to that file, so its tests match the current PR heads.
#
# Why: phase-2 (refresh.sh tests) keeps both sides when PRs edit the same
# registration list / method. That produces invalid Python for a few hot files,
# and the old fallback was to copy the loiter branch's integrated copies - which
# assert loiter-era semantics and drift from the PRs as review evolves (a flow
# lockout test failed for exactly this reason). This rebuilds those files from
# the PRs instead.
#
# Method: reset the file to base, then for each PR (in prs.txt order) that
# changes this file, whole-file 3-way merge (PR merge-base -> PR head) onto the
# working copy. resolve_additive.py clears the additive conflicts (our 4.7 side
# empty -> keep our addition, drop master-only entries; no common base -> union
# both additions). Anything genuinely 3-way - both sides editing the same
# existing lines, or a diff3 mis-alignment that splits a method across markers -
# is LEFT with <<< markers. Resolve those by hand (see REFRESH_NOTES.md "Phase 2"
# for the split-method recipe: take cur, then insert the new method verbatim from
# the PR head) and re-run; clean PRs are skipped via the .applied sidecar.
#
# Usage:
#   Tools/SFD/rebuild_testfile.sh <relpath> [pr ...]
# With no PR list, the PRs that touch <relpath> are derived from prs.txt (in
# order), excluding the superseded ones in SKIP below.
#
# Env overrides: SFD_BASE (default upstream/ArduPilot-4.7),
#                SFD_MASTER (default upstream/master).
set -u
DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(git rev-parse --show-toplevel)"
cd "$ROOT" || exit 1
BASE="${SFD_BASE:-upstream/ArduPilot-4.7}"
MASTER="${SFD_MASTER:-upstream/master}"
SKIP="31274"   # superseded upstream - see REFRESH_NOTES.md

F="$1"; shift || true
[ -n "${F:-}" ] || { echo "usage: rebuild_testfile.sh <relpath> [pr ...]"; exit 1; }

prs_all() { sed 's/#.*//' "$DIR/prs.txt" | awk 'NF{print $1}'; }
ref_for() { echo "refs/sfdpr/$1"; }

# derive the PR list if not given: PRs whose net change touches F, in prs.txt order
if [ "$#" -gt 0 ]; then
  PRS="$*"
else
  PRS=""
  for n in $(prs_all); do
    case " $SKIP " in *" $n "*) continue;; esac
    ref="$(ref_for "$n")"
    git rev-parse --verify -q "$ref" >/dev/null || continue
    mb="$(git merge-base "$ref" "$MASTER")"
    if ! git diff --quiet "$mb" "$ref" -- "$F"; then PRS="$PRS $n"; fi
  done
fi
echo "rebuild $F from PRs:$PRS"

APPLIED="$DIR/.state/$(echo "$F" | tr '/' '_').applied"
PENDING="$APPLIED.pending"
mkdir -p "$DIR/.state"

if grep -q '^<<<<<<< \|^>>>>>>> \|^||||||| ' "$F" 2>/dev/null; then
  echo "ERROR: $F has conflict markers - resolve them, then re-run"; exit 3
fi
# A PR that stopped for hand resolution is not recorded as applied by the run
# that stopped, because the resolution happens after it exits. The file is now
# marker-free, so that resolution is done: record it, or the next run re-merges
# the same PR onto a file that already has it and duplicates every addition.
if [ -s "$PENDING" ]; then
  cat "$PENDING" >> "$APPLIED"
  echo "  #$(cat "$PENDING")  hand-resolved, recorded as applied"
  rm -f "$PENDING"
fi
[ -f "$APPLIED" ] || { git show "$BASE:$F" > "$F"; : > "$APPLIED"; echo "(reset $F to base)"; }

TMP="$(mktemp -d)"; trap 'rm -rf "$TMP"' EXIT
for n in $PRS; do
  grep -qx "$n" "$APPLIED" && continue
  ref="$(ref_for "$n")"
  mb="$(git merge-base "$ref" "$MASTER")"
  # A PR stacked on another unmerged PR has its merge-base with master BEFORE
  # the PR it sits on, so its net change would re-add that parent's tests and
  # duplicate every method.  Prefer the newest already-applied ancestor head.
  for prev in $(cat "$APPLIED" 2>/dev/null); do
    pref="$(ref_for "$prev")"
    git rev-parse --verify -q "$pref" >/dev/null || continue
    git merge-base --is-ancestor "$pref" "$ref" 2>/dev/null || continue
    git merge-base --is-ancestor "$mb" "$pref" 2>/dev/null || continue
    mb="$(git rev-parse "$pref")"
  done
  git show "$mb:$F" > "$TMP/b" 2>/dev/null || : > "$TMP/b"
  git show "$ref:$F" > "$TMP/t" 2>/dev/null || { echo "  #$n: no head copy"; echo "$n">>"$APPLIED"; continue; }
  cp "$F" "$TMP/cur"
  if git merge-file -q --diff3 -L cur -L "base#$n" -L "pr#$n" "$TMP/cur" "$TMP/b" "$TMP/t"; then
    cp "$TMP/cur" "$F"; echo "$n" >> "$APPLIED"; echo "  #$n  clean"
  else
    cp "$TMP/cur" "$F"
    python3 "$DIR/resolve_additive.py" "$F"
    if grep -q '^<<<<<<< ' "$F"; then
      echo "$n" > "$PENDING"
      echo "  #$n  MANUAL conflicts remain in $F - resolve by hand, then re-run"
      exit 2
    fi
    echo "$n" >> "$APPLIED"; echo "  #$n  applied (additive auto-resolved)"
  fi
done
python3 -m py_compile "$F" && echo "OK: $F rebuilt and compiles"