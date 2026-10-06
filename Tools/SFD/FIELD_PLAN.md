# Field plan: flying the PRs refresh10's flights did not exercise

Written 2026-10-04 after the forensic pass on refresh10's logs 31-34
(`/mnt/d/logs/SmallFastDronev1-O4/refresh10`, firmware
`SmallFastDrone V4.7.2-beta1 (199bc6e5)`).

Those flights validated #34456, #32473, #34362, #33585 and #33484. Results
are posted on each PR and the PRs were squashed. Six more PRs in the build
were not exercised:

| PR | What hid it on refresh10 |
|---|---|
| #32972 | `EK3_RNG_USE_HGT 6` makes the range finder the height source below 0.9 m, so the baro is not used through spool-up. Replay with `EK3_GND_EFF_DZ=8` matched the flight within 3 cm. |
| #32553 | Same parameter: the terrain update is skipped while the range finder is the height source, so no ground-effect error reached the offset. The reopen fired but had nothing to correct. |
| #34380 | `AVOID_ENABLE 0`. AC_Avoid is the only consumer of the height cap, so the cap was never applied. |
| #33568 | `EK3_SRC_OPTIONS 8` runs flow on core 1 from boot, so no core went from GPS (absolute) to flow (relative). |
| #34292 | `FLOW_HGT_MIN 0` disables the floor. |
| #32471 | `ACC_ZBIAS_LEARN 8` sets only the acro bit; learning (bit 0) and applying (bit 1) are off. |

Each flight below changes the one parameter that hid its PR. Fly every
flight with `LOG_REPLAY 1` and `LOG_DISARMED 2`. Every log is replayed, and
each flight names the Replay A/B that gives its baseline.


## Where it stands after 2026-10-05 (logs 35-39)

Firmware `96250adaf3`. All five logs replay exactly.

| PR | Status |
|---|---|
| #33585 | Validated again with the new end timing: flow lane primary above range to 22 m, 6.4% drift, no HAGL step when the range came back. Squashed. |
| #34456 | In-flight refusal validated (log37, `EK3_OPTIONS 92`). |
| #34292 | Floor works and aiding is kept; no phantom velocity on this airframe to remove. |
| #32972 | Mixed. Negative `EK3_GND_EFF_DZ` holds height through spool-up (0.03-0.07 m against 0.21-0.27 m), but -8 is worse after liftoff (0.35 against 0.17 m rms). Replay sweep done; no more flying needed. |
| #32553 | Helps. Replaying log36 without the reopen, height above ground during the 0.6 m dwell (59-75 s) is off by 0.47 m rms on core 1 against 0.21 m as flown (core 0: 0.11 against 0.04 m); with `EK3_GND_EFF_DZ=2` it is 0.54 against 0.22 m. No difference above range. The 2026-10-06 retraction: the first A/B used a no-reopen binary that was byte-identical to the beta, so its "nothing to correct" was wrong. |
| #34380 | Not flown: in log37 the flow lane was never primary, so no limit was published. |
| #33568 | Not flown: `EK3_SRC_OPTIONS` stayed 8. |
| #34630 | No resets: log39's acro was above the range finder, so both flow axes dropped together and no single-axis lockout occurred. |
| #32471 | Not flown. |

## Since then (2026-10-06, beta `fde4de75a6`)

| PR | Status |
|---|---|
| #33359 | Replay of logs 31-35 and 39 (probe on the height source): the switch engaged in flight with the vehicle's terrain-stable flag clear for ~72 s on log31, including a 21 s hover below 0.9 m that tracked the range to 0.10 m rms. Refresh8's log29 (the 0.84 m step) showed a defect: back up the step the switch re-engaged on a stale terrain offset and pulled the height down 0.45 m. Fixed in topup9 (`d52e4a8a8c`); Replay of log29 lands at +0.10/-0.35 m instead of -0.57/-0.63 m. **Fly it: item 1 below.** |
| #34208 | Lock-free rate target (volatile sequence instead of std::atomic) in topup8. Same behaviour by design; any flight on the fast rate thread exercises it. |
| #34642 | SITL-only (gyro rate follows INS_GYRO_RATE). Nothing to fly. |
| #34380 | Review fix is a comment and a test leg; the flight below is unchanged. |

## Next flight

Firmware `fde4de75a6` (topup9). Keep `EK3_OPTIONS 94`.

1. **#33359 step-up, the log29 sortie again.** `EK3_RNG_USE_HGT 6`,
   `EK3_OPTIONS` bit 3 (94 has it), GPS lane (set 1), LOITER. Take off from
   the top of the step, hover at about 0.45 m for 10 s, fly off the edge
   and hover over the lower ground for at least 10 s, fly back over the top,
   hover 10 s and land on top. Do it twice.
   Pass: crossing back up, the log shows "EKF3 IMU0/1 terrain offset reset
   from range" if the offset is stale, and the EKF altitude does not step
   down when the switch re-engages. Landed back on top, EKF altitude is
   within about 0.15 m of where it was before takeoff (log29: -0.45 m).
   Off the edge it
   still hands back to baro within about 0.3 s with no altitude step.
   Baseline: Replay of the same log with `d52e4a8a8c` reverted.
   Risk: the old behaviour stepped the EKF altitude down 0.4 m at about
   0.3 m up, and Copter moves its target with a height reset. Hover no lower
   than 0.4 m and keep STABILIZE on a switch.
2. **#34380, per-core lanes.** `AVOID_ENABLE 1`, `EK3_SRC_OPTIONS 8`.
   Select set 2 before arming (or in flight, which needs `EK3_OPTIONS`
   bit 1). Take off on the flow lane and climb at full stick from 5 m to
   about 25 m in LOITER. Pass: it climbs through 9.5 m without being held,
   relative position stays valid, and the descent back into range is clean.
   Without #34380, AC_Avoid would hold it at about 9.5 m.
3. **#33568 with #34380, single source set.** This is flight 2 below:
   `EK3_SRC_OPTIONS 0`, `AVOID_ENABLE 1`, take off on set 1, switch to set
   2 in flight (no lane change, so bit 1 is not involved). Pass: XKF4.AID
   goes 0 to 2, the flow speed limit caps LOITER, and the climb passes
   9.5 m.
4. **#32471.** Flight 4 below: one pack at `ACC_ZBIAS_LEARN 9`, then one at
   11, each with a 60 s hover.
5. **#34630, if time allows.** Hard acro on the flow lane while staying
   inside the range finder's reach, below about 12 m, so a lockout has a
   fresh range to recover against. Watch the OSD lane item: a reset turns
   that axis's arrow off for 250 ms, with no message (bit 6). XKF7.FVC
   counts them.

Restore `EK3_SRC_OPTIONS 8` and `AVOID_ENABLE 0` afterwards.


## Flight 1: baro as height source at takeoff (#32972, #32553)

Done 2026-10-05 (log36); no repeat needed. Kept for reference.

Change `EK3_RNG_USE_HGT -1`. Keep `EK3_GND_EFF_DZ -8`, `GNDEFF_ALT 0.5`
and `GNDEFF_TMO 2`. Fly on the GPS lane (set 1).

Procedure:

1. Arm in LOITER and hold the motors spooled on the ground for at least
   5 s before lifting off. Log32 showed a 2 m baro dip through spool-up and
   a 7 m spike at liftoff.
2. Lift off, hover at about 0.4 m for 30 s, then climb to 5 m. This keeps
   the vehicle in ground effect after the 5 s takeoff cap expires, which is
   #32553's case.
3. Land slowly. Repeat once with a brisk takeoff straight to 5 m.

Pass for #32972:

- EKF height (XKF1.PD) holds within about 0.2 m of the range finder
  through spool-up and the liftoff spike.
- No height reset.
- Baseline: Replay with `--parm EK3_GND_EFF_DZ=8` shows the old behaviour
  on the same flight.

Pass for #32553:

- "terrain offset reopened after takeoff" when the takeoff window closes.
- In the 0.4 m dwell after it, HAGL minus the tilt-corrected range falls
  toward the PR's SITL figure: 0.26 m rms, against 0.50 m on master.
- Baseline: Replay built with the reopen commit reverted. This is a valid
  A/B on this firmware because its takeoff flag is what gets replayed,
  which the PR says older logs could not give.

Risk: if the protection fails, LOITER climbs or drops at liftoff. Keep
STABILIZE on a switch.


## Flight 2: flow lane climb with the cap enforced (#34380, #33568)

Change `AVOID_ENABLE 1` and `EK3_SRC_OPTIONS 0`, so all cores share one
active set.

Procedure:

1. Take off on set 1 (GPS) and climb to 5 m.
2. Switch to set 2 (flow). Hover for 20 s, then fly back and forth at
   3-5 m/s.
3. Climb at full stick from 5 m to about 25 m in LOITER. Hover there and
   fly a short leg.
4. Switch back to set 1 and land.

Pass for #33568:

- XKF4.AID (added by this PR) goes from 0 to 2 within a few seconds of the
  switch, and back to 0 after returning to set 1.
- The flow speed limit visibly caps LOITER speed.
- No position-controller limit cycle at 25 m (the roll oscillation the PR
  shows at height).
- Baseline: Replay built without #33568 gives master's scaler and AID.

Pass for #34380:

- The climb passes the 9.5 m cap (0.7 x 15 m - 1 m) without being held.
- Relative position stays valid above it.
- The opposite case, where the range finder is the height source and the
  vehicle backs down into range, is covered by SITL. Do not fly it.
- The 15 m range finder reaches well past the 9.5 m cap, so today's
  fresh-range change should not show. If the return is lost between 9 and
  10.5 m, the limit should come back to 9.5 m within 0.5 s and the vehicle
  hold where it can still measure, with relative position kept.

Pass for #33585's new timing, on the descent back into range at step 4:

- The fallback ends once the range has been back for 2 s, with no step in
  height above ground (XKF5.HAGL) and no loss of relative position.

Afterwards set `EK3_SRC_OPTIONS` back to 8 and `AVOID_ENABLE` back to 0,
unless the cap is wanted.


## Flight 3: flow floor (#34292)

Set `FLOW_HGT_MIN 0.15` as a first guess. Across logs 31-34 the flow
innovation ratio (XKF5.NI) rises from 3-4 above 0.3 m to 14-44 between 0.05
and 0.2 m, while OF.Qual stays at about 200. Only 49 samples lie below
0.2 m, because the vehicle passes through quickly. The range finder reads
from 0.05 m, so a 0.15 m floor can be measured.

Procedure, all on set 2 (flow) in LOITER:

1. Take off, then hover at 0.3 m and at 0.15-0.2 m for about 10 s each.
   This also gives the data to refine the floor.
2. Descend slowly and land. Stay armed on the ground for 20 s.
3. Take off and land again.

Pass:

- Below the floor the flow lane shows no phantom horizontal velocity
  against the GPS lane on the descent. The PR's hardware figures were
  +/-0.5 m/s before and +/-0.1 m/s after.
- Armed on the ground, relative aiding stops once and does not restart.
- On the climb, relative aiding restarts at roughly 0.2-0.4 m.

Two known limits of the rest reading, from the 2026-10-05 review:

- It is learned on the ground before takeoff from a flow sample, and kept
  until the EKF restarts. Check OF.Qual is non-zero on the ground before
  each takeoff, or the flight uses the previous flight's reading.
- It compares range readings, so land on the same hard, level surface you
  took off from. Soft ground, a dip or a tilted stance reading more than
  5 cm further can leave flow aiding off after landing and trigger the
  EKF failsafe while landed.


## Flight 4: hover Z-bias learning (#32471), two packs

Pack 1, `ACC_ZBIAS_LEARN 9` (bit 0 plus the acro bit):

- Hover steadily in LOITER at 2-5 m for at least 60 s, land and disarm.
- On disarm, `INS_ACC_VRFB_Z` and the second IMU's value are saved.
- They roughly equal XKF2.AZ at the end of the hover minus the value at
  arming. The PR measured about 0.09 m/s/s on two airframes.

Pack 2, `ACC_ZBIAS_LEARN 11` (bits 0 and 1 plus the acro bit), same
takeoff and hover:

- XKF2.AZ starts close to its hover value and moves less after takeoff
  than in pack 1.
- EKF height against the range finder in the first 20 s after liftoff has
  a smaller error than in pack 1.
- Replay reproduces the applied bias, which is carried in RISK.

Bit 2 (moving platform) needs arming on a car or boat. Skip it unless that
is convenient.


## Notes

- Flights 1 and 4 can share packs, because the bias learning does not
  depend on the height source. Keep flights 2 and 3 separate so the
  source-set and floor changes do not overlap.
- Leave the RC source set switch where it is. A boot on set 2 with a
  recorded origin gives the log32 Replay divergence another chance to show:
  log32 core 0 departs at rounding level from 5.2 s, just after the
  recorded-origin boot that only log32 has, reaching 1.4 m in the terrain
  offset by 80 s and 7 cm in PD by 339 s. It is not precision, and the
  cause is not yet found.
- Afterwards, restore refresh10's values: `EK3_RNG_USE_HGT 6`,
  `EK3_SRC_OPTIONS 8`, `AVOID_ENABLE 0`, and `ACC_ZBIAS_LEARN` as wanted.
  Keep `EK3_OPTIONS 94`.
- The A/B Replay binaries can be built before flying: beta without the
  #32553 reopen, beta without #33568, and beta without #32471's bias
  application.
