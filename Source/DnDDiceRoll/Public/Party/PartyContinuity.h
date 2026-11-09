#pragma once
#include "CoreMinimal.h"
#include "Combat/CombatBattle.h"

/** Persistent party operations composed by the run. Layout owns cells/enemies; the
 * completed battle owns every party value and stable ID, including dead members.
 * Reward grants +1 gear (cap 3) and 1..2 potions (cap 9) to a conscious survivor.
 * Recovery is explicit, capped and survivor-only; Carry never rests or awards loot.
 * The run owns exactly-once reward/camp phase gates. Advance retains the earlier
 * atomic reward-plus-carry interface for existing callers and public party tests.
 * SRD level-up (XP / ApplyLevelUp) is between battles only; Compact keeps victory auto-level.
 */
class DNDDICEROLL_API FPartyContinuity
{
public:
    static bool Carry(FCombatBattle& Battle, const FCombatEncounter& Layout, int32 Seed, FString& Error);
    static bool Reward(FCombatBattle& Battle, int32 RecipientId, int32 Potions, FString& Error);
    static bool Recover(FCombatBattle& Battle, FString& Error);
    static FString RecoveryDescription();
    static bool Advance(FCombatBattle& Battle, const FCombatEncounter& Layout, int32 Seed, int32 RecipientId, FString& Error);

    /** SRD: sum ExperienceReward of defeated enemies; divide evenly (floor) among conscious party; set bLevelUpAvailable. Compact: no-op. */
    static void AwardEncounterExperience(FCombatBattle& Battle);
    /**
     * SRD between battles: spend one pending level-up for AdventurerId into ClassToAdvance (must meet multiclass prereq when new).
     * HitDieRoll is the seeded class hit die (1..sides). Compact: rejects.
     */
    static bool ApplyLevelUp(FCombatBattle& Battle, int32 AdventurerId, EAdventurerRole ClassToAdvance, int32 HitDieRoll, FString& Error);
};