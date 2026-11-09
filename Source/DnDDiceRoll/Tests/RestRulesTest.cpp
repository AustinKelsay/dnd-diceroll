#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Rules/RestRules.h"
#include "Combat/CombatBattle.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRestRulesTest, "Project.Expedition.Rules.RestAndHitDice",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRestRulesTest::RunTest(const FString& Parameters)
{
    TestEqual(TEXT("Fighter d10"), FRestRules::HitDieSides(EAdventurerRole::Fighter), 10);
    TestEqual(TEXT("Rogue d8"), FRestRules::HitDieSides(EAdventurerRole::Rogue), 8);
    TestEqual(TEXT("Cleric d8"), FRestRules::HitDieSides(EAdventurerRole::Cleric), 8);
    TestEqual(TEXT("Wizard d6"), FRestRules::HitDieSides(EAdventurerRole::Wizard), 6);
    TestEqual(TEXT("Roll plus Con"), FRestRules::HitDieHealing(5, 2), 7);
    TestEqual(TEXT("Never negative"), FRestRules::HitDieHealing(1, -3), 0);
    TestEqual(TEXT("Level 1 regains 1"), FRestRules::HitDiceAfterLongRest(0, 1), 1);
    TestEqual(TEXT("Level 5 regains 2"), FRestRules::HitDiceAfterLongRest(1, 5), 3);
    TestEqual(TEXT("Capped at level"), FRestRules::HitDiceAfterLongRest(4, 5), 5);
    TestTrue(TEXT("Second Wind on short rest"), FRestRules::RefreshedBy(ERestPool::SecondWind) == ERestKind::ShortRest);
    TestTrue(TEXT("Superiority on short rest"), FRestRules::RefreshedBy(ERestPool::Superiority) == ERestKind::ShortRest);
    TestTrue(TEXT("Slots on long rest"), FRestRules::RefreshedBy(ERestPool::SpellSlots) == ERestKind::LongRest);
    TestTrue(TEXT("Arcane Recovery once a day"), FRestRules::RefreshedBy(ERestPool::ArcaneRecovery) == ERestKind::LongRest);
    TestTrue(TEXT("Camp is a long rest"), FRestRules::CampRest() == ERestKind::LongRest);
    return !HasAnyErrors();
}
#endif
