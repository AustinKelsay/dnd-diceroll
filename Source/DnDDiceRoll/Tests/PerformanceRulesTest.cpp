#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Rules/PerformanceRules.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPerformanceSkillTest, "Project.Expedition.Rules.PerformanceSkill",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FPerformanceSkillTest::RunTest(const FString& Parameters)
{
    TestEqual(TEXT("Success earns base coin"), FPerformanceRules::CoinsFor(15, 15, 5), 5);
    TestEqual(TEXT("Beat by 10 doubles"), FPerformanceRules::CoinsFor(25, 15, 5), 10);
    TestEqual(TEXT("Failure earns nothing"), FPerformanceRules::CoinsFor(14, 15, 5), 0);
    TestEqual(TEXT("Success warms the crowd"), FPerformanceRules::AttitudeShiftFor(15, 15), 1);
    TestEqual(TEXT("Small miss: no change"), FPerformanceRules::AttitudeShiftFor(11, 15), 0);
    TestEqual(TEXT("Booed"), FPerformanceRules::AttitudeShiftFor(10, 15), -1);
    TestTrue(TEXT("Indifferent +1 = friendly"), FPerformanceRules::ShiftAttitude(ENpcAttitude::Indifferent, 1) == ENpcAttitude::Friendly);
    TestTrue(TEXT("Clamped at friendly"), FPerformanceRules::ShiftAttitude(ENpcAttitude::Friendly, 2) == ENpcAttitude::Friendly);
    TestTrue(TEXT("Clamped at hostile"), FPerformanceRules::ShiftAttitude(ENpcAttitude::Indifferent, -3) == ENpcAttitude::Hostile);
    TestEqual(TEXT("Attitude name"), FString(FPerformanceRules::AttitudeName(ENpcAttitude::Hostile)), FString(TEXT("hostile")));
    int32 SeedA = 99, SeedB = 99;
    const FPerformanceOutcome A = FPerformanceRules::Resolve(4, ERollMode::Normal, 12, 5, SeedA);
    const FPerformanceOutcome B = FPerformanceRules::Resolve(4, ERollMode::Normal, 12, 5, SeedB);
    TestTrue(TEXT("Seeded and deterministic"), A.Check.Total == B.Check.Total && A.Coins == B.Coins && SeedA == SeedB && SeedA != 99);
    TestEqual(TEXT("Resolve coins match"), A.Coins, FPerformanceRules::CoinsFor(A.Check.Total, 12, 5));
    return !HasAnyErrors();
}
#endif
