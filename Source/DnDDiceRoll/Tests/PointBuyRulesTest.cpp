#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Combat/CombatBattle.h"
#include "Rules/PointBuyRules.h"

namespace PointBuyTestLocal
{
FAbilityScores PbMake(int32 Str, int32 Dex, int32 Con, int32 Int, int32 Wis, int32 Cha)
{
    FAbilityScores S; S.Strength = Str; S.Dexterity = Dex; S.Constitution = Con; S.Intelligence = Int; S.Wisdom = Wis; S.Charisma = Cha; return S;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPointBuyCostTableTest, "Project.Expedition.Creator.PointBuyCosts",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FPointBuyCostTableTest::RunTest(const FString& Parameters)
{
    const int32 Expected[] = {0, 1, 2, 3, 4, 5, 7, 9};
    for (int32 Score = 8; Score <= 15; ++Score) { TestEqual(FString::Printf(TEXT("Cost of %d"), Score), FPointBuyRules::Cost(Score), Expected[Score - 8]); }
    TestEqual(TEXT("7 is below point buy"), FPointBuyRules::Cost(7), INDEX_NONE);
    TestEqual(TEXT("16 is above point buy"), FPointBuyRules::Cost(16), INDEX_NONE);
    TestEqual(TEXT("Baseline spends nothing"), FPointBuyRules::Remaining(FPointBuyRules::Baseline()), 27);
    TestEqual(TEXT("SRD example: a 14 costs 7"), FPointBuyRules::Cost(14), 7);
    TestEqual(TEXT("Raise 13->14 costs 2"), FPointBuyRules::RaiseCost(PointBuyTestLocal::PbMake(13, 8, 8, 8, 8, 8), EAbility::Strength), 2);
    TestEqual(TEXT("Raise 15 impossible"), FPointBuyRules::RaiseCost(PointBuyTestLocal::PbMake(15, 8, 8, 8, 8, 8), EAbility::Strength), INDEX_NONE);
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPointBuyValidationTest, "Project.Expedition.Creator.PointBuyValidation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FPointBuyValidationTest::RunTest(const FString& Parameters)
{
    using namespace PointBuyTestLocal;
    FString Error;
    TestTrue(TEXT("Standard array is exactly 27"), FPointBuyRules::Validate(PbMake(15, 14, 13, 12, 10, 8), Error));
    TestEqual(TEXT("Standard array leaves 0"), FPointBuyRules::Remaining(PbMake(15, 14, 13, 12, 10, 8)), 0);
    TestTrue(TEXT("Three 15s cost 27"), FPointBuyRules::Validate(PbMake(15, 15, 15, 8, 8, 8), Error));
    TestFalse(TEXT("Over budget rejected"), FPointBuyRules::Validate(PbMake(15, 15, 15, 9, 8, 8), Error));
    TestFalse(TEXT("16 rejected"), FPointBuyRules::Validate(PbMake(16, 8, 8, 8, 8, 8), Error));
    TestFalse(TEXT("7 rejected"), FPointBuyRules::Validate(PbMake(7, 8, 8, 8, 8, 8), Error));
    TestTrue(TEXT("Underspending is legal"), FPointBuyRules::Validate(PbMake(10, 10, 10, 10, 10, 10), Error));
    // Stepping: raises stop exactly when the next step would overspend.
    FAbilityScores S = PbMake(15, 15, 14, 8, 8, 8); // 9+9+7 = 25, 2 left
    TestTrue(TEXT("14->15 needs 2, 2 left"), FPointBuyRules::CanRaise(S, EAbility::Constitution));
    TestTrue(TEXT("Raise applies"), FPointBuyRules::Raise(S, EAbility::Constitution));
    TestEqual(TEXT("All spent"), FPointBuyRules::Remaining(S), 0);
    TestFalse(TEXT("No more raises"), FPointBuyRules::Raise(S, EAbility::Intelligence));
    TestEqual(TEXT("Unchanged on refusal"), S.Intelligence, 8);
    TestFalse(TEXT("Cannot lower below 8"), FPointBuyRules::Lower(S, EAbility::Intelligence));
    TestTrue(TEXT("Lower refunds"), FPointBuyRules::Lower(S, EAbility::Strength));
    TestEqual(TEXT("15->14 refunds 2"), FPointBuyRules::Remaining(S), 2);
    TestEqual(TEXT("Forecast 14 -> +2"), FPointBuyRules::ModifierForecast(S, EAbility::Strength), 2);
    TestEqual(TEXT("Forecast 8 -> -1"), FPointBuyRules::ModifierForecast(S, EAbility::Wisdom), -1);
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPointBuyRecommendedTest, "Project.Expedition.Creator.RecommendedArrays",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FPointBuyRecommendedTest::RunTest(const FString& Parameters)
{
    int32 Classes = 0;
    for (uint8 Raw = 0; Raw <= static_cast<uint8>(EAdventurerRole::Artificer); ++Raw)
    {
        const EAdventurerRole Role = static_cast<EAdventurerRole>(Raw);
        if (!FPointBuyRules::IsSrdClass(Role)) { continue; }
        ++Classes;
        FString Error;
        const FAbilityScores S = FPointBuyRules::Recommended(Role);
        TestTrue(FString::Printf(TEXT("Class %d array is legal"), Raw), FPointBuyRules::Validate(S, Error));
        TestEqual(FString::Printf(TEXT("Class %d array spends 27"), Raw), FPointBuyRules::Remaining(S), 0);
    }
    TestEqual(TEXT("12 SRD classes"), Classes, 12);
    TestFalse(TEXT("Artificer is not SRD"), FPointBuyRules::IsSrdClass(EAdventurerRole::Artificer));
    TestEqual(TEXT("Wizard INT 15"), FPointBuyRules::Recommended(EAdventurerRole::Wizard).Intelligence, 15);
    TestEqual(TEXT("Cleric DEX 8"), FPointBuyRules::Recommended(EAdventurerRole::Cleric).Dexterity, 8);
    return !HasAnyErrors();
}
#endif
