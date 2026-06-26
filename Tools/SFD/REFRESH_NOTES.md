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

## Still to do

- Tests are deferred (Tools/autotest dropped per commit). Pull them in as a batch.
- VALT (#32270) and throw-mode (#32955 + local) excluded pending updated PRs.
