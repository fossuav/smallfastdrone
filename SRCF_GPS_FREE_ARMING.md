# SRCF GPS-free arming - design note

Branch `SmallFastDrone-4.7.0-gps-optflow-fallback`. Written 2026-08-19
from code reading, implemented and measured in SITL the same day, then
bench tested on the small quad and extended on 2026-08-20. Numbers below
are SITL measurements unless they say otherwise, and were written before
any of it flew. It has since flown eight times indoors - see
`SRCF_FLIGHT_TEST_LOG_5.md` and the summary at the end of this file,
which is where this note's own reasoning gets checked against the air.

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

Measured: 46.3m, and stable rather than growing, which is the signature
of a fixed frame offset rather than of drift.

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

Amended after session 5. "No common frame" is true only where the
vehicle armed with no origin at all. Armed on a recorded origin or a
GCS `SET_GPS_GLOBAL_ORIGIN` both lanes are in a real earth frame from
the start and the offset is a real disagreement, so the handover now
applies `SRCF_RECOV_POS_NSIGMA` in that case and only that case. Field
log 346 took this handover on a GPS repeater indoors with the lanes
25.8 m and 10.6-14.7 sigma apart and flew into a wall 2.1 s later; see
`SRCF_FLIGHT_TEST_LOG_5.md`.

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

## What was built

`a96c9c0f41..dc246ea71a`, in the order above plus DAL and Replay support
for the alignment so a log still replays.

Two things came out of the build that the design did not anticipate.

**The alignment cannot be applied at the moment of the switch.**
`set_ekf_primary_lane` only records a request; `switchLane` runs on the
EKF's next update, so at the instant SRCF commands the handover the flow
lane is still primary and `alignLanePosition` refuses it - correctly,
because shifting the lane that is flying the vehicle would inject a
position jump with no reset reported. The alignment is therefore pending
until the commanded switch lands, which is inside the post-switch
detector mute either way.

**Home is set from the flow lane if nothing stops it.** The EKF origin
appears as soon as *either* lane sets one, and `update_home_from_EKF`
fires on the next loop. Measured: origin at t=56.0s, home at t=56.2s,
handover at t=67.3s, and home 30.7m from GPS truth and never revised.
Home is now held off until the handover. This was predicted as a risk in
the first draft, dismissed after a mismeasurement - ORGN Type 0 is the
EKF origin and Type 1 is home, not the other way round - and only found
by measuring the right record.

| | without | with |
|---|---|---|
| lane separation after handover | 46.3 m | 1.4-1.6 m |
| home error against GPS truth | 30.7 m | 0.0 m |

## The bench session

Flashed to the small quad with the OSD lane panel enabled. Three things
came out of it.

**The feature is invisible until `SRCF_ENABLE = 2`.** The first bench
test saw no ground lane switch at all, because the parameter was left at
1, which is the opt-in behaving exactly as designed - at 1 nothing about
the ground changes. It is the first thing to check, and it is worth
saying out loud because "I flashed the firmware" is not enough.

**The ground selection is very asymmetric.** Killing GPS on a switch
took about 12 s to move to the flow lane; restoring it moved back in
about 3 s. Reproduced and attributed in SITL:

| | GPS killed | GPS restored |
|---|---|---|
| lane 0 drops/regains `horiz_pos_abs` | 10.6 s | 1.0 s |
| SRCF debounce after that | 1.8 s | 1.8 s |
| total to lane switch | 12.4 s | 2.8 s |

The debounce contributes the same 1.8 s either way, so the asymmetry is
entirely the filter: `horiz_pos_abs` is `!posTimeout && PV_AidingMode ==
AID_ABSOLUTE`, and EKF3 dead reckons for about 10 s before conceding,
then re-acquires on one fix. None of it applies in flight, where the
ladder runs on receiver status and sessions 2 and 3 measured 0.20-0.26 s.
Worth keeping straight: the two look identical from the OSD.

**Nothing on the OSD changed during those 12 s**, which is what the next
section is about.

## Position type on the OSD

The lane number says which sensors are navigating but not what the answer
is worth, and a lane coasting on a dead GPS looked exactly like a healthy
one. The panel now reads `EKF<n> <TYPE>`:

| | |
|---|---|
| `ABS` | absolute, fusing its source |
| `CST` | absolute but coasting - no GPS fusion for 4 s, running on the last fix |
| `REL` | relative only, drifts without bound. The flow lane |
| `DRK` | wind or drag relative |
| `NON` | no horizontal position |

`DEAD_RECKONING` is the wrong flag for this and it is worth recording why,
because it is the obvious one to reach for. EKF3 sets it as
`(PV_AidingMode != AID_NONE) && doingWindRelNav && !((doingFlowNav &&
gndOffsetValid) || doingNormalGpsNav || doingBodyVelNav)`
(`AP_NavEKF3_Control.cpp:850`) - it is wind or drag relative navigation
and is explicitly cleared whenever flow is navigating, and
`doingWindRelNav` needs `assume_zero_sideslip()`, false on a copter. A
field driven by it would read blank for the entire flow-lane flight. The
state that matters is `horiz_pos_rel && !horiz_pos_abs`.

`CST` is what fills the 12 s gap. `using_gps` clears 4 s after the last
GPS fusion (`AP_NavEKF3_Control.cpp:842`) against `posTimeout` for
`horiz_pos_abs`, so measured against a kill at t=0:

| t+ | |
|---|---|
| 4.6 s | `CST` |
| 10.6 s | `horiz_pos_abs` drops |
| 12.4 s | lane switch, `REL` |

so the panel reacts at 4.6 s where it used to react at 12.4 s. It stays
`ABS` until then, which is honest rather than a compromise - the position
really is still built on a fix a few seconds old.

Gated on `ahrs.using_gps()`, the primary lane's *configured* sources,
else a beacon or extnav lane - absolute position, never uses GPS - reads
as coasting for its whole flight. Flashes on `CST` and `NON`, where the
position on screen is not being held up by a source, and not on `REL`,
where a flow lane is working as configured.

## Test status

`SRCFArmWithoutGPS` covers the whole path: no GPS at boot, arm and take
off in Loiter on the flow lane, translate, acquire GPS in flight, assert
the handover, the lane separation afterwards and the home error. It
asserts the separation rather than the statustexts, because without the
alignment every message still arrives and only the frames are wrong.

`SRCFFirstFixOffsetBound` covers the case session 5 flew: a recorded
origin, arming without GPS, and acquisition on a static capture 100 m
out. It asserts the refusal, the warning and that the vehicle holds
station, then clears the spoof in the same flight and asserts the
handover completes - a bound that had simply disabled the handover
would pass the first half on its own.

`SRCFGroundLaneFollowsGPS` covers the bench case that
`SRCFArmWithoutGPS` does not: GPS present at boot, killed on the ground,
restored. Loose timeouts on purpose - it asserts the direction of travel,
not the timing.

`SRCFCoastingShown` asserts the *lead time* of `CST` rather than that it
occurs, because the whole value is that it appears while the lane number
still says nothing; a version that only fired alongside the lane switch
would pass a weaker test and be useless. It reads `XKF4.SS`, since
EKF_STATUS_REPORT does not carry `using_gps`.

There is no way to read the OSD panel back without an SFML build, so both
OSD assertions pin the panel's inputs rather than its output.

Green alongside the existing suite: SRCFGPSLossLadder, SRCFGPSSpoof,
SRCFSlowSpoofPositionOffset, SRCFStaticSpoofNoRecovery,
SRCFDisabledRegression, SRCFRCFailsafeDrift, SRCFBrakeNavLossDemote,
EKF3SRCPerCore, OpticalFlow.

## What flying it still needs

Session 5 flew it once, indoors, and crashed: the handover was taken on
a GPS repeater's fix 26 m from truth and Loiter chased it into a wall.
That is what the offset bound above is for; the bound itself has not
flown. `SRCF_FLIGHT_TEST_LOG_5.md` has the flight. The rest of this
section is unchanged and still stands.

Everything sessions 1-4 measured was 5-9 m over textured ground with
GPS present.
Indoor flow at 1-2 m is the regime session 3 listed as unresolved (focus
height, ground effect), and `EK3_FLOW_GAIN_H = 4` gives
`gainHgt / max(HAGL, gainHgt) = 1.0` below 4 m, so there is no detune at
all - the configuration that produced the 2.4 s roll limit cycle at 7-9 m
with gain 12. Expect to have to measure a low-altitude value.

`EK3_GLITCH_RAD` matters here too. Session 3's log 48 had a flow-to-GPS
return wedged for 18.8 s at 0; 25 is the settled value on both airframes
and this manoeuvre is exactly that transition.

## Open questions

0. A lane that has not yet set its own origin still adds
   `public_origin.get_distance_NE_postype(EKF_origin)` in `getPosNE`
   (`AP_NavEKF3_Outputs.cpp:280`) with `EKF_origin` at (0,0). Once
   another lane makes the common origin valid, that term is the distance
   from the site to Null Island. **Not reproduced**: in SITL the two
   lanes set their origins 0.2s apart and nothing measurable appeared,
   so this is a code path, not a demonstrated bug. It is more exposed on
   a real vehicle, where `calcGpsGoodToAlign` can differ per lane on the
   per-core yaw and mag test ratios - which is exactly what session 4's
   eudrone showed. No guard has been added for something unproven.
1. The ground lane decision still has no log trace. `source_fallback_update`
   returns from the disarmed branch before the logging block, which did not
   matter until the ground behaviour became a feature. An attempt to log
   `SRCF` while disarmed was reverted: the call site provably executes - the
   arming statustext is sent on the line before it - but neither
   `WriteStreaming` nor plain `Write` produced a record, with
   `LOG_FILE_RATEMAX` and `LOG_DARM_RATEMAX` both 0, while the same helper
   logs normally once armed. `ShouldLog` and the rate limiter both read as
   though they should pass. Unexplained, so nothing was shipped. Until it is
   solved, a lane that refuses to move says nothing in the log about which
   gate held, and the OSD position type is the only diagnosis available.
2. What is the pilot's escape between arming and the first fix? AltHold
   plus manual flying is the honest answer today, and it should be
   written down rather than assumed. `CST` and `REL` at least tell the
   pilot which world they are in, which they did not before.
3. Should the first-fix switch be automatic at all, or should it be
   announced and left to the pilot? Automatic matches the rest of SRCF;
   a several-hundred-metre position reset mid-flight is a bigger event
   than anything the ladder does today. Session 5 is the case for
   leaving it to the pilot: the offset bound now refuses the fixes it
   can prove are wrong, but where it passes, the pilot still gets 2 s
   to notice a bad handover from inside the aircraft.
4. How is a stale recorded origin detected? Nothing in the parameters
   carries provenance or age. A pre-arm that compares the recorded
   origin against the last known GPS position would catch the drive-to-a-
   new-site case, but only if there was a fix at some point.

   Session 5 made this urgent and then worked around it. `RECORD_ORIGIN`
   rewrote the origin on three consecutive flights, once from a flight
   flown under a GPS repeater, ending 39.0 m horizontal and 51.2 m
   vertical from the takeoff point - and with the offset bound in place
   that refuses an honest handover rather than merely misplacing home.
   The workaround is to clear bit 3 and pin the origin by hand. A check
   would still be better: on the ground in log 348 the receiver reported
   144-153 m altitude against a recorded origin of 89.3, four seconds
   before it armed, and nothing compared the two.
5. Does the flow lane need a bound on how far it may dead reckon before
   the first fix? Session 3 measured 0.6 m over 102 s at 0.006 m/s in
   the calibrated case, but indoors at low height with a fresh
   calibration is not that case.

## What session 5 settled

Eight indoor flights, all under a GPS repeater, in
`SRCF_FLIGHT_TEST_LOG_5.md`.

The first-fix handover does need a bound, and it is the offset one. Log
346 took a fix 26 m out at 10.6-14.7 sigma and flew into a wall 2.1 s
later; that is exactly what the bound refuses, and it postdates the
flight. Since it went in, three fixes have been refused and three
accepted, and the accepted ones flew normally.

The origin has to be accurate for the bound to mean anything, and
`RECORD_ORIGIN` is hostile to that - see open question 4.

Nothing the receiver reports about itself can substitute. Satellite
count, HDOP, `HAcc`, `VAcc`, speed accuracy, `EK3_CHECK_SCALE` and
`gpsGoodToAlign` were each measured; each passes a fix whose position is
wrong. Neither can velocity divergence, displacement between the lanes,
altitude consistency, the receiver against itself, or the GPS lane's own
position innovation - all tried, all overlapping.

A bad fix is only visible when the vehicle moves. Static, it is
indistinguishable from a good one - log 355 held `PD` at 0.10-0.54 m for
thirty seconds after its handover and then the same fix understated a
1 m/s translation threefold. That is why the handover, which this note
places at the end of a hover, is the hardest possible moment to judge a
fix, and why the offset bound rather than any quality test is what
guards it.

Detecting faster does not help the case that crashed. The cross-lane
velocity difference was 0.65 at impact and needed about 4 s to build
against a 2.1 s failure, and the commanded switch itself lands in 4.4 ms.
Confirmation time and the post-switch mute are both irrelevant to it.

`EK3_OPTIONS` bit 0 was inert under per-core source sets and now is not:
it asked whether the GPS lane could dead reckon, which needs flow on the
same core, where the question is whether the vehicle can wait.
