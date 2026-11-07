# SFD flight card: refresh13 topup1

The flights still to fly, in order. Results go in `FLIGHT_VALIDATION.md`;
earlier cards are in git history.

Firmware: refresh13 topup1, built from `896e11846d`. SFD-O4 flew it on
2026-11-07 (logs 84-88, `FLIGHT_VALIDATION.md`); the T25 has not.

- SFD-O4: SmallFastDronev1 `arducopter.apj`, md5 `666ed4aa`.
- T25: MicoAir743v2 `arducopter.apj`, md5 `4d9d5598`.

Fly every flight with `LOG_REPLAY 1` and `LOG_DISARMED 2`, and STABILIZE
on a switch.

## SFD-O4

### Before the first flight

- Set `LOIT_DRAG_COEF` 0.3. It moved to a new parameter index in
  refresh13, so a value saved before it reads the board default, 0.01.
- `LOIT_OPTIONS` 1. Bit 6 is gone; if it reads 65, set 1.
- `EK3_OPTIONS` 94.

### Bench, props off

1. #32401: a pending arm on the switch. Flip the arm switch while a
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
2. #31274: motor test each motor in turn at 5-10 % for 3 s. Pass: no
   "Motor Test: motor N errors X% at Y rpm" for a motor that did not spin,
   and ESC.Err at or below 1 on the motor under test.
3. #34630: the OSD flow quality floor, disarmed. `EK3_FLOW_QMIN` 60 with
   `EK3_OPTIONS` 94, then cover the flow sensor. Pass: the lane strength
   reads below 50 and flashes. With `EK3_OPTIONS` 86 the same cover shows
   the strength with no floor and no flashing. Restore `EK3_OPTIONS` 94
   and `EK3_FLOW_QMIN` to its usual value (40).

### Every flight

- #34942: note any arm refused on "EKF3[n] height error", and any
  "vertical velocity reset at rest" or "forced reset", with the time since
  the disarm. After a hard landing, leave it still for at least 1 s before
  re-arming. Still owed: a re-arm after a landing without a reboot.
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
- Every flight: the #34942 notes as for SFD-O4.

## Not exercised by this card

- #31005: a fence floor breach at zero throttle now flies the RTL. Do not
  provoke it.
- #33585's terrain-database anchor: SFD-O4's `EK3_OPTIONS` 94 has bit 2,
  which bypasses it.
- #32471's refusal of a zeroing that lands during the arm needs a Lua or
  DDS arm; a switch arm shows no difference.
