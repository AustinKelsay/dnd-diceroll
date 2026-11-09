#pragma once

#include "CoreMinimal.h"
#include "Rules/WeaponRules.h"

/** SRD two-weapon fighting on top of FWeaponRules. */
struct DNDDICEROLL_API FTwoWeaponRules
{
    /** Needs the Attack action this turn, two Light weapons (melee or ranged, SRD 5.2.1) and an unused bonus action. */
    static bool CanOffHandAttack(const FWeaponProfile& Main, const FWeaponProfile& OffHand, bool bTookAttackAction, bool bBonusActionUsed);
    /** Off-hand damage adds no positive ability modifier unless the creature has the Two-Weapon Fighting style. */
    static int32 OffHandDamageModifier(int32 AbilityModifier, bool bTwoWeaponFightingStyle);
};
