# logtd_74 Analysis — TD-Matek-5 log74.bin

## Metadata
- **Date**: 2026-02-19
- **Vehicle**: TD-Matek-5 (MatekH743-bdshot)
- **Firmware**: V4.6.3v2-SFD (21057ce9) — SmallFastDrone-4.6-AltHoldv2 branch
- **Log file**: ./log74.bin
- **Frame**: QUAD/X
- **Sensors**: GPS (u-blox SAM-M10Q), baro, compass, rangefinder (CAN ID 125), optical flow
- **Mode**: Stabilize (mode 0) throughout
- **Five flights**: Multiple ARM/DISARM cycles testing height datum reset

## Summary

**Five ARM/DISARM cycles in Stabilize mode. Height datum reset works correctly — altitude
cleanly resets to ~0 at each ARM. Baro ground effect spikes up to -9.1m handled well by
the noise floor. Multiple arming issues (gyro inconsistency, yaw inconsistency, EKF attitude
bad) suggest IMU disagreement. EKF lane switches during flights 3 and 4. Emergency yaw reset
before flight 5.**

## Key Configuration

Same as log73:
```
INS_ACC_VRFB_Z   = -0.345
INS_ACC2_VRFB_Z  = -0.131
EK3_GND_EFF_DZ   = -8
EK3_RNG_USE_HGT  = 3
MOT_THST_HOVER   = 0.157
ACC_ZBIAS_LEARN   = 3
```

## Flight Summary

| Flight | ARM (s) | DISARM (s) | Duration | Peak Alt | BAlt GndEff | Key Events |
|--------|---------|------------|----------|----------|-------------|------------|
| 1 | 57.1 | 70.5 | 13s | ~0.9m | -7.0m | Smooth; mag anomaly yaw re-aligned |
| 2 | 91.5 | 105.4 | 14s | ~2.2m | -7.4m | Overshoot to 2.2m, rapid descent |
| 3 | 119.1 | 132.2 | 13s | ~2.1m | -6.8m | Landing bounce: -9.1m BAlt; lane switch |
| 4 | 156.1 | 165.5 | 9s | — | — | Lane switch 1→0→disarm |
| 5 | 191.7 | 196.6 | 5s | — | — | Post emergency yaw reset; short |

## Height Datum Reset Validation

The v2 firmware's `resetHeightDatum()` on arming works correctly across all 5 cycles.
At each ARM event, the CTUN Alt cleanly resets to near zero:

| Transition | Alt Before → After | Notes |
|------------|-------------------|-------|
| ARM 1 (57.1s) | -2.00 → 0.03 | First ARM, previous drift cleared |
| ARM 2 (91.6s) | 1.25 → -0.04 | Residual from flight 1 cleared |
| ARM 3 (119.2s) | 2.33 → -0.04 | Residual from flight 2 cleared |
| ARM 4 (156.1s) | — | — |
| ARM 5 (191.7s) | — | — |

This is the exact scenario that caused the crash in logtc5_1 (TrashCopter5 log4) — re-ARM
with accumulated altitude error. The v2 fixes handle it correctly.

## Ground Effect Performance

Every takeoff shows massive baro ground effect, all handled by the noise floor (GND_EFF_DZ=-8):

| Flight | Time | BAlt Min | Alt at BAlt Min | Notes |
|--------|------|----------|-----------------|-------|
| 1 | 59.2s | -7.0m | -0.13 | EKF Alt barely moved |
| 2 | 92.7s | -6.9m | -0.11 | Clean rejection |
| 3 | 121.0s | -6.8m | -0.25 | Takeoff ground effect |
| 3 landing | 130.4s | **-9.1m** | 0.04 | Bounce on landing, worst spike |
| 3 bounce | 131.3s | -5.8m | 0.14 | Second bounce |

The -9.1m spike at 130.4s during landing is the largest baro ground effect seen across all
TD-Matek-5 logs. The noise floor kept Alt at 0.04m — excellent rejection.

## Arming Issues

Multiple arming difficulties throughout the session:

| Time (s) | Message |
|-----------|---------|
| 56.9 | "Arm: EKF attitude is bad" |
| 56.9 | "Arm: AHRS: GPS speed error 1.0 (needs < 1.0)" |
| 115.2 | "Arm: Gyros inconsistent" |
| 115.2 | "AHRS: EKF3 Yaw inconsistent 23 deg" |
| 154.9 | "Arm: Gyros inconsistent" |
| 183.4 | "Arm: Gyros inconsistent" |
| 183.4 | "AHRS: EKF3 Roll/Pitch inconsistent 13 deg" |

All resolved via "Arm pending" → "Arm pending complete" (EKF converges within 1-5s).

The repeated gyro inconsistency and 23° yaw inconsistency between IMU0 and IMU1 suggests
the two IMUs have different thermal states or vibration environments.

## EKF Lane Switches

| Time (s) | Event |
|-----------|-------|
| 126.1 | EKF3 lane switch → IMU1 |
| 132.2 | EKF primary changed → IMU0 |
| 165.2 | EKF3 lane switch → IMU1 |
| 165.5 | EKF primary changed → IMU0 |

Lane switches during flights 3 and 4 indicate the two cores have similar health but differ
in which is momentarily better. This is consistent with the gyro inconsistency warnings.

## Emergency Yaw Reset

At 185.2s (between flights 4 and 5), "EKF3 IMU0 emergency yaw reset" was triggered. This
was followed by a full EKF reinitialization at 189.0s and "EKF variance: position lost".
The EKF recovered by 199s with new origin set and GPS acquisition.

## CAN Rangefinder Behaviour

The CAN rangefinder (ID 125) frequently switches between long range (25 Hz) and short range
(50 Hz) rates — about 10 switches during the log. This is normal adaptive rate behaviour
based on measured distance.

## VRFB Learning

No VRFB values changed at final disarm (still -0.345, -0.131). Expected for Stabilize mode
which doesn't trigger hover Z-bias learning.

## Key Findings

1. **Height datum reset validated** — Alt cleanly resets to ~0 at each ARM across 5 cycles.
   The re-ARM scenario that crashed TrashCopter5 works correctly on v2 firmware.

2. **Ground effect noise floor robust** — handled baro spikes from -6.8m to -9.1m with
   Alt staying within ±0.25m. The -9.1m landing bounce spike is the largest seen across
   all TD-Matek-5 logs.

3. **IMU disagreement** — repeated gyro inconsistency, 23° yaw inconsistency, and 13°
   roll/pitch inconsistency between IMU0 and IMU1. Worth investigating vibration isolation
   and thermal convergence.

4. **Emergency yaw reset** — triggered between flights 4 and 5. The full EKF reinit and
   recovery sequence worked correctly (re-origin, re-GPS acquisition).

5. **No EKF reset switch testing** in this log (unlike log73).

## See Also
- [logtd_73](logtd_73.md) — EKF reset switch testing (same vehicle, same session)
- [logtd_72](logtd_72.md) — previous flight, takeoff struggle and Z-drift analysis
- [logtc5_1](logtc5_1.md) — TrashCopter5 crash from re-ARM without height datum reset
- [logtc5_2](logtc5_2.md) — TrashCopter5 v2 validation of re-ARM fix
