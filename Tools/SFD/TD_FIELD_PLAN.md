# TD25 Field Plan

SmallFastDrone 4.7.2 beta field test plan: everything that changed in the
beta after the 8 October flights, what to set up for it, and how to fly
it. The 8 October logs were on `7b0e4dd8`; fly `d2014d6afb` or later,
`26f507edf4` or later for T3, and the topup21 beta (14 October) or later
for T6.

- **Vehicle**: TD25, MicoAir743v2, 5"
- **Navigation**: MAVLink flow + MAVLink range finder, no GPS, baro height
- **Loop rate**: `SCHED_LOOP_RATE` 200

## Results from 14 October (outdoors, grass and concrete)

One log on `6f5d6dc0`, three flights, Loiter outdoors in sun.

- **The car bonnet.** Flying onto the bonnet in Loiter the range dropped
  from 1.4 m to 0.3 m in a tenth of a second. The EKF's height above the
  ground, which turns flow into speed, follows a range finder step over a
  second or so and was still at 1.0 m. A flow axis locked out over the
  shiny surface, and the lockout recovery reset the speed to one flow
  sample scaled by that 1.0 m: three times too much, 2.1 m/s east that
  the vehicle did not have. Loiter rolled 25 degrees to stop it, into the
  wall. **Fixed in topup21**: the recovery now waits while the range and
  that height disagree (`flow recovery deferred, range step`). Replayed on
  this log the bonnet reset no longer happens.
- **Failsafes over bright concrete** were the flow sensor dropping to
  quality 0 for 0.6-8.6 s, eight times. It is otherwise excellent outside
  (median 114 against 57-59 indoors). Each long dropout lost position
  about 3 s later and went to VALT, and Loiter came back by itself when
  quality returned: three times in flight 2, the limit.
- **Loiter refused for the rest of a flight** twice (380 s, 448 s): a
  lockout at the edge of a dropout met a sample of quality 2 or 8 and
  latched flow out until disarm, though quality was back over 100 within
  seconds. **Changed in topup21**: the latch now lets go once quality has
  stayed at 66 or more (`EK3_FLOW_QMIN` 60 plus 10%) for 2 s, with `flow
  quality recovered`. Replayed, the two latches let go 2.6 s and 11.6 s
  after they were set.
- **The jitter** is most likely Loiter taking over after a restore: it
  stops the 0.4-0.7 m/s the vehicle carries with a 5-6 degree lean for
  about a second. One restore (297.8 s) came during a dropout, so Loiter
  braked for 1.6 s and VALT took over again.

## Results from 13 October

Ten logs on `6f5d6dc0`: T1-T4 flown, T5 not.

- **T1**: VALT climbs at 1.01-1.05 times the stick's rate off centre and
  stops within 0.6-1.1 s of centring, carrying on 0.2-0.35 m from about
  0.6 m/s and 0.5-0.76 m from about 1.4 m/s. How did it feel? The logs
  cannot say whether that is too loose. `PILOT_SPD_UP` is now 1.5 (2.5
  on 8 October), so full stick climbs 40% slower than it did, whatever
  `VALT_POS_EXPO` does.
- **T2**: works. Every switch to source set 2 in Loiter gave `EKF
  Failsafe: changed to VALT Hold Mode` about 6 s later.
- **T3**: firmware bug, fixed in `26f507edf4`. On source set 2 the
  failsafe clears about 1 s after the change to VALT, and the restore
  then tried Loiter 3 s after the sticks were centred, before there was a
  position. It was refused (`Mode change to Loiter failed: requires
  position`) and did not try again, so Loiter never came back: 6 of 13
  restores. **Re-fly T3 on the new firmware.**
- **T4**: works. Loiter was accepted after every re-arm, over 7 flights
  in two boots.
- **Flow**: much better than on 8 October. It now follows rotation in
  flight, and median quality is 57-59 against 51. About half the samples
  are still below `EK3_FLOW_QMIN` 60, so a lockout usually latches flow
  out until disarm: 9 latches, 5 of them at touchdown. **Keep QMIN at
  60.** Replayed, 50 still catches the 8 October flyaway, but on T4 it
  turns two latches in Acro into five velocity resets of 1.3-4.2 m/s,
  which without GPS we cannot check.

## What changed since 8 October

| Change | What you will notice | Status | Test |
|---|---|---|---|
| VALT position blend now scales with loop rate | VALT feels different at 200 Hz: looser off centre, the same at centre and full stick | every VALT flight | T1 |
| EKF failsafe can switch to VALT (`FS_EKF_ACTION` 4) | A flow loss in Loiter drops you into VALT instead of AltHold | needs setup | T2 |
| EKF failsafe returns to Loiter when it clears (`FS_OPTIONS` bit 6) | After a failsafe, Loiter comes back by itself once flow is healthy and the sticks are centred | needs setup; re-fly on `26f507edf4` | T3 |
| Flow rest reading cleared on disarm, and after 1 s with no flow sample | Nothing, unless a second flight in the same boot misbehaves near the ground | passive | T4 |
| Throw mode can hand off to VALT (`THROW_NEXTMODE` 29) | Only if you throw or drop | optional | T5 |
| VALT: GCS take-off, surface tracking re-init, EKF height reset, position offset | Nothing with your setup | not exercised | - |
| AGL height floor hold, range height switch, flat-ground return timing | Nothing with your setup | not exercised | - |

## Before every flight

- [ ] Logging stays as on 8 October: `LOG_REPLAY` 1, `LOG_DISARMED` 2.
      We need both to replay what happened.
- [ ] Bail-out ready: Acro is on mode switch position 1 (`FLTMODE1` 1).
      Fly every test at 1.5-2 m over open floor.
- [ ] Sitting still, armed or not, the GCS ground speed reads near zero.
      On 8 October the EKF speed ran away on the ground between flights.
      If it is not near zero, reboot before taking off.
- [ ] If `flow quality NN too low to recover` appears at take-off, Loiter
      is gone for that flight (the `EK3_FLOW_QMIN` latch). Land, disarm,
      re-arm and try again.
- [ ] Write down the time or flight number of each test, and anything
      that felt wrong.

## Tests, in flying order

### T1: VALT feel at 200 Hz, and a flow check (no setup)

**Why.** The blend set by `VALT_POS_EXPO` was applied once per loop, so
at your 200 Hz it held on to the position error about twice as long as
intended. It now behaves as it does at 400 Hz. With `VALT_POS_EXPO` 3,
mid-stick climbs and descents follow the stick more freely; centre stick
and full stick are unchanged.

**Fly.**

1. Take off in VALT and hover at 1.5 m for 20 s, hands off.
2. Small, medium and full throttle steps up and down, letting go to
   centre after each.
3. Back to hover. Rock gently in roll, then pitch, for about 10 s each.
   This lets us check the flow sensor tracks rotation in flight.
4. Land.

**Look for.** Does it stop cleanly at centre stick? Does mid-stick feel
too loose compared with before?

**Option.** If it feels too loose, try `VALT_POS_EXPO` 2. That comes
close to the mid-stick feel the old code gave at 200 Hz with 3
(calculated, not flown), and now feels the same at any loop rate.

### T2: EKF failsafe switches to VALT (setup)

**Set.** `FS_EKF_ACTION` 2 -> **4**. Put a spare 3-position switch on
**EKF Source Set**, e.g. `RC7_OPTION` 90 (low = source set 1, middle =
source set 2). Your source set 2 already has no horizontal source, so
moving the switch to middle takes flow out of navigation on purpose.
Keep it low for take-off.

**Fly.**

1. Loiter at 1.5-2 m, settled.
2. Source switch to middle. Within a few seconds expect `EKF variance`
   and then `EKF Failsafe: changed to VALT Hold Mode`.
3. Fly VALT for 10-15 s: height should hold at centre stick, and it will
   drift sideways as AltHold does.
4. Source switch back to low, then land (or go straight on to T3).

**Look for.** The change of mode is clean, with no height step or jump.
If the radio is already in failsafe when the EKF fails, it lands instead
of going to VALT; no need to test that in the air.

### T3: Loiter comes back after the failsafe clears (setup)

**Set.** `FS_OPTIONS` 16 -> **80** (adds bit 6, keeps your bit 4). Keep
the T2 setup.

**Fly.**

1. As T2 until VALT. Leave the mode switch on Loiter throughout. Expect
   `EKF Failsafe Cleared` about 1 s later, with the switch still in the
   middle: source set 2 has no horizontal source, so the check stops
   expecting a position. Nothing should come back yet.
2. Source switch back to low. Expect `started relative aiding`.
3. Centre roll and pitch, hands off. About 3 s after aiding has started,
   expect `EKF Failsafe: restored Loiter Mode`.

*Corrected 13 October*: this said the failsafe clears only after the
switch back. It clears on source set 2, and before `26f507edf4` the
restore fired there and was refused; see Results.
4. Repeat once with the roll stick held off centre: nothing should come
   back until you centre it.
5. Repeat once and move the mode switch while in VALT: that cancels the
   restore, and you stay in whatever you picked.

**Limits.** At most 3 restores per flight; disarming resets the count.
Landing during the failsafe cancels it. It waits while any other
failsafe (radio, battery) is active.

> **The flow latch can stop this.** With `EK3_FLOW_QMIN` 75 and TD25's
> flow quality around 48 on 8 October, a lockout while flow comes back
> latches flow out until disarm, and Loiter cannot return. That is the
> latch doing its job, not a restore fault. If you see
> `flow quality NN too low to recover` here, note the time and land. Best
> done over the surface that gave the best flow quality.

### T4: Several flights in one boot (passive)

**Why.** With `FLOW_HGT_MIN` 0.08 the EKF learns the range finder reading
on the ground before each take-off, to tell resting from hovering below
the focus height. The reading now clears on disarm, and while disarmed
after a second with no stored flow sample, so a stale one cannot carry
into the next flight.

**Fly.** Fly T1-T3 as three or more flights without rebooting between
them, landing and taking off from the same spot.

**Look for.** `stopped aiding` just after a take-off, or Loiter refusing
after a re-arm. Either one, note the flight.

### T6: Outdoors over mixed ground (topup21)

**Why.** Flow should come back by itself after a dropout, and the
recovery should not act on a sudden step in the ground.

**Set.** `EK3_OPTIONS` 92 -> **28**, so every flow speed reset is
announced (`flow vel reset`), as recommended on 8 October.

**Fly.**

1. Loiter at 1.5-2 m over the concrete that dropped out before, and let
   it fail to VALT as it did on 14 October.
2. Expect `flow quality recovered` about 2 s after flow returns if it
   had latched (`too low to recover`), then Loiter is available again.
3. Only if you are comfortable: walking pace in Loiter over a step you
   can afford to hit, a low box rather than a car, with Acro ready.
   Expect `flow recovery deferred, range step` and no lurch.

**Look for.** Any lurch or lean in Loiter you did not command; note the
time.

### T5: Throw or drop into VALT (optional)

**Why.** 4.7.0 could hand off from throw to VALT; that was lost from the
beta and is back from `d2014d6afb`. VALT still waits for a position
before the hand-off, as in 4.7.0.

**Set.** `THROW_NEXTMODE` 18 -> **29**. Only if you throw or drop this
airframe.

**Look for.** After `Throw height achieved` the mode changes to VALT and
holds height at centre stick, not stuck in Throw.

## Parameters for this session

| Parameter | 8 Oct | For tests | Test | Afterwards |
|---|---|---|---|---|
| `FS_EKF_ACTION` | 2 | 4 | T2, T3 | Keep 4 if you prefer VALT to AltHold |
| `FS_OPTIONS` | 16 | 80 | T3 | Keep 80 if the restore behaved |
| `RC7_OPTION` (any spare 3-position switch) | 0 | 90 | T2, T3 | Back to 0, or keep for testing |
| `VALT_POS_EXPO` | 3 | 3, then 2 if loose | T1 | Whichever felt right |
| `THROW_NEXTMODE` | 18 | 29 | T5 | Only for throw tests |
| `EK3_OPTIONS` | 28 (92 since) | 28 | T6 | 28, so flow resets are announced |
| `LOG_REPLAY` / `LOG_DISARMED` | 1 / 2 | 1 / 2 | all | Keep |

## Not exercised with your setup

These changed too, but nothing on TD25 as configured reaches them, so
there is nothing to fly. Tell us before changing any of these settings.

- AGL height floor hold now needs a fused reading: your MAVLink range
  finder never reports too-low, so the hold never engages.
- Range height switch speed gate and the gate after flow comes back:
  only with `EK3_RNG_USE_HGT` above 0 (yours is -1).
- Flat-ground return timing: only when flying above the range finder's
  8 m, and the flat-ground fallback also needs `EK3_OPTIONS` bit 5, which
  is off.
- VALT surface tracking re-init: only with surface tracking on
  (`SURFTRAK_MODE` is 0).
- VALT GCS take-off and position offset handling: only from a GCS
  take-off command or a script holding a position offset.

## Send back

- [ ] Every `.bin` from the session, including the short ones.
- [ ] One line per flight: which test, roughly when, and what you felt.
- [ ] The parameter file as flown, if anything changed beyond the table
      above.

Changes since 8 October are the beta's topup14 to topup17. The VALT, EKF
failsafe and flow changes are separate commits on the beta, so these
flights are their flight evidence.
