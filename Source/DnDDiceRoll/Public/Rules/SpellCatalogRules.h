#pragma once

#include "CoreMinimal.h"
#include "Rules/AbilityRules.h"
#include "Rules/DamageRules.h"
#include "Rules/SpellRules.h"
#include "Rules/StandardActionRules.h"

enum class EAdventurerRole : uint8;

/** SRD casting time tag. Extended = 1 minute or longer (not castable in a combat turn). */
enum class ESpellCastTime : uint8 { Action, BonusAction, Reaction, Extended };
enum class ESpellArea : uint8 { None, Cone, Cube, Cylinder, Line, Sphere };
enum class ESpellResolution : uint8 { None, Automatic, MeleeAttack, RangedAttack, Save };
enum class ESpellEffect : uint8 { Utility, Damage, Healing, Condition, Modifier };
/** Hand-modelled numeric riders for non-damage combat spells. */
enum class ESpellModifier : uint8 { None, RollBonusD4, RollPenaltyD4, AcBonus, AcBase, AcFloor, Speed, MaxHp, TempHpPerTurn, Ward, AttackerDisadvantage, AttackerAdvantage, BonusDamageD6, BonusDamageD4, WeaponBonus };
/** Mechanics: the catalog resolves it (damage/heal/save/attack/condition/modifier). Partial: only the first effect is modelled. DataOnly: listed, effect unimplemented. */
enum class ESpellImplementation : uint8 { Mechanics, Partial, DataOnly };
/** SRD conditions, bit index for FSrdSpell::Conditions. */
enum class ESpellCondition : uint8 { Blinded, Charmed, Deafened, Frightened, Grappled, Incapacitated, Invisible, Paralyzed, Petrified, Poisoned, Prone, Restrained, Stunned, Unconscious, Count };

namespace SpellClass
{
    enum : uint16 { Bard = 1 << 0, Cleric = 1 << 1, Druid = 1 << 2, Paladin = 1 << 3, Ranger = 1 << 4, Sorcerer = 1 << 5, Warlock = 1 << 6, Wizard = 1 << 7 };
}

/** One SRD 5.2.1 spell. Static data only; nothing here is saved. */
struct DNDDICEROLL_API FSrdSpell
{
    const TCHAR* Id;
    const TCHAR* Name;
    int32 Level;
    ESpellSchool School;
    uint16 Classes;
    /** Subclass/expanded lists (Life, Devotion, Fiend, Circle of the Land); empty when none. */
    const TCHAR* Subclasses;
    ESpellCastTime CastTime;
    const TCHAR* CastingTimeText;
    bool bRitual;
    /** Range in feet; Touch = 5, Self = 0. */
    int32 RangeFeet;
    const TCHAR* RangeText;
    bool bVerbal, bSomatic, bMaterial;
    bool bConcentration;
    const TCHAR* Duration;
    ESpellArea Area;
    int32 AreaFeet;
    ESpellResolution Resolution;
    EAbility SaveAbility;
    bool bHalfOnSave;
    ESpellEffect Effect;
    int32 DieSides;
    int32 FlatBonus;
    /** Adds the caster's spellcasting modifier. */
    bool bAddModifier;
    EDamageType DamageType;
    /** Cantrips: dice at character tiers 1/5/11/17 in [0..3]. Leveled spells: dice when cast with slot level [i]. */
    int32 Dice[10];
    uint32 Conditions;
    ESpellModifier Modifier;
    int32 ModifierValue;
    ESpellImplementation Implementation;
};

struct DNDDICEROLL_API FSpellCatalogRules
{
    static int32 Num();
    static const FSrdSpell& At(int32 Index);
    /** By slug ("fire-bolt") or display name; nullptr if not in the SRD list. */
    static const FSrdSpell* Find(const FString& IdOrName);
    static uint16 ClassBit(EAdventurerRole Role);
    static bool OnClassList(EAdventurerRole Role, const FSrdSpell& Spell);
    static bool HasCondition(const FSrdSpell& Spell, ESpellCondition Condition);
    static EActionCost ActionCost(const FSrdSpell& Spell);
    static bool CastableInCombat(const FSrdSpell& Spell) { return Spell.CastTime != ESpellCastTime::Extended; }
    /** Dice count: cantrips by character level (SRD 5/11/17 tiers), others by slot level; 0 when the slot is too low. */
    static int32 DiceCount(const FSrdSpell& Spell, int32 CharacterLevel, int32 SlotLevel);
    /** Average effect (damage or healing) rounded down, adding SpellModifier when the spell does. */
    static int32 AverageEffect(const FSrdSpell& Spell, int32 CharacterLevel, int32 SlotLevel, int32 SpellModifier);

    /** SRD class spell progression (class levels 1-20 for known/cantrips, slot tables for 1-5). */
    static int32 CantripsKnown(EAdventurerRole Role, int32 ClassLevel);
    /** Known casters (Bard, Ranger, Sorcerer, Warlock): fixed count. Prepared casters (Cleric, Druid, Paladin, Wizard): modifier + level (Paladin half), min 1. 0 = no Spellcasting yet. */
    static int32 SpellsKnownOrPrepared(EAdventurerRole Role, int32 ClassLevel, int32 AbilityModifier);
    static bool PreparesSpells(EAdventurerRole Role);
    /** Highest leveled spell this class can learn/prepare at this class level (its own slot table or pact slot level). */
    static int32 MaxLearnableLevel(EAdventurerRole Role, int32 ClassLevel);
    /** SRD: a spell can be learned/prepared only from the class list and only at a level the class has slots for. */
    static bool CanLearn(EAdventurerRole Role, int32 ClassLevel, const FSrdSpell& Spell);
    /** Multiclass: each spell is learned through one class at that class's level. */
    static bool HeroCanLearn(EAdventurerRole Primary, int32 PrimaryLevels, EAdventurerRole Secondary, int32 SecondaryLevels, const FSrdSpell& Spell);

    static FString ClassListText(uint16 Classes);
    static const TCHAR* CastTimeTag(ESpellCastTime CastTime);
    /** "3rd-level evocation | Sorcerer, Wizard | Action | 150 feet | V, S, M | Instantaneous" (+ " | Concentration", " | Ritual"). */
    static FString Tags(const FSrdSpell& Spell);
};
