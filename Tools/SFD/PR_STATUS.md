# SFD PR status

A periodic check of every open PR in `prs.txt`. For each one it records whether
the PR needs a rebase, whether it has a CI failure of its own making, which
AI-review findings and maintainer comments are still open, and what was done
about them. Each pass adds a dated section at the top, and older passes stay
below for comparison. Per-PR detail and decisions still belong in
`../ardupilot-pr-analysis/<PR>/README.md`; this file is the cross-PR view.

## How to take a pass

```sh
Tools/SFD/ai_review_status.py --sweep     # AI round per PR, stale or current, open PRs missing from the manifest
gh pr view <N> --repo ArduPilot/ardupilot --json mergeable,mergeStateStatus   # CONFLICTING/DIRTY = rebase
git merge-tree --write-tree --name-only --no-messages upstream/master <pr-head>  # which files conflict
python3 .claude/skills/pr-checks/ci_failures.py <N> --repo ArduPilot/ardupilot # failing CI and why
Tools/SFD/ai_review_status.py --detail <N> # the latest round's text, to compare against the PR's notes
```

A PR whose head has not moved since its latest AI round still has every
finding of that round in its code. The question is whether each finding was
answered on the PR, recorded as declined or deferred in its notes, or is still
open. Check the PR's notes for the round's head SHA first. If it is missing,
the round was never triaged.

---

## 2026-11-07 (night): AP-Review at the squashed heads, rebases and docs fixed

AP-Review re-read the squashed heads: ACCEPT on #32471, #32473, #34457,
#34941, #32401, #33318 and #34630; COMMENT, nothing blocking, on #31005,
#31274, #30841, #33478, #33507, #34942 and #32475. Every round confirms
the squashes left the trees unchanged. The user asked for the rebases and
the documentation-only points; pushed with a lease each, descriptions
patched:

- #31005 `a2dcc51bb7` and #30841 `c15a37c45d`: rebased onto master
  `b7c08d4dbc` (force), the tests registered in master's new lists;
  builds clean, MinAltFenceZeroThrottle and ESCTelemLogRate pass, both
  mergeable again.
- #31274 `c922349b3c` (ff): the ESC.Err field doc and the description
  say bidirectional DShot now reads the 250-frame percentage, 100 until
  250 replies are counted.
- #33478 `d1bf6d5093` (force, tip rebuilt): the test comment, its commit
  message and the description give the noisy-range leg's real catch rate
  (3 of 3 runs here, 3 of 17 in the bot's).
- #33507: the stacking sentence, already fixed.

Open, code or test changes for a later round: #33478's leg should count
hold onsets in its window; #34942's rest latch should clear in
updateMovementCheck(); #32475's nextmode_requires_position() should
answer true when nextmode() is null; #30841's test should guard against
dropped log blocks. Each is in its PR's archive README.

## 2026-11-07 (evening): eleven PRs squashed after the flight audit, pushed

The audit (`FLIGHT_VALIDATION.md`, 2026-11-07) found flown fixes to fold
on three PRs, an unflown pair on #31005, and test or doc folds on seven.
Pushed to `andyp1per` in one atomic push, each with its old head as the
lease, every tree unchanged (backups `pre-squash/<PR>-1107`):

- #34942 `94f24cdd9c` -> `4eec3d0dbf` (23 -> 17)
- #33478 `74c4ceb8c4` -> `bf9810a469` (11 -> 7)
- #31274 `2b31e43213` -> `0f9555fb6f` (14 -> 13)
- #30841 `5e6288c3b1` -> `af27953c1b` (3 -> 2)
- #31005 `806fd8d2cf` -> `2a11dfad65` (5 -> 3)
- #34457 `e1c95f7d8c` -> `e6d636fab3` (16 -> 12)
- #33507 `651b648a9b` -> `a869b2ac57` (19 -> 14, restacked on #34457)
- #34941 `38635910f0` -> `76bdb6617a` (3 -> 2)
- #32475 `9149e72d4f` -> `e730ee0cd6` (34 -> 32)
- #32471 `ed77480c74` -> `3b3b5009d9` (68 -> 65)
- #32473 `7c0932de0b` -> `c1a80e5209` (72 -> 69, restacked on #32471)

Description to patch: #33507's stacking sentence names `e1c95f7d8c`,
`6e218cb0ef`, `fae59a24fc` and `651b648a9b`; it is now twelve commits
ending at `e6d636fab3`, then `b47647db89` and `a869b2ac57`. No other
description cites a commit the squash removed. The local `pr-*` branches
are still at the old heads (three are checked out in other worktrees);
the squashes are on `sq/<PR>-1107`, `-1107r` where messages were
reworded.

Open, for Andy: #33484's other-axis gate (worse on the T25's ground
resets), folding #32471's four first-flight fixes, splitting #32270's
`ee9dc05040`, #34457's fused floor at the merge squash, #33585's flown fix
under review, and #32972's two messages that name the T25.

## 2026-11-07: the overnight AP-Review round, fixed

AP-Review read the 11-06 heads: ACCEPT on #31005, #32473, #33478, #33484,
#34457 and #34877, COMMENT on #30841, #32401, #32471, #33318, #33507 and
#34630. The user asked for every point, should-fix, minor and optional,
to be fixed. Local branches are `r1107/<PR>-*` (push plan in the session
scratchpad):

- #32401 rebased on master (force), plus three fixes: the buzzer and
  logger refresh on quiet retries, and the duplicate pre-arm print.
- #34630 restacked on #34543's `871498e4aa` (force), which fixes the
  blank lane items in a DCM fallback.
- #32471: a bounded lead on the seed's zero-time age (ff). #32473
  restacked on it (force).
- #30841: the rate text, and an ESCTelemLogRate test (ff).
- #33318 squashed to one commit per subsystem (force, same tree).
- #33478: the noisy-range span limit at 5.2 s (ff).
- #34457: a finiteness check in the touchdown test (ff). #33507
  restacked on it (force).

Descriptions to patch: #33507 (the bound and the stack sentence), #30841
(the rate sentence) and #34630 (the stack count and a stale paragraph).
The rest of #32401's review is left for maintainers: the primed EKF
reset, the Lua race, and the cross-vehicle and surprise-arming questions.

## 2026-11-06: the AP-Review fix round on ten PRs, pushed and answered

The fixes for each PR's current round were pushed to `andyp1per` with a
lease, and each PR has a reply. Per-PR detail is in
`../ardupilot-pr-analysis/<PR>/README.md`.

- #33478 `d322e6c4d0`: the settled-start rule for the step hold.
  Description updated.
- #32401 `749911703c` (force, `c2a704089d` reworded): a GCS disarm cancels
  a pending arm, there is no reset once armed, the reset happens only when
  the AHRS pre-arm check fails, failure messages come every 3 s, and there
  is a new PendingArmOnSwitch test. Kept Copter-only, with an answer to
  peterbarker's request that it not be. Description updated.
- #31005 `806fd8d2cf`: the fence floor's RTL now flies at zero throttle,
  except with the interlock or e-stop off. New MinAltFenceZeroThrottle
  test.
- #32471 `279457aa35`: a zero time counts as a zeroing only while under
  1 s old, with a new leg in HoverBiasSavedFirstFlightAfterRest. #32473
  `16a9f14b47` restacked on it (force).
- #34457 `d8da8db695`: the touchdown test needs 50 valid XKFA samples per
  landing. #33507 `1ed5fa2dc4` restacked on it, with the floor bias std
  bound at 0.95 (force). Description updated.
- #30841 `9816840ba6` (force, squashed): ESC logging in AP_ESC_Telem at
  ESC_TLM_LOG_RATE, default 100 Hz, as peterbarker asked. Description
  rewritten.
- #33484 `707e0e621f`: comment only. The bus-thread finding was answered
  with no change. #34630 `b9e7518181` restacked (force), with the reset's
  yaw covariance term measured and left out.
- #33318 is now LOIT_DRAG_COEF (`079952af70` after its own AP-Review
  fixes), pushed earlier the same day.

The 11-05 list's #32471 accel switch fix, #34457 SIM_TERRAIN fix and
#34877 example fix (`9377f42416`) were pushed before this round, and
#33318's source-set rework was superseded. Still open: peterbarker's
changes-requested on #32401, and #31005's ALT_MIN plus polygon recovery
window, left for a separate PR. The bot's rounds at the new heads have not
been read yet. refresh13 carries every head above.

## 2026-11-05: AI-review pass over the REQUEST CHANGES and COMMENT rounds

25 open PRs had a current AP-Review round that was not ACCEPT; three were
new since the morning sweep (#32471, #32972, #34380).

VALT (#32270) and Throw (#32475) are active SFD work, not parked: keep their
branches, tests and descriptions current, but do not promote them upstream
(no replies pressing for review or re-opening the design question with
lthall) unless Andy asks. #32270's open items: the companion mavlink PR for
mode 29 (COPTER_MODE_VALT) is not opened, Andy to decide.

Fixed locally, not pushed:
- #32471: an accel switch on the ground since the EKF last zeroed the bias
  now refuses the first-flight save ("accel switched on ground"); EKF
  reports each accel's bias from the core set up on it. `r1105/32471-switch2`
  (`cab832f2f7`, `c7fcf29ee3`, `0c4475f079`); #32473 restacked as
  `r1105/32473-switch`. In the beta from topup11.
- #34877: the example keeps a timed wait (UINT32_MAX in SITL, 1 s on
  hardware) so it still tests the SITL semaphore fix. `r1105/34877-fix3`.
- #33318: Andy rejected the LOIT_OPTIONS opt-in (again). Reworked as
  `r1105/33318-srcset` (`ff59584ac8`): the drag subtraction applies only
  while the primary core's source set takes velocity from optical flow;
  GPS is as master. A source-set switch at 3.8 m/s steps the feed-forward
  by the whole drag (4.3 m/s/s, speed swings to 1.3 m/s); a 1 s ramp only
  trims it. With EK3_SRC_OPTIONS 8 the beta's primary-lane switches would
  flip it the same way (not measured). The beta flies the subtraction on
  every source set today. Andy: fly it on and off first; topup11 carries
  a temporary LOIT_OPTIONS bit 6 for that.
- #34457: OpticalFlowAGLKfTouchdown waits for a zero range on the ground,
  which the CMAC terrain tile in CI puts at 0.55 m; fixed with SIM_TERRAIN 0
  as `717595bef8` on `r1105/34457-sf`, #33507 restacked on it as
  `r1105/33507-flat`. Not pushed.

Drafted, not posted: replies on #32471, #34941 (correction to the reply to
tridge on short tilts), #33568, #34584, #34251, #32398, #33478, #34583,
#34877; description edits for #33478, #33568, #34583. #32972's should-fix
belongs to #32768 (hands off). Old blockers on #30841, #31005, #32391,
#32401, #34443, #34871 and #32270's mode number are still open; #34871's
param_parse failure comes from `@Vehicles` on parameters, which is only
valid on log messages (`@Param{Copter}:` is the parameter form).

## 2026-11-03: the gyro bias across an EKF reset (#34942)

Users report yaw jitter without a compass and poor VALT on the new betas.
One cause found so far: every bootstrap zeroed the gyro biases, and with no
yaw reference #33498 holds the Z bias in flight at what was learned at
rest, so a quick re-arm after the forced reset this PR makes routine flew
with what had been relearned from zero (SITL 0.6 deg/s, armed 6.8 s after:
15.1 deg of drift in a 60 s hover, 2.8 deg kept). Andy's call: the EKF's
own reset keeps the biases, the EKF_RESET switch zeroes them. Refresh12
`dfd8194b85`, `80cbb8106b`, `19e5ca3831`, `76a16f3278` (EKFResetGyroBias);
port `sq/34942-1103b` (`c83b148144`, `713ca4946f`). Unflown. Also found:
VALT_POS_EXPO moved from var_info2 29 to 30 on 10-31, so a value set on a
4.7.1 or 4.7.2 beta before then reads 0, or a saved TKOFF_CPU_AVG; fix
not decided. Record in ../ardupilot-pr-analysis/34942/README.md.

## 2026-11-02 (night): third AP-Review sweep

ACCEPT: #31274 and #34942 (both at the heads before the evening squash),
#32475, #33543. COMMENT, all small:
- #34630: the EKFn_EN one-liners now say the strength floor needs
  EK3_OPTIONS bit 3 (`38acc82154`). Port `355d45685c`.
- #34642: the Sim on Hardware fix folded into "let the SITL gyro rate
  follow INS_GYRO_RATE" (SITL-only code, cannot fly). `49312747a0`.
- #31216: bootloader LED on PE3, the status LED, rather than PA8, the LED
  strip pad; bootloader rebuilt. `ba01eac9ac`. Refresh12 brought up to
  the released-board version (`de64bcd386`, `9e4067f7e0`), which it had
  not carried.
- #34584: description only (three new commits, five vectors).
- #34543: only the test-gap minor (DRK, QUAL, REJ, tilt arrows) left.
- T25: two more logs on c46a09ec after TD's VRFB learning (0.0131 saved).
  Loiter holds a baro height the prop wash biases 1.6 m high (SURFTRAK_MODE
  0, EK3_RNG_USE_HGT -1, BARO1_THST_SCALE -150); another forced reset after
  a landing. Record in ../ardupilot-pr-analysis/34942/README.md.

## 2026-11-02 (evening): T25 garage logs, #34942 static yaw, two squashes

Two T25 garage loiter logs on c46a09ec (flow only, baro height, no yaw
source; record in ../ardupilot-pr-analysis/34942/README.md). A landing
with the EKF 2.2 m above the baro let #34942's ground height check hold
the status at zero, and upstream's disarmed forced reset fired after 5 s.
The static yaw reference survived that bootstrap, so the old yaw was fused
against the new one: a 2.9 deg/s Z gyro bias, and the vehicle turned
about 2.5 deg/s in the next Loiter flight with the stick centred.
- #34942: the static yaw reference is stored through one function and
  retaken after every bootstrap and yaw reset (`ba9712dbdb`,
  `c06189bb8d`), with EKFBootstrapResetStaticYaw and
  StaticYawKeepsCompassYaw (`826c2dfd73`, `55d75d80a4`). A 30 s hold on
  the forced reset for a height error alone was tried and dropped
  (`a8ed785070`, removed by `b5121d648a`): the 5 s reset was written for
  that height test, and with the static fix the replayed status recovers
  7.5 s after the disarm, sooner than with the hold.
- Squash audit against the T25 logs
  (../ardupilot-pr-analysis/squash-audit-2026-10-31/T25-2026-11-02.md):
  two on-top commits flew and are folded: #31274's full-window error rate
  into "report the bdshot error rate as a percentage" (ESC.Err 100% until
  the first window filled, then at most 1%), and #34942's static yaw
  reference at boot into "keep a shock from holding off the on-ground
  stillness check" (yaw held within 0.03 deg from power-on). Everything
  else stays on top.
- Mode 29 on SFD is VALT, not RATE_ACRO as the log tools name it.
- Open: INS_ACC_VRFB_Z on the T25 is still 0.0417 (the card asks for 0).

## 2026-11-02 (later)

Second AP-Review sweep after the morning pushes: #32471, #32473, #34678,
#34941 and #33879 ACCEPT (the last three newly labelled); COMMENT on
#31274, #32475, #34543, #34630, #33543, #34584, #34642 and #34942. Fixed on
refresh12, then ported (not pushed); cold reads by Claude, Codex being out
until 11-04:
- #31274: the stored DShot percentage latches stale in update(), so the
  32-bit micros() wrap cannot revive it (`aa789238ce`; host probe 84 of
  1.8M checks passing at the wrap before, none after). Port `05edaf83de`.
- #32475: leaving throw, or completing with THROW_SRC_SET 0, restores the
  pre-throw source set only while THROW_SRC_INI's set is still active, and
  says "keeping" when a picked set wins (`0aa542c7ef`, test `69ee81e93b`).
  Port `9149e72d4f`.
- #34584: ATSC.AngPSc logged from the recorded copy (`d7a07b2217`). Port
  `474a5f073b`.
- #34642: Sim on Hardware keeps its base gyro rate (`c6c03d649e`). Port
  `d464e2156a`.
- #34543/#34630: the EKF lane OSD items behind AP_OSD_EKF_LANES_ENABLED,
  off on boards and on in SITL, one-line EKFn_EN descriptions, build option
  OSD_EKF_LANES (`9b0cd90701`, `d071c1ebdd`, `1351f2a70c`); on for the ten
  SFD boards (`670b9812e4`). 1036/1132 bytes on JHEM_JHEF405. Ports
  `871498e4aa`, `0b2644e19d`.
- #34942: the reset at rest waits for the fusion horizon, and the rejected
  ground height clip is no tighter than 2 m (`4795b33bc8`, `742952b71f`,
  test `fe3b070713`). Port `b39c0e9485`.
- #33543 rebased onto master for a re-review, description to match (`9f1b08db4e`).
- Refresh-only: AHRS reports no primary core (-1) while DCM, SIM or External
  fly, as #34543 does on master (`5b816923f3`). Rover OSDEKFLanesDCMFallback
  had failed on refresh12 since the fallback leg arrived, the OSD marking
  lane 0 as flying during a DCM fallback; Copter no longer treats a DCM
  fallback as the primary changing to 0 (inertial frame reset, "EKF primary
  changed:0"). Unflown.

Combined refresh12: SmallFastDronev1 copter builds clean, and the changed
tests pass, Rover OSDEKFLanesDCMFallback included.

## 2026-11-02

AP-Review sweep over every open SFD PR: nine new rounds since 10-31, all at
the squashed heads. #29000, #34210 and #34292 ACCEPT; #31274, #32471, #32473,
#32475 and #34543 COMMENT; #34630 REQUEST CHANGES. Fixed on refresh12, then
ported (not pushed):
- #31274: a short DShot window divides its errors by at least 250 frames
  (`35c5b6ae2d`), so one miss after a restart reads 0.4% rather than 100% for
  half a second; a dead ESC now shows after 3 replies rather than 1. Port
  `eea919627c`.
- #32471: `_rsem` on the two accel bias wrappers a Lua or DDS arm reaches
  (`915f070c11`), what bit 2 costs on the ground in ACC_ZBIAS_LEARN
  (`2e7a5f895c`), the AccelBiasMovingPlatform docstring (`7094e5ccd6`). The
  PR alone also gets the 1 Hz loop driving the inhibit both ways
  (`0581269c93`), which #32473, and so refresh12, already does. #32473
  restacked on it, `863cc99654`.
- #32475: throw stops writing THPH and sending "Throw position regained" once
  the next mode runs (`e48795b46f`; the bot's `return` would raise an internal
  error in the next mode's controller), the hold drift read from SIMSTATE
  (`b5de7f578a`, the location ratchet), the SIM_IMU_POS comments
  (`4aa7810362`), a handoff test (`ccd6da4d3d`). Port `df174b0731`.
- #34543 and #34630 rebased onto master 1d44127b86, keeping master's
  ScriptingOSDDrawScreen. #34630's OSDEKFLanes flake fixed with SIM_TERRAIN 0,
  as #34543 already has (5 of 6 to 10 of 10 with CI's tile). No refresh12
  change.
- #33543's commit message reworded as hardening (same tree); #32398 retitled.
- Open: on #31274 a channel getting under about 50 replies/s never fills a
  250-frame window, so it reads 100% (from 988f8abb9e; read from the code,
  not seen on hardware). Codex is out until 11-04; the cold reads were by
  Claude.

Records: `../ardupilot-pr-analysis/<PR>/README.md`, "2026-11-02".

## 2026-10-31

No sweep. The day's changes, all on Andy's PRs:

- #34942 added to prs.txt and taken by refresh12 topup3. Its CI failure,
  MAV_CMD_SET_EKF_SOURCE_SET in sitltest-copter-tests1e at `3da7a8cf2e`,
  was commit 5's stillness cap leaving the static yaw reference unset.
  Fixed and pushed, `3da7a8cf2e` -> `dc76e048a6`.
  - Quick review: COMMENT. Codex's one finding is master behaviour the fix
    restores: 60 s still on a set with no yaw source pulls the yaw 97 deg,
    the same with and without the PR.
  - PR body updated: the T25 row now reads 9 refused, 7 of them re-arms.
  - CI passes at `dc76e048a6` (110 of 110), sitltest-copter-tests1e
    included. No AI review yet.
  - Later the same day, `dc76e048a6` -> `2a52c965e2`: the reset at rest
    waits while a measured vertical velocity is being fused. An SFD ship
    test had a deck bump reset a GPS-fused velocity mid bias-learning.
    Codex's three findings on the first version (latching, slow sources,
    body velocity per axis) are closed by the audit. Quick review APPROVE.
    The PR body does not mention it yet; a draft is waiting for Andy.
- #32471 pushed `04e501e13d` -> `54d41e98c1` -> `5afa8d69fb`, and #32473
  `3ca94847db` -> `f945737262`: test changes only. The bit 2 tests now arm
  where the EKF is right on the ground, with or without #34942. Quick
  reviews APPROVE. Andy keeps bit 2 as it is. CI on both new heads was
  still running at the time of writing.
- AP-Review's rounds at the 10-30 and 10-31 heads (17 PRs, `ai_review_status.py
  --sweep`). Blockers on #31274 and #32475, both confirmed in the code and fixed
  on refresh12, then ported (not pushed):
  - #32475: throw starts horizontal control when a position returns while it
    holds on (`b12501f5af`, test `b1bfb811be`); SITL drift 120.5 m to 0.0 m in
    20 s, all 20 Throw tests pass on the stack and on the PR alone.
    `r1031/32475c`.
  - #31274: the DShot error rate keeps a full window's rate across a restart
    (`213ce15bb2` and five follow-ups from Codex's cold reads); probe, no
    failing ESC reads under 1% in any case tried. `r1031/31274g`.
  - Should-fix items left as measured and decided, needing replies: #32471's
    quick re-arm, #34210's saturation feedback. Most other rows are
    description items the unposted 10-30 drafts cover. Records:
    `../ardupilot-pr-analysis/<PR>/README.md`, "2026-10-31".
- Posted the same evening, at Andy's word: descriptions on #29000, #31274,
  #34210, #34456, #34630, #32475 and #34251 (retitled), the 10-30 drafts brought
  up to date with the day's flights and fixes; replies on #29000 (the why),
  #31274 and #32475 (the blocker fixes), #32471 (the quick re-arm, and the
  flights), #34210 (rmackay9's question, and the saturation feedback) and
  #34456 (peterbarker's retry question). Not posted: #32232 and #32238
  (rishabsingh3003's PRs) and #34543's reworded commit messages, which belong
  to its squash.
- Andy's decisions the same evening: #31274 keeps its 5 s error-rate window
  (late-starting failures dilute; listed as a known limit), and #32475 keeps
  THROW_ALT_DCSND's new release-relative meaning, now stated in its
  description with a reply to the bot's should-fix.
- #32473's CI: one sitltest-quadplane-tests1c job failed after 53 min with no
  failed step and no log; looks like a runner timeout, rerun needed.
- Found on the way, master's and not filed: the static yaw reference is
  taken before the initial yaw alignment and not refreshed by a yaw reset
  while the vehicle stays still.

## 2026-10-30

The AI review bot re-reviewed 18 PRs at the heads pushed on 10-29. Andy
asked for every suggested change to be made (except on #32768 and
#32972, left alone), #34251 reworded rather than given the save, and the
descriptions updated. Each fix went in as a new commit with an A/B,
then a quick review: a Codex cold read per commit plus an audit of the
bot's findings (Claude reviewers once the Codex quota ran out). Records:
`../ardupilot-pr-analysis/<PR>/README.md`, "2026-10-30".

| PR | Pushed (old -> new) | What changed |
|---|---|---|
| #29000 | `4cc85b68cf` -> `fbf9c380f9` | start-up fast rates no longer left applied under tcal; RateThreadTempCal |
| #34251 | `64b0c17788` -> `dff8813535` (force) | commits reworded to what the diff does; re-measured in SITL |
| #34809 | `d65d3b4baa` -> `ea1a881ad7` (force) | AP_OSD message: flush() no longer services the port since #34882 |
| #34543 | `17f017fb8c` -> `af50a66828` | lanes kept through a DCM fallback; Rover test; flash check tightened |
| #34630 | `d402ac272a` -> `9a64b9bc5f` | flow floor shown only with bit 3 or while latched; test tightened |
| #34210 | `93dcf216c0` -> `aa2cf2ddcf` | ceiling winds down at any land speed (two airframes, no dive); FS_OPTIONS doc |
| #32471 | `b399a6ab1b` -> `d9e7d85f5c` (force) | ACC_ZBIAS_LEARN AP_UInt8 (folded); restarted-log inhibit fix moved here |
| #32473 | `9396a8ee52` -> `3ca94847db` (force) | restacked on #32471 |
| #31216 | `758b814166` -> `2f220cecef` (force) | on master `278a68a57a`; retired ALLOW_ARM_NO_COMPASS; iFlight photos |
| #31274 | `4ef30eefe0` -> `db7371c4d8` | gate narrowed to FC bidirectional DShot; three DShot stats fixes (need hardware) |
| #32475 | `f7a5aeb84f` -> `60cee67154` | four blockers and five review rounds; 19 Throw tests |
| #32232 | `5e0c45cf62` -> `158e8386a3` (rishabsingh3003) | two-range-finder substitution; XKF5 filter |
| #32238 | `f4361d8b65` -> `47714568af` (rishabsingh3003) | ESC_CALIBRATION=9 early return replaces bit 2; DroneCAN limit documented |
| #34456 | none | description and reply drafted; the late-REV3 fix measured to change nothing |

Open after the round:
- Description and reply drafts for 11 PRs are unposted (session
  scratchpad `r1030/DRAFTS.md`).
- #32238's DroneCAN bit misses DroneCAN baro/airspeed at boot; kept and
  documented by Andy's decision, so the bot will keep raising it.
- #32475: after THROW_SRC_SET switches to an unaided set, EKF3 dead-reckons
  a position for about 7 s, so a next mode needing a position takes over
  and the EKF failsafe lands it. Closing it means reading the set's source
  configuration (ARCH-3); Andy to decide.
- #32475 against #32514: on refresh12 ThrowDropTwiceWithoutReentry and
  ThrowNextModeAutoStaleSubmode fail every time and ThrowSrcSetHandoff
  sometimes, because #32514 no longer raises the EKF failsafe for a
  source set with no horizontal source. One of the two has to adapt
  before both can merge (ARCH-1). Later the same day: four test
  problems, not a code clash. #32475's tests are fixed on refresh12
  (`28223b9b5b`..`c0c0f45759`) and on #32475, pushed `60cee67154` ->
  `3d634e4a5f`; neither PR's code changes.
- CI on the pushed heads was still running; #34456 and #34696 had
  infrastructure/flaky jobs rerun.
- Added to prs.txt the same evening and taken by refresh12 topup1: #33366
  (robertlong13, the CPU load measurement behind SFD-O4 log64's blocked
  takeoff) and #34941 (Andy's, opened that day, the flow lane timeout
  behind log61's Loiter). Neither has had an AI review. #33366 still
  carries Andy's CHANGES_REQUESTED of 2026-07-29 ("I will need to test on
  my hardware"): the code comments were answered, the hardware test has
  not run (log64 shows the bug, not the fix). Andy's field-data comment of
  2026-10-30 leaves open whether the 99.5% peak default has margin at an
  87% average with the rate thread. #33366's TKOFF_CPU_AVG/PEAK and
  #32475's THROW_SRC_SET/THROW_DROP_CNF both take var_info2 60/61 against
  master, so whichever merges second renumbers; SFD moves #33366's to
  41/42, on 30/31 until 2026-10-31 (REFRESH_NOTES.md, "Parameter indices").
- Later still: #34543 pushed `af50a66828` -> `351aea9eaa`, test only:
  `SIM_TERRAIN 0` in OSDEKFLanes, whose no-flow-data leg missed its flash
  whenever SITL had the CMAC terrain tile cached (quick review, COMMENT
  for the bot's four minor rows, unchanged). #32471 pushed by Andy
  `d9e7d85f5c` -> `04e501e13d` (v3: the motors-on part saved with bit 2
  clear), after refresh12 topup1's lock; taken by refresh12 topup2.

The beta took the code fixes as refresh11 topup5 (`0cb49e7dfb`), and a
full refresh12 was built on the new heads (REFRESH_HISTORY.md).

## 2026-10-29

Master at `5b36047100`. 61 PRs in the manifest: 49 open, 12 merged (#34882
merged 2026-10-27; #33569, #34601 and #30980 also merged but are not marked as
merged in `prs.txt`). No open PR had been pushed since its latest AI round.

### Summary

**Needs rebase.** Each conflicts with master in one hunk:

| PR | Conflicts in | Note |
|---|---|---|
| #31274 | `Tools/autotest/arducopter.py` | also flake8 E303/E301 |
| #32238 | `AP_BoardConfig.h` | rishabsingh3003's PR |
| #34809 | `Tools/autotest/arducopter.py` | conflicting since #34882 merged |
| #34456 | `AP_NavEKF3.cpp`, `bindings.desc` | also a Replay link failure |
| #32475 | `ArduCopter/mode_throw.cpp` | master renamed AHRS accessors |
| #32471 | `Tools/Replay/LR_MsgHandler.cpp` | master changed how Replay applies EKF events to the other EKF, so this merge needs care |
| #32473 | the same Replay file | stacked on #32471; its copy lacks #32471's `eff85fc256` |
| #32768 | `Tools/autotest/quadplane.py` | frozen for a maintainer's review; rebased by the user's decision |
| #32972 | `quadplane.py`, `AP_NavEKF3.h` (`_primary_core` became `AP_UInt8`) | carries all 32 of #32768's commits |

After #34933 lands, check #33484, #33359, #33585 and #34380 for conflicts.
They all edit `FuseOptFlow()` too. This has not been checked yet.

**CI failures the PRs cause:**
- #34456 fails to link Replay, every run since 2026-10-04. The EKF3 frontend
  now calls `sources.setPosVelYawSourceSet()`, which writes
  `AP::logger().Write_Event()`. That cannot link into `AP_DAL_Standalone`.
  The SFD branch carries the same call (`AP_NavEKF3.cpp`).
- flake8 E131 fails on #34209, #34210, #32475 and #32232. Test registrations
  are indented by 13 spaces. This came from the 2026-10-03 rebase sweep, and
  the fix that went onto #32471 never reached these four.
- #31274 has flake8 E303/E301, and QuadPlane TakeoffCheck still expects the
  old "ESC RPM out of range" message.
- #30841 fails DynamicRpmNotches, because ESC logging now needs the Motors log
  bit. This is one of the bot's blockers, and fixing it is a design change.
- #31216 fails markdownlint, with 30 README errors.
- Probably flakes or unrelated failures, to rerun:
  - #33568 tracker LoggerMsgChunks
  - #33507 MinAltFence
  - #34457 and #33359 QAUTOTUNE
  - #34543 and #34630 Renode
  - #31770 copter-tests2b
  - #29000 and #32553 ROS2 build-test
  - #34251, two autotest jobs
  - #34696 copter-tests1d

**Unanswered maintainer comments.** These carry more weight than the bot.

- #34457
  - peterbarker, 2026-10-14, three inline comments on `OptFlowFusion.cpp`:
    L811 "no reading the EKF can consume?", L818 "0.25?" and L833 "0.3?".
  - rmackay9, 2026-10-14: "struggled to convince ourself that it was correct
    and safe... graph of the estimated velocity before and after (even in
    SITL)".
  - Neither is in the notes.
- #34456
  - peterbarker, 2026-10-14, inline at `AP_NavEKF3.cpp:940`: "What makes this
    event so special...". It is the same Replay retry path as the link failure.
  - tridge's CHANGES_REQUESTED from 2026-09-30 has not been re-reviewed.
- #34363: peterbarker, 2026-10-12: "narrow that to only log for the lanes
  actually using the relevant sources?"
- #34210: rmackay9, 2026-08-30: "I'd much prefer if we could improve the
  existing failsafe" (ARCH-5).
- #32475: lthall, 2026-06-27, objects to custom throw handling in the takeoff
  check and the RC code.
- #32391: rmackay9 and IamPete1, 2026-04-14: "use a stand-alone bit". This
  agrees with the bot's blocker.
- #31770
  - tpwrules' CHANGES_REQUESTED of 2026-04-02, with 7 inline comments.
  - tpwrules, 2026-04-04: "does not work on my Cube Orange".
- #31005: IamPete1, 2025-08-27; peterbarker inline, 2025-09-29.
- #30841: peterbarker, 2025-08-19: "Allocate a new logging bit... interval as
  a parameter".
- #31274
  - peterbarker, 2025-10-29: the `#endif` comment.
  - IamPete1: "test all" only checks the last ESC.
- #33543: peterbarker, 2026-06-23, asked for instrumentation and the actual
  effect. His questions have not been answered.
- #29000: tridge and rmackay9, 2025-01-07; peterbarker, 2025-05-01. The PR
  carries the NeedAnswer label.
- #32398: peterbarker, 2026-08-31: "@bnsgeyer is planning on making this value
  a parameter now".
- #32401: peterbarker, 2026-03-08: "shouldn't be Copter-specific". Still
  unresolved.

### AI findings still open

**Blocking findings (REQUEST CHANGES):**

- #32391, LEVEL check
  - It reuses AIRSPEED bit 9, and the ARMING_CHECK to SKIPCHK conversion
    silently drops the lean check. In SITL it "arms while rolled 90 degrees".
  - The failure is reported as `Check::INS`, not LEVEL.
  - The SFD firmware has this.
- #31005, min-alt fence
  - `ap.auto_armed` is already cleared, so RTL runs
    `make_safe_ground_handling()` and the copter still falls. The bot's fix is
    `set_auto_armed(true)`.
  - It needs a zero-stick STABILIZE test.
  - The SFD firmware has this.
- #32401, pending arm
  - An acknowledged GCS disarm does not cancel a pending arm. It self-armed
    about 60 s later in SITL.
  - The one-shot EKF reset can run while armed.
  - Plus 7 suggestions.
- #34210, land failsafe
  - The ground wind-down only works for `LAND_SPD_MS` below 1.0, because the
    integrator band is a fixed 1 m/s. At 2.0, servo1 sits near 1505 PWM for
    60 s. The beta has the same code.
  - The description's scope is wrong, and the vertical controller's
    integrator can wind up.
  - Fails flake8.
- #32475, throw
  - The uprighting drop gate has no time bound, so with DCSND=5 the vehicle is
    left armed in THROW on the ground.
  - Drop confirmation is timer-only under CONST_POS_MODE.
  - `requires_position()` is false, so the EKF failsafe is report-only in
    PosHold.
  - `THROW_ALT_DCSND` changes meaning.
  - The source-set restore and handoff are not retried after a rejected
    `set_mode`, and nothing is restored with NEXTMODE=18.
  - Fails flake8.
  - Plus 3 minor findings.
- #31274, motortest
  - `motor_test_seq` is 1-based but is used as a 0-based channel.
  - The fixed 1.0 limit does not fit DroneCAN's cumulative `error_count`,
    which can block takeoff.
  - The test expects the old message, and flake8 fails.
- #30841, ESC logging: Rover, Sub, Blimp and Tracker lose ESC, ESCX and EDT2
  logging, and DynamicRpmNotches fails.
- #31770, DFU
  - Interrupts, SysTick and NVIC are not stopped before the jump.
  - `VTOR=0` is ITCM on H7.
  - The F4/F7 RCC names do not compile.
- #31216, Borg H7 (draft): README lint errors, a missing and misnamed image,
  and no LED defaults.
- #34209: flake8 only. The design notes were answered; tridge and peterbarker
  have not re-reviewed.
- #34251
  - The bot is wrong when it says "superseded by master". The PR targets
    ArduPilot-4.7, where those lines still exist, so it needs a reply.
  - The "after" table cannot be reproduced; expect MODE 1, HMNCS 3.
  - The title says "persist", but the diff only deletes.
- #33318: parked until the O4 flies #33639.
  - Local `fix/33318` has 13 unpushed commits (opt-in `LOIT_OPTIONS` bit 1 and
    others).
  - The bot's finding (4) is not covered: "EKF nav-gain scaler floored by
    rngOnGnd even with the option off".
- #34380: the short-reach band was put to a maintainer on 2026-10-07 with no
  answer since. With a 27.5 m reach on RNGFND1_MAX 40, the vehicle holds
  28.3-28.7 m and loses relative position.
- #33568
  - Blocked on merge order (#33585, #34380).
  - The description is stale: "Validated by replaying a real flow flight", and
    #34380 "being reworked".
- #32270: parked by the 2026-10-14 decision. Mode 29 is not in
  `ardupilotmega.xml`.

**Non-blocking findings with no decision recorded:**

- #32398: retitle the PR to the commit subject; optionally add
  `#if ARMING_DELAY_MS < 0 #error`.
- #32514: line 36 of the description names `has_horiz_pos_vel_source()`, which
  is now `configured_to_use_horizontal_position_or_velocity_source()`.
- #34583
  - The angle P boost returns, up to 4x, so it needs a release note.
  - The test checks the ATSC log, not the scale actually used.
  - `RTDT.dtAvg` holds the nominal dt.
- #34543
  - Lane items go blank under the DCM fallback on Plane and Rover.
  - There is no compile-time option or `build_options` entry.
  - The `EKFn_EN` descriptions are 766 characters each.
  - `8f2e289008` and `3d3a382b3f` still carry squashed subjects.
  - The test misses QUAL, TILT, REJ, DRK and a single-axis arrow.
- #34630
  - The OSD treats `EK3_FLOW_QMIN` as a floor even without the AGL KF option.
  - `ResetVelocityToFlow` leaves out the attitude and yaw covariance terms. That
    code comes from #33484's commits.
- #34851: three options for #33585's `terrainState += posResetD` line: move it
  into this PR, land #33585 first, or answer with a reason.
- #33478
  - `EK3_FEATURE_OPTFLOW_AGL_KF` has no `build_options.py` entry.
  - The `|| posVelFusionDelayed` change has no regression test.
- #32553
  - The description is stale: "then climbs out", "reset commit is kept".
  - The log36 Replay has not been posted.
- #32972: trim the squashed fixup messages in `1bc26699c4`, `de7062749a` and
  `08146e80d9`.
- #33359: it must go in after #33507, but nothing enforces that. The bias
  sweep has not been re-run on top of #33507.
- #33543: reword the commit and title as defensive hardening. The bot showed
  that the "stale object_ptr" case does not happen.
- #29000: reply to the maintainers with the `set_primary()` FIFO re-timing
  rationale.
- #32232 (rishab's)
  - The XKF5 `C != 0` filter lets a bad offset pass after a lane switch.
  - On a baro-primary vehicle the movement detector releases on baro drift
    alone.
  - The description's figures are stale.
  - The commits need squashing.
- #32238 (rishab's)
  - DroneCAN baro and airspeed are probed only once, so slow nodes go missing.
  - The ESC calibration value stays latched.
  - There are no tests.

**Nothing owed:** #33484, #34292, #34783, #34809, #34933, #33585, #34362,
#32471, #32473, #34208 (waiting on lthall), #33507 (blocked on #34457),
#33879, and #32768 (one deferred finding).

**Never AI-reviewed, and none has the AIReview label:**
- #34696: its own `/pr-review` rounds only; copter-tests1d is red.
- #34642: Codex reads only.
- #34584: the notes say draft until #34583 merges, but GitHub shows it ready.
- #34678: the pilot's log and a hardware test are still owed.

**Notes gaps in `../ardupilot-pr-analysis`.** The latest round is not recorded
for #32471 (rounds 8-9), #32473 (round 8), #34209, #32232, #32553 (round 5),
#32972 (round 11), #33478, #34456, #34543, #34630, #34583, #34851, #32401,
#32398 and #32514. There is no directory for #34933 or for the older non-EKF
PRs.

### Done in this pass

Scope set by the user: rebase what conflicts and fix CI failures, with nothing
structural. Review findings and replies are left for later. The user lifted
#32768's freeze for this rebase only.

The branches were rebuilt as local `rb1029/<PR>` branches in the scratch
worktree `../pr-rebase-scratch`. Each was cherry-picked onto
master `5b36047100` (#34251 onto ArduPilot-4.7 `fbd0a3160c`).

Each branch passed:
- `check_branch_conventions.py` (markdownlint was run separately through `npx`)
- flake8 and pre-commit on the changed files
- `check_param_tables.py`
- the SITL builds for the vehicles it touches
- for EKF branches, CI's replay job reproduced locally: `./waf replay` plus
  `tool/AP_DAL_Standalone`
- `git range-diff` against the original, which showed only the conflict hunks
  as changed

Hardware builds: CubeOrangePlus (#34543, #32238), IFLIGHT_BORG_H7 (#31216) and
the CubeOrange bootloader (#31770).

| PR | Old head | New head | What changed besides the new base |
|---|---|---|---|
| #31274 | `8c2326eb22` | `4ef30eefe0` | Master moved `test_takeoff_check_mode` into `vehicle_test_suite.py`, so the Copter-only copy was dropped and the shared helper now expects "ESC RPM or errors out of range". This fixes the conflict, the QuadPlane TakeoffCheck failure and the flake8 E303/E301 together. |
| #32238 | `03bad59f95` | `f4361d8b65` | Kept master's `AP_UInt8 _alt_config`. rishabsingh3003's PR, pushed as a maintainer edit. |
| #34809 | `466ca4d393` | `d65d3b4baa` | Added the registration after master's `ScriptingOSDDisplayPortReplies`. |
| #34456 | `44fe0dba8b` | `62e7e604d0` | Took master's `AP_UInt8 _primary_core` and its renamed `get_variances` binding. New commit `AP_DAL: stub Write_Event in the standalone example`, placed before the EKF3 commit that first needs it, fixes the Replay CI link. |
| #32475 | `0a6dc7eea1` | `f7a5aeb84f` | `ahrs.has_status()` became `configured_backend_has_status()` (renamed on master in `86a5ac8231`) in every commit that adds one. The 13-space test registrations were fixed in the commits that add them. Each commit builds. |
| #32471 | `eff85fc256` | `b399a6ab1b` | Dropped `f4dbf5fce4` "Replay: do not read past a log record that predates a message field". Master's `9610cb59b7` does the same thing (it zeroes the struct and copies `MIN(logged, struct)`). 42 commits. |
| #32473 | `b7c4f1601d` | `9396a8ee52` | Restacked on the rebuilt #32471, which now includes `eff85fc256` (#32473's copy of #32471 lacked it), plus its 4 own commits, including the other session's `6118e47b19` and `b7c4f1601d`. 46 commits. |
| #32768 | `873d61cf47` | `f9f5c05e7f` | Added the registration after master's `TiltrotorVectoringRoll`. |
| #32972 | `08146e80d9` | `a7c55d69db` | Restacked on the rebuilt #32768. Kept master's `AP_UInt8 _primary_core` beside the PR's dead-zone comment. |
| #34209 | `6458b0e27e` | `634669a051` | Same base. Indent fix folded into the autotest commit. |
| #34210 | `7d66105e4a` | `93dcf216c0` | Same base. Indent fix folded into the autotest commit. |
| #32232 | `8b490c6438` | `5e0c45cf62` | Same base. Indent fix folded into `autotest: cover the range finder on-ground assumption`. rishabsingh3003's PR, pushed as a maintainer edit. |
| #31216 | `64387d5333` | `758b814166` | Same base. `markdownlint-cli2 --fix` on the README; whitespace and blank lines only. |
| #34543 | `023c651ac8` | `17f017fb8c` | Clean rebase, done to pick up master's Renode CI fix (`e26395c47c`, 10-17). A rerun reuses the old pinned Renode revision and fails again. |
| #31770 | `390b750db2` | `9b6ecff7b3` | Clean rebase. Its CI run is too old to rerun, so a new push is the only way to run CI again. |
| #29000 | `dfff83a3a3` | `4cc85b68cf` | Clean rebase, for the same reason (ROS2 build-test). |
| #34251 | `edc71a20d7` | `64b0c17788` | Clean rebase onto ArduPilot-4.7, for the same reason. |

The two fixes that change code or what a test checks (#31274's message move
and #34456's stub) got a cold read from a single reviewer agent with no Codex
cross-check. It found no code defect. The stub's commit message was reworded at
its suggestion ("the next AP_NavEKF3 commit needs it", not "now that").

Failed jobs rerun with no code change: #33568, #33507, #34457, #33359, #32553
and #34696.

**Not fixed, deliberately:**

- #30841 DynamicRpmNotches/DataFlashErase: the PR gates ESC logging on the
  Motors log bit, so the tests find no ESC data. This is the bot's blocker and
  a design decision.
- #34630 Renode: it needs a push to pick up master's Renode fix. Its copy of
  #34543 is one commit out of date (`baac7de7a8`), so that push means a
  deliberate restack on #33484 and #34543.

**Tooling noted along the way:** `Tools/SFD/resolve_registration_union.py`
crashes with `NameError: io` (missing `import io`). It also expects diff3
conflicts, while a plain cherry-pick gives two-sided ones.

**Push status:**
- 2026-10-30: the 15 andyp1per branches were pushed (`--force-with-lease` on the
  old heads above). GitHub shows each at its new head, MERGEABLE.
- #32238 and #32232 were pushed to rishabsingh3003's fork as maintainer
  edits. Both MERGEABLE.

**After the push (2026-10-30 morning):**

- The reruns passed: #33568, #33507, #33359, #32553.
- #34457 failed QAUTOTUNE on its rerun too. QAUTOTUNE is flaky on master
  as well (master plane runs on 2026-10-26 and 2026-10-27 failed on it), so
  this failure is not the PR's.
- #34696 failed SuperSimpleCircle on its rerun as well ("Prearm bit never went
  true"). The PR causes this:
  - its own EKFFailsafeRestoreMode ends with the battery failsafe latched (it
    drops `SIM_BATT_VOLTAGE` to 11.4), so the next test in tests1d cannot arm.
  - Fix: after the final `do_RTL()`, set `SIM_BATT_VOLTAGE` back to 12.5 and
    call `clear_battery_failsafe()`, as master's battery tests do. It is
    folded into `autotest: cover the EKF failsafe mode restore`, and the new
    head is `89a96878eb` on the same base.
  - A/B in SITL with `EKFFailsafeRestoreMode,SuperSimpleCircle`: before, it
    reproduced CI ("PreArm: Battery failsafe"); after, both pass.
  - Pushed 2026-10-30 (`ec0722bbed` -> `89a96878eb`).
- #31216: markdownlint now passes. CI then reached `test_new_boards.py`, which
  fails because the README references `blitz_h7_pro.png` (copied from the
  Blitz board) and the branch carries no image. It needs a real Borg H7 image;
  that is the same item Hwurzburg asked for.
- #31274: the macOS `build (sitl)` job failed on an infrastructure error (the
  runner could not reach github.com for `modules/gbenchmark`). Rerun it once
  its workflow run completes.
