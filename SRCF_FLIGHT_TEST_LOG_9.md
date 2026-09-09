# SRCF field test log - session 9

Branch `SmallFastDrone-4.7.0-gps-optflow-fallback`. Vehicle SmallFastDronev1,
logs 3, 4 and 8, 2026-09-09. Logs 3 and 4 flown against `c791a380`, log 8
against `f856f69f`.

Outcome: logs 3 and 4 false-tripped with the session 6 witness gate in the
build, and the cause was not in SRCF. `AHRS_ORIGIN_*` still named North
Carolina and the recorded origin was adopted 2.4 s before the GPS driver had
even been detected, so both flights ran 6,623 km from their own origin. Log 8,
flown on the fix, has the origin, the altitude datum and the terrain database
all correct, and both the velocity and rate detectors quiet - and the offset
detector latching on 493 m of real flow-lane drift.

## The stale origin was adopted before the GPS existed

Session 8 item 4 said to fix `AHRS_ORIGIN_*` on the vehicle rather than rely on
the boot order. The boot order turns out not to be something an operator can
rely on at all:

| | log 3 | log 4 |
|---|---|---|
| GPSDisable switch (`RCIN.C11`) | 0 - no RC link until 25.1 s | 988 (LOW) throughout |
| recorded origin adopted | 5.0116 s | 5.0117 s |
| GPS driver's first message | 7.4466 s | 7.4472 s, **3D fix, 23 sats, HDop 0.62** |

Neither flight disabled GPS at the moment it mattered. In log 3 the pilot's
switch was not even reachable - the RC link came up 20 s later. The origin is
taken before the driver is detected, on every boot.

`use_recorded_origin_maybe()` guards on `using_gps_for_pos()`, which reads
`sources.getPosXYSource(primary)`. By 5.011 s SRCF has already announced "no
GPS, arming on flow lane" and moved the primary to core 1, whose
`EK3_SRC2_POSXY` is 0. So under per-core source sets the guard answers "no GPS"
for a vehicle whose core 0 is configured for GPS and whose receiver fixes two
seconds later. Its own comment describes exactly the failure it then permits:
GPS will set a correct origin when it gets a fix, and adopting here prevents
that because the EKF origin is immutable.

`record_origin()` cannot recover it either. It writes `state.origin` on the
`origin_ok` false-to-true transition - which is the stale value it has just
adopted. Log 4 ran with `AHRS_OPTIONS` 24 rather than 16 for exactly this
reason and behaved identically.

## What a 6,363 km east offset does to the GPS lane

`Location::get_vector_xy_from_origin_NE_cm()` and `get_distance_NE_ftype()`
scale the east offset by `longitude_scale((lat + origin.lat)/2)` - the *mean*
latitude of vehicle and origin. With the origin 16.5 degrees of latitude away,
flying north moves that mean latitude and rescales the whole 6,363 km:

```
v_east_false = -(east_offset . tan(lat_mean) / 2R) . v_north  ~=  -0.47 . v_north
```

Measured on log 3 over 145.5-146.5 s: core 0's `PE` moved **-6.5 m** while its
own `VE` and the GPS both read -2.7 m, an artifact of **-3.8 m/s** against
-3.78 predicted at `v_north` = +8.0. Core 1 moved -3.0 m - the truth - because
it integrates flow and never converts lat/lng. `PN` is smooth in both lanes, as
it must be: the north axis carries no scale factor.

Not explained: the artifact arrives as a ~4.5 m step once per second rather
than smoothly, while GPS position innovations stay at 0.02-0.4 m throughout.
That shape says position reset rather than fusion, and the 1 Hz carrier has not
been identified. It does not change the magnitude or the cause.

## Logs 3 and 4: which detector fired

Both on excellent fixes, and the session 6 witness gate passed in both -
`RFND.Stat` 4 at 11 m in log 3 and 12.6-14.7 m in log 4, so `flow_is_witness`
was true and every detector was free to vote.

| | log 3 @147.9 s | log 4 @86.7 s |
|---|---|---|
| detector | position **rate**, `PVot` 11-15 | **velocity**, `VVot` 1-14, `OVot` in lockstep |
| signal vs gate | `PR` 4.86 vs 2.34 | `VD` 4.40 vs 2.53 |
| `PD` | 15.3 m | 43-47 m |
| GPS | 20 sats, HDop 0.68, HAcc 0.26 m | 23 sats, HDop 0.62, HAcc 0.50 m |

Log 3 is the origin artifact: without the 1 Hz stepping `PD` grows at ~0.7 m/s
and nothing votes. Log 4 is not - its velocity divergence is east-axis, core 0
reading 15.84 m/s against a GPS truth of 16.5 while core 1 read 19.73. The flow
lane was over-reading east velocity by ~19% at 18 m/s with the rangefinder
walking out to its 15 m limit, which is a stale high AGL and a separate
problem.

## Consequences beyond the false trips

The terrain database asked for the wrong continent: `TERR.Status` 1 at
35.18744, -79.37140 with `Loaded` 0 and `Pending` 224 for 29 rows in log 3 and
26 in log 4, clearing only when the GPS lane became primary at 34.6 s / 31.0 s.
Every row after that is Status 2 at the true site, so terrain itself was never
broken - it had no valid position to look up.

The altitude datum never recovered at all. On the ground in log 3, GPS AMSL read
**117.09 m** (175 samples, std 0.30) while the vehicle reported **139.53 m** -
the North Carolina origin altitude, inherited through `EK3_SRC1_POSZ` = baro.
The terrain database put the site at 115.1 m, and at arming a **24.43 m**
terrain reference offset was written to close a gap that was the origin error.

`correctEkfOriginHeight()` exists to reconcile exactly this and could not.
`setOrigin()` left `ekfOriginHgtVar` at its zero init, so an origin height that
came out of a parameter file was treated as known to the accuracy of a fix. The
correction gate needs `innov^2/(ekfOriginHgtVar + originHgtObsVar) < 25`, and
the prior grows by `sq(baroDriftRate*dt)` per call - about 5e-4 m^2/s at the GPS
rate - so admitting a 22 m innovation would have taken on the order of eleven
hours.

## What was fixed

Three changes, and one proposal dropped:

- the recorded origin is adopted during the arming sequence rather than at
  boot, but only where a GPS is configured at all (`get_type()` over
  `GPS_MAX_RECEIVERS`; `num_sensors()` is still 0 at 5 s and would have
  defeated it). A vehicle with no receiver - Sub, an indoor arm on a board
  without one - still takes it at boot;
- a recorded origin that names somewhere other than the origin actually set is
  reported when the real one arrives, which is the only moment the two can be
  compared;
- an externally set origin gets an honest height variance so the existing
  origin-height correction can run for it.

Dropped: scoping `configuredToUseGPSForPosXY()` to all cores under per-core
source sets. With adoption at arming the primary genuinely is the flow lane at
that moment, and that is exactly when the recorded origin is wanted; the change
would have refused it and broken the indoor arm. Also dropped: a pre-arm
refusal on a far-away datum. `readGpsData()` sets the origin from any fix that
passes the align checks whatever the position sources say, so a recorded origin
is only ever reachable when there is no fix to compare it against.

## Log 8: the fixes confirmed

| | log 3 | log 8 |
|---|---|---|
| EKF origin | North Carolina, boot-adopted | the site, GPS-set at 17.35 s |
| `AHRS_ORIGIN_*` after flight | unchanged | **rewritten to the site at 17.35 s** |
| field elevation | 140 m | 114 m |
| `XKF1.OH` | 139.5 (22.4 m out) | 114.19 vs GPS AMSL 114.44 |
| terrain | Status 1, Loaded 0 for 35 s | Status 2, Loaded 336 by 9 s |
| terrain ref offset | +24.43 m | -0.80 m |
| position states | 6.4e6 m | 22 m |
| 1 Hz east step | -4.5 m/s | gone |
| `PR` at the vote | 4.86 m/s | 0.35 |
| `VD` at the vote | 2.08 | 0.61 |

`record_origin()` doing the self-heal is the part that could not work before.

## Log 8: the offset detector on a real disagreement

At 244.4 s in Loiter, nine seconds from touchdown, `OVot` climbed 1-14 and
latched while `VVot` and `PVot` stayed at zero. `PD` 493 m against `PSig` 11.0
is 45 sigma against a 4-sigma gate, and this offset is not an artifact:

- GPS lane: PN 21.6, PE 3.5
- flow lane: PN 321.1, PE -390.1
- GPS: **29 satellites, HDop 0.48, HAcc 0.41 m, SAcc 0.11 m/s**

The monitor moved the vehicle onto the lane that was wrong. Reported position
jumped 51.70973,-0.63661 to 51.71238,-0.64234 and terrain began looking up a
spot 500 m away (`CHeight` -31.8 m). Nothing followed only because the pilot
was already descending through 14 m at 2.6 m/s.

`PD` was 4.7 m entering acro at 123.3 s and **89 m ten seconds later**, then
wandered 165, 437, 316, 515 and settled near 490, in steps aligned with six
`stopped aiding` / `started relative aiding` cycles between 129.6 and 201.6 s.
This sortie was flown much harder than the previous two:

| acro segment | mean speed | mean AGL | `RFND.Stat` median |
|---|---|---|---|
| log 3 | 7.6 m/s | ~15 m | 3 |
| log 4 | 10.6 m/s | ~21 m | 1 |
| **log 8** | **17.3 m/s** (max 27) | **~29 m** (max 61) | **1**, mean 1.04 |

The rangefinder returned nothing for ~96% of the acro segment against
`RNGFND1_MAX` 15 m. That is session 8's mechanism run to completion. Logs 3 and
4 had **zero** aiding dropouts; log 8 had six.

## The witness gate's timescale

`flow_is_witness = flow_usable && rangefinder_alt_ok()` - both instantaneous,
while the quantity they gate, `pos_div`, is accumulated. At 244.4 s the vehicle
was back low and slow with the rangefinder returning, so the gate passed on a
frame that had been destroyed 100 s earlier.

`FlwU` read 1 for the whole flight, including while the lane was 490 m adrift:
`flow_usable` is `healthy && horiz_pos_rel && optflow.healthy()` and none of
those notice a frame that has walked away. Worse, SRCF cannot see the dropouts
at all - **`FlwU` never went 0 across 1868 armed samples** despite six of them,
because the EKF restarts relative aiding within 4 ms of stopping.

The fix committed this session counts the transitions in the EKF instead. A
lane that has ceased aiding dead reckoned through the gap and resumed on a
position of its own, so it is latched out as a spoof witness for the rest of
the flight. It is still available as a fallback if GPS is genuinely lost -
there it is all there is - but it may no longer be the evidence that demotes a
healthy GPS lane.

## A dead reckoned frame also blocks the way back

Derived from the source and log 8's numbers, not measured: none of the
three flights exercised the FLOW_LOSS auto-recovery, and log 8's switch was
to FLOW_SPOOF, which is latched by design.

The recovery from the flow lane to the GPS lane requires the two to agree:

```c
const bool offset_ok = pos_sigma_valid &&
                       (pos_div < SRCF_RECOV_POS_NSIGMA * pos_sigma);   // 6.0
```

A lane that has dead reckoned cannot satisfy that. On log 8's numbers -
`PD` 493 m against `PSig` 11.0 - the ratio is 45 against a bound of 6, so
the vehicle would go out to the flow lane on a GPS loss and then be refused
the return, warned `SRCF: GPS ...m off, staying on flow` every
`SRCF_RECOV_TIME`. The comment at that gate anticipates the slow case, and
sigma growth does keep pace with smooth dead reckoning; it does not keep
pace with an aiding-dropout jump.

So the frame break does not stop the vehicle reaching the flow lane. It
stops it coming back, which matters more on a profile that expects to cross
between the lanes repeatedly.

Relaxing the bound was rejected. It is the only gate that sees a static
spoof: a captured receiver reporting a fixed position with no motion
presents no velocity difference and no divergence rate against a hovering
vehicle, so velocity agreement cannot stand in for it.

**Decision: a manual handback.** The automatic path keeps refusing and says
why, and the pilot forces the return with a source set change - the idiom
that already clears the spoof latch. The handback then realigns the flow
lane into the GPS lane's frame, which repairs the break and restores the
witness. That reverses the earlier choice not to clear the latch on
realignment: with an explicit pilot authorisation and an alignment actually
run, the offset test is meaningful again and there is no reason to keep the
monitor disabled.

## Still open

1. **Log 4's velocity trip is untouched.** The flow lane over-read east
   velocity by ~19% at 18 m/s with no aiding dropout, so neither the witness
   gate nor the new latch would have blocked it. A stale AGL height at the
   rangefinder limit is the likely term and it has not been measured.
2. **The latch and the manual handback have no field data.** The latch would
   have blocked log 8's trip - first dropout 115 s before the vote - and
   would not have blocked logs 3 or 4. The handback path has never been
   exercised at all, in the air or in any of these logs: the recovery gate
   it addresses is reached from FLOW_LOSS, and no flight here entered that
   state. Both are SITL-only.
3. **SRCF's premise is backwards on this airframe.** The design assumes a
   spoofed GPS drags the GPS lane while the flow lane keeps measuring real
   motion. In acro this flow lane does not keep measuring real motion; it
   wanders hundreds of metres. Every divergence detector is therefore measuring
   the flow lane's error and attributing it to GPS.
4. **`SRCF_POSD_NSIG` was flown at 4.0.** The parameter's own description calls
   it off by default and unflown. It has now been flown and it false-tripped;
   0 is the setting for the next sortie until item 3 has an answer.
5. **The 1 Hz carrier of the origin artifact is unexplained.** Academic now
   that the origin is right, but the record should not claim a mechanism it
   does not have.
6. Session 8 item 4 is closed. Item 2's "nothing has flown with it" no longer
   holds: the witness gate has flown three sorties and prevented no trip. In
   log 4 it held the votes for 1.5 s while `flow_usable` was false and they
   ran as soon as it opened; in logs 3 and 8 its terms were satisfied
   throughout. Items 1 and 3 stand.

### Settled this session

- Logs 3 and 4 were not SRCF failures. A recorded origin adopted before the GPS
  driver was detected put both flights 6,623 km from their own origin, and the
  longitude-scale artifact that follows is what the rate detector latched on.
- The altitude half of the same datum error was 22.4 m and structurally
  uncorrectable, and got written into the terrain database as a 24.4 m offset.
- With the origin correct, the velocity and rate detectors sit at a quarter of
  their gates through an acro sortie flown harder than either of the flights
  that tripped them.
- The witness gate is measured at the wrong timescale: instantaneous terms
  guarding an accumulated quantity, on a lane whose usability flag never once
  reported the six dropouts that broke its frame.
