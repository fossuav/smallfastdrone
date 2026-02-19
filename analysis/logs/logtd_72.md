# logtd_72 Analysis — TD-Matek-5 log72.bin

## Metadata
- **Date**: 2026-02-19
- **Vehicle**: TD-Matek-5 (MatekH743-bdshot)
- **Firmware**: V4.6.3v2-SFD (b9467d23) — SmallFastDrone-4.6-AltHoldv2 branch
- **Log file**: ./log72.bin
- **Frame**: QUAD/X
- **Sensors**: GPS (u-blox SAM-M10Q, 3D fix, 13-16 sats), baro, compass, rangefinder (type 24), optical flow (type 6)
- **Mode**: AltHold (mode 2)
- **One flight**: ARM 67.8s → LAND mode 124.5s → DISARM 128.9s (~61s flight)

## Summary

**Struggled to take off due to baro ground effect while motors spooled. Once airborne, altitude
hold had ±0.7m oscillation from variable rangefinder readings over uneven outdoor terrain.
Flight ended with automatic LAND mode at 124.5s (reason 53).**

## Key Configuration

```
# Sensors
EK3_SRC1_POSXY  = 3     (GPS)
EK3_SRC1_VELXY  = 3     (GPS)
EK3_SRC1_POSZ   = 1     (baro)
EK3_SRC1_VELZ   = 3     (GPS)
EK3_SRC1_YAW    = 1     (compass)
EK3_RNG_USE_HGT = 3     (rangefinder blending below 3m)
EK3_GND_EFF_DZ  = -8    (noise floor mode)
FLOW_TYPE       = 6     (optical flow enabled)

# VRFB at boot
INS_ACC_VRFB_Z  = -0.297
INS_ACC2_VRFB_Z = -0.222

# Baro
BARO1_THST_SCALE = -100
BARO1_THST_FILT  = 1.0

# Position Controller
PSC_POSZ_P      = 1.0
PSC_VELZ_P      = 5.0
PSC_ACCZ_P      = 0.138   (3.6x below default 0.5)
PSC_ACCZ_I      = 0.277   (3.6x below default 1.0)

# Ground effect
TKOFF_GNDEFF_ALT = 0.5
TKOFF_GNDEFF_TMO = 1.5

# Other
MOT_THST_HOVER  = 0.136
INS_GYRO_FILTER = 75
ACC_ZBIAS_LEARN = 3
```

## Flight Timeline

| Time (s) | Event |
|-----------|-------|
| 3.2 | Boot — V4.6.3v2-SFD (b9467d23) |
| 4.0 | VRFB loaded: IMU0=-0.297, IMU1=-0.222 |
| 10.0 | EKF3 waiting for GPS config |
| 15.4 | EKF3 IMU0/IMU1 initialised, MAG0 yaw aligned |
| 17.0 | Tilt alignment complete |
| 25.6 | EKF3 origin set, GPS 3D fix |
| 43.2 | EKF3 using GPS |
| 61.8 | "Arm: Throttle too high" — failed arm attempt |
| 65.3 | "Arm: Throttle (RC3) is not neutral" — second failed attempt |
| **67.8** | **ARMED** |
| 68.3 | EV15 (takeoff detected) — but throttle still zero |
| 68–71 | Motors spinning, no pilot throttle. BAlt drops to **-9.0m** (ground effect) |
| **71.5** | Pilot raises throttle. Vehicle lifts off |
| 72–73 | Overshoot to 1.86m, then oscillation |
| 74–82 | Settling: Alt 1.5-2.2m, BAlt 1.8-3.3m (baro very noisy) |
| 82–124 | Hover at DAlt=1.42m. Alt oscillates 0.7-1.7m |
| **124.5** | **Mode switch to LAND** (reason 53) |
| 125.5 | EV73 |
| 128.3 | Throttle cut (RC3=987) |
| 128.9 | BAlt=-2.14m (ground effect at landing) |
| **128.9** | **DISARMED** |
| 129–140 | Post-disarm: Alt drifts from 0.2 to 2.3m while on ground |
| 130.1 | RC8 Relay1 LOW (source set switch) |
| 142.7 | "GPS Glitch or Compass error" |
| 151+ | GPS degrades: Status 2, 3 sats, HDop=7.1, Alt=-457m |
| 157.1 | "EKF variance: over thresholds" |
| 167.1 | "Radio Failsafe - Disarming" |

## Issue 1: Takeoff Struggle

### What Happened

Between ARM (67.8s) and pilot throttle application (71.5s), the motors were spinning with
no commanded thrust for **3.7 seconds**. During this time, baro ground effect created a
massive false reading:

| Time | ThO | Alt (EKF) | BAlt (baro) | Notes |
|------|-----|-----------|-------------|-------|
| 67.8 | 0.000 | 0.10 | -0.08 | ARM — motors spool |
| 68.8 | 0.000 | -0.08 | -2.89 | Ground effect starts |
| 70.4 | 0.000 | -0.34 | -2.85 | EKF thinks 0.34m underground |
| 71.0 | 0.000 | -0.41 | -2.58 | CRt=-53 cm/s |
| **71.5** | **0.167** | -0.40 | **-9.04** | **Pilot raises throttle, 9m baro spike** |
| 72.0 | 0.150 | -0.02 | 0.26 | Liftoff — baro normalizes |
| 72.6 | 0.167 | 0.43 | 0.60 | Climbing |
| 73.4 | 0.115 | 1.86 | 1.47 | **Overshoot** |

The EKF handled the -9m baro spike reasonably well (Alt only reached -0.41m thanks to
EK3_GND_EFF_DZ=-8 noise floor), but the negative altitude at liftoff caused an initial
overshoot to 1.86m before settling.

### Root Cause

The 3.7s delay between ARM and throttle is a pilot issue (arming difficulties — two failed
attempts due to throttle position). During this time the motors spin at idle and generate
prop wash over the baro. TKOFF_GNDEFF_TMO=1.5s may be too short — the ground effect
protection expired before the pilot raised throttle.

## Issue 2: Z-Drift / Altitude Oscillation

### Altitude Performance

During the 82–124s hover period:

| Metric | Value |
|--------|-------|
| Target (DAlt) | 1.42m (constant) |
| Alt mean | ~1.3m |
| Alt range | **0.67 – 1.69m** (±0.5m from mean) |
| BAlt range | -0.73 – 1.84m (2.6m total swing) |
| RFND range | 0.11 – 1.82m |
| CRt range | -97 to +41 cm/s |

### Correlation Table: Alt vs BAlt vs RFND vs HAGL

| Time | Alt | BAlt | RFND | HAGL | Offset | Notes |
|------|-----|------|------|------|--------|-------|
| 84s | 1.64 | 1.12 | 1.18 | 2.23 | +0.53 | Early, offset still positive |
| 90s | 1.20 | 0.72 | 0.81 | 0.89 | -0.29 | |
| 96s | 1.37 | 0.82 | 0.88 | 1.03 | -0.33 | |
| 102s | 1.26 | 0.70 | 0.87 | 0.78 | -0.48 | |
| 108s | 1.44 | 0.84 | 0.91 | 1.11 | -0.33 | |
| 111s | 1.24 | 0.29 | 0.25 | 0.73 | -0.48 | Low point |
| 114s | 0.67 | -0.73 | 0.11 | 0.12 | -0.45 | **RFND reads 11cm — near ground?** |
| 120s | 1.42 | 1.40 | 1.24 | 0.99 | -0.47 | Good agreement |
| 123s | 1.60 | 1.68 | 1.74 | 1.41 | -0.25 | |

### Root Cause: Uneven Terrain + Rangefinder Variation

The rangefinder swings from 0.11m to 1.82m during the hover while the EKF altitude stays
near 1.3m. This means the vehicle is hovering over **uneven outdoor terrain** — the
rangefinder is measuring true distance to ground which varies as the vehicle drifts
horizontally.

With EK3_RNG_USE_HGT=3, the EKF blends rangefinder below 3m. The terrain offset (XKF5)
swings from +0.53 to -0.52m as the rangefinder data fights with the baro. This terrain
offset variation directly drives altitude oscillation.

The baro is also noisy (BAlt range 2.6m during hover), which is partly real altitude
variation, partly thermal effects (baro temp rose from 34°C to 54°C during boot/ground
phase), and partly residual prop wash.

### Contributing Factor: Low PSC_ACCZ Gains

PSC_ACCZ_P=0.138 and PSC_ACCZ_I=0.277 are 3.6x below defaults. This makes the altitude
controller sluggish in responding to disturbances. The vehicle can't correct altitude errors
quickly enough when the terrain offset changes.

## EKF Health

The EKF was healthy throughout the flight:
- **errRP**: 0.006–0.012 (very low, stable)
- **TS**: 48 throughout (mag+airspeed timeout — normal for no airspeed sensor)
- **SV/SP/SH**: Normal ranges (SV≤0.3, SP≤0.1, SH≤0.2)
- **Innovations**: All active and varying — no fusion failures
- **MAG_FUSION**: Transitions from 2 (ground) to 1 (in-flight) correctly

XKF1 PD stayed within ±0.8m of zero — the EKF estimate is reasonable, the oscillation
is real altitude variation from the controller struggling with terrain and baro noise.

## VRFB Bias Learning

| Event | IMU0 | IMU1 |
|-------|------|------|
| Boot | -0.297 | -0.222 |
| Disarm | **-0.345** | **-0.132** |

Both IMUs learned in the same direction (negative). IMU0 moved from -0.30 to -0.35, IMU1
moved from -0.22 to -0.13. MOT_THST_HOVER moved from 0.136 to 0.157.

## Post-Flight GPS Degradation

After disarm, the GPS degraded rapidly:
- 131s: 16 sats, HDop=0.78, Status=3 (normal)
- 141s: 16 sats, HDop=0.78, Alt=140m (drifting from 148m)
- 151s: **3 sats**, HDop=7.1, Status=2 (2D fix)
- 161s: 4 sats, HDop=12.3, Alt=**-457m** (GPS altitude garbage)

This caused the "GPS Glitch or Compass error" at 142.7s and "EKF variance over thresholds"
at 157.1s. The GPS degradation happened after the flight so it didn't affect flight
performance, but it's worth investigating (antenna issue? interference?).

## Key Findings

1. **Takeoff struggle**: 3.7s delay between ARM and throttle caused -9m baro ground effect
   spike. The EKF handled it well (Alt only -0.41m) but the negative starting altitude caused
   overshoot on actual liftoff.

2. **Altitude oscillation ±0.5m**: Driven by rangefinder variation over uneven outdoor
   terrain, combined with noisy baro and low PSC_ACCZ gains. The terrain offset (XKF5) swings
   ±0.5m as the rangefinder fights the baro.

3. **No EKF failures**: All innovations active, no timeouts, no forced resets. The v2
   firmware is working correctly.

4. **Post-flight GPS loss**: Rapid satellite loss after disarm (16→3 sats). Did not affect
   flight. May indicate antenna or interference issue.

5. **Mode 9 (LAND) at 124.5s**: Triggered with reason 53 — likely crash check or failsafe.
   The vehicle was stable at the time (Roll/Pitch <8°), so this may have been triggered by
   the altitude oscillation exceeding a threshold.

## Recommendations

### Parameter Changes
| Parameter | Current | Suggested | Reason |
|-----------|---------|-----------|--------|
| PSC_ACCZ_P | 0.138 | **0.3–0.5** | Too sluggish, can't correct altitude errors quickly |
| PSC_ACCZ_I | 0.277 | **0.6–1.0** | Match P increase |
| TKOFF_GNDEFF_TMO | 1.5 | **3.0** | Ground effect protection expired before pilot raised throttle |
| EK3_RNG_USE_HGT | 3 | **-1** | Over uneven terrain, rangefinder creates oscillation. -1 disables blending while keeping rangefinder for HAGL |

### Investigate
- **GPS antenna**: Post-flight satellite loss from 16 to 3 is unusual. Check antenna
  mounting, cable, and potential interference sources.
- **Optical flow**: FLOW_TYPE=6 configured but no OF data logged. Check if the sensor is
  working. Flow would help with position hold over terrain.
- **LAND mode trigger**: Determine what triggered reason 53 at 124.5s. If crash check,
  the altitude oscillation may be triggering it.

## See Also
- [TD-Matek-5 previous logs](../README.md) — logtd series
- [EK3_RNG_USE_HGT feedback loop](../topics/ekf_rng_use_hgt_feedback.md) — discovered in logjk6/7
