#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Combat/CombatBattle.h"
#include "Rules/LevelRules.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLevelRulesTest, "Project.Expedition.Rules.LevelRules",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FLevelRulesTest::RunTest(const FString& Parameters)
{
    TestEqual(TEXT("Level 2 at 300 XP"), FLevelRules::XpForLevel(2), 300);
    TestEqual(TEXT("Level 5 at 6500 XP"), FLevelRules::XpForLevel(5), 6500);
    TestEqual(TEXT("6499 XP is level 4"), FLevelRules::LevelForXp(6499), 4);
    TestEqual(TEXT("355000 XP is level 20"), FLevelRules::LevelForXp(400000), 20);
    TestEqual(TEXT("Barbarian d12"), FLevelRules::HitDieSides(EAdventurerRole::Barbarian), 12);
    TestEqual(TEXT("Wizard d6"), FLevelRules::HitDieSides(EAdventurerRole::Wizard), 6);
    TestEqual(TEXT("Bard d8"), FLevelRules::HitDieSides(EAdventurerRole::Bard), 8);
    // Fighter CON +2: 12 at level 1, +8 a level after.
    TestEqual(TEXT("Fighter 1"), FLevelRules::MaxHitPoints(EAdventurerRole::Fighter, 1, EAdventurerRole::None, 0, 2), 12);
    TestEqual(TEXT("Fighter 5"), FLevelRules::MaxHitPoints(EAdventurerRole::Fighter, 5, EAdventurerRole::None, 0, 2), 44);
    TestEqual(TEXT("Fighter 3 / Wizard 2"), FLevelRules::MaxHitPoints(EAdventurerRole::Fighter, 3, EAdventurerRole::Wizard, 2, 2), 12 + 16 + 12);
    TestEqual(TEXT("At least 1 per level"), FLevelRules::AverageLevelHitPoints(EAdventurerRole::Wizard, -5), 1);
    TestTrue(TEXT("ASI at 4"), FLevelRules::IsAsiLevel(EAdventurerRole::Wizard, 4));
    TestTrue(TEXT("Fighter ASI at 6"), FLevelRules::IsAsiLevel(EAdventurerRole::Fighter, 6));
    TestFalse(TEXT("Wizard none at 6"), FLevelRules::IsAsiLevel(EAdventurerRole::Wizard, 6));
    TestEqual(TEXT("Rogue 10 has 3"), FLevelRules::AsiCount(EAdventurerRole::Rogue, 10), 3);
    FAbilityScores S; S.Strength = 19; FString Error;
    TestFalse(TEXT("Cap 20"), FLevelRules::ApplyAsi(S, EAbility::Strength, EAbility::Strength, Error));
    TestEqual(TEXT("Rejected ASI leaves scores"), S.Strength, 19);
    TestTrue(TEXT("+1/+1"), FLevelRules::ApplyAsi(S, EAbility::Strength, EAbility::Dexterity, Error));
    TestTrue(TEXT("Applied"), S.Strength == 20 && S.Dexterity == 11);
    FAbilityScores F; F.Strength = 16; F.Intelligence = 12;
    TestFalse(TEXT("Wizard needs INT 13"), FLevelRules::CanMulticlass(EAdventurerRole::Fighter, EAdventurerRole::Wizard, F, Error));
    F.Intelligence = 13;
    TestTrue(TEXT("Fighter -> Wizard"), FLevelRules::CanMulticlass(EAdventurerRole::Fighter, EAdventurerRole::Wizard, F, Error));
    FAbilityScores W; W.Intelligence = 16;
    TestFalse(TEXT("Wizard -> Fighter needs STR or DEX 13"), FLevelRules::CanMulticlass(EAdventurerRole::Wizard, EAdventurerRole::Fighter, W, Error));
    TestEqual(TEXT("Paladin 5 is caster 2"), FLevelRules::CasterLevel(EAdventurerRole::Paladin, 5), 2);
    TestEqual(TEXT("Artificer 3 is caster 2"), FLevelRules::CasterLevel(EAdventurerRole::Artificer, 3), 2);
    TestEqual(TEXT("Fighter is no caster"), FLevelRules::CasterLevel(EAdventurerRole::Fighter, 5), 0);
    TestEqual(TEXT("Caster 5: three 3rd-level? two"), FLevelRules::MulticlassSlots(5, 3), 2);
    TestEqual(TEXT("Caster 3: two 2nd"), FLevelRules::MulticlassSlots(3, 2), 2);
    TestEqual(TEXT("Caster 20: one 9th"), FLevelRules::MulticlassSlots(20, 9), 1);
    TestTrue(TEXT("Rogue gains thieves' tools"), FLevelRules::MulticlassProficiencies(EAdventurerRole::Rogue).Contains(TEXT("thieves")));
    return true;
}
#endif