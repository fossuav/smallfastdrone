# logtc5_2 Analysis — TrashCopter5 log7.bin & log8.bin

## Metadata
- **Date**: 2026-02-18
- **Vehicle**: TrashCopter5 (MatekH743-bdshot)
- **Firmware**: V4.6.3v2-SFD (15b50447) — **SmallFastDrone-4.6-AltHoldv2 branch**
- **Log files**: ./log7.bin (1 flight), ./log8.bin (2 flights, including re-ARM)
- **Frame**: QUAD/X_REV (FRAME_CLASS=1, FRAME_TYPE=18)
- **Sensors**: Baro only (AID_NONE). No GPS fix, no rangefinder, no optical flow, no compass.

## Summary

**All three flights successful.** The v2 firmware fixes eliminate the baro fusion blackout
that caused the log4.bin crash. The re-ARM flight (log8 flight 2) works correctly — baro
fusion continues uninterrupted through the height datum reset.

| Flight | Log | Duration | Alt Hold | Stdev | Notes |
|--------|-----|----------|----------|-------|-------|
| 1 | log7 | 66s (19–85s) | 2.96m mean | **0.04m** | VRFB=0 at boot. Excellent stability |
| 2 | log8 flt1 | 58s (21–79s) | 1.61m mean | 0.23m | VRFB=0→learned. Slow upward drift |
| 3 | log8 flt2 | 41s (92–134s) | 2.42m mean | 0.38m | **Re-ARM with VRFB loaded.** Slow climb, wider scatter |

## Flight Timeline

### log7 (1 flight)
| Time (s) | Event |
|-----------|-------|
| 3.9 | Boot — V4.6.3v2-SFD (15b50447) |
| 5.2 | EKF3 IMU0/IMU1 initialised, origin set |
| 6.5 | Tilt alignment complete |
| **19.0** | **ARMED** — EV10, EV60 (alt reset), EV57 (pos reset) |
| 22.5 | Takeoff complete (EV15) |
| 24.7 | EV28 |
| 78.7 | Land complete (EV17) |
| **84.7** | **DISARMED** |
| — | INS_ACC_VRFB_Z saved: -0.113, INS_ACC2_VRFB_Z saved: +0.177 |

### log8 (2 flights)
| Time (s) | Event |
|-----------|-------|
| 4.0 | Boot — same firmware. VRFB loaded: IMU0=-0.113, IMU1=+0.177 |
| 5.2 | EKF3 initialised |
| 6.5 | Tilt alignment complete |
| **21.2** | **ARMED (flight 1)** |
| 24.6 | Takeoff complete |
| 74.8 | Land complete |
| **79.0** | **DISARMED** — VRFB saved: -0.101, +0.122 |
| 79–92 | **Disarm gap — 13 seconds** |
| **92.3** | **ARMED (flight 2, re-ARM)** — the critical test case |
| 95.8 | Takeoff complete |
| 130.8 | Land complete |
| **133.5** | **DISARMED** — VRFB saved: -0.101, +0.122 (unchanged) |
| 135.8 | Mode switch to STAB (post-landing) |

## Re-ARM Validation (log8 flight 2)

This is the same scenario that caused total EKF fusion loss in log4.bin. With the v2 fixes:

**Innovations alive through re-ARM:**

| Time (s) | IPD | IVD | Status |
|-----------|-----|-----|--------|
| 88.0 | -0.41 | -0.08 | Disarmed, baro fusing |
| 91.0 | -0.16 | -0.04 | Pre-ARM |
| 92.1 | -0.08 | -0.03 | **Post-ARM — innovations continuing** |
| 93.1 | 0.04 | 0.06 | Active |
| 95.1 | 0.07 | 0.20 | Takeoff phase |
| 98.1 | 0.75 | -1.42 | Ground effect clearing |

Compare to log4.bin: IPD dropped to 0.000 at ARM and froze for 38 seconds. Here, IPD
transitions smoothly through the ARM event with no interruption.

**XKF4 health through re-ARM:**

| Time (s) | SH | TS | SS | Status |
|-----------|------|------|-------|--------|
| 92.0 | 0.00 | 0 | 65703 | Pre-ARM normal |
| 93.1 | 0.00 | 0 | 71847 | Ground effect active (bits 11+12) |
| 102.0 | 0.02 | 0 | 67751 | Takeoff clearing (bit 12 clears) |
| 107.1 | 0.00 | 0 | 65703 | Clean hover |

No timeouts (TS=0), no attitude errors (errRP=0.0), clean SS transitions.

**XKF1 PD through re-ARM:**

| Time (s) | PD (m) | VD (m/s) | Status |
|-----------|--------|----------|--------|
| 91.4 | -0.03 | -0.00 | On ground |
| 96.5 | 0.28 | 0.02 | Post-ARM, near zero |
| 101.5 | 0.05 | 0.38 | Taking off |
| 106.5 | -0.01 | 0.12 | Stable hover |
| 111.5 | 0.06 | 0.05 | Stable |
| 121.5 | -0.09 | 0.08 | Stable |
| 131.6 | 0.25 | -0.11 | Landing |

PD stays within ±0.3m throughout — no drift, no divergence.

## Altitude Hold Performance

### log7 — Best Performance (VRFB=0)

Steady hover at ~3m for 40 seconds with **0.04m stdev** (±0.15m range). This is excellent
altitude hold for a baro-only vehicle with no velocity aiding. CRt mean = 0.0 cm/s (zero
drift). ThO = 0.113 (matches MOT_THST_HOVER perfectly).

### log8 flight 1 — Slow Upward Drift

Hover starts at ~1.2m, slowly drifts to ~1.9m over 40 seconds. Mean CRt = 2.7 cm/s
(slight upward bias). Stdev = 0.23m. The drift is mild and altitude control works correctly
(controller tracks the drifting estimate).

### log8 flight 2 — Wider Scatter with VRFB

Hover starts at ~1m, drifts upward to ~3m over 25 seconds then stabilizes. Mean CRt = 5.8
cm/s. Stdev = 0.38m. The upward drift is faster than flight 1, correlating with the loaded
VRFB values applying a frozen correction from boot.

### Alt vs BAlt Offset

| Flight | Alt-BAlt offset (steady hover) | Notes |
|--------|-------------------------------|-------|
| log7 | ±0.15m (converged) | VRFB=0, no frozen correction |
| log8 flt1 | +0.3 → +0.5m (growing) | VRFB=0 at boot, learning in-flight |
| log8 flt2 | +0.3 → +1.25m (growing) | VRFB loaded from flt1. Frozen correction shifts EKF vs baro |

The growing Alt-BAlt offset in log8 correlates with the VRFB frozen correction. IMU0
has VRFB_Z = -0.1 m/s² (EKF sees less gravity = estimates higher altitude than baro reads).
The baro slowly pulls the estimate back, but with the heavy noise floor from EK3_GND_EFF_DZ=-8
during ground effect and normal baro noise afterward, convergence is slow.

## Ground Effect Handling

Landing at 130s in log8 flight 2 shows the noise floor working correctly:

| Time | Alt (EKF) | BAlt (baro) | Notes |
|------|-----------|-------------|-------|
| 128.0 | 2.36 | 1.39 | Descending |
| 130.2 | 1.27 | **-7.46** | Ground effect: 8.7m baro spike |
| 131.3 | 0.27 | -2.12 | EKF tracks through ground effect |
| 133.3 | 0.16 | -0.23 | Settled on ground |

The baro spikes to -7.46m (ground effect prop wash), but the EKF altitude only drops
to 1.27m — the noise floor (variance=64 from GND_EFF_DZ=-8) correctly deweights the
corrupted baro reading. This is a significant improvement over the original code's fixed
4x scaler, which would have been insufficient for a 7.5m baro spike.

## VRFB Bias Learning

| Event | INS_ACC_VRFB_Z (IMU0) | INS_ACC2_VRFB_Z (IMU1) |
|-------|----------------------|------------------------|
| log7 boot | 0.000 | 0.000 |
| log7 disarm | -0.113 | +0.177 |
| log8 boot | -0.113 | +0.177 |
| log8 flt1 disarm | -0.101 | +0.122 |
| log8 flt2 disarm | -0.101 | +0.122 |

Observations:
- **IMU0 and IMU1 biases are in opposite directions** (-0.10 vs +0.12). This is unusual and
  suggests the IMUs have genuinely different AccZ offsets or the learning is capturing
  different states.
- VRFB values stabilized between flights 1 and 2 (identical at disarm). The 2-second
  filter is converging.
- The non-zero VRFB correlates with the increased altitude drift in log8 vs log7.
  Whether this helps or hurts depends on whether the hover bias is stable across
  flights. The widening Alt-BAlt offset in log8 flt2 suggests the frozen correction may
  be overcorrecting.

## Key Findings

1. **Re-ARM works correctly** — The v2 firmware fixes completely prevent the baro fusion
   blackout. Innovations, variance, and altitude estimate all behave normally through the
   second ARM in log8.

2. **Altitude hold quality is good** — log7 achieves 0.04m stdev in steady hover (baro-only,
   no velocity aiding). This is near the limit of what baro can provide.

3. **Ground effect noise floor effective** — 7.5m baro spike at landing handled correctly
   by the -8 noise floor, keeping EKF altitude error under 1m during ground effect.

4. **Slow upward altitude drift with VRFB** — Flights with loaded VRFB show 3-6 cm/s upward
   CRt vs 0 cm/s with VRFB=0. The frozen correction may be creating a small systematic error.
   This is not dangerous (the controller tracks the drift) but reduces hold precision.

5. **No EKF errors** — Zero timeouts, zero attitude errors, zero forced resets across all
   three flights. Clean EKF operation throughout.

## Recommendations

### Investigate
- **VRFB drift correlation** — The upward altitude drift in log8 correlates with non-zero
  VRFB values. Consider whether ACC_ZBIAS_LEARN=3 is appropriate for a baro-only vehicle,
  or if the learned bias from one hover altitude/temperature doesn't transfer well to the next.
- **Baro thermal drift** — Temperature drops 2-4°C during flight (motor cooling). This
  contributes to altitude drift and may interact with the VRFB correction.

### No Changes Needed
- The v2 firmware is working correctly for this vehicle
- EK3_GND_EFF_DZ=-8 is appropriate (noise floor handles 7.5m ground effect spikes)
- AltHold performance is good for a baro-only platform

## See Also
- [logtc5_1 analysis](logtc5_1.md) — log4.bin crash analysis (original firmware)
- [TrashCopter5 vehicle notes](../vehicles/TrashCopter5.md)
