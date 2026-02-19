# logtd_73 Analysis — TD-Matek-5 log73.bin

## Metadata
- **Date**: 2026-02-19
- **Vehicle**: TD-Matek-5 (MatekH743-bdshot)
- **Firmware**: V4.6.3v2-SFD (21057ce9) — SmallFastDrone-4.6-AltHoldv2 branch
- **Log file**: ./log73.bin
- **Frame**: QUAD/X
- **Sensors**: GPS (u-blox SAM-M10Q, 3D fix, ~18 sats), baro, compass, rangefinder (CAN), optical flow
- **Mode**: Stabilize (mode 0) throughout
- **One flight**: ARM 17.2s → DISARM 91.7s (~74s flight)
- **Purpose**: Testing EKF bootstrap reset aux switch (RC15_OPTION=187)

## Summary

**Pilot toggled the EKF reset switch 7 times during a Stabilize flight. Each toggle triggered
a full EKF reinitialization with altitude estimate discontinuities up to 3.2m. All resets
reported "failed" because the IMU delay buffer can't refill instantly. The flight was
unaffected because Stabilize mode doesn't use EKF altitude for control.**

## Key Configuration

```
# EKF Reset Switch
RC15_OPTION = 187    (EKF_RESET aux function)

# VRFB at boot (carried from log72)
INS_ACC_VRFB_Z  = -0.345
INS_ACC2_VRFB_Z = -0.131

# Same as log72
EK3_GND_EFF_DZ   = -8
EK3_RNG_USE_HGT  = 3
PSC_ACCZ_P       = 0.138
PSC_ACCZ_I       = 0.277
MOT_THST_HOVER   = 0.157
TKOFF_GNDEFF_TMO = 1.5
ACC_ZBIAS_LEARN  = 3
```

## Flight Timeline

| Time (s) | Event |
|-----------|-------|
| 3.2 | Boot — V4.6.3v2-SFD (21057ce9) |
| 4.0 | VRFB loaded: IMU0=-0.345, IMU1=-0.131 |
| 15.4 | EKF3 IMU0/IMU1 initialised, MAG aligned |
| 17.0 | Tilt alignment complete |
| **17.2** | **ARMED** (Stabilize) |
| 17.8 | Throttle raised, RC3 reaches 1175 |
| 18.3 | BAlt=-8.11 (ground effect peak) |
| 19.5 | Liftoff, climbing |
| 21.5 | Alt=1.5m, hover |
| **25.0** | **EKF reset #1** — PD: -1.74→-1.68, errRP→0.1414 |
| **34.9** | **EKF reset #2** — PD: -1.25→-1.25, VD zeroed |
| 45.1 | EKF3 origin set (first GPS origin) |
| **48.1** | **EKF reset #3** — PD: -0.62→-0.57 |
| 58.3 | EKF3 origin set (second) |
| **58.4** | **EKF reset #4** — PD: -2.92→+0.28 (**3.2m jump**) |
| 68.5 | EKF3 origin set (third) |
| **74.0** | **EKF reset #5** — PD: -3.17→-1.07 (2.1m jump) |
| **82.3** | **EKF reset #6** — PD: -0.68→-0.81 |
| **85.6** | **EKF reset #7** — PD: -1.79→-1.15 (0.6m jump) |
| **91.7** | **DISARMED** |

## EKF Reset Switch Behaviour

### What Happens at Each Toggle

The aux switch calls `AP::ahrs().reset_ekf_bootstrap()` → `NavEKF3::InitialiseFilterBootstrap()`.
This sets `statesInitialised = false` on each core and runs the full bootstrap sequence:

1. errRP resets to 0.1414 (initial attitude error estimate)
2. SS (status flags) resets to 0, then rebuilds from 1024
3. VD (vertical velocity) zeros immediately
4. PD (position down) jumps as the EKF reinitializes from current sensor data

The function returns `false` because the IMU delay buffer isn't yet filled after reinitialization,
so the aux switch reports "EKF bootstrap reset failed" — even though the EKF has already been
fully reinitialized and disrupted.

### PD Discontinuities

| Reset | Time | PD Before | PD After | Jump |
|-------|------|-----------|----------|------|
| #1 | 25.0s | -1.737 | -1.678 | 0.06m |
| #2 | 34.9s | -1.247 | -1.248 | ~0m (VD zeroed) |
| #3 | 48.1s | -0.620 | -0.573 | 0.05m |
| #4 | 58.4s | -2.924 | +0.276 | **3.20m** |
| #5 | 74.0s | -3.172 | -1.069 | **2.10m** |
| #6 | 82.3s | -0.676 | -0.809 | 0.13m |
| #7 | 85.6s | -1.789 | -1.154 | **0.64m** |

Reset #4 had the largest jump because the origin was being re-set at the same time (58.3s),
causing PD to swing to -2.924 just before the reset snapped it to +0.276.

### XKF4 Health at Resets

Each reset shows the same pattern:
- errRP jumps to 0.1414 (initial value = sqrt(2)/10)
- SS resets to 0 then rebuilds to 1024
- TS stays at 50 throughout (mag timeout — normal)
- SV preserved across reset (velocity variance doesn't reset)

At reset #7 (85.6s), SV had reached 0.30 and errRP was 0.2143 (above EK3_ERR_THRESH=0.20),
suggesting the EKF was already struggling before this final reset.

## Ground Effect

At ARM (17.2-18.3s), the baro ground effect reached **-8.1m**. The noise floor
(GND_EFF_DZ=-8) handled it well — EKF Alt only reached +0.10m during the spike.
Pilot throttle rose quickly this time (RC3=987→1412 in 1.5s), avoiding the log72 problem
of sitting just below mid-stick.

## VRFB Learning

No VRFB change at disarm (still -0.345, -0.131). This is expected — Stabilize mode
doesn't trigger hover Z-bias learning. MOT_THST_HOVER also unchanged at 0.157.

## Key Findings

1. **EKF reset switch is dangerous in flight** — triggers full reinitialization with PD jumps
   up to 3.2m. In AltHold mode this would cause violent altitude corrections. In Stabilize
   mode the vehicle is unaffected since the EKF isn't controlling altitude.

2. **"Failed" message is misleading** — the EKF HAS been reinitialized (prints "IMU0
   initialised"), it just can't return true until the IMU buffer refills. The damage is
   already done by the time the failure is reported.

3. **Multiple EKF origin set events** — the EKF origin was set 3 times (45.1, 58.3, 68.5s)
   during the flight. This is unusual and may be related to the repeated reinitializations
   clearing the origin state.

4. **Ground effect handled well** — -8.1m baro spike at ARM, Alt stayed within ±0.1m.

## Recommendations

1. **Guard EKF reset switch against in-flight use** — refuse to reinitialize while armed,
   or at minimum while in a position-controlled mode (AltHold, Loiter, Auto, etc.)
2. **Fix the "failed" message** — if the EKF was reinitialized, the message should reflect
   that the reset occurred but buffer refill is pending, not that it "failed"

## Analysis: Making EKF Reset Safer in AltHold

If the EKF reset switch must work in flight (e.g. for recovery from a corrupted EKF state
while in AltHold), the key issue is the altitude estimate discontinuity. Three approaches
with increasing safety:

### Approach 1: EKF reinit only (current behaviour)

After `InitialiseFilterBootstrap()`, PD lands at an arbitrary value determined by the
bootstrap sensor read. In log73 this caused jumps up to 3.2m.

```
Before: DAlt=1.5m, Alt=1.5m, error=0
After:  DAlt=1.5m, Alt=-1.7m, error=3.2m → violent climb
```

### Approach 2: EKF reinit + height datum reset

Call `resetHeightDatum()` after the reinit completes. This zeros PD and flushes the delay
buffers (the v2 version also resets baroHgtOffset and storedBaro). The altitude estimate
becomes 0, but the controller target is unchanged:

```
Before: DAlt=1.5m, Alt=1.5m, error=0
After:  DAlt=1.5m, Alt=0.0m, error=1.5m → moderate climb
```

Better (1.5m vs 3.2m error), but still a step input to the controller.

### Approach 3: EKF reinit + height datum reset + controller target reset (recommended)

After the reinit and datum reset, also reset the altitude controller target (DAlt) to 0.
This treats the reset as "fresh takeoff from current position":

```
Before: DAlt=1.5m, Alt=1.5m, error=0
After:  DAlt=0.0m, Alt=0.0m, error=0 → no correction
```

The vehicle holds its current throttle output (MOT_THST_HOVER). As the EKF reconverges
over ~1-2s, Alt gradually recovers to the true altitude and the controller smoothly
resumes tracking. VD is zeroed so the EKF momentarily doesn't know its vertical velocity,
causing a brief drift window, but this is far safer than a 3.2m step.

### Implementation notes

- The v2 `resetHeightDatum()` already handles the EKF side: zeroing PD, flushing output
  buffers, resetting baroHgtOffset, clearing storedBaro, and updating public_origin.alt
- The controller target reset would need to happen in the vehicle code (Copter side),
  similar to what happens at arming when `set_alt_target_to_current_alt()` is called
- The "failed" return from `InitialiseFilterBootstrap()` is because `storedIMU.is_filled()`
  returns false immediately after reinit — the buffer needs ~50ms to refill. The datum
  reset should be deferred until after the buffer fills (i.e. when the function finally
  returns true), not called immediately
- Conservative option: still block in position modes, offer Approach 3 only as an
  opt-in via `EK3_OPTIONS` bitmask for experienced users
