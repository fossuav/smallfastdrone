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
  30/31 (REFRESH_NOTES.md, "Parameter indices").
- Later still: #34543 pushed `af50a66828` -> `351aea9eaa`, test only:
  `SIM_TERRAIN 0` in OSDEKFLanes, whose no-flow-data leg missed its flash
  whenever SITL had the CMAC terrain tile cached (quick review, COMMENT
  for the bot's four minor rows, unchanged). #32471 pushed by Andy
  `d9e7d85f5c` -> `04e501e13d` (v3: the motors-on part saved with bit 2
  clear), after refresh12 topup1's lock.

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
