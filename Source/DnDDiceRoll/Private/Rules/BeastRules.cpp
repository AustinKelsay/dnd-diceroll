#include "Rules/BeastRules.h"

namespace BeastTable
{
struct FEntry { const TCHAR* Name; int32 Dc; };
static const FEntry Beasts[] = {{TEXT("Wolf"), 13}, {TEXT("Giant Rat"), 10}, {TEXT("Boar"), 13}, {TEXT("Brown Bear"), 15}, {TEXT("Giant Spider"), 15}};
}

bool FBeastRules::IsBeast(const FString& Profile) { return CalmDc(Profile) > 0; }

int32 FBeastRules::CalmDc(const FString& Profile)
{
    for (const BeastTable::FEntry& Entry : BeastTable::Beasts) { if (Profile == Entry.Name) { return Entry.Dc; } }
    return 0;
}

EBeastBehaviour FBeastRules::BehaviourFor(int32 Total, int32 Dc)
{
    if (Total < Dc) { return EBeastBehaviour::Hostile; }
    return Total - Dc >= 5 ? EBeastBehaviour::Redirected : EBeastBehaviour::Calmed;
}

int32 FBeastRules::CalmOutOf400(int32 Modifier, ERollMode Mode, const FString& Profile, bool bHurtThisFight)
{
    const int32 Dc = CalmDc(Profile);
    return Dc > 0 && !bHurtThisFight ? FCheckRules::SuccessOutOf400(Modifier, Dc, Mode) : 0;
}

FBeastOutcome FBeastRules::Resolve(int32 Modifier, ERollMode Mode, const FString& Profile, bool bHurtThisFight, int32& RandomState)
{
    FBeastOutcome Outcome;
    const int32 Dc = CalmDc(Profile);
    if (Dc <= 0 || bHurtThisFight) { return Outcome; } // rejected: no roll, RNG untouched
    Outcome.Check = FCheckRules::Resolve(Modifier, Dc, Mode, RandomState);
    Outcome.Behaviour = BehaviourFor(Outcome.Check.Total, Dc);
    return Outcome;
}

int32 FBeastRules::MountCost(int32 SpeedCells) { return (FMath::Max(SpeedCells, 0) + 1) / 2; }

bool FBeastRules::ControlledMountMayTake(const FString& ActionName)
{
    return ActionName == TEXT("Dash") || ActionName == TEXT("Disengage") || ActionName == TEXT("Dodge");
}
