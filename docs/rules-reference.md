# Tactical battle and party rules

This work includes material from the System Reference Document 5.2.1 (“SRD 5.2.1”) by Wizards of the Coast LLC, available at https://www.dndbeyond.com/srd. The SRD 5.2.1 is licensed under the Creative Commons Attribution 4.0 International License, available at https://creativecommons.org/licenses/by/4.0/legalcode.

Source: [SRD 5.2.1 PDF](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf), legal notice p.1; combat pp.6–18; Cleric pp.36–37, Fighter pp.47–48, Rogue pp.61–62, Wizard pp.77–78 and named spell entries. dnd-diceroll changes and simplifies the source below. Characters, names, encounter and setting are original. [Source notes](srd-source-notes.md) retain the primary-source verification.

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

Healing caps at maximum HP. All positive healing rescues a downed ally and clears the deadline. Healing, potions and Rescue never restore a dead adventurer. Each starting adventurer owns one potion. Slots and Second Wind uses are limited across encounters; only the explicit one-use camp replenishes them. Wrong roles, unavailable budgets/resources, invalid targets/range/sight and repeated spent actions are rejected without state or RNG changes.

## Downed, rescue and permanent loss

An adventurer reduced to zero HP becomes **Downed**, cannot act and has three rescue ticks remaining. At each entry into that adventurer's scheduled initiative slot, decrement the countdown and skip the turn. The third tick makes the adventurer **Dead**. Ticks occur only on accepted turn progression, never elapsed time, frames, previews or rejected commands. A rescued adventurer can act at its next scheduled slot. Enemies at zero HP die immediately. Bodies block movement for the current encounter; enemies attack conscious opponents only.

If no conscious adventurer remains, the party loses immediately: every remaining downed adventurer dies because no rescuer can continue. If all enemies die while a conscious adventurer remains, the party wins; any unresolved downed adventurer dies at that point. Rescue **before** the finishing blow. The victory HUD explicitly states this policy. Dead members retain their identities and never receive turns, healing, loot or resurrection. Survivors can continue with a smaller party. A wipe cannot continue.

## Seeded expedition, rewards and recovery

Each run starts four fresh adventurers with full health, two fighter Wind uses, two cleric/wizard slots, one owned potion each and no gear ranks. The visible seed reproduces the same gameplay with the same commands. Each fresh expedition has a distinct stable run identity, including when reusing a seed; resume retains it. No permanent power carries to a new run.

Choose one of two retained opportunities with Z/X. Guarded Stream trails have two scouts (one ranged); dangerous Watchtower roads have three raiders including an archer, higher health/armor and a larger damage die. Previews disclose enemy count/style, terrain and actual reward, but withhold exact placement, initiative and rolls. Generated 16–18 by 11–12 grids use three bounded obstacle recipes; the party starts in a column four cells from one edge and the enemy line forms 5–7 cells away, leaving open ground behind both. Gaps and lanes connect all floor/spawns; there is no retry-based generation. All five combats defeat the guards; winning the fifth recovers the Waystone artifact and ends the expedition.

After either of the first four victories, select a conscious survivor with 1–4 and press L. That survivor receives one gear rank (cap 3) and one potion on a guarded route or two on a dangerous route (cap 9). A previously cleared dangerous road secures supplies: the next guarded route offers two potions, disclosed in its preview. Selected opportunity, outcome, survivor count and loot recipient remain in run history. Loot cannot be claimed twice, awarded to the dead, or skipped to start another fight. The finale grants the artifact and ends the run instead of offering unusable loot.

After the first loot claim there is exactly one explicit recovery decision: C takes camp, V skips it permanently. Taking camp restores **10 HP**, capped at maximum health, to each conscious survivor; the fighter regains **one Second Wind use**, and the cleric/wizard regain **one slot**, capped at two. It grants no potions or gear and never resurrects a dead adventurer. This is a dnd-diceroll recovery adaptation, not the SRD rest procedure. Camp cannot be deferred past this decision or repeated. Later rewards lead directly to the next route choice.

Every adventurer's identity, role, HP, life state, resources, gear and potions carry into the next generated encounter. Only placement and initiative change; dead bodies remain roster markers and receive no turns. There is no automatic healing on a route/encounter transition, recruitment or resurrection. Rejected route/reward/recovery commands preserve the whole run and both random streams. A full wipe ends the run immediately; N at either ending starts a fresh party with the next seed.

## Tactical adaptations and omissions

- Cardinal movement and Manhattan ranges; one cell represents five feet. No diagonals, difficult terrain, Dash, Disengage, reactions, opportunity attacks, advantage or cover modifiers. Blocked terrain obscures sight, including rays touching blocked corners; combatants do not obscure sight.
- Spell and weapon ranges are shortened for the arena: use the displayed cell ranges. Cure Wounds uses adjacent/self touch; Magic Missile uses six cells and one target for all darts. Wizard Fire Bolt uses four cells. Spell components, damage types/resistances and multi-target dart assignment are omitted.
- Rogue eligibility implements only the conscious-adjacent-ally condition, with the prebuilt ranged weapon qualifying. Advantage/disadvantage and other Sneak Attack eligibility paths are absent. The single attack Action enforces once per turn. Cleric's ranged attack is a fixed profile adaptation rather than an additional named SRD feature.
- Deterministic rescue countdown, guaranteed Rescue and potion use with an Action cost, loss of unresolved downed allies at victory, immediate wipe when no rescuer remains, potion ownership and gear progression are game adaptations. SRD death saves, stabilization, massive damage, zero-HP damage and rest rules are not implemented.
- Enemies perform finite path search, move toward a firing cell, attack a legal conscious opponent if possible, and end their turn. Rejected player input cannot delay enemy progression.

## Play

Every command acts for the active adventurer (gold ring and initiative token); clicking another card or pressing 1–4 only inspects them, and selects the loot recipient after a victory. A (or the Attack / Move button) selects attack/movement mode: glowing tiles are reachable, hovering one previews the path, and a red marker floats over each legal target. Hovering a target shows hit chance, damage range and whether a strong or critical hit could drop it; these forecasts use the same tuning as resolution and never roll. F selects the role action; the rogue keeps its automatic Sneak Attack basic attack. R selects Rescue and P selects a potion; friendly targets get green markers. Choose a target with the mouse or arrows+Enter; arrows follow the screen at any camera angle, and corner brackets show the keyboard cursor. For Second Wind target the fighter's own cell. Space ends the turn. Q/E or right-drag rotates and the wheel zooms. Accepted actions and enemy turns play out on the board with the actual dice; Tab skips the current animation and T cycles 1x, 2x and instant playback. Animation never changes or delays the committed result: an accepted command finishes any playback first, and rejected input never skips it. ? (or the Help button) shows all controls. Z/X or the route cards choose a retained route opportunity, L claims loot for the selected conscious survivor, C/V take/skip the single camp, and N at victory/defeat starts a fresh run. Route choice and loot/camp cannot be bypassed by repeated input.


## Save, resume and ending records

On startup, N chooses a fresh expedition and O resumes the current checkpoint. An unfinished saved expedition must be resumed rather than replaced; N directs you to O. If the previous save is unreadable or incompatible, explicit N preserves the original separately and starts fresh, with visible feedback. Startup itself does not overwrite a run. During play, every accepted action, enemy turn, route/reward/recovery and ending is checkpointed before it appears. K explicitly confirms a save. O always resumes the latest committed state, including movement, spent Action/Bonus Action, injuries, rescue deadlines, permanent deaths, selected routes/history and random streams. Repeated load cannot reroll a resolved action or revive a committed death. A failed checkpoint rejects the candidate action and retains the previous live/disk state; an enemy checkpoint failure pauses enemy progression until a successful K or O. Error feedback explains the failure. Save/resume rebuilds the board and resets targeting/selection; camera framing resets.

H opens/closes ending records; Page Up / Page Down inspect older/newer pages. This pauses enemy presentation while inspecting them. Each completed expedition records its identity, seed, outcome, victories and survivors exactly once. Reloading or saving the same ending adds no duplicate. Fresh runs with the same seed retain separate records. Records grant no health, resources, equipment or other power. N at an ending starts fresh. Invalid/incompatible/incomplete saves fail without partial installation. Local single-player Mac storage uses schema version 3 and an atomic file replacement; there is no stale-save fallback, cloud sync, multiwriter or power-loss guarantee. Developer storage details are in [architecture](architecture.md).

## Issue #28: milestone levels and Fighter prototypes

These are compact dnd-diceroll adaptations, not full tabletop class progression. Start at level 1. Each of victories 1–4 advances each conscious survivor once; unresolved downed members die before this award. Dead members retain their death-level. The fifth encounter is fought at level 5 and awards only the artifact. Every earned level adds +1 fixed basic-attack damage: Fighter melee, Rogue ranged (with its existing eligible Sneak Attack dice), Cleric ranged and Wizard Fire Bolt. At levels 1/2/3/4/5 their fixed modifiers before gear are respectively 3/4/5/6/7 for Fighter, Rogue and Cleric, and 0/1/2/3/4 for Wizard. No HP, slots, Wind, maneuver uses or potions refill on advancement.

At startup or after a completed expedition, select Champion (J or pointer) or Battle Master (B or pointer), then explicitly N/New Expedition. Champion's basic attacks critically hit on natural 19–20, including automatic hits on 19. Battle Master has two Precision Strikes for the entire expedition: M or its button selects an attack costing one Action and one maneuver use, adding +2 to hit and +2 fixed damage, spent even on a miss. Critical dice use the existing attack rule. The camp does not restore maneuver uses. F still selects Second Wind. Subclass is fixed and saved with the new run; resume does not use the startup selection. Help and Records gate these new shortcuts.

The Canvas party cards show level and Fighter specialization; reward status reports the milestone earned. Four loot awards may go to one survivor: gear remains capped at rank 3. Schema version 3 retains the discovery filename current-v1.json. Version 1 (small grids) and version 2 (large grids before levels/subclasses) bytes are rejected; explicit startup New Run preserves them beside the checkpoint before creating a fresh run. No implicit migration or overwrite occurs.
