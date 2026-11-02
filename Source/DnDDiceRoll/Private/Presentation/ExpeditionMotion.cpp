#include "Presentation/ExpeditionMotion.h"
#include "Combat/CombatBattle.h"

namespace
{
float Ease(float Alpha) { Alpha = FMath::Clamp(Alpha, 0.0f, 1.0f); return Alpha * Alpha * (3.0f - 2.0f * Alpha); }

// One row per EExpeditionMotion. Tune here, not in the playback code.
//                    WindUp Strike Recover Reach  Pullback Hop
const FMotionProfile Table[] = {
    /* Melee  */ {0.65f, 0.16f, 0.14f, 0.42f, 0.12f,  0.0f},
    /* Shoot  */ {0.65f, 0.30f, 0.14f, 0.05f, 0.12f,  0.0f},
    /* Cast   */ {0.30f, 0.50f, 0.14f, 0.08f, 0.10f,  8.0f},
    /* Heal   */ {0.60f, 0.55f, 0.00f, 0.20f, 0.00f, 10.0f},
    /* Step   */ {0.00f, 0.15f, 0.00f, 0.00f, 0.00f, 16.0f},
    /* Recoil */ {0.00f, 0.38f, 0.00f, 0.16f, 0.00f,  0.0f},
    /* Dodge  */ {0.00f, 0.38f, 0.00f, 0.28f, 0.00f,  4.0f},
    /* Fall   */ {0.00f, 0.50f, 0.00f, 0.00f, 0.00f,  0.0f},
    /* Rise   */ {0.00f, 0.55f, 0.00f, 0.00f, 0.00f,  6.0f},
};
static_assert(UE_ARRAY_COUNT(Table) == static_cast<int32>(EExpeditionMotion::Count), "One motion profile per motion");
}

namespace ExpeditionMotion
{
const FMotionProfile& Profile(EExpeditionMotion Motion)
{
    return Table[FMath::Clamp(static_cast<int32>(Motion), 0, UE_ARRAY_COUNT(Table) - 1)];
}

float TotalSeconds(const FMotionProfile& Profile) { return Profile.WindUp + Profile.Strike + Profile.Recover; }
float WindUpLean(const FMotionProfile& Profile, float Alpha) { return -Profile.Pullback * Ease(Alpha); }
float StrikeLean(const FMotionProfile& Profile, float Alpha) { return FMath::Lerp(-Profile.Pullback, Profile.Reach, Ease(Alpha)); }
float Arc(float Peak, float Alpha) { return Peak * FMath::Sin(FMath::Clamp(Alpha, 0.0f, 1.0f) * PI); }

EExpeditionSpell SpellFor(uint8 CommandType, bool bWizard, bool bRanged)
{
    switch (static_cast<ECombatCommandType>(CommandType))
    {
    case ECombatCommandType::MagicMissile: return EExpeditionSpell::MagicMissile;
    case ECombatCommandType::CureWounds: return EExpeditionSpell::CureWounds;
    case ECombatCommandType::Attack: return bWizard && bRanged ? EExpeditionSpell::FireBolt : EExpeditionSpell::None;
    default: return EExpeditionSpell::None;
    }
}

FString SpellName(EExpeditionSpell Spell)
{
    switch (Spell)
    {
    case EExpeditionSpell::FireBolt: return TEXT("Fire Bolt");
    case EExpeditionSpell::MagicMissile: return TEXT("Magic Missile");
    case EExpeditionSpell::CureWounds: return TEXT("Cure Wounds");
    default: return FString();
    }
}
}