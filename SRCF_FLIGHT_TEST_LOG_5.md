# SRCF field test log - session 5

Branch `SmallFastDrone-4.7.0-gps-optflow-fallback`. Vehicle
SmallFastDronev1. Logs 346-353, 2026-08-20/21, indoors under a GPS
repeater. Sections 5b onward were written as the session went, each
against the build named in it.

Read this before the rest: **five claims in this file were wrong and are
corrected further down, and the corrections are the useful part.** In
order - a repeater dragging the vehicle in log 349 (it was pilot input),
the first genuine spoof detection (both trips were manoeuvre-timed but
turned out to be real, then the reading changed twice), satellite count
separating a repeater from open sky (it tracks acquisition age), enabling
every `EK3_GPS_CHECK` bit helping (a replay artifact from a starved I/O
path), and innovation leading the handover by 74 s (a stale logged field).
Each was caught by measuring rather than by argument, and the checks that
caught them are worth more than the claims were.

Where it ended, after eight flights: the offset bound refuses displaced
and wandering fixes and accepts close ones, a corrected origin removed
the false refusals, and nothing the receiver reports about itself
discriminates a bad fix - log 353 refused one reporting 27 satellites at
0.88 m whose position disagreed with measured motion threefold. The one
crash is prevented by not handing over, never by detecting faster.

Session 5 proper, the crash: the first flight of the GPS-free arming path anywhere but
SITL, and it ended in a wall. The vehicle armed indoors on a recorded
origin with GPS switched off, hovered 130 s on the flow lane, and GPS
was then re-enabled - indoors, under a GPS repeater. The fix that came
back sat 26 m from truth and read healthy on every quality field the
receiver reports. The first-fix handover has no consistency gate, by
design, so it took that fix; Loiter chased the repeater's wandering
position and hit a wall 2.1 s later. The spoof detectors did fire, at
7.0 s, which is the earliest they structurally could and 4.9 s after
the impact. A bound on the handover is added below, conditioned on an
origin that predates takeoff.

## The flight

| t (s) | |
|---|---|
| 1.36 | `RC11: GPSDisable HIGH` - GPS off from boot |
| 4.99 | `SRCF: no GPS, arming on flow lane`, lane switch 1 |
| 4.99 | `AHRS: using recorded origin:35.18744,-79.37126,147`, home set from it |
| 36.9 | armed, LOITER |
| 137.79 | `RC11: GPSDisable LOW` - pilot restores GPS |
| 156.50 | `EKF3 IMU0 is using GPS`; the `SRCF_RECOV_TIME` hold starts |
| 166.687 | `SRCF: GPS acquired, using GPS lane`, lane switch 0, flow lane aligned |
| 166.79 | `GPS Glitch or Compass error` (ERR GPS/2), 0.1 s later |
| ~168.79 | impact - roll steps to -32.9 then 170 deg, pitch to -82 |
| 169.39 | pilot `MotorEStop` |
| 173.687 | `SRCF: GPS spoof suspected, using flow lane` |

The vehicle hovered at 0.91 m AGL (`RFND` 0.71-1.09, mean 0.91) with
flow quality 124-144 and held position to about 2 cm: the flow lane's
`PN`/`PE` moved 0.02 m over the last 2.6 s before the handover.

## The fix that looked healthy

Through the 28 s between the receiver returning a 3D fix and the
handover, with the vehicle demonstrably parked:

| | |
|---|---|
| satellites | 8-9 |
| HDop | 0.89-1.34 |
| reported HAcc | 1.24-3.16 m |
| reported speed | up to 4.09 m/s |
| reported altitude spread | 17.7 m (124.8 to 142.5) |
| distance from the flow lane | 25.3-26.7 m |

The OSD lane panel cannot help here: the GPS lane really was fusing
GPS, so it read `EKF0 ABS` throughout.

**Corrected after log 348.** This section originally called those
numbers immaculate and concluded the quality channels carried nothing.
That is wrong, and it cost a flight. Judged against what this same
receiver does under open sky - 20-26 satellites, HDop 0.53-0.62, `HAcc`
0.20-0.49 m across logs 332-337 - eight satellites and `HAcc` 2.7 m is
a third of the constellation at five to thirteen times the error. The
"a spoof reads healthy by construction" rule is about a synthesised
constellation; a repeater re-broadcasts a real sky view down an
attenuating path, so it gives fewer satellites and worse accuracy, and
that is measurable. See open item 2.

## Why nothing stopped the handover

Three reasons, each sufficient on its own.

**The detectors cannot vote while flow is primary.** `can_vote`
requires `primary == SRCF_GPS_LANE` (`source_fallback.cpp:289`). For
the whole 130 s flight nothing was watching, which is correct for the
quantities it watches - they are cross-lane comparisons and the flow
lane is one of the terms - but it means the first-fix handover is
entered with no accumulated evidence at all.

**The handover itself had no gate.** The `FLOW_NO_GPS` exit waited only
for the GPS lane to hold `horiz_pos_abs` for `SRCF_RECOV_TIME`. Over
the 101 samples of that hold:

| | measured | gate |
|---|---|---|
| `PD` | 25.30-26.71 m | none |
| `PD`/`PSig` | 10.6-14.7 | none; `FLOW_LOSS` bounds this at 6 |
| `VD` | 0.017-0.579 | 1.6 |
| `PR` | -0.196 to +0.187 | 1.9 |

`PD` was not merely large, it was flat - 1.4 m of variation over ten
seconds - which is the signature of a fixed frame offset. The rate
detectors were never going to see it: a repeater has no walk rate, and
a hovering vehicle gives it no velocity signature either. This is the
static-capture shape session 2 identified and bounded, on the one
transition that does not use the bound.

**The alignment then erased the evidence.** `align_lane_position` fires
on the tick after the switch lands, and the flow lane's states jump
from (-2.756, -0.630) to (12.221, -21.779) - a 25.9 m shift onto the
GPS lane's frame. `PD` restarts from zero. With the 5 s
`SRCF_POST_SWITCH_MUTE_MS` and 2.0 s `SRCF_CNF_TIME` on top,
166.687 + 5.0 + 2.0 = 173.687 to the tenth of a second. The vehicle
hit the wall at 2.1 s.

## The runaway

The repeater's reported position walked WNW. Between 166.79 and 168.64
the GPS lane's east position went -21.78 to -23.86 (1.13 m/s west)
while the flow lane - the one still measuring real motion - shows the
vehicle going 2.36 m east in the same window. The estimate moved
opposite to the truth at roughly the same speed the truth was moving.

Loiter did what it is supposed to do with that: sustained demand to
`DesRoll` -7.1 deg and `DesPitch` -10.3 deg, tracked by the attitude
controller to within 0.1 deg. The correction never registered, because
the position it was correcting does not respond to the vehicle moving,
so it kept pushing. 2.5 m of travel in 1.9 s, into the wall.

The pilot's escape was the mode switch or the e-stop, and only the
e-stop was reached. Two seconds is not enough time to diagnose "my
position source is lying" from inside the aircraft.

## The gate

`ArduCopter`: the `FLOW_NO_GPS` handover now applies the same
`SRCF_RECOV_POS_NSIGMA` bound the `FLOW_LOSS` recovery uses, but only
when an origin existed before takeoff. That condition is the whole
design:

- Armed with no origin at all, the flow lane's position is referenced
  to wherever relative aiding began and the GPS lane to the origin it
  sets on acquisition. The difference is the distance flown since
  arming - 46 m in `SRCFArmWithoutGPS` - and a bound would reject an
  honest handover outright. This is the case the original comment was
  written about, and it is unchanged.
- Armed on a recorded origin or a GCS `SET_GPS_GLOBAL_ORIGIN`, both
  lanes are in a real earth frame from the start and `PD` is a real
  disagreement. Log 346 sat at 10.6-14.7 sigma against a bound of 6.

The flow lane is aligned to the GPS lane only on an accepted handover,
so the bound is measured on the evidence rather than after it has been
zeroed. A refused fix warns once, on the same "this would have switched
by now" timer the recovery path uses:

```
SRCF: GPS acquired 26m off, staying on flow
```

The vehicle stays on the flow lane, which is what it had been flying
successfully for 130 s.

The obvious alternative - gate on velocity instead, which needs no
common frame - does not work here and the log says so: `VD` never
exceeded 0.58 against a 1.6 threshold for the entire hold. A repeater
that has settled disagrees only in position.

`autotest`: `SRCFFirstFixOffsetBound` flies the log 346 sequence -
recorded origin, arm without GPS, acquire on a `SIM_GPS1_SPOOF` mode 3
capture 100 m out - and asserts the handover is refused, that the pilot
is told, and that the vehicle does not drift. It then clears the spoof
in the same flight and asserts the handover completes, because a bound
that simply disabled the handover would pass the first half.

It reproduces the field signature rather than only the outcome, which
is the point of using a static capture rather than an arbitrary offset:

| | log 346 | SITL |
|---|---|---|
| `PD` | 25.3-26.7 m | 99.7 m |
| `PD`/`PSig` | 10.6-14.7 | 15.6-16.2 |
| `VD` (gate 1.6) | 0.017-0.579 | 0.02-0.08 |
| `PR` (gate 1.9) | -0.196 to +0.187 | -0.05 to +0.01 |
| held on flow | - | 1.6 m |

Its first version asserted 1.3 s after the GPS lane became usable and
proved nothing: the 30 s wait was timed from switching the receiver on,
and the EKF took 28.7 s to start using the fix it had been handed. The
wait is now timed from `EKF3 IMU0 is using GPS` and has to cover
`SRCF_RECOV_TIME` for the handover plus `SRCF_RECOV_TIME` again for the
warning that says why it did not happen.

Green against the change: `SRCFFirstFixOffsetBound`,
`SRCFArmWithoutGPS` (the gate must stay inert where no origin predates
takeoff), `SRCFStaticSpoofNoRecovery` (the other caller of the shared
warning) and `SRCFGPSLossLadder` (ordinary recovery still recovers -
the regression a bound set too tight would break).

## This airframe's configuration

Two values differ from the octaquad's and neither should be changed on
the strength of that alone:

- `EK3_GLITCH_RAD = 0`. Session 3 raised the octaquad 0 to 25 and
  described 0 as right for a 50 m/s vehicle "inherited from one". This
  airframe hovers at 9.5% throttle against the octaquad's 26.5%, so it
  is plausibly that vehicle and 0 is plausibly deliberate. It did make
  this crash worse either way: a glitch was declared 0.1 s after the
  handover and held for 7.0 s, and at 0 the lane crawls toward the bad
  fix rather than resetting. Settle which speed class this airframe is
  before touching it - not from the octaquad's number.
- `EK3_FLOW_GAIN_H = 12` against the octaquad's measured 4. Inert at
  0.9 m AGL: `gainHgt / max(HAGL, gainHgt)` is 1.0 below either value,
  so there is no detune at 12 or at 4 and this flight cannot say which
  is right. The octaquad's 2.4 s roll limit cycle at 12 was at 7-9 m.

`SRCF_VEL_THR = 1.6` and `SRCF_POSR_THR = 1.9` are correct here and
should stay: they are this airframe's own values from session 2's
five-flight replay, and session 4 flew log 338 at 1.9 deliberately
rather than the octaquad's 2.6. Neither was approached - `VD` peaked at
0.58 and `PR` at 0.20.

`EK3_OPTIONS = 26` rather than the 126 sessions 1 and 2 flew on this
airframe; bits 2, 5 and 6 have been cleared since, and only bit 5 is
recorded as deliberate (session 3, terrain following). `AHRS_OPTIONS =
24` is bits 3 and 4, so the origin came from a previous flight -
accurate here, and the thing that made `PD` meaningful.

## 5b: the gate's first flight - log 347

Same room, same evening, firmware `d8be1f94`. GPS off at boot, armed on
the flow lane at 23.6 s in Loiter, GPS restored on the switch at 53.9 s,
and the GPS lane reached absolute position at 131.3 s.

`SRCF: GPS acquired 35m off, staying on flow` at 141.4 s - 10.1 s after
the lane became usable, which is `SRCF_RECOV_TIME` to the tenth, so the
message arrived at the instant the handover would have. There was no
handover. The vehicle flew on for another 98 s and landed normally.

Over the whole 108 s the GPS lane was usable and refused:

| | measured | gate |
|---|---|---|
| `PD` | 19.8-46.1 m | 6 x `PSig`, i.e. 7.0-17.4 m |
| `PSig` | 1.16-2.90 m | |
| `PD`/`PSig` | 12.4-34.8 | 6 |
| `VD` | 0.003-1.272 | 1.6, never crossed |
| `PR` | -1.09 to +0.88 | 1.9, never crossed |
| `VVot`/`PVot`/`OVot` | 0 | 20 |

The refusal was right, and not because the origin was stale. The flow
lane held station to 5 cm - `PN` -0.16 to -0.20 m, `PE` +0.26 to
+0.32 m over 168 s at 0.64 m AGL, flow quality 113-134 - while the
receiver reported 195 m of path length with fixes up to 34 m apart,
speeds to 2.71 m/s, an altitude spanning 70.1 to 131.8 m against an
origin altitude of 139 m, and 7-8 satellites at HDop 1.09-1.37
throughout. A stationary vehicle cannot produce that, whatever the
origin is.

### A large offset hides the wander from PD

`PD` is the magnitude of the offset vector, and magnitude is blind to
motion across it. Between 130.1 and 142.1 s the reported position moved
6.6 m on a bearing of 194 deg while the standing offset pointed at
296 deg - 102 deg apart, near enough perpendicular - and `PD` changed by
0.9 m. `PR`, which differentiates `PD`, therefore saw almost nothing of
a 0.55 m/s walk.

So the offset test catches the standing disagreement and misses the
dynamics, while the vector displacement of the two lanes over the same
window differed by the full 6.6 m against the flow lane's 5 cm. Worth
remembering when reading `PR` on any flight with a large `PD`.

### The flow-lost path is not gated

At 239.8 s, between `LAND_COMPLETE_MAYBE` at 239.4 and `LAND_COMPLETE`
at 240.2, the monitor printed `SRCF: flow lost, back on GPS lane` and
took the lane it had spent 98 s refusing. On the ground at touchdown
that is harmless and expected - flow quality collapses and the
rangefinder goes under its minimum - but the branch is the same one that
runs in flight, and it has no offset check at all.

The alternative is the AltHold demotion with no position, so which is
right is not obvious: a lane 35 m out still holds station, and nothing
does not. Left open rather than patched off one landing.

## 5c: an honest fix refused - log 348

The inside-to-outside flight, on `d8be1f94`. Armed indoors on the flow
lane at 22.5 s, flew out, and never took the GPS lane:

```
33.30  EKF3 IMU0 is using GPS
43.48  SRCF: GPS acquired 40m off, staying on flow
```

No flyaway - the vehicle flew the whole 305 s on flow and landed. But
the fix it refused was a good one. Outside, from 170 s on, the receiver
held `Status 4` with 14-18 satellites, HDop 0.76-0.89, `HAcc` down to
0.22 m, and a position that settled to a few metres for 85 s.

The origin was the problem. Back-solving the takeoff point from that
fix and the flow lane's own displacement - the flow lane reports
displacement from where it began aiding, so subtracting it from a
trusted fix gives where the vehicle actually started:

| | |
|---|---|
| mean GPS position, 180-265 s | 35.1872560, -79.3713662, 140.5 m |
| flow lane displacement since takeoff | N -20.4 m, E +3.9 m |
| implied takeoff point | 35.1874390, -79.3714087 |
| recorded origin | 35.1876869, -79.3717117, 89.3 m |
| **origin error** | **39.0 m horizontal, -51.2 m vertical** |

The firmware said 40 m. So the gate was correct about the disagreement
and wrong about which side of it was lying, which is the failure this
change bought and it has now happened.

`AHRS_OPTIONS` was still 24, so bit 3 was still rewriting the origin
from each flight. It has moved every time: 35.18744/-79.37126/147 in
log 346, 35.18748/-79.37127/139 in log 347, 35.18769/-79.37171/89 here.
An origin recorded on a flight flown under a repeater is a repeater's
idea of where the vehicle was.

Before the gate existed a bad origin only put home in the wrong place.
It now blocks the handover as well, so `RECORD_ORIGIN` and this feature
are actively hostile to each other and the origin should be pinned by
hand with bit 3 cleared.

### What would have caught it, and what would not

Two candidate discriminators were tested against the logs and both fail:

- Offset stability. A wrong datum ought to give a constant offset where
  a wrong fix wanders, but measured over each flight's usable window
  log 348 spread 38.7 m against log 347's 26.3 m - the bad datum varies
  *more*, because the vehicle flew 20 m and the fix improved from 5 to
  19 satellites during it.
- Displacement and altitude consistency, from open item 6, for the same
  reason it failed there: it cannot run for long enough before the
  handover decision is due.

What was plainly visible is the origin itself. From 8.1 to 18.5 s, on
the ground and four seconds before arming, the receiver reported a 3D
fix at 144-153 m altitude against a recorded origin of 89.3 m. Nothing
compares those two numbers. A disarmed check that did would have caught
this before the props turned, and it is design note open question 4.

Its limit is worth stating: it validates a datum against whatever fix
is on offer, so under a repeater it would compare a good origin against
a bad fix and cry wolf. It belongs as a warning the pilot can weigh,
not a refusal.

### What signal to use

Five candidates, measured over logs 346, 347 and 348 rather than argued.

**EKF innovations do not work**, which is worth recording because they
are the obvious thing to reach for. GPS-lane test ratios never came
near failing - `XKF4.SP` peaked at 0.24, 0.46 and 0.60 - and the
honest flight had the *largest* raw innovations of the three, `IPN`
and `IPE` to -9.8/+13.0 and -20.0/+8.9 m. Once a lane aligns to a
source its position state is driven by that source, so it has no
independent truth to innovate against. That is why SRCF compares two
lanes in the first place.

**`EK3_OPTIONS` bit 0 (JammingExpected) is structurally inert here.**
It re-requires the preflight GPS checks after a fix is lost, but only
where `canDeadReckon`, which needs `doingFlowNav` on the same core.
Under per-core source sets lane 0 has no flow, so the condition is
false on the GPS lane in every SRCF configuration.

**`EK3_GPS_CHECK` has the relevant checks switched off.** The default
31 is bits 0-4; the three a repeater violates - bit 5 pos drift, bit 6
vert speed, bit 7 horiz speed - are exactly the ones excluded, because
they also fail when the vehicle is moving at alignment. Log 347's
receiver walked 195 m and reported 2.7 m/s while parked, so they would
have caught it. Whether 255 also blocks the honest in-flight alignment
of an inside-to-outside flight cannot be answered from a log, since
the checks run inside the filter. It needs a SITL A/B before anyone
sets it.

**`gpsGoodToAlign` alone is not enough**, though it does carry signal.
`XKFS.GPS_GTA` is computed continuously, before and after alignment,
and was true 5.5%, 39.5% and 84.6% of the time. But log 347 holds it
unbroken for 45.1 s, so any hold short of 60 s lets a repeater through
and 60 s leaves only 33% margin.

**`gpsGoodToAlign` and a raw quality floor together separate
completely.** Longest unbroken run of each:

| log | `GPS_GTA` | >=12 sats and `HAcc` <=1.0 m | both |
|---|---|---|---|
| 346 repeater | 11 s | 0 | **0** |
| 347 repeater | 45 s | 0 | **0** |
| 348 in to out | 257 s | 132 s | **132 s** |

Neither repeater flight holds both for a single sample. The two are
independent - one is the filter's own verdict on GPS velocity
consistency, the other the receiver's constellation and accuracy - so
the conjunction is not double-counting one measurement.

**Comparing the lanes over time is directionally right and not
sufficient.** Restricted to the first-fix state with the lane usable,
displacement mismatch over a 60 s window runs at a median of 18.0 m on
log 347 against 3.0 m on log 348, but log 348 peaks at 26.1 m because
most of its time in that state was still indoors under the repeater.
The tails overlap. It belongs as confirmation once the fix is already
trusted, not as the gate.

## Replaying the GPS checks, and a result that was an artifact

Logs 346-348 carry DAL data, so `gpsGoodToAlign` can be recomputed under
different `EK3_GPS_CHECK` and `EK3_CHECK_SCALE` rather than argued about.
The baseline column reproduces each flight's logged value exactly - 11.0,
45.1 and 257.5 s - which is what makes the rest of the table believable.

Longest unbroken `gpsGoodToAlign` on the GPS lane, seconds:

| log | scale 100, checks 31 | 100, all | 50, 31 | 50, all | 20, 31 | 20, all |
|---|---|---|---|---|---|---|
| 346 repeater | 11 | 11 | 0 | 0 | 0 | 0 |
| 347 repeater | 45 | 45 | 33 | 33 | 0 | 0 |
| 348 honest | 257 | 249 | 201 | 201 | 0 | 0 |

**Enabling every GPS check does almost nothing**, and an earlier note here
saying it dropped log 346 from 11 s to 0 was wrong. That figure came from a
replay of a log sitting on the Windows mount, where the tool ran I/O
starved for hours and the sweep picked up a stale output file. Copied to
local disk the same replay takes 0.8 s and gives 11. The reason the extra
bits change nothing is that they are the on-ground drift and speed checks,
and every long pass in this table is in flight where they do not run.

`EK3_CHECK_SCALE` is the only lever that moves anything, and 20 is too far:
it takes the honest fix to zero as well. 50 is the setting that separates -
the repeaters manage 0 s and 33 s against 201 s - and it is a mild change,
leaving 2.5 m of horizontal accuracy and 0.5 m/s of speed accuracy against
a receiver measured at 0.49 m and 0.26 m/s on good flights.

So the pair is `EK3_CHECK_SCALE = 50` with a 60 s hold: 1.8x margin over
the repeater and 3.4x under the honest fix. `EK3_GPS_CHECK` can stay at 31.

The lesson worth keeping is about the tool rather than the parameter. A
replay that is starved of input still exits zero, and a sweep that reads
whatever log file is newest will report its neighbour's answer. The
baseline column exists to catch exactly that, and it did - a run that
cannot reproduce the flown value is not evidence about anything else.

## 5d: the handover works, and hands over to the repeater - log 349

Flown on `d8be1f94` again, so no code change, only the two parameters:
the origin pinned to the back-solved takeoff point with `AHRS_OPTIONS`
16, and `EK3_CHECK_SCALE` 50.

The origin fix did what it was supposed to. `PD` at the first fix was
**4.3 m against `PSig` 1.2 m** - 3.6 sigma, inside the bound of 6 - where
log 348 saw 40 m. The handover completed at 193.6 s, 10.0 s after the
lane became usable:

```
183.56  EKF3 IMU0 is using GPS        (114 s after GPS was re-enabled)
193.59  SRCF: GPS acquired, using GPS lane
```

That is the first accepted handover on an aircraft, after four refusals.

Then it dragged the vehicle. The flow lane held station for 30 s and
from 233 s moved **20 m in 20 s** - 19.4 m north, 5.4 m west - with
Loiter demanding up to -11.2 deg pitch and +16.6 deg roll, still indoors
at 0.99 m and still on seven satellites. `VD` climbed to 1.72 against
the 1.6 gate, held it, and the velocity detector latched at 254.7 s:

```
254.69  SRCF: GPS spoof suspected, using flow lane
255.49  RC9: MotorEStop HIGH
```

The ladder worked. That is the first field spoof detection that was not
a false trip, and it caught a repeater dragging the vehicle 61 s after
the handover. The pilot stopped the motors 0.8 s later, so whether the
flow lane would have settled it is still unknown.

### Nothing checked whether the fix was worth taking

The handover was authorised by the offset bound, which passed honestly:
the origin was right and the repeater's reported position happened to
agree with it at that moment. Fix quality is not consulted anywhere on
that path - `SRCF_FIXQ_TIME` is a bypass for when the bound refuses, not
a condition on accepting. So neither it nor the EKF checks were in a
position to stop this.

Nor could they have been at that setting. Replayed, log 349 holds
`gpsGoodToAlign` for 70 s at scale 100 and 64 s at 50, and only 0 s at
20 - which also takes log 348's honest fix to 0. And its reported
accuracy was good: `HAcc` 0.8-1.0 m, inside the 1.0 m bar the first
version of the quality path used. The accuracy half of that test would
have passed this fix.

### Satellite count is what separates, and it cannot be tuned

| environment | p5 | p50 | max |
|---|---|---|---|
| repeater indoors, logs 346, 347, 349 | 6-7 | 8 | 8-9 |
| log 348, indoor then outdoor | 8 | 15 | 19 |
| open sky, logs 332, 333, 336, 337 | 15-25 | 18-25 | 22-26 |

Across three repeater flights and four open-sky ones the counts never
overlap, and the gap between 9 and 15 is wide. It is not a coincidence
of one site: a repeater re-broadcasts through an attenuating path, so
only the strongest satellites survive it.

`EK3_CHECK_SCALE` cannot reach this. The satellite check is
`gps.num_sats() < 6`, hardcoded and deliberately not scaled, so the one
channel that separates cleanly is the one the EKF's own machinery has no
knob for. That is the answer to whether the EKF parameters can carry
this on their own: no, and this is why.

So the twelve-satellite bar was arbitrary in origin and is now the
best-supported number here, while the accuracy bar beside it is not
load-bearing and log 349 shows it passing a repeater. What the design
needs is fix quality as a **necessary** condition on the first-fix
handover rather than a sufficient bypass around the offset bound.

## 5e: two corrections, and what log 350 actually showed

Log 350 flew `0e6b09d0`, whose only live change was the satellite
requirement. It refused the handover exactly as designed - `SRCF: GPS on
9 sats, staying on flow` at 159.0 s, 10.1 s after the lane became usable
- and then took it at 218.0 s anyway, because the satellite count had
climbed past twelve.

### The count climbs, so it does not separate

The receiver went 8, 9, 10, 11, 12 over thirty seconds and reached 15-17
for the rest of the flight, with the vehicle stationary at about 1 m
throughout. Logs 346, 347 and 349 sat at six to nine because their fixes
were younger, not because an indoor fix is capped there. The reasoning
in session 5d - that a re-broadcast path lets only the strongest
satellites through - was wrong; attenuation delays acquisition, it does
not limit it. `SRCF_FIXQ_SATS` is now off by default.

Log 350's fix was also a much better one: mean position 6.6 m from the
true takeoff point against 14.0, 16.5 and 24.7 m on the three before it,
and altitude within 3 m of truth.

### The vehicle was never dragged, in either flight

Session 5d says a repeater dragged the vehicle 20 m in 20 s in log 349.
That is wrong and it reached a commit message. `RCIN.C2` was 1534-1653
through the whole excursion - up to 150 us of forward pitch - and
`PSCN.TPN` walked from -21.3 to -2.3 with the measured position tracking
it to within 0.3 m. The pilot flew it there. Log 350 is the same:
stick 1513-1612, target walking, position tracking to 0.3 m.

Log 346, which is the flight that crashed, is genuinely not this.
`RCIN.C1` and `RCIN.C2` are 1501 - centre - for the whole event, the
target is frozen at -21.86, and the measured position walks away from it
to -24.17. Hands off, estimate walking, controller chasing.

That is the distinction that matters and session 5d blurred it: a
*wandering* fix flies the vehicle into a wall, a *displaced but steady*
one does not, and logs 349 and 350 were the second kind.

### Both spoof trips were the fix, not the manoeuvre

**This section originally said the opposite.** It is corrected below; the
paragraph that follows records what was wrong with it.



Session 5d also calls log 349's trip the first field detection that was
not a false trip. It has the false-trip signature instead. `VD` sat at
0.15-0.51 through the steady translation and only moved when the pilot
reversed the sticks:

| log | `VD` while translating | at the reversal | trip |
|---|---|---|---|
| 349 | 0.15-0.51 | 0.93, 1.36, 1.72, 1.92 | 254.7 s, roll 1687-1709, pitch 1360 |
| 350 | 0.08-0.21 | 0.45, 0.98, 1.46, 1.72 | 250.6 s, roll 1543-1585, pitch 1268-1333 |

The reversal is when the disagreement appears, but it is not what
causes it. Through log 350's trip the raw receiver reported 0.20-0.29
m/s and the flow lane 0.16-0.36 - they agree - while the GPS *lane*
reported 1.07-1.52. The lane disagrees with its own measurement.

`XKF3` says why: the GPS lane's position innovation grows monotonically
to 3.07 m over those five seconds and `XKF4.SV` reaches 1.01, the
rejection threshold. The receiver's reported position was walking at
about 0.6 m/s while its reported velocity said 0.2-0.3, so the fix
contradicts itself, and the filter chasing that walk puts it into the
velocity state.

That is log 346's mechanism at a smaller scale - a walking fix - and the
detector caught it. Both trips were correct. Reading them as false trips
came from matching the reversal in time without asking which lane was
wrong, which is the same mistake as reading the excursion as a drag
without checking the sticks.

So the position after four indoor flights is that the handover happened
twice and flew normally both times, the detector false-tripped twice,
and the only genuine failure remains log 346's wandering fix.

### The repeater was on for log 350

Which is the useful part. It delivered 15-17 satellites, `HAcc` 1.23 and
a mean position 6.6 m from the true takeoff point - almost certainly the
roof antenna's own offset, converged on accurately once the receiver had
a good lock, where the 14-25 m of logs 346, 347 and 349 is scatter
around that same point from a poorer one. A repeater is therefore not
identifiable by satellite count, by reported accuracy, or by how far off
it is. What separates the one dangerous flight from the rest is that its
fix *wandered*.

Set against that, the ladder now reads as working. Log 346's 26 m at
10.6-14.7 sigma is what the offset bound refuses, and it did not exist
on that build; logs 347 and 348 were refused and flew on; logs 349 and
350 were accepted on small offsets and flew normally. The only false
refusal was the corrupt origin, and that is fixed.

### The detectors have no margin indoors

Whole armed flight, both indoor flights, against gates of 1.6 and 1.9:

| log | `VD` p95/p99/max | `PR` p95/p99/max |
|---|---|---|
| 349 | 1.01 / 1.89 / 1.99 | 0.96 / 1.61 / 1.82 |
| 350 | 1.43 / 1.69 / 1.74 | 1.04 / 1.45 / 1.59 |

Ordinary manoeuvring at about 1 m AGL crosses the velocity gate and
comes within 4% of the position-rate one. Two flights, two trips. The
thresholds were measured at 5-9 m outdoors, and session 3 already said
flow near the ground is its own problem; the indoor case forces the
regime the setup notes tell people to avoid.

The 30% rule would put `SRCF_VEL_THR` at about 2.6 and `SRCF_POSR_THR`
at 2.4 for this regime - and both should be left alone, because the
excursions are the detector working. See the correction above.

## 5f: what the replay said about an adaptive threshold

The plan was to replace the fixed `SRCF_VEL_THR` with one that tracks
conditions, on the reasoning that no constant can cover a 1 m hover, a
stick reversal and 30 km/h. Replayed over nine flights, the premise does
not hold.

**Speed does not predict it, and the sign is backwards.** Worst benign
`VD` per speed and roll/pitch-rate cell, armed and upright only:

| speed \ rot rad/s | 0-0.25 | 0.25-0.5 | 0.5-1.0 | 1.0+ |
|---|---|---|---|---|
| 0-1 m/s | 3.66 | 3.64 | 2.63 | 1.53 |
| 1-2 m/s | 2.65 | 2.79 | 2.81 | 1.04 |
| 2-4 m/s | 2.75 | 2.36 | 2.80 | 0.68 |
| 4-8 m/s | 1.77 | 1.87 | 1.86 | 1.65 |

Divergence is worst when slow, not fast, and rotation rate does not
order it at all. Session 3's "it scales with velocity" held on one
octaquad flight and does not survive the pool.

**Height orders it, but as a U.** The worst excursions sit at 0.2-1.0 m
(logs 348, 349) and at 16-21 m (logs 333, 337), with the quiet band at
5-9 m - which is where the setup notes already tell people to work.

**And the low-height end is not noise to be accommodated.** It is the
GPS lane chasing a walking fix, per the correction in 5e. A threshold
tuned to sit above it would be tuned to ignore the one signature that
matters.

Two other candidates were tried and neither separates: `EK3_GLITCH_RAD`
makes no difference at all to the velocity excursion (replayed at 0 and
25, identical to two decimal places), and testing the receiver against
itself - reported position rate against reported speed - gives 0.56-0.63
m/s of median disagreement on logs 346 and 347 but 0.06-0.12 on 349 and
350, indistinguishable from open sky's 0.09-0.19.

So no threshold change, adaptive or otherwise. The fixed thresholds are
catching real faults indoors, and the work that would have gone into
scaling them is better spent on how fast the detection is: log 346 had
2.1 s hands-off before the wall, and log 350 took about 5 s from the
innovation starting to grow.

## 5g: the fast tell is the GPS lane's own innovation

GPS is slow to settle and optical flow plus a rangefinder is effectively
instant, so the flow lane knows the truth first. That asymmetry is worth
using: a manoeuvring vehicle grows the GPS lane's position innovation
through lag alone, but innovation growing *while the flow lane says the
vehicle is holding station* is a fix walking away from reality.

Measured over nine flights, innovation on the GPS lane restricted to
samples where the flow lane reports under 0.5 m/s:

| log | environment | p50 | p95 | max |
|---|---|---|---|---|
| 332 | open sky | 0.02 | 0.06 | 0.11 |
| 333 | open sky | 0.04 | 0.13 | 0.21 |
| 336 | open sky | 0.04 | 0.07 | 0.11 |
| 337 | open sky | 0.04 | 0.14 | 22.59 |
| 346 | repeater, crashed | 0.62 | 2.03 | 3.60 |
| 347 | repeater | 0.62 | 6.75 | 8.40 |
| 348 | indoor then out | 0.25 | 5.72 | 20.97 |
| 349 | repeater | 0.28 | 2.47 | 6.18 |
| 350 | repeater, tripped | 0.23 | 3.65 | 5.76 |

Eighteen to a hundred times apart on the p95. Log 337's 22.59 is three
bursts totalling nine samples, the longest half a second, so a one second
sustained requirement excludes all of it.

It is also far earlier than anything the monitor watches now. Innovation
above 2 m sustained for a second:

| | first seen | GPS lane usable | handover | outcome |
|---|---|---|---|---|
| 346 | 92.7 s | 156.7 | 166.7 | impact at 168.8 |
| 350 | 139.4 s | 148.9 | 218.0 | trip at 250.6 |

Seventy-four and seventy-nine seconds before their handovers. In log 346
the fix was provably walking from 92.7 s, a minute before anything
noticed and seventy-six seconds before the wall.

So it is not only a faster detector, it would have refused both
handovers. And unlike satellite count it is not a proxy for the
environment - it is the lane reporting that the fix it is being handed
disagrees with its own prediction, cross-checked against a lane with no
lag. It needs no origin and no common frame.

Open sky p95 is 0.06-0.14 m, so a bar of 1 m held for a second sits
seven times above the benign envelope while both repeater flights go
straight through it.

What it does not do is work while the vehicle is moving: the gate is the
flow lane reporting nearly stationary, and during sustained flight there
is nothing to separate lag from a walk. That suits the first-fix
handover, which happens from a hover, and it suits log 346's hands-off
case, which is the one that crashed.

Not built. Needs a per-lane innovation accessor, the same shape as the
divergence and sigma ones, and the benign side rests on four open-sky
flights.

## 5h: the innovation detector was built on a stale number - logs 351, 352

Log 351 flew 390 s on flow with GPS never enabled and ended with
`SRCF: no nav source, AltHold`, so the flow lane gave out rather than the
monitor. Log 352 flew the detector from 5g and handed over anyway.

The measurement 5g was built on is wrong. `innovVelPos[3]` and `[4]` are
written only inside `if (fusePosData)` in `FuseVelPosNED`, so a lane that
is not fusing position holds whatever it last had - and the GPS lane is
not fusing before it takes GPS up, which is exactly the window the "74 s
before the handover" was measured over. It was reading leftovers.

Restricted to samples where the lane really is fusing, the separation
does survive:

| | open sky (332, 333, 336, 337) | repeater (346, 347, 350, 352) |
|---|---|---|
| p50 | 0.02-0.04 | 0.09-1.58 |
| p95 | 0.06-0.14 | 0.55-7.03 |

but the lead time does not. Against the velocity detector already in the
monitor:

| log | innovation over 1 m sustained 2 s | `VD` trip | |
|---|---|---|---|
| 352 | 166.4 s | 161.3 s | 5.1 s later |
| 350 | 254.9 s | 250.6 s | 4.3 s later |
| 346 | 166.8 s | 168.8 s | 2.0 s earlier |

Slower than what exists in two cases of three. And it cannot gate the
handover at all: before acquisition the innovation is stale, and at
acquisition the lane resets position onto the fix, so it starts from
zero however wrong that fix is. Log 352 shows it directly - 2.5-3.6 m
while stale, collapsing to 0.08 m at 106.5 s and staying under 0.25 m
through the handover at 112.7 s. Reverted.

The lesson is the one this file keeps relearning in different clothes. A
logged field is not a measurement until you know when it is written.
`XKF3` looks like a continuous trace and is a sample-and-hold, and
nothing in the log says which. The check that would have caught it is
the same one that caught the replay artifact: reproduce something known
first. Had the detector been replayed against the flights it was fitted
to, the reset at acquisition would have shown up before any code was
written.

## 5i: how tight the switchover timing actually is

The switch itself is not the problem. In log 346 the spoof statustext is
at 173.6865 and `EKF3 lane switch 1` at 173.6909 - **4.4 ms**. Everything
else is detection.

**The real detections tripped with one vote of margin.** Replaying the
vote integrator over the logs reproduces the firmware's own counters
exactly - logged `VVot` peaks of 19, 19 and 10 on logs 349, 350 and 352
against replayed 19, 19 and 10 - and 19 of 20 means those trips happened
by a single sample, 0.1 s. A slightly shorter excursion misses entirely.
Getting that agreement needs the session 2 artifact modelled: the
firmware votes, then switches, then logs, so the tick completing a trip
carries the new state and a replay filtering on `St` is one short.

Trips per flight against `SRCF_CNF_TIME`, six outdoor benign flights and
the indoor set:

| | 0.5 s | 1.0 s | 1.5 s | 2.0 s |
|---|---|---|---|---|
| outdoor benign, 6 flights | 1 flight trips | 1 flight trips | none | none |
| logs 349, 350 | 3, 3 | 1, 1 | 1, 1 | at the edge |
| log 352 | 2 | 1 | - | at the edge |

So 1.5 s is where the detections gain margin without a benign flight
tripping. It is worth having for the cases where detection is the
mechanism, which is logs 349, 350 and 352 - each had thirty seconds to a
minute in hand.

**It does not rescue log 346, and neither does the mute.** That failure
ran from handover to wall in 2.1 s, and the 5 s post-switch mute covers
all of it, so the obvious move is to exempt `VD` from the mute:
`alignLanePosition` shifts position states and output history, not
velocity, so a velocity difference means the same thing either side of a
switch. Recomputing `VD` from both lanes' `XKF1` velocities through that
muted window says it would not have helped:

| t (s) | `VD` | |
|---|---|---|
| 166.7 | 0.22 | handover |
| 168.8 | 0.65 | impact |
| 171.0 | 1.87 | first above 1.6 |

`VD` was 0.65 at the moment of impact and did not cross the threshold
until 2.2 s after it. Both lanes see a vehicle accelerating from rest
and agree about it; the disagreement only builds as the GPS lane's
estimate diverges, which took 4.3 s.

So cross-lane velocity divergence is structurally too slow for a
two-second failure, and no confirmation time or mute change rescues it.
What prevents that class is not detecting faster but not handing over -
which is the offset bound, and it postdates the flight it would have
saved.

## 5j: the ladder working, and quality metrics finally buried - log 353

Flown on `15c477c348` with `SRCF_CNF_TIME` at 1.5 and `SRCF_FIXQ_SATS`
at 0. Armed on flow at 31.4 s, GPS restored at 42.9, the lane usable at
53.0, and at 63.2 s:

```
SRCF: GPS acquired 6m off, staying on flow
```

Refused, and it stayed refused for the whole flight. The vehicle flew
117 s on flow and landed normally at 148.5 s; the lane switch at 150.4
and the radio failsafe at 156.3 are both after disarm.

The fix it refused reported, over the 90 s it was on offer:

| | |
|---|---|
| satellites | 21-27, median 27 |
| `HAcc` | 0.65-1.16 m, median 0.88 |
| reported speed, median | 0.079 m/s |

and the vehicle hovered at 0.92 m indoors throughout. Twenty-seven
satellites and sub-metre claimed accuracy. Against that, `PD` ran
4.29-13.19 m with a median of 8.04 against a bound of 5.2-6.9 m.

The disagreement is about motion, not just datum. The pilot was flying -
`RCIN.C2` spans 1367-1711 - so the flow lane's travel is real, and per
30 s window the GPS lane says 15.0, 1.9 and 3.5 m where flow says 4.8,
4.2 and 5.3. It overstates the movement threefold in one window and
understates it twofold in the next.

So this is the end of the line for receiver-reported quality as a
discriminator, which four sessions have now approached from every angle:
satellite count, HDOP, `HAcc`, `VAcc`, speed accuracy, `EK3_CHECK_SCALE`
and the filter's own `gpsGoodToAlign`. A fix can report the best numbers
in the whole log set and still disagree with measured motion by a factor
of three. Only comparing it against something that measures the world
separately catches that, which is what the offset bound does.

`SRCF_CNF_TIME` at 1.5 also came through its first flight clean. `VD`
reached 1.88 against a 1.6 gate, so it crossed, and `VVot` never left
zero because it did not hold for 1.5 s. That is one indoor flight with
real stick input and no false trip at the shortened window.

## Still open

1. **The gate has never passed an honest fix outdoors.** It has accepted
   three times - logs 349, 350 and 352 - and every one was indoors on a
   repeater. The case the feature exists for, flying out of a building
   and taking up a fix that is right because it genuinely is, has not
   been flown. Log 348 came closest and was refused on a bad origin.
2. **A two second failure cannot be detected, only refused.** Log 346
   ran handover to wall in 2.1 s while `VD` was 0.65 at impact and did
   not cross its gate until 2.2 s after. No confirmation time or mute
   change reaches it, so the offset bound is the whole defence for that
   class and it has one flight of evidence behind it.
3. **The pilot has no way through a refusal.** Log 353 stayed on flow
   for its whole flight with the pilot unable to say "I can see it is
   fine, take it". `RCx_OPTION = 90` already means the pilot is
   intervening in source selection and does nothing in `FLOW_NO_GPS`.
4. **The flow-lost fallback takes the GPS lane with no offset check.**
   Same branch in flight as at touchdown, and in log 347 it fired during
   landing onto a lane the gate had refused for 98 s. Whether it should
   be gated is not obvious: the alternative when flow dies is AltHold
   with no position at all.
5. **Whether the first-fix handover should be automatic.** Design note
   open question 3, and stronger now: the pilot knows whether they are
   indoors and the monitor does not, and none of the automatic
   discriminators tried in this session work.
6. **A stale recorded origin has no provenance check.** Pinning it by
   hand fixed the symptom on this airframe; `RECORD_ORIGIN` overwrote it
   on three consecutive flights before that, once from a repeater's idea
   of where the vehicle was.
7. **The demote fires during touchdown.** Log 351 lost the flow lane at
   0.06 m AGL - below `RNGFND1_MIN` - and changed `LOITER` to `ALT_HOLD`
   6.7 s before `LAND_COMPLETE_MAYBE`, so position hold drops out in the
   last centimetres of every GPS-free landing.
8. **`SRCF_CNF_TIME` at 1.5 has one flight.** Log 353 flew it clean with
   `VD` reaching 1.88 against a 1.6 gate, but the replay behind the
   change reproduces the logged vote peaks and is one tick conservative,
   so read it as "no worse than 2.0" rather than proven.
9. Sessions 3 and 4 items stand: `SRCF_VEL_THR = 3.0` unflown, no
   GPS-loss cycle at cruise, the offset detector's long soak, and the
   altitude-hold and ground-effect items on the octaquad.

### Settled this session, so nobody re-opens them

- Receiver-reported quality does not discriminate. Satellite count,
  HDOP, `HAcc`, `VAcc`, speed accuracy, `EK3_CHECK_SCALE` and
  `gpsGoodToAlign` were each measured and each says "excellent" on a fix
  whose position is wrong.
- Indoor flow at about 1 m is characterised: log 351 held 378 s
  continuously, quality 101-157, `RFND.Stat` 4 for all 7100 samples,
  and gave out only below the rangefinder's minimum range.
- Velocity divergence, displacement, altitude consistency, GPS
  self-consistency and position innovation were all tried as
  discriminators and none separates. `vd_envelope.py`,
  `innov_detector.py`, `xcheck_replay.py`, `gate_replay.py` and
  `cnf_time_sweep.py` are how, and they run in seconds from local disk.
