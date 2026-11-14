# CLAUDE_local.md - SmallFastDrone rules

Rules for this fork that the shared playbook (`CLAUDE.md`, installed by
aap) does not carry. Read this before any task here, and the files it
names before the work they cover. In a worktree of a branch that lacks
this file or `Tools/SFD/`, read them from the primary checkout.

## The analysis repos

Three repos sit beside the primary checkout. `Tools/SFD/ANALYSIS_REPOS.md`
has the full rules; the short form:

- `../ardupilot-pr-analysis` is public: the per-PR record, one directory
  per PR number and nothing else. No customer, tester or airframe names,
  no locations, no real-flight logs, nothing from the private repos.
- `../analysis` and `../analysis-private` are private. Their existence
  may be mentioned; their contents never leave them: no file or directory
  names, paths, quotes or descriptions in this repo, commit messages, PR
  text or the public archive. Write "the private analysis repo".
- Customer-, tester- and vehicle-specific analysis goes in the private
  repo. Per-PR conclusions go in the public archive under the PR number.

## Read before

| Work | Read first |
|---|---|
| any SFD refresh or topup | `Tools/SFD/ANALYSIS_REPOS.md`, `Tools/SFD/README.md`, `Tools/SFD/REFRESH_NOTES.md` ("Current state", "Every refresh") |
| a flight log, a Replay, a tester's report | `Tools/SFD/ANALYSIS_REPOS.md`, `Tools/SFD/FLIGHT_VALIDATION.md`, `Tools/SFD/FIELD_PLAN.md` |
| /pr-review or a batch PR pass | `Tools/SFD/PR_REVIEW_RULES.md`, `Tools/SFD/ANALYSIS_REPOS.md` |
| a change on the SFD branches | the entries of `Tools/SFD/ARCHITECTURE_RULES.md` it touches |

## Standing rules

- Notes about this fork go in `Tools/SFD/`, not in agent memory, so they
  are on every machine that has the repo.
- The VALT and Throw PRs are active in SFD and are updated with the rest
  of the stack; they are just not promoted upstream.
