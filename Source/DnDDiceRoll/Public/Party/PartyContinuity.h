#pragma once
#include "CoreMinimal.h"
#include "Combat/CombatBattle.h"

/** Persistent party operations composed by the run. Layout owns cells/enemies; the
 * completed battle owns every party value and stable ID, including dead members.
 * Reward grants 1..2 potions (cap 9) and one reward card to a conscious survivor: Gear (+1 gear rank, cap 3), Reserve (+1 cap and
 * +1 use on each pool, once per hero) or Honed (once per hero) (#218). Which cards a run offers is the run's decision.
 * Recovery is explicit, capped and survivor-only; Carry never rests or awards loot.
 * The run owns exactly-once reward/camp phase gates. Advance retains the earlier
 * atomic reward-plus-carry interface for existing callers and public party tests.
 */
class DNDDICEROLL_API FPartyContinuity
{
public:
    static bool Carry(FCombatBattle& Battle, const FCombatEncounter& Layout, int32 Seed, FString& Error);
    /** The Gear card, as every reward before #218. */
    static bool Reward(FCombatBattle& Battle, int32 RecipientId, int32 Potions, FString& Error);
    /** Applies Card and the potions; a rejection (with a named Error) changes nothing. The caller decides which cards its run offers. */
    static bool Reward(FCombatBattle& Battle, int32 RecipientId, int32 Potions, ERewardCard Card, FString& Error);
    static bool Recover(FCombatBattle& Battle, FString& Error);
    static FString RecoveryDescription();
    static bool Advance(FCombatBattle& Battle, const FCombatEncounter& Layout, int32 Seed, int32 RecipientId, FString& Error);
};
