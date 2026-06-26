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
Tools/SFD/refresh.sh run       # cherry-pick; stops on each NEW conflict
# ... resolve, git add, git commit -C <sha>, bump .state/progress.idx, re-run ...
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

Deferred autotest changes still need to come in so the branch can be validated.
They are pulled in after the feature cherry-picks; method TBD (most likely a
batch apply of the autotest paths from the integrated loiter branch, since the
PRs' tests collide with each other when applied independently).
