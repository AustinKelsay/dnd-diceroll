#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Rules/InspirationRules.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInspirationRulesTest, "Project.Expedition.Rules.Inspiration",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FInspirationRulesTest::RunTest(const FString& Parameters)
{
    TestFalse(TEXT("Off by default"), FInspirationRules::EnabledByDefault());
    bool bHeldInspiration = false;
    TestTrue(TEXT("Grant works"), FInspirationRules::Grant(bHeldInspiration) && bHeldInspiration);
    TestFalse(TEXT("No stacking"), FInspirationRules::Grant(bHeldInspiration));
    TestTrue(TEXT("Spend uses it"), FInspirationRules::Spend(bHeldInspiration) && !bHeldInspiration);
    TestFalse(TEXT("Nothing to spend"), FInspirationRules::Spend(bHeldInspiration));
    TestTrue(TEXT("Spent gives advantage"), FInspirationRules::ModeAfterSpending(ERollMode::Normal, true) == ERollMode::Advantage);
    TestTrue(TEXT("Spent cancels disadvantage"), FInspirationRules::ModeAfterSpending(ERollMode::Disadvantage, true) == ERollMode::Normal);
    TestTrue(TEXT("Unspent leaves mode"), FInspirationRules::ModeAfterSpending(ERollMode::Disadvantage, false) == ERollMode::Disadvantage);
    return !HasAnyErrors();
}
#endif
