#pragma once

#include "CoreMinimal.h"
#include "Rules/AbilityRules.h"
#include "Rules/DamageRules.h"

/** SRD 5.1 weapon properties as bits. */
namespace EWeaponProperty
{
    enum Type : uint16
    {
        Ammunition = 1 << 0, Finesse = 1 << 1, Heavy = 1 << 2, Light = 1 << 3, Loading = 1 << 4,
        Range = 1 << 5, Reach = 1 << 6, Thrown = 1 << 7, TwoHanded = 1 << 8, Versatile = 1 << 9
    };
}

/** SRD 5.1 weapons (Greatclub, Light hammer, Sickle, Flail, Morningstar, Lance, Maul, Pike, Trident, War pick, Blowgun, Net, Sling omitted for now). */
enum class EWeapon : uint8
{
    Club, Dagger, Handaxe, Javelin, Mace, Quarterstaff, Spear, LightCrossbow, Dart, Shortbow,
    Battleaxe, Glaive, Greataxe, Greatsword, Halberd, Longsword, Rapier, Scimitar, Shortsword, Warhammer, Whip,
    HandCrossbow, HeavyCrossbow, Longbow, Count
};

struct DNDDICEROLL_API FWeaponProfile
{
    const TCHAR* Name = TEXT("");
    bool bMartial = false;
    /** Ranged weapon (bows, crossbows, darts); thrown melee weapons are not ranged weapons. */
    bool bRanged = false;
    int32 DieCount = 1;
    int32 DieSides = 4;
    EDamageType DamageType = EDamageType::Bludgeoning;
    uint16 Properties = 0;
    /** Range property, in feet; 0 when absent. */
    int32 NormalFeet = 0;
    int32 LongFeet = 0;
    /** Versatile die when wielded two-handed; 0 when not versatile. */
    int32 VersatileSides = 0;
    bool Has(EWeaponProperty::Type Property) const { return (Properties & Property) != 0; }
};

/** Where a target sits relative to a weapon. Long range imposes disadvantage. */
enum class ERangeBand : uint8 { Melee, Normal, Long, OutOfRange };

/** Pure SRD weapon property rules on the 5-foot grid. No state and no random numbers. */
struct DNDDICEROLL_API FWeaponRules
{
    static constexpr int32 FeetPerCell = 5;
    static const FWeaponProfile& Profile(EWeapon Weapon);
    /** Finesse: higher of STR and DEX. Ranged weapons: DEX. Otherwise (including thrown melee weapons): STR. */
    static EAbility AttackAbility(const FWeaponProfile& Weapon, const FAbilityScores& Scores);
    /** Melee reach in cells: 2 with Reach, else 1. */
    static int32 ReachCells(const FWeaponProfile& Weapon);
    /** Band for an attack at a Manhattan distance; bThrow selects a thrown attack with a Thrown weapon. */
    static ERangeBand Band(const FWeaponProfile& Weapon, int32 DistanceCells, bool bThrow);
    /** Damage dice: versatile die when two-handed, dice doubled (not modifiers) on a critical. */
    static void DamageDice(const FWeaponProfile& Weapon, bool bTwoHanded, bool bCritical, int32& OutCount, int32& OutSides);
    /** Ability modifier added to damage: an off-hand light-weapon attack adds only a negative modifier. */
    static int32 DamageModifier(int32 AbilityModifier, bool bOffHand);
    /** Two-weapon fighting needs a light melee weapon in each hand. */
    static bool CanTwoWeaponFight(const FWeaponProfile& Main, const FWeaponProfile& OffHand);
    /** Heavy weapons impose disadvantage on Small creatures. */
    static bool HeavyImposesDisadvantage(const FWeaponProfile& Weapon, bool bSmallCreature);
    /** Loading: one piece of ammunition per action, bonus action or reaction, whatever the attack count. */
    static int32 AttacksPerAction(const FWeaponProfile& Weapon, int32 AttacksAvailable);
    /** Ammunition: a ranged attack needs a piece; returns false and leaves Ammo untouched when empty. */
    static bool SpendAmmunition(const FWeaponProfile& Weapon, int32& Ammo);
    /** Two-handed (or versatile used two-handed) weapons cannot be wielded with a shield. */
    static bool CanWield(const FWeaponProfile& Weapon, bool bTwoHanded, bool bShield);
};
