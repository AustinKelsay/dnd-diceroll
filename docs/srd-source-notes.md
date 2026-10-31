# SRD source notes for implementers

Verified by the read-only primary-source rules check on 2026-10-30. These are reference facts, not a claim that every feature is implemented. The [rules reference](rules-reference.md) describes the actual supported subset and adaptations.

Licensed source: [Wizards SRD 5.2.1 PDF](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf), English release May 1, 2025, under CC BY 4.0. See [official downloads](https://www.dndbeyond.com/srd) and the PDF's page 1 for prescribed attribution. Online Basic Rules verification pages are not the reusable licensed source; see the [Creator FAQ](https://www.dndbeyond.com/creator-faq). Retain attribution, license links and identification of changes. The setting and characters are original.

## Combat facts

- Attacks use d20 plus applicable ability and proficient bonus against AC; equality hits, natural 1 misses, natural 20 hits and crits. Critical damage rolls all attack damage dice twice and adds modifiers once, including extra Sneak Attack dice when eligible.
- Initiative is a Dexterity check at combat start, highest first and retained across rounds. Deterministic stable-ID ties are our digital policy.
- A turn allows movement up to Speed and one action. A qualifying feature can grant one Bonus Action; a Reaction needs its trigger and becomes available again at the creature's next turn start.
- SRD grid squares represent five feet. Diagonal and orthogonal steps each cost five feet, difficult terrain doubles cost and filled corners cannot be cut. Our cardinal/Manhattan implementation is an explicit adaptation.

See PDF combat pp. 6–18 and [official combat verification](https://www.dndbeyond.com/sources/dnd/br-2024/playing-the-game).

## Zero HP facts and V1 decisions

The SRD distinguishes living unconscious, stable and dead. Unstable zero-HP creatures roll death saves at their turn start: 10+ succeeds, three successes stabilize, three failures kill; natural 1 gives two failures and natural 20 restores one HP. Healing or stabilization resets counters. Damage at zero gives failures (two on a critical); massive damage can kill outright. Help can stabilize with a DC 10 Wisdom (Medicine) check. Stable creatures stop death saves and regain one HP after 1d4 hours. Positive healing ends zero-HP unconsciousness; ordinary healing never revives the dead. See PDF pp. 17–18 and the [condition glossary](https://www.dndbeyond.com/sources/dnd/br-2024/rules-glossary).

V1 requires a bounded rescue window. If using a deterministic countdown instead of death saves, specify exactly when it ticks, how rescue works, and how unresolved downed allies are handled when a battle ends. Define the terminal when no conscious rescuer remains; do not silently confuse unconsciousness with actual death. Never carry an unresolved downed state into an encounter without a documented transition policy. The one-use expedition camp is also an adaptation with explicit restoration amounts, not an assumed tabletop Short Rest. Neither rescue nor camp can restore a dead adventurer.

## Small level-one role subset

These features are available reference choices, not complete class implementations. Prebuilt fixed profiles do not imply V1 leveling.

| Feature | SRD 5.2.1 fact |
| --- | --- |
| Fighter Second Wind | Bonus Action; heals self for d10 plus Fighter level; two uses at level one. Short Rest restores one use, Long Rest all. |
| Rogue Sneak Attack | Extra d6 once per turn at level one on a Finesse/Ranged hit with Advantage, or a non-incapacitated ally within five feet of the target and no Disadvantage. Part of the attack, not an extra action. Cunning Action begins at level two. |
| Cleric spellcasting | Wisdom; two level-one slots at level one; slots normally restored on Long Rest. |
| Wizard spellcasting | Intelligence; two level-one slots at level one. Arcane Recovery can restore one level-one slot after a Short Rest, once before another Long Rest. |
| Cure Wounds | Action, Touch, level-one slot; **2d8 plus spellcasting modifier** healing. |
| Fire Bolt | Action, cantrip, 120 feet; ranged spell attack for d10 Fire at level one. |
| Magic Missile | Action, level-one slot, 120 feet; three simultaneous darts at visible creatures, each d4+1 Force. No attack/save/critical. Darts may share targets. |

Class source: PDF Cleric pp. 36–37, Fighter pp. 47–48, Rogue pp. 61–62, Wizard pp. 77–78; [official class verification](https://www.dndbeyond.com/sources/dnd/br-2024/character-classes). Spell verification: [Cure Wounds](https://www.dndbeyond.com/spells/2619079-cure-wounds), [Fire Bolt](https://www.dndbeyond.com/spells/2618890-fire-bolt), [Magic Missile](https://www.dndbeyond.com/spells/2619022-magic-missile). Cantrips cost no slot. Current spellcasting permits spending only one spell slot to cast a spell per turn.

V1 may simplify action availability, ranges, spell targeting, reactions, components, conditions and recovery. Document actual choices in the player-facing rules reference and enforce costs in the authoritative command interface. Selecting a different portrait never grants an out-of-turn action.
