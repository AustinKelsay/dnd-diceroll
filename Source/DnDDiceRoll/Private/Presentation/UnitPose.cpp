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
    case EUnitPose::Brace: return TEXT("Brace");
    default: return TEXT("Idle");
    }
}

bool IsLooping(EUnitPose Pose) { return Pose == EUnitPose::Idle || Pose == EUnitPose::Walk; }

EUnitPose Rest(ECombatLife Life)
{
    return Life == ECombatLife::Dead ? EUnitPose::Death : Life == ECombatLife::Downed ? EUnitPose::Downed : EUnitPose::Idle;
}

bool IsSpellAttack(ECombatCommandType Type, EAdventurerRole Role, bool bRanged)
{
    return Type == ECombatCommandType::Attack && (Role == EAdventurerRole::Wizard || (Role == EAdventurerRole::Cleric && bRanged));
}

EUnitPose ForCommand(ECombatCommandType Type, bool bRanged, bool bSpellAttack)
{
    if (bSpellAttack && Type == ECombatCommandType::Attack) { return EUnitPose::Cast; }
    switch (Type)
    {
    case ECombatCommandType::Move: return EUnitPose::Walk;
    case ECombatCommandType::Attack:
    case ECombatCommandType::PrecisionStrike: return bRanged ? EUnitPose::Ranged : EUnitPose::Melee;
    case ECombatCommandType::MagicMissile:
    case ECombatCommandType::CureWounds: return EUnitPose::Cast;
    case ECombatCommandType::SecondWind: return EUnitPose::Brace; // a martial self-heal, not a spell
    // Special1 and Special2 depend on the ability (spell or not, attack or ward): see ForAbility. A command with no ability facts has no clip.
    default: return EUnitPose::Idle; // EndTurn, Special1, Special2, Potion, Rescue, Grapple, Shove, Escape: no dedicated clip.
    }
}

EUnitPose ForEffect(EAbilityEffect Effect, bool bRanged, bool bSpell)
{
    // The lead's order (#150): a spell casts, whatever its effect; a non-spell attack swings or shoots; a non-spell self or ally effect, a swap and an
    // actively used mitigation included, braces; anything else has no clip.
    if (bSpell) { return EUnitPose::Cast; }
    switch (Effect)
    {
    case EAbilityEffect::Strike:
    case EAbilityEffect::WeaponAttack:
    case EAbilityEffect::Area:
    case EAbilityEffect::Push:
    case EAbilityEffect::Surge:    // seeded damage to an enemy (a barbarian's Wild Bolt): an attack when it is not a spell
    case EAbilityEffect::Missile: return bRanged ? EUnitPose::Ranged : EUnitPose::Melee;
    case EAbilityEffect::Rage:
    case EAbilityEffect::Protect:
    case EAbilityEffect::Healing:
    case EAbilityEffect::Swap:
    case EAbilityEffect::Mitigation: return EUnitPose::Brace;
    default: return EUnitPose::Idle;
    }
}

EUnitPose ForAbility(const FAbilityFacts& Ability, bool bRanged) { return ForEffect(Ability.Effect, bRanged, Ability.bSpell); }

EUnitPose ForDamage() { return EUnitPose::Hit; }
EUnitPose ForFall(bool bDead) { return bDead ? EUnitPose::Death : EUnitPose::Downed; }
EUnitPose ForRecovery() { return EUnitPose::Recover; }
}
