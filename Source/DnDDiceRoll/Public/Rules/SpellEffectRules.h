#pragma once

#include "CoreMinimal.h"
#include "Rules/DamageRules.h"
#include "Rules/SpellCatalogRules.h"

/** SRD 5.2.1 creature types. */
enum class ECreatureType : uint8 { Aberration, Beast, Celestial, Construct, Dragon, Elemental, Fey, Fiend, Giant, Humanoid, Monstrosity, Ooze, Plant, Undead, Count };

/** Creature type and condition immunities by role/profile key (party, stat blocks, legacy monsters). Data-driven for new monsters. */
struct DNDDICEROLL_API FCreatureRules
{
    static uint16 Bit(ECreatureType Type) { return static_cast<uint16>(1u << static_cast<uint8>(Type)); }
    static ECreatureType TypeOf(const FString& Role);
    static const TCHAR* TypeName(ECreatureType Type);
    /** ESpellCondition bits the creature can't have (SRD stat block Condition Immunities). */
    static uint32 ConditionImmunities(const FString& Role);
    /** SRD 5.2.1 Sleep: creatures immune to Exhaustion (and creatures that don't sleep) automatically succeed. */
    static bool ImmuneToExhaustion(const FString& Role);
};

/** Who a spell effect lands on. */
enum class ESpellEffectTarget : uint8 { Foe, Ally, Self };

namespace SpellEffectFlag
{
enum : uint32
{
    EndsOnAnyDamage = 1u << 0,      // Sleep, Hypnotic Pattern
    EndsOnCasterSideDamage = 1u << 1, // Charm Person, Animal Friendship, Suggestion: you or your allies damage it
    SleepEscalates = 1u << 2,       // Sleep: failing the repeat save turns Incapacitated into Unconscious
    RepeatSave = 1u << 3,           // repeats the save at the end of each of the target's turns
    AdvantageInCombat = 1u << 4,    // Charm Person/Monster: advantage on the save while you or allies fight it
    NoCastDamage = 1u << 5,         // the catalog's dice are not dealt on casting (Web's burning, Hex, Fire Shield)
    SparesAllies = 1u << 6,         // area spell of "creatures of your choice"
    AttackersDisadvantage = 1u << 7, // Blur
    AttackersAdvantage = 1u << 8,   // Faerie Fire, Shining Smite
    Invisible = 1u << 9,            // Invisibility / Greater Invisibility
    EndsOnHostileAct = 1u << 10,    // Invisibility, Sanctuary: ends when the target attacks, casts or deals damage
    NoReactions = 1u << 11,         // Slow
    SaveOnDamage = 1u << 12,        // Hideous Laughter: repeats the save (with advantage) whenever it takes damage
    MirrorImages = 1u << 13,        // Mirror Image: three duplicates
    FireShield = 1u << 14,          // retaliation 2d8 on melee hits; Choice 0 warm (resist cold, deal fire), 1 chill
    WardEvilGood = 1u << 15,        // Protection from Evil and Good
    Lethargy = 1u << 16,            // Haste: when it ends the target loses its next turn
    MarkOnHit = 1u << 17,           // Hex, Hunter's Mark: the caster's attack hits on the target deal the extra dice
    OwnWeaponHit = 1u << 18,        // Divine Favor: the target's own weapon hits deal the extra dice
    Smite = 1u << 19,               // cast right after hitting the target with a melee weapon this turn
    SpeedZero = 1u << 20,           // Hypnotic Pattern
    CantStand = 1u << 21,           // Hideous Laughter
    DexSaveAdvantage = 1u << 22,    // Haste
    TempHpPerTurn = 1u << 23,       // Heroism: spellcasting modifier at the start of each of its turns
    ResistChoice = 1u << 24,        // Protection from Energy: Choice 0 fire, 1 cold, 2 lightning, 3 acid, 4 thunder
    StartTurnDamage = 1u << 25,     // Searing Smite: the extra dice again at the start of each target turn, then a save ends it
    Sanctuary = 1u << 26,           // attackers make a WIS save or lose the attack
    DeathWard = 1u << 27,           // first drop to 0 HP leaves 1 HP instead
    Buff = 1u << 28,                // resolved as a lasting effect (no catalog damage or condition roll)
    StrSaveAdvantage = 1u << 29,    // Enlarge
};
}

/** One spell's combat effect beyond the catalog row: conditions, creature-type limits, buffs, debuffs, riders. SRD 5.2.1 wording. */
struct FSpellEffectSpec
{
    const TCHAR* Id = TEXT("");
    ESpellEffectTarget Target = ESpellEffectTarget::Foe;
    uint32 Flags = 0;
    /** ESpellCondition bits applied on a failed save (0 = the catalog row's). */
    uint32 Conditions = 0;
    /** FCreatureRules::Bit mask of types it can affect (0 = any). */
    uint16 OnlyTypes = 0;
    /** Creatures it can target at its own level (0 = one, or the area) and extra per slot level above. */
    int32 Targets = 0;
    int32 TargetsPerSlot = 0;
    /** Conditions of untracked effects last this many of the target's turns (Command, Color Spray). */
    int32 ConditionRounds = 0;
    /** d4 added (+4 Bless) or subtracted (-4 Bane) on attack rolls and saving throws. */
    int32 RollDie = 0;
    int32 AcBonus = 0;
    /** Mage Armor: base AC 13 + DEX with no armor. */
    int32 AcBase = 0;
    /** Barkskin: AC can't be less than this. */
    int32 AcFloor = 0;
    int32 SpeedFeet = 0;
    /** Speed percent: 200 doubles (Haste), 50 halves (Slow); 0 leaves it. */
    int32 SpeedPercent = 0;
    /** Aid: max and current HP + this per slot level above 1. */
    int32 MaxHpPerSlot = 0;
    /** ESpellCondition bits the target can't have while it lasts (Heroism: Frightened; Freedom of Movement: Paralyzed, Restrained). */
    uint32 ConditionImmunity = 0;
    /** Divine Smite: HitDice more against these creature types (FCreatureRules::Bit). */
    uint16 BonusVsTypes = 0;
    /** The extra dice take the weapon's damage type (Enlarge). */
    bool bWeaponTyped = false;
    /** Flat bonus to saving throws (Warding Bond +1) and DEX saves (Slow -2). */
    int32 SaveBonus = 0;
    int32 DexSaveBonus = 0;
    /** Magic Weapon: + to weapon attack and damage rolls (+1, +2 with a 4th-level slot, +3 with 6th). */
    int32 WeaponBonus = 0;
    /** Extra dice on hits (Hex, Hunter's Mark, Divine Favor, Enlarge) or the smite/rider dice; per slot above the spell's level. */
    int32 HitDice = 0;
    int32 HitSides = 0;
    EDamageType HitType = EDamageType::Force;
    int32 HitDicePerSlot = 0;
    /** False Life: temp HP dice + flat, + per slot above. */
    int32 TempDice = 0;
    int32 TempSides = 0;
    int32 TempFlat = 0;
    int32 TempPerSlot = 0;
    /** FDamageDefenses::Bit mask of resisted damage types. */
    uint16 Resist = 0;
};

struct DNDDICEROLL_API FSpellEffectRules
{
    static int32 Num();
    static const FSpellEffectSpec& At(int32 Index);
    static const FSpellEffectSpec* Find(const FString& Id);
    /** A buff/debuff handled by the effect resolver (no catalog damage or condition roll). */
    static bool IsBuff(const FSpellEffectSpec& Spec);
    /** Creature-type requirement (Hold Person humanoids, Animal Friendship beasts). */
    static bool CanAffect(const FSpellEffectSpec& Spec, const FString& Role);
    /** Creatures the cast can target with this slot. */
    static int32 MaxTargets(const FSpellEffectSpec& Spec, int32 SpellLevel, int32 Slot);
    /** Protection from Energy choice to damage type. */
    static EDamageType EnergyChoice(int32 Choice);
};
