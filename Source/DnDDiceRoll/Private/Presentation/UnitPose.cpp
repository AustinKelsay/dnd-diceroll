#include "Presentation/UnitPose.h"

namespace ExpeditionPose
{
const TCHAR* ClipName(EUnitPose Pose)
{
    switch (Pose)
    {
    case EUnitPose::Walk: return TEXT("Walk");
    case EUnitPose::Melee: return TEXT("Melee");
    case EUnitPose::Ranged: return TEXT("Ranged");
    case EUnitPose::Cast: return TEXT("Cast");
    case EUnitPose::Hit: return TEXT("Hit");
    case EUnitPose::Downed: return TEXT("Downed");
    case EUnitPose::Death: return TEXT("Death");
    case EUnitPose::Recover: return TEXT("Recover");
    default: return TEXT("Idle");
    }
}

bool IsLooping(EUnitPose Pose) { return Pose == EUnitPose::Idle || Pose == EUnitPose::Walk; }

EUnitPose Rest(ECombatLife Life)
{
    return Life == ECombatLife::Dead ? EUnitPose::Death : Life == ECombatLife::Downed ? EUnitPose::Downed : EUnitPose::Idle;
}

EUnitPose ForCommand(ECombatCommandType Type, bool bRanged)
{
    switch (Type)
    {
    case ECombatCommandType::Move: return EUnitPose::Walk;
    case ECombatCommandType::Attack:
    case ECombatCommandType::PrecisionStrike: return bRanged ? EUnitPose::Ranged : EUnitPose::Melee;
    case ECombatCommandType::MagicMissile:
    case ECombatCommandType::CureWounds:
    case ECombatCommandType::Special1:
    case ECombatCommandType::Special2: return EUnitPose::Cast;
    default: return EUnitPose::Idle; // EndTurn, SecondWind, Potion, Rescue: no dedicated clip.
    }
}

EUnitPose ForEffect(EAbilityEffect Effect, bool bRanged)
{
    switch (Effect)
    {
    case EAbilityEffect::Strike:
    case EAbilityEffect::WeaponAttack:
    case EAbilityEffect::Area:
    case EAbilityEffect::Push: return bRanged ? EUnitPose::Ranged : EUnitPose::Melee;
    case EAbilityEffect::Rage:
    case EAbilityEffect::Protect:
    case EAbilityEffect::Surge:
    case EAbilityEffect::Healing:
    case EAbilityEffect::Missile: return EUnitPose::Cast;
    default: return EUnitPose::Idle; // Swap, Mitigation.
    }
}

EUnitPose ForDamage() { return EUnitPose::Hit; }
EUnitPose ForFall(bool bDead) { return bDead ? EUnitPose::Death : EUnitPose::Downed; }
EUnitPose ForRecovery() { return EUnitPose::Recover; }
}
