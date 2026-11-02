# Tactical battle and party rules

This work includes material from the System Reference Document 5.2.1 (“SRD 5.2.1”) by Wizards of the Coast LLC, available at https://www.dndbeyond.com/srd. The SRD 5.2.1 is licensed under the Creative Commons Attribution 4.0 International License, available at https://creativecommons.org/licenses/by/4.0/legalcode.

Source: [SRD 5.2.1 PDF](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf), legal notice p.1; combat pp.6–18; Cleric pp.36–37, Fighter pp.47–48, Rogue pp.61–62, Wizard pp.77–78 and named spell entries. dnd-diceroll changes and simplifies the source below. Characters, names, encounter and setting are original. [Source notes](srd-source-notes.md) retain the primary-source verification.

## Turns and attacks

Initiative is d20 plus the prebuilt initiative modifier, highest first, with ascending stable-ID ties. A turn grants movement, one Action and one Bonus Action. Second Wind, Rage and Frenzy spend the Bonus Action in the implemented subset. Movement can be split around actions. Attacks roll d20 plus the prebuilt combined ability/proficiency bonus against Armor Class; equality hits, natural 1 misses and natural 20 hits critically. A critical doubles all attack damage dice, including eligible Sneak Attack dice, and adds fixed modifiers once. HP is clamped at zero.

The fighter has a melee d6+3 attack; rogue and cleric have ranged d6+3 attacks, and wizard has Fire Bolt, a ranged spell attack for d10 with no damage modifier. These are fixed profiles, not full character sheets. Gear rank adds +1 damage per rank to basic attacks, including the wizard's casting focus, to a cap of +3. Magic Missile does not receive gear damage or critical hits.

## Roles and costs

| Action | User and targets | Cost | Effect |
| --- | --- | --- | --- |
| Second Wind | Fighter, self while injured | Bonus Action; one of two starting uses | d10+1 healing |
| Sneak Attack | Rogue's qualifying ranged/finesse basic attack | Included in the attack Action, at most once per turn | Extra d6 on hit when another conscious ally is within one cell of the enemy |
| Cure Wounds | Life Cleric, injured conscious ally/self within one cell and sight | Action; one Slot and one shared Kit use | 2d8+3 healing |
| Magic Missile | Wizard, conscious enemy within six cells and sight | Action; one of two starting level-one slots | Three darts, each d4+1, all assigned to one target; automatic hit |
| Rescue | Any conscious adventurer, downed ally within one cell and sight | Action; no resource | Restore 1 HP |
| Healing potion | Any conscious adventurer, injured living ally/self within one cell and sight | Action; one owned bottle | 2d4+2 healing |

Healing caps at maximum HP. Cure Wounds and authored kit healing require a conscious target; they never heal a downed or dead adventurer. Shared potion and Rescue can restore a downed ally and clear the deadline. Neither restores a dead adventurer. Each starting adventurer owns one potion. Slots and Second Wind uses are limited across encounters; only the explicit one-use camp replenishes them. Wrong roles, unavailable budgets/resources, invalid targets/range/sight and repeated spent actions are rejected without state or RNG changes.

## Downed, rescue and permanent loss

An adventurer reduced to zero HP becomes **Downed**, cannot act and has three rescue ticks remaining. At each entry into that adventurer's scheduled initiative slot, decrement the countdown and skip the turn. The third tick makes the adventurer **Dead**. Ticks occur only on accepted turn progression, never elapsed time, frames, previews or rejected commands. A rescued adventurer can act at its next scheduled slot. Enemies at zero HP die immediately. Bodies block movement for the current encounter; enemies attack conscious opponents only.

If no conscious adventurer remains, the party loses immediately: every remaining downed adventurer dies because no rescuer can continue. If all enemies die while a conscious adventurer remains, the party wins; any unresolved downed adventurer dies at that point. Rescue **before** the finishing blow. The victory HUD explicitly states this policy. Dead members retain their identities and never receive turns, healing, loot or resurrection. Survivors can continue with a smaller party. A wipe cannot continue.

### House rule: two-round Downed (`bTwoRoundDowned`, default on) — SRD deviation, needs Austin's sign-off

When `bTwoRoundDowned` is on (the default; saved in schema 5 together with each unit's `DownedAtSlot`), it replaces the rescue-tick countdown above. SRD 5.2.1 death saving throws are **not** used.

| Event | Result |
|---|---|
| Adventurer drops to 0 HP | **Downed** for 2 full rounds; the clock ticks at the initiative slot where it dropped (its own slot if it dropped on its own turn). The HUD and forecasts show the rounds left. |
| Any healing (Cure Wounds, potion, Rescue, kit healing) while Downed | Revives at the healed HP and clears the clock. |
| Any attack or damage while Downed | Dead. |
| 2 full rounds pass with no healing | Dead. |
| Spare the Dying | Stops the clock (stable at 0 HP). |
| Victory / wipe | Unchanged: unresolved Downed adventurers die. |
| Active unit drops on its own turn (opportunity or readied attack, hazard) | Its turn ends at once. |

Enemy AI prefers conscious targets and attacks a Downed hero only when no conscious one is reachable. Off-hand attacks are not offered against Downed targets. With the flag off, the three-tick rescue countdown applies.

## Seeded expedition, rewards and recovery

Each run starts four fresh adventurers with full health, two fighter Wind uses, two cleric/wizard slots, one owned potion each and no gear ranks. The visible seed reproduces the same gameplay with the same commands. Each fresh expedition has a distinct stable run identity, including when reusing a seed; resume retains it. No permanent power carries to a new run.

Choose one of two retained opportunities with Z/X. Guarded Stream trails have two scouts (one ranged); dangerous Watchtower roads have three raiders including an archer, higher health/armor and a larger damage die. Previews disclose enemy count/style, terrain and actual reward, but withhold exact placement, initiative and rolls. Generated 16–18 by 11–12 grids use three bounded obstacle recipes; the party starts in a column four cells from one edge and the enemy line forms 5–7 cells away, leaving open ground behind both. Gaps and lanes connect all floor/spawns; there is no retry-based generation. All five combats defeat the guards; winning the fifth recovers the Waystone artifact and ends the expedition.

After either of the first four victories, select a conscious survivor with 1–4 and press L. That survivor receives one gear rank (cap 3) and one potion on a guarded route or two on a dangerous route (cap 9). A previously cleared dangerous road secures supplies: the next guarded route offers two potions, disclosed in its preview. Selected opportunity, outcome, survivor count and loot recipient remain in run history. Loot cannot be claimed twice, awarded to the dead, or skipped to start another fight. The finale grants the artifact and ends the run instead of offering unusable loot.

After the first loot claim there is exactly one explicit recovery decision: C takes camp, V skips it permanently. Taking camp restores **10 HP**, capped at maximum health, to each conscious survivor; the fighter regains **one Second Wind use**, and the cleric/wizard regain **one slot**, capped at two. It grants no potions or gear and never resurrects a dead adventurer. This is a dnd-diceroll recovery adaptation, not the SRD rest procedure. Camp cannot be deferred past this decision or repeated. Later rewards lead directly to the next route choice.

Every adventurer's identity, role, HP, life state, resources, gear and potions carry into the next generated encounter. Only placement and initiative change; dead bodies remain roster markers and receive no turns. There is no automatic healing on a route/encounter transition, recruitment or resurrection. Rejected route/reward/recovery commands preserve the whole run and both random streams. A full wipe ends the run immediately; N at either ending starts a fresh party with the next seed.

## Tactical adaptations and omissions

- Cardinal movement and Manhattan ranges; one cell represents five feet. No diagonals, difficult terrain, Dash, Disengage, reactions, opportunity attacks or cover modifiers. Blocked terrain obscures sight, including rays touching blocked corners; combatants do not obscure sight.
- Spell and weapon ranges are shortened for the arena: use the displayed cell ranges. Cure Wounds uses adjacent/self touch; Magic Missile uses six cells and one target for all darts. Wizard Fire Bolt uses four cells. Spell components, damage types/resistances and multi-target dart assignment are omitted.
- Rogue eligibility implements only the conscious-adjacent-ally condition, with the prebuilt ranged weapon qualifying. Advantage also qualifies Sneak Attack; disadvantage always denies it, even with an adjacent ally. The single attack Action enforces once per turn. Cleric's ranged attack is a fixed profile adaptation rather than an additional named SRD feature.
- Deterministic rescue countdown, guaranteed Rescue and potion use with an Action cost, loss of unresolved downed allies at victory, immediate wipe when no rescuer remains, potion ownership and gear progression are game adaptations. SRD death saves, stabilization, massive damage, zero-HP damage and rest rules are not implemented.
- Enemies perform finite path search, move toward a firing cell, attack a legal conscious opponent if possible, and end their turn. Rejected player input cannot delay enemy progression.

## Play

Every command acts for the active adventurer (gold ring and initiative token); clicking another card or pressing 1–4 only inspects them, and selects the loot recipient after a victory. A (or the Attack / Move button) selects attack/movement mode: glowing tiles are reachable, hovering one previews the path, and a red marker floats over each legal target. Hovering a target shows hit chance, damage range and whether a strong or critical hit could drop it; these forecasts use the same tuning as resolution and never roll. F selects the role action; the rogue keeps its automatic Sneak Attack basic attack. R selects Rescue and P selects a potion; friendly targets get green markers. Choose a target with the mouse or arrows+Enter; arrows follow the screen at any camera angle, and corner brackets show the keyboard cursor. For Second Wind target the fighter's own cell. Space ends the turn. Q/E or right-drag rotates and the wheel zooms. Accepted actions and enemy turns play out on the board with the actual dice; Tab skips the current animation and T cycles 1x, 2x and instant playback. Animation never changes or delays the committed result: an accepted command finishes any playback first, and rejected input never skips it. ? (or the Help button) shows all controls. Z/X or the route cards choose a retained route opportunity, L claims loot for the selected conscious survivor, C/V take/skip the single camp, and N at victory/defeat starts a fresh run. Route choice and loot/camp cannot be bypassed by repeated input.


## Save, resume and ending records

On startup, N chooses a fresh expedition and O resumes the current checkpoint. An unfinished saved expedition must be resumed rather than replaced; N directs you to O. If the previous save is unreadable or incompatible, explicit N preserves the original separately and starts fresh, with visible feedback. Startup itself does not overwrite a run. During play, every accepted action, enemy turn, route/reward/recovery and ending is checkpointed before it appears. K explicitly confirms a save. O always resumes the latest committed state, including movement, spent Action/Bonus Action, injuries, rescue deadlines, permanent deaths, selected routes/history and random streams. Repeated load cannot reroll a resolved action or revive a committed death. A failed checkpoint rejects the candidate action and retains the previous live/disk state; an enemy checkpoint failure pauses enemy progression until a successful K or O. Error feedback explains the failure. Save/resume rebuilds the board and resets targeting/selection; camera framing resets.

H opens/closes ending records; Page Up / Page Down inspect older/newer pages. This pauses enemy presentation while inspecting them. Each completed expedition records its identity, seed, outcome, victories and survivors exactly once. Reloading or saving the same ending adds no duplicate. Fresh runs with the same seed retain separate records. Records grant no health, resources, equipment or other power. N at an ending starts fresh. Invalid/incompatible/incomplete saves fail without partial installation. Local single-player Mac storage uses schema version 4 and an atomic file replacement; there is no stale-save fallback, cloud sync, multiwriter or power-loss guarantee. Developer storage details are in [architecture](architecture.md).

## Issue #28: milestone levels and Fighter prototypes

These are compact dnd-diceroll adaptations, not full tabletop class progression. Start at level 1. Each of victories 1–4 advances each conscious survivor once; unresolved downed members die before this award. Dead members retain their death-level. The fifth encounter is fought at level 5 and awards only the artifact. Every earned level adds +1 fixed basic-attack damage: Fighter melee, Rogue ranged (with its existing eligible Sneak Attack dice), Cleric ranged and Wizard Fire Bolt. At levels 1/2/3/4/5 their fixed modifiers before gear are respectively 3/4/5/6/7 for Fighter, Rogue and Cleric, and 0/1/2/3/4 for Wizard. No HP, slots, Wind, maneuver uses or potions refill on advancement.

At startup or after a completed expedition, select Champion (J or pointer) or Battle Master (B or pointer), then explicitly N/New Expedition. Champion's basic attacks critically hit on natural 19–20, including automatic hits on 19. Battle Master has two Precision Strikes for the entire expedition: M or its button selects an attack costing one Action and one maneuver use, adding +2 to hit and +2 fixed damage, spent even on a miss. Critical dice use the existing attack rule. The camp does not restore maneuver uses. F still selects Second Wind. Subclass is fixed and saved with the new run; resume does not use the startup selection. Help and Records gate these new shortcuts.

The Canvas party cards show level and Fighter specialization; reward status reports the milestone earned. Four loot awards may go to one survivor: gear remains capped at rank 3. Schema version 3 retains the discovery filename current-v1.json. Version 1 (small grids) and version 2 (large grids before levels/subclasses) bytes are rejected; explicit startup New Run preserves them beside the checkpoint before creating a fresh run. No implicit migration or overwrite occurs.

## Issue #39: catalog conversion

Current immutable numeric profiles and abilities are catalog-backed. Stable build IDs are `fighter.champion`, `fighter.battle-master`, `rogue.base`, `cleric.base`, and `wizard.base`; the last three retain existing base gameplay without new subclass features. Instance names/IDs are separate from class/build. Schema 4 stores the catalog revision and selected identities and rejects earlier schemas; explicit startup NewRun preserves incompatible bytes using the existing recovery flow. The issue #28 schema discussion above is historical. Second Wind remains **1d10+1 per use**, Cure Wounds 2d8+3, Magic Missile 3d4+3, and leveling adds only basic damage with no refill.

## Issue #40: selected parties and Barbarian controls

The next-party builder selects four independent slots, including duplicate builds. 1–4 focuses a slot; Up/Down cycles the five implemented classes, Left/Right cycles compatible kits, G restores the familiar preset. N starts the selected party; O always resumes the saved party. Editing is available at startup and endings, never during an active run. J/B retain slot-one Fighter shortcuts. Historical fixed-party and #28 controls above describe their delivery stage.

F selects the acting hero's first catalog special and M its second, falling back to basic Attack if absent. Rogue Sneak Attack stays passive. Rage and Frenzy spend Bonus Action; Shelter, Long Claw and Wild Bolt spend Action. The [Barbarian contract](runs/full-roster/02-barbarian-kits.md) defines exact costs, effects, durations and recovery. Buff previews make no damage/healing claim; Wild Bolt has two discrete seeded outcomes. Help and Records gate builder navigation and combat shortcuts. [Control contract](runs/full-roster/03-ui-controls.md) records button IDs and keyboard recipe.

## Issue #43: divine, nature and support kits

The current catalog has66 approved subclasses across12 classes, with no interim bases. Life replaces the familiar Cleric base and exposes Cure Wounds and Sanctuary as its two authored actions. Cure Wounds retains2d8+3 but shares the two-use Kit budget with Sanctuary and also spends one Slot. Camp restores one capped Kit use and one capped Life Slot; other new kits have only Kit uses. The [support kit contract](runs/full-roster/09-support-kits.md) defines all19 additions, conscious-target healing, exact owner-bound effects and omissions. Catalog revision5 retains unpublished schema4 and incompatible-checkpoint preservation. Earlier delivery sections above are historical.

Issue #44 adds the four Tasha's Artificer specializations through original compact adaptations: Alchemist healing/safe area, Armorer self defense/melee push, Artillerist ranged push/ally protection, Battle Smith adjacent ally guard/bonus reach strike. Each has two shared Kit uses (camp +1 capped2), no Slots, and two authored actions. Cannon/defender effects belong to their owner with no separate actors or turns. Exact profiles, targets, budgets, durations and omissions are in [the Artificer contract](runs/full-roster/11-artificer-kits.md). Catalog revision6/schema4 are the current source contract; final #45 acceptance remains pending.

## Final compact roster source contract (#45)

The current catalog contains exactly70 approved subclasses across13 classes (40 from2014 PHB,30 from Tasha). Historical interim/base and compatibility descriptions above are superseded by the [final source contract](runs/full-roster/13-final-roster.md). Evocation now authors Magic Missile as its first action: Action, one Kit plus one Slot, guaranteed3d4+3 at range6 with sight. Its only other special is Sculpted Burst: Bonus, one Kit, safe-area3 at range4 with sight. Special1 is unavailable for this build. Camp recovers one Kit and one Slot within their caps; leveling, route and Resume do not refill. Catalog revision7/schema4 retains strict incompatible-file preservation. Other69 kits keep their batch contracts. Root final acceptance remains pending.
## Issue #53: ability scores and modifiers

Every combatant has six ability scores (STR, DEX, CON, INT, WIS, CHA; range 1-30) and the SRD modifier floor((score - 10) / 2). Adventurer scores are prebuilt per role (Fighter 16/12/15/8/13/10, Rogue 8/16/13/12/10/14, Cleric 13/12/14/10/16/8, Wizard 8/12/14/16/13/10) and chosen so that the key attack ability modifier plus the level-1 proficiency bonus equals the existing +5 attack bonus. Enemies use generic original scores (archers and scouts lean on Dexterity). Scores are read through the const `FCombatBattle::AbilityScores`/`AbilityModifier` queries, derived from the saved role, so no save schema change is needed until the build catalog (#39) owns them. Existing attack, damage, AC and initiative values are unchanged; the Rogue's prebuilt +4 initiative is kept even though DEX 16 gives +3.

## Issue #54: proficiency, skills, saves, tools and expertise

Proficiency bonus is 2 + (level - 1) / 4. All 18 SRD skills map to their abilities (STR: Athletics; DEX: Acrobatics, Sleight of Hand, Stealth; INT: Arcana, History, Investigation, Nature, Religion; WIS: Animal Handling, Insight, Medicine, Perception, Survival; CHA: Deception, Intimidation, Performance, Persuasion). `FProficiencyRules::BuildCharacter` validates class skill picks against the SRD 5.2.1 class list and count, two background-style picks from any unchosen skill, and the class's skill Expertise picks; any invalid pick rejects the whole build with no partial result. Expertise doubles the proficiency bonus. Saving throw proficiencies: Fighter STR/CON, Rogue DEX/INT, Cleric WIS/CHA, Wizard INT/WIS, Bard DEX/CHA, Ranger STR/DEX. SRD 5.2.1 class skill lists: Fighter adds Persuasion, Rogue has no Performance, Wizard adds Nature, Bard picks any 3, Ranger picks 3 of Animal Handling, Athletics, Insight, Investigation, Nature, Perception, Stealth, Survival. **Expertise is skills only (SRD 5.2.1):** Rogue 2 at level 1 and 2 more at 6; Bard 2 at level 2 and 2 more at 9; Ranger Deft Explorer 1 at level 2 and Expertise 2 more at 9; Wizard Scholar 1 at level 2, which must be a proficient Arcana, History, Investigation, Medicine, Nature or Religion. Thieves' tools give proficiency only (no tool Expertise), so the strongbox pick-lock total is d20 + DEX + proficiency. Prebuilt choices: Fighter Athletics, Perception + Intimidation, Survival; Rogue Stealth, Sleight of Hand, Acrobatics, Deception + Insight, Investigation, Expertise Stealth and Sleight of Hand (plus Acrobatics and Deception at 6); Cleric Medicine, Religion + Insight, History; Wizard Arcana, Investigation + History, Religion, Scholar Arcana at 2; Bard Performance, Persuasion, Deception + Insight, History, Expertise Performance and Persuasion at 2 (plus Deception and Insight at 9); Ranger Perception, Stealth, Survival + Athletics, Nature, Deft Explorer Stealth at 2 (plus Perception and Survival at 9). Prebuilt choices grow with level, so a level-6 Rogue no longer falls back to scores only. `FCombatBattle::Character(Id)` is a const query; no save schema change.

## Issue #55: ability checks, skill checks and saving throws

`FCheckRules` resolves a d20 + modifier against a DC (1..30). Ability checks and saves have no automatic success on a natural 20 or failure on a natural 1. Advantage and disadvantage combine SRD-style: any advantage plus any disadvantage is a normal roll, however many sources. A normal roll spends one die; advantage or disadvantage spends two from the same seeded stream as combat dice. Forecasts are exact over all 400 pairs of d20 faces (shown rounded to a whole percent) and spend no random numbers. Contested checks: the actor must beat the defender; a tie keeps the status quo. `FCombatBattle::SkillCheck`/`SavingThrow` roll from the battle's random state and reject unknown or non-conscious units and out-of-range DCs without spending RNG; `SkillCheckChance`/`SavingThrowChance` are const forecasts.

## Issue #56: attack advantage and disadvantage

Attack rolls resolve advantage and disadvantage per SRD 5.1: any number of sources of each cancel to a normal roll; otherwise two d20s are rolled and the higher (advantage) or lower (disadvantage) is kept, spending exactly one extra RNG draw. The source implemented now is a ranged attack (weapon range above one cell) made while an adjacent conscious hostile can see the attacker and isn't Incapacitated (SRD 5.2.1): an Incapacitated, Stunned, Paralyzed, Unconscious, Petrified or Blinded neighbour imposes nothing, and neither does one that can't see the attacker (invisible attacker, heavy obscurement, or darkness without darkvision reach). Forecasts report the mode, its named sources and the exact chance out of 400 (normal 20x, advantage 400-(20-x)^2, disadvantage x^2 for x winning faces) without spending RNG. Feedback and playback show both d20s and the mode. Conditions (#57) will add further sources.

## Issue #68: damage types, resistance, vulnerability and immunity

Every damaging action has an SRD damage type: Fighter slashing (longsword), Rogue piercing (shortbow), Cleric radiant (its fixed ranged profile; an adaptation), Wizard fire (Fire Bolt), Magic Missile force, Raider slashing, Archer piercing. Defenses come from the target's profile (its Role) in a small SRD table: Skeleton (vulnerable bludgeoning; immune poison), Zombie (immune poison), Ice Mephit (vulnerable bludgeoning and fire; immune cold and poison), Fire Elemental (resists bludgeoning, piercing and slashing; immune fire and poison). The current Raider and Archer have none, so existing play is unchanged. After all other modifiers, including critical dice and Sneak Attack, immunity sets damage to 0, resistance halves it (rounded down) and vulnerability doubles it; with both, halve then double. Repeated sources count once. The forecast range, the can-drop warning and the result use the same adjustment, and the dice spent are the same as against an undefended target. Feedback shows the type and response only when one applies. Magic Missile's three darts are adjusted as one total (an adaptation).

## Issue #69: critical hits and weapon properties

Critical hits double every damage die of the attack, including Sneak Attack, and add modifiers once (already true in battle; now covered by a worked test). A pure SRD weapon table and property rules (`FWeaponRules`) cover 24 SRD weapons and every listed property on the 5-foot grid: finesse (higher of STR/DEX), ranged weapons use DEX, thrown melee weapons use STR; reach is 2 cells; range bands convert feet to cells (normal, long = disadvantage, out of range); ammunition needs and spends one piece; thrown weapons use their range only when thrown; versatile uses its larger die two-handed; two-handed (or versatile two-handed) weapons exclude a shield; light weapons allow two-weapon fighting with no positive off-hand damage modifier; heavy weapons impose disadvantage on Small wielders; loading limits a weapon to one shot per action. Battle units still use their existing per-unit damage die and cell range: wiring a weapon to each adventurer needs a persisted equipment field (save schema change, coordinate with #39), so it is deferred.

## Issue #70: armor, shields, and Dexterity caps

`FArmorRules` holds the SRD armor table (padded through plate, plus the shield). AC = base + DEX modifier for light armor or no armor, + DEX up to +2 for medium, base only for heavy; a shield adds +2. Heavy armor with a Strength requirement costs 10 ft of speed below that score; armor marked for it gives Stealth disadvantage. Proficiency: Fighter all armor and shields, Cleric light/medium/shields, Rogue light, Wizard none. Wearing armor or a shield without proficiency gives disadvantage on STR/DEX checks, saves and attacks and prevents spellcasting. Battle AC is still the saved per-unit value: equipping armor per adventurer needs persisted equipment (save schema change, coordinate with #39), so battle wiring is deferred.

## Issue #63: cover

Attack rolls (Attack, Precision Strike) check cover from blocked cells. From the attacker's best cell corner, a line is traced to each of the four corners of the target's cell; a line is blocked when it passes through the interior of a blocked cell (running along an edge or touching a corner does not block). One or two blocked lines give half cover (+2 AC), three or four give three-quarters cover (+5 AC). No line of sight between cell centers remains total cover: the target cannot be targeted. The forecast hit chance and resolution both use the effective AC, the result reports `CoverBonus`, and feedback names the cover when it applies. Cover is derived from the map each time, so nothing new is saved. Decision for Austin: creatures do not grant cover yet; Magic Missile and touch spells ignore cover (no attack roll); Dexterity-save cover bonuses wait for saving-throw effects.

## Issue #86: passive scores, Help, and group checks

`FGroupCheckRules`: a passive score is 10 + the check modifier, +5 with advantage and -5 with disadvantage. Help adds one advantage source to the helped check, so any disadvantage still cancels it to a normal roll. A group check succeeds when at least half the group succeeds (an empty group fails). `GroupSuccessPerMillion` gives the exact group chance from each member's exact d20 chance and spends no RNG; `Resolve` rolls members in order with the same RNG stepping as single checks. Library only: exposing Help as a battle or exploration command waits for the action economy work (#60), since nothing here is saved.

## Issue #71: spell facts and slots by level

`FSpellRules` holds static SRD facts for Fire Bolt, Sacred Flame, Magic Missile, Cure Wounds, Guiding Bolt, Burning Hands, Healing Word and Thunderwave: level, school, casting time, range, components, duration, class list, effect dice and damage type, and the extra dice (or darts) per slot level above the spell's level. `Summary` gives the tooltip line (for example "1st-level evocation | 1 action | 120 ft | V, S | Instantaneous"). The full-caster slot table for Cleric and Wizard covers character levels 1-5 and spell levels 1-3 (2; 3; 4/2; 4/3; 4/3/2). Battle still spends the single saved `SpellSlots` pool and the HUD tooltips are not wired: replacing the pool with per-level slots is a save schema change (coordinate with #39), and the hover tooltips touch `ExpeditionHUD.cpp` (after #50).

## Issue #73: spell attacks, save DC, and saving-throw spells

`FSpellcastingRules`: Cleric casts with Wisdom and Wizard with Intelligence. Spell save DC = 8 + proficiency bonus + ability modifier; spell attack bonus = proficiency bonus + ability modifier. Each spell has a delivery: attack roll (Fire Bolt, Guiding Bolt), saving throw (Sacred Flame: DEX, nothing on a success; Burning Hands: DEX, half; Thunderwave: CON, half), automatic (Magic Missile) or healing. Forecast helpers give the exact attack hit chance (natural 1 misses, natural 20 hits) and the target's save-fail chance in 1/400ths with no RNG; half damage rounds down. Battle Magic Missile and Cure Wounds are unchanged; new save spells in battle wait for the per-level slot pool (#71 schema change).

## Issue #72: cantrips and cantrip scaling

`FCantripRules`: cantrips (Fire Bolt, Sacred Flame) cost no slot and scale their dice by character level: x1 at 1-4, x2 at 5-10, x3 at 11-16, x4 at 17+. Fire Bolt is 1d10, then 2d10 at level 5; Sacred Flame 3d8 at 11. Leveled spells report their slot level as cost and do not scale here (see upcasting in #71). The current Wizard basic attack is not converted to Fire Bolt yet: that is an adaptation call for Austin.

## Issue #75: area-of-effect templates

`FAreaRules::Cells` maps SRD shapes onto the 5-foot grid with exact integer geometry, and `Covers` answers the same question per cell so previews equal resolution. Cone, line and cube start from the caster's cell (never including it) and aim at any cell offset; sphere and cylinder are centered on a grid intersection (the top-left corner of the origin cell) and include cells whose centers are within the radius. A cone includes cells whose center is ahead of the caster, within its length, and no farther off-axis than half its forward distance (a 15-ft cone east is 1 + 3 + 3 cells). A line is 5 ft wide: cells whose centers are within half a cell of the aim line. A cube snaps to the dominant axis and sits flush against the caster's face, centered on the caster's row for odd sizes. Decision for Austin: templates use true (Euclidean) length, so a 30-ft diagonal line covers 4 cells while movement counts diagonals as 5 ft. Blocking terrain and the HUD range-tile preview are not wired yet (#50).

## Issue #88: light, vision and darkvision

`FLightRules`: cells are bright, dim or dark. Dim light lightly obscures (disadvantage on sight-based Perception); darkness heavily obscures (sight checks fail and the observer is effectively blinded). Darkvision (60 ft on Goblin, Skeleton, Zombie, Ice Mephit and Fire Elemental profiles) treats darkness as dim and dim as bright within range. Attacking a target you cannot see has disadvantage; attacking one that cannot see you has advantage; both cancel. Hiding needs obscurement. Battle lighting per cell is not wired: it needs a light value per terrain cell (saved terrain, with #64). Adventurers have no darkvision until ancestries exist (decision for Austin).

## Issue #90: Sleight of Hand

`FSleightRules`: lifting or planting an item is a Dexterity (Sleight of Hand) check against the target's passive Perception as the DC. Meeting it takes the item unnoticed; missing by 1-4 achieves nothing; missing by 5 or more gets the thief caught (the target turns hostile). The forecast is exact and spends no RNG. Rogues from level 2 can do it as a bonus action (Cunning Action, use an object). Route and social node wiring and item transfer wait for saved inventory (#39); an alert target rolling active Perception is left for the contest resolver when nodes exist.

## Issue #91: Performance

`FPerformanceRules`: a Charisma (Performance) check at a camp, town or merchant. Success earns the node's base coin and improves the audience's attitude one step (hostile, indifferent, friendly); beating the DC by 10 doubles the coin; missing by 5 or more gets the performer booed (attitude one step worse, no coin). Coin amounts are Expedition tuning for Austin. Paying coin into the run and remembering NPC attitude need saved currency and node state (#84, #39).

## Issue #92: Animal Handling and mounts

`FBeastRules`: only beasts (Wolf DC 13, Giant Rat 10, Boar 13, Brown Bear 15, Giant Spider 15) can be handled, and not once they have been hurt this fight. A Wisdom (Animal Handling) check that misses leaves the beast hostile; meeting the DC calms it (it will not attack unless attacked); beating it by 5 redirects it out of the fight. Rejected attempts roll nothing and keep the RNG. Mounting or dismounting costs half speed; a controlled mount may only Dash, Disengage or Dodge. Beast profiles are not in the current enemy roster and the calmed behaviour needs a saved per-combatant flag (#39); mounts are optional design for Austin.

## Issue #93: knowledge and Wisdom checks outside combat

`FLoreRules` gives each skill a use. Recall before a fight: Religion for undead, Arcana for elementals (DC 15), Nature for beasts and History for humanoids (DC 10); a success reveals one fact (resistances first, then abilities, then weaknesses) plus one per 5 over the DC, up to three. Identify: Arcana DC 15 for magic items and runes, History DC 10 for old mundane items. Medicine at camp (DC 10) treats an injury for hit points equal to the patient's level. Insight reads an NPC when it meets 10 + the NPC's Deception modifier. Revealed facts and treated injuries need saved run state (#39) and HUD wiring (#50); amounts are tuning for Austin.

## Issue #94: tools and kits

`FToolRules`: a tool check adds the proficiency bonus when you are proficient with the tool you are holding; also being proficient in the matching skill grants advantage (the SRD lets the GM do this; we always do). Some tasks require the tool: no thieves' tools, no lockpicking. A healer's kit (10 uses) stabilizes a dying creature with no check and spends one use. Herbalism kit proficiency lets you brew antitoxin and potions of healing. The existing `ETool` set is unchanged. Kit charges and tool items need saved inventory (#39, the issue's schema change).

## Issue #85: exploration hazards

`FHazardRules` resolves a hazard with one skill check: cliff (Athletics DC 12, 4 HP on a fail), ledge (Acrobatics DC 12, 3 HP), getting lost (Survival DC 13, 1 supply and a reroute) and a hidden passage (Investigation DC 15, success opens a shortcut). `ForecastOutOf400` shows the exact chance before committing. Placing hazards on the map, spending supplies and saving rerouted paths need run state (#39); DCs and costs are tuning for Austin.

## Issue #80: stealth, hidden attackers and surprise

`FStealthRules`: passive scores are 10 + modifier (+/-5 for advantage/disadvantage). A creature is hidden from an observer when its Stealth total meets or beats the observer's passive Perception; the tie going to the hider is our reading and is flagged for Austin. Attacking while hidden gives advantage (an unseen target gives disadvantage; both cancel) and always reveals the attacker. A side is surprised only if every ambusher is hidden from it, and surprised creatures cannot act in round 1. A per-unit hidden flag and pre-battle ambush state need battle and run state (#39).

## Issue #81: locks and traps

`FLockTrapRules`: lock DCs are 10/15/20 for simple, good and superior locks, and picking one needs thieves' tools. A trap is found when an Investigation total (or passive Perception) meets its DC. Disarming needs thieves' tools; a miss by 5 or more sets the trap off. Chest and door contents, opened state and trap placement need run state (#39).

## Issue #82: social influence

`FSocialRules`: Persuasion or Intimidation meets an attitude DC (friendly 10, indifferent 15, hostile 20) to move the NPC one step toward friendly. Deception is a contest against Insight and ties go to the target. A swayed hostile NPC avoids the fight, a newly friendly one shares information and an already friendly one gives a reward. NPC attitudes and granted rewards need run state (#39); the outcome table is a design call for Austin.

## Issue #84: coins and prices

`FCoinRules` keeps money in copper (1 gp = 10 sp = 100 cp) and formats it largest coin first. Shop prices follow the SRD equipment list; merchants buy at half price. Haggling is a house rule for Austin: Persuasion 15 takes 10% off, 20 takes 20% off. Party purse, shop stock and owned items need run state (#39).

## Issue #60: standard actions (rules only)

`FStandardActionRules`: Rogues from level 2 can Dash, Disengage or Hide as a bonus action (Cunning Action); otherwise each costs the action. Each Dash adds the speed again. Dodge gives attackers it can see disadvantage and gives the dodger advantage on Dexterity saves. Disengage stops leaving reach from provoking. Per-turn Dodge/Disengage flags, the new command types and bonus-action tracking in a battle need battle state and command wiring; they wait for #39 and are left out here.

## Issue #65: falling, prone and forced movement (rules only)

`FDiceRules::Roll` sums plain dice with the combat random stream. `FMovementRules`: a fall deals 1d6 per 10 ft (max 20d6) and leaves the creature prone; standing costs half the speed rounded up; crawling costs double; a push stops before a blocker; attacks against a prone target have advantage within 5 ft and disadvantage beyond. A saved prone condition, elevation on the map and applying pushes in a battle need battle state (#39).

## Issue #66: grapple and shove (rules only)

`FGrappleRules`: grappling and shoving work on targets at most one size larger. Both are Athletics contests where the actor must beat the defender (ties hold); `ContestOutOf400` gives the exact chance. A grappled creature's speed is 0, dragging it costs double, and escaping is the reverse contest. A won shove knocks the target prone or pushes it 1 cell. Saved grappled/prone conditions and creature sizes on profiles need battle state (#39).

## Issue #87: Inspiration (optional, rules only)

`FInspirationRules`: off by default. A creature holds at most one Inspiration; granting while held does nothing. Spending it gives advantage on one d20 test (cancelling disadvantage to a normal roll). Who holds Inspiration across battles, what grants it and the toggle setting need saved state (#39).

## Issue #89: carrying capacity and optional encumbrance (rules only)

`FEncumbranceRules`: capacity is 15 x STR lb and push/drag/lift is 30 x STR. Over capacity the speed drops to 1 cell; over push/drag/lift the creature cannot move. The variant rule is off by default: over 5 x STR costs 2 cells of speed, over 10 x STR costs 4 cells and gives disadvantage on Strength/Dexterity/Constitution checks, attacks and saves. Item weights round up to whole pounds (a potion counts as 1 lb; flagged for Austin). Carried inventory and the toggle need saved state (#39).

## Issue #67: death saves (rules only)

`FDeathSaveRules`: a death save succeeds on 10+, a natural 1 counts as two failures and a natural 20 brings the creature back with 1 hit point. Three successes stabilize, three failures kill. Damage at 0 HP adds a failure (two on a critical), and damage of at least the hit point maximum kills outright, as does massive damage on the way down. Temporary hit points absorb damage first and don't stack. Medicine DC 10 stabilizes. Tallies on downed adventurers and temporary hit points in a battle need battle state (#39); the current Downed/Dead flow is unchanged.

## Issue #62: two-weapon fighting (rules only)

`FTwoWeaponRules`: after taking the Attack action with a light melee weapon, a creature holding a second light melee weapon can spend its bonus action on an off-hand attack. Off-hand damage adds no positive ability modifier unless it has the Two-Weapon Fighting style (negative modifiers still apply). Adding the off-hand command, bonus-action tracking and fighting styles to battles waits for battle wiring and #39.

## Issue #59: opportunity attack triggers (rules only)

`FOpportunityRules` measures reach in grid cells with diagonals counting as 1. A step provokes when it leaves a reactor's reach voluntarily, the mover hasn't disengaged, and the reactor still has its reaction. `FirstProvokingCell` flags the path cell being left (one provocation per reactor). Spending and restoring reactions, resolving the attack mid-move and the AI's use of it need saved reaction state in battles (#39) and are not wired.

## Issue #74: concentration (rules only)

`FConcentrationRules`: after taking damage a concentrating caster makes a Constitution save against DC 10 or half the damage, whichever is higher, with an exact forecast. Starting another concentration spell or becoming incapacitated ends it. None of the current eight spells needs concentration, so nothing in battles changes yet; tracking the active spell needs battle state (#39) once such spells arrive.

## Issue #76: rests and hit dice (rules only)

`FRestRules`: hit dice are d10 for Fighters, d8 for Rogues and Clerics, d6 for Wizards. A spent hit die heals its roll plus the Constitution modifier (never below 0). A long rest returns half the character's level in hit dice (minimum 1). Second Wind, Action Surge, Channel Divinity and superiority dice refresh on a short rest; hit points, hit dice, spell slots and Arcane Recovery on a long rest. Decision for Austin: a camp counts as a long rest. Spent hit dice and per-pool usage need saved state (#39).

## Issue #77: class feature scaling tables (rules only)

`FClassFeatureRules`: Second Wind heals 1d10 + fighter level; Action Surge from level 2 (twice from 17); Fighters get Extra Attack at 5 (3 attacks at 11, 4 at 20). Sneak Attack is 1d6 per two rogue levels rounded up, needs a finesse or ranged weapon, works once per turn, and needs advantage or an ally next to the target (never with disadvantage). Cunning Action from level 2. Channel Divinity is 1 use at 2, 2 at 6, 3 at 18. Arcane Recovery restores slot levels equal to half the wizard level rounded up. Existing battle code is unchanged; per-level uses and once-per-turn flags need saved state (#39).

## Issue #78: superiority dice (rules only)

`FSuperiorityRules`: the Battle Master's die is a d8 (d10 from 10, d12 from 18) with 4 dice from level 3 (5 from 7, 6 from 15), refreshed on a short rest (see `FRestRules`). Maneuver riders come with an attack and never cost the Action. `PrecisionHitOutOf` gives the exact hit chance with the die added, out of 400 x die sides. Maneuver save DC is 8 + proficiency bonus + Strength or Dexterity modifier. The existing Precision Strike is unchanged; spent dice per rest need saved state (#39).

## Save schema 5 (#57, #59, #61, #64, #69/#70, #71)

Save version 5 adds required fields; versions 1-4 are rejected with no migration. Per combatant: `Conditions` (Prone, Poisoned, Blinded, Frightened, Restrained, Grappled with source and rounds left), `bReactionAvailable` (spent by an opportunity attack when an enemy leaves reach, restored at the owner's turn start), `SlotsByLevel` (empty, or slots for spell levels 1-3 within the full-caster table for the unit's level; level 1 mirrors `SpellSlots`), `MainHand` (SRD weapon index or -1 for the fixed damage die), `ArmorWorn` (SRD armor index or -1 for the fixed AC) and `bShieldEquipped` (a two-handed weapon with a shield is rejected). Per encounter: `DifficultCells` (entering one costs two movement; routes take the cheapest path). Per battle: `AttacksRemaining` (Extra Attack: a level 5+ Fighter gets one more weapon attack after the Attack action). Weapon hits roll the weapon's dice (versatile two-handed without a shield) and attacks target armor AC from `FArmorRules` with the unit's Dexterity; legacy units roll and defend exactly as before. Equipped units attack at weapon reach or long range (long range at disadvantage), finesse and ranged weapons use DEX, `Ammunition` (required, 0-99) is spent per shot, and unproficient armor gives attack disadvantage, blocks battle spells and, under the STR requirement, costs 10 ft. Barbarian, Monk, Paladin and Ranger gain Extra Attack at level 5, and Sneak Attack lands once per turn (`SneakAttackUsedBy`, required). `FCombatCommand.SlotLevel` (not saved; 0 = lowest open slot) upcasts Magic Missile (+1 dart per level) and Cure Wounds (+1d8); Burning Hands (15-ft cone, DEX save) and Thunderwave (15-ft cube, CON save, 10-ft push on a failure) roll once and halve on a save. Shove and Grapple (party commands) run the Athletics vs. Athletics/Acrobatics contest and apply Prone or Grappled. Generated encounters place rubble (difficult terrain) east of each obstacle, dress enemies in Leather or Studded Leather (their DEX bonus now adds 0-2 AC) and start the first scout of a quiet route Prone. The HUD forecast shows armor-sourced AC, conditions, difficult-terrain move cost and which enemies a previewed move provokes. Conditions also include Incapacitated, Stunned, Paralyzed and Unconscious: incapacitating conditions remove actions, bonus actions and reactions; paralyzed, unconscious and petrified creatures have speed 0 (Stunned and Incapacitated keep their speed per SRD 5.2.1; with no action they can still move), unconscious creatures also fall Prone, stunned, paralyzed and unconscious creatures auto-fail STR/DEX saves and grant attack advantage (hits within 5 ft on paralyzed/unconscious are critical); Restrained gives DEX save disadvantage; Poisoned and Frightened give ability-check disadvantage; Grappled sets speed 0; a frightened creature cannot step closer to a visible source. The forecast previews Burning Hands and Thunderwave (save-fail odds, slot dice, every unit in the area) and Shove/Grapple (exact contest odds); keys Y/U/I/W pick them and `=` cycles the upcast slot. Catalog builds wear trained SRD armor (and shield) chosen so armor AC equals the published profile AC with no penalties (druids non-metal); weapons are not equipped (they would change attack bonus and damage). Every class has prebuilt ability scores whose key ability is +3 (Monk/Ranger DEX, Druid WIS, Bard/Sorcerer/Warlock CHA, Artificer INT, Barbarian/Paladin STR).

## #79 Leveling, multiclass and SRD defaults

- **Advancement:** SRD XP table (milestone XP awarded per encounter), max level 20. HP = full hit die at level 1 + average (die/2+1+CON) per later level. Starting parties are deliberately at SRD level-1 HP (roughly half the old pools); balance is tuned through encounters, not HP.
- **Multiclass:** one secondary class. It adds HP/hit dice, its signature feature (Fighter: Second Wind; Cleric: Cure Wounds + 2 slots; Wizard: Magic Missile + 2 slots, +Shield) and every playable level 1-5 feature of that class at the secondary class level (FeatureKit uses combined class levels). Multiclass Cure Wounds heals 1d8 + the hero's WIS modifier (minimum 1) and may target self.
- **ASI:** at class levels 4/8/12/16/19 (Fighter also 6/14, Rogue also 10).
- **Action economy tags:** every command and feature is tagged [Action], [Bonus Action], [Reaction] or [Free].
- **Reactions:** opportunity attacks (always on) and Shield (party Wizard, 1 slot, +5 AC until its next turn, auto-cast only when it turns a hit into a miss; never spends kit-reserved slots). Counterspell is not implemented (no enemy casters).
- **SRD dice:** basic damage has no level bonus; Cure Wounds 1d8+3; Second Wind 1d10+fighter level; Magic Missile 3×(1d4+1); cantrips scale at 5/11/17.
- **Superiority dice (default on):** Battle Master spends a d8 added to the attack roll (Precision Attack). 2 dice before Fighter 3, 4 from Fighter 3. `bSuperiorityDice=false` restores the legacy flat +2 hit/damage.
- **Sneak Attack (default on):** ceil(level/2)d6, once per turn. `bScaledSneakAttack=false` keeps 1d6.
- **Defaults:** Extra Attack (Fighter/Barbarian/Monk/Paladin/Ranger 5), opportunity attacks and advantage/disadvantage always on; death saves off (`bDeathSaves=false`).
- **Level 1-5 features:** `FClassFeatureRules::FeaturesThrough` lists them per class with tags; `*` in the summary marks features listed but not yet playable: Monk Martial Arts, Monk Deflect Missiles and Paladin Channel Divinity. All others (Fighting Style, Action Surge, Channel Divinity, Bardic Inspiration, Wild Shape, Ki, Sorcery Points, Uncanny Dodge, Reckless Attack, Arcane Recovery at the rest point, ...) are playable with a tagged HUD button, tooltip and saved uses.
- **Menus:** `FBuildCatalog::ProgressSheet` provides level/XP, classes, HP/hit dice, resources, features and level-up/ASI flags for the HUD.
- **Reward info sheet:** `FFeatureKit::RewardSheet` lists the selected survivor's features as `Name [Tag]: usage`; the reward panel shows it on one line. Kept HUD-independent so the #110 menu rework can rebase over it.
- **Deferred:** encounter tuning for halved HP (changing enemy stats shifts seeded-run tests such as Run.ManySeedsConnectedActionableAndVaried and Schema5.CampTreat; needs maintainer sign-off). Counterspell: N/A, no enemy casters.

## SRD enemies, Counterspell and action-tag audit

- **Attribution:** enemy stat blocks (Bandit, Thug, Scout, Acolyte) and Counterspell come from the System Reference Document 5.1 by Wizards of the Coast LLC, licensed under CC-BY-4.0 (https://creativecommons.org/licenses/by/4.0/legalcode). Bandit Archer is the SRD Bandit using its SRD light crossbow.
- **Blocks (MonsterRules):** HP is the SRD average, AC/attack/damage/scores from the block. Thug has Multiattack (two mace attacks, bludgeoning). Acolyte casts Sacred Flame (DEX save vs DC 12, 1d8 radiant on a failure). Enemy initiative is the DEX modifier.
- **Counterspell:** Wizard or Sorcerer 5, Reaction, within 12 cells with line of sight, spends one slot; enemy cantrips are stopped with no check. Automatic when ready.
- **Enemy AI:** moves to a firing cell, Dashes when still out of reach, uses every Multiattack attack, a bloodied Scout or Bandit Archer next to a hero Disengages and falls back, a foe with no reachable hero Hides (ranged) or Dodges (melee). Moving away provokes opportunity attacks normally.
- **Encounter tuning (party HP stays halved):** the opening fight has no archer; the archer is a Bandit Archer until two encounters are cleared, then a Scout; a dangerous route's lead foe is a Thug only after one cleared encounter; with 3+ foes the middle one is an Acolyte. The old per-encounter enemy HP scaling is removed.
- **Deliberate test changes:** Schema5 now checks each enemy's armor and AC against its block; CombatBattleTest's sealed enemy now Dodges before ending its turn (2 results); MonsterRulesTest covers blocks and the actions above. Restore now accepts a dead active unit once combat has ended (a foe can die to an opportunity attack on its own turn).

| Audit finding | Where | Status |
| --- | --- | --- |
| Superiority Die (Precision Attack) showed Action; it is a free rider on an attack | BuildCatalog ability panel | Fixed (f0a156b) |
| Defense fighting style also applies when unarmored (ArmorWorn >= 0); SRD needs armor | Fighting-style AC | Reported, not changed |
| Unmodelled SRD level 1-5 features | Class feature table | Listed, marked * |
## Verified level gating (SRD 5.1, levels 1-5) — audit 2026-11-02

Class features unlock on **class level**, never character level. Proficiency bonus and cantrip dice use **total character level**. Locked by `Project.Expedition.Rules.LevelGating`.

| Class | 1 | 2 | 3 | 4 | 5 |
|---|---|---|---|---|---|
| Fighter | Fighting Style, Second Wind | Action Surge | Archetype: Champion Improved Critical / Superiority Dice* | ASI | Extra Attack |
| Rogue | Expertise, Sneak Attack 1d6 | Cunning Action | Thief: Fast Hands; Sneak 2d6 | ASI | Uncanny Dodge; Sneak 3d6 |
| Cleric | Spellcasting, Life: Disciple of Life | Channel Divinity (1) | 2nd-level slots | ASI | Destroy Undead; 3rd-level slots |
| Wizard | Spellcasting, Arcane Recovery | Evocation: Sculpt Spells | 2nd-level slots | ASI | 3rd-level slots (Counterspell) |
| Barbarian | Rage, Unarmored Defense | Reckless Attack, Danger Sense | Berserker: Frenzy | ASI | Extra Attack, Fast Movement |
| Monk | Martial Arts, Unarmored Defense | Ki, Unarmored Movement | Deflect Missiles, Open Hand Technique | ASI, Slow Fall | Extra Attack, Stunning Strike |
| Paladin | Divine Sense, Lay on Hands | Fighting Style, Spellcasting, Divine Smite | Divine Health, Oath Channel Divinity | ASI | Extra Attack, 2nd-level slots |
| Ranger | Favored Enemy, Natural Explorer | Fighting Style, Spellcasting | Hunter: Colossus Slayer, Primeval Awareness | ASI | Extra Attack, 2nd-level slots |
| Sorcerer | Spellcasting, Draconic: Dragon Resilience | Font of Magic (Sorcery Points) | Metamagic | ASI | 3rd-level slots (Counterspell) |
| Warlock (SRD 5.2.1) | Pact Magic (1 slot, short-rest recovery), Eldritch Invocations (1) | Magical Cunning, 3 invocations, 2 slots | Fiend: Dark One's Blessing, Fiend Spells*; 2nd-level slots | ASI | 5 invocations; 3rd-level pact slots |
| Druid | Spellcasting | Wild Shape, Land: Natural Recovery | 2nd-level slots | ASI | 3rd-level slots |
| Bard (SRD 5.2.1) | Spellcasting (4 prepared), Bardic Inspiration d6 | Expertise*, Jack of All Trades (Initiative) | Lore: Bonus Proficiencies*, Cutting Words [Reaction] | ASI | Font of Inspiration (short rest), d8; 9 prepared |

\* Deviation by directive: Superiority Dice (Battle Master) replace Precision Strike and are listed beside the Champion row.

Multiclassing: Extra Attack does not stack (max of the classes); Sneak Attack dice and Cunning Action use Rogue class level; spell slots use the class's own table when only one class has Spellcasting, otherwise the SRD multiclass spellcaster table (full = levels, Paladin/Ranger = half rounded up per SRD 5.2.1, Warlock Pact Magic separate). Half casters single-class (SRD 5.2.1): Spellcasting at level 1, slots = the full-caster row at ceil(level / 2) (Paladin/Ranger 1: two 1st-level slots). Changed deliberately from the 5.1 rule (no slots at 1, round down); tests updated: LevelGating (Paladin 1 slots, Paladin 1/Wizard 1 = caster 2), LevelRules (Paladin 5 = caster 3), SpellCatalog.LevelGating (Paladin 1 prepares Bless).

## Spells (SRD 5.2.1 catalog)

Source: System Reference Document 5.2.1 ("SRD 5.2.1") by Wizards of the Coast LLC, available at https://dnd.wizards.com/resources/systems-reference-document and licensed under CC BY 4.0 (https://creativecommons.org/licenses/by/4.0/legalcode). See [SRD source notes](srd-source-notes.md). The machine-readable copy comes from 5e-bits/5e-database (`src/2024/en/5e-SRD-Spells.json`, MIT). `tools/spells/normalize_srd521.py` reads save type, half-on-save, area, healing and upcast dice from the SRD 5.2.1 spell text, and `tools/spells/gen_spell_catalog.py` writes `Source/DnDDiceRoll/Private/Rules/SpellCatalogData.inl`. Changes from the SRD: values are reduced to catalog fields, and the wording is not reproduced.

All 339 SRD 5.2.1 spells are in `FSpellCatalogRules`. Each spell is tagged with its level, school, class lists, casting time (Action, Bonus Action, Reaction or Extended), range, components, duration, concentration, ritual, area, attack or save, dice and damage type, upcast or cantrip scaling, conditions and modifier. Class lists gate learning by class level, using the slot tables for each class's own level when multiclassing. Concentration also comes from the catalog. Menus show the tag line, for example `3rd-level evocation | Sorcerer, Wizard | Action | 150 feet | V, S, M | Instantaneous`.

**Status:**

- **Mechanics:** dice, a save plus a condition, or a hand-modelled modifier are stored in the catalog.
- **Partial:** only the first effect is modelled (Prismatic Spray, Prismatic Wall, Storm of Vengeance). Ice Knife, Ice Storm, Flame Strike and Meteor Swarm now keep both damage types; Fire Shield, Spirit Guardians and Wall of Thorns model the caster's chosen (first) type and count as Mechanics.
- **DataOnly:** tags only. The menu marks these spells "Effect not yet modelled".

**Combat casting (`ECombatCommandType::CastSpell` + `FCombatCommand::SpellId`):** `FSpellCatalogRules::CombatResolvable` picks the Mechanics spells with an Action or Bonus Action cast whose effect the battle can resolve: damage by melee/ranged spell attack or save (plus Magic Missile), all healing, and save-based conditions. `FCombatBattle::CastableSpells` and `SpellTargets` expose the menu and legal targets. A cast checks the class list and class level (primary or multiclass), armor, the action or bonus action, and the slot (lowest open slot at or above the spell level, or the exact upcast level). It uses the class spellcasting ability for the save DC and attack bonus, range in 5-ft cells with line of sight (Touch and target-less Self = 1 cell), and area templates (cones/lines/Self cubes from the caster, spheres/cylinders at the aim point, ranged cubes as spheres of half the side). Areas hit every creature inside except the caster, allies included. Attack spells crit on a natural 20 (dice doubled); saves take half or none per the catalog; damage goes through resistance/vulnerability/immunity and protection. Concentration spells replace the caster's previous concentration, and their conditions (RoundsLeft -1) end when concentration breaks (failed CON save or 0 HP); other spell conditions last one round. The eight older dedicated commands stay as they were.

Heroes cap at level 5 in battle (slots up to 3rd level), so 4th- and 5th-level spells are resolvable by the catalog but not castable yet.

> The two tables below are the original Phase 0 snapshot. Current coverage is in **Spell status (cantrips to 4th level)** at the end of this file.

| Level | Castable in battle (level 5 hero) |
| --- | --- |
| Cantrip | 11: Acid Splash, Chill Touch, Eldritch Blast, Fire Bolt, Poison Spray, Produce Flame, Ray of Frost, Sacred Flame, Shocking Grasp, Starry Wisp, Vicious Mockery |
| 1 | 17: Animal Friendship, Burning Hands, Charm Person, Color Spray, Command, Cure Wounds, Dissonant Whispers, Entangle, Grease, Guiding Bolt, Healing Word, Ice Knife, Inflict Wounds, Magic Missile, Ray of Sickness, Sleep, Thunderwave |
| 2 | 13: Acid Arrow, Flame Blade, Flaming Sphere, Heat Metal, Hold Person, Mind Spike, Moonbeam, Phantasmal Force, Scorching Ray, Shatter, Spiritual Weapon, Suggestion, Web |
| 3 | 11: Bestow Curse, Call Lightning, Fear, Fireball, Hypnotic Pattern, Lightning Bolt, Mass Healing Word, Sleet Storm, Spirit Guardians, Stinking Cloud, Vampiric Touch |

**Combat gaps:** no forecast for CastSpell; the one-leveled-spell-per-turn rule, prepared/known limits and the 1-minute duration cap are not enforced; Ice Knife's cold burst hits only the target; multi-beam spells (Eldritch Blast, Scorching Ray) resolve as one attack; zones and auras (Moonbeam, Spirit Guardians, Web, Sleet Storm) resolve as a single pulse; Charmed, Deafened, Invisible and Petrified have no combat condition yet (Charm Person, Animal Friendship, Suggestion only spend the slot); no creature-type checks; Modifier and buff spells are not castable; kit builds (Bard, Druid, Warlock and others) cast leveled spells only from their kit slot pool.

| Level | Mechanics | Partial | DataOnly |
| --- | --- | --- | --- |
| Cantrip | 11 | 0 | 16 |
| 1 | 34 | 0 | 23 |
| 2 | 21 | 0 | 36 |
| 3 | 16 | 0 | 26 |
| 4 | 15 | 0 | 19 |
| 5 | 15 | 0 | 23 |
| 6 | 14 | 0 | 17 |
| 7 | 6 | 1 | 13 |
| 8 | 9 | 0 | 8 |
| 9 | 5 | 2 | 9 |
| **Total** | **146** | **3** | **190** |

**Combat-wired changes:**

- **Cure Wounds:** now Abjuration. It heals 2d8 + modifier, plus 2d8 per slot level above 1st.
- **Healing Word:** now Abjuration. It heals 2d4 + modifier, plus 2d4 per slot level above 1st.

**Adaptations:**

- **Slot pool:** the run-layer level-1 slot pool stays capped at 2, and only builds with a slot cap use it. Sorcerer, Warlock, Bard and similar kits spend kit uses instead. The per-level SRD slots are in `SlotsByLevel`.
- **Restore validation:** the check now compares against `FBuildCatalog::SlotCap` instead of a hard-coded Cleric/Wizard role list.
- **Engine cap (`FClassFeatureRules::EngineLevelCap = 5`):** levels 6-20 exist as data only (`DataFeaturesThrough`, `FWarlockRules`, `FBardRules`), locked by `Rules.LevelGating` data gates: Warlock Mystic Arcanum 11/13/15/17, Eldritch Master 20, Contact Patron 9; Bard Magical Discoveries 6, Countercharm 7, Expertise 9, Magical Secrets 10, Superior Inspiration 18, Words of Creation 20.

### Warlock and Bard (SRD 5.2.1) status

| Feature | Lvl | Tag | Status |
|---|---|---|---|
| Pact Magic short-rest recovery | W1 | Free | Playable (only when no other slot-casting class; one shared pool) |
| Eldritch Invocations | W1 | Free | Count/prereqs enforced; Agonizing Blast, Armor of Shadows, Eldritch Mind, Pact of the Blade playable; others data only |
| Magical Cunning | W2 | Free | Playable at the rest point, once per long rest |
| Fiend: Dark One's Blessing | W3 | Free | Playable (kill or foe drop within 10 ft) |
| Fiend: Fiend Spells | W3 | - | Data only |
| Bardic Inspiration d6/d8/d10/d12 | B1 | Bonus | Playable |
| Expertise, Lore Bonus Proficiencies | B2/B3 | Free | Expertise wired into `FProficiencyRules` (2 at B2, 4 at B9); Lore Bonus Proficiencies data only |
| Jack of All Trades | B2 | Free | Playable on Initiative |
| Cutting Words | B3 | Reaction | Playable (auto when it can turn a hit into a miss) |
| Font of Inspiration | B5 | Free | Short-rest recharge playable; slot-to-use conversion not modelled |
| Song of Rest | - | - | Not in SRD 5.2.1 (removed) |
| Pact Boon | - | - | Now the Pact of the Blade/Chain/Tome invocations |

### SRD 5.1 → SRD 5.2.1 spell differences (generated)

- SRD 5.1: 319 spells; SRD 5.2.1: 339 spells (this catalog).
- Not in SRD 5.2.1 (dropped from the catalog): Branding Smite, Feeblemind.
- New in SRD 5.2.1: Aura of Life, Befuddlement, Charm Monster, Chromatic Orb, Dissonant Whispers, Divine Smite, Dragon's Breath, Elementalism, Ensnaring Strike, Hex, Ice Knife, Mind Spike, Phantasmal Force, Power Word Heal, Ray of Sickness, Searing Smite, Shining Smite, Sorcerous Burst, Starry Wisp, Summon Dragon, Tsunami, Vitriolic Sphere.
- Subclass spell tags (Life, Devotion, Fiend, Land) are not on SRD 5.2.1 spell entries; they are subclass features, so the catalog subclass column is empty.

| Spell | Change (5.1 → 5.2.1) |
| --- | --- |
| Acid Splash | school Conjuration → Evocation |
| Aid | adds Bard, Druid, Ranger |
| Animal Shapes | concentration True → False |
| Antipathy/Sympathy | adds Bard |
| Arcane Eye | drops Cleric |
| Arcane Hand | adds Sorcerer |
| Augury | adds Druid, Wizard |
| Bane | adds Warlock |
| Barkskin | concentration True → False |
| Blindness/Deafness | school Necromancy → Transmutation |
| Color Spray | adds Bard |
| Command | adds Bard |
| Cone of Cold | adds Druid |
| Conjure Fey | drops Warlock |
| Contingency | school Evocation → Abjuration |
| Continual Flame | adds Druid |
| Create Food and Water | drops Druid |
| Cure Wounds | school Evocation → Abjuration |
| Dancing Lights | school Evocation → Illusion |
| Demiplane | adds Sorcerer |
| Detect Magic | adds Warlock |
| Dispel Magic | adds Ranger |
| Divination | adds Cleric, Wizard |
| Divine Favor | school Evocation → Transmutation; concentration True → False |
| Dominate Beast | adds Ranger |
| Earthquake | school Evocation → Transmutation |
| Enhance Ability | adds Ranger, Wizard |
| Enlarge/Reduce | adds Bard, Druid |
| Entangle | adds Ranger |
| Enthrall | concentration False → True |
| Etherealness | school Transmutation → Conjuration |
| Faerie Fire | adds Bard |
| Fire Shield | adds Druid, Sorcerer |
| Flame Blade | adds Sorcerer |
| Flaming Sphere | adds Sorcerer |
| Flesh to Stone | adds Druid, Sorcerer; drops Warlock |
| Forcecage | concentration False → True |
| Freezing Sphere | adds Sorcerer |
| Gate | adds Warlock |
| Gentle Repose | adds Paladin |
| Giant Insect | school Transmutation → Conjuration |
| Glibness | school Transmutation → Enchantment |
| Goodberry | school Transmutation → Conjuration |
| Grease | adds Sorcerer |
| Greater Restoration | adds Paladin, Ranger |
| Gust of Wind | adds Ranger |
| Hallow | school Evocation → Abjuration |
| Heal | school Evocation → Abjuration |
| Healing Word | school Evocation → Abjuration |
| Heroes' Feast | adds Bard |
| Hideous Laughter | adds Warlock |
| Incendiary Cloud | adds Druid |
| Magic Weapon | concentration True → False; adds Ranger, Sorcerer |
| Mass Cure Wounds | school Conjuration → Abjuration |
| Mass Heal | school Conjuration → Abjuration |
| Mass Healing Word | school Evocation → Abjuration; adds Bard |
| Mass Suggestion | drops Warlock |
| Meld into Stone | adds Druid, Ranger |
| Message | adds Druid |
| Mirror Image | adds Bard |
| Mislead | adds Warlock |
| Phantasmal Killer | adds Bard |
| Planar Binding | adds Warlock |
| Poison Spray | school Conjuration → Necromancy |
| Prayer of Healing | school Evocation → Abjuration; adds Paladin |
| Prismatic Spray | adds Bard |
| Prismatic Wall | adds Bard |
| Protection from Evil and Good | concentration True → False; adds Druid |
| Reincarnate | school Transmutation → Necromancy |
| Resilient Sphere | school Evocation → Abjuration |
| Revivify | school Conjuration → Necromancy; adds Druid, Ranger |
| Sending | school Evocation → Divination |
| Shatter | drops Warlock |
| Sleep | concentration False → True |
| Slow | adds Bard |
| Spare the Dying | adds Druid |
| Speak with Animals | adds Warlock |
| Speak with Dead | adds Wizard |
| Spiritual Weapon | concentration False → True |
| Stoneskin | school Abjuration → Transmutation |
| Sunbeam | adds Cleric |
| Sunburst | adds Cleric |
| Symbol | adds Druid |
| Telepathic Bond | adds Bard |
| Teleportation Circle | adds Warlock |
| True Strike | concentration True → False |
| Vampiric Touch | adds Sorcerer |
| Warding Bond | adds Paladin |
| Weird | adds Warlock |

### Spell table

| Spell | Lvl | School | Classes | Subclass lists | Cast | Conc. | Status |
| --- | --- | --- | --- | --- | --- | --- | --- |
| Acid Splash | C | Evocation | Sorcerer, Wizard |  | Action |  | Mechanics |
| Chill Touch | C | Necromancy | Sorcerer, Warlock, Wizard |  | Action |  | Mechanics |
| Dancing Lights | C | Illusion | Bard, Sorcerer, Wizard |  | Action | Yes | DataOnly |
| Druidcraft | C | Transmutation | Druid |  | Action |  | DataOnly |
| Eldritch Blast | C | Evocation | Warlock |  | Action |  | Mechanics |
| Elementalism | C | Transmutation | Druid, Sorcerer, Wizard |  | Action |  | DataOnly |
| Fire Bolt | C | Evocation | Sorcerer, Wizard |  | Action |  | Mechanics |
| Guidance | C | Divination | Cleric, Druid |  | Action | Yes | DataOnly |
| Light | C | Evocation | Bard, Cleric, Sorcerer, Wizard |  | Action |  | DataOnly |
| Mage Hand | C | Conjuration | Bard, Sorcerer, Warlock, Wizard |  | Action |  | DataOnly |
| Mending | C | Transmutation | Bard, Cleric, Druid, Sorcerer, Wizard |  | Extended |  | DataOnly |
| Message | C | Transmutation | Bard, Druid, Sorcerer, Wizard |  | Action |  | DataOnly |
| Minor Illusion | C | Illusion | Bard, Sorcerer, Warlock, Wizard |  | Action |  | DataOnly |
| Poison Spray | C | Necromancy | Druid, Sorcerer, Warlock, Wizard |  | Action |  | Mechanics |
| Prestidigitation | C | Transmutation | Bard, Sorcerer, Warlock, Wizard |  | Action |  | DataOnly |
| Produce Flame | C | Conjuration | Druid |  | Bonus |  | Mechanics |
| Ray of Frost | C | Evocation | Sorcerer, Wizard |  | Action |  | Mechanics |
| Resistance | C | Abjuration | Cleric, Druid |  | Action | Yes | DataOnly |
| Sacred Flame | C | Evocation | Cleric |  | Action |  | Mechanics |
| Shillelagh | C | Transmutation | Druid |  | Bonus |  | DataOnly |
| Shocking Grasp | C | Evocation | Sorcerer, Wizard |  | Action |  | Mechanics |
| Sorcerous Burst | C | Evocation | Sorcerer |  | Action |  | DataOnly |
| Spare the Dying | C | Necromancy | Cleric, Druid |  | Action |  | DataOnly |
| Starry Wisp | C | Evocation | Bard, Druid |  | Action |  | Mechanics |
| Thaumaturgy | C | Transmutation | Cleric |  | Action |  | DataOnly |
| True Strike | C | Divination | Bard, Sorcerer, Warlock, Wizard |  | Action |  | DataOnly |
| Vicious Mockery | C | Enchantment | Bard |  | Action |  | Mechanics |
| Alarm | 1 | Abjuration | Ranger, Wizard |  | Extended (ritual) |  | DataOnly |
| Animal Friendship | 1 | Enchantment | Bard, Druid, Ranger |  | Action |  | Mechanics |
| Bane | 1 | Enchantment | Bard, Cleric, Warlock |  | Action | Yes | Mechanics |
| Bless | 1 | Enchantment | Cleric, Paladin |  | Action | Yes | Mechanics |
| Burning Hands | 1 | Evocation | Sorcerer, Wizard |  | Action |  | Mechanics |
| Charm Person | 1 | Enchantment | Bard, Druid, Sorcerer, Warlock, Wizard |  | Action |  | Mechanics |
| Chromatic Orb | 1 | Evocation | Sorcerer, Wizard |  | Action |  | DataOnly |
| Color Spray | 1 | Illusion | Bard, Sorcerer, Wizard |  | Action |  | Mechanics |
| Command | 1 | Enchantment | Bard, Cleric, Paladin |  | Action |  | Mechanics |
| Comprehend Languages | 1 | Divination | Bard, Sorcerer, Warlock, Wizard |  | Action (ritual) |  | DataOnly |
| Create or Destroy Water | 1 | Transmutation | Cleric, Druid |  | Action |  | DataOnly |
| Cure Wounds | 1 | Abjuration | Bard, Cleric, Druid, Paladin, Ranger |  | Action |  | Mechanics |
| Detect Evil and Good | 1 | Divination | Cleric, Paladin |  | Action | Yes | DataOnly |
| Detect Magic | 1 | Divination | Bard, Cleric, Druid, Paladin, Ranger, Sorcerer, Warlock, Wizard |  | Action (ritual) | Yes | DataOnly |
| Detect Poison and Disease | 1 | Divination | Cleric, Druid, Paladin, Ranger |  | Action (ritual) | Yes | DataOnly |
| Disguise Self | 1 | Illusion | Bard, Sorcerer, Wizard |  | Action |  | DataOnly |
| Dissonant Whispers | 1 | Enchantment | Bard |  | Action |  | Mechanics |
| Divine Favor | 1 | Transmutation | Paladin |  | Bonus |  | Mechanics |
| Divine Smite | 1 | Evocation | Paladin |  | Bonus |  | Mechanics |
| Ensnaring Strike | 1 | Conjuration | Ranger |  | Bonus | Yes | Mechanics |
| Entangle | 1 | Conjuration | Druid, Ranger |  | Action | Yes | Mechanics |
| Expeditious Retreat | 1 | Transmutation | Sorcerer, Warlock, Wizard |  | Bonus | Yes | DataOnly |
| Faerie Fire | 1 | Evocation | Bard, Druid |  | Action | Yes | Mechanics |
| False Life | 1 | Necromancy | Sorcerer, Wizard |  | Action |  | DataOnly |
| Feather Fall | 1 | Transmutation | Bard, Sorcerer, Wizard |  | Reaction |  | DataOnly |
| Find Familiar | 1 | Conjuration | Wizard |  | Extended (ritual) |  | DataOnly |
| Floating Disk | 1 | Conjuration | Wizard |  | Action (ritual) |  | DataOnly |
| Fog Cloud | 1 | Conjuration | Druid, Ranger, Sorcerer, Wizard |  | Action | Yes | DataOnly |
| Goodberry | 1 | Conjuration | Druid, Ranger |  | Action |  | DataOnly |
| Grease | 1 | Conjuration | Sorcerer, Wizard |  | Action |  | Mechanics |
| Guiding Bolt | 1 | Evocation | Cleric |  | Action |  | Mechanics |
| Healing Word | 1 | Abjuration | Bard, Cleric, Druid |  | Bonus |  | Mechanics |
| Hellish Rebuke | 1 | Evocation | Warlock |  | Reaction |  | Mechanics |
| Heroism | 1 | Enchantment | Bard, Paladin |  | Action | Yes | Mechanics |
| Hex | 1 | Enchantment | Warlock |  | Bonus | Yes | Mechanics |
| Hideous Laughter | 1 | Enchantment | Bard, Warlock, Wizard |  | Action | Yes | DataOnly |
| Hunter's Mark | 1 | Divination | Ranger |  | Bonus | Yes | Mechanics |
| Ice Knife | 1 | Conjuration | Druid, Sorcerer, Wizard |  | Action |  | Mechanics |
| Identify | 1 | Divination | Bard, Wizard |  | Extended (ritual) |  | DataOnly |
| Illusory Script | 1 | Illusion | Bard, Warlock, Wizard |  | Extended (ritual) |  | DataOnly |
| Inflict Wounds | 1 | Necromancy | Cleric |  | Action |  | Mechanics |
| Jump | 1 | Transmutation | Druid, Ranger, Sorcerer, Wizard |  | Bonus |  | DataOnly |
| Longstrider | 1 | Transmutation | Bard, Druid, Ranger, Wizard |  | Action |  | Mechanics |
| Mage Armor | 1 | Abjuration | Sorcerer, Wizard |  | Action |  | Mechanics |
| Magic Missile | 1 | Evocation | Sorcerer, Wizard |  | Action |  | Mechanics |
| Protection from Evil and Good | 1 | Abjuration | Cleric, Druid, Paladin, Warlock, Wizard |  | Action |  | Mechanics |
| Purify Food and Drink | 1 | Transmutation | Cleric, Druid, Paladin |  | Action (ritual) |  | DataOnly |
| Ray of Sickness | 1 | Necromancy | Sorcerer, Wizard |  | Action |  | Mechanics |
| Sanctuary | 1 | Abjuration | Cleric |  | Bonus |  | Mechanics |
| Searing Smite | 1 | Evocation | Paladin |  | Bonus |  | Mechanics |
| Shield | 1 | Abjuration | Sorcerer, Wizard |  | Reaction |  | Mechanics |
| Shield of Faith | 1 | Abjuration | Cleric, Paladin |  | Bonus | Yes | Mechanics |
| Silent Image | 1 | Illusion | Bard, Sorcerer, Wizard |  | Action | Yes | DataOnly |
| Sleep | 1 | Enchantment | Bard, Sorcerer, Wizard |  | Action | Yes | Mechanics |
| Speak with Animals | 1 | Divination | Bard, Druid, Ranger, Warlock |  | Action (ritual) |  | DataOnly |
| Thunderwave | 1 | Evocation | Bard, Druid, Sorcerer, Wizard |  | Action |  | Mechanics |
| Unseen Servant | 1 | Conjuration | Bard, Warlock, Wizard |  | Action (ritual) |  | DataOnly |
| Acid Arrow | 2 | Evocation | Wizard |  | Action |  | Mechanics |
| Aid | 2 | Abjuration | Bard, Cleric, Druid, Paladin, Ranger |  | Action |  | Mechanics |
| Alter Self | 2 | Transmutation | Sorcerer, Wizard |  | Action | Yes | DataOnly |
| Animal Messenger | 2 | Enchantment | Bard, Druid, Ranger |  | Action (ritual) |  | DataOnly |
| Arcane Lock | 2 | Abjuration | Wizard |  | Action |  | DataOnly |
| Arcanist's Magic Aura | 2 | Illusion | Wizard |  | Action |  | DataOnly |
| Augury | 2 | Divination | Cleric, Druid, Wizard |  | Extended (ritual) |  | DataOnly |
| Barkskin | 2 | Transmutation | Druid, Ranger |  | Bonus |  | Mechanics |
| Blindness/Deafness | 2 | Transmutation | Bard, Cleric, Sorcerer, Wizard |  | Action |  | DataOnly |
| Blur | 2 | Illusion | Sorcerer, Wizard |  | Action | Yes | Mechanics |
| Calm Emotions | 2 | Enchantment | Bard, Cleric |  | Action | Yes | DataOnly |
| Continual Flame | 2 | Evocation | Cleric, Druid, Wizard |  | Action |  | DataOnly |
| Darkness | 2 | Evocation | Sorcerer, Warlock, Wizard |  | Action | Yes | DataOnly |
| Darkvision | 2 | Transmutation | Druid, Ranger, Sorcerer, Wizard |  | Action |  | DataOnly |
| Detect Thoughts | 2 | Divination | Bard, Sorcerer, Wizard |  | Action | Yes | DataOnly |
| Dragon's Breath | 2 | Transmutation | Sorcerer, Wizard |  | Bonus | Yes | DataOnly |
| Enhance Ability | 2 | Transmutation | Bard, Cleric, Druid, Ranger, Sorcerer, Wizard |  | Action | Yes | DataOnly |
| Enlarge/Reduce | 2 | Transmutation | Bard, Druid, Sorcerer, Wizard |  | Action | Yes | Mechanics |
| Enthrall | 2 | Enchantment | Bard, Warlock |  | Action | Yes | DataOnly |
| Find Steed | 2 | Conjuration | Paladin |  | Action |  | DataOnly |
| Find Traps | 2 | Divination | Cleric, Druid, Ranger |  | Action |  | DataOnly |
| Flame Blade | 2 | Evocation | Druid, Sorcerer |  | Bonus | Yes | Mechanics |
| Flaming Sphere | 2 | Conjuration | Druid, Sorcerer, Wizard |  | Action | Yes | Mechanics |
| Gentle Repose | 2 | Necromancy | Cleric, Paladin, Wizard |  | Action (ritual) |  | DataOnly |
| Gust of Wind | 2 | Evocation | Druid, Ranger, Sorcerer, Wizard |  | Action | Yes | DataOnly |
| Heat Metal | 2 | Transmutation | Bard, Druid |  | Action | Yes | Mechanics |
| Hold Person | 2 | Enchantment | Bard, Cleric, Druid, Sorcerer, Warlock, Wizard |  | Action | Yes | Mechanics |
| Invisibility | 2 | Illusion | Bard, Sorcerer, Warlock, Wizard |  | Action | Yes | DataOnly |
| Knock | 2 | Transmutation | Bard, Sorcerer, Wizard |  | Action |  | DataOnly |
| Lesser Restoration | 2 | Abjuration | Bard, Cleric, Druid, Paladin, Ranger |  | Bonus |  | DataOnly |
| Levitate | 2 | Transmutation | Sorcerer, Wizard |  | Action | Yes | DataOnly |
| Locate Animals or Plants | 2 | Divination | Bard, Druid, Ranger |  | Action (ritual) |  | DataOnly |
| Locate Object | 2 | Divination | Bard, Cleric, Druid, Paladin, Ranger, Wizard |  | Action | Yes | DataOnly |
| Magic Mouth | 2 | Illusion | Bard, Wizard |  | Extended (ritual) |  | DataOnly |
| Magic Weapon | 2 | Transmutation | Paladin, Ranger, Sorcerer, Wizard |  | Bonus |  | Mechanics |
| Mind Spike | 2 | Divination | Sorcerer, Warlock, Wizard |  | Action | Yes | Mechanics |
| Mirror Image | 2 | Illusion | Bard, Sorcerer, Warlock, Wizard |  | Action |  | Mechanics |
| Misty Step | 2 | Conjuration | Sorcerer, Warlock, Wizard |  | Bonus |  | DataOnly |
| Moonbeam | 2 | Evocation | Druid |  | Action | Yes | Mechanics |
| Pass without Trace | 2 | Abjuration | Druid, Ranger |  | Action | Yes | DataOnly |
| Phantasmal Force | 2 | Illusion | Bard, Sorcerer, Wizard |  | Action | Yes | Mechanics |
| Prayer of Healing | 2 | Abjuration | Cleric, Paladin |  | Extended |  | DataOnly |
| Protection from Poison | 2 | Abjuration | Cleric, Druid, Paladin, Ranger |  | Action |  | DataOnly |
| Ray of Enfeeblement | 2 | Necromancy | Warlock, Wizard |  | Action | Yes | DataOnly |
| Rope Trick | 2 | Transmutation | Wizard |  | Action |  | DataOnly |
| Scorching Ray | 2 | Evocation | Sorcerer, Wizard |  | Action |  | Mechanics |
| See Invisibility | 2 | Divination | Bard, Sorcerer, Wizard |  | Action |  | DataOnly |
| Shatter | 2 | Evocation | Bard, Sorcerer, Wizard |  | Action |  | Mechanics |
| Shining Smite | 2 | Transmutation | Paladin |  | Bonus | Yes | Mechanics |
| Silence | 2 | Illusion | Bard, Cleric, Ranger |  | Action (ritual) | Yes | DataOnly |
| Spider Climb | 2 | Transmutation | Sorcerer, Warlock, Wizard |  | Action | Yes | DataOnly |
| Spike Growth | 2 | Transmutation | Druid, Ranger |  | Action | Yes | Mechanics |
| Spiritual Weapon | 2 | Evocation | Cleric |  | Bonus | Yes | Mechanics |
| Suggestion | 2 | Enchantment | Bard, Sorcerer, Warlock, Wizard |  | Action | Yes | Mechanics |
| Warding Bond | 2 | Abjuration | Cleric, Paladin |  | Action |  | DataOnly |
| Web | 2 | Conjuration | Sorcerer, Wizard |  | Action | Yes | Mechanics |
| Zone of Truth | 2 | Enchantment | Bard, Cleric, Paladin |  | Action |  | DataOnly |
| Animate Dead | 3 | Necromancy | Cleric, Wizard |  | Extended |  | DataOnly |
| Beacon of Hope | 3 | Abjuration | Cleric |  | Action | Yes | DataOnly |
| Bestow Curse | 3 | Necromancy | Bard, Cleric, Wizard |  | Action | Yes | Mechanics |
| Blink | 3 | Transmutation | Sorcerer, Wizard |  | Action |  | DataOnly |
| Call Lightning | 3 | Conjuration | Druid |  | Action | Yes | Mechanics |
| Clairvoyance | 3 | Divination | Bard, Cleric, Sorcerer, Wizard |  | Extended | Yes | DataOnly |
| Conjure Animals | 3 | Conjuration | Druid, Ranger |  | Action | Yes | Mechanics |
| Counterspell | 3 | Abjuration | Sorcerer, Warlock, Wizard |  | Reaction |  | DataOnly |
| Create Food and Water | 3 | Conjuration | Cleric, Paladin |  | Action |  | DataOnly |
| Daylight | 3 | Evocation | Cleric, Druid, Paladin, Ranger, Sorcerer |  | Action |  | DataOnly |
| Dispel Magic | 3 | Abjuration | Bard, Cleric, Druid, Paladin, Ranger, Sorcerer, Warlock, Wizard |  | Action |  | DataOnly |
| Fear | 3 | Illusion | Bard, Sorcerer, Warlock, Wizard |  | Action | Yes | Mechanics |
| Fireball | 3 | Evocation | Sorcerer, Wizard |  | Action |  | Mechanics |
| Fly | 3 | Transmutation | Sorcerer, Warlock, Wizard |  | Action | Yes | DataOnly |
| Gaseous Form | 3 | Transmutation | Sorcerer, Warlock, Wizard |  | Action | Yes | DataOnly |
| Glyph of Warding | 3 | Abjuration | Bard, Cleric, Wizard |  | Extended |  | DataOnly |
| Haste | 3 | Transmutation | Sorcerer, Wizard |  | Action | Yes | Mechanics |
| Hypnotic Pattern | 3 | Illusion | Bard, Sorcerer, Warlock, Wizard |  | Action | Yes | Mechanics |
| Lightning Bolt | 3 | Evocation | Sorcerer, Wizard |  | Action |  | Mechanics |
| Magic Circle | 3 | Abjuration | Cleric, Paladin, Warlock, Wizard |  | Extended |  | DataOnly |
| Major Image | 3 | Illusion | Bard, Sorcerer, Warlock, Wizard |  | Action | Yes | DataOnly |
| Mass Healing Word | 3 | Abjuration | Bard, Cleric |  | Bonus |  | Mechanics |
| Meld into Stone | 3 | Transmutation | Cleric, Druid, Ranger |  | Action (ritual) |  | Mechanics |
| Nondetection | 3 | Abjuration | Bard, Ranger, Wizard |  | Action |  | DataOnly |
| Phantom Steed | 3 | Illusion | Wizard |  | Extended (ritual) |  | DataOnly |
| Plant Growth | 3 | Transmutation | Bard, Druid, Ranger |  | Extended |  | DataOnly |
| Protection from Energy | 3 | Abjuration | Cleric, Druid, Ranger, Sorcerer, Wizard |  | Action | Yes | DataOnly |
| Remove Curse | 3 | Abjuration | Cleric, Paladin, Warlock, Wizard |  | Action |  | DataOnly |
| Revivify | 3 | Necromancy | Cleric, Druid, Paladin, Ranger |  | Action |  | DataOnly |
| Sending | 3 | Divination | Bard, Cleric, Wizard |  | Action |  | DataOnly |
| Sleet Storm | 3 | Conjuration | Druid, Sorcerer, Wizard |  | Action | Yes | Mechanics |
| Slow | 3 | Transmutation | Bard, Sorcerer, Wizard |  | Action | Yes | Mechanics |
| Speak with Dead | 3 | Necromancy | Bard, Cleric, Wizard |  | Action |  | DataOnly |
| Speak with Plants | 3 | Transmutation | Bard, Druid, Ranger |  | Action |  | DataOnly |
| Spirit Guardians | 3 | Conjuration | Cleric |  | Action | Yes | Mechanics |
| Stinking Cloud | 3 | Conjuration | Bard, Sorcerer, Wizard |  | Action | Yes | Mechanics |
| Tiny Hut | 3 | Evocation | Bard, Wizard |  | Extended (ritual) |  | DataOnly |
| Tongues | 3 | Divination | Bard, Cleric, Sorcerer, Warlock, Wizard |  | Action |  | DataOnly |
| Vampiric Touch | 3 | Necromancy | Sorcerer, Warlock, Wizard |  | Action | Yes | Mechanics |
| Water Breathing | 3 | Transmutation | Druid, Ranger, Sorcerer, Wizard |  | Action (ritual) |  | DataOnly |
| Water Walk | 3 | Transmutation | Cleric, Druid, Ranger, Sorcerer |  | Action (ritual) |  | DataOnly |
| Wind Wall | 3 | Evocation | Druid, Ranger |  | Action | Yes | Mechanics |
| Arcane Eye | 4 | Divination | Wizard |  | Action | Yes | DataOnly |
| Aura of Life | 4 | Abjuration | Cleric, Paladin |  | Action | Yes | DataOnly |
| Banishment | 4 | Abjuration | Cleric, Paladin, Sorcerer, Warlock, Wizard |  | Action | Yes | Mechanics |
| Black Tentacles | 4 | Conjuration | Wizard |  | Action | Yes | Mechanics |
| Blight | 4 | Necromancy | Druid, Sorcerer, Warlock, Wizard |  | Action |  | Mechanics |
| Charm Monster | 4 | Enchantment | Bard, Druid, Sorcerer, Warlock, Wizard |  | Action |  | Mechanics |
| Compulsion | 4 | Enchantment | Bard |  | Action | Yes | Mechanics |
| Confusion | 4 | Enchantment | Bard, Druid, Sorcerer, Wizard |  | Action | Yes | DataOnly |
| Conjure Minor Elementals | 4 | Conjuration | Druid, Wizard |  | Action | Yes | DataOnly |
| Conjure Woodland Beings | 4 | Conjuration | Druid, Ranger |  | Action | Yes | Mechanics |
| Control Water | 4 | Transmutation | Cleric, Druid, Wizard |  | Action | Yes | Mechanics |
| Death Ward | 4 | Abjuration | Cleric, Paladin |  | Action |  | DataOnly |
| Dimension Door | 4 | Conjuration | Bard, Sorcerer, Warlock, Wizard |  | Action |  | Mechanics |
| Divination | 4 | Divination | Cleric, Druid, Wizard |  | Action (ritual) |  | DataOnly |
| Dominate Beast | 4 | Enchantment | Druid, Ranger, Sorcerer |  | Action | Yes | Mechanics |
| Fabricate | 4 | Transmutation | Wizard |  | Extended |  | DataOnly |
| Faithful Hound | 4 | Conjuration | Wizard |  | Action |  | Mechanics |
| Fire Shield | 4 | Evocation | Druid, Sorcerer, Wizard |  | Action |  | Mechanics |
| Freedom of Movement | 4 | Abjuration | Bard, Cleric, Druid, Ranger |  | Action |  | DataOnly |
| Giant Insect | 4 | Conjuration | Druid |  | Action | Yes | DataOnly |
| Greater Invisibility | 4 | Illusion | Bard, Sorcerer, Wizard |  | Action | Yes | DataOnly |
| Guardian of Faith | 4 | Conjuration | Cleric |  | Action |  | DataOnly |
| Hallucinatory Terrain | 4 | Illusion | Bard, Druid, Warlock, Wizard |  | Extended |  | DataOnly |
| Ice Storm | 4 | Evocation | Druid, Sorcerer, Wizard |  | Action |  | Mechanics |
| Locate Creature | 4 | Divination | Bard, Cleric, Druid, Paladin, Ranger, Wizard |  | Action | Yes | DataOnly |
| Phantasmal Killer | 4 | Illusion | Bard, Wizard |  | Action | Yes | Mechanics |
| Polymorph | 4 | Transmutation | Bard, Druid, Sorcerer, Wizard |  | Action | Yes | DataOnly |
| Private Sanctum | 4 | Abjuration | Wizard |  | Extended |  | DataOnly |
| Resilient Sphere | 4 | Abjuration | Wizard |  | Action | Yes | DataOnly |
| Secret Chest | 4 | Conjuration | Wizard |  | Action |  | DataOnly |
| Stone Shape | 4 | Transmutation | Cleric, Druid, Wizard |  | Action |  | DataOnly |
| Stoneskin | 4 | Transmutation | Druid, Ranger, Sorcerer, Wizard |  | Action | Yes | DataOnly |
| Vitriolic Sphere | 4 | Evocation | Sorcerer, Wizard |  | Action |  | Mechanics |
| Wall of Fire | 4 | Evocation | Druid, Sorcerer, Wizard |  | Action | Yes | Mechanics |
| Animate Objects | 5 | Transmutation | Bard, Sorcerer, Wizard |  | Action | Yes | DataOnly |
| Antilife Shell | 5 | Abjuration | Druid |  | Action | Yes | DataOnly |
| Arcane Hand | 5 | Evocation | Sorcerer, Wizard |  | Action | Yes | Mechanics |
| Awaken | 5 | Transmutation | Bard, Druid |  | Extended |  | DataOnly |
| Cloudkill | 5 | Conjuration | Sorcerer, Wizard |  | Action | Yes | Mechanics |
| Commune | 5 | Divination | Cleric |  | Extended (ritual) |  | DataOnly |
| Commune with Nature | 5 | Divination | Druid, Ranger |  | Extended (ritual) |  | DataOnly |
| Cone of Cold | 5 | Evocation | Druid, Sorcerer, Wizard |  | Action |  | Mechanics |
| Conjure Elemental | 5 | Conjuration | Druid, Wizard |  | Action | Yes | Mechanics |
| Contact Other Plane | 5 | Divination | Warlock, Wizard |  | Extended (ritual) |  | Mechanics |
| Contagion | 5 | Necromancy | Cleric, Druid |  | Action |  | Mechanics |
| Creation | 5 | Illusion | Sorcerer, Wizard |  | Extended |  | DataOnly |
| Dispel Evil and Good | 5 | Abjuration | Cleric, Paladin |  | Action | Yes | DataOnly |
| Dominate Person | 5 | Enchantment | Bard, Sorcerer, Wizard |  | Action | Yes | Mechanics |
| Dream | 5 | Illusion | Bard, Warlock, Wizard |  | Extended |  | Mechanics |
| Flame Strike | 5 | Evocation | Cleric |  | Action |  | Mechanics |
| Geas | 5 | Enchantment | Bard, Cleric, Druid, Paladin, Wizard |  | Extended |  | Mechanics |
| Greater Restoration | 5 | Abjuration | Bard, Cleric, Druid, Paladin, Ranger |  | Action |  | DataOnly |
| Hallow | 5 | Abjuration | Cleric |  | Extended |  | DataOnly |
| Hold Monster | 5 | Enchantment | Bard, Sorcerer, Warlock, Wizard |  | Action | Yes | Mechanics |
| Insect Plague | 5 | Conjuration | Cleric, Druid, Sorcerer |  | Action | Yes | Mechanics |
| Legend Lore | 5 | Divination | Bard, Cleric, Wizard |  | Extended |  | DataOnly |
| Mass Cure Wounds | 5 | Abjuration | Bard, Cleric, Druid |  | Action |  | Mechanics |
| Mislead | 5 | Illusion | Bard, Warlock, Wizard |  | Action | Yes | DataOnly |
| Modify Memory | 5 | Enchantment | Bard, Wizard |  | Action | Yes | Mechanics |
| Passwall | 5 | Transmutation | Wizard |  | Action |  | DataOnly |
| Planar Binding | 5 | Abjuration | Bard, Cleric, Druid, Warlock, Wizard |  | Extended |  | DataOnly |
| Raise Dead | 5 | Necromancy | Bard, Cleric, Paladin |  | Extended |  | DataOnly |
| Reincarnate | 5 | Necromancy | Druid |  | Extended |  | DataOnly |
| Scrying | 5 | Divination | Bard, Cleric, Druid, Warlock, Wizard |  | Extended | Yes | DataOnly |
| Seeming | 5 | Illusion | Bard, Sorcerer, Wizard |  | Action |  | DataOnly |
| Summon Dragon | 5 | Conjuration | Wizard |  | Action | Yes | DataOnly |
| Telekinesis | 5 | Transmutation | Sorcerer, Wizard |  | Action | Yes | Mechanics |
| Telepathic Bond | 5 | Divination | Bard, Wizard |  | Action (ritual) |  | DataOnly |
| Teleportation Circle | 5 | Conjuration | Bard, Sorcerer, Warlock, Wizard |  | Extended |  | DataOnly |
| Tree Stride | 5 | Conjuration | Druid, Ranger |  | Action | Yes | DataOnly |
| Wall of Force | 5 | Evocation | Wizard |  | Action | Yes | DataOnly |
| Wall of Stone | 5 | Evocation | Druid, Sorcerer, Wizard |  | Action | Yes | DataOnly |
| Blade Barrier | 6 | Evocation | Cleric |  | Action | Yes | Mechanics |
| Chain Lightning | 6 | Evocation | Sorcerer, Wizard |  | Action |  | Mechanics |
| Circle of Death | 6 | Necromancy | Sorcerer, Warlock, Wizard |  | Action |  | Mechanics |
| Conjure Fey | 6 | Conjuration | Druid |  | Action | Yes | Mechanics |
| Contingency | 6 | Abjuration | Wizard |  | Extended |  | DataOnly |
| Create Undead | 6 | Necromancy | Cleric, Warlock, Wizard |  | Extended |  | DataOnly |
| Disintegrate | 6 | Transmutation | Sorcerer, Wizard |  | Action |  | DataOnly |
| Eyebite | 6 | Necromancy | Bard, Sorcerer, Warlock, Wizard |  | Action | Yes | Mechanics |
| Find the Path | 6 | Divination | Bard, Cleric, Druid |  | Extended | Yes | DataOnly |
| Flesh to Stone | 6 | Transmutation | Druid, Sorcerer, Wizard |  | Action | Yes | Mechanics |
| Forbiddance | 6 | Abjuration | Cleric |  | Extended (ritual) |  | DataOnly |
| Freezing Sphere | 6 | Evocation | Sorcerer, Wizard |  | Action |  | Mechanics |
| Globe of Invulnerability | 6 | Abjuration | Sorcerer, Wizard |  | Action | Yes | DataOnly |
| Guards and Wards | 6 | Abjuration | Bard, Wizard |  | Extended |  | DataOnly |
| Harm | 6 | Necromancy | Cleric |  | Action |  | Mechanics |
| Heal | 6 | Abjuration | Cleric, Druid |  | Action |  | DataOnly |
| Heroes' Feast | 6 | Conjuration | Bard, Cleric, Druid |  | Extended |  | DataOnly |
| Instant Summons | 6 | Conjuration | Wizard |  | Extended (ritual) |  | DataOnly |
| Irresistible Dance | 6 | Enchantment | Bard, Wizard |  | Action | Yes | Mechanics |
| Magic Jar | 6 | Necromancy | Wizard |  | Extended |  | Mechanics |
| Mass Suggestion | 6 | Enchantment | Bard, Sorcerer, Wizard |  | Action |  | Mechanics |
| Move Earth | 6 | Transmutation | Druid, Sorcerer, Wizard |  | Action | Yes | DataOnly |
| Planar Ally | 6 | Conjuration | Cleric |  | Extended |  | DataOnly |
| Programmed Illusion | 6 | Illusion | Bard, Wizard |  | Action |  | DataOnly |
| Sunbeam | 6 | Evocation | Cleric, Druid, Sorcerer, Wizard |  | Action | Yes | Mechanics |
| Transport via Plants | 6 | Conjuration | Druid |  | Action |  | DataOnly |
| True Seeing | 6 | Divination | Bard, Cleric, Sorcerer, Warlock, Wizard |  | Action |  | DataOnly |
| Wall of Ice | 6 | Evocation | Wizard |  | Action | Yes | Mechanics |
| Wall of Thorns | 6 | Conjuration | Druid |  | Action | Yes | Mechanics |
| Wind Walk | 6 | Transmutation | Druid |  | Extended |  | DataOnly |
| Word of Recall | 6 | Conjuration | Cleric |  | Action |  | DataOnly |
| Arcane Sword | 7 | Evocation | Bard, Wizard |  | Action | Yes | Mechanics |
| Conjure Celestial | 7 | Conjuration | Cleric |  | Action | Yes | Mechanics |
| Delayed Blast Fireball | 7 | Evocation | Sorcerer, Wizard |  | Action | Yes | DataOnly |
| Divine Word | 7 | Evocation | Cleric |  | Bonus |  | Mechanics |
| Etherealness | 7 | Conjuration | Bard, Cleric, Sorcerer, Warlock, Wizard |  | Action |  | DataOnly |
| Finger of Death | 7 | Necromancy | Sorcerer, Warlock, Wizard |  | Action |  | DataOnly |
| Fire Storm | 7 | Evocation | Cleric, Druid, Sorcerer |  | Action |  | Mechanics |
| Forcecage | 7 | Evocation | Bard, Warlock, Wizard |  | Action | Yes | DataOnly |
| Magnificent Mansion | 7 | Conjuration | Bard, Wizard |  | Extended |  | DataOnly |
| Mirage Arcane | 7 | Illusion | Bard, Druid, Wizard |  | Extended |  | DataOnly |
| Plane Shift | 7 | Conjuration | Cleric, Druid, Sorcerer, Warlock, Wizard |  | Action |  | DataOnly |
| Prismatic Spray | 7 | Evocation | Bard, Sorcerer, Wizard |  | Action |  | Partial |
| Project Image | 7 | Illusion | Bard, Wizard |  | Action | Yes | DataOnly |
| Regenerate | 7 | Transmutation | Bard, Cleric, Druid |  | Extended |  | DataOnly |
| Resurrection | 7 | Necromancy | Bard, Cleric |  | Extended |  | DataOnly |
| Reverse Gravity | 7 | Transmutation | Druid, Sorcerer, Wizard |  | Action | Yes | DataOnly |
| Sequester | 7 | Transmutation | Wizard |  | Action |  | DataOnly |
| Simulacrum | 7 | Illusion | Wizard |  | Extended |  | DataOnly |
| Symbol | 7 | Abjuration | Bard, Cleric, Druid, Wizard |  | Extended |  | Mechanics |
| Teleport | 7 | Conjuration | Bard, Sorcerer, Wizard |  | Action |  | Mechanics |
| Animal Shapes | 8 | Transmutation | Druid |  | Action |  | DataOnly |
| Antimagic Field | 8 | Abjuration | Cleric, Wizard |  | Action | Yes | DataOnly |
| Antipathy/Sympathy | 8 | Enchantment | Bard, Druid, Wizard |  | Extended |  | Mechanics |
| Befuddlement | 8 | Enchantment | Bard, Druid, Warlock, Wizard |  | Action |  | Mechanics |
| Clone | 8 | Necromancy | Wizard |  | Extended |  | DataOnly |
| Control Weather | 8 | Transmutation | Cleric, Druid, Wizard |  | Extended | Yes | DataOnly |
| Demiplane | 8 | Conjuration | Sorcerer, Warlock, Wizard |  | Action |  | DataOnly |
| Dominate Monster | 8 | Enchantment | Bard, Sorcerer, Warlock, Wizard |  | Action | Yes | Mechanics |
| Earthquake | 8 | Transmutation | Cleric, Druid, Sorcerer |  | Action | Yes | Mechanics |
| Glibness | 8 | Enchantment | Bard, Warlock |  | Action |  | DataOnly |
| Holy Aura | 8 | Abjuration | Cleric |  | Action | Yes | Mechanics |
| Incendiary Cloud | 8 | Conjuration | Druid, Sorcerer, Wizard |  | Action | Yes | Mechanics |
| Maze | 8 | Conjuration | Wizard |  | Action | Yes | DataOnly |
| Mind Blank | 8 | Abjuration | Bard, Wizard |  | Action |  | DataOnly |
| Power Word Stun | 8 | Enchantment | Bard, Sorcerer, Warlock, Wizard |  | Action |  | Mechanics |
| Sunburst | 8 | Evocation | Cleric, Druid, Sorcerer, Wizard |  | Action |  | Mechanics |
| Tsunami | 8 | Conjuration | Druid |  | Extended | Yes | Mechanics |
| Astral Projection | 9 | Necromancy | Cleric, Warlock, Wizard |  | Extended |  | DataOnly |
| Foresight | 9 | Divination | Bard, Druid, Warlock, Wizard |  | Extended |  | DataOnly |
| Gate | 9 | Conjuration | Cleric, Sorcerer, Warlock, Wizard |  | Action | Yes | DataOnly |
| Imprisonment | 9 | Abjuration | Warlock, Wizard |  | Extended |  | Mechanics |
| Mass Heal | 9 | Abjuration | Cleric |  | Action |  | DataOnly |
| Meteor Swarm | 9 | Evocation | Sorcerer, Wizard |  | Action |  | Mechanics |
| Power Word Heal | 9 | Enchantment | Bard, Cleric |  | Action |  | DataOnly |
| Power Word Kill | 9 | Enchantment | Bard, Sorcerer, Warlock, Wizard |  | Action |  | Mechanics |
| Prismatic Wall | 9 | Abjuration | Bard, Wizard |  | Action |  | Partial |
| Shapechange | 9 | Transmutation | Druid, Wizard |  | Action | Yes | DataOnly |
| Storm of Vengeance | 9 | Conjuration | Druid |  | Action | Yes | Partial |
| Time Stop | 9 | Transmutation | Sorcerer, Wizard |  | Action |  | DataOnly |
| True Polymorph | 9 | Transmutation | Bard, Warlock, Wizard |  | Action | Yes | DataOnly |
| True Resurrection | 9 | Necromancy | Cleric, Druid |  | Extended |  | DataOnly |
| Weird | 9 | Illusion | Warlock, Wizard |  | Action | Yes | Mechanics |
| Wish | 9 | Conjuration | Sorcerer, Wizard |  | Action |  | Mechanics |

## Spell slot tables (Phase 1, SRD 5.2.1)

- `FSpellRules::SlotsFor` is data-driven through character level 20 and slot level 9 (`TableSpellLevel`): full casters read the shared 20x9 table in `FLevelRules`; Paladin/Ranger read it at ceil(level/2) from class level 2; Warlock Pact Magic uses slot level min(ceil(L/2),5) with 1/2/3/4 slots at 1/2/11/17.
- Combatant pools (`SlotsByLevel`) hold slot levels 1..`FSpellRules::MaxSpellLevel` = 4; saves with the old 3-entry pool still load.
- Long rest (`FFeatureKit::RefillSlotTable`, called from `Refill`) fills every slot level above 1st straight from the table; 1st level stays tied to the build's flat `SpellSlots` cap.
- **Exception (by rule, not code):** with `EngineLevelCap = 5`, no class or multiclass reaches a 4th-level slot (SRD: full casters at 7, half casters at 13, Warlock at 7). 4th-level spells are castable the moment the level cap is raised; no table or pool change is needed. Slot levels 5-9 need `MaxSpellLevel` raised alongside the 20-level expansion.

## Lasting spells (Phase 2, SRD 5.2.1)

- `FSpellTacticsRules` (data tables): zone shape/trigger per spell, `DurationRounds` (1 minute = 10 rounds, 10 minutes / 1 hour capped to the fight), repeat-save list, `SpendsSlot`.
- `FActiveSpell` entries in the snapshot track zones and held effects with serial, caster, target, rounds left and concentration; saved/restored and validated.
- Zones fire on cast only where SRD says so (Moonbeam, Evard's Black Tentacles, Wall of Fire); Spirit Guardians, Conjure Animals and Conjure Woodland Beings trigger on entering or ending a turn in the area. Spike Growth: difficult terrain (step cost 2) plus 2d4 per 5 ft moved.
- Repeat saves at end of target's turn (e.g. Hold Person); Fear needs no line of sight.
- Concentration ending removes every entry the caster concentrates on; dead or missing casters and targets drop their entries.
- One spell slot per turn (`bSlotSpentThisTurn`) for catalog and legacy kit spells; cantrips are exempt.
- `ForecastSpell` is a no-RNG preview: hit chance /400 or save DC, min/max amount, affected ids and cells, concentration and duration.
- `PreparedSpells` limit is enforced in `UsableSpell` and validated at Start/Restore.
- Exception: `EngineLevelCap = 5` blocks 4th-level slots in battle, so 4th-level behaviour is tested through the rules library, not live casts.

## Conditions and creature types (Phase 3, SRD 5.2.1)

- `ECombatCondition` adds Charmed, Deafened, Invisible, Petrified. Petrified is helpless (auto-fail STR/DEX saves, attackers have advantage) and resists all damage.
- `FCreatureRules`: creature type per monster role (unknown roles are Humanoid), condition immunities and exhaustion immunity from the stat blocks.
- `FSpellEffectRules` spec table: type gates (Hold Person and Charm Person humanoids only, Animal Friendship and Dominate Beast beasts only), targets per slot, held conditions, end triggers (any damage, damage from the caster's side, hostile act), repeat saves.
- Charmed: can't attack or target the charmer (`LegalTargets`). Frightened: can't move closer to a visible source (`LegalMoves`/`PathTo`). Invisible: advantage attacking, disadvantage being attacked.
- Sleep: incapacitated, then unconscious and prone on a failed end-of-turn save; ends on damage; exhaustion-immune creatures (undead, constructs) succeed automatically. Hideous Laughter: prone, can't stand, re-save with advantage when damaged.
- An incapacitating condition ends the victim's concentration. A spell's -1 conditions end with the caster's last effect on that target.
- Not modelled: Suggestion and Dominate Beast control (the condition is held, but the AI is not driven), Command options other than Grovel/Halt, Deafened effects on verbal spells.

## Buffs and debuffs (Phase 4, SRD 5.2.1)

- AC: Shield of Faith +2, Haste +2, Slow -2, Warding Bond +1, Mage Armor 13 + DEX (unarmored only), Barkskin floor 17.
- d20: Bless +1d4 / Bane -1d4 to attacks and saves; Warding Bond +1 to saves; Haste advantage on DEX saves; Enlarge advantage on STR saves; Slow -2 on DEX saves.
- Speed: Longstrider +10 ft, Expeditious Retreat and Haste x2, Slow x0.5, Hypnotic Pattern 0. Slow removes reactions. Haste lethargy: incapacitated for 1 round when it ends.
- Damage: Divine Favor +1d4 radiant, Hex/Hunter's Mark +1d6 on the marked target, Enlarge +1d4, Magic Weapon +1 (+2 at slot 4, +3 at slot 6), Fire Shield 2d8 retaliation and warm/chill resistance, Protection from Energy (choice), Protection from Poison, Stoneskin.
- Defence: Blur (disadvantage), Faerie Fire (advantage), Sanctuary (WIS save or the attack is lost; ends on a hostile act), Mirror Image (3 duplicates, d6 >= 3 per duplicate), Protection from Evil and Good, Death Ward (drop to 1 HP once), Freedom of Movement, Heroism (immune to frightened, temp HP each turn).
- HP: Aid +5 max HP per slot above 1st (reverted at the end), False Life 2d4+4 (+5 per slot) temp HP.
- Smites: Divine Smite (+1d8 vs fiends and undead), Searing Smite (start-of-turn fire, CON save ends), Shining Smite (attackers have advantage); bonus action against a creature hit in melee this turn.
- Not modelled: Warding Bond damage sharing, Hex ability-check disadvantage, Reduce half of Enlarge/Reduce, critical doubling of smite dice, Invisibility hiding beyond the condition.
- Coverage at the end of Phase 4 (cantrips to L4 one-cast resolvable): L0 11/27, L1 34/57, L2 24/57, L3 14/42, L4 15/34 = 98/217 (was 62/217). Final numbers: see Spell status below.

## Spell effect hooks (VFX API, no art)

The rules module emits presentation data only; it never reads it back and ships no art.

- `FCombatResult::SpellEvents` — every `FSpellCastEvent` produced by an accepted `Submit`, in order. Rejected commands emit none.
- `FCombatBattle::OnSpellEvent` — optional `TFunction<void(const FSpellCastEvent&)>`, called once per event when an accepted `Submit` returns (same events, same order).
- `FCombatBattle::SpellVisuals() const` — query: one `Persist` event per spell in effect now (zone cells, caster and target cells, rounds left). Consumes no random numbers; use it after loading a save or to rebuild looping visuals.

`ESpellEventKind`: `Cast` (the spell goes off), `Trigger` (a lasting spell fires on someone: Spirit Guardians, Moonbeam entry, Blink vanishing, reactions), `Tick` (per-turn effect: Heat Metal, start/end-of-turn damage), `End` (a lasting spell ends), `Persist` (snapshot from `SpellVisuals()` only).

`FSpellCastEvent` fields: `SpellId`, `School` (ESpellSchool), `SpellLevel`, `SlotLevel`, `Tier` (cantrip damage tier 0-3), `SizeClass`, `bPersist` (still in effect after this event; keep the loop until an `End` with the same `Serial`), `AreaShape` (ESpellArea), `AreaFeet`, `Cells`, `CasterId`, `CasterCell`, `TargetIds`, `TargetCells`, `DurationRounds` (0 instant, -1 until dispelled), `Serial`.

`SizeClass` (FSpellUtilityRules::SizeClass) by spell level: 0 cantrip, 1 levels 1-2, 2 level 3, 3 levels 4-6, 4 levels 7-9.

Tests: `Project.Expedition.Spells.VisualHooks` (size classes, Fire Bolt cast event, Fireball area event, Moonbeam persist snapshot, per-turn events and End).

## Utility spells and final status (Phase 5, SRD 5.2.1)

`FSpellUtilityRules` (Private/Rules/SpellUtilityRules.cpp) classifies every cantrip-to-4th-level spell that the damage/heal/condition catalog path and the buff table (`FSpellEffectRules`) do not cover. Each row has a kind and a one-line ruling; the combat side lives in `CombatSpellUtility.cpp`.

Kinds (`ESpellUtility`):

- **Engine:** resolved by an existing engine path (zone table, held effects).
- **Reaction:** Shield, Counterspell, Absorb Elements, Hellish Rebuke, Feather Fall and similar, offered at the trigger.
- **Beams:** one attack (or dart) per beam, each aimed through `FCombatCommand::ExtraTargetIds`. Eldritch Blast 1/2/3/4 at levels 1/5/11/17, Scorching Ray 3 + 1 per slot above 2nd, Magic Missile 3 + 1 per slot above 1st.
- **ChoiceAttack:** the caster picks the damage type with `Command.Choice` (Chromatic Orb, Sorcerous Burst) or a two-part attack (Ice Knife).
- **WeaponCantrip:** a weapon attack with the spellcasting modifier (True Strike, Shillelagh).
- **Restore:** ends a state: stabilize, revive, end conditions, dispel, end curses.
- **Teleport:** moves to `Command.Destination` (Misty Step, Dimension Door).
- **Push:** forced movement (Gust of Wind).
- **Obscure:** heavily obscured zones that block sight (Fog Cloud, Darkness, the illusion images).
- **Light:** bright-light zones; Darkness and Daylight dispel each other by level.
- **Silence:** no verbal spells inside, thunder immunity.
- **Summon:** a token that acts on the caster's turn.
- **Zone, Debuff, Buff:** lasting area, single-target penalty or self/ally benefit outside the Phase 4 table.
- **Exception:** no tactical-grid equivalent; the reason is stored on the row and asserted by `Rules.SpellCoverage`.

Exception re-audit (this phase): Minor Illusion, Silent Image and Major Image now block sight as image zones (5-ft, 15-ft and 20-ft cubes); Remove Curse ends Hex and Bestow Curse on a touched creature; Blink rolls 1d6 at the end of the caster's turn and on 4-6 the caster is Ethereal (untargetable and unharmed) until its next turn starts. The spells that stay exceptions are exploration, social, ritual or object-state spells (detection, communication, scouting, long casts, object repair, shapechanging without stat blocks). Goodberry stays an exception because under 5.2.1 the eater spends its own Bonus Action and a downed creature cannot eat, and the battle has no consumable inventory. Detect Magic and Detect Evil and Good stay because every creature's position is already known on the grid.

Upcasting: `Project.Expedition.Spells.UpcastRules` checks every 1st- to 4th-level spell (dice never drop from its level to slot 9, a 4th-level slot averages more when the dice grow, extra targets per slot, beams, per-slot riders) plus SRD spot checks with a 4th-level slot (Fireball 9d6, Lightning Bolt 9d6, Burning Hands 6d6, Thunderwave 5d8, Guiding Bolt 7d6, Shatter 5d8, Inflict Wounds 5d10, Cure Wounds 8d8, Healing Word 8d4, Magic Missile 6 darts, Scorching Ray 5 rays). `Project.Expedition.Spells.UpcastBattle` casts with 3rd-level slots in battle (Magic Missile 5 darts, Burning Hands 5d6, Inflict Wounds 4d10). Live 4th-level casts wait on the engine level cap (Start rejects a 4th-level slot at level 5).

### Handled utility spells (`FSpellUtilityRules`, 54 rows)

| Lvl | Spell | Kind | Ruling |
|---|---|---|---|
| 0 | Eldritch Blast | Beams | one attack per beam; ExtraTargetIds aims each beam (empty: all at the first target via the catalog path) |
| 0 | Guidance | Buff | +1d4 to contested checks (Shove, Grapple, Hide) |
| 0 | Resistance | Buff | once per turn -1d4 damage of the Choice type |
| 0 | Sorcerous Burst | ChoiceAttack | Command.Choice = EDamageType; 8s explode up to the spellcasting modifier in extra d8s |
| 0 | Dancing Lights | Light | lights as one 10-ft bright zone |
| 0 | Light | Light | 20-ft bright light around the touched creature's cell |
| 0 | Minor Illusion | Obscure | 5-ft image of an object: blocks sight into and out of its cell (investigation and creature belief not modelled; sound option is exploration) |
| 0 | Spare the Dying | Restore | stabilizes a downed ally (15/30/60/120 ft by tier) |
| 0 | Shillelagh | WeaponCantrip | club/quarterstaff uses the spellcasting modifier and d8/d10/d12/2d6 damage |
| 0 | True Strike | WeaponCantrip | weapon attack with the spellcasting modifier; +radiant d6s from level 5 |
| 1 | Magic Missile | Beams | darts auto-hit for 1d4+1 each, split across targets |
| 1 | Chromatic Orb | ChoiceAttack | Command.Choice = EDamageType; matching d8s leap the orb up to slot-level times within 30 ft |
| 1 | Ice Knife | ChoiceAttack | 1d10 piercing attack, then a cold burst (DEX save) on the target and adjacent creatures, hit or miss |
| 1 | Ensnaring Strike | Debuff | next hit: STR save or Restrained and 1d6 piercing per turn |
| 1 | Fog Cloud | Obscure | 20-ft radius (+20 ft/slot) heavily obscured: no sight in or out |
| 1 | Silent Image | Obscure | 15-ft cube image (3x3 cells, concentration): blocks sight into and out of it |
| 1 | Hellish Rebuke | Reaction | auto: after damage from a creature within 60 ft, DEX save 2d10 fire (+1d10/slot), half on success |
| 1 | Shield | Reaction | auto: when hit by an attack it turns into a miss (+5 AC until your next turn) |
| 2 | Scorching Ray | Beams | one attack per ray, each at its own target |
| 2 | Darkvision | Buff | 150-ft darkvision |
| 2 | Enhance Ability | Buff | advantage on checks of the Choice ability |
| 2 | Levitate | Buff | aloft: out of melee reach |
| 2 | See Invisibility | Buff | ignores the Invisible condition |
| 2 | Calm Emotions | Debuff | WIS save: affected foes become indifferent (Pacified) or allies lose Charmed/Frightened |
| 2 | Ray of Enfeeblement | Debuff | CON save or melee attacks at disadvantage and -1d8 damage |
| 2 | Spike Growth | Engine | 20-ft zone, difficult terrain, 2d4 per 5 ft moved |
| 2 | Continual Flame | Light | permanent 20-ft bright light |
| 2 | Darkness | Obscure | 15-ft magical darkness: blocks sight, darkvision too; dispels light of level 2 or lower |
| 2 | Gust of Wind | Push | 60x10-ft line toward the target: STR save or pushed 15 ft; repeats on turn start in the line |
| 2 | Lesser Restoration | Restore | ends Blinded, Deafened, Paralyzed or Poisoned (Command.Choice picks) |
| 2 | Silence | Silence | 20-ft sphere: no verbal spells, thunder immunity |
| 2 | Find Steed | Summon | steed token at the caster: slam 1d8+slot radiant each caster turn (mounted combat not modelled) |
| 2 | Misty Step | Teleport | bonus action: 30 ft to Command.Destination in sight |
| 2 | Pass without Trace | Zone | 30-ft emanation: +10 to allies' Hide (Stealth) checks |
| 3 | Beacon of Hope | Buff | advantage on WIS saves and death saves; healing is maximised |
| 3 | Blink | Buff | end of each of your turns roll 1d6: 4-6 you are Ethereal (untargetable, no damage) until your next turn starts |
| 3 | Fly | Buff | 60-ft fly speed; can't be reached by ground melee |
| 3 | Conjure Animals | Engine | pack as a 10-ft zone: DEX save on enter/end turn; the pack's own movement is not modelled |
| 3 | Daylight | Light | 60-ft sunlight; dispels Darkness of level 3 or lower |
| 3 | Major Image | Obscure | 20-ft cube image (5x5 cells on the grid, concentration): blocks sight into and out of it |
| 3 | Counterspell | Reaction | auto (FFeatureKit Counterspell): CON save or the enemy spell fails; enemy casters are stat-block cantrips only |
| 3 | Dispel Magic | Restore | ends spells on the target; above the slot level needs a check DC 10 + level |
| 3 | Remove Curse | Restore | touch: ends every curse on the creature (Hex, Bestow Curse) |
| 3 | Revivify | Restore | touch: a dead ally returns with 1 HP (the 1-minute window is the battle) |
| 3 | Plant Growth | Zone | 100-ft overgrowth: 4 movement per 5 ft (the 8-hour enrichment is exploration) |
| 4 | Confusion | Debuff | 10-ft sphere WIS save; d10 behaviour each turn |
| 4 | Resilient Sphere | Debuff | DEX save: sealed sphere, untargetable and no damage in or out |
| 4 | Conjure Woodland Beings | Engine | 10-ft emanation: WIS save on enter/end turn; the Disengage bonus action is not modelled |
| 4 | Faithful Hound | Summon | hound token: a foe within 5 ft makes a DEX save or takes 4d8 force each caster turn |
| 4 | Giant Insect | Summon | spider token: moves 40 ft and bites 1d6+3+slot piercing + 1d4+slot poison each caster turn |
| 4 | Dimension Door | Teleport | 500 ft to Command.Destination; ExtraTargetIds[0] adjacent ally comes along |
| 4 | Aura of Life | Zone | 30-ft emanation: necrotic resistance; downed allies regain 1 HP at turn start |
| 4 | Conjure Minor Elementals | Zone | 15-ft emanation: caster hits on creatures in it add 2d8 of the Choice type (enemies-only difficult terrain not modelled) |
| 4 | Guardian of Faith | Zone | 10-ft guardian zone: DEX save, 20 radiant (10 on save) on enter/turn start; vanishes after 60 dealt |

### Documented exceptions (69, cantrip to 9th; 69 at levels 0-4)

| Lvl | Spell | Reason |
|---|---|---|
| 0 | Druidcraft | exploration/social utility: no combat effect on the tactical grid |
| 0 | Elementalism | exploration/social utility: no combat effect on the tactical grid |
| 0 | Mage Hand | object interaction only; the hand can't attack and there are no objects on the grid |
| 0 | Mending | 1-minute cast; repairs objects (no object state) |
| 0 | Message | exploration/social utility: no combat effect on the tactical grid |
| 0 | Prestidigitation | exploration/social utility: no combat effect on the tactical grid |
| 0 | Thaumaturgy | exploration/social utility: no combat effect on the tactical grid |
| 1 | Alarm | 1-minute ritual ward for rest scenes; nothing to trigger in a started battle |
| 1 | Comprehend Languages | exploration/social utility: no combat effect on the tactical grid |
| 1 | Create or Destroy Water | exploration/social utility: no combat effect on the tactical grid |
| 1 | Detect Evil and Good | senses locations and types only: every combatant's location is already known on the grid, so it changes nothing tactically |
| 1 | Detect Magic | senses magic auras only: no magic items or hidden glyphs on the grid; it reveals no creature |
| 1 | Detect Poison and Disease | exploration/social utility: no combat effect on the tactical grid |
| 1 | Disguise Self | exploration/social utility: no combat effect on the tactical grid |
| 1 | Feather Fall | no falling or elevation on the grid |
| 1 | Find Familiar | 1-hour ritual: can't be cast in combat; a pre-summoned familiar needs encounter setup (future) |
| 1 | Floating Disk | carries objects; no inventory weight on the grid |
| 1 | Goodberry | berries are eaten later with the eater's Bonus Action (1 HP each); no consumable inventory in battle, and a downed ally can't eat |
| 1 | Identify | exploration/social utility: no combat effect on the tactical grid |
| 1 | Illusory Script | exploration/social utility: no combat effect on the tactical grid |
| 1 | Jump | no jumping or elevation on the grid |
| 1 | Purify Food and Drink | exploration/social utility: no combat effect on the tactical grid |
| 1 | Speak with Animals | exploration/social utility: no combat effect on the tactical grid |
| 1 | Unseen Servant | object chores only; the servant can't attack and there are no objects on the grid |
| 2 | Alter Self | shape options are exploration (aquatic, disguise); natural weapons need a weapon swap (future) |
| 2 | Animal Messenger | exploration/social utility: no combat effect on the tactical grid |
| 2 | Arcane Lock | no doors or objects on the grid |
| 2 | Arcanist's Magic Aura | exploration/social utility: no combat effect on the tactical grid |
| 2 | Augury | 1-minute ritual divination |
| 2 | Detect Thoughts | information/social; the probe needs AI state (not modelled) |
| 2 | Dragon's Breath | grants the target a Magic action command each turn; party commands are class kits (future command) |
| 2 | Enthrall | Perception penalty only; no passive Perception in battle |
| 2 | Find Traps | exploration/social utility: no combat effect on the tactical grid |
| 2 | Gentle Repose | affects corpses out of combat |
| 2 | Knock | no doors or locks on the grid |
| 2 | Locate Animals or Plants | exploration/social utility: no combat effect on the tactical grid |
| 2 | Locate Object | exploration/social utility: no combat effect on the tactical grid |
| 2 | Magic Mouth | 1-minute ritual |
| 2 | Prayer of Healing | 10-minute cast: out-of-combat healing |
| 2 | Rope Trick | extradimensional refuge for rests; no elevation on the grid |
| 2 | Spider Climb | no walls or ceilings to climb on the grid |
| 2 | Zone of Truth | exploration/social utility: no combat effect on the tactical grid |
| 3 | Animate Dead | 1-minute cast; undead servants persist between battles (party roster, future) |
| 3 | Clairvoyance | 10-minute cast, remote sensing |
| 3 | Create Food and Water | exploration/social utility: no combat effect on the tactical grid |
| 3 | Gaseous Form | an escape form: no attacks, spells or objects while gaseous, and its use (squeezing through cracks) needs terrain the grid lacks |
| 3 | Glyph of Warding | 1-hour cast: a prepared trap, not a combat action |
| 3 | Magic Circle | 1-minute cast |
| 3 | Meld into Stone | no stone terrain to meld into |
| 3 | Nondetection | exploration/social utility: no combat effect on the tactical grid |
| 3 | Phantom Steed | 1-minute cast; travel mount |
| 3 | Sending | exploration/social utility: no combat effect on the tactical grid |
| 3 | Speak with Dead | exploration/social utility: no combat effect on the tactical grid |
| 3 | Speak with Plants | exploration/social utility: no combat effect on the tactical grid |
| 3 | Tiny Hut | 1-minute ritual shelter for rests |
| 3 | Tongues | exploration/social utility: no combat effect on the tactical grid |
| 3 | Water Breathing | no water on the grid |
| 3 | Water Walk | no water on the grid |
| 3 | Wind Wall | needs a wall shape and projectile deflection on lines (future area shape) |
| 4 | Arcane Eye | remote sensing |
| 4 | Control Water | no water on the grid |
| 4 | Divination | ritual divination |
| 4 | Fabricate | 10-minute crafting |
| 4 | Hallucinatory Terrain | 10-minute cast; illusory terrain needs AI belief state |
| 4 | Locate Creature | exploration/social utility: no combat effect on the tactical grid |
| 4 | Polymorph | needs a beast stat-block swap with its own HP (future summons/shapes system) |
| 4 | Private Sanctum | 10-minute cast warding an area for rests |
| 4 | Secret Chest | exploration/social utility: no combat effect on the tactical grid |
| 4 | Stone Shape | reshapes stone: no destructible or editable terrain |

### Spell status (cantrips to 4th level)

Castable = resolvable in battle by the catalog, buff, utility or engine paths (`Rules.SpellCoverage`, floors asserted). Every spell is either castable or a documented exception.

| Level | Castable | Total | Documented exceptions |
|---|---|---|---|
| 0 | 20 | 27 | 7 |
| 1 | 40 | 57 | 17 |
| 2 | 39 | 57 | 18 |
| 3 | 25 | 42 | 17 |
| 4 | 24 | 34 | 10 |
| **Total** | **148** | **217** | **69** |

4th-level spells are resolvable but need a 4th-level slot, which no hero has under `EngineLevelCap = 5`.

## Character creator (LOCAL PROPOSAL, outside the approved CONTEXT.md scope)
The creator isn't part of the approved CONTEXT.md scope. It's a self-contained local proposal for Austin to accept or drop. Prebuilt catalog heroes behave exactly as before, and a slot with an empty `Sheet` is a prebuilt.

**Rules (SRD 5.2.1, level 1):**
- **Point buy.** 27 points, scores 8–15 before bonuses, cost 8:0 … 13:5, 14:7, 15:9 (`FPointBuyRules`). Each class starts from its recommended array. There are no species or background bonuses (there is no species/background system).
- **Classes.** All 12 SRD classes show their hit die, number of skill picks, Expertise count and a level-1 feature summary (`FCharacterCreator::Classes`/`ClassInfo`). Starting HP is the hit die plus the CON modifier, minimum 1.
- **Skills.** Picks come from the class's 5.2.1 list. Expertise must be on a proficient skill and is only offered when the class grants it at level 1 (Rogue 2).
- **Colours.** Three colours (primary, secondary, accent) from the shared palette.
- **Name.** 1–24 characters: letters, plus space, `-` and `'` after the first character. Each word is capitalised automatically.
- **Kit.** The class's interim base kit until a catalog build exists.

**UI** (`FCreatorScreen` model, drawn by `AExpeditionHUD::DrawCreator`):
- Open it with "Create hero"/"Edit hero" on the title panel, or press Enter on the title.
- The panel starts below the top bar (design y 56) over a dim layer. While it's open, a full-screen blocker plus `Buttons.Reset()` stop clicks reaching the HUD underneath.
- Keyboard: Up/Down move between rows (wrapping). Left/Right change the focused row (−/+ score, class, skill cursor, colour, Confirm↔Cancel). Enter picks. Letters, space, `-` and `'` type into the name. Backspace deletes. Escape cancels.
- Keys are routed in `InputKey(const FInputKeyEventArgs&)` while the creator is open, so game bindings don't fire. The `FInputKeyParams` overload is `final` in 5.8.
- Mouse ids: `CrPlus<i>`/`CrMinus<i>`, `CrClassNext`/`CrClassPrev`, `CrSkill<i>`, `CrColor<slot>+/-`, `CrConfirm`, `CrCancel`.
- Clicking a skill cycles it: none → proficient → Expertise (if allowed) → none.

**Behaviour:**
- Confirm writes the sheet into the selected party slot and keeps the slot `Id`. If the sheet is invalid, Confirm leaves the slot untouched and shows the reason.
- Choosing a prebuilt class or subclass (`CycleClass`, Champion, Battle Master) clears the custom sheet. Cycling kit (`CycleBuild`) keeps it.
- Style: dark panels with parchment, gold and ink. The watercolour "candlelit arcane" wash (`Wash`, stepped at 12 fps) appears only on magic accents: the selected ability row, and the emblem and Confirm glow once the hero is ready.

**Save:**
- `FPartyBuildSelection::Sheet` (`FHeroSheet`) is an optional field within schema 5. The strict codec skips a missing `Sheet`, so older v5 files load as prebuilts.
- New files that contain `Sheet` can't be read by a build that predates the creator: its strict field check rejects the unknown field. If that matters, bump to schema 6 before merging.

**Risk: `AsiChoices` is a `TArray<uint8>`** (on `FHeroSheet` and the party selection). Byte arrays are the one place where `FJsonObjectConverter` output and our strict reader could disagree: a number array versus a packed or base64 form, and enum bytes being renumbered if `EAbility` order ever changes. There's no ASI at level 1 today, so the array is always empty in saves and the path is unexercised. Before levelling custom heroes, add a round-trip test with a non-empty `AsiChoices`, or store ability names as strings.

**Tests:**
- `Project.Expedition.Creator.*`: DraftValidation, CustomHeroInRun, Roster, ScreenKeyboard, ScreenSkillsAndConfirm.
- Point-buy and class-table tests from the earlier creator commits.

**Captures:** `docs/runs/character-creator/captures.md` (dev-only `-CreatorTrial=<dir>` harness; real viewport, not mocks).
