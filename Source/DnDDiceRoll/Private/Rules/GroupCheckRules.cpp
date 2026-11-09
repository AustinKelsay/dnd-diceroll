#include "Rules/GroupCheckRules.h"

int32 FGroupCheckRules::PassiveScore(int32 Modifier, ERollMode Mode)
{
    return 10 + Modifier + (Mode == ERollMode::Advantage ? 5 : Mode == ERollMode::Disadvantage ? -5 : 0);
}

ERollMode FGroupCheckRules::HelpedMode(int32 AdvantageSources, int32 DisadvantageSources, bool bHelped)
{
    return FCheckRules::CombineMode(AdvantageSources + (bHelped ? 1 : 0), DisadvantageSources);
}

bool FGroupCheckRules::GroupSucceeds(int32 Successes, int32 GroupSize)
{
    return GroupSize > 0 && Successes * 2 >= GroupSize;
}

int32 FGroupCheckRules::GroupSuccessPerMillion(const TArray<FGroupMember>& Group, int32 Dc)
{
    // Distribution of success counts, member by member.
    TArray<double> Ways; Ways.Init(0.0, Group.Num() + 1); Ways[0] = 1.0;
    for (int32 Index = 0; Index < Group.Num(); ++Index)
    {
        const double P = FCheckRules::SuccessOutOf400(Group[Index].Modifier, Dc, Group[Index].Mode) / 400.0;
        for (int32 Count = Index + 1; Count >= 0; --Count)
        {
            Ways[Count] = Ways[Count] * (1.0 - P) + (Count > 0 ? Ways[Count - 1] * P : 0.0);
        }
    }
    double Chance = 0.0;
    for (int32 Count = 0; Count <= Group.Num(); ++Count) { if (GroupSucceeds(Count, Group.Num())) { Chance += Ways[Count]; } }
    return FMath::RoundToInt(Chance * 1000000.0);
}

FGroupCheckOutcome FGroupCheckRules::Resolve(const TArray<FGroupMember>& Group, int32 Dc, int32& RandomState)
{
    FGroupCheckOutcome Out;
    for (const FGroupMember& Member : Group)
    {
        Out.Members.Add(FCheckRules::Resolve(Member.Modifier, Dc, Member.Mode, RandomState));
        Out.Successes += Out.Members.Last().bSuccess ? 1 : 0;
    }
    Out.bSuccess = GroupSucceeds(Out.Successes, Group.Num());
    return Out;
}
