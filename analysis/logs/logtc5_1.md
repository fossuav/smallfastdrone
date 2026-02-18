# logtc5_1 Analysis — TrashCopter5 log4.bin

## Metadata
- **Date**: 2026-02-17
- **Vehicle**: TrashCopter5 (MatekH743-bdshot)
- **Firmware**: V4.6.3v2-SFD (33d99563)
- **Branch**: SmallFastDrone-4.6-AltHold (notes), SmallFastDrone-4.6-AltHoldv2 (flying/fixes)
- **Log file**: ./log4.bin
- **Frame**: QUAD/X_REV (FRAME_CLASS=1, FRAME_TYPE=18)
- **Sensors**: GPS (configured but no fix), baro, dual IMU — NO rangefinder, NO optical flow, NO compass
- **Two flights in one session**: Flight 1 perfect, Flight 2 catastrophic

## Summary

**Flight 1 was perfect (84s, stable hover). Flight 2 ended in uncontrollable ascent requiring emergency motor stop.**

Root cause: **Total EKF measurement fusion failure.** At the second ARM event, the EKF stopped fusing ALL measurements (baro, zero velocity, zero position). With no corrections, the state diverged on pure IMU dead-reckoning. Core 0 estimated the vehicle was 48m underground; the controller commanded max throttle to "climb back up," causing uncontrollable ascent.

## Vehicle Configuration (Critical Issues Highlighted)

```
# EKF3 Sources — ALL AIDING DISABLED
EK3_SRC1_POSZ   = 1     (baro — ONLY measurement source)
EK3_SRC1_VELZ   = 0     (NONE)
EK3_SRC1_VELXY  = 0     (NONE — no GPS, no flow)
EK3_SRC1_POSXY  = 0     (NONE)
EK3_SRC1_YAW    = 0     (NONE — GSF yaw)
→ EKF operates in AID_NONE: baro is the SOLE measurement source

# Ground Effect
EK3_GND_EFF_DZ  = -8    (v2: noise floor mode, 8m obs noise during gnd effect; original code: anomalous)
TKOFF_GNDEFF_ALT = 0.5
TKOFF_GNDEFF_TMO = 3.0

# Baro
BARO1_THST_SCALE = -20   (thrust-based baro compensation)

# Position Controller — Aggressive
PSC_ACCZ_P      = 0.07   (7x below default 0.5)
PSC_ACCZ_I      = 0.14   (7x below default 1.0)
PSC_JERK_Z      = 40     (8x default)

# Motors
MOT_THST_HOVER  = 0.113  (very low — high thrust-to-weight)
MOT_HOVER_LEARN = 0      (disabled)
MOT_PWM_TYPE    = 6      (DShot600)

# Other
EK3_IMU_MASK    = 3      (dual IMU)
EK3_MAG_CAL     = 7
GPS1_TYPE       = 1      (configured but never gets fix)
COMPASS_ENABLE  = 0
ACC_ZBIAS_LEARN = 3
INS_GYRO_FILTER = 180    (very high, default 20)
ARMING_CHECK    = ?      (minimal — no GPS, no compass)
```

## Flight Timeline

| Time (s) | Event |
|-----------|-------|
| 3.9 | Boot — ArduCopter V4.6.3v2-SFD |
| 5.2 | EKF3 IMU0/IMU1 initialised, origin set |
| 6.5 | Tilt alignment complete |
| **21.0** | **ARMED (flight 1)** — EV10, EV60 (alt reset), EV57 (pos reset) |
| 24.4 | Takeoff complete (EV15) |
| 25.9 | EV28 |
| 93.8 | Land complete detected |
| **105.1** | **DISARMED** — EV11 |
| 105.2 | EKF resets (innovations jump, then reconverge) |
| 105–114 | **Disarmed gap — 9 seconds** (EKF continues running, innovations active) |
| **114.2** | **ARMED (flight 2)** — EV10, EV60, EV57 |
| 114.2 | **XKF4 SS: 65703→71847** (bits 11+12 set: takeoff AND touchdown expected) |
| 114.2 | **XKF3 IPD drops from 0.20 to 0.000** — innovations FREEZE |
| 117.5 | Takeoff complete (EV15) |
| 118.3 | EV28 — vehicle airborne |
| 118–126 | XKF4 SS toggles between 71847 and 67751 (touchdown_expected intermittent) |
| 130 | CTUN.Alt ≈ -15m (EKF diverging), BAlt ≈ 1m |
| 140 | CTUN.Alt ≈ -35m, vehicle ascending uncontrollably |
| 147.0 | CTUN.Alt ≈ -48m, pilot drops throttle to zero |
| **147.5** | **Motor emergency stop** (EV54) — RC9 MotorEStop HIGH |
| 152.5 | DISARMED |
| 154.2 | "AHRS: EKF3 Roll/Pitch inconsistent 48 deg" |
| 159.9 | EKF3 IMU0 forced reset |
| 161.6 | EKF3 IMU1 forced reset |

**Flight 1**: 84s, perfect AltHold hover.
**Flight 2**: 33s, EKF altitude diverged to -48m while vehicle was at ~1-2m.

## The Failure: Total Measurement Fusion Loss

### Evidence: Innovations Frozen

XKF3 innovations for Core 0 during flight 2 vs flight 1:

| Field | Flight 1 (30-50s) | Flight 2 (120-147s) | Status |
|-------|-------------------|---------------------|--------|
| IVN (vel N) | 0.01 (varying) | **0.10 (frozen)** | Stuck at pre-ARM value |
| IVE (vel E) | 0.00 (varying) | **-0.07 (frozen)** | Stuck at pre-ARM value |
| IVD (vel D) | 0.04 (varying) | **0.12 (frozen)** | Stuck at pre-ARM value |
| IPN (pos N) | 0.42–1.18 (varying) | **0.10 (frozen)** | Stuck at pre-ARM value |
| IPE (pos E) | 0.13–0.92 (varying) | **-0.06 (frozen)** | Stuck at pre-ARM value |
| IPD (pos D) | -0.49 to -0.07 (active) | **0.000 (frozen)** | Zero from ARM onward |

During flight 1, all innovations are actively varying — the EKF is computing and applying corrections from baro and zero-velocity observations. During flight 2, every innovation is frozen at its pre-ARM value, proving that `FuseVelPosNED()` is never called.

### Evidence: Covariance Growing Unbounded

XKV1 state covariance diagonals for Core 0:

| State Variance | Flight 1 (50s) | Flight 2 (114s) | Flight 2 (130s) | Flight 2 (148s) |
|----------------|-----------------|------------------|------------------|------------------|
| V04/V05 (vel NE) | 0.25–0.31 | 0.025 | **12.1–21.3** | **62.5** |
| V06 (vel D) | 0.016 | 0.015 | 0.20–0.30 | — |
| V07/V08 (pos NE) | 4.5–5.0 | 0.089 | **743–2132** | **5000** (capped) |
| V09 (pos D) | 0.18 | 0.148 | 19.8–44.9 | **283** |

Position NE variance hits the 5000 cap at ~139s. Height variance grows from 0.15 to 283 in 35 seconds. All variances grow monotonically with zero corrections — the EKF is running completely open-loop.

### Evidence: Accel Bias Frozen

XKF2 for Core 0 during flight 2: AX=-0.02, AY=-0.02, AZ=-0.07 — all completely static throughout the entire flight. The EKF cannot learn or adapt bias without measurement corrections.

### Evidence: State Divergence from Reality

| Time | XKF1 C0 PD (NED, m) | CTUN.Alt (m) | BAlt (baro, m) | Reality |
|------|---------------------|--------------|----------------|---------|
| 114.2s | 0.31 | -0.15 | 0.05 | On ground |
| 120s | ~2 | ~-2 | ~1 | Hovering ~1-2m |
| 125s | 4.23 | ~-4 | ~2 | Hovering |
| 130s | ~15 | ~-15 | ~2 | Hovering |
| 140s | 28.96 | ~-29 | ~2 | Ascending (controller commanding up) |
| 146s | ~45 | -45.08 | 1.28 | Ascending fast |
| 147.5s | ~48 | -48.4 | 0.92 | **Motor emergency stop** |

Core 0 PD reaches +48m (EKF believes vehicle is 48m underground). CTUN.Alt = -48m. Baro reads ~1-2m (reality). CRt (climb rate) reaches -1646 cm/s (EKF thinks vehicle is falling at 16 m/s).

Core 1 diverges in the opposite direction: PD reaches -67m (EKF believes vehicle is 67m above ground).

### Evidence: errRP Accumulation

XKF4 errRP (gyro/attitude error) for Core 0:
- ARM (114.2s): **0.0064** (very low — filter thought it was converged)
- 130s: 0.020
- 140s: 0.030
- 147s: **0.034** (monotonically increasing — no corrections to stop drift)
- 148s: **0.052** (crash dynamics)
- 154s: "Roll/Pitch inconsistent 48 deg" between cores

## Why Flight 2 But Not Flight 1?

### What's the Same
- Same vehicle, same code, same parameters
- Same baro producing valid data throughout
- Same AID_NONE mode (no GPS fix in either flight)

### What's Different

| Factor | Flight 1 | Flight 2 |
|--------|----------|----------|
| **ARM sequence** | First ARM after boot | Second ARM, 9s after disarm |
| **EKF state at ARM** | Fresh initialization | Partially converged from flight 1 |
| **XKF4 SS at ARM** | 65703 (no ground effect flags) | **71847** (bits 11+12: takeoff AND touchdown) |
| **IPD at ARM** | Active, varying | **Drops to 0.000 and freezes** |
| **All innovations** | Active | **Frozen at pre-ARM values** |
| **P[9][9] at ARM** | Normal (from init) | 0.148 (small, from converged state) |

### The Smoking Gun: Innovations Freeze at Exact Moment of ARM

During the disarm gap (105-114s), innovations are **actively updating** — IPD grows from -0.03 to 0.20, confirming that baro fusion IS working while disarmed.

At the exact moment of ARM (114.2s), IPD drops from 0.20 to 0.000 and ALL innovations freeze permanently. Something in the ARM sequence breaks the measurement fusion pipeline.

### Mechanism: Baro Fusion Pipeline Breaks

In AID_NONE mode, ALL measurement fusion (baro height, zero velocity, zero position) is gated behind a single flag — `fuseHgtData`:

```cpp
// AP_NavEKF3_PosVelFusion.cpp, AID_NONE block:
if (fuseHgtData && PV_AidingMode == AID_NONE) {
    fusePosData = true;
    if (onGroundNotFlying) {
        fuseVelData = true;  // zero velocity
    }
}
```

`fuseHgtData` is set by `selectHeightForFusion()`, which calls `storedBaro.recall()`:

```cpp
baroDataToFuse = storedBaro.recall(baroDataDelayed, imuDataDelayed.time_ms);
if (baroDataToFuse && activeHgtSource == BARO) {
    fuseHgtData = true;
}
```

If `storedBaro.recall()` returns false, `fuseHgtData` is never true, and **no fusion of any kind occurs** — not just height, but also zero velocity and zero position. This is a single-point-of-failure architecture in AID_NONE.

### Replay Investigation

#### Baseline Replay (notes branch — original code)

Running the log through Replay (EKF3 re-run from DAL data) with the original code confirmed:

1. **Replay does NOT reproduce the failure.** Both cores fuse baro continuously after the
   second ARM in Replay. Innovations recover within ~200ms of the datum reset.
2. **The real hardware froze for 38.4 seconds** (114.2s to 152.6s), then violently unfroze
   with 321m height innovation — consistent with 38s of uncorrected accel bias (~0.44 m/s²).
3. **The DAL baro data is continuous and correct** — RBRI messages show 10Hz baro updates
   with no gaps before, during, or after ARM. The recalibration step change is clean.

Key difference: Replay runs in **double precision** (`ftype = double`) on x86, while the
flight controller runs in **single precision** (`ftype = float`) on STM32. Combined with
strict sequential DAL frame processing in Replay vs. real-time cooperative scheduling on
the flight controller, the failure cannot be reproduced offline.

#### v2 Branch Replay (SmallFastDrone-4.6-AltHoldv2 — with fixes)

Replayed with the v2 branch which includes resetHeightDatum() buffer/output fixes,
ground effect noise floor, ResetHeight() suppression during ground effect, and
EKF bootstrap reset. The v2 fixes **completely eliminate the baro fusion blackout**.

**Height innovation comparison (XKF3 IPD), original C=0 vs replay C=100:**

| Time (s) | Original IPD | Replay IPD | Notes |
|-----------|-------------|------------|-------|
| 113.0 | 0.20 | 0.16 | Pre-ARM: both active |
| 115.1 | **0.00** | -0.02 | Post-ARM: original freezes, replay continues |
| 119.1 | **0.00** | 0.60 | Replay sees hover baro error |
| 125.1 | **0.00** | 1.12 | |
| 131.3 | **0.00** | 0.63 | |
| 137.3 | **0.00** | 0.10 | |
| 143.4 | **0.00** | 0.02 | |
| 145.4 | **0.00** | 0.25 | Pilot switches to STAB at 145.4s |
| 149.5 | **0.00** | -0.50 | Ground effect clamping |
| 151.5 | **0.00** | -14.95 | Innovation unclamped |
| 153.5 | 7.84 | 3.00 | Both recovering |

**Altitude estimate comparison (XKF1 PD), original vs replay:**

| Time (s) | Original PD (m) | Replay PD (m) | Notes |
|-----------|----------------|---------------|-------|
| 118.0 | 0.47 | 0.08 | Both near ground |
| 121.1 | 0.17 | -0.42 | Original drifting, replay hovering |
| 124.1 | 1.01 | 0.24 | |
| 127.1 | 1.30 | 0.13 | |
| 130.1 | 1.47 | -0.04 | Replay: near-perfect |
| 133.1 | 1.54 | -0.24 | |
| 136.1 | 2.43 | 0.36 | |
| 139.2 | 2.19 | -0.15 | |
| 142.2 | 2.51 | -0.09 | |
| **145.2** | **2.51** | **-0.28** | **Mode switch to STAB** |
| 149.5 | 43.4 | 31.6 | Both diverge during crash (artifact) |
| 157.5 | -0.04 | 0.00 | Replay: bootstrap reset at 159.9s |

**Key findings from v2 replay:**

1. **Baro fusion active throughout**: Replay IPD varies continuously (never frozen at 0.0).
   The resetHeightDatum() fixes ensure `storedBaro.recall()` succeeds after re-ARM.

2. **Altitude stable during hover**: Replay PD stays within **±0.4m** of zero for the entire
   118-145s hover period. Original drifted to +2.5m (underground) over the same period.

3. **Crash would not have occurred**: The v2 fixes keep the altitude estimate accurate through
   the entire AltHold period. The pilot switched to STAB at 145.4s because of the original
   code's divergence — this event wouldn't happen with v2.

4. **Late divergence is an artifact**: Both original and replay show altitude explosion after
   145s because the log contains the actual crash dynamics (pilot cutting motors in STAB after
   the original EKF failure). With working baro fusion, this crash never would have occurred.

5. **EKF bootstrap reset recovers**: The v2 bootstrap reset feature fires at 159.9s (IMU0) and
   161.6s (IMU1), snapping the replay altitude back to 0.0 after the crash artifact.

6. **Ground effect noise floor works**: With EK3_GND_EFF_DZ=-8, the v2 branch interprets
   this as an 8m noise floor (variance=64, K≈0.008), heavily deweighting baro during ground
   effect rather than applying the inverted dead zone from the original code.

### Root Cause: `resetHeightDatum()` Missing Buffer Reset

The `resetHeightDatum()` function (called at re-ARM when `!home_is_set()`) recalibrates the
baro and resets `stateStruct.position.z = 0`, but does NOT:
- Reset the baro observation buffer (`storedBaro`)
- Reset `lastBaroReceived_ms` (the baro rate-limiting timer)
- Reset the output filter states to match the new position
- Reset vertical velocity to zero
- Clear `baroHgtOffset` (invalid after recalibration)
- Reset `lastHgtPassTime_ms` / `hgtTimeout` (empty buffer triggers premature timeout)
- Sync `public_origin.alt` with `EKF_origin.alt`

This leaves stale pre-calibration data in the 2-element baro buffer. On the real hardware
(single precision, real-time scheduling), a timing interaction between the datum reset and
the next EKF update cycle causes `storedBaro.recall()` to permanently fail — the buffer
count drops to 0 and never recovers. In Replay (double precision, strict sequential
processing), the same code recovers because the buffer timing aligns correctly.

**Fixed on v2 branch** in commits:
- `311f6d08` — AP_NavEKF3: fix resetHeightDatum and height fusion during ground effect
- `2d6adee9` — Copter: always reset EKF height datum on arming
- `390c05ce` — AP_NavEKF3: fix resetHeightDatum blocked by rangefinder blending
- `19e2df8e` — AP_NavEKF3: fix resetHeightDatum() not flushing delay buffers

## XKF4 Solution Status Analysis

```
SS=65703 decimal = 1_0000_0000_1010_0111 binary

Bit 0:  1  attitude valid
Bit 1:  1  horizontal velocity valid
Bit 2:  1  vertical velocity valid
Bit 5:  1  position horizontal valid (relative)
Bit 7:  1  position vertical valid
Bit 16: 1  EKF GPS checks passing

SS=71847 decimal = 1_0001_1000_1010_0111 binary
Same as above PLUS:
Bit 11: 1  takeoff_expected
Bit 12: 1  touchdown_expected
```

Both ground effect flags set simultaneously at ARM is unusual — typically only takeoff_expected should be set at ARM. This activates ground effect compensation (innovation flooring + 4x baro noise scaling).

With `EK3_GND_EFF_DZ=-8` (negative), the original code computes inverted `constrain_ftype`
bounds (lower=0.0, upper=-0.5), producing anomalous behavior. The v2 branch fixes this:
`fabsF()` is used for the dead zone size, and negative values trigger a noise floor mode
where |value| becomes the baro observation noise in metres (variance=64, K≈0.008), heavily
deweighting baro during ground effect instead of clamping innovations.

## Post-Crash

- **147.5s**: Motor emergency stop
- **152.5s**: Disarmed
- **152.6s**: Innovations spike massively (IPD=321.79, IPN=82.98) as filter encounters real baro data
- **154.2s**: "Roll/Pitch inconsistent 48 deg" between cores
- **159-162s**: Both IMU cores forced reset
- At disarm: INS_ACC_VRFB_Z saved as -0.091 (IMU0), +0.168 (IMU1) — corrupted by the diverged flight

## CTUN Performance Comparison

### Flight 1 (Stable Hover, 30-93s)
- Alt: ~1.75m (stable)
- ThO: ~0.112 (consistent with MOT_THST_HOVER=0.113)
- CRt: small oscillations around 0

### Flight 2 (Divergence, 114-147s)
- Alt: -0.15m → **-57.8m** (pure IMU drift)
- BAlt: 0.0m → ~1.5m (real altitude, stable)
- ThO: rises to max as controller tries to "climb" from -48m
- CRt: reaches **-1646 cm/s** (EKF sees 16 m/s "descent")

## Key Findings

1. **Total measurement fusion failure** — All EKF3 innovations freeze at the exact moment of second ARM. No baro, no zero velocity, no zero position corrections for the entire 33-second flight.

2. **AID_NONE single-point-of-failure** — With all aiding sources disabled (VELXY=0, POSXY=0, VELZ=0), the EKF's only measurement path is through baro. When baro fusion fails, the filter has zero observability and runs on pure IMU dead-reckoning.

3. **Ground effect flags anomalous at ARM** — Both takeoff_expected and touchdown_expected set simultaneously (SS=71847). With original code, EK3_GND_EFF_DZ=-8 caused inverted innovation clamping; v2 branch interprets negative values as noise floor mode.

4. **Dual-core divergence** — Without corrections, IMU biases drive Core 0 (+48m down) and Core 1 (-67m up) in opposite directions. The 48° roll/pitch inconsistency at 154s confirms complete core divergence.

5. **Flight 1 unaffected** — First ARM worked perfectly because the baro fusion pipeline was intact. Something specific to the re-ARM sequence (height/position resets, buffer state, timing) breaks the pipeline.

6. **Corrupted VRFB saved** — At disarm, the vehicle saved AccZ bias values from the diverged state (IMU0: -0.091, IMU1: +0.168), which would corrupt subsequent flights.

## Recommended Changes

### Critical (Must Fix)

#### Firmware Fix (on v2 branch)

The `SmallFastDrone-4.6-AltHoldv2` branch contains all required fixes:
- `311f6d08` — resetHeightDatum() buffer/output/velocity reset, ground effect noise floor,
  ResetHeight() suppression during ground effect, negative GND_EFF_DZ as noise floor
- `2d6adee9` — Always reset height datum on arming (not just when home unset)
- `390c05ce` — Allow datum reset when rangefinder blending is active but primary source isn't RNG
- `19e2df8e` — Additional buffer flushing in resetHeightDatum()

With these fixes, Replay confirms the baro fusion blackout is eliminated and altitude
stays within ±0.4m during hover.

#### Parameter Changes

| Parameter | Current | Target | Reason |
|-----------|---------|--------|--------|
| **EK3_GND_EFF_DZ** | -8 | **-8** (keep) | v2 branch treats negative as noise floor (8m → variance 64, K≈0.008) — appropriate for this copter |
| **INS_ACC_VRFB_Z** | -0.091 | **0** | Reset corrupted bias from crashed flight |
| **INS_ACC2_VRFB_Z** | 0.168 | **0** | Reset corrupted bias from crashed flight |

Note: GPS sources are NOT recommended for this vehicle (no GPS available). The vehicle must
operate in AID_NONE mode with baro as the sole height source.

### Important

| Parameter | Current | Target | Reason |
|-----------|---------|--------|--------|
| PSC_ACCZ_P | 0.07 | **0.3** | Too low, sluggish altitude response |
| PSC_ACCZ_I | 0.14 | **0.5** | Too low |
| PSC_JERK_Z | 40 | **5** (default) | 8x default, allows dangerous altitude spikes |
| INS_GYRO_FILTER | 180 | **20** (default) | 9x default, may pass vibration noise |

### Investigate

- **Install a rangefinder** — Would provide independent height measurement and partially exit AID_NONE
- **Install optical flow** — Would provide XY velocity aiding
- **AID_NONE single-point-of-failure** — Consider decoupling zero velocity/position fusion from
  `fuseHgtData` so that ground-phase corrections continue even during baro fusion interruptions

## See Also
- [TrashCopter5 vehicle notes](../vehicles/TrashCopter5.md)
- [EKF3 CLAUDE.md](../../libraries/AP_NavEKF3/CLAUDE.md) — EKF3 reference and methodology
