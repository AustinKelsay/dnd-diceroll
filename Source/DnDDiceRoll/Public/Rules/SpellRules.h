#pragma once

#include "CoreMinimal.h"
#include "Rules/DamageRules.h"

enum class EAdventurerRole : uint8;

enum class ESpellSchool : uint8 { Abjuration, Conjuration, Divination, Enchantment, Evocation, Illusion, Necromancy, Transmutation };

/** In-game and near-term SRD 5.1 spells. Order is stable. */
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
    const TCHAR* Duration = TEXT("Instantaneous");
    bool bCleric = false;
    bool bWizard = false;
    /** Base effect dice (per dart for Magic Missile). */
    int32 DieCount = 0;
    int32 DieSides = 0;
    bool bHealing = false;
    EDamageType DamageType = EDamageType::Force;
    /** Extra dice per slot level above Level (Magic Missile: extra darts). 0 = no upcast benefit. */
    int32 UpcastDicePerLevel = 0;
};

struct DNDDICEROLL_API FSpellRules
{
    /** Slot levels held in a combatant's SlotsByLevel pool (engine-castable). Raise toward TableSpellLevel when the level cap grows past 6. */
    static constexpr int32 MaxSpellLevel = 4;
    /** Slot levels covered by the SRD slot tables (data through character level 20). */
    static constexpr int32 TableSpellLevel = 9;
    static const FSpellFacts& Facts(ESpell Spell);
    static const TCHAR* SchoolName(ESpellSchool School);
    /** SRD single-class slot table for class levels 1-20 (data-driven, slot levels 1-9): full casters, half casters (Paladin, Ranger from 2), Warlock pact slots. */
    static int32 SlotsFor(EAdventurerRole Role, int32 CharacterLevel, int32 SpellLevel);
    /** Slots for a (possibly multiclassed) hero: class levels, not character level, decide the table (SRD multiclassing). */
    static int32 HeroSlotsFor(EAdventurerRole Primary, int32 PrimaryLevels, EAdventurerRole Secondary, int32 SecondaryLevels, int32 SpellLevel);
    /** Highest spell level with a slot; 0 when none. */
    static int32 MaxSlotLevel(EAdventurerRole Role, int32 CharacterLevel);
    static bool OnClassList(EAdventurerRole Role, ESpell Spell);
    /** Dice (or darts) when cast with a slot of SlotLevel; 0 when the slot is too low. Cantrips ignore SlotLevel. */
    static int32 UpcastDiceCount(ESpell Spell, int32 SlotLevel);
    /** Tooltip line: "1st-level evocation | 1 action | 120 ft | V, S | Instantaneous". */
    static FString Summary(ESpell Spell);
};
