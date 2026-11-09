#include "Rules/ClassFeatureRules.h"
#include "Combat/CombatBattle.h"

int32 FClassFeatureRules::SecondWindHealing(int32 D10Roll, int32 Level) { return D10Roll + Level; }
int32 FClassFeatureRules::ActionSurgeUses(int32 Level) { return Level >= 17 ? 2 : Level >= 2 ? 1 : 0; }

int32 FClassFeatureRules::AttacksPerAction(EAdventurerRole Role, int32 Level)
{
    // SRD 5.2.1: Barbarian, Monk, Paladin and Ranger get one Extra Attack at 5; only the Fighter gains more.
    if (Role == EAdventurerRole::Barbarian || Role == EAdventurerRole::Monk || Role == EAdventurerRole::Paladin || Role == EAdventurerRole::Ranger) { return Level >= 5 ? 2 : 1; }
    if (Role != EAdventurerRole::Fighter) { return 1; }
    return Level >= 20 ? 4 : Level >= 11 ? 3 : Level >= 5 ? 2 : 1;
}

int32 FClassFeatureRules::SneakAttackDice(int32 Level) { return Level >= 1 ? (Level + 1) / 2 : 0; }

bool FClassFeatureRules::SneakAttackAllowed(bool bFinesseOrRanged, bool bUsedThisTurn, ERollMode EffectiveMode, bool bAllyAdjacentToTarget)
{
    if (!bFinesseOrRanged || bUsedThisTurn || EffectiveMode == ERollMode::Disadvantage) { return false; }
    return EffectiveMode == ERollMode::Advantage || bAllyAdjacentToTarget;
}

bool FClassFeatureRules::HasCunningAction(int32 Level) { return Level >= 2; }
int32 FClassFeatureRules::ChannelDivinityUses(int32 Level) { return Level >= 18 ? 3 : Level >= 6 ? 2 : Level >= 2 ? 1 : 0; }
int32 FClassFeatureRules::ArcaneRecoveryLevels(int32 Level) { return Level >= 1 ? (Level + 1) / 2 : 0; }
