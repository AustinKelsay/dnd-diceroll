#pragma once

#include "CoreMinimal.h"

/** SRD 5.1 damage types. */
enum class EDamageType : uint8 { Acid, Bludgeoning, Cold, Fire, Force, Lightning, Necrotic, Piercing, Poison, Psychic, Radiant, Slashing, Thunder };

/** How a target's defenses changed one damage instance. */
enum class EDamageResponse : uint8 { Normal, Resisted, Vulnerable, ResistedAndVulnerable, Immune };

/** Resistances, vulnerabilities and immunities as one bit per damage type. Multiple sources of the same one count once. */
struct DNDDICEROLL_API FDamageDefenses
{
    uint16 Resistances = 0;
    uint16 Vulnerabilities = 0;
    uint16 Immunities = 0;
    static uint16 Bit(EDamageType Type) { return static_cast<uint16>(1u << static_cast<uint8>(Type)); }
    FDamageDefenses& Resist(EDamageType Type) { Resistances |= Bit(Type); return *this; }
    FDamageDefenses& Vulnerable(EDamageType Type) { Vulnerabilities |= Bit(Type); return *this; }
    FDamageDefenses& Immune(EDamageType Type) { Immunities |= Bit(Type); return *this; }
};

/** Pure SRD damage-defense rules. Applied after every other modifier to the damage of one instance. */
struct DNDDICEROLL_API FDamageRules
{
    static const TCHAR* TypeName(EDamageType Type);
    static const TCHAR* ResponseName(EDamageResponse Response);
    static EDamageResponse Response(EDamageType Type, const FDamageDefenses& Defenses);
    /** Immunity zeroes; resistance halves (round down); vulnerability doubles; both resist then double. */
    static int32 Adjust(int32 Amount, EDamageType Type, const FDamageDefenses& Defenses);
    /** SRD monster defenses by profile (unit Role). Profiles not in the table have none. */
    static FDamageDefenses ProfileDefenses(const FString& Profile);
};
