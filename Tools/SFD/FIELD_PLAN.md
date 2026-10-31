# SFD flight card: refresh12, the beta after topup3

SFD-O4 flew this card on 2026-10-31 (logs 66-71, `FLIGHT_VALIDATION.md`).
Before flashing `95ad9b9c` or later (SmallFastDronev1 md5 `f1c7c139`), which
puts VALT_POS_EXPO back on its June index and moves TKOFF_CPU_AVG/PEAK:

- After the flash, set `VALT_POS_EXPO` 0. It reads the 95 stored as
  `TKOFF_CPU_AVG` on 2026-10-31. TKOFF_CPU_AVG/PEAK return to 95/99.5.
- `ACC_ZBIAS_LEARN` is 15 with log71's bit 2 values saved. For 11, set
  `INS_ACC_VRFB_Z` 0.0301 and `INS_ACC2_VRFB_Z` 0.0244 (log69's).
- `EK3_FLOW_QMIN` is still 60 from bench check 0d.

Still to fly from this card: 0b and 0d, item 1's second takeoff in one boot,
item 5's re-arm after a landing, item 7's restarted log, and the T25.

Firmware: refresh12 topup3 at `f93bd10a` or later, the code to be promoted as the
beta ("SmallFastDrone V4.7.2"). SmallFastDronev1 `arducopter.apj` for SFD-O4, md5
`7558599a` as built from `f93bd10a`; MicoAir743v2 built from the same commit for
the T25. Standing settings as before: `LOG_REPLAY 1`, `LOG_DISARMED 2`,
STABILIZE on a switch, and on SFD-O4 `EK3_OPTIONS 94`. Each check says what in
the log shows the changed code ran; a change with no check here gets no flight
evidence from this card.

New since the last flights (SFD-O4 `52b1d36f`, T25 `0cb49e7dfb`): the
2026-10-30 review fixes, which SFD-O4 has not flown; #33366 (CPU load
measurement); #34941 (a banked flow lane no longer times out); #32471 v3 (the
hover bias saved as the change since arming); and #34942 (the on-ground height
check, and the vertical velocity reset at rest).

## Before the first flight, both airframes

- Set `INS_ACC_VRFB_Z` and `INS_ACC2_VRFB_Z` to 0. SFD-O4 holds -0.044 and
  -0.049, and the T25 holds 0.042, saved by the old learner as totals. v3
  would apply them as the change since arming and count the ground bias twice.
- Do not set `ACC_ZBIAS_LEARN` bit 2 (value 4) for ordinary flying. Bit 2
  stops the EKF learning its static accel bias on the ground, and #34942 then
  refuses to arm on "EKF3[n] height error" once that bias has built a
  vertical velocity at rest. Only item 7 uses it.
- After a drop or a hard landing, leave the vehicle still on the ground for at
  least a second before re-arming (#34942).

## SFD-O4: bench, props off

### 0a. #29000: fast-rate start-up

Boot with the usual `FSTRATE_ENABLE`. Pass: "Rate CPU ..., rate set to N Hz"
appears once during boot (it was twice), and RTDT shows the usual loop rate
once flying.

### 0b. #31274: bidirectional DShot error rate, motor test

Motor test each motor in turn at 5-10 % for 3 s. Pass: no "Motor Test: motor N
errors X% at Y rpm"; if one appears, N must be the output that spun. ESC.Err
stays at or below 1 for every motor, including just after boot when the
reversal commands (`SERVO_BLH_RVMASK`) go out. Failed replies are charged to
the motor that missed them, so a motor whose Err stands out is real.

### 0c. #32238, optional: ESC_CALIBRATION 9 skips the radio wait

`ESC_CALIBRATION 9`, power on with the transmitter off. Pass: the boot log
starts about 2 s sooner than with 0. Restore afterwards if 9 is not wanted.

### 0d. #34630: OSD flow quality floor (disarmed)

`EK3_FLOW_QMIN 60` with `EK3_OPTIONS 94` (bit 3 on). Cover the flow sensor:
the lane strength reads below 50 and flashes. Then `EK3_OPTIONS 86` (bit 3
off): the same cover shows the strength with no floor and no flashing. Restore
94 and `EK3_FLOW_QMIN 0`.

## SFD-O4: flights, in order

### 1. #33366 and #31274: two takeoffs in one boot

`BRD_IDLE_STATS 3` (bit 0 the threads.txt reader as before, bit 1 the new PM2
log message). Keep `TKOFF_RPM_MIN` at about 70 % of the idle RPM from the bench
log (1000 now), `TKOFF_RPM_MAX 0`. Take off in LOITER, land and disarm, then
take off again without rebooting.

Pass:
- Neither takeoff is held with "Takeoff blocked: CPU overload" or "ESC RPM or
  errors out of range".
- The `@SYS/threads.txt` copies after each arming show LOAD and PEAK changing
  between armings. log64 read 0.0 % at the first arming of every boot, then
  froze at 87.4 %.
- ESC.Err stays below 1 on all four motors.

PM2.Peak against the 99.5 % default, at an 87 % average with the rate thread,
is the margin question left open on #33366. If takeoff is held, the PM2 and
ESC.Err values at that moment are what we need.

### 2. #32471 v3, pack 1: the hover bias as the change since arming

`ACC_ZBIAS_LEARN 9` (learn, plus the acro bit), with the saved values zeroed
as above. Hover steadily in LOITER at 2-5 m for at least 60 s, land and
disarm. Pass: each saved value is about XKF2.AZ at the end of the hover minus
XKF2.AZ at arming for that IMU's core. log62's would have been about +0.03,
where the old learner saved -0.03.

### 3. #32471 v3, pack 2: apply it

`ACC_ZBIAS_LEARN 11` (learn and apply), same takeoff and hover. Pass: XKF2.AZ
moves less after takeoff than in pack 1, and the EKF height against the range
finder over the first 20 s after liftoff is closer than log63 flight 1 (0.14 m
core 0, 1.10 m core 1, rms). Replay of log63 with the value this design would
have saved gave 0.11 and 0.54 m. Keep 11 for the rest of the card.

### 4. #34941: a flow lane held past 45 deg does not time out

GPS lane primary with flow on core 1, as log65. Inside the range finder's
reach, an acro segment holding more than 45 deg of tilt for more than 5 s.
Pass: no "EKF3 IMU1 stopped aiding" / "started relative aiding" pair while
tilted, and core 1's speed stays near GPS through and after the tilt. log65 had
six such pairs, each zeroing it.

### 5. #34942, every flight: no false refusals, no stray resets

Nothing to set. After each landing and re-arm, note any "EKF3[n] height error"
refusal and any "EKF3 IMUn vertical velocity reset at rest". With GPS vertical
velocity agreeing, SFD-O4 should see neither: Replay of SFD-O4 logs 54-65
refused none of 20 arms. If one appears, the XKF1.VD and XKF3.IPD at that moment
are what we need.

### 6. #32475, only if Throw is flown

`THROW_NEXTMODE` ALT_HOLD or LOITER (not AUTO), `THROW_SRC_SET 0`, and switch
out of THROW and back between throws. Upward toss to 2-3 m. Pass: "Throw:"
messages show detection, uprighting and the hand-off within about 3 s, and no
"Internal Error". The hand-off to a source set with no horizontal source is
still open on the PR; these settings avoid it.

### 7. #32471, optional: the inhibit logged again in a restarted log

`ACC_ZBIAS_LEARN 12` (bit 2 plus the acro bit). Powered and disarmed, erase the
logs from the GCS so a new log starts, then fly a short ACRO segment and land.
Pass: the new log has a REV3 accel bias inhibit event near its start, and
Replay of it matches the flight (XKF2.AZ). With bit 2 the EKF cannot learn its
static bias on the ground, so #34942 may refuse the arm on "height error". If
it does, note XKF1.VD at rest and skip the item: that is bit 2's trade, not a
fault. Restore `ACC_ZBIAS_LEARN 11` afterwards.

## T25

Flow only, no compass and no yaw source, so #34942 and its static yaw fix act
on every flight. `ACC_ZBIAS_LEARN 10` can stay once `INS_ACC_VRFB_Z` is 0.

### T1. #34942 and the static yaw fix, every flight

- Boot: with the vehicle left still after power-on, XKF1.Yaw and XKF1.GZ stay
  steady on the ground until the first movement. Before the fix, the PR's
  stillness cap could fuse a yaw reference that was never taken.
- After each landing: note any "EKF3[1] height error" refusal and any
  "EKF3 IMU0 vertical velocity reset at rest". Replay of the MicoAir logs
  refused 7 ordinary re-arms, 1.1 to 3.9 s after a disarm, and 2 first arms.
  Each one seen in flight, with how long after the disarm, tells us whether
  that cost is acceptable.
- If the pilot repeats the drop test, as Flight test 3: a switch disarm at
  about 1 m over soft ground, still for 2 s, re-arm in VALT and take off, stick
  at minimum after the takeoff. Pass: the reset message after the drop
  whenever XKF1.VD read over 0.5 m/s at rest, and the re-arm either refused on
  "height error" or going up without climbing at minimum stick. Flight test 3
  climbed 2 m.

### T2. #34941: a banked flow lane does not time out

Inside the range finder's reach, an ACRO segment holding more than 45 deg of
tilt for more than 5 s. Pass: no "EKF3 IMU0 stopped aiding" / "started
relative aiding" pair while tilted, and the velocity is not zeroed coming out
of it.

### T3. #33366, optional: the CPU load check

`BRD_IDLE_STATS` is 0 on the T25, which turns the takeoff load check off. To
exercise it, set 3 and take off twice in one boot. Pass as item 1.

## Not exercised by this card

- #32232: the substitution change needs two downward range finders.
- #34210: the cap wind-down runs only after a climb-away latches the cap.
- #34543: Copter never falls back to DCM.
- #29000's temperature-calibration park: bench calibration only.
- #34942's skip of the height check while falling: it needs a re-arm during a
  fall after a disarm in the air. SITL covers it; do not provoke it.

## Afterwards

Restore `TKOFF_RPM_MIN` (or keep it), `ACC_ZBIAS_LEARN` (11 on SFD-O4 if
keeping v3's apply, otherwise 8), `EK3_FLOW_QMIN`, `EK3_OPTIONS 94`, and
`BRD_IDLE_STATS` as wanted. Put each log's results in `FLIGHT_VALIDATION.md`
with the item numbers above.

---

# Earlier card (topup13)

Firmware: topup13 or later (carries #34457's floor hold for flight 1). Keep `EK3_OPTIONS 94`. Fly every flight with
`LOG_REPLAY 1` and `LOG_DISARMED 2`. Keep STABILIZE on a switch. What
earlier flights showed is in `FLIGHT_VALIDATION.md`; the log40/log44 step
sortie is done.

## Flights, in order

### 1. #34457: land after hard flying and stay armed

Done 2026-10-30, log65: passed (see `FLIGHT_VALIDATION.md`).

The case log39 hit: after an acro sortie a lane's main filter thought it
was climbing on the ground, and its AGL KF height coasted 1.5 m. Fly hard
acro on the flow lane inside the range finder's reach (below about 12 m),
then land in LOITER or ALT_HOLD and stay armed on the ground for 10 s
before disarming.

Pass: on both cores XKFA.HAgl stays within about 0.3 m of its floor for
the whole time on the ground (log39 core 1: 0.28 -> 1.8 m), and XKF1.VD
on the ground shows whether the main filter thought it was climbing.

### 2. #32471: hover Z-bias learning, two packs

Flown 2026-10-30, logs 62-63, but pack 2 was the wrong setting: 11 leaves
bit 2 clear, which double-counts the ground bias as #32471's record says
it will. Still owed: pack 3 at `ACC_ZBIAS_LEARN 15`, same takeoff and
hover. Replay of log63 as 15 was worse than no correction on core 0 (see
`FLIGHT_VALIDATION.md`).

Pack 1, `ACC_ZBIAS_LEARN 9` (bit 0 plus the acro bit): hover steadily in
LOITER at 2-5 m for at least 60 s, land and disarm. On disarm
`INS_ACC_VRFB_Z` and the second IMU's value are saved; they roughly equal
XKF2.AZ at the end of the hover minus its value at arming.

Pack 2, `ACC_ZBIAS_LEARN 11` (bits 0 and 1 plus the acro bit), same
takeoff and hover: XKF2.AZ starts close to its hover value and moves less
after takeoff than in pack 1, and the EKF height against the range finder
in the first 20 s after liftoff has a smaller error. Flight 1 can share a
pack with either.

### 3. #34630, if time allows: provoke a lockout

Done 2026-10-30, log65: three resets, each repaired the lane.

log43 never tripped one: flow quality stayed above 144 and no axis
dropped out. Fly harder: full-rate roll and pitch flips in ACRO or
STABILIZE at 3-8 m, inside the range finder's reach, on the flow lane.
Watch the OSD lane item for an arrow going off for 250 ms. XKF7.FVC counts
the resets.

### 4. #33568, optional: the flow speed cap

log42 and log63 never reached the flow speed limit. The cap is
(`EK3_FLOW_MAX` - 1) x height, 6.4 x height at 7.4, so it binds only below
`LOIT_SPEED_MS` / 6.4: 2.3 m at 15. On the flow lane in LOITER at about
1.5 m, full stick in one direction for at least 3 s. Pass: ground speed is
held near 6.4 x height (about 10 m/s at 1.5 m) rather than `LOIT_SPEED_MS`.

### 5. #34678, optional ground check: righting a flipped copter

On soft ground with the props clear, put the copter on its back, arm in
STABILIZE and raise the throttle to roll it upright. Pass: no "Internal
Error" message (flow_of_control) and it re-arms afterwards without a
reboot.

## Afterwards

Restore `EK3_RNG_USE_HGT 6`, `EK3_SRC_OPTIONS 8`, `AVOID_ENABLE 0`, and
`ACC_ZBIAS_LEARN` as wanted. Keep `EK3_OPTIONS 94`.

## Notes

- Baselines are Replays of the same logs with the PR's change removed.
  For flight 1, the topup12 Replay binary (no floor hold) is the baseline.
- The aiding mode change after a source set switch to flow takes the
  designed 10 s (`posRetryTimeUseVel_ms`), not a few seconds.
- Coming back into range finder reach after more than 5 s out of it, the
  terrain offset snaps to the range in one sample (upstream EKF3). The
  step is the height drift built up while out of reach (log41: 1.3 m on
  both cores), so judge the return by HAGL agreeing with the range
  afterwards, not by the absence of a step.
