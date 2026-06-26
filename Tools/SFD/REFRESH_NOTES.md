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
- **ArduPlane** - #32768's `update_home()` block in `commands.cpp` used master's
  `AP_GPS_FixType::FIX_3D`; on 4.7 `gps.status()` returns `AP_GPS::GPS_Status`, so
  it was changed to `AP_GPS::GPS_OK_FIX_3D` to match the line just below it. The
  copter-only code pass missed this - **build plane as well** (`./waf plane`) so
  plane-only breaks surface before the test pass.

## Phase 2 - tests

Two steps:

1. `refresh.sh tests` - replays the commit list keeping ONLY the autotest hunks
   and keeps both sides on test-vs-test collisions. Fine for the files where PRs
   touch disjoint code, which is most of them.
2. `refresh.sh rebuild-tests` - rebuilds the few HOT files where PRs edit the same
   registration list / method, so keep-both yields invalid Python. These are
   `arducopter.py`, `arduplane.py`, `quadplane.py`, `vehicle_test_suite.py`.

`rebuild-tests` does NOT use the loiter branch. The earlier loiter-scaffold
fallback asserted loiter-era semantics and drifted from the PRs as they evolved -
`EK3_FlowAxisLockoutRecovery` failed against the current #33484 code purely
because of that. Instead `rebuild_testfile.sh` resets each hot file to base and
whole-file 3-way merges each PR's net change (`merge-base..head`) in order;
`resolve_additive.py` clears the additive conflicts; the rest stop for hand
resolution. Every reconstructed test body is then byte-identical to the PR head
it came from (verify with a per-method diff against `refs/sfdpr/<n>`).

### Two manual cases to expect in rebuild-tests

- 4.7 lacks a master test that a PR's diff context includes (e.g. arduplane's
  `UTMGlobalPosition*`, arducopter's `Scripting6DoFMotors`). `resolve_additive.py`
  handles these: it keeps only the PR's own addition and drops the master-only
  entries 4.7 does not carry.
- diff3 mis-alignment: when a PR adds a method right where 4.7 already has one with
  a similar docstring, the merge can split 4.7's method - def+docstring inside the
  markers, body left in the common region below. This happened with #33507 (vs
  `EK3_FlowAxisLockoutRecovery`) and #33568 (vs `LoiterFlowBrakeOvershoot`).
  Recipe: resolve the conflict to OUR side only (reconnects 4.7's method to its
  body), then insert the PR's new method verbatim from `refs/sfdpr/<n>` just before
  4.7's method, and add the one registration line. Confirm with py_compile and a
  no-dangling-registration check (a `self.X,` whose `def X` you dropped).

### Validation (last refresh)

Copter: 14 of 15 reconstructed tests pass, including the previously-failing
`EK3_FlowAxisLockoutRecovery` (the point of the rebuild). The one Copter failure,
`VibrationRectificationBiasLearning`, is a real code-integration gap - see below.

Plane/QuadPlane: `AmslAltPreservedAfterUpdateHomeAtDifferentElevation` passes.
`EK3HeightDatumResetFlushesBuffers` is flaky, not regressed: its #32770 threshold
is 0.1 m and the post-reset transient sits right on it (0.073 m pass, 0.107 m fail
across runs). Both its test body and #32770's code (`3e3a57d7d1`) are faithful to
the PR head; the threshold is just tight under SITL variance.

### Open: VibrationRectificationBiasLearning fails (#32471 code gap)

`test.Copter.VibrationRectificationBiasLearning` fails deterministically:
`INS_ACC_VRFB_Z should be non-zero with bitmask 7, got 0.000002`. The test is
byte-identical to #32471's head, so it is faithful; the VRF hover Z-bias feature
just is not learning on our branch. The Copter learning glue is present
(`Attitude.cpp` update/save_hover_bias_learning) but its per-IMU loop is gated on
`ahrs.get_accel_bias_z_for_imu()` and uses the EKF accel-Z bias. Two #32471
commits are NOT patch-present on our branch (altered during the code-pass AHRS
refactor resolution):

    AP_NavEKF3: add hover Z-bias correction for vibration rectification
    Copter:     add hover Z-bias learning for vibration rectification

so the EKF side the learning depends on is incompletely integrated. Fix is in the
feature code (re-apply those two against the current AHRS), not the test. Until
then this test is expected red.

## Current state / pick up here

- Branch `SmallFastDrone-4.7.1-beta` = beta7 + the full stack; `./waf copter` and
  `./waf plane` both build.
- Phase 1 (feature code) complete; conflict resolutions recorded by rerere.
- Phase 2 tests: the hot files are now rebuilt from the PR heads (not loiter) via
  `refresh.sh rebuild-tests`; every reconstructed body is byte-identical to its
  PR. Suite loads, SITL runs, 16 of 18 reconstructed tests pass.
- Next:
  1. Fix the #32471 VRF code gap (re-apply the two hover Z-bias commits against
     the current AHRS) so `VibrationRectificationBiasLearning` goes green - the
     test is correct, the feature is not learning. See the section above.
  2. Re-apply the build fixups (incl. the new ArduPlane one) on a from-scratch
     refresh - rerere does not cover them.
  3. Fold in VALT (#32270) and throw-mode (#32955 + local) once their PRs rebase;
     their loiter-local tests (EK3_AglKfRngHeightSwitch, EKFSourceSetFailsafe,
     Throw*, ModeVAltHold) were dropped because no current PR carries them.

## Excluded (pending updated PRs)

- VALT (#32270), throw-mode RPM (#32955) and the local throw-mode work.
