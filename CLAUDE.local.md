# Local playbook supplement (this machine only)

The installed playbook (CLAUDE.md, .claude/skills/) is shared via the aap
repo and used by multiple people; /aap-update overwrites it. Rules that
depend on repos only this machine has do not belong there - they live in
this repo and this file points at them.

- Before any /pr-review run, batch PR refresh, or PR-facing writing, read
  and apply `Tools/SFD/PR_REVIEW_RULES.md`: the refresh sweep for new PRs,
  the ../ardupilot-pr-analysis archive currency check, and the rule that
  findings and fixes are vetted against the private flight-test analyses
  in ../analysis (cited publicly only as "flight tests show X").
