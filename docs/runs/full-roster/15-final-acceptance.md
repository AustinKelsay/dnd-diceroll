# Full roster acceptance report

This is the selected reviewer report for issues #37 and #39–#45. Bulk machine reports, logs, crash reports and original captures remain ignored under `.local/`, `Saved/` and `Builds/`; their exact paths are in the [ledger](ledger.md). This report supplies the reproducible scenarios and observed outcomes without requiring access to the author's local filesystem. Integration and maintainer approval remain separate.

This report describes pre-integration acceptance. [The later integration record](16-staging-integration.md) supersedes its pending-approval/integration statements without changing the execution results or limitations below.

## Verified source and execution

Final runtime source `b356fb12b912c2b95d4bb6f2cd9351dd08befdf3`, merged with staging `9d133cd7ef0ec727a292aa9c30daf7ddd3aaa913`. Sorted Source paths and bytes SHA256: `45afb663bff0be446b541d660504d71d67f758fd03358bbd748eccf5d5260ebd`. Apple Silicon macOS26.5.2, Python3.15, Xcode26.6, pinned UE5.8.4/CL58902052. Later delivery changes are documentation only.

| Check | Exact command | Observed result |
| --- | --- | --- |
| Environment | `python3 tools/dev.py doctor` | Supported host and pinned engine pass |
| Development editor build | `python3 tools/dev.py build` | Pass; no compiler warnings |
| Full automation | `python3 tools/dev.py test --filter Project.` | **83 executed,83 passed,0 warnings** |
| Offline helpers | `python3 -m unittest discover -s tools/tests -v` | **50 passed** |
| Mac Development package | `python3 tools/dev.py package` | Build/cook/stage/archive/headless startup pass; no startup errors/warnings |
| Independent review | Full baseline-to-head Standards and Spec, then fresh correction rechecks | **0 remaining actionable findings on each axis** |

Commands used `/opt/homebrew/bin/python3` on the supported host. Final staged contribution/whitespace outcomes are recorded in the ledger. Final automation report: `.local/automation/20261102T013127071711Z/index.json`. Final package receipt: `Builds/20261102T013212213930Z/verified.json`. Headless startup does not establish native rendering; the native observations below are separate.

## Selected public regressions

- `FinalCommands`: independent literal40PHB+30Tasha lists match exactly70 catalog records/13classes. Each constructible build has at most two authored specials; every relevant authoritative command equals that authored set. Evocation's former third API command fails this test before correction; final Evocation authors Magic Missile and Bonus Sculpted Burst only.
- Per-batch worked outcomes cover every subclass at combat Start/Forecast/Submit/Snapshot seams. Shared damage/healing/protection/exchange/push/area/surge policies have literal outcome, cost, target/range/sight, rejection/RNG, ownership/lifecycle and playback-mode coverage. Preview repetition leaves RNG unchanged.
- `MartialRunProgression`: seed42, generated routes, mixed Rune Knight/Soulknife/Devotion/Hunter; duplicate Hunters; melee Champion/Open Hand/Vengeance/Beast; support-poor Assassin/Soulknife/Hunter/Swarmkeeper. Each passes all five fights, survivor entry levels1–5, four rewards, one camp, build identity and ending-record checks with JSON/Restore state/RNG equality.
- Actual two-Missile spending reaches Kit0/Slot0 on separate public turns. Camp restores exactly1/1, next route and fresh-file Resume preserve1/1. Failed writes retain full live state/RNG and exact checkpoint bytes; retry matches uninterrupted outcomes. Shared permanent-death, death-level/history, reward caps, malformed/incompatible byte preservation and death/effect rollback tests remain passing.
- `CappedCampAfterBonusVictory`: real generated Artillerist combat, actual Protector Cannon immediately before the winning basic attack, actual victory checkpoint and fresh session. **Before:** reward commits but camp commit rejects Kit1→2 because historical spent Bonus implied a below-cap pool. **After:** camp commits Kit2, retains the historical spent budget, and fresh file Resume equals complete state/RNG. Fighting-phase spent-pool inference and ability ownership remain enforced.

## Native procedure and observations

Root used separate ad-hoc signed clones of verified archives, unique bundles `com.YourCompany.DnDDiceRoll.Roster45FinalQA` and `com.YourCompany.DnDDiceRoll.Roster45DeliveryQA`, private container checkpoints, and cua-driver strict window input. Original editor/checkouts/saves and archives were untouched. Each action was bracketed by fresh snapshots; successful commands required inspected rendered changes and checkpoint readback, not driver receipts alone.

| Scenario | Observed outcome |
| --- | --- |
| Human builder labels | Slot4 cycled Evocation to Order of Scribes. Class/subclass names, both ability descriptions and costs fit at1280×720 and1024×576 client sizes. All70 choices are covered through public builder tests; native inspection is representative. |
| Current-theme large grid | Keyboard preset/NewRun/Stream trail produced seed20261030,18×11 with Champion/Thief/Life/Evocation. Tabletop, party cards, grid and six-action bar rendered. |
| Named protection | Life Sanctuary named Ivo in its preview. Commit spent Bonus/one Kit and recorded owner3→target4, reduction3 until next own turn. The party card shows Ivo plus stable identity. |
| Movement and sight | Ivo moved(4,6)→(7,6), leaving Move1. Sculpted Burst correctly showed the covered Scout as illegal; visible Vault bow was legal. |
| Two actual Wizard specials | Burst previewed and committed3 damage, HP10→7, Bonus/Kit spent. Magic Missile previewed6–15/3d4+3; commit rolled1+2+3+3=9, killed the bow, spent Action/Kit/Slot, and left Kit0/Slot1/Move1 with both action budgets spent. |
| Final fresh-process Resume | The genuine played checkpoint was copied byte-for-byte into the final package's fresh private QA root. O/Resume preserved exact disk bytes, builds, cells, HP, defeated enemy, named protection, Kit/Slot pools and budgets. No save fields were edited to manufacture this scenario. |
| Final layout | The earlier log-border/budget overlap was corrected by32 design pixels. Final-package Resume at1280×720 and1024×576 shows separate log/budget rows and fitted cards, protection/resources, top controls and six actions. Captures were inspected at both sizes. |
| Pointer attempt | Grounded Help clicks through background and supported foreground fallback produced no inspected effect. No pointer pass is claimed. |

Gameplay command observations used source32c1b7b; the only subsequent runtime change is final HUD spacing. Final packageb356fb1 established fresh-process Resume and corrected layout. Selected captures and exact paths are in the ledger; bulk captures remain ignored. Both owned apps quit normally and driver sessions ended. No native all70-ability, sound, performance or human-balance pass is inferred.

## Remaining limitations and integration

- Engine risks #32 and #35 remain unresolved and accepted as limitations. Neither recurred in the bounded final45 native pass; earlier slice recurrences remain recorded honestly.
- Final native rendering emitted15 inherited theme material/Nanite usage warnings and15 explanatory lines, plus the existing AudioUnit sample-rate warning; no Error/Fatal lines occurred. These assets are unchanged from staging. Warning-free headless package/automation does not establish warning-free rendering. [Theme follow-up #105](https://github.com/AustinKelsay/dnd-diceroll/issues/105) records reproduction and asset-ownership constraints; no material/engine fix is claimed here.
- Nine newly supported classes use existing primitive fallback rigs; detailed character art, sound and human balance remain follow-ups. Kits are compact adaptations rather than complete tabletop implementations.
- Source, independent review, package and bounded native acceptance are complete locally. Maintainer approval/staging integration and release remain separate. Keep #37 and implementation tickets open until integration.
