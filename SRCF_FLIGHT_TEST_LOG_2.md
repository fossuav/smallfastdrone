# SRCF field test log - session 2

Branch `SmallFastDrone-4.7.0-gps-optflow-fallback`. Vehicle
SmallFastDronev1. Logs 332-336, 2026-08-12. Flights flown against
`41201aeb`; the code changes at the end follow it.

Outcome: the GPS-loss rung is field-validated in every regime that
matters - hands-off hover, 5 m/s translation, and above the
rangefinder ceiling. Seven loss/recovery cycles across three flights,
0.20 s handover and 12.2 s recovery every time. The spoof detector
false-tripped on the first flight, was root-caused to the fixed
threshold rather than the new significance gate, and was retuned and
revalidated in the air. Spoof detection is now known to be limited to
velocity-consistent spoofers; that limit is measured, not assumed.

## Card 1 - log332 - false spoof trip at 5 m

`SRCF: GPS spoof suspected, using flow lane` 151 s into a 5.4 min
soak. GPS was healthy throughout: 3D fix, 22 sats, HDop 0.60, HAcc
0.40 m, speed accuracy 0.08 m/s. `EKFC.Bad`=0 for the whole flight.

The trip was **not** affected by `SRCF_NSIGMA`. The gate is

    vel_gate = MAX(SRCF_VEL_THR, SRCF_NSIGMA * VSig)

and `VSig` through the trip window sat at 0.29-0.32, so the sigma
term was 0.73-0.80 and the 0.8 floor won on every sample. `VD`
crossed 0.8 at 211.2 s, held above it for exactly 20 samples
(`SRCF_CNF_TIME` = 2.0 s) and latched at 216.2 s. This is the
session-1 mechanism unchanged: `SRCF_NSIGMA` cannot suppress anything
while `2.5 * VSig` is below `SRCF_VEL_THR`, which on this vehicle
means below about 7 m.

Across the 4-7 m band the floor governed 83% of samples.

The split vote counters did work: `PVot` was 0 for the entire flight,
with `PR` peaking 0.63 against the raised `SRCF_POSR_THR` of 0.9. The
session-1 recommendation was sound at that altitude - but see log333,
where it is not.

Session 1 measured `VD` max 0.774 at 8 m and the plan quoted it
against `SRCF_POSR_THR` 0.9. `VD` is tested against `SRCF_VEL_THR`
0.8, so the real margin was 3%, not the comfortable one implied.
Flying the proper soak the plan demanded is what pushed it over.

The latch afterwards is by design. `FLOW_SPOOF` has no recovery path
(`source_fallback.cpp:238` guards recovery to `FLOW_LOSS`) and
`gps_untrusted` is set, so the vehicle stayed on the flow lane for
171 s until disarm. It remained controllable throughout.

`PD` at the handover was 9.80 m, against the 0.7-1.5 m session 1 saw.
The position controller absorbed it with attitude tracking error
under 0.4 deg and no lurch - GPS ground speed dipped 4.8 to 4.15 m/s
over 0.8 s and recovered.

## Card 3 - log334 - hands-off hover, and log333 under motion

log333 was flown at 5.0-5.9 m/s at every handover, so it is Card 4
rather than Card 3, at roughly double that card's 2-3 m/s. log334 is
the hands-off card: GPS speed 0.07-0.15 m/s at the switch.

| | log334 c1 | log334 c2 | log334 c3 |
|---|---|---|---|
| excursion within 2 s | 0.14 m | 0.17 m | 0.18 m |
| handover latency | 0.20 s | 0.20 s | 0.20 s |
| recovery | 12.2 s | 12.2 s | 12.2 s |
| `PD` at handover | 1.54 m | 2.48 m | 4.26 m |
| hands-off hold, 36 s | 1.82 m | 0.97 m | pilot input |

Against a criterion of 2 m, an 11x margin. Recovery is essentially
deterministic at `SRCF_RECOV_TIME` plus about 2.2 s of gate settling.

**`PD` grows with airborne time.** It is the flow lane's accumulated
dead-reckoning drift, so the offset the position controller must
absorb depends on how long the vehicle has been up, not on height or
speed: 1.54 m at the first trigger of a flight, 9.81 m at the second
trigger of log333, 9.80 m at the log332 trip. Session 1's 0.7-1.5 m
was an early-flight switch, not a stable figure.

In hover the detector is nowhere near tripping: `VD` max 0.79, `PR`
max 0.98, both vote counters flat at 0. The false trips come from
manoeuvring, not from the feature at rest.

## Card 5 - log335 - loss and recovery above the rangefinder ceiling

Flown with no valid `RFND` returns at all, which is the condition the
card targets.

| | |
|---|---|
| speed at switch | 0.14 m/s |
| excursion within 2 s | 0.17 m |
| attitude tracking error | 0.07-0.08 deg |
| `PD` at handover | 4.21 m |
| hold on flow lane | 30.2 s |
| recovery | 12.2 s |

`recovery_ok` went true 1 s after GPS returned and stayed true: `VD`
0.01-0.18 and `PR` -0.37 to +0.18 against gates of 1.60 and 1.90.
Under session-1 code this rung was impossible - `PR` swung past
+/-0.5 continuously and reset the timer indefinitely. `VSig` reached
0.89 up there, the highest measured.

## Threshold change and its validation

`SRCF_VEL_THR` 0.8 -> 1.6 and `SRCF_POSR_THR` 0.9 -> 1.9, set in the
field after log334 and flown in log335 and log336.

log335 did not reproduce the failure envelope: p50 height 11.0 m and
p50 speed 0.05 m/s, so it never approached either gate and proves
nothing about the change. log336 was flown at p50 height 5.5 m with
`VSig` 0.13-0.42, matching log332's 4-7 m and 0.14-0.49, and reached
`VD` 0.88 against log332's 0.94. That is the right envelope.

Each flight replayed through the vote integrator under both threshold
sets, `CNF_TIME` 2.0:

| flight | in flight | @ 0.8/0.9 | @ 1.6/1.9 |
|---|---|---|---|
| log332, 4-7 m, sustained 5 m/s | 1 trip | vel 1, peak 20/0 | 0, peak 0/0 |
| log333, 9-16 m, 5 m/s | clean | vel 1, peak 20/18 | 0, peak 0/0 |
| log334, hands-off hover | clean | 0, peak 0/0 | 0, peak 0/0 |
| log335, 11 m, bursts | clean | 0, peak 0/0 | 0, peak 0/0 |
| log336, 5.5 m | clean | **pos 3**, peak 15/20 | 0, peak 0/0 |

The replay reproduces log332's real trip, which is what makes the
rest of the column trustworthy; an earlier version of it did not, and
the error was a time window that truncated the last two samples of
the ramp. Note also that the firmware votes, then switches state,
then logs, so the tick that completes a trip is logged with the new
state - filtering on `St`=0 silently drops it.

Five false trips across three of five flights under the old
thresholds; none under the new, in any flight, with the vehicle's own
counters never leaving zero. log336 would have tripped on the
**position** detector where log332 tripped on velocity: at low
altitude `PR` is the more trigger-happy of the two, reaching 1.47
against the old 0.9.

log333 is the reason `SRCF_POSR_THR` 0.9 could not stand. It reached
`PR` 1.78 at 9-16 m and drove `PVot` to 18 of 20, alongside `VVot` 25
- that flight only survived because `SRCF_CNF_TIME` had been raised
to 10 for the day.

## What the detector can and cannot see

SITL spoof signatures measured against the new thresholds, from
`SRCFGPSSpoof`:

| | `VD` | `PR` | `VSig` | confirmed by |
|---|---|---|---|---|
| mode 2, R=1.5 | 1.90 | 1.92 | 0.49-0.68 | velocity, `VVot` 19 |
| mode 1, R=1.5 | 0.77-0.81 | 1.51-1.58 | 0.60 | neither |
| mode 1, R=2.5 | 1.42 | 2.61 | 0.48-0.71 | pos-rate, `PVot` 19 |

Mode 2 adds the walk rate to the reported velocity
(`SIM_GPS.cpp:595-599`); mode 1 walks position only.

Set against the field envelope:

| detector | benign max | weakest spoof | separable? |
|---|---|---|---|
| velocity | 1.44 (log333) | 1.77 (mode 2) | yes, 1.23x band |
| pos-rate | 1.78 (log333) | 1.51 (mode 1, R=1.5) | **no, overlaps** |

A position-only walk slow enough to sit inside the envelope two
healthy lanes cover in ordinary flight cannot be told from that
flight. On this airframe that means roughly below 1.9 m/s. Detection
now rests on the velocity detector catching velocity-consistent
spoofers - the ones that actually drag the vehicle - at `VD` 1.77
against a gate of 1.6, with benign `VD` 1.44 the same 11% under it.
The usable band is [1.44, 1.77] and the configuration sits in the
middle of it. There is nowhere to move: raising the gates loses the
only mode still detected, lowering them brings back the false trips.

Session 1 put the SITL spoof at 2.9-3.9 sigma; these measurements are
consistent with the low end of that range and refine it per detector.
The sigma framing does not rescue the position case, because benign
`PR` reaches 3.2 sigma while the R=1.5 mode 1 walk reaches only 2.5.
`SRCF_NSIGMA` is still worth keeping - replayed with it off, the old
thresholds produce nine velocity false trips instead of one - but it
cannot close the gap alone.

## Bonus - the RC failsafe rung, unplanned

log334 took four in-flight Radio Failsafes, two of them while on the
flow lane. All four went to Brake automatically under
`FS_THR_ENABLE=7`; the pilot recovered manually via the mode switch
each time.

| | lane | entered from | result |
|---|---|---|---|
| 1 | GPS | hover | held 0.44 m over 33 s |
| 2 | **flow** | hover | held 0.58 m over 19 s |
| 3 | GPS | 5.0 m/s | stopped in 2.52 m |
| 4 | **flow** | 5.7 m/s | stopped in 3.89 m |

Brake works on the flow lane in the field, from hover and from
5.7 m/s. The `FS_ALTH_TMO` drift rung was not exercised - position
was available throughout, so it never demoted, and no
`Failsafe: AltHold, no position` appears. That path remains
SITL-only.

## VSig by height

Measured on the GPS lane with both lanes healthy. This is the table
the session was meant to bring back.

| Height (RFND AGL) | `VSig` p50/max | `VD` p95/max | `PR` p95/max |
|---|---|---|---|
| 0-3 m | 0.17 / 0.23 | 0.54 / 0.58 | +0.28 / 0.29 |
| 3-7 m | 0.34 / 0.45 | 0.44 / 0.53 | +0.77 / 0.78 |
| 7-11 m | 0.45 / 0.55 | 0.35 / 0.68 | +0.49 / 0.60 |
| 11-16 m | 0.52 / 0.59 | 0.61 / 0.93 | +0.27 / 1.52 |
| above ceiling | 0.58 / 0.89 | - | - |

`VSig` scales with height as the geometry predicts, and the plan's
estimate of about 0.6 at 13-16 m was right. The worst single
excursions of the session (`VD` 1.44, `PR` 1.78, both log333) fall
outside this table because the rangefinder was invalid at those
moments.

## Code changes

`Copter`: `SRCF_VEL_THR` default 0.8 -> 1.6, `SRCF_POSR_THR` default
0.9 -> 1.9, `SRCF_POSR_THR` range widened to 3 so the new default is
not against the stop, and the `SRCF_ENABLE` description now states
the detection limit.

`autotest`: `SRCFGPSSpoof` asserted only that a spoof was detected,
never by which detector. Under the old flat 0.8 velocity threshold
the mode 1 walk cleared `SRCF_VEL_THR` by 0.01 m/s; when
`SRCF_NSIGMA` raised that gate to 1.50 the subtest kept passing on
the position detector while its name still claimed the velocity one.
It now pins the thresholds it depends on, uses R=2.5 for the mode 1
phase so the walk outruns `SRCF_POSR_THR`, and asserts the peak vote
counters per phase.

That assertion needs `SRCF` in the log, which closes session-1 open
item 1: `configure_source_fallback_per_core` now sets
`LOG_FILE_RATEMAX=0`, because `SRCF` is written by `WriteStreaming`
at exactly 10 Hz and the suite-wide cap of 10 drops it entirely.

The vote peaks must be scoped to the current arm. One onboard log
spans several arm/disarm cycles and the counters reset on disarm, so
an unscoped scan reports the previous phase's peaks - which is
exactly how the first version of the assertion failed.

## Vehicle state at end of session

```
EK3_SRC2_YAW    = 1
EK3_OPTIONS     = 126
EK3_SRC_OPTIONS = 8
SRCF_ENABLE     = 1
SRCF_VEL_THR    = 1.6    (was 0.8)
SRCF_POSR_THR   = 1.9    (was 0.9)
SRCF_CNF_TIME   = 2.0
SRCF_RECOV_TIME = 10.0
SRCF_NSIGMA     = 2.5
```

`SRCF_CNF_TIME` was raised to 10 for logs 333 and 334 as a field
workaround while the thresholds were wrong, and returned to 2.0 for
logs 335 and 336. It should stay at 2.0.

## Still open

1. The new thresholds are validated on five flights from one airframe
   flown by one pilot. The benign envelope came from 5-6 m/s
   translation and hard manoeuvring; faster or more aggressive flying
   will push `VD` and `PR` up and there is only 11% of headroom. Every
   `SRCF_ENABLE=1` flight should keep adding to the `VSig`/`VD`/`PR`
   table.
2. The position-only spoof limit belongs in user-facing documentation,
   not just the parameter description. The feature should not be
   described as detecting GPS spoofing without qualification.
3. `SRCF_NSIGMA` = 2.5 is still not calibrated. It measurably helps
   (nine velocity false trips become one on the old thresholds) but
   the value itself remains a choice made from two SITL points.
4. Card 6, the AltHold demotion rung, was not flown. It needs
   `EK3_OPTIONS` bit6 cleared for that card only and remains
   SITL-covered.
5. Session-1 open item 3 stands: the pre-arm still only validates
   while `SRCF_ENABLE > 0`, and a lane whose yaw source is None is a
   latent arming blocker under per-core source sets regardless of
   SRCF.
