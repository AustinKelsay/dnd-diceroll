# Current state of `staging`

Updated 2026-11-03 for [factory follow-through #122](https://github.com/AustinKelsay/dnd-diceroll/issues/122), after [menu release #121](https://github.com/AustinKelsay/dnd-diceroll/pull/121) and the maintainer's rules rollout decision. This is a snapshot: read current issue/PR discussions and fetch before resuming. Update this note in the PR that changes what a new agent needs to know; historical run records under `docs/runs/` describe their own moment.

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
- Full compact roster (PR #106, parent #37 and slices #39–#45), promoted to `main` through PRs #107/#108: any four independent heroes from 70 subclasses across 13 classes, at most two authored specials each, schema 4/catalog revision 7. These are authored compact kits rather than full tabletop class/subclass rules. [Acceptance](full-roster/15-final-acceptance.md), [integration verification](full-roster/16-staging-integration.md).
- Menu and party-selection polish (PR #110, issue #109): visible class/subclass selection, hero previews, keyboard focus, contextual Help, and clearer route/reward/camp/ending screens. [Native acceptance](menu-polish/02-native-acceptance.md), [review](menu-polish/03-review.md). PR #121 promoted this runtime source to `main` (`3599b99`); its tree matches the pre-cleanup `staging` baseline (`df25f32`).

## Active work and ownership

- [Compact gameplay depth #112](https://github.com/AustinKelsay/dnd-diceroll/issues/112) is owned by the separate `feature/gameplay-depth` session. It owns targeting/forecasts (#50) and action clarity (#58), followed by measured balance, encounter identity, progression, audiovisual consistency and integrated acceptance. Branch work is not staging delivery; read its issue and current branch evidence before resuming. Do not take over its checkout or duplicate #50/#58 from the rules branch.
- [Rules roadmap #95](https://github.com/AustinKelsay/dnd-diceroll/issues/95) is owned by Landon (`CouchCoopGaming`). The [2026-11-03 maintainer decision](https://github.com/AustinKelsay/dnd-diceroll/issues/95#issuecomment-6609461075) supersedes the epic's older SRD 5.1/spec-only framing. The approved direction is an opt-in **SRD 5.2.1** ruleset alongside **Compact**, which remains the default. This is approved future work, not integrated behavior.
  - Nine themed deliveries are planned in order: foundation libraries; remaining libraries; ruleset/save schema; battle rules; class features/advancement/enemies; spells; rest/exploration; presentation; hero creator. Only the bottom PR is open for review against `staging`; later branches open after the previous PR merges and are updated to the latest `staging`.
  - Only PR 1 is currently `ready-for-agent`: #53–#56 and #68–#70, assigned to Landon. It adds rules libraries and tests with no game callers or gameplay changes. PR 2 also remains libraries only, but its tickets still await maintainer triage. Approval of the roadmap does not make every child ready.
  - PR 3 introduces schema 5, which persists the expedition-level ruleset choice made when a run starts. Schema 4 saves must load as Compact; unknown future saves must remain discoverable and preserved. A seeded full-run regression must prove unchanged Compact behavior against the agreed staging baseline, with existing test expectations retained. Coordinate with #112 as its Compact changes integrate.
  - SRD mode follows 5.2.1, including death saves by default. The two-round Downed house rule (#114) is a separate SRD option, off by default. Landon's duplicate #50/#58 implementations are excluded; use the versions delivered by #112.
  - Every rules PR needs independent Standards and Spec review plus a clean supported-Mac build, full `Project.` automation with zero warnings, and helper tests before merge. Windows evidence supplements this gate. The maintainer retains triage; see the decision for exact scope and split limits.
- [Windows host validation #51](https://github.com/AustinKelsay/dnd-diceroll/issues/51) and [Windows save hardening #83](https://github.com/AustinKelsay/dnd-diceroll/issues/83) remain assigned to Landon. Windows is not yet a validated host; the integrated PR #49 correction does not close these follow-ups.

## Stability, parked work and limits

- [Issue #32](https://github.com/AustinKelsay/dnd-diceroll/issues/32): recurring scene-culling assertion; [issue #35](https://github.com/AustinKelsay/dnd-diceroll/issues/35): intermittent packaged startup crash in the LLM tracker. Both remain accepted risks, owned by the separate `feature/32-renderer-stability` session. Preserve its checkout and processes; no fix is integrated. Green headless automation does not establish rendered stability.
- [Issue #105](https://github.com/AustinKelsay/dnd-diceroll/issues/105): inherited theme materials emit Nanite usage/default-material warnings in rendered Mac packages; null-rendered automation does not expose this path.
- [Hex discussion #111](https://github.com/AustinKelsay/dnd-diceroll/issues/111#issuecomment-6609473858): squares stay. Do not build further work on `feature/111-hex-grid`; revisit only after the opt-in rules work and #112 land, through a separate proposal/ADR.
- [Destructible terrain #99](https://github.com/AustinKelsay/dnd-diceroll/issues/99) and adjacent ideas #100–#104 are parked. The draft lives on `docs/99-destructible-terrain-spec`, not staging; nine owner decisions remain open. Do not treat its proposed save version or implementation details as accepted scope.
- New roster classes still need themed meshes through the same `Art/` record and import recipe as #38; current primitive fallback rigs are deliberate. Native pointer paths, physical Shift-Tab, sound-by-ear and human balance remain unverified limitations of the latest menu delivery; see its acceptance record for exact coverage.

## Repository rules now enforced on GitHub

`staging` and `main` require a PR, the "Repository hygiene and helper tests" check, and one approving review; force pushes and branch deletion are blocked. The repository owner retains admin bypass for release promotion.

## Full-roster integration (#37 / #45)

PR #106 integrated the complete reviewed roster at staging `e9e52f18b3e9e69f8030792f4f96ab3bcde1885e` under the owner's explicit shipping instruction. That tree is identical to the reviewed feature head. Fresh integration build passes without compiler warnings, Project automation83/83 with zero warnings, helpers50/50 and staging CI pass. The validated package and inspected native keyboard/Resume evidence remain applicable because runtime source and assets are unchanged. [Integration record](full-roster/16-staging-integration.md) supersedes earlier pending-integration handoffs. Source contract, save compatibility and known limitations remain unchanged. Parent #37 and slices #39–#45 are delivered through #106; live issue state and staging→main promotion are recorded on GitHub.
