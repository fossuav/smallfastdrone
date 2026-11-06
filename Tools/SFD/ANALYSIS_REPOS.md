# The analysis repos

Read this before any SFD refresh or topup, and before any analysis work
(a flight log, a Replay, a tester's report). Three repos sit beside the
primary checkout, in the same parent directory; resolve them from there,
not from a scratch worktree.

| Repo | Visibility | What it is |
|---|---|---|
| `../ardupilot-pr-analysis` | public | the per-PR record: one directory per PR number |
| `../analysis` | private | the flight-test record |
| `../analysis-private` | private | a second private record, same rules as `../analysis` |

## ../ardupilot-pr-analysis (public)

One directory per PR, named by number, `README.md` first: what was
measured for that PR, the A/Bs and Replays, the decisions and the review
rounds. `REPLAY_LOGS.md` says which logs each EKF PR is replayed against,
by short id. Its own `CLAUDE.md` governs editing it.

- PR-specific analysis goes in that PR's directory. Nothing else gets a
  directory: no vehicle, customer, tester or dated directories.
- It is public. No customer or tester names, no vehicle owners, no
  locations, no real-flight logs, no private log paths, and nothing from
  the private repos.
- It may be cited by path and quoted anywhere: Tools/SFD, commit messages,
  PR bodies and comments.

## ../analysis and ../analysis-private (private)

- Their existence may be acknowledged. Their contents may not be revealed
  anywhere outside them: no file or directory names, no paths, no quotes,
  no description of how they are organised. That holds in Tools/SFD,
  commit messages, PR bodies and comments, the public archive and code
  comments. Write "the private analysis repo" and state the conclusion
  with the numbers it needs.
- Customer-, tester- and vehicle-specific analysis (a tester's flights, a
  customer's airframe, a named vehicle) is written there, never in the
  public archive.
- Tools/SFD keeps its own record of the beta's flights
  (`FLIGHT_VALIDATION.md`). Write findings there in their own words, never
  as a pointer into the private repos.
- Each has its own conventions for editing it; follow them.

## Before proceeding

- Refresh or topup: after `refresh.sh changed`, read the public archive
  entry for every PR that moved, and the private record for the flights
  behind it, before re-stacking. A refresh that drops or re-resolves a
  measured fix undoes a flight result silently. `PR_REVIEW_RULES.md` has
  the per-PR checks.
- Analysis: before forming a diagnosis, search the private record for the
  vehicle and its earlier flights, and the public archive for the PRs on
  the code path. A vehicle's history often already explains the symptom,
  or rules out the obvious mechanism.
- Afterwards: vehicle- and tester-specific findings go in the private
  repo, per-PR conclusions in the public archive under the PR number, and
  the beta's flight outcome in `FLIGHT_VALIDATION.md`.

Paths into `../analysis` that some public-archive READMEs carry predate
this note (2026-11-06); they are not a precedent.
