#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Rules/ConcentrationRules.h"
#include "Rules/SpellRules.h"

/** F4 regression: RequiresConcentration reads FSpellFacts::bConcentration. */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRequiresConcentrationFlagRegressionTest, "Project.Expedition.Rules.RequiresConcentrationFlag",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRequiresConcentrationFlagRegressionTest::RunTest(const FString& Parameters)
{
    TestFalse(TEXT("Shield flag false"), FSpellRules::Facts(ESpell::Shield).bConcentration);
    TestFalse(TEXT("RequiresConcentration follows Shield flag"), FConcentrationRules::RequiresConcentration(ESpell::Shield));
    TestTrue(TEXT("Shield of Faith flag true"), FSpellRules::Facts(ESpell::ShieldOfFaith).bConcentration);
    TestTrue(TEXT("RequiresConcentration follows Shield of Faith flag"), FConcentrationRules::RequiresConcentration(ESpell::ShieldOfFaith));
    for (int32 SpellIndex = 0; SpellIndex < (int32)ESpell::Count; ++SpellIndex)
    {
        const ESpell Spell = (ESpell)SpellIndex;
        TestEqual(TEXT("RequiresConcentration mirrors facts"), FConcentrationRules::RequiresConcentration(Spell) ? 1 : 0, FSpellRules::Facts(Spell).bConcentration ? 1 : 0);
    }
    return !HasAnyErrors();
}
#endif
