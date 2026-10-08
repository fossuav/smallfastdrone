# SFD flight card

Firmware: the beta with the #33478 step hold (topup12 or later). Keep
`EK3_OPTIONS 94`. Fly every flight with `LOG_REPLAY 1` and `LOG_DISARMED 2`.
Keep STABILIZE on a switch. What earlier flights showed is in
`FLIGHT_VALIDATION.md`.

## Flights, in order

### 1. #33478 and #33359: the log40 step sortie again

The same flight as log40, on topup12. `EK3_RNG_USE_HGT 6`,
`EK3_SRC_OPTIONS 8`, GPS lane (set 1) primary, so core 1 runs flow and
fuses the AGL KF velocity as velD (bit 4) while core 0 flies the vehicle.
LOITER, hovering at about 0.45 m on top, below 0.9 m over the lower
ground.

1. Take off from the top of the step and hover 10 s.
2. Fly off the edge and hover over the lower ground for at least 10 s.
3. Fly back over the top, hover 10 s, fly off and back once more, and land
   on top.

Pass:

- Off each edge, core 1's altitude (XKF1 C=1) stays within about 0.3 m of
  core 0's. On log40 it climbed 0.9 m in 6 s with baro flat.
- XKFA.VFuse on core 1 drops for up to 5 s after each edge and then
  resumes.
- Landed back on top, both cores within about 0.3 m of their altitude
  before takeoff (log40: core 0 -0.16 m, core 1 +0.36 m).
- #33359 as before: the hand-back to baro off the edge with no altitude
  step, and "terrain offset reset from range" where the offset is stale.

Risk: as log40. Hover no lower than 0.4 m.

### 2. #34630, if time allows: provoke a lockout

log43 never tripped one: flow quality stayed above 144 and no axis
dropped out. Fly harder: full-rate roll and pitch flips in ACRO or
STABILIZE at 3-8 m, inside the range finder's reach, on the flow lane.
Watch the OSD lane item for an arrow going off for 250 ms. XKF7.FVC counts
the resets.

### 3. #33568, optional: the flow speed cap

log42 never reached the flow speed limit. On the flow lane in LOITER below
about 5 m, full stick in one direction for 3 s. Pass: ground speed is held
below the flow limit (EKF3 flow speed cap) rather than `LOIT_SPEED`.

## Afterwards

Restore `EK3_RNG_USE_HGT 6`, `EK3_SRC_OPTIONS 8`, `AVOID_ENABLE 0`, and
`ACC_ZBIAS_LEARN` as wanted. Keep `EK3_OPTIONS 94`.

## Notes

- Baselines are Replays of the same logs with the PR's change removed.
  For flight 1, the topup11 Replay binary (no step hold) is the baseline.
- The aiding mode change after a source set switch to flow takes the
  designed 10 s (`posRetryTimeUseVel_ms`), not a few seconds.
- Coming back into range finder reach after more than 5 s out of it, the
  terrain offset snaps to the range in one sample (upstream EKF3). The
  step is the height drift built up while out of reach (log41: 1.3 m on
  both cores), so judge the return by HAGL agreeing with the range
  afterwards, not by the absence of a step.
