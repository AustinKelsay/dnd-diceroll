# Martial compact adaptations (#41)

Sole-writer source unit on `feature/37-full-roster`, fixed baseline `45b4d51586c8e44e27c13535973ef8828088f9bb`. No binary assets, editor lifecycle, MCP, packaging or integration work. Root owns fresh independent review and rendered/package acceptance.

The catalog publishes 30 implemented kits across eight classes: the 24 martial subclasses below, four Barbarian paths, and interim Cleric/Wizard kits. This is 28 approved subclasses plus two interim kits, not completion of the 70-subclass parent. `rogue.base` is removed; the familiar preset uses Thief. Class cycling retains the original Fighter/Rogue/Cleric/Wizard/Barbarian order, then appends Monk/Paladin/Ranger. All choices use the existing four-slot builder and at most two F/M specials.

These are original compact tactical adaptations, not reproductions of full tabletop features. Every new kit has its listed effects from encounter one. Each new kit owns two Kit uses shared between its two specials; each use costs one, camp restores one up to two. Turns, encounters, levels and Resume never refill the pool. Champion and Battle Master retain their previous Wind/maneuver policies, including no maneuver recovery at camp. All earned levels still add one basic damage without changing HP or spent resources. Every Rogue retains conditional Sneak Attack.

Each table entry gives action budget, range and fixed amount. All targeting requires sight and a conscious target. Healing affects another injured conscious ally; shared Rescue/potion remain available for downed allies. Protection can target self only when expressly listed. A bonus special retains the basic action. Guaranteed special damage has no critical dice; basic attacks retain normal criticals. No special creates another turn, adventurer, recruitment or resurrection.

| Build | F / first special | M / second special |
| --- | --- | --- |
| Fighter / Champion | Second Wind: bonus, self, d10+1 heal, two Wind | Passive critical 19–20; no second active special |
| Fighter / Battle Master | Second Wind as Champion | Precision Strike: action, weapon range, one maneuver, +2 hit/+2 damage; two maneuvers |
| Fighter / Eldritch Knight | Arcane Ward: bonus, self, protection 3 | Flame Arc: action, range 3, area damage 4 |
| Fighter / Psi Warrior | Protective Field: bonus, ally range 3, protection 3 | Telekinetic Thrust: action, range 3, damage 5 and push |
| Fighter / Rune Knight | Stone Rune: action, self, protection 4 | Giant Sweep: action, range 1, area damage 5 |
| Rogue / Thief | Quick Exchange: bonus, ally range 3, swap | Fast Hands: bonus, ally range 1, heal 6 |
| Rogue / Assassin | Ambush: action, enemy range 1, damage 9 | Extraction: bonus, ally range 1, swap |
| Rogue / Arcane Trickster | Illusory Ward: bonus, self, protection 3 | Force Trick: action, range 3, damage 3 and push |
| Rogue / Phantom | Wails: action, range 3, area damage 4 | Ghost Shelter: bonus, ally range 3, protection 2 |
| Rogue / Soulknife | Psychic Blade: bonus, range 4, damage 4 | Psychic Exchange: action, ally range 4, swap |
| Monk / Open Hand | Open Palm: bonus, range 1, damage 5 and push | Patient Defense: action, self, protection 3 |
| Monk / Shadow | Shadow Exchange: bonus, ally range 4, swap | Veil: action, ally range 2, protection 3 |
| Monk / Four Elements | Water Whip: action, range 3, damage 5 and push | Cinder Wave: action, range 2, area damage 4 |
| Monk / Mercy | Hand of Healing: bonus, ally range 1, heal 8 | Hand of Harm: action, range 1, damage 6 |
| Monk / Astral Self | Astral Arms: bonus, range 2, damage 5 | Astral Shelter: bonus, self, protection 3 |
| Paladin / Devotion | Sacred Shelter: action, ally range 3, protection 4 | Lay on Hands: bonus, ally range 1, heal 8 |
| Paladin / Ancients | Nature's Wrath: action, range 2, damage 5 and push | Living Ward: bonus, ally range 2, protection 3 |
| Paladin / Vengeance | Avenging Smite: bonus, range 1, damage 8 | Relentless Exchange: action, ally range 2, swap |
| Paladin / Glory | Heroic Exchange: bonus, ally range 3, swap | Inspiring Touch: action, ally range 1, heal 8 |
| Paladin / Watchers | Watcher's Rebuke: action, range 2, area damage 4 | Vigil: bonus, self, protection 3 |
| Ranger / Hunter | Volley: action, range 4, area damage 4 | Hunter's Finish: bonus, range 4, damage 4 |
| Ranger / Beast Master | Companion Pounce: action, range 3, damage 4 and push | Companion Guard: bonus, ally range 3, protection 3 |
| Ranger / Fey Wanderer | Fey Exchange: bonus, ally range 4, swap | Dreadful Strike: action, range 4, damage 6 |
| Ranger / Swarmkeeper | Gathered Swarm: bonus, range 3, area damage 3 | Driving Swarm: action, range 4, damage 5 and push |

## Effect and persistence contract

Protection belongs to its acting adventurer, targets one compatible conscious actor, and expires when its owner next begins a turn. Recasting replaces that owner's target/amount. Different owners remain independent; only the strongest incoming reduction applies, including existing Rage/Shelter. Owner down/death, target down/death, and battle termination clear the relevant effects. No temporary effect mutates the catalog profile. GuardTurns is the bounded lifetime category (one until next own turn), not a global round counter.

Exchange swaps two conscious allies' occupied cells without traversing intervening cells, spending the catalog action budget and resource, and preserving remaining ordinary movement. Push follows damage and moves a still-conscious enemy one cardinal cell away (X wins axis ties), only into an open in-bounds cell. A blocked push still commits damage. Area damage affects the selected enemy followed by adjacent conscious enemies in encounter order; walls block secondary sight. Forecast text lists affected names and actual expected HP loss without RNG. Special damage is fixed, except the pre-existing Wild Bolt discrete outcomes.

Ordered result facts contain actual HP loss, mitigation, healing and final movement cells. Playback consumes those facts once and converges to the committed snapshot under ordinary, accelerated and Finish playback. Legacy damage fields are not replayed on the generic-special path. Beast Master is an owner-issued companion command/guard, with no companion actor or separate initiative.

Value-only reflected KitUses, GuardTurns and GuardAmount extend the existing owner-held GuardTargetId. Detached restore checks catalog resource caps, ability ownership, amount/duration/target compatibility, conscious owner/target, terminal cleanup and spent-pool contradictions. Existing level/death-history, equipment and resource validation remains active. Catalog revision is 3; schema remains unpublished version 4 and checkpoint filename remains `current-v1.json`. Older catalog definitions are incompatible and use the existing explicit New Run preservation flow.

`EFighterSubclass` deliberately remains the legacy Champion/Battle Master adapter. New Fighters carry `None` there and use stable catalog IDs plus generic special facts. Existing role values 0–5 remain unchanged; Monk 6, Paladin 7 and Ranger 8 are appended.

Omissions: no spellbooks, tabletop ambush surprise subsystem, ki progression, persistent summoned creature inventory, marks, saving-throw status engine, feats or aura simulation. Identity comes from the compact combinations above: bonus versus action, self versus ally protection, exchange, push, area targeting, healing and reach. These adaptations stay within the approved compact-kit tuning authority.

## Verification record

Controlled behavioral red: `.local/automation/20261101T230254389403Z/index.json` reports Thief command acceptance with both cell-exchange assertions failing before exchange resolution. The earlier catalog-only red is `.local/automation/20261101T230123484688Z/index.json`; it is not offered as behavioral evidence.

Final checks and remaining limitations are recorded in the ledger. Native rendering/input, packaging and independent review are root-owned gates. Accepted engine scene-culling risk #32 remains unresolved. Automated pointer coverage, sound and human balance remain limitations.
