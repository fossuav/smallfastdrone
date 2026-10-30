# SFD flight card: 2026-10-30 review-fix top-up

Firmware: beta `0cb49e7dfb` (SmallFastDronev1 `arducopter.apj`, md5
`fb6e5ec3`). Standing settings as before: `LOG_REPLAY 1`, `LOG_DISARMED 2`,
`EK3_OPTIONS 94`, STABILIZE on a switch. Each check says what log evidence
shows the changed code ran; a change with no check here gets no flight
evidence from this card.

## Bench, props off

### 0a. #29000: fast-rate start-up

Boot with the usual `FSTRATE_ENABLE 1`. Pass: "Rate CPU ..., rate set to
N Hz" appears once during boot (it was twice), and RTDT shows the usual
loop rate once flying.

### 0b. #31274: bidirectional DShot error rate, motor test

Motor test each motor in turn at 5-10 % for 3 s. Pass: no "Motor Test:
motor N errors X% at Y rpm"; if one appears, N must be the output that
spun. In the log ESC.Err stays at or below 1 for every motor, including
just after boot when the reversal commands (`SERVO_BLH_RVMASK 14`) go
out: their missing replies no longer count. Failed replies are now
charged to the motor that missed them, so a motor whose Err stands out
from the others is real.

### 0c. #32238, optional: ESC_CALIBRATION 9 skips the radio wait

`ESC_CALIBRATION 9`, power on with the transmitter off. Pass: the boot
log starts about 2 s sooner than with 0. Restore afterwards if 9 is not
wanted.

## Flights

### 1. #31274: takeoff RPM and error check

Read the idle RPM from ESC.RPM in the bench log and set `TKOFF_RPM_MIN` to
about 70 % of it (`TKOFF_RPM_MAX 0`). Take off in ALT_HOLD or LOITER and
fly a normal sortie. Pass: takeoff is not held with "ESC RPM or errors
out of range", and ESC.Err stays below 1 on all four motors through the
flight. If takeoff is held, the ESC.Err values at that moment are what we
need. This is the first hardware run of the three DShot statistics fixes.

### 2. #34630: OSD flow quality floor (ground, disarmed)

`EK3_FLOW_QMIN 60` (if it is 0) with `EK3_OPTIONS 94` (bit 3 on). Cover
the flow sensor: the lane strength reads below 50 and flashes. Then
`EK3_OPTIONS 86` (bit 3 off): the same cover shows the strength with no
floor and no flashing. Restore 94.

### 3. #32471, optional: inhibit re-logged in a restarted log

`ACC_ZBIAS_LEARN 12` (bit 2 plus the acro bit). Powered and disarmed,
erase the logs from the GCS so a new log starts, then fly a short ACRO
segment and land. Pass: the new log has a REV3 accel bias inhibit event
near its start, and Replay of it matches the flight (XKF2.AZ).

### 4. #32475, only if Throw is flown

`THROW_NEXTMODE` ALT_HOLD or LOITER (not AUTO), `THROW_SRC_SET 0`, and
switch out of THROW and back between throws. Upward toss to 2-3 m. Pass:
"Throw:" messages show detection, uprighting and the hand-off within
about 3 s, and no "Internal Error". Two review findings are still open
(AUTO as next mode, the source-set hand-off); these settings avoid both.

## Not exercised by this card

- #32232: the substitution change needs two downward range finders.
- #34210: the cap wind-down runs only after a climb-away latches the cap.
- #34543: Copter never falls back to DCM.
- #29000's temperature-calibration park: bench calibration only.

## Afterwards

Restore `TKOFF_RPM_MIN` (or keep it), `ACC_ZBIAS_LEARN 8`, `EK3_FLOW_QMIN`
and `EK3_OPTIONS 94`.

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
