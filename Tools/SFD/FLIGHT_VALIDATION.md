# SFD flight validation record

What each flight showed for the PRs in the build, newest first. The flights
still to fly are in `FIELD_PLAN.md`; this file is the history behind them.
Per-PR detail lives in `../ardupilot-pr-analysis/<PR>/README.md`.

Every log is flown with `LOG_REPLAY 1` and `LOG_DISARMED 2` and replayed;
the A/B baselines are Replays of the same log with the PR's change removed.


## 2026-10-30: SFD-O4 logs 64-65, takeoff blocked, lockouts and landed hold

Firmware `52b1d36f`, `ACC_ZBIAS_LEARN 11` still.

log64 could not take off: "Takeoff blocked: CPU overload" every 2 s through
two armings, at an average of 84-92%. The check (`342a8bdfd8`, upstream in
4.7 and unchanged on master) blocks on average over 95% or peak over 99.5%,
and the peak is the highest roughly 1 s load since boot: sysperf.c sets it
at initialisation only. One busy second anywhere in the session blocks
takeoff until reboot, and the message reports the average, not the peak
that blocked (ARCH-2). The peak is not logged, so what spiked is unknown;
this boot also had a radio failsafe and turtle mode before arming. Main
loop load (PM) matched logs 61-65. `BRD_IDLE_STATS 0` turns the check off
at once, as `use_idle_stats()` reads it live; a reboot clears the peak.

(Corrected the same day: the peak-since-boot account above is wrong. The
log does carry the figures, in the `@SYS/threads.txt` copies the logger
writes after each arming: LOAD 0.0% PEAK 0.0% at the first arming of every
boot, then 87.3% / 99.5% at log64's second. Measurement does not start at
boot, because `main_loop()` tests `BRD_IDLE_STATS` before the parameters
load; the logger's first read after arming starts it, and the first few
samples after that start tripped the 99.5% peak. Later reads leave it
frozen: the average stayed at exactly 87.4%. So the radio failsafe and
turtle mode played no part. Open PR #33366 fixes the measurement and the
check, and is in `prs.txt`; see
`../ardupilot-pr-analysis/pending-takeoff-load-peak/`.)

log65, card items 3 and 1 (acro to 56 m, range finder good 25% of it, 48%
over 45 deg of tilt; GPS lane primary, flow on core 1 throughout):

- #34630/#33484: the first lockouts provoked in flight. Core 1 reset three
  times (XKF7.FVC 0 to 3, FVU never set), range good at 9-14 m, tilt
  33-44 deg, quality 126-159. Each repaired a lane already wrong: 4.7 to
  18.9 m/s at 63.6 s (GPS 18.1), 0.1 to 20.7 at 83.2 s (GPS 18.5), 1.7 to
  16.1 at 132.5 s (GPS 18.9), and all within 1 m/s a second later. The
  deferral at 91.9 s ("range stale") held off with core 1 2.5 m/s high,
  converging in 2 s.
- The near-zero speeds those resets repaired came from log61's mechanism:
  six aiding drops, each after up to 5 s over 45 deg, zeroed core 1's
  velocity (18.9 to 0.3 m/s at 51.4 s, GPS 18.7). Without a lockout reset
  to re-anchor it, core 1 stayed wrong for 8 s or more (113.4 s: 4-8 m/s
  against 16-25). The lockout reset needs the range finder, which is why
  log65 recovered and log61, above range, did not.
- #34457, card item 1: 23 s armed on the ground after landing. Both cores'
  AGL KF held the 0.05 m floor throughout (core 0 0.53 at touchdown). The
  main filters read -0.2 m/s on the ground, under the 0.25 m/s coast
  stop log39 defeated. Passed. The main filters' terrain height read 0.4-1.8
  m below the ground there (range out of range low), not the AGL KF's.

## 2026-10-30: SFD-O4 logs 62-63, hover Z-bias packs and full-stick flow Loiter

Firmware `52b1d36f`, GPS lane (core 0) for both hovers, LOITER at 2-3 m.

#32471, card item 2. log62 (`ACC_ZBIAS_LEARN 9`, learn only, 58 s hover)
saved -0.029/-0.035 against XKF2.AZ at the end of the hover of
-0.020/-0.026: the code saves the total bias, not the card's "end minus
arming" (-0.050 at arming, so that would be +0.03). log63 flight 1 (11,
learn and apply, 65 s) failed all three of the card's checks: AZ swung
0.06/0.07 after takeoff against 0.04, and the EKF height against the range
finder over the first 20 s was 0.14/1.10 m rms against 0.04/0.64 m. Baro
stayed within 0.2 m of the range finder in both, so core 1 (no GPS VZ) was
following its own vertical estimate.

Why: the saved total includes the ground bias, and with bit 2 clear the
EKF has learned this boot's ground bias into its state while disarmed.
Nothing offsets the state when the correction switches on at arming, so the
ground bias is counted twice (ARCH-7). The residual then has to climb by
about the previous flight's ground bias (-0.05 in log62; it climbed
0.06/0.07). Replay of log63 flight 1 with RISK.VRFBZ patched, same sensor
data (reproduces C=0/1 exactly):

| Correction | Core 0 height err, 2-20 s rms | Core 1 | Core 1 AZ swing |
|---|---|---|---|
| saved total, as flown (-0.029/-0.035) | 0.14 m | 1.10 m | 0.07 |
| none | 0.12 m | 0.65 m | 0.04 |
| saved total with bit 2 (15; REV3 16/17 inserted) | 0.33 m | 0.75 m | 0.10 |
| vibration part only, from log62 (+0.021/+0.015) | 0.11 m | 0.54 m | 0.03 |

Applying the total with bit 2 clear is worse than applying nothing.
#32471's record predicts that: its "Store the motors-on delta" row puts the
double count on bit 2 and rejects storing the delta for fleet compatibility,
so pack 2 should have flown 15. Replayed as 15 the double count is gone (AZ
0.00 at arm), but the release at arm restores the bias variance and the bias
swings to +0.06/+0.09 after liftoff; core 0 is worse than with no
correction. The delta is best on this flight: the first real-flight evidence
against that row, which stands until decided. Fly 9 until a pack at 15 has
flown. The saved total also moved 0.025 between two consecutive flights
(log63: -0.018 then -0.044), as large as the vibration offset itself.
(Corrected the same day: this said "fly 9 until #32471 saves the vibration
part", written before the record's rejected row was read.)

#33568, card item 4 (log63 flight 2, flow lane, `LOIT_SPEED_MS 15`): five
full-stick pushes of 0.5-1.6 s at 3-6 m reached 9.9 m/s. The flow speed cap
there is 20-36 m/s ((`EK3_FLOW_MAX` - 1) x height), so it never bound; it
binds only below `LOIT_SPEED_MS` / 6.4, 2.3 m. Not tested. What it did show:
core 1 tracked GPS within 0.5-1 m/s throughout. Along each push direction
the velocity never went negative after release, but stick came back 1.1-3.0
s later with the vehicle still at 0.85 m/s or more, so no stop completed
hands-off. #33318 did not run (`LOIT_OPTIONS 1`, its bit 1 clear); this was
Loiter with #33639. (Corrected the same day: this credited #33318 with "no
lurch back".)
#34457: 6 s armed after landing, core 1's AGL KF held 0.12 m.

## 2026-10-30: SFD-O4 log61, Loiter on the flow lane did not stop

Firmware `52b1d36f`, GPS on core 0 and flow on core 1 (`EK3_SRC_OPTIONS 8`,
`EK3_OPTIONS 94`). Source set 2 was selected disarmed at 136.7 s, so core 1
was primary for the whole acro sortie: up to 40 m, above the 15 m range
finder for most of it, and over 45 deg of tilt for 32% of the flight.
Loiter at 255.8 s, at 26 m and 13 m/s, held a 32 deg lean and sped up to
14.5 m/s; the pilot went back to acro at 260.1 s and to source set 1 at
268.5 s. Loiter on GPS at 278.5 s stopped from 10.5 m/s in 1.8 s.

Why Loiter did not stop: core 1 read 0.3-0.9 m/s. Loiter seeds its velocity
I term from the current lean (6 m/s/s east), saw no motion, and kept the
lean as if holding against wind (PIDE.I 6.0 rising to 8.9).

Why core 1 read zero (Replay reproduces core 1 to 0.1 m/s):

- 215.7-220.6 s over 45 deg of tilt, so no flow was fused; the IMU carried
  the velocity (17.9 against GPS 18.3 m/s at 220 s).
- 220.5 s: the 5 s flow timeout took core 1 to AID_NONE and straight back
  to AID_RELATIVE, which zeroes horizontal velocity (upstream; #33568 keeps
  it only for absolute to relative). 18.8 to 0.8 m/s at 19 m/s true, with
  4.5 s more over 45 deg. Again at 235.3 and 240.3 s, with runaways to
  37-48 m/s between. Core 1 ended 200-340 m from GPS.
- AP_Terrain looks SRTM up at the primary's position, here core 1's, and
  200-340 m off it found ground 25-35 m higher (RTER 25-35 m, 0-8 m
  before). With bit 2 the flow fusion scaled by `-PD - RTER`, below zero
  and clamped to the floor, so fusion drove the velocity to zero. This is
  the collapse #33585's record leaves open; a relative lane's position is
  not a location (ARCH-16).

Replay A/B, core 1 at 254-260 s against GPS 13-14.5 m/s: as flown 0.3-3.9;
bit 2 off (#33585's anchor, following the database's shape at the drifted
position) 4.5-6.8; RTER stripped from the log (flat ground only) 20-30. All
three identical from 222 to 234 s: no terrain setting gives a usable
velocity once the lane has been reset.

| PR | Status |
|---|---|
| #34456 | Worked: set 2 moved the primary to core 1 (136.7 s), set 1 back to core 0 in flight (268.5 s). |
| #33585, bit 2 | Ran above range throughout. Bit 2 bypasses the anchor; see the A/B above. |
| #33569 | Active: nav gain scaler about 0.48 on core 1 at 25 m. No bearing on a zero velocity. |
| #33318, #33639 | Loiter on GPS stopped from 10.5 m/s in 1.8 s, no overshoot. #33318 itself did not run: `LOIT_OPTIONS 1` leaves its bit 1 clear (corrected the same day). |
| #33568 | Not exercised: GPS never lost on core 0. |
| #33484, #34630 | Not provoked: XKF7.FVC 0 on both cores. |
| #34543, #34630 OSD | Ran, but core 1 reported itself healthy throughout (XKF4.SS unchanged, SV and SP 0), so nothing could flag it. What was drawn needs the pilot. |
| #33478, #33507 | AGL KF valid 68% of samples, velD fused under 1%. Not exercised. |
| #34457 | Disarmed 1.4 s after landing; AGL KF held 0.07-0.08 m on both cores with RFND out of range low. Card item 1 still needs 10 s armed on the ground. |
| #32768 | Arming stepped both cores' height 2.9 m (origin 41 s, armed 154 s). Hands off. |
| #32553 | "terrain offset reopened after takeoff" on both cores at 160.7 s. |
| #32471, `7ab6409ca3` | `ACC_ZBIAS_LEARN 8`: armed releases leaving acro at 255.8 and 278.5 s ran the new path. AZ moved at most 0.02 m/s/s (0.07 on core 1 at 278.5). No A/B yet. |
| #31274, DShot error counting | Ran all flight on bdshot: ESC.Err max 0.03-0.16% per output. |
| #32238, `16fe775812` | `BRD_FAST_BOOT 1` with `ESC_CALIBRATION 0`: the ESC calibration startup check waits for the radio again. 9 skips it. |
| #32514 | Source set change in flight at 268.5 s; no EKF failsafe. |
| #34210, #34696, #32475, #32270, #34678 | Not exercised. |


## 2026-10-30: squash audit of SFD-O4 logs 54-59 and the T25/TD25 flights

Which held-back commits these flights ran: SFD-O4 log54 (`0e2c9772`), 55-57
(`6d754361`) and 58-59 (`59386e92`); Tae's T25 on `5b70a271` and twice on
`308a7e73`, and TD25 on `0e2c9772`. Evidence is a message or event only the
changed path produces, or a Replay A/B through the beta at `6307d4d424`
with the change removed (log54-59).

| PR | Change | Status |
|---|---|---|
| #33484 | range step deferral and its speed form | Flown: "flow recovery deferred, range step", only printed when the speed-error test says the range has not settled, at 176.89 s in TD25's operational flight and 123.03 s in the T25 runaway's crash. Squashed. |
| #33484 | reset only on a sample the other axis passed | Ran at three resets (log57 core 1 941.4 s, T25 122.4 and 124.0 s); Replay without it identical on all three logs. Stays on top. |
| #34630 | quiet reset messages, lane reset flag | Flown: the three resets counted in `XKF7.FVC` with `EK3_OPTIONS` bit 6 and no message. Nothing to squash: the PR had not flown, so these are features, not fixes on flown commits. |
| #34543, #34630 | OSD lane items, flow signal strength, QMIN floor | Ran every frame on both vehicles (`OSD_TYPE` 5, `OSD1_EKFn_EN` 1; QMIN 60 on the T25). What was drawn is not in the log: needs the pilots. |
| #33359 | the four flow-axis gates | Changed the decision in flight: all-axes-pass in log57 (644.2-644.4, 941.2-941.6 s) and log58 (200.4-200.6 s), the hidden-speed test in log56 (149.4 s), the stamp ordering in log56 and log57; the first gate never. With all four removed the flow lane's height moves up to 0.10 m in log57 (GPS lane primary), identical elsewhere. Not squashed: three of the four also change the body odometry path, which no flight has run. |
| #34380 | flat-ground return on fused range readings | Replay without it identical on log54-59. Not flown. |
| #34457/#33507 | floor hold when the range reads too low | Replay without it identical on log54-59: never engaged. Not flown. |
| #34696 | restore waits for a position | No EKF failsafe cleared in flight: the T25's cleared after disarm, log54's after touchdown cancelled the restore. |
| #32514 | failsafe count held through spool-down | No failsafe began during a spool-down. |
| #34292 | rest reading cleared after 1 s without flow | No flow gap over 1 s in any log. |
| #32270 | VALT offset snap and loop-rate blend (`ee9dc05040`) | The blend runs in every 200 Hz VALT flight; the snap needs a held offset or surface tracking. One commit, half flown. |
| #34851, #34933 | | Flown (log58/59 Replay; the T25 on `308a7e73`); nothing on top. |


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
| #34292 | `FLOW_HGT_MIN 0` sets no focus height of the sensor's own; the floor stays at the range finder ground clearance plus 5 cm (0.15 m with the default `RNGFND1_GNDCLR` 0.10), so it acted only at liftoff, at touchdown and on the ground armed. (Corrected 2026-10-18: this said `FLOW_HGT_MIN 0` disables the floor, which the code does not do.) |
| #32471 | `ACC_ZBIAS_LEARN 8` sets only the acro bit; learning (bit 0) and applying (bit 1) are off. |

Open from these logs: log32 core 0 departs from its Replay at rounding level
from 5.2 s, just after the recorded-origin boot that only log32 has,
reaching 1.4 m in the terrain offset by 80 s and 7 cm in PD by 339 s. It is
not precision, and the cause is not yet found. Leaving the RC source set
switch on set 2 at boot, with a recorded origin, gives it another chance to
show.
