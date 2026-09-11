# Refresh findings

Notes from rebuilding the PR stack onto 4.7-beta7 (see refresh.sh / prs.txt).
Re-check these each refresh; most are because a PR was written against master
and master/4.7 have diverged.

## SmallFastDrone-4.7-base (the replay base)

refresh.sh now stacks onto `SmallFastDrone-4.7-base` instead of vanilla 4.7. The
base = `upstream/ArduPilot-4.7` + the 14 merged-upstream PRs + #33115 + the 11
permanent SFD-local hwdef commits + the AP_AHRS 4.7-compat fixup (baked in, so the
replay no longer re-applies it). copter + plane build.

Rebuild the base when 4.7 advances, a baked PR's head moves, or an in-flight PR
merges (promote it in): branch off `upstream/ArduPilot-4.7`, replay the merged-PR
list (30994 31619 32469 32392 32200 32396 32945 31500 32770 32022 32389 32202
32399 32937 29768 32045 32472 33587, then 33115 last), re-apply the AP_AHRS compat
fixup, then cherry-pick the 11 hwdef commits. #31005 is NOT baked (still open
upstream) - it stays in the replay's prs.txt.

Six more merged-upstream PRs were folded onto the branch after the refresh and
are NOT yet in the base: #33780 (IIS2MDC), #33988 (board rotation during gyro
cal), #33990 (DShot GCR quintets), #34122 (NTF units), #34057 (MAG_CAL=7
ground yaw anchor) and #34120 (ICP201XX). Promote them into the merged-PR list
at the next base rebuild, before #33115.

Rebuilt on 2026-09-03 against 4.7.1 (`dbe792162d`). #29768, #32045, #32472 and
#33587 merged upstream since the previous refresh and were promoted into the base;
their PR heads still carry the resolved parameter indices (ACC_ZBIAS_LEARN 23,
GNDEFF_ 24), so baking #32472 ahead of the still-in-flight #32471 does not move
them. 30994 and 32469 now contribute nothing - 4.7 carries them already.

The base also carries the merged PRs' tests: `refresh.sh tests` for the non-hot
files plus `rebuild-tests` for the four hot files (arducopter / arduplane /
quadplane / vehicle_test_suite), all compiling; the suite loads and
EK3_NoGPSLeakWhenNotSource + DynamicNotches pass on the base. One manual case when
re-running rebuild-tests: #30994's quintuple-notch test is already in 4.7, so keep
the `cur` side and add `30994` to its `.applied` sidecar (it modifies an existing
test rather than adding a new method, so the split-method recipe does not apply).

## Reconciled against 4.7's refactors during the merge (verify they still hold)

- **#31274 Motortest error rate** - revived. 4.7 already renamed the getter to
  `get_raw_rpm_and_error_rate()` and refactored the takeoff path into
  `motors_takeoff_check()`, so the PR's own takeoff_check.cpp inline version is
  dropped (keep `ours`). What is carried is the ESC error-rate gate in
  `are_motors_running(..., float max_error_rate)`; the sole caller lives in
  AP_Vehicle.cpp (not takeoff_check.cpp) and is updated to pass `1.0f`. The PR's
  AP_Periph / autotest commits are redundant against 4.7 and skipped. When
  re-running from the PR head, take the gate commits by SHA rather than
  `tail -n +2`, which re-lists the reworked (different patch-id) commits.

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
- **#32972 baro-ground-effect** - the PR had branched from an OLD #32768 and carried
  ~13 duplicate base commits (resetHeightDatum tolerance, the BaroDriftClearedAtArm /
  AmslAltPreserved tests, Plane guards), so it conflicted as a superseded version.
  Fixed the PR by restacking it on the CURRENT #32768 tip with only its 4 own commits
  (suppress ResetHeight during ground effect, negative EK3_GND_EFF_DZ as baro noise
  floor, pre-takeoff baro reference, BaroGroundEffectAtTakeoff test) - the fixed
  pr-baro-gnd-effect is force-pushed. On a from-scratch refresh it now applies clean
  (vanilla -> #32768 -> the 4 commits). Later folded in df8c0dfcc8: restrict the
  spool-up anchor and the ResetHeight suppression to non-fly-forward vehicles
  (assume_zero_sideslip - get_time_flying_ms/takeoff_expected are is_flying() based
  and unsafe in plane flight) and skip the baro innovation floor while the clean
  reference is active so it does not clamp the drift correction. NOTE the current SFD
  branch's #32972 cherry-pick predates the spool-up commits, so it carries them
  locally as ebed712c36 (already plane-safe). On the next refresh that re-pulls
  #32972, drop ebed712c36 - the spool-up plus the guard come from #32972.
- **#32945 getLLH GPS-source guard** - rmackay9's merged fix; supersedes the local
  pr-ekf3-gps-source-leak commit (guards getLLH's three GPS fallbacks on
  pos_from_GPS instead of inside getGPSLLH). Applies clean.

## Post-merge fixups (NOT captured by rerere - reapply each refresh)

These commits apply cleanly (no conflict, so rerere never sees them) but still need
redoing on every refresh. Two kinds, with different long-term homes:

- **4.7-backport artifacts** - the PR's master version is fine; it only breaks when
  cherry-picked onto 4.7 (master-only names). These never go upstream; reapply
  forever (or until the PR is rebased onto 4.7).
- **Genuine upstream bugs** - the PR is wrong on master too (its own test fails
  against its own code). The real home is the PR itself; report/submit there and
  drop the local fixup once the PR head carries it. Reapply locally until then.

### 4.7-backport artifacts

- **AP_AHRS** - master renamed `HAL_NAVEKF[23]_AVAILABLE` to `AP_AHRS_NAVEKF[23]_ENABLED`
  and uses an `ekf3.EKF3` backend accessor. Added compat `#define`s in
  `AP_AHRS_config.h` and rewrote `ekf3.EKF3.*` -> `EKF3.*` (from #32202, #32471).
  The fixup now splits: the `#define`s plus #32202's one call site are baked into
  the base, and the four hover-Z-bias accessors are rewritten after #32471 lands
  in the replay (they do not exist yet when the base is built).
- **AP_AHRS backend headers** - #32768 modifies master's `AP_AHRS_NavEKF2.h` /
  `AP_AHRS_NavEKF3.h`, which 4.7 does not have, so the cherry-pick resolves them
  as whole-file additions. Nothing includes them; delete both - they survived
  the 2026-09-04 refresh and were finally removed on 2026-09-07.
- **resetHeightDatum** - #32768 reaches the filter through master's
  `backends_and_estimates` list. On 4.7 there is no such list and no
  `configured_backend` pointer, so the port is a `configured_ekf_type()` switch:
  the case taken stands in for `has_height_datum()`, the EKFs that were not
  selected follow in an `if (configured_reset || !configured_decides)` block,
  and `ret` reports whether any of them moved. The published-location refresh
  ports verbatim, and master's per-backend estimate re-copy is unnecessary
  because 4.7's `_get_location()` calls `EKF3.getLLH()` directly. Do not port
  `has_height_datum()` onto `AP_AHRS_Backend`: with the shim headers gone
  nothing overrides it and nothing calls it.
- **GPS fix enum** - #32768's `update_home`/`resetHeightDatum` guards use master's
  `AP_GPS_FixType::FIX_3D`. On 4.7 `dal.gps().status()` returns
  `AP_DAL_GPS::GPS_Status`, so use `AP_DAL_GPS::GPS_OK_FIX_3D` (AP_NavEKF2 and
  AP_NavEKF3 both need this).
- **#31274 getter name** - the PR renames `get_raw_rpm()` to take an error rate;
  4.7 already ships that as `get_raw_rpm_and_error_rate()`. Fix the motor_test
  call site, and re-apply the local AP_Vehicle commit that passes `1.0f` (the PR
  updates takeoff_check.cpp, which 4.7 has refactored away).
- **VALT AltHold guard** - #32270 guards on `MODE_ALTHOLD_ENABLED`, which master
  defines and 4.7 does not; AltHold is unconditional here, so drop the guard
  (it trips `-Werror=undef`) and the matching `#endif` in mode_althold.cpp.
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

### Genuine upstream bugs - belong in #32471, reapply locally until fixed there

- **AP_NavEKF3 covariance gates** - FIXED UPSTREAM as of the 2026-09-03 refresh.
  The #32471 head now gates all four covariance-prediction sites in
  `AP_NavEKF3_core.cpp` on `inhibitDelVelBiasStates`, so the local revert is no
  longer needed and was not re-applied. Re-check this if the PR head moves again.
- **autotest VRF skip**. Subtests D/E of `VibrationRectificationBiasLearning`
  assert hover Z-bias learning that the branch was thought unable to deliver. That
  was a misdiagnosis - see the VRF section: with the covariance commit the test
  reaches 0.19 against a 0.01 threshold and passes. The skip is still applied here
  and is probably unnecessary; re-measure and drop it.

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

### Manual cases to expect in rebuild-tests

The 2026-09-03 refresh hit six stops: #30994 (below), #32473, #33484, #33507 and
#33568 in arducopter, and #32768 in quadplane. Three shapes cover them all:

- The PR side is a strict superset of the other two (0 deletions from base, our
  side empty or a fragment). Take the PR side. Check afterwards for a method that
  now appears twice - our copy may have matched into the common region above, in
  which case delete the orphaned head rather than the PR's copy (#33484).
- A registration list where each side adds different entries: union them, dropping
  any entry whose `def` does not exist (master-only tests such as
  `CircuitStatusScript` and `UTMGlobalPosition*`), and dropping our side entirely
  when it merely repeats entries already present above (#32768 in quadplane).
- Our side already carries the PR's addition, so the PR contributes nothing
  (#30994's quintuple-notch test, already in 4.7). Keep `cur` and record the PR in
  the `.applied` sidecar.

Always finish with: py_compile, a duplicate-`def` scan (only `def tests(` should
repeat, once per vehicle class), a dangling-registration scan, and a suite load.

### Older notes on the two original manual cases

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

### Validation

2026-09-05 refresh (refresh4): the 47 SFD tests were run. 45 pass, 2 fail, no
crashes.

The optical flow FPE is fixed UPSTREAM and the local guard is gone. #34292 now
carries `only apply the flow focus height gate to a recalled sample`
(flowDataToFuse && takeOffDetected && tiltOK), so do not re-fold the local
AP_NavEKF3 fix. Its commit message also records why zeroing the struct would
have been the wrong fix: EstimateTerrainOffset would then fuse an invented zero
flow rate, which Copter never trips but Plane does because EK3_FLOW_USE
defaults to 2 there. All four tests it used to crash now pass.

The throw pair stays fixed: ThrowDropSourceSwitch and ThrowModeNoGPS both pass,
as in refresh3.

The two failures:

- **EK3_OptflowAssumeFlatGnd** - still failing, but FURTHER ON than in
  refresh3, so #33585's new head fixed the earlier problem. It used to fail the
  first subtest with "terrain offset did not go stale"; it now reaches
  "Terrain data is preferred and does not need bit 2" and fails with "Terrain
  altitude did not keep EKF relative position valid". Not an environment gap:
  TERRAIN_ENABLE was set and took effect, and the S36E149 tile is present in
  both tilecache and terrain/. This is #33585's own test failing against its own
  code - report it there rather than working around it here.
- **HeightDatumKeptOnMidairRearm** - unchanged, still not a regression. The PR's
  own assertions pass; the test's recovery tail wants the descent arrested above
  30 m and it is not.

Run the set with `Tools/SFD/run_sfd_tests.sh` - it derives the list from the
branch, puts the watch list first and the throw tests last, and `--resume`
picks up an interrupted run instead of starting over.

### #32232 keeps the terrain offset fresh on the ground, forever

`EK3_OptflowAssumeFlatGnd`'s leg "the assumption does not carry over from an
earlier flight" waits on the ground for `EKF_POS_VERT_AGL` to clear after
killing the rangefinder. It never clears on this branch. Master passes.

Cause: #32232's `f82183e6fb` ("AP_NavEKF3: ground clearance fusion fix") added

    } else if (onGround && sensor->status() == OutOfRangeLow) {
        range_distance = rngOnGnd;

(later `!takeOffDetected` instead of `onGround`). A downward rangefinder sitting
on the ground reads below its minimum, so this substitutes the known ground
clearance and fuses it. `EstimateTerrainOffset()` then refreshes
`gndHgtValidTime_ms` on every cycle, and `gndOffsetValid` - which is that
timestamp inside 5 s - never goes stale while the vehicle has not taken off.

It does not distinguish "below minimum because we are sitting on the ground"
from "this sensor is failed". The test kills the rangefinder with
`RNGFND1_MIN` above `RNGFND1_MAX`, so no reading can ever be Good and every one
is OutOfRangeLow - a dead sensor - and the filter keeps fusing a synthetic range
from it indefinitely.

Established by bisect over the 218-commit stack with a minimal reproducer
(take off, land, kill the rangefinder, wait for POS_VERT_AGL to clear): the
bare base is GOOD, `f82183e6fb` is the first BAD commit and its predecessor is
GOOD. Confirmed by disabling only that `else if` at the branch tip, which makes
the offset go stale after 4.8 s and the probe pass. Reverting the *later*
#32232 commit alone does not help - the substitution comes from this one.

Reverting the hunk confirms the substitution is what holds the flag up: delete
the `OutOfRangeLow` branch in `AP_NavEKF3_Measurements.cpp` and
`EK3_OptflowAssumeFlatGnd` passes, all five legs. Restore it and the leg fails
again (measured 2026-09-07).

RESOLVED 2026-09-07, and not the way this section assumed - the leg was at
fault, not either PR. Authoritative account in
`../ardupilot-pr-analysis/33585/`, "Stacked with #32232 the leg failed, and the
leg was the thing at fault". Short version:

- `RNGFND1_MIN` above `RNGFND1_MAX` does not deny the EKF range data. Every
  reading becomes `OutOfRangeLow`, which is still data on a build that
  substitutes a ground clearance for a short one, so the leg could not tell
  "this flight measured no terrain offset" from "this flight measured its own
  ground clearance".
- `kill_rangefinder()` now points the sensor away from `ROTATION_PITCH_270`,
  which `readRangeFinder()` skips whatever the backend reports. The test then
  passes on #33585 alone AND on the stack with #32232 as published, unmodified.
- #32232 does have a defect of its own - with the sensor dead `detectTakeoff()`
  is left with only its gyro criterion, which a SITL climb does not reach, so
  `takeoff_detected` never sets - but this test no longer sees it and that PR
  needs its own coverage. Recorded in `../ardupilot-pr-analysis/32232/`.

Two claims this file carried on 2026-09-07 are WITHDRAWN:

- That `gndOffsetMeasured` re-latching inside the 5 s window was the mechanism.
  It is not. With #32232 as published the flag is held by `gndOffsetValid`, not
  by `flatGroundAssumed()`, so no change to #33585's guard could have made the
  leg pass. Tightening `gndOffsetMeasured` to require the update to postdate the
  flight start was tried here and left the leg failing; the other six flow tests
  stayed green, so it does not over-tighten, it just does nothing. The original
  account above - `gndOffsetValid` never going stale - was right all along.
- That the fix therefore belonged in #33585's guard. The `inFlight &&
  takeOffDetected` bound that followed was withdrawn upstream as well.
  `libraries/AP_NavEKF3/CLAUDE.md` already records both flags as unreliable
  under "Flight-State Flags"; reading it first would have cost nothing.
  `!inFlight` -> `onGround` is what landed instead.

Lesson worth keeping: the subsystem playbook had the answer before the
experiment did.

A probe that looks equivalent and is not, recorded so nobody repeats it:
setting `RNGFND1_MAX = 0.01` to get OutOfRangeHigh instead of OutOfRangeLow
leaves the leg failing, which reads as the substitution being innocent. It is
not. `AP_RangeFinder_Backend::update_status()` is `> max` high, `< min` low,
else Good - and on the ground the analog rangefinder reads about 0, which with
`RNGFND1_MIN = 0` is neither. The sensor stays Good, the `Status::Good` branch
fuses the reading directly, and `gndHgtValidTime_ms` is held fresh by that
instead. The probe swapped the substitution for an equivalent path rather than
removing it. `kill_rangefinder()`'s MIN-above-MAX is what actually forces
OutOfRangeLow.

### The optical flow FPE - found, fixed, and what it cost

Four SFD tests aborted SITL with `ERROR: Floating point exception` -
EKFSourceSetFailsafe, EK3NoAidAccelBiasXY, OpticalFlowGPSLossAiding and
LoiterNoCompassYawGPS. None of them configures optical flow.

Cause: `SelectFlowFusion()` declares `of_elements ofDataDelayed;` as an
uninitialised stack local. `storedOF.recall()` leaves it untouched when no flow
sample sits at the fusion time horizon. Every other read of it is gated on
`flowDataToFuse`; the focus-height check from #34292 was not, so it tested
stack garbage for `minHeight` and multiplied it by the equally stale
`rangeDataDelayed.rng`. SITL traps that and aborts before takeoff. Fixed by
adding the `flowDataToFuse` guard. The bug is in #34292 itself, not the
backport - report it there.

This is the root playbook's rule about stack variables needing explicit
initialisation, and it cost most of a day. What actually found it, and what did
not:

- **Bisection failed twice, and both times produced a confident wrong answer.**
  The fault is intermittent - stack contents vary per run - so a single sample
  per probe is a coin flip. The first bisect blamed a commit that only deletes
  two `@Units` comment lines. The second blamed a real commit and survived
  until the revert test, which is the only reason it did not get reported.
  MEASURE THE RATE before bisecting anything that is not deterministic: at a
  fixed commit this gave CRASH, PASS, PASS.
- **A probe that times out is not a probe that passed.** Both bad bisects rest
  on that confusion. Make the probe report TIMEOUT distinctly and treat it as
  no information.
- **The revert test is what caught the second wrong answer.** After a bisect
  points at a commit, revert it from the tip and re-run before believing it.
- **The backtrace found it in minutes.** `kernel.yama.ptrace_scope` must be 0
  for dumpstack.sh to attach - it works on WSL2 - and `DEBUGINFOD_URLS=` empty,
  or gdb stalls trying to download symbols and the run times out first. The
  stack named `SelectFlowFusion` directly. Do this before bisecting, not after.
- Rates measured with a minimal reproducer (inject SIM_ACC1/2/3_BIAS_X 0.5,
  arm, climb 5 m): branch tip 5 crashes and 2 timeouts in 7, beta 0 in 6,
  fixed tip 4 passes in 4.

### The covariance gates: this file was wrong

An earlier note here said the four covariance-prediction gates were "FIXED
UPSTREAM" and that the local revert "was not re-applied". Both halves are wrong.
The #32471 head still gates all four on `accelBiasLearningInhibited()`, and the
beta DOES carry the revert to `inhibitDelVelBiasStates` (from its commit
"AP_NavEKF3: keep accel-bias covariance alive while learning is inhibited"). So
the branch and the beta differ here. Re-check before assuming either state.

### Running the tests in this environment

Several SFD tests take longer than the agent's own limits allow, and long-lived
background runs get reaped under memory pressure. Run the set from a normal
shell instead:

    python3 .claude/skills/autotest/run_autotest.py --timeout 21600 <steps...>

Deriving the SFD test set: the tests registered on this branch that vanilla 4.7
does not have, plus the handful whose bodies this branch modifies (Clamp,
EK3AccelBias, GPSBlendingAffinity, GyroFFT, ThrowDoubleDrop).

Two rules carried forward from the refresh3 run, both learned by getting them
wrong:

- Rebuild BOTH vehicles after any shared-library fix. The optical-flow FPE fix
  went in with `./waf copter` only, so the Plane and QuadPlane steps ran a
  six-hour-old arduplane; QuadPlane crashed on the already-fixed bug and its
  Plane sibling passed on the stale binary purely because the fault was
  intermittent. A pass on a stale binary is not evidence.
- A crashing test costs about 45 minutes of reconnect stall before the harness
  gives up, so a run with several crashers looks wedged when it is merely slow.
  Confirm against the log, not intuition: the per-test buildlog is buffered and
  its mtime lags badly.

### VibrationRectificationBiasLearning - two #32471 bugs (one fixed, one skipped)

The test failed at subtest D (ACC_ZBIAS_LEARN bit 2): INS_ACC_VRFB_Z stayed ~0.
Subtest A (no bit 2) passes, so basic VRF learning works; bit 2 ("inhibit EKF
learning while disarmed") broke it. Both layers are in #32471 itself - the PR head
reproduces both, so its own test fails against its own code.

1. Covariance-gate over-reach - FIXED (AP_NavEKF3, commit 87b1e2edac). #32471
   implemented bit 2 by switching four covariance-prediction gates in
   AP_NavEKF3_core.cpp from `!inhibitDelVelBiasStates` to
   `!accelBiasLearningInhibited()` (accel-bias process noise, variance
   save/restore, inactive-state zeroing, min-variance safety reset). So while
   bit 2 (or acro) inhibits, the accel-bias covariance collapses with the safety
   reset disabled and never recovers - post-arm Kalman gain ~0. Reverted those
   four to `inhibitDelVelBiasStates`; bit 2 stays only on the fusion gates. No-op
   whenever bit 2/acro is clear. Lifted learning ~50x but not to threshold.

2. Hover observability limit - THIS DIAGNOSIS WAS WRONG. Corrected 2026-09-03.
   The claim was that bit 2 leaves only weak baro-position coupling, so subtests
   D/E could never reach 0.01 from a 30 s hover (~0.0004 in 180 s measured). That
   is not what limits it. Rebasing #32471 onto master and running the test there
   gives 0.189 in the same 30 s hover - the assertion is achievable and the test
   passes.

   The real discriminator is `AP_NavEKF3: keep accel-bias covariance alive while
   learning is inhibited`. With it, subtest D reads 0.18-0.19; without it, 0.000002.
   Measured as a clean A/B on two branches off the same master base differing only
   by that commit (#32471 has it, #32473 does not). The earlier "~50x but not to
   threshold" reading came from a partial version of that fix, and the loiter
   cross-check compared two trees that both lacked the finished one.

   Consequence: **#32473 was stale against #32471** - rebased onto it 2026-09-03.
   That alone was not enough. D then read 0.000097 rather than 0.000002, still
   1400x under subtest A's 0.139 on the same flight profile. The second cause is
   in #32473's own commit: it gated in-flight learning on `takeOffDetected`, which
   is written only by `detectOptFlowTakeoff()` and so stays false for the whole
   flight on any vehicle without optical flow. That made the rangefinder, baro and
   GPS arms of the `heightRefGood` switch unreachable and reduced the gate to
   `onGroundNotMoving`. Subtest A passes either way because it learns on the ground
   before takeoff, so D is the only subtest that exercises the air path.

   Replaced with `!onGround`, which is what the delta velocity bias axis inhibit in
   `CovariancePrediction()` already uses for this same question. D 0.000097 -> 0.178
   against a 0.15 injected bias, A and C unchanged, test passes. `inFlight` also
   measured (D 0.174) and rejected: in the `assume_zero_sideslip()` branch of
   `detectFlight()` it needs GPS ground speed over 5 m/s plus airspeed, height change
   or takeoff_expected, so it never sets on a GPS-denied plane. Only the non fly
   forward branch has the height/rangefinder/time-flying sources, which is why a
   Copter-only test cannot see the difference. On Copter the two are equivalent here
   because `takeoff_expected` latches true for the whole armed-on-ground window, so
   `heightRefGood` is false there for a baro height source either way.

   The local autotest skip of subtests D/E is probably unnecessary on this branch,
   which does carry the covariance commit; re-measure before carrying that skip again.

(An earlier note here blamed two "not patch-present" commits; that was a red
herring - those commits are functionally present, just modified by the AHRS-refactor
resolution so git cherry flags them. The cause was the gate over-reach above.)

## Current state / pick up here

Refreshed 2026-09-04 onto the UNCHANGED `SmallFastDrone-4.7-base` (4.7 had moved
by one AP_HAL_Linux commit; the base was deliberately not rebuilt, so the six
merged-upstream PRs below are still re-folded by hand).

- Branch `SmallFastDrone-4.7.1-refresh2` = `SmallFastDrone-4.7-base` + the 38
  in-flight PRs + the re-folded local work. `./waf copter` and `./waf plane` both
  build.
- No prs.txt PR merged upstream this round; nothing to promote into the base.
- Two PRs joined prs.txt: #32475 (throw mode - now a real PR, see below) and
  #33879 (FFT notch tune persistence, pairs with #34251).
- Phase 1: 13 conflict stops. Recurring shapes were additive parameter blocks and
  the master-vs-4.7 API differences in the backport section above.
- Phase 2: `tests` then `rebuild-tests`. All four hot files compile; every vehicle
  suite loads with no duplicate or dangling registrations (1337 registrations
  across copter/plane/quadplane/heli/sub/rover).
- SITL tests: NOT YET RUN. This is the next step.
- Next:
  1. Run the SFD test set and record the results here.
  2. ThrowDropSourceSwitch / ThrowModeNoGPS: the throw state machine resets out of
     Throw_Wait_Throttle_Unlimited on the freefall check and the vehicle falls to
     the ground. Pre-existing (A/B'd against the pre-refresh branch); it is local
     throw-mode work, so it is ours to fix.
  3. HeightDatumKeptOnMidairRearm: work out why the ALT_HOLD recovery from a 17 m/s
     fall only arrests at 12.6 m against the test's 30 m floor.
  4. Rebuild the DFU bootloader binaries for the boards that gained ENABLE_DFU_BOOT.
  5. `Scripting6DoFMotors` is newly carried (it arrived with a PR-side take and its
     Lua scripts exist in this tree). Drop it if it fails - it is not an SFD test.

### Parameter indices: three PRs now claim var_info2 index 25

#32471 rebased onto master moved ACC_ZBIAS_LEARN from 23 to 25, because master
occupies 21/22/23 with SURFTRAK_GLDST, SURFTRAK_GLSAM and FLIP_ - none of which
exist on 4.7. #32270 (VALT_POS_EXPO) and #32475 (THROW_DROP_AG) independently
claim 25 as well, for the same reason. Only one of the three can have it, so the
stack forces a local renumbering however it is resolved.

Resolved by holding the numbering the shipping beta already uses, so no user's
saved value moves: ACC_ZBIAS_LEARN 23, THROW_DROP_AG 21, VALT_POS_EXPO 29
(GNDEFF_ 24 comes from the merged #32472 and is untouched). Verify after every
replay - all three are local fixups on top of the PR heads, and each PR head will
keep drifting while master's index space fills. Check with a per-group duplicate
scan of ParametersG2 var_info / var_info2, not by eye.

### A duplicate parameter index bricks every test, and only SITL says so

`FLOW_HGT_MIN` (#34292) and `FLOW_HF_RATEF` (#33497) both took AP_OpticalFlow
index 8 - each was the next free one against master, and nothing rejects that
until AP_Param validates the table at boot. The build is clean; SITL dies with
`PANIC: Bad parameter table` before the first heartbeat, so EVERY test fails
with "Did not receive heartbeat" and none of them is really about the parameter.

`AP_Param.cpp` sets `ENABLE_DEBUG 0`, which reduces the panic to that bare
string. Flip it to 1 and rebuild to get the real message
(`Duplicate group idx 8 for _HF_RATEF`), then flip it back.

Better, run `Tools/SFD/check_param_tables.py` after the code pass and before the
test run - it scans every GroupInfo table for duplicate indices, idx >= 64 and
over-long names, and reports all of them at once rather than one boot at a time.
This class of clash will recur on every refresh: each PR picks the next free
index against master, and the stack puts several of them in one table.

The same merge dropped `FLOW_HGT_MIN`'s documentation block inside
`FLOW_OPTIONS`'s, leaving FLOW_OPTIONS with no `@Param` block of its own. Check
that a newly-inserted parameter has not been spliced into its neighbour's docs.

### #32475 throw mode now comes from the PR

The throw work is no longer local: the PR head was rebuilt and pushed, and it
already carries THROW_SRC_SET registration and every THROW_ index under the group
limit. It sits last in prs.txt, where the local stack used to be applied. The two
local fixups (`restore THROW_SRC_SET parameter registration`, `keep throw param
indices within the group limit`) are superseded and must NOT be re-folded; only
THROW_DROP_AG's index is still adjusted (see above). The throw-drop baro de-weight
in `baro_ground_effect.cpp` is still local - re-fold it, and keep #34210's
`vibe_comp_active()` broadening when resolving it.

### The base does NOT carry the hot files' tests

Correcting an earlier claim here. `SmallFastDrone-4.7-base`'s arducopter.py,
arduplane.py, quadplane.py and vehicle_test_suite.py are byte-identical to vanilla
4.7. `rebuild_testfile.sh` resets each hot file to `$SFD_BASE`, which defaults to
`upstream/ArduPilot-4.7` there (refresh.sh defaults it to the SFD base instead), so
every test that lives on the branch rather than in 4.7 has to be re-folded after
`rebuild-tests`. Currently: EK3_NoGPSLeakWhenNotSource, EKFBootstrapReset (three
commits), ScriptingOSD, the MSP VTX suite helpers, arduplane's
EK3HeightDatumResetFlushesBuffers, and the MAG_CAL=7 sub test.

### A merged PR silently deletes its own tests from the rebuild

When a PR in prs.txt merges upstream, its tests move into master and therefore sit
BELOW every PR's `merge-base(pr, master)`. The PR's net change stops adding them,
the reset base (4.7) never had them, and they vanish - while any registration that
came from elsewhere survives and dangles. Seen twice this refresh:

- #32472 merged, so TakeoffGroundEffectAlt / TouchdownGroundEffectAlt disappeared.
- LoiterFlowBrakeOvershoot disappeared the same way; master carries a version of
  it, and #33318's own copy differs, so it was restored from the PR head.

The suite load is what catches this, not py_compile and not a marker scan: a
dangling registration is only an AttributeError at `tests()` time. Always finish
`rebuild-tests` by importing every vehicle module and calling `tests()` on each
class, and diff the resulting `def` set against the previous branch.

### hover_and_check_matched_frequency: a compile-clean runtime break

#34208 makes it keyword-only. The rebuild took the new signature but not the
call-site updates, leaving five positional calls that py_compile accepts and that
raise TypeError the moment the test runs. After any refresh that touches #34208,
walk the AST for positional calls into keyword-only methods.

### Stacked PRs and rebuild-tests

`rebuild_testfile.sh` bases each PR's net change on `merge-base(pr, master)`. For a
PR stacked on another unmerged PR that base predates the parent, so the net change
re-adds the parent's tests and every method lands twice (#32972 on #32768 did this).
The script now walks the already-applied list and prefers the newest applied
ancestor's head as the base. Watch for duplicate `def` names after a rebuild.

## Promoting the refreshed branch

`refresh.sh promote <branch>` backs the old tip up to `<branch>` with the next free
index (`SmallFastDrone-4.7.1-beta` -> `SmallFastDrone-4.7.1.1-beta`) and then moves
the branch. A refresh always rewrites history, so the backup is the only way back -
do not move the branch with `git branch -f` by hand. The follow-up push is a force
push and needs its own grant.

## Folded in from SmallFastDrone-4.7-beta (2026-09-03)

The previous-generation branch had carried work that the 4.7.1 line never picked
up. Comparing the two by commit subject found nine PRs worth taking; they are now
on the branch, the four open ones added to prs.txt and the merged ones listed
above for the next base rebuild. #34120 (ICP201XX) was taken as well: no SFD board
uses that baro today, so it is carried for future hardware rather than for any
current target.

Two adaptations were needed:

- **#34210 land failsafe** conflicts in `baro_ground_effect.cpp`, where it changes
  `vibration_check.high_vibes` to `vibe_comp_active()` (which is
  `high_vibes || forced`). 4.7.1 moved that block into AP_GroundEffect, so keep our
  one-line `gndeff.update()` call and make the same broadening at the library's
  input: `gndeff.set_high_vibrations(vibe_comp_active())`.
- **#34208** makes `hover_and_check_matched_frequency` keyword-only. The keep-both
  test merge keeps both signatures and leaves the older one bodyless; delete it.

Checked and deliberately NOT taken: #33991 (ICM-56686), #33781 (LSM6DSO) and
#31895 (Brahma H7) are for hardware this branch does not enable; #33443 (TBS LUCID H7 AIO) is a
sibling of the board SmallFastDronev1 includes rather than a dependency; #31919
(deferred baro calibration) overlaps the baro path this branch already modifies.

Note `mode_throw.cpp` differs between the two branches only by Unicode-to-ASCII
comment conversion - there is no functional throw gap in the code. The throw
*tests* do differ, and the old branch's versions are the newer ones; see the
Validation section.

## Audit against SmallFastDrone-4.7-beta (2026-09-03)

Compared the 4.7 branch subject-by-subject and then by content. 81 subjects differ,
but nearly all are squashed or renamed equivalents of work this branch carries -
verified by grepping for the feature rather than trusting the subject. Confirmed
present (in several cases this branch is ahead): the EK3_OPTIONS ground-clearance
and height-estimator bits, arm-time height datum reset, IIS2MDC offset
cancellation, the attitude-controller rate-target logging, VTX_TYPES handling, the
SITL gyro rate following INS_GYRO_RATE, EKFC ekf_check logging, the rangefinder
unknown-type guard, the SPI period work, the SFD IMU consistency window, the
TerrainLoiterToCircle bounds, and the ChibiOS pin (identical across both branches
and upstream 4.7).

Throw mode is fully reconciled: mode_throw.cpp differs only by ASCII comment
conversion, every THROW_ parameter is present (indices differ but all stay under
the 64 group limit) and the next-mode whitelist already matches, including ACRO
and VALT. The tests were the whole gap and are now taken from the 4.7 branch.

### The three differences, and what we are doing about them

- **Gyro recalibration in the EKF bootstrap reset** - DELIBERATELY NOT CARRIED.
  The 4.7 branch's `InitialiseFilterBootstrap()` recalibrates the gyros when it
  finds the vehicle stationary on the ground (`calibrate_gyros()`, which blocks and
  needs the vehicle still). It was dropped from #32202 before that PR merged, so
  its absence here is intentional, not an oversight - do not re-add it on a future
  audit. Revisit only if the bootstrap reset misbehaves without it.
- **XKVL logging** - optical-flow control limits (2 files, ~53 lines).
- **XKFR logging** - the rangefinder height-switch decision (4 files, ~81 lines).
  Both are diagnostics for the AGL KF work this branch carries. Left out for now;
  the plan is to offer them upstream as the AGL KF PRs they diagnose merge, rather
  than carry them locally.

### DFU bootloader binaries

Still outstanding, as before. The 4.7 branch has binaries built with DFU enabled
for MatekH743, MambaH743v4, MicoAir743v2, MicoAir743-AIO, TBS_LUCID_H7 and
SmallFastDronev1, and ours differ. Do NOT copy them across: a bootloader that does
not match its own hwdef is a bricking risk, and MicoAir743-AIO's and
SmallFastDronev1's hwdef-bl.dat differ between the branches. Rebuild them here.

## Local work NOT in prs.txt (re-fold after a from-scratch refresh)

A from-scratch refresh rebuilds ONLY the prs.txt stack on a vanilla 4.7 base; the
following live on the branch as local commits and are lost unless re-applied after
the code pass:

- The AP_GroundEffect throw-drop baro de-weight in `baro_ground_effect.cpp`
  (takeoff window asserted post-detection). Throw mode itself is NO LONGER local -
  it comes from #32475 in prs.txt as of the 2026-09-04 refresh.
- 11 per-board SFD hwdef enables + the new SmallFastDronev1 board (SFD-local, no PR):
  MambaH743v4, MatekH743(+bdshot), MicoAir405v2/743v2/743-AIO, BETAFPV-F405,
  BlitzF745(+AIO), ARK_FPV. These are baked into SmallFastDrone-4.7-base, so they
  only need re-applying when the base itself is rebuilt. DFU bootloader binaries for
  the DFU-enabled boards still need rebuilding.
- The SFD README and the whole of Tools/SFD. The base is vanilla 4.7 plus PRs, so
  it carries ArduPilot's own README and no refresh tooling; restore both from the
  previous branch after the code pass.
- The branch-only tests, which `refresh.sh tests` never replays because they are
  not in prs.txt, and which `rebuild-tests` wipes because it resets the hot files
  to vanilla 4.7: EK3_NoGPSLeakWhenNotSource, EKFBootstrapReset (three commits),
  ScriptingOSD, the MSP VTX suite helpers, arduplane's
  EK3HeightDatumResetFlushesBuffers, the MAG_CAL=7 sub test, plus the harness
  adaptations (`adapt two SFD tests to the 4.7 harness`, `use mavutil.location in
  HeightDatumKeptOnMidairRearm`, `adapt the folded-in tests to the 4.7 harness`).
  `register LoiterFlowBrakeOvershoot` still applies; `check the SITL gyro rate`
  and `only compare EKF3 cores while armed` are now redundant - the PR heads carry
  both.
- The VRF assertion skip is NOT re-folded. The covariance fix that made subtests
  D/E unreachable is upstream, so re-measure before ever carrying the skip again.
- The parameter index fixups, which each PR head keeps undoing: ACC_ZBIAS_LEARN
  back to 23 and THROW_DROP_AG back to 21. THROW_SRC_SET registration and the
  THROW_YAW_TYPE/DEG move to 27/28 are no longer needed - #32475's head ships
  both.

Throw-mode RPM (#32955) is still genuinely excluded (pending an updated PR).

Optical flow flat-ground (EK3_OPTIONS bit 5, OptflowAssumeFlatGnd) is #33585,
squashed onto #33478's head (it needs bit 4 to exist) and slotted after #33478 in
prs.txt. The PR is stacked on #33478, so its diff shows the bit-4 commits until
that merges.

VALT (#32270) is no longer excluded - it is in prs.txt in the submitted tier, and
as of the 2026-09-03 refresh the whole feature comes from the PR: its head now
carries the blend commit plus three the branch never had (ground idle at mid-stick,
the take-off test, and bounding the position correction in ground effect). The old
local VALT commits are superseded - do not re-fold them. VALT_POS_EXPO no longer
ships at 29: the 2026-09-04 head moved it to 25, which collides with
ACC_ZBIAS_LEARN, so it is pinned back to 29 locally (see the index section).
The mode_althold.cpp refactor (`alt_hold_run_flying` extracted so ModeVelAltHold can
override it) needs the trailing `#endif // MODE_ALTHOLD_ENABLED` dropped on 4.7.

## Cross-PR check against the flight analyses (2026-09-05)

Every mechanism named in the private topics was checked for presence on the
assembled branch, then the highest-risk clusters were checked for
*reachability* - a mechanism can be present and still be made unreachable by
another PR's gate. Method: pull the parameter and identifier tokens out of each
topic, resolve them against the branch's own generated parameter list
(`param_parse.py`, so names that compose at build time are not false alarms)
and against the source, then read the composite gates where several PRs land in
the same function.

Renames, not losses (topics use the older name):

- `BARO1_THST_FILT` -> `BARO_THST_FILT` (moved to the AP_Baro frontend)
- `EK3_FLOW_MIN_H` -> `FLOW_HGT_MIN` (#34292 moved it to AP_OpticalFlow)
- `TKOFF_GNDEFF_ALT` / `_TMO` -> `GNDEFF_ALT` / `GNDEFF_TMO` (AP_GroundEffect subgroup)
- `PSC_POSZ_P` -> `PSC_D_POS_P` (the NED refactor renamed Z to D)
- `XKF6` -> `XKFA` for the AGL KF log fields (HAgl/VAgl/Bias/Valid)

Deliberately absent, confirmed against the topic text rather than assumed:

- `EK3_RNG_TERR_RT` / `terrRate` - a proposal in the topic's "Risks / validation"
  section, never shipped.
- The ground-effect Z accel-bias inhibit (`zAxisInhibit`) - measured against its
  own absence and removed for doing nothing at `ACC_ZBIAS_LEARN=2`. The EKF3
  playbook section describing it is 4.6-branch history.
- `cb5026417f` (the four CovariancePrediction gates on `inhibitDelVelBiasStates`)
  - dropped after it failed `AccelBiasMovingPlatform`. The branch correctly has
  all four on `accelBiasLearningInhibited()`.

Superseded, so the topic's text is behind the code rather than the code behind
the flights:

- #34210's flat `LAND_FS_THROTTLE_CAP 0.9` is now a PI controller on baro climb
  rate (`LAND_FS_CAP_P/I/I_BAND_MS/MAX`); `LAND_FS_RUNAWAY_CLIMB_M 10` is intact.
- #32768's "reset the datum unconditionally at arming" is now gated on
  `ap.disarmed_in_air`, which is what `HeightDatumKeptOnMidairRearm` covers.

Values that survived the re-stack unchanged: #32475's throw constants (30 deg,
2500 ms, 5 deg, 3.0, 5 m/s, 100 ms), #33484's `FLOW_AXIS_LOCKOUT_MS = 500` (the
flight-informed value, not the PR's 1000), #33318's
`desired_vel_norm * (_brake_accel_mss + drag_decel_mss)`.

### Finding: the rangefinder height switch disables the arm-time datum reset

`EK3_RNG_USE_HGT > 0` with the AGL KF (`EK3_OPTIONS` bit 3) makes the height
source RANGEFINDER *while the vehicle is parked*, and
`NavEKF3_core::resetHeightDatum()` returns false for any source but baro or GPS,
so #32768's arm-time baro-drift reset never runs.

The switch fires on the ground because `selectHeightForFusion()` forces
`terrainStable = true` whenever the AGL KF is valid (`AP_NavEKF3_PosVelFusion.cpp`
~1509), overriding Copter's `terrainHgtStable`, which is otherwise false unless
taking off or landing. With the AGL KF also supplying `heightAboveGnd` and a
fresh `lastAglRngFuseTime_ms`, every term of the switch-on branch
(`belowLowerSwHgt && trustTerrain && prevTnb.c.z >= 0.7f`) holds at rest.

Measured, not inferred. Probe: arm with an analog rangefinder and
`EK3_OPTIONS = 8`, count `EKF_ALT_RESET` (EV id 60, written only when
`resetHeightDatum()` returns true):

| `EK3_RNG_USE_HGT` | EKF_ALT_RESET at arm |
|---|---|
| -1 (default) | 1 |
| 70 | **0** |

Same binary, same arm sequence, one parameter apart.

The consequence is latent rather than immediate: while the rangefinder holds the
height source the drift is not visible (30 s of injected drift left `relative_alt`
at -0.01 m), and it surfaces when the vehicle climbs past the switch ceiling and
falls back to baro carrying drift that was never cleared. The GPS re-anchor in
`resetHeightDatum()` is skipped too, so the reported AMSL keeps the drift.

`ekf3_althold_baro_ge.md` records the accommodation that used to cover this -
allow the reset when `onGroundNotMoving` even if `activeHgtSource` is
RANGEFINDER through `EK3_RNG_USE_HGT` blending, provided the *configured*
primary source is not the rangefinder. That exception is not in the merged code.
It is a one-branch change to the guard in `AP_NavEKF3_PosVelFusion.cpp:373`,
plus a regression test (the probe above is the shape); both belong on #32768,
not on the branch.

No test covers the combination today: `BaroDriftClearedAtArm` runs at the
`EK3_RNG_USE_HGT` default of -1, and the one Copter test that sets the switch
(`EK3_AglKfVelForVelD`) sets it to -1 for an unrelated reason.

## Final audit against SmallFastDrone-4.7.0 (2026-09-06)

Release gate for the 4.7.1 beta: does the re-stacked branch drop anything the
4.7.0 branch carried? Method, so the next reader knows the coverage. Merge base
is `cb872be0ca` (4.7.0-beta8); 4.7.0 is 217 commits past it, this branch 413.
`git cherry` by patch-id flags 154 of 4.7.0's commits as absent, which is
expected - 4.7.0 sits on 4.7.0-beta8 and this branch on 4.7.1, so every rebased
hunk changes its patch-id. Subjects are no better: the re-stack squashes and
renames. What settles it is the feature, so each of the 154 was scored by
sampling its added lines and grepping this tree for them, twice by different
paths (git grep against HEAD, and grep against the working tree). The two runs
agree on all 154 with no divergence above 25 points, and everything either run
scored near zero was then read by hand.

Verdict: nothing needs re-folding. The differences that survive that pass are
the PR forms of the same work. Where this branch and 4.7.0 disagree it is
because the PR shipped differently from the branch-local version it grew out of,
which is the point of re-stacking from prs.txt.

Two whole-branch inventories back that up. Parameters: 25 SFD-added `@Param`
names on each branch, and every 4.7.0-only name is a rename already listed under
"Renames, not losses" above (`FLOW_MIN_H`, `TKOFF_GNDEFF_*`, the `ACC_VRFB_Z`
family, `1_THST_FILT`). Autotests: 25 SFD-added tests on 4.7.0 against 49 here,
with all three 4.7.0-only names renamed and widened (`BaroDriftResetOnArm` ->
`BaroDriftClearedAtArm`, `EK3_FlowMinHeightFloor` -> `OpticalFlowFocusHeight`,
`EK3_AglKfRngHeightSwitch` -> `BaroDriftClearedWithRangefinderHeightSwitch`).

### The one to keep an eye on

The EKF diagnostics. `XKVL` and `XKFR` were already parked above, and the same
reasoning covers `EKFC` (the Copter `ekf_check` decision trace: the filtered
variances the failsafe actually compares, which `XKF4` does not log). All three
are logging only, all three exist on 4.7.0, and all three are the first thing
wanted when a failsafe or an AGL KF height switch has to be diagnosed from a
flight log rather than reasoned about. Offer them upstream with the PRs they
diagnose; carry them locally if a log lands that they would have explained.
`XKF7`'s `FVR` field (`flowVelResetReason`) is the same shape - the rest of the
flow reset-churn logic is here, only the reason code is not.

### Differences that are settled, so a future audit does not re-raise them

- IIS2MDC offset cancellation: this branch carries the merged #33780 form, which
  writes `OFF_CANC`. 4.7.0's local disable (writing `0x00`) is not carried.
- MSP VTX: enabled per board on the H7 targets rather than forced for every
  consumer of `minimize_fpv_osd.inc`, so the 1 MB boards (BlitzF745,
  BlitzF745AIO, BETAFPV-F405, MicoAir405v2) fall back to the
  `HAL_PROGRAM_SIZE_LIMIT_KB > 2048` default. SmallFastDronev1 picks it up
  through TBS_LUCID_H7.
- `INS_HNTC2` notch conversion: #34251 as it now stands stops seeding the
  defaults, and #33879 force-saves the tuned values instead. 4.7.0's
  `set_and_save_by_name` pair was the earlier revision of the same PR.
- The DroneCAN `READING_TYPE_UNDEFINED` guard, the `_read_fifo()` beat delay
  half of the SPI period work, the `TerrainLoiterToCircle` 0.7/1.3 bounds and
  the throw constant unit comments are 4.7.0-local and not carried.
- `EK3_OPTIONS` bits moved: 4.7.0 has AglKfForOptflow at bit 4 and the velD
  fusion at bit 5; here they are bits 3 and 4, with bit 5 now
  OptflowAssumeFlatGnd. `ARMING_CHECK`/`SKIPCHK` LEVEL moved from bit 21 to
  bit 9 with #32391. A 4.7.0 parameter file does not mean the same thing on this
  branch.
- `INDOOR_TUNING_PLAYBOOK.md` is branch-local documentation on 4.7.0 and is not
  in this tree.

This section supersedes the "Confirmed present" list in the 2026-09-03 audit
above, which named several of the items here as already carried.

## Targeted refresh: #32768 and #33585 (2026-09-07)

`refresh.sh fetch` moved two PRs and the base was unchanged, so the stack was
not rebuilt. The README is right that there is no commit-level incremental
path, but that is about replaying prs.txt; when one PR moves and the branch is
otherwise current, its old-head-to-new-head delta lands on the tip as ordinary
commits, the way local work does between refreshes. Rebuild from prs.txt as
usual next time - this is a top-up, not a substitute.

#33585 needed nothing. Both of its new commits are the local ones that were
pushed to it, and both are already patch-present here.

#32768 went from 17 commits to 25 on the same base, so the delta is
`git diff <old head> <new head>` and a `git commit-tree` of the new tree onto
the old head cherry-picks it with proper 3-way semantics. Two of the eight new
commits are the branch's own rangefinder-switch work, now upstreamed, which the
3-way sees as already applied. Six files conflicted, fourteen hunks.

Things worth knowing next time:

- The squashed delta is the wrong tool for `arducopter.py`. It produced one
  ~180-line conflict that aligned the branch's `assert_origin_frame_consistent`
  against master's `UTMGlobalPosition*`, neither of which the delta touches.
  Applying the same delta restricted to that file with `git apply --3way` gave
  16 clean hunks and 4 real conflicts instead.
- Taking "theirs" wholesale on a mis-anchored hunk pulled 165 lines of
  master-only UTM tests onto the branch. `check_test_api.py` caught it through
  the `takeoff(altitude_min=...)` inside them, which is the second time that
  script has paid for itself. Run it even when the change looks confined.
- `BaroDriftClearedWithRangefinderHeightSwitch` no longer sets `EK3_OPTIONS=8`.
  The PR traced the on-ground switch to `AP_AHRS` forwarding `terrainHgtStable`
  on change only, with the first false landing before the cores exist, rather
  than to the AGL KF override this branch also has. Its new precondition check
  fails the test if the switch is not actually active, so a branch where only
  the AGL KF path fires would say so rather than passing hollow.
- A fresh worktree needs `git submodule update --init --recursive` before waf;
  the build fails on `modules/littlefs` and then `modules/lwip`, neither of
  which says anything about the change under test.

Both vehicles build and `check_param_tables.py` and `check_test_api.py` are
clean. The affected cases were run - the BaroDrift family,
`AmslAltPreservedOnRearmAtDifferentElevation`, `HeightDatumKeptOnMidairRearm`,
the two BaroGroundEffect cases, Plane's `EK3HeightDatumResetFlushesBuffers` and
the QuadPlane update_home case - and all 11 pass.

`BaroDriftClearedWithRangefinderHeightSwitch` passing without the local
`EK3_OPTIONS=8` is the answer to the open question above: its precondition
check confirms the rangefinder really is the height source at the arm, so the
PR's account of why the switch fires holds on this branch too.

`AmslAltPreservedOnRearmAtDifferentElevation` failed first time on a flush
race, not on behaviour. It reads the dataflash log while SITL still holds it
open, and the re-arm's `EKF_ALT_RESET` was still buffered, so the count read 1
against the 2 it wants. Measured rather than assumed: the file left behind has
both events and is still missing the final disarm, and a wait before the read
makes it pass with 2. The AMSL assertions the test exists for passed
throughout. The wait is local for now and belongs on #32768.

The full 50-step set then ran: 49 pass, and the one failure is
`EK3_OptflowAssumeFlatGnd`'s "does not carry over from a previous flight" leg.
A/B'd rather than assumed, because this refresh moves `terrainState` inside
`resetHeightDatum()` and that is the same area: the identical failure and
message appear on `SmallFastDrone-4.7.1-beta` with the refresh commits absent.
That leg was since traced to the test itself and fixed on #33585's fork branch
(see the #32232 section above), but the fix is NOT in the PR head this branch
tracks, so the failure stands here until #33585 is pushed and re-picked. The
branch still carries the old `kill_rangefinder()`.

The summariser in `run_sfd_tests.sh` was reporting the wrong reason for that
failure. `grep -c` prints 0 and exits 1 with no matches, so the `|| echo 0`
fallback produced "0\n0" and killed the arithmetic; and the `FAILED:` line it
quotes names the last exception, which after a failure is the teardown reboot
rather than the test. It now also prints the caught exception.

## Lock is behind again (2026-09-08)

`refresh.sh changed` against `applied.lock`:

  #32232: 1f6792c9f8 -> 28cbfe4adf   (squashed head, post-review)
  #27893: b4c8c0e9be -> 5c1bf145dc

Base unchanged. #33585's PR head has NOT moved - its review folds live on the
`pr-optflow-flat-ground` fork branch at `23dfeccb54` and are unpushed, so the
`kill_rangefinder()` fix that makes `EK3_OptflowAssumeFlatGnd` pass is not
fetchable yet. Worth waiting for that push before the next top-up, so the two
land together and the test set comes back clean.

## An interrupted rebase can poison the rerere cache (2026-09-10)

Ctrl+C during a rebase left sixteen zero-length objects in `.git/objects`,
truncated source files in the worktree, and `fatal: bad object HEAD`. That
much is obvious once seen and `git fsck` finds it. The part that is not
obvious, and cost the most time, is what happened next.

The interrupted run had recorded a rerere resolution, and its `postimage`
was written zero-length. Redoing the rebase then printed

    Staged 'libraries/AP_NavEKF3/AP_NavEKF3_OptFlowFusion.cpp' using previous resolution.

and staged git's canonical **empty blob** as the resolution. The conflict
looked resolved, the file was zero bytes, and the failure surfaced hundreds
of lines later as undefined references at link time. An ordinary
`echo > file` in the same directory worked, which rules out the filesystem
and is worth checking early to avoid chasing a disk fault that is not there.

Recipe when a rebase has been interrupted:

```sh
find .git/objects -type f -size 0            # unreadable objects
find .git/rr-cache -type f -size 0           # poisoned resolutions
git fsck --no-progress | grep -iE 'missing|broken|corrupt'
```

Delete the empty objects (they are already unreadable, so nothing is lost)
and delete the individual `rr-cache/<hash>/` directory rather than the whole
cache - the other entries are legitimate. `badTimezone` errors from `fsck`
are ancient upstream commits and are not damage.

Recovery is cheap when the branch also exists on the remote: `git update-ref`
the branch back to the pushed commit and `git checkout -f`. Interrupting a
build or a test run is harmless; it is the git-writing operations that are
not safe to stop.

## #33478 and #33507 now carry the same commit (2026-09-10)

Both branches carry `AP_NavEKF3: only decay AGL KF velocity when the range
finder is absent`, and after the 2026-09-10 rebase the two copies are
**patch-identical** (patch-id `72da83de` on both). That is deliberate: they
fix the same pre-existing defect, and matching patch-ids let git dedupe them
instead of conflicting.

For the refresh it means the second of the two to be cherry-picked produces
an **empty commit**. #33478 comes first in the manifest, so expect
`git cherry-pick` to report nothing to commit when it reaches #33507's copy.
Skip it - do not "resolve" it into a duplicate.

The clean fix is to split that commit into its own small PR that both depend
on, which has not been done.

## The EKF branches are on master, not their old bases (2026-09-10)

#33507, #33478, #34292 and #32972 were rebased onto upstream master on
2026-09-10 - they had been 883, 1353, 114 and 115 commits behind. The
refresh cherry-picks onto `SmallFastDrone-4.7-base`, which is much older, so
these now travel further than they used to and may conflict where they
previously did not.

That rebase caught one real breakage worth expecting again: `takeoff()` was
renamed on master, `alt_min` becoming `altitude_min`, and #33507's autotest
died on it. `user_takeoff()` kept its own `alt_min`, so the rename is
narrower than a blanket search suggests.

## AI review state before the next refresh (2026-09-11)

`Tools/SFD/ai_review_status.py` reports the automated dev-call review for every
manifest PR: which round is latest, the head it was taken at, and whether the PR
has been pushed since. A review that no longer matches the head says nothing
about the code the refresh is about to cherry-pick, so it counts as absent.

State at this sweep: 11 current, 5 stale, 26 never reviewed. Four of the five
stale ones went stale in the 2026-09-10 rebase wave. #32972 survives only because
the bot re-verified the newer head mid-round; #34292 was re-verified at
`946708d630` and has moved again since.

The 28 open PRs with no current review were labelled `AIReview` to queue a round.
#33484 and #34292 already carry the label with a stale review, so re-triggering
them needs something other than adding it again.

#27893 merged upstream on 2026-09-08 (`99d4933d58`). It stays in `prs.txt`: it
merged to master, and `SmallFastDrone-4.7-base` is at 2026-06-27, so the merge is
not reachable from the base and dropping the line would lose the fast-rate
foundation. It can only be promoted into the base once 4.7 carries it.
`applied.lock` still pins the pre-merge head `b4c8c0e9be`; the real head is
`5c1bf145dc`.

## #34305 stays out of the manifest (2026-09-11)

#34305 "do not test flow data the terrain estimator does not have" is open,
AI-reviewed and has an entry in ../ardupilot-pr-analysis, which makes it look
like a sweep omission every time. It is deliberately not part of the SFD feature
set - recorded here so the next sweep stops rediscovering it.

## Where the AI-review pass stopped (2026-09-11)

Run `Tools/SFD/ai_review_status.py` first - it regenerates everything below
and the table in the README's upstreaming section. Nothing here needs to be
trusted from memory.

State at the end of the session: **9 current, 7 stale, 26 never reviewed.**
#32232 and #33585 moved from current to stale in the session itself, because
pushing to them is what invalidates the review that was taken at the old head.
That is expected, not a regression - but it means both now need a fresh round
before the next refresh stacks them.

Pushed:

    origin          SmallFastDrone-4.7.1-beta   25f3b86e88
    rishabsingh3003 ek3_gnd_clear (#32232)      8bec444e50
    andyp1per       pr-optflow-flat-ground      36f86f06eb  (#33585)

### Owed on #32232, in order

1. **Post the refutation.** The review's safety-critical finding was refuted
   from the archive's measured indoor leg and no reply exists on the PR, so a
   reader sees REQUEST CHANGES with two commits that do not address it. Cite
   the evidence as "flight and SITL tests show the estimate tracked 10.3 m
   against 10.2 m with the range finder as the only vertical observation" -
   never the private repo or its file names.
2. **Re-run the five-leg set.** The 20 Hz stream rate was being lost at every
   reboot, so legs 3, 4 and 5 were sampled at 5 Hz. The archive's recorded
   numbers for those legs are of uncertain provenance until re-run at the new
   head; see the 2026-09-11 section in `../ardupilot-pr-analysis/32232/`.

### Owed on #33585

Two BUG findings are untouched: the datum reset not carrying the measured
terrain AGL (`ResetPositionD()`), and the height-timeout path bypassing the
same carry (`ResetHeight()`). Both are reset-path changes in code whose own
history on this PR includes three fixes withdrawn across rounds five to
seven, so measure before patching. The one-line fix that did go in is
tier 3 - established by reading the write gate at the base, not by running
anything - and log308's replay is now two heads behind.

### Not opened at all

#32471, #33484 and #34292 are the remaining REQUEST CHANGES PRs. Worktrees
already exist for the last two (`../pr-33484`, `../rev-34292`), so they are
cheap to pick up. #34292's review is the one claiming a live bug rather than
hygiene: the terrain withhold bypassed when range data arrives in the same
cycle.

### Two traps worth knowing before the next session

- **A worktree does not inherit submodules.** A fresh `git worktree add`
  needs `git submodule update --init --recursive` or `./waf copter` dies on
  `modules/littlefs/lfs.c`, then again in `dronecangen`. Both failures print
  through a pipe with exit 0, so check the exit code rather than the tail.
- **Amending is gated** by `pre_bash_check.py`, and minting the grant is the
  user's act. Rebuilding the commit instead works with allowed operations
  only: save the diff, `git checkout -B tmp HEAD~1`, re-apply, re-commit with
  the right message, `git branch -f <branch> HEAD`. Used twice this session,
  once to fix a mangled subject and once to remove a commit SHA that did not
  exist. Note that the same hook also matches the literal command text inside
  a heredoc, so a note describing it has to paraphrase.

## Corrections to the pick-up section above (2026-09-11, later)

Three of its pointers were wrong, and each cost time before being caught.

- **"#32232 ... refuted from the archive's measured indoor leg"** reads as if
  no archive entry existed for it. `../ardupilot-pr-analysis/32232/` did exist
  and is thorough; the local clone was simply behind. The 10.3 m against
  10.2 m figure the section quotes is real and lives in that entry, at the
  pre-squash head. **Fetch the archive before concluding an entry is missing** -
  the currency check in `PR_REVIEW_RULES.md` is worthless against a stale clone,
  and a missing directory looks exactly like a never-written one.
- **"no reply exists on the PR"** for #32232 was already false when it was
  written: the reply went up at 12:15Z and the note was committed at 13:02Z.
- **"Worktrees already exist for the last two (`../pr-33484`, `../rev-34292`)"**
  - neither exists. Nothing on this machine has them. Budget the clone and the
  submodule init.
- **"#32471, #33484 and #34292 are the remaining REQUEST CHANGES PRs"** - #33484
  is `REVIEW_REQUIRED` on GitHub. Its REQUEST CHANGES is the automated round's
  own verdict, which is not a GitHub review state. `ai_review_status.py` reports
  the bot's verdict; `gh pr view --json reviewDecision` reports the human one.
  They disagree often enough that a sweep should carry both.

The general shape: a pick-up note written at the end of a session records what
the session believed, and some of that is already stale by the time it is
committed. Re-derive each pointer before acting on it, exactly as the root
playbook says about a claim marked checked.

## The base branch was never advanced to the 2026-09-03 rebuild (2026-09-11)

`changed` opened the 2026-09-11 refresh with "base SmallFastDrone-4.7-base:
CHANGED 1bf6b3ddc0->99414094f6 (full rebuild)". That reads as the base having
moved forward. It had moved **backward by three months**.

- `99414094f6`, what the branch pointed at: commit date 2026-06-27, does not
  contain 4.7.1 `dbe792162d`.
- `1bf6b3ddc0`, what `applied.lock` records and the shipping branch descends
  from: commit date **2026-09-03**, contains 4.7.1. This is the rebuild the
  "Rebuilt on 2026-09-03 against 4.7.1" paragraph at the top of this file
  describes.

The rebuild was done and the beta was built on it; only the branch pointer was
left behind on the June base. `git branch -a --contains 1bf6b3ddc0` finds it in
`SmallFastDrone-4.7.1-beta` and three origin branches but **not** in
`SmallFastDrone-4.7-base`, and the base branch's reflog goes back to "Created
from upstream/ArduPilot-4.7" with only commits appended - it was never reset,
so it had always been the June lineage.

Cost of not catching it: 68 files, including all three `AP_GroundEffect` files
and five upstream boards with their bootloaders (ARKV6S, BCubeF745v2,
CORVON743V2, ORBITH743v2, TBS_LUCID_H7). The refresh would have produced a
branch with no ground effect at all, and #34362 could not have applied.

Fixed with `refresh.sh promote SmallFastDrone-4.7-base 1bf6b3ddc0`, which parked
the June tip as `SmallFastDrone-4.7-base.1`. `origin/SmallFastDrone-4.7-base`
still carries the old history, so that push is a force push and has not been
done.

**Author dates are what made this hard to see.** Both candidates showed
2026-06-27 under `git log -1 --format=%ad`, because a base built by
cherry-picking keeps the original author dates. Only `%cd` separates them. When
checking which of two bases is newer, use the commit date, or better, test for
a known-recent commit: `git merge-base --is-ancestor dbe792162d <base>`.

Cheap check to run before every refresh, which would have caught this in one
line:

    git merge-base --is-ancestor "$(awk '/^BASE/{print $2}' Tools/SFD/applied.lock)" \
        SmallFastDrone-4.7-base || echo "base branch does not contain the locked base"

The plan size is the other tell: 397 commits to apply on the wrong base against
249 on the right one, because the correct base already carries the merged PRs.
A plan that suddenly grows by half is a base problem, not a PR problem.

## Resolving a conflict from the previous branch: two guards, not one (2026-09-11)

Most refresh conflicts are the same shape - the PR is written against master,
4.7 differs, and the branch being replaced already carries a resolution that
built and passed tests. Taking that resolution is the right instinct and
`Tools/SFD/resolve_from_branch.py` automates it, but it is only sound under
**two** conditions. Missing the second one produced a guaranteed build break
within minutes of the script being written.

1. **No commit still to be applied touches the file.** Otherwise the branch's
   copy is a state from the future and taking it silently skips those commits.
2. **No PR that touches the file has moved since `applied.lock`.** The branch
   was built from the locked heads. For a PR pushed since, the branch's copy is
   *stale*, and taking it reverts whatever that PR changed.

Guard 2 is the one that is easy to miss, because the file looks fine. #32232
renamed `takeOffDetected` to `movedSinceArming` after the lock. Taking the
branch's `AP_NavEKF3_VehicleStatus.cpp` while resolving a #33484 conflict put
the old name back into a file whose header now declares only the new one -
three uses of a member that does not exist. Nothing in the conflict hinted at
it: the rename was nowhere near the conflicting hunk.

The tell, if you are resolving by hand: the branch's version of the region
disagrees with the tree in a way the current commit does not explain. Stop and
check `applied.lock` against `refs/sfdpr/<pr>` before trusting it.

`changed` already prints which PRs moved, and that list is exactly the set that
makes the branch stale. At this refresh it was #32232, #32972, #27893, #33359,
#33478, #33585, #33484, #34292, #33507 and #34208, plus the four new ones - so
the branch was an unsafe source for most of the EKF tree and a safe one for
files only #32768 touches.

To recover after taking a stale file: `git checkout HEAD -- <path>` to get the
pre-cherry-pick version, then `git show <sha> -- <path> | git apply -3` to
re-raise the real conflict and resolve it by hand. `git checkout --merge` does
not work once the path has been `git add`ed, because that collapses the stages.

### The other shape: commits that arrive already applied

Several #32972 commits came out **empty** this refresh, because #32768 is
applied first and its later commits supersede them - `resetHeightDatum()`
returning bool, the extracted `reset_height_datum()` helper in
`AP_Arming_Copter.cpp`, `baroHgtOffsetNeedsInit` replacing
`baroHgtOffset = 0.0f`. The two PRs overlap heavily and three commits even
share a subject line. For those, keep HEAD, confirm `git diff --cached` is
empty, and bump `progress.idx` without committing.

Check before assuming: `git show <sha> --stat` and confirm every addition is
already in the tree. `8f7487dfa5` looked like a no-op and was one, but it is
named "refresh on-ground references" and it would have been easy to drop
`vertCompFiltState.pos` with it.

## HeightDatumKeptOnMidairRearm: three 4.7 adaptations (2026-09-11)

The test lives on #32768 and its body is written against master. Carry the
PR head's version and re-apply these three, rather than keeping a local
rewrite - the 2026-09-11 refresh produced a stale 150 m copy because a
diff3 resolution took the branch's older version over the PR's, and the
result failed its own 30 m assertion at 12.1 m.

    takeoff(250, mode='GUIDED', altitude_max=260, timeout=180)
      -> takeoff(250, mode='GUIDED', max_err=10, timeout=180)

Copter's takeoff() on 4.7 has `max_err`, a tolerance, where master has
`altitude_max`, a ceiling: the bound is `alt_min + max_err`, so a 260 m
ceiling on a 250 m takeoff is `max_err=10`.

    ground_amsl_m = start.get_alt_m(AltFrame.ABSOLUTE)   -> start.alt
    Location(lat, lng, alt, AltFrame.ABSOLUTE)           -> mavutil.location(lat, lng, alt, 0)

`sitl_start_location()` returns a `mavutil.location`, which has only
lat/lng/alt/heading. 4.7 does have its own `Location` class with
`get_alt_m()` (vehicle_test_suite.py), which is why a grep for the method
finds it and suggests the call is fine - it is the receiver that is wrong.

Both of the last two sit *after* the 30 m assertion, so they are only
reached once that passes. That is the shape check_test_api.py does not
cover: it checks helper names, keyword names and script names, not methods
called on an object a helper returned.

## Refresh of 2026-09-11: outcome

Branch `SmallFastDrone-4.7.1-refresh5`, 241 commits on the base. Copter,
plane and heli build. The SFD set is green: **60 of 60**, 0 crashes, about
seven minutes end to end - the 6-8 hour budget elsewhere in this file is for
runs with a crasher in them, which cost ~45 minutes each in reconnect stall.

The base was the story. See "The base branch was never advanced to the
2026-09-03 rebuild": `changed` reported a full rebuild moving forward when the
branch had moved three months *back*, and refreshing onto it would have
dropped all of AP_GroundEffect. The plan went from 397 commits to 249 once it
was pointed at the right commit, which is the tell to remember.

19 conflict stops in the code pass, mostly #32768/#32972 overlap - the two
share three commit subjects and six of #32972's commits arrived already
applied and committed empty. Genuine 4.7 work: `resetHeightDatum()` returning
bool via `configured_ekf_type()`, and `AP_GPS_FixType::FIX_3D` ->
`AP_DAL_GPS::GPS_OK_FIX_3D`.

Four build breaks after the code pass, all master-isms the PR heads carry:
`ekf3.EKF3` for 4.7's `EKF3`, `takeOffDetected` for `movedSinceArming`
(#32232 renamed it after the other PRs were written), `zeroStatesVarCov()`
for `zeroRows()`/`zeroCols()`, and a VALT guard on `MODE_ALTHOLD_ENABLED`
which 4.7 does not define. Plus the standing index collision:
ACC_ZBIAS_LEARN and THROW_DROP_AG both took 25 again, held at 23 and 21.

### Three new helper scripts, and the two that were wrong first

- `resolve_from_branch.py` - takes the shipped branch's resolution for a
  conflicted file. Two guards, and the second is the one that bites; see
  "Resolving a conflict from the previous branch: two guards, not one".
- `resolve_hotfile.py` - the rebuild-tests recipes. It grew three corrections
  in one session, each from a rule that was right for the file in front of it
  and wrong for the next: duplicate detection must be qualified by enclosing
  class (it deleted 29 classes from vehicle_test_suite.py before that);
  `self.X,` is only a registration inside a `tests*()` body; and a def may
  only be inserted from a PR head when the PR's **own diff** adds it, because
  a PR head contains the whole of master.
- `resolve_registration_union.py` - the registration-list union alone.

### What the gates caught that nothing else would have

`check_test_api.py` found nine `takeoff()` calls using master's
`altitude_min`/`altitude_max`. On 4.7 `max_err` is a tolerance and the ceiling
is `alt_min + max_err`, so `altitude_max=6` on a `takeoff(4, ...)` is
`max_err=2`, not a rename.

The suite load found a dangling `SITLGyroRate` and 17 duplicate registrations.
Neither is visible to py_compile.

The def-set diff against the previous branch is useful but **not** a
specification: it reported `EK3_PerCoreOptflowLogging` missing, and restoring
it reintroduced a test #34363 had already replaced with `EK3_PerCoreLogging`.
Missing from the branch is not the same as wanted, because the branch is stale
wherever a PR has moved.

### Owed

- `HeightDatumKeptOnMidairRearm` is the only test that needed a real answer,
  and it was the test at fault, not the code. See the adaptations section
  above; the ordering fix went to #32768 as `0ca1c9e775`.
- That push makes #32768's dev-call APPROVE stale. Four PRs now need a fresh
  round before the next refresh: #32232, #33585 and #32768 all went stale by
  being pushed to.
- The rr-cache is 13M against the ~2M the README quotes. Plausible after a
  refresh with a 938-line conflict in it, but it is committed every time.
