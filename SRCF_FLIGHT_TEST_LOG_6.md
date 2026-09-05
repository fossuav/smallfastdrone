# SRCF field test log - session 6

Branch `SmallFastDrone-4.7.0-gps-optflow-fallback`. Vehicle
SmallFastDronev1. Log 356, 2026-09-05, outdoors in the UK, flown against
`15c477c3` - the same build sessions 5j and 5k flew.

Outcome: the first flight at a new site, and the flow lane arrived at
arming in a position frame 6,623 km wrong. 1.40 s after arming the
position offset detector confirmed a spoof on the resulting lane
disagreement, latched, and the vehicle flew the whole 250 s acro sortie
on the flow lane: no terrain data, no SmartRTL, six EKF failsafe cycles
and the wrong lane carrying the vehicle throughout. Handling was
unaffected.

**Four readings in this file were wrong before the measurement was
finished, and the corrections are the useful part.** In order:

- That an offset that large is too big to be a spoof and should be
  refused as a frame error. A spoofer parks a receiver at an arbitrary
  distant point, so there is no such bound; recorded below as rejected.
- That the stale recorded origin was the root cause. A distant origin is
  legitimate and this one demonstrably worked right up to the lane
  switch. The root cause is that the ground lane selection returns to the
  GPS lane without aligning the flow lane, so nothing ever put the flow
  lane into an earth frame.
- That `PR` measured anything here. At 6.62e6 m the float32 spacing makes
  every non-zero sample an exact multiple of 0.250 m/s. It is the
  quantisation of a misaligned frame.
- That `flow_usable` was the wrong test for a witness gate, on a
  misread of `FlwU` against `HasP`. They agree exactly; it is the right
  test, it is just not sufficient.

The acro measurement below is independent of all of that and outlives it:
the cross-lane velocity difference sits above its gate for 78% of this
sortie, so the monitor as configured cannot fly this profile whatever the
origin says.

## The flight

| t (s) | |
|---|---|
| 13.48 | `RC11: GPSDisable HIGH` - already high when the RC link came up |
| 28.38 | `RC11: GPSDisable LOW` |
| 29.20 | GPS 3D fix, 9 satellites, HDop 1.05, at 51.7095/-0.6366 |
| 33.09 | `EKF3 IMU1 started relative aiding` |
| 35.08 | `SRCF: no GPS, arming on flow lane`, lane switch 1 |
| 35.08 | `AHRS: using recorded origin:35.18744,-79.37141,139` |
| 39.82 | `EKF3 IMU0 is using GPS` |
| 41.79 | `EKF3 lane switch 0` - ground selection returns to the GPS lane |
| 77.28 | armed; home set from the GPS lane to the true position |
| 78.68 | `SRCF: GPS spoof suspected, using flow lane`, `GU` latched |
| 79.40 | ACRO, held to 329.4 |
| 92.08 | `EKF variance`, `EKF Failsafe` - first of six |
| 117.13 | `SmartRTL deactivated: bad position` |
| 321.58 | `Battery Failsafe` |
| 358.0 | disarmed |
| 359.89 | `EKF3 lane switch 0` - the latch clearing on disarm |

## A distant origin is not the fault

`AHRS_ORIGIN_LAT/LON/ALT` read 35.18744, -79.37141, 139.5 - the North
Carolina site of session 5 - while the receiver put the vehicle at
51.7095, -0.6366 on `Status` 3 with 14-16 satellites and HDop 0.79-0.86.
That is a 6,623 km stale datum, and it is worth being explicit that it
was not what broke the flight, because the obvious reading is that it
was.

Nothing about the distance hurt. The board is an H743
(`SmallFastDronev1` includes `TBS_LUCID_H7`), so `HAL_WITH_EKF_DOUBLE` is
set: the horizontal position states are `double` and
`EK3_POSXY_STATE_LIMIT` is 50e6 rather than the 1e6 float limit
(`AP_NavEKF3_core.h:97-101`). Core 0 carried its 6.62e6 m state
uncrushed:

| | |
|---|---|
| logged `XKF1` core 0 | `PN` 1,839,222 m, `PE` 6,363,081 m |
| magnitude | 6,623,559 m |
| computed from the two coordinates | dN 1,839,233, dE 6,363,119, 6,623,599 m |
| core 0 innovation ratios, armed | `SV` max 0.32, `SP` max 0.35 |

40 m of disagreement in 6.6 million, and a lane whose own tests never
came near failing. No clipping, no precision loss.

Everything downstream of it worked as well, for as long as the GPS lane
was primary:

- **Home was correct.** `ORGN` type 1 at arming is 51.7095518,
  -0.636655, the true position.
- **Position reporting was correct.** `TERR` at 76.98 and 77.98 s -
  armed, GPS lane primary, origin 6,623 km away - reads 51.7095517,
  -0.6366549 with `Status` 2 and `TerrH` 115 m. Terrain was healthy
  *with* the stale origin.

A GPS lane computes position as `origin.get_distance_NE(gps_loc)`, so any
origin works and the lane self-corrects on its first fix, which core 0
did at 39.82 s. The origin being far from home is ordinary.

## The flow lane was never in an earth frame

A relative-aiding lane has no such anchor. Its position state is
displacement from wherever aiding began, and the only thing mapping that
to the earth is the assumption that it began at the origin.

The flow lane started relative aiding at 33.09 s, **two seconds before
the origin existed**. Its states were near zero, and when the origin was
set those zeros became "at the origin". `getPosNE` adds
`public_origin.get_distance_NE_postype(EKF_origin)`
(`AP_NavEKF3_Outputs.cpp:280`), which is zero once both lanes share the
origin, so the flow lane reported the origin's coordinates. Its absolute
position error is exactly the origin error, and it had no mechanism of
its own to discover that.

## The ground handback skips the alignment

SRCF already carries the fix for this. `ahrs.align_lane_position()` at
`source_fallback.cpp:275` pulls the flow lane into the GPS lane's frame,
and the design note's section 3 is about exactly this failure. It runs
only when `align_pending` is set and the primary is back on the GPS lane,
and before this session `align_pending` was set only by the armed
`FLOW_NO_GPS` handover.

The disarmed branch did neither:

```c
// source_fallback.cpp:186-189, in source_fallback_ground_lane()
srcf_state.lane_state = (ahrs.get_primary_core_index() == SRCF_FLOW_LANE) ?
                        LaneState::FLOW_NO_GPS : LaneState::GPS_PRIMARY;

// source_fallback.cpp:216, in the !armed early return
srcf_state.align_pending = false;
```

`lane_state` was recomputed from the primary index every disarmed tick
and `align_pending` was cleared unconditionally, so the flow to GPS
handback at 41.79 s was a plain relabel: no alignment, no offset bound,
no statustext. The vehicle armed 35 s later in `GPS_PRIMARY` with the
flow lane still 6,623 km adrift.

One thing in the log looks like an alignment and is not. `PD` reads
exactly 0.0 for 78.78-83.58 s, then returns to 6,623,560.5. That is the
5 s `SRCF_POST_SWITCH_MUTE_MS` making `div_ok` false and leaving the
locals at their initialised zero - 78.68 + 5.0 = 83.68 to the sample. The
flow lane was not aligned at any point in the flight.

**So the stale origin was the trigger, not the cause.** Had the handback
aligned, this flight is uneventful with the wrong datum left in place:
both lanes read the true position, `PD` is near zero so no spoof trip,
terrain and SmartRTL keep working, and the acro sortie runs on the GPS
lane. The wrong `AHRS_ORIGIN_*` would have cost nothing but a parameter
sitting unused.

## Why the monitor switched

`SRCF_POSD_NSIG` was 4, its second flight ever - session 4's log 338 flew
it benign at the old site.

| | |
|---|---|
| `PD` | 6,623,560 m |
| `PSig` | 0.75 m, floored to the 2.0 m `SRCF_POSD_MIN_SIGMA` |
| gate | 4 x 2.0 = 8.0 m |
| ratio | about 828,000 |
| `OVot` | 1 to 15 over 1.40 s, `SRCF_CNF_TIME` 1.5 |

`VVot` and `PVot` never left zero. `VD` ran 0.006-0.030 and `PR` 0.0: two
lanes sitting on the ground agree about velocity however far apart their
frames are, so only the offset detector could see this. That is the
detector doing exactly what `source_fallback.cpp:362-381` says.

The consequence is that the monitor moved the vehicle from a healthy lane
fusing a 16-satellite fix to the lane that was actually wrong, because
the spoof branch assumes the GPS lane is the faulty term. `pos_div` is
symmetric and cannot say which term is at fault.

## What a large offset cannot tell you

The first reading of this flight was that a 6,623 km offset is
"physically impossible" for a spoof and should be refused as a frame
error rather than latched. **That is wrong.** A spoofer commonly parks a
receiver at a fixed distant point - Lima is the canonical one - so an
offset of thousands of kilometres is an ordinary spoof signature, and
there is no magnitude above which the frame explanation becomes the safe
assumption. Had this been a capture rather than a misaligned lane, moving
to the flow lane and latching is precisely the right behaviour, and a
magnitude bound would have handed the vehicle back to the attacker.

So the switch decision was not wrong on its inputs. Session 2 recorded
the general form of this - "a disagreement between the lanes says one of
the fix and the origin is wrong, and the offset alone cannot say which",
now a code comment at `source_fallback.cpp:322-326` - and log 348 was the
same ambiguity resolved the other way, where the gate refused an honest
18-satellite fix because the origin was 39 m out. Log 356 is that
ambiguity at 6,623 km.

What follows is that the discrimination cannot live in the detector. The
two lanes have to be in a common frame *before* the detector is asked to
compare them, which is what the alignment is for and why its absence on
the ground path is the defect.

## Why it never switched back

By design, and the log shows the design working. The spoof branch sets
`gps_untrusted` and `FLOW_SPOOF`, and recovery at
`source_fallback.cpp:398` requires `!gps_untrusted`. `St` = 2 and `GU` = 1
for all 2,792 armed samples after the trip. The `EKF3 lane switch 0` at
359.89 s is the disarm reset at 358.0 s, not a recovery.

## The terrain errors

`TERR.Status` = 1 for every one of the 92 samples from the switch to
disarm, with `TerrH` 0.0 and the requested position reported as 35.1874,
-79.3714. Before the switch it read `Status` 2 at the true position with
`TerrH` about 115 m.

With the flow lane primary the reported position is its own, so the
terrain library asks for tiles at the old site and there are none on the
card. `Pending` sat at 224-336 for the rest of the flight. The
`SmartRTL deactivated: bad position` at 117.13 s is the same cause.

## The EKF failsafes are the switch, not a fault

Six EKFCHECK cycles: entries at 92.1, 103.1, 230.6, 244.5, 278.4 and
294.8 s, the longest running 122 s. None is a variance breach. Over the
92.1 s entry the `EKFC` inputs read `VV` 0.0020, `PV` 0.0018, `HV`
0.05-0.22, `MV` 0.06-0.19 against a threshold of 0.8.

The field that fails is `HasP` = 0. `ekf_check` computes
`has_position = ekf_has_relative_position() || ekf_has_absolute_position()`
(`ekf_check.cpp:81`) and both read `ahrs.has_status()`, which is
primary-lane keyed. On the flow lane there is no `HORIZ_POS_ABS` at all,
and `HORIZ_POS_REL` dropped repeatedly - eleven `EKF3 IMU1 stopped
aiding` events.

The flow lane could not hold aiding because it had no height reference.
Rangefinder instance 0 over the acro segment:

| `RFND.Stat` | samples | |
|---|---|---|
| 1 (NoData) | 3866 | 78% |
| 3 (OutOfRangeHigh) | 486 | 10% |
| 4 (Good) | 608 | **12%** |

against `RNGFND1_MAX` 15 m while the vehicle flew at 14-53 m, with
`EK3_OPTIONS` = 26 and bit 6 (flow above the rangefinder ceiling) clear.

Derived from core 0's logged health plus the primary-keyed code path
above, not separately measured: with the primary left on lane 0,
`ekf_has_absolute_position()` would have been true throughout and none of
the six failsafes would have occurred.

## Acro cannot be flown with the monitor armed

Handling was unaffected. Rate tracking over 85-320 s:

| | desired | actual |
|---|---|---|
| roll | -8.376 | -8.371 |
| pitch | 9.658 | 9.608 |
| yaw | -10.82 | -10.79 |

No mode change; `FS_EKF_ACTION` = 2 correctly took no action because ACRO
does not require position.

But `VD` is a velocity difference and therefore frame-independent, so it
is a valid measurement despite the misaligned lane. Over the same window:

| | value | gate |
|---|---|---|
| `VD` p50 / p95 / max | 3.99 / 28.8 / 33.9 | 1.6 |
| samples above the gate | 1829 of 2350, **78%** | |
| longest unbroken run | **118.2 s** | |

### `PR` is unusable in this log, and the first reading of it was wrong

An earlier version of the table above carried "`|PR|` above gate 78%,
longest run 16.8 s" beside those `VD` figures. **That is not a
measurement of anything.** `pos_div` is a `float`, and at the 6.62e6 m
this flight ran at, float32 spacing is 0.500 m; `pos_rate` differences it
over the 2 s `SRCF_POS_RATE_WINDOW`, giving a 0.250 m/s quantum. Every
one of the 2307 non-zero `|PR|` samples in the armed flight is an exact
multiple of 0.250. `PR` here is the quantisation of a misaligned frame,
not vehicle behaviour, so the position-rate detector was also being fed
noise for the whole sortie.

`VD` is unaffected: a velocity difference of order 0-34 has no such
problem. Everything said about the acro envelope rests on `VD` alone.

Once the lanes are aligned this cannot arise, because `pos_div` is then
metres rather than megametres. It is recorded because it is the second
time this file has caught a logged field that looked like a signal and
was an artifact, after the stale `innovVelPos` of session 5h.

The flow lane cannot track this profile. At 110.5 s GPS truth was
15.8 m/s on course 162 deg; core 0 read `VN`/`VE` -15.00/+6.08 while core
1 read -12.96/+9.64 - the same magnitude with 19 deg of heading error,
which is an unaided lane dead reckoning through a hard turn with no
height reference for 88% of the flight.

Mechanism only, and the counterfactual is not measured: a lane's own
estimate does not depend on which lane is primary, so on aligned lanes
with `SRCF_ENABLE = 1` this sortie should have latched a false spoof on
the velocity detector within `SRCF_CNF_TIME` of the first sustained
manoeuvre. What would falsify it is one acro flight with the lanes
aligned and the monitor armed, with the counters read afterwards. Until
that is flown, treat the monitor as unflyable in acro rather than
assuming it.

The structural gap is that `can_vote` (`source_fallback.cpp:351`) is
`div_ok && !gps_bad_now && (primary == SRCF_GPS_LANE)`. It never asks
whether the flow lane is a usable witness, and `flow_usable` sits two
dozen lines above it unconsulted. An unaided flow lane above the
rangefinder ceiling is not evidence about GPS, and the monitor votes a
latching spoof on it.

### The witness gate helps and does not close it

Corrected: an earlier version of this section said `FlwU` logged 1 for
the whole flight while `HasP` was 0, and used that to argue `flow_usable`
was the wrong test. **Wrong, and measured the other way.** Over the armed
flight `FlwU` is 0 for 1884 samples and `HasP` is 0 for the same 1884.
They agree, because `horiz_pos_rel` already requires `optflow_gnd_offset`
(`AP_NavEKF3_Control.cpp:831`). `flow_usable` is the right shape.

It is not sufficient, which only a sweep shows. Longest unbroken run of
`VD` over its gate while a candidate still permits the vote, over
85-320 s; anything reaching 15 samples latches at `SRCF_CNF_TIME` 1.5:

| candidate | samples voting | longest run |
|---|---|---|
| today: no witness test | 1829 | 1182 (118.2 s) |
| `flow_usable` | 310 | 61 (6.1 s) |
| `flow_usable` and rangefinder returning | **86** | **54 (5.4 s)** |
| `flow_usable` held 1 s | 292 | 61 (6.1 s) |
| `flow_usable` held 5 s | 163 | 61 (6.1 s) |
| `flow_usable` held 10 s | 43 | 38 (3.8 s) |

A 21x cut in exposure and it still latches, by a factor of three. The
health flags say "flow data is arriving and a height is known", which is
not the same as "this estimate is accurate at 20 m/s through a hard
turn" - at 110.5 s the lane had the speed right and the velocity vector
19 deg out while every flag was green. No boolean over the existing flags
separates those, so the witness gate is necessary and something else is
needed for aerobatics. `gate_sweep.py` is the sweep.

**What it costs, stated rather than shipped quietly: spoof detection
stops above the rangefinder ceiling.** With `EK3_OPTIONS` bit 6 the flow
lane keeps navigating up there on a flat-ground assumption, and an
assumed height is an assumed velocity scale, so the gate calls it not a
witness. Both test airframes fly bit 6. Against that, the detector has
never been shown to work above the ceiling and every false trip that
region produced is in this record: session 1's log 329 tripped at
13-16.5 m, and session 5f's height sweep put the worst benign excursions
at 0.2-1.0 m and 16-21 m with the quiet band at 5-9 m. Removing a
measured false-trip region and no demonstrated capability is the right
trade, but it is a trade.

Run against session 1's false trip (log 329, at its own 0.8 gate and 2.0
confirmation) the gate halves the exposure, 9 samples over the gate to 4:
that vehicle was hovering on the 15 m rangefinder ceiling with `RFND.Stat`
flickering 3 and 4 through the vote. **Not a clean retrospective test,
and it should not be read as one** - log 329 tripped on the OR-relay
between `PR` and `VD` that the split vote counters already fixed, so a
velocity-only sweep cannot reproduce that trip at all. Directionally
supportive, nothing more.

## A log-reading trap

`RFND.Dist` carries stale values when `Stat` is not 4. Reading the field
without the status gives "rangefinder to 23.1 m" on a sensor whose
`RNGFND1_MAX` is 15, and the first pass through this log did exactly
that. Split by `Instance` and filter on `Stat == 4` before quoting any
rangefinder number.

## Vehicle state for this flight

```
AHRS_OPTIONS    = 16      bit 4 only; bit 3 correctly off since session 5
AHRS_ORIGIN_LAT = 35.18744    the previous site, 6,623 km away
AHRS_ORIGIN_LON = -79.37141
AHRS_ORIGIN_ALT = 139.5
SRCF_ENABLE     = 2
SRCF_VEL_THR    = 1.6
SRCF_POSR_THR   = 1.9
SRCF_CNF_TIME   = 1.5
SRCF_RECOV_TIME = 10.0
SRCF_NSIGMA     = 2.5
SRCF_POSD_NSIG  = 4       second flight ever
SRCF_FIXQ_TIME  = 0
SRCF_FIXQ_SATS  = 0
EK3_OPTIONS     = 26      bit 6 clear: no flow above the ceiling
EK3_GLITCH_RAD  = 0
EK3_FLOW_GAIN_H = 12
RNGFND1_MAX     = 15
FS_EKF_ACTION   = 2
```

## What to change

1. **Align the flow lane on the ground-side handback.** The root fix, and
   it reuses the `align_pending` path the armed handover already has:
   set it when the ground selection commands flow to GPS, and service it
   once the switch has landed and the GPS lane holds real absolute
   position. With this in, log 356's stale datum is inert.

2. **`can_vote` must require the flow lane to be a usable witness.**
   `flow_usable` and a live rangefinder, so the lane is aiding on flow
   with a height to scale flow rate by. Independent of the frame fault.
   Necessary and measured insufficient: it cuts the voting samples 21x
   and still leaves a 5.4 s run against a 1.5 s confirmation, so it
   improves the monitor without making it safe to arm for aerobatics.

3. **Something else is still needed before acro.** The candidates that
   remain are not thresholds - the sweep says no boolean over the health
   flags separates a bad witness from a good one under high dynamics.
   The two shapes worth considering are not voting in modes that do not
   navigate on the estimate, which costs a short exposure on entry to a
   position mode and nothing else, and making the spoof state
   recoverable rather than latched. Both are behaviour changes with real
   trade-offs and neither is measured yet, so neither is proposed here.

4. **Origin provenance, still open and now a nicety.** A check comparing
   the recorded origin against the current fix before adopting it would
   have caught this too, and it remains design note open question 4. It
   is the harder change - under a repeater it compares a good origin
   against a bad fix, so a refusal has to be recoverable by the pilot -
   and after change 1 the stale datum is no longer load-bearing.

5. Rejected: bounding `pos_div` so an implausibly large offset is treated
   as a frame error rather than a spoof. There is no such bound. A
   spoofer parks a receiver at an arbitrary distant point, so the
   magnitude that looks impossible is an ordinary capture signature, and
   the bound would hand the vehicle back to an attacker in exactly the
   case the detector exists for. Recorded so nobody proposes it again.

## Still open

1. **The acro envelope is unmeasured on aligned lanes.** The `VD` table
   says the monitor cannot survive this profile, but it was taken with
   the flow lane both primary and misaligned, and the counterfactual is
   inspection rather than measurement. One acro sortie with the lanes
   aligned and `SRCF_ENABLE = 1` settles it, and it is now the gating
   measurement for anything past the witness gate: the sweep says the
   remaining shortfall is a factor of three, and whether that survives on
   an aligned lane at the same speeds is exactly what is not known.
   Until it is flown, acro wants `SRCF_ENABLE = 0`.
2. **The offset detector has now fired twice in the field and neither was
   a spoof.** Log 338 was benign, log 356 was a misaligned lane. It has
   still never seen the slow position-only walk it was built for outside
   SITL (`SRCFSlowSpoofPositionOffset`), and session 3's long soak for
   the ratio plateau is still not flown.
3. **The field guide tells you to pin the origin and does not tell you to
   check it.** `SRCF_SETUP_NOTES.md` section 8 says to clear
   `AHRS_OPTIONS` bit 3 and set `AHRS_ORIGIN_LAT/LON/ALT` by hand to the
   actual takeoff point, which is what this vehicle did - for the
   previous site. Change 1 makes that survivable rather than harmless:
   home and the fence still come from the datum during a GPS-free arm.
4. **`SRCF_ENABLE = 2` is what adopts a recorded origin at all.** Derived
   from the source, not measured: `use_recorded_origin_maybe`
   early-returns on `using_gps_for_pos()` (`AP_AHRS.cpp:1556`), which is
   primary-lane keyed, so the recorded origin can only be taken up once
   SRCF has moved the primary to the flow lane. At `SRCF_ENABLE = 1`
   nothing on the ground changes and the origin comes from GPS. That
   makes 2 the wrong setting for any flight that expects GPS, and nothing
   says so at arming.
5. Session 5 items 1-9 stand. Item 1 in particular is unchanged: the
   first-fix gate has still never passed an honest fix outdoors, and this
   flight did not exercise it - the vehicle armed on the GPS lane and the
   ordinary detector path is what fired.

### Settled this session

- A distant EKF origin is legitimate on a copter and costs nothing while
  the GPS lane is primary. Position, home and terrain were all correct at
  6,623 km, on double-precision position states well inside
  `EK3_POSXY_STATE_LIMIT`. Do not read a large origin-to-home distance as
  a fault on its own.
- A large cross-lane position offset does not distinguish a frame error
  from a spoof, in either direction. Log 348 refused an honest fix on a
  39 m bad datum; log 356 latched on a 6,623 km misaligned lane. The
  offset is symmetric and the ranking has to come from outside it.
- The EKF failsafe on the flow lane is `has_position`, not variance. Six
  cycles with every variance input under a quarter of the threshold.
  Same shape as the dow ceiling demotion in
  `../analysis/topics/dow_althold_ekf_failsafe.md`.
- Acro handling is unaffected by any of this. The vehicle flew 250 s with
  demand and response matching to 0.05 deg/s on all three axes while its
  position estimate was on another continent.
- A witness gate on `can_vote` is necessary and not sufficient, and the
  sweep is what says so rather than an argument. No boolean over the
  lane's existing health flags separates a flow lane that is measuring
  from one that is merely receiving data: at 110.5 s the lane had the
  speed right and the velocity vector 19 deg out with every flag green.
  Do not expect a tighter health test to close this.
- A logged field is not a measurement until you know its numerical
  range. `PR` at 6.62e6 m is float32 quantisation, and it reads as a
  plausible 0-33 m/s signal. Second instance in this feature's record
  after session 5h's sample-and-hold innovation.
