# SFD refresh history

Dated record of each refresh and the investigations around it, newest first.
Conclusions only: where a claim was later withdrawn it has been removed or
rewritten here, with a one-line note where the wrong turn is itself the
lesson. The procedure, the standing fixups and the traps live in
REFRESH_NOTES.md; if something here still needs doing, it belongs in that
file's checklist, not here.

## 2026-09-18 - refresh6

Branch `SmallFastDrone-4.7.1-refresh6` on base `a5eb325674`, not yet promoted.
Copter, plane, heli and sub build. Final run: 70 of 71, 0 crashes; the one
failure is TerrainOffsetGroundEffectRecovery, failing by design as on master.

- The notes were split into a checklist and this history, and the audit that
  split prompted found refresh5 had silently dropped six merged-upstream PRs
  (#33780, #33988, #33990, #34122, #34057, #34120) and the MSP VTX tests. They
  were on the branch but in neither the base nor prs.txt. Restored, the PRs as a
  "merged to master, not in 4.7" tier at the top of prs.txt, and
  `audit_dropped.py` written so the next refresh checks for it.
- The base gained one commit, `hwdef: SFD boards set ARMING_DELAY_MS`: #32398's
  new head reads that name, and the SFD hwdefs' `ARMING_DELAY_MSEC 0` would have
  matched nothing and restored a 2 s arming delay with a clean build. 4.7's six
  new commits (Sub, a Linux CAN fix) were not worth a base rebuild.
- Code pass: 284 commits, nine stops, rerere carried the rest including all of
  #32471's rework. Fixups as before (ekf3.EKF3, takeOffDetected,
  zeroStatesVarCov, the VALT guard, ACC_ZBIAS_LEARN and THROW_DROP_AG back to
  23 and 21, FLOW_HF_RATEF and FLOW_HGT_MIN held at 8 and 9), now committed per
  module rather than folded into the picks.
- Test pass: #34251 targets the 4.7 branch, and measured from master its net
  change is the whole 4.7 history; its merge wrecked arducopter.py, which was
  rebuilt from scratch once the scripts measured from the 4.7 fork point.
  `resolve_hotfile.py` kept #32972's stale copies of seven #32768 methods; the
  parent's copies went back. Fourteen dangling registrations were re-folded from
  the beta or dropped as master-only.
- First full run: 62 of 68. `OpticalFlowFocusHeight` and
  `OpticalFlowAGLKalmanFilter` needed master's `SIM_SONAR_OFFSET` (backported to
  SITL); `AmslAltPreservedOnRearmAtDifferentElevation` was the flush race again
  (the closed log has both resets; a 5 s wait fixes it).
- The other failures were investigated on the branch before promoting and
  fixed there (REFRESH_NOTES "Fixes made on refresh6"). Two were real stack
  interactions. #34292's flow floor left aiding churning every 5 s after
  touchdown. And with #32232 making the range finder the height source on the
  ground, the baro offset learned the spool-up ground-effect error and kept it:
  the EKF height ran 2.5 m high for a whole SITL flight. It is a master bug
  (a range finder that reads on the ground takes the same path), now #34432. On
  the way there it was attributed to #32972 and then #32232, and a dead-zone
  version replaced the first freeze; the /pr-review of #32232 found the dead
  zone ratchets on baro noise, and a six-scenario A/B of four variants went
  back to the freeze. The third, Replay, was
  a harness race: the larger log buffer master added never took effect without
  a reboot, and SITL panicked on a full buffer, which looked like a hang. A gdb
  run as SITL's parent found the panic; `ptrace_scope` 1 had blocked attaching.
  TerrainOffsetGroundEffectRecovery's preconditions had depended on whether a
  terrain tile was in the run directory.
- Parameters: refresh6 made #32473's acro inhibit opt-in, so the base gained a
  shared `sfd_defaults.parm` setting ACC_ZBIAS_LEARN bit 3 on every SFD board,
  and refresh6 was replayed onto it. `param_changes.py` now lists such changes
  each refresh.
- New scripts: `audit_dropped.py`, `check_suite_load.py`, `param_changes.py`,
  `check_pr_test_lines.py`, `refold_methods.py`, `fix_takeoff_kwargs.py`.
  `run_sfd_tests.sh` now counts TestSuite-defined and Sub tests and compares
  against the branch's own 4.7. `refresh.sh backup` now skips indices used on
  origin: its first backup of the base took `.1`, the name origin already uses
  for the June lineage (renamed locally to `.2`).
- PR updates pushed from the refresh6 fixes, after /pr-review of each: #34292
  force-pushed at `54cd8177fa` (also the AP-Review blocker, a no-range-finder
  build break), #33498 `d6eea72cd9`, #32553 `3216b0579e`, #32768 `51afc9c222`,
  and the ground effect fix opened as #34432 (`f2be29f74e`). #32232 got a
  description update and a reply, no commits. With #34432's version folded in,
  refresh6 ran 70 of 71 again, 0 crashes, the same designed failure.


## 2026-09-15 - the shipping beta jumps position on a GPS-to-flow fall back

`SmallFastDrone-4.7.1-beta` carries #33568 at `4bb2ef3583`. That head adds the
AID_ABSOLUTE -> AID_RELATIVE edge but still runs the generic mode-change resets
on it, so `ResetVelocity()` zeroes the horizontal velocity and `ResetPosition()`
moves the position to `lastKnownPositionNE`.

The origin re-bases onto the vehicle at 1 Hz only while GPS is in use, which
ends 4 s after GPS position fusion stops; the fall back fires at 10 s. A vehicle
still moving when it loses GPS and continues on flow jumps back by roughly the
distance flown in those 6 s and loses its velocity estimate. Measured in SITL
at 5.4 m/s: a 21.0 m and 5.9 m/s step, EKF position error 0.6 m -> 23.6 m half a
second later. A hovering vehicle shows nothing, which is how the 2026-09-12
archive check refuted it wrongly.

Fixed on the PR (pushed as `9e04d0e0a7`, since reworked): skip both resets on
the ABSOLUTE -> RELATIVE edge (step 0.6 m and 0.06 m/s), and replace the
per-cycle `readyToUseGPS()` guard with "GPS is the configured source and a 3D
fix arrived within gpsNoFixTimeout_ms" - under a sustained glitch the beta's
code bounces REL <-> ABS (12 aiding-mode changes per core in ~80 s), the new
head does not, and a dead receiver still falls back 10.6 s after its last fix.
`OpticalFlowGPSLossAiding` now flies ~100 m out through the fall back and
asserts position and velocity continuity.

Same sweep: #32471 (Replay inhibit fix, rebased onto master), #33498 (guard
reads actual yaw fusion only; all-compasses-failed leg 0.29 -> 0.03 deg/s) and
#33585 (ExtNav-start terrain carry) moved. #34360 merged to master, not 4.7, so
it stays in prs.txt; #33585 no longer carries its commits, so #34360 stays
ahead of it. #32473 moved the acro inhibit behind ACC_ZBIAS_LEARN bit 3,
default off, all three axes.

## 2026-09-12 - PR review pass over the REQUEST CHANGES PRs

Per-PR detail is in `../ardupilot-pr-analysis/<n>/`.

- #32398 was redesigned: input renamed to `ARMING_DELAY_MS`, `#error` on the old
  upstream `ARMING_DELAY_SEC`, but nothing catches `ARMING_DELAY_MSEC` - the
  name from its own earlier revision, and the name every SFD hwdef uses. See
  the post-merge fixup in REFRESH_NOTES.md.
- Heads moved: #34363 (DataFlashErase ceiling 1980 -> 1990 KiB), #33507 (AGL KF
  variance caps, bias bounded by EK3_ACC_BIAS_LIM), #33585 (terrain state
  carried across a height timeout reset), #34292 (FlowHeightMinTerrainPath),
  #32553 (rebased from a 12 May master; latch fix;
  TerrainOffsetGroundEffectRecovery), #32398 (rebased from a 17 March master).
- #34380 joined the manifest after #33568, which widens how many vehicles reach
  the flow height limit it fixes.
- AI review sweep: 41 of 42 current (#27893 had merged). The heads moved again
  afterwards, so this no longer holds.
- `origin/SmallFastDrone-4.7-base` force-pushed to `1bf6b3ddc0`, with the June
  tip `99414094f6` pushed alongside as `SmallFastDrone-4.7-base.1`.

## 2026-09-11 - refresh5

Branch `SmallFastDrone-4.7.1-refresh5`, 241 commits on the base, promoted to
`SmallFastDrone-4.7.1-beta`. Copter, plane and heli build. SFD set **60 of 60**,
0 crashes, about seven minutes end to end.

- The base was the story: `changed` reported "CHANGED 1bf6b3ddc0->99414094f6
  (full rebuild)", which read as forward and was three months backward - the
  2026-09-03 base rebuild had been done and the beta built on it, but the
  `SmallFastDrone-4.7-base` pointer was never moved off the June lineage.
  Refreshing onto it would have dropped 68 files including all of
  AP_GroundEffect. Fixed with `refresh.sh promote SmallFastDrone-4.7-base
  1bf6b3ddc0`. The plan went from 397 commits to 249.
- 19 conflict stops, mostly #32768/#32972 overlap (three shared subjects; six
  #32972 commits arrived already applied). Genuine 4.7 work:
  `resetHeightDatum()` returning bool via `configured_ekf_type()`, and
  `AP_GPS_FixType::FIX_3D` -> `AP_DAL_GPS::GPS_OK_FIX_3D`.
- Four build breaks, all master-isms: `ekf3.EKF3`, `takeOffDetected` (#32232
  renamed it `movedSinceArming` after the others were written),
  `zeroStatesVarCov()`, and VALT's `MODE_ALTHOLD_ENABLED` guard. ACC_ZBIAS_LEARN
  and THROW_DROP_AG both took 25 again; held at 23 and 21.
- `check_test_api.py` found nine master-style `takeoff()` calls; the suite load
  found a dangling `SITLGyroRate` and 17 duplicate registrations.
- `HeightDatumKeptOnMidairRearm` failed its 30 m assertion at 12.1 m because a
  diff3 resolution took the branch's older 150 m copy over the PR's; the test
  was at fault, not the code. The ordering fix went to #32768 as `0ca1c9e775`.
- New helpers: `resolve_from_branch.py`, `resolve_hotfile.py`,
  `resolve_registration_union.py`. Each grew corrections within the session;
  the rules they encode are in REFRESH_NOTES.md.

Also this day: `ai_review_status.py` written; the 28 open manifest PRs with no
current review were labelled `AIReview`. Pushed #32232 (`8bec444e50`) and #33585
(`36f86f06eb`). The end-of-session pick-up note was wrong in three places within
hours (an archive entry "missing" because the local clone was behind, a PR reply
"not posted" that already was, worktrees that did not exist) - hence the
REFRESH_NOTES rule that a pick-up pointer is re-derived before acting on it.

## 2026-09-10

- #33507, #33478, #34292 and #32972 rebased onto master (they had been 883, 1353,
  114 and 115 commits behind). #33507's autotest died on master's `takeoff()`
  rename (`alt_min` -> `altitude_min`; `user_takeoff()` kept `alt_min`).
- #33478 and #33507 both carry `AP_NavEKF3: only decay AGL KF velocity when the
  range finder is absent`, patch-identical on purpose so git dedupes it.
- A Ctrl+C during a rebase poisoned the rerere cache with a zero-length
  postimage; see the trap in REFRESH_NOTES.md.
- The six fixes from the 2026-09-09 flow-vs-GPS lane flights were implemented
  (PLAN.md) and ported to their PRs, including the new #34360, #34361, #34362
  and #34363.

## 2026-09-07 - targeted top-up for #32768 and #33585

Two PRs moved on an otherwise current branch, so their old-head-to-new-head
deltas were landed on the tip instead of re-stacking (recipe in
REFRESH_NOTES.md). The affected 11 cases passed, then 49 of the 50-step set;
the one failure, `EK3_OptflowAssumeFlatGnd`'s "does not carry over from a
previous flight" leg, was identical on the pre-top-up beta.

That leg's story, which took most of 2026-09-05..07:

- The flag it waits on never cleared on the stack. Bisect over the 218-commit
  stack: #32232's `f82183e6fb` substitutes the known ground clearance for an
  OutOfRangeLow reading while not taken off, which keeps `gndHgtValidTime_ms`
  and therefore `gndOffsetValid` fresh forever. Deleting that branch makes the
  leg pass; restoring it fails it again.
- The leg was the thing at fault, not either PR. `RNGFND1_MIN` above
  `RNGFND1_MAX` does not deny the EKF range data - every reading becomes
  OutOfRangeLow, which is still data on a build that substitutes a clearance -
  so the leg could not tell "no terrain measured" from "measured its own
  ground clearance". `kill_rangefinder()` now points the sensor away from
  `ROTATION_PITCH_270`, which `readRangeFinder()` skips; the test passes on
  #33585 alone and on the stack with #32232 unmodified.
- Wrong turn worth remembering: `gndOffsetMeasured` re-latching was blamed, and
  a tighter guard on #33585 was written for it. It changed nothing, because the
  flag was held by `gndOffsetValid`. `libraries/AP_NavEKF3/CLAUDE.md` already
  recorded both flight-state flags as unreliable; reading it first would have
  saved the experiment. `!inFlight` -> `onGround` is what landed.
- Also wrong: setting `RNGFND1_MAX = 0.01` to get OutOfRangeHigh. On the ground
  the analog sensor reads ~0, which with `RNGFND1_MIN = 0` is Good, so the
  probe swapped the substitution for the direct Good-reading path.
- #32232 has a defect of its own (with the sensor dead `detectTakeoff()` has
  only its gyro criterion, which a SITL climb does not reach); recorded in
  `../ardupilot-pr-analysis/32232/`.

`BaroDriftClearedWithRangefinderHeightSwitch` no longer sets `EK3_OPTIONS=8`:
#32768 traced the on-ground rangefinder switch to AP_AHRS forwarding
`terrainHgtStable` on change only, and the test's precondition check confirms
the switch is active at arm on this branch. `AmslAltPreservedOnRearmAtDifferentElevation`
failed once on a dataflash flush race (log read while SITL still held it open);
a wait before the read fixed it, carried locally and owed to #32768.

## 2026-09-06 - release audit against SmallFastDrone-4.7.0

Does the re-stacked branch drop anything 4.7.0 carried? `git cherry` flags 154
of 4.7.0's commits as absent - expected, since every rebased hunk changes its
patch-id. Each was scored by sampling its added lines and grepping this tree,
twice by different paths; everything near zero was read by hand. Verdict:
nothing needs re-folding. Parameters: 25 SFD-added `@Param` names on each branch,
every 4.7.0-only name a rename. Autotests: 25 on 4.7.0 against 49 here, the three
4.7.0-only names renamed and widened (`BaroDriftResetOnArm` ->
`BaroDriftClearedAtArm`, `EK3_FlowMinHeightFloor` -> `OpticalFlowFocusHeight`,
`EK3_AglKfRngHeightSwitch` -> `BaroDriftClearedWithRangefinderHeightSwitch`).
The settled differences are listed in REFRESH_NOTES.md.

## 2026-09-05 - refresh4 test run and the cross-PR check

47 SFD tests, 45 pass. The optical flow FPE was found and the fix moved into
#34292. The two failures were `EK3_OptflowAssumeFlatGnd` (resolved 09-07) and
`HeightDatumKeptOnMidairRearm` (resolved 09-11).

The FPE: four tests that do not configure flow aborted SITL with a floating
point exception. `SelectFlowFusion()` declares `of_elements ofDataDelayed;` as
an uninitialised stack local that `storedOF.recall()` leaves untouched when no
sample is at the horizon; #34292's focus-height check read it without the
`flowDataToFuse` guard every other read has. Zeroing the struct would have been
wrong: EstimateTerrainOffset would then fuse an invented zero flow rate, which
Plane trips because EK3_FLOW_USE defaults to 2 there. It cost most of a day;
the lessons are under "Intermittent faults" in REFRESH_NOTES.md.

Cross-PR check against the private flight analyses: every mechanism the topics
name was checked for presence, and the riskiest clusters for reachability.
Renames and deliberate absences are listed in REFRESH_NOTES.md. One real
finding: `EK3_RNG_USE_HGT > 0` made the height source RANGEFINDER while parked,
and `resetHeightDatum()` refuses any source but baro or GPS, so the arm-time
baro-drift reset never ran (EKF_ALT_RESET at arm: 1 with the default -1, 0 with
70). Fixed upstream in #32768 (`allow the arm-time datum reset under the
rangefinder switch`) and covered by `BaroDriftClearedWithRangefinderHeightSwitch`.

## 2026-09-04 - refresh2

Onto the unchanged base (4.7 had moved by one AP_HAL_Linux commit; the base was
deliberately not rebuilt). 38 in-flight PRs, 13 conflict stops, 1337
registrations across the vehicle suites with none duplicate or dangling.

- #32475 throw mode became a real PR and replaced the local throw stack.
- #33879 joined, pairing with #34251.
- #32471 rebased onto master moved ACC_ZBIAS_LEARN to 25 (master occupies
  21-23 with SURFTRAK_GLDST, SURFTRAK_GLSAM and FLIP_), and #32270 and #32475
  independently took 25 too. Pinned to the shipping numbering.
- `FLOW_HGT_MIN` (#34292) and `FLOW_HF_RATEF` (#33497) both took AP_OpticalFlow
  index 8: a clean build that panics every SITL boot. `check_param_tables.py`
  was written for it.
- A merged PR silently deleted its own tests from the hot-file rebuild
  (#32472's TakeoffGroundEffectAlt / TouchdownGroundEffectAlt, and
  LoiterFlowBrakeOvershoot).
- ThrowDropSourceSwitch and ThrowModeNoGPS failed (the state machine fell out
  of Throw_Wait_Throttle_Unlimited on the freefall check); both pass from
  refresh3 on.

## 2026-09-03 - base rebuilt on 4.7.1, and the 4.7-beta fold-in

`SmallFastDrone-4.7-base` rebuilt against 4.7.1 (`dbe792162d`) as `1bf6b3ddc0`
(though the branch pointer was left behind; see 2026-09-11). #29768, #32045,
#32472 and #33587 had merged and were promoted into it.

Folded in from the previous-generation `SmallFastDrone-4.7-beta`: nine PRs it
carried that the 4.7.1 line had not picked up, the open ones added to prs.txt.
The merged six (#33780, #33988, #33990, #34122, #34057, #34120) turned out to be
in 4.7 itself by the next refresh. #34210 needed its `vibe_comp_active()`
broadening moved into AP_GroundEffect's input; #34208's keyword-only signature
left a bodyless duplicate to delete. Deliberately not taken: #33991, #33781,
#31895 (hardware not enabled here), #33443 (a sibling of SmallFastDronev1's
board, not a dependency), #31919 (overlaps the baro path already modified).

VibrationRectificationBiasLearning subtests D/E (ACC_ZBIAS_LEARN bit 2) failed
with INS_ACC_VRFB_Z ~0. The first diagnosis, a hover observability limit, was
wrong. Two real causes, both measured as clean A/Bs:

- #32471's covariance gates collapsed the accel-bias covariance while bit 2
  inhibited. The discriminator was #32471's covariance-restore commit: subtest D
  0.18-0.19 with it, 0.000002 without.
- #32473 was stale against #32471, and its own gate used `takeOffDetected`,
  which only `detectOptFlowTakeoff()` writes, so it stays false for a whole
  flight without flow and reduced the gate to `onGroundNotMoving`. Replaced with
  `!onGround` (D 0.000097 -> 0.178 against a 0.15 injected bias). `inFlight` also
  measured (0.174) and rejected: on the `assume_zero_sideslip()` path it never
  sets on a GPS-denied plane.

A local revert of the four gates to `inhibitDelVelBiasStates` was tried and
dropped after it failed `AccelBiasMovingPlatform`. Earlier notes claimed both
that the gates were fixed upstream and that the branch carried the revert; the
state is as recorded under "Settled" in REFRESH_NOTES.md.

#32972 had branched from an old #32768 and carried ~13 duplicate commits; it
was restacked on the current #32768 with only its own four and force-pushed.
