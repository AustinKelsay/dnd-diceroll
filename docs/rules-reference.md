# Tactical battle and party rules

This work includes material from the System Reference Document 5.2.1 (“SRD 5.2.1”) by Wizards of the Coast LLC, available at https://www.dndbeyond.com/srd. The SRD 5.2.1 is licensed under the Creative Commons Attribution 4.0 International License, available at https://creativecommons.org/licenses/by/4.0/legalcode.

Source: [SRD 5.2.1 PDF](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf), legal notice p.1; combat pp.6–18; Cleric pp.36–37, Fighter pp.47–48, Rogue pp.61–62, Wizard pp.77–78 and named spell entries. Expedition changes and simplifies the source below. Characters, names, encounter and setting are original. [Source notes](srd-source-notes.md) retain the primary-source verification.

## Turns and attacks

Initiative is d20 plus the prebuilt initiative modifier, highest first, with ascending stable-ID ties. A turn grants movement, one Action and one Bonus Action. Only Second Wind spends the Bonus Action in this subset. Movement can be split around actions. Attacks roll d20 plus the prebuilt combined ability/proficiency bonus against Armor Class; equality hits, natural 1 misses and natural 20 hits critically. A critical doubles all attack damage dice, including eligible Sneak Attack dice, and adds fixed modifiers once. HP is clamped at zero.

The fighter has a melee d6+3 attack; rogue and cleric have ranged d6+3 attacks, and wizard has Fire Bolt, a ranged spell attack for d10 with no damage modifier. These are fixed profiles, not full character sheets. Gear rank adds +1 damage per rank to basic attacks, including the wizard's casting focus, to a cap of +3. Magic Missile does not receive gear damage or critical hits.

## Roles and costs

| Action | User and targets | Cost | Effect |
| --- | --- | --- | --- |
| Second Wind | Fighter, self while injured | Bonus Action; one of two starting uses | d10+1 healing |
| Sneak Attack | Rogue's qualifying ranged/finesse basic attack | Included in the attack Action, at most once per turn | Extra d6 on hit when another conscious ally is within one cell of the enemy |
| Cure Wounds | Cleric, injured living ally/self within one cell and sight | Action; one of two starting level-one slots | 2d8+3 healing |
| Magic Missile | Wizard, conscious enemy within six cells and sight | Action; one of two starting level-one slots | Three darts, each d4+1, all assigned to one target; automatic hit |
| Rescue | Any conscious adventurer, downed ally within one cell and sight | Action; no resource | Restore 1 HP |
| Healing potion | Any conscious adventurer, injured living ally/self within one cell and sight | Action; one owned bottle | 2d4+2 healing |

Healing caps at maximum HP. All positive healing rescues a downed ally and clears the deadline. Healing, potions and Rescue never restore a dead adventurer. Each starting adventurer owns one potion. Slots and Second Wind uses are limited across encounters; the next-fight fixture grants no replenishment. Wrong roles, unavailable budgets/resources, invalid targets/range/sight and repeated spent actions are rejected without state or RNG changes.

## Downed, rescue and permanent loss

An adventurer reduced to zero HP becomes **Downed**, cannot act and has three rescue ticks remaining. At each entry into that adventurer's scheduled initiative slot, decrement the countdown and skip the turn. The third tick makes the adventurer **Dead**. Ticks occur only on accepted turn progression, never elapsed time, frames, previews or rejected commands. A rescued adventurer can act at its next scheduled slot. Enemies at zero HP die immediately. Bodies block movement for the current encounter; enemies attack conscious opponents only.

If no conscious adventurer remains, the party loses immediately: every remaining downed adventurer dies because no rescuer can continue. If all enemies die while a conscious adventurer remains, the party wins; any unresolved downed adventurer dies at that point. Rescue **before** the finishing blow. The victory HUD explicitly states this policy. Dead members retain their identities and never receive turns, healing, loot or resurrection. Survivors can continue with a smaller party. A wipe cannot continue.

## Encounter transition and progression fixture

After victory, select a conscious survivor with 1–4 and press N. That survivor receives one gear rank (cap 3) and one potion (cap 9), then the same explicit crossing layout starts with new enemies. Every adventurer's identity, role, HP, life state, resources, gear and potions carry forward. Only placement and initiative change; dead bodies are placed as markers and never revived. An invalid layout or recipient leaves the completed battle, loot and RNG untouched. Repeated N during a fight cannot award loot again. There is no leveling.

This fixture exercises consequences and composition for the next run module. Generated routes, three-encounter objective, camp recovery and save/resume are later slices. The future camp must explicitly replenish living survivors through the authoritative run seam; carrying the party into another encounter never implies resting or resurrection.

## Tactical adaptations and omissions

- Cardinal movement and Manhattan ranges; one cell represents five feet. No diagonals, difficult terrain, Dash, Disengage, reactions, opportunity attacks, advantage or cover modifiers. Blocked terrain obscures sight, including rays touching blocked corners; combatants do not obscure sight.
- Spell and weapon ranges are shortened for the arena: use the displayed cell ranges. Cure Wounds uses adjacent/self touch; Magic Missile uses six cells and one target for all darts. Wizard Fire Bolt uses four cells. Spell components, damage types/resistances and multi-target dart assignment are omitted.
- Rogue eligibility implements only the conscious-adjacent-ally condition, with the prebuilt ranged weapon qualifying. Advantage/disadvantage and other Sneak Attack eligibility paths are absent. The single attack Action enforces once per turn. Cleric's ranged attack is a fixed profile adaptation rather than an additional named SRD feature.
- Deterministic rescue countdown, action-based guaranteed Rescue, loss of unresolved downed allies at victory, immediate no-rescuer wipe, potion ownership and gear progression are game adaptations. SRD death saves, stabilization, massive damage, zero-HP damage and rest rules are not implemented.
- Enemies perform finite path search, move toward a firing cell, attack a legal conscious opponent if possible, and end their turn. Rejected player input cannot delay enemy progression.

## Play

Click a party member/card or press 1–4 to inspect; only the yellow active adventurer can act. A selects attack/movement mode. Cyan outlines show reachable cells and red outlines show legal targets for the selected action. F selects the role action; the rogue keeps its automatic Sneak Attack basic attack. R selects Rescue and P selects a potion. Choose the target using mouse or arrows+Enter; for Second Wind target the fighter's own cell. The bottom panel describes costs, targets, range and effects. Space ends the selected adventurer's turn. Q/E rotates and wheel zooms. Arrows follow the initial board orientation after rotation. N after victory claims loot for the selected conscious survivor and starts the next explicit encounter. Restart PIE for a fresh party.
