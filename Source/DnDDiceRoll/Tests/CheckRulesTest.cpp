#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Rules/CheckRules.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRollModeAndChanceTest, "Project.Expedition.Rules.CheckChancesAreExact",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRollModeAndChanceTest::RunTest(const FString& Parameters)
{
    TestTrue(TEXT("No sources is normal"), FCheckRules::CombineMode(0, 0) == ERollMode::Normal);
    TestTrue(TEXT("Advantage"), FCheckRules::CombineMode(1, 0) == ERollMode::Advantage);
    TestTrue(TEXT("Disadvantage"), FCheckRules::CombineMode(0, 2) == ERollMode::Disadvantage);
    TestTrue(TEXT("Two advantages and one disadvantage cancel"), FCheckRules::CombineMode(2, 1) == ERollMode::Normal);
    for (int32 Modifier = -2; Modifier <= 7; Modifier += 3)
    {
        for (int32 Dc = 1; Dc <= 30; ++Dc)
        {
            for (ERollMode Mode : {ERollMode::Normal, ERollMode::Advantage, ERollMode::Disadvantage})
            {
                int32 Successes = 0;
                for (int32 First = 1; First <= 20; ++First)
                {
                    for (int32 Second = 1; Second <= 20; ++Second)
                    {
                        const int32 Kept = Mode == ERollMode::Advantage ? FMath::Max(First, Second) : Mode == ERollMode::Disadvantage ? FMath::Min(First, Second) : First;
                        Successes += Kept + Modifier >= Dc ? 1 : 0;
                    }
                }
                TestEqual(FString::Printf(TEXT("+%d vs DC %d (%s)"), Modifier, Dc, FCheckRules::ModeName(Mode)), FCheckRules::SuccessOutOf400(Modifier, Dc, Mode), Successes);
            }
        }
    }
    TestEqual(TEXT("Advantage on DC 11 is 75%"), FCheckRules::SuccessPercent(0, 11, ERollMode::Advantage), 75);
    TestEqual(TEXT("Natural 20 does not beat DC 25 on a check"), FCheckRules::SuccessOutOf400(0, 25, ERollMode::Advantage), 0);
    TestEqual(TEXT("Natural 1 still passes DC 1 with +0"), FCheckRules::SuccessOutOf400(0, 1, ERollMode::Disadvantage), 400);
    TestFalse(TEXT("Contest tie keeps the status quo"), FCheckRules::ActorWinsContest(14, 14));
    TestTrue(TEXT("Higher actor total wins"), FCheckRules::ActorWinsContest(15, 14));
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCheckResolveTest, "Project.Expedition.Rules.CheckResolutionIsSeeded",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FCheckResolveTest::RunTest(const FString& Parameters)
{
    for (int32 Seed = 1; Seed <= 200; ++Seed)
    {
        int32 Normal = Seed, Again = Seed, Advantage = Seed, Disadvantage = Seed, Manual = Seed;
        const FD20Outcome One = FCheckRules::Resolve(3, 13, ERollMode::Normal, Normal);
        const FD20Outcome Two = FCheckRules::Resolve(3, 13, ERollMode::Normal, Again);
        TestTrue(TEXT("Same seed, same roll"), One.Kept == Two.Kept && Normal == Again);
        FRandomStream Stream(Manual);
        const int32 Die = Stream.RandRange(1, 20);
        TestEqual(TEXT("Normal spends one combat-style die"), One.Rolls[0], Die);
        TestEqual(TEXT("Normal advances the state once"), Normal, Stream.GetCurrentSeed());
        TestEqual(TEXT("Normal rolls once"), One.RollCount, 1);
        TestTrue(TEXT("Total and success"), One.Total == One.Kept + 3 && One.bSuccess == (One.Total >= 13));
        const FD20Outcome Adv = FCheckRules::Resolve(0, 25, ERollMode::Advantage, Advantage);
        TestTrue(TEXT("Advantage keeps the higher of two"), Adv.RollCount == 2 && Adv.Kept == FMath::Max(Adv.Rolls[0], Adv.Rolls[1]) && Adv.Rolls[0] == Die);
        TestFalse(TEXT("DC 25 at +0 never succeeds"), Adv.bSuccess);
        const FD20Outcome Dis = FCheckRules::Resolve(0, 10, ERollMode::Disadvantage, Disadvantage);
        TestTrue(TEXT("Disadvantage keeps the lower of two"), Dis.RollCount == 2 && Dis.Kept == FMath::Min(Dis.Rolls[0], Dis.Rolls[1]));
        TestEqual(TEXT("Both two-die modes spend the same stream"), Advantage, Disadvantage);
    }
    return !HasAnyErrors();
}

#endif
