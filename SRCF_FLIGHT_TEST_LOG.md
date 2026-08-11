# SRCF field test log - session 1

Branch `SmallFastDrone-4.7.0-gps-optflow-fallback`. Vehicle
SmallFastDronev1. Logs 326-330, 2026-08-11. Flights flown against
`432aba9209`; the code changes at the end are `ee39611791..62d8f804bc`.

Outcome: Flight 1 passed. Flight 2 passed on the second attempt after
a false spoof trip caused by flying above the rangefinder ceiling.
Flights 3-5 not yet flown. Two config errors found and fixed on the
day, and one detector design issue that has since been fixed in code.

## Config errors found

Both were divergences from the parameter table in the test plan.

**`EK3_SRC2_YAW = 0`** (plan calls for 1). Blocked arming entirely -
logs 326 and 327 never got off the ground. With no yaw source the
flow lane never does an initial yaw alignment: `XKF1` C=1 Yaw sat at
0.00 for every sample while C=0 held the real heading (163 deg in
326, 49 deg in 327). `AP_AHRS::attitudes_consistent()` compares every
active core against the primary at a 20 deg gate
(`AP_AHRS.cpp:54,2746-2764`), so the pre-arm failed permanently. The
reported error equals core 0's heading, which is the tell - it is an
unaligned lane, not drift, and "wait or reboot" can never clear it.
`EK3_SRC3_YAW` had been set to 1 instead. The passing SITL tests use
`EK3_SRC2_YAW=1` (`Tools/autotest/arducopter.py:3760,3812`).

**`EK3_OPTIONS = 124`**, missing bit1 (manual lane switching); plan
calls for `current | 2` = 126. Would not have blocked Flight 1, but
from Flight 2 the SRCF pre-arm rejects with `SRCF: bad
EK3_SRC_OPTIONS/EK3_OPTIONS` (`AP_Arming_Copter.cpp:239-248`), and
without it the monitor cannot command a lane switch at all.

`flight.lua` was not involved - it only writes `EK3_SRC1_*` and
`EK3_SRC_OPTIONS`. The stored parameters were simply wrong.

### Pre-arm gap

The SRCF pre-arm is gated on `srcf_enable > 0`, so during Flight 1
(`SRCF_ENABLE=0`) it validates nothing - exactly the flight that
first exercises the lane config. Even when enabled it does not check
`EK3_SRC2_YAW`, the one misconfiguration that guarantees the vehicle
cannot arm. The plan's claim that the pre-arm "validates the whole
lane config and names what is wrong" is not currently true.

## Flight 1 - log328, shadow lane, `SRCF_ENABLE=0`

Pass on every criterion.

| Criterion | Measured |
|---|---|
| `XKF4` C=1 `AID=2`, C=0 `AID=0` | both constant while armed |
| `PI` stays 0 | 0 throughout |
| `XKFS.SS` 0 on C=0, 1 on C=1 | confirmed, per-core sets live |
| cross-lane vel diff under ~0.5 m/s in hover | mean 0.077, max 0.229 |
| no EKF variance | `EKFC.Bad`=0, `FCnt`=0 |
| flies as normal | attitude tracks desired within 0.1 deg |

`EKF3 IMU1 MAG0 initial yaw alignment complete` now appears at 2.03 s
alongside IMU0, confirming the yaw fix. Vibration low (`VIBE.Z` max
7.7), zero clips, flow quality 112-224.

## Altitude: the EKF under-reads AGL

At the top of the Flight 1 box: `RFND` 8.8 m, baro 7.5 m, `CTUN.Alt`
6.5 m. The rangefinder is the honest one - regressing d(RFND)/dt
against GPS-Doppler climb rate gives slope 1.115, corr 0.874, so RFND
is within ~12% of truth while the EKF altitude is ~25% low.
`XKF6.HAgl` tracks RFND closely, so the AGL-KF is fine; it is the
main filter's baro-referenced height that is low, by design with
`EK3_OPTIONS` bit5.

Rangefinder max range is 15 m (`RRNH.MaxD`). Consequence for every
remaining flight card: **fly the events by rangefinder AGL, not the
GCS altitude readout.** 8 m true is about 6 m indicated.

## Flight 2 attempt 1 - log329, false spoof trip

`SRCF: GPS spoof suspected, using flow lane` at 122.89 s with GPS
healthy the whole time (`GpsB`=0, `GpsL`=1). The EKF never faulted -
`EKFC.Bad`=0, core-1 variances under 0.15, zero clips.

Cause was altitude. The flight ran at 13-16.5 m against the 15 m
sensor; `RFND.Stat` was 3 (out of range high) for most of the armed
time with readings to 22 m. The vote started at 120.99 s, inside a
rangefinder dropout - `XKFR` shows `RFresh`=0 with `RAge` climbing
720 to 1720 ms, and `XKF6` shows the AGL-KF coasting at 13.4 m with
`HAglStd` tripling 0.14 to 0.52, then stepping 1.25 m when the
rangefinder returned.

The deeper issue is geometry. Flow rate is velocity/height, so at
15 m the flow lane sees about a third of the angular signal it does
at 5 m. Through a direction reversal it could not keep up: at
122.59 s core 0 read VE -1.76 m/s against core 1's -0.72, and by
123.09 s the two lanes had opposite signs on VE.

### The vote relayed between two signals

| t (s) | VD (thr 0.8) | PR (thr 0.5) | Vote |
|---|---|---|---|
| 120.99 | 0.503 | 0.533 | 1 |
| 121.89 | 0.690 | 0.574 | 10 |
| 122.09 | 0.852 | 0.515 | 12 |
| 122.29 | 0.973 | 0.357 | 14 |
| 122.79 | 1.054 | -0.042 | 19 |
| 122.89 | - | - | latch |

PR held threshold for ~1.2 s and carried the vote to 13; VD then
crossed 0.8 and carried it the rest of the way. Neither held for the
full 2 s. `source_fallback.cpp:172-177` feeds one counter from an OR
of both conditions, so `SRCF_CNF_TIME` means "2 s of either", not
"2 s of sustained divergence".

### The lanes actually agreed

From `XKV1` (state covariance) immediately after the switch:

| | GPS lane C=0 | flow lane C=1 |
|---|---|---|
| NE velocity variance | 0.011, 0.011 | 0.167, 0.189 |
| NE position variance | 0.026, 0.025 | 7.69, 7.59 |
| 1-sigma velocity (2D) | 0.15 m/s | 0.60 m/s |
| 1-sigma position (2D) | 0.23 m | 3.9 m |

Combined 1-sigma velocity is 0.615 m/s, so the 1.065 m/s VD peak that
latched the spoof was **1.7 sigma**. Position divergence 1.63 m
against a 3.9 m combined sigma is **0.4 sigma**. The detector called a
spoof on lanes that were in complete statistical agreement, purely
because 0.8 and 0.5 are fixed constants that are tight at 8 m and
meaningless at 16 m.

Caveat: `XKV1` logs the primary core only, so the flow lane's
covariance is measured from 122.98 s, just after the switch. Nothing
physical changed across that instant and the lane had been in
relative aiding since 2.1 s, so the pre-trip values are almost
certainly the same or larger - but that is inferred, not measured.

### Aftermath was by design

The spoof branch sets `gps_untrusted` and enters `FLOW_SPOOF`;
auto-recovery exists only for the `FLOW_LOSS` rung
(`source_fallback.cpp:216-220`). The vehicle held the flow lane for
38 s until the disarm reset at 160.5 s. It stayed controllable -
position controller tracked target within ~1.5 m, controlled descent,
normal landing.

## Flight 2 attempt 2 - log330, at 8 m

No trip. `RFND` max 8.97 m with `Stat`=4 throughout, so the altitude
was right this time.

| Criterion | Result |
|---|---|
| zero `SRCF:` statustexts | yes, `St`=0, no lane switch |
| `Vote` max well under 20 | 5, one excursion 90.28-91.08 s |
| `VD` well under 0.8 | max 0.774 (97% of threshold) |
| `PR` well under 0.5 | max 0.566 (over threshold) |

Passes the gate the plan specifies, not the margin language. The
excursion was not provoked by anything - at 90.0-90.7 s the vehicle
was in a pitch/roll reversal at 0.56-1.07 m/s. That gentle a
manoeuvre reaching a quarter of a false latch is the same brittleness
as log329, just further from the edge.

The soak was also short: 72 s airborne at up to 3.1 m/s, against the
plan's 4-5 min with brisk translations, hard stops and fast yaw.

One `EKF3 IMU1 flow vel reset (axis lockout)` at 122.79 s, 0.1 s
before `LAND_COMPLETE` - one flow axis stale while the other passed
(`AP_NavEKF3_OptFlowFusion.cpp:827-834`). Expected at touchdown. Only
a concern if it starts appearing airborne.

## PR is the problem signal, not VD

| | log328 (reconstructed) | log329 (15 m) | log330 (8 m) |
|---|---|---|---|
| PR max | 0.527 | 0.58 | 0.566 |
| VD max | 0.62 | 1.06 | 0.774 |

PR crosses 0.5 on every flight at every altitude, including the
gentle ones. VD has never crossed 0.8 at the design altitude - only
at 15 m. `SRCF_POSR_THR=0.5` is too tight for this airframe's flow
lane; it is a permanent low-level contributor to the vote rather than
a discriminator.

## Design change - implemented same evening

Rejected first: gating the detector on rangefinder freshness. It
would have suppressed the log329 trip but caps the operating
envelope, and altitude operation is a requirement.

Implemented instead, in `ee39611791..62d8f804bc`:

- `AP_NavEKF3` logs state variances for every core rather than the
  primary only, and exposes a lane's NE velocity variance plus a
  frontend helper for the combined 1-sigma of a lane and the primary.
  `getVariances()` returns innovation test ratios, a different
  quantity, and was the only thing reachable before.
- `AP_AHRS` passes that through as `get_lane_divergence_sigma()`.
- `Copter` splits the vote integrator into one counter per signal, so
  a decaying signal can no longer hand over to a rising one, and adds
  `SRCF_NSIGMA`: a divergence must clear the fixed threshold *and*
  that many combined-lane sigmas. Recovery is judged against the same
  gates, else at altitude the flow lane's own imprecision blocks it
  indefinitely.
- `Copter` pre-arm now names `EK3_SRC2_YAW` when it is left at None.

The existing parameters keep their meaning: `SRCF_NSIGMA` is an
additional gate, not a redefinition, and 0 restores the old behaviour.

### What the verification changed

`SRCF_NSIGMA` was first set to 4. `SRCFGPSSpoof` then **failed** - the
gate suppressed real detection too. Measured rather than guessed:

| | divergence | combined sigma | ratio |
|---|---|---|---|
| SITL spoof, 1.5 m/s walk | VD 1.87, PR 1.92 | 0.50-0.64 | 2.9-3.9 sigma |
| Field false trip (log329) | VD 1.07, PR 0.58 | 0.615 | 1.7 sigma |

2.5 clears both and is what shipped. 7/7 SITL tests green: the three
SRCF tests plus EKFSourceSetFailsafe, OpticalFlowGPSLossAiding,
EKF3SRCPerCore and OpticalFlow.

The separation is a factor of under two, not the clean margin the
proposal assumed. A spoof slower than 1.5 m/s scales down with it into
the same range as the false trip, and the slowest detectable spoof
gets slower as the flow lane's uncertainty grows. The gate buys
altitude immunity at a real cost in spoof sensitivity. Two data points
is not a calibration - every `SRCF_ENABLE=1` flight should add its
`VSig` range to the picture.

### SRCF is invisible in SITL autotest logs

Pre-existing, not caused by the log format change. The autotest suite
sets `LOG_FILE_RATEMAX=10`
(`Tools/autotest/vehicle_test_suite.py:3213`) while the vehicle runs
0. `SRCF` is written by `WriteStreaming` at exactly 10 Hz, so
`should_log_streaming` (`AP_Logger_Backend.cpp:745`) drops it on any
jitter, and with no data write the `FMT` never lands either.

Consequence: the SITL tests can only assert on statustexts, never on
detector internals. Worth fixing if `VVot`/`PVot`/`VSig` are to be
regression-tested rather than eyeballed in field logs.

### Log field rename

`SRCF.Vote` is gone, replaced by `SRCF.VVot` and `SRCF.PVot`, and
`SRCF.VSig` is new. The analysis of logs 326-330 above predates the
rename and refers to `Vote`.

## Vehicle state at end of session

```
EK3_SRC2_YAW    = 1      (was 0 - the arming blocker)
EK3_OPTIONS     = 126    (was 124 - added bit1 manual lane switching)
EK3_SRC_OPTIONS = 8
EK3_IMU_MASK    = 3
EK3_PRIMARY     = 0
SRCF_ENABLE     = 1
SRCF_VEL_THR    = 0.8    (default)
SRCF_POSR_THR   = 0.5    (default - see recommendation below)
SRCF_CNF_TIME   = 2.0
SRCF_RECOV_TIME = 10.0
LOG_REPLAY      = 1
```

The vehicle still carries the session-1 firmware. Reflashing to
`62d8f804bc` adds `SRCF_NSIGMA`, default 2.5.

## Next session

1. Decide on Flight 2. Either fly the full 4-5 min card with hard
   stops and fast yaw for real soak evidence, or accept the thin
   margin and set `SRCF_POSR_THR = 0.9` (about 1.6x the worst PR
   across three flights) before continuing. Leave `SRCF_VEL_THR` at
   0.8 either way - it has behaved sensibly at 8 m.
2. Flight 3, the core GPS-loss test, at 8 m rangefinder AGL - about
   6 m indicated on this vehicle. Watch the cross-lane position
   offset at the moment of the switch; it ran 0.7-1.5 m at 8 m in
   log330, and that is what the position-reset handling has to
   absorb.
3. Flight 4, fallback under a 2-3 m/s translation, only if 3 is
   clean.
4. Flight 5 cannot run as written. `EK3_OPTIONS=126` includes bit6,
   so flow stays valid above the rangefinder ceiling and the
   demotion cannot trigger. Skip it, or clear bit6 for that card
   only.
5. Keep a Flight 3 log for the Replay check of the
   `requestLaneSwitch` DAL events.

### Code work

Done in `ee39611791..62d8f804bc`: split vote counters, `SRCF_NSIGMA`
significance gate, all-core `XKV1` logging, pre-arm `EK3_SRC2_YAW`
check. Flash before the next session.

Still open:

1. `SRCF` cannot be logged in SITL autotest, so no test asserts on
   `VVot`/`PVot`/`VSig`. Either raise `LOG_FILE_RATEMAX` for the SRCF
   tests or write `SRCF` at a rate below the cap.
2. `SRCF_NSIGMA=2.5` rests on two data points. Widen that with field
   `VSig` ranges before treating it as calibrated.
3. The pre-arm still only validates while `SRCF_ENABLE > 0`. A lane
   whose yaw source is None is a latent arming blocker under per-core
   source sets regardless of SRCF, so the general check belongs in
   `NavEKF3::pre_arm_check` next to the existing MAG_CAL/SRC1_YAW
   one - conditioned on the compass being used for yaw, else a
   no-compass vehicle is falsely rejected.

### Plan amendments

All applied to `SRCF_FLIGHT_TEST_PLAN.md` in the same revision as this
document: bench checks for `EK3_SRC2_YAW` and the IMU1 yaw-alignment
message, the rangefinder-AGL altitude note and 15 m ceiling, a written
action for a Flight 2 trip, the confirmed-unmet Flight 5 precondition,
the qualified pre-arm claim, `SRCF_NSIGMA` in the parameter table, and
the `VVot`/`PVot`/`VSig` field names throughout.
