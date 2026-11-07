#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Rules/ProficiencyRules.h"
// Only for the EAdventurerRole definition, which still lives in CombatBattle.h; no battle code is used.
#include "Combat/CombatBattle.h"

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
    TestEqual(TEXT("Thieves' tools proficiency, no expertise (SRD 5.2.1 Expertise is skills only)"), Rogue.ToolBonus(ETool::ThievesTools, EAbility::Dexterity), 5);
    TestTrue(TEXT("Second expertise: Sleight of Hand"), Rogue.HasExpertise(ESkill::SleightOfHand));
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
    return !HasAnyErrors();
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPrebuiltProficiencyLevelsTest, "Project.Expedition.Rules.PrebuiltProficienciesAcrossLevels",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FPrebuiltProficiencyLevelsTest::RunTest(const FString& Parameters)
{
    for (int32 Level = 1; Level <= 20; ++Level)
    {
        const FCharacterRules Rogue = FProficiencyRules::PrebuiltCharacter(EAdventurerRole::Rogue, Level);
        TestEqual(TEXT("Prebuilt Rogue keeps requested level"), Rogue.Level, Level);
        TestEqual(TEXT("Rogue Expertise count changes at level 6"), FMath::CountBits(Rogue.ExpertiseMask), Level >= 6 ? 4 : 2);
        TestTrue(TEXT("Rogue always has Stealth Expertise"), Rogue.HasExpertise(ESkill::Stealth));
        TestEqual(TEXT("Additional Expertise starts at level 6"), Rogue.HasExpertise(ESkill::Acrobatics), Level >= 6);
        FCharacterRules Built;
        FString Error;
        TestTrue(TEXT("Level-aware prebuilt picks pass public validation"), FProficiencyRules::BuildCharacter(
            EAdventurerRole::Rogue, Level, Rogue.Scores, FProficiencyRules::PrebuiltChoice(EAdventurerRole::Rogue, Level), Built, Error));
    }
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
    Rejected(TEXT("No class"), EAdventurerRole::None, Good);
    return !HasAnyErrors();
}
#endif
