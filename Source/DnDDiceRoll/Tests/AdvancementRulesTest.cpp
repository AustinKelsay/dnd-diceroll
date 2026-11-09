#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Rules/AdvancementRules.h"
#include "Rules/RestRules.h"
#include "Combat/CombatBattle.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAdvancementXpHpGatesTest, "Project.Expedition.Rules.AdvancementXpHpGates",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FAdvancementXpHpGatesTest::RunTest(const FString& Parameters)
{
    TestEqual(TEXT("L1 XP 0"), FAdvancementRules::XpForLevel(1), 0);
    TestEqual(TEXT("L2 XP 300"), FAdvancementRules::XpForLevel(2), 300);
    TestEqual(TEXT("L5 XP 6500"), FAdvancementRules::XpForLevel(5), 6500);
    TestEqual(TEXT("L15 XP 165000"), FAdvancementRules::XpForLevel(15), 165000);
    TestEqual(TEXT("299 XP still L1"), FAdvancementRules::LevelFromTotalXp(299), 1);
    TestEqual(TEXT("300 XP L2"), FAdvancementRules::LevelFromTotalXp(300), 2);
    TestEqual(TEXT("6500 XP L5"), FAdvancementRules::LevelFromTotalXp(6500), 5);
    TestEqual(TEXT("165000 XP L15"), FAdvancementRules::LevelFromTotalXp(165000), 15);
    TestEqual(TEXT("XP to L2 from 0"), FAdvancementRules::XpToNextLevel(1, 0), 300);
    TestEqual(TEXT("Pending 4 at L1/6500"), FAdvancementRules::PendingLevelUps(1, 6500), 4);

    TestEqual(TEXT("Fighter d10"), FAdvancementRules::HitDieSides(EAdventurerRole::Fighter), 10);
    TestEqual(TEXT("Wizard d6"), FAdvancementRules::HitDieSides(EAdventurerRole::Wizard), 6);
    TestEqual(TEXT("RestRules delegates Paladin"), FRestRules::HitDieSides(EAdventurerRole::Paladin), 10);

    TestEqual(TEXT("Fighter L1 Con+2: 12"), FAdvancementRules::MaxHpAtLevel1(EAdventurerRole::Fighter, 2), 12);
    TestEqual(TEXT("Rolled 7 + Con2 = 9"), FAdvancementRules::HpGainFromRoll(7, 2), 9);
    TestEqual(TEXT("Clamp roll 0 -> 1 + Con0 = 1"), FAdvancementRules::HpGainForClassLevel(EAdventurerRole::Wizard, 0, 0), 1);
    TestEqual(TEXT("Clamp roll 99 on d6 -> 6 + Con1 = 7"), FAdvancementRules::HpGainForClassLevel(EAdventurerRole::Wizard, 99, 1), 7);

    TestEqual(TEXT("Prof L5 is 3"), FAdvancementRules::ProficiencyBonus(5), 3);
    TestTrue(TEXT("Subclass at class L3"), FAdvancementRules::OffersSubclassAt(EAdventurerRole::Fighter, 3));
    TestTrue(TEXT("ASI at character L4"), FAdvancementRules::OffersAsiAt(4));
    TestFalse(TEXT("No ASI at L5"), FAdvancementRules::OffersAsiAt(5));

    TestEqual(TEXT("Wizard cantrips L4"), FAdvancementRules::CantripsKnown(EAdventurerRole::Wizard, 4), 4);
    TestEqual(TEXT("Bard known L1"), FAdvancementRules::SpellsKnownOrPrepared(EAdventurerRole::Bard, 1, 3), 4);

    // Multiclass
    TestTrue(TEXT("Fighter prereq STR or DEX 13"), FAdvancementRules::MeetsMulticlassPrerequisite(EAdventurerRole::Fighter, 13, 10, 10, 10, 10, 10));
    TestTrue(TEXT("Fighter prereq via DEX"), FAdvancementRules::MeetsMulticlassPrerequisite(EAdventurerRole::Fighter, 10, 13, 10, 10, 10, 10));
    TestFalse(TEXT("Fighter fails below 13"), FAdvancementRules::MeetsMulticlassPrerequisite(EAdventurerRole::Fighter, 12, 12, 10, 10, 10, 10));
    TestTrue(TEXT("Paladin needs STR and CHA"), FAdvancementRules::MeetsMulticlassPrerequisite(EAdventurerRole::Paladin, 13, 10, 10, 10, 10, 13));
    TestFalse(TEXT("Paladin fails CHA"), FAdvancementRules::MeetsMulticlassPrerequisite(EAdventurerRole::Paladin, 13, 10, 10, 10, 10, 12));

    FClassLevels Levels;
    Levels.Set(EAdventurerRole::Wizard, 3);
    Levels.Set(EAdventurerRole::Fighter, 2);
    TestEqual(TEXT("Character level 5"), Levels.CharacterLevel(), 5);
    TestEqual(TEXT("Multiclass caster level Wizard3 = 3"), FAdvancementRules::MulticlassSpellcasterLevel(Levels), 3);
    TestEqual(TEXT("Multiclass slots: 4 first at caster 3"), FAdvancementRules::MulticlassSlotsFor(Levels, 1), 4);
    TestEqual(TEXT("Multiclass slots: 2 second"), FAdvancementRules::MulticlassSlotsFor(Levels, 2), 2);

    FClassLevels Half;
    Half.Set(EAdventurerRole::Paladin, 4);
    TestEqual(TEXT("Paladin 4 -> caster 2"), FAdvancementRules::MulticlassSpellcasterLevel(Half), 2);
    FClassLevels Art;
    Art.Set(EAdventurerRole::Artificer, 1);
    TestEqual(TEXT("Artificer 1 -> caster 1 (round up)"), FAdvancementRules::MulticlassSpellcasterLevel(Art), 1);

    TestTrue(TEXT("Multiclass Fighter gains martial weapons"), FAdvancementRules::MulticlassProficienciesGained(EAdventurerRole::Fighter).Contains(TEXT("Martial weapons")));
    TestEqual(TEXT("Sorcerer multiclass proficiencies empty"), FAdvancementRules::MulticlassProficienciesGained(EAdventurerRole::Sorcerer).Num(), 0);

    const FClassAuraColor Wiz = FAdvancementRules::ClassColor(EAdventurerRole::Wizard);
    TestTrue(TEXT("Wizard blue-ish"), Wiz.B > Wiz.R && Wiz.B > Wiz.G);
    FClassLevels BlendLevels;
    BlendLevels.Set(EAdventurerRole::Wizard, 1);
    BlendLevels.Set(EAdventurerRole::Cleric, 1);
    const FClassAuraColor Blend = FAdvancementRules::BlendClassColors(BlendLevels);
    const FClassAuraColor Cleric = FAdvancementRules::ClassColor(EAdventurerRole::Cleric);
    TestTrue(TEXT("Blend between wizard and cleric"), Blend.R != Wiz.R || Blend.B != Wiz.B);
    TestTrue(TEXT("Blend not identical to cleric alone"), Blend.R != Cleric.R || Blend.B != Cleric.B || Blend.G != Cleric.G);
    return !HasAnyErrors();
}
#endif