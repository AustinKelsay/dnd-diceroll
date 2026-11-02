# Current state of `staging`

Updated 2026-11-01 after [full-roster integration #106](https://github.com/AustinKelsay/dnd-diceroll/pull/106). Update this note in the PR that changes what a new agent needs to know; run records under `docs/runs/` stay as written and describe their own moment.

## Integrated

- Expedition V1 ([ledger](expedition-v1/ledger.md)), promoted to `main` in PR #25.
- Presentation polish (PR #27, [record](expedition-polish/01-presentation-polish.md)). The checkpoint's statement that staging integration was blocked is superseded; the blocker became accepted-risk issue #32.
- Larger encounter grids (PR #30, issue #29, [record](expedition-polish/04-larger-grids.md)).
- Five encounters and survivor levels 1–5 (PR #31, issue #28, [ledger](level-five/ledger.md)).
- dnd-diceroll naming across game and workflow (PR #34, issue #33).
- `main` promoted from `staging` in PR #36.
- Dark-fantasy theme (PR #97, issue #38): our own Blender worlds, cast and candlelit tabletop under `/Game/DnDDiceRoll/Theme`, with sources and records under `Art/`. Provenance and the import recipe are in [assets](../assets.md); `-ExpeditionPrimitives` restores the primitive look.
- Contributor onboarding and supported-host wording (PRs #47/#96), plus the Windows MSVC/save-commit correction (PR #49). Windows remains an unvalidated host; Mac behavior was reverified on the pinned engine.
- `main` promoted from `staging` in PR #98, including the theme and platform/onboarding changes above.
- Full 2014 Player’s Handbook + Tasha’s compact roster (PR #106, parent #37 and slices #39–#45): any four independent heroes from70 subclasses/13classes, at most two authored specials each, schema4/catalog revision7. [Acceptance](full-roster/15-final-acceptance.md), [integration verification](full-roster/16-staging-integration.md).

## Open

- Issue #32: one-time rendering assertion in scene culling, accepted risk.
- Issue #35: intermittent packaged startup crash during macOS launch.
- Issue #105: inherited theme materials emit Nanite usage/default-material warnings in rendered Mac packages; null-rendered automation does not expose this path.
- New roster classes still need themed meshes through the same `Art/` record and import recipe as #38; current primitive fallback rigs are deliberate. Pointer automation, sound and human balance remain unverified limitations.

## Repository rules now enforced on GitHub

`staging` and `main` require a PR, the "Repository hygiene and helper tests" check, and one approving review; force pushes and branch deletion are blocked. The repository owner retains admin bypass for release promotion.

## Full-roster integration (#37 / #45)

PR #106 integrated the complete reviewed roster at staging `e9e52f18b3e9e69f8030792f4f96ab3bcde1885e` under the owner's explicit shipping instruction. That tree is identical to the reviewed feature head. Fresh integration build passes without compiler warnings, Project automation83/83 with zero warnings, helpers50/50 and staging CI pass. The validated package and inspected native keyboard/Resume evidence remain applicable because runtime source and assets are unchanged. [Integration record](full-roster/16-staging-integration.md) supersedes earlier pending-integration handoffs. Source contract, save compatibility and known limitations remain unchanged. Parent #37 and slices #39–#45 are delivered through #106; live issue state and staging→main promotion are recorded on GitHub.
