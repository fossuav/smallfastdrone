# SRCF field flight test plan - session 2

Branch `SmallFastDrone-4.7.0-gps-optflow-fallback` @ `4650570584`.
Vehicle SmallFastDronev1. Written 2026-08-11 after session 1.

Session 1 flew Flights 1 and 2 and found two config errors and one
detector fault; see `SRCF_FLIGHT_TEST_LOG.md`. The GPS-loss rung -
the actual point of the feature - has still never been flown. This
session flies it, and validates the code that came out of session 1.

Setup, site requirements and the lane parameter block are unchanged
from `SRCF_FLIGHT_TEST_PLAN.md`; only the deltas are repeated here.
The cards below supersede that plan's Flights 2-5.

## What this session proves (and doesn't)

Three things, in this order of importance:

1. The GPS-loss rung in the field: clean handover to the flow lane,
   controllable flight on flow, auto-recovery. Never yet flown.
2. That `SRCF_NSIGMA` actually suppresses the session-1 false trip.
   The value 2.5 was set from one SITL spoof and one field trip and
   has never met real air.
3. A `VSig` dataset across the height band, which is what the gate
   needs to be calibrated rather than guessed.

Still not proven: spoof detection in the field. Transmitting GNSS
spoofing is illegal, so the spoof path stays SITL-only and the field
can only show the detector staying quiet when it should.

## Before leaving - bench

- [ ] Parameter backup of the current config, before anything else.
- [ ] Flash `4650570584` or later. Session 1 flew `432aba9209`, which
      predates every code change below.
- [ ] Confirm `SRCF_NSIGMA` exists and reads 2.5. If the parameter is
      absent the flash did not take.
- [ ] Confirm the rest of the lane block still matches the session-1
      table, in particular `EK3_SRC2_YAW=1` and `EK3_OPTIONS=126`.
- [ ] Pre-arm negative test, five minutes and worth it. With
      `SRCF_ENABLE=1`, set `EK3_SRC2_YAW=0` and reboot: arming must
      now be refused with `SRCF: set EK3_SRC2_YAW`. The AHRS "Yaw
      inconsistent" message may appear as well - what matters is that
      something names the parameter. Set it back to 1, reboot,
      confirm arming passes. This is the check that would have saved
      session 1's first two flights.
- [ ] Short ground log with `SRCF_ENABLE=1`, motors armed briefly.
      Confirm in the log: `XKV1` present for both `C=0` and `C=1`,
      and `SRCF` carrying `VVot`, `PVot` and `VSig`. All three are
      new. If `SRCF` is missing, check `LOG_FILE_RATEMAX` is 0 - at
      10 it silently drops the message.
- [ ] Guarded RC switch on GPS Disable (`RCx_OPTION=65`), cycled on
      the bench.
- [ ] `LOG_REPLAY=1`, SD card with room.

**Do not use the CRSF indoor/outdoor profile.** Unchanged from
session 1: `flight.lua` rewrites `EK3_SRC1_*` and `EK3_SRC_OPTIONS`
and will break the lane assignment.

## Site

As session 1, plus: Card 2 and Card 5 need clear airspace to 20 m and
a ground reference for judging 15 m by eye, because the GCS altitude
reads about 25% low. Fly every height call by rangefinder AGL.

Reminder of the numbers that matter on this vehicle: 8 m true is
about 6 m indicated; the rangefinder ceiling is 15 m.

## What changed in the code, and what each card tests

| Change | Card that tests it |
|---|---|
| `SRCF_NSIGMA` significance gate | 2 (the profile that false-tripped) |
| Split vote counters | 1 and 2 (no relay confirmation) |
| Recovery judged against the same gates | 5 (recovery at altitude) |
| All-core `XKV1` | bench, then every card |
| `EK3_SRC2_YAW` pre-arm | bench |

Note the GPS-loss trip itself is **not** affected by `SRCF_NSIGMA`.
It fires on receiver status - no 3D fix, or no message for 1 s - not
on cross-lane divergence. Cards 3-5 exercise the loss path whatever
the gate is doing.

## Cards - fly in order, review logs between

### Card 1 - proper soak at 8 m (`SRCF_ENABLE=1`)

Session 1's soak was 72 s of gentle flying and does not count. Fly
the whole thing: 4-5 min Loiter at up to 8 m rangefinder AGL, with
brisk translations, hard stops, fast yaw, and a low pass at ~2 m.

- Pass: zero `SRCF:` statustexts, `VVot` and `PVot` both well under
  20.
- Record: `VSig` range, `VD` and `PR` peaks. Session 1 at this height
  gave `VD` max 0.774 and `PR` max 0.566 with the old code.
- If `PVot` climbs while `VVot` stays quiet, that is the session-1
  pattern - `PR` is the noisy signal on this airframe. Raise
  `SRCF_POSR_THR` to 0.9 before continuing, not `SRCF_VEL_THR`.
- Abort: any trip with GPS healthy means the gate is not doing its
  job at the design altitude. Land, pull the log, stop for the day.

### Card 2 - altitude regression (`SRCF_ENABLE=1`)

This is the session-1 false trip, deliberately reproduced. Climb to
13-16 m rangefinder AGL and fly the same profile that tripped it:
translations at 2-3 m/s with direction reversals, the kind of turn
that had the two lanes reading opposite velocity signs.

- Expect: **no trip.** At the `VSig` of about 0.6 m/s session 1
  measured up there, `SRCF_NSIGMA=2.5` puts both effective gates at
  roughly 1.5 m/s, against a session-1 `VD` peak of 1.065 - about
  45% margin.
- Record `VSig` at height. This is the single most useful number of
  the session. If it comes out much below 0.6 the gates are tighter
  than predicted and the margin is smaller than it looks.
- The rangefinder will drop out above 15 m; that is expected and is
  part of the condition being tested.
- If it trips anyway: the fix is insufficient, not the flying. Land,
  and treat 2.5 as a lower bound rather than raising it blind - the
  SITL spoof only clears 2.9 sigma, so there is not much room above
  2.5 before real detection starts failing.

### Card 3 - GPS-loss fallback and recovery (the core test)

The card session 1 never reached. At ~8 m rangefinder AGL, hands-off
Loiter hover.

1. Flip GPS Disable ON.

   Expect within ~1 s: `SRCF: GPS lost, using flow lane` then
   `EKF3 lane switch 1`. The vehicle should not visibly move.

2. Hold hands-off 20-30 s, then gentle stick inputs - expect a
   slightly softer, speed-limited response.
3. Flip GPS Disable OFF.

   Expect `SRCF: GPS recovered` + `EKF3 lane switch 0` after
   ~10-25 s.

4. Land, disarm.

- Pass: excursion under ~2 m at the switch, controllable throughout,
  clean recovery, no `EKF variance`.
- Watch `SRCF.PD` at the switch - that offset is what the position
  controller's reset handling absorbs. Session 1 saw 0.7-1.5 m at
  this height.
- Abort: any lurch, lean or position walk -> GPS back ON at once; if
  it does not settle -> AltHold and land. The stock EKF failsafe
  (LAND) is the final backstop.

### Card 4 - fallback under motion (only if Card 3 clean)

Card 3 again, but flip GPS Disable during a slow 2-3 m/s translation.
Same expectations; recover and land.

### Card 5 - loss and recovery at altitude (only if 2-4 clean)

The recovery gate change, tested where it matters. At 13-15 m
rangefinder AGL, hands-off hover, GPS Disable ON, hold 20-30 s, GPS
Disable OFF.

- Expect recovery to complete. Under the session-1 code this was
  impossible up here: `PR` swung past +/-0.5 continuously, resetting
  the recovery timer, and the vehicle stayed on the flow lane until
  disarm.
- Pass: `SRCF: GPS recovered` within ~10-25 s of GPS returning.
- Higher workload than Card 3 and the flow lane is at its weakest.
  Skip without hesitation if anything about Cards 3-4 felt marginal.

### Card 6 - AltHold demotion rung (optional)

Cannot trigger at `EK3_OPTIONS=126` - bit6 keeps flow valid above the
rangefinder ceiling. To fly it, clear bit6 (`EK3_OPTIONS=62`) for
this card only and restore 126 afterwards; note that doing so changes
the configuration every other card was flown in, so fly it last.

At ~8 m with GPS Disable ON, climb past the rangefinder ceiling.
About 5 s after range is lost expect `SRCF: no nav source, AltHold`
and the mode change. Fly AltHold manually, descend, GPS ON, land.

SITL already covers this rung. Skipping it costs little.

## Bring back

The `VSig` table is the deliverable that outlasts this session:

| Height (RFND AGL) | `VSig` range | `VD` peak | `PR` peak | Card |
|---|---|---|---|---|
| ~2 m (low pass) | | | | 1 |
| ~8 m | | | | 1, 3, 4 |
| 13-16 m | | | | 2, 5 |

Also per card: state transitions, `VVot`/`PVot` peaks, `PD` at each
switch, `EKFC.Bad` (must never set), `XKF4` `AID`/`PI` against the
statustext timeline, and `XKV1` `V04`/`V05` for both cores.

Keep a Card 3 log for the Replay check of the `requestLaneSwitch` DAL
events (`Tools/Replay/check_replay.py`).

## What would make this session a pass

Cards 1-4 clean, Card 2 in particular showing no trip at altitude.
That would make the GPS-loss rung flight-validated and the
significance gate field-tested once.

It would not make `SRCF_NSIGMA=2.5` calibrated. That needs the `VSig`
spread from several more ordinary flights, and the honest position
stays: the gap between a real spoof and a false trip is under a
factor of two, so the gate trades spoof sensitivity for altitude
immunity and we do not yet know the exchange rate.

## Rollback

`SRCF_NSIGMA=0` restores the session-1 fixed-threshold behaviour -
which is the configuration that false-tripped, so it is a diagnostic
setting, not a safe one. `SRCF_ENABLE=0` disarms the monitor in
place. Full revert: restore the parameter backup, or reflash
`432aba9209` for session-1 behaviour.
