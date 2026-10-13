# Proposed SmallFastDrone architectural rules

Candidates for `ARCHITECTURE_RULES.md`, waiting for the user to confirm,
edit or reject them. Nothing here is a standard, and no review holds a
change against it. A confirmed rule moves to `ARCHITECTURE_RULES.md` with
the date; a rejected one moves to "Rejected" below with the reason, so it is
not proposed again. The format is the one in `aap`'s `RULES_PROPOSED.md`.

## Proposed

### SFD-1: "EKF failsafe cleared" does not mean there is a position

Rule:   Code that acts when Copter's EKF failsafe clears, and then needs a
        position or a position mode, tests `position_ok()` itself.
Why:    SFD carries #32514: `ekf_check()` expects a position only where
        the mode or the source set needs one. In AltHold or VALT on a
        source set with no horizontal source the failsafe therefore clears
        1 s after it trips, with no position. #34696's restore keyed on
        the clear and was refused in 6 of 13 failsafes on TD25
        (2026-10-13); fixed in `26f507edf4`.
Scope:  ArduCopter on SmallFastDrone-4.7.2-beta and later. Moves to `aap`
        if #32514 merges upstream.
Check:  Every reader of `failsafe.ekf`, `failsafe_ekf_off_event()` and
        `ekf_check_state.fail_count` that then asks for a position mode
        or a position.
Holder: the user (pending)
Status: proposed 2026-10-13

## Rejected

None yet.
