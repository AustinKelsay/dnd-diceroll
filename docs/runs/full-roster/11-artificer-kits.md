# Artificer compact kits (#44)

Fixed clean baseline `478c7a17c8f64719a1ebd75391f3964ffa377ed5`, sole writer on `feature/37-full-roster`. Approved #37/#44 delegate this tuning. Original compact adaptations of the four Tasha's specializations, available from encounter one; no tabletop rules reproduction or 2024 content. Root owns fresh independent Standards/Spec review, package and inspected native QA.

Catalog revision 6 publishes 70 approved kits across 13 classes, with no interim bases. Artificer appends stable role ordinal 13. Existing 66 kits retain IDs, stats and commands, including Life's actual Cure Wounds first action and Sanctuary second, exact 2d8+3 Cure dice and saved arcane RNG behavior. Schema 4 remains unpublished; discovery is `current-v1.json`, with unchanged incompatible-file preservation. No new persistent fields or rules framework are introduced.

Every Artificer has HP26, AC15, attack +5, d6+3 basic attack at range3, speed4, initiative+1 and one potion. Earned levels add one basic damage each. Every special spends one shared Kit use; each independent adventurer starts with two, cap2, and the one camp recovers exactly one to conscious survivors. There are no Slots. Turns, equipment, forms, levels, route and Resume never refill. Each has basic attack and exactly two authored specials in the existing F/M action bar, alongside shared movement, Rescue, potion and End Turn.

| Build ID | F / first special | M / second special |
| --- | --- | --- |
| artificer.alchemist | Restorative Elixir: Action, other injured conscious ally range2, heal8 | Volatile Flask: Bonus, enemy range3, safe area damage3 |
| artificer.armorer | Defensive Field: Bonus, self range0, protection4 until next own turn | Thunder Gauntlet: Action, enemy range1, damage6 then push |
| artificer.artillerist | Force Cannon: Action, enemy range5, damage5 then push | Protector Cannon: Bonus, other ally range3, protection2 until next own turn |
| artificer.battle-smith | Defender Guard: Action, other ally range1, protection4 until next own turn | Defender Strike: Bonus, enemy range2, guaranteed damage5 |

Targets require Manhattan range and blocked-cell sight; all must be conscious. Healing requires missing HP and never rescues downed/dead adventurers; shared Rescue/potion still provide the established downed rescue. Siblings differ by effect, target and budget combinations: support with safe area; self armor with melee control; ranged control with ally protection; adjacent ally defense with a bonus reach strike.

Cannon and defender are concrete owner-issued effects, with no spawned actor, target, additional hero, initiative or turn. Offensive commands resolve immediately. Protection owns one target per caster and lasts until the start of that owner's next turn. Reapplication replaces that owner's target/amount, duplicate owners coexist independently, and incoming damage uses the strongest reduction rather than a sum. Owner or target down/death clears relevant protection; terminal cleanup clears temporary effects before route carry. Resume neither expires nor refreshes protection. Existing GuardTargetId/GuardTurns/GuardAmount and KitUses encode these policies and retain strict catalog-based validation.

Damage resolves before push. Push moves a still-conscious enemy one cardinal cell away, X winning direction ties, only into an open in-bounds unoccupied cell; blocked push retains damage. Safe area affects primary then adjacent conscious enemies in encounter order, radius-one Manhattan and sight, excluding allies. Fixed specials consume no RNG; basic attacks retain existing dice and critical rules. Forecasts use the same facts without RNG and ordered committed results drive playback.

Literal worked fixture: owner(1,1), injured ally(1,2) HP5, primary(2,1) HP40, secondary(2,2) HP40. Elixir leaves ally13; Flask leaves both enemies37, ally5. Field gives owner reduction4; Gauntlet leaves primary34 at(3,1). Force Cannon leaves primary35 at(3,1); Protector Cannon gives ally reduction2. Defender Guard gives ally reduction4; Defender Strike leaves primary35 in its original cell. Each spends Kit2→1 and only its listed Action/Bonus budget. Both slots can spend the shared pool on one turn; no third command is added.

Omissions: infusion inventory, crafting/economy, spellbooks, armor-model selector, permanent summons, independent companion turns, reactive triggers, unrestricted status engine, recruitment and revival. Existing immutable catalog/effect implementations provide the entire addition.

Public tests cover all eight literal slots, restrictions and atomic RNG preservation, repeated forecasts, normal/accelerated/finished playback, common pool spending, owner protection lifecycle, strict JSON and actual file/fresh-session Resume, continued commands, checkpoint rollback, mixed and duplicate five-fight progression and explicit spent-pool camp0→1/route1/Resume1. Evidence and actual intermediate outcomes are in [the ledger](ledger.md).

Full-roster delivery is not complete: #45 still owns final integration/accessibility, readable human builder labels, concise logs/target names, mixed Missile/F/M/camp coverage and final acceptance. Root owes fresh reviews/package/native QA for this slice. Engine risks #32/#35, automated pointer, sound and human balance remain limitations. Issue #44 stays open until staging integration.
