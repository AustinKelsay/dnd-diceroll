#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Rules/SuperiorityRules.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSuperiorityRulesTest, "Project.Expedition.Rules.SuperiorityDice",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSuperiorityRulesTest::RunTest(const FString& Parameters)
{
    TestEqual(TEXT("d8 at 3"), FSuperiorityRules::DieSides(3), 8);
    TestEqual(TEXT("d10 at 10"), FSuperiorityRules::DieSides(10), 10);
    TestEqual(TEXT("d12 at 18"), FSuperiorityRules::DieSides(18), 12);
    TestEqual(TEXT("None before 3"), FSuperiorityRules::DicePerRest(2), 0);
    TestEqual(TEXT("Four at 3"), FSuperiorityRules::DicePerRest(3), 4);
    TestEqual(TEXT("Five at 7"), FSuperiorityRules::DicePerRest(7), 5);
    TestEqual(TEXT("Six at 15"), FSuperiorityRules::DicePerRest(15), 6);
    TestFalse(TEXT("Riders are free"), FSuperiorityRules::RiderCostsAction());
    TestEqual(TEXT("Only a natural 20 hits AC 30"), FSuperiorityRules::PrecisionHitOutOf(0, 30, ERollMode::Normal, 8), 160);
    TestEqual(TEXT("Everything but a natural 1 hits AC 2"), FSuperiorityRules::PrecisionHitOutOf(0, 2, ERollMode::Normal, 8), 3040);
    TestEqual(TEXT("+5 into AC 15 with a d8"), FSuperiorityRules::PrecisionHitOutOf(5, 15, ERollMode::Normal, 8), 2480);
    TestTrue(TEXT("Advantage helps"), FSuperiorityRules::PrecisionHitOutOf(5, 15, ERollMode::Advantage, 8) > 2480);
    TestEqual(TEXT("Save DC 8 + PB + mod"), FSuperiorityRules::ManeuverSaveDc(2, 3), 13);
    return !HasAnyErrors();
}
#endif
