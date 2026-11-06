#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Rules/StealthRules.h"

/** F1 regression: SRD 5.2.1 Surprise is Initiative Disadvantage; no skipped turn. */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSurpriseInitiativeRegressionTest, "Project.Expedition.Rules.SurpriseInitiativeDisadvantage",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSurpriseInitiativeRegressionTest::RunTest(const FString& Parameters)
{
    TestTrue(TEXT("Surprised -> Initiative Disadvantage"), FStealthRules::InitiativeMode(true) == ERollMode::Disadvantage);
    TestTrue(TEXT("Not surprised -> Normal Initiative"), FStealthRules::InitiativeMode(false) == ERollMode::Normal);
    // Ambush detection still works; the effect is on Initiative, not on whether the creature gets a turn.
    TestTrue(TEXT("Hidden ambushers surprise"), FStealthRules::IsSurprised({14, 16}, 12));
    TestFalse(TEXT("Spotted ambusher: not surprised"), FStealthRules::IsSurprised({14, 10}, 12));
    return !HasAnyErrors();
}
#endif
