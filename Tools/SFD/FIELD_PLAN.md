# SFD flight card: 4.7.3-beta3 (refresh2), 2026-11-23

The flights still to fly, in order. Results go in `FLIGHT_VALIDATION.md`;
earlier cards are in git history.

## Firmware

`SmallFastDrone V4.7.3-beta3 (f0f3296b)`, built 2026-11-23 from `f0f3296bd9`:
`bf6cae79aa`'s code with the `BETA` firmware type that refresh2 dropped. Later
commits on the beta add only notes. Files are in
`../sfd-473-r2/build/<board>/bin/arducopter.apj`. That worktree's branch is still
at `bf6cae79aa`, so build from `f0f3296bd9` or later if these are rebuilt.

| Airframe | Board | `.apj` md5 | Flash free |
|---|---|---|---|
| SFD-O4 | SmallFastDronev1 | `5f229c0e` | 36,492 |
| T25, T25 RED, TD25 | MicoAir743v2 | `64bb74fb` | 230,016 |
| Throw quad | TBS_LUCID_H7 | `83007450` | 36,732 |

**SFD-O4 has flown this code** (logs 110-113, build `b9b81e77`, which is
`bf6cae79` plus notes, without the `BETA` type; see its section below). **No other airframe has
flown 4.7.3.** Their last flights: the throw quad on refresh16 topup1, TD25
on `b81f2132`, T25 RED on refresh17 topup1. New to them:

- #35105: a landed Copter counts as on the ground while armed.
- #35312: while landed and armed, the flow lane's velocity variance is held
  up, so a touchdown error cannot lock the flow out and walk the velocity
  away.
- #35077: `FLOW_ALT_MIN` replaces `FLOW_HGT_MIN` (below).
- #33484: a lockout takes its full 500 ms again after a flow dropout, tilt,
  dip or fast flow, and a reset needs a new range reading.
- #35308: flow samples are averaged between EKF updates, which matters on
  a sensor faster than 20 Hz.
- #34292 re-scoped, #32768's any-core datum reset, #34942's rest check.

Not in this build: the review fixes to #35307 and #35312 on local
`fy/35312`. Their validation has not finished, so do not fly them.

## Before the first flight, every airframe

1. The GCS shows `SmallFastDrone V4.7.3-beta3 (bf6cae79)`.
2. `LOG_REPLAY` 1, `LOG_DISARMED` 2, STABILIZE on a switch.
3. **`FLOW_ALT_MIN`.** It keeps the old `FLOW_HGT_MIN` value but means
   something stronger: below it the flow is **not fused at all** once the
   vehicle has moved, where the old one only floored the height. It must
   be the sensor's focus limit and well under the lowest height you fly.
   T25 RED's 0.08 is fine. Anything near hover height will cost flow aiding.
4. **`LOIT_OPTIONS`.** 4.7.3 changed the default from 1 to 0. A board that
   never set it has lost coordinated turns; set 1 to keep them.
5. **Range finder mount.** If `RNGFND1_POS_Z` minus `INS_POS1_Z` is over
   0.1 m, the landed hold does not run on this build. That airframe behaves
   as beta2 did on the ground: skip F3 and F4 and disarm promptly.
6. **Flow resets are silent.** `EK3_OPTIONS` 92 and 94 include bit 6, so no
   "flow vel reset" message appears. See them as a flashing flow arrow on
   the OSD and as steps in `XKF7.FVC`. Deferrals still print.
7. **`EK3_FLOW_MAX` 4.0 or less.** Many airframes carry 7.4, which lets Loiter
   fly the flow past the EKF's fixed 4.2 rad/s input limit: that raised two EKF
   failsafes on SFD-O4's log111. Its documented range stops at 4.

## Flights, every airframe

Open ground, thumb on the STABILIZE switch.

- **F1, hover check.** Take off in a mode that needs no position (STABILIZE,
  ALT_HOLD or VALT), 1-2 m, 30 s. Pass: no EKF failsafe, no internal error,
  height steady.
- **F2, position hold.** Switch to LOITER at 1.5-3 m, sticks centred 30 s,
  then a small box with a stop on each side. Pass: holds without walking
  off, and stops without rolling back.
- **F3, land and sit armed.** Land in LOITER on flat ground, throttle down,
  hands off, and let it auto-disarm (`DISARM_DELAY`, 10 s by default). Pass:
  it sits still, the OSD speed stays near zero, and no flow arrow flashes.
  In the log: `XKF1` VN/VE under 0.5 m/s while landed, `XKF5.NI` under 100.
- **F4, lift off from armed.** Land again, and before it disarms lift off in
  LOITER to about 1.5 m. Pass: goes straight up with no sideways lean. This
  is the case #35312 protects, so be ready for STABILIZE.

**Switch to STABILIZE and land** if it leans or accelerates away with the
sticks centred, the height runs, or an EKF failsafe fires that you did not
expect.

**Stop flying that airframe and keep the log** if the velocity walks away on
the ground, it does the same thing twice, or an internal error appears.

### Note the time of any of these

- "EKF3 IMUn flow recovery deferred, range stale", "restarted" or "step".
- "EKF3 IMUn vertical velocity reset at rest" or an arm refused on "EKF3[n]
  height error". After a hard landing, leave it still for 1 s before
  re-arming. A re-arm after a hard landing without a reboot is still owed.
- "PreArm: Internal errors" after a take-off (#34678), especially in ACRO or
  STABILIZE.
- "ground mag anomaly, yaw re-aligned", and the height it came at. On log96
  it turned the yaw 13 deg the wrong way at 1.7-2.4 m. On SFD-O4 a stale
  `COMPASS_MOT` causes those at high current; the ones under 40 A have
  another cause (`FLIGHT_VALIDATION.md`, 2026-11-23).

## SFD-O4

Settings as flown on logs 89-90: `LOIT_DRAG_COEF` 0.3, `LOIT_OPTIONS` 1,
`EK3_OPTIONS` 94, `EK3_SRC_OPTIONS` 8, `EK3_FLOW_QMIN` 40, `ACC_ZBIAS_LEARN`
10 (log89's saved 0.025 / 0.020, no further learning). `EK3_FLOW_MAX` is
4.0 since log112: at 7.4 Loiter flew the flow past the EKF's fixed 4.2
rad/s input limit and raised two EKF failsafes on log111.

F1 to F4 and a re-arm without a reboot are flown on the flow lane (log112).
Next: recalibrate the compass, then set `COMPASS_MOT_X/Y/Z` to -0.69, 0.04,
0.23 (`COMPASS_MOTCT` stays 2), the values fitted from 18 flights, and
`EK3_HGT_I_GATE` back to its default of 500 from 1000 (`FLIGHT_VALIDATION.md`,
2026-11-23). Fly it on source set 1 first, with some high-current passes, and
the fit on that log should read near zero. The bench checks below are still to
do.

From beta4 (refresh3) a source set switch no longer moves the lane: with
`EK3_SRC_OPTIONS` 8 each core runs its own set, and #34456 now refuses a
selection there (the switch reports it failed). Pick the lane with #35366's
switch instead: set the switch that used to select the source set to
`RCx_OPTION` 188 (low, middle, high: cores 0, 1, 2). In flight it needs
`EK3_OPTIONS` bit 1, which 94 has.

### Bench, props off

1. #35122, the LED with a non-GPS position, shown only while the primary
   lane uses no GPS, so here only with core 1, the flow lane, primary. Pass:
   - disarmed with core 0 primary: slow blue flash before the GPS is in use,
     slow green once it is;
   - core 1 made primary with the `RCx_OPTION` 188 switch while disarmed, once
     the flow lane has relative aiding: green, blue, dark, repeating;
   - armed with core 1 primary: solid green. Solid blue means the position
     was not seen.
2. #32401, a pending arm on the switch. Flip the arm switch while a pre-arm
   check still fails, such as before the EKF or GPS is ready. Pass:
   - the failure message and a beep come once every 3 s, and the pre-arm
     failure prints once;
   - "Arm pending: EKF bootstrap reset" appears only if the failing check
     is the AHRS one;
   - it arms by itself once the checks pass.

   Repeat, and disarm from the GCS while the arm is pending. Pass: it does
   not arm by itself afterwards. Keep the props off for this one.
3. #31274, motor test each motor in turn at 5-10 % for 3 s. Pass: no "Motor
   Test: motor N errors" for a motor that did not spin, and ESC.Err at or
   below 1 on the motor under test.
4. #34630, the OSD flow quality floor, disarmed. `EK3_FLOW_QMIN` 60, then
   cover the flow sensor. Pass: the lane strength reads below 50 and
   flashes. Restore `EK3_FLOW_QMIN` 40. Since #35307 the floor also shows
   with `EK3_OPTIONS` bit 3 clear, so the old 86 check no longer applies.

## T25 and T25 RED

T25, flow only, no compass, second MicoAir743v2 (board `00360049`):

- `EK3_RNG_USE_HGT` 6, so the range finder gives the height below about
  0.48 m (`RNGFND1_MAX` 8).
- Calibrate `BARO1_THST_SCALE` for this board: clamp the quad or hold it
  nose-down and ramp the throttle in ACRO. Until then use -205.
- Keep `ACC_ZBIAS_LEARN` 10; flight 05's saved 0.023 is this board's.
- #35122, props off: slow blue flash until the flow lane has relative
  aiding, then green, blue, dark, repeating while disarmed, and solid green
  once armed.

T25 RED (`0021001F`), flow only, one lane, VALT, `EK3_FLOW_QMIN` 60,
`FLOW_ALT_MIN` 0.08, `EK3_RNG_USE_HGT` -1. Its last flight skimmed the floor
twice at 0.02-0.07 m, and under 0.10 m the flow measured nothing, so keep it
above 0.3 m. Note what surface it takes off from: that log fits a take-off
from 0.3-0.6 m above the floor flown over, still to confirm.

## TBS_LUCID_H7 throw quad

Settings as logs 15-16: log13's with `EK3_SRC3_YAW` 1, `EK3_RNG_USE_HGT` 24
and `EK3_RNG_USE_SPD` 4. Also:

- `ARMING_SKIPCHK` 780, so it arms held nose-down or sideways.
- `ARMING_OPTIONS` 8, so a failing pre-arm prints its reason every 30 s.
- `EK3_HGT_I_GATE` 500, from 1000. Replay of its 12 logs and 26 throws at
  gates from 300 to 1200 shows almost no difference (`FLIGHT_VALIDATION.md`,
  2026-11-23). Replay cannot show how it would have flown, so watch the first
  throws.
- `FS_DR_ENABLE` 0, so a flow dropout gets the EKF failsafe's VALT
  (`FS_EKF_ACTION` 4) rather than an RTL. On log19 the default gave a 20 m
  RTL above the range finder after 0.33 s without flow.
- Height is the larger risk: 3-6 m off GPS around the throws on the baro.
  `EK3_SRC2_POSZ` 2 followed the range finder in Replay of logs 13 and 15
  and is worth trying next. A step in the ground then becomes a step in
  height, and above 15 m it falls back to the baro.

Before arming:

- With the mode switch on LOITER, arming waits about 32 s after boot for a
  GPS position. THROW does not need one.
- The ARK range finder can stick at No Data on the ground. Lift or tilt the
  quad and try again.

First throw into VALT, high and over grass: switch to THROW straight from a
GPS flight, arm, and throw within about 10 s. Pass: "EKF3 IMU0 stopped
aiding" at the switch; "Throw height achieved" or "lost position", never
"good position", before "EKF3 IMU0 fusing optical flow"; and an `XKF7.FVC`
step on the flow lane once the flow starts.

Then into Loiter (`THROW_NEXTMODE` 5). Pass:

- "Throw height achieved, flow settling" or "lost position", then "position
  regained" or "Throw: LOITER waiting for position", and the hand-over
  within about 2.5 s of detection (log16: 1.63-2.39 s);
- `XKF7.FVC` steps by one on the flow lane at the switch;
- Loiter stops within about 3.5 m by GPS and holds within about 0.7 m with
  the sticks centred (log16).

Known, not changed: the height target was 0.65-1.0 m above the vehicle at
each Loiter hand-over, and Loiter then climbed 1.3-2.1 m. The range finder
switched to its long-range mode about 0.7 s before every throw and read No
Data for up to 1.5 s, so check the ARK Flow MR's range mode settings.

## Not exercised by this card

- #31005, a fence floor breach at zero throttle flying the RTL. Do not
  provoke it.
- #35039, a take-off from inverted. Do not provoke it.
- #33585's terrain-database anchor: SFD-O4's `EK3_OPTIONS` 94 has bit 2,
  which bypasses it.
- #35307's recovery without the AGL KF: it does not run with `EK3_OPTIONS`
  bit 3 set, as the SFD default of 92 and SFD-O4's 94 have it.
- #32471's refusal of a first flight after bit 2 held the bias: it needs
  `ACC_ZBIAS_LEARN` bit 0, which the SFD default of 8 and the 10 on SFD-O4
  and T25 leave clear.
- A restart after more than 5 s without a range reading ("range restarted")
  needs a range finder that drops out in flight.
