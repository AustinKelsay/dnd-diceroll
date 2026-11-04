#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Rules/ProficiencyRules.h"
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
    Choice = FProficiencyRules::PrebuiltChoice(EAdventurerRole::Rogue); Choice.bThievesToolsExpertise = false;
    Rejected(TEXT("Too few rogue expertise picks"), EAdventurerRole::Rogue, Choice);
    Rejected(TEXT("No class"), EAdventurerRole::None, Good);
    return !HasAnyErrors();
}
#endif
