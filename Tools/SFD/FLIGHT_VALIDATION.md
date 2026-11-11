# SFD flight validation record

What each flight showed for the PRs in the build, newest first. The flights
still to fly are in `FIELD_PLAN.md`; this file is the history behind them.
Per-PR detail lives in `../ardupilot-pr-analysis/<PR>/README.md`. Read
`ANALYSIS_REPOS.md` before analysing a flight: what to check first, and
what may and may not be written here about the private flight record.

Every log is flown with `LOG_REPLAY 1` and `LOG_DISARMED 2` and replayed;
the A/B baselines are Replays of the same log with the PR's change removed.


## 2026-11-10: the T25 outdoors and the TD25 indoors with a broken range finder, `7b00f3b5`

Both flow only (MAVLink flow and range finder, no GPS or compass),
`EK3_OPTIONS` 92, baro height, `EK3_FLOW_QMIN` 60, `FS_EKF_ACTION` 4. All
three logs replay exactly.

The T25 on its second board (`00360049`), VALT and Loiter outdoors, 199 s:
clean. Range finder Good throughout, flow quality median 120. Loiter's
settled hold 0.05-0.11 m median from target; stops from 0.9-1.6 m/s in
0.8-1.0 s, 0.16 m past at most. Height 0.02-0.11 m rms from target. One
lockout reset in the air (#33484), right: Loiter entered at about 5 m/s, 1
m up, where `EK3_FLOW_MAX` 2.5 caps the measurable speed at 2.5 m/s; the
reset took 2.2 m/s and the flow agreed after it. The EKF height rose up to
1.95 m against the range while the baro warmed 39 -> 50 C, outdoors over
unknown ground, so baro creep is not separated from terrain.

The TD25 (`00480035`), two VALT flights indoors, 154 s and 170 s, with the
range finder compromised (operator report): it read 0 m (out of range low)
for 98% of flight 1 and was Good for only 41% of flight 2, with 0.019 m
Good readings while flying.
- Every time it was out about 5 s the EKF dropped its relative position
  and terrain height together, and the EKF failsafe followed ("EKF
  variance: position lost"): all of flight 1 from 42 s, and four times in
  flight 2. VALT needs no position, so the pilot saw no change; Loiter
  would have been refused.
- #33484: nine lockout resets in the air, none reaching the control
  (VALT). Seven scaled the flow by a near-zero AGL KF height (0.05-0.23 m
  from the 0 m and 0.019 m readings), taking the velocity to 0.1-0.6 m/s,
  4.99 -> 0.12 m/s at -12 deg pitch once. One at 507.74 s took 15.28 m/s
  from a single flow sample: the range had just come back (6.3 m after
  20 s at 0) and the AGL KF re-initialised to it in the same 0.1 s as the
  reset. After 5 s without a range the AGL KF restarts from the next
  sample and the reset's "range settled" check compares it with that same
  sample, so the first range after an outage passes every range gate; and
  the reset uses one flow sample however good the height. Fixed on the
  beta as refresh14 topup2 (`c3c24b9c7f`, `REFRESH_NOTES.md`): a reset
  now waits for three range samples in a row to confirm a restarted AGL
  KF, and replayed, this one is refused (#33484).
  The samples were the EKF's 50 ms polls, though, and this MAVLink range
  finder reports a reading as Good for 500 ms, so one isolated reading
  after an outage could still confirm its own restart in 320 ms (found
  by the review bot, 2026-11-11). Fixed on #33484 (`daaca832c4`) and on
  the beta as refresh14 topup6 (`35b31b96a7`): the DAL logs each range
  finder's reading time, and a sample from a reading already counted
  does not count again. BLK 03 replays unchanged.
  The resets on a near-zero height are not fixed: the readings were wrong
  but consistent.
- #33484's quality latch fired at 582.13 s (quality 57) and released in
  the air 2.4 s later.
- #34457's floor hold pinned the AGL KF to its floor in the air: at
  40.74 s in flight 1 its height snapped from 0.36 to 0.05 m while the
  sensor read 0 at 1-1.4 m, the known limit its comment states (a sensor
  returning zero on a lost return), and its coast stop held the AGL KF
  velocity at 0 meanwhile.
- A third log (boot 185, recorded before the two flights) was powered
  17.5 min and never armed; the EKF stayed sane.

## 2026-11-10: a customer's flow quad on `7b00f3b5`, five flights

A customer's SmallFastDronev1 quad: one IMU (so one EKF core), ARK Flow
with its range finder on DroneCAN, compass yaw, no GPS. Five flights on
the beta head `7b00f3b5`: logs 15 and 16 (ALT_HOLD, LOITER to 17.3 m,
FLOWHOLD), and three drops in THROW into VALT or LOITER (logs 18, 20,
22). The same airframe's stock 4.7.3-beta1 logs flew only STABILIZE and
ALT_HOLD, so there is no stock Loiter to compare with. Settings changed
between flights; `EK3_OPTIONS` 92, `EK3_RNG_USE_HGT` 10, `EK3_FLOW_GAIN_H`
12, `EK3_FLOW_QMIN` 0, `THROW_NEXTMODE` VALT.

Replay: log16 exact, log20 near-exact while armed (0.026 m/s). Logs 15,
18 and 22 diverge before arming and give no A/B: the logger dropped
80,000-166,000 messages while disarmed (ATTITUDE_FAST from log18, with SD
write stalls), and log15 has logging holes (`LOG_DISARMED` 0). The
flights themselves dropped nothing.

Worked as designed:
- #32475: drops detected 0.24-0.25 s after release; upright from up to
  137 deg in under 0.95 s; no motor saturation.
- #32270 VALT: climb rate 0.20 m/s rms against demand, height 0.08-0.12 m
  rms against target.
- #33359: range finder height below 1.5 m, handing back at 1.50-1.54 m
  and taking over at 0.97-1.03 m, steps 0.05-0.15 m; within 0.10 m rms of
  the range. Replay of log16 without it: takeoff height 0.92 m below the
  range.
- #33478: Replay with bit 4 off, climb-rate error against the range rate
  +0.30 -> -0.03 m/s (log20 VALT hover), +0.11 -> +0.02 (log16 descent);
  one window slightly worse (-0.01 -> -0.08).
- #34457 (floor held 0.10-0.13 m at all four touchdowns), #32232,
  #32768 (refused correctly at the hand-held throw arms), #32553, #34362,
  #34942 (reset at rest; a height-error pre-arm refusal while carried was
  right), #34057 (yaw within 0.5 deg of the compass on the ground).
- Above the range finder (log16, out of range high 251.8-265.0 s):
  #33585's flat-ground height tracked the range within 0.1-0.9 m and
  aiding held; #34380 flew 86.6 s above master's 9.5 m cap; #33569 scaled
  the gains exactly 12/h above 12 m.
- #33318: this airframe's drag fits 0.15-0.21 per m/s, so the 0.2 flown
  from log18 is right; the stops at 0.01 overran 0.15-0.40 m.
- Fast rates at 2 kHz with no drops, rate targets interpolated; #33366,
  #30841, #33990, #32238 as designed.

Problems in the code:
- #33484: on log22 the flow sensor stopped tracking in LOITER at 1.4 m
  (its speed fell from 2.2-2.6 to 0.2-0.5 m/s in one step at quality
  87-93, the lean unchanged) and five lockout resets in 2.4 s each set the
  correct ~2.7 m/s to the sensor's ~0. Each was followed by a 43-55 deg
  pitch, and the pilot disarmed in the air. The same on log20 (an
  all-zero frame) and log15 (an oscillation sample). `EK3_FLOW_QMIN`
  cannot catch a sensor that reports good quality.
- The throw's hand-off counted VALT as needing a position (the beta's own
  `Copter: let throw mode hand off to VALT`): with none after the drop on
  log20, it held level with no horizontal control for 1.7 s at 0.1-0.3 m,
  and would have stayed in THROW after 5 s.
- #34941, a hypothesis: carried at about 58 deg for 12 s on log20, flow
  was withheld without a timeout and the EKF dead-reckoned to 3.7 m/s;
  master would have timed out and restarted at zero.
- Smaller: a 5.39 m height drop onto a ground-effect baro in log20's
  crash landing (range lost with `touchdown_expected` clear; Replay
  reproduces it); log15's EKF following prop-wash baro to -8.4 m while on
  the ground with the props spinning, cleaned up by #34942; #33478's
  switch-on after a 1.4 s gap overshooting 0.47 m.

Not exercised: #32401, #31005, #34210, #34696, #32972, #32471, #34292,
#34120, #34251, #33879, #34456, #34543, #34363, #32514, #32397, #33568,
#34209, #34678.

Setup advice given: the flow sensor did not track in the later flights
(light or surface first); ARK Flow node dropouts of about 1.1 s;
`EK3_FLOW_MAX` 7.4 is above its range and the EKF caps flow at 4.2 rad/s
anyway; no compass motor compensation (a 23 deg yaw reset);
`INS_HNTC2_REF` 1 moved the second notch off its 172 Hz line;
ATTITUDE_FAST logging overruns the card.

Fixes, 2026-11-10, the first two on the beta as refresh14 topup1
(`8449cb6dcc`..`654ae03aa9`, `REFRESH_NOTES.md`):
- Throw into VALT: `nextmode_requires_position()` lists VALT with
  ALT_HOLD (local branch `fix1110/throw-valt`, `42353f7c05`, test
  `ThrowDropNoPositionNextModeVALT` `9dc9ebc818`). The test reproduces
  log20's "waiting for position" and "staying in throw" without it; all
  21 throw tests pass with it.
- #34941: the withheld time counts against the flow timeout only while
  `get_time_flying_ms()` says the vehicle flies (`fix1110/34941-beta`
  `9b76ccf4cb`, test `6feb2001ad`; on the PR as `fix1110/34941`
  `9609f96a0d`, test `2e71d2c61b`, both with `amend!` commits to
  autosquash). Replay of log20 confirms the carry: reverted, the lane
  times out at 102.3 s and keeps relative position, against 4.0 m/s and
  7 s without one as flown. It does not explain the "lost position":
  neither build had a position at 111.6 s. Logs 16, 61, 63, 65, 84, 87
  and 88 replay identically with the fix.
- #33484: no candidate survived the Replay set (logs A, B, C, SFD-O4 7,
  65 and 73, the T25's, logs 16 and 20 here). Deferring a reset while the
  gap grows faster than the accel bias allows removes the bad resets but
  lengthens the phantom logs' errors (log C: 1.5 to 8.0 s over 3 m/s).
  Not fixed; the narrow null-frame guard (log20's 124.1 s reset) and
  AC_PosControl absorbing velocity resets are the open options.
- Throw with THROW_ALT_DCSND 0 (found 2026-11-11): all three tosses were
  uprighted after the top of the climb at 3.2-3.8 m and flown 1.7-3.4 m
  back down to the release point before the hand-over, at up to 3.9 m/s,
  to 0.47 m above the ground on log18 and 0.13 m on log20. The release
  point is the EKF height when freefall began, which in the hand on log18
  was 1.8 m below the range finder's. Fixed on #32475 (`f4fcfb44ba`, test
  `3f8d1623ee`) and on the beta as refresh14 topup4 (`01f474c138`,
  `9d2cdae5a1`): with 0 the arrest starts from where uprighting ends and
  the height it stops at is held, never below the release point. SITL:
  handed over 1.5 m below a 5.9 m top against 2.9 m.

## 2026-11-08: the T25 on `7b00f3b5`, a flow quality latch indoors

The T25's first flight on refresh13 code: `7b00f3b5` is the beta head,
whose code is refresh13 topup1's (`896e11846d`). It flew on the T25's
first board (`002F003C`), not the second the card named, with
`EK3_RNG_USE_HGT` -1 and `BARO1_THST_SCALE` -207, indoors in dim light:
VALT 9.5-41.7 s, then VALT and a Loiter 56.2-332.6 s. Replay follows the
flown core to 0.000 throughout.

- Loiter held: target minus estimate with the sticks centred, median
  0.10 m, p95 0.36 m. VALT takeoffs 1.7-1.8 s. IMU0 VibeZ median 5.2
  against 20.7 on this board's 11-06 flight 06, no clips.
- `EK3_FLOW_QMIN` latch and EKF failsafe: flow quality 12-30 from 326 s,
  with the sensor reporting zero flow while the vehicle moved; at 331.63 s
  an axis lockout found quality 23, under `EK3_FLOW_QMIN` 60, aiding
  stopped, and the failsafe went to VALT at 332.58 s. Loiter was refused
  at 347.2 s and the vehicle landed at 353.7 s; the release, quality 66
  for 2 s, never came in the air. Replayed with `EK3_FLOW_QMIN` 0 the
  reset fires, the core keeps relative aiding, and its velocity is within
  0.03 m/s of the latched core's from 333.3 s on, so the latch cost 21 s
  of Loiter. Below quality 40 the flow did not follow the IMU (slope 0.19,
  correlation 0.13), so the setting stays.
- The flow measured poorly in this room at every quality: against the
  IMU, correlation 0.43 at quality 70-99, where the 11-06 flights read
  0.94-0.96. The light or the floor, not the code.
- Baro creep: the EKF height fell 1.2-1.5 m behind the range finder over
  the Loiter, at a steady 48 C and steady throttle, and the pilot lowered
  the target by about 1 m to hold height. The AGL KF stayed on the range
  (0.18 m rms).
- Relative aiding began at 20.0 s, 8.9 s after the first liftoff: the
  arm came 2.6 s after the EKF started, and of the flow start conditions
  only the gyro bias variance test was still open (from the source, by
  elimination). VALT needs no position.
- #34942: a re-arm 2.8 s after a soft landing, without a reboot, was
  accepted and flew. VD read -0.03 m/s at rest, so the reset at rest had
  nothing to do; the re-arm after a hard landing is still owed.
- #30841: ESC logged at 24.8 Hz armed, `ESC_TLM_LOG_RATE` 25. #31274:
  ESC.Err read 100 until the window filled, 1 s after the first arm, and
  at or below 1% after that.

## 2026-11-07: squash audit of SFD-O4 logs 73-88 and the T25 on topup9 and topup11

Which commits on top of Andy's PRs these flights ran, and so may fold into
the commit they fix (rule as on 10-31). Flights: SFD-O4 logs 73-83
(topup11 `52ec3c77`; 82-83 on topup12 code) and 84-88 (refresh13 topup1,
`896e11846d`); the T25 on topup9 (`a8487f0` 01-08) and topup11 (`52ec3c7`
01-06 and 08, the second board's `52ec3c77` 05-07). Evidence is a message,
event or log field only that path produces, or Replay of the as-flown
build with the change removed. Per-PR detail is in
`../ardupilot-pr-analysis/<PR>/README.md` under 2026-11-07.

| PR | Folded | Stays on top, and why |
|---|---|---|
| #34942 | the fusion-horizon wait into the reset at rest (log78; T25 topup11 02, the reset one sample later); the three gyro-bias-across-reset commits into one (T25 topup9 02 and 06, the forced resets); tests | the 2 m clip and the 5 s yaw-fusion store never ran; the yaw-reset retake flew but stays its own commit, as it calls the structure-only commit's function |
| #33478 | the settled-start rule and its doc into the step hold (logs 84-87: core 1's VFuse differs without it, velD against GPS 0.156 against 0.162 m/s rms on log84); tests | |
| #31274 | the 250-frame window into the percentage commit (exactly 0.400% armed on log80, log82 and T25 topup9 01) | the stale latch: no serial ESC telemetry, no 71.6 min log |
| #30841 | the doc into the logging commit (25 Hz on logs 84-88) | |
| #31005 | the fix into its base, the test fixup into the test: neither has flown (`FENCE_ENABLE` 0 everywhere) | |
| #34457, #33507 | test fixes; #33507 restacked | the fused floor flew and is verified, but folding it moves the structure commit: left for the merge squash |
| #34941, #32475, #32471, #32473 | test and doc folds; #32473 restacked | #32475's two fixes: no Throw; #32471's four first-flight fixes ran only the common path (Andy's call to fold them into the rule) |

Pushed 2026-11-07 in one atomic push to `andyp1per`, each with its old
head as the lease, every tree unchanged (backups `pre-squash/<PR>-1107`):
#34942 23 to 17, #33478 11 to 7, #31274 14 to 13, #30841 3 to 2, #31005 5
to 3, #34457 16 to 12, #33507 19 to 14, #34941 3 to 2, #32475 34 to 32,
#32471 68 to 65 and #32473 72 to 69. Commit messages on #34942, #32471,
#34457 and #31274 no longer name the airframes.

Not squashed: #32401 (under peterbarker's review; log86 ran `026ae77fc2`,
one "Arm:" line and no "PreArm:" copy), #32270 (`ee9dc05040` half flown;
the split is Andy's call), #33585 (`8bb9b58062` flown, its target not,
under rmackay9's review), #32972 (`13fe97b7ea` never ran: its one AID_NONE
entry was disarmed), #32514 and #34696 (their cases never occurred),
#34292 (no disarmed flow gap over 1 s), #34543 (no DCM fallback), #34630
(bench check 0d owed), #33359, #29000, #34210 and the rest.

#33484's other-axis gate (`6bbe92baae`), Replay of the T25's topup11
flights with it removed: on 03 it blocked a reset at touchdown (68.2 s),
and core 0 then ran from 0.54 to 2.42 m/s on the landed, then disarmed,
vehicle, 10.3 m by 75.5 s; without it 0.04-0.08 m/s. On 02 it delayed a
reset on the ground by 0.6 s (0.39 against 0.03 m/s). It has acted four
times on the T25, twice worse, both on the ground. It stays on top and
needs a fix or removal.

Replay does not reproduce the arm-pending bootstrap resets: T25 topup9
01, 02 and 05 diverge from them, so those windows are not evidence.

## 2026-11-07: SFD-O4 log88, an RTL on the flow lane flew away from home

Firmware as log84, boot 701, `LOIT_DRAG_COEF` still 0. Source set 2 by
switch at 135.7 s ("EKF3 lane switch 1") and never switched back, so core
1 on flow flew the Loiter at 162.7 s, the ACRO to 298.5 s and the RTL by
switch (RC8, mode reason 53) at 298.5 s. The battery failsafe at 300.5 s
read 19.76 V after 1101 mAh (warning only).

The RTL flew to core 1's home. At 298.5 s the vehicle was 63-67 m from
home, home bearing 101-107 deg (GPS, core 0); core 1 put it 108-110 m
away at 295-299 deg, its position 173 m off. RTL flew 290-294 deg at 30
m/s (`RTL_SPEED_MS` 0, so `WP_SPD` 40), core 1 read itself home at 303 s
with the vehicle 163 m from it, and it held there until the pilot took
over at 308.2 s. Heading and velocity followed core 1's position as
designed; the position was the error. Core 1's position error against
core 0: 26 m at 130 s (before the switch), 49 m at 160 s, 77 m at 220 s,
145 m at 265 s, 296 m at 280 s, 182 m at 295 s.

Mechanism, from logs 84, 87 and 88 and Replay of 87 and 88 (each
reproduces the flown cores exactly): EKF3 fuses no flow above 45 deg of
tilt (`DCM33FlowMin`, `AP_NavEKF3_OptFlowFusion.cpp:99`), so through
sustained ACRO the flow lane runs on the IMU alone, its innovations frozen
for up to 16 s. Its velocity error against core 0 grows while tilted and
shrinks while level:

| | over 45 deg | growth tilted | shrink level | mean error tilted / level |
|---|---|---|---|---|
| log84, 211-422 s | 107 of 211 s | +0.12 m/s per s | -0.09 m/s per s | 1.3 / 1.2 m/s |
| log87, 105-205 s | 68 of 100 s | +0.28 | -0.77 | 7.7 / 4.4 |
| log88, 136-299 s | 98 of 163 s | +0.12 | -0.11 | 4.1 / 2.4 |

The position integrates it. Ruled out by Replay, each moving core 1's
median velocity error by 3 % or less: the terrain-database flow height (`EK3_OPTIONS` 90
against 94), `EK3_FLOW_DELAY` 30, 50 and 70 against 10, and a throwaway
probe holding the flow gyro bias at zero or learning it only below 0.5
rad/s (the bias is learned against the delayed IMU rate, upstream too).
Ruled out by the log: yaw (rotating core 1's velocity by the yaw
difference leaves the error unchanged, correlation 0.11 and -0.29), and
dead reckoning (core 1 never set XKF4.SS bit 18, so `FS_DR_ENABLE` 2 did
not act; were it to on this lane, its RTL would go to the same wrong
home).

So an RTL, or any mode flying to home, on the flow lane after ACRO goes to
wherever the lane has drifted. Switch to source set 1 before it.


## 2026-11-07: SFD-O4 log87, ACRO with the flow lane primary

Firmware as log84, boot 699, `LOIT_DRAG_COEF` still 0 from log86. Armed by
switch at 92 s; ACRO, source set 2 by switch at 107.3 s ("EKF3 lane switch
1", core 1 on flow primary) and back to set 1 at 203.7 s, a Loiter on core
1 from 189.2 to 197.4 s, an RTL by switch (RC8, mode reason 53) from 253.2
to 273.9 s on core 0, landed in Loiter and disarmed by switch at 291.9 s.
Mostly 17-68 m up, above the 15 m range finder. The battery failsafe at
272.3 s (22.11 V, 1100 mAh) was the warning only.

- #34942: XKF1.VD -0.01/0.00 m/s at the arm, no refusal, no reset after
  the landing. No re-arm.
- #32471: no refusal messages; saved 0.090/0.076 (log86 saved 0.055/0.052,
  log85 0.004/0.002). `ACC_ZBIAS_LEARN` 9 learns without applying.
- #34457: XKFA.HAgl 0.05 m on both cores at the landing, disarmed at once.
- #34941: six stretches over 45 deg for 6.8-18.2 s, all with the range
  finder out of range. Core 1 stayed in relative aiding throughout, with no
  lockout reset (XKF7.FVC 0). Above 5 m/s its speed was a median 2.6 m/s
  from GPS (95th percentile 8.1, worst 10.7), mostly high, against 0.17 /
  0.74 / 2.15 for core 0; log84, flown lower, read 0.8 / 2.3 / 4.1.
- #34630: one "EKF3 IMU1 flow recovery deferred, range stale" at 290.9 s,
  at the touchdown, with no reset.
- Open: the Loiter on core 1 at 16-20 m did not hold. Entered from ACRO at
  15.5 m/s with core 1 135 m east of core 0, it slowed in 3 s, then with
  the sticks centred drifted west at about 1.5 m/s (GPS, core 0) for 5 s,
  7 m, until the pilot took over. Core 1 read 1.1-1.3 m/s north against
  0.2-0.4 from GPS, and its position moved south at 2 m/s against its own
  velocity, taking Loiter's target with it. Mechanism not established; it
  resembles log61's Loiter on the flow lane above the range finder
  (`../ardupilot-pr-analysis/33585/`). Replay of this log can show which
  fusion moved core 1's position.
  (Replay, the same day, at the flown code, which it reproduces exactly:
  the flow sensor measured the drift correctly, 0.5 s averages of raw
  flow within about 0.4 m/s of GPS in the body frame. Core 1 had no
  position fusion; its flow corrections moved its position, while its
  velocity kept an error built up before the Loiter, falling from 1.8 m/s
  at 191 s to 0.7 m/s at 197 s. That error came from the ACRO: see log88
  below for the mechanism, measured on both logs.)


## 2026-11-07: SFD-O4 logs 85-86, LOIT_DRAG_COEF 0.3 against 0 outdoors

Firmware as log84. Outdoors on GPS in Loiter, `LOIT_SPEED_MS` 15,
`LOIT_ANG_MAX` 45, `LOIT_BRK_DELAY` 0.3, one flight each: log85 at 0.3
(pack at 24.1-23.9 V), log86 at 0 (the same pack after it, 23.7-23.2 V).
Three hands-off releases from 10.7-12.6 m/s in each, nose first, two
eastbound and one westbound in each log, 5.8-8.8 s hands off. Along the
direction of motion, from PSCN/PSCE:

| | log85, 0.3 | log86, 0 | SITL, 0.3 / 0 |
|---|---|---|---|
| ahead of the target at the release | -0.22 to -0.39 m | +0.80 to +0.83 m | |
| most ahead of the target in the stop | 0.15-0.47 m | 1.25-1.65 m | |
| most backward speed after the stop, EKF / GPS | -0.01 to -0.06 / 0.00 m/s | -0.32 to -0.83 / -0.34 to -0.80 m/s | -0.00 / -0.36 m/s |
| back past the target | none | 0.56-0.79 m | |
| creep forward afterwards | none | 0.14-0.23 m/s for 3-5 s | |
| position at the end of the window | 0.08-0.15 m ahead | 0.23-0.41 m behind | |

At 0.3 the vehicle followed Loiter's desired speed down, under 0.3 m/s
2.4-3.3 s after the release, then settled onto the target over the next
2-3 s. At 0 it flew 0.4-0.6 m/s above the desired speed in the push, so
it was 0.8 m ahead of its target at the release; it stopped about 2 s
after the release while the desired speed was still 1 m/s, rolled back
past the target and crept forward again. That is the pattern SITL
predicts for 0. The eastbound roll-backs (0.76, 0.83 m/s) were larger
than the westbound (0.32); wind was not measured. Decision: keep 0.3.
log84's 1.6 m run past at 0.3 came on an entry into Loiter from ACRO,
not a release, and none of these releases repeats it.

- #32401, log86: the switch up at 28.6 s with no position gave "Arm: Need
  Position Estimate" once and "Arm pending"; the switch down at 29.7 s
  gave "Arm pending cancelled"; the switch up at 33.1 s, 0.45 s after the
  EKF started using GPS, armed at once. The 3 s repeat and the cancel by
  a GCS disarm were not exercised.
- #32471: saved 0.0038/0.0017 on log85 (armed 5.8 min after power-on)
  and 0.055/0.052 on log86 (armed 33 s after), with no refusal messages.
  `ACC_ZBIAS_LEARN` 9 learns without applying.
- #34942: one arm per boot, no refusals or resets. The re-arm after a
  landing is still owed.


## 2026-11-07: SFD-O4 log84, refresh13 topup1, an outdoor acro sortie

Firmware reporting `ea9c6ab5` (topup1's records commit, the same code as
`896e11846d`). One flight outdoors, core 0 on GPS and primary throughout,
core 1 on flow (`EK3_SRC_OPTIONS` 8): armed by switch in Loiter at 201 s,
211 s of ACRO up to 50 m and 27 m/s, Loiter from 422 s, landed at 451 s.
The card's settings were in: `LOIT_DRAG_COEF` 0.3, `LOIT_OPTIONS` 1,
`EK3_OPTIONS` 94, `ESC_TLM_LOG_RATE` 25; `EK3_FLOW_QMIN` was 40 and
`ACC_ZBIAS_LEARN` 9.

- #30841: ESC logged at 25.0 Hz per motor armed and 24.9 Hz over the
  whole log, disarmed included. Passed. ESC.Err in flight at most 0.24 %.
- #34942: XKF1.VD 0.00 m/s on both cores at the arm, no "height error"
  refusal and no reset at rest. One landing and no re-arm, so the re-arm
  case is still owed.
- #32471: "Hover Z-bias not learned: no steady hover", as expected for an
  acro flight.
- #34457: XKFA.HAgl held 0.05 m (core 0) and 0.1 m (core 1) on the ground
  after the landing. (Corrected the same day: 0.05 m on core 1 as well;
  the 0.1 was a once-a-second table's rounding.)
- Flow lane (#34941, #34630): core 1 stayed in relative aiding the whole
  flight, mostly above the 15 m range finder, with no lockout resets
  (XKF7.FVC 0) and no "stopped aiding". Two "flow recovery deferred, range
  stale" (303.6 s, 324.3 s) passed without a reset, and core 1 read within
  0-3 m/s of GPS at 10-27 m/s. (Corrected the same day: that was sampled
  once a second. At every GPS sample above 5 m/s core 1 was a median 0.8
  m/s from GPS, 95th percentile 2.3, worst 4.1, against 0.15 / 0.6 / 2.2
  for core 0 on GPS, which is the timing between the two.)
- The arm-time datum reset re-anchored to GPS: XKF1.PD went from 0.03 to
  -1.86 m, the GPS drift over 3 min on the ground (origin 119.64 m, GPS
  121.5 m at the arm), and the origin did not move. As designed (#32768).
- #33585's terrain anchor cannot run here: `EK3_OPTIONS` bit 2 makes
  `terrainAnchored()` false.
- #33318: no A/B. One hands-off stop at 0.3: entering Loiter from ACRO at
  7.7 m/s (422.1 s, sticks centred until 427.3 s), the vehicle ran 1.6 m
  past the Loiter target, stopped at 425 s, then slid back towards it at
  up to 0.37 m/s (EKF; GPS 0.2-0.26) until the pilot took over. SITL with
  the O4's drag predicts no roll-back at 0.3. One stop, entered from ACRO
  rather than released in Loiter, with no run at 0 to compare: it decides
  nothing. The 11 m/s stop at 432 s had stick inputs through it.


## 2026-11-06 (evening): T25 on a new flight controller, topup11 (52ec3c7)

TD moved the T25 to a second MicoAir743v2, freshly calibrated with
temperature calibration, and flew three logs on topup11 with the EKF on
IMU0 alone (`EK3_IMU_MASK` 1, `EK3_OPTIONS` 92): 05 a hover Z-bias learn
(ACC_ZBIAS_LEARN 3, STABILIZE then ALT_HOLD), 06 VALT for 74 s then Loiter
for 61 s, 07 five switches between Loiter and VALT. TD's report was very
positive.

- Vibration on IMU0 is a quarter of the old board's (VibeZ median 4-7
  against 21), and 05 learned and saved 0.023 m/s/s on its first flight
  against 0.056-0.066 on the old board: less vibration, less rectification.
  06 and 07 flew on it.
- No clipping in flight: 06's 10 clips and 07's 12 came at the touchdowns,
  each followed by one flow velocity reset on the ground. Flow quality
  never fell under 40 (medians 110-129). Loiter held within 0.30-0.32 m
  (95th percentile) with the sticks centred; the AGL KF height followed
  the range finder to 0.14-0.21 m rms.
- VALT's takeoff is faster than on the old board (stick up to 0.3 m:
  2.4 s on 06, 1.6 s on 07, against 4.1 s) but the cause is the same: in
  spool-up the thrust-compensated baro swings -3.4 m then +4.7 m with the
  quad on the floor, so on 06 (BARO1_THST_SCALE -175) the EKF climbed to
  2.2 m with the range at 0.29 m and VALT read a 1 m/s climb that was
  0.4 m/s. 07 (-205) lifted within a second. EK3_RNG_USE_HGT 6, which
  Replay showed fixing the old board's flight 06 takeoff, is not yet set.
- Replay with EK3_RNG_USE_HGT 6 (tier 1b; topup11 and refresh13 Replay
  agree, and both reproduce the flown core exactly without it). With
  RNGFND1_MAX 8 the range is the height source below 0.34-0.48 m, so only
  around takeoff and landing. EKF height minus range over the takeoff:
  1.52 -> 0.09 m rms on 06, 0.49 -> 0.02 on 05, 0.38 -> 0.21 on 07. The
  climb rate VALT reads at 06's liftoff drops from 0.6-0.9 m/s to
  0.24-0.31, against the range finder's 0.17-0.29. On 07 the switch back
  to baro at 0.48 m left the height 0.25 m low, from the baro offset
  learned during the spool-up swing; it then drifted with the baro.
  The landings showed no step. The hover stays on baro, so the creep and
  the thrust compensation offset remain (median height minus range in
  flight +0.63 -> +0.58 m on 06, +0.55 -> +0.21 on 07).
- Baro height creep in ALT_HOLD and Loiter, about 0.28 m in 20-24 s
  (range finder falling at constant EKF height); the baro calibration was
  learning (TCAL_ENABLED 2), and the new board's BARO1_THST_SCALE has not
  been calibrated.


## 2026-11-06: SFD-O4 logs 81-83, an ALT_HOLD creep and LOIT_DRAG_COEF indoors

### log81: the vehicle climbed 0.6 m while the flow core's height stood still

Firmware `52ec3c77` (topup11), a hover Z-bias learning flight
(ACC_ZBIAS_LEARN 11): about four minutes powered on the ground, then 55 s
of ALT_HOLD at 1-1.7 m. EK3_OPTIONS 94, EK3_ALT_M_NSE 1, EK3_RNG_USE_HGT 6
(baro height above 0.9 m), EK3_AGL_ABIAS_P 0.05. The pilot saw it creep up
by about 20 cm at a time.

From 272 to 309.5 s the throttle stick sat in the deadband and the flow
core (core 1, primary) held its height at 1.51-1.56 m against a 1.61 m
target, while the range finder rose from 1.14 to 1.76 m in steps, the baro
from 0.80 to 1.24 m and the GPS core from 0.70 to 1.43 m. The flow core read
a 0.05-0.09 m/s climb throughout (the vehicle's own, from the range finder,
about 0.015 m/s), with a standing -0.45 to -0.74 m baro innovation. Its
vertical velocity came from EK3_OPTIONS bit 4 (#33478, the AGL KF's velocity
fused as velD; innovation -0.01), and the AGL KF's velocity ran 0.04 m/s
(up to 0.09) above the rate of its own height.

Replay (topup11 Replay, core 1 identical to the flight), flow core height
minus range finder over 272-309 s, tier 1b:

| build | drift | flow core climb, mean | AGL KF velocity error, mean |
|---|---|---|---|
| as flown | -0.49 m | 0.065 m/s | +0.041 m/s |
| EK3_AGL_ABIAS_P 0.3 | -0.54 m | 0.047 m/s | +0.012 m/s |
| EK3_OPTIONS 78 (bit 4 off) | -0.04 m | 0.121 m/s | not fused |

With the velD fusion the flow core is far more sure of its height (std 0.084
m against 0.233 m without, vertical velocity 0.029 against 0.082 m/s), so the
baro at 1 m noise moves it about a seventh as much; the velocity it trusts
is a few cm/s off, so the height cannot follow the vehicle, and ALT_HOLD,
holding that height, lets it climb. The AGL KF's accel-bias state only starts
learning at liftoff (from 0, std 1.0), claims 0.017 within 12 s, and then
moves another 0.1 m/s/s. Faster bias tracking halves its velocity error but
not the drift: the overconfidence, not the AGL KF's error, is the cause. This
is #33478's open covariance issue in flight. Turning bit 4 off removes the
creep but leaves the vertical velocity 0.1 m/s off; not recommended without a
test. Why the climb came in steps is not explained.

The IMUs warmed about 6 degC on the ground (to 40-41 degC) and cooled 12 degC in the
hover (40 to 28 degC by 310 s), and both cores' accel Z bias moved by about
0.07 m/s/s with it, the same with bit 4 off (TCAL covers 4-70 degC). That
is the bias the AGL KF was chasing, and likely why the learner's saved value
swings between first flights (here IMU0 -0.002 -> 0.013, IMU1 0.013 ->
-0.002): it samples a bias still moving. Not proven.

### logs 82-83: LOIT_DRAG_COEF 0 against 0.3 indoors

Firmware reporting `9b6b564a` (topup12 code), LOIT_OPTIONS 1,
LOIT_BRK_DELAY 0.3, daylight, ACC_ZBIAS_LEARN 9. Flow against the drag
reference 0.75-0.81 (log82) and 0.79-0.88 (log83); range medians 0.99 and
1.29 m.

| | log82, 0 | log83, 0.3 |
|---|---|---|
| peak speed / desired, median | 0.99 (15 pulses) | 1.04 (14) |
| releases with 3 s or more hands off | 3, from 1.5-1.9 m/s: roll-back +0.03, +0.00, -0.07 m/s | 1, from 0.5 m/s: -0.09 m/s |
| target minus position after a release, median of means | +0.15 m | +0.06 m |
| releases running past the target | 4 of 13 | 3 of 12 |

No difference that these speeds can show, and nothing worse: with 0.3 the
vehicle stopped and stayed, no creep forward. log82's longest stop, from 1.5
m/s, ran 0.11 m past its target and settled 1.62 m from the release (about
1.1 m per m/s at LOIT_BRK_DELAY 0.3). Still too few long hands-off
releases. The outdoor test at 8-10 m/s decides it.


## 2026-11-06: SFD-O4 logs 79-80, the #33318 A/B in daylight

Firmware `52ec3c77` (topup11), indoors in daylight, the topup11 card's pack
1. log79 `LOIT_OPTIONS 1` (#33318 on), log80 `LOIT_OPTIONS 65` (#33318
off, the drag back in the feed-forward as 4.7 has it), one boot each,
LOIT_SPEED_MS 15, LOIT_ANG_MAX 45, LOIT_BRK_DELAY 1. Both flown the same
way: short pitch pulses (0.2-0.5 s, about a third of full stick) every 2-2.5
s, back and forth along one line, peak speeds 1-1.7 m/s, at 0.75-0.85 m
(log79) and 0.8-1.2 m (log80), not the card's 1.5 m and 3 s pushes.

The flow saw the floor: against the rotor-drag reference it read 0.72-0.85
of the speed on both (the two regressions bracket it, correlation 0.94),
against 0.44-0.58 on log73's flicks at dusk at about the same height
(0.48 against the IMU). Loiter held.

| | log79, 1 (#33318 on) | log80, 65 (#33318 off) | SITL, 1 / 65 |
|---|---|---|---|
| peak speed / desired, median of 19-20 pulses | 1.06 (0.92-1.17) | 1.11 (0.94-1.23) | 1.08 / 1.44 |
| roll-back after the longest hands-off release | -0.07 m/s (21 s hands off) | -0.14 m/s (with a +80 us counter nudge) | -0.08 / -0.40 m/s |
| target minus position along the motion, 2 s after a release, mean | +0.10 m (18 releases) | -0.00 m (10) | - |
| releases running past the target | 5 of 18 | 4 of 10 | - |

So with the drag back in the feed-forward the O4 runs about 0.1 m further
ahead of its target as it stops, the direction SITL predicts, but at these
speeds the difference is a tenth of SITL's and neither flight pulls back.
At these settings Loiter's drag model (g tan(LOIT_ANG_MAX) v / LOIT_SPEED_MS,
0.65 m/s/s per m/s) has about twice the O4's drag (0.28-0.37 from 2 to 10
m/s, log63's GPS lane, wind not separated), which shrinks the difference
#33318 makes. The pilot saw no pull-back on either flight and a long forward
drift after each release on both: the commanded stop (desired velocity
decaying on the modelled drag alone for LOIT_BRK_DELAY's 1 s, then the
brake), followed within 0.35 m, and about a third longer in reality than
the EKF's, as the EKF's speed reads 0.76-0.77 of it here. Decision: keep
#33318 (LOIT_OPTIONS 1); the A/B that matters is at speed (`FIELD_PLAN.md`).

The learner saved on both (each a new boot): log79 IMU0 0.029 -> 0.039,
IMU1 0.019 -> 0.040; log80 0.039 -> -0.002, 0.040 -> 0.013. Across the
three first flights of 11-05/06, IMU0 saved -0.002 to 0.039, a spread the
size of the correction; each flight held only about 7 s of steady hover
before the pulses.


## 2026-11-06: T25 on topup11 (52ec3c7), two ceiling strikes on IMU1

TD flew five logs on topup11 with the EKF moved to IMU1 for the first time
(01-02: INS_USE 0 with EK3_IMU_MASK 1, which runs core 0 on IMU1's accel
and gyro; 03-04: INS_USE 1, EK3_IMU_MASK 2; 05: mask 3, core 0 primary
throughout) and INS_FAST_SAMPLE changed from 1 to 2. Every earlier T25 log
ran the EKF on IMU0. 02 and 04 climbed into the garage ceiling; 03's
ALT_HOLD wandered; 05 flew normally.

Cause: IMU1 is a BMI270 (IMU0 a BMI088), and its accel reads the Z
specific force low by an amount set by motor RPM: about 0 below 24k, 0.3
to 1.0 m/s/s at 26k, 3 to 7 at 28k, 8 to 11 at 30-34k, paired sample by
sample against IMU0, which agrees with the range finder. On the ground
they agree within 0.03. The curve is the same in the 11-03 (722d77b) and
topup9 logs, where nothing flew on IMU1, so it is the sensor, not this
week's firmware; its logged VibeZ stays at 2-3 while IMU0 reads 20-28,
so the vibration figure does not show it. Hover sits at the knee (24-26k),
so any climb makes IMU1 read less lift, the EKF sees sinking and the
height controller adds throttle: positive feedback. 02 (VALT, 35.5 s):
throttle 0.64, IMU0 -16.3, IMU1 -8.9 m/s/s, into the ceiling. 04: an ACRO
punch at 62 s gave the IMU1 EKF a 5.5 m/s false descent, it rejected its
height measurements and believed itself 36 m below the floor for 8 s,
and ALT_HOLD at 71.4 s climbed on that. The learner's IMU1 values (0.010,
0.176, 0.120, 0.199) are this curve sampled at each flight's hover RPM;
IMU0's (0.066, against 0.062 and 0.056 on topup9) are consistent.

Firmware ruled out by Replay (topup9 `a8487f0` and topup11 builds):

- topup11 reproduces the flights exactly; topup9 on the same IMU1 data is
  as wrong (height against the range finder 4.69/1.69/8.98 m rms on
  02/03/04, against 4.58/1.69/8.90). On 02 the two IMU1 EKFs take
  opposite wrong paths from the failed first takeoff (baro thrust swing
  of +-4 m on the floor), so topup11 fuses the AGL velD at 28.5 s (test
  ratio 0.35) where topup9 rejects it (1.3-1.8). Not a new failure.
- On IMU0 (EK3_IMU_MASK 1 forced) the same flights hold: 03 0.22 m rms,
  04 1.27 m, identical on both builds.
- 01-02 logged no IMU0 Replay data (INS_USE 0), so their IMU0 feed was
  rebuilt from the 200 Hz IMU messages (filtered accel and gyro, a log
  rewriter), which on 03/04 follows the real
  IMU0 Replay to 0.07 m
  rms. On IMU0, 01 holds 0.21 m against 0.45 as flown, and 02 0.28 m
  against 1.28 from the second takeoff to the strike (24-37 s): IMU0 sees
  the climb into the ceiling (VD -4.6 m/s at 36.75 s) where IMU1 read a
  descent (+1.1). 02's failed first takeoff reads 1.8-2.4 m on the floor
  on IMU0 as well: that one is the baro, not the accel.
- TD's topup9 IMU0 flights 02-08 through topup11: same or better (05
  worst -5.1 to +0.9 m, 06 -3.3 to -1.1 m, the topup10 touchdown fix).

Open, not new: on 04's IMU0 Replay, after the drop at 62 s the AGL step
hold (#33478, since 10-08) kept velD out for 7 s with baro (2 m noise)
alone, and a 1 m/s velocity error grew a 3 m height error. Only reached
here because IMU1 had already thrown the vehicle about.
Measured 2026-11-06 (`../ardupilot-pr-analysis/33478/README.md`): IMU0
clipped in the ACRO punch and the hold started on the velocity error that
followed, not on a terrain step; by 64.25 s the error was past the AGL
velD gate, so ending the hold early would not have fused it. Without the
hold 04 reads 0.71 m rms against 1.27, but the SFD-O4 terrain-edge flights
lose what the hold bought (log40 0.32 -> 0.82 m, log44 0.43 -> 0.98 m).
Counting a step only from a settled innovation (under 0.25 m, or after a
range gap) keeps those and fixes 04, at half the hold's benefit on log37:
`505ecae715`, pushed to #33478 on 2026-11-06, and on the beta as topup13
(`c21c1d917c`).

Back to the T25's working setup: INS_USE 1, EK3_IMU_MASK 1,
INS_FAST_SAMPLE 1, INS_ACC2_VRFB_Z 0, INS_ACC_VRFB_Z 0.066 kept,
ACC_ZBIAS_LEARN 10. Plots, tables and the IMU0 rebuild are in the
private analysis repo.

Flight 06 (boot 168), back on IMU0 with those settings (INS_ACC2_VRFB_Z
left at 0.199, unused with EK3_IMU_MASK 1): VALT, Loiter, VALT, Loiter,
VALT, 175 s armed, flew well. Loiter held 0.1-0.2 m with the sticks
centred; no clipping (IMU0 VibeZ median 20).

- VALT sat on the floor for about 3 s after the stick went up. At
  spool-up the thrust-compensated baro swung to -5.9 m and then +3.6 to
  +4.1 m with the quad on the floor (BARO1_THST_SCALE -205, set in hover,
  over-corrects there). takeoff_expected only limits baro-low innovations,
  so the EKF climbed to 3.3-3.8 m with the range at 0.04-0.20 m and VALT
  held 0.40 throttle until the baro came back. Same as flight 02's first
  takeoff. Replay with EK3_RNG_USE_HGT 6, the range finder as the height
  source below about 0.5 m as SFD-O4 flies: takeoff 1.73 -> 0.06 m rms
  against the range, the whole flight 0.54 -> 0.37 m; it only acts below
  about 0.34 m, so the hover is unchanged.
- Dim light, second Loiter: OF.Qual fell below 60 at 163 s and to 12-18 at
  171-176 s. Flow against the IMU: quality 70 and up slope 1.0-1.14,
  correlation 0.86-0.97; 40-49 correlation 0.26; under 40 none.
  EK3_FLOW_QMIN only gates lockout recovery, so that flow was still fused.
  Loiter held with the sticks centred at quality 42-58 (error under
  0.15 m) and lost track under full pitch at quality 14-37 (position error
  1.2 m) before the pilot switched to VALT at 175.0 s. The sensor in the
  dark, as on SFD-O4 logs 73-77.
- The step hold, flown: two holds, at 98.8 s (range -0.31 m) and 114.3 s
  (+0.43 m), both from a settled innovation (0.02 m); EKF height within
  0.05 m through each. The settled-start rule keeps both.


## 2026-11-05: SFD-O4 log78, the topup11 card's landing hops

Firmware `52ec3c77` (topup11), indoors after dark, one boot: a 0.8 s arm
without take-off, then four hops in ALT_HOLD to about 1-2 m, each ended by
a full-rate descent to the floor (1.1-1.9 m/s at touchdown, 6.5-7.7 g peak
`IMU.AccZ`), disarm, and a re-arm 2.4-3.1 s later. Flow lane primary
(`EK3_SRC_OPTIONS` 8), `EK3_OPTIONS` 94, `ACC_ZBIAS_LEARN` 11,
`BRD_IDLE_STATS` 1. The card's pack 2, which also covers the refresh12
card's item 1 (two take-offs in one boot) and item 5 (re-arm after a
landing).

| PR | Status |
|---|---|
| #34457 / #33507 fused floor | Flown, passed. On the ground after every landing the range finder read 0-0.04 m (out of range low), so the floor path held the AGL KF: `XKFA.HAgl` 0.05-0.20 m on both cores until the next arm (card: within about 0.3 m; SITL without it 2.5 m) |
| #34942 reset at rest | Flown, ran twice. After hops 2 and 4 the main filters still carried 0.99-1.01 and 0.50-0.52 m/s of descent on the floor (PD sinking 0.7 m on core 0); "vertical velocity reset at rest" took `XKF1.VD` to 0.00 0.1 s after each disarm, on both cores after hop 2 and core 0 after hop 4. No "height error" refusal on any re-arm |
| #33366, #31274 | Four take-offs in one boot, none held ("Takeoff blocked" or "ESC RPM or errors out of range"); `ESC.Err` at most 0.02-0.03% on every motor |
| #32471 first-flight rule | Hop 1 saved (`INS_ACC_VRFB_Z` 0.0256 -> 0.0292, IMU1 0.0147 -> 0.0188) after an 11 s flight; hops 2-4 "Hover Z-bias not saved: not first flight" |
| #32553 | "terrain offset reopened after takeoff" at every take-off |
| #33484 | "flow recovery deferred, range stale" just before two touchdowns, as on the October flights |

The compass's in-flight "ground mag anomaly" re-alignment fired again during
hop 1, as on log73 (the compass reads 15-25 deg off near the take-off
spot). The other topup6-11 changes did not
run: no forced EKF reset, no EKF accel bias reset, no flow lane loss of
aiding at touchdown.


## 2026-11-05: SFD-O4 logs 73-77, indoor Loiter drift: the flow sensor cannot see the floor at night

Firmware `52ec3c77` (topup11). log73 is the topup11 indoor card's pack 1
flight 1 (`LOIT_OPTIONS` 1, `EK3_FLOW_QMIN` 40, flow lane primary), flown
at 15:57 GMT; log74 to log77 are props-off hand carries after dark, on
the same semi-glossy floor under the room's overhead lights.
The full record is in the private analysis repo.

log73: Loiter at 0.9 m held to about 0.1 m until a 0.2 s backward flick at
166.7 s; after it the vehicle kept going the way it was released and would
not hold, landed at 174.5 s. The cause is the measurement, not the code.
Against rotor drag (body-X specific force, slope calibrated on log63's GPS
lane) the flow read 0.44-0.58 of the fore-aft speed through the flicks, and
0.48 against the IMU.

The hand carries measure it directly, against a strapdown of IMU0's Replay
deltas between still points (pass lengths 3.5-6.1 m, the pilot's "about
5 m"). Looking straight down at the floor the flow read 0.08-0.37 at 0.8 m,
erratic and sometimes backwards, and 0.38-0.68 at 1.7 m. Rolled about 35 deg
(log75) it read 0.74-0.91; with the node's IR LED taped over (log76) it was
unchanged (0.08, 0.15). With two rugs on the line (log77, LED on, level,
0.8 m) it read 0.77-0.96 over the plain cream rug on both passes, in both
directions, and around zero over the red patterned rug and the floor. Flow
quality looked normal throughout (110-183). The same floor read 0.88 in
June daylight (log276, this building); outdoors this sensor reads 1.02
(log63). So it is the floor at night: a semi-gloss finish with little
texture of its own, whose reflections the sensor tracks once daylight no
longer lights the floor (inferred). Indoor flow at night needs a plain matte
mat under the flying area.

Ruled out on log73, by Replay unless stated: the in-flight "ground mag
anomaly" yaw reset (18.5 deg at 167.1 s; the compass is 15-25 deg off about
1.3 m from the take-off spot) and the compass altogether, `EK3_FLOW_QMIN` 0
(identical output), `EK3_FLOW_DELAY` 40-120, `EK3_OPTIONS` bits 3 and 4,
`EK3_FLOW_M_NSE` 0.15, either IMU; no clipping; loop, fast-rate, CAN and
flow message timing as in June and October; node gyro against the FC 0.97-0.99.

| PR | Status |
|---|---|
| #32471 first-flight rule | Saved at the disarm (`INS_ACC_VRFB_Z` 0.0301 -> 0.0256, IMU1 0.0244 -> 0.0147), no "not saved": the flight after a 0.9 s arm without take-off, 121 s after the EKF started |
| #34942 | Two arms, no "height error" refusal, no reset at rest |
| #33318 / `LOIT_OPTIONS` bit 6 A/B | Not readable: flicks, not the card's held stick, and the flow under-read. Re-fly in daylight |
| #34457 fused floor | Not exercised: disarmed at touchdown |


## 2026-10-31: squash audit of logs 66-71 and the T25 on 0cb49e7d

Which commits on top of Andy's PRs these flights ran, and so may fold into
the commit they fix. Evidence is a message, event or log field only that
path produces, or Replay with the change removed against the as-flown build
(refresh12 `f93bd10a6f`; for the T25, Replay rebuilt at `0cb49e7dfb`). Full
tables in `../ardupilot-pr-analysis/squash-audit-2026-10-31/`. Nothing
squashed yet.

| PR | May fold now | Stays on top, and why |
|---|---|---|
| #34942 | `2a52c965e2` into G `21a8e3a683` (neither ran) | static yaw fix `dc76e048a6`: commit 5 flew (core 1 yaw moves 0.14-0.29 deg at every boot without it), the fix needs a flight with no yaw source |
| #32471 | `a7fdc4d09b`, `7e0882eb10` into `f1b61c9cf8` (flown with apply, log63 and 67-71); two runs of tests | `01227784b6`, `19206748ae`, `3973add04c` never ran; v3 flew but cannot move below `01227784b6` |
| #32473 | nothing; bit 3 and `968ea7d7e5` flew (variance at its floor at every armed acro exit) | still stacked on #32471 from before v3: restack, one rerere'd conflict |
| #29000 | `fcc3339b2b` into `4cc85b68cf` (fix flown: one "rate set" at boot, two without; base needs tcal) | |
| #31274 | ESC_Telem fixes into `cb66e83fde`, motor test fixes into `a9ae4cdc82`, docs, tests | the four DShot commits until the bot's blocker is fixed; SIM_ESC_ERR_RATE and the AP_Motors helper stay separate |
| #34210 | all: nothing in it has flown (`LAND_FS_OPTIONS` 0 everywhere) | |
| #33484 | `fab41a7cf9` acted once (T25 ft3 130.22 s, a later sample), not shown better: Andy's call | `e0e4a38f57`: no PX4Flow airframe flies |
| #34380 | `4c3f1a7724` flew and held core 1's HAGL on the range at 17 m in log69 (0.05 m without); its lines are #33585's | target is #33585's `0b7027979e`, or it stays its own commit |
| #34543 | `78aa029352` into `cbda421ea8` (every OSD frame, no stall); test fixups | DCM fix `a2f1de5edd` never ran |
| #34630 | its 10-30 fixes fold freely (no display commit confirmed) | `17ead03a5e` if bench check 0d showed the floor only at 94 |
| #34292 | Rover pair `244d608769`+`b43ace7296`, Rover test fixup | rest-reading clear `19b9e2bd9a` never acted |
| #34696, #32514, #33359, #32475, #32270 | test fixups only (#32475's 25 test commits among themselves) | restore never fired (T25 pitch rests 25 us off trim, outside `RC2_DZ`); spool-down count never ran; odometry half unflown; no Throw since the 10-30 fixes; `ee9dc05040` half flown |

Squashed and pushed the same evening (`applied.lock` at `1626f01266`): #34942
10 to 9, #32471 50 to 44, #32473 restacked 50 to 48, #34543 14 to 12, #34630
37 to 33, #34292 27 to 25, #29000 3 to 2, #34210 6 to 3, #31274 30 to 13 and
#32475 54 to 28, every tree unchanged but #32473's restack. #32471's two apply
fixes folded into the flown commits that introduce what they call, not into
`f1b61c9cf8`, whose early successors would not build. The your-call items
(#33484's gate, #34380's target, splitting #32270's commit, #34630's
`17ead03a5e`) were left on top.

Nothing on top, or nothing flown to fold: #34941, #34456, #34457, #33507,
#33478, #33585, #33568, #34851, #34933 and the rest. Corrections: the
range-reads-too-low hold of #34457 never engaged in 12 logs, so log65's
landed floor was held by its other commits; `bf1ea90b4e`'s message needs
the boot yaw shift added.

## 2026-10-31: SFD-O4 logs 66-71, the refresh12 card

Firmware `ea8cef60` (the card commit, same code as `f93bd10a`), one flight
per boot (STAT_BOOTCNT 651-658; 652 and 657 have no log, LOG_DISARMED 2 on
USB, probably the bench). The beta's Replay follows logs 66 and 68-71 on
both cores to 1e-7. log67 diverges at 29.87 s, yaw 1.7 deg and VD 0.08 m/s:
see the bootstrap reset below.

Bench. 0a (#29000): "rate set to 2007Hz" once at boot in every log, RTDT
1973 Hz armed as in logs 63-65; passed. 0b (#31274): no motor test in these
logs. Armed, ESC.Err stayed at or below 0.9% on every motor except log70's
hard acro (1.85, 1.54 and 1.82% on motors 1, 3 and 4 at 340-370 s). At
boot all four read 75-99% from 4.9 to 5.6 s, alike on every motor and log;
log65 read 97-99% for 3 s. The takeoff gate reads it only armed.
(Corrected the same day: that contrast says nothing about #31274. log65's
build carries the same #31274 code, builds without its DShot fixes (log57,
log59) show the same 0.7 s, and log65's 3 s comes from its ESC telemetry
starting 2 s later.) 0c
(#32238, `ESC_CALIBRATION 9`): every log starts at 3.07 s against 5.10 s in
logs 63 and 65, 2.0 s sooner; passed. 0d (#34630): not in the logs;
`EK3_FLOW_QMIN` is still 60.

Item 1 (#33366), log66: the first two armings (36-256 s) were held, "Takeoff
blocked: CPU overload" every 2 s, until `TKOFF_CPU_AVG` was raised by hand
from 3 to 80, 90 and 95. Nobody set 3: the boot dump reads 3 against a
default of 95. `ParametersG2::var_info2` index 30 held `VALT_POS_EXPO`
(also a float) on the June builds, 4.7-beta, 4.7-beta6 and 4.7.0, until
the 4.7.1 rebase moved it to 29 with no "30 was" marker. This board still
held that value, and `40c9fdd94b` (2026-10-30) put `TKOFF_CPU_AVG` on 30 as
free. Upstream builds are not affected. At 95 the third arming took off;
armed load read 92.3-93.9% average, 1-3% under the limit, and 85-89%
disarmed. The warning also runs while disarmed, as upstream's does. Only
one takeoff per boot, so the two-takeoff check was not flown;
`BRD_IDLE_STATS` was 1, so no PM2. The "peak" is the highest of a smoothed
load and can read 0.1% under the window average (92.6/92.5%).

The same scan of shipped tables finds one older float-on-float reuse, live
since 4.7.1: var_info2 29 was `THROW_YAW_DEG` on those June builds and is
`VALT_POS_EXPO` now (this board reads 0). Indices 41-46 and 48-58 have
held nothing on any branch.

Item 2 (#32471 v3), log66 (`ACC_ZBIAS_LEARN 9`, 68 s hover): saved
-0.0053/-0.0076. XKF2.AZ read -0.030 at arming and -0.039/-0.034 over the
last 3 s of hover, so it saved end minus arming, as v3 intends (AZ logs at
0.01). Passed. The next three flights saved +0.020 to +0.030 instead.
log66 armed with the least negative ground AZ of the six (-0.03, against
-0.04 to -0.07), and the delta takes up the ground bias's spread from boot
to boot. That is a hypothesis from one airframe.

Item 3, log67 (11) applied pack 1's -0.005/-0.008, so its flight hardly
tests the apply. The height against the range finder is the rms over 2-20 s
after liftoff, cores 0/1. This measure reads log63 flight 1 as 0.15/0.83 m,
not the 0.14/1.10 recorded above, which took core 1 differently. log66 (no
apply) read 0.18/0.73, log67 0.11/0.43 and log68 (applying 0.025/0.022)
0.17/0.42. The AZ swing within 20 s read 0.02/0.03, 0.04/0.05 and
0.03/0.03, so pack 2 swung more than pack 1 and failed that check. Replay
with RISK.VRFBZ patched, same sensor data:

| Flight, correction | Core 0 | Core 1 | AZ swing |
|---|---|---|---|
| log68, as flown (0.025/0.022) | 0.17 m | 0.42 m | 0.03/0.03 |
| log68, none | 0.18 m | 0.59 m | 0.05/0.05 |
| log67, as flown (-0.005/-0.008) | 0.11 m | 0.43 m | 0.04/0.05 |
| log67, log68's value (0.025/0.022) | 0.08 m | 0.30 m | 0.01/0.03 |
| log71 (bit 2), as flown (0.030/0.024) | 0.18 m | 0.44 m | 0.08/0.12 |
| log71 (bit 2), none | 0.19 m | 0.47 m | 0.06/0.10 |

v3's learned value takes 0.13-0.17 m off core 1 and halves the swing. Core 0
barely moves. With bit 2 the swing comes from the release at arming, as the
log63 replay at 15 showed, and the correction makes little difference.

Item 4 (#34941), logs 69 and 70, flow on core 1 (log69 GPS-primary, log70
flow-primary): 18 stretches over 45 deg lasting more than 5 s, up to 31 s,
at 10-25 m/s. Neither log has a "stopped aiding" or "started relative
aiding" in flight (log65 had six pairs), and core 1's speed was never
zeroed. Where the range was good (log70 162-167 s and 217-222 s, 9.5-14 m)
it stayed within 1-3 m/s of GPS. Above the range finder it read up to 14
m/s high (log69 129-136 s, 13-35 m), which is flow scale error. Passed.
log70 also flew #34630's stale-range deferral: "flow recovery deferred,
range stale" at 228.24 s, with core 1 6-7 m/s high and no range. Once the
range returned at 230 s it converged within 2 s without a reset. Core 1
also took three lockout resets, unannounced as `EK3_OPTIONS` bit 6 quiets
them: XKF7.FVC at log68 77.57 s, log69 181.27 s and log70 287.57 s.

Item 5 (#34942): 8 arms, with no "height error" refusal and no reset at
rest after any of the six landings. VD at arm read 0.00-0.01 m/s. Every
flight ended in a power cycle, so no re-arm after a landing was flown.
Item 6: no Throw.

Item 7, log71, flown at 15 (learn and apply as well as bit 2, not the
card's 12) and on a boot log, not a restarted one, so the restart case was
not flown. REV3 16 at boot and 17 at arming, AZ 0.00 at arming, Replay
matches, and no height error. It saved -0.042/-0.039, the whole hover bias,
so a return to 11 needs log69's 0.030/0.024 put back.

Arm pending (#32401), log67: 5 s after the switch (`ARMING_PEND_TIM 5`), still with
no position, it reset both EKF cores at 29.87 s ("Arm pending: EKF bootstrap
reset"). The GPS checks were already passing (XKF4.GPS 0). The EKF first
uses GPS about 30 s after it initialises (34.4-35.9 s on the other boots),
and the reset restarted that wait: GPS at 59.56 s, 24 s late. The reset is
not a DAL event, so Replay cannot follow it. Upstream's
`InitialiseFilterBootstrap()` (41744c884d) has the same gap, but compiles it
out by default.

## 2026-10-30: SFD-O4 logs 64-65, takeoff blocked, lockouts and landed hold

Firmware `52b1d36f`, `ACC_ZBIAS_LEARN 11` still.

log64 could not take off: "Takeoff blocked: CPU overload" every 2 s through
two armings, at an average of 84-92%. The check (`342a8bdfd8`, upstream in
4.7 and unchanged on master) blocks on average over 95% or peak over 99.5%,
and the peak is the highest roughly 1 s load since boot: sysperf.c sets it
at initialisation only. One busy second anywhere in the session blocks
takeoff until reboot, and the message reports the average, not the peak
that blocked (ARCH-2). The peak is not logged, so what spiked is unknown;
this boot also had a radio failsafe and turtle mode before arming. Main
loop load (PM) matched logs 61-65. `BRD_IDLE_STATS 0` turns the check off
at once, as `use_idle_stats()` reads it live; a reboot clears the peak.

(Corrected the same day: the peak-since-boot account above is wrong. The
log does carry the figures, in the `@SYS/threads.txt` copies the logger
writes after each arming: LOAD 0.0% PEAK 0.0% at the first arming of every
boot, then 87.3% / 99.5% at log64's second. Measurement does not start at
boot, because `main_loop()` tests `BRD_IDLE_STATS` before the parameters
load; the logger's first read after arming starts it, and the first few
samples after that start tripped the 99.5% peak. Later reads leave it
frozen: the average stayed at exactly 87.4%. So the radio failsafe and
turtle mode played no part. Open PR #33366 fixes the measurement and the
check, and is in `prs.txt`; see
`../ardupilot-pr-analysis/pending-takeoff-load-peak/`.)

log65, card items 3 and 1 (acro to 56 m, range finder good 25% of it, 48%
over 45 deg of tilt; GPS lane primary, flow on core 1 throughout):

- #34630/#33484: the first lockouts provoked in flight. Core 1 reset three
  times (XKF7.FVC 0 to 3, FVU never set), range good at 9-14 m, tilt
  33-44 deg, quality 126-159. Each repaired a lane already wrong: 4.7 to
  18.9 m/s at 63.6 s (GPS 18.1), 0.1 to 20.7 at 83.2 s (GPS 18.5), 1.7 to
  16.1 at 132.5 s (GPS 18.9), and all within 1 m/s a second later. The
  deferral at 91.9 s ("range stale") held off with core 1 2.5 m/s high,
  converging in 2 s.
- The near-zero speeds those resets repaired came from log61's mechanism:
  six aiding drops, each after up to 5 s over 45 deg, zeroed core 1's
  velocity (18.9 to 0.3 m/s at 51.4 s, GPS 18.7). Without a lockout reset
  to re-anchor it, core 1 stayed wrong for 8 s or more (113.4 s: 4-8 m/s
  against 16-25). The lockout reset needs the range finder, which is why
  log65 recovered and log61, above range, did not.
- #34457, card item 1: 23 s armed on the ground after landing. Both cores'
  AGL KF held the 0.05 m floor throughout (core 0 0.53 at touchdown). The
  main filters read -0.2 m/s on the ground, under the 0.25 m/s coast
  stop log39 defeated. Passed. The main filters' terrain height read 0.4-1.8
  m below the ground there (range out of range low), not the AGL KF's.

## 2026-10-30: SFD-O4 logs 62-63, hover Z-bias packs and full-stick flow Loiter

Firmware `52b1d36f`, GPS lane (core 0) for both hovers, LOITER at 2-3 m.

#32471, card item 2. log62 (`ACC_ZBIAS_LEARN 9`, learn only, 58 s hover)
saved -0.029/-0.035 against XKF2.AZ at the end of the hover of
-0.020/-0.026: the code saves the total bias, not the card's "end minus
arming" (-0.050 at arming, so that would be +0.03). log63 flight 1 (11,
learn and apply, 65 s) failed all three of the card's checks: AZ swung
0.06/0.07 after takeoff against 0.04, and the EKF height against the range
finder over the first 20 s was 0.14/1.10 m rms against 0.04/0.64 m. Baro
stayed within 0.2 m of the range finder in both, so core 1 (no GPS VZ) was
following its own vertical estimate.

Why: the saved total includes the ground bias, and with bit 2 clear the
EKF has learned this boot's ground bias into its state while disarmed.
Nothing offsets the state when the correction switches on at arming, so the
ground bias is counted twice (ARCH-7). The residual then has to climb by
about the previous flight's ground bias (-0.05 in log62; it climbed
0.06/0.07). Replay of log63 flight 1 with RISK.VRFBZ patched, same sensor
data (reproduces C=0/1 exactly):

| Correction | Core 0 height err, 2-20 s rms | Core 1 | Core 1 AZ swing |
|---|---|---|---|
| saved total, as flown (-0.029/-0.035) | 0.14 m | 1.10 m | 0.07 |
| none | 0.12 m | 0.65 m | 0.04 |
| saved total with bit 2 (15; REV3 16/17 inserted) | 0.33 m | 0.75 m | 0.10 |
| vibration part only, from log62 (+0.021/+0.015) | 0.11 m | 0.54 m | 0.03 |

Applying the total with bit 2 clear is worse than applying nothing.
#32471's record predicts that: its "Store the motors-on delta" row puts the
double count on bit 2 and rejects storing the delta for fleet compatibility,
so pack 2 should have flown 15. Replayed as 15 the double count is gone (AZ
0.00 at arm), but the release at arm restores the bias variance and the bias
swings to +0.06/+0.09 after liftoff; core 0 is worse than with no
correction. The delta is best on this flight: the first real-flight evidence
against that row, which stands until decided. Fly 9 until a pack at 15 has
flown. The saved total also moved 0.025 between two consecutive flights
(log63: -0.018 then -0.044), as large as the vibration offset itself.
(Corrected the same day: this said "fly 9 until #32471 saves the vibration
part", written before the record's rejected row was read.)

#33568, card item 4 (log63 flight 2, flow lane, `LOIT_SPEED_MS 15`): five
full-stick pushes of 0.5-1.6 s at 3-6 m reached 9.9 m/s. The flow speed cap
there is 20-36 m/s ((`EK3_FLOW_MAX` - 1) x height), so it never bound; it
binds only below `LOIT_SPEED_MS` / 6.4, 2.3 m. Not tested. What it did show:
core 1 tracked GPS within 0.5-1 m/s throughout. Along each push direction
the velocity never went negative after release, but stick came back 1.1-3.0
s later with the vehicle still at 0.85 m/s or more, so no stop completed
hands-off. #33318 did not run (`LOIT_OPTIONS 1`, its bit 1 clear); this was
Loiter with #33639. (Corrected the same day: this credited #33318 with "no
lurch back".)
#34457: 6 s armed after landing, core 1's AGL KF held 0.12 m.

## 2026-10-30: SFD-O4 log61, Loiter on the flow lane did not stop

Firmware `52b1d36f`, GPS on core 0 and flow on core 1 (`EK3_SRC_OPTIONS 8`,
`EK3_OPTIONS 94`). Source set 2 was selected disarmed at 136.7 s, so core 1
was primary for the whole acro sortie: up to 40 m, above the 15 m range
finder for most of it, and over 45 deg of tilt for 32% of the flight.
Loiter at 255.8 s, at 26 m and 13 m/s, held a 32 deg lean and sped up to
14.5 m/s; the pilot went back to acro at 260.1 s and to source set 1 at
268.5 s. Loiter on GPS at 278.5 s stopped from 10.5 m/s in 1.8 s.

Why Loiter did not stop: core 1 read 0.3-0.9 m/s. Loiter seeds its velocity
I term from the current lean (6 m/s/s east), saw no motion, and kept the
lean as if holding against wind (PIDE.I 6.0 rising to 8.9).

Why core 1 read zero (Replay reproduces core 1 to 0.1 m/s):

- 215.7-220.6 s over 45 deg of tilt, so no flow was fused; the IMU carried
  the velocity (17.9 against GPS 18.3 m/s at 220 s).
- 220.5 s: the 5 s flow timeout took core 1 to AID_NONE and straight back
  to AID_RELATIVE, which zeroes horizontal velocity (upstream; #33568 keeps
  it only for absolute to relative). 18.8 to 0.8 m/s at 19 m/s true, with
  4.5 s more over 45 deg. Again at 235.3 and 240.3 s, with runaways to
  37-48 m/s between. Core 1 ended 200-340 m from GPS.
- AP_Terrain looks SRTM up at the primary's position, here core 1's, and
  200-340 m off it found ground 25-35 m higher (RTER 25-35 m, 0-8 m
  before). With bit 2 the flow fusion scaled by `-PD - RTER`, below zero
  and clamped to the floor, so fusion drove the velocity to zero. This is
  the collapse #33585's record leaves open; a relative lane's position is
  not a location (ARCH-16).

Replay A/B, core 1 at 254-260 s against GPS 13-14.5 m/s: as flown 0.3-3.9;
bit 2 off (#33585's anchor, following the database's shape at the drifted
position) 4.5-6.8; RTER stripped from the log (flat ground only) 20-30. All
three identical from 222 to 234 s: no terrain setting gives a usable
velocity once the lane has been reset.

| PR | Status |
|---|---|
| #34456 | Worked: set 2 moved the primary to core 1 (136.7 s), set 1 back to core 0 in flight (268.5 s). |
| #33585, bit 2 | Ran above range throughout. Bit 2 bypasses the anchor; see the A/B above. |
| #33569 | Active: nav gain scaler about 0.48 on core 1 at 25 m. No bearing on a zero velocity. |
| #33318, #33639 | Loiter on GPS stopped from 10.5 m/s in 1.8 s, no overshoot. #33318 itself did not run: `LOIT_OPTIONS 1` leaves its bit 1 clear (corrected the same day). |
| #33568 | Not exercised: GPS never lost on core 0. |
| #33484, #34630 | Not provoked: XKF7.FVC 0 on both cores. |
| #34543, #34630 OSD | Ran, but core 1 reported itself healthy throughout (XKF4.SS unchanged, SV and SP 0), so nothing could flag it. What was drawn needs the pilot. |
| #33478, #33507 | AGL KF valid 68% of samples, velD fused under 1%. Not exercised. |
| #34457 | Disarmed 1.4 s after landing; AGL KF held 0.07-0.08 m on both cores with RFND out of range low. Card item 1 still needs 10 s armed on the ground. |
| #32768 | Arming stepped both cores' height 2.9 m (origin 41 s, armed 154 s). Hands off. |
| #32553 | "terrain offset reopened after takeoff" on both cores at 160.7 s. |
| #32471, `7ab6409ca3` | `ACC_ZBIAS_LEARN 8`: armed releases leaving acro at 255.8 and 278.5 s ran the new path. AZ moved at most 0.02 m/s/s (0.07 on core 1 at 278.5). No A/B yet. |
| #31274, DShot error counting | Ran all flight on bdshot: ESC.Err max 0.03-0.16% per output. |
| #32238, `16fe775812` | `BRD_FAST_BOOT 1` with `ESC_CALIBRATION 0`: the ESC calibration startup check waits for the radio again. 9 skips it. |
| #32514 | Source set change in flight at 268.5 s; no EKF failsafe. |
| #34210, #34696, #32475, #32270, #34678 | Not exercised. |


## 2026-10-30: squash audit of SFD-O4 logs 54-59 and the T25/TD25 flights

Which held-back commits these flights ran: SFD-O4 log54 (`0e2c9772`), 55-57
(`6d754361`) and 58-59 (`59386e92`); Tae's T25 on `5b70a271` and twice on
`308a7e73`, and TD25 on `0e2c9772`. Evidence is a message or event only the
changed path produces, or a Replay A/B through the beta at `6307d4d424`
with the change removed (log54-59).

| PR | Change | Status |
|---|---|---|
| #33484 | range step deferral and its speed form | Flown: "flow recovery deferred, range step", only printed when the speed-error test says the range has not settled, at 176.89 s in TD25's operational flight and 123.03 s in the T25 runaway's crash. Squashed. |
| #33484 | reset only on a sample the other axis passed | Ran at three resets (log57 core 1 941.4 s, T25 122.4 and 124.0 s); Replay without it identical on all three logs. Stays on top. |
| #34630 | quiet reset messages, lane reset flag | Flown: the three resets counted in `XKF7.FVC` with `EK3_OPTIONS` bit 6 and no message. Nothing to squash: the PR had not flown, so these are features, not fixes on flown commits. |
| #34543, #34630 | OSD lane items, flow signal strength, QMIN floor | Ran every frame on both vehicles (`OSD_TYPE` 5, `OSD1_EKFn_EN` 1; QMIN 60 on the T25). What was drawn is not in the log: needs the pilots. |
| #33359 | the four flow-axis gates | Changed the decision in flight: all-axes-pass in log57 (644.2-644.4, 941.2-941.6 s) and log58 (200.4-200.6 s), the hidden-speed test in log56 (149.4 s), the stamp ordering in log56 and log57; the first gate never. With all four removed the flow lane's height moves up to 0.10 m in log57 (GPS lane primary), identical elsewhere. Not squashed: three of the four also change the body odometry path, which no flight has run. |
| #34380 | flat-ground return on fused range readings | Replay without it identical on log54-59. Not flown. |
| #34457/#33507 | floor hold when the range reads too low | Replay without it identical on log54-59: never engaged. Not flown. |
| #34696 | restore waits for a position | No EKF failsafe cleared in flight: the T25's cleared after disarm, log54's after touchdown cancelled the restore. |
| #32514 | failsafe count held through spool-down | No failsafe began during a spool-down. |
| #34292 | rest reading cleared after 1 s without flow | No flow gap over 1 s in any log. |
| #32270 | VALT offset snap and loop-rate blend (`ee9dc05040`) | The blend runs in every 200 Hz VALT flight; the snap needs a held offset or surface tracking. One commit, half flown. |
| #34851, #34933 | | Flown (log58/59 Replay; the T25 on `308a7e73`); nothing on top. |


## 2026-10-08: refresh11 log44, flight card item 1 again (#33478 step hold)

Firmware `4fc73825` (topup12). Replays exactly. The log40 sortie over the
~0.7 m step: four edge crossings, landed on top. Core 1 (flow lane, fusing
the AGL KF velocity) against core 0, with the hold and in a Replay without
it:

| Edge | Core 1 - core 0, hold | Without the hold |
|---|---|---|
| Off, 88.6 s (6-8 s after) | +0.33 m | +0.99 to +1.33 m |
| Back on top, 110.3 s | +0.63 m peak | +1.36 m |
| Off, 122.4 s | within 0.12 m | +0.79 to +1.16 m |
| Back on top, 132.0 s | -0.13 to -0.32 m | +0.30 to +0.71 m |

Landed: core 1 +0.05 m from its height after arming, against +0.41 m
without the hold (core 0 +0.26 m both ways, its baro drift). XKFA.VFuse
paused after each edge and resumed. #33478's step hold: validated. Arming
stepped both cores' height -1.66 m on the ground (#32768's arm datum, as on
log42; hands off).

## 2026-10-08: #34457 floor behaviour, logs 31-44

All fourteen logs carry #34457's floor commits. Armed on the ground before
takeoff the AGL KF height stayed within 0.02 m of its floor on every log.
After touchdown it held within 0.07 m except log39 core 1, where the main
filter believed it was climbing at 0.3-0.57 m/s on the ground after an acro
sortie, so the coast stop (which leaves anything faster than 0.25 m/s to
the IMU) never fired and the AGL KF height coasted 0.28 -> 1.8 m. Fixed on
fix2/34457 (not yet flown or in the beta): with the range last read near
the floor and reading too low for 1 s, an AGL KF height 0.3 m above that
reading is held on the floor. Replay: log39 held at the floor, the other
13 logs unchanged. log38's 0.14 m is a real hop.

## 2026-10-08: refresh11 logs 41-43, flight card items 2, 3 and 5

Firmware `9d8e8053`. All three replay exactly. No failsafes, EKF errors or
uncommanded lane switches.

| PR | Status |
|---|---|
| #34380 | Validated (log41, card 2). The flow lane climbed from 4 to 35 m at 2.5 m/s, through 9.5 m without being held, and relative position stayed valid. Back into range at 169 s both cores' HAGL stepped 1.3 m in one sample: upstream EKF3's terrain reset after a 5 s range gap, correcting the baro height drift built up out of reach, after which HAGL agreed with the range. Not a defect of this PR or #33585. |
| #33568 | Validated (log42, card 3, two flights). Aiding went to mode 2 10 s after each switch to flow (the designed fallback time) and back to 0 within 0.2 s of returning to GPS. Both climbs passed 9.5 m, and there was no limit cycle at 25 m or above. Not shown: the flow speed cap, which never became the binding limit. Flow tracking at 29-33 m was poor (5.3 m of GPS movement seen as 2.4 m, and 8.5 m seen against 1.1 m). |
| #34630 | Not provoked (log43, card 5). 62 deg of tilt and 6.7 rad/s of flow at 2-6 m, but flow quality stayed above 144 and no lockout reset happened in flight. The one reset was at touchdown. |
| #32768 | Seen, not acted on (hands off): at the second arm of log42 the height stepped -3.4 m on the ground, the GPS altitude's drift since boot. Height above home was unaffected. |

## 2026-10-08: #33478 step hold, from log40

Replay of log40 found core 1's 0.9 m climb off the edge came from #33478:
the AGL KF absorbs a range step over 3-5 s with its velocity 0.2-0.3 m/s
wrong, and core 1 fused that as velD. fix/33478 holds the AGL KF velocity
out of velD for 5 s after a step, only while a real height source outside
ground effect is trusted. Replay of 12 flights (velD error, m/s): log40
0.089 -> 0.046, log29 0.107 -> 0.043, log30 0.097 -> 0.055, log34
0.085 -> 0.036, log39 0.232 -> 0.086, log32 0.116 -> 0.266 (fusion held
off over its 61 samples), the rest unchanged. Log40's core 1 stays within
0.03 m of fusion off at the edge and lands at +0.25 m against +0.36 m. Not
yet flown: card item 1.

## 2026-10-08: refresh11 log40, flight card item 1 (#33359 step-up)

Firmware `9d8e8053`, `EK3_OPTIONS 94`, `EK3_RNG_USE_HGT 6`, core 0 (GPS)
primary throughout. Replays exactly. One sortie over a ~0.7 m step with two
excursions over the lower ground. Touchdown is measured against the altitude
before takeoff, so 0 is right.

| Replay | core 0 | core 1 |
|---|---|---|
| as flown | -0.16 m | +0.36 m |
| #33359 fix removed | -0.57 m | +0.01 m |
| `EK3_OPTIONS` bit 4 off (#33478) | -0.16 m | -0.21 m |
| fix removed, bit 4 off | -0.57 m | -0.69 m |

| PR | Status |
|---|---|
| #33359 | Helps. The third crossing reset core 0's offset ("IMU0 terrain offset reset from range") and the switch-in step was 0.15 m against 0.55 m without the fix. The second crossing did not reset: the disagreement was 0.23 m, under the 0.3 m threshold, and both cores stepped down about 0.27 m, the same with and without the fix. A 0.2 m threshold lands at +0.10/0.00 m (bit 4 off); 0.15 m overshoots to +0.26/+0.20 m. The residual is baro drift over the excursion, which the offset inherits either way, so the threshold is unchanged on one flight. Hand-back to baro off the edge was clean (core 0 within 0.11 m). |
| #33478 | Problem. Off the edge, core 1 (flow lane) fused the AGL KF velocity as velD and climbed at 0.25 m/s for 5 s while baro was the source and flat: 0.9 m of height error, and baro innovations up to 1.4 m at a test ratio of 0.14. The PR describes a step as a brief transient (0.36 m/s in SITL); here it was sustained. With bit 4 off, core 1 tracks core 0. The vehicle was not affected because core 0 was primary. |

## 2026-10-06 to 10-08: Replay of earlier flights against the current code

| PR | Status |
|---|---|
| #33359 | Replay of logs 31-35 and 39 with a probe on the height source: the switch engaged in flight with the vehicle's terrain-stable flag clear for ~72 s on log31, including a 21 s hover below 0.9 m that tracked the range to 0.10 m rms. Refresh8's log29 (a 0.84 m step) showed a defect: back up the step the switch re-engaged on a stale terrain offset and pulled the height down 0.45 m. Fixed in topup9 (`d52e4a8a8c`); Replay of log29 lands at +0.10/-0.35 m instead of -0.57/-0.63 m. Topup10 also gates the AGL height the switch reads on the observation's limits; identical on Replay of six flights. Not yet flown with the fix. |
| #34208 | Lock-free rate target (volatile sequence instead of std::atomic) in topup8. Same behaviour by design; any flight on the fast rate thread exercises it. |
| #34642 | SITL-only (gyro rate follows INS_GYRO_RATE). Nothing to fly. |
| #34380 | Review fixes are a comment and test legs; its flight is unchanged. |
| #32553 | Helps. Replaying log36 without the reopen, height above ground during the 0.6 m dwell (59-75 s) is off by 0.47 m rms on core 1 against 0.21 m as flown (core 0: 0.11 against 0.04 m); with `EK3_GND_EFF_DZ=2` it is 0.54 against 0.22 m. No difference above range. Retraction (2026-10-06): the first A/B used a no-reopen binary that was byte-identical to the beta, so its "nothing to correct" was wrong. |


## 2026-10-05: logs 35-39

Firmware `96250adaf3`. All five logs replay exactly.

| PR | Status |
|---|---|
| #33585 | Validated again with the new end timing: flow lane primary above range to 22 m, 6.4% drift, no HAGL step when the range came back. Squashed. |
| #34456 | In-flight refusal validated (log37, `EK3_OPTIONS 92`). |
| #34292 | Floor works and aiding is kept; no phantom velocity on this airframe to remove. |
| #32972 | Mixed. Negative `EK3_GND_EFF_DZ` holds height through spool-up (0.03-0.07 m against 0.21-0.27 m), but -8 is worse after liftoff (0.35 against 0.17 m rms). Replay sweep done; no more flying needed. |
| #32553 | See the 2026-10-06 row above. |
| #34380 | Not flown: in log37 the flow lane was never primary, so no limit was published. |
| #33568 | Not flown: `EK3_SRC_OPTIONS` stayed 8. |
| #34630 | No resets: log39's acro was above the range finder, so both flow axes dropped together and no single-axis lockout occurred. |
| #32471 | Not flown. |

### Done: baro as height source at takeoff (#32972, #32553), flown as log36

`EK3_RNG_USE_HGT -1`, `EK3_GND_EFF_DZ -8`, `GNDEFF_ALT 0.5`,
`GNDEFF_TMO 2`, GPS lane. Arm in LOITER, hold the motors spooled on the
ground at least 5 s, lift off and hover at about 0.4 m for 30 s, climb to
5 m, land slowly; repeat with a brisk takeoff straight to 5 m. The pass
criteria were EKF height within about 0.2 m of the range finder through
spool-up and liftoff with no height reset (#32972, baseline Replay with
`EK3_GND_EFF_DZ=8`), and "terrain offset reopened after takeoff" with the
0.4 m dwell's HAGL error falling toward the SITL 0.26 m rms (#32553,
baseline Replay with the reopen reverted). Results in the tables above.

### Done: flow floor (#34292), flown in logs 35-39

`FLOW_HGT_MIN 0.15` as a first guess: across logs 31-34 the flow innovation
ratio (XKF5.NI) rises from 3-4 above 0.3 m to 14-44 between 0.05 and 0.2 m
while OF.Qual stays near 200. Known limits of the rest reading, from the
2026-10-05 review: it is learned on the ground before takeoff from a flow
sample and kept until the EKF restarts (check OF.Qual is non-zero on the
ground before each takeoff), and it compares range readings, so land on the
same hard, level surface you took off from - soft ground, a dip or a tilted
stance reading more than 5 cm further can leave flow aiding off after
landing and trigger the EKF failsafe while landed.


## 2026-10-04: refresh10 logs 31-34

Firmware `SmallFastDrone V4.7.2-beta1 (199bc6e5)`. Those flights validated
#34456, #32473, #34362, #33585 and #33484; results are posted on each PR
and the PRs were squashed. Six more PRs in the build were not exercised:

| PR | What hid it on refresh10 |
|---|---|
| #32972 | `EK3_RNG_USE_HGT 6` makes the range finder the height source below 0.9 m, so the baro is not used through spool-up. Replay with `EK3_GND_EFF_DZ=8` matched the flight within 3 cm. |
| #32553 | Same parameter: the terrain update is skipped while the range finder is the height source, so no ground-effect error reached the offset. |
| #34380 | `AVOID_ENABLE 0`. AC_Avoid is the only consumer of the height cap, so the cap was never applied. |
| #33568 | `EK3_SRC_OPTIONS 8` runs flow on core 1 from boot, so no core went from GPS (absolute) to flow (relative). |
| #34292 | `FLOW_HGT_MIN 0` sets no focus height of the sensor's own; the floor stays at the range finder ground clearance plus 5 cm (0.15 m with the default `RNGFND1_GNDCLR` 0.10), so it acted only at liftoff, at touchdown and on the ground armed. (Corrected 2026-10-18: this said `FLOW_HGT_MIN 0` disables the floor, which the code does not do.) |
| #32471 | `ACC_ZBIAS_LEARN 8` sets only the acro bit; learning (bit 0) and applying (bit 1) are off. |

Open from these logs: log32 core 0 departs from its Replay at rounding level
from 5.2 s, just after the recorded-origin boot that only log32 has,
reaching 1.4 m in the terrain offset by 80 s and 7 cm in PD by 339 s. It is
not precision, and the cause is not yet found. Leaving the RC source set
switch on set 2 at boot, with a recorded origin, gives it another chance to
show.
