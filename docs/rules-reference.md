# Tactical battle rules — slice 1

This work includes material from the System Reference Document 5.2.1 (“SRD 5.2.1”) by Wizards of the Coast LLC, available at https://www.dndbeyond.com/srd. The SRD 5.2.1 is licensed under the Creative Commons Attribution 4.0 International License, available at https://creativecommons.org/licenses/by/4.0/legalcode.

Source: [SRD 5.2.1 PDF](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf), legal notice p.1; combat pp.13–17. Expedition changes and simplifies the source as described below. Characters, names, encounter and setting are original.

## Supported math

An attack rolls d20 plus the attack bonus (the prebuilt combined ability/proficiency modifier) against Armor Class; equality hits. Natural 1 misses regardless of total. Natural 20 hits regardless of AC and is critical. Damage is one configured die plus the fixed modifier; a critical rolls two dice and adds the modifier once. HP cannot fall below zero. Initiative is d20 plus the configured initiative modifier (the prebuilt Dexterity check modifier), highest first.

## Explicit tactical adaptations

- Cardinal grid movement and Manhattan attack ranges; no diagonal movement. One cell represents five feet, displayed in cell units. Shortest legal path determines movement cost; terrain, combatants and inactive bodies block movement.
- One attack action and a movement budget per turn; unused movement can be spent after attacking. No Dash, Disengage, reactions, opportunity attacks, advantage, cover modifiers, bonus actions or critical fumbles in this slice.
- Blocked terrain prevents sight. A diagonal ray touching a blocked cell at a corner is obscured. Combatants do not obscure ranged sight.
- Initiative ties use ascending stable combatant ID. Initiative modifier and attack/damage/defense stats are prebuilt tuning, not full character sheets. The fighter uses a melee attack; rogue/cleric/wizard have simple ranged attacks here. Class abilities belong to the next slice.
- Zero HP makes a combatant inactive and skipped by turn progression. Detailed downed, rescue and permanent-death behavior belongs to the next slice; this is not a tabletop death-save implementation.
- Enemies seek the shortest reachable firing position, move up to their budget, attack a legal opponent when possible, and end the turn. No unreachable target can stall a turn.
- Victory means all enemies are inactive; defeat means all adventurers are inactive. This slice has one encounter and no run-level routing, recovery or save UI.

## Play

Launch the existing sandbox in PIE or the game. Four colored adventurers face three red raiders. The active adventurer is selected automatically on a new party turn. Click a party member or its card (or press 1–4) to inspect; only the active combatant can act. Cyan outlines show reachable cells. Red outlines show legal targets. Click an empty cyan cell to move or a red target to attack. Arrows position an orange grid cursor and Enter selects, moves or attacks that cell through the same commands; arrows follow the board in its initial camera orientation, even after rotation. The cursor starts at each active adventurer. Space ends the selected adventurer's turn. Q/E rotates the overhead camera; mouse wheel zooms. The bottom panel shows budgets, attack preview and recent roll/action results. Restart PIE to begin the same seeded battle again.
