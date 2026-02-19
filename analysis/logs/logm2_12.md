# logm2_12 Analysis — TD-MicoAir-2 log12.bin

## Metadata
- **Date**: 2026-02-19
- **Vehicle**: TD-MicoAir-2 (MicoAir743v2)
- **Firmware**: V4.6.3v2-SFD (21057ce9) — SmallFastDrone-4.6-AltHoldv2 branch
- **Log file**: ./log12.bin
- **Frame**: QUAD/X
- **Sensors**: Optical flow (FLOW_TYPE=5), baro, dual IMU, rangefinder (MAVLink, 7m max)
- **No GPS, no compass** — fully GPS-denied
- **Three flights**: ARM/DISARM cycles testing AltHold with auto-takeoff
- **Purpose**: First AltHold tests on v2 firmware for this vehicle

## Key Configuration

```
# Altitude sources
EK3_SRC1_POSXY  = 0    (none — GPS-denied)
EK3_SRC1_VELXY  = 5    (optical flow)
EK3_SRC1_POSZ   = 1    (baro)
EK3_SRC1_VELZ   = 0    (none)
EK3_SRC1_YAW    = 0    (none — compass-less)
EK3_RNG_USE_HGT = -1   (rangefinder disabled for EKF height)

# Ground effect
EK3_GND_EFF_DZ  = 7    (POSITIVE = deadzone mode, 7m)
TKOFF_GNDEFF_TMO = 3.0  (longer than TD-Matek-5's 1.5)
TKOFF_GNDEFF_ALT = 0.5

# Hover/control
PSC_ACCZ_P      = 0.381
PSC_ACCZ_I      = 0.763
MOT_THST_HOVER  = 0.381  (heavier than TD-Matek-5's 0.157)
PILOT_TKOFF_ALT = 150    (1.5m auto-takeoff target)
EK3_PRIMARY     = 1      (prefers IMU1)

# Z-bias
INS_ACC_VRFB_Z  = 0.018
INS_ACC2_VRFB_Z = 0.024
ACC_ZBIAS_LEARN = 3
```

## Flight Summary

| Flight | ARM (s) | DISARM (s) | Duration | Mode | Key Events |
|--------|---------|------------|----------|------|------------|
| 1 | 7.7 | 8.9 | 1.2s | Stabilize | Accidental arm, no liftoff |
| 2 | 10.3 | 15.4 | 5.1s | AltHold | Auto-takeoff, brief hover, descent |
| 3 | 18.0 | 33.1 | 15.1s | AltHold→Loiter→AltHold→Stabilize | Longest flight, multiple mode changes |

## Height Datum Reset

Works correctly at each ARM:

| ARM | Alt Before → After |
|-----|-------------------|
| #1 (7.7s) | -0.143 → 0.000 |
| #2 (10.3s) | -0.061 → 0.000 |
| #3 (18.0s) | 0.056 → 0.024 |

## Ground Effect — Extreme Bidirectional Swings

This vehicle produces dramatically different baro ground effect compared to TD-Matek-5. The baro
swings both **negative** (downwash pressure) and **positive** (rebound/low-pressure), with total
swings of 14-21m.

### Flight 2 (10.3-15.4s)

| Time | BARO.Alt | CTUN.Alt (EKF) | Notes |
|------|----------|---------------|-------|
| 10.5s | -0.16 | 0.001 | Motors spinning up |
| 10.9s | -0.88 | -0.013 | Ground effect starts |
| 11.1s | -0.90 | -0.033 | Peak negative (mild) |
| 12.5s | -3.18 | — | Boost phase ground effect |
| 12.7s | **-7.66** | -0.376 | **Peak negative** |
| 12.9s | +3.68 | -0.385 | **Positive reversal!** |
| 13.1s | +5.65 | — | Peak positive |

Total swing: **13.3m** (-7.66 to +5.65). EKF Alt only moved 0.4m — excellent rejection
by the 7m deadzone during this phase.

### Flight 3 (18.0-33.1s)

| Time | BARO.Alt | CTUN.Alt (EKF) | Notes |
|------|----------|---------------|-------|
| 19.5s | -6.12 | -0.066 | Boost phase |
| 19.7s | **-8.19** | -0.078 | **Peak negative** |
| 19.9s | +5.75 | — | **Positive reversal** |
| 20.1s | +6.28 | 0.201 | Climbing |
| 22.0s | +3.67 | 1.472 | Hovering at 1.5m target |
| 23.4s | +7.00 | 1.487 | Baro climbing while EKF stable |
| 24.2s | +7.89 | 1.486 | Approaching deadzone boundary |
| 24.9s | +12.25 | -0.044 | **Deadzone breakthrough!** |
| 25.1s | **+13.13** | -0.085 | **Peak positive = 13m** |

Total swing: **21.3m** (-8.19 to +13.13). This is by far the largest baro ground effect
seen across all vehicles.

### Deadzone Breakthrough Event (24.9s)

At ~24.7s the baro altitude reading crossed the 7m deadzone boundary. The EKF suddenly
started fusing the grossly incorrect baro readings, causing the altitude estimate to jump
from ~1.5m (correct) to wildly incorrect values. In XKF1 PD data:

- Before (24.5s): PD = 0.05 (EKF estimates 0.05m below datum — roughly correct)
- 24.7s: PD = -0.07
- 24.8s: PD = -0.29
- 24.9s: PD = -0.66
- 25.0s: PD = -1.20
- 25.1s: PD = **-1.54** (EKF thinks vehicle jumped to 1.54m above datum)

This is an 1.6m PD step in 0.6s caused by the deadzone boundary being crossed while
the baro is reading absurdly high values. The EKF is now trusting a baro that reads 13m
when the vehicle is at ~1.5m.

## Mode Changes During Flight 3

| Time | Mode | Trigger |
|------|------|---------|
| 18.0s | AltHold | ARM |
| 25.9s | **Loiter** | CH6=2011 (0.4s!) |
| 26.3s | AltHold | CH6=1500 |
| 26.5s | Stabilize | CH6=987 |

The brief Loiter switch (0.4s) was likely accidental. The pilot then went AltHold briefly
before switching to Stabilize at 26.5s. Throttle was at 1001 (above idle) with the vehicle
still in the air, then the pilot manually flew in Stabilize for the remainder of flight 3.

After Stabilize switch (26.5-33.1s), the pilot climbed with manual throttle (RC3=1640)
then descended. The EKF Alt railed at -8.42 in CTUN during this phase — the height
datum offset from the earlier ground effect was still in effect.

## VRFB Learning

| Event | IMU0 | IMU1 |
|-------|------|------|
| Boot | 0.018 | 0.024 |
| Disarm flight 1 (8.9s) | 0.018 | 0.024 |
| Disarm flight 2 (15.4s) | 0.018 | 0.024 |
| Disarm flight 3 (33.1s) | **0.021** | **0.048** |

VRFB only changed at flight 3 disarm — this is the flight with the longest AltHold hover
period (~8s in AltHold before mode switches). Flights 1 (Stabilize) and 2 (5s in AltHold)
were too short for significant learning.

IMU1 VRFB change was larger (0.024→0.048 = +0.024 m/s²) than IMU0 (0.018→0.021 = +0.003).
This is consistent with EK3_PRIMARY=1 — IMU1 is the preferred core and has more opportunity
to learn.

## EKF Aiding Behaviour

Optical flow aiding starts during flight and stops at landing:
- Flight 2: Relative aiding started at 10.6s (IMU1) and 11.8s (IMU0)
- Flight 3: Relative aiding started at 18.0s (both IMUs together)

Between flights (disarmed on ground), aiding stops and restarts for next flight. The
EKF3 origin was never set in this log (no GPS, no external position).

## Key Findings

1. **Extreme bidirectional baro ground effect** — swings from -8.2m to +13.1m (21m total).
   Far worse than TD-Matek-5's unidirectional -7 to -9m swings. The positive rebound
   is unique to this smaller vehicle.

2. **Deadzone mode (EK3_GND_EFF_DZ=7) breaks down** — when the baro reading exceeds
   the 7m deadzone, the EKF suddenly fuses incorrect baro data, causing 1.6m altitude
   estimate jumps. The deadzone creates a hard boundary that the extreme positive baro
   readings routinely breach.

3. **Height datum reset works correctly** across all 3 ARM cycles.

4. **VRFB learning functional** — small change (0.024 m/s² on IMU1) after 8s of AltHold hover.

5. **Pilot abandons AltHold** — switches to Stabilize mid-flight, suggesting altitude
   control was not performing well.

## Recommendations

1. **Switch to noise floor mode**: Change `EK3_GND_EFF_DZ` from `7` (deadzone) to `-14`
   (noise floor). The noise floor mode uses the absolute value as minimum baro observation
   noise, avoiding the hard deadzone boundary that causes altitude jumps when breached.
   A value of -14 would cover the full 21m swing range.

2. **Consider enabling rangefinder for EKF height**: `EK3_RNG_USE_HGT=3` (as used on
   TD-Matek-5) would provide an alternative altitude source less susceptible to ground effect,
   reducing reliance on the corrupted baro.

3. **Increase TKOFF_GNDEFF_TMO** if needed — the current 3.0s seems adequate for this
   vehicle's takeoff speed, but monitor for timeout issues.

## See Also
- [logm2_13](logm2_13.md) — next flight session, same vehicle, continued AltHold testing
- [logm2_6](logm2_6.md) — earlier AltHold testing on v1 firmware (pre-ground effect fixes)
- [logtd_72](logtd_72.md) — TD-Matek-5 comparison (noise floor mode, unidirectional ground effect)
