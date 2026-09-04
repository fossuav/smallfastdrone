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
