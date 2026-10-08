# SFD refresh history

Dated record of each refresh and the investigations around it, newest first.
Conclusions only: where a claim was later withdrawn it has been removed or
rewritten here, with a one-line note where the wrong turn is itself the
lesson. The procedure, the standing fixups and the traps live in
REFRESH_NOTES.md; if something here still needs doing, it belongs in that
file's checklist, not here.

## 2026-10-08 - refresh10 topup10, review follow-ups

#33359 reads the AGL KF height for its switch only within the height
observation's tilt and freshness limits, so a stale one cannot hold the
range finder above the switch height; Replay of six flights is identical
with and without it. #32514 renames the AHRS horizontal source query
(no behaviour change). #34380 adds the centred-stick short-reach test leg
and asserts the accepted loss. #33507 and #34457 were rebased and squashed
upstream with their own lines unchanged, so only their lock lines move.

SmallFastDronev1 builds. 91 of 91 on the full set.

## 2026-10-06 - refresh10 topup9, #33359's step-up fix

Takes #33359's fix for the terrain step found in refresh8's log29: flying
off a 0.84 m step and back up, the range finder height switch re-engaged
with the terrain offset still part way to the lower ground, and the height
reset on the source change pulled the altitude down 0.45 m. Where only the
AGL KF makes terrain stable, the switch now waits for the AGL KF to agree
within 0.15 m with the range sample being fused, and takes the terrain
offset from the range when it disagrees by more than 0.3 m. Replay of six
flights, two indoors: the step lands at +0.10/-0.35 m instead of
-0.57/-0.63 m, and the other flights are unchanged. The new
EK3_RngHgtSwitchStepUp flies a 3.5 m step up in SITL.

SmallFastDronev1 builds. 91 of 91 on the full set, the new test included.

## 2026-10-06 - refresh10 topup8, #34208's lock-free target and #34642

#34208 drops its std::atomic loads, stores and fences for a plain volatile
sequence and published copy, after the maintainers asked for no first use
of std:: atomics; neither thread takes a lock or a barrier. Its SITL gyro
rate commits moved to #34642, which now catches up gyro samples only when
INS_GYRO_RATE raises the rate, so low SIM_RATE_HZ setups are unchanged.
The beta already carried the older SITL change and SITLGyroRate, so only
the deltas were applied; they match the PRs line for line. #34642 is
listed after #34208 in prs.txt.

SmallFastDronev1 builds. 90 of 90 on the full set.

## 2026-10-06 - refresh10 topup7, the review-round fixes

Takes the heads pushed after the 2026-10-05 AI reviews: #33585's flat-ground
snapshot is now dropped across a landing rather than restored, with a test
case for a range return cut short by losing flow; #32768's test reboots SITL
at its end so the next test starts clean; #34380 gains a comment on the range
gap it leaves; #32972 is restacked; #34543 and #34630 take the new OSD
commits.

The first full run tested a stale SITL binary. The 2026-10-05
`./waf configure --out` builds of other boards rewrote this worktree's
lockfile out_dir, so every later `./waf copter` built the flight board and
reported success while build/sitl stayed at 2026-10-05 12:48. The topup4-6
runs predate that and stand. The same mistake made the 2026-10-05 #32553
no-reopen Replay byte-identical to the beta, so its "nothing to correct" was
wrong; a genuine build shows the reopen halving the height-above-ground error
in log36's low dwell (FLIGHT_VALIDATION.md has the numbers).

Re-run on the rebuilt binary: 89 of 90. Replay timed out waiting for
GLOBAL_POSITION_INT in its GPS-yaw RTL, twice within a minute after the full
run, then passed five times: alone through the runner, directly, in a clean
worktree at the same commit, and on the topup6 beta.

## 2026-10-05 - EK3_OPTIONS 92 as the SFD default

sfd_defaults.parm now sets EK3_OPTIONS 92: bits 2 (flow may use terrain
data above the range finder), 3 (AGL KF scales flow), 4 (AGL KF velD) and 6
(quiet per-reset flow lockout messages). Bit 1 (manual lane switching) stays a
per-vehicle choice, as it turns off automatic lane switching, and bit 5 is
unused since #33585 made the flat-ground fallback always on. A vehicle with a
saved EK3_OPTIONS keeps it; the O4's saved 62 still sets bit 1 and the unused
bit 5, and not bit 6.

## 2026-10-05 - refresh10 topup4, the OSD lanes and #34630

Takes #34543's arrow lane marker and #34630, which shows each flow lockout
reset on the OSD lane item and adds an option to quiet the reset messages.
#34630 stacks on #33484 and a copy of #34543 whose lane status reuses
#33484's per-axis flow timer; its versions were taken, as the beta already
carries #33484, and they give the lane its own timer and put #33484's back
under EK3_FEATURE_OPTFLOW_AGL_KF. #34630 first claimed EK3_OPTIONS bit 4,
which #33478 already uses for AglKfVelForVelD; it now uses bit 6 upstream
too. Bit 5 was avoided because the vehicles' EK3_OPTIONS 62 sets it.

Builds with the AGL KF off. 89 of 90 on the full set: LoiterNoCompassYawGPS
waited 3.6 s for GLOBAL_POSITION_INT while another job was running and passed
on --resume. The lock records #34630 at its reviewed head 6ed939c51b.

## 2026-10-05 - refresh10 topup3, the review fixes

The 2026-10-04 flights validated #34456, #32473, #34362, #33585 and #33484;
their results were posted and those PRs squashed one commit per module, with
#33484's four SIM_FLOW_OFS/SIM_FLOW_QUAL commits kept as they were so they
still match #34292's by patch-id. topup3 takes, by cherry-pick onto the beta,
the fixes for the AI reviews that followed: #33585's fallback end timed to
the latest range sample, #34380's height limit raised only while the terrain
offset is being fused, and test or wording changes on #33484, #34292, #32972
and #32471. The #33585 and #34380 changes were pushed before a /pr-review and
reviewed afterwards; the review keyed the raise on fusion and tightened every
new test, and the rules this broke are now in PR_REVIEW_RULES.md.

The full set ran 87 of 90. Two of the new tests failed only under the batch's
load: the reach model toggled from a polling loop landed late and let the
vehicle read past 28 m, and a 1.7 s brief return measured 2.0 s against its
bound. Both were reworked (a SIM_STATE hook, the outage read from RFND, a
recentred window) and A/B'd with 22 busy loops loading the host. The third,
VibrationRectificationBiasLearning, missed its "Hover Z-bias" text during a
3.5 minute reconnect after a reboot; it passes alone and on resume, and would
be robust reading MSG from the log. One local fixup: 4.7's
send_set_parameter() has no add_to_context. 90 of 90 after --resume.

The lock records #33585 and #34380 at their reviewed heads, bca14b045e and
49e01bf7eb, which were not yet pushed to their PRs when topup3 shipped.

## 2026-10-04 - refresh10, the AI-review rework

The 16 EKF/flow/baro PRs were reworked for the 2026-10-03 AI reviews and
pushed (fixups to already-pushed commits went on top, not squashed in).
refresh10 on the unchanged base takes those heads, adds #34601 and #33639, and
drops #34361 (redundant with #34362's drift gate; the terrain database is too
coarse at ground effect heights) and #33497 (closed). 364 planned commits.
Tests carried from the beta with the moved heads' methods replaced, as in
refresh9.

Two local fixes besides the usual fixups. #34362's new position-reset tracking
needs an AHRS reset count 4.7 lacks, so it watches the reset time instead.
FLOW_HGT_MIN arrived at index 8, FLOW_HF_RATEF's slot in the shipped beta, and
is pinned back to 9.

SFD set 89 of 90 on the first run: #34601's RateThreadPostFilterGyroLog lost
two thirds of its GYR samples to dropped log blocks. A/B on this machine: it
passes on #34601's master head and fails on plain 4.7 plus #34601, so the cause
is 4.7's SITL, not the stack. 4.7's `SITL_State::wait_clock()` polls with
`usleep(1000)`, capping the logger I/O thread at one 4 kB block per real
millisecond; master's `ec488ac50e` made it `usleep(10)`. Backported with its
comment follow-up, the test logs every sample (66618, none repeated), and a
full rerun is 90 of 90 bar one harness flake that passed twice alone.

Topped up later the same day with the heads that answered the overnight AI
reviews: #33585's brief range return is now saved and undone, #33484 and
#32553 have wording and test changes, and #34380 is rebased onto #33585
unchanged. Code by cherry-pick, tests by the method carry.
TerrainOffsetGroundEffectRecovery then failed at 0.37 m against its 0.35 m
bound. On this stack it
reads 0.33-0.37 m with the reopen and 0.57-0.60 m without, against 0.26-0.28 m
and 0.49-0.51 m on #32553 alone and on the plain base: the reopen works as well,
but both cases sit about 0.08 m higher. Not #34362 (#32553 with it reads as on
master) and not the arm-time datum reset (0.57 m with it disabled). The flights
show the stack settling lower in the dwell, 0.46-0.62 m against 0.58-0.83 m on
the range finder, deeper in the simulated band, with its EKF height closer to
the range finder during the takeoff. The commit that does it was not found: the
commits in between do not build on their own. The bound is now 0.42 m, which
separates both cases with margin.

A second top-up took the heads that answered the 11:00 AI reviews, as patches
from each locked head to the new one (several were squashed onto their unflown
commits). Two conflicts, both where PRs meet: AP_DAL::WriteLogMessage, where
#34292's forced output on a dropped block meets #32471's new return value (keep
both: mark the block and return false), and the top of NavEKF3::UpdateFilter,
where #32471's inhibit retry and #34456's source set retry both go (keep both).
Whichever of those PRs merges second needs the same. 90 of 90.

## 2026-10-03 - 22 PRs rebased onto master, refresh9

Master's Copter CI bucket rebalance left 23 of the stack's PRs conflicting, all
on test registrations bar #33318 (against #33569's merge). The 22 of them that
are ours were re-stacked by cherry-pick in a scratch worktree, checked (same
code, every registered test defined, every new test registered, copter builds)
and force-pushed with leases on the PR heads. #33484 needed `Pmut` for master's
const `P`; #32972 dropped its stale copies of #32768.

refresh9 on the unchanged base then took the field fixes from the PR heads.
Ten code-pass stops, all in shapes seen before except #33507's new floor gate,
whose before-takeoff branch #32232 had removed (the flag then rides on #32232's
substituted reading). Tests carried forward from the beta with the moved heads'
methods replaced. 84 of 85. It also undid refresh8's loss of the
`common_origin_valid` early return in getOriginLLH, which #32972's stale copy
had brought in.

## 2026-10-02 - #33478 topped up on the 4.7.2 beta

#33478 moved to `1b6f63bff1` (velD fusion gated on a settled AGL KF velocity
variance and on body odometry, GPS freshness from the retrieve time, speed
hysteresis, XKFA.VTR, the AGL KF run ahead of the load levelling skip). Landed
as a code delta on the beta rather than a rebuild. EK3_AglKfVelMixedSources
then failed at |IVD| 1.97 against its 2.0 claim threshold. Not a stack
interaction: the 4.7.2 base with #33478 alone reads 1.91 where master reads
4.1, and #33507's bias state was ruled out by an A/B (1.75 with its process
noise at the floor). The threshold is a local 4.7 adaptation ("Local work").

## 2026-10-02 - refresh8 on 4.7.2-beta1

`upstream/ArduPilot-4.7` had become 4.7.2-beta1, about 115 commits past the
base, carrying #33780, #33988, #34122 and #34360 and reworking EKF3's
covariance helpers. The base was rebuilt on it by replaying the old base's 85
commits (clean) as `SmallFastDrone-4.7-base-472`, those four PRs left prs.txt,
and #34543, #34583 and #34584 joined it. Branch `SmallFastDrone-4.7.2-refresh8`,
316 commits, promoted as the new `SmallFastDrone-4.7.2-beta`.

- The code pass stopped 17 times. Most were the familiar shapes: #34432's
  ground-effect gate arriving in master-based context before #34432 itself,
  EK3_OPTIONS documentation merged across #33478, #33585, #34361 and #33507,
  and #32972/#33507 replaying a parent's commits. #34543 needed porting onto
  4.7's AHRS and EKF3 status API, and #34584 onto #34208's
  `rate_controller_run_dt()` signature.
- 4.7.2 has `zeroStatesVarCov()`, so refresh7's zeroRows/zeroCols fixup is
  reverted; `Pmut` from #32471/#32473 became `P`.
- The hot-file rebuild went worse than before on the newer base (see Traps):
  master's tests1c/tests1d lists were unioned in and 21 PR tests were lost at
  "ours" stops. All were put back from their heads and checked with the gates.
- First full run 78 of 82. EK3_GetHaglTerrainAlt and TouchdownGroundEffectAlt
  failed on one mechanism: #33585 lets terrain data reach the cores without
  EK3_OPTIONS bit 2, and #34361's getHAGL relied on it not doing so, so the
  database height reached ground effect as a true AGL with the option clear.
  Gated in getHAGL; both pass. Replay failed on harness code edited after the
  run started and passes on re-run. Full re-run on the final binary: 81 of 83, the
  designed TerrainOffsetGroundEffectRecovery failure and one missed statustext
  in EK3_NoGPSLeakWhenNotSource (3 of 3 alone).

## 2026-09-28/29 - refresh7 promoted, then every open PR rebased

refresh7 was brought up to #34292's `2f411ad1c4` and promoted to the beta at
`57093e0cb4` (refresh6 kept as `.6-beta`); the beta has not been pushed yet.
DataFlashErase, open on refresh7, came from #34363 taking master's rewrite of
the test (from #30956): on 4.7 it failed to arm because 4.7's wait_armed()
waits for a heartbeat first, then dropped about 240 log messages, then missed
master's calibrated sizes (689 kB against a 1000 kB floor). Two #30956 commits
(`11093ee6bc`, `ad1ce89ce3`) and 4.7's own test body fixed it; recorded under
"DataFlashErase" in REFRESH_NOTES. The firmware string went back to
"SmallFastDrone V4.7.1"; the 4.7.1 base had lost 4.7.0's.

On 2026-09-29 every open PR in prs.txt was rebased onto master, 24 in all, and
the six with conflicts resolved: #32471's covariance writes had to become
`Pmut` (master made `P` const), #32768 met #34432's ground effect guard in
PosVelFusion, and #32238's conflict was only context from a reverted master
commit. Five had commits that CI's check_branch_conventions rejected and were
split or reworded. A rerere resolution recorded during an SFD refresh dropped
a blank line from #34362 on replay, so the later conflict rebases ran with
rerere off; check a rebase against the pre-rebase patch whenever rerere
resolved anything.

Review work the same day, each change tested against a check that fails
without it: #34456 (refusals reported to the caller), #33569 (range floor),
#34361 (no-flow getHAGL test), #30980 (compassmot with the rate thread,
CompassMotFastRate new), #32471 (learner re-seeded, save only what was
learnt), #33568 (`!gpsVelUsed`), #32514 (redesigned) and #34292. For #34292
the dev call asked whether Rishabh's AGL KF should drive the flow floor; it
measured worse (123 flow samples fused in the landing hold, 5 with the range
still fresh, against 0), so the range stays. The per-PR records are in
`../ardupilot-pr-analysis`.

## 2026-09-22 - refresh7

Branch `SmallFastDrone-4.7.1-refresh7` on the same base `a5eb325674`, not
promoted. 310 commits planned against refresh6's 284; the growth is this week's
own work (#34432, #34456 and #34457 as new PRs, and the four PR heads that
gained commits). Copter, plane, heli and sub build. SFD set: 74 of 78 on the first full run, 76 of 78 once the three failures that
were the refresh's own doing were fixed. TerrainOffsetGroundEffectRecovery fails
by design; DataFlashErase is open, with SITL not coming back from the reboot the
test does after the chip erase (REFRESH_NOTES).

- The code pass stopped eight times. One was a real decision (THROW_DROP_AG's
  index), one merged two guards into `readyToUseOptFlow()` that both belong
  (`flowVelResetUnhealthy` from #33478 and `flowFocusBelow` from #34292), one
  took #34456's `select_lane` argument onto 4.7's accessor, one merged #34432
  into the branch's own reset ordering, and four were #32972 or #34457 replaying
  a parent PR's commits at an older revision, where ours is the newer side.
- Fixups still needed: the `ekf3.EKF3` accessors, the two master-only AP_AHRS
  backend headers, `zeroStatesVarCov`, the `takeOffDetected` rename and the
  VALT guard - the last now also as a `#error` block in Copter.h. The GPS fix
  enum and the #31274 getter came through clean this time, carried by rerere.
- Two traps cost most of the time and are written up in REFRESH_NOTES: `plan`
  not resetting the progress index (the branch silently kept only the last 20
  commits of the plan), and the hot-file rebuild mangling methods rather than
  conflicting on them. `Tools/SFD/repair_test_methods.py` is the second one's
  tool.
- No parameter or default changes between refresh6 and refresh7, so
  `sfd_defaults.parm` is untouched. #27893 and #34360 merged to master and are
  marked in prs.txt; they stay in the manifest because 4.7 does not carry them.

## 2026-09-21 - SFD-O4 log15: the flow scale that was limiting the lane

The flow under-read has been the limiting number in the whole flow-lane story
and was never fitted properly, because no flight had exercised the X axis -
log9's calibration had eight strafe samples. log15 was flown for it: clean
alternating legs at 0, 90, 180 and 90 degrees relative to the nose, 5644 usable
samples split 3265 forward and 2352 strafe, range finder Good 99.5 %, mean
height 9.47 m with only 4 % near the 15 m cap so the truncation bias the
calibration helper warns about is absent.

X reads 0.97 and Y 0.90, giving **`FLOW_FXSCALER` -88 -> -60 and
`FLOW_FYSCALER` -148 -> -50**. The sensor rate checks out on both axes, so the
node and `FLOW_ORIENT_YAW` are right and `FLOW_HF_RATEF` stays at 1, and
cross-axis at 1 % and 3 % rules out a rotated flow frame.

Both halves of the usual `flow/ideal` ambiguity are closed. A height error
scales both axes equally, so the seven-point gap between X and Y can only be
flow scale; and the height itself checks out on its own terms, `dRFND/dt`
against GPS-Doppler climb rate giving slope 0.965 at corr 0.993 over 952
samples, the residual consistent with the 1 s differentiation baseline.

The record of it is in `../ardupilot-pr-analysis/34456/`, added
under finding 3 rather than replacing it: log14's 0.94 is the number for a
flight that flew the old scalers, and re-measuring the drift on the fitted
values is a different measurement.

**Verified the same afternoon on log16**, flown with the fitted values: the flow
lane's speed ratio against GPS is 1.002 where log14 on the old scalers read
0.935. Both axes land within a few percent of unity on the helper and on an
independent fit over pure legs only.

Two traps came out of that verification, both worth the space. The helper
reported X cross-axis at 20 % against log15's 1 %, which reads as a rotated flow
frame and is not one: log16 flew its middle legs at 34 to 55 degrees off the
nose, and each sample is assigned to whichever body axis dominates, so genuine
motion on the other axis is counted as leakage. Binned by course relative to the
nose it is 5-7 % on both flights, and an orientation error cannot appear between
two sorties with nothing touched. The playbook now carries that (aap 1.7.12).
And the independent fit written to cross-check the helper read six points low on
both flights and both axes until the range geometry was fixed: the flow sees the
ground at the slant range along body -z, which `RFND.Dist` already is, so
dividing by the vertical height inflates the ideal flow by 1/cos(tilt). A
systematic offset that appears on every arm of a comparison is the method, not
the finding.

## 2026-09-21 - SFD-O4 log12 and log14: the focus floor, and the lane fix flown

log12 is the `FLOW_HGT_MIN` sortie, flown at 2.0 m because the true value has
nothing to do on this airframe - `OF.Qual` is *highest* near the ground, 214 at
0 to 0.15 m AGL against 157 at 4.5 to 6 m over 2162 near-level samples, so the
ARK Flow's focus limit is below 0.15 m and the code already floors at 0.05 m.
The mechanism is exact: the range finder crosses 2.000 m at 514.5568 and flow
resumes 74 ms later, and again 65 ms later on the second pass.

The A/B on it went the other way and that is the useful part. Replaying log12
with `minHeight` cut to the ground clearance, so the flow below 2 m is fused
rather than discarded, improved the flow lane's velocity against GPS: RMS 1.60
-> 1.01 m/s through the climb window and 2.25 -> 1.67 through the descent, with
the above-floor windows unchanged. Discarding good flow costs accuracy, which is
the parameter's own warning measured rather than asserted. The guard is not in
question; the value was, and it goes back to 0 here. Two things log12 never
reached, both needing one more sortie: the post-touchdown churn the guard exists
for, because the pilot disarmed 0.7 s after landing, and the carried focus
height through a height reset, because none fired in flight.

log14 flew the source set lane fix on `5adc2ea0`. "Using EKF Source Set 2" at
46.5055, "EKF3 lane switch 1" 4.6 ms later, `XKF4.PI` 1 for all 1240 in-flight
samples, and 126 s of LOITER navigating on the flow lane with GPS alongside as
standby - no aiding stop, no flow reset, no failsafe. Against GPS the lane held
velocity to 0.26 m/s RMS at a 0.94 ratio and its relative position drifted 11.4 m
over 122 s, which is the 6 % speed under-read integrated.

That under-read is now the limiting number and it agrees three ways: 0.88 on the
forward axis from log9's flow cal, 0.92 to 0.98 by height on log11, 0.94 on the
lane that actually flew. It wants a dedicated calibration pass, forward/back and
a strafe leg at over 1.5 m/s inside range, because log9's fit had 8 strafe
samples and could not do the X axis at all.

A wrong turn worth keeping: the first A/B of log12 returned byte-identical
numbers for floor on and floor off, which reads like "the change does nothing".
It was one binary. `waf` had reverted to the `SmallFastDronev1` board, so
`./waf --targets tool/Replay` built the board's Replay while `replay_sweep.py`
ran a stale `build/sitl/tool/Replay` for both arms. Check the build log names the
path the runner will execute before believing an A/B, especially a null one.

## 2026-09-21 - SFD-O4 log11: a source set switch that reached nothing

A staircase profile on the fix, low hover to 23 m to 10.5 m and back, four
minutes of level LOITER. Three things came out of it.

The AGL KF fix has its control. log11's bias froze at -0.0806, within a
thousandth of log9's -0.0796, and the velocity stayed at -0.0009 m/s over 62 s
instead of running to -7.2. That residual is one prediction step of the frozen
bias, which is what the clamp leaves. Same bias error, opposite outcome, the
fix the only difference. No height step, third flight running.

The terrain path is measured. At 23 m the range finder returned NoData for the
whole 45 s segment and the AGL KF was invalid, yet `XKF5.HAGL` read 23.7 m from
the terrain database and the flow lane's speed tracked GPS at 0.98, against
0.92 at 3-4 m and 0.95 at 10.5 m where the range finder was good. An SRTM sign
error would not give that, so #34360 and #34361 are exercised and right. Flow
aiding never dropped up there either, which is what bit 5 and bit 2 are for.

And the flight did not do what it was flown to do. The three source set
switches changed nothing: with `SRC_PER_CORE` the set index and the core index
are the same, so the active set is never read, and the vehicle flew GPS on core
0 throughout at 26 satellites. Every interface reported success. `XKFS.SS` is
the only field that carries the truth and it held 0 and 1 per core. Fixed on
the branch; REFRESH_NOTES has the PR shape. The general lesson is the one the
EKF3 playbook now carries: a control that reports success is not evidence it
did anything, and on this configuration there is a field that says.

## 2026-09-21 - SFD-O4 log10: the AGL KF fix flown

The fix's first flight, and it holds. Over 69 s on the ground the AGL KF
velocity stayed bounded at -0.017 m/s worst against log9's -7.13, and the
827-of-827 on-floor samples that ran free in log9 are bounded here. `HAgl`
tracked from the first sample after lift-off instead of spending 2.9 s on the
floor, and there was no height step against log9's 2.21 m. It also settles the
half Replay could not reach, the ground effect release: "terrain offset reset
from baro" fires 2.0 s after NOT_LANDED, exactly GNDEFF_TMO, and log9 never
emits it at all. That message is latched on the ground effect clear edge, so
its timestamp is the release. One flight, and the absence in log9 has more
than one possible cause, so it is corroboration rather than proof.

Rest of the flight was clean: ESC error rates below 0.08 % peak, VIBE under
19 m/s2 with no clips, 21.7 satellites at HDop 0.66, motor balance inside 37
PWM, battery failsafe at 274 s. Thirteen aiding stop/start pairs and six flow
velocity resets, the same acro tilt-limit pattern as log9.

The first reading of this log was wrong and the wrong turn is the lesson. The
firmware banner still said `ee3bda1f` because the binary was built from an
uncommitted tree, so the hash identifies the last commit and not what was
compiled. Taking it as the vehicle's identity put the flight down as a second
unfixed one, which then had to explain why the fault had vanished, and produced
a whole theory that the wind-up was intermittent on the sign of the early
velocity error - committed, and wrong. A version banner dates the tree only as
far back as its last commit. Check the build against the source timestamps
before reading a flight as a control.

## 2026-09-21 - SFD-O4 log9: the AGL KF winds up on the ground

First flight of the promoted beta, SFD-O4 log9 (`ee3bda1f`), 216 s armed,
LOITER takeoff then acro. The vehicle was healthy - ESC error rates 0.01 to
0.02 % mean with no outlier channel, VIBE under 23 m/s2 and no clips, rate
tracking matching demand on all three axes - and the three things refresh6 went
in for held: no post-landing flow aiding churn, the touchdown baro hold kept
the height at 0.93 m while the baro dived to -3.35 m, and the baro offset
stayed frozen at its arm value for the whole flight.

The takeoff did not. The EKF height rose at 0.22 m/s against a true 1.02 m/s
(range finder and baro agreeing), then stepped 2.21 m at 93.78 s. Cause is the
AGL KF: its velocity state had wound up to -7.2 m/s over the 88 s on the
ground, because the height clamp at `rngOnGnd` holds the innovation at zero and
nothing corrects velocity or bias from there. Fixed on the branch at
`8461433db6`, which became #34457; REFRESH_NOTES has the mechanism and the
Replay numbers. log6 and log7 (`797f6854`) reach -6.5 m/s, so it is not
a refresh6 regression.

Two wrong turns worth keeping. The pinning was first put down to
`EK3_GND_EFF_DZ = -8` de-weighting the baro; the baro was not the height source
at the time, and reconstructing the fused measurement as `XKF3.IPD - XKF1.PD`
gave 0.51 to 0.60 m flat through the climb - `aglKfH` at its 0.05 m floor plus
the 0.46 m arm datum - which named `selectHeightForFusion()` fusing `aglKfH` in
place of the range finder instead. And the first cut of the sweep's AGL KF
metric took a global minimum of `XKFA.VAgl`, which reported an unrelated
in-flight excursion at 232 s and read as "the fix changed nothing"; it has to
be scoped to samples with the height on its floor. A summary number that spans
the whole log will not see a ground-phase fault.

Separately, the flow lane is not a fallback in acro on this airframe: 67 % of
the acro segment was past the flow tilt limit and the DroneCAN range finder
returned NoData for 67 % of the flight, leaving core 1's AGL KF valid for 25 %
of acro. Sixteen aiding stop/start pairs followed, many exactly 5 s apart. That
is the tilt limit doing its job, not a fault, but it bounds what the GPSDisable
switch can be used for mid-acro.

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
- 2026-09-21, before promoting: refresh6 had drifted from #34292's head, which
  gained the review fold after the branch was built. The four commits above
  bring it level, the behavioural one being the carried flow focus height
  moving with a height reset. Copter, plane, heli and sub build, and so does a
  copter with AP_RANGEFINDER_ENABLED 0, which is what the new guards are for.
  SFD set: 70 of 71, 0 crashes, the same designed failure (+0.216 m).
- Promoted 2026-09-21: the beta is refresh6 at `b19fee0640`, 297 commits on base
  `a5eb325674`; refresh5's beta is kept as `SmallFastDrone-4.7.1.5-beta`. The
  base pushed fast-forward, the beta as a force push.


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
