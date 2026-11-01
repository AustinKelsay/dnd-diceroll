#include "Rules/ArmorRules.h"
#include "Combat/CombatBattle.h"

namespace ArmorTable
{
    static const FArmorProfile Profiles[(int32)EArmor::Count] = {
        { TEXT("Unarmored"), EArmorCategory::None, 10, 0, false },
        { TEXT("Padded"), EArmorCategory::Light, 11, 0, true },
        { TEXT("Leather"), EArmorCategory::Light, 11, 0, false },
        { TEXT("Studded leather"), EArmorCategory::Light, 12, 0, false },
        { TEXT("Hide"), EArmorCategory::Medium, 12, 0, false },
        { TEXT("Chain shirt"), EArmorCategory::Medium, 13, 0, false },
        { TEXT("Scale mail"), EArmorCategory::Medium, 14, 0, true },
        { TEXT("Breastplate"), EArmorCategory::Medium, 14, 0, false },
        { TEXT("Half plate"), EArmorCategory::Medium, 15, 0, true },
        { TEXT("Ring mail"), EArmorCategory::Heavy, 14, 0, true },
        { TEXT("Chain mail"), EArmorCategory::Heavy, 16, 13, true },
        { TEXT("Splint"), EArmorCategory::Heavy, 17, 15, true },
        { TEXT("Plate"), EArmorCategory::Heavy, 18, 15, true },
        { TEXT("Shield"), EArmorCategory::Shield, 2, 0, false },
    };
}

const FArmorProfile& FArmorRules::Profile(EArmor Armor)
{
    const int32 Index = FMath::Clamp((int32)Armor, 0, (int32)EArmor::Count - 1);
    return ArmorTable::Profiles[Index];
}

int32 FArmorRules::DexCap(EArmorCategory Category)
{
    switch (Category)
    {
    case EArmorCategory::Medium: return 2;
    case EArmorCategory::Heavy: return 0;
    default: return -1;
    }
}

int32 FArmorRules::ArmorClass(EArmor Armor, bool bShield, int32 Dexterity)
{
    const FArmorProfile& Worn = Profile(Armor == EArmor::Shield ? EArmor::None : Armor);
    int32 Dex = FAbilityRules::Modifier(Dexterity);
    const int32 Cap = DexCap(Worn.Category);
    if (Worn.Category == EArmorCategory::Heavy) { Dex = 0; }
    else if (Cap >= 0) { Dex = FMath::Min(Dex, Cap); }
    return Worn.BaseAC + Dex + (bShield || Armor == EArmor::Shield ? Profile(EArmor::Shield).BaseAC : 0);
}

bool FArmorRules::IsProficient(EAdventurerRole Role, EArmorCategory Category)
{
    if (Category == EArmorCategory::None) { return true; }
    switch (Role)
    {
    case EAdventurerRole::Fighter: return true;
    case EAdventurerRole::Cleric: return Category != EArmorCategory::Heavy;
    case EAdventurerRole::Rogue: return Category == EArmorCategory::Light;
    default: return false;
    }
}

FArmorPenalties FArmorRules::Penalties(EAdventurerRole Role, EArmor Armor, bool bShield, int32 Strength)
{
    FArmorPenalties Out;
    const FArmorProfile& Worn = Profile(Armor);
    const bool bArmorOk = IsProficient(Role, Worn.Category);
    const bool bShieldOk = !bShield || IsProficient(Role, EArmorCategory::Shield);
    Out.bStrDexDisadvantage = Out.bCannotCastSpells = !(bArmorOk && bShieldOk);
    Out.bStealthDisadvantage = Worn.bStealthDisadvantage;
    Out.SpeedPenaltyFeet = (Worn.StrengthRequirement > 0 && Strength < Worn.StrengthRequirement) ? 10 : 0;
    return Out;
}
