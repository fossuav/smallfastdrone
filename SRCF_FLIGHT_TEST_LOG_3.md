# SRCF field test log - session 3

Branch `SmallFastDrone-4.7.0-gps-optflow-fallback`. Vehicle: a second
airframe, MatekH743-bdshot on an OCTAQUAD/X_REV, hover throttle 0.265
against SmallFastDronev1's 0.095. Logs 39-57, 2026-08-13/14. Flights
flown against `60b12780`; the code changes at the end follow it.

Outcome: SRCF is field-validated on a second airframe - four GPS-loss
cycles, 0.25 s handover and 10-12 s recovery every time, matching
session 2 to a tenth of a second. Getting there took a flow
calibration, a position-hold retune and two bugs in the calibration
tooling that had been reporting a correct sensor as 45% out. The
mission profile itself, 30 km/h at 25 ft, then put the velocity spoof
detector one vote from a latching false trip, which is a limit of the
detector rather than of the tune. A position-offset detector was added
to cover the case the rate detectors structurally cannot, off by
default and unflown.

## Bringing the sensors up - log 39

An ARK Flow MR on DroneCAN node 124 supplies both flow (`FLOW_TYPE=6`,
121 Hz) and the rangefinder (`RNGFND1_TYPE=24`, 20 Hz). 92 s in
ALT_HOLD under 1.4 m, no GPS fix at any point.

What the flight settled:

| check | result |
|---|---|
| flow orientation | node gyro vs FC IMU slope +1.01 X, +1.02 Y |
| flow node output rate | same slopes - no `FLOW_HF_RATEF` needed |
| flow quality | mean 163, zero samples below 50 airborne |
| rangefinder | `Stat=4` all flight; the 87 bad samples are post-landing |
| fusion | `HSrc=2` 98.5%, `XKF6.Valid=1`, range innovation +/-0.15 m |

The orientation result is stronger than it looks. `FLOW_ORIENT_YAW`
rotates the node's flow and its gyro alike, so both axes reading +1.0
against the body-frame FC IMU confirms the 180 deg setting matches the
mounting: 180 deg out reads -1.0 on both, 90 deg swaps the axes and
collapses the correlation. The same measurement rules out the ARK Flow
`integration_interval` fault, which no `FLOW_*SCALER` can correct.

Flow scale was not measurable - no GPS, no truth. The scalers in force
(`FXSCALER=-88`, `FYSCALER=-148`) were the small copter's, never
measured on this airframe.

## What the flow calibration tooling got wrong

Log 41 was the calibration flight: heading held, deliberate forward and
strafe runs, 3665 and 4307 usable samples. It first reported flow
reading **0.55** of truth and asked for `FXSCALER=+650`. That was
wrong, and it was `flow_cal_check.py`, not the sensor. Three defects,
all now fixed:

1. **Sign.** Every log ever run through it reported "sign inverted".
   `writeOptFlowMeas` negates the raw flow before compensating
   (`AP_NavEKF3_Measurements.cpp:225`, `flowRadXYcomp = -rawFlow +
   bodyRate`) while `ROFH`/`OF` log the driver's pre-negation values.
   The tool compared against the model with the wrong sign.
2. **Height reference.** It defaulted to `XKF6.HAgl` on the reasoning
   that calibrating against the height the EKF feeds the flow fusion
   makes EKF velocity right by construction. But the AGL KF is
   filtered, and on this airframe it lags the rangefinder by 0.7 s.
   Same flight, same data, only the range source changed:

   | range source | X ratio | corr | Y ratio | corr |
   |---|---|---|---|---|
   | `XKF6.HAgl` | 0.55 | 0.87 | 0.56 | 0.75 |
   | `RFND.Dist` | 0.97 | 0.97 | 0.91 | 0.93 |

   The AGL KF's lag was being absorbed into the flow scaler. Default is
   now the raw rangefinder.
3. **A hard 8 m rangefinder cap**, a leftover from small-copter use.
   Flying at 7-9 m, only the short readings survived, so the height
   used came out low and the scale came out low with it - 0.75 on an
   axis that is actually 1.03, reported `[reliable]` with `corr` 0.96,
   because truncation biases a fit without scattering it. The
   fingerprint was a degenerate height spread, p5 7.82 against a mean
   of 7.77. Now capped at `RNGFND1_MAX` with a warning when samples
   crowd the limit.

The rangefinder itself was validated before any of this was trusted:
`d(RFND)/dt` against GPS Doppler climb rate gives slope **1.02**, and
`RFND` against `BARO` gives **0.97** at corr 0.89. Two independent
witnesses, both 1:1.

Settled scalers: `FXSCALER -57`, `FYSCALER -110`. Log 43 confirmed
1.03 and 1.10 against the -57/-68 pair; Y was walked to -110 as the
midpoint of a 10% spread across two flights.

## Altitude hold on the rangefinder - log 39

The pilot reported poor height hold. The rangefinder was the height
source 704 of 715 in-flight samples, and the controller tracked its own
target well - 0.069 m RMS, 0.159 m peak over seven parked-target
segments. The estimate is what under-responds:

| band | gain of `CTUN.Alt` per `RFND.Dist` | corr |
|---|---|---|
| slow (>5 s) | 0.83 | 0.90 |
| fast (<5 s) | **0.31** | 0.63 |

Over parked segments the rangefinder moved 0.16-0.67 m while the
control altitude moved 0.04-0.18 m, with as little as 1.8 m of
horizontal travel - too little for terrain to explain, and the EKF's
terrain state moved only 0.12 m all flight. The filter carries a
standing vertical position innovation of +/-0.25 m (`XKF3.IPD`, std
0.147) against its own height measurement.

A first hypothesis - that `posDownObsNoise = MAX(aglKfP[0][0],
sq(EK3_RNG_M_NSE))` discards the AGL KF's 0.015 m^2 covariance in
favour of 0.25 m^2 - was **not supported**: `EK3_RNG_M_NSE` is 0.5 on
both airframes, and the small copter holds height well. But the small
copter is not a control. Its switch threshold is `RNGFND1_MAX` 15 x
`EK3_RNG_USE_HGT` 6% = **0.9 m** and it cruises at 5-10 m, so across
ten of its logs the rangefinder is the height source for 0-7% of
airborne time. It holds height on baro. The comparison was never
apples to apples and this remains open.

## Ground effect - what could not be measured

`EK3_GND_EFF_DZ` is 4.0 here (the stock default) against -8.0 on the
small copter, which selects the fork's noise-floor mode. `TKOFF_GNDEFF_TMO`
is 0 against 3. So the airframes are not running the same mechanism.

No better value could be derived from log 39, and the reasons are worth
recording:

- Spool-up baro is clean, within +/-0.2 m. The parameter is specified
  against a sustained pre-takeoff offset this vehicle does not show.
- What there is instead is a 0.3 s liftoff transient to -5.73 m. Three
  samples; four in the whole log below -1 m. Sizing a dead zone to a
  spike would be fitting noise.
- The sustained +0.54 to +0.67 m in-flight offset is flat from 0.25 to
  1.25 m AGL. Ground effect decays with height; this does not.
- It is confounded with thermal drift. Ground-to-ground the baro moved
  +0.30 m with the motors off at both ends, while its own temperature
  fell 61.2 to 59.0 C.
- The flight never left ground effect. Max 1.38 m on a machine with
  about a metre of rotor span, so there is no out-of-ground-effect
  baseline to measure against.

`XKF5.BOf` converged to +0.589 m, so `calcFiltBaroOffset()` is tracking
the offset and the 1.5 m handover should be smooth.

The flight that would produce the numbers: arm, sit 10 s, lift to
0.4 m for 15 s, climb to 8-10 m for 30 s, descend in 2 m steps holding
10 s each. The 8-10 m hold is the missing piece.

## Flow LOITER and the 2.4 s limit cycle - logs 43, 44

Switching the source set to flow in LOITER produced a sustained sway.
Station-keeping, on flow only:

| window | source | speed | roll sd | period |
|---|---|---|---|---|
| 95-135 s | GPS | 1.66 m/s | 1.1 deg | 11.9 s |
| 137-146 s | flow | 0.30 m/s | 4.8 deg | 2.5 s |
| 151-156 s | flow | 0.44 m/s | 5.6 deg | 2.3 s |
| 195-215 s | GPS | 1.61 m/s | 0.8 deg | 9.8 s |

Ruled out by measurement: lag (band-passed at the sway frequency the
EKF velocity aligns with raw GPS at 0.00 s in every window), velocity
noise (0.17-0.28 m/s residual, same as the GPS windows), and scale
(1.03/1.10 once the tooling was fixed).

The cause was `EK3_FLOW_GAIN_H = 12`. The scaler is `gainHgt /
MAX(HAGL, gainHgt)` (`AP_NavEKF3_Outputs.cpp:440`), applied to both the
velocity and acceleration setpoints at `AC_PosControl.cpp:735` and
`:753`, and active only while navigating on flow. At the 7-9 m flown it
gives **1.00** - no detune at all. The parameter's own description
warns that larger values carry "more risk of a flow-driven
oscillation".

Log 44 set it to 4. Matched to the 6-8 m band so height is not a
confound:

| | VGain | roll sd | true ground speed |
|---|---|---|---|
| log 43 | 1.00 | 2.7 deg | 0.41 m/s |
| log 44 | 0.71 | **1.4 deg** | **0.28 m/s** |

Both better - the loop was not holding position, it was oscillating
around it. `PSC_NE_*` and `EK3_FLOW_GAIN_H` are byte-identical on both
airframes; what differs is a third of the thrust margin and far more
inertia.

Log 46 tried `PSC_NE_VEL_D` 0.25 to 0.10 and `FLTD` 3 to 2, and felt
worse. It does not test what it looks like: `XKF4.AID` was 0
(absolute) and `VGain` 1.00 for the entire source-set window - the EKF
never moved to flow. What got worse was GPS LOITER, degraded by gains
acting on the source actually being flown. Both reverted.

## The source switch that ran away - log 48

Returning from flow to GPS in LOITER produced a 15 m dash at 5.7 m/s.
Flow was not at fault: dead reckoning over 102 s accumulated **0.6 m**
of position error against GPS truth (0.006 m/s).

At t=191.74 the EKF position state jumped **4.54 m in one sample** and
landed 3.6 m from GPS truth. LOITER set off to correct. Then it stuck:
a GPS glitch was declared at 195.7 s and, with `EK3_GLITCH_RAD = 0`,
the bounded-update path crawls toward GPS instead of resetting. The
error grew to 7 m and the glitch did not clear until 214.5 s -
**18.8 seconds**, with 13-15 satellites throughout.

`EK3_GLITCH_RAD = 0` is right for a 50 m/s vehicle and was inherited
from one. This airframe peaks near 6 m/s. Set to 25.

The structural fix is per-core lanes. The global source-set switch
takes both lanes off GPS, so the return requires re-establishing
absolute position from scratch. Under `EK3_SRC_OPTIONS = 8` lane 0
stays on GPS continuously.

`EK3_FLOW_M_NSE` was tried at 0.1 on this flight. Measured flow
measurement noise - residual scatter of gyro-compensated flow about
GPS-implied flow, per axis - is 0.13-0.35 rad/s across three logs, and
that is an upper bound since it also contains GPS and attitude error.
The shipped 0.25 sits inside it. Returned to 0.25; dead reckoning was
0.3 m/79 s at 0.25 and 0.6 m/102 s at 0.1, indistinguishable.

## SRCF soak - log 53

`EK3_SRC_OPTIONS=8`, `EK3_OPTIONS=94`, `EK3_GLITCH_RAD=25`,
`SRCF_ENABLE=1`. The lane split came up correctly on the first try:

```
EKF3 IMU0 is using GPS
EKF3 IMU1 fusing optical flow
EKF3 IMU1 started relative aiding
```

No trip in 103 s. But `PVot` reached **9 of 20** in a single 1.6 s
excursion where `|PR|` hit 2.19 against the 1.9 gate - 45% of the way
to a false spoof latch, on an airframe whose thresholds came from a
different one. `|VD|` maxed at 1.22 against 1.6.

`SRCF_POSR_THR` raised to 2.6. Session 2's precedent was a 7% margin
off five flights; one flight justifies more.

`SRCF_NSIGMA` is inert here exactly as it was in session 2: `2.5 x
VSig` is 1.04 at the median against a 1.9 floor, so the `MAX()` clamps
to the fixed threshold at every sample.

## GPS loss ladder - log 55

Four cycles, all clean, using an `RC7: GPSDisable` switch.

| cycle | detect | recover |
|---|---|---|
| 1 | 0.25 s | 12.13 s |
| 2 | 0.24 s | 12.10 s |
| 3 | 0.25 s | - |
| 4 | 0.26 s | 10.11 s |

Session 2 measured 0.20 s and 12.2 s on the other airframe. `VVot` and
`PVot` were **zero for the entire six-minute flight** with the raised
threshold, and `GU` never latched.

Flow-lane hold with hands off: **0.5 m over 16 s** and 1.1 m over 19 s.

Three transitions showed 10-22 m of movement, which is pilot input,
not the monitor: pitch stick 427-489 us off centre on those three
against 0-258 us on the other five. `PD` at the switch instants was
0.0-3.1 m, so no jump was injected.

Every lane switch logs an `EKF_YAW_RESET`, which looks alarming and is
not: the two lanes' yaw differed by 0.48 to 2.37 deg at all eight
switches.

## The mission profile, and the detector that nearly tripped - log 57

Target is 30 km/h at 25 ft (8.33 m/s, 7.62 m). `VVot` reached
**19 of 20**.

| speed band | max \|VD\| | max \|PR\| |
|---|---|---|
| 0-2 m/s | 1.26 | 1.71 |
| 2-4 m/s | 1.41 | 1.97 |
| 4-6 m/s | 1.58 | 1.79 |
| 6-8 m/s | 2.10 | 1.23 |
| 8-12 m/s | **2.17** | 1.77 |

`VD` is a velocity difference, so it scales with velocity. The gate is
1.6. At cruise the benign envelope is 135% of it, sustained 8.3 s
across two excursions, monotonic and still rising at cutoff.

Raising `SRCF_CNF_TIME` does not fix it - a longer window only delays
the trip while the speed is held. `SRCF_NSIGMA` does not either: `2.5 x
VSig` was 1.20-1.48 during the excursion, under the floor exactly when
it was needed.

`SRCF_VEL_THR` must go to 3.0 for this profile, and the cost is real:
session 2's velocity-consistent SITL spoof sustained `VD` at 1.77, so
at 3.0 it is undetectable. **There is no fixed threshold that survives
30 km/h and catches a 1.77 spoof.** The GPS-loss path is unaffected -
it runs on `GpsB`/`GpsL`, not the vote counters, which is why log 55's
four cycles ran with both counters at zero.

`SRCF_POSR_THR` was the wrong worry. It maxed at 1.97 against 2.6 with
no speed trend, and `PVot` never left zero.

## A position offset detector

Position inequality is speed-invariant where velocity inequality is
not, because the flow lane's position uncertainty accumulates from the
same dead reckoning error that opens the offset. `SRCF_POSD_NSIG`
tests `PD` against `SRCF_POSD_NSIG x pos_sigma`, feeding the same
confirmation path. Default 0.

The denominator is floored at 2 m. The flow lane only earns
uncertainty by dead reckoning, so without a floor the divisor is
smallest just after takeoff: log 57 pairs a 2.4 m offset with a 0.79 m
sigma at 0.2 m AGL, a ratio of 3.11 against an airborne worst of 2.05.
The detector would be most exposed where it is least useful. The floor
applies to detection only; on the recovery bound it would loosen an
already deliberately loose gate.

SITL (`SRCFSlowSpoofPositionOffset`) covers both halves. A 0.8 m/s
position-only walk, inside both rate gates:

| phase | `SRCF_POSD_NSIG` | VVot | PVot | OVot |
|---|---|---|---|---|
| default | 0 | 0 | 0 | **0** |
| enabled | 3 | 0 | 0 | **19** |

The disabled phase settles session 2's open question: a walk this slow
is genuinely invisible to both shipped detectors over 35 s. The cost of
seeing it is latency - **36.7 m of drag before detection**, because an
integrating detector cannot fire until the offset has built.

## Replay across eight flights

The detector logic was replayed over every log carrying `SRCF` records
in the current format, with `pos_sigma` rebuilt as
`sqrt(getPosVarianceNE(lane) + getPosVarianceNE(primary))` from
`XKV1.V07+V08`. The replay reproduces the firmware's own counters
first - `PVot 9/9` on log 53, `VVot 25/25` and `PVot 18/18` on log 333
- and only then is anything it says about the new detector believed.

Floored `PD/pos_sigma`, benign:

| log | airframe | max | trips at NSIG |
|---|---|---|---|
| 53 | octaquad | 1.51 | 1.5 |
| 55 | octaquad | 0.80 | - |
| 57 | octaquad | 1.64 | - |
| 332 | small quad | **2.32** | 1.5, 2.0 |
| 333 | small quad | 1.79 | 1.5 |
| 334 | small quad | 0.97 | - |
| 335 | small quad | 1.07 | - |
| 336 | small quad | 2.28 | 1.5, 2.0 |

`SRCF_POSD_NSIG` at 2.5 and above is clean on all eight. **Start a
field tester at 4** - 1.7x over the worst - and tighten toward 3 only
after they have soaked their own airframe. 3 leaves 29% margin, which
is the margin `VVot` had at 19 of 20.

Logs 329 and 330 predate the two-detector split (single `Vote`, no
`VSig`) and were skipped rather than approximated.

The small quad having the higher ratio is not what the octaquad data
predicted, and log 332 is the session-2 flight that false-tripped
`VVot`. Whatever made that flight unusual pushed both detectors, which
argues against assuming the two are orthogonal.

## Tooling changes

`flow_cal_check.py`: the sign, height-reference and range-cap fixes
above, plus a full-DCM body velocity (yaw-only is ~6% out at 20 deg of
tilt) and an automatic GPS lag scan. The lag scan is reported, not
trusted - it picked 0.35-0.45 s where an independent measurement of
GPS velocity against the EKF's delay-compensated velocity puts the true
lag near 0.10 s.

`log-analyze/SKILL.md`: the height-reference and truncation traps are
written up, since both produced a confidently wrong number that
reported itself as reliable.

## Vehicle state at end of session

```
EK3_SRC_OPTIONS = 8
EK3_OPTIONS     = 94      (126 - 32, AglKfVelForVelD cleared)
EK3_GLITCH_RAD  = 25      (was 0)
EK3_RNG_USE_HGT = 6
EK3_FLOW_GAIN_H = 4       (was 12)
EK3_FLOW_M_NSE  = 0.25
FLOW_FXSCALER   = -57     (was -88)
FLOW_FYSCALER   = -110    (was -148)
SRCF_ENABLE     = 1
SRCF_VEL_THR    = 1.6     -> 3.0 needed for the 30 km/h profile
SRCF_POSR_THR   = 2.6     (was 1.9)
SRCF_POSD_NSIG  = 0       (4 suggested for a tester)
SRCF_CNF_TIME   = 2.0
SRCF_RECOV_TIME = 10.0
SRCF_NSIGMA     = 2.5     (inert - the fixed floor always wins)
```

`EK3_OPTIONS` bit 5 was cleared because it produced terrain-following:
it fuses the AGL KF's terrain-relative vertical velocity as velD, and
engages only when no velZ source is active, which on the flow lane
(`EK3_SRC2_VELZ=0`) is always. The cost is that vertical velocity on
flow is now IMU plus baro only.

## Still open

1. `SRCF_VEL_THR = 3.0` is required for the mission profile and was
   not flown. The soak that measured 2.17 ended as `VVot` hit 19; a
   sustained 30 km/h leg could produce more. Fly it and re-read the
   envelope before trusting the number.
2. Spoof detection and the 30 km/h profile are incompatible at any
   fixed velocity threshold. Either the mission accepts no spoof
   detection, or the gate has to scale with speed - which is a code
   change, and the position-offset detector is the first attempt at
   sidestepping it.
3. The GPS-loss cycle has not been flown at cruise. Everything on the
   flow lane has been at or below 5.5 m/s. A 3-5% residual scale error
   at 8.33 m/s is 15-25 m per minute of outage; that is arithmetic and
   deserves a measurement.
4. `SRCF_POSD_NSIG` has never been flown at any value. It is off by
   default for that reason. A tester should soak with it at 0 and read
   `PD` against the new `PSig` field on their own airframe first.
5. Altitude hold on rangefinder height is unresolved. The estimate
   follows the rangefinder at 0.31 in the sub-5 s band and the cause is
   not `EK3_RNG_M_NSE`, which is identical on both airframes. The
   SITL A/B on that parameter was never run.
6. Ground effect is uncharacterised on this airframe, and
   `EK3_GND_EFF_DZ`/`TKOFF_GNDEFF_TMO` are on the stock path rather
   than the fork's. The flight profile that would settle it is in this
   log.
7. The baro sits at 61 C before the motors spin and drifts 0.30 m in
   78 s of cooling. That contaminates every altitude measurement on
   this airframe and is a bench problem, not a tuning one.
8. Session 2 open items 2, 3 and 5 stand unchanged: the detection
   limits still belong in user-facing documentation, `SRCF_NSIGMA`
   remains uncalibrated and is now measured inert on two airframes,
   and the pre-arm still only validates while `SRCF_ENABLE > 0`.
   Session 2 item 6 is closed - log 55 flew the recovery path four
   times. Item 4, the AltHold demotion rung, remains SITL-only.
