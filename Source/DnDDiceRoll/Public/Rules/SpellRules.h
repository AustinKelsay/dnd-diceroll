#pragma once

#include "CoreMinimal.h"
#include "Rules/DamageRules.h"

enum class EAdventurerRole : uint8;

enum class ESpellSchool : uint8 { Abjuration, Conjuration, Divination, Enchantment, Evocation, Illusion, Necromancy, Transmutation };

/** In-game and near-term SRD 5.2.1 spells. Order is stable. */
enum class ESpell : uint8 { FireBolt, SacredFlame, MagicMissile, CureWounds, GuidingBolt, BurningHands, HealingWord, Thunderwave, Shield, ShieldOfFaith, Guidance, Bless, MageArmor, RayOfFrost, ShockingGrasp, Light, InflictWounds, Bane, Sanctuary, FeatherFall, DetectMagic, Sleep, MistyStep, SpiritualWeapon, ScorchingRay, HoldPerson, Aid, Count };

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
    /** SRD 5.2.1 class lists that include this spell (tags only; Artificer included for the in-game roster). */
    bool bBard = false;
    bool bCleric = false;
    bool bDruid = false;
    bool bPaladin = false;
    bool bRanger = false;
    bool bSorcerer = false;
    bool bWarlock = false;
    bool bWizard = false;
    bool bArtificer = false;
    /** Base effect dice (per dart for Magic Missile). */
    int32 DieCount = 0;
    int32 DieSides = 0;
    bool bHealing = false;
    /** Ignored when bHealing is true (Cure Wounds / Healing Word restore HP; no damage type). */
    EDamageType DamageType = EDamageType::Force;
    /** Extra dice per slot level above Level (Magic Missile: extra darts; healing: extra dice). 0 = no upcast benefit. */
    int32 UpcastDicePerLevel = 0;
    /** Flat AC bonus while the spell lasts (Shield +5, Shield of Faith +2). */
    int32 ArmorClassBonus = 0;
    /** While active, Magic Missile deals no damage to you (Shield). */
    bool bNegatesMagicMissile = false;
    /** Flat numeric addend on each die/dart (Magic Missile +1) or Aid HP bump. */
    int32 FlatBonus = 0;
    /** Ritual tag (Detect Magic). Casting still uses the listed casting time when not cast as a ritual. */
    bool bRitual = false;
};

struct DNDDICEROLL_API FSpellRules
{
    static constexpr int32 MaxSpellLevel = 3;
    static constexpr int32 EngineLevelCap = 5;
    static const FSpellFacts& Facts(ESpell Spell);
    static const TCHAR* SchoolName(ESpellSchool School);
    /**
     * Slot count for a spell level at character level 1-5.
     * Full casters (Bard/Cleric/Druid/Sorcerer/Wizard), half casters (Paladin/Ranger),
     * Artificer (half-caster starting at 1), Warlock pact slots (count lives only at MaxSlotLevel).
     */
    static int32 SlotsFor(EAdventurerRole Role, int32 CharacterLevel, int32 SpellLevel);
    /** Highest spell level with a slot; 0 when none. Warlock: current pact slot level. */
    static int32 MaxSlotLevel(EAdventurerRole Role, int32 CharacterLevel);
    static bool OnClassList(EAdventurerRole Role, ESpell Spell);
    /** Spells on a class list with spell Level <= MaxSpellLevelFilter (0 = cantrips only when filter is 0). */
    static TArray<ESpell> SpellsOnList(EAdventurerRole Role, int32 MaxSpellLevelFilter = 9);
    /** Dice (or darts) when cast with a slot of SlotLevel; 0 when the slot is too low. Cantrips ignore SlotLevel. */
    static int32 UpcastDiceCount(ESpell Spell, int32 SlotLevel);
    /** Tooltip line: "1st-level evocation | 1 action | 120 ft | V, S | Instantaneous". */
    static FString Summary(ESpell Spell);
};