#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Rules/AbilityRules.h"
#include "Combat/CombatBattle.h"
#include "Rules/AbilityRules.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAbilityModifierTest, "Project.Expedition.Rules.AbilityModifiers",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FAbilityModifierTest::RunTest(const FString& Parameters)
{
    // SRD worked examples: 1 -> -5, 8 -> -1, 10 -> 0, 11 -> 0, 20 -> +5, 30 -> +10.
    const int32 Scores[] = {1, 8, 9, 10, 11, 12, 20, 30};
    const int32 Expected[] = {-5, -1, -1, 0, 0, 1, 5, 10};
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(Scores); ++Index)
    {
        TestEqual(FString::Printf(TEXT("Modifier of %d"), Scores[Index]), FAbilityRules::Modifier(Scores[Index]), Expected[Index]);
    }
    FAbilityScores Scores6;
    TestTrue(TEXT("Default scores are valid"), Scores6.IsValid());
    Scores6.Wisdom = 31;
    TestFalse(TEXT("Scores above 30 are invalid"), Scores6.IsValid());
    Scores6.Wisdom = 0;
    TestFalse(TEXT("Scores below 1 are invalid"), Scores6.IsValid());
    TestEqual(TEXT("Abbreviation"), FString(FAbilityRules::AbilityAbbreviation(EAbility::Charisma)), FString(TEXT("CHA")));
    return !HasAnyErrors();
}

#endif
