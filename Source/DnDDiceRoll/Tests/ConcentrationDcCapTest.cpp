#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Rules/ConcentrationRules.h"

/** Concentration DC is max(10, half damage), capped at 30. */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FConcentrationDcCapRegressionTest, "Project.Expedition.Rules.ConcentrationSaveDcCap",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FConcentrationDcCapRegressionTest::RunTest(const FString& Parameters)
{
    TestEqual(TEXT("Below 20 damage stays DC 10"), FConcentrationRules::SaveDc(19), 10);
    TestEqual(TEXT("20 damage -> DC 10"), FConcentrationRules::SaveDc(20), 10);
    TestEqual(TEXT("22 damage -> DC 11"), FConcentrationRules::SaveDc(22), 11);
    TestEqual(TEXT("60 damage -> DC 30"), FConcentrationRules::SaveDc(60), 30);
    TestEqual(TEXT("61 damage still DC 30"), FConcentrationRules::SaveDc(61), 30);
    TestEqual(TEXT("1000 damage still DC 30"), FConcentrationRules::SaveDc(1000), 30);
    return !HasAnyErrors();
}
#endif
