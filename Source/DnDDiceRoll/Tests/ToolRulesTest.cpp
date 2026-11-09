#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Rules/ToolRules.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FToolKitsTest, "Project.Expedition.Rules.ToolsAndKits",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FToolKitsTest::RunTest(const FString& Parameters)
{
    const FToolCheck Pro = FToolRules::Check(3, 2, true, true, false, true);
    TestTrue(TEXT("Thieves' tools proficiency adds PB"), Pro.bAllowed && Pro.Modifier == 5 && Pro.Mode == ERollMode::Normal);
    const FToolCheck Amateur = FToolRules::Check(3, 2, true, false, false, true);
    TestTrue(TEXT("Without proficiency: ability only"), Amateur.bAllowed && Amateur.Modifier == 3);
    TestFalse(TEXT("No tools: lock cannot be picked"), FToolRules::Check(3, 2, false, true, false, true).bAllowed);
    TestTrue(TEXT("Tool + skill proficiency: advantage"), FToolRules::Check(1, 2, true, true, true, false).Mode == ERollMode::Advantage);
    int32 Uses = FToolRules::HealersKitUses;
    TestTrue(TEXT("Healer's kit stabilizes without a check"), FToolRules::HealersKitStabilize(Uses) && Uses == 9);
    int32 Empty = 0;
    TestFalse(TEXT("Empty kit does nothing"), FToolRules::HealersKitStabilize(Empty));
    TestEqual(TEXT("Empty stays empty"), Empty, 0);
    TestTrue(TEXT("Herbalist brews"), FToolRules::CanBrewHealing(true, true));
    TestFalse(TEXT("Needs the kit"), FToolRules::CanBrewHealing(true, false));
    return !HasAnyErrors();
}
#endif
