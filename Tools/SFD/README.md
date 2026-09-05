# SFD branch refresh tooling

Rebuilds the Small Fast Drone feature set on top of a fresh `ArduPilot-4.7`
base by cherry-picking the in-flight upstream PRs (listed in `prs.txt`) in the
same order as the README "Included Features" list.

This is run periodically: the PRs evolve in review, so re-stacking the current
PR heads onto the latest 4.7 both produces an up-to-date branch and surfaces
where our local copy has drifted from - or been superseded by - upstream.

## Process

```sh
# from a checkout sitting on a vanilla ArduPilot-4.7 branch:
Tools/SFD/refresh.sh fetch     # fetch every PR head (pull/N/head)
Tools/SFD/refresh.sh changed   # which PRs (or the base) moved since the last lock
Tools/SFD/refresh.sh plan      # ordered apply list (skips commits already in base)
Tools/SFD/refresh.sh run       # cherry-pick code; stops on each NEW conflict
# ... resolve, git add, git commit -C <sha>, bump .state/progress.idx, re-run ...
./waf configure --board sitl && ./waf copter   # build; fix any master-isms (REFRESH_NOTES)
./waf plane                                    # also build plane - the copter-only
                                               # pass misses plane-only breaks
Tools/SFD/check_param_tables.py                # MANDATORY before any test run: stacked
                                               # PRs collide parameter indices, which
                                               # builds clean and then panics in
                                               # AP_Param before SITL's first heartbeat,
                                               # failing every test. See PR_REVIEW_RULES.
Tools/SFD/refresh.sh tests          # phase 2a: deferred Tools/autotest changes
Tools/SFD/refresh.sh rebuild-tests  # phase 2b: rebuild hot files from PR heads

# after a clean refresh, capture state for next time and commit it:
Tools/SFD/refresh.sh rerere-save    # prune + archive resolutions to rr-cache.tar.gz
Tools/SFD/refresh.sh lock           # record the PR head SHAs to applied.lock
git add Tools/SFD/rr-cache.tar.gz Tools/SFD/applied.lock && git commit -m "Tools: refresh SFD rerere cache + lock"

# finally, move the shipping branch onto the refreshed one:
Tools/SFD/refresh.sh promote SmallFastDrone-4.7.1-beta
```

## Backups

A refresh rebuilds the stack from scratch, so promoting it **rewrites** the branch
it replaces - the old commits are unreachable the moment the branch moves. Never
move a branch by hand; use `promote`, which parks the old tip first:

```sh
Tools/SFD/refresh.sh promote <branch> [source]   # backup + move (source defaults to HEAD)
Tools/SFD/refresh.sh backup  [branch]            # just the backup
```

The backup is `<branch>` with the next free index spliced in before the `-beta`
suffix, so `SmallFastDrone-4.7.1-beta` parks as `SmallFastDrone-4.7.1.1-beta`,
then `.2`, and so on. Indices are compared numerically, so `.10` follows `.9`.
Backups are local branches; pushing one needs the usual `/prepare-for-push` grant.
To undo a promote, point the branch back at its backup.

### Portable rerere + incremental refresh

`run`/`survey`/`tests`/`rebuild-tests` call `ensure_rerere`, which enables rerere
and seeds an empty local cache from the committed `rr-cache.tar.gz`. So recorded
conflict resolutions travel with the repo - a refresh on a different machine
replays them instead of re-resolving from zero (the `.git/rr-cache` is otherwise
machine-local). `rerere-save` prunes unresolved entries, runs `git rerere gc`, and
re-archives the cache to commit; it stays ~2 MB.

There is no commit-level "only re-apply changed PRs" - the stack is linear, so a
change in a mid-stack PR shifts every cherry-pick after it. Instead the refresh is
incremental in *effort*: `run` re-applies everything, but unchanged conflicts
replay from the rerere cache automatically and only genuinely-new conflicts stop
the run. `changed` tells you up front which PRs moved (vs `applied.lock`) so you
know where to look; if it reports nothing moved and the base is unchanged, there
is nothing to refresh. The recurring manual cost that rerere does NOT cover is the
post-merge build fixups (REFRESH_NOTES) - reapply those by hand each rebuild.

`git rerere` is enabled (`rerere.enabled true`), so once a conflict is resolved
its resolution is recorded and replayed automatically on the next refresh -
subsequent runs only stop on genuinely new conflicts.

`Tools/autotest` changes are deferred (dropped per commit) because the SITL
tests collide heavily across PRs and with the base. They are pulled in as a
single batch after the feature code is in place (see "Tests" below).

## Standing notes / divergences

- **#31274 Motortest error rate is superseded upstream.** 4.7 already has the
  feature via `get_raw_rpm_and_error_rate()` and `motors_takeoff_check()` /
  `are_motors_running(..., 1.0f)`. Keep the base version; the PR needs a rebase
  before it is worth carrying. The refresh resolves these conflicts to base.
- VALT (#32270), Throw Mode RPM (#32955) and the local throw-mode work are
  intentionally excluded until their PRs are refreshed.

## Tests (phase 2)

Two steps, run after the code pass and a successful build:

```sh
Tools/SFD/refresh.sh tests          # keep-both for files PRs touch disjointly
Tools/SFD/refresh.sh rebuild-tests  # rebuild the hot files from the PR heads
```

`tests` keeps only the autotest hunks and keeps both sides on collisions - fine
for the files where PRs do not overlap. For the few HOT files where PRs edit the
same registration list / method (`arducopter.py`, `arduplane.py`, `quadplane.py`,
`vehicle_test_suite.py`), keep-both yields invalid Python, so `rebuild-tests`
reconstructs them with `rebuild_testfile.sh`: reset to base, then 3-way merge each
PR's net change to the file in order, auto-resolving the additive conflicts via
`resolve_additive.py` and stopping on the rest for hand resolution. The result
matches the current PR heads (not the loiter branch). See REFRESH_NOTES.md
"Phase 2" for the two manual conflict shapes and the standing validation findings.

## Review rules

`PR_REVIEW_RULES.md` in this directory carries the local review rules that sit
on top of the shared playbook: the refresh sweep for newly opened PRs (which
feeds `prs.txt` and the README), the ../ardupilot-pr-analysis archive currency
check, and vetting review findings against the private flight-test analyses.
Read it before a /pr-review run or a batch refresh.
