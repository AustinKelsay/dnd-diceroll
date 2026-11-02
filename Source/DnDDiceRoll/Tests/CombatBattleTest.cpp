#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Combat/CombatBattle.h"
#include "Party/BuildCatalog.h"

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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatGridBoundsTest, "Project.Expedition.Combat.GridBoundsAllowLargeEncounters",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FCombatGridBoundsTest::RunTest(const FString& Parameters)
{
    FString Error;
    FCombatEncounter Large = Duel();
    Large.Width = 18; Large.Height = 12;
    Large.Combatants[1].Cell = {17, 11};
    TestTrue(TEXT("18 x 12 encounter is legal"), FCombatBattle().Start(Large, 42, Error));
    Large.Width = 24; Large.Height = 24;
    TestTrue(TEXT("24 x 24 is the largest legal grid"), FCombatBattle().Start(Large, 42, Error));
    Large.Width = 25;
    TestFalse(TEXT("Wider than 24 is rejected"), FCombatBattle().Start(Large, 42, Error));
    TestFalse(TEXT("Clear reason"), Error.IsEmpty());
    return !HasAnyErrors();
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
    // Deliberate change (enemy AI): a foe that cannot reach anyone takes the Dodge action before ending its turn.
    if (!TestEqual(TEXT("Sealed enemy Dodges, then ends"), Results.Num(), 2)) { return false; }
    TestTrue(TEXT("Dodge accepted"), Results[0].bAccepted && Results[0].Feedback.Contains(TEXT("dodges")));
    TestTrue(TEXT("Enemy ended legally"), Results[1].bAccepted);
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

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatPresentationFactsTest, "Project.Expedition.Combat.PresentationFactsAndForecast",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FCombatPresentationFactsTest::RunTest(const FString& Parameters)
{
    FCombatEncounter Encounter = Duel();
    Encounter.BlockedCells.Add({1, 0});
    FCombatBattle Battle;
    FString Error;
    if (!TestTrue(TEXT("Starts"), Battle.Start(Encounter, 42, Error))) { return false; }
    TestTrue(TEXT("Illegal destination has no path"), Battle.PathTo(10, {1, 0}).IsEmpty());
    const TArray<FIntPoint> Expected = Battle.PathTo(10, {1, 1});
    TestEqual(TEXT("Path length equals movement cost"), Expected.Num(), 2);
    FIntPoint Previous = Battle.Find(10)->Cell;
    for (FIntPoint Cell : Expected)
    {
        TestEqual(TEXT("Path steps are cardinal and contiguous"), FMath::Abs(Cell.X - Previous.X) + FMath::Abs(Cell.Y - Previous.Y), 1);
        TestFalse(TEXT("Path avoids terrain"), Encounter.BlockedCells.Contains(Cell));
        Previous = Cell;
    }
    const FCombatResult Move = Battle.Submit({ECombatCommandType::Move, 10, {1, 1}});
    TestTrue(TEXT("Move reports its path"), Move.Path == Expected);
    TestTrue(TEXT("Move reports actor and type"), Move.ActorId == 10 && Move.Type == ECombatCommandType::Move);

    int32 Hits = 0;
    for (int32 Seed = 1; Seed <= 40; ++Seed)
    {
        FCombatEncounter Close = Duel();
        Close.Combatants[1].Cell = {1, 0};
        Close.Combatants[1].ArmorClass = 12;
        Close.Combatants[1].MaxHP = Close.Combatants[1].HP = 9;
        FCombatBattle Fight;
        if (!Fight.Start(Close, Seed, Error)) { return false; }
        const FCombatSnapshot Before = Fight.Snapshot();
        const FActionForecast Forecast = Fight.Forecast(10, ECombatCommandType::Attack, 20);
        TestTrue(TEXT("Forecast consumes no random numbers"), SameState(Before, Fight.Snapshot()));
        TestTrue(TEXT("Legal attack has a forecast"), Forecast.bValid && Forecast.bAttackRoll);
        TestEqual(TEXT("Hit chance counts natural 20 and excludes natural 1"), Forecast.HitChancePercent, 70);
        TestTrue(TEXT("Damage range from die and modifier"), Forecast.MinAmount == 4 && Forecast.MaxAmount == 9);
        TestTrue(TEXT("Maximum damage reaches remaining health"), Forecast.bCanDrop);
        TestFalse(TEXT("Illegal target has no forecast"), Fight.Forecast(10, ECombatCommandType::Attack, 10).bValid);
        const FCombatResult Attack = Fight.Submit({ECombatCommandType::Attack, 10, {}, 20});
        TestTrue(TEXT("Attack reports target and defenses"), Attack.TargetId == 20 && Attack.TargetArmorClass == 12 && Attack.AttackBonus == 5);
        if (!Attack.bHit) { TestTrue(TEXT("A miss rolls no effect dice"), Attack.EffectDice.IsEmpty()); continue; }
        ++Hits;
        int32 Sum = Attack.EffectModifier;
        for (const FDieRoll& Die : Attack.EffectDice) { Sum += Die.Value; TestTrue(TEXT("Die value within its sides"), Die.Value >= 1 && Die.Value <= Die.Sides); }
        TestEqual(TEXT("Reported dice and modifier sum to damage"), Sum, Attack.Damage);
        TestEqual(TEXT("Critical doubles the weapon die"), Attack.EffectDice.Num(), Attack.bCritical ? 2 : 1);
        TestEqual(TEXT("Death is reported"), Attack.Died.Contains(20), Fight.Find(20)->Life == ECombatLife::Dead);
    }
    TestTrue(TEXT("Fixture seeds include hits"), Hits > 0);
    return !HasAnyErrors();
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatReachTest, "Project.Expedition.Combat.ReachMatchesLegalTargets",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FCombatReachTest::RunTest(const FString& Parameters)
{
    const ECombatCommandType Actions[] = {ECombatCommandType::Attack, ECombatCommandType::PrecisionStrike, ECombatCommandType::MagicMissile,
        ECombatCommandType::CureWounds, ECombatCommandType::Potion, ECombatCommandType::Rescue};
    for (const EAdventurerRole Role : {EAdventurerRole::Fighter, EAdventurerRole::Cleric, EAdventurerRole::Wizard})
    {
        FCombatEncounter Encounter = Duel();
        Encounter.Width = Encounter.Height = 8;
        FCombatantState& Hero = Encounter.Combatants[0];
        Hero.Cell = {1, 1}; Hero.Range = 3; Hero.AdventurerRole = Role; Hero.SpellSlots = 2; Hero.Potions = 1; Hero.HP = 12;
        if (Role == EAdventurerRole::Fighter) { Hero.FighterSubclass = EFighterSubclass::BattleMaster; Hero.ManeuverUses = 1; }
        Encounter.Combatants[1].Cell = {3, 2};                  // in sight at distance 3
        FCombatantState Hidden = Encounter.Combatants[1];
        Hidden.Id = 21; Hidden.Cell = {1, 4};                   // distance 3 behind the wall at (1, 3)
        FCombatantState Far = Encounter.Combatants[1];
        Far.Id = 22; Far.Cell = {6, 3};                         // distance 7: missile only
        FCombatantState Friend = Hero;
        Friend.Id = 11; Friend.Cell = {2, 1}; Friend.InitiativeBonus = -5; Friend.HP = 5;
        Encounter.Combatants.Append({Hidden, Far, Friend});
        Encounter.BlockedCells = {{1, 3}};
        FCombatBattle Battle;
        FString Error;
        if (!TestTrue(TEXT("Starts"), Battle.Start(Encounter, 42, Error))) { return false; }
        const FCombatSnapshot Before = Battle.Snapshot();
        for (const ECombatCommandType Action : Actions)
        {
            const FActionReach Reach = Battle.Reach(10, Action);
            const TArray<int32> Targets = Battle.LegalTargets(10, Action);
            for (const FCombatantState& Unit : Battle.Snapshot().Encounter.Combatants)
            {
                if (Targets.Contains(Unit.Id)) { TestTrue(TEXT("Every legal target stands on an in-sight cell"), Reach.InSight.Contains(Unit.Cell)); }
                TestFalse(TEXT("No cell is both in sight and obscured"), Reach.InSight.Contains(Unit.Cell) && Reach.Obscured.Contains(Unit.Cell));
            }
            for (const FIntPoint Cell : Reach.Obscured) { TestFalse(TEXT("Obscured cells hold no legal target"), Battle.Snapshot().Encounter.Combatants.ContainsByPredicate([&](const FCombatantState& Unit) { return Unit.Cell == Cell && Targets.Contains(Unit.Id); })); }
        }
        const FActionReach Attack = Battle.Reach(10, ECombatCommandType::Attack);
        TestTrue(TEXT("Visible enemy in weapon range"), Attack.InSight.Contains(FIntPoint(3, 2)) && Battle.LegalTargets(10).Contains(20));
        TestTrue(TEXT("Wall and the enemy behind it are obscured"), Attack.Obscured.Contains(FIntPoint(1, 3)) && Attack.Obscured.Contains(FIntPoint(1, 4)) && !Battle.LegalTargets(10).Contains(21));
        TestFalse(TEXT("Weapon reach stops at its range"), Attack.InSight.Contains(FIntPoint(5, 1)) || Attack.Obscured.Contains(FIntPoint(5, 1)));
        TestFalse(TEXT("Attacks never reach the user's own cell"), Attack.InSight.Contains(FIntPoint(1, 1)));
        if (Role == EAdventurerRole::Wizard)
        {
            const FActionReach Missile = Battle.Reach(10, ECombatCommandType::MagicMissile);
            TestTrue(TEXT("Missile reaches six cells"), Missile.InSight.Contains(FIntPoint(6, 2)) && !Missile.InSight.Contains(FIntPoint(6, 3)) && !Missile.Obscured.Contains(FIntPoint(6, 3)));
        }
        else { TestTrue(TEXT("Only wizards reach with Magic Missile"), Battle.Reach(10, ECombatCommandType::MagicMissile).InSight.IsEmpty()); }
        const FActionReach Potion = Battle.Reach(10, ECombatCommandType::Potion);
        TestTrue(TEXT("Touch reach includes self and adjacent ally"), Potion.InSight.Contains(FIntPoint(1, 1)) && Potion.InSight.Contains(FIntPoint(2, 1)) && Potion.InSight.Num() + Potion.Obscured.Num() == 5);
        TestEqual(TEXT("Cure reach only for clerics"), Battle.Reach(10, ECombatCommandType::CureWounds).InSight.IsEmpty(), Role != EAdventurerRole::Cleric);
        TestTrue(TEXT("Precision Strike only for Battle Masters"), Battle.Reach(10, ECombatCommandType::PrecisionStrike).InSight.IsEmpty() == (Role != EAdventurerRole::Fighter));
        TestTrue(TEXT("Not the active unit: no reach"), Battle.Reach(11, ECombatCommandType::Attack).InSight.IsEmpty());
        TestTrue(TEXT("Reach consumes no random numbers"), SameState(Before, Battle.Snapshot()));
    }
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatCatalogReachTest, "Project.Expedition.Combat.ReachMatchesCatalogAbilities",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FCombatCatalogReachTest::RunTest(const FString& Parameters)
{
    int32 Checked = 0;
    for (const FBuildDefinition& Build : FBuildCatalog::All())
    {
        FCombatEncounter Encounter = Duel();
        Encounter.Width = Encounter.Height = 8;
        FCombatantState Hero;
        if (!TestTrue(TEXT("Build creates"), FBuildCatalog::Create(Build.Id, 10, TEXT("Hero"), Hero))) { continue; }
        Hero.Cell = {1, 1}; Hero.Side = ECombatSide::Party; Hero.InitiativeBonus = 20; Hero.HP = FMath::Max(1, Hero.MaxHP - 1);
        Encounter.Combatants[0] = Hero;
        Encounter.Combatants[1].Cell = {2, 1};
        FCombatantState Friend = Hero;
        Friend.Id = 11; Friend.Cell = {1, 2}; Friend.InitiativeBonus = -5; Friend.HP = 1;
        Encounter.Combatants.Add(Friend);
        FCombatBattle Battle;
        FString Error;
        if (!TestTrue(TEXT("Starts"), Battle.Start(Encounter, 42, Error)) || Battle.ActiveId() != 10) { continue; }
        for (const FAbilityFacts& Ability : FBuildCatalog::Abilities(*Battle.Find(10)))
        {
            const FActionReach Reach = Battle.Reach(10, Ability.Command);
            const TArray<int32> Targets = Battle.LegalTargets(10, Ability.Command);
            for (const int32 Id : Targets) { TestTrue(FString::Printf(TEXT("%s: legal target cell is in reach"), *Build.Id), Reach.InSight.Contains(Battle.Find(Id)->Cell)); }
            if (Ability.Target == EAbilityTarget::Self && Targets.Num() > 0)
            {
                TestTrue(FString::Printf(TEXT("%s: self ability reaches only its user"), *Build.Id), Reach.InSight.Num() == 1 && Reach.InSight.Contains(FIntPoint(1, 1)) && Reach.Obscured.IsEmpty());
            }
            if (Targets.IsEmpty()) { TestTrue(FString::Printf(TEXT("%s: unusable ability has no reach"), *Build.Id), Reach.InSight.IsEmpty() && Reach.Obscured.IsEmpty()); }
            ++Checked;
        }
    }
    TestTrue(TEXT("Catalog abilities were checked"), Checked > 0);
    return !HasAnyErrors();
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSubclassForecastTest, "Project.Expedition.Combat.SubclassForecastAndBudget",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSubclassForecastTest::RunTest(const FString& Parameters)
{
    FString Error;
    auto Layout = FCombatBattle::TutorialEncounter();
    Layout.BlockedCells.Reset(); Layout.Combatants.SetNum(2);
    auto& Fighter = Layout.Combatants[0]; auto& Enemy = Layout.Combatants[1];
    Fighter.FighterSubclass = EFighterSubclass::Champion; Fighter.InitiativeBonus = 30;
    // This fixture repurposes the tutorial Rogue as a synthetic AC target.
    // Remove its authored party identity/resource when changing its side and role.
    Enemy.BuildId.Reset(); Enemy.ClassId.Reset(); Enemy.SubclassId.Reset(); Enemy.KitUses = 0;
    Enemy.Side = ECombatSide::Enemy; Enemy.AdventurerRole = EAdventurerRole::None;
    Enemy.Cell = {2,1}; Enemy.ArmorClass = 40;
    FCombatBattle Battle;
    TestTrue(TEXT("Start Champion"), Battle.Start(Layout, 42, Error));
    TestEqual(TEXT("Champion 19 and 20 hit even AC40"), Battle.Forecast(1,ECombatCommandType::Attack,2).HitChancePercent,10);
    Fighter.FighterSubclass = EFighterSubclass::BattleMaster; Fighter.ManeuverUses = 2; Layout.bSuperiorityDice = false; // legacy flat path
    Enemy.ArmorClass = 12;
    TestTrue(TEXT("Start Battle Master"), Battle.Start(Layout,42,Error));
    const auto Forecast = Battle.Forecast(1,ECombatCommandType::PrecisionStrike,2);
    TestTrue(TEXT("Maneuver is targetable"), Forecast.bValid);
    TestEqual(TEXT("Precision damage minimum"), Forecast.MinAmount,6);
    const auto Result = Battle.Submit({ECombatCommandType::PrecisionStrike,1,{},2});
    TestTrue(TEXT("Maneuver accepted"),Result.bAccepted);
    TestEqual(TEXT("Precision reports combined attack modifier"),Result.AttackBonus,7);
    TestEqual(TEXT("One use spent even on miss"),Battle.Find(1)->ManeuverUses,1);
    const auto Before = Battle.Snapshot();
    TestFalse(TEXT("Spent Action rejects repeated maneuver"),Battle.Submit({ECombatCommandType::PrecisionStrike,1,{},2}).bAccepted);
    TestEqual(TEXT("Rejected maneuver preserves RNG"),Battle.Snapshot().RandomState,Before.RandomState);
    return !HasAnyErrors();
}
#endif
