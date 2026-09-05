# SRCF field test log - session 7

Branch `SmallFastDrone-4.7.0-gps-optflow-fallback`. No new flight:
2026-09-05 desk analysis of field log 356 (SmallFastDronev1, the session
6 acro sortie) and eudrone log 55 (the session 3 octaquad GPS-loss
ladder), asking what those flights say about flying on optical flow
alone.

Two questions, both from the operator:

1. Could log 356 have been flown with no GPS at all and still had a
   usable RTL, assuming the origin had been right?
2. Can flow plus rangefinder harden the position estimate at speed and
   low altitude?

Short answers: **no on the first, for that flight profile**, and **the
second is a witness rather than an improvement, inside an envelope this
sortie spent most of its time outside**. The measurements are below, and
one of them started as a tool artifact that would have wrecked a
calibration if acted on.

One claim in this file was wrong when first written and is corrected in
place below: the tilt limit was attributed to the vehicle-side
rangefinder clamp in `AP_SurfaceDistance`, when it is EKF3's own
`DCM33FlowMin` and it stops flow being fused at all rather than merely
degrading its height.

## The raw data, cross-checked first

Neither answer is worth anything if the inputs are not what they claim,
and one of them was not.

**GPS is genuine truth here.** Over the armed flight of log 356:
`Status` 3-4, 14-19 satellites (p50 18), HDop 0.69-0.94 (p50 0.71),
`HAcc` 0.34-0.66 m (p50 0.52), `SAcc` 0.08-0.44 m/s (p50 0.13), at
speeds to 29.7 m/s. Nothing about it needs qualifying, which is what
makes the flight worth analysing.

**Flow and the rangefinder are healthy too, but establishing that meant
fixing the tool.** `flow_cal_check.py` first reported `flow/ideal` 0.37
on X and 0.59 on Y, both marked `[reliable]`, and recommended
`FLOW_FXSCALER=1475` against a current -88. Acting on that would have
tripled the gain on a sensor that is close to correct.

The cause is `interp()`: it had no maximum-gap guard, so the height
series was interpolated straight across the gaps where the rangefinder
returned nothing. Log 356 flew at 14-53 m against a 15 m sensor, valid
about 10-12% of the time, so most flow samples were handed a ~14 m
height while the vehicle was at 30-53 m. That inflates the expected flow
about threefold and deflates the ratio by the same factor. Truncation
biases a fit without scattering it, which is why the correlation stayed
high and the tool called it reliable.

With a 0.5 s gap guard on the height series:

| | as shipped | gap-guarded |
|---|---|---|
| `flow/ideal` X / Y | 0.37 / 0.59 | **0.85 / 0.88** |
| corr X / Y | 0.83 / 0.82 | 0.90 / **0.99** |
| samples used | 16998 | 3310 |
| mean height | 13.43 m | 9.23 m |
| suggested scalers | 1475 / 452 | 74 / -30 |
| auto GPS lag | 0.50 s | 0.10 s |

The bad heights were corrupting the lag scan as well: with them gone the
scan picks 0.10 s, which is where an independent measurement put the
true lag in session 3.

**It is not only a problem for flights that leave the rangefinder's
range.** Log 55 is the clean case - a valid return for every armed
sample the lane comparison paired - and the guard still drops 12 flow
samples out of 8832 there. Those twelve move the fitted forward scale
from 0.80 to **1.04** and the correlation from 0.75 to 0.90, at a
forced lag on both sides:

| log 55, forward axis | as shipped | gap-guarded |
|---|---|---|
| `flow/ideal` | 0.80 | **1.04** |
| corr | 0.75 | 0.90 |
| verdict | UNRELIABLE | reliable, `FLOW_FYSCALER=-147` |
| samples | 7440 | 7428 |

The fit is through the origin, so a sample handed a height far smaller
than the truth gets a hugely inflated expected flow and dominates the
sum of squares. Twelve of them in eight thousand are enough to move the
answer by 30% and to flip the reliability verdict. On this evidence the
octaquad's flow scale is close to correct rather than 20% low, which is
a materially different conclusion about that airframe from the same
flight.

So the honest raw-sensor picture is: sensor output rate correct (node
gyro against FC IMU, slope +0.93 X and +0.99 Y), orientation correct on
the same evidence, flow quality mean 145 with p5 124, and flow reading
**12-15% low**. That last number is what the lane-level measurement
below independently agrees with, and the agreement is the point - the
two methods only converge after the fix.

The fit still shows 17-20% cross-axis, above the tool's own 15% trust
threshold, so **do not apply the suggested scalers from this flight**. A
calibration sortie flown as `SRCF_SETUP_NOTES.md` section 5 describes -
heading held, forward then strafe runs, 5-9 m - is what banks the
12-15%.

## Q1: flying it GPS-free, and what RTL would have done

The flow lane dead reckons on both flights whatever the primary is, so
its displacement can be compared against the GPS lane's directly.
Displacements rather than positions, so log 356's 6,623 km frame error
is irrelevant.

| | path flown | final error | peak error | peak/path |
|---|---|---|---|---|
| log 356, small quad, 14-53 m | 4565 m | 138.0 m | **357.2 m** at t=198 | 7.8% |
| log 55, octaquad, 5-15 m | 392 m | 3.2 m | **31.1 m** at t=215 | 7.9% |

Read the peak, not the final. Log 356's final 138.0 m flatters it
because the vehicle came back near where it started and the error partly
cancelled; RTL is triggered whenever it is triggered, and the number
that matters is how wrong the estimate could be at that moment.

**The binding constraint is the rangefinder ceiling, not speed.** Log
356 had a height reference for about a tenth of the flight and took 19
position steps over 5 m in a single sample - the estimate is not merely
drifting there, it is discontinuous. Log 55 stayed inside its envelope,
had a valid rangefinder for 100% of its armed samples, and took 2 such
steps.

The two flights agreeing on 7.8% and 7.9% of path flown is a striking
coincidence given how little else they share, and it is consistent with
the mechanism: a 12-15% velocity scale error integrates into position
and partly cancels on a closed circuit. **Two logs is not a calibration
and this should be read as a rule of thumb, not a constant** - what
would falsify it is a third flight, ideally a there-and-back leg rather
than a circuit, where the cancellation does not apply and the error
should land nearer the full 12-15%.

So GPS-free RTL is viable within the rangefinder's envelope at roughly
8% of the path flown. A 400 m circuit gets home to about 30 m. That
4.5 km sortie would have had RTL 350 m out at its worst.

## Q2: hardening the estimate at speed and low altitude

Flow-lane velocity error against the GPS lane, restricted to samples
where the rangefinder was actually returning:

| speed band | log 55 octaquad | log 356 small quad |
|---|---|---|
| 0-5 m/s | 0.22 / 0.60 (n=5589) | - |
| 5-10 m/s | 0.38 / 0.78 (n=354) | 1.09 / 1.89 (n=99) |
| 10-15 m/s | - | 1.83 / 2.71 (n=91) |
| 15-20 m/s | - | 1.41 / **6.41**, max 17.2 (n=39) |

p50 / p95 in m/s. For scale, the same flight's GPS reported `SAcc`
0.08-0.44 m/s, and with no rangefinder gate at all log 356's flow lane
runs 3.99 p50 and 28.82 p95.

**As an accuracy improvement, no.** Flow is comparable to GPS at walking
pace and three to ten times worse by 15 m/s, so it cannot sharpen a
healthy GPS estimate. **As an independent witness, yes**, and its
resolution is its own error: it can see a fault that displaces velocity
by more than about 2-3 m/s at 10-15 m/s. That is exactly what SRCF uses
it for, and it is why `SRCF_VEL_THR` at 1.6 is marginal at speed - the
gate sits inside the witness's own noise once the vehicle is moving.

### Speed requires tilt, and tilt is what breaks flow

This is the part that was not obvious before measuring it, and it is a
structural limit rather than a tuning one.

| speed band | tilt p50 | tilt p95 |
|---|---|---|
| 5-10 m/s | 29.9 deg | 61.4 |
| 10-15 m/s | 35.9 | 68.7 |
| 15-20 m/s | **49.3** | 69.8 |
| 20-25 m/s | 46.6 | 60.6 |
| 25-35 m/s | 46.2 | 57.8 |

Against flow error by tilt on the same flight: 1.26 p50 and 2.63 p95
between 15 and 30 deg, against 1.76 p50 and **10.34 p95** between 30 and
45. A multirotor has to lean to overcome drag, so going faster
guarantees the attitude that degrades the measurement. Two mechanisms
arrive together - the flow sensor goes off-nadir, and the rangefinder
returns slant range instead of height.

There is a hard edge, and **this section first named the wrong one.** It
blamed `AP_SurfaceDistance.cpp:41`, which corrects with
`MAX(0.707f, c.z)` and so under-corrects past 45 degrees. That clamp is
real but it is on the vehicle-side height - surface tracking, terrain
following, `rangefinder_alt_ok()` - and the EKF never reads it. EKF3
takes the rangefinder directly in `readRangeFinder()` and tilt-corrects
with an unclamped `prevTnb.c.z` throughout `EstimateTerrainOffset()`.

The gate that actually binds is EKF3's own, and it is harder than a
clamp: `DCM33FlowMin = 0.71f` (`AP_NavEKF3.h:577`, a const rather than a
parameter). Above about 45 degrees of tilt, `tiltOK` goes false and
**flow is not fused at all** - `AP_NavEKF3_OptFlowFusion.cpp:44` for the
terrain estimate and `:1036` for the AGL Kalman filter, which stops
updating with it. The two numbers being 0.707 and 0.71 is why the wrong
one fitted the symptom.

Measured over the acro flight, the fraction of samples inside the gate:

| speed band | within `DCM33FlowMin` |
|---|---|
| 5-10 m/s | 79% |
| 10-15 m/s | 67% |
| 15-20 m/s | **35%** |
| 20-25 m/s | 42% |
| 25-35 m/s | 38% |

55% over the whole flight. So in the 15-20 m/s band the flow lane was
running on IMU dead reckoning about two thirds of the time with nothing
correcting it, which is where the p95 error jumps to 6.41 and the max to
17.2 m/s. Not a degraded measurement - an absent one.

The whole-flight correlation of speed against tilt is only r = +0.33,
because acro puts plenty of tilt on at low speed too. The median trend
is the real signal and it is monotonic to 20 m/s.

### Relaxing the tilt gate, replayed

`DCM33FlowMin` is a const rather than a parameter, so the question "would
a higher limit help" is a rebuild per value and a Replay. Log 356 is an
unusually good subject for it: the whole window is ACRO, so the
trajectory is pilot-commanded and the estimate never feeds back into it.
Replay's usual limit - that it re-runs the estimator and not the vehicle
- barely applies, because here the vehicle genuinely would have flown the
same path.

Two checks before the table. At the stock value the replayed flow lane
reproduces the flight's own to under 1% on the aggregates - verr p50 3.96
against 3.99, peak drift 346 m against 357 - though per-sample
reproduction is 1.58 m/s at p95, which is what a dead-reckoning lane
does. And Replay is bit-deterministic here: two runs of one binary gave
identical digits, so anything below is the parameter and not the harness.

| `DCM33FlowMin` | tilt | verr p50 / mean | peak drift | % of path |
|---|---|---|---|---|
| **0.71 stock** | 45 deg | 3.96 / 9.41 | 346 m | **7.6%** |
| 0.68 | 47 | 4.06 / 10.44 | 863 | 18.9% |
| 0.65 | 49 | 4.05 / 10.48 | 1339 | 29.3% |
| 0.62 | 52 | 3.63 / 10.20 | 1710 | **37.5%** |
| 0.60 | 53 | 3.03 / 5.79 | 773 | 16.9% |
| 0.55 | 57 | 3.03 / 5.76 | 736 | 16.1% |
| 0.50 | 60 | 2.79 / 3.17 | 218 | 4.8% |
| 0.35 | 70 | 2.79 / 3.15 | 220 | 4.8% |

**The mapping is chaotic and the answer is no, not on this evidence.**
Relaxing the gate makes dead-reckoned position five times worse through
0.68 to 0.62, recovers at 0.60, and only beats stock at 0.50. A change of
0.03 in the constant moves peak drift by 900 m. That is not a trend, and
0.50 looking 40% better than stock is one draw from a spread of 4.8% to
37.5% rather than a demonstration that 60 degrees is right.

The mechanism is in the flight: the flow lane stopped and restarted
aiding eleven times. Each restart re-anchors position, so where the
trajectory ends up depends on exactly which samples were fused before
each reset. Position drift is close to chaotic by construction here and
is the wrong metric for ranking estimator variants on this log. Velocity
error is the more robust one, and even that is flat-to-worse from 0.71
through 0.62 before improving.

There is a physical reason to expect the gate near where it is:
predicted range goes as `heightAboveGnd / c.z`, so at 60 degrees it
doubles, and the sensor is looking at ground well ahead of or behind the
vehicle where the flat-ground assumption under it stops holding.

The confound that matters most: **the height ceiling and the tilt gate
are not independent on this airframe.** Much of the tilt-limited data had
no valid rangefinder either, so relaxing the tilt gate admitted flow that
still had no height to scale it by. Testing the tilt gate properly needs
a flight that isolates it - sustained time at 45-60 degrees with the
rangefinder in range - which log 356 is not.

`tilt_gate_sweep.py` is the harness. It edits the header and rebuilds per
value, and restores on the way out.

### The envelope

What this data supports: **below about 15 m/s, below about 30 degrees of
tilt, inside the rangefinder's range.** In that box the flow lane tracks
GPS velocity to 1-2 m/s and dead reckons at roughly 8% of path flown.
Outside it, flow degrades faster than speed increases.

Session 3's log 57 is the independent check on the fast-and-low corner,
and it agrees: 30 km/h at 7.6 m gave `|VD|` max 2.17 in the 8-12 m/s
band, against 2.71 p95 measured here at 10-15 m/s.

## What this changes for SRCF

Nothing in the code, but it puts numbers under two things the record
previously only asserted.

The witness gate committed in session 6 - `flow_usable` and a live
rangefinder - is doing the physically right thing, and this says why:
outside the rangefinder's range the witness's own error is 4 to 34 m/s,
which is not evidence about anything. It also says why the gate is not
sufficient. Inside the range but past 30 deg of tilt the error still
reaches 10 m/s at p95, and no flag distinguishes that case, because
every health flag is green while the geometry is wrong.

Session 6's open item on the acro envelope stands, and this sharpens
what the flight should look for: the question is not whether the monitor
false-trips in acro, which it plainly does, but whether it false-trips
inside the envelope above. A sortie held under 15 m/s and 30 deg with
the rangefinder in range is the one that would tell us whether SRCF is
usable in fast flight at all.

## Tooling

`flow_cal_check.py` gains a `RANGE_MAX_GAP` of 0.5 s on the height
series, and reports how many flow samples it dropped for want of a
height. That count is not a warning about the fit - it is a statement
about the flight, and on log 356 it reads 17470 dropped against 3310
used, which tells the operator immediately that they flew above their
rangefinder.

Applied to the installed copy and to both source copies in the `aap`
repository, since the installed one is overwritten on the next install.
Every previous flow calibration taken with this tool is suspect to the
degree that flight left the rangefinder's range, and log 55 shows the
suspicion is not confined to flights that obviously did.

`flow_nav_viability.py` is the displacement and velocity-error analysis
in the tables above. `tilt_gate_sweep.py` is the Replay sweep of
`DCM33FlowMin`, which patches the header, rebuilds per value and restores
on the way out - including after a failure, with a check that it did.

## Still open

1. **The 8% rule of thumb has two points behind it and both are
   circuits.** A there-and-back leg would not benefit from the
   cancellation and should land nearer the full 12-15% scale error.
   Until that is flown, treat 8% as the optimistic end.
2. **The fast-and-low corner is thin.** Log 356 contributes 39 samples
   between 15 and 20 m/s with a valid rangefinder, and 24 samples in the
   5-10 m height band - about 2.4 seconds. Log 55 has volume but flew at
   0.4 m/s median. The corner that matters for a fast low mission is
   carried by session 3's log 57 and by inference.
3. **The cross-axis is unexplained.** 17-20% after the height fix, on a
   flight whose sensor-rate and orientation checks both pass. The tool
   attributes that to a flow-frame rotation or a yaw error; on an acro
   sortie with 169 deg of roll excursion, an attitude-dependent term is
   at least as likely. It needs a calibration flight to separate.
4. **No flow scale should be taken from this flight.** The 12-15% is
   believable as a magnitude and not as a per-axis scaler, for the
   reason above.
5. **The tilt gate is untested on a flight that isolates it.** The
   Replay sweep above cannot separate the tilt limit from the height
   ceiling, because most of log 356's over-tilt samples had no valid
   rangefinder either. What would answer it is a sortie that spends
   sustained time between 45 and 60 degrees of bank with the rangefinder
   in range, replayed the same way. Until then the gate stays at 0.71
   and the chaotic sweep is the reason, not an argument from the
   constant's provenance.
6. **Earlier flow calibrations want re-reading.** The octaquad's
   settled `FLOW_FYSCALER` of -110 came from session 3 flights measured
   with the unguarded tool; log 55 re-read with the guard suggests -147
   and a scale of 1.04 rather than 0.80. That is not a recommendation to
   change it - session 3 fitted its own calibration flights, not log 55,
   and those should be re-run through the fixed tool before anything
   moves. It is a reason to re-run them.
