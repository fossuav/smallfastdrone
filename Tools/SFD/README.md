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
Tools/SFD/refresh.sh plan      # ordered apply list (skips commits already in base)
Tools/SFD/refresh.sh run       # cherry-pick code; stops on each NEW conflict
# ... resolve, git add, git commit -C <sha>, bump .state/progress.idx, re-run ...
./waf configure --board sitl && ./waf copter   # build; fix any master-isms (REFRESH_NOTES)
./waf plane                                    # also build plane - the copter-only
                                               # pass misses plane-only breaks
Tools/SFD/refresh.sh tests          # phase 2a: deferred Tools/autotest changes
Tools/SFD/refresh.sh rebuild-tests  # phase 2b: rebuild hot files from PR heads
```

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
