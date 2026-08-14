# SRCF field test log - session 4

Two threads, 2026-08-14. The small quad (SmallFastDronev1) flew the
position offset detector enabled for the first time - log 338, against
`df3037db`. And a tester's X8 ("eudrone", MatekH743-bdshot on an
OCTAQUAD/X_REV, against `60b12780`) was blocked from arming by a
growing "EKF3 Yaw inconsistent" pre-arm - logs 50/51, root-caused to
`EK3_MAG_CAL = 7`, fixed on SmallFastDrone-4.7-beta and cherry-picked
here, Replay-validated, not yet flown.

## The position offset detector's first flight - log 338

`SRCF_POSD_NSIG = 4` live, everything else at the small quad's values
(`SRCF_VEL_THR` 1.6, `SRCF_POSR_THR` 1.9 - not the octaquad's 2.6).
108 s armed in LOITER, gentle: ground speed mean 0.9 m/s, max 3.9.

| | result |
|---|---|
| `VVot` / `PVot` / `OVot` | 0 for the entire flight |
| `PD` at disarm | 4.05 m, monotonic, ~2.2 m/min of flow-lane dead reckoning |
| `PSig` | 0.75 -> 1.86 m; the 2 m floor was the denominator throughout |
| peak floored ratio | **2.03** - 51% of the 4.0 gate |
| rate detector margins | `VD` max 0.41 vs 1.6, `PR` max 0.36 vs 1.9 |

The unfloored `PD/PSig` crept 1.93 -> 2.06 -> 2.18 across the flight
and was still rising at disarm, inside the replay envelope (worst
benign 2.32, log 332). So 4 holds with 49% margin where 3 would have
left 27%, under the 30% rule. Keep 4. What the number needs next is a
longer soak: the design assumes `PSig` growth eventually catches `PD`
growth and the ratio plateaus, and a 108 s flight cannot show the
plateau.

The `EKF_YAW_RESET` at 169.4 s is the ordinary first in-flight yaw
alignment (both lanes, 169.4 and 177.0 s), with no step in `PD`.

Not covered, deliberately: a GPS-loss cycle with the detector
enabled, any real speed, and the detection half - the slow-walk catch
still rests on SITL (`SRCFSlowSpoofPositionOffset`) alone. This
flight closes session 3 open item 4's first half only: the detector
has now flown benign at 4.

## The X8 that would not arm - eudrone logs 50/51

Pre-arm "EKF3 Yaw inconsistent" growing without bound - 86 -> 114 deg
over log 50, 99 -> 169 over log 51 - so "Wait" could never clear it.
The suspect was the dual-lane architecture. It is exonerated, and
credited.

The mechanism, measured: `EK3_MAG_CAL = 7` puts both cores in 3-axis
mag fusion with field-state learning while on the ground
(`XKFS.MAG_FUSION = 2` from 2.6 s). A stationary vehicle gives that
fusion no yaw observability - a yaw error is absorbed by the body
field states with zero innovation, and `XKF4.SM` stayed under 0.34
while the yaw walked. The trigger is origin set (~17.5 s in both
logs): the earth field states reset to the WMM table, the local field
disagrees (the site is disturbed - "Check mag field (xy diff:333>300)"
and a 3 s raw-mag transient at 33-36 s in log 51), and each core
reconciles the mismatch by rotating yaw its own way, then
free-integrates its own gyro-Z bias at ~0.35 deg/s. Core 0's learned
body biases walked to (-90, -118) mGauss absorbing it. DCM, on the
same magnetometer, held 54-61 deg throughout.

Why the lanes are credited rather than blamed: the pre-arm number is
the core-vs-core split (verified 87.8 measured vs "86" reported), and
with two active cores the DCM yaw comparison is skipped - a
single-lane setup would have raised no message and **armed** with a
yaw estimate 100+ deg wrong and rotating.

The fix (`1196739139` on SmallFastDrone-4.7-beta, `9621bf2cd4`
here): while mode 7
is learning on the ground, fuse the magnetic heading alongside the
3-axis fusion. The heading comes from the raw compass, not the
learned states, so the anchor is not circular; with yaw pinned and
the earth field held to the tables, the body biases become observable
at a fixed heading - which is the battery-signature learning the mode
exists for. Replay A/B on the failure logs, same tree, only the fix
different:

| | log 51 core 0 | log 50 cores at 141 s |
|---|---|---|
| unfixed | 54 -> 207 deg over 105 s | 336 / 90 deg - 114 deg apart |
| fixed | holds ~54, tracks the real 33 s field shift to ~62 as DCM did | 53.3 / 54.2 deg |

In the fixed replay the body field states converge to (-7, 57, 119)
mGauss and settle instead of walking - the learning now works.

Bench-validated on the small quad, log 339, against `9621bf2c`
itself: 47 s on the ground through origin set with `MAG_FUSION = 2`
the whole time, both cores within 0.2 deg of DCM, then a ~200 deg
hand rotation that both cores tracked to within 1.6 deg of the
compass and 0.7 deg of each other. Body field states settled at
(12, 19, 19) mGauss. The unfixed filter rotated its yaw without the
vehicle moving; the fixed one moves only when the vehicle does. The
small quad never provoked the failure unfixed, so the disturbed-site
half of the evidence rests on the log 50/51 replay; the eudrone's
next outing at its own site closes that for free.

Consequences:

- The fix is bench-validated (log 339, above) and on both branches.
  Remaining: the eudrone at its own site, whose next outing covers
  it.
- The X8 default parameter file ships `EK3_MAG_CAL = 7`: fine with
  the fix, a latent arm-blocker (or worse, single-lane) without it.
  The small quad also flies 7 and armed clean on log 338 - its site
  and calibration are friendlier, not immune.
- The eudrone needs a compass calibration regardless; its field
  correction is off by a couple hundred mGauss, which is what made
  the WMM reset violent.
- Stock `EK3_MAG_CAL = 4` (ALWAYS) has the same on-ground defect.
  Left stock deliberately.

## Still open

1. Session 3 items 1-3 and 5-8 stand unchanged. Item 4 is half
   closed: `SRCF_POSD_NSIG` has now flown benign at 4, one short
   gentle flight.
2. The offset detector still needs the long soak (ratio plateau), a
   GPS-loss cycle with it enabled, and an in-flight detection test.
3. The mag cal fix is Replay-validated on the failure logs and
   bench-validated on the small quad (log 339). Outstanding only:
   the eudrone at its own disturbed site.
