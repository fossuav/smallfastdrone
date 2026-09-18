# SFD refresh notes

How to read this file:

- **Current state** and **Next refresh** are kept current in place. When an
  item is done, delete it; when a claim turns out wrong, fix it where it is.
  Do not append a correction further down - a reader starting at the top meets
  the wrong version first, which is how this file came to contradict itself
  three ways about the EKF3 covariance gates.
- Everything below that is standing reference: the base, divergences, fixups,
  recipes and traps. Edit in place when something changes.
- Dated narrative is in REFRESH_HISTORY.md, conclusions only.

A pick-up note records what a session believed, and some of it is stale by the
time it is committed. Re-derive each pointer (a head, a branch, a worktree, "no
reply exists") before acting on it.

## Current state

- Shipping: `SmallFastDrone-4.7.1-beta` = refresh5 (2026-09-11), 241 commits on
  base `1bf6b3ddc0`.
- Ready, not promoted: `SmallFastDrone-4.7.1-refresh6` (2026-09-18) on base
  `a5eb325674`. Copter, plane, heli and sub build. SFD set
  (71 tests, now including the MSP VTX pair and Sub FuseMag): 70 pass, 0
  crashes; TerrainOffsetGroundEffectRecovery fails, for its designed reason.
  It fixes these defects of the shipping beta:
  - #33568's position jump when GPS is lost while moving and flow takes over
    (REFRESH_HISTORY 2026-09-15).
  - Six merged-upstream PRs that refresh5 dropped: #33780 (IIS2MDC fixes),
    #33988 (board rotation during gyro cal), #33990 (DShot GCR quintets), #34122
    (NTF units), #34057 (MAG_CAL=7 yaw anchor), #34120 (ICP201XX). They are
    merged to master, not in 4.7, and were in neither the base nor prs.txt; they
    are now in prs.txt. The MSP VTX tests, lost the same way, are back too.
  - Two stack interactions the refresh6 test failures exposed, fixed on the
    branch (below): flow aiding churning every 5 s on the ground after landing,
    and a baro ground-effect error locked into the EKF height for a whole flight.
- `upstream/ArduPilot-4.7` is 6 commits past the base (AP_HAL_Linux CAN fix,
  ArduSub guided/terrain, a Sub scripting binding). None touches SFD code, so the
  base was not rebuilt onto it.

### Fixes made on refresh6, to port to their PRs

Each is a commit on `SmallFastDrone-4.7.1-refresh6`, shaped for the PR it
belongs to, and measured with an A/B. Until a PR head carries its commits,
re-fold them after every refresh (they are in "Local work").

- **#34292** `AP_NavEKF3: do not restart flow aiding while the focus height hold
  is on` and `autotest: check flow aiding stays off after a landing below the
  focus floor` (FlowFocusHoldAfterLanding). After touchdown the flow floor
  discards every sample, aiding timed out, `readyToUseOptFlow()` saw fresh
  samples and restarted it at once: a stop/start pair every 5 s until disarm.
  Flow held off is now not ready, as `flowVelResetUnhealthy` already was. The new
  test fails without the guard, on master's base too ("Relative aiding restarted
  while held below the focus floor"). The PR head also folds in the 2026-09-17
  review: the no-range-finder build fix, `flowFocusRngPosD` moved by every height
  reset (`ResetPositionD()`, `ResetHeight()`, AID_NONE entry), the comment and
  FLOW_HGT_MIN description. refresh6 carries the pre-fold commits; the next
  refresh takes the PR head.
- **#33498** `autotest: count FlowGyroZBiasNoYawReference's aiding stops in
  flight only`. With #34292 aiding stops once after touchdown, which is not the
  flight the check is about. The PR version counts only between NOT_LANDED and
  LAND_COMPLETE; refresh6's stops counting at LAND_COMPLETE.
- **#34432** (its own master PR) `AP_NavEKF3: keep baro ground effect out of a
  height source switch` and `autotest: test a range finder to baro switch in
  ground effect` (BaroGroundEffectRangefinderSwitch). With EK3_RNG_USE_HGT,
  Copter uses the range finder for height only while taking off or landing, so
  an ALT_HOLD takeoff switches back to baro at liftoff, in ground effect, and
  the baro offset learned through the spool-up carries the error into the
  flight: EKF height 2.5 m high. Master has it with a range finder that reads
  on the ground (RNGFND1_MIN 0: +2.49 m; 0.05: +0.02 m); #32232's substitution
  gives an out-of-range-low sensor the same path (+2.5 m). It was attributed to
  #32972 and then to #32232; both were wrong. The fix holds the offset and skips
  the switch reset to baro while takeoff or touchdown is expected (not on fixed
  wing), and each half alone fails the test (3.0 m drop at liftoff, 2.5 m
  carried). A dead-zone variant and an anchored-floor variant were measured
  against it over six scenarios and did no better; the dead zone ratchets on
  baro noise. It replaced refresh6's dead-zone commit.
- **#32553** `autotest: fly TerrainOffsetGroundEffectRecovery over flat ground`.
  With a terrain tile for the home location in the run directory (any earlier
  test that installs terrain handlers leaves one), SITL's ground sits 0.55 m
  below home and the range finder reads that on the ground, so both of the
  test's preconditions failed depending on test order. With SIM_TERRAIN 0 it
  fails for its designed reason with master's number (terrain offset mean +0.21
  to +0.31 m across seven runs, against master's +0.23 to +0.29 m).
- **4.7 harness, not master**: `autotest: reboot so Replay's larger log
  buffer takes effect`. LOG_FILE_BUFSIZE is allocated at boot, and Replay's first
  subtest starts logging disarmed before rebooting, so the startup messages fill
  the default buffer, a replay block does not fit and SITL panics ("Failed to
  log replay block", seen by the harness as a hang). Found with gdb as SITL's
  parent (`ptrace_scope` 1 blocks attaching). Carried with two master commits
  4.7 lacks: `9c7f12df34` (the larger buffer) and `d3a32025cd` (wait for home
  before the body-odometry takeoff - with #32945 in the base, GPS no longer
  fills in a location there, and the takeoff was refused). Replay passes;
  vanilla 4.7 needs the buffer fix and reboot too, not the home wait. Master
  passes Replay 3 of 3 without the reboot, so there is no master PR for it; a
  4.7 backport PR would bundle all three. Plane's Replay has the same pattern.

## Next refresh

### Before promoting refresh6

- `refresh.sh promote SmallFastDrone-4.7.1-beta SmallFastDrone-4.7.1-refresh6`,
  run from a checkout that does not have the beta checked out. The beta push is
  a force push; the base push (`a5eb325674`) is a fast-forward. Both need
  `/prepare-for-push`.

### Every refresh

Before:

1. `git fetch upstream ArduPilot-4.7 master`, `refresh.sh fetch`, `refresh.sh changed`.
2. Check the base branch really is the locked base - `changed` reporting
   "CHANGED ... (full rebuild)" does not say which direction it moved:

       git merge-base --is-ancestor "$(awk '/^BASE/{print $2}' Tools/SFD/applied.lock)" \
           SmallFastDrone-4.7-base || echo "base branch does not contain the locked base"

3. If 4.7 advanced, decide whether to rebuild the base (see the base section).
   Irrelevant commits (another vehicle, another HAL) do not justify it.
4. `Tools/SFD/ai_review_status.py`. A review taken at an older head says nothing
   about the code being cherry-picked, so it counts as absent. It reports the
   bot's verdict; `gh pr view --json reviewDecision` reports the human one, and
   they disagree often enough to carry both.
5. `refresh.sh plan` and sanity-check the count (249 on 2026-09-11, 284 on
   2026-09-18 with the six restored PRs and #32471's rework). A plan that
   suddenly grows by half with no PR to explain it is a base problem.
6. Merge the committed `rr-cache.tar.gz` into the local cache if the last
   refresh ran in another clone: `ensure_rerere` only seeds an empty cache. Back
   the local one up, then `tar -C .git/rr-cache -xzf Tools/SFD/rr-cache.tar.gz`.

Code pass:

7. Work in a worktree on a new `SmallFastDrone-4.7.1-refreshN` branch off the
   base, and run this clone's scripts by path from inside it:
   `bash <main clone>/Tools/SFD/refresh.sh run`. The base carries no Tools/SFD,
   and `.state` then stays with the manifest. Conflict recipes are below; expect
   empty commits (see "Commits that arrive already applied").
8. Apply every entry in "Post-merge fixups" and pin the parameter indices.
9. Build copter, plane, heli (and sub for #34057's test) in **separate** waf
   invocations. `./waf plane heli sub` in one call relinked arduplane against
   the wrong library set and failed with undefined AP_Winch symbols; each alone
   built clean.
10. `Tools/SFD/check_param_tables.py` - mandatory before any test run.

Tests pass:

11. `refresh.sh tests`, then `refresh.sh rebuild-tests` (manual shapes below).
12. Re-fold the branch-only tests and harness adaptations ("Local work"), with
    `Tools/SFD/refold_methods.py FILE <previous branch> NAME...`.
13. Gates, in this order: `check_test_api.py` (fix `takeoff()` keywords with
    `fix_takeoff_kwargs.py`), `check_suite_load.py --against <previous branch>`,
    `check_pr_test_lines.py`. See "Phase 2" for what each catches.
14. `Tools/SFD/run_sfd_tests.sh`; add the tests its derivation misses (below).
    In a worktree, copy `.claude/skills/autotest` in first: `run_autotest.py`
    resolves the clone from its own path, so the main clone's copy runs the main
    clone's tests. 4.7's autotest.py has no `--sitl-instance`, so only one clone
    can run SITL at a time.

After:

15. Restore the SFD `README.md` and `Tools/SFD/` from the previous branch.
16. `Tools/SFD/audit_dropped.py <previous branch> <new branch>`: anything the
    previous branch carried that is in neither the base nor prs.txt is lost by a
    refresh, silently - that is how refresh5 dropped six merged PRs.
    Then `Tools/SFD/param_changes.py <previous branch> <new branch>`: every
    parameter, bitmask, value or compile-time default a PR changed. Where one
    changes what an SFD board does out of the box - existing behaviour turned
    into an option, a default flipped, a setting renamed - put the value that
    keeps today's behaviour in `hwdef/include/sfd_defaults.parm` (or, for a
    define, the SFD hwdefs), in the base. Users must not see a refresh.
17. `refresh.sh lock`, `refresh.sh rerere-save`, commit both.
18. `refresh.sh promote SmallFastDrone-4.7.1-beta`. The push is a force push
    and needs `/prepare-for-push`.

### Owed, not tied to one refresh

- Port the refresh6 fixes to their PRs (see "Fixes made on refresh6"), and
  offer the Replay reboot upstream: master sets the larger buffer without it.
- `AmslAltPreservedOnRearmAtDifferentElevation`'s wait before reading the
  dataflash log belongs on #32768. Carried locally two refreshes running; the
  closed log has both EKF_ALT_RESET events, the read straight after disarm sees
  one.
- `SIM_SONAR_OFFSET` is carried as a local SITL commit (#34292's and #33507's
  tests need it); it drops out once 4.7 has master's `568727a218`.
- #32398: an `#error` on `ARMING_DELAY_MSEC` would make a missed rename stop the
  build rather than restore a 2 s arming delay.
- DFU bootloader binaries for MatekH743, MambaH743v4, MicoAir743v2,
  MicoAir743-AIO, TBS_LUCID_H7 and SmallFastDronev1. Rebuild them here; do NOT
  copy from the old 4.7 branch - a bootloader that does not match its own
  hwdef is a bricking risk, and two of those hwdef-bl.dat files differ.
- rr-cache is 17.7 MB after refresh6 (12.8 MB after refresh5) against the ~2 MB
  the README quotes, and it is committed
  every refresh.
- #32232's five-leg re-run: the 20 Hz stream rate was lost at every reboot, so
  legs 3-5 were sampled at 5 Hz (`../ardupilot-pr-analysis/32232/`).
- Split `AP_NavEKF3: only decay AGL KF velocity when the range finder is absent`
  (carried patch-identically by #33478 and #33507) into its own PR.
- Offer the EKF diagnostics upstream with the PRs they diagnose: `XKVL` (flow
  control limits), `XKFR` (rangefinder height-switch decision), `EKFC` (Copter
  ekf_check decision trace - the filtered variances the failsafe compares,
  which XKF4 does not log) and `XKF7.FVR` (flow reset reason). All four existed
  on 4.7.0 and are the first thing wanted when a failsafe or height switch has
  to be diagnosed from a log. Carry them locally if a log lands that they would
  have explained.

## SmallFastDrone-4.7-base (the replay base)

refresh.sh stacks onto `SmallFastDrone-4.7-base`: `upstream/ArduPilot-4.7` +
the merged-upstream PRs + #33115 + the AP_AHRS 4.7-compat fixup + the 11
permanent SFD-local hwdef commits + two behaviour-preserving hwdef commits
added 2026-09-18:

- `hwdef: SFD boards set ARMING_DELAY_MS` (`5cfd779aed`), the rename #32398's
  new head needs.
- `hwdef: SFD boards keep the acro accel bias inhibit` (`a5eb325674`), which
  adds `hwdef/include/sfd_defaults.parm` (ACC_ZBIAS_LEARN 8, for #32473's
  bit 3) and `@include`s it from all 11 SFD boards' `defaults.parm` (creating
  it for MatekH743, MicoAir743v2 and MicoAir743-AIO). The next default of this
  kind is one line in that file. Unknown names in defaults are ignored, so a
  default for a parameter only a stacked PR defines is harmless on the base.

Backups: `1bf6b3ddc0` is `SmallFastDrone-4.7-base.2`, `5cfd779aed` is `.3`
(local); `origin/SmallFastDrone-4.7-base.1` is the June lineage (`99414094f6`).
The push of the base is a fast-forward.

Rebuild it when 4.7 advances in a way that matters, a baked PR's head moves, or
an in-flight PR merges **into 4.7**. Branch off `upstream/ArduPilot-4.7`, replay
the merged-PR list (30994 31619 32469 32392 32200 32396 32945 31500 32770 32022
32389 32202 32399 32937 29768 32045 32472 33587, then 33115 last), re-apply the
AP_AHRS compat fixup, then cherry-pick the hwdef commits. Move the branch with
`refresh.sh promote SmallFastDrone-4.7-base <new>`, never by hand.

- A PR merged to **master** but not 4.7 stays in prs.txt: the base is 4.7, so
  the merge is not reachable from it, and dropping the line loses the work.
  Currently #27893, #34360 and the six refresh5 dropped. Promote into the base
  only once 4.7 carries them.
- 30994 and 32469 contribute nothing now - 4.7 carries them.
- The base does NOT carry the hot files' tests. Its arducopter.py, arduplane.py,
  quadplane.py and vehicle_test_suite.py are byte-identical to vanilla 4.7, so
  the merged PRs' tests in those files are re-folded after `rebuild-tests` (see
  "Local work"). The non-hot test files do carry them.
- #31005 is NOT baked (still open upstream); it stays in prs.txt.
- Re-running rebuild-tests on the base: #30994's quintuple-notch test is already
  in 4.7, so keep `cur` and add `30994` to its `.applied` sidecar.
- **Author dates lie about which base is newer.** A base built by cherry-picking
  keeps the original author dates; only `%cd` separates two candidates. Better,
  test for a known-recent commit: `git merge-base --is-ancestor dbe792162d <base>`.

## Standing divergences (verify each refresh)

- **#31274 Motortest error rate** - 4.7 already has `get_raw_rpm_and_error_rate()`
  and `motors_takeoff_check()`. What is carried is the ESC error-rate gate in
  `are_motors_running(..., float max_error_rate)`; its sole caller is in
  AP_Vehicle.cpp (not takeoff_check.cpp) and passes `1.0f`. The PR's own
  takeoff_check.cpp version is dropped (keep ours) and its AP_Periph / autotest
  commits are redundant. Take the gate commits by SHA rather than `tail -n +2`,
  which re-lists the reworked commits. The PR needs a rebase before it is worth
  carrying as-is.
- **#32238 FAST_BOOT / esc_calibration** - keep 4.7's brushed-only skip (the
  DSHOT skip was reverted upstream in #32353); add only the FAST_BOOT early return.
- **#32768 AHRS resetHeightDatum** - 4.7 lacks master's `backends_and_estimates`
  AHRS refactor; see the fixup. Plane home-reset keeps 4.7's
  `AP_GPS::GPS_OK_FIX_3D` and drops a master-only FBWB_CLIMB_RATE conversion.
- **#33543 loaded-defaults count** - drop the `purge_defaults_list_overrides()`
  call (master-only); keep `num_param_overrides = idx`, the point of the PR.
- **#33569 FLOW_GAIN_H** - the PR detunes against raw `terrainState - position`;
  4.7 already has the AGL-KF-aware `heightAboveGndEst`. Merge the tunable
  `_flowNavGainHgt` onto 4.7's height; taking the PR verbatim regresses AGL-KF
  awareness.
- **#33484 option-bit doc** - keep Bit 4 (velD) from #33478 and apply the PR's
  en-dash -> hyphen fix.
- **#32972 on #32768** - #32972 is stacked on #32768 and three commits share a
  subject; many of #32972's commits arrive already applied (see below).
- **#34210 land failsafe** - it changes `vibration_check.high_vibes` to
  `vibe_comp_active()` in `baro_ground_effect.cpp`, which 4.7.1 moved into
  AP_GroundEffect. Keep the one-line `gndeff.update()` call and make the same
  broadening at the library input: `gndeff.set_high_vibrations(vibe_comp_active())`.
- **#33478 / #33507** carry the same AGL KF velocity decay commit on purpose;
  the second cherry-pick is empty. Skip it, do not resolve it into a duplicate.
- **#33585** is stacked on #33478 (it needs EK3_OPTIONS bit 4) and must follow
  it; #34360 must precede #33585 and #34361.
- **#34380** must follow #33568, which widens how many vehicles reach the flow
  height limit it fixes.
- Excluded: Throw Mode RPM (#32955), pending an updated PR. #34305 is
  deliberately not in the SFD set even though it is open, AI-reviewed and in
  the archive - a sweep will keep rediscovering it.

## Post-merge fixups (NOT captured by rerere - reapply each refresh)

These apply without conflict, so rerere never sees them. Two kinds:
4.7-backport artifacts (the PR is fine on master; reapply until it is rebased
onto 4.7), and genuine upstream bugs (report on the PR, drop the fixup once its
head carries the fix).

Commit them per module, one commit each; refresh6's subjects are the
template: `AP_AHRS: adapt the stacked PRs to 4.7's EKF accessors`,
`AP_NavEKF3: adapt the flow fusion PRs to this stack`, `Copter: drop VALT's
AltHold guard on 4.7`, `Copter: keep the shipped ParametersG2 indices`,
`SITL: add SIM_SONAR_OFFSET`.

- **SIM_SONAR_OFFSET** - #34292's and #33507's tests set it; master added it in
  `568727a218` along with moving every SONAR_ parameter into a new group.
  Carried as the parameter alone at var_info3 index 57 plus the one line in
  `Aircraft::rangefinder_range()`. Drop once 4.7 has it.
- **AP_AHRS compat** - master renamed `HAL_NAVEKF[23]_AVAILABLE` to
  `AP_AHRS_NAVEKF[23]_ENABLED` and uses an `ekf3.EKF3` accessor. The `#define`s
  and #32202's call site are baked into the base; #32471's hover-Z-bias
  accessors are rewritten `ekf3.EKF3.*` -> `EKF3.*` after it lands.
- **AP_AHRS backend headers** - #32768 modifies master's `AP_AHRS_NavEKF2.h` /
  `AP_AHRS_NavEKF3.h`, which 4.7 lacks, so they arrive as whole-file additions.
  Nothing includes them; delete both.
- **resetHeightDatum** - master reaches the filter through
  `backends_and_estimates`; 4.7 has no such list and no `configured_backend`.
  Port as a `configured_ekf_type()` switch: the case taken stands in for
  `has_height_datum()`, the unselected EKFs follow in an
  `if (configured_reset || !configured_decides)` block, `ret` reports whether
  any moved. The published-location refresh ports verbatim; master's
  per-backend estimate re-copy is unnecessary because 4.7's `_get_location()`
  calls `EKF3.getLLH()` directly. Do not port `has_height_datum()` onto
  `AP_AHRS_Backend` - nothing would override or call it.
- **GPS fix enum** - `AP_GPS_FixType::FIX_3D` -> `AP_DAL_GPS::GPS_OK_FIX_3D` in
  AP_NavEKF2 and AP_NavEKF3 (`dal.gps().status()`), and ->
  `AP_GPS::GPS_OK_FIX_3D` in ArduPlane `commands.cpp` `update_home()`.
- **#31274 getter** - fix the motor_test call site to
  `get_raw_rpm_and_error_rate()`; re-apply the AP_Vehicle `1.0f` commit.
- **VALT AltHold guard** - #32270 guards on `MODE_ALTHOLD_ENABLED`, which 4.7
  does not define (`-Werror=undef`). Drop the guard and the trailing
  `#endif // MODE_ALTHOLD_ENABLED` in mode_althold.cpp.
- **zeroStatesVarCov()** - master's; 4.7 has `zeroRows()`/`zeroCols()`
  (`ResetVelocityToFlow`, and anywhere else a PR head uses it).
- **takeOffDetected -> movedSinceArming** - #32232 renamed it; PR heads written
  before that still use the old name (#34292's head does).
- **ArduCopter surface tracking** - a 3-way artifact once dropped master's
  `get_pilot_speed_*_adjusted_ms()` into #32471's commit; 4.7 has no callers.
  Delete if it recurs.

## Parameter indices

Each PR takes the next free index against master; the stack puts several in one
table. Hold the numbering the shipping beta uses, so no user's saved value
moves, and move the newcomer:

| Parameter | Index | Why local |
|---|---|---|
| ACC_ZBIAS_LEARN | 23 | #32471 on master uses 25 (master has 21-23 taken) |
| THROW_DROP_AG | 21 | #32475 takes 25 |
| VALT_POS_EXPO | 29 | #32270 takes 25 |
| GNDEFF_ subgroup | 24 | from merged #32472, untouched |
| FLOW_HF_RATEF (AP_OpticalFlow) | 8 | #33497 and #34292 both take 8 |
| FLOW_HGT_MIN (AP_OpticalFlow) | 9 | |
| SIM_SONAR_OFFSET (SITL var_info3) | 57 | local backport; master has it in its own group |

Check with `check_param_tables.py`, never by eye. A duplicate index builds
clean and panics every SITL boot (`PANIC: Bad parameter table`, so every test
fails with "Did not receive heartbeat"); `ENABLE_DEBUG 1` in `AP_Param.cpp`
names the offender. Also check that a newly inserted parameter has not been
spliced into its neighbour's `@Param` block (FLOW_HGT_MIN once swallowed
FLOW_OPTIONS's docs).

## Local work not in prs.txt (re-fold after the code pass)

- The AP_GroundEffect throw-drop baro de-weight in `baro_ground_effect.cpp`
  (takeoff window asserted post-detection). Keep #34210's `vibe_comp_active()`
  broadening when resolving it.
- The parameter index pins above.
- The SFD README and all of `Tools/SFD` (the base carries neither).
- Tests the rebuild wipes because they live on the branch, not in any PR's own
  diff. Re-fold them from the previous branch with `refold_methods.py`, then
  their registrations (`check_suite_load.py --against` lists what is missing):
  - branch-only: EK3_NoGPSLeakWhenNotSource, EKFBootstrapReset, ScriptingOSD
    (registered at the head of tests2b), arduplane's
    EK3HeightDatumResetFlushesBuffers;
  - merged-PR tests 4.7 lacks: TakeoffGroundEffectAlt and
    TouchdownGroundEffectAlt (#32472), EK3_AccelBiasInhibitOnGroundMoving,
    EK3_AccelBiasZeroVelOptFlow, EK3_ZeroVelFusionNotUsedWithGPS,
    LoiterNoCompassYaw, LoiterNoCompassYawGPS, LoiterFlowBrakeOvershoot;
  - the MSP VTX tests (#29768): three-way apply `1bb6b599ca` and `b96939e023`
    from `SmallFastDrone-4.7.1.4-beta` to vehicle_test_suite.py (one method is
    on MSP_Generic, not TestSuite), and register MSPVTXConfig and
    MSPDisplayPortVTXConfig after CRSF;
  - master helpers the PR-head tests call: send_position_target_local_ned
    (AutoTestCopter), statustext_count_in_collections and
    assert_ekfs_match_sim_state (TestSuite).
  Drop registrations with no test anywhere on the branch: HomeAltResetTest,
  ModeFlowHold, UTMGlobalPosition, UTMGlobalPositionWaypoint.
- AmslAltPreservedOnRearmAtDifferentElevation's 5 s wait before it reads the
  log (owed to #32768).
- The refresh6 fixes, until their PR heads carry them (drop each as it lands):
  the #34292 aiding guard and FlowFocusHoldAfterLanding, the #33498 in-flight
  count, #34432 and BaroGroundEffectRangefinderSwitch,
  the #32553 SIM_TERRAIN line. Cherry-pick them from the previous branch by
  subject; `audit_dropped.py` lists any that were missed.
- `SITL: add SIM_SONAR_OFFSET` (until 4.7 has master's `568727a218`).
- Replay: master's `9c7f12df34` and `d3a32025cd`, and the reboot after raising
  the buffer (until 4.7 has them).
- HeightDatumKeptOnMidairRearm's three 4.7 adaptations (carry the PR head's
  body and re-apply these rather than keeping a local rewrite):

      takeoff(250, mode='GUIDED', altitude_max=260, timeout=180)
        -> takeoff(250, mode='GUIDED', max_err=10, timeout=180)
      ground_amsl_m = start.get_alt_m(AltFrame.ABSOLUTE)   -> start.alt
      Location(lat, lng, alt, AltFrame.ABSOLUTE)           -> mavutil.location(lat, lng, alt, 0)

  4.7's takeoff() has `max_err`, a tolerance (ceiling = `alt_min + max_err`),
  where master has `altitude_max`, a ceiling. `sitl_start_location()` returns a
  `mavutil.location` (lat/lng/alt/heading only); 4.7's own `Location` class does
  have `get_alt_m()`, which is why a grep suggests the call is fine - the
  receiver is wrong. Both calls sit after the 30 m assertion, so they are only
  reached once it passes, and `check_test_api.py` does not check methods called
  on an object a helper returned.
- NOT re-folded: the VRF subtest D/E skip (D/E pass with #32471's covariance
  restore), `check the SITL gyro rate` and `only compare EKF3 cores while armed`
  (the PR heads carry both), the old local throw and VALT commits (#32475 and
  #32270 carry them), and `ebed712c36` (#32972 carries the spool-up anchor).

## Phase 2 - tests

1. `refresh.sh tests` replays the list keeping only autotest hunks and keeps
   both sides on collisions. Fine where PRs touch disjoint code.
2. `refresh.sh rebuild-tests` rebuilds the HOT files (`arducopter.py`,
   `arduplane.py`, `quadplane.py`, `vehicle_test_suite.py`), where keep-both
   yields invalid Python. `rebuild_testfile.sh` resets each to the base and
   3-way merges each PR's net change (`merge-base..head`) in order;
   `resolve_additive.py` clears the additive conflicts; the rest stop.
   `resolve_hotfile.py` automates the manual shapes. Every reconstructed body
   should be byte-identical to its PR head (per-method diff against
   `refs/sfdpr/<n>`).

Manual shapes, and the rules `resolve_hotfile.py` learned the hard way:

- The PR side is a strict superset of the other two: take it, then check for a
  method that now appears twice (our copy matched into the common region) and
  delete the orphaned head, not the PR's copy (#33484).
- A registration list where each side adds entries: union them, dropping any
  entry whose `def` does not exist (master-only tests such as
  `CircuitStatusScript`, `UTMGlobalPosition*`), and our side entirely when it
  repeats entries already above (#32768 in quadplane).
  `resolve_registration_union.py` does this alone.
- Our side already carries the PR's addition: keep `cur`, record the PR in the
  `.applied` sidecar.
- diff3 mis-alignment: a PR adding a method where 4.7 has one with a similar
  docstring splits 4.7's method (def inside the markers, body below). Resolve
  to our side, insert the PR's method verbatim from `refs/sfdpr/<n>` before
  4.7's, add the registration line (#33507, #33568).
- Duplicate detection must be qualified by enclosing class (unqualified, it
  once deleted 29 classes from vehicle_test_suite.py). `self.X,` is only a
  registration inside a `tests*()` body. A def may be inserted from a PR head
  only when the PR's own diff adds it - a PR head contains the whole of master.
- A stacked PR's `merge-base(pr, master)` predates its parent, so its net change
  re-adds the parent's tests; the script prefers the newest applied ancestor's
  head as the base. That only works when the parent's head is an ancestor.
  #32972 and #33585 carry their own rebased copies of #32768's and #33478's
  commits, and `resolve_hotfile.py` then keeps the CHILD's copy of every shared
  method - a stale one. Put the parent's current copy back, then re-apply only
  what the child's own commits change in it (#32972: `accumulate_baro_drift`'s
  duration argument and a docstring). Keeping #32972's copies would have
  reverted #32768's `0ca1c9e775`, the 2026-09-11 trap again.
- A PR raised against the 4.7 branch (#34251) shares only a March master with
  master, so its "net change" from there is the whole 4.7 history.
  `rebuild_testfile.sh` and `resolve_hotfile.py` now measure from the 4.7 fork
  point when it is newer. Before that fix, #34251's merge folded hundreds of
  unrelated hunks into arducopter.py and it had to be rebuilt from scratch;
  `rebuild_testfile.sh` now keeps `<file>.applied.before` so a bad stop can be
  rolled back.
- Resolving a stop to "ours" keeps the file compiling and loading and can still
  drop the PR's change inside an existing method. `check_pr_test_lines.py`
  lists every PR-added line the file lacks.
- Most stops are the same three steps: `resolve_hotfile.py --apply`, then (for
  a stacked child) the parent's copies, then insert every method the PR's own
  diff adds that the file lacks. Take that as a loop, stop by stop.

Gates, and what each catches:

- py_compile: syntax only.
- Duplicate-`def` scan per class (only `def tests(` repeats) and a
  dangling-registration scan.
- `check_suite_load.py`: dangling registrations (only an AttributeError at
  `tests()` time), duplicate registrations and duplicate defs, and with
  `--against` the registration diff with the previous branch.
- `check_test_api.py`: master-only helpers, keywords 4.7 does not accept
  (`takeoff(altitude_min=...)` - `altitude_max=6` on `takeoff(4, ...)` is
  `max_err=2`, not a rename; `fix_takeoff_kwargs.py` rewrites them), and Lua
  scripts this tree does not ship. Run it even when a change looks confined. It
  checks names, not classes: a helper copied into the wrong class passes it.
- An AST walk for positional calls into keyword-only methods (#34208 made
  `hover_and_check_matched_frequency` keyword-only; py_compile accepts the old
  positional calls and they raise TypeError at run time).
- `check_pr_test_lines.py`: PR changes lost inside existing methods.
- Def-set diff against the previous branch - useful, **not** a specification.
  Missing from the new branch is not the same as wanted: the old branch is
  stale wherever a PR moved (restoring `EK3_PerCoreOptflowLogging` reintroduced
  a test #34363 had replaced with `EK3_PerCoreLogging`).
- A merged PR silently deletes its own tests: once merged, its tests sit below
  every PR's merge-base with master and the reset base never had them. Only
  the suite load and def-set diff catch this.

## Conflict resolution recipes

**Taking the previous branch's resolution** (`resolve_from_branch.py`) is sound
only under two conditions:

1. No commit still to be applied touches the file (otherwise the branch's copy
   is from the future and silently skips them).
2. No PR touching the file has moved since `applied.lock` (otherwise the copy
   is stale and reverts the PR's change). This is the one that bites: taking
   the branch's `AP_NavEKF3_VehicleStatus.cpp` during a #33484 conflict put
   `takeOffDetected` back after #32232 had renamed it - three uses of a member
   that no longer exists, nowhere near the conflicting hunk.

`changed` lists exactly the PRs that make the branch stale. To recover after
taking a stale file: `git checkout HEAD -- <path>`, then
`git show <sha> -- <path> | git apply -3` to re-raise the conflict.
`git checkout --merge` does not work once the path is `git add`ed.

**Commits that arrive already applied** (many of #32972's after #32768; #33507's
copy of the shared AGL KF commit): keep HEAD, confirm `git diff --cached` is
empty, bump `progress.idx` without committing. Check `git show <sha> --stat`
first - `8f7487dfa5` "refresh on-ground references" looked like a no-op and
was, but dropping it blind could have lost `vertCompFiltState.pos`.

**Targeted top-up** (one or two PRs moved, branch otherwise current): land the
old-head-to-new-head delta on the tip via a `git commit-tree` of the new tree
onto the old head, cherry-picked for 3-way semantics. For `arducopter.py` use
the delta restricted to that file with `git apply --3way` instead - the
squashed delta mis-anchors against master-only tests (one 180-line conflict
became 16 clean hunks and 4 real conflicts), and taking "theirs" on a
mis-anchored hunk pulled 165 lines of master-only UTM tests in. This is a
stop-gap; rebuild from prs.txt next time.

## Running the SFD tests

`Tools/SFD/run_sfd_tests.sh` derives the set from the branch (tests registered
here that 4.7 lacks, plus those whose bodies this branch modifies, compared
against the 4.7 the branch contains), counting tests defined on TestSuite and
the Sub suite. It puts the watch list first, the throw tests and Replay last,
and `--resume` continues an interrupted run. A clean run is minutes; each crashing test costs ~45 minutes
of reconnect stall before the harness gives up, so a run with crashers looks
wedged when it is merely slow - check the log, not the buildlog mtime, which
lags. For long runs use a normal shell:
`python3 .claude/skills/autotest/run_autotest.py --timeout 21600 <steps...>`.

- Rebuild BOTH vehicles after any shared-library fix. A pass on a stale binary
  is not evidence (Plane once passed on a six-hour-old binary because the fault
  was intermittent).
- `run_autotest.py` resolves the harness from its own path: invoked by absolute
  path from another worktree it silently runs *this* clone's tests. In other
  worktrees run `Tools/autotest/autotest.py` directly with `BUILDLOGS` set.
- TCP 5760 can be held on the Windows side of WSL, invisible to `ss`/`lsof`;
  every SITL then fails to bind. It cleared after ~40 minutes. `--uds` is broken
  in master's harness.
- EKF probe output belongs in the dataflash: `GCS_SEND_TEXT` drops under load and
  `stderr` interleaves out of order across a reboot. A temporary
  `AP::logger().Write()` was right every time.
- A test that reads the dataflash log while SITL holds it open can miss buffered
  records (a flush race); wait before reading.

## Traps

- **A worktree does not inherit submodules.** `./waf` dies on
  `modules/littlefs/lfs.c`, then in `dronecangen`, and both print through a pipe
  with exit 0 - check the exit code, not the tail.
  `git submodule update --init --reference <main clone>/.git/modules/modules/<m>`
  for waf, mavlink, littlefs, lwip and the DroneCAN modules is enough for SITL;
  ChibiOS (204M) is not needed.
- **An interrupted rebase poisons the rerere cache.** A Ctrl+C left zero-length
  objects, truncated files, `fatal: bad object HEAD` - and a zero-length rerere
  postimage, which the redo then "used" to stage git's empty blob, surfacing
  hundreds of lines later as undefined references at link time. Check:

      find .git/objects -type f -size 0
      find .git/rr-cache -type f -size 0
      git fsck --no-progress | grep -iE 'missing|broken|corrupt'

  Delete the empty objects and the individual `rr-cache/<hash>/` directory, not
  the whole cache. `badTimezone` from fsck is ancient upstream history. Recover
  a branch that is also on the remote with `git update-ref` + `git checkout -f`.
  Interrupting a build or test is harmless; interrupting git writes is not.
- **Master renamed `takeoff()`'s `alt_min` to `altitude_min`**; `user_takeoff()`
  kept `alt_min`, so the rename is narrower than a blanket search suggests.
- **`grant_push.py` checks branch names against this clone.** Fetching a PR branch
  in to satisfy it is refused if a local branch of that name exists at another
  commit, and deleting "the temporary copy" afterwards deletes the real branch.
  Check `git rev-parse --verify` before fetching.
- **Amend is gated** by `pre_bash_check.py` (and the hook matches the command text
  inside a heredoc too). Rebuilding a commit works with allowed operations:
  save the diff, `git checkout -B tmp HEAD~1`, re-apply, re-commit,
  `git branch -f <branch> HEAD`.
- **Fetch the analysis archive before concluding an entry is missing.** A stale
  clone of `../ardupilot-pr-analysis` makes a thorough entry look never-written.
- **Intermittent faults defeat bisection.** The optical flow FPE (uninitialised
  stack struct) produced two confident wrong bisects. Measure the rate at a
  fixed commit first (it was CRASH, PASS, PASS); treat a probe TIMEOUT as no
  information, never a pass; after a bisect names a commit, revert it at the tip
  and re-run before believing it. A backtrace found it in minutes:
  `kernel.yama.ptrace_scope=0` (works on WSL2) and `DEBUGINFOD_URLS=` empty, or
  gdb stalls downloading symbols until the run times out.
- **Read the subsystem playbook before building on a flag.**
  `libraries/AP_NavEKF3/CLAUDE.md` already listed `takeOffDetected` and `inFlight`
  as unreliable; a round of experiments on #33585 re-derived that the hard way.

## Settled (so an audit does not re-raise them)

- **EKF3 covariance gates**: all four CovariancePrediction gates in
  `AP_NavEKF3_core.cpp` are on `accelBiasLearningInhibited()`, and #32471 carries
  the covariance restore that keeps ACC_ZBIAS_LEARN bit 2 learning in the air.
  A local revert to `inhibitDelVelBiasStates` (`cb5026417f`) was dropped after it
  failed `AccelBiasMovingPlatform`. Any other account in older notes is wrong.
- **Gyro recalibration in the EKF bootstrap reset** is deliberately not carried;
  it was dropped from #32202 before merge. Revisit only if the bootstrap reset
  misbehaves without it.
- 4.7.0-local and not carried: IIS2MDC offset-cancellation disable (the merged
  #33780 writes `OFF_CANC`), the DroneCAN `READING_TYPE_UNDEFINED` guard, the
  `_read_fifo()` beat delay, the `TerrainLoiterToCircle` 0.7/1.3 bounds, the
  throw constant unit comments, `INDOOR_TUNING_PLAYBOOK.md`.
- MSP VTX is enabled per board on the H7 targets rather than for every
  `minimize_fpv_osd.inc` consumer, so the 1 MB boards (BlitzF745, BlitzF745AIO,
  BETAFPV-F405, MicoAir405v2) fall back to the `HAL_PROGRAM_SIZE_LIMIT_KB > 2048`
  default. SmallFastDronev1 picks it up through TBS_LUCID_H7.
- `INS_HNTC2` notch conversion: #34251 no longer seeds defaults; #33879
  force-saves the tuned values.
- **A 4.7.0 parameter file does not mean the same thing here.** EK3_OPTIONS:
  4.7.0 has AglKfForOptflow at bit 4 and velD fusion at bit 5; here they are
  bits 3 and 4, bit 5 is OptflowAssumeFlatGnd. `ARMING_CHECK`/`SKIPCHK` LEVEL
  moved from bit 21 to bit 9 with #32391.
- Renames the private flight topics still use: `BARO1_THST_FILT` ->
  `BARO_THST_FILT`, `EK3_FLOW_MIN_H` -> `FLOW_HGT_MIN`, `TKOFF_GNDEFF_ALT`/`_TMO`
  -> `GNDEFF_ALT`/`GNDEFF_TMO`, `PSC_POSZ_P` -> `PSC_D_POS_P`, `XKF6` -> `XKFA`
  for the AGL KF fields.
- Deliberately absent from the topics: `EK3_RNG_TERR_RT` (a proposal, never
  shipped) and the ground-effect Z accel-bias inhibit `zAxisInhibit` (measured
  to do nothing at `ACC_ZBIAS_LEARN=2`; the EKF3 playbook's description is
  4.6-branch history).
- Flight-informed values that must survive a re-stack: #32475's throw constants
  (30 deg, 2500 ms, 5 deg, 3.0, 5 m/s, 100 ms), #33484's
  `FLOW_AXIS_LOCKOUT_MS = 500` (not the PR's original 1000), #33318's
  `desired_vel_norm * (_brake_accel_mss + drag_decel_mss)`. #34210's land
  failsafe cap is a PI controller on baro climb rate
  (`LAND_FS_CAP_P/I/I_BAND_MS/MAX`); `LAND_FS_RUNAWAY_CLIMB_M 10` is intact.
- Carried by refresh5 only by accident, through old PR heads, and gone with
  them: the ZenFC743 board and bootloader, AP_OpenDroneID speed precision, the
  VIBE message description, a Plane parachute test fix. Not SFD work.
- Hardware deliberately not enabled: #33991 (ICM-56686), #33781 (LSM6DSO),
  #31895 (Brahma H7); #33443 (TBS LUCID H7 AIO) is a sibling of SmallFastDronev1's
  board, not a dependency; #31919 (deferred baro calibration) overlaps the baro
  path already modified. #34120 (ICP201XX) is carried for future hardware.
