# SFD flight card: refresh14 topup6

The flights still to fly, in order. Results go in `FLIGHT_VALIDATION.md`;
earlier cards are in git history.

Firmware: refresh14 topup6, built from `cc0af6427f`: refresh13 topup1's
code (`896e11846d`, flown by SFD-O4 on 2026-11-07 and by the T25 as
`7b00f3b5` on 2026-11-08) plus topup1's two fixes from a customer quad's
flights, the throw into VALT without a position and #34941's timeout for
a vehicle that is not flying, topup2's #33484 lockout recovery that
waits for a restarted AGL KF to be confirmed, topup3's replay event for
a bootstrap reset, topup4's throw hand-over with THROW_ALT_DCSND 0,
topup5's SmartRTL return point reset (#35119), which also changes how
arming resets the SmartRTL path, and topup6's AGL KF restart that copies
of one range reading no longer confirm (`REFRESH_NOTES.md`). Nothing to
set.

- SFD-O4: SmallFastDronev1 `arducopter.apj`, md5 `e54e2ba4`.
- T25: MicoAir743v2 `arducopter.apj`, md5 `da5e2c6f`.
- topup5: any SmartRTL return on either vehicle exercises the new arming
  reset and should come home as before; the in-flight reset itself needs
  `smartrtl-reset.lua` installed and a Scripting1 (300) switch.

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
  re-arming. Still owed: a re-arm after a hard landing without a reboot
  (the T25 re-armed after a soft one on 2026-11-08).
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
- topup1's two fixes: the throw into VALT acts only on a drop with no
  position at its end, and #34941's timeout only on a vehicle held tilted
  past the flow tilt limit for over 5 s while not flying (carried before a
  throw).
- topup2's fix acts only on a lockout pending when the range finder comes
  back after more than 5 s without a reading ("flow recovery deferred,
  range restarted"); a range finder that reads throughout never meets it.
- topup3 changes nothing the vehicle does: a log with a pending arm's or
  the EKF_RESET switch's reset now replays through it.
- topup4 acts only with THROW_TYPE 1 and THROW_ALT_DCSND 0, on a toss
  still above the release point when it is upright: the customer quad's
  tosses are the case, and a hand toss upwards on either vehicle would
  show it (expect the hand-over near the top, not at the hand).
- topup6 acts only where topup2 does, on a lockout pending when the range
  finder comes back after more than 5 s without a reading, and then only
  when the first reading back is held. Its other two changes do nothing
  on these builds. Its logs need a topup6 Replay.
