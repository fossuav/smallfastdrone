# logm2_13 Analysis — TD-MicoAir-2 log13.bin

## Metadata
- **Date**: 2026-02-19
- **Vehicle**: TD-MicoAir-2 (MicoAir743v2)
- **Firmware**: V4.6.3v2-SFD (21057ce9) — SmallFastDrone-4.6-AltHoldv2 branch
- **Log file**: ./log13.bin
- **Frame**: QUAD/X
- **Sensors**: Optical flow (FLOW_TYPE=5), baro, dual IMU, rangefinder (MAVLink, 7m max)
- **No GPS, no compass** — fully GPS-denied
- **Two flights**: AltHold testing with mid-flight Stabilize switches
- **Purpose**: Continued AltHold testing, VRFB learning validation

## Key Configuration

Same as [logm2_12](logm2_12.md), with VRFB carried from log12 flight 3:
```
EK3_GND_EFF_DZ   = 7     (deadzone mode)
EK3_RNG_USE_HGT  = -1    (rangefinder disabled)
PSC_ACCZ_P       = 0.381
MOT_THST_HOVER   = 0.381
PILOT_TKOFF_ALT  = 150
TKOFF_GNDEFF_TMO = 3.0
INS_ACC_VRFB_Z   = 0.021  (carried from log12)
INS_ACC2_VRFB_Z  = 0.048  (carried from log12)
```

## Flight Summary

| Flight | ARM (s) | DISARM (s) | Duration | Modes | Key Events |
|--------|---------|------------|----------|-------|------------|
| 1 | 7.7 | 19.3 | 11.6s | AltHold (5s) → Stabilize (7s) | EKF altitude divergence, pilot rescues |
| 2 | 27.1 | 45.5 | 18.4s | AltHold (10s) → Stabilize (9s) | Longest hover, significant VRFB learning |

Both flights follow the same pattern: ARM in AltHold → pilot switches to Stabilize
mid-flight when altitude control proves unreliable → manual flight → land and DISARM.

## Height Datum Reset

Works correctly at each ARM:

| ARM | Alt Before → After |
|-----|-------------------|
| #1 (7.7s) | -0.126 → 0.000 |
| #2 (27.1s) | 0.163 → -0.009 |

## Flight 1 Detailed Analysis (7.7-19.3s)

### Takeoff (9.5-10.3s)

- EV 15 (AUTO_ARMED) at 9.5s — pilot raises throttle above mid-stick
- Boost phase: RC3 peaks at 2011 (full) for 0.2s, then settles at 1490
- BARO ground effect peak: **-8.52m** at 10.3s (comparable to log12)
- Positive reversal: +5.0m at 10.5s, climbing to +7.1m by 10.9s
- EV 28 (TAKEOFF COMPLETE) at 10.3s

### EKF Altitude Divergence (10.5-12.5s)

The EKF altitude estimate is fundamentally wrong during this phase. While the vehicle
is climbing, the EKF thinks it's descending:

| Time | DAlt (target) | CTUN.Alt (EKF) | BARO.Alt | Vehicle actual |
|------|---------------|----------------|----------|---------------|
| 10.5s | -0.212 | -0.252 | +5.0 | Climbing |
| 11.0s | 0.094 | -0.254 | +7.2 | At ~0.5m |
| 11.5s | 0.618 | -0.217 | +8.2 | At ~1.0m |
| 12.0s | 0.825 | -0.466 | +10.3 | At ~1.5m |
| 12.2s | 0.676 | **-0.614** | +11.0 | At ~1.5m |

The DAlt (desired altitude) increases toward the 1.5m PILOT_TKOFF_ALT target, but CTUN.Alt
(the EKF's altitude estimate) stays negative. The position controller sees a 1.3m error
and commands maximum throttle to try to climb, causing the actual vehicle to overshoot.

### Mode Switch to Stabilize (12.5s)

At 12.2s, the pilot switches CH6 from 1500 (AltHold) to 987 (Stabilize). This was a
rescue — the altitude controller was commanding climb while the vehicle was already
overshooting. In Stabilize mode the pilot regains manual throttle control.

After Stabilize switch: pilot reduces throttle to ~1150-1280 range, manually controlling
descent. The EKF altitude continued diverging (reaching -7.0m at 15.4s) but this no
longer matters in Stabilize mode.

### XKF1 PD Divergence

The two EKF cores develop dramatically different altitude estimates:

| Time | IMU0 PD | IMU1 PD | Difference |
|------|---------|---------|------------|
| 10.3s | 0.017 | 0.003 | 0.01m |
| 11.0s | 0.094 | -0.248 | 0.34m |
| 12.0s | 0.504 | -0.466 | 0.97m |
| 13.0s | 0.122 | -1.096 | 1.22m |
| 15.0s | -0.088 | -2.344 | 2.26m |
| 18.0s | 0.045 | -1.058 | 1.10m |

IMU1 (the preferred core, EK3_PRIMARY=1) had dramatically worse altitude tracking. Its PD
went to -2.3 (thinking the vehicle was 2.3m above datum) while IMU0 stayed closer to
reality. This may be because each core processes baro innovations differently through their
independent delay buffers.

## Flight 2 Detailed Analysis (27.1-45.5s)

### Takeoff (28.6-29.1s)

- ARM at 27.1s, throttle at ~1515 (above mid-stick)
- Baro ground effect peak: **-7.04m** at 29.0s
- Positive reversal: +4.55m at 29.1s, then +6.2m by 29.5s
- Takeoff smoother than flight 1 (less aggressive throttle)

### AltHold Phase (29-36.7s)

The EKF altitude tracking improved in flight 2 compared to flight 1. CTUN.Alt tracked
closer to DAlt, and the vehicle reached the 1.5m hover target. However, the baro readings
remained extreme (7-11m while hovering at 1.5m).

From 31-36s: DAlt stable at 1.48m, CTUN.Alt converging toward 1.48m. The pilot held
throttle at ~1533 (near hover). This is the longest sustained AltHold hover across both logs.

### Mode Switch to Stabilize (36.7s)

At 36.5s, CH6 switches from 1500 to 987. The pilot reduces throttle to 1397→1258, then
gradually increases to 1402-1412 for level flight in Stabilize.

The XKF1 PD data shows continued EKF core divergence after the mode switch:

| Time | IMU0 PD | IMU1 PD | Difference |
|------|---------|---------|------------|
| 29.0s | 0.003 | -0.010 | 0.01m |
| 33.0s | -0.539 | -0.339 | 0.20m |
| 37.0s | -1.122 | -0.300 | 0.82m |
| 40.0s | -0.512 | +1.852 | 2.36m |
| 43.0s | -1.394 | +1.378 | 2.77m |
| 45.0s | -1.354 | +2.002 | **3.36m** |

By disarm, the two cores disagreed by **3.4m** on altitude. This extreme divergence
confirms the baro data is corrupting the EKF altitude estimate differently in each core.

### VRFB Learning — Significant Change

| Event | IMU0 | IMU1 | Notes |
|-------|------|------|-------|
| Boot (from log12) | 0.021 | 0.048 | |
| Disarm flight 1 (19.3s) | 0.021 | 0.048 | No change — only 5s in AltHold |
| Disarm flight 2 (45.5s) | **-0.178** | **0.109** | **Large change** |

IMU0 VRFB changed by **-0.199 m/s²** (0.021 → -0.178). This is the largest single-flight
VRFB change seen across all vehicles. IMU1 changed by +0.061 (0.048 → 0.109), a moderate
increase.

The large IMU0 change suggests the accelerometer Z-bias was significantly miscalibrated.
A VRFB of -0.178 means the hover Z-acceleration measurement was off by 0.178 m/s², which
at MOT_THST_HOVER=0.381 represents a ~2% hover throttle equivalent error. The 10s AltHold
hover in flight 2 allowed the learning algorithm enough data to converge.

## Baro Ground Effect Comparison

| Vehicle | Negative Peak | Positive Peak | Total Swing | EK3_GND_EFF_DZ |
|---------|--------------|---------------|-------------|----------------|
| TD-MicoAir-2 (this) | -8.5m | +13.1m | **21.6m** | 7 (deadzone) |
| TD-Matek-5 | -9.1m | ~0m | **9.1m** | -8 (noise floor) |

The MicoAir-2 produces bidirectional swings 2.4x larger than the Matek-5's unidirectional
spikes. This is likely due to the smaller frame creating more chaotic propwash interaction
with the barometer.

## Arming Issues

- At 49.0s: "PreArm: AHRS: EKF3 still initialising" — tried to re-arm too soon after
  flight 2 disarm. The EKF needs time to reinitialize after the extreme state divergence.

## Key Findings

1. **EKF altitude fundamentally wrong in AltHold** — the EKF thinks the vehicle is
   descending while it's climbing. The baro is the only altitude source (rangefinder
   disabled in EKF), and it's reading +6-13m due to ground effect while the vehicle
   is at 1-2m. The 7m deadzone doesn't help because:
   - During the deadzone phase (0-7m), the EKF relies on dead reckoning which drifts
   - When baro readings breach 7m, the EKF suddenly fuses wildly incorrect values

2. **Pilot rescues vehicle by switching to Stabilize** in both flights — this is the
   correct response to unreliable altitude hold. The vehicle is perfectly flyable in
   Stabilize mode.

3. **EKF core divergence** reaches 3.4m by end of flight 2 — the two cores process the
   extreme baro noise differently, producing increasingly divergent altitude estimates.

4. **VRFB learning works** — the largest single-flight change seen (-0.199 m/s² on IMU0),
   made possible by 10s of AltHold hover in flight 2.

5. **Height datum reset works** correctly across both ARM events.

## Recommendations

1. **Switch to noise floor mode**: `EK3_GND_EFF_DZ = -14` (negative = noise floor). This
   de-weights the baro with a 14m observation noise floor rather than creating a hard
   deadzone boundary. The noise floor mode:
   - Doesn't create a hard boundary that causes altitude jumps
   - Continuously de-weights baro rather than alternating between "ignore" and "trust"
   - Was proven effective on TD-Matek-5 with its 9m ground effect

2. **Enable rangefinder in EKF**: `EK3_RNG_USE_HGT = 3`. The rangefinder provides an
   altitude measurement immune to propwash effects. Even with its 7m range limit,
   it would provide a reliable altitude anchor during the critical takeoff and
   low-altitude hover phases where ground effect is worst.

3. **Consider both changes together** — noise floor for baro rejection + rangefinder for
   a reliable altitude source. This is the configuration that works on TD-Matek-5.

## See Also
- [logm2_12](logm2_12.md) — previous flight session, same vehicle, first v2 AltHold tests
- [logm2_6](logm2_6.md) — earlier AltHold testing on v1 firmware
- [logtd_72](logtd_72.md) — TD-Matek-5 with noise floor mode (working AltHold)
- [logtd_74](logtd_74.md) — TD-Matek-5 height datum reset validation
