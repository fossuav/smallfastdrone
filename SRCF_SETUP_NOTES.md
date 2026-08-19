# SRCF setup notes - field tester guide

How to put the GPS/optical-flow source fallback (SRCF) on your
airframe: flashing the firmware, the required configuration, what has
to be recalibrated per airframe, and the staged flight program that
takes you from first hover to the detectors running armed.

SRCF runs two EKF lanes on different source sets - lane 0 on GPS,
lane 1 on optical flow - and moves the primary lane to flow when GPS
is lost or looks spoofed, without tripping the EKF failsafe. GPS loss
recovers automatically; a spoof detection latches GPS untrusted until
disarm or a pilot source-set change. It is field-validated on two
airframes (a small quad and a 1 m octaquad): detection in 0.2-0.3 s,
recovery in 10-12 s, repeatably. The detection thresholds are NOT
transferable between airframes or mission profiles - calibrating them
on yours is most of this document.

Hardware assumed: a downward optical flow sensor plus rangefinder
(both test airframes fly an ARK Flow MR on DroneCAN) and a GPS. Two
usable IMUs are required for the two lanes.

## 1. Firmware

Branch `SmallFastDrone-4.7.0-gps-optflow-fallback`, based on
ArduCopter 4.7.0.

```
./waf configure --board <your-board>     # e.g. MatekH743-bdshot
./waf copter
```

Flash `build/<your-board>/bin/arducopter.apj` with your GCS (Mission
Planner: Setup -> Install Firmware -> Load custom firmware), or
`./waf --targets bin/arducopter --upload` with the board on USB.

Save a full parameter file before flashing. Parameters survive the
flash (same parameter format), but the backup is your way home: to
back out, restore stock 4.7.0 firmware and that file.

### DFU flash with STM32CubeProgrammer

The .apj route needs the ArduPilot bootloader already on the board. A
blank board, a bricked one, or one arriving with another firmware's
bootloader gets flashed over USB DFU instead, using the combined
bootloader-plus-firmware image the build also produces:
`build/<your-board>/bin/arducopter_with_bl.hex`.

1. Enter DFU: hold the boot button (or bridge the BOOT0 pad) while
   plugging in USB. The board enumerates as "STM32 BOOTLOADER" or
   "DFU in FS Mode" - no COM port appears. If nothing shows up it is
   almost always the DFU driver or a charge-only cable.
2. In CubeProgrammer set the connection type to USB, refresh, pick
   the USB port, Connect.
3. Open `arducopter_with_bl.hex` under "Erasing & programming" and
   Download. The load address comes from the hex; leave it alone.
   "Verify programming" is worth the extra seconds.
4. Disconnect in CubeProgrammer, unplug, power up normally. First
   boot after a bootloader flash can take a few seconds longer.

Parameters usually survive - the hex does not cover the storage
sectors - but do not rely on it: if you had to do a full chip erase
they are gone, so check against your saved file either way.

## 2. Motor wiring

The reference octaquad build runs two 4-in-1 ESCs: one on outputs
S1-4, the other on S5-8. Getting the two connectors the right way
round matters - swapped banks put every motor in the wrong frame
position, which no configuration survives - and the same goes for the
motor order within each bank.

Props off, verify with Mission Planner's motor test (Setup ->
Optional Hardware -> Motor Test) before the first flight: each button
must spin the motor the OCTAQUAD X_REV diagram expects, in both
position and direction. The test letters run A, B, C... clockwise
from the front-right arm - diagram order, not output order - so do
not assume button A is S1. A motor spinning the wrong way is fixed in
ESC configuration or by swapping any two of its three phase wires,
not in ArduPilot parameters.

## 3. Required configuration

### The default parameter file

A default parameter set (`SRCF_defaults.param`, provided alongside
these notes) is the complete configuration of the reference X8
octaquad - everything in this section and section 4, plus the
airframe's tune and hardware setup. To use it:

1. Flash the SRCF firmware first (section 1). Stock firmware does
   not know the `SRCF_*` and fork `EK3_*` parameters and will
   silently drop them.
2. In Mission Planner: CONFIG -> Full Parameter List -> Compare
   Params, pick the file, review the diff, apply, Write Params,
   reboot the board.
3. Flight modes and switches are yours to arrange: set `FLTMODE1-6`
   and the `RCx_OPTION` channel assignments to suit your radio. Keep
   GPS Disable (65) and source-set select (90) on switches you can
   reach in flight - the test program uses both.

The file carries the donor vehicle's per-unit calibrations, so redo
your own accel and compass calibrations after loading (section 5
covers the compass), and run the section 5 checks - flow scalers
included - on your unit rather than assuming an identical build
measures identical.

The rest of this section is what the file sets and why, so that a
loaded parameter is a choice you can defend rather than a line in a
file.

The lane split, without which SRCF will not arm:

| parameter | value | why |
|---|---|---|
| `EK3_SRC_OPTIONS` | 8 | bit 3: per-core source sets - lane 0 follows SRC1, lane 1 follows SRC2 |
| `EK3_OPTIONS` | bit 1 set | manual lane switching; SRCF owns the lane choice |
| `EK3_IMU_MASK` | 3 | two lanes |
| `EK3_SRC1_POSXY/VELXY/VELZ` | 3/3/3 | GPS set |
| `EK3_SRC1_POSZ`, `EK3_SRC1_YAW` | as currently flown | typically 1 (baro) and 1 (compass) |
| `EK3_SRC2_POSXY` | 0 | flow lane is dead reckoning |
| `EK3_SRC2_VELXY` | 5 | optical flow |
| `EK3_SRC2_VELZ` | 0 | none |
| `EK3_SRC2_YAW` | 1 | 0 blocks arming |
| `FLOW_TYPE`, `RNGFND1_TYPE` | per hardware | ARK Flow on DroneCAN: 6 and 24 |
| `RCx_OPTION` | 65 | GPS Disable - the reversible field lever for loss testing |
| `RCx_OPTION` | 90 | EKF source set select - manual flow flying, and clears a spoof latch |

Both test airframes additionally fly `EK3_OPTIONS = 94` (bits 1, 2, 3,
4, 6), which adds the fork's flow-at-height and AGL-KF behaviour. Only
bit 1 is required by SRCF. Do not set bit 5 on the flow lane: with
`EK3_SRC2_VELZ = 0` it fuses terrain-relative vertical velocity
whenever flow is primary, which turns into terrain following.

Set `SRCF_ENABLE = 1` on the bench *before* the first SRCF flight and
try to arm: the pre-arm names any misconfiguration ("bad
EK3_SRC_OPTIONS/EK3_OPTIONS", "need 2 lanes in EK3_IMU_MASK", "need
SRC1 GPS, SRC2 flow", "set EK3_SRC2_YAW", "optical flow disabled").
Note the pre-arm only validates while `SRCF_ENABLE > 0`, so a
disabled-but-misconfigured setup stays silent.

On boot with the split active you should see:

```
EKF3 IMU0 is using GPS
EKF3 IMU1 fusing optical flow
EKF3 IMU1 started relative aiding
```

## 4. SRCF parameters

| parameter | default | validated values | meaning |
|---|---|---|---|
| `SRCF_ENABLE` | 0 | 1 | master enable; 2 also allows arming without GPS, see section 8 |
| `SRCF_VEL_THR` | 1.6 | 1.6 low-speed; 3.0 for 30 km/h | cross-lane velocity difference gate, m/s |
| `SRCF_POSR_THR` | 1.9 | 2.6 | cross-lane position growth rate gate, m/s |
| `SRCF_POSD_NSIG` | 0 (off) | unflown; start at 4 | position offset detector, in sigmas |
| `SRCF_CNF_TIME` | 2.0 | 2.0 | confirmation window; 20 votes at the 10 Hz monitor |
| `SRCF_RECOV_TIME` | 10.0 | 10.0 | GPS must be healthy this long before return |
| `SRCF_NSIGMA` | 2.5 | 2.5 (measured inert) | adaptive floor; fixed thresholds always win in practice |

The three detectors, and what each can and cannot see:

- `SRCF_VEL_THR` (velocity difference): catches a spoofer whose
  velocity moves. Its benign envelope scales with airspeed - at
  30 km/h it needs 3.0, at which the known velocity-consistent spoof
  (sustained VD 1.77 in SITL) becomes invisible. There is no fixed
  value that survives 30 km/h and catches that spoof.
- `SRCF_POSR_THR` (position growth rate): catches a spoofer whose
  reported velocity is consistent with its position walk. No speed
  trend observed; 2.6 gave a clean six-minute soak.
- `SRCF_POSD_NSIG` (position offset): speed-invariant, and the only
  one that sees a slow position-only walk (0.8 m/s in SITL, invisible
  to both rate detectors). The cost is latency - roughly 37 m of drag
  before it can fire, because an integrating detector cannot trip
  until the offset has built. Replay over eight flights is clean at
  2.5 and above; start at 4 (1.7x over the worst benign ratio) and
  tighten toward 3 only after soaking your own airframe. It has never
  flown enabled.

Do not copy the thresholds and consider it done. Section 6 tells you
how to measure your own envelope; the rule is at least ~30% margin
over the worst benign excursion, which is the margin that was one vote
from a false trip when it was missing.

Related but separate: with `FS_OPTIONS` bit 6, an RC failsafe with no
position estimate falls back to AltHold and drifts for `FS_ALTH_TMO`
(default 30 s) before landing.

## 5. What must be recalibrated on your airframe

Assumed done already: the standard build bring-up (accel, RC, ESC, a
basic tune). SRCF-specific, in order:

**Rangefinder first.** Everything downstream divides by height.
Validate against two witnesses in a GPS hover log: `RFND` vs `BARO`
altitude (slope ~1.0) and `d(RFND)/dt` vs GPS climb rate (slope
~1.0).

**Flow orientation and node rate** (`FLOW_ORIENT_YAW`). From any hover
log, no GPS needed:

```
python3 .claude/skills/log-analyze/flow_cal_check.py <log.bin>
```

The sensor-gyro vs FC-IMU slope must be +1.0 on both axes: 180 deg of
orientation error reads -1.0 on both, 90 deg swaps the axes, and a
slope well off 1.0 is the DroneCAN integration-interval fault, which
no scaler can fix (`FLOW_HF_RATEF` exists for that case). Fix
whatever it reports before touching scalers.

**Compass.** The flow lane's yaw source is the compass
(`EK3_SRC2_YAW = 1`), and while dead reckoning a yaw error rotates
flow velocity directly into position drift - the flow lane holds
position only as well as it holds heading. The flow scale calibration
below also rotates GPS velocity into the body frame by yaw, so do
this before it. Three steps:

1. The bench compass dance: standard onboard calibration, well away
   from vehicles and steel. Good enough to arm and fly, not good
   enough to stop there.
2. Fly figure-8s in ALT_HOLD for a couple of minutes with GPS lock,
   working through a full circle of headings with some throttle and
   speed variation so motor interference shows in the data. ALT_HOLD
   because it flies fine on a rough compass; LOITER leans on the
   heading you are calibrating.
3. Feed the log to the MAGFit web tool
   (firmware.ardupilot.org/Tools/WebTools/MAGFit). It fits offsets,
   iron correction, scale and motor compensation against the world
   magnetic model and checks orientation. Apply what it suggests,
   reboot, and confirm on the next flight that the mag innovations
   stay quiet (`XKF4.SM` comfortably below 1).

**Flow scale** (`FLOW_FXSCALER` / `FLOW_FYSCALER`). Not transferable -
the octaquad needed -57/-110 where the small quad flew -88/-148, on
the same sensor model. Calibration flight: outdoors with GPS, heading
held, deliberate forward runs then strafe runs at a few m/s, 5-9 m
AGL, a couple of minutes total. Run `flow_cal_check.py` on the log;
it prints per-axis ratios and suggested scalers, using raw rangefinder
height by default (do not switch it to the AGL KF height - the KF's
lag absorbs into the scaler and reads as a scale error). Fly the new
scalers once to confirm ratios near 1.0; if two flights disagree by a
few percent, split the difference.

**`EK3_FLOW_GAIN_H`.** The position-controller detune on flow scales
as `EK3_FLOW_GAIN_H / max(HAGL, EK3_FLOW_GAIN_H)`, so any value at or
above your operating height means no detune at all. 12 at 7-9 m gave
a sustained 2.4 s roll limit cycle; 4 halved roll sd and improved the
actual hold on both airframes. Set it well below operating height; 4
is validated at 6-8 m.

**`EK3_GLITCH_RAD`.** 25 unless you fly 50 m/s class (where 0 is
correct). At 0 a modest position jump on the flow-to-GPS return can
wedge the glitch logic for tens of seconds instead of resetting.

**Baro sanity.** Log a bench cool-down. One test airframe's baro sat
at 61 C and drifted 0.30 m in 78 s of cooling; that noise floor
contaminates every altitude measurement you will make.

## 6. Field program

One change per flight. Each stage gates the next.

1. **Bring-up hover.** ALT_HOLD, under 2 m, over flat ground, SRCF
   off. Afterwards: flow quality mean well above 50, `RFND` healthy
   in flight, orientation slopes +1.0.
2. **Compass figure-8.** A couple of minutes of ALT_HOLD figure-8s
   with GPS lock; run the log through MAGFit and apply (section 5).
3. **Flow calibration flight** (section 5). Then one confirm flight
   on the new scalers.
4. **Flow LOITER.** At 5-9 m, switch the source set to flow with the
   RC 90 switch, station-keep, switch back gently. Watch for the
   fast sway that means `EK3_FLOW_GAIN_H` is not detuning. When
   A/B-ing gains here, confirm in the log (`XKF4.AID = 2`, relative
   aiding) that the EKF was actually on flow for the window - one of
   our "flow tuning" flights turned out to be tuning GPS LOITER.
5. **SRCF soak.** `SRCF_ENABLE = 1`, default thresholds, normal
   flying including your target speeds, no GPS-loss cycling. The
   point is the benign envelope: afterwards read max `|VD|`, max
   `|PR|`, `PD` against `PSig`, and the peak `VVot`/`PVot`/`OVot`
   counts. Set your thresholds from these with ~30% margin. If a
   counter approached 20, the threshold it feeds was about to
   false-trip.
6. **GPS-loss ladder.** Low speed, 5-9 m AGL, comfortable arena.
   Flip GPS Disable (RC 65), watch the lane switch, hold position on
   flow for 15-30 s, restore, wait out recovery. Expect detection in
   ~0.25 s, recovery in `SRCF_RECOV_TIME` plus ~2 s, hands-off drift
   on flow of ~0.5-1 m over 20 s. Repeat several cycles. An
   `EKF_YAW_RESET` event at every lane switch is expected and benign
   - check the yaw step is a couple of degrees, not tens.
7. **Envelope expansion.** Repeat the soak at mission speed and
   re-read the envelope before trusting any threshold at that speed.
   Our 8-12 m/s band produced `|VD|` 2.17 and a `VVot` of 19/20
   against the 1.6 default - one vote from a latching false trip.
8. **Position offset detector.** Only after a clean soak with
   `SRCF_POSD_NSIG = 0`: read the flight's max `PD/PSig` ratio, then
   enable at 4 and repeat the soak before relying on it.

Not yet flown by anyone, deliberately left to arithmetic: a GPS-loss
cycle at cruise speed. A 3-5% residual flow scale error at 8.3 m/s is
15-25 m of drift per minute of outage. Measure it before the mission
depends on it.

### GCS messages in the field

| message | meaning |
|---|---|
| `SRCF: GPS lost, using flow lane` | loss detected, lane moved |
| `SRCF: GPS recovered` | recovery hold passed, back on GPS |
| `SRCF: GPS spoof suspected, using flow lane` | a detector confirmed; GPS latched untrusted |
| `SRCF: GPS returned Xm off, staying on flow` | GPS is back but disagrees with the flow lane's position |
| `SRCF: GPS trust reset` | latch cleared (disarm or pilot source-set change) |
| `SRCF: flow lost, back on GPS lane` | flow lane became unusable while primary |
| `SRCF: no nav source, AltHold` | final rung: neither lane has position |
| `SRCF: lane switch unavailable` | wanted to switch and could not |

### OSD lane panel

The fork adds an OSD panel showing which EKF lane is flying the
vehicle: set `OSDn_EKFLANE_EN = 1` and place it with the `_X`/`_Y`
positions. With the source sets from section 3 it reads `EKF0` on the
GPS lane and `EKF1` on flow (`EKF-` if no lane is reporting). The GCS
messages above flash once and are gone; this is the one persistent
indicator of what is navigating, so put it on your flight screen for
the loss ladder and the spoof work. It shows the lane, not a sensor
name, and does not flash on a non-zero lane - what a lane means is
set by your source sets, so know your own mapping. It is deliberately
not satellite count or HDOP: under a spoof both read healthy by
construction.

## 7. Reading the logs

The `SRCF` record, 10 Hz:

| field | meaning |
|---|---|
| `St` | 0 GPS primary, 1 flow after GPS loss, 2 flow after spoof |
| `GU` | GPS untrusted latch |
| `VD`, `PD`, `PR` | cross-lane velocity difference, position offset, offset growth rate |
| `VVot`, `PVot`, `OVot` | confirmation counters (velocity / position rate / position offset); a detector trips at 20 |
| `VSig`, `PSig` | combined 1-sigma velocity / position uncertainty of the two lanes |
| `GpsB`, `FlwU`, `GpsL` | GPS receiver loss confirmed; flow lane usable; GPS lane usable |

A healthy benign flight has all three counters at or near zero
throughout. The GPS-loss path runs on `GpsB`/`GpsL`, not the vote
counters, so loss handling keeps working however you set the spoof
thresholds.

## 8. Arming without GPS

`SRCF_ENABLE = 2` lets the vehicle arm on the flow lane when GPS is not
available - indoors, in a hangar, under cover - and take up the GPS lane
once a fix arrives in flight. At 1 the monitor always arms on the GPS
lane, which is the behaviour every field session so far has flown.

What happens, in order:

1. On the ground with no usable GPS lane and a usable flow lane, the
   monitor moves the primary to the flow lane after 2 s and says
   `SRCF: no GPS, arming on flow lane`. The arming GPS checks stand down
   because they are keyed on the primary lane's sources, so Loiter arms
   on flow-derived position.
2. You fly. Position is relative: hold is as good as your flow
   calibration, and drift is bounded by residual scale error times
   distance flown.
3. When the GPS lane acquires and holds position for `SRCF_RECOV_TIME`,
   the monitor takes it up and says `SRCF: GPS acquired, using GPS lane`.
   The flow lane is pulled into the GPS lane's position frame at the same
   moment, so `PD` and the spoof detectors mean afterwards what they mean
   on an ordinary flight.

What you do not have until step 3 completes:

- **No home**, so no RTL and no fence. Home is deliberately held off
  until the handover: the EKF origin appears about 11 s earlier, and home
  taken from the flow lane in that window is wrong by the distance flown
  since arming and is never revised. Your escape in that window is
  AltHold and your own eyes.
- **No absolute altitude reference** beyond baro.

Setting an origin before takeoff gives you home from the start.
`AHRS_OPTIONS` bit 4 (USE_RECORDED_ORIGIN_FOR_NONGPS) with
`AHRS_ORIGIN_LAT/LON/ALT` will do it, but understand what it is: bit 3
(RECORD_ORIGIN) saves the origin of the *last* flight, so if you have
driven to a new site it is stale and nothing warns you. A GCS
SET_GPS_GLOBAL_ORIGIN at the actual takeoff point is the accurate
version. Either way the handover itself is safe - the lane alignment
does not depend on the origin being right - but home and the fence do.

None of this has flown. It is SITL-validated only: `SRCFArmWithoutGPS`
arms with no GPS, takes off in Loiter on flow, acquires in flight, and
checks the lanes end up 1.5 m apart rather than the 46 m they sit at
without the alignment.

## 9. Known limits

- No fixed `SRCF_VEL_THR` both survives 30 km/h flight and catches a
  velocity-consistent spoof. At high cruise you are choosing between
  spoof coverage and false trips; the position offset detector is the
  current answer for slow walks, and it is late by design.
- `SRCF_NSIGMA` is inert on both test airframes - the fixed
  thresholds always win the max(). Leave it at 2.5.
- The flow lane dead-reckons position. Hold is sub-metre over tens of
  seconds when calibrated; over minutes, drift is bounded by your
  residual scale error times distance flown.
- The AltHold demotion rung (both lanes lost) has only been exercised
  in SITL.
- Flow near the ground is its own problem set (focus height, ground
  effect); do the low work in ALT_HOLD and the SRCF work at 5-9 m.
- Arming without GPS (`SRCF_ENABLE = 2`, section 8) has never flown. It
  is also the one part of SRCF whose takeoff happens at low height on
  flow alone, which is the regime the previous limit is about.
