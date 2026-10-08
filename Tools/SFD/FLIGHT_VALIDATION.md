# SFD flight validation record

What each flight showed for the PRs in the build, newest first. The flights
still to fly are in `FIELD_PLAN.md`; this file is the history behind them.
Per-PR detail lives in `../ardupilot-pr-analysis/<PR>/README.md`.

Every log is flown with `LOG_REPLAY 1` and `LOG_DISARMED 2` and replayed;
the A/B baselines are Replays of the same log with the PR's change removed.


## 2026-10-08: refresh11 log44, flight card item 1 again (#33478 step hold)

Firmware `4fc73825` (topup12). Replays exactly. The log40 sortie over the
~0.7 m step: four edge crossings, landed on top. Core 1 (flow lane, fusing
the AGL KF velocity) against core 0, with the hold and in a Replay without
it:

| Edge | Core 1 - core 0, hold | Without the hold |
|---|---|---|
| Off, 88.6 s (6-8 s after) | +0.33 m | +0.99 to +1.33 m |
| Back on top, 110.3 s | +0.63 m peak | +1.36 m |
| Off, 122.4 s | within 0.12 m | +0.79 to +1.16 m |
| Back on top, 132.0 s | -0.13 to -0.32 m | +0.30 to +0.71 m |

Landed: core 1 +0.05 m from its height after arming, against +0.41 m
without the hold (core 0 +0.26 m both ways, its baro drift). XKFA.VFuse
paused after each edge and resumed. #33478's step hold: validated. Arming
stepped both cores' height -1.66 m on the ground (#32768's arm datum, as on
log42; hands off).

## 2026-10-08: #34457 floor behaviour, logs 31-44

All fourteen logs carry #34457's floor commits. Armed on the ground before
takeoff the AGL KF height stayed within 0.02 m of its floor on every log.
After touchdown it held within 0.07 m except log39 core 1, where the main
filter believed it was climbing at 0.3-0.57 m/s on the ground after an acro
sortie, so the coast stop (which leaves anything faster than 0.25 m/s to
the IMU) never fired and the AGL KF height coasted 0.28 -> 1.8 m. Fixed on
fix2/34457 (not yet flown or in the beta): with the range last read near
the floor and reading too low for 1 s, an AGL KF height 0.3 m above that
reading is held on the floor. Replay: log39 held at the floor, the other
13 logs unchanged. log38's 0.14 m is a real hop.

## 2026-10-08: refresh11 logs 41-43, flight card items 2, 3 and 5

Firmware `9d8e8053`. All three replay exactly. No failsafes, EKF errors or
uncommanded lane switches.

| PR | Status |
|---|---|
| #34380 | Validated (log41, card 2). The flow lane climbed from 4 to 35 m at 2.5 m/s, through 9.5 m without being held, and relative position stayed valid. Back into range at 169 s both cores' HAGL stepped 1.3 m in one sample: upstream EKF3's terrain reset after a 5 s range gap, correcting the baro height drift built up out of reach, after which HAGL agreed with the range. Not a defect of this PR or #33585. |
| #33568 | Validated (log42, card 3, two flights). Aiding went to mode 2 10 s after each switch to flow (the designed fallback time) and back to 0 within 0.2 s of returning to GPS. Both climbs passed 9.5 m, and there was no limit cycle at 25 m or above. Not shown: the flow speed cap, which never became the binding limit. Flow tracking at 29-33 m was poor (5.3 m of GPS movement seen as 2.4 m, and 8.5 m seen against 1.1 m). |
| #34630 | Not provoked (log43, card 5). 62 deg of tilt and 6.7 rad/s of flow at 2-6 m, but flow quality stayed above 144 and no lockout reset happened in flight. The one reset was at touchdown. |
| #32768 | Seen, not acted on (hands off): at the second arm of log42 the height stepped -3.4 m on the ground, the GPS altitude's drift since boot. Height above home was unaffected. |

## 2026-10-08: #33478 step hold, from log40

Replay of log40 found core 1's 0.9 m climb off the edge came from #33478:
the AGL KF absorbs a range step over 3-5 s with its velocity 0.2-0.3 m/s
wrong, and core 1 fused that as velD. fix/33478 holds the AGL KF velocity
out of velD for 5 s after a step, only while a real height source outside
ground effect is trusted. Replay of 12 flights (velD error, m/s): log40
0.089 -> 0.046, log29 0.107 -> 0.043, log30 0.097 -> 0.055, log34
0.085 -> 0.036, log39 0.232 -> 0.086, log32 0.116 -> 0.266 (fusion held
off over its 61 samples), the rest unchanged. Log40's core 1 stays within
0.03 m of fusion off at the edge and lands at +0.25 m against +0.36 m. Not
yet flown: card item 1.

## 2026-10-08: refresh11 log40, flight card item 1 (#33359 step-up)

Firmware `9d8e8053`, `EK3_OPTIONS 94`, `EK3_RNG_USE_HGT 6`, core 0 (GPS)
primary throughout. Replays exactly. One sortie over a ~0.7 m step with two
excursions over the lower ground. Touchdown is measured against the altitude
before takeoff, so 0 is right.

| Replay | core 0 | core 1 |
|---|---|---|
| as flown | -0.16 m | +0.36 m |
| #33359 fix removed | -0.57 m | +0.01 m |
| `EK3_OPTIONS` bit 4 off (#33478) | -0.16 m | -0.21 m |
| fix removed, bit 4 off | -0.57 m | -0.69 m |

| PR | Status |
|---|---|
| #33359 | Helps. The third crossing reset core 0's offset ("IMU0 terrain offset reset from range") and the switch-in step was 0.15 m against 0.55 m without the fix. The second crossing did not reset: the disagreement was 0.23 m, under the 0.3 m threshold, and both cores stepped down about 0.27 m, the same with and without the fix. A 0.2 m threshold lands at +0.10/0.00 m (bit 4 off); 0.15 m overshoots to +0.26/+0.20 m. The residual is baro drift over the excursion, which the offset inherits either way, so the threshold is unchanged on one flight. Hand-back to baro off the edge was clean (core 0 within 0.11 m). |
| #33478 | Problem. Off the edge, core 1 (flow lane) fused the AGL KF velocity as velD and climbed at 0.25 m/s for 5 s while baro was the source and flat: 0.9 m of height error, and baro innovations up to 1.4 m at a test ratio of 0.14. The PR describes a step as a brief transient (0.36 m/s in SITL); here it was sustained. With bit 4 off, core 1 tracks core 0. The vehicle was not affected because core 0 was primary. |

## 2026-10-06 to 10-08: Replay of earlier flights against the current code

| PR | Status |
|---|---|
| #33359 | Replay of logs 31-35 and 39 with a probe on the height source: the switch engaged in flight with the vehicle's terrain-stable flag clear for ~72 s on log31, including a 21 s hover below 0.9 m that tracked the range to 0.10 m rms. Refresh8's log29 (a 0.84 m step) showed a defect: back up the step the switch re-engaged on a stale terrain offset and pulled the height down 0.45 m. Fixed in topup9 (`d52e4a8a8c`); Replay of log29 lands at +0.10/-0.35 m instead of -0.57/-0.63 m. Topup10 also gates the AGL height the switch reads on the observation's limits; identical on Replay of six flights. Not yet flown with the fix. |
| #34208 | Lock-free rate target (volatile sequence instead of std::atomic) in topup8. Same behaviour by design; any flight on the fast rate thread exercises it. |
| #34642 | SITL-only (gyro rate follows INS_GYRO_RATE). Nothing to fly. |
| #34380 | Review fixes are a comment and test legs; its flight is unchanged. |
| #32553 | Helps. Replaying log36 without the reopen, height above ground during the 0.6 m dwell (59-75 s) is off by 0.47 m rms on core 1 against 0.21 m as flown (core 0: 0.11 against 0.04 m); with `EK3_GND_EFF_DZ=2` it is 0.54 against 0.22 m. No difference above range. Retraction (2026-10-06): the first A/B used a no-reopen binary that was byte-identical to the beta, so its "nothing to correct" was wrong. |


## 2026-10-05: logs 35-39

Firmware `96250adaf3`. All five logs replay exactly.

| PR | Status |
|---|---|
| #33585 | Validated again with the new end timing: flow lane primary above range to 22 m, 6.4% drift, no HAGL step when the range came back. Squashed. |
| #34456 | In-flight refusal validated (log37, `EK3_OPTIONS 92`). |
| #34292 | Floor works and aiding is kept; no phantom velocity on this airframe to remove. |
| #32972 | Mixed. Negative `EK3_GND_EFF_DZ` holds height through spool-up (0.03-0.07 m against 0.21-0.27 m), but -8 is worse after liftoff (0.35 against 0.17 m rms). Replay sweep done; no more flying needed. |
| #32553 | See the 2026-10-06 row above. |
| #34380 | Not flown: in log37 the flow lane was never primary, so no limit was published. |
| #33568 | Not flown: `EK3_SRC_OPTIONS` stayed 8. |
| #34630 | No resets: log39's acro was above the range finder, so both flow axes dropped together and no single-axis lockout occurred. |
| #32471 | Not flown. |

### Done: baro as height source at takeoff (#32972, #32553), flown as log36

`EK3_RNG_USE_HGT -1`, `EK3_GND_EFF_DZ -8`, `GNDEFF_ALT 0.5`,
`GNDEFF_TMO 2`, GPS lane. Arm in LOITER, hold the motors spooled on the
ground at least 5 s, lift off and hover at about 0.4 m for 30 s, climb to
5 m, land slowly; repeat with a brisk takeoff straight to 5 m. The pass
criteria were EKF height within about 0.2 m of the range finder through
spool-up and liftoff with no height reset (#32972, baseline Replay with
`EK3_GND_EFF_DZ=8`), and "terrain offset reopened after takeoff" with the
0.4 m dwell's HAGL error falling toward the SITL 0.26 m rms (#32553,
baseline Replay with the reopen reverted). Results in the tables above.

### Done: flow floor (#34292), flown in logs 35-39

`FLOW_HGT_MIN 0.15` as a first guess: across logs 31-34 the flow innovation
ratio (XKF5.NI) rises from 3-4 above 0.3 m to 14-44 between 0.05 and 0.2 m
while OF.Qual stays near 200. Known limits of the rest reading, from the
2026-10-05 review: it is learned on the ground before takeoff from a flow
sample and kept until the EKF restarts (check OF.Qual is non-zero on the
ground before each takeoff), and it compares range readings, so land on the
same hard, level surface you took off from - soft ground, a dip or a tilted
stance reading more than 5 cm further can leave flow aiding off after
landing and trigger the EKF failsafe while landed.


## 2026-10-04: refresh10 logs 31-34

Firmware `SmallFastDrone V4.7.2-beta1 (199bc6e5)`. Those flights validated
#34456, #32473, #34362, #33585 and #33484; results are posted on each PR
and the PRs were squashed. Six more PRs in the build were not exercised:

| PR | What hid it on refresh10 |
|---|---|
| #32972 | `EK3_RNG_USE_HGT 6` makes the range finder the height source below 0.9 m, so the baro is not used through spool-up. Replay with `EK3_GND_EFF_DZ=8` matched the flight within 3 cm. |
| #32553 | Same parameter: the terrain update is skipped while the range finder is the height source, so no ground-effect error reached the offset. |
| #34380 | `AVOID_ENABLE 0`. AC_Avoid is the only consumer of the height cap, so the cap was never applied. |
| #33568 | `EK3_SRC_OPTIONS 8` runs flow on core 1 from boot, so no core went from GPS (absolute) to flow (relative). |
| #34292 | `FLOW_HGT_MIN 0` disables the floor. |
| #32471 | `ACC_ZBIAS_LEARN 8` sets only the acro bit; learning (bit 0) and applying (bit 1) are off. |

Open from these logs: log32 core 0 departs from its Replay at rounding level
from 5.2 s, just after the recorded-origin boot that only log32 has,
reaching 1.4 m in the terrain offset by 80 s and 7 cm in PD by 339 s. It is
not precision, and the cause is not yet found. Leaving the RC source set
switch on set 2 at boot, with a recorded origin, gives it another chance to
show.
