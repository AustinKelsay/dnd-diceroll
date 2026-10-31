#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Combat/CombatBattle.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatStartTest, "Project.Expedition.Combat.ValidEncounter",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FCombatStartTest::RunTest(const FString& Parameters)
{
    FCombatEncounter Encounter;
    FCombatantState Ally;
    Ally.Id = 10;
    Ally.Cell = {0, 0};
    Encounter.Combatants.Add(Ally);
    FCombatantState Enemy;
    Enemy.Id = 20;
    Enemy.Side = ECombatSide::Enemy;
    Enemy.Cell = {7, 7};
    Encounter.Combatants.Add(Enemy);
    FCombatBattle Battle;
    FString Error;
    TestTrue(TEXT("A valid encounter can begin"), Battle.Start(Encounter, 42, Error));
    TestEqual(TEXT("Both combatants are retained"), Battle.Snapshot().Encounter.Combatants.Num(), 2);
    TestEqual(TEXT("Initiative includes everyone"), Battle.Snapshot().TurnOrder.Num(), 2);
    return !HasAnyErrors();
}

namespace
{
FCombatEncounter Duel()
{
    FCombatEncounter Encounter;
    Encounter.Width = Encounter.Height = 5;
    FCombatantState Ally;
    Ally.Id = 10;
    Ally.Cell = {0, 0};
    Ally.InitiativeBonus = 30;
    Encounter.Combatants.Add(Ally);
    FCombatantState Enemy;
    Enemy.Id = 20;
    Enemy.Side = ECombatSide::Enemy;
    Enemy.Cell = {4, 4};
    Enemy.InitiativeBonus = -10;
    Encounter.Combatants.Add(Enemy);
    return Encounter;
}
bool SameState(const FCombatSnapshot& A, const FCombatSnapshot& B)
{
    return FCombatSnapshot::StaticStruct()->CompareScriptStruct(&A, &B, 0);
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatMovementTest, "Project.Expedition.Combat.MovementAndAtomicRejection",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FCombatMovementTest::RunTest(const FString& Parameters)
{
    FCombatEncounter Encounter = Duel();
    Encounter.BlockedCells.Add({1, 0});
    FCombatBattle Battle;
    FString Error;
    if (!TestTrue(TEXT("Starts"), Battle.Start(Encounter, 42, Error))) { return false; }
    TestEqual(TEXT("Guaranteed initiative modifier wins"), Battle.ActiveId(), 10);
    TestTrue(TEXT("Reachable preview contains a legal destination"), Battle.LegalMoves(10).Contains(FIntPoint(0, 2)));
    TestFalse(TEXT("Preview excludes terrain"), Battle.LegalMoves(10).Contains(FIntPoint(1, 0)));
    const FCombatSnapshot Before = Battle.Snapshot();
    const TArray<FCombatCommand> Illegal = {
        {ECombatCommandType::Move, 20, {3, 4}},
        {ECombatCommandType::Move, 10, {1, 0}},
        {ECombatCommandType::Move, 10, {-1, 0}},
        {ECombatCommandType::Move, 10, {4, 4}},
        {ECombatCommandType::Attack, 10, {}, 20},
        {ECombatCommandType::EndTurn, 999}
    };
    for (const FCombatCommand& Command : Illegal)
    {
        TestFalse(TEXT("Illegal command rejected"), Battle.Submit(Command).bAccepted);
        TestTrue(TEXT("Rejection preserves entire snapshot including RNG"), SameState(Before, Battle.Snapshot()));
    }
    TestTrue(TEXT("Move accepted"), Battle.Submit({ECombatCommandType::Move, 10, {0, 2}}).bAccepted);
    TestEqual(TEXT("Movement costs shortest cardinal path"), Battle.Snapshot().MovementLeft, 2);
    TestEqual(TEXT("Position changes"), Battle.Find(10)->Cell, FIntPoint(0, 2));
    TestFalse(TEXT("Cannot overspend remaining movement"), Battle.Submit({ECombatCommandType::Move, 10, {3, 2}}).bAccepted);
    Encounter.Combatants[1].Cell = Encounter.Combatants[0].Cell;
    const FCombatSnapshot After = Battle.Snapshot();
    TestFalse(TEXT("Overlapping encounter rejected"), Battle.Start(Encounter, 9, Error));
    TestTrue(TEXT("Invalid restart preserves battle"), SameState(After, Battle.Snapshot()));
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatAttackTest, "Project.Expedition.Combat.AttackRollsAndBudgets",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FCombatAttackTest::RunTest(const FString& Parameters)
{
    // Worked examples from pinned FRandomStream: two initiative d20 draws precede attack.
    // Seed 42: attack 12, d6 4. Seed 28: natural 1. Seed 0: natural 20, d6 6 + 2.
    struct FExample { int32 Seed; int32 AC; int32 Roll; bool bHit; bool bCritical; int32 Damage; };
    const FExample Examples[] = {{42, 17, 12, true, false, 7}, {42, 18, 12, false, false, 0},
        {28, 1, 1, false, false, 0}, {0, 40, 20, true, true, 11}};
    for (const FExample& Example : Examples)
    {
        FCombatEncounter Encounter = Duel();
        Encounter.Combatants[1].Cell = {1, 0};
        Encounter.Combatants[1].ArmorClass = Example.AC;
        FCombatBattle Battle;
        FString Error;
        if (!TestTrue(TEXT("Starts"), Battle.Start(Encounter, Example.Seed, Error))) { return false; }
        TestTrue(TEXT("Legal target preview"), Battle.LegalTargets(10).Contains(20));
        const FCombatResult Result = Battle.Submit({ECombatCommandType::Attack, 10, {}, 20});
        TestTrue(TEXT("Attack accepted"), Result.bAccepted);
        TestEqual(TEXT("Known natural roll"), Result.NaturalRoll, Example.Roll);
        TestEqual(TEXT("Armor/automatic hit rule"), Result.bHit, Example.bHit);
        TestEqual(TEXT("Critical rule"), Result.bCritical, Example.bCritical);
        TestEqual(TEXT("Damage dice doubled, modifier once"), Result.Damage, Example.Damage);
        TestEqual(TEXT("HP changes by actual damage"), Battle.Find(20)->HP, 20 - Example.Damage);
        const FCombatSnapshot After = Battle.Snapshot();
        TestFalse(TEXT("Second attack rejected"), Battle.Submit({ECombatCommandType::Attack, 10, {}, 20}).bAccepted);
        TestTrue(TEXT("Duplicate cannot consume RNG"), SameState(After, Battle.Snapshot()));
        TestTrue(TEXT("Movement remains available after attack"), Battle.Submit({ECombatCommandType::Move, 10, {0, 1}}).bAccepted);
        TestTrue(TEXT("End turn advances to enemy"), Battle.Submit({ECombatCommandType::EndTurn, 10}).bAccepted);
        TestEqual(TEXT("Enemy turn"), Battle.ActiveId(), 20);
        const TArray<FCombatResult> EnemyResults = Battle.PlayEnemyTurn();
        TestTrue(TEXT("Enemy does bounded legal work"), EnemyResults.Num() >= 1 && EnemyResults.Num() <= 3);
        for (const FCombatResult& EnemyResult : EnemyResults) { TestTrue(TEXT("Every enemy command is legal"), EnemyResult.bAccepted); }
        TestEqual(TEXT("Turn returns to party"), Battle.ActiveId(), 10);
        TestEqual(TEXT("New round"), Battle.Snapshot().Round, 2);
        TestEqual(TEXT("Movement resets"), Battle.Snapshot().MovementLeft, 4);
        TestTrue(TEXT("Action resets"), Battle.Snapshot().bActionAvailable);
    }
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatSightTest, "Project.Expedition.Combat.LineOfSightAndFriendlyTargets",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FCombatSightTest::RunTest(const FString& Parameters)
{
    FCombatEncounter Encounter = Duel();
    Encounter.Combatants[0].Range = 8;
    Encounter.Combatants[1].Cell = {2, 2};
    Encounter.BlockedCells = {{1, 0}}; // diagonal corner is obscured
    FCombatBattle Battle;
    FString Error;
    Battle.Start(Encounter, 42, Error);
    const FCombatSnapshot Before = Battle.Snapshot();
    TestTrue(TEXT("Blocked sight excludes target"), Battle.LegalTargets(10).IsEmpty());
    TestFalse(TEXT("Cannot attack through a corner"), Battle.Submit({ECombatCommandType::Attack, 10, {}, 20}).bAccepted);
    TestFalse(TEXT("Cannot attack self"), Battle.Submit({ECombatCommandType::Attack, 10, {}, 10}).bAccepted);
    TestFalse(TEXT("Cannot attack missing target"), Battle.Submit({ECombatCommandType::Attack, 10, {}, 999}).bAccepted);
    TestFalse(TEXT("Unknown command rejected"), Battle.Submit({static_cast<ECombatCommandType>(255), 10}).bAccepted);
    TestTrue(TEXT("All rejections and previews preserve state"), SameState(Before, Battle.Snapshot()));
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatTerminalTest, "Project.Expedition.Combat.VictoryDefeatAndInactiveTurns",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FCombatTerminalTest::RunTest(const FString& Parameters)
{
    for (bool bPartyWins : {true, false})
    {
        FCombatEncounter Encounter = Duel();
        Encounter.Combatants[1].Cell = {1, 0};
        if (!bPartyWins)
        {
            Encounter.Combatants[0].Side = ECombatSide::Enemy;
            Encounter.Combatants[1].Side = ECombatSide::Party;
        }
        Encounter.Combatants[1].HP = 1;
        FCombatBattle Battle;
        FString Error;
        Battle.Start(Encounter, 0, Error); // guaranteed natural 20 for this worked seed
        TestTrue(TEXT("Finishing attack"), Battle.Submit({ECombatCommandType::Attack, 10, {}, 20}).bAccepted);
        TestEqual(TEXT("Zero HP clamped"), Battle.Find(20)->HP, 0);
        TestTrue(TEXT("Expected terminal"), Battle.Snapshot().Phase == (bPartyWins ? EBattlePhase::Victory : EBattlePhase::Defeat));
        const FCombatSnapshot Terminal = Battle.Snapshot();
        TestFalse(TEXT("Terminal cannot act"), Battle.Submit({ECombatCommandType::EndTurn, 10}).bAccepted);
        TestTrue(TEXT("Terminal enemies do nothing"), Battle.PlayEnemyTurn().IsEmpty());
        TestTrue(TEXT("Terminal state preserved"), SameState(Terminal, Battle.Snapshot()));
    }
    FCombatEncounter Encounter = Duel();
    Encounter.Combatants[1].Cell = {1, 0}; Encounter.Combatants[1].HP = 1;
    FCombatantState SecondEnemy = Encounter.Combatants[1];
    SecondEnemy.Id = 30; SecondEnemy.HP = 20; SecondEnemy.Cell = {4, 4};
    Encounter.Combatants.Add(SecondEnemy);
    FCombatBattle Battle;
    FString Error;
    // Force a hit for every non-natural-1 roll while keeping initiative first.
    Encounter.Combatants[0].AttackBonus = 30;
    Battle.Start(Encounter, 42, Error);
    Battle.Submit({ECombatCommandType::Attack, 10, {}, 20});
    TestEqual(TEXT("First enemy inactive"), Battle.Find(20)->HP, 0);
    Battle.Submit({ECombatCommandType::EndTurn, 10});
    TestEqual(TEXT("Turn skips zero HP enemy"), Battle.ActiveId(), 30);
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatEnemyTest, "Project.Expedition.Combat.EnemyBoundedAndDeterministic",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FCombatEnemyTest::RunTest(const FString& Parameters)
{
    FCombatEncounter Encounter = Duel();
    // A sealed enemy cannot approach; it must still end its turn.
    Encounter.BlockedCells = {{3, 4}, {4, 3}};
    FCombatBattle A, B;
    FString Error;
    A.Start(Encounter, 42, Error); B.Start(Encounter, 42, Error);
    TestTrue(TEXT("Party cannot be automated"), A.PlayEnemyTurn().IsEmpty());
    A.Submit({ECombatCommandType::EndTurn, 10}); B.Submit({ECombatCommandType::EndTurn, 10});
    const TArray<FCombatResult> Results = A.PlayEnemyTurn(); B.PlayEnemyTurn();
    TestEqual(TEXT("Sealed enemy ends immediately"), Results.Num(), 1);
    TestTrue(TEXT("Enemy ended legally"), Results[0].bAccepted);
    TestEqual(TEXT("Party turn restored"), A.ActiveId(), 10);
    TestTrue(TEXT("Identical seed/commands give same full state"), SameState(A.Snapshot(), B.Snapshot()));
    // Longer real arena sequence uses only previewed public commands on both replicas.
    A.Start(FCombatBattle::TutorialEncounter(), 7, Error); B.Start(FCombatBattle::TutorialEncounter(), 7, Error);
    for (int32 Step = 0; Step < 300 && A.Snapshot().Phase == EBattlePhase::Fighting; ++Step)
    {
        const int32 Id = A.ActiveId();
        if (A.Find(Id)->Side == ECombatSide::Enemy) { A.PlayEnemyTurn(); B.PlayEnemyTurn(); }
        else
        {
            TArray<int32> Targets = A.LegalTargets(Id);
            if (!Targets.IsEmpty())
            {
                const FCombatCommand Attack{ECombatCommandType::Attack, Id, {}, Targets[0]};
                A.Submit(Attack); B.Submit(Attack);
            }
            const FCombatCommand End{ECombatCommandType::EndTurn, Id};
            A.Submit(End); B.Submit(End);
        }
        TestTrue(TEXT("Replay deterministic after every turn"), SameState(A.Snapshot(), B.Snapshot()));
    }
    TestTrue(TEXT("Enemies finish a passive party within bounded turns"), A.Snapshot().Phase == EBattlePhase::Defeat);
    return !HasAnyErrors();
}
#endif
