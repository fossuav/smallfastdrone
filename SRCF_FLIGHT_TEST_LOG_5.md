# SRCF field test log - session 5

Branch `SmallFastDrone-4.7.0-gps-optflow-fallback`. Vehicle
SmallFastDronev1. Log 346, 2026-08-20. Flown against `23a05a44`; the
code change at the end follows it.

Outcome: the first flight of the GPS-free arming path anywhere but
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

Sat count and HDOP are the two numbers a pilot has on the GCS and both
were immaculate. The receiver's own accuracy estimate bottomed out at
1.24 m while it was 26 m out. This is the case the OSD lane panel
cannot help with either: the GPS lane really was fusing GPS, so the
panel read `EKF0 ABS` throughout.

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

## Still open

1. The gate has now refused once in the field (log 347, 35 m at
   12.4-34.8 sigma) and has never been tested near its bound. Every
   case so far - 26 m, 35 m, and 100 m in SITL - has been ten times the
   threshold or more. A repeater closer to the true position, or an
   origin a few metres out, would sit near 6 and decide the flight;
   that case is untested in both directions.
2. The flow-lost fallback takes the GPS lane with no offset check, and
   it is the same branch in flight as at touchdown. In log 347 it fired
   during the landing, where it costs nothing, onto a lane the gate had
   been refusing for 98 s. Whether it should be gated is a real
   question and not an obvious one: the alternative when the flow lane
   dies is the AltHold demotion with no position at all, and a lane
   35 m out at least holds station. Needs deciding before it happens at
   height rather than on the ground.
3. Whether the first-fix handover should be automatic at all. This is
   design note open question 3, and this log is the argument for
   announcing it and leaving it to the pilot: a several-hundred-metre
   position reset mid-flight is a bigger event than anything the ladder
   does, and 2 s is not long enough for a pilot to work out what has
   happened. The gate reduces how often the question arises; it does
   not answer it.
4. A stale recorded origin still has no provenance check (design note
   open question 4), and it now matters more: with the gate in, a stale
   origin blocks an honest handover rather than silently corrupting
   home. A pre-arm comparing the recorded origin against the last known
   GPS position would catch the drive-to-a-new-site case.
5. Indoor flow at 1-2 m is still uncharacterised. This flight held
   0.9 m for 130 s at quality 134 with sub-decimetre position, which is
   better than session 3's open item feared, but it is one flight in
   one room.
6. An origin-free cross-check was prototyped as a replay over eleven
   logs and mostly does not work. Two were tried on a sliding window,
   both frame-free so neither needs the origin: the vector displacement
   of the two lanes against each other, and the change in reported GPS
   altitude against baro. Measured at a 5 s window, the longest that
   fits the budget below:

   | | horiz mismatch / max(travelled, 2 m) | vert mismatch |
   |---|---|---|
   | worst benign of 9 flights | 2.84 (log 337) | 4.2 m (log 330) |
   | log 346, decision window | 1.33 | 2.0 m |
   | log 347, decision window | 3.33 | 25.5 m |

   Displacement does not separate at any window length tried (20, 8,
   5 s): the bad case hovers so the floor governs it, while benign fast
   flight travels far enough to divide the ratio down. Altitude catches
   log 347 at six times the benign worst and is silent on log 346,
   which is the flight that crashed. The offset bound catches both.

   The reason is the budget rather than the test. `XKF4.AID` puts the
   GPS lane at `AID_ABSOLUTE` from 159.7 s in log 346 against a
   handover at 166.687 - **7.0 s** - and a slow wander cannot be
   observed in seven seconds. log 347 only looks easy because it never
   handed over and the check had 108 s.

   So the lever for flying in from outside on an approximate origin is
   time, not a sharper instantaneous test: keep flying on flow, which
   both flights did safely, and require an origin-free check to hold
   for tens of seconds before accepting a fix the offset bound would
   reject. Untestable on log 346, whose fix existed for 7 s, but it
   would have prevented that crash by never firing the handover.

   Replay tool: `xcheck_replay.py`.
7. Sessions 3 and 4 items stand: `SRCF_VEL_THR = 3.0` unflown, no
   GPS-loss cycle at cruise, the offset detector's long soak, and the
   altitude-hold and ground-effect items on the octaquad.
