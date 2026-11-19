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
    TestTrue(TEXT("ASI at Fighter level 4"), FAdvancementRules::OffersAsiAt(EAdventurerRole::Fighter, 4));
    TestFalse(TEXT("No ASI at Fighter level 5"), FAdvancementRules::OffersAsiAt(EAdventurerRole::Fighter, 5));

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
    FClassLevels Pal1;
    Pal1.Set(EAdventurerRole::Paladin, 1);
    TestEqual(TEXT("Paladin 1 -> caster 1 (half rounded up)"), FAdvancementRules::MulticlassSpellcasterLevel(Pal1), 1);
    FClassLevels Mix;
    Mix.Set(EAdventurerRole::Wizard, 1);
    Mix.Set(EAdventurerRole::Paladin, 1);
    TestEqual(TEXT("Wizard 1 / Paladin 1 -> caster 2 (SRD 5.2.1 p. 25)"), FAdvancementRules::MulticlassSpellcasterLevel(Mix), 2);
    TestEqual(TEXT("Wizard 1 / Paladin 1 uses the multiclass table (3 first, not Paladin's 2)"), FAdvancementRules::MulticlassSlotsFor(Mix, 1), 3);
    TestEqual(TEXT("Wizard 1 / Paladin 1 has no 2nd-level slots"), FAdvancementRules::MulticlassSlotsFor(Mix, 2), 0);
    FClassLevels BothHalf;
    BothHalf.Set(EAdventurerRole::Paladin, 3);
    BothHalf.Set(EAdventurerRole::Ranger, 3);
    TestEqual(TEXT("Paladin 3 / Ranger 3 -> caster 4 (each half rounded up)"), FAdvancementRules::MulticlassSpellcasterLevel(BothHalf), 4);
    FClassLevels Pal3Wiz3;
    Pal3Wiz3.Set(EAdventurerRole::Paladin, 3);
    Pal3Wiz3.Set(EAdventurerRole::Wizard, 3);
    TestEqual(TEXT("Paladin 3 / Wizard 3 -> caster 5"), FAdvancementRules::MulticlassSpellcasterLevel(Pal3Wiz3), 5);
    FClassLevels SrdExample;
    SrdExample.Set(EAdventurerRole::Ranger, 4);
    SrdExample.Set(EAdventurerRole::Sorcerer, 3);
    TestEqual(TEXT("Ranger 4 / Sorcerer 3 -> caster 5 (SRD 5.2.1 example)"), FAdvancementRules::MulticlassSpellcasterLevel(SrdExample), 5);
    FClassLevels Art;
    Art.Set(EAdventurerRole::Artificer, 1);
    TestEqual(TEXT("Artificer 1 -> caster 1 (round up)"), FAdvancementRules::MulticlassSpellcasterLevel(Art), 1);

    TestTrue(TEXT("Multiclass Fighter gains martial weapons"), FAdvancementRules::MulticlassProficienciesGained(EAdventurerRole::Fighter).Contains(TEXT("Martial weapons")));
    TestEqual(TEXT("Sorcerer multiclass proficiencies empty"), FAdvancementRules::MulticlassProficienciesGained(EAdventurerRole::Sorcerer).Num(), 0);

    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSpellcastingCounts521Test, "Project.Expedition.Rules.SpellcastingCounts521",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSpellcastingCounts521Test::RunTest(const FString& Parameters)
{
    // #177 review finding 2: the Cantrips and Prepared Spells columns of the SRD 5.2.1 class tables (pp. 31, 36, 41, 53, 58, 65, 71, 77).
    using R = EAdventurerRole;
    TestEqual(TEXT("Ranger 1 prepares 2"), FAdvancementRules::SpellsKnownOrPrepared(R::Ranger, 1, 3), 2);
    TestEqual(TEXT("Sorcerer 3 prepares 6"), FAdvancementRules::SpellsKnownOrPrepared(R::Sorcerer, 3, 3), 6);
    TestEqual(TEXT("Cleric 5 with WIS +3 prepares 9"), FAdvancementRules::SpellsKnownOrPrepared(R::Cleric, 5, 3), 9);
    TestEqual(TEXT("Bard 5 prepares 9"), FAdvancementRules::SpellsKnownOrPrepared(R::Bard, 5, 3), 9);
    TestEqual(TEXT("Druid 1 knows 2 cantrips"), FAdvancementRules::CantripsKnown(R::Druid, 1), 2);
    TestEqual(TEXT("Cleric 4 knows 4 cantrips"), FAdvancementRules::CantripsKnown(R::Cleric, 4), 4);
    struct FRow { R Role; const TCHAR* Name; int32 Cantrips[5]; int32 Prepared[5]; };
    const FRow Rows[] = {
        {R::Bard, TEXT("Bard"), {2, 2, 2, 3, 3}, {4, 5, 6, 7, 9}},
        {R::Cleric, TEXT("Cleric"), {3, 3, 3, 4, 4}, {4, 5, 6, 7, 9}},
        {R::Druid, TEXT("Druid"), {2, 2, 2, 3, 3}, {4, 5, 6, 7, 9}},
        {R::Paladin, TEXT("Paladin"), {0, 0, 0, 0, 0}, {2, 3, 4, 5, 6}},
        {R::Ranger, TEXT("Ranger"), {0, 0, 0, 0, 0}, {2, 3, 4, 5, 6}},
        {R::Sorcerer, TEXT("Sorcerer"), {4, 4, 4, 5, 5}, {2, 4, 6, 7, 9}},
        {R::Warlock, TEXT("Warlock"), {2, 2, 2, 3, 3}, {2, 3, 4, 5, 6}},
        {R::Wizard, TEXT("Wizard"), {3, 3, 3, 4, 4}, {4, 5, 6, 7, 9}},
    };
    for (const FRow& Row : Rows)
    {
        for (int32 Level = 1; Level <= 5; ++Level)
        {
            TestEqual(FString::Printf(TEXT("%s %d cantrips"), Row.Name, Level), FAdvancementRules::CantripsKnown(Row.Role, Level), Row.Cantrips[Level - 1]);
            TestEqual(FString::Printf(TEXT("%s %d prepared spells"), Row.Name, Level), FAdvancementRules::SpellsKnownOrPrepared(Row.Role, Level, 3), Row.Prepared[Level - 1]);
            TestEqual(FString::Printf(TEXT("%s %d ignores the ability modifier"), Row.Name, Level), FAdvancementRules::SpellsKnownOrPrepared(Row.Role, Level, -1), FAdvancementRules::SpellsKnownOrPrepared(Row.Role, Level, 5));
        }
    }
    TestEqual(TEXT("A Fighter prepares no spells"), FAdvancementRules::SpellsKnownOrPrepared(R::Fighter, 5, 3), 0);
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAsiByClassLevelTest, "Project.Expedition.Rules.AsiByClassLevel",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FAsiByClassLevelTest::RunTest(const FString& Parameters)
{
    // #177 review finding 3: ASIs follow each class table (SRD 5.2.1), not total character level.
    using R = EAdventurerRole;
    TestFalse(TEXT("Fighter 2 / Rogue 2: the fourth character level (Rogue 2) grants no ASI"), FAdvancementRules::OffersAsiAt(R::Rogue, 2));
    TestFalse(TEXT("Fighter 2 / Rogue 2: nor would Fighter 2"), FAdvancementRules::OffersAsiAt(R::Fighter, 2));
    TestTrue(TEXT("Fighter 4 / Rogue 1: Fighter 4 at character level 5 grants an ASI"), FAdvancementRules::OffersAsiAt(R::Fighter, 4));
    TestTrue(TEXT("Fighter 6 grants an ASI"), FAdvancementRules::OffersAsiAt(R::Fighter, 6));
    TestTrue(TEXT("Fighter 14 grants an ASI"), FAdvancementRules::OffersAsiAt(R::Fighter, 14));
    TestFalse(TEXT("Fighter 10 does not"), FAdvancementRules::OffersAsiAt(R::Fighter, 10));
    TestTrue(TEXT("Rogue 10 grants an ASI"), FAdvancementRules::OffersAsiAt(R::Rogue, 10));
    TestFalse(TEXT("Rogue 6 does not"), FAdvancementRules::OffersAsiAt(R::Rogue, 6));
    TestFalse(TEXT("Wizard 6 does not"), FAdvancementRules::OffersAsiAt(R::Wizard, 6));
    TestFalse(TEXT("No class at all grants nothing"), FAdvancementRules::OffersAsiAt(R::None, 4));
    for (R Role : {R::Barbarian, R::Bard, R::Cleric, R::Druid, R::Fighter, R::Monk, R::Paladin, R::Ranger, R::Rogue, R::Sorcerer, R::Warlock, R::Wizard})
    {
        for (int32 Level : {4, 8, 12, 16}) { TestTrue(FString::Printf(TEXT("Class %d level %d grants an ASI"), (int32)Role, Level), FAdvancementRules::OffersAsiAt(Role, Level)); }
        for (int32 Level : {1, 2, 3, 5, 19, 20}) { TestFalse(FString::Printf(TEXT("Class %d level %d grants no ASI"), (int32)Role, Level), FAdvancementRules::OffersAsiAt(Role, Level)); }
    }
    return !HasAnyErrors();
}
#endif