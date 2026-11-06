#include "Rules/TwoWeaponRules.h"

bool FTwoWeaponRules::CanOffHandAttack(const FWeaponProfile& Main, const FWeaponProfile& OffHand, bool bTookAttackAction, bool bBonusActionUsed)
{
    return bTookAttackAction && !bBonusActionUsed && FWeaponRules::CanTwoWeaponFight(Main, OffHand);
}

int32 FTwoWeaponRules::OffHandDamageModifier(int32 AbilityModifier, bool bTwoWeaponFightingStyle)
{
    return bTwoWeaponFightingStyle ? AbilityModifier : FWeaponRules::DamageModifier(AbilityModifier, true);
}
