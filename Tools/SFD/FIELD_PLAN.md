# SFD flight card: refresh15

The flights still to fly, in order. Results go in `FLIGHT_VALIDATION.md`;
earlier cards are in git history.

Firmware: refresh15, promoted 2026-11-12 and built from `3e1a71309a`
(the head, `c42bd1fb40`, carries the same code). Its code is refresh14
topup6's, which SFD-O4 flew as log89, with the PR heads of 2026-11-12:
#35039 and the reworked #34678 (the land detector gives the flight mode
one loop to see a take-off made through the rate thread), #35122's LED
pattern for a non-GPS position in place of the local LED commits, and
#31274's ESC.Err description. The EKF is unchanged. SFD-O4 flew it as
log90 (`FLIGHT_VALIDATION.md`, 2026-11-12). Nothing to set.

topup1 (`24b7a1a5c0` since the squash) adds the EKF's flow velocity at an in-flight start
of flow aiding and four throw changes, for the TBS_LUCID_H7 throw quad
below (`REFRESH_NOTES.md`). On SFD-O4 and the T25 it acts only where the
flow lane starts aiding in flight after a spell without it. Nothing to
set. SFD-O4 flew it as log94 (`FLIGHT_VALIDATION.md`, 2026-11-12), but
its flow lane was aided from the ground to the landing, so the change
did not run, and Replay gives the same EKF output as refresh15's.
The throw quad flew it as logs 15-16 (`FLIGHT_VALIDATION.md`,
2026-11-12): the init and the flow check worked on every throw from no
aiding, three into Loiter among them, and the first throw after a GPS
flight struck the ground. topup2 (`a3a948e4c3`, unflown) closes that
path: a source set with nothing to measure the horizontal position or
velocity stops aiding at once instead of dead reckoning for up to 15 s,
and the flow check fails closed. On SFD-O4 it changes nothing in
Replay; a T25 switched in flight to a set with nothing now stops aiding
at the switch, not 5 s later. Nothing to set.

- SFD-O4: SmallFastDronev1 `arducopter.apj`, md5 `a959195a` (topup1:
  `c27c4fda`).
- T25: MicoAir743v2 `arducopter.apj`, md5 `fe2ba189` (topup1:
  `7151fd38`).
- Throw quad: TBS_LUCID_H7 `arducopter.apj`, md5 `fe76ac42` (topup1:
  `add8c39b`).
- refresh14 topup5: any SmartRTL return on either vehicle exercises the new arming
  reset and should come home as before; the in-flight reset itself needs
  `smartrtl-reset.lua` installed and a Scripting1 (300) switch.

Fly every flight with `LOG_REPLAY 1` and `LOG_DISARMED 2`, and STABILIZE
on a switch.

## SFD-O4

Settings as flown on logs 89-90: `LOIT_DRAG_COEF` 0.3, `LOIT_OPTIONS` 1,
`EK3_OPTIONS` 94, `EK3_SRC_OPTIONS` 8, `EK3_FLOW_QMIN` 40. From log94
`ACC_ZBIAS_LEARN` is 10, applying log89's saved 0.025 / 0.020 with no
further learning. Nothing to change.

### Bench, props off

1. #35122: the LED with a non-GPS position. It shows only while the
   primary lane uses no GPS, so on this vehicle only on source set 2.
   Pass:
   - disarmed on source set 1: slow blue flash before the GPS is in use
     and slow green once it is, as before;
   - switched to source set 2 while disarmed, once the flow lane has
     relative aiding: green, blue, then dark, repeating;
   - armed on source set 2: solid green. Solid blue means the position was
     not seen.
2. #32401: a pending arm on the switch. Flip the arm switch while a
   pre-arm check still fails, such as before the EKF or GPS is ready.
   Pass:
   - the failure message and a beep come once every 3 s, not 4 times a
     second, and the pre-arm failure prints once;
   - "Arm pending: EKF bootstrap reset" appears only if the failing check
     is the AHRS one;
   - it arms by itself once the checks pass.

   Then repeat, and disarm from the GCS while the arm is pending. Pass: it
   does not arm by itself afterwards. The old code self-armed about 60 s
   later in SITL, which is why the props are off.
3. #31274: motor test each motor in turn at 5-10 % for 3 s. Pass: no
   "Motor Test: motor N errors X% at Y rpm" for a motor that did not spin,
   and ESC.Err at or below 1 on the motor under test.
4. #34630: the OSD flow quality floor, disarmed. `EK3_FLOW_QMIN` 60 with
   `EK3_OPTIONS` 94, then cover the flow sensor. Pass: the lane strength
   reads below 50 and flashes. With `EK3_OPTIONS` 86 the same cover shows
   the strength with no floor and no flashing. Restore `EK3_OPTIONS` 94
   and `EK3_FLOW_QMIN` to its usual value (40).

### Every flight

- #34678: note any "PreArm: Internal errors" after a take-off, especially
  one in ACRO or STABILIZE, where the mode clears the landed flag itself.
  log90's and log94's ACRO take-offs were clean, but it is a race, so
  every take-off counts.
- #34942: note any arm refused on "EKF3[n] height error", and any
  "vertical velocity reset at rest" or "forced reset", with the time since
  the disarm. After a hard landing, leave it still for at least 1 s before
  re-arming. Still owed: a re-arm after a hard landing without a reboot
  (the T25 on 2026-11-08 and SFD-O4 on log89 re-armed after soft ones).
- #32471: note any "Hover Z-bias not saved: ..." message, especially
  "accel switched on ground".

## T25

Flow only, no compass, on the second MicoAir743v2 (board `00360049`).

- Set `EK3_RNG_USE_HGT` 6, so the range finder gives the height below
  about 0.48 m (`RNGFND1_MAX` 8).
- Calibrate `BARO1_THST_SCALE` for this board: clamp the quad or hold it
  nose-down, ramp the throttle in ACRO, and run `baro_thst_cal.py` on the
  log. Until then use -205.
- Keep `ACC_ZBIAS_LEARN` 10; flight 05's saved 0.023 is this board's.
- #35122, props off: slow blue flash until the flow lane has relative
  aiding, then green, blue, dark, repeating while disarmed, and solid
  green once armed.
- Every flight: the #34678 and #34942 notes as for SFD-O4.

## TBS_LUCID_H7 throw quad

topup2's flights. Settings as logs 15-16 (`FLIGHT_VALIDATION.md`,
2026-11-12): log13's with `EK3_SRC3_YAW` 1, `EK3_RNG_USE_HGT` 24 and
`EK3_RNG_USE_SPD` 4. With the first two the EKF yaw was within 12 deg
of DCM at every switch to set 2 and no throw spun after it (log13: up
to 178 deg off on three throws). Also set:

- `ARMING_SKIPCHK` 780 to skip the lean check, so the quad arms held
  nose-down or sideways. The Level skip is bit 9 here (#32391); a
  4.7.0 parameter file's bit 21 does nothing on this build.
- `ARMING_OPTIONS` 8 (bit 3, pending arm on the switch, without bit 0),
  so a failing pre-arm check prints its reason every 30 s instead of
  only flashing the LED yellow.
- Height is now the larger risk, in VALT and Loiter alike: 3-6 m off
  GPS around the throws on the baro. With `EK3_RNG_USE_SPD` 4 the
  range finder's speed gate dropped it on log15's throw 3 once the toss's
  true 5.8 m/s was known, and the height jumped 2.9 m to the baro.
  `EK3_SRC2_POSZ` 2, the range finder as the flow set's height, has no
  speed gate and followed the range finder in Replay of logs 13 and 15
  (no jump on throw 3); a step in the ground becomes a step in height,
  and above 15 m it falls back to the baro. Worth trying next.

Before arming:
- With the mode switch on LOITER, arming waits about 32 s after boot
  for a GPS position. THROW does not need one.
- The ARK range finder can stick at No Data on the ground ("Rangefinder
  1: No Data"): from boot to 46 s on one setup flight, and for 33 s
  after throw 3's landing on log14. Both times it came back the moment
  the quad was lifted, so lift or tilt it and try again.

Into Loiter (`THROW_NEXTMODE` 5) as on log16, STABILIZE on a switch.
Pass:
- "Throw height achieved, flow settling" or "lost position", then
  "position regained" or "Loiter waiting for position", and the
  hand-over within about 2.5 s of detection (log16: 1.63-2.39 s, 0.43-0.83
  m/s out);
- on the flow lane XKF7.FVC steps by one at the switch;
- Loiter stops within about 3.5 m by GPS and holds within about 0.7 m
  with the sticks centred (log16).

On topup2 every switch into THROW should print "EKF3 IMU0 stopped
aiding" at once, before arming, and the first throw after a GPS flight
then behaves like any other. Fly that case into VALT first, high and
over grass: switch to THROW straight from the GPS flight, arm, and
throw within about 10 s (log15's struck the ground 11.5 s after
arming). Pass: "stopped aiding" at the switch; "Throw height
achieved" or "lost position", never "good position", before "EKF3
IMU0 fusing optical flow"; and an XKF7.FVC step on the flow lane once
the flow starts.

Known, not changed by topup2:
- The throw's height target was 0.65-1.0 m above the vehicle at each
  Loiter hand-over, after it sank in the throw's last second, and Loiter
  then climbed 1.3-2.1 m (to 4.3 m on log16's throw 1).
- The range finder switched to its long-range mode at 5 Hz about 0.7 s
  before every throw and read No Data for up to 1.5 s, so the flow and
  the AGL KF lose the range over the throw. Check the ARK Flow MR's
  range mode settings.
## Not exercised by this card

- #31005: a fence floor breach at zero throttle now flies the RTL. Do not
  provoke it.
- #35039: a take-off from inverted. Do not provoke it.
- #33585's terrain-database anchor: SFD-O4's `EK3_OPTIONS` 94 has bit 2,
  which bypasses it.
- #32471's refusal of a zeroing that lands during the arm needs a Lua or
  DDS arm; a switch arm shows no difference.
- refresh14 topup1's throw into VALT acts only on a drop with no position at its
  end. (Its other fix, #34941's timeout for a vehicle that is not flying,
  ran on log89 while SFD-O4 was handled disarmed.)
- refresh14 topup2's fix acts only on a lockout pending when the range finder comes
  back after more than 5 s without a reading ("flow recovery deferred,
  range restarted"); a range finder that reads throughout never meets it.
- refresh14 topup3 changes nothing the vehicle does: a log with a pending arm's or
  the EKF_RESET switch's reset now replays through it.
- refresh14 topup4 acts only with THROW_TYPE 1 and THROW_ALT_DCSND 0, on a toss
  still above the release point when it is upright: the customer quad's
  tosses are the case, and a hand toss upwards on either vehicle would
  show it (expect the hand-over near the top, not at the hand).
- refresh14 topup6 acts only where its topup2 does, on a lockout pending when the range
  finder comes back after more than 5 s without a reading, and then only
  when the first reading back is held. Its other two changes do nothing
  on these builds. Its logs need a refresh14 topup6 or later Replay.
