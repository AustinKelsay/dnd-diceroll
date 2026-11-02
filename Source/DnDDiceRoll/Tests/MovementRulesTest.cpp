#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Rules/MovementRules.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMovementRulesTest, "Project.Expedition.Rules.FallingProneAndForcedMovement",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FMovementRulesTest::RunTest(const FString& Parameters)
{
    int32 DiceSeed = 7;
    TestEqual(TEXT("Three d1 sum to 3"), FDiceRules::Roll(3, 1, DiceSeed), 3);
    TestEqual(TEXT("No dice roll zero"), FDiceRules::Roll(0, 6, DiceSeed), 0);
    bool bDiceInRange = true;
    for (int32 Trial = 0; Trial < 200; ++Trial)
    {
        const int32 Rolled = FDiceRules::Roll(2, 6, DiceSeed);
        bDiceInRange &= Rolled >= 2 && Rolled <= 12;
    }
    TestTrue(TEXT("2d6 stays in 2..12"), bDiceInRange);
    TestEqual(TEXT("9 ft is no die"), FMovementRules::FallingDice(9), 0);
    TestEqual(TEXT("30 ft is 3d6"), FMovementRules::FallingDice(30), 3);
    TestEqual(TEXT("Capped at 20d6"), FMovementRules::FallingDice(500), 20);
    const int32 FallSeedBefore = DiceSeed;
    const int32 FallTaken = FMovementRules::FallDamage(30, DiceSeed);
    TestTrue(TEXT("30 ft fall deals 3..18"), FallTaken >= 3 && FallTaken <= 18);
    TestNotEqual(TEXT("Fall advances the stream"), DiceSeed, FallSeedBefore);
    TestTrue(TEXT("Fall leaves prone"), FMovementRules::LandsProne(10));
    TestFalse(TEXT("Short drop does not"), FMovementRules::LandsProne(5));
    TestEqual(TEXT("Stand from 6 cells costs 3"), FMovementRules::StandUpCost(6), 3);
    TestEqual(TEXT("Stand from 5 cells costs 3"), FMovementRules::StandUpCost(5), 3);
    TestEqual(TEXT("Crawl doubles"), FMovementRules::CrawlCost(2), 4);
    TestEqual(TEXT("Push stops at wall"), FMovementRules::PushDistance(2, 1), 1);
    TestEqual(TEXT("Free push"), FMovementRules::PushDistance(1, 5), 1);
    TestTrue(TEXT("Adjacent prone target"), FMovementRules::AttackAgainstProne(1) == ERollMode::Advantage);
    TestTrue(TEXT("Distant prone target"), FMovementRules::AttackAgainstProne(4) == ERollMode::Disadvantage);
    return !HasAnyErrors();
}
#endif
