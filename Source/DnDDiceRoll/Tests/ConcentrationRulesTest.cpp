#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Rules/ConcentrationRules.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FConcentrationRulesTest, "Project.Expedition.Rules.Concentration",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FConcentrationRulesTest::RunTest(const FString& Parameters)
{
    TestEqual(TEXT("Small hit DC 10"), FConcentrationRules::SaveDc(7), 10);
    TestEqual(TEXT("22 damage DC 11"), FConcentrationRules::SaveDc(22), 11);
    TestEqual(TEXT("Forecast uses the save math"), FConcentrationRules::KeepOutOf400(2, 30, ERollMode::Normal), FCheckRules::SuccessOutOf400(2, 15, ERollMode::Normal));
    TestTrue(TEXT("Second concentration spell ends the first"), FConcentrationRules::NewSpellEndsCurrent(true, true));
    TestFalse(TEXT("Other spells do not"), FConcentrationRules::NewSpellEndsCurrent(true, false));
    TestTrue(TEXT("Incapacitated drops it"), FConcentrationRules::EndsOnIncapacitated(true));
    for (int32 SpellIndex = 0; SpellIndex < (int32)ESpell::Count; ++SpellIndex)
    {
        TestFalse(TEXT("Current spells need no concentration"), FConcentrationRules::RequiresConcentration((ESpell)SpellIndex));
    }
    return !HasAnyErrors();
}
#endif
