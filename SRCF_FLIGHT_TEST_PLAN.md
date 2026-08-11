# SRCF field flight test plan

GPS -> optical-flow -> AltHold fallback with cross-lane monitoring:
first flights.

Branch: `SmallFastDrone-4.7.0-gps-optflow-fallback` @ `facfce10bc`.
SITL status: 3/3 feature tests + 4/4 regression tests green
(SRCFGPSLossLadder, SRCFGPSSpoof, SRCFDisabledRegression;
EKFSourceSetFailsafe, OpticalFlowGPSLossAiding, EKF3SRCPerCore,
OpticalFlow). Plan written 2026-08-11.

## What this session proves (and doesn't)

Goal: flight-validate the GPS-loss rung - clean handover to the flow
lane, controllable flight on flow, auto-recovery - and soak the spoof
detector for false trips. Actual spoof injection is SITL-only
(transmitting GNSS spoofing is illegal); the field validates that the
detector stays quiet on real flights. The AltHold demotion rung is
optionally exercised at the end.

## Bench setup - before leaving

- [ ] Save a full parameter backup of the current config.
- [ ] Flash the branch build (SmallFastDronev1 target, 58 KB flash
      free).
- [ ] Set the parameter block below; reboot; confirm arming checks
      PASS with `SRCF_ENABLE=1` - the new pre-arm validates the whole
      lane config and names what is wrong.
- [ ] Map a spare, guarded RC switch to GPS Disable
      (`RCx_OPTION=65`). Cycle it on the bench: GCS shows GPS fix
      lost/return.
- [ ] SD card has plenty of free space - `LOG_REPLAY=1` grows logs.

| Param                   | Value         | Note                                          |
|-------------------------|---------------|-----------------------------------------------|
| `EK3_IMU_MASK`          | 3             | two lanes; lane 1 = IMU2 carries the flow lane |
| `EK3_PRIMARY`           | 0             | boot on the GPS lane                          |
| `EK3_SRC_OPTIONS`       | 8             | per-core source sets. NOT 9 - bit0 (fuse-all-vel) is an arming reject with bit3 |
| `EK3_OPTIONS`           | current \| 2  | add bit1 manual lane switch (e.g. 24->26, 56->58); keep existing bits |
| `EK3_SRC1_*`            | GPS set       | POSXY=3 VELXY=3 VELZ=3, POSZ/YAW as currently flown |
| `EK3_SRC2_*`            | flow set      | POSXY=0 VELXY=5 VELZ=0 YAW=1, POSZ same as SRC1 |
| `SRCF_ENABLE`           | per flight    | thresholds stay default: VEL_THR 0.8, POSR_THR 0.5, CNF 2 s, RECOV 10 s |
| `RCx_OPTION`            | 65            | GPS Disable - the reversible field lever for GPS loss |
| `FS_EKF_ACTION/_THRESH` | stock         | the EKF failsafe stays live as backstop       |
| `LOG_REPLAY`            | 1             | enables the later Replay validation of lane-switch events |

**Do not use the CRSF indoor/outdoor profile this session.**
`flight.lua` rewrites `EK3_SRC1_*` and `EK3_SRC_OPTIONS` (the outdoor
profile sets bit0), which conflicts with the lane assignment and will
block arming. Leave the environment menu alone.

## Site and conditions

- Textured ground for flow (grass/dirt beats uniform tarmac); light
  wind for the first fallback flights.
- Open area, no people downwind; all fallback events at ~8 m AGL -
  inside rangefinder range, good flow, height to react.
- Pilot ready on the mode switch at all times: AltHold is the manual
  escape at every step. Disarm resets lanes and latches.

## Flight cards - fly in order, review logs between

### Flight 1 - shadow-lane shakeout (`SRCF_ENABLE=0`)

Per-core lanes live, monitor off. Normal takeoff, 2-3 min gentle
Loiter box at 5-10 m, land.

- Pass: flies exactly as normal; no lane-switch or SRCF text; no EKF
  variance.
- Log review before Flight 2: `XKF4` C=1 shows `AID=2` (flow lane
  aiding in shadow) and C=0 `AID=0`; `PI` stays 0; cross-lane
  velocity difference (`XKF1` C=0 vs C=1) under ~0.5 m/s in hover;
  new `XKFS.SS` shows 0 on C=0 and 1 on C=1.
- Abort: anything abnormal in handling -> land, restore param backup,
  done for the day.

### Flight 2 - false-trip soak (`SRCF_ENABLE=1`)

Monitor armed, GPS healthy throughout. 4-5 min Loiter including
brisk translations, hard stops, fast yaw, a low pass (~2 m) and up to
~10 m - the divergence-noise worst cases.

- Pass: zero `SRCF:` statustexts. Log: `SRCF.Vote` max well under 20;
  `VD` stays well under 0.8; `PR` well under 0.5.
- If `Vote` peaks above ~10, raise `SRCF_VEL_THR` to 1.0-1.2 and/or
  `SRCF_POSR_THR` to 0.7 before Flight 3.

### Flight 3 - GPS-loss fallback and recovery (the core test)

1. Steady hands-off Loiter hover at ~8 m.
2. Flip GPS Disable ON.

   Expect within ~1 s: `SRCF: GPS lost, using flow lane` then
   `EKF3 lane switch 1`. The vehicle should not visibly move (SITL:
   bounded well under 2 m).

3. Hold hands-off 20-30 s, then gentle stick inputs - expect slightly
   softer, speed-limited response (flow-lane detune is by design).
4. Flip GPS Disable OFF.

   Expect `SRCF: GPS recovered` + `EKF3 lane switch 0` after
   ~10-25 s (recovery hold plus GPS lane realignment).

5. Land, disarm.

- Abort: any lurch, lean, or position walk -> GPS back ON
  immediately; if not settling -> AltHold and land. The stock EKF
  failsafe (LAND) remains the final backstop.
- Pass: excursion under ~2 m at the switch, controllable throughout,
  clean recovery, no `EKF variance`.

### Flight 4 - fallback under motion (only if Flight 3 clean)

Repeat Flight 3, but flip GPS Disable during a slow 2-3 m/s
translation. Expect the same handover with flight remaining
controllable on flow; recover and land.

### Flight 5 - AltHold demotion rung (optional)

Only if Flights 3-4 were clean AND `EK3_OPTIONS` bit6 (flat-ground
flow) is NOT set - with bit6, flow correctly stays valid above the
rangefinder ceiling and this card cannot trigger.

At ~8 m with GPS Disable ON (on the flow lane), climb steadily past
the rangefinder ceiling. About 5 s after range is lost expect
`SRCF: no nav source, AltHold` and the mode change. Fly AltHold
manually, descend, GPS ON, land.

Higher workload - skip without hesitation; SITL already covers this
rung.

## Post-session review

- `SRCF` log: state transition timeline, `Vote` max, distributions of
  `VD`/`PD`/`PR`, `GpsB`/`FlwU`/`GpsL` flags around each event.
- `EKFC`: `Bad` never set; holdoff visible after each commanded
  switch.
- `XKF4` both cores: `AID` and `PI` traces match the statustext
  timeline.
- Keep the Flight 3 log for the Replay check of the new
  `requestLaneSwitch` DAL events (`Tools/Replay/check_replay.py`).

"Flight-validated" means: Flights 1-3 clean, plus zero false trips
across the next several ordinary GPS flights with `SRCF_ENABLE=1`.
Until then the spoof latch path remains SITL-validated only.

## Rollback

`SRCF_ENABLE=0` disarms the monitor in place. Full revert: restore
the param backup (`EK3_SRC_OPTIONS`, `EK3_OPTIONS`, `EK3_SRC2_*`,
`EK3_IMU_MASK`), or reflash the previous firmware.
