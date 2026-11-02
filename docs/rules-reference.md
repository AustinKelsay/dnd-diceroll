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

Proficiency bonus is 2 + (level - 1) / 4. All 18 SRD skills map to their abilities (STR: Athletics; DEX: Acrobatics, Sleight of Hand, Stealth; INT: Arcana, History, Investigation, Nature, Religion; WIS: Animal Handling, Insight, Medicine, Perception, Survival; CHA: Deception, Intimidation, Performance, Persuasion). `FProficiencyRules::BuildCharacter` validates class skill picks against the SRD 5.1 class list and count, two background-style picks from any unchosen skill, and the Rogue's two level-1 expertise picks (a pick may be thieves' tools); any invalid pick rejects the whole build with no partial result. Expertise doubles the proficiency bonus. Saving throw proficiencies: Fighter STR/CON, Rogue DEX/INT, Cleric WIS/CHA, Wizard INT/WIS. Prebuilt choices: Fighter Athletics, Perception + Intimidation, Survival; Rogue Stealth, Sleight of Hand, Acrobatics, Deception + Insight, Investigation, expertise Stealth and thieves' tools; Cleric Medicine, Religion + Insight, History; Wizard Arcana, Investigation + History, Religion. `FCombatBattle::Character(Id)` is a const query; no save schema change.

## Issue #55: ability checks, skill checks and saving throws

`FCheckRules` resolves a d20 + modifier against a DC (1..30). Ability checks and saves have no automatic success on a natural 20 or failure on a natural 1. Advantage and disadvantage combine SRD-style: any advantage plus any disadvantage is a normal roll, however many sources. A normal roll spends one die; advantage or disadvantage spends two from the same seeded stream as combat dice. Forecasts are exact over all 400 pairs of d20 faces (shown rounded to a whole percent) and spend no random numbers. Contested checks: the actor must beat the defender; a tie keeps the status quo. `FCombatBattle::SkillCheck`/`SavingThrow` roll from the battle's random state and reject unknown or non-conscious units and out-of-range DCs without spending RNG; `SkillCheckChance`/`SavingThrowChance` are const forecasts.

## Issue #56: attack advantage and disadvantage

Attack rolls resolve advantage and disadvantage per SRD 5.1: any number of sources of each cancel to a normal roll; otherwise two d20s are rolled and the higher (advantage) or lower (disadvantage) is kept, spending exactly one extra RNG draw. The source implemented now is a ranged attack (weapon range above one cell) made while a conscious hostile is adjacent, which imposes disadvantage. Forecasts report the mode, its named sources and the exact chance out of 400 (normal 20x, advantage 400-(20-x)^2, disadvantage x^2 for x winning faces) without spending RNG. Feedback and playback show both d20s and the mode. Conditions (#57) will add further sources.

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
