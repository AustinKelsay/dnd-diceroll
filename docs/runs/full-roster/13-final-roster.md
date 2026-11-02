# Final roster source unit (#45)

Fixed clean source baseline `91cf0d638869cc4578d73af6b8a6ec861277cdf7`, existing owned `feature/37-full-roster` checkout. This record covers source/test/UI work only; root retains independent Standards/Spec review, current-theme package, native render/input/Resume inspection, integration and PR delivery. Neither #45 nor parent #37 is complete or integrated by this unit.

## Membership and provenance

The independent literal lists in `FFinalCommandTest` encode the approved [parent #37](https://github.com/AustinKelsay/dnd-diceroll/issues/37): 40 subclasses from the 2014 PHB and 30 from Tasha's, exactly 70 unique IDs across 13 classes. Both expected lists must exist and the published catalog must contain exactly 70 records; every build constructs and every relevant public special command must equal the authored ability set, with at most two abilities. No base placeholders, 2024 additions, other-book exclusives, internal subclass choices or duplicate reprints count as entries.

| Class | PHB | Tasha | Total |
| --- | ---: | ---: | ---: |
| Artificer | 0 | 4 | 4 |
| Barbarian | 2 | 2 | 4 |
| Bard | 2 | 2 | 4 |
| Cleric | 7 | 3 | 10 |
| Druid | 2 | 3 | 5 |
| Fighter | 3 | 2 | 5 |
| Monk | 3 | 2 | 5 |
| Paladin | 3 | 2 | 5 |
| Ranger | 2 | 2 | 4 |
| Rogue | 3 | 2 | 5 |
| Sorcerer | 2 | 2 | 4 |
| Warlock | 3 | 2 | 5 |
| Wizard | 8 | 2 | 10 |

Exact names/IDs are independently listed by book in the test and in the approved parent. Provenance is the parent's verified [2014 PHB contents](https://www.dndbeyond.com/sources/dnd/phb-2014) and [Tasha contents](https://www.dndbeyond.com/sources/dnd/tcoe), including its official class-guide cross-checks. This unit audits that approved membership; it makes no new paid-rules reproduction claim. All kits remain original compact adaptations described in the batch contracts.

## Evocation reconciliation

Revision **7** supersedes revision6's three-command Evocation interpretation. Schema4 remains unpublished; discovery stays `current-v1.json`, with strict incompatible-file preservation and no migration.

- F: actual authored `MagicMissile`, Action, enemy within six cells and sight, 3d4+3 guaranteed damage, one Slot plus one shared Kit. Existing public dice, range, critical/gear exclusion and RNG resolution remain unchanged. Facts require both pools. The existing resolver supplies actual dice forecasts; no fixed Missile amount is substituted.
- M: `Special2`, Sculpted Burst, Bonus, enemy within four cells and sight, three safe-area damage to primary and adjacent visible conscious enemies. Allies remain excluded. Costs one shared Kit.
- `Special1` is retired for catalog Evocation. `HasSpecial` equals only the authored set; no hidden Missile compatibility grant remains. Uncatalogued standalone legacy fixtures retain their existing seams. Other69 kits are unchanged.

Changed literal expectations are intentional: familiar Sculpted Burst now uses Special2 and removes three rather than five HP; worked first Evocation slot now resolves Missile (seed42/four initiative draws yields12), second slot now deals three to both primary and secondary. Sibling scenarios select the authored first command. Evocation's wall/range test retains its identity and now checks Missile's authored six-cell boundary. Old named Missile public tests and outcomes remain.

The new real run spends two Missiles on separate public turns to Kit0/Slot0. Failed writes preserve exact file bytes and complete state, effects/deaths/resources/RNG; retry equals uninterrupted resolution and demonstrably consumes RNG. Camp restores exactly1/1; next route and actual fresh-session file Resume retain1/1 and full state/RNG. Mixed Missile/Burst depletion and ordinary/accelerated/skipped playback remain covered by the revised literal scenarios.

## Controls and readability

Preserve Canvas, upstream palette/assets/scene/camera and all existing HandleButton routes. Four builder cards show independent hero names, human class names and wrapped subclass labels. Descriptions wrap at a readable fixed scale rather than shrinking whole paragraphs. Forecast panels cap width and wrap complete facts; the recent two log messages wrap in a wider panel. Party cards allocate additional height and wrap combined resources and protection. Protection resolves actual names from authoritative encounter context and appends stable instance identity, disambiguating duplicate builds/names. Basic attack and up to two authored specials remain alongside Rescue, Potion and End Turn. Help/records gating remains unchanged.

Reference remains1280-wide Canvas scaling, targeting1280×720 and1024×576. These are source layout policies, not inspected renders. Root must verify long names, panel overlap, tactical facts/costs/expiry, actual keyboard/pointer routes and narrower text in the combined theme. Theme meshes cover four original roles; new classes retain primitive fallback rigs. No binary assets were added.

## Verification and remaining root acceptance

Pinned UE5.8.4 Development editor build succeeded without compiler warnings. Full Project automation **82/82, zero warnings**, report `.local/automation/20261102T010919452129Z/index.json`. Helpers **50/50**. Exact commands, intermediate failures and fingerprint are in the ledger.

Existing per-subclass literal outcomes are retained except the explicit Evocation revisions above. Expanded MartialRunProgression includes mixed, duplicate ranged, melee and support-poor compositions, levels1–5, four recorded rewards, camp and actual ending record. Existing shared permanent-death/countdown/history/reward validation, malformed/incompatible byte preservation, failed death/effect checkpoints, const forecasts and playback-mode tests remain passing; no duplicate shared-policy framework/tests were added.

Root still owns independent review, package and native current-theme keyboard/builder/combat/fresh-process Resume and pointer attempt. Accepted engine risks #32/#35, sound and human balance remain limitations. Other-owner editor PID50002, its checkout/saves and shared endpoint were untouched. No GUI, app/editor/MCP launch, package, asset, engine, endpoint, branch/worktree, fetch/merge, push/PR, issue mutation or external message occurred.
