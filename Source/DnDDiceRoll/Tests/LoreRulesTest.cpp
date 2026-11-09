#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Rules/LoreRules.h"
#include "Rules/ProficiencyRules.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLoreChecksTest, "Project.Expedition.Rules.KnowledgeAndWisdomChecks",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FLoreChecksTest::RunTest(const FString& Parameters)
{
    TestTrue(TEXT("Religion recalls undead"), FLoreRules::RecallSkill(TEXT("Skeleton")) == ESkill::Religion);
    TestTrue(TEXT("Arcana recalls elementals"), FLoreRules::RecallSkill(TEXT("Ice Mephit")) == ESkill::Arcana);
    TestTrue(TEXT("Nature recalls beasts"), FLoreRules::RecallSkill(TEXT("Wolf")) == ESkill::Nature);
    TestTrue(TEXT("History recalls humanoids"), FLoreRules::RecallSkill(TEXT("Bandit")) == ESkill::History);
    TestEqual(TEXT("Arcana recall is medium"), FLoreRules::RecallDc(TEXT("Fire Elemental")), 15);
    TestEqual(TEXT("Miss reveals nothing"), FLoreRules::FactsRevealed(9, 10), 0);
    TestEqual(TEXT("Success reveals one"), FLoreRules::FactsRevealed(10, 10), 1);
    TestEqual(TEXT("+5 reveals two"), FLoreRules::FactsRevealed(15, 10), 2);
    TestEqual(TEXT("Capped at three"), FLoreRules::FactsRevealed(30, 10), 3);
    TestTrue(TEXT("Arcana identifies runes at DC 15"), FLoreRules::IdentifySkill(true) == ESkill::Arcana && FLoreRules::IdentifyDc(true) == 15);
    TestTrue(TEXT("History identifies heirlooms at DC 10"), FLoreRules::IdentifySkill(false) == ESkill::History && FLoreRules::IdentifyDc(false) == 10);
    TestEqual(TEXT("Medicine treats level 3 for 3"), FLoreRules::TreatmentHitPoints(12, 3), 3);
    TestEqual(TEXT("Failed Medicine treats nothing"), FLoreRules::TreatmentHitPoints(9, 3), 0);
    TestTrue(TEXT("Insight 13 reads a +3 liar"), FLoreRules::ReadsIntent(13, 3));
    TestFalse(TEXT("Insight 12 does not"), FLoreRules::ReadsIntent(12, 3));
    return !HasAnyErrors();
}
#endif
