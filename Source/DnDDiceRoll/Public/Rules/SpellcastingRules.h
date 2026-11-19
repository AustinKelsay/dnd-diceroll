#pragma once

#include "CoreMinimal.h"
#include "Rules/AbilityRules.h"
#include "Rules/CheckRules.h"
#include "Rules/SpellRules.h"

enum class EAdventurerRole : uint8;

/** How a spell resolves against its target. */
enum class ESpellDelivery : uint8 { Automatic, Attack, Save, Healing };

struct DNDDICEROLL_API FSpellDelivery
{
    ESpellDelivery Kind = ESpellDelivery::Automatic;
    /** Save spells only. */
    EAbility SaveAbility = EAbility::Dexterity;
    bool bHalfOnSave = false;
};

/** SRD 5.2.1 Spellcasting Ability, spell attacks and saving-throw spells. Read-only, no RNG. */
struct DNDDICEROLL_API FSpellcastingRules
{
    static bool IsSpellcaster(EAdventurerRole Role);
    /** Wis: Cleric/Druid/Ranger. Cha: Bard/Paladin/Sorcerer/Warlock. Int: Wizard/Artificer (and non-casters). */
    static EAbility SpellcastingAbility(EAdventurerRole Role);
    /** 8 + proficiency bonus + ability modifier. */
    static int32 SaveDc(int32 ProficiencyBonus, int32 AbilityModifier);
    /** Proficiency bonus + ability modifier. */
    static int32 AttackBonus(int32 ProficiencyBonus, int32 AbilityModifier);
    static FSpellDelivery Delivery(ESpell Spell);
    /** Attack-roll hit chance in 1/400ths: natural 1 misses, natural 20 hits. */
    static int32 AttackHitOutOf400(int32 AttackBonus, int32 ArmorClass, ERollMode Mode);
    /** Chance the target fails its save, in 1/400ths. */
    static int32 SaveFailOutOf400(int32 TargetSaveModifier, int32 Dc, ERollMode Mode);
    /** Damage after the save: full on a failure; half (rounded down) or none on a success. */
    static int32 DamageAfterSave(int32 Rolled, bool bSaved, bool bHalfOnSave);
};
