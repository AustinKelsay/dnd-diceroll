
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

    // House rule: first 15 ft free, then 1d6 / 10 ft.
    TestEqual(TEXT("15 ft free drop"), FMovementRules::FallingDice(15), 0);
    TestEqual(TEXT("16-24 ft still free (no full 10)"), FMovementRules::FallingDice(24), 0);
    TestEqual(TEXT("25 ft is 1d6"), FMovementRules::FallingDice(25), 1);
    TestEqual(TEXT("35 ft is 2d6"), FMovementRules::FallingDice(35), 2);
    TestEqual(TEXT("Capped at 20d6"), FMovementRules::FallingDice(500), 20);

    const int32 FallSeedBefore = DiceSeed;
    const int32 FallTaken = FMovementRules::FallDamage(35, DiceSeed);
    TestTrue(TEXT("35 ft fall deals 2..12"), FallTaken >= 2 && FallTaken <= 12);
    TestNotEqual(TEXT("Fall advances the stream"), DiceSeed, FallSeedBefore);
    TestTrue(TEXT("Damaging fall leaves prone"), FMovementRules::LandsProne(25));
    TestFalse(TEXT("Free drop does not"), FMovementRules::LandsProne(15));

    TestEqual(TEXT("Stand from 6 cells costs 3"), FMovementRules::StandUpCost(6), 3);
    TestEqual(TEXT("Stand from 5 cells costs 3"), FMovementRules::StandUpCost(5), 3);
    TestEqual(TEXT("Crawl doubles"), FMovementRules::CrawlCost(2), 4);
    TestEqual(TEXT("Difficult terrain doubles"), FMovementRules::DifficultTerrainCost(3), 6);
    TestEqual(TEXT("Push stops at wall"), FMovementRules::PushDistance(2, 1), 1);
    TestEqual(TEXT("Free push"), FMovementRules::PushDistance(1, 5), 1);
    TestTrue(TEXT("Adjacent prone target"), FMovementRules::AttackAgainstProne(1) == ERollMode::Advantage);
    TestTrue(TEXT("Distant prone target"), FMovementRules::AttackAgainstProne(4) == ERollMode::Disadvantage);
    return !HasAnyErrors();
}
#endif
