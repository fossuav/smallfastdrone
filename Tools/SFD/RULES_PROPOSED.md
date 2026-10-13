# Proposed SmallFastDrone architectural rules

Candidates for `ARCHITECTURE_RULES.md`, waiting for the user to confirm,
edit or reject them. Only a rule that is true on the SFD branches and not
upstream belongs here; a general principle goes to `aap`. Nothing here is
a standard, and no review holds a change against it. A confirmed rule
moves to `ARCHITECTURE_RULES.md` with the date; a rejected one moves to
"Rejected" below with the reason, so it is not proposed again. The format
is the one in `aap`'s `RULES_PROPOSED.md`.

## Proposed

None.

## Rejected

- **SFD-1, "EKF failsafe cleared" does not mean there is a position**
  (2026-10-13): too specific, and not SFD-only once stated as a principle.
  Rewritten as `aap`'s ARCH-2, "Test for what you need, not for something
  that usually comes with it".
