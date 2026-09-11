# Plan: six fixes from the 2026-09-09 flow-vs-GPS lane flights

Written 2026-09-09 as a working plan. **All six are implemented as of
2026-09-10** - see "Outcome" at the end. The design and the reasoning below
are left as written; where the work contradicted them, the outcome section
says so rather than the text being edited.

Branch: `SmallFastDrone-4.7.1-beta`, all six as individual commits, then
ported to their target PRs.


## The flights, in three lines

Two flights on 2026-09-09, an outdoor BF_X quad, `RNGFND1_MAX=15` on an
airframe that flies well above it. `log5` hit an EKF failsafe climbing past
the rangefinder ceiling with `EK3_OPTIONS` bits 2 and 5 clear; `EK3_OPTIONS=62`
fixed that. `log7` then flew with `EK3_SRC_OPTIONS=8` (SRC_PER_CORE), so a GPS
lane (core 0) and a flow-only lane (core 1, `VELZ=None`) ran on the same IMU,
baro and rangefinder at the same instant. That rig is what makes the findings
below attributable.

The headline: `touchdown_expected` latched for 51 s in a stationary 17.9 m
hover, which opened the EKF ground-effect baro protection, and the flow lane's
altitude ran away 5.6 m against a flat baro with `XKF3.IPD` pinned at exactly
-0.5000. The GPS lane was immune because GPS velZ held it.


## Where the evidence lives

| | |
|---|---|
| `../analysis/logs/log7_sfdo4.md` | the full flight note, all numbers. **Private repo.** |
| `../analysis/logs/log5_sfdo4.md` | the before case |
| `../analysis/vehicles/SFD-O4.md` | airframe identity and configuration |
| `../analysis/topics/optflow_horizontal_velocity_lockout.md` | the acro flight test of the lockout recovery |
| `../ardupilot-pr-analysis/new-groundeffect-touchdown-gate/` | fixes 1a, 1b, 2 - design, alternatives, validation |
| `../ardupilot-pr-analysis/new-ekf3-hagl-terrain-alt/` | fix 3 |
| `../ardupilot-pr-analysis/new-ekf3-percore-optflow-logging/` | fix 6 |
| `../ardupilot-pr-analysis/33484/` | fixes 4 and 5, appended 2026-09-09 |
| `../ardupilot-pr-analysis/32972/` finding 6 | the EKF-side cost |
| `../ardupilot-pr-analysis/32472/` | the merged PR the gate defects belong to |

`../analysis` and `../analysis-private` are private and must never be cited by
name in a PR body, PR comment or commit message. See `PR_REVIEW_RULES.md` in
this directory. Public-facing text says "flight tests show X" with the numbers.

The logs themselves are in no repo. `log5.bin` and `log7.bin` from the
2026-09-09 session need copying to whichever machine does the Replay work;
both carry `LOG_REPLAY=1`.

Posted publicly so far: two comments on
https://github.com/ArduPilot/ardupilot/pull/32972 (the finding, and a
correction to the field name behind `slow_descent`), plus, on 2026-09-10,
PRs #34360 and #34361 and a cross-reference comment on #33585.


## What Replay can and cannot settle

Checked, not assumed, and it decides the work. Do not re-litigate this.

`log_RFRN` carries `takeoff_expected` and `touchdown_expected` as bits
(`libraries/AP_DAL/LogStructure.h:98-99`) and `AP_DAL::get_touchdown_expected()`
reads them straight back. **Replay re-feeds the flags the flight recorded**, so
it can show what the innovation floor cost and can never show a gate fix
working. Separately, `getHAGL()` has no consumer inside the EKF - every caller
is vehicle code, which Replay does not run - so a Replay computes the corrected
value and reports nothing.

| Fix | Replay | SITL A/B | Autotest |
|---|---|---|---|
| 1a, 1b, 2 ground-effect gate | no | required | yes |
| 3 getHAGL terrain | no | required | yes |
| 4 reset range-freshness gate | **decisive** | secondary | re-verify existing |
| 5 reset count visible | no | sanity only | no |
| 6 per-core logging | no | confirm C=0 and C=1 differ | no |


## The six fixes

Line numbers are as of `797f6854` on this branch.

### Fix 6 - log XKF5 and XKFA for every core

`libraries/AP_NavEKF3/AP_NavEKF3_Logging.cpp:187`. Drop the
`core_index != frontend->primary` guard on `Log_Write_XKF5`. `XKFA` is written
inside that function and inherits the guard.

Why first: no behaviour change, and it is the reason the flight that motivated
all of this could not say whether `AglKfVelForVelD` ever fused on the lane
under test. Every `XKF5`/`XKFA` number in the flight note is the GPS lane's.
Do this and the next flight answers its own questions.

Leave the same guard on `Log_Write_Beacon`, `Log_Write_BodyOdom` and
`Log_Write_State_Variances` - the argument is weaker there.

Fallback if a reviewer objects on log bandwidth: gate on
`sources.option_is_set(SRC_PER_CORE)` instead. Measure the dataflash rate
before and after on the same SITL run so that argument has a number.

Commit: `AP_NavEKF3: log XKF5 and XKFA for every core`
Target: new PR against master.

### Fix 5 - make the flow velocity reset count visible

`libraries/AP_NavEKF3/AP_NavEKF3_OptFlowFusion.cpp:870`. The statustext is
gated on `flowVelResetWindowCount == 1` while `FLOW_RESET_MAX_IN_WINDOW` allows
5 per 10 s window, so up to four resets per window are silent. On log7 that
read as two resets when there were three, and the missing one was the
interesting half of a pair. Either put the count in the message or emit on
every reset and let `flow aiding unhealthy` carry the churn warning.

Commit: `AP_NavEKF3: report every optical flow velocity reset`
Target: #33484.

### Fix 3 - serve the terrain-database AGL from getHAGL()

`libraries/AP_NavEKF3/AP_NavEKF3_Outputs.cpp:330`. Add the SRTM branch between
the two that exist, mirroring what `FuseOptFlow` already does at
`AP_NavEKF3_OptFlowFusion.cpp:378-381`:

```cpp
#if EK3_FEATURE_OPTFLOW_SRTM
    if (!gndOffsetValid && terrain_srtm_alt_valid) {
        HAGL = terrain_srtm_alt - outputDataNew.position.z - posOffsetNED.z;
        return !hgtTimeout && healthy();
    }
#endif
```

Order matters: the AGL KF and `terrainState` measure the ground actually under
the vehicle, the database is a 30 m-posted model, so the database goes last.

**Do not serve the flat-ground assumption here.** `EK3_OPTIONS` bit 5
(`OptflowAssumeFlatGnd`, #33585) is on this branch and is an assumption, not a
measurement. `getHAGL()` callers include `AP_GroundEffect`, which uses it to
decide the vehicle is near the ground. Master has no bit 5, so this only bites
when the change is carried onto this branch - which is exactly what we are
doing, so get it right here.

This is the root fix for fix 1: with it, log7 takes the `height_is_agl` branch
and never reaches the drift fallback at all.

Commit: `AP_NavEKF3: use the terrain database AGL in getHAGL`
Target: new PR against master. Deliberately not folded into #33585 - bit 2 is
upstream, this stands alone, and #33585's option-bit review is delicate enough.

### Fix 2 - deadband on descent_demanded

`libraries/AP_GroundEffect/AP_GroundEffect.cpp:135`:

```cpp
const bool descent_demanded = d_active && target_climb_rate_ms < 0.0f;
```

`target_climb_rate_ms` is `_pos_control->get_vel_desired_U_ms()`, which is
`-PSCD.DVD` in a log. **Not `CTUN.DCRt`** - that logs `get_vel_target_U_ms()`,
a different signal, and an earlier reading of this flight got it wrong.

Measured over the 51 s latch on the correct field: negative in **100.00% of 508
samples**, p50 -0.0000, max -0.0000, longest continuous run 50.8 s. In a hover
the desired vertical velocity settles to a small persistently negative residual
and never reaches zero, so `< 0.0f` reads a commanded descent for as long as
the vehicle hovers.

Change to `< -AP_GROUNDEFFECT_DESCENT_DEADBAND_MS` with the constant at
0.05 m/s. Sized from data: 0.05 removes 98.4% of those samples, 0.01 already
removes 98.2% so the value is not delicate, and the same flight's real approach
ran -0.504 m/s at p50 and -1.90 at p5 - three orders of magnitude of margin.

A constant, not a parameter. `../ardupilot-pr-analysis/32472/` records what
happens when a threshold here competes with the operating envelope: it becomes
a floor on the useful hover band.

Note `slow_descent_demanded` on the next line also reads
`target_climb_rate_ms >= -1.0f`; leave that bound alone.

Commit: `AP_GroundEffect: deadband the commanded descent test`
Target: new PR against master (follow-up to merged #32472).

### Fix 1a - bound the touchdown latch

Same file. `takeoff_expected` already has an unconditional
`AP_GROUNDEFFECT_TAKEOFF_MAX_MS` (5 s) cap at line 115 for the same reason. A
touchdown that has not happened within a similar window is not a touchdown.

**Not sized from data.** Measure how long the gate is legitimately open across
a real touchdown in SITL before choosing the constant. Do that measurement
first; do not copy 5 s across because it is nearby.

Commit: `AP_GroundEffect: bound how long a touchdown may be expected`
Target: same new PR as fix 2.

### Fix 1b - do not infer ground proximity from drift alone

`libraries/AP_GroundEffect/AP_GroundEffect.cpp:155-157`:

```cpp
const float drift_ne_m = (pos_ne_m - _state.takeoff_pos_ne_m).length();
near_ground = (drift_ne_m >= AP_GROUNDEFFECT_TAKEOFF_DRIFT_NE_MAX_M)
              || (height_m < _alt_m);
```

Measured: drift was 21.8 m, 1.8 m past the 20 m threshold, so `near_ground` was
true unconditionally at 17.9 m. The comment above it argues for not trusting
`height_m` beyond that distance, which is not the same as asserting proximity.

Minimal inversion:

```cpp
near_ground = (drift_ne_m < AP_GROUNDEFFECT_TAKEOFF_DRIFT_NE_MAX_M)
              && (height_m < _alt_m);
```

**This is a real trade and it is why this one is last.** Failing this way
costs ground-effect protection on a baro-only vehicle landing more than 20 m
from its takeoff point - the case the branch was written for. With fix 3 in,
any vehicle with a rangefinder or terrain coverage takes the `height_is_agl`
branch and never reaches here, so the exposure is narrower than it looks, but
it is not zero. A/B the distant-landing case explicitly, not just the hover
repro.

If fixes 2, 1a and 3 clear the SITL repro on their own, this one still goes in
because the rule is wrong on its own terms - but land it separately so it can
be reverted alone.

Commit: `AP_GroundEffect: do not infer ground proximity from drift alone`
Target: same new PR as fix 2.

### Fix 4 - gate the flow velocity reset on range freshness

`libraries/AP_NavEKF3/AP_NavEKF3_OptFlowFusion.cpp:839-857`. The reset requires
`aglKfValid`, which survives 5 s without a range fusion - long enough for
`aglKfH` to coast metres low. Add a freshness requirement using
`lastAglRngFuseTime_ms`, which master carries alongside `aglKfValid`;
`aglKfRngGapMax_ms` is **not** in master, so define a local constant here
rather than stacking this on #33478. #33478 computes the equivalent as
`aglKfRngCurrent` at `AP_NavEKF3_PosVelFusion.cpp:802` - match its 500 ms.

Why: on log7 the recovery fired three times and two re-anchored to roughly a
fifth of the true velocity (13.98 -> 6.58 and 14.67 -> 2.87 m/s against 12.3
and 12.7 truth, one 100 ms sample each). All three fired in the first samples
after `cos(tilt)` climbed back through `DCM33FlowMin` (0.71). That gate stops
flow fusion and AGL KF range fusion together, so it creates the single-axis
staleness the lockout detects *and* leaves `aglKfH` coasting at the moment
`ResetVelocityToFlow` needs it as a scale factor. The recovered velocity is
linear in `range`, and `aglKfH` was 2.20 m against a true vertical AGL of
6.23 m at the worst one.

All three had been without range for longer than 500 ms, so the gate suppresses
all three. **The open question is whether it also suppresses the indoor
recoveries logs A/B/C wanted.** That is the Replay sweep, below - rerun it with
the gate in and read the excursion/reset table the same way.

Consider pairing with a blended rather than stepped re-anchor. 12.5 m/s in one
100 ms sample is not a correction a position controller can absorb, and the
position-snap experiment in
`../analysis/topics/optflow_horizontal_velocity_lockout.md` shows big
instantaneous corrections on this path make hold quality worse.

Commit: `AP_NavEKF3: require a current range before re-anchoring flow velocity`
Target: #33484.


## Order

1. Fix 6 (logging)
2. Fix 5 (reset count)
3. Fix 3 (getHAGL terrain)
4. Fix 2 (deadband)
5. Fix 1a (latch bound) - measure the constant first
6. Fix 4 (reset freshness) - then the Replay sweep
7. Fix 1b (drift inversion) - separate commit, separately revertable

Every commit must build. `./waf copter` after each, and `./waf plane` at the
end: `Tools/SFD/REFRESH_NOTES.md` records that a copter-only pass misses
plane-only breaks.

Run `Tools/SFD/check_param_tables.py` before any test run if a parameter is
added. Fix 2's deadband is a constant, so this only matters if that changes.


## Commands

```sh
./waf configure --board sitl && ./waf copter

# existing tests that touch this code - run before and after
Tools/autotest/autotest.py test.Copter.EK3_FlowAxisLockoutRecovery
Tools/autotest/autotest.py test.Copter.EK3_OptflowAssumeFlatGnd
Tools/autotest/autotest.py test.Copter.EK3_AglKfVelForVelD
Tools/autotest/autotest.py test.Copter.BaroGroundEffectAtTakeoff
Tools/autotest/autotest.py test.Copter.TakeoffGroundEffectAlt
Tools/autotest/autotest.py test.Copter.TouchdownGroundEffectAlt

# Replay (fix 4 only)
./waf configure --board sitl && ./waf replay
build/sitl/tool/Replay <log7.bin>
```

The A/B recipe for before/after runs - merge-base build in a scratch worktree,
throwaway harness, logs moved aside between runs - is under "Before/after A/B
runs" in `Tools/autotest/CLAUDE.md`.


## New SITL tests needed

Each must be demonstrated to **fail on unfixed code**.
`../ardupilot-pr-analysis/32972/` and `32472/` both record tests that
discriminated nothing: `BaroGroundEffectAtTakeoff` arms in ALT_HOLD at idle and
therefore tests the anchor against a glitch rather than a takeoff, and an
assertion on `GLOBAL_POSITION_INT.relative_alt` passes whether the code works
or not because AP_AHRS falls back to raw baro exactly in the state the test
creates.

1. **Gate repro** (fixes 2, 1a, 1b): hover at 20 m, more than 20 m from the
   takeoff point, no rangefinder in range. Assert `XKF4.SS` bit 12 clear.
   Before: set for the whole hover.
2. **Gate still arms**: a real descent to touchdown at the same distance.
   Assert bit 12 sets. This is the one that catches an over-tight fix.
3. **getHAGL** (fix 3): flow vehicle above rangefinder range with terrain data
   served and bit 2 set. Assert on the height, not on a downstream flag.
   `../ardupilot-pr-analysis/33585/` records a test in this area that failed
   because the harness served no terrain data at all - check tiles are actually
   delivered before believing a green run.
4. **Per-core logging** (fix 6): `EK3_SRC_OPTIONS=8`, two cores, assert `XKF5`
   and `XKFA` carry both `C=0` and `C=1` *and that the values differ*. Two
   messages appearing is not enough.

A SITL test for fix 4's tilt-gate case would need a manoeuvre crossing
`DCM33FlowMin` with flow enabled. `SIM_FLOW_OFS` does not currently provide
that; check whether it can be made to before assuming an autotest is possible.


## Decisions already taken

- Fix 3 goes in its own PR, not into #33585.
- Fix 4 does not stack on #33478; it defines its own freshness constant.
- Fixes 1a/1b/2 are one new PR against master, three commits.
- Fix 2's deadband is a constant, not a parameter.
- Fix 6 prefers dropping the guard outright over gating it on SRC_PER_CORE.
- All six are wanted, including 1b, even if the earlier fixes clear the repro.


## Open questions

- ~~The constant for fix 1a.~~ Measured: 35.2 s is the longest legitimate
  window at the documented extremes of `GNDEFF_ALT` and `LAND_SPD_MS`, so the
  cap went at 60 s. See the outcome section for why that is weaker than the
  plan assumed.
- ~~Whether fix 4's freshness gate costs the indoor recoveries.~~ Replayed:
  it costs none of them and suppresses all three of log7's misfires.
- Whether to blend rather than step fix 4's re-anchor. **Still open.** Not
  attempted; still a separate change and a separate commit if done.
- Whether `SIM_FLOW_OFS` can be extended to drive a tilt-gate crossing.
  **Still open.** Not investigated.

## Outcome, 2026-09-10

Seven commits on the branch over base `fd37f6f5fa`, in the planned order:
`e825b34855` (fix 6), `ad8cc7fbf3` (5), `b003fc6c4d` (3), `a18992efe3` (2),
`6ae889d8a0` (1a), `23299c535c` (4), `0b1c1124b8` (1b), plus `cc97f59161`
and `181feececa` for the autotests. Copter builds after each, plane at the
end.

Two PRs against master, both stacked in that order because they touch
adjacent lines: **#34360** (a sign fix in `FuseOptFlow` the plan did not
know about, found while reviewing fix 3) and **#34361** (fix 3 itself).
Fixes 1a/1b/2, 4, 5 and 6 remain on the branch with no PR.

Where the work contradicted the plan:

- **Fix 1a's constant cannot do what the plan implies.** The plan reasoned
  from `takeoff_expected`'s 5 s cap that "a touchdown that has not happened
  within a similar window is not a touchdown". Measured, the legitimate
  window is `GNDEFF_ALT` over the descent rate and reaches 35.2 s at the
  documented extremes - so 5 s would have halved a default landing, and the
  60 s cap that clears it would not have fired on the 51 s hover that
  motivated the work. Fix 2 is what closes that case; 1a only stops a latch
  running for a whole flight.
- **Fix 3's proposed code had two defects the plan did not anticipate**, both
  caught in review: the terrain height is measured up from the origin while
  the position state is down, so substituting it into the `terrainState`
  expression inverts the sign; and the zero timestamp reads as fresh for the
  first five seconds of uptime. The same sign defect is in master's
  `FuseOptFlow`, which is #34360.
- **The plan's "do not serve the flat-ground assumption here" holds** and was
  verified: `flatGndAssumed` is a local in `AP_NavEKF3_Control.cpp` and
  writes nothing the new branch reads.

## Closed, 2026-09-10

All six fixes have a PR, and the three items left owed above are resolved.

| fix | PR |
|---|---|
| 1a, 1b, 2 - the touchdown gate | #34362 |
| 3 - terrain-database AGL from getHAGL | #34361, on #34360 |
| 4 - range freshness on the flow reset | #33484 |
| 5 - reset count visible | #33484 |
| 6 - XKF5 and XKFA per core | #34363 |

Plus two the plan did not foresee: **#34360**, the `FuseOptFlow` sign fix
found while reviewing fix 3, and **#34292**, the near-ground flow floor split
out of #33484.

**The dataflash cost for fix 6 is measured**: 21.9 kB/s becomes 22.5 kB/s on
the same autotest, +597 B/s for the two messages, 2.7% of the log. The
per-core ceiling is +450 B/s for XKF5 at 10 Hz, +1.13 kB/s at 25 Hz with
`MASK_LOG_ATTITUDE_FAST`.

**All four SITL tests exist**: `TouchdownGroundEffectCruise` (1),
`TouchdownGroundEffectAlt` subtest C, rewritten (2), `EK3_GetHaglTerrainAlt`
(3) and `EK3_PerCoreOptflowLogging` (4). The per-core test does not compare
values across cores as the section above proposed - `C` is written from
`core_index`, so a duplicated primary cannot alias and the comparison
discriminates nothing. It asserts the `XKFS` source set per core instead.

**The Replay of log7 cannot show what it was scoped to show, and should not
be carried as owed.** `AP_DAL::get_takeoff_expected()` returns
`_RFRN.takeoff_expected`, i.e. the flag is recorded in the DAL and replayed
verbatim. The fixes finding 6 is waiting on are in `AP_GroundEffect`, which
computes those flags *before* the DAL, so Replay feeds the EKF the flags as
they latched on the original flight no matter what the vehicle code now
does. What log7 can still answer is the EKF-side question - how much of the
5.6 m is the innovation floor and how much the observation deweighting -
by replaying it at `EK3_GND_EFF_DZ` +4 against -8. That belongs to
`../32972/` M3, not here.

The same limitation applies more widely and is worth carrying into the next
run: any fix whose effect is upstream of the DAL is invisible to Replay.
