#pragma once

#include "CoreMinimal.h"
#include "Rules/CheckRules.h"
#include "Rules/StandardActionRules.h"

enum class EAdventurerRole : uint8;

/** One SRD class feature gained at levels 1-5, with its action-economy tag (#79). */
struct FClassFeature
{
    const TCHAR* Name = TEXT("");
    int32 Level = 1;
    EActionCost Cost = EActionCost::Free;
    /** True when the battle engine resolves it today; false = listed and tagged, not yet playable. */
    bool bPlayable = false;
};

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
    /** Engine cap: heroes advance to level 5; higher-level rows are data only (see DataFeaturesThrough). */
    static constexpr int32 EngineLevelCap = 5;
    /** SRD features a class has by this class level (clamped to EngineLevelCap), each tagged Action/Bonus/Reaction/Free. */
    static TArray<FClassFeature> FeaturesThrough(EAdventurerRole Role, int32 Level);
    /** Uncapped table (levels 1-20 where recorded: Warlock and Bard so far) for data and level-gating tests; not playable past the cap. */
    static TArray<FClassFeature> DataFeaturesThrough(EAdventurerRole Role, int32 Level);
    /** "Name [Tag]" list joined by ", " for menus. */
    static FString FeatureSummary(EAdventurerRole Role, int32 Level);
};
