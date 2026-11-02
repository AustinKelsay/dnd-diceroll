#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Combat/CombatBattle.h"
#include "Rules/ProficiencyRules.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSkillCatalogTest, "Project.Expedition.Rules.SkillCatalog",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSkillCatalogTest::RunTest(const FString& Parameters)
{
    const TCHAR* Names[] = {TEXT("Athletics"), TEXT("Acrobatics"), TEXT("Sleight of Hand"), TEXT("Stealth"), TEXT("Arcana"),
        TEXT("History"), TEXT("Investigation"), TEXT("Nature"), TEXT("Religion"), TEXT("Animal Handling"), TEXT("Insight"),
        TEXT("Medicine"), TEXT("Perception"), TEXT("Survival"), TEXT("Deception"), TEXT("Intimidation"), TEXT("Performance"), TEXT("Persuasion")};
    const EAbility Abilities[] = {EAbility::Strength, EAbility::Dexterity, EAbility::Dexterity, EAbility::Dexterity,
        EAbility::Intelligence, EAbility::Intelligence, EAbility::Intelligence, EAbility::Intelligence, EAbility::Intelligence,
        EAbility::Wisdom, EAbility::Wisdom, EAbility::Wisdom, EAbility::Wisdom, EAbility::Wisdom,
        EAbility::Charisma, EAbility::Charisma, EAbility::Charisma, EAbility::Charisma};
    TestEqual(TEXT("All 18 SRD skills"), FProficiencyRules::SkillCount, 18);
    for (int32 Index = 0; Index < 18; ++Index)
    {
        const ESkill Skill = static_cast<ESkill>(Index);
        TestEqual(FString::Printf(TEXT("Skill %d name"), Index), FString(FProficiencyRules::SkillName(Skill)), FString(Names[Index]));
        TestTrue(FString::Printf(TEXT("%s ability"), Names[Index]), FProficiencyRules::SkillAbility(Skill) == Abilities[Index]);
    }
    for (int32 Level = 1; Level <= 4; ++Level) { TestEqual(FString::Printf(TEXT("Proficiency at %d"), Level), FProficiencyRules::ProficiencyBonus(Level), 2); }
    TestEqual(TEXT("Proficiency at 5"), FProficiencyRules::ProficiencyBonus(5), 3);
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProficiencyBonusesTest, "Project.Expedition.Rules.ProficiencyAndExpertise",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FProficiencyBonusesTest::RunTest(const FString& Parameters)
{
    const FCharacterRules Rogue = FProficiencyRules::PrebuiltCharacter(EAdventurerRole::Rogue, 1);
    TestEqual(TEXT("Expertise doubles: Stealth DEX +3 + 2x2"), Rogue.SkillBonus(ESkill::Stealth), 7);
    TestEqual(TEXT("Proficient only: Acrobatics +3 + 2"), Rogue.SkillBonus(ESkill::Acrobatics), 5);
    TestEqual(TEXT("Unproficient: Athletics STR -1"), Rogue.SkillBonus(ESkill::Athletics), -1);
    TestEqual(TEXT("Thieves' tools proficiency, no tool Expertise in 5.2.1"), Rogue.ToolBonus(ETool::ThievesTools, EAbility::Dexterity), 5);
    TestTrue(TEXT("Rogue Expertise: two skills at level 1"), Rogue.HasExpertise(ESkill::SleightOfHand) && FMath::CountBits(Rogue.ExpertiseMask) == 2);
    TestEqual(TEXT("Rogue level 6 has four Expertise skills"), FMath::CountBits(FProficiencyRules::PrebuiltCharacter(EAdventurerRole::Rogue, 6).ExpertiseMask), 4);
    TestEqual(TEXT("No herbalism proficiency"), Rogue.ToolBonus(ETool::HerbalismKit, EAbility::Wisdom), 0);
    TestEqual(TEXT("Rogue DEX save proficient"), Rogue.SaveBonus(EAbility::Dexterity), 5);
    TestEqual(TEXT("Rogue STR save not proficient"), Rogue.SaveBonus(EAbility::Strength), -1);
    const FCharacterRules Fighter5 = FProficiencyRules::PrebuiltCharacter(EAdventurerRole::Fighter, 5);
    TestEqual(TEXT("Level 5 fighter Athletics +3 + 3"), Fighter5.SkillBonus(ESkill::Athletics), 6);
    TestEqual(TEXT("Level 5 fighter CON save +2 + 3"), Fighter5.SaveBonus(EAbility::Constitution), 5);
    TestFalse(TEXT("Fighter has no expertise"), Fighter5.HasExpertise(ESkill::Athletics));
    for (EAdventurerRole Role : {EAdventurerRole::Fighter, EAdventurerRole::Rogue, EAdventurerRole::Cleric, EAdventurerRole::Wizard})
    {
        const FCharacterRules Character = FProficiencyRules::PrebuiltCharacter(Role, 1);
        TestEqual(TEXT("Two save proficiencies"), FMath::CountBits(Character.SaveMask), 2);
        TestEqual(TEXT("Class + background skill count"), FMath::CountBits(Character.SkillMask), FProficiencyRules::ClassProficiencies(Role, 1).SkillPicks + 2);
    }
    FCombatBattle Battle;
    FString Error;
    TestTrue(TEXT("Tutorial starts"), Battle.Start(FCombatBattle::TutorialEncounter(), 9, Error));
    const FCombatSnapshot Before = Battle.Snapshot();
    int32 RogueId = 0, ArcherId = 0;
    for (const FCombatantState& Unit : Before.Encounter.Combatants)
    {
        if (Unit.AdventurerRole == EAdventurerRole::Rogue) { RogueId = Unit.Id; }
        if (Unit.Role == TEXT("Archer")) { ArcherId = Unit.Id; }
    }
    TestEqual(TEXT("Battle query uses the prebuilt rogue"), Battle.Character(RogueId).SkillBonus(ESkill::Stealth), 7);
    TestEqual(TEXT("Enemy query has a skill bonus"), Battle.Character(ArcherId).SkillBonus(ESkill::Perception), 3);
    TestTrue(TEXT("Queries leave state untouched"), FCombatSnapshot::StaticStruct()->CompareScriptStruct(&Before, &Battle.Snapshot(), 0));
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProficiencyRejectionTest, "Project.Expedition.Rules.InvalidProficiencyChoicesRejected",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FProficiencyRejectionTest::RunTest(const FString& Parameters)
{
    const FAbilityScores Scores = FAbilityRules::PrebuiltScores(EAdventurerRole::Fighter);
    const FCharacterRules Sentinel = FProficiencyRules::PrebuiltCharacter(EAdventurerRole::Wizard, 3);
    auto Rejected = [&](const TCHAR* What, EAdventurerRole Role, const FProficiencyChoice& Choice)
    {
        FCharacterRules Out = Sentinel;
        FString Error;
        TestFalse(What, FProficiencyRules::BuildCharacter(Role, 1, Scores, Choice, Out, Error));
        TestFalse(FString(What) + TEXT(" explains"), Error.IsEmpty());
        TestTrue(FString(What) + TEXT(" leaves output untouched"), Out.SkillMask == Sentinel.SkillMask && Out.Level == Sentinel.Level &&
            Out.SaveMask == Sentinel.SaveMask && Out.ExpertiseMask == Sentinel.ExpertiseMask && Out.Scores.Strength == Sentinel.Scores.Strength);
    };
    FProficiencyChoice Good = FProficiencyRules::PrebuiltChoice(EAdventurerRole::Fighter);
    FCharacterRules Built;
    FString Error;
    TestTrue(TEXT("Prebuilt fighter choice is legal"), FProficiencyRules::BuildCharacter(EAdventurerRole::Fighter, 1, Scores, Good, Built, Error));
    FProficiencyChoice Choice = Good; Choice.ClassSkills = {ESkill::Arcana, ESkill::Athletics};
    Rejected(TEXT("Off-list class skill"), EAdventurerRole::Fighter, Choice);
    Choice = Good; Choice.ClassSkills.Add(ESkill::History);
    Rejected(TEXT("Too many class skills"), EAdventurerRole::Fighter, Choice);
    Choice = Good; Choice.ClassSkills = {ESkill::Athletics, ESkill::Athletics};
    Rejected(TEXT("Repeated class skill"), EAdventurerRole::Fighter, Choice);
    Choice = Good; Choice.BackgroundSkills = {ESkill::Athletics, ESkill::Nature};
    Rejected(TEXT("Background repeats a class skill"), EAdventurerRole::Fighter, Choice);
    Choice = Good; Choice.ExpertiseSkills = {ESkill::Athletics};
    Rejected(TEXT("Fighter cannot take expertise"), EAdventurerRole::Fighter, Choice);
    Choice = FProficiencyRules::PrebuiltChoice(EAdventurerRole::Rogue); Choice.ExpertiseSkills = {ESkill::Arcana};
    Rejected(TEXT("Expertise on an unproficient skill"), EAdventurerRole::Rogue, Choice);
    Choice = FProficiencyRules::PrebuiltChoice(EAdventurerRole::Rogue); Choice.ExpertiseSkills = {ESkill::Stealth};
    Rejected(TEXT("Too few rogue expertise picks"), EAdventurerRole::Rogue, Choice);
    Choice = FProficiencyRules::PrebuiltChoice(EAdventurerRole::Rogue); Choice.ClassSkills[3] = ESkill::Performance;
    Rejected(TEXT("Performance is not a 5.2.1 Rogue skill"), EAdventurerRole::Rogue, Choice);
    Rejected(TEXT("No class"), EAdventurerRole::None, Good);
    // Wizard Scholar: one of Arcana, History, Investigation, Medicine, Nature, Religion, and only if proficient.
    const FAbilityScores WizScores = FAbilityRules::PrebuiltScores(EAdventurerRole::Wizard);
    FCharacterRules Wiz;
    TestTrue(TEXT("Prebuilt wizard 2 Scholar legal"), FProficiencyRules::BuildCharacter(EAdventurerRole::Wizard, 2, WizScores, FProficiencyRules::PrebuiltChoice(EAdventurerRole::Wizard, 2), Wiz, Error));
    TestTrue(TEXT("Scholar doubles Arcana"), Wiz.HasExpertise(ESkill::Arcana));
    TestFalse(TEXT("Wizard 1 has no Scholar"), FProficiencyRules::BuildCharacter(EAdventurerRole::Wizard, 1, WizScores, FProficiencyRules::PrebuiltChoice(EAdventurerRole::Wizard, 2), Wiz, Error));
    Choice = FProficiencyRules::PrebuiltChoice(EAdventurerRole::Wizard); Choice.ClassSkills = {ESkill::Arcana, ESkill::Insight}; Choice.ExpertiseSkills = {ESkill::Insight};
    TestFalse(TEXT("Scholar rejects Insight"), FProficiencyRules::BuildCharacter(EAdventurerRole::Wizard, 2, WizScores, Choice, Wiz, Error));
    Choice.ClassSkills = {ESkill::Arcana, ESkill::Investigation}; Choice.ExpertiseSkills = {ESkill::Nature};
    TestFalse(TEXT("Scholar needs proficiency"), FProficiencyRules::BuildCharacter(EAdventurerRole::Wizard, 2, WizScores, Choice, Wiz, Error));
    // Bard Expertise 2 at level 2 and 4 at 9; Ranger Deft Explorer 1 at 2 and Expertise 3 at 9.
    const int32 Levels[] = {1, 2, 9};
    const int32 BardExp[] = {0, 2, 4}, RangerExp[] = {0, 1, 3};
    for (int32 Index = 0; Index < 3; ++Index)
    {
        const FCharacterRules Bard = FProficiencyRules::PrebuiltCharacter(EAdventurerRole::Bard, Levels[Index]);
        const FCharacterRules Ranger = FProficiencyRules::PrebuiltCharacter(EAdventurerRole::Ranger, Levels[Index]);
        TestEqual(FString::Printf(TEXT("Bard %d expertise"), Levels[Index]), FMath::CountBits(Bard.ExpertiseMask), BardExp[Index]);
        TestEqual(FString::Printf(TEXT("Ranger %d expertise"), Levels[Index]), FMath::CountBits(Ranger.ExpertiseMask), RangerExp[Index]);
        TestEqual(TEXT("Bard 3 class + 2 background skills"), FMath::CountBits(Bard.SkillMask), 5);
        TestTrue(TEXT("Bard DEX/CHA saves"), Bard.IsSaveProficient(EAbility::Dexterity) && Bard.IsSaveProficient(EAbility::Charisma));
        TestTrue(TEXT("Ranger STR/DEX saves"), Ranger.IsSaveProficient(EAbility::Strength) && Ranger.IsSaveProficient(EAbility::Dexterity));
    }
    Choice = FProficiencyRules::PrebuiltChoice(EAdventurerRole::Ranger); Choice.ClassSkills[0] = ESkill::Arcana;
    TestFalse(TEXT("Arcana is not a Ranger skill"), FProficiencyRules::BuildCharacter(EAdventurerRole::Ranger, 1, Scores, Choice, Wiz, Error));
    Choice = FProficiencyRules::PrebuiltChoice(EAdventurerRole::Fighter); Choice.ClassSkills = {ESkill::Persuasion, ESkill::Athletics};
    TestTrue(TEXT("Persuasion is a 5.2.1 Fighter skill"), FProficiencyRules::BuildCharacter(EAdventurerRole::Fighter, 1, Scores, Choice, Wiz, Error));
    return !HasAnyErrors();
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAllSrdClassTablesTest, "Project.Expedition.Rules.AllSrdClassTables",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FAllSrdClassTablesTest::RunTest(const FString& Parameters)
{
    using S = ESkill;
    struct FRow { EAdventurerRole Role; EAbility SaveA; EAbility SaveB; int32 Picks; int32 ListSize; };
    const FRow Rows[] = {
        {EAdventurerRole::Barbarian, EAbility::Strength, EAbility::Constitution, 2, 6},
        {EAdventurerRole::Druid, EAbility::Intelligence, EAbility::Wisdom, 2, 8},
        {EAdventurerRole::Monk, EAbility::Strength, EAbility::Dexterity, 2, 6},
        {EAdventurerRole::Paladin, EAbility::Wisdom, EAbility::Charisma, 2, 6},
        {EAdventurerRole::Sorcerer, EAbility::Constitution, EAbility::Charisma, 2, 6},
        {EAdventurerRole::Warlock, EAbility::Wisdom, EAbility::Charisma, 2, 7}};
    for (const FRow& Row : Rows)
    {
        const FClassProficiencies Class = FProficiencyRules::ClassProficiencies(Row.Role, 1);
        TestEqual(TEXT("Skill picks"), Class.SkillPicks, Row.Picks);
        TestEqual(TEXT("Skill list size"), Class.SkillList.Num(), Row.ListSize);
        FProficiencyChoice Choice; Choice.bNoBackground = true; Choice.ClassSkills = {Class.SkillList[0], Class.SkillList[1]};
        FCharacterRules Character; FString Error;
        TestTrue(FString::Printf(TEXT("Class %d builds without a background"), static_cast<int32>(Row.Role)),
            FProficiencyRules::BuildCharacter(Row.Role, 1, FAbilityRules::PrebuiltScores(Row.Role), Choice, Character, Error));
        TestTrue(TEXT("Save A"), Character.IsSaveProficient(Row.SaveA));
        TestTrue(TEXT("Save B"), Character.IsSaveProficient(Row.SaveB));
        TestEqual(TEXT("Two skills"), FMath::CountBits(Character.SkillMask), 2);
    }
    TestTrue(TEXT("Druid has the Herbalism Kit"), FProficiencyRules::ClassProficiencies(EAdventurerRole::Druid, 1).ToolMask != 0);
    FProficiencyChoice Off; Off.bNoBackground = true; Off.ClassSkills = {S::Arcana, S::Stealth};
    FCharacterRules Ignored; FString Error;
    TestFalse(TEXT("Off-list skill rejected"), FProficiencyRules::BuildCharacter(EAdventurerRole::Sorcerer, 1, FAbilityRules::PrebuiltScores(EAdventurerRole::Sorcerer), Off, Ignored, Error));
    FProficiencyChoice Extra = Off; Extra.ClassSkills = {S::Arcana, S::Deception}; Extra.BackgroundSkills = {S::Stealth};
    TestFalse(TEXT("No-background build rejects background picks"), FProficiencyRules::BuildCharacter(EAdventurerRole::Sorcerer, 1, FAbilityRules::PrebuiltScores(EAdventurerRole::Sorcerer), Extra, Ignored, Error));
    FProficiencyChoice Art; Art.bNoBackground = true;
    TestFalse(TEXT("Artificer is not an SRD class"), FProficiencyRules::BuildCharacter(EAdventurerRole::Artificer, 1, FAbilityRules::PrebuiltScores(EAdventurerRole::Fighter), Art, Ignored, Error));
    return !HasAnyErrors();
}
#endif
