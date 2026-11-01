#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAbilityPrebuiltProfilesTest, "Project.Expedition.Rules.AbilitiesMatchPrebuiltProfiles",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FAbilityPrebuiltProfilesTest::RunTest(const FString& Parameters)
{
    FCombatBattle Battle;
    FString Error;
    TestTrue(TEXT("Tutorial starts"), Battle.Start(FCombatBattle::TutorialEncounter(), 7, Error));
    const FCombatSnapshot Before = Battle.Snapshot();
    for (const FCombatantState& Unit : Before.Encounter.Combatants)
    {
        const FAbilityScores Scores = Battle.AbilityScores(Unit.Id);
        TestTrue(FString::Printf(TEXT("%s has valid scores"), *Unit.Name), Scores.IsValid());
        if (Unit.Side != ECombatSide::Party) { continue; }
        // Level 1 proficiency is +2; the prebuilt attack bonus must equal key modifier + 2.
        const int32 Key = FAbilityRules::Modifier(Scores.Get(FAbilityRules::AttackAbility(Unit.AdventurerRole)));
        TestEqual(FString::Printf(TEXT("%s attack bonus derives from abilities"), *Unit.Name), Key + 2, Unit.AttackBonus);
        TestEqual(FString::Printf(TEXT("%s modifier query"), *Unit.Name),
            Battle.AbilityModifier(Unit.Id, FAbilityRules::AttackAbility(Unit.AdventurerRole)), Key);
    }
    const FCombatantState& Fighter = Before.Encounter.Combatants[0];
    TestEqual(TEXT("Fighter initiative derives from DEX"), FAbilityRules::Modifier(Battle.AbilityScores(Fighter.Id).Dexterity), Fighter.InitiativeBonus);
    TestEqual(TEXT("Unknown combatants have no modifier"), Battle.AbilityModifier(999, EAbility::Strength), 0);
    TestTrue(TEXT("Queries spend no RNG and change no state"),
        FCombatSnapshot::StaticStruct()->CompareScriptStruct(&Before, &Battle.Snapshot(), 0));
    return !HasAnyErrors();
}
#endif
