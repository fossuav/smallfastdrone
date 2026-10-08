# SFD flight card

Firmware: the current beta (topup11 or later). Keep `EK3_OPTIONS 94`. Fly
every flight with `LOG_REPLAY 1` and `LOG_DISARMED 2`. Keep STABILIZE on a
switch. What earlier flights showed is in `FLIGHT_VALIDATION.md`.

## Flights, in order

### 1. #33359 step-up: the log29 sortie again (twice)

`EK3_RNG_USE_HGT 6`, `EK3_OPTIONS` bit 3 (94 has it), GPS lane (set 1),
LOITER.

1. Take off from the top of the step and hover at about 0.45 m for 10 s.
2. Fly off the edge and hover over the lower ground for at least 10 s.
3. Fly back over the top, hover 10 s, and land on top.

Pass:

- Crossing back up, the log shows "EKF3 IMU0/1 terrain offset reset from
  range" if the offset is stale, and the EKF altitude does not step down
  when the switch re-engages.
- Landed back on top, the EKF altitude is within about 0.15 m of where it
  was before takeoff (log29: -0.45 m).
- Off the edge it still hands back to baro within about 0.3 s with no
  altitude step.

Risk: the old behaviour stepped the EKF altitude down 0.4 m at about 0.3 m
up, and Copter moves its target with a height reset. Hover no lower than
0.4 m.

### 2. #34380: flow lane climb on per-core lanes

`AVOID_ENABLE 1`, `EK3_SRC_OPTIONS 8`. Select set 2 before arming (or in
flight, which needs `EK3_OPTIONS` bit 1).

1. Take off on the flow lane and climb to 5 m in LOITER.
2. Climb at full stick from 5 m to about 25 m.
3. Descend back into the range finder's reach and land.

Pass: it climbs through 9.5 m (0.7 x 15 m - 1 m) without being held,
relative position stays valid, and the descent back into range is clean
(#33585: the fallback ends once the range has been back for 2 s, with no
step in XKF5.HAGL). Without #34380 AC_Avoid would hold it at about 9.5 m.

### 3. #33568 with #34380: GPS to flow on a single source set

`AVOID_ENABLE 1`, `EK3_SRC_OPTIONS 0`, so all cores share one active set.
This is the case from the 4.7.1 forum report rmackay9 linked on #33568.

1. Take off on set 1 (GPS) and climb to 5 m.
2. Switch to set 2 (flow). Hover for 20 s, then fly back and forth at
   3-5 m/s.
3. Climb at full stick from 5 m to about 25 m in LOITER. Hover there and
   fly a short leg.
4. Switch back to set 1 and land.

Pass for #33568:

- XKF4.AID goes from 0 to 2 within a few seconds of the switch, and back to
  0 after returning to set 1.
- The flow speed limit visibly caps LOITER speed.
- No position-controller limit cycle at 25 m.

Pass for #34380: the climb passes 9.5 m without being held and relative
position stays valid above it. If the return is lost between 9 and 10.5 m,
the limit comes back to 9.5 m within 0.5 s and the vehicle holds where it
can still measure. Do not fly the opposite case, the range finder as the
height source with the vehicle backing down into range; SITL covers it.

### 4. #32471: hover Z-bias learning, two packs

Pack 1, `ACC_ZBIAS_LEARN 9` (bit 0 plus the acro bit): hover steadily in
LOITER at 2-5 m for at least 60 s, land and disarm. On disarm
`INS_ACC_VRFB_Z` and the second IMU's value are saved; they roughly equal
XKF2.AZ at the end of the hover minus its value at arming (the PR measured
about 0.09 m/s/s on two airframes).

Pack 2, `ACC_ZBIAS_LEARN 11` (bits 0 and 1 plus the acro bit), same
takeoff and hover: XKF2.AZ starts close to its hover value and moves less
after takeoff than in pack 1, and the EKF height against the range finder in
the first 20 s after liftoff has a smaller error. Replay reproduces the
applied bias, which is carried in RISK. Bit 2 (moving platform) needs arming
on a car or boat; skip it unless convenient.

### 5. #34630, if time allows: lockout resets in low acro

Hard acro on the flow lane while staying inside the range finder's reach,
below about 12 m, so a lockout has a fresh range to recover against. Watch
the OSD lane item: a reset turns that axis's arrow off for 250 ms, with no
message (bit 6). XKF7.FVC counts them.

### 6. #34678, optional ground check: righting a flipped copter

On soft ground with the props clear, put the copter on its back, arm in
STABILIZE and raise the throttle to roll it upright. Pass: no "Internal
Error" message (flow_of_control) and it re-arms afterwards without a
reboot. It stays landed until upright, so the crash check does not disarm
it mid-roll.

## Afterwards

Restore `EK3_RNG_USE_HGT 6`, `EK3_SRC_OPTIONS 8`, `AVOID_ENABLE 0`, and
`ACC_ZBIAS_LEARN` as wanted. Keep `EK3_OPTIONS 94`.

## Notes

- Flights 1 and 4 can share packs, because the bias learning does not
  depend on the height source. Keep flights 2 and 3 separate so the
  source-set changes do not overlap.
- Leave the RC source set switch where it is at boot; a boot on set 2 with a
  recorded origin gives the log32 Replay divergence (FLIGHT_VALIDATION.md)
  another chance to show.
- Baselines are Replays of the same logs with the PR's change removed: beta
  without `d52e4a8a8c` for #33359, without #33568, and without #32471's bias
  application. They can be built before flying.
