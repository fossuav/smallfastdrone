# SRCF GPS-free arming - design note

Branch `SmallFastDrone-4.7.0-gps-optflow-fallback`. Written 2026-08-19,
from code reading only. Nothing here has been built, simulated or
flown.

Goal: arm indoors with no GPS fix, fly out of the door, and have the
GPS lane take over when it acquires - the reverse of the ladder SRCF
already flies. Sessions 1-4 all began outdoors on the GPS lane and used
flow as the fallback; this begins on flow and treats GPS as the
arrival.

The blocking arming checks turn out to be lane-keyed already, so the
pre-arm side is mostly free. The work is elsewhere: the two lanes end
up in different position frames, and every SRCF gate that compares them
is then meaningless.

## What blocks arming today

In Loiter, or any mode requiring position, or with a circle/polygon
fence enabled:

`AP_Arming_Copter.cpp:405` computes

    mode_requires_gps = AP::ahrs().using_gps() && mode_requires_position

and `using_gps()` resolves through `NavEKF3::using_gps()` to
`sources.usingGPS(primary)` (`AP_NavEKF_Source.cpp:332`), which reads
the primary lane's **configured** sources and never looks at the fix.
Lane 0 runs SRC1 with POSXY=GPS, so it is unconditionally true. The
parent check then runs and fails on "GPS 1: Bad fix"
(`AP_Arming.cpp:741`) and "AHRS: waiting for home"
(`AP_Arming.cpp:752`). Separately `position_ok()` fails, because lane 0
sitting in AID_NONE reports no `PRED_HORIZ_POS_*`.

And the primary cannot be anything but lane 0: `source_fallback.cpp:108-119`
calls `set_ekf_primary_lane(SRCF_GPS_LANE)` on every disarmed tick.

In AltHold with no fence, arming already works - `mode_requires_position`
is false and nothing else asks for a position estimate. That is stock
behaviour, not an SRCF feature.

## What is already lane-aware, and costs nothing

Because `using_gps()` keys on the primary lane, putting the primary on
lane 1 while disarmed makes the entire GPS arming block evaporate with
no change to `AP_Arming`. With SRC2 at POSXY=0, VELXY=5, YAW=1,
`usingGPS(1)` is false.

`Copter::ekf_has_relative_position()` (`system.cpp:262`) accepts
`PRED_HORIZ_POS_REL` from the primary while disarmed, which lane 1 has:
`readyToUseOptFlow()` (`AP_NavEKF3_Control.cpp:571`) has no on-ground
gate, so the flow lane enters AID_RELATIVE on the bench. So
`position_ok()` passes and Loiter arming passes.

Commanding the lane on the ground also already works. Under
ManualLaneSwitch, `requestLaneSwitch` takes effect on the next
`UpdateFilter` (`AP_NavEKF3.cpp:1005-1012`), armed or not, and the
disarmed force-to-`EK3_PRIMARY` path at `AP_NavEKF3.cpp:1068-1078`
follows `requested_lane_override` rather than fighting it.

So the pilot-visible half of the user's request - "make the GPS checks
apply only to the current lane" - is a consequence of the lane
selection, not a separate change.

## The position frame problem

This is the part that needs real work.

Lane 1 never leaves AID_RELATIVE. `readyToUseGPS()` returns false for it
at `AP_NavEKF3_Control.cpp:616` because SRC2_POSXY is NONE, so the
AID_RELATIVE case at `:321` can never promote it. Its position states
are relative to wherever it started relative aiding, which is the
indoor spot.

Lane 0, when GPS first goes good outdoors, sets the origin at that
moment and at that place (`AP_NavEKF3_Measurements.cpp:691-700`, which
even corrects the origin altitude for the height already gained), then
`ResetPosition()` snaps its states to the GPS position relative to that
origin (`AP_NavEKF3_PosVelFusion.cpp:130-140`).

Both lanes report through the common `public_origin` frame
(`getPosNE`, `AP_NavEKF3_Outputs.cpp:280`), so at that instant

    pos_div = horizontal distance flown since the flow lane started aiding

Consequences, in order of severity:

- `SRCF_RECOV_POS_NSIGMA` rejects the handover. The vehicle prints
  `SRCF: GPS returned 250m off, staying on flow` and never switches.
  That bound was built in session 2 to catch a static capture; here it
  fires on an honest receiver.
- The offset does not go away. Every later fallback to flow injects the
  same step into `PD`, so the frames have to be reconciled, not
  tolerated once.
- `pos_rate` spikes across the step, which delays recovery for the 2 s
  rate window even if the bound is relaxed.

The spoof detectors themselves are safe: `can_vote` requires
`primary == SRCF_GPS_LANE`, so nothing votes while flying on flow, and
`SRCF_POST_SWITCH_MUTE_MS` covers the 5 s after the switch.

The lurch is also already handled - `switchLane` reports the delta as a
position reset (`AP_NavEKF3.cpp:1136`) and the position controller
absorbs it. The problem is the gates, not the ride.

## Does this need an origin anyway?

Three separate questions, with three different answers.

**Flying does not need an origin.** `getPosNE` returns true in
AID_RELATIVE without `validOrigin` (`AP_NavEKF3_Outputs.cpp:277-281`),
and `get_relative_position_NED_origin` (`AP_AHRS.cpp:1798-1806`) needs
nothing more. Loiter and the position controller run on relative
position. A flow-only box indoors with no origin at all is flyable.

**Home needs an origin.** `Copter::set_home` refuses without one
(`commands.cpp:59-63`). No home means no RTL (`mode_rtl.cpp:84`), no
fence centre, and altitude-above-home is meaningless
(`inertia.cpp:27`). If the answer to "what is the escape between arming
indoors and GPS arriving" is to be anything other than AltHold and the
pilot, an origin has to exist before takeoff. That is the real argument
for `USE_RECORDED_ORIGIN_FOR_NONGPS`
(`AP_AHRS.h:1117`, `AP_AHRS.cpp:1540`), and it is a good one.

**An origin does not fix the frame problem. An accurate origin does.**
This is the trap. The recorded origin is written by `RECORD_ORIGIN`
from a previous flight (`AP_AHRS.cpp:1527`), so it is stale by
construction. With a stale datum, lane 1 dead reckons from it while
lane 0, on first fix, resets to `EKF_origin.get_distance_NE(gpsloc)` -
the true offset from that same stale datum. Lane 0 is then absolutely
correct and lane 1 is wrong by exactly the staleness, and `pos_div` is
the staleness. Drive 5 km to a new site and the failure is reproduced
with a 5 km offset instead of a distance-flown one. Nothing warns: the
parameters are populated and plausible.

So use the recorded origin, but for what it actually buys. Accurate by
construction is either a GCS `SET_GPS_GLOBAL_ORIGIN` at the takeoff
point, or a site you always fly from. Neither is guaranteed by a
parameter, which is why the lane alignment below is the fix and the
recorded origin is the convenience.

One ordering hazard worth naming: `use_recorded_origin_maybe` early-returns
on `using_gps_for_pos()` (`AP_AHRS.cpp:1553`), which is also
primary-keyed. It therefore fires only once SRCF has already moved the
primary to the flow lane. That works, but it is an accident of ordering
rather than a decision, and it should be made explicit.

Altitude is the minor case: `ekfGpsRefHgt` is seeded from the origin
altitude (`AP_NavEKF3_Control.cpp:746`) but is corrected continuously
(`AP_NavEKF3_Measurements.cpp:857`), and with POSZ=BARO on both lanes
GPS height is not fused at all. A wrong recorded altitude is not a
flight-safety item here.

Note that `RECORD_ORIGIN` and `USE_RECORDED_ORIGIN_FOR_NONGPS` are
fork-only (`d8a9394aac`); they are not in ArduPilot main. That matters
if any of this is ever proposed upstream.

## Design

### 1. Ground lane selection

Replace the forced lane 0 in the disarmed branch of
`source_fallback_update()` with a debounced choice: the GPS lane when
`gps_lane_usable`, the flow lane otherwise. The lane status fetch has to
move above the disarmed early-return, which currently returns before it.

Debounce is not optional. A marginal indoor fix that comes and goes will
otherwise flap the primary, and `switchLane` announces every change at
MAV_SEVERITY_CRITICAL. Reuse the `gps_bad_count` idiom, or settle the
choice once and hold it until the state actually changes for several
seconds.

### 2. A state for "no absolute position yet"

`FLOW_LOSS` is the wrong path to reuse. Its recovery gates compare two
lanes that have no common frame, so they cannot mean anything before
the first fix. Add a state whose exit condition is only that the GPS
lane reaches `horiz_pos_abs` and holds it - no cross-lane consistency,
no offset bound. After the switch the machine is exactly today's.

### 3. Align the flow lane at the first fix

A `NavEKF3::alignLanePosition(lane, reference_lane)` that offsets
`stateStruct.position.xy()`, `outputDataNew`, `outputDataDelayed` and
the `storedOutput` ring. That is mechanically what `moveEKFOrigin()`
already does (`AP_NavEKF3_core.cpp:2290-2312`), so the shape is proven
and the accessor follows the pattern of the sigma accessors added in
sessions 1 and 2: expose in `AP_NavEKF3`, pass through `AP_AHRS`, call
from `Copter`.

Call it on the flow lane immediately **after** SRCF switches to the GPS
lane, never while the flow lane is primary. Aligning a non-primary lane
is invisible to control, and it leaves `pos_div` at the honest
flow-drift figure, which is what every existing gate was calibrated
against.

### 4. Opt-in and annunciation

This changes arming behaviour, so it must not ride on `SRCF_ENABLE=1`.
`SRCF_ENABLE=2` is the cheapest fit - everything is gated on `> 0`
already - with the parameter's `@Values` extended.

The pilot needs to know which world they are in, at arming and not only
in the log:

- something at arming that names the lane and the absence of GPS
- a warning when the origin in use came from the recorded parameters,
  since a wrong home is silent otherwise
- the `EKFLANE` OSD panel from session 2 already covers the in-flight
  state

### 5. Home

`Copter::update_home_from_EKF()` (`commands.cpp:4`) already sets home in
flight once origin and location exist, and `AP_Arming_Copter.cpp:770`
already has the "home will be set later" branch. Nothing new is needed
in that path. What is needed is a decision about the window between
arming and the first fix, during which there is no home, no RTL and no
fence. The AltHold rung and the session-2 RC-failsafe drift path
(`FS_OPTIONS` bit 6, `FS_ALTH_TMO`) are the only escapes, and the drift
rung is still SITL-only.

## Work estimate

| area | change | rough size |
|---|---|---|
| `ArduCopter/source_fallback.cpp` | ground selection, new state, first-acquisition path | ~100 lines |
| `AP_NavEKF3`, `AP_AHRS` | `alignLanePosition` and pass-through | ~40 lines |
| `ArduCopter` pre-arm and parameters | `SRCF_ENABLE=2`, statustexts | ~20 lines |
| `Tools/autotest` | two tests plus registration | ~150 lines |
| docs | setup notes section, session 5 plan | - |

One commit per module, as usual.

## Test plan

SITL first, and it is genuinely testable there:

- `SRCFArmWithoutGPS`: boot with `SIM_GPS1_ENABLE=0`, arm in Loiter on
  the flow lane, take off, enable GPS, assert the lane switch, that home
  gets set, and that `PD` is small **after** the switch rather than
  before it. The last assertion is the one that fails without the
  alignment.
- A regression that with GPS present at boot nothing changes: primary
  lane 0, no new statustexts, `SRCFGPSLossLadder` unaffected.
- `wait_ready_to_arm` needs a no-GPS variant. Session 2 already
  established the SIMSTATE and dfreader workarounds for asserting
  without a fix.

Field, and this is where the honest gaps are. Everything sessions 1-4
measured was 5-9 m over textured ground with GPS present. Indoor flow at
1-2 m is the regime session 3 listed as unresolved (focus height, ground
effect), and `EK3_FLOW_GAIN_H = 4` gives
`gainHgt / max(HAGL, gainHgt) = 1.0` below 4 m, so there is no detune at
all - the configuration that produced the 2.4 s roll limit cycle at 7-9 m
with gain 12. Expect to have to measure a low-altitude value.

`EK3_GLITCH_RAD` matters here too. Session 3's log 48 had a flow-to-GPS
return wedged for 18.8 s at 0; 25 is the settled value on both airframes
and this manoeuvre is exactly that transition.

## Open questions

1. What is the pilot's escape between arming and the first fix? AltHold
   plus manual flying is the honest answer today, and it should be
   written down rather than assumed.
2. Should the first-fix switch be automatic at all, or should it be
   announced and left to the pilot? Automatic matches the rest of SRCF;
   a several-hundred-metre position reset mid-flight is a bigger event
   than anything the ladder does today.
3. How is a stale recorded origin detected? Nothing in the parameters
   carries provenance or age. A pre-arm that compares the recorded
   origin against the last known GPS position would catch the drive-to-a-
   new-site case, but only if there was a fix at some point.
4. Does the flow lane need a bound on how far it may dead reckon before
   the first fix? Session 3 measured 0.6 m over 102 s at 0.006 m/s in
   the calibrated case, but indoors at low height with a fresh
   calibration is not that case.
