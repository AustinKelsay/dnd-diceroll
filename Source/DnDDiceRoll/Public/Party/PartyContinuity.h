#pragma once
#include "CoreMinimal.h"
#include "Combat/CombatBattle.h"

/** Between-encounter seam for the next run module. A successful victory transition gives
 * one conscious recipient +1 weapon damage rank (cap 3) and one potion (cap 9).
 * Layout owns cells/enemies; the previous battle owns every party value and identity.
 * Atomic rejection, including invalid layouts, preserves the completed battle and RNG.
 * Run owns reward/camp phase allowances; these operations validate a completed victory.
 * Carry never heals, replenishes, or awards loot.
 */
class DNDDICEROLL_API FPartyContinuity
{
public:
    static bool Carry(FCombatBattle& Battle, const FCombatEncounter& Layout, int32 Seed, FString& Error);
    static bool Reward(FCombatBattle& Battle, int32 RecipientId, int32 Potions, FString& Error);
    static bool Recover(FCombatBattle& Battle, FString& Error);
    static FString RecoveryDescription();
    static bool Advance(FCombatBattle& Battle, const FCombatEncounter& Layout, int32 Seed, int32 RecipientId, FString& Error);
};
