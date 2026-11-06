#pragma once

#include "CoreMinimal.h"
#include "Rules/CheckRules.h"

enum class EAdventurerRole : uint8;

/** Level-scaling class features as pure tables (SRD levels 1-20). */
struct DNDDICEROLL_API FClassFeatureRules
{
    /** Fighter: 1d10 + fighter level. */
    static int32 SecondWindHealing(int32 D10Roll, int32 Level);
    static int32 ActionSurgeUses(int32 Level);
    static int32 AttacksPerAction(EAdventurerRole Role, int32 Level);
    /** Rogue: 1d6 per two levels, rounded up. */
    static int32 SneakAttackDice(int32 Level);
    /** Finesse or ranged weapon, once per turn, and either advantage or an ally next to the target without disadvantage. */
    static bool SneakAttackAllowed(bool bFinesseOrRanged, bool bUsedThisTurn, ERollMode EffectiveMode, bool bAllyAdjacentToTarget);
    static bool HasCunningAction(int32 Level);
    /** Cleric uses between rests: 1 at 2, 2 at 6, 3 at 18. */
    static int32 ChannelDivinityUses(int32 Level);
    /** Wizard: combined slot levels recovered, half the level rounded up. */
    static int32 ArcaneRecoveryLevels(int32 Level);
};
