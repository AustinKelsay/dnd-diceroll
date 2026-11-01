# Current state of `staging`

Updated 2026-11-01 with [issue #46](https://github.com/AustinKelsay/dnd-diceroll/issues/46). Update this note in the PR that changes what a new agent needs to know; run records under `docs/runs/` stay as written and describe their own moment.

## Integrated

- Expedition V1 ([ledger](expedition-v1/ledger.md)), promoted to `main` in PR #25.
- Presentation polish (PR #27, [record](expedition-polish/01-presentation-polish.md)). The checkpoint's statement that staging integration was blocked is superseded; the blocker became accepted-risk issue #32.
- Larger encounter grids (PR #30, issue #29, [record](expedition-polish/04-larger-grids.md)).
- Five encounters and survivor levels 1–5 (PR #31, issue #28, [ledger](level-five/ledger.md)). This is the latest delivery record.
- dnd-diceroll naming across game and workflow (PR #34, issue #33).
- `main` promoted from `staging` in PR #36.

## Open

- Issue #32: one-time rendering assertion in scene culling, accepted risk.
- Issue #35: intermittent packaged startup crash during macOS launch.
- Issues #37–#45: full roster with selectable parties, and issue #38: dark-fantasy theme. Both are claimed by the repository owner on their own feature branches; coordinate before touching party, roster, or theme assets.

## Repository rules now enforced on GitHub

`staging` and `main` require a PR, the "Repository hygiene and helper tests" check, and one approving review; force pushes and branch deletion are blocked. The repository owner retains admin bypass for release promotion.
