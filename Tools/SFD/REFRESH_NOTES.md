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
  as whole-file additions. Nothing includes them; delete both.
- **resetHeightDatum** - #32768 reaches the filter through master's
  `backends_and_estimates` list. On 4.7, reset the backend that
  `configured_ekf_type()` selects and return its result; the published-location
  refresh (`state.location_ok = _get_location(state.location)`) ports verbatim,
  and master's per-backend estimate re-copy is unnecessary because 4.7's
  `_get_location()` calls `EKF3.getLLH()` directly.
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

2026-09-05 refresh (refresh3): the 49 SFD tests were run. 46 pass, 3 fail.

Two long-standing failures are GONE. ThrowDropSourceSwitch and ThrowModeNoGPS
both pass - the pair this file recorded across two refreshes as hanging on
"Stabilizing throw height" while the vehicle fell to the ground. #32475's
rebuilt head carries the fix, so retire the "ours to fix" entry.
HeightDatumKeptOnMidairRearm's own assertions still pass; only the recovery
tail fails (see below). VibrationRectificationBiasLearning passes with no local
skip.

The three failures:

- **EK3_OptflowAssumeFlatGnd** - NEW, and open. Fails its own first subtest
  (EK3_OPTIONS=0) with "terrain offset did not go stale" after climbing clear
  of a killed rangefinder. The test body is byte-identical to #33585's head and
  #33585's code is present, so the rebuild is faithful. A/B'd against the
  optical-flow FPE guard: it fails identically without the guard, so that fix is
  not the cause. Next step is whether gndOffsetValid stays true because
  activeHgtSource is still RANGEFINDER - #33359 changed that switch to use the
  AGL KF - or whether #33585's head fails its own test after its latest push.
- **HeightDatumKeptOnMidairRearm** - unchanged and still not a regression. The
  PR's own assertions pass; it fails the test's recovery tail, which wants the
  descent arrested above 30 m. 2.1 m this run, against 12.6 / 1.3 / 12.0 / 2.8
  previously. Do not relax the threshold before understanding why the ALT_HOLD
  recovery from a ~17 m/s fall is that slow on 4.7.
- **Scripting6DoFMotors** - removed. Master-only Lua example.

Method notes for the next run:

- Rebuild BOTH vehicles after any shared-library fix. The FPE fix went in with
  `./waf copter` only, so the Plane and QuadPlane steps ran a six-hour-old
  arduplane and the QuadPlane test crashed on the already-fixed bug. Its Plane
  sibling passed on the stale binary purely because the fault is intermittent -
  a pass on a stale binary is not evidence.
- A crashing test costs ~45 minutes of reconnect stall before the harness gives
  up, so a run with several crashers looks wedged when it is merely slow.
  Confirm against the log mtime, not intuition: the per-test buildlog is
  buffered and its mtime lags badly.

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
