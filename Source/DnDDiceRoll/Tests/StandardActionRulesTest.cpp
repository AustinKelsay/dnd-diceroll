#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Rules/StandardActionRules.h"
#include "Combat/CombatBattle.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStandardActionRulesTest, "Project.Expedition.Rules.StandardActions",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FStandardActionRulesTest::RunTest(const FString& Parameters)
{
    TestTrue(TEXT("Rogue 2 dashes as a bonus action"), FStandardActionRules::CanTakeAsBonusAction(EStandardAction::Dash, EAdventurerRole::Rogue, 2));
    TestFalse(TEXT("Rogue 1 cannot"), FStandardActionRules::CanTakeAsBonusAction(EStandardAction::Dash, EAdventurerRole::Rogue, 1));
    TestFalse(TEXT("Fighter cannot"), FStandardActionRules::CanTakeAsBonusAction(EStandardAction::Disengage, EAdventurerRole::Fighter, 5));
    TestFalse(TEXT("Dodge is never cunning"), FStandardActionRules::CanTakeAsBonusAction(EStandardAction::Dodge, EAdventurerRole::Rogue, 5));
    TestTrue(TEXT("Hide after attacking costs the bonus action"), FStandardActionRules::Cost(EStandardAction::Hide, EAdventurerRole::Rogue, 3, true) == EActionCost::BonusAction);
    TestTrue(TEXT("Fresh turn uses the action"), FStandardActionRules::Cost(EStandardAction::Hide, EAdventurerRole::Rogue, 3, false) == EActionCost::Action);
    TestEqual(TEXT("No dash"), FStandardActionRules::MovementBudget(6, 0), 6);
    TestEqual(TEXT("Dash doubles"), FStandardActionRules::MovementBudget(6, 1), 12);
    TestEqual(TEXT("Action and cunning dash triple"), FStandardActionRules::MovementBudget(6, 2), 18);
    TestTrue(TEXT("Dodge imposes disadvantage"), FStandardActionRules::AttackAgainstDodger(true, true) == ERollMode::Disadvantage);
    TestTrue(TEXT("Unseen attacker ignores Dodge"), FStandardActionRules::AttackAgainstDodger(true, false) == ERollMode::Normal);
    TestTrue(TEXT("Dodge helps Dex saves"), FStandardActionRules::DexSaveWhileDodging(true) == ERollMode::Advantage);
    TestTrue(TEXT("Leaving reach provokes"), FStandardActionRules::LeavingReachProvokes(false));
    TestFalse(TEXT("Disengage prevents it"), FStandardActionRules::LeavingReachProvokes(true));
    return !HasAnyErrors();
}
#endif
