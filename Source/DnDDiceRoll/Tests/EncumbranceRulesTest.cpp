#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Rules/EncumbranceRules.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEncumbranceRulesTest, "Project.Expedition.Rules.Encumbrance",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FEncumbranceRulesTest::RunTest(const FString& Parameters)
{
    TestFalse(TEXT("Variant off by default"), FEncumbranceRules::VariantEnabledByDefault());
    TestEqual(TEXT("STR 10 carries 150"), FEncumbranceRules::CarryingCapacity(10), 150);
    TestEqual(TEXT("STR 10 drags 300"), FEncumbranceRules::PushDragLift(10), 300);
    TestEqual(TEXT("Base rule ignores 100 lb"), FEncumbranceRules::SpeedCells(6, 100, 10, false), 6);
    TestEqual(TEXT("Over capacity crawls 1 cell"), FEncumbranceRules::SpeedCells(6, 151, 10, false), 1);
    TestEqual(TEXT("Over drag limit stuck"), FEncumbranceRules::SpeedCells(6, 301, 10, false), 0);
    TestEqual(TEXT("Variant encumbered -2"), FEncumbranceRules::SpeedCells(6, 51, 10, true), 4);
    TestEqual(TEXT("Variant heavily encumbered -4"), FEncumbranceRules::SpeedCells(6, 101, 10, true), 2);
    TestEqual(TEXT("Variant at 50 is fine"), FEncumbranceRules::SpeedCells(6, 50, 10, true), 6);
    TestTrue(TEXT("Heavy load disadvantage"), FEncumbranceRules::PhysicalRollMode(101, 10, true) == ERollMode::Disadvantage);
    TestTrue(TEXT("Base rule no disadvantage"), FEncumbranceRules::PhysicalRollMode(101, 10, false) == ERollMode::Normal);
    TestEqual(TEXT("Chain shirt 20 lb"), FEncumbranceRules::Weight(EShopItem::ChainShirt), 20);
    for (int32 ItemIndex = 0; ItemIndex < (int32)EShopItem::Count; ++ItemIndex)
    {
        TestTrue(TEXT("Every item weighs something"), FEncumbranceRules::Weight((EShopItem)ItemIndex) > 0);
    }
    return !HasAnyErrors();
}
#endif
