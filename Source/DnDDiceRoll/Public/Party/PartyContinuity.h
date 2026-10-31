#pragma once
#include "CoreMinimal.h"
#include "Combat/CombatBattle.h"

/** Persistent party operations composed by the run. Layout owns cells/enemies; the
 * completed battle owns every party value and stable ID, including dead members.
 * Reward grants +1 gear (cap 3) and 1..2 potions (cap 9) to a conscious survivor.
 * Recovery is explicit, capped and survivor-only; Carry never rests or awards loot.
 * The run owns exactly-once reward/camp phase gates. Advance retains the earlier
 * atomic reward-plus-carry interface for existing callers and public party tests.
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
