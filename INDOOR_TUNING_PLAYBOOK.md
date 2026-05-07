# Indoor Copter Tuning Playbook

**Version:** 1.0.0
**Applies to branch:** `SmallFastDrone-4.7-beta` (verified at `41c83011e2`)
**Upstream base:** ArduCopter 4.7-beta

Parameter names and ranges in this document have been cross-checked against
the source on the branch above. If you are flying a different ArduPilot
release — including upstream 4.6 or master — some parameter names and
behaviours will differ (notably the `PSC_*Z` → `PSC_D_*` rename, the
`PILOT_TKOFF_ALT` → `PILOT_TKO_ALT_M` rename, and the dedicated `VALT`
flight mode).

A field reference for diagnosing and tuning small ArduCopter multirotors flying
indoors (confined space, low altitude, no GPS or marginal GPS, typically
< 5 m AGL with rangefinder + optical flow). Distilled from ~80 indoor flight
logs and the EKF3/AltHold work that followed.

The playbook assumes you have:
- A DataFlash `.bin` log from the flight that misbehaved.
- Read access to MAVExplorer / pymavlink-style log inspection (`log-analyze` skill, Mission Planner, MAVExplorer).
- The vehicle's current parameter file.

It is **purely a tuning guide**. It does not change any source code.

When you update the playbook, bump the version using semver: patch for
typos and small clarifications, minor for adding/restructuring sections,
major if recommendations change in a way that would invalidate prior
parameter sets derived from this document.

---

## 1. Quick-start configurations

Pick the row that matches the vehicle's sensor stack, then refine using the
diagnostic flow in §2.

### 1.1 Indoor with rangefinder + optical flow (recommended)

The robust indoor configuration. Rangefinder anchors AGL, flow gives velocity,
baro is deweighted hard during ground effect. Position-hold and altitude-hold
both work.

| Parameter | Value | Why |
|-----------|-------|-----|
| `EK3_SRC1_VELXY` | `5` (OpticalFlow) | Primary horizontal velocity source |
| `EK3_SRC1_POSXY` | `0` | No absolute horizontal aiding indoors |
| `EK3_SRC1_VELZ`  | `0` | EKF derives Z-velocity from baro+IMU |
| `EK3_SRC1_POSZ`  | `1` (Baro) | Baro is the EKF height observation; rangefinder anchors via blending or HAGL |
| `EK3_RNG_USE_HGT`| `-1` **or** `70` | See §4.1 — `-1` disables blend, `70` keeps rangefinder primary across full indoor envelope |
| `EK3_GND_EFF_DZ` | `-8` (negative mode) | Use \|value\| as baro noise floor variance during ground effect. Strong deweighting (R=64 m², K≈0.008). Documented `@Range: -10 .. 10` |
| `BARO1_THST_SCALE` | calibrate (§4.3) | Throttle-to-baro pressure compensation, Pa per unit throttle. Documented `@Range: -300 .. 300` but firmware does not clamp; values down to −800 are in use on ducted airframes |
| `BARO_THST_FILT` | `1.0` Hz | Low-pass on throttle before correction; reduces transients during rapid throttle changes. **Note:** the parameter has no per-instance number — it is `BARO_THST_FILT`, not `BARO1_THST_FILT` |
| `TKOFF_GNDEFF_ALT` | `5` m | Ground-effect protection altitude (uses HAGL when available). `@Range: 0 .. 5` |
| `TKOFF_GNDEFF_TMO` | `3.0` s | Minimum ground-effect window post-arm. `@Range: 0 .. 5`; firmware caps at 5 s regardless of stored value |
| `INS_ACC_VRFB_Z` | `0` initially | Vibration rectification bias on IMU 0 Z axis; let it learn. Per-IMU: `INS_ACC_VRFB_Z`, `INS_ACC2_VRFB_Z`, `INS_ACC3_VRFB_Z`. `@Range: -0.5 .. 0.5` m/s² |
| `ACC_ZBIAS_LEARN`| `3` (Learn+Use) or `7` (Learn+Use+InhibitOnGround) | Bitmask: bit0=Learn and save on disarm, bit1=Use saved values, bit2=Disable EKF zero-velocity ground learning while disarmed |
| `EK3_MAG_CAL`    | `7` (GROUND_AND_INFLIGHT) | Learn on ground, freeze through takeoff/climb, resume after yaw alignment |
| `COMPASS_MOTCT`  | `2` (current) | Motor-current compensation; calibrate per airframe |
| `RNGFND1_MIN_CM` | as low as the sensor permits (2–3 cm for ToF lasers) | Avoid OutOfRangeLow during low hovers |
| `ARMING_CHECK`   | exclude GPS-related bits indoors | See §3.6 |

### 1.2 Indoor, baro-only (no rangefinder, no flow)

Performance is fundamentally limited by the baro alone; this is a fall-back.

| Parameter | Value | Why |
|-----------|-------|-----|
| `EK3_SRC1_POSZ` | `1` (Baro) | Only available height source |
| `EK3_GND_EFF_DZ` | `4.0` (positive mode) | **Do not** use the negative noise floor — the baro is the only altitude reference; deweighting it strands the EKF on IMU integration and it ground-sucks. Positive `4.0` keeps the existing 4× scaler |
| `BARO1_THST_SCALE` | calibrate (§4.3) | Compensates the dominant disturbance |
| `BARO_THST_FILT` | `0.5–1.0` Hz | More filtering helps because there is no second source to absorb transients |
| Flight mode | use **VALT** (mode 29) instead of ALT_HOLD | Velocity-controlled altitude hold — see §1.4. Robust to baro corruption during manoeuvres because `pos_desired` is rebound to current position whenever the stick is off-centre |
| `PSC_D_ACC_P` | match `MOT_THST_HOVER` (default formula in `init_z_controller`) | Mismatched gains amplify ground-effect overshoot. Was `PSC_ACCZ_P` before the 4.7 rename — see §6 |
| Physical | foam shroud over baro, isolate from prop wash | The real fix |

### 1.3 Drop / throw launch indoors

Add to §1.1:

| Parameter | Value | Why |
|-----------|-------|-----|
| `THROW_TYPE` | `1` (drop) or `0` (upward throw) | Match launch method |
| `THROW_NEXTMODE` | `1` (ACRO), `2` (ALT_HOLD), `5` (LOITER), `18` (Throw), `29` (VALT) | Mode after recovery. Note: the parameter's documented `@Values` list does not yet enumerate ACRO and VALT; both are accepted by the dispatch in `mode_throw.cpp` on this branch |
| `THROW_DROP_AG` | `2` | Recovery aggressiveness multiplier. `@Range: 1.0 .. 4.0` |
| `THROW_DROP_CNF` | `0` (100 ms minimum) for hand drops | Confirmation time, decoupled from `THROW_ALT_DCSND`. `@Range: 0 .. 5 s` |
| `THROW_ALT_DCSND` | `0` for hand drops, `3–5` m for high releases | Controlled descent before altitude arrest |
| `THROW_SRC_INI` | `2` or `3` if flow-only — the no-aiding source set | Avoid EKF variance growth from tumbling-flow |
| `THROW_SRC_SET` | `1` | Restore flow source set on completion |
| `THROW_YAW_TYPE` | `0`–`3` | New on this branch. `0`=hold release yaw, `1`=face direction of travel, `2`=face reverse direction, `3`=face `THROW_YAW_DEG` absolute heading |
| `THROW_YAW_DEG` | only used when `THROW_YAW_TYPE=3` | Absolute compass heading target, degrees from North |
| `MOT_SPOOL_TIME` | `0.05` if minimum altitude loss matters | Faster spool, more attitude authority sooner |

### 1.4 Velocity-control altitude hold (VALT mode)

For pilots who prefer rate-style altitude stick on small indoor vehicles, use
the **VALT** flight mode (`Mode::Number::VALT = 29`) directly. On this branch
this is its own flight mode (`ArduCopter/mode_valt.cpp`), not a bitmask
option on AltHold. Earlier work on the 4.6 branch implemented the same
behaviour as `ALTH_OPTIONS` bit 0, but on 4.7 that parameter does not exist —
fly VALT instead.

Behaviour: at mid-stick the controller freezes `pos_desired` at the current
EKF altitude and the position-P loop holds; off-mid-stick the stick commands
a climb/descent rate directly. Surface tracking does not run in VALT, so it
cannot fight the pilot's stick. VALT is more robust to baro corruption during
manoeuvres than default AltHold because it removes the position-error
build-up that standard AltHold has to close when sticks return to centre.

VALT is gated behind `MODE_VALT_ENABLED` (default ON for full Copter builds,
may be off in space-constrained AP_Periph or minimum-feature builds — check
`build-options` if it does not appear in your mode list).

---

## 2. Log diagnostic flow

A single log carries enough signal to make most of the calls below. Walk
through these checks in order — earlier checks unblock later ones.

### 2.1 Sanity checks (always first)

| Question | Log fields | Pass/fail |
|----------|------------|-----------|
| Did the EKF stay healthy? | `MSG` events for `EKF Failsafe`, `EKF variance`, `EKF primary changed`. `XKFS.SS/FS/TS` bits. | No failsafe, no late-flight lane switches |
| Did vibration stay below threshold? | `VIBE.VibeX/Y/Z` | Below 30 m/s² peak, below ~15 m/s² sustained |
| Were there clipping events? | `VIBE.Clip0/1/2` | Should not increment in flight |
| Was the loop rate met? | `PM.Load`, `PM.MaxT`, `PM.LR` | Load < 70 %, MaxT < 2× nominal period |
| Did the compass behave? | `MAG.MagX/Y/Z`, `XKF4.SM` (mag innovation test ratio) | SM < 1.0, no in-flight yaw resets after the first |

If any of these fail, **fix them before tuning altitude or position**. A
contaminated EKF will make every other measurement look like a tuning problem.

### 2.2 Altitude-hold root-cause flow

```
              Symptom: BAlt diverges from RFND
                            │
            ┌───────────────┴────────────────┐
       at takeoff                       in flight
            │                                │
   ┌────────┼────────┐                ┌──────┼──────┐
  GE jump   datum   slow            thermal  prop   terrain
  (BAlt    not     spool             drift   wash   offset
  -5 to    reset                                    feedback
  -8 m)
   │        │        │                  │      │      │
   §4.4    §4.5    §4.6                §4.7   §4.3   §4.1
```

Concrete checks for each branch:

**A. Ground-effect jump at takeoff** — plot `BARO.Alt` vs `RFND.Dist1` and
`CTUN.ThO` for the first 5 s after arm. If `BAlt` swings −4 m to −8 m below
zero while the vehicle is still on the ground (RFND ~ 0.05–0.20 m), that is
prop-wash ground effect, not a sensor fault. The size of the swing
tells you what `BARO1_THST_SCALE` should be (§4.3) and whether the noise
floor is sufficient (§4.4).

**B. Datum not reset** — look for the `EKF_ALT_RESET` event in `MSG` near
arming time, or check that `XKF1.PD` ≈ 0 immediately after `EV id=10`
(ARMED). If `PD` is non-zero at arm, `resetHeightDatum()` did not run or was
blocked. Causes: home set long before arm (>30 s baro drift), `activeHgtSource
== RANGEFINDER` from blending while still on the ground (fixed in current
firmware), or rare race with the on-ground motion gate. Workaround: arm
immediately after setting home; keep the vehicle still until `XKF1.PD ≈ 0`
shows.

**C. Slow-spool drift before liftoff** — if `time_flying_ms` stays at 0 for
multiple seconds after `TKOFF_GNDEFF_TMO` expires, the post-TMO window has
the EKF chasing prop-wash baro at full weight. Symptom: PD drifts to −0.3 to
−0.5 m on the ground. Mitigations: bump `TKOFF_GNDEFF_TMO` to 10, take off
briskly, or use `EK3_RNG_USE_HGT > 0` so the rangefinder anchors PD via the
HAGL Kalman filter.

**D. Thermal drift during hover** — plot `BARO.Temp` and `BARO.Alt` together.
A 5–20 °C drop during hover is normal (prop airflow cools the FC) and produces
0.5–1.5 m of altitude drift over a 1–3 minute hover that **is not** correctable
by `THST_SCALE` (it is not throttle-correlated). Mitigations: physical
insulation of the FC, recalibrate `INS_TCAL` properly (§4.8), or fly with
rangefinder-anchored height.

**E. Prop-wash steady-state offset** — at hover, plot `BARO.Alt − RFND.Dist1`
vs `CTUN.ThO`. A linear, throttle-correlated offset is what `THST_SCALE`
corrects (§4.3). Negative correlation (more throttle → lower BAlt) is the
normal case and the sign convention expects negative `THST_SCALE`.

**F. Terrain-offset feedback (rangefinder vehicles only)** — when
`EK3_RNG_USE_HGT > 0`, the height-source switching uses EKF altitude. If the
EKF altitude is itself baro-contaminated, the rangefinder gets locked out
above the threshold, producing a positive feedback loop. Symptoms: terrain
offset (`XKF6.HAgl` minus measured RFND) wanders ±1 to ±2 m; flow velocity
appears 2–4× larger than reality; vehicle leans backward in LOITER as if
chasing phantom motion. Fix: `EK3_RNG_USE_HGT = -1` (disables blend) or `70`
(rangefinder is primary across the full indoor envelope). The current
firmware also routes the GE altitude check through HAGL when available, which
breaks the secondary feedback loop.

### 2.3 Attitude oscillation check

```
ATC_RAT_RLL_*, ATC_ANG_*  →  RATE.RDes, RATE.R, RATE.RTar, RATE.ROut
                              ATT.DesRoll, ATT.Roll
```

Compute, over 10–30 s of stable hover:

```
overshoot_ratio = std(RATE.R) / std(RATE.RDes)
```

Interpretation:

| Ratio | Reading |
|-------|---------|
| < 1.0 | Rate loop damped, follows demand |
| 1.0–1.3 | Healthy |
| 1.3–1.5 | Marginal — watch for limit cycle |
| > 1.5  | Limit cycle present — `ANG_P × plant_gain` too high |

If the ratio is > 1.3, also FFT `RATE.RDes` and `RATE.R`. A coherent peak in
both at 5–15 Hz with `Tar→Act` lag of 15–30 ms is the angle-rate limit cycle:
the angle controller is commanding the rate loop to chase its own ringing.

Asymmetric airframes (battery aligned with one axis, motors wider on the
other) produce different plant gains on roll vs pitch. Autotune does not
fully compensate; you typically need a larger `ANG_P` reduction on the axis
that oscillates first.

**Fix order** (smallest change first):
1. Reduce `ATC_ANG_RLL_P` and/or `ATC_ANG_PIT_P` by 25–30 % on the
   oscillating axis. This breaks the resonance loop without changing
   rate-loop bandwidth — at `ANG_P=20` a 1° error still commands 20 deg/s.
2. If oscillation persists, reduce `ATC_RAT_<axis>_P` by ~15 % and match
   `ATC_RAT_<axis>_I` to it.
3. Do **not** raise `ATC_RAT_*_FLTE` to suppress oscillation — it adds phase
   lag at the oscillation frequency and makes the limit cycle worse.
4. Do **not** rely on `ATC_RAT_*_SMAX` — its Dmod scales rate-loop P+D only,
   not angle P. The loop mismatch can drive instability.

### 2.4 Yaw consistency check

Plot `XKF1[0].Yaw` and `XKF1[1].Yaw` (or the `IMU_INSTANCE` equivalents).
Healthy: cores agree within 1–3° throughout flight. Diverging during takeoff
to >10° usually means motor magnetic interference is corrupting compass body
offsets faster than the EKF can fuse them.

Cross-check: plot raw `MAG.MagX/Y/Z` against `BAT.Curr` (or `CTUN.ThO`).
Strong correlation (|r| > 0.3) with current/throttle is the smoking gun.

Fixes:
- `EK3_MAG_CAL = 7` — learns on ground, freezes during takeoff and climb,
  resumes after the first in-flight yaw alignment.
- `COMPASS_MOTCT = 2` — current-based motor compensation. Calibrate it per
  airframe (Mission Planner: Compass-Mot wizard, current-based).
- `ATC_ANG_YAW_P` should usually be 4.5–6.0. Defaults near 17 chase compass
  noise aggressively and produce visible yaw twitchiness. An autotuned value
  much higher than that is a warning sign on a noisy compass.

### 2.5 Position / flow check (where present)

Plot `OF.flowX/Y` (rad/s), `OF.qual`, `OF.bodyX/Y`, `RFND.Dist1`,
`XKF1.VN/VE`. Healthy indoor:

| Metric | Healthy |
|--------|---------|
| `OF.qual` | > 100 average, > 50 instantaneous |
| `XKF1.VN/VE` during hover | ±0.1 m/s |
| Position drift over 60 s hover | < 3 m |

Failure modes:
- **Quality < 50** — too dark, too plain a floor texture, too high above the
  surface, or motion blur from rate excursions. Fix lighting, lower the
  vehicle, or add texture to the floor.
- **Quality OK but velocity wrong** — almost always rangefinder scaling /
  terrain offset. See §2.2 branch F.
- **Backward lean in LOITER** — terrain offset feedback (§2.2 F).

### 2.6 Compass calibration health (without flying)

A failing on-board `COMPASS_LEARN` repeats forever and never persists
acceptable offsets. Diagnose by raw `|M|` variance, not by GCS messages:

| Regime | What to do | What `|M|` should do |
|--------|------------|----------------------|
| Static (drone stationary, motors off, 30 s) | Sit still | std ≤ 5 mG (sensor noise floor) |
| Rotation (slow rotations through ±90° on each axis) | Move | |M| invariant, std ≤ 10 mG |

| Static std | Rotation std | Diagnosis | Fix |
|------------|--------------|-----------|-----|
| ≤ 5 mG | ≤ 10 mG, but `\|Ofs\|` huge (>1500 mG) | Fixed bias from motors / battery / magnetised steel — calibration handles it | Run cal, raise `COMPASS_OFFS_MAX` if needed |
| ≤ 5 mG | 30–100 mG | **Loose magnetic component** moving relative to compass during handling (e.g. battery connector flopping on its leads) | Find and secure / remove the loose part |
| 20–50 mG | similar | Active interference (current loop, switching regulator near compass) | Distance, ferrite chokes |
| ≤ 5 mG | ≤ 10 mG | Clean — orientation is the only remaining concern | Run the cal |

Compute raw `|M|` from `MAG.MagX/Y/Z` after undoing `COMPASS_ORIENT` and
adding back `Ofs`. The post-everything `MagXYZ` already has soft-iron and
offsets applied; for the variance test the rotation undo is what matters.

A re-cal performed on top of a loose component will accept *something* near
`COMPASS_CAL_FIT` and persist a wrong cal — diagnose first, then re-cal.

---

## 3. Common indoor failure modes and recovery

### 3.1 Vehicle hits the ceiling at takeoff

Either:
- **Aggressive feedforward**: `MOT_THST_HOVER` is much higher than the actual
  hover throttle. The Z-position controller's feedforward over-commands. Check
  `CTUN.ThO` during stable hover; if hover ThO is e.g. 0.07 but
  `MOT_THST_HOVER = 0.125`, the controller will spike to ~2× the real hover
  thrust at first motor engagement.
  - Fix: `MOT_THST_HOVER` to within ~10 % of measured hover ThO.
  - Fix: `TKOFF_SLEW_TIME = 1.0–1.5 s` to slow the motor ramp.
  - Fix: `PILOT_TKO_ALT_M = 1.0` for a controlled ramp. (Older firmware exposed this as `PILOT_TKOFF_ALT` in centimetres — on 4.7 it has been renamed to `PILOT_TKO_ALT_M` and is in metres. Existing param files are auto-converted on first boot.)
- **GE-induced setpoint shift**: Ground-effect baro contamination drives the
  EKF altitude below zero, and AltHold's ResetHeight or a similar correction
  shifts the target up to compensate.
  - Fix: `TKOFF_GNDEFF_TMO ≥ 3 s`, `EK3_GND_EFF_DZ = -8` on rangefinder
    vehicles, accurate `BARO1_THST_SCALE`.

### 3.2 Vehicle won't take off / sits on the ground at full throttle

EKF altitude target is below current altitude. Causes:
- Datum not reset at arm and baro drifted negative pre-arm.
- Ground-effect window too short and EKF chased the −5 to −8 m wash.
- Rangefinder out of range (very-low hover < `RNGFND1_MIN_CM`) and EKF fell
  back to corrupt baro.

Fixes: arm immediately after powering-up, `TKOFF_GNDEFF_TMO=5` (the
documented and firmware-clamped maximum on this branch — earlier-branch
notes referenced 10 s but the 4.7 firmware clamps at 5),
`RNGFND1_MIN_CM` set to the sensor's true minimum (2–3 cm for ToF lasers).

### 3.3 LOITER drifts backward / leans into nothing

Terrain-offset feedback (§2.2 F). The EKF altitude is wrong, so HAGL is wrong,
so flow velocity computed from rangefinder × angular rate is amplified or
inverted. The position controller fights phantom velocity by leaning the
vehicle, which produces real motion in the opposite direction.

Fix: `EK3_RNG_USE_HGT = -1` or `70`. Re-fly and verify XKF6.HAgl ≈ measured
RFND.

### 3.4 Vehicle "ground-sucks" near the floor

Baro reads severely low at low altitude (worse than mid-air ground effect).
Without an independent height source the positive feedback loop pulls the
vehicle down: descend → more ground effect → worse baro → descend more.

Fixes:
- Fit a rangefinder. This is the only structural fix.
- Until then: `EK3_GND_EFF_DZ = 4` (positive — keep baro authoritative; do
  **not** use a deweighted noise floor on a baro-only vehicle), `BARO_THST_FILT`
  to absorb transients, physical baro isolation.

### 3.5 EKF lane switches mid-flight

Most common indoor cause is compass core divergence from motor interference
(§2.4). Less common: GPS glitch (irrelevant indoors), large vibration spike
crossing innovation gates.

Diagnostic order:
1. Check `XKF4.SM` (mag innovation test ratio). > 1.0 sustained → compass.
2. Check `VIBE.VibeZ` and `VIBE.Clip*` near the switch time → IMU.
3. Check `XKF4.SH` (height innovation) → baro / source switching.

### 3.6 Pre-arm / EKF won't go ready indoors

If GPS-related arming checks are blocking arm:

```
ARMING_CHECK = 786390   # standard mask minus GPS bits — review against current ArduPilot bit definitions
```

Inspect the arming-check bit list in `AP_Arming::Check` for the current
release before disabling broad checks. A tighter, more sustainable indoor
mask disables only the GPS-position bits and keeps INS / baro / RC / compass
checks active.

If the EKF reports unhealthy on arming attempt:
- Confirm vehicle is stationary (`onGroundNotMoving`) — gyro and accel
  norms should be at the noise floor before arming.
- For "EKF3 core 0 unhealthy" after movement: trigger an EKF bootstrap reset
  (RC option 187), wait for the GCS confirmation, then arm. The bootstrap
  reset will refuse to run unless the vehicle is confirmed stationary.

### 3.7 Power-on while moving (carried, walking)

If the FC is powered on while being carried, gyro init runs against motion
and the EKF bootstrap converges with contaminated state. Symptoms: persistent
`EKF3 core unhealthy`, large yaw drift, pre-arm refuses repeatedly.

Recovery procedure:
1. Set the vehicle on a level stationary surface.
2. Wait several seconds for `onGroundNotMoving` to latch.
3. Trigger EKF bootstrap reset (RC option 187, or `EKF_RESET` aux switch).
4. Wait for "EKF bootstrap reset performed".
5. Arm normally with arming checks enabled.

---

## 4. Calibration and parameter procedures

### 4.1 `EK3_RNG_USE_HGT` selection

| Value | Behaviour | Use when |
|-------|-----------|----------|
| `-1` | Disable rangefinder height-source blending. Rangefinder still feeds the EKF; switching is suppressed. | Default for indoor with significant baro propwash. Eliminates terrain-offset feedback at the cost of leaving baro as the EKF height observation when above the (now-disabled) blend threshold. |
| `0–2` | Blend off / very low. Rangefinder anchors only at the lowest altitudes. | Outdoor with rangefinder for terrain-following only |
| `3–10` | Standard blending. Rangefinder primary below `(value/100) × RNGFND1_MAX_CM`. | Outdoor mixed |
| `70` | Rangefinder primary across (almost) full sensor range. Best indoor option when the rangefinder is reliable to the ceiling. | Indoor, where the entire flight envelope is below the rangefinder's max range |

The HAGL Kalman filter (compile-time enabled in current firmware) makes the
height-source switching baro-independent, which removes the original feedback
loop that motivated `-1`. On firmware with that fix, prefer `70` over `-1`
when the rangefinder is dependable indoors.

### 4.2 `EK3_GND_EFF_DZ` selection

```
DZ > 0  → existing 4× baro noise scaler. Default 4.0.
DZ < 0  → use |DZ| as the **noise floor in metres**, giving variance |DZ|².
         R = DZ² → Kalman gain ~0.005–0.01 → strong baro deweighting.
         Required for vehicles where baro propwash exceeds ~3 m.
```

Choose `DZ ≈ -(observed peak baro propwash in m)`. Examples:
- Small indoor quad with −4 m wash: `DZ = -5`.
- Ducted-frame quad with −8 m wash: `DZ = -8`.

**Do not** use the negative noise-floor mode on baro-only vehicles. Without
an independent height source the EKF runs nearly open-loop on IMU integration
during the GE window, drifts, and the controller follows.

### 4.3 `BARO1_THST_SCALE` calibration

Pa per unit throttle. Negative values are normal (more throttle → lower
BAlt).

Procedure:
1. Fly a stable hover with a working rangefinder, `RNG_USE_HGT > 0`, GE
   protection clear.
2. Extract `BARO.Alt` and `RFND.Dist1` over the steady-hover segment.
3. `baro_error_m = mean(BAlt − RFND)` (positive = baro reads too high).
4. `THST_SCALE = -(baro_error_m × 12 Pa/m) / hover_throttle`
5. Verify against multiple hover throttle levels — the relationship must be
   linear. If it isn't, the dominant disturbance is thermal (§4.7), not
   propwash.

Order of magnitude per airframe class:

| Class | Hover throttle | Typical `THST_SCALE` |
|-------|----------------|----------------------|
| Very-low-hover-throttle quad (T:W ≥ 8:1, indoor) | ~0.07–0.10 | −20 to −80 |
| Mid-hover-throttle indoor optical-flow quad | ~0.15–0.25 | −150 to −300 |
| Large indoor / overpowered indoor | ~0.30–0.45 | −500 to −800 |
| Ducted shroud (extra negative-pressure zone) | ~0.18–0.20 | −600 to −800 |

Do not copy a `THST_SCALE` between airframes. Calibrate per vehicle.

### 4.4 `BARO_THST_FILT`

Low-pass cutoff (Hz) on throttle before the thrust-pressure correction is
applied. `0` disables. **Note:** the parameter is `BARO_THST_FILT` (no
instance number) — distinct from `BARO1_THST_SCALE` which is per-baro.

| Setting | Effect |
|---------|--------|
| `0` | Sharp step changes during throttle transients — adds altitude pop |
| `0.1–0.5 Hz` | Heavy filtering. Indoor / low-bandwidth altitude controller. Removes altitude transients during stick-driven climbs |
| `1.0 Hz` | Default-ish indoor — smooth, ~1 s lag. **Recommended starting point indoors** |
| `2.0 Hz` | Mild — outdoor / responsive |

Combine with `THST_SCALE` calibration: the filter does not change the steady
state, only the transient.

### 4.5 `TKOFF_GNDEFF_ALT` and `TKOFF_GNDEFF_TMO`

| Parameter | Semantics |
|-----------|-----------|
| `TKOFF_GNDEFF_ALT` | Altitude (m) below which ground-effect protection is active. The check uses HAGL when available (rangefinder + AGL KF). |
| `TKOFF_GNDEFF_TMO` | Minimum time (s) ground-effect protection remains active after arm, regardless of altitude. |

Set `TKOFF_GNDEFF_ALT` to ~2–3× the airframe's measured ground-effect zone
(typically 1× rotor diameter). For a small indoor vehicle, 5 m is generous
and safe.

Set `TKOFF_GNDEFF_TMO` to be longer than the worst-case spool-to-liftoff
time. Default 0 is **not safe** — TMO=0 means ground-effect protection ends
the moment EKF altitude crosses `TKOFF_GNDEFF_ALT`, which can happen during a
prop-wash dip while the vehicle is still on the ground. Use 3 s as a sensible
indoor minimum. The firmware clamps the effective TMO to 5 s regardless of
the stored value, so `5` is the upper bound — slow-spool airframes that
cannot lift off within 5 s of throttle-up will need a different mitigation
(brisker takeoff technique, lower `MOT_SPOOL_TIME`, or rangefinder-anchored
height via `EK3_RNG_USE_HGT > 0`).

### 4.6 Datum reset on arm

The current firmware resets the EKF height datum unconditionally on arm
(rather than only when home is unset), and the reset itself flushes baro
delay buffers, output state, complementary filter state, baro offset, and
the height-pass timer. There is normally nothing to tune.

If `XKF1.PD` is non-zero immediately after `EV id=10`, suspect:
- The vehicle moved during arming (motion gate blocks reset on `!motorsArmed
  && !onGround`).
- The configured primary `EK3_SRC1_POSZ` is rangefinder, which the reset
  refuses by design (rangefinder is its own datum).

### 4.7 INS thermal calibration

Two failure modes:

**A. TCAL not run** — accel Z drifts with FC temperature; you'll see baro
temperature and EKF Z-bias trend together over a long hover. Run
`INS_TCAL1_ENABLE = 1` and complete a tcal cycle on each IMU.

**B. TCAL run upside-down** — the most common silent failure on flight
controllers that mount inverted (`AHRS_ORIENTATION = 12`, etc.). The
calibration learns gravity-coupled scale-factor drift with the wrong sign,
so it actively makes thermal drift worse during real flight. Diagnostic:
`INS_TCAL1_ACC1_Z` is 1000–2000× larger than the X and Y coefficients for
the same axis (X/Y see ~0g in both orientations; Z flips sign with
orientation, so the scale-factor component lands entirely on Z when
miscalibrated).

Fix: zero the suspect Z coefficients

```
INS_TCAL1_ACC1_Z = 0
INS_TCAL1_ACC2_Z = 0
INS_TCAL1_ACC3_Z = 0
INS_TCAL2_ACC1_Z = 0
INS_TCAL2_ACC2_Z = 0
INS_TCAL2_ACC3_Z = 0
```

Then recalibrate with the FC right-side-up (gravity in the orientation it
flies in) and verify the new coefficients are reasonable.

### 4.8 Vibration rectification bias (`INS_ACC_VRFB_Z`)

Vibration moves the accelerometer asymmetrically; the rectified component
appears as a DC bias on AccZ. Without correction the EKF interprets it as
specific force, drifts altitude, and the controller follows.

Estimation: compare ground (motors at idle) and hover AccZ on the same IMU,
controlled for temperature. The hover-minus-ground delta (after temperature
regression) is the VRF.

| Class | Typical VRF on Z |
|-------|------------------|
| Small indoor low-vibration quad | +0.05 to +0.10 m/s² |
| Larger frame, more vibration | up to +0.30 m/s² |
| Pathological | approaches the parameter cap |

Initial setting: `INS_ACC_VRFB_Z = 0` (per-IMU also: `INS_ACC2_VRFB_Z`,
`INS_ACC3_VRFB_Z`) and `ACC_ZBIAS_LEARN = 3` (Learn-and-save + Use). Let the
EKF learn, then read back and persist. Use `ACC_ZBIAS_LEARN = 7` to
additionally suppress the EKF's on-ground zero-velocity learning while
disarmed (bit 2). The parameter `@Range` is ±0.5 m/s²; the internal
hover-bias correction clamp is ±0.6 m/s². If the learned value saturates
the parameter range you almost certainly have a different problem
(miscalibrated TCAL, tight motor mounts, broken vibration isolation).

### 4.9 Compass motor compensation (`COMPASS_MOTCT`)

`COMPASS_MOTCT = 2` (current-based) is preferred over throttle-based when a
current sensor is available. Calibrate per airframe and re-calibrate after
significant mass changes — battery swap to a different cell count, payload
change. After a re-cal, verify that mag/current correlation drops below
~|0.1| on each axis.

`COMPASS_OFFS_MAX` default 1800 mG is loose. Reduce to 600 mG to reject
future bad calibrations at pre-arm.

### 4.10 `ATC_ANG_YAW_P`

Default for new airframes is often 4.5. Autotune sometimes pushes this much
higher (15–20). On a noisy compass, high `ANG_YAW_P` produces visible yaw
twitchiness at hover and amplifies in-flight yaw realignment shocks.
Reduce to 4.5–6.0 unless there is a measured yaw-tracking deficiency.

---

## 5. Tuning order

If the vehicle is new or unflown, follow this order. Skipping ahead before
the prerequisites are in place produces tuning data that is contaminated and
that you will have to redo.

1. **Mechanical**: rigid baro mounting (foam shroud), compass distance from
   power cables and battery connector, no loose magnetic components.
2. **Calibrate gyro / accel level / accel full** (right-side up).
3. **Compass cal** (pass §2.6 diagnostic first if it has been failing).
4. **`MOT_THST_HOVER`** correct to within ~10 % of measured hover ThO.
5. **`BARO1_THST_SCALE`** calibrated against rangefinder at steady hover.
6. **`BARO_THST_FILT = 1.0`** (or lower for slow controllers).
7. **`EK3_GND_EFF_DZ`** to match observed peak prop-wash (negative mode for
   rangefinder vehicles, positive `4` for baro-only).
8. **`TKOFF_GNDEFF_ALT/TMO`** to match airframe.
9. **`EK3_MAG_CAL = 7`, `COMPASS_MOTCT = 2`** with motor-comp cal completed.
10. **`EK3_RNG_USE_HGT`** to `-1` or `70` (with HAGL switching available).
11. **Rate / angle autotune** in a calm environment outdoors if possible —
    indoor turbulence biases autotune values high.
12. **Reduce `ANG_*_P`** if the §2.3 oscillation check shows overshoot >1.3.
13. **`ATC_ANG_YAW_P`** down to 4.5–6.0 from any autotuned excess.
14. **`ACC_ZBIAS_LEARN = 3`, `INS_ACC_VRFB_Z = 0`** — let it learn for a
    flight, persist the result. Use `7` to additionally inhibit on-ground
    learning while disarmed.
15. Optional: fly the **VALT** flight mode (mode 29) once basic AltHold is
    solid, for stick-rate altitude control with mid-stick position-hold.

---

## 6. Quick parameter reference (indoor-relevant subset)

```
# --- Source set (indoor flow + RFND) ---
EK3_SRC1_VELXY     = 5      # Optical flow
EK3_SRC1_POSXY     = 0      # No absolute horizontal aiding
EK3_SRC1_POSZ      = 1      # Baro
EK3_SRC1_VELZ      = 0
EK3_RNG_USE_HGT    = -1     # or 70 with HAGL switching
FLOW_TYPE          = 10     # vehicle-specific
RNGFND1_TYPE       = 10     # vehicle-specific
RNGFND1_MIN_CM     = 2      # as low as the sensor permits
RNGFND1_MAX_CM     = 1200

# --- Baro ground effect / propwash ---
BARO1_THST_SCALE   = <calibrate>     # Pa per unit throttle, negative; per-baro
BARO_THST_FILT     = 1.0             # Hz; NOT BARO1_THST_FILT — single param, no instance suffix
EK3_GND_EFF_DZ     = -8              # negative = noise floor mode (rangefinder vehicles)
                                     # use 4.0 for baro-only vehicles
TKOFF_GNDEFF_ALT   = 5               # m, @Range 0..5
TKOFF_GNDEFF_TMO   = 3               # s, @Range 0..5 (firmware caps at 5)

# --- IMU ---
INS_ACC_VRFB_Z     = 0               # IMU 0; let learn. Per-IMU: INS_ACC_VRFB_Z, INS_ACC2_VRFB_Z, INS_ACC3_VRFB_Z
ACC_ZBIAS_LEARN    = 3               # bitmask: 1=Learn+Save, 2=Use, 4=Disable on-ground learning
                                     # 3 = Learn+Use; 7 = Learn+Use+Inhibit-on-ground
INS_TCAL1_ENABLE   = 1               # only if calibrated right-side-up

# --- Compass ---
EK3_MAG_CAL        = 7               # GROUND_AND_INFLIGHT (on ground + after first in-air yaw reset)
COMPASS_MOTCT      = 2               # current-based motor comp
COMPASS_OFFS_MAX   = 600             # tighter than default 1800
COMPASS_USE2/3     = ... per redundancy

# --- Position controller (4.7 names — old PSC_*Z renamed to PSC_D_*) ---
PSC_D_POS_P        = 1.0             # was PSC_POSZ_P. Higher than default helps thermal-drift regimes
PSC_D_VEL_P        = 5.0             # was PSC_VELZ_P
PSC_D_ACC_P        = match hover throttle (use AC_PosControl::init_z_controller defaults)
PSC_D_ACC_I        = same            # was PSC_ACCZ_I
# Horizontal: PSC_NE_VEL_P/I/D (was PSC_VELXY_*)
# Old names map to new names automatically via AP_Param conversion at first boot.

# --- Attitude (after autotune review) ---
ATC_ANG_RLL_P      = autotune output, but test §2.3 and reduce 25–30 % if oscillating
ATC_ANG_PIT_P      = same
ATC_ANG_YAW_P      = 4.5–6.0
ATC_RAT_*_SMAX     = 0                # leave off for performance — don't use to mask oscillation

# --- Motor / takeoff ---
MOT_THST_HOVER     = within 10% of measured hover ThO
TKOFF_SLEW_TIME    = 1.0–1.5 s on aggressive airframes; @Range 0.25..5.0
PILOT_TKO_ALT_M    = 1.0              # metres. Was PILOT_TKOFF_ALT (centimetres) — auto-converted on first boot

# --- Velocity-control altitude hold ---
# On 4.7 this is the dedicated VALT flight mode (Mode::Number = 29).
# There is no ALTH_OPTIONS parameter on this branch; assign VALT to a
# flight-mode switch position to use it.

# --- Throw / drop (if applicable) ---
THROW_TYPE         = 0 (upward) or 1 (drop)
THROW_NEXTMODE     = 1 (ACRO), 2 (ALT_HOLD), 5 (LOITER), 18 (Throw), 29 (VALT)
                   # ACRO and VALT accepted by code on this branch though @Values doc may not list them
THROW_DROP_AG      = 2                # @Range 1.0..4.0
THROW_DROP_CNF     = 0                # 100 ms minimum; @Range 0..5 s
THROW_ALT_DCSND    = 0                # hand drops; 3–5 for high releases
THROW_SRC_INI      = 2 or 3           # only on flow-only vehicles
THROW_SRC_SET      = 1
THROW_YAW_TYPE     = 0                # 0=hold release yaw, 1=face direction of travel,
                                      # 2=face reverse, 3=face THROW_YAW_DEG
THROW_YAW_DEG      = 0                # absolute compass heading, used only when THROW_YAW_TYPE=3
```

---

## 7. What this playbook does not cover

- Outdoor wind-rejection tuning (different regime: thermal drift dominates,
  prop-wash baro is negligible above ~10 m AGL).
- GPS-aided tuning, RTK / moving-base, or geofencing.
- Fixed-wing / VTOL transition tuning.
- ESC / motor / propeller selection.
- Notch-filter design (`INS_HNTCH_*`) — assumes a working notch is already
  in place; if not, run a vibration-isolated FFT on `IMU.GyroX/Y/Z` and use
  the harmonic notch wizard before any of the above.

For each of these, build a separate playbook from logs taken in the matching
regime — indoor-tuned parameters do not transfer cleanly to outdoor, and
vice versa.
