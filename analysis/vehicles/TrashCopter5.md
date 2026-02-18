# TrashCopter5 — Vehicle Notes

## Vehicle Description

- **Frame**: Quad X_REV (FRAME_CLASS=1, FRAME_TYPE=18)
- **FC**: MatekH743-bdshot (APJ=1013)
- **Firmware**: V4.6.3v2-SFD (33d99563)
- **Board hash**: 004C002B 3130510D 33343837
- **IMU**: Dual IMU — EK3_IMU_MASK=3 (both IMUs)
- **Baro**: On-board, BARO1_THST_SCALE=-20
- **Rangefinder**: None (RNGFND1_TYPE=0)
- **GPS**: Configured (GPS1_TYPE=1) but never got fix
- **Compass**: Disabled (COMPASS_ENABLE=0) — yaw from EKF GSF
- **Optical flow**: None (FLOW_TYPE=0)
- **MOT_THST_HOVER**: 0.113 (~11% hover throttle — very high TWR)
- **Motor PWM**: DShot600 (MOT_PWM_TYPE=6)
- **Notch filter**: FFT-based (INS_HNTCH_MODE=3), 40 Hz center, harmonics 7
- **Secondary notch**: INS_HNTC2_ENABLE=1, 100 Hz
- **Tertiary notch**: INS_HNTC3_ENABLE=1, 100 Hz, mode 4

## Flight Log Summary

| # | Log | Date | AltHold (s) | Alt Err Std | Key Finding |
|---|-----|------|-------------|-------------|-------------|
| 1 | [logtc5_1](../logs/logtc5_1.md) flight 1 | Feb 17 | 84 | Good | Perfect hover, ThO=0.112 |
| 1 | [logtc5_1](../logs/logtc5_1.md) flight 2 | Feb 17 | 33 | **CRASH** | Total EKF fusion loss → uncontrollable ascent → motor estop |

## Current Configuration

```
# EKF Sources — ALL AIDING DISABLED (AID_NONE)
EK3_SRC1_POSZ        = 1        (baro — only measurement)
EK3_SRC1_VELZ        = 0        (NONE)
EK3_SRC1_VELXY       = 0        (NONE)
EK3_SRC1_POSXY       = 0        (NONE)
EK3_SRC1_YAW         = 0        (NONE — GSF yaw)

# EKF config
EK3_IMU_MASK         = 3        (both IMUs)
EK3_MAG_CAL          = 7
EK3_GND_EFF_DZ       = -8       (NEGATIVE — misconfigured, default 4.0)
EK3_RNG_USE_HGT      = -1
EK3_GLITCH_RAD       = 0        (disabled, default 25)
EK3_RNG_M_NSE        = 0.01     (very low, but no rangefinder installed)
EK3_FLOW_USE          = 1        (but no flow sensor)
EK3_SRC_OPTIONS      = 0

# Ground effect
TKOFF_GNDEFF_ALT     = 0.5
TKOFF_GNDEFF_TMO     = 3.0

# Baro compensation
BARO1_THST_SCALE     = -20

# Position controller
PSC_ACCZ_P           = 0.07     (7x below default 0.5)
PSC_ACCZ_I           = 0.14     (7x below default 1.0)
PSC_VELZ_P           = 4.0      (default 5.0)
PSC_JERK_Z           = 40       (8x default — dangerous)
PSC_JERK_XY          = 40       (8x default)

# Motor
MOT_THST_HOVER       = 0.113
MOT_THST_EXPO        = 0.6
MOT_SPIN_MIN         = 0.07     (below default 0.15)
MOT_SPIN_ARM         = 0.03     (below default 0.1)
MOT_HOVER_LEARN      = 0        (disabled)
MOT_OPTIONS          = 1

# IMU
INS_GYRO_FILTER      = 180      (9x default 20)
INS_GYRO_RATE        = 2        (fast sampling)

# Accel bias
ACC_ZBIAS_LEARN      = 3        (enabled)
INS_ACC_VRFB_Z       = -0.091   (CORRUPTED from crash)
INS_ACC2_VRFB_Z      = 0.168    (CORRUPTED from crash)

# GPS / Compass
GPS1_TYPE            = 1         (configured but no fix)
COMPASS_ENABLE       = 0

# Arming
ARMING_CHECK         = ?         (minimal)
```

## Known Issues

1. **EKF fusion loss on re-ARM** — Second ARM in logtc5_1 caused total measurement fusion
   failure. All innovations froze, state diverged 48m on IMU dead-reckoning. See
   [logtc5_1](../logs/logtc5_1.md) for full analysis.

2. **AID_NONE single-point-of-failure** — All sources set to NONE except POSZ=baro. In AID_NONE
   mode, all fusion is gated behind `fuseHgtData`. If baro recall fails, the EKF has zero
   observability.

3. **EK3_GND_EFF_DZ=-8 (negative)** — Creates inverted `constrain_ftype` bounds in the
   innovation flooring code, producing anomalous behavior. Should be default 4.0 or 0 to disable.

4. **Corrupted VRFB values** — INS_ACC_VRFB_Z=-0.091 and INS_ACC2_VRFB_Z=0.168 were saved from
   the crashed flight's diverged state. Must be reset to 0 before next flight.

5. **PSC gains extremely low** — PSC_ACCZ_P=0.07, PSC_ACCZ_I=0.14 are 7x below defaults.
   Combined with PSC_JERK_Z=40 (8x default), the altitude controller is both sluggish and
   capable of large overshoots.

6. **INS_GYRO_FILTER=180** — 9x the default of 20 Hz. May pass excessive vibration noise
   through to the EKF.

7. **No sensors installed** — Despite GPS1_TYPE=1, no GPS fix was obtained. No rangefinder,
   no optical flow, no compass. The vehicle has no external aiding whatsoever.

## Recommended Changes (for next flight)

### Critical (must fix)

| Parameter | Current | Target | Reason |
|-----------|---------|--------|--------|
| **EK3_GND_EFF_DZ** | -8 | **4** (default) | Negative value creates anomalous innovation flooring |
| **INS_ACC_VRFB_Z** | -0.091 | **0** | Corrupted from crash |
| **INS_ACC2_VRFB_Z** | 0.168 | **0** | Corrupted from crash |
| **EK3_SRC1_VELXY** | 0 | **3** (GPS) | Need velocity source to exit AID_NONE |
| **EK3_SRC1_POSXY** | 0 | **3** (GPS) | Need position source |
| **EK3_SRC1_VELZ** | 0 | **3** (GPS) | Need vertical velocity |

### Important

| Parameter | Current | Target | Reason |
|-----------|---------|--------|--------|
| PSC_ACCZ_P | 0.07 | **0.3** | Too low |
| PSC_ACCZ_I | 0.14 | **0.5** | Too low |
| PSC_JERK_Z | 40 | **5** (default) | Allows dangerous altitude spikes |
| INS_GYRO_FILTER | 180 | **20** (default) | May pass vibration noise |

### Investigate

- Why GPS never got a fix (antenna, connection, or environment?)
- Consider adding rangefinder for indoor altitude hold
- Consider adding optical flow for indoor position/velocity

## See Also
- [logtc5_1](../logs/logtc5_1.md) — flight log analysis (flights 1 & 2)
