#include "Rules/DamageRules.h"

const TCHAR* FDamageRules::TypeName(EDamageType Type)
{
    static const TCHAR* Names[] = {TEXT("acid"), TEXT("bludgeoning"), TEXT("cold"), TEXT("fire"), TEXT("force"), TEXT("lightning"),
        TEXT("necrotic"), TEXT("piercing"), TEXT("poison"), TEXT("psychic"), TEXT("radiant"), TEXT("slashing"), TEXT("thunder")};
    return Names[static_cast<uint8>(Type)];
}

const TCHAR* FDamageRules::ResponseName(EDamageResponse Response)
{
    switch (Response)
    {
    case EDamageResponse::Resisted: return TEXT("resisted");
    case EDamageResponse::Vulnerable: return TEXT("vulnerable");
    case EDamageResponse::ResistedAndVulnerable: return TEXT("resisted, vulnerable");
    case EDamageResponse::Immune: return TEXT("immune");
    default: return TEXT("normal");
    }
}

EDamageResponse FDamageRules::Response(EDamageType Type, const FDamageDefenses& Defenses)
{
    const uint16 Bit = FDamageDefenses::Bit(Type);
    if (Defenses.Immunities & Bit) { return EDamageResponse::Immune; }
    const bool bResist = (Defenses.Resistances & Bit) != 0;
    const bool bVulnerable = (Defenses.Vulnerabilities & Bit) != 0;
    return bResist && bVulnerable ? EDamageResponse::ResistedAndVulnerable : bResist ? EDamageResponse::Resisted :
        bVulnerable ? EDamageResponse::Vulnerable : EDamageResponse::Normal;
}

int32 FDamageRules::Adjust(int32 Amount, EDamageType Type, const FDamageDefenses& Defenses)
{
    switch (Response(Type, Defenses))
    {
    case EDamageResponse::Immune: return 0;
    case EDamageResponse::Resisted: return Amount / 2;
    case EDamageResponse::Vulnerable: return Amount * 2;
    case EDamageResponse::ResistedAndVulnerable: return (Amount / 2) * 2;
    default: return Amount;
    }
}

FDamageDefenses FDamageRules::ProfileDefenses(const FString& Profile)
{
    // Subset of SRD 5.1 stat blocks; nonmagical-weapon qualifiers are treated as always applying (no magic weapons yet).
    FDamageDefenses Defenses;
    if (Profile == TEXT("Skeleton")) { Defenses.Vulnerable(EDamageType::Bludgeoning).Immune(EDamageType::Poison); }
    else if (Profile == TEXT("Zombie")) { Defenses.Immune(EDamageType::Poison); }
    else if (Profile == TEXT("Ice Mephit")) { Defenses.Vulnerable(EDamageType::Bludgeoning).Vulnerable(EDamageType::Fire).Immune(EDamageType::Cold).Immune(EDamageType::Poison); }
    else if (Profile == TEXT("Fire Elemental"))
    {
        Defenses.Resist(EDamageType::Bludgeoning).Resist(EDamageType::Piercing).Resist(EDamageType::Slashing).Immune(EDamageType::Fire).Immune(EDamageType::Poison);
    }
    return Defenses;
}
