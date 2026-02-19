# logtc5_3 Analysis — TrashCopter5 log21.bin + log22.bin

## Metadata
- **Date**: 2026-02-19
- **Vehicle**: TrashCopter5 (MatekH743-bdshot)
- **Firmware**: V4.6.3v2-SFD (21057ce9) — SmallFastDrone-4.6-AltHoldv2 branch
- **Log files**: ./log21.bin (no flight), ./log22.bin (3 flights)
- **Frame**: QUAD/X_REV
- **Sensors**: GPS (u-blox NEO-M9N via DroneCAN), baro, dual IMU, no optical flow, no rangefinder
- **All sources disabled** — EK3_SRC1_POSXY=0, VELXY=0, VELZ=0, YAW=0 (baro-only altitude)
- **Purpose**: AltHold flight testing

## Key Configuration

```
# Altitude sources
EK3_SRC1_POSXY  = 0    (none)
EK3_SRC1_VELXY  = 0    (none)
EK3_SRC1_POSZ   = 1    (baro)
EK3_SRC1_VELZ   = 0    (none)
EK3_SRC1_YAW    = 0    (none)
EK3_RNG_USE_HGT = -1   (rangefinder disabled)
EK3_GND_EFF_DZ  = -8   (noise floor mode)
EK3_PRIMARY     = 0

# Control gains — CRITICAL
PSC_ACCZ_P      = 0.07     (62% of recommended 0.113 = MOT_THST_HOVER)
PSC_ACCZ_I      = 0.14     (62% of recommended 0.226 = 2×hover)
PSC_VELZ_P      = 4.0
PSC_VELZ_I      = 0.0      (default, no velocity integrator)
PSC_POSZ_P      = 1.0

# Hover/motor
MOT_THST_HOVER  = 0.113
MOT_THST_EXPO   = 0.60
PILOT_TKOFF_ALT = 0        (no auto-takeoff)
TKOFF_GNDEFF_TMO = 3.0
TKOFF_SLEW_TIME = 0.5

# Scheduler
SCHED_LOOP_RATE = 200
INS_GYRO_RATE   = 2        (2 kHz gyro sampling)
INS_FAST_SAMPLE = 3

# Batch logging + FFT (heavy logging)
INS_LOG_BAT_MASK = 3       (both IMUs)
INS_LOG_BAT_CNT  = 2048
INS_LOG_BAT_LGIN = 10
FFT_ENABLE       = 1
FFT_NUM_FRAMES   = 5
SCR_ENABLE       = 1

# Z-bias
INS_ACC_VRFB_Z   = 0.0
INS_ACC2_VRFB_Z  = 0.0
ACC_ZBIAS_LEARN  = 3
```

## Flight Summary

### Log 21 — No Flight
- Boot, mode changes (Stabilize → Loiter → AltHold), two failed arm attempts:
  - "Arm: Throttle (RC3) is not neutral" at 22.9s and 40.5s
- Short log (17.9 MB, ~40s). No CTUN altitude change, no motor output.

### Log 22 — Three Flights

| Flight | ARM (s) | DISARM (s) | Duration | Key Events |
|--------|---------|------------|----------|------------|
| 1 | 20.2 | 83.2 | 63.0s | **Good AltHold** — 3.3m hover, responsive |
| 2 | 87.2 | 95.1 | 7.9s | Failed attempt — barely lifted, disarmed |
| 3 | 143.2 | 155.6 | 12.4s | **Bad flight** — laggy takeoff, 2.9m overshoot, panic |

All flights in AltHold (mode 2). Log22 is 63.1 MB (heavy from batch IMU logging: 16,751 ISBD entries).

## CPU Load Assessment — NOT the Problem

| Metric | Value | Status |
|--------|-------|--------|
| PM Load | 1.9-3.0% | Normal |
| PM NLon (long loops) | 0 | **Perfect** |
| PM MaxT | 5.2-5.4 ms | Normal for 200 Hz |
| NL (nominal loops/10s) | 2000 | 200 Hz target met |
| Loop overruns | 0 | No missed loops |
| ERR messages | None during flight | Clean |
| Rate adjustment | 2000→1973→1972→1971 Hz (gyro) | Trivial drift, normal |

**The batch IMU logging (INS_LOG_BAT_MASK=3), FFT, and scripting create a large log file
but do NOT overload the CPU.** The 200 Hz scheduler loop is light enough to handle everything.

## Flight 1 — Good Flight (20.2-83.2s)

### Takeoff (27.5s)

The pilot had RC3 at 1610 (well above mid-stick ~1500) when TAKEOFF_COMPLETE triggered at 27.5s.
ThO jumped instantly from 0.000 to **0.126** (hover throttle). The vehicle launched cleanly:

| Time | ThO | DAlt | Alt | BAlt | Notes |
|------|-----|------|-----|------|-------|
| 27.4s | 0.000 | -0.062 | -0.062 | -1.51 | Still landed |
| 27.5s | **0.126** | -0.065 | -0.066 | -2.65 | **Instant hover throttle** |
| 27.6s | 0.128 | -0.055 | -0.048 | **-11.49** | Peak ground effect! |
| 27.7s | 0.120 | -0.047 | -0.017 | -7.86 | Vehicle lifting |
| 27.8s | 0.119 | -0.041 | +0.014 | +1.04 | Airborne, GE clears |
| 28.5s | 0.110 | -0.035 | +0.039 | +1.27 | Climbing normally |

The baro ground effect peaked at **-11.49m** (worse than flight 3!) but the vehicle transitioned
through it in 0.3s because the controller immediately applied full hover throttle. DAlt-Alt error
never exceeded 0.1m during takeoff.

### Hover Phase (31-83s)

Good altitude hold at 2.4-4.4m. ThO oscillated between 0.07-0.16 (around hover 0.125).
The pilot actively flew for 63 seconds with responsive control. DAlt tracked Alt within ~0.3m
during steady hover.

## Flight 2 — Failed Attempt (87.2-95.1s)

The pilot armed but kept throttle low (C3=990) for 4 seconds, then slowly raised to 1477.
Motors never exceeded 1130. Vehicle barely got airborne — baro ground effect only -2.19m.
Disarmed after 8s. No significant altitude change.

## Flight 3 — Bad Flight (143.2-155.6s) — THE PROBLEM

### Sequence of Events

**Phase 1: Idle on ground (143.2-149.0s, 6 seconds)**
- ARM at 143.2s, C3=990 (throttle at minimum)
- Motors at idle (1030). ThO=0.000.
- EKF altitude slowly drifts: 0.003 → 0.130m

**Phase 2: Slow throttle ramp (149.0-150.5s)**
- Pilot raises C3: 990 → 1121 → 1243 → 1483
- **All below mid-stick (~1500)** → controller commands zero climb
- ThO stays at 0.000. Motors still at idle (1030).

**Phase 3: Stuck on ground with motors running (150.5-152.4s) — THE LAG**

| Time | C3 | ThO | DAlt | Alt | BAlt | Problem |
|------|-----|-----|------|-----|------|---------|
| 150.5s | 1523 | **0.007** | 0.146 | 0.128 | -1.88 | First throttle, tiny |
| 150.8s | 1591 | 0.004 | 0.230 | 0.133 | -2.71 | GE growing |
| 151.0s | 1648 | 0.016 | 0.328 | 0.141 | -2.56 | Still barely any throttle |
| 151.4s | 1789 | 0.053 | 0.719 | 0.152 | -2.67 | DAlt racing ahead |
| 151.6s | 1879 | 0.091 | 0.987 | 0.156 | **-6.65** | GE extreme, ThO < hover! |
| 151.8s | ~1900 | 0.105 | 1.321 | 0.165 | **-9.94** | Peak GE, ThO still < 0.125 |
| 152.0s | 1855 | 0.109 | 1.615 | 0.171 | +0.08 | GE clears, still won't fly |
| 152.4s | 1922 | 0.153 | 2.171 | 0.247 | +1.53 | Finally above hover, lifts |

**The vehicle sat on the ground for 2 seconds with the pilot at 3/4+ throttle because
ThO never reached hover throttle (0.125).** PSC_ACCZ_P=0.07 is so low that even with
a 2m altitude error, the controller can barely produce any throttle.

The pilot experienced this as **"laggy sticks"** — they're commanding maximum climb but
the vehicle won't take off.

**Phase 4: Uncontrolled overshoot (152.4-154.4s)**

When the vehicle finally lifted, there was a **2.5m DAlt-Alt error** accumulated from the lag:

| Time | DAlt | Alt | Error | C3 | ThO | Notes |
|------|------|-----|-------|-----|-----|-------|
| 152.4s | 2.17 | 0.25 | **1.92** | 1922 | 0.153 | Just airborne |
| 152.8s | 2.88 | 0.54 | 2.34 | 1347 | 0.144 | Pilot cuts throttle |
| 153.2s | 3.23 | 0.73 | 2.50 | **1084** | 0.123 | Pilot near idle! |
| 153.4s | 3.23 | 0.74 | **2.49** | **990** | 0.113 | Throttle at minimum |
| 153.8s | 3.03 | 1.62 | 1.41 | 990 | 0.082 | Still climbing! |
| 154.2s | 2.44 | **2.75** | -0.31 | 990 | 0.038 | Overshoots DAlt |
| 154.4s | 2.00 | **2.89** | -0.89 | 990 | 0.031 | Peak altitude |

The vehicle climbed to **2.89m** while the pilot had throttle at MINIMUM (990). The accumulated
altitude error from the ground effect phase drove the overshoot.

**Phase 5: Panic response (154.6-155.6s)**

The pilot went full throttle (C3=1479→1862→2011) as the vehicle was falling back,
then the vehicle landed hard at 155.4s (baro spike -0.44m = landing impact).

### Why Flight 1 Worked But Flight 3 Didn't

The critical difference was **takeoff technique**, not the vehicle or firmware:

| Factor | Flight 1 | Flight 3 |
|--------|----------|----------|
| C3 at takeoff trigger | **1610** (well above mid) | **1523** (barely above mid) |
| ThO at takeoff | **0.126** (instant hover) | **0.007** (nearly zero) |
| Time to ThO > hover | **0.0s** (instant) | **1.9s** (slow ramp) |
| Peak baro ground effect | **-11.49m** (worse!) | -9.94m |
| Time in ground effect | **0.3s** | **2.0s** |
| Max DAlt-Alt error | **0.1m** | **2.5m** |
| Result | Clean takeoff | Overshoot to 2.9m |

**Flight 1 had WORSE ground effect (-11.49m vs -9.94m) but succeeded because the pilot
already had the stick well above mid-stick, causing instant hover throttle application.**

## Root Cause Analysis

**This is NOT a CPU overload issue. It is a control gain issue.**

### Primary cause: PSC_ACCZ_P = 0.07 is too low for takeoff

The ArduCopter tuning guide recommends PSC_ACCZ_P = MOT_THST_HOVER and PSC_ACCZ_I = 2×hover.
With MOT_THST_HOVER = 0.113, the recommended gains are P=0.113, I=0.226. The current P=0.07
is **62% of recommended** — adequate for steady-state hover (as flight 1 demonstrated) but
insufficient for the transient demands of takeoff:
- The controller ramps throttle too slowly during the critical ground-to-air transition
- With a slow throttle ramp, the vehicle spends 2+ seconds in the ground effect zone
- The accumulated DAlt-Alt error (2.5m) then drives a violent overshoot once airborne

### Contributing factors

1. **PSC_VELZ_I = 0.0** (default) — No velocity integrator. Adding some VELZ_I (0.5-1.0)
   would help the velocity loop converge faster during transients like takeoff and descent
   arrest, even though the ACCZ_I integrator handles steady-state.

2. **PILOT_TKOFF_ALT = 0** — No auto-takeoff. With auto-takeoff enabled (e.g. 100 = 1.0m),
   the takeoff code applies a throttle boost that gets the vehicle off the ground quickly,
   bypassing the slow PSC_ACCZ ramp. This would completely avoid the ground effect
   accumulation problem.

3. **No velocity source (EK3_SRC1_VELZ=0)** — The EKF has no way to cross-check altitude
   rate. It relies entirely on baro altitude, which is corrupted by ground effect.

4. **No rangefinder (EK3_RNG_USE_HGT=-1)** — No baro-independent altitude source for
   low-altitude flight.

5. **Slow throttle technique in flight 3** — Ramping C3 from 990 through 1500 slowly means
   the controller spends a long time at very low throttle output, extending the ground
   contact phase and maximizing ground effect exposure.

## Post-Flight EKF Failure

After flight 3 disarm (155.6s), the EKF deteriorated rapidly:

| Time | Event |
|------|-------|
| 155.6s | DISARM |
| 159.5s | PreArm: Gyros inconsistent |
| 171.5s | PreArm: Need Alt Estimate |
| 171.5s | PreArm: EKF attitude is bad |
| 171.5s | PreArm: AHRS: EKF3 still initialising |

XKF4 SP (position innovation) climbed from 0 to 1.30 in the 5 seconds after disarm,
indicating the EKF state was severely corrupted by the flight. The baro ground effect
at landing (-0.44m dip then +1.53m rebound) pushed residual errors into the EKF state.

## Recommendations

1. **Set PSC_ACCZ_P = MOT_THST_HOVER = 0.113, PSC_ACCZ_I = 0.226** — per the ArduCopter
   tuning guide (P = hover throttle, I = 2× hover throttle). This is a 61% increase from
   the current 0.07/0.14 and should eliminate the slow takeoff ramp.

2. **Try PSC_VELZ_I = 0.5-1.0** — currently at default 0.0. Adding velocity integration
   would help the velocity loop converge faster during transients like takeoff and descent
   arrest.

3. **Enable auto-takeoff**: `PILOT_TKOFF_ALT = 100` (1.0m). This applies a throttle boost
   during takeoff that gets the vehicle off the ground quickly, avoiding the slow ramp
   through the ground effect zone.

4. **Takeoff technique**: Until gains are fixed, raise the throttle stick decisively above
   mid-stick before arming, or raise quickly after arming. Don't slowly ramp through the
   mid-stick transition zone.

## See Also
- [logtc5_2](logtc5_2.md) — previous TrashCopter5 session, v2 firmware validation (3 good flights)
- [logtd_72](logtd_72.md) — TD-Matek-5 with PSC_ACCZ_P=0.138 (similar low-gain issue, milder)
