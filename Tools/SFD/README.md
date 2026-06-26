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
Tools/SFD/refresh.sh tests     # phase 2: bring deferred Tools/autotest changes in
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

`refresh.sh tests` brings the deferred `Tools/autotest` changes in (inverse of the
code pass - keeps only autotest hunks, keep-both on collisions). Run it after the
code pass and a successful build. Keep-both breaks the few files where PRs edit the
same registration list/method; those get the integrated loiter copy as scaffold.
See REFRESH_NOTES.md "Phase 2" for the exact steps, the known flow-lockout
test-vs-code mismatch, and what is left to do.
