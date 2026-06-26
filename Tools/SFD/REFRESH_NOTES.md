# Refresh findings

Notes from rebuilding the PR stack onto 4.7-beta7 (see refresh.sh / prs.txt).
Re-check these each refresh; most are because a PR was written against master
and master/4.7 have diverged.

## Superseded upstream - needs a PR rebase before it is worth carrying

- **#31274 Motortest error rate** - 4.7 already implements this differently
  (`get_raw_rpm_and_error_rate()`, `motors_takeoff_check()` /
  `are_motors_running(..., 1.0f)`). The PR was dropped entirely. Rebase it on
  current upstream or retire it.

## Divergences resolved during the merge (verify they still hold)

- **#32238 FAST_BOOT / esc_calibration** - kept 4.7's brushed-only skip (the
  DSHOT skip was reverted upstream in #32353) and added only the FAST_BOOT
  early-return.
- **#32471 / #32472 parameter index clash** - both grabbed ParametersG2 index
  23. Kept `ACC_ZBIAS_LEARN`=23; moved `TKOFF_GNDEFF_ALT`->24, `TKOFF_GNDEFF_TMO`
  ->25 (later migrated into the `AP_GroundEffect` GNDEFF_ subgroup at 24).
- **#32768 AHRS resetHeightDatum** - 4.7 lacks the master "backends_and_estimates"
  AHRS refactor, so the `origin_alt_tolerance_m` arg was threaded through 4.7's
  explicit `EKF2/EKF3/sim` calls. Plane home-reset kept 4.7's `AP_GPS::GPS_OK_FIX_3D`
  enum and dropped a master-only FBWB_CLIMB_RATE param conversion.
- **#33543 loaded-defaults count** - dropped the `purge_defaults_list_overrides()`
  call (master-only function, absent in 4.7); kept the `num_param_overrides = idx`
  fix that is the point of the PR.
- **#33569 FLOW_GAIN_H** - the PR detunes against raw `terrainState - position`;
  4.7 already has the AGL-KF-aware `heightAboveGndEst`. Merged the tunable
  `_flowNavGainHgt` onto 4.7's better height (taking the PR verbatim would have
  regressed AGL-KF awareness).
- **#33484 option-bit doc** - kept Bit 4 (velD) description from #33478 and applied
  the PR's en-dash -> hyphen ASCII fix.

## Post-merge build fixups (NOT captured by rerere - reapply each refresh)

These commits apply cleanly but do not compile on 4.7 because they use master-only
names. rerere only replays *conflict* resolutions, so these must be redone (or the
PRs rebased) on every refresh:

- **AP_AHRS** - master renamed `HAL_NAVEKF[23]_AVAILABLE` to `AP_AHRS_NAVEKF[23]_ENABLED`
  and uses an `ekf3.EKF3` backend accessor. Added compat `#define`s in
  `AP_AHRS_config.h` and rewrote `ekf3.EKF3.*` -> `EKF3.*` (from #32202, #32471).
- **AP_NavEKF3** - `ResetVelocityToFlow` used master's combined `zeroStatesVarCov()`;
  4.7 has `zeroRows`/`zeroCols` (from #33484).
- **ArduCopter** - a 3-way merge artifact dropped master's surface-tracking
  `get_pilot_speed_*_adjusted_ms()` into #32471's hover-bias commit; 4.7 has no
  declarations or callers, so they were removed.

## Phase 2 - tests (`refresh.sh tests`)

The code pass (`run`) drops every `Tools/autotest` change so the SITL tests do not
collide commit-by-commit. `refresh.sh tests` replays the same commit list keeping
ONLY the autotest hunks and auto-resolves test-vs-test collisions by keeping both
sides.

Reality of the last run: ~34 test commits came in; 13 keep-both collisions in 4
files (`arducopter.py` x10, `vehicle_test_suite.py`, `arduplane.py`, `quadplane.py`).
Keep-both produces invalid Python where two PRs edit the same registration list or
redefine the same method, so those 4 files were replaced with the integrated copies
from the loiter branch:

    git checkout SmallFastDrone-4.7-beta-loiter -- Tools/autotest/arducopter.py \
        Tools/autotest/vehicle_test_suite.py Tools/autotest/arduplane.py \
        Tools/autotest/quadplane.py

After that the suite imports and all the reconstructed-feature tests are present
(EK3_FlowAxisLockoutRecovery, EK3_FlowMinHeightFloor, EK3_AglKfVelForVelD,
TakeoffGroundEffectAlt, LoiterFlowBrakeOvershoot, BaroDriftResetOnArm, ...).

### Validation finding (important)

`test.Copter.EK3_FlowAxisLockoutRecovery` FAILS with "flow vel reset fired without
the AGL KF gate". This is NOT a reconstruction bug - the gate is correct in the
rebuilt code (`AP_NavEKF3_OptFlowFusion.cpp` ~824-828 requires
`AglKfForOptflow && aglKfValid`, and the reset + its GCS message only fire inside
that block). It is a loiter-test-vs-current-PR-code mismatch: the loiter test file
asserts loiter-era semantics, and #33484 evolved during review. This is the cost of
using loiter's test files for the conflicted merges.

To validate faithfully, the conflicted test files need the PRs' OWN current test
versions merged (the per-PR test-conflict resolution we skipped), not loiter's. The
cleanly-applied (non-conflicted) PR tests and the feature tests that match the
rebuilt code can be run now; the loiter-sourced files are scaffolding.

## Current state / pick up here

- Branch `SmallFastDrone-4.7.1-beta` = beta7 + the full stack; `./waf copter` builds.
- Phase 1 (feature code) complete; all conflict resolutions recorded by rerere.
- Phase 2 (tests) brought in; 4 files are loiter copies (scaffold), suite loads,
  SITL runs.
- Next:
  1. Do the proper per-PR test merge for `arducopter.py` / `vehicle_test_suite.py`
     so tests match the rebuilt code (replaces the loiter scaffold).
  2. Run the matching feature autotests to get green validations.
  3. Re-apply the build fixups above (they are committed here but a from-scratch
     refresh re-introduces the master-isms - rerere does not cover them).
  4. Then fold in VALT (#32270) and throw-mode (#32955 + local) once their PRs
     are rebased.

## Excluded (pending updated PRs)

- VALT (#32270), throw-mode RPM (#32955) and the local throw-mode work.
