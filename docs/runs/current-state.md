# Current state of `staging`

Updated 2026-11-01 with [issue #46](https://github.com/AustinKelsay/dnd-diceroll/issues/46). Update this note in the PR that changes what a new agent needs to know; run records under `docs/runs/` stay as written and describe their own moment.

## Integrated

- Expedition V1 ([ledger](expedition-v1/ledger.md)), promoted to `main` in PR #25.
- Presentation polish (PR #27, [record](expedition-polish/01-presentation-polish.md)). The checkpoint's statement that staging integration was blocked is superseded; the blocker became accepted-risk issue #32.
- Larger encounter grids (PR #30, issue #29, [record](expedition-polish/04-larger-grids.md)).
- Five encounters and survivor levels 1–5 (PR #31, issue #28, [ledger](level-five/ledger.md)). This is the latest delivery record.
- dnd-diceroll naming across game and workflow (PR #34, issue #33).
- `main` promoted from `staging` in PR #36.
- Dark-fantasy theme (PR #97, issue #38): our own Blender worlds, cast and candlelit tabletop under `/Game/DnDDiceRoll/Theme`, with sources and records under `Art/`. Provenance and the import recipe are in [assets](../assets.md); `-ExpeditionPrimitives` restores the primitive look.

## Open

- Issue #32: one-time rendering assertion in scene culling, accepted risk.
- Issue #35: intermittent packaged startup crash during macOS launch.
- Issues #37–#45: full roster with selectable parties, claimed by the repository owner on its own feature branch; coordinate before touching party or roster work. New characters need themed meshes through the same `Art/` record and import recipe as #38.
- Issue #48 and PR #49: Windows MSVC build error and a save-commit failure after the first checkpoint, from CouchCoopGaming on a Windows host with the pinned engine. Windows is not a validated host; the maintainer verifies Unreal claims on the supported Mac before integration.
- Issue #46 and PR #47: this onboarding pass.

## Repository rules now enforced on GitHub

`staging` and `main` require a PR, the "Repository hygiene and helper tests" check, and one approving review; force pushes and branch deletion are blocked. The repository owner retains admin bypass for release promotion.
