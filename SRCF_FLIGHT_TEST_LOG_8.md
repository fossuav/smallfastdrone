# SRCF field test log - session 8

Branch `SmallFastDrone-4.7.0-gps-optflow-fallback`. Vehicle
SmallFastDronev1, log 2, 2026-09-05 11:32 UTC, flown against `15c477c3`.
The second acro sortie of the same day as log 356, an hour after it, on
the same airframe and the same build.

Outcome: the flight session 6 asked for and did not have. Log 2 flew acro
on a **correct origin** with the lanes properly aligned and a 31-satellite
fix, and the velocity detector still latched a false spoof 64 s in. That
settles session 6's open item 1 - SRCF as shipped false-trips on ordinary
acro against about the best GPS in the whole log set - and it is the first
field evidence that the session 6 witness gate would have prevented it.

It also answers the operator's question about what stops the flow lane
aiding, which turned out to be the thing session 7 assumed it was not, and
withdrew an explanation session 7 had given.

## The natural experiment on the stale origin

`AHRS_ORIGIN_LAT/LON/ALT` still read 35.18744, -79.37141, 139.5 in this
log - the North Carolina datum, unchanged from log 356 an hour earlier.
The origin actually used was 51.7095377, -0.636653, the true site, set
from GPS at 17.3 s.

So the same wrong parameter was harmless here and catastrophic there,
which is exactly the mechanism session 6 derived and this is the control
for it. GPS was usable at boot, so `use_recorded_origin_maybe`
early-returned on `using_gps_for_pos()` and the recorded datum was never
adopted. In log 356 the pilot had GPS disabled at boot, SRCF moved the
primary to the flow lane first, and that is the only door the stale value
can come through. Two sorties, one day, one parameter, opposite outcomes.

The alignment fix committed in session 6 addresses the other half of that
and is unrelated to which origin gets adopted; neither flight was flown
with it.

## A false spoof trip on a 31-satellite fix

```
 97.1  ACRO
161.1  SRCF: GPS spoof suspected, using flow lane
161.1  EKF3 lane switch 1
```

The vote, at `SRCF_VEL_THR` 1.6 and `SRCF_CNF_TIME` 1.5:

| | |
|---|---|
| detector | velocity; `VVot` 5 to 14 then latch |
| `PVot` / `OVot` | 0 throughout |
| `VD` | 2.06-2.14 |
| `PD` | 21.5-23.3 m, real dead-reckoning drift |

And the receiver over the same window:

| | |
|---|---|
| satellites | **31-32** |
| HDop | 0.46-0.48 |
| `HAcc` | 0.42-0.43 m |
| `SAcc` | 0.07-0.15 m/s |

That is the best fix anywhere in this record, better than the open-sky
flights of session 2. The GPS lane was not the faulty term; the flow lane
was, and the monitor moved the vehicle onto it. A false trip, on the
configuration the feature is meant to fly.

`PD` at 21-23 m is ordinary flow-lane drift for 88 s of flight at
session 7's rule of thumb, not a frame error, so the lanes really were
aligned. This is the aligned-lane acro sortie session 6 said was the
gating measurement.

### The witness gate would have blocked it

Across all 31 rangefinder samples in the 1.5 s vote window, `RFND.Stat`
was 1 - NoData, holding a stale 16.84 m - so `rangefinder_alt_ok()` is
false for 0 of 31. `flow_is_witness` is therefore false for the whole
window and not one of those fifteen votes would have been cast.

Derived from the gate's logged input rather than replayed: SRCF is a
vehicle-level function and Replay only runs the EKF, so this is a direct
reading of what the gate consumes, not a demonstration of what it emits.
An autotest already covers the emission; what was missing was a real
flight where the input said block, and this is one.

Session 6 shipped that gate as "necessary and measured insufficient",
with the insufficiency measured on log 356. That still stands - it does
not close the acro case in general - but on the one field false trip
available it is sufficient.

## What gates the flow lane's aiding

The operator's question, and it matters because session 7 swept
`DCM33FlowMin` and would have been arguing in a circle if lean were the
answer: relaxing a constant that itself causes the aiding drops, then
reading the drops as evidence about the constant.

The demote is `flowFusionTimeout` - 5 s without `FuseOptFlow` reaching
its update (`AP_NavEKF3_Control.cpp:313`) - and fusion needs
`flowDataToFuse && tiltOK` (`AP_NavEKF3_OptFlowFusion.cpp:85`). So lean
could have been it.

State over the 5 s of no-fusion before each drop:

| | log 2 | log 356 |
|---|---|---|
| aiding drops | 4 | 11 |
| `tiltOK` share | 0% | 1% |
| **rangefinder returning** | **0%** | **0%** |
| `NI` innovation rejections | 1 to 20 | 1 to 255 |

Both conditions are false together on both flights, so the state alone
cannot separate them. The sweep can. Counting the replayed EKF's own
aiding drops per swept value:

| `DCM33FlowMin` | drops, log 356 |
|---|---|
| 0.71 stock | 11 |
| 0.62 | 11 |
| 0.50 | 11 |
| the flight itself | 11 |

Invariant, and matching the flight - which is a tighter reproduction
check on the replay than the velocity one session 7 used. Pre-drop `c.z`
runs p50 0.61 on log 2 and 0.55 on log 356, hugging the gate rather than
far beyond it, so 0.50 would have admitted 89% and 62% of those samples
respectively. It admitted them and the lane still timed out.

**So the height reference gates the aiding, not lean.** With no range
data the terrain state is inhibited and freezes, the predicted flow drifts
away from the measured as the real height changes, the innovation gate
rejects - that is the `NI` climb - and `prevFlowFuseTime_ms` goes stale
whatever the tilt gate says. Lean merely co-occurs, because on this
profile the vehicle is banked when it is fast and fast when it is high.

Two consequences. Session 7's sweep is **not** circular, which is the
question that was asked. And session 7's explanation of why the sweep
looked chaotic - that the eleven restarts made drift chaotic by
construction - is **withdrawn**, because the restart count does not move.
The chaos is real and now unexplained; it comes from which 45-60 degree
samples get fused during the rest of the flight.

## Still open

1. **The witness gate has one field data point and it is favourable.**
   Log 2's false trip would have been blocked. That is one flight, on one
   airframe, and it does not touch the case session 7 measured where the
   rangefinder is in range and the tilt is high.
2. **The acro false trip is now a measured field failure, not a
   prediction.** Any build flown in acro with `SRCF_ENABLE > 0` and
   without the witness gate should be expected to latch. The gate is
   committed; nothing has flown with it.
3. **Why the tilt sweep is chaotic is unknown.** The reset explanation is
   gone and nothing replaces it. It does not change the recommendation -
   leave `DCM33FlowMin` alone - but the record should not pretend to a
   mechanism it does not have.
4. **The stale `AHRS_ORIGIN_*` is still set on this airframe.** Harmless
   on any flight that has GPS at boot, as this one shows, and the trigger
   for log 356. Fix it on the vehicle rather than relying on the boot
   order.
5. Session 7 items 1-6 stand.

### Settled this session

- SRCF false-trips in acro on a correct origin and an excellent fix. Not
  inferred from log 356's frame error any more; measured on 31 satellites
  at 0.42 m.
- The flow lane's aiding is gated by the missing height reference, and
  the tilt gate is not the binding term. Measured by invariance across
  the sweep rather than argued from the state, because the state cannot
  separate them.
- A wrong recorded origin is inert whenever GPS is usable before the
  ground lane selection runs. Same parameter, same day, same airframe,
  opposite outcomes an hour apart.
