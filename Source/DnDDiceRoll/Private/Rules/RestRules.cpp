#include "Rules/RestRules.h"
#include "Combat/CombatBattle.h"

int32 FRestRules::HitDieSides(EAdventurerRole Role)
{
    switch (Role)
    {
    case EAdventurerRole::Fighter: return 10;
    case EAdventurerRole::Rogue: return 8;
    case EAdventurerRole::Cleric: return 8;
    case EAdventurerRole::Wizard: return 6;
    default: return 0;
    }
}

int32 FRestRules::HitDieHealing(int32 Roll, int32 ConstitutionModifier) { return FMath::Max(0, Roll + ConstitutionModifier); }

int32 FRestRules::HitDiceAfterLongRest(int32 Available, int32 Level)
{
    return FMath::Min(Level, Available + FMath::Max(1, Level / 2));
}

ERestKind FRestRules::RefreshedBy(ERestPool Pool)
{
    switch (Pool)
    {
    case ERestPool::SecondWind:
    case ERestPool::ActionSurge:
    case ERestPool::ChannelDivinity:
    case ERestPool::Superiority:
        return ERestKind::ShortRest;
    default:
        return ERestKind::LongRest;
    }
}

ERestKind FRestRules::CampRest() { return ERestKind::LongRest; }
