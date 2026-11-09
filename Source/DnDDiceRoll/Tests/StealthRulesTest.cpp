#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Rules/StealthRules.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStealthRulesTest, "Project.Expedition.Rules.StealthAndSurprise",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FStealthRulesTest::RunTest(const FString& Parameters)
{
    TestEqual(TEXT("Passive Perception +2"), FStealthRules::PassiveScore(2, ERollMode::Normal), 12);
    TestEqual(TEXT("Advantage adds 5"), FStealthRules::PassiveScore(2, ERollMode::Advantage), 17);
    TestEqual(TEXT("Disadvantage subtracts 5"), FStealthRules::PassiveScore(2, ERollMode::Disadvantage), 7);
    TestTrue(TEXT("Tie hides"), FStealthRules::HiddenFrom(12, 12));
    TestFalse(TEXT("Below is seen"), FStealthRules::HiddenFrom(11, 12));
    TestTrue(TEXT("Hidden attacker has advantage"), FStealthRules::AttackModeWhileHidden(true, false) == ERollMode::Advantage);
    TestTrue(TEXT("Unseen target imposes disadvantage"), FStealthRules::AttackModeWhileHidden(false, true) == ERollMode::Disadvantage);
    TestTrue(TEXT("Both hidden cancels"), FStealthRules::AttackModeWhileHidden(true, true) == ERollMode::Normal);
    TestFalse(TEXT("Attacking reveals"), FStealthRules::StaysHiddenAfterAttack());
    TestTrue(TEXT("Surprised when all ambushers hidden"), FStealthRules::IsSurprised({15, 13}, 12));
    TestFalse(TEXT("One noticed ambusher spoils surprise"), FStealthRules::IsSurprised({15, 9}, 12));
    TestFalse(TEXT("No ambushers, no surprise"), FStealthRules::IsSurprised({}, 12));
    TestTrue(TEXT("Surprised Initiative Disadvantage"), FStealthRules::InitiativeMode(true) == ERollMode::Disadvantage);
    TestTrue(TEXT("Alert Initiative Normal"), FStealthRules::InitiativeMode(false) == ERollMode::Normal);
    return !HasAnyErrors();
}
#endif
