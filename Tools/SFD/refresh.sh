#!/bin/bash
#
# SFD branch refresh
# -----------------
# Rebuild the Small Fast Drone feature set on top of a vanilla ArduPilot-4.7
# base by cherry-picking the upstream PR branches listed in prs.txt, in order.
#
# Why this exists: the SFD branches are a curated stack of in-flight upstream
# PRs. Those PRs keep evolving in review, so periodically we want to re-stack
# the *current* PR heads onto the latest 4.7 to (a) get an up-to-date branch
# and (b) surface where our local copy has drifted or been superseded upstream.
#
# Key behaviours:
#   - PR heads are fetched via GitHub's pull/N/head, so author/fork is
#     irrelevant (works for andyp1per, rishabsingh3003, rmackay9, ...).
#   - Commits already patch-present in the base are skipped (git cherry).
#   - Tools/autotest changes are deferred (dropped per commit) - the SITL
#     tests collide heavily across PRs; pull them in as a batch afterwards
#     with the "tests" step.
#   - git rerere replays previously recorded conflict resolutions, so a
#     refresh after this first one only stops on genuinely new conflicts.
#
# Usage:
#   Tools/SFD/refresh.sh fetch     # fetch all PR heads listed in prs.txt
#   Tools/SFD/refresh.sh plan      # compute the ordered apply list
#   Tools/SFD/refresh.sh run       # cherry-pick; stop on each new conflict
#   Tools/SFD/refresh.sh survey    # like run but auto-resolve to base + log
#   Tools/SFD/refresh.sh status    # progress / remaining
#
# After a "run" stops on a conflict: resolve it (rerere records it), `git add`
# the files, `git commit -C <sha>` (the sha is printed), then re-run "run".
#
# Env overrides: SFD_BASE (default upstream/ArduPilot-4.7),
#                SFD_MASTER (default upstream/master).

set -u
DIR="$(cd "$(dirname "$0")" && pwd)"
BASE="${SFD_BASE:-upstream/ArduPilot-4.7}"
MASTER="${SFD_MASTER:-upstream/master}"
DEFER="Tools/autotest"
ST="$DIR/.state"
mkdir -p "$ST"

prs() { sed 's/#.*//' "$DIR/prs.txt" | awk 'NF{print $1}'; }

do_fetch() {
  local rs=""
  for n in $(prs); do rs="$rs +pull/$n/head:refs/sfdpr/$n"; done
  echo "fetching $(prs | wc -l) PR heads ..."
  git fetch upstream $rs
}

do_plan() {
  : > "$ST/apply.txt"; : > "$ST/sha_pr.tsv"
  for n in $(prs); do
    local base; base="$(git merge-base "refs/sfdpr/$n" "$MASTER")"
    # PR's own commits, oldest first, that are NOT already patch-present in BASE
    git cherry "$BASE" "refs/sfdpr/$n" "$base" | awk '/^\+/{print $2}' | while read -r s; do
      printf '%s\t%s\n' "$s" "$n" >> "$ST/sha_pr.tsv"
    done
  done
  # preserve order, drop exact-duplicate SHAs (PRs that share a branch base)
  awk -F'\t' '!seen[$1]++{print $1}' "$ST/sha_pr.tsv" > "$ST/apply.txt"
  echo "plan: $(wc -l < "$ST/apply.txt") commits to apply"
}

prog() { cat "$ST/progress.idx" 2>/dev/null || echo 0; }

do_run() {  # mode: stop (default) or survey
  local mode="${1:-stop}"
  [ -s "$ST/apply.txt" ] || { echo "run 'plan' first"; exit 1; }
  mapfile -t S < "$ST/apply.txt"
  local i; i="$(prog)"; local n=${#S[@]}
  while [ "$i" -lt "$n" ]; do
    local sha="${S[$i]}" pr subj
    pr="$(grep -m1 -F "$sha" "$ST/sha_pr.tsv" | cut -f2)"
    subj="$(git log -1 --format=%s "$sha")"
    if git merge-base --is-ancestor "$sha" HEAD 2>/dev/null; then i=$((i+1)); echo "$i">"$ST/progress.idx"; continue; fi
    git cherry-pick -n "$sha" >/dev/null 2>&1
    # defer autotest changes (rerere/resolutions never needed for tests)
    git checkout HEAD -- "$DEFER" 2>/dev/null
    git diff --cached --name-only --diff-filter=A -- "$DEFER" | xargs -r git rm -f -q 2>/dev/null
    # add any files rerere auto-resolved (no conflict markers left)
    for f in $(git diff --name-only --diff-filter=U -- . ":!$DEFER"); do
      grep -q '^<<<<<<<' "$f" 2>/dev/null || git add "$f"
    done
    local unresolved; unresolved="$(git diff --name-only --diff-filter=U -- . ":!$DEFER")"
    if [ -n "$unresolved" ]; then
      if [ "$mode" = survey ]; then
        echo "CONFLICT|#$pr|$(git rev-parse --short "$sha")|$subj|$(echo $unresolved|tr '\n' ' ')" >> "$ST/conflicts.log"
        for f in $unresolved; do git checkout --ours -- "$f" 2>/dev/null || git rm -f -q "$f"; git add "$f"; done
      else
        echo "== CONFLICT at #$pr  $(git rev-parse --short "$sha")  $subj"
        echo "   files: $unresolved"
        echo "   resolve, 'git add' them, then: git commit -C $sha ; echo $((i+1)) > $ST/progress.idx ; re-run"
        exit 2
      fi
    fi
    if git diff --cached --quiet; then
      git checkout -- . 2>/dev/null; git cherry-pick --quit 2>/dev/null
      echo "skip   #$pr  $subj" >> "$ST/applied.log"
    else
      git commit -C "$sha" -q --no-verify
      git cherry-pick --quit 2>/dev/null
      echo "apply  #$pr  $(git rev-parse --short HEAD)  $subj" >> "$ST/applied.log"
    fi
    i=$((i+1)); echo "$i">"$ST/progress.idx"
  done
  echo "DONE: processed $i/$n"
}

case "${1:-run}" in
  fetch)  do_fetch ;;
  plan)   do_plan ;;
  run)    do_run stop ;;
  survey) : > "$ST/conflicts.log"; do_run survey ;;
  status) echo "progress $(prog)/$(wc -l < "$ST/apply.txt" 2>/dev/null || echo '?'); $(git rev-list --count "$BASE"..HEAD 2>/dev/null) commits on branch" ;;
  *) echo "usage: refresh.sh {fetch|plan|run|survey|status}"; exit 1 ;;
esac
