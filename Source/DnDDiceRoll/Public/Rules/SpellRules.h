#pragma once

#include "CoreMinimal.h"
#include "Rules/DamageRules.h"

enum class EAdventurerRole : uint8;

enum class ESpellSchool : uint8 { Abjuration, Conjuration, Divination, Enchantment, Evocation, Illusion, Necromancy, Transmutation };

/** In-game and near-term SRD 5.2.1 spells. Order is stable. */
enum class ESpell : uint8 { FireBolt, SacredFlame, MagicMissile, CureWounds, GuidingBolt, BurningHands, HealingWord, Thunderwave, Count };

/** Static SRD spell facts used by tooltips and rules. Nothing here is saved. */
struct DNDDICEROLL_API FSpellFacts
{
    const TCHAR* Name = TEXT("");
    /** 0 = cantrip. */
    int32 Level = 0;
    ESpellSchool School = ESpellSchool::Evocation;
    const TCHAR* CastingTime = TEXT("1 action");
    /** Range in feet; 0 = touch or self (see RangeText). */
    int32 RangeFeet = 0;
    const TCHAR* RangeText = TEXT("");
    bool bVerbal = true;
    bool bSomatic = true;
    bool bMaterial = false;
    bool bConcentration = false;
    const TCHAR* Duration = TEXT("Instantaneous");
    bool bCleric = false;
    bool bWizard = false;
    /** Base effect dice (per dart for Magic Missile). */
    int32 DieCount = 0;
    int32 DieSides = 0;
    bool bHealing = false;
    /** Ignored when bHealing is true (Cure Wounds / Healing Word restore HP; no damage type). */
    EDamageType DamageType = EDamageType::Force;
    /** Extra dice per slot level above Level (Magic Missile: extra darts). 0 = no upcast benefit. */
    int32 UpcastDicePerLevel = 0;
};

struct DNDDICEROLL_API FSpellRules
{
    static constexpr int32 MaxSpellLevel = 3;
    static const FSpellFacts& Facts(ESpell Spell);
    static const TCHAR* SchoolName(ESpellSchool School);
    /** Full-caster slot table (Cleric, Wizard) for character levels 1-5; others have none. */
    static int32 SlotsFor(EAdventurerRole Role, int32 CharacterLevel, int32 SpellLevel);
    /** Highest spell level with a slot; 0 when none. */
    static int32 MaxSlotLevel(EAdventurerRole Role, int32 CharacterLevel);
    static bool OnClassList(EAdventurerRole Role, ESpell Spell);
    /** Dice (or darts) when cast with a slot of SlotLevel; 0 when the slot is too low. Cantrips ignore SlotLevel. */
    static int32 UpcastDiceCount(ESpell Spell, int32 SlotLevel);
    /** Tooltip line: "1st-level evocation | 1 action | 120 ft | V, S | Instantaneous". */
    static FString Summary(ESpell Spell);
};
