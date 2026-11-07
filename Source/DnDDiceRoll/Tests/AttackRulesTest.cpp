#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Rules/AttackRules.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAttackChanceTest, "Project.Expedition.Rules.AttackChancesAreExact",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FAttackChanceTest::RunTest(const FString& Parameters)
{
    for (int32 Modifier : {-5, 0, 5, 30})
    {
        for (int32 ArmorClass = 1; ArmorClass <= 30; ++ArmorClass)
        {
            for (ERollMode Mode : {ERollMode::Normal, ERollMode::Advantage, ERollMode::Disadvantage})
            {
                const TArray<FAttackRollSource> Sources = {{TEXT("Test source"), Mode}};
                int32 Hits = 0;
                for (int32 First = 1; First <= 20; ++First)
                {
                    for (int32 Second = 1; Second <= 20; ++Second)
                    {
                        const int32 Kept = Mode == ERollMode::Advantage ? FMath::Max(First, Second) : Mode == ERollMode::Disadvantage ? FMath::Min(First, Second) : First;
                        Hits += Kept == 20 || (Kept != 1 && Kept + Modifier >= ArmorClass) ? 1 : 0;
                    }
                }
                TestEqual(TEXT("Forecast matches every pair of faces"), FAttackRules::SuccessOutOf400(Modifier, ArmorClass, Sources), Hits);
            }
        }
    }
    const TArray<FAttackRollSource> Advantage = {{TEXT("Unseen attacker"), ERollMode::Advantage}};
    TestEqual(TEXT("Natural 20 can hit otherwise unreachable AC with advantage"), FAttackRules::SuccessOutOf400(0, 25, Advantage), 39);
    TestEqual(TEXT("Natural 1 misses otherwise guaranteed AC"), FAttackRules::SuccessOutOf400(30, 1, {}), 380);
    TestEqual(TEXT("Normal +0 against AC 11"), FAttackRules::SuccessPercent(0, 11, {}), 50);
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAttackSourcesAndResolutionTest, "Project.Expedition.Rules.AttackSourcesAndSeededResolution",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FAttackSourcesAndResolutionTest::RunTest(const FString& Parameters)
{
    const TArray<FAttackRollSource> Other = {{TEXT("Unseen attacker"), ERollMode::Advantage}, {TEXT("Restrained target"), ERollMode::Advantage}};
    const TArray<FAttackRollSource> Sources = FAttackRules::WithRangedAdjacency(Other, true, true);
    TestEqual(TEXT("Inputs are untouched"), Other.Num(), 2);
    TestEqual(TEXT("Named adjacency fact appended"), Sources.Num(), 3);
    TestEqual(TEXT("Adjacency reason can be listed"), Sources.Last().Name, FString(TEXT("Adjacent hostile")));
    TestTrue(TEXT("One disadvantage cancels all advantages"), FAttackRules::CombineMode(Sources) == ERollMode::Normal);
    TestEqual(TEXT("Melee does not add ranged penalty"), FAttackRules::WithRangedAdjacency(Other, false, true).Num(), 2);
    TestEqual(TEXT("No threatening hostile adds no penalty"), FAttackRules::WithRangedAdjacency(Other, true, false).Num(), 2);
    const TArray<FAttackRollSource> Adjacent = FAttackRules::WithRangedAdjacency({}, true, true);
    TestTrue(TEXT("Ranged attack beside hostile has disadvantage"), FAttackRules::CombineMode(Adjacent) == ERollMode::Disadvantage);
    for (int32 Seed = 1; Seed <= 200; ++Seed)
    {
        for (int32 Modifier : {0, 30})
        {
            for (const TArray<FAttackRollSource>& Facts : {Other, Sources, Adjacent})
            {
                int32 State = Seed, Again = Seed, Manual = Seed;
                const FAttackOutcome Outcome = FAttackRules::Resolve(Modifier, 25, Facts, State);
                const FAttackOutcome Repeat = FAttackRules::Resolve(Modifier, 25, Facts, Again);
                const FD20Outcome Check = FCheckRules::Resolve(Modifier, 25, FAttackRules::CombineMode(Facts), Manual);
                TestTrue(TEXT("Same seed repeats rolls and state"), Outcome.Roll.Kept == Repeat.Roll.Kept && State == Again);
                TestTrue(TEXT("Uses the check resolver's one/two-die stream"), Outcome.Roll.Kept == Check.Kept && State == Manual);
                TestEqual(TEXT("Source facts retained in result"), Outcome.Sources.Num(), Facts.Num());
                TestEqual(TEXT("Critical iff kept natural 20"), Outcome.bCritical, Outcome.Roll.Kept == 20);
                TestEqual(TEXT("Attack natural 1/20 and total match forecast predicate"), Outcome.Roll.bSuccess,
                    Outcome.Roll.Kept == 20 || (Outcome.Roll.Kept != 1 && Outcome.Roll.Total >= 25));
            }
        }
    }
    return !HasAnyErrors();
}
#endif
