#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Rules/OpportunityRules.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOpportunityRulesTest, "Project.Expedition.Rules.OpportunityAttackTriggers",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FOpportunityRulesTest::RunTest(const FString& Parameters)
{
    const FIntPoint GuardCell(0, 0);
    TestEqual(TEXT("Diagonal counts 1"), FOpportunityRules::GridDistance(GuardCell, FIntPoint(1, 1)), 1);
    TestTrue(TEXT("Adjacent in reach"), FOpportunityRules::InReach(GuardCell, 1, FIntPoint(1, -1)));
    TestFalse(TEXT("Two away out of reach"), FOpportunityRules::InReach(GuardCell, 1, FIntPoint(2, 0)));
    TestTrue(TEXT("Glaive reaches two"), FOpportunityRules::InReach(GuardCell, 2, FIntPoint(2, 2)));
    TestTrue(TEXT("Leaving reach provokes"), FOpportunityRules::StepProvokes(GuardCell, 1, FIntPoint(1, 0), FIntPoint(2, 0), false, false, true));
    TestFalse(TEXT("Moving within reach does not"), FOpportunityRules::StepProvokes(GuardCell, 1, FIntPoint(1, 0), FIntPoint(1, 1), false, false, true));
    TestFalse(TEXT("Disengage does not"), FOpportunityRules::StepProvokes(GuardCell, 1, FIntPoint(1, 0), FIntPoint(2, 0), true, false, true));
    TestFalse(TEXT("Forced movement does not"), FOpportunityRules::StepProvokes(GuardCell, 1, FIntPoint(1, 0), FIntPoint(2, 0), false, true, true));
    TestFalse(TEXT("Spent reaction does not"), FOpportunityRules::StepProvokes(GuardCell, 1, FIntPoint(1, 0), FIntPoint(2, 0), false, false, false));
    const TArray<FIntPoint> RunPath = {FIntPoint(1, 0), FIntPoint(1, 1), FIntPoint(2, 1), FIntPoint(3, 1)};
    TestEqual(TEXT("Flags the cell being left"), FOpportunityRules::FirstProvokingCell(RunPath, GuardCell, 1, false, true), 1);
    TestEqual(TEXT("Disengaged path is safe"), FOpportunityRules::FirstProvokingCell(RunPath, GuardCell, 1, true, true), (int32)INDEX_NONE);
    TestEqual(TEXT("Spent reaction is safe"), FOpportunityRules::FirstProvokingCell(RunPath, GuardCell, 1, false, false), (int32)INDEX_NONE);
    const TArray<FIntPoint> FarPath = {FIntPoint(4, 0), FIntPoint(5, 0)};
    TestEqual(TEXT("Never in reach"), FOpportunityRules::FirstProvokingCell(FarPath, GuardCell, 1, false, true), (int32)INDEX_NONE);
    return !HasAnyErrors();
}
#endif
