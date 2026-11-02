# Divine, nature and support compact kits (#43)

Fixed clean baseline `ba17e0c6b9b2cd8c02f966eb9b1f3aa7782ed1de`. Approved #37/#43 delegate compact tuning and public testing seams. Sole implementation writer; no assets, GUI/MCP, package, original checkout/editor/saves or remote operations. Root owns independent Standards/Spec review, package and native acceptance.

All 19 approved subclasses below are playable from encounter one, alongside all 47 prior approved subclasses: 66 kits across 12 classes, no interim bases. Artificer's four belong to #44. Each hero retains basic attack, shared movement, Rescue, potion and End Turn, and exactly two authored specials. These are original compact adaptations, not full tabletop rules.

Every special spends one of two shared Kit uses. Camp recovers one Kit use, capped at two, to conscious survivors. Life also starts with two Slots and recovers one capped Slot at camp; only Cure Wounds spends a Slot as well as a Kit use. Slots must remain at least the remaining Kit balance. Cure Wounds is the actual first authored command, with its existing public name and 2d8+3 dice; there is no third hidden ability or distinct generic healing alias. Other new kits have no Slots. No turn, form, level, route or Resume refills a resource. Levels still add only one basic damage each.

Targets require Manhattan range and blocked-cell sight. Ally means another conscious party member; self means the owner. Cure Wounds accepts conscious self or ally. Healing requires missing HP and never heals downed or dead adventurers; shared Rescue/potion retain their existing purpose. All compositions and duplicate builds use independent adventurer IDs/pools/effect ownership.

| Build | F / first special | M / second special |
| --- | --- | --- |
| Cleric / Knowledge | Knowing Exchange: Bonus, ally range 4, exchange occupied cells | Revealed Opening: Action, enemy range 5, guaranteed damage 6 |
| Cleric / Life | Cure Wounds: Action, friendly range 1, heal 2d8+3; also spends one Slot | Sanctuary: Bonus, ally range 3, protection 3 until next own turn |
| Cleric / Light | Radiant Burst: Action, enemy range 3, safe area damage 5 | Warding Flare: Bonus, self range 0, protection 3 until next own turn |
| Cleric / Nature | Grasping Vine: Action, enemy range 3, damage 4, then push one open cell | Renewing Touch: Bonus, ally range 1, heal 7 |
| Cleric / Tempest | Thunder Shove: Bonus, enemy range 1, damage 5, then push one open cell | Storm Burst: Action, enemy range 3, safe area damage 4 |
| Cleric / Trickery | Decoy Ward: Bonus, self range 0, protection 4 until next own turn | Decoy Exchange: Action, ally range 4, exchange occupied cells |
| Cleric / War | War Strike: Bonus, enemy range 1, guaranteed damage 7 | Battle Shelter: Action, ally range 2, protection 3 until next own turn |
| Cleric / Order | Ordered Exchange: Action, ally range 2, exchange occupied cells | Voice of Shelter: Bonus, ally range 4, protection 3 until next own turn |
| Cleric / Peace | Mending Word: Action, ally range 3, heal 8 | Peaceful Exchange: Bonus, ally range 3, exchange occupied cells |
| Cleric / Twilight | Twilight Shelter: Action, ally range 3, protection 4 until next own turn | Dusk Burst: Bonus, enemy range 2, safe area damage 3 |
| Druid / Land | Thorn Field: Action, enemy range 4, safe area damage 4 | Bark Shelter: Bonus, ally range 2, protection 2 until next own turn |
| Druid / Moon | Beast Mantle: Bonus, self range 0, protection 4 until next own turn | Lunar Claw: Action, enemy range 2, guaranteed damage 7 |
| Druid / Spores | Spore Cloud: Bonus, enemy range 1, safe area damage 4 | Fungal Renewal: Action, self range 0, heal 7 |
| Druid / Stars | Archer Star: Bonus, enemy range 5, guaranteed damage 4 | Chalice Light: Action, ally range 3, heal 6 |
| Druid / Wildfire | Spirit Ram: Action, enemy range 4, damage 4, then push one open cell | Spirit Exchange: Bonus, ally range 2, exchange occupied cells |
| Bard / Lore | Cutting Repulse: Bonus, enemy range 3, damage 3, then push one open cell | Restoring Verse: Action, ally range 2, heal 7 |
| Bard / Valor | Combat Inspiration: Bonus, ally range 1, protection 4 until next own turn | Valiant Strike: Action, enemy range 3, guaranteed damage 6 |
| Bard / Creation | Dancing Exchange: Bonus, ally range 4, exchange occupied cells | Resonant Burst: Action, enemy range 2, safe area damage 5 |
| Bard / Eloquence | Reassuring Word: Action, ally range 4, protection 2 until next own turn | Unsettling Note: Bonus, enemy range 4, guaranteed damage 5 |

Protection is owner-bound: one target per owner; new applications replace that owner's ward. Independent duplicate owners coexist; only the strongest reduction applies to incoming damage. Expiry is the start of the owner's next turn, not a global round boundary. Owner or target down/death clears the relevant ward; combat termination and route carry clear temporary state. No temporary effect changes a base profile or creates HP pools. Beast Mantle is a bounded defensive form (four damage reduction); Lunar Claw is an independent owner attack, available with or without the mantle. Neither changes the hero's profile, resource caps or initiative. Decoy and Wildfire spirit names describe the listed owner-issued protection/displacement effects; they create no actor, target, turn, recruitment or revival.

The existing shared ordered-area query remains radius-one Manhattan, primary then other conscious enemies in encounter order, enemy-only with primary sight. Adjacent secondary cells cannot have an intervening wall. Push resolves after damage, only moves a conscious survivor into one open in-bounds cell, with X winning direction ties. Blocked pushes still commit damage. Exchange spends the owner's budget and retains ordinary movement. No reactive trigger is introduced, so no recursive damage/support chain exists. Committed ordered facts carry actual loss/healing/protection and movement; playback consumes committed facts without rolling. Cure's existing dice playback uses its committed Healing value once.

Cleric retains HP22/AC13/d6+3/range3/initiative1. Druid has HP24/AC13/d6+3/range3/initiative1; Bard HP22/AC13/d6+3/range3/initiative2. All have +5 attack, speed4 and one starting potion. Earlier kits and arcane saved-RNG draw order remain unchanged. Druid11 and Bard12 append explicit persisted role ordinals. Catalog revision5 retains unpublished schema4 and current-v1.json discovery; unsupported revisions reject, explicit New Run preserves incompatible bytes through the existing recovery flow. Direct recognized-build resource validation and run/JSON profile/death/history/equipment validation remain strict; bounded uncatalogued fixtures remain supported.

Omissions: full spellbooks, tabletop domain acquisition, saving throws, charm, damage types, permanent forms, creature inventories, summoned actors, independent companion turns, reactions, unrestricted status language, resurrection and 2024 options. Sibling identities differ by effect/target/budget combinations as well as range/amount. The pre-existing final45 polish and mixed Missile/F/M/camp follow-ups remain unchanged.

Verification, source fingerprint, intermediate failures and report paths are in [the ledger](ledger.md). Headless tests do not prove native readability/input, sound or balance. Engine risks32/35 remain unresolved. No staging integration or full70 delivery is claimed; keep43 open until integration.
