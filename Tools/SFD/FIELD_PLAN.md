# SFD flight card

Firmware: topup12 or later; flight 1 needs a beta carrying #34457's floor
hold (`7f83487786`, topup13). Keep `EK3_OPTIONS 94`. Fly every flight with
`LOG_REPLAY 1` and `LOG_DISARMED 2`. Keep STABILIZE on a switch. What
earlier flights showed is in `FLIGHT_VALIDATION.md`; the log40/log44 step
sortie is done.

## Flights, in order

### 1. #34457: land after hard flying and stay armed

The case log39 hit: after an acro sortie a lane's main filter thought it
was climbing on the ground, and its AGL KF height coasted 1.5 m. Fly hard
acro on the flow lane inside the range finder's reach (below about 12 m),
then land in LOITER or ALT_HOLD and stay armed on the ground for 10 s
before disarming.

Pass: on both cores XKFA.HAgl stays within about 0.3 m of its floor for
the whole time on the ground (log39 core 1: 0.28 -> 1.8 m), and XKF1.VD
on the ground shows whether the main filter thought it was climbing.

### 2. #32471: hover Z-bias learning, two packs

Pack 1, `ACC_ZBIAS_LEARN 9` (bit 0 plus the acro bit): hover steadily in
LOITER at 2-5 m for at least 60 s, land and disarm. On disarm
`INS_ACC_VRFB_Z` and the second IMU's value are saved; they roughly equal
XKF2.AZ at the end of the hover minus its value at arming.

Pack 2, `ACC_ZBIAS_LEARN 11` (bits 0 and 1 plus the acro bit), same
takeoff and hover: XKF2.AZ starts close to its hover value and moves less
after takeoff than in pack 1, and the EKF height against the range finder
in the first 20 s after liftoff has a smaller error. Flight 1 can share a
pack with either.

### 3. #34630, if time allows: provoke a lockout

log43 never tripped one: flow quality stayed above 144 and no axis
dropped out. Fly harder: full-rate roll and pitch flips in ACRO or
STABILIZE at 3-8 m, inside the range finder's reach, on the flow lane.
Watch the OSD lane item for an arrow going off for 250 ms. XKF7.FVC counts
the resets.

### 4. #33568, optional: the flow speed cap

log42 never reached the flow speed limit. On the flow lane in LOITER below
about 5 m, full stick in one direction for 3 s. Pass: ground speed is held
below the flow limit (EKF3 flow speed cap) rather than `LOIT_SPEED`.

### 5. #34678, optional ground check: righting a flipped copter

On soft ground with the props clear, put the copter on its back, arm in
STABILIZE and raise the throttle to roll it upright. Pass: no "Internal
Error" message (flow_of_control) and it re-arms afterwards without a
reboot.

## Afterwards

Restore `EK3_RNG_USE_HGT 6`, `EK3_SRC_OPTIONS 8`, `AVOID_ENABLE 0`, and
`ACC_ZBIAS_LEARN` as wanted. Keep `EK3_OPTIONS 94`.

## Notes

- Baselines are Replays of the same logs with the PR's change removed.
  For flight 1, the topup12 Replay binary (no floor hold) is the baseline.
- The aiding mode change after a source set switch to flow takes the
  designed 10 s (`posRetryTimeUseVel_ms`), not a few seconds.
- Coming back into range finder reach after more than 5 s out of it, the
  terrain offset snaps to the range in one sample (upstream EKF3). The
  step is the height drift built up while out of reach (log41: 1.3 m on
  both cores), so judge the return by HAGL agreeing with the range
  afterwards, not by the absence of a step.
