# SmallFastDrone PR review rules (local supplement)

Rules for /pr-review runs, batch PR refreshes and PR-facing writing that
apply on top of the shared playbook (root CLAUDE.md and
.claude/skills/pr-review/SKILL.md). They live here rather than in the
playbook because they depend on repos that sit beside this checkout on this
machine only: the public analysis archive ../ardupilot-pr-analysis and the
private flight-test analyses ../analysis and ../analysis-private. Resolve
those paths from the primary checkout, not from a scratch worktree.

## A refresh run starts with a sweep

When the run is a batch pass over the fork's open PRs - after a rebase wave,
or after a round of AI reviews and fixes - enumerate before reviewing
anything:

```bash
gh pr list --author andyp1per --repo ArduPilot/ardupilot --state open \
    --json number,headRefName,updatedAt,reviewDecision
```

Reconcile that list against the branch README's PR list and the analysis
archive. A PR opened since the last pass is exactly the one with no archive
entry and no prior review, so it joins the run rather than being discovered
after it; a PR that merged or closed drops out. Update the README's list as
part of the sweep - a "not yet submitted" line for a PR that is open
misleads the next run.

## Check the parameter tables before running anything

Stacking PRs collides parameter indices. Each PR picks the next free index
against master, so two of them landing in the same table both take it, and the
refresh is the first place they meet. This is not rare - expect it on every
refresh that integrates more than a couple of PRs touching the same library.

Nothing catches it at build time. AP_Param validates the tables at startup, so
SITL panics before its first heartbeat and EVERY test fails with "Did not
receive heartbeat" - a symptom that says nothing about parameters and sends you
diagnosing the wrong thing. Worse, `AP_Param.cpp` ships with `ENABLE_DEBUG 0`,
which reduces the panic to a bare "Bad parameter table" naming no offender.

So run the check as a pass, immediately after the code pass and before the test
run:

```sh
Tools/SFD/check_param_tables.py
```

It reports every offender at once - duplicate group idx, idx >= 64, names over
AP_Param's 16-character limit, and duplicate keys or names in the top-level
tables. Fix them all, then test.

When resolving a collision, keep the index the shipping branch already uses and
move the newcomer, so no user's saved value changes; note the local renumbering
in REFRESH_NOTES so the next refresh redoes it. Do not assume the PR heads are
right just because they are newer - on 4.7 the index space differs from master's,
and a PR that rebased onto master may have moved a parameter the branch already
shipped.

The same applies to any parameter a conflict resolution touches: re-check with
the script rather than by eye, because a clash between two tables you did not
edit is exactly the one you will miss.

## Tests taken from PR heads call master's API

The hot-file rebuild copies test bodies verbatim from PR heads, which are
written against master. Where 4.7's harness differs, the test compiles, loads,
registers and then dies at run time on the first call. py_compile does not catch
it and neither does the suite load, because the attribute is only resolved when
the body runs - so each one costs a full test run to find.

Seen in the 2026-09-04 refresh, one failing test at a time:

- `takeoff(altitude_min=..., altitude_max=...)`; 4.7 has `alt_min` and `max_err`,
  and `max_err` is a tolerance where `altitude_max` is a ceiling.
- `assert_ekfs_match_sim_state` and `statustext_count_in_collections`, helpers
  that live in master's vehicle_test_suite.py.
- `install_example_script_context("Copter_Motors_6DoF.lua")`, a master-only file.

After `rebuild-tests`, before the test run:

```sh
Tools/SFD/check_test_api.py
```

It reports all of them at once - helpers with no definition, keywords 4.7's own
signature does not accept, and install_*_script*() naming a Lua file this tree
does not ship. Each check is diffed against vanilla 4.7 so the pre-existing
upstream quirks (`change_alt_frame`, `uint8`,
`guided_move_global_relative_alt`) do not drown the real findings.

It resolves a script name held in a local variable, which a plain grep for
string literals misses - that is how Scripting6DoFMotors' missing
Copter_Motors_6DoF.lua slipped through a first pass.

## The analysis archive is part of the thread

A PR under active work keeps its investigation record in
../ardupilot-pr-analysis/<number>/. Check the entry exists and is current
before reviewing - compare its last commit against the PR's last push - and
read it before the diff: it records the validated numbers, the rejected
alternatives and the rounds already argued, which is what stops a fresh
review from re-litigating settled questions or "improving" a design choice
the archive shows was measured. A PR in the run with no entry, or an entry
that predates the latest round of changes, is itself a finding: bring the
archive up to date as part of the run.

## Hold the diff against the flight record, not just the source

The measured behaviour behind these changes lives in the private flight-test
analysis repos (../analysis, ../analysis-private). Reviews run without that
context have asserted wrong conclusions with full confidence: reasoning that
a mechanism must misbehave when a flight log already shows it behaving, or
proposing a simplification that re-introduces a failure a flight measured.
For each PR in the run, find its topics in the private repo (the archive
entry usually links them), confirm the current diff still delivers what the
flights established, and vet every finding - and every AI-suggested fix
already applied - that touches measured behaviour against the data before
accepting it. A finding contradicted by flight data is refuted; a fix that
would undo a flight-validated behaviour is a bug being introduced, however
sound the reasoning reads.

The flight-test repos are private. Public-facing text - PR bodies, comments,
commit messages, the public archive - cites the evidence only as "flight
tests show X" with the numbers, never the repo, its file names, or any
vehicle, person or location.
