#include "Rules/WeaponRules.h"

namespace WeaponTable
{
using namespace EWeaponProperty;
constexpr EDamageType B = EDamageType::Bludgeoning, P = EDamageType::Piercing, S = EDamageType::Slashing;
const FWeaponProfile Profiles[] = {
    {TEXT("Club"), false, false, 1, 4, B, Light},
    {TEXT("Dagger"), false, false, 1, 4, P, Finesse | Light | Thrown | Range, 20, 60},
    {TEXT("Handaxe"), false, false, 1, 6, S, Light | Thrown | Range, 20, 60},
    {TEXT("Javelin"), false, false, 1, 6, P, Thrown | Range, 30, 120},
    {TEXT("Mace"), false, false, 1, 6, B, 0},
    {TEXT("Quarterstaff"), false, false, 1, 6, B, Versatile, 0, 0, 8},
    {TEXT("Spear"), false, false, 1, 6, P, Thrown | Range | Versatile, 20, 60, 8},
    {TEXT("Light crossbow"), false, true, 1, 8, P, Ammunition | Range | Loading | TwoHanded, 80, 320},
    {TEXT("Dart"), false, true, 1, 4, P, Finesse | Thrown | Range, 20, 60},
    {TEXT("Shortbow"), false, true, 1, 6, P, Ammunition | Range | TwoHanded, 80, 320},
    {TEXT("Battleaxe"), true, false, 1, 8, S, Versatile, 0, 0, 10},
    {TEXT("Glaive"), true, false, 1, 10, S, Heavy | Reach | TwoHanded},
    {TEXT("Greataxe"), true, false, 1, 12, S, Heavy | TwoHanded},
    {TEXT("Greatsword"), true, false, 2, 6, S, Heavy | TwoHanded},
    {TEXT("Halberd"), true, false, 1, 10, S, Heavy | Reach | TwoHanded},
    {TEXT("Longsword"), true, false, 1, 8, S, Versatile, 0, 0, 10},
    {TEXT("Rapier"), true, false, 1, 8, P, Finesse},
    {TEXT("Scimitar"), true, false, 1, 6, S, Finesse | Light},
    {TEXT("Shortsword"), true, false, 1, 6, P, Finesse | Light},
    {TEXT("Warhammer"), true, false, 1, 8, B, Versatile, 0, 0, 10},
    {TEXT("Whip"), true, false, 1, 4, S, Finesse | Reach},
    {TEXT("Hand crossbow"), true, true, 1, 6, P, Ammunition | Range | Light | Loading, 30, 120},
    {TEXT("Heavy crossbow"), true, true, 1, 10, P, Ammunition | Range | Heavy | Loading | TwoHanded, 100, 400},
    {TEXT("Longbow"), true, true, 1, 8, P, Ammunition | Range | Heavy | TwoHanded, 150, 600},
};
static_assert(UE_ARRAY_COUNT(Profiles) == static_cast<int32>(EWeapon::Count), "One profile per weapon");
}

const FWeaponProfile& FWeaponRules::Profile(EWeapon Weapon)
{
    return WeaponTable::Profiles[FMath::Clamp(static_cast<int32>(Weapon), 0, static_cast<int32>(EWeapon::Count) - 1)];
}

EAbility FWeaponRules::AttackAbility(const FWeaponProfile& Weapon, const FAbilityScores& Scores)
{
    if (Weapon.Has(EWeaponProperty::Finesse)) { return Scores.Dexterity > Scores.Strength ? EAbility::Dexterity : EAbility::Strength; }
    return Weapon.bRanged ? EAbility::Dexterity : EAbility::Strength;
}

int32 FWeaponRules::ReachCells(const FWeaponProfile& Weapon)
{
    return Weapon.Has(EWeaponProperty::Reach) ? 2 : 1;
}

ERangeBand FWeaponRules::Band(const FWeaponProfile& Weapon, int32 DistanceCells, bool bThrow)
{
    if (DistanceCells < 1) { return ERangeBand::OutOfRange; }
    const bool bRangedAttack = Weapon.bRanged || (bThrow && Weapon.Has(EWeaponProperty::Thrown));
    if (!bRangedAttack) { return DistanceCells <= ReachCells(Weapon) ? ERangeBand::Melee : ERangeBand::OutOfRange; }
    const int32 Feet = DistanceCells * FeetPerCell;
    return Feet <= Weapon.NormalFeet ? ERangeBand::Normal : Feet <= Weapon.LongFeet ? ERangeBand::Long : ERangeBand::OutOfRange;
}

void FWeaponRules::DamageDice(const FWeaponProfile& Weapon, bool bTwoHanded, bool bCritical, int32& OutCount, int32& OutSides)
{
    OutSides = bTwoHanded && Weapon.Has(EWeaponProperty::Versatile) ? Weapon.VersatileSides : Weapon.DieSides;
    OutCount = Weapon.DieCount * (bCritical ? 2 : 1);
}

int32 FWeaponRules::DamageModifier(int32 AbilityModifier, bool bOffHand)
{
    return bOffHand ? FMath::Min(AbilityModifier, 0) : AbilityModifier;
}

bool FWeaponRules::CanTwoWeaponFight(const FWeaponProfile& Main, const FWeaponProfile& OffHand)
{
    return !Main.bRanged && !OffHand.bRanged && Main.Has(EWeaponProperty::Light) && OffHand.Has(EWeaponProperty::Light);
}

bool FWeaponRules::HeavyImposesDisadvantage(const FWeaponProfile& Weapon, bool bSmallCreature)
{
    return bSmallCreature && Weapon.Has(EWeaponProperty::Heavy);
}

int32 FWeaponRules::AttacksPerAction(const FWeaponProfile& Weapon, int32 AttacksAvailable)
{
    return Weapon.Has(EWeaponProperty::Loading) ? FMath::Min(AttacksAvailable, 1) : AttacksAvailable;
}

bool FWeaponRules::SpendAmmunition(const FWeaponProfile& Weapon, int32& Ammo)
{
    if (!Weapon.Has(EWeaponProperty::Ammunition)) { return true; }
    if (Ammo <= 0) { return false; }
    --Ammo;
    return true;
}

bool FWeaponRules::CanWield(const FWeaponProfile& Weapon, bool bTwoHanded, bool bShield)
{
    const bool bBothHands = Weapon.Has(EWeaponProperty::TwoHanded) || (bTwoHanded && Weapon.Has(EWeaponProperty::Versatile));
    return !(bBothHands && bShield);
}
