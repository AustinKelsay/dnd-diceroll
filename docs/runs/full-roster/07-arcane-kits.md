# Arcane compact adaptations (#42)

Fixed baseline `4eddb88cf9b69ae5b04416d6c8215174dd112f58`; sole writer on the existing feature branch/worktree. No binary assets, GUI/MCP, package, remote update or integration operations. Root owns independent review and package/native acceptance.

The catalog publishes 48 kits across ten classes: 47 approved subclasses and interim Cleric. All 19 approved arcane subclasses are listed below. This does not complete the 70-subclass parent. Evocation replaces `wizard.base` in the familiar preset; existing Magic Missile public scenarios now name that real authored build without changing their scenario intent or expected dice bounds.

These are original compact adaptations. Every listed effect is usable from encounter one. Each kit owns two shared Kit uses, each special costs one; camp recovers one capped at two. Neither turns, encounters, levels nor Resume refill them. Each earned level adds one basic damage only. Existing martial/Barbarian resource and recovery policies remain intact. Every target must be conscious, within Manhattan range and in sight. Ally means another party member; self means only the owner. Healing requires missing HP and cannot rescue/revive.

Evocation retains the prototype Magic Missile command as a public compatibility adapter (three d4+1 darts, range six, Action). It consumes one Slot and one shared Kit use. Its legacy two Slots recover one at camp, capped at two; this adapter cannot grant extra casts beyond the kit budget. Other Wizards own no Slots or Missile adapter. Only the two authored F/M abilities are displayed. Slot balance cannot be lower than remaining Kit balance on Evocation. These facts live in the catalog, not UI decisions.

| Build | F / first special | M / second special |
| --- | --- | --- |
| Sorcerer / Draconic Bloodline | Dragon Breath: Action, enemy range 3, safe area damage 5 | Dragon Scales: Bonus, self range 0, protection 3 |
| Sorcerer / Wild Magic | Chaos Bolt: Action, enemy range 4, seeded damage 4 or 8 | Chaotic Exchange: Bonus, ally range 3, exchange cells |
| Sorcerer / Aberrant Mind | Mind Thrust: Action, enemy range 4, damage 5, push one cell | Telepathic Shelter: Bonus, ally range 4, protection 2 |
| Sorcerer / Clockwork Soul | Bastion: Bonus, ally range 4, protection 4 | Restore Balance: Action, ally range 3, heal 6 |
| Warlock / Archfey | Fey Repulse: Bonus, enemy range 3, damage 4, push one cell | Misty Exchange: Action, ally range 4, exchange cells |
| Warlock / Fiend | Dark Blast: Action, enemy range 4, damage 8 | Dark Renewal: Bonus, self range 0, heal 5 |
| Warlock / Great Old One | Thought Shield: Action, ally range 3, protection 3 | Repelling Thought: Bonus, enemy range 4, damage 4, push one cell |
| Warlock / Fathomless | Tentacle Lash: Action, enemy range 3, damage 5, push one cell | Ocean Shelter: Bonus, self range 0, protection 3 |
| Warlock / Genie | Vessel Exchange: Bonus, ally range 4, exchange cells | Vessel Shelter: Action, self range 0, protection 4 |
| Wizard / Abjuration | Projected Ward: Bonus, ally range 4, protection 4 | Warding Burst: Action, enemy range 2, safe area damage 3 |
| Wizard / Conjuration | Benign Exchange: Bonus, ally range 5, exchange cells | Conjured Ram: Action, enemy range 3, damage 5, push one cell |
| Wizard / Divination | Foreseen Opening: Action, enemy range 5, damage 6 | Foreseen Shelter: Bonus, ally range 5, protection 2 |
| Wizard / Enchantment | Compelling Repulse: Bonus, enemy range 2, damage 3, push one cell | Beguiling Exchange: Action, ally range 3, exchange cells |
| Wizard / Evocation | Sculpted Burst: Action, enemy range 4, safe area damage 5 | Focused Spark: Bonus, enemy range 5, damage 3 |
| Wizard / Illusion | Mirror Ward: Bonus, self range 0, protection 4 | Illusory Exchange: Action, ally range 3, exchange cells |
| Wizard / Necromancy | Grave Bolt: Action, enemy range 4, damage 7 | Life Transfer: Bonus, ally range 2, heal 5 |
| Wizard / Transmutation | Transposing Exchange: Action, ally range 3, exchange cells | Restoring Stone: Bonus, ally range 1, heal 7 |
| Wizard / Bladesinging | Singing Blade: Bonus, enemy range 1, damage 7 | Bladesong Ward: Action, self range 0, protection 3 |
| Wizard / Order Of Scribes | Written Burst: Bonus, enemy range 5, safe area damage 3 | Manifest Exchange: Action, ally range 2, exchange cells |

Protection is owned by its caster, replaces that caster's prior ward, and expires at the start of that owner's next turn. Independent owners coexist; strongest reduction applies. Owner or target down/death clears the relevant ward; terminal combat clears all transient effects. Temporary effects never alter base profiles. Existing shared primitive tests cover replacement, strongest reduction, expiry and loss.

A single ordered-target query serves both forecasts and resolution. Deterministic area forecasts expose ordered per-target loss/mitigation facts, checked against committed facts. Areas select the primary then other conscious enemies within Manhattan radius one of it in encounter order, with sight from the primary; allies are excluded. No wider geometry is introduced. A radius-one secondary cannot have an intervening wall on this grid; targeting sight and ordinary range boundaries still apply. Push follows damage, uses the existing open-cell/boundary checks and cannot move a defeated enemy. Exchanges swap occupied ally cells using the owner's budget, retaining movement. Tentacle and conjured effects create no independent actor or initiative.

Chaos Bolt draws exactly once from saved combat RNG on commit. Previews do not draw. LastSurge ownership/bounds now derive from catalog Surge facts and a spent compatible resource pool, preserving Barbarian behavior. Nonconscious owners and terminal/route transitions clear the marker. No per-turn surge refill or external random source exists.

Schema remains unpublished version 4, catalog revision advances to 4; checkpoint discovery remains `current-v1.json`. Prior catalog checkpoints are incompatible and retain the existing explicit New Run preservation flow. Strict recognized-build ValidResources checks remain in direct combat validation; bounded uncatalogued public fixtures remain supported.

Omissions: full spellbooks, damage types, saving throws, charm/fear/stun subsystems, additional summoned actors, independent familiar turns, resurrection, unrestricted effect language and 2024 options. Names describe the concrete compact effects above; they do not promise omitted tabletop mechanics. Balance remains for human playtesting.

Verification and intermediate failures are recorded in the ledger. Headless automation does not establish rendered readability, pointer operation, sound or native acceptance. Engine risks #32/#35 remain unresolved.

Final author checks: pinned build passed without compiler warnings; full Project automation **67/67, zero warnings**, helpers **50/50**. Final report `.local/automation/20261101T235209116078Z/index.json`; logs and intermediate failures are in the ledger. Root accepted the slice locally after independent Standards/Spec reviews (no actionable source findings), successful Mac Development package/startup and inspected mixed-party keyboard/combat/fresh-process Resume. The ledger records exact observations and remaining limitations; no integration or full70 delivery is claimed.
