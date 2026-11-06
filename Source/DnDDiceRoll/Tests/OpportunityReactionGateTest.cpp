#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Rules/OpportunityRules.h"

/** F3 regression: FirstProvokingCell respects an unavailable reaction. */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOpportunityReactionGateRegressionTest, "Project.Expedition.Rules.OpportunityReactionGate",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FOpportunityReactionGateRegressionTest::RunTest(const FString& Parameters)
{
    const FIntPoint Guard(0, 0);
    const TArray<FIntPoint> Path = {FIntPoint(1, 0), FIntPoint(2, 0)};
    TestEqual(TEXT("Reaction available: provokes leaving reach"), FOpportunityRules::FirstProvokingCell(Path, Guard, 1, false, true), 0);
    TestEqual(TEXT("Reaction spent: no provoking cell"), FOpportunityRules::FirstProvokingCell(Path, Guard, 1, false, false), (int32)INDEX_NONE);
    TestFalse(TEXT("StepProvokes false when reaction spent"), FOpportunityRules::StepProvokes(Guard, 1, FIntPoint(1, 0), FIntPoint(2, 0), false, false, false));
    TestTrue(TEXT("StepProvokes true when reaction free"), FOpportunityRules::StepProvokes(Guard, 1, FIntPoint(1, 0), FIntPoint(2, 0), false, false, true));
    return !HasAnyErrors();
}
#endif
