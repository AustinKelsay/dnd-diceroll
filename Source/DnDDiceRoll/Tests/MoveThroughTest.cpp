#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Combat/CombatBattle.h"
#include "ExpeditionInputHarness.h"
#include "ExpeditionTestSupport.h"
#include "Persistence/ExpeditionSave.h"
#include "Presentation/BoardOverlay.h"
#include "Run/ExpeditionRun.h"

/**
 * SRD 5.2.1 move-through (#240, catalog revision 11; docs/runs/journey-checkpoint/M1-movement-contract.md). A mover passes cells held by
 * allies, Downed creatures and bodies, never a conscious opponent's, and never ends on an occupied cell. Revisions 7-10 and SRD keep the
 * older rule, under which every unit blocks its cell.
 */
namespace MoveThroughTests
{
FCombatantState Unit(int32 Id, ECombatSide Side, FIntPoint Cell, ECombatLife Life = ECombatLife::Conscious)
{
    FCombatantState U;
    U.Id = Id; U.Side = Side; U.Cell = Cell; U.Life = Life;
    U.Name = FString::Printf(TEXT("Unit %d"), Id); U.Role = Side == ECombatSide::Party ? TEXT("Fighter") : TEXT("Raider");
    U.Speed = 5; U.InitiativeBonus = -10;
    if (Life != ECombatLife::Conscious) { U.HP = 0; }
    if (Life == ECombatLife::Downed) { U.RescueTurnsLeft = 2; }
    return U;
}

/** A one-row corridor (y = 1) between walls, Width cells long. */
FCombatEncounter Corridor(int32 Width, bool bMoveThrough)
{
    FCombatEncounter E;
    E.Width = Width; E.Height = 3; E.bMoveThrough = bMoveThrough;
    for (int32 X = 0; X < Width; ++X) { E.BlockedCells.Add({X, 0}); E.BlockedCells.Add({X, 2}); }
    return E;
}

/** The first combatant in the list acts first. */
bool StartWithFirstActing(FAutomationTestBase& Test, FCombatBattle& Battle, FCombatEncounter E)
{
    E.Combatants[0].InitiativeBonus = 30;
    FString Error;
    return Test.TestTrue(TEXT("The encounter starts: ") + Error, Battle.Start(E, 42, Error)) && Test.TestEqual(TEXT("The first unit acts"), Battle.ActiveId(), E.Combatants[0].Id);
}

TArray<FIntPoint> Sorted(TArray<FIntPoint> Cells)
{
    Cells.Sort([](FIntPoint A, FIntPoint B) { return A.Y == B.Y ? A.X < B.X : A.Y < B.Y; });
    return Cells;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMoveThroughPassesAlliesAndBodiesTest, "Project.Expedition.Combat.MoveThroughPassesAlliesDownedAndDead",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FMoveThroughPassesAlliesAndBodiesTest::RunTest(const FString& Parameters)
{
    using namespace MoveThroughTests;
    // Hero 10 at (0,1); a conscious ally at (1,1), a dead foe at (2,1), a Downed ally at (3,1), an open cell at (4,1) and a living foe at (6,1).
    for (const bool bMoveThrough : {true, false})
    {
        FCombatEncounter E = Corridor(7, bMoveThrough);
        E.Combatants = {Unit(10, ECombatSide::Party, {0, 1}), Unit(11, ECombatSide::Party, {1, 1}), Unit(21, ECombatSide::Enemy, {2, 1}, ECombatLife::Dead),
            Unit(12, ECombatSide::Party, {3, 1}, ECombatLife::Downed), Unit(20, ECombatSide::Enemy, {6, 1})};
        FCombatBattle Battle;
        if (!StartWithFirstActing(*this, Battle, E)) { return false; }
        const FString Rule = bMoveThrough ? TEXT("Move-through: ") : TEXT("Older rule: ");
        if (!bMoveThrough)
        {
            TestTrue(Rule + TEXT("the ally next to the hero blocks every move"), Battle.LegalMoves(10).IsEmpty());
            TestFalse(Rule + TEXT("a move past the ally is refused"), Battle.Submit({ECombatCommandType::Move, 10, {4, 1}}).bAccepted);
            continue;
        }
        TestEqual(Rule + TEXT("the only legal destinations are the open cells past the ally, the body and the Downed ally"),
            Sorted(Battle.LegalMoves(10)), TArray<FIntPoint>{FIntPoint(4, 1), FIntPoint(5, 1)});
        TestEqual(Rule + TEXT("the path passes every occupied cell in order"), Battle.PathTo(10, {4, 1}),
            TArray<FIntPoint>{FIntPoint(1, 1), FIntPoint(2, 1), FIntPoint(3, 1), FIntPoint(4, 1)});
        for (const FIntPoint Occupied : {FIntPoint(1, 1), FIntPoint(2, 1), FIntPoint(3, 1)})
        {
            const FCombatSnapshot Before = Battle.Snapshot();
            TestTrue(Rule + TEXT("no path ends on an occupied cell"), Battle.PathTo(10, Occupied).IsEmpty());
            TestFalse(Rule + TEXT("a move that ends on an occupied cell is refused"), Battle.Submit({ECombatCommandType::Move, 10, Occupied}).bAccepted);
            TestTrue(Rule + TEXT("the refusal changes nothing"), FCombatSnapshot::StaticStruct()->CompareScriptStruct(&Before, &Battle.Snapshot(), 0));
        }
        const FCombatResult Moved = Battle.Submit({ECombatCommandType::Move, 10, {4, 1}});
        TestTrue(Rule + TEXT("the move through is accepted"), Moved.bAccepted);
        TestEqual(Rule + TEXT("the result reports the cells entered"), Moved.Path, TArray<FIntPoint>{FIntPoint(1, 1), FIntPoint(2, 1), FIntPoint(3, 1), FIntPoint(4, 1)});
        TestEqual(Rule + TEXT("the hero stands at the destination"), Battle.Find(10)->Cell, FIntPoint(4, 1));
        TestEqual(Rule + TEXT("each cell passed costs one movement"), Battle.Snapshot().MovementLeft, 1);
        TestEqual(Rule + TEXT("the passed units keep their cells"), Battle.Find(11)->Cell + Battle.Find(21)->Cell + Battle.Find(12)->Cell, FIntPoint(6, 3));
    }
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMoveThroughLivingFoeBlocksTest, "Project.Expedition.Combat.MoveThroughLivingFoeBlocks",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FMoveThroughLivingFoeBlocksTest::RunTest(const FString& Parameters)
{
    using namespace MoveThroughTests;
    // A living foe in the corridor holds it: the hero can reach the cell before it, never the open cells behind it.
    FCombatEncounter E = Corridor(7, true);
    E.Combatants = {Unit(10, ECombatSide::Party, {0, 1}), Unit(21, ECombatSide::Enemy, {1, 1}, ECombatLife::Dead), Unit(20, ECombatSide::Enemy, {3, 1})};
    FCombatBattle Battle;
    if (!StartWithFirstActing(*this, Battle, E)) { return false; }
    TestEqual(TEXT("Only the cell before the living foe is a destination"), Battle.LegalMoves(10), TArray<FIntPoint>{FIntPoint(2, 1)});
    TestTrue(TEXT("No path leads past the living foe"), Battle.PathTo(10, {4, 1}).IsEmpty());
    TestFalse(TEXT("A move past the living foe is refused"), Battle.Submit({ECombatCommandType::Move, 10, {4, 1}}).bAccepted);
    // The same foe Downed would not exist (enemies die at 0 HP); dead, it no longer blocks.
    FCombatEncounter Dead = E;
    Dead.Combatants.Add(Unit(22, ECombatSide::Enemy, {6, 1}));
    Dead.Combatants[2] = Unit(20, ECombatSide::Enemy, {3, 1}, ECombatLife::Dead);
    FCombatBattle After;
    if (!StartWithFirstActing(*this, After, Dead)) { return false; }
    TestTrue(TEXT("Past a dead foe the open cells are destinations"), After.LegalMoves(10).Contains(FIntPoint(4, 1)) && After.LegalMoves(10).Contains(FIntPoint(5, 1)));
    // Living heroes block enemies the same way: an opponent is an opponent from either side.
    FCombatEncounter Foe = Corridor(7, true);
    Foe.Combatants = {Unit(20, ECombatSide::Enemy, {0, 1}), Unit(10, ECombatSide::Party, {2, 1}), Unit(11, ECombatSide::Party, {6, 1})};
    FCombatBattle Enemy;
    if (!StartWithFirstActing(*this, Enemy, Foe)) { return false; }
    TestEqual(TEXT("A living hero blocks an enemy"), Enemy.LegalMoves(20), TArray<FIntPoint>{FIntPoint(1, 1)});
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMoveThroughEnemyTurnTest, "Project.Expedition.Combat.MoveThroughEnemyTurns",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FMoveThroughEnemyTurnTest::RunTest(const FString& Parameters)
{
    using namespace MoveThroughTests;
    // Enemy 20 at (5,1), speed 5, range 1; its ally 21 at (4,1) and a hero's body at (3,1); the hero 10 at (0,1).
    for (const bool bMoveThrough : {true, false})
    {
        FCombatEncounter E = Corridor(6, bMoveThrough);
        E.Combatants = {Unit(20, ECombatSide::Enemy, {5, 1}), Unit(21, ECombatSide::Enemy, {4, 1}), Unit(11, ECombatSide::Party, {3, 1}, ECombatLife::Dead),
            Unit(10, ECombatSide::Party, {0, 1})};
        FCombatBattle Battle;
        if (!StartWithFirstActing(*this, Battle, E)) { return false; }
        const TArray<FCombatResult> Turn = Battle.PlayEnemyTurn();
        if (bMoveThrough)
        {
            TestTrue(TEXT("Move-through: the enemy moves first"), !Turn.IsEmpty() && Turn[0].Type == ECombatCommandType::Move && Turn[0].bAccepted);
            TestEqual(TEXT("Move-through: it passes its ally and the body to stand beside the hero"), Battle.Find(20)->Cell, FIntPoint(1, 1));
            TestTrue(TEXT("Move-through: then it attacks"), Turn.Num() >= 2 && Turn[1].Type == ECombatCommandType::Attack && Turn[1].bAccepted);
        }
        else
        {
            TestEqual(TEXT("Older rule: its ally walls it in"), Battle.Find(20)->Cell, FIntPoint(5, 1));
            TestTrue(TEXT("Older rule: it only ends its turn"), Turn.Num() == 1 && Turn[0].Type == ECombatCommandType::EndTurn);
        }
    }
    // The only cell beside the hero is a body's: the enemy never stops on it, and stays put rather than ending on an occupied cell.
    FCombatEncounter E = Corridor(6, true);
    E.Combatants = {Unit(20, ECombatSide::Enemy, {4, 1}), Unit(11, ECombatSide::Party, {1, 1}, ECombatLife::Dead), Unit(10, ECombatSide::Party, {0, 1})};
    E.Combatants[0].Speed = 3;
    FCombatBattle Battle;
    if (!StartWithFirstActing(*this, Battle, E)) { return false; }
    const TArray<FCombatResult> Turn = Battle.PlayEnemyTurn();
    TestEqual(TEXT("The firing cell is taken, so the enemy does not move"), Battle.Find(20)->Cell, FIntPoint(4, 1));
    TestTrue(TEXT("Every command it submitted was accepted"), !Turn.ContainsByPredicate([](const FCombatResult& R) { return !R.bAccepted; }));
    // With speed short of the open cell before the body, it walks the farthest stoppable cell along its route.
    FCombatEncounter Far = Corridor(8, true);
    Far.Combatants = {Unit(20, ECombatSide::Enemy, {7, 1}), Unit(21, ECombatSide::Enemy, {4, 1}), Unit(10, ECombatSide::Party, {0, 1})};
    Far.Combatants[0].Speed = 3;
    FCombatBattle Short;
    if (!StartWithFirstActing(*this, Short, Far)) { return false; }
    Short.PlayEnemyTurn();
    TestEqual(TEXT("Three cells would end on its ally, so it stops one short"), Short.Find(20)->Cell, FIntPoint(5, 1));
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMoveThroughSealedHeroTest, "Project.Expedition.Combat.MoveThroughFreesTheSealedHero",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FMoveThroughSealedHeroTest::RunTest(const FString& Parameters)
{
    using namespace MoveThroughTests;
    // #217's evidence, trace rot2-03:20261119:10 fight 4 (revision 9 before #214's layout fix, evidence/pr214-e0311a1f): a 17 x 12 sunken
    // causeway. The last hero (4) at (7,8) has the bodies of the skirmisher (7,7), the raider (6,8) and the archer (8,8) beside it and the
    // causeway wall at (7,9); the brute at (6,6) could never reach it, and it could never leave. The heroes 1-3 lie dead at (5,7), (4,5), (6,7).
    for (const bool bMoveThrough : {true, false})
    {
        FCombatEncounter E;
        E.Width = 17; E.Height = 12; E.bMoveThrough = bMoveThrough;
        for (int32 X = 6; X <= 10; ++X) { E.BlockedCells.Add({X, 1}); E.BlockedCells.Add({X, 9}); }
        for (int32 Y = 3; Y <= 7; ++Y) { E.BlockedCells.Add({8, Y}); }
        E.Combatants = {Unit(4, ECombatSide::Party, {7, 8}), Unit(1, ECombatSide::Party, {5, 7}, ECombatLife::Dead), Unit(2, ECombatSide::Party, {4, 5}, ECombatLife::Dead),
            Unit(3, ECombatSide::Party, {6, 7}, ECombatLife::Dead), Unit(20, ECombatSide::Enemy, {6, 6}), Unit(21, ECombatSide::Enemy, {7, 7}, ECombatLife::Dead),
            Unit(22, ECombatSide::Enemy, {6, 8}, ECombatLife::Dead), Unit(23, ECombatSide::Enemy, {8, 8}, ECombatLife::Dead)};
        E.Combatants[0].Speed = 4;
        FCombatBattle Battle;
        if (!StartWithFirstActing(*this, Battle, E)) { return false; }
        if (!bMoveThrough)
        {
            TestTrue(TEXT("Older rule: the sealed hero has no move"), Battle.LegalMoves(4).IsEmpty());
            continue;
        }
        TestTrue(TEXT("Move-through: the hero can reach (7,6), beside the brute"), Battle.LegalMoves(4).Contains(FIntPoint(7, 6)));
        for (const FIntPoint Body : {FIntPoint(7, 7), FIntPoint(6, 8), FIntPoint(8, 8), FIntPoint(6, 7)})
        { TestFalse(TEXT("Move-through: no body's cell is a destination"), Battle.LegalMoves(4).Contains(Body)); }
        TestEqual(TEXT("Move-through: the route crosses the skirmisher's body"), Battle.PathTo(4, {7, 6}), TArray<FIntPoint>{FIntPoint(7, 7), FIntPoint(7, 6)});
        TestTrue(TEXT("Move-through: the hero walks out"), Battle.Submit({ECombatCommandType::Move, 4, {7, 6}}).bAccepted);
        TestTrue(TEXT("Move-through: and can attack the brute"), Battle.LegalTargets(4).Contains(20));
    }
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMoveThroughRevisionTest, "Project.Expedition.Run.MoveThroughFromRevision11",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FMoveThroughRevisionTest::RunTest(const FString& Parameters)
{
    using ExpeditionTestSupport::SameRun;
    TestEqual(TEXT("New runs are created under revision 11"), FBuildCatalog::Revision, 11);
    for (int32 Revision = FBuildCatalog::MinimumRevision; Revision <= FBuildCatalog::Revision; ++Revision)
    {
        TestEqual(FString::Printf(TEXT("Compact revision %d"), Revision), FExpeditionRun::MovesThroughUnits(ERuleset::Compact, Revision), Revision >= 11);
        TestFalse(FString::Printf(TEXT("SRD revision %d keeps the older rule"), Revision), FExpeditionRun::MovesThroughUnits(ERuleset::SRD, Revision));
    }
    for (const int32 Revision : {10, 11})
    {
        FExpeditionRun Run; FString Error;
        TestTrue(TEXT("A run starts"), Run.NewRun(42, Error, TEXT("move-through"), EFighterSubclass::Champion, ERuleset::Compact, Revision));
        const bool bExpected = Revision >= 11;
        TestEqual(FString::Printf(TEXT("Revision %d: the opening encounter's rule"), Revision), Run.Battle().Snapshot().Encounter.bMoveThrough, bExpected);
        int32 Fights = 0;
        for (int32 Fight = 0; Fight < FExpeditionRun::EncounterCount && Run.Progress().Phase == EExpeditionPhase::Route; ++Fight)
        {
            if (!Run.ChooseRoute(Run.Progress().Offers[0].Id, Error)) { break; }
            ++Fights;
            TestEqual(FString::Printf(TEXT("Revision %d fight %d carries the run's rule"), Revision, Fight + 1), Run.Battle().Snapshot().Encounter.bMoveThrough, bExpected);
            // Mid-fight: the real save round trip restores the rule from the saved revision, never from the snapshot.
            if (Fight == 0)
            {
                FExpeditionSaveData Data; Data.Current = Run.Snapshot();
                FString Json; FExpeditionSaveData Loaded;
                TestTrue(TEXT("The checkpoint encodes"), FExpeditionSave::Encode(Data, Json, Error));
                TestFalse(TEXT("The rule is not a saved field"), Json.Contains(TEXT("MoveThrough")));
                TestTrue(TEXT("The checkpoint decodes"), FExpeditionSave::Decode(Json, Loaded, Error));
                TestFalse(TEXT("Decoded, the snapshot holds no rule"), Loaded.Current.Battle.Encounter.bMoveThrough);
                FExpeditionRun Resumed;
                TestTrue(TEXT("It restores"), Resumed.Restore(Loaded.Current, Error));
                TestEqual(TEXT("Restore derives the rule from the run's revision"), Resumed.Battle().Snapshot().Encounter.bMoveThrough, bExpected);
                FExpeditionSnapshot Forged = Run.Snapshot(); Forged.Battle.Encounter.bMoveThrough = !bExpected;
                TestTrue(TEXT("A snapshot whose flag disagrees restores"), Resumed.Restore(Forged, Error));
                TestEqual(TEXT("and still plays its revision's rule"), Resumed.Battle().Snapshot().Encounter.bMoveThrough, bExpected);
            }
            if (!ExpeditionTestSupport::FinishCombat(Run)) { break; }
            if (Run.Progress().Phase == EExpeditionPhase::Reward && !Run.ClaimReward(1, Error) && !Run.ClaimReward(2, Error) && !Run.ClaimReward(3, Error)) { Run.ClaimReward(4, Error); }
            if (Run.Progress().Phase == EExpeditionPhase::Recovery) { Run.Recover(true, Error); }
        }
        TestTrue(FString::Printf(TEXT("Revision %d played at least two fights"), Revision), Fights >= 2);
    }
    FExpeditionRun Srd; FString Error;
    TestTrue(TEXT("An SRD run starts"), Srd.NewRun(42, Error, TEXT("move-through-srd"), EFighterSubclass::Champion, ERuleset::SRD));
    TestTrue(TEXT("It chooses a route"), Srd.ChooseRoute(Srd.Progress().Offers[0].Id, Error));
    TestFalse(TEXT("SRD keeps the older rule at revision 11"), Srd.Battle().Snapshot().Encounter.bMoveThrough);
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMoveThroughPreviewTest, "Project.Expedition.Presentation.MovePreviewPassesBodies",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FMoveThroughPreviewTest::RunTest(const FString& Parameters)
{
    // The reach contour, the path ribbon and the destination bracket read LegalMoves and PathTo: with a body in front of the acting hero
    // the ribbon runs through it, and neither the body's cell nor an ally's is ever offered as a destination.
    FExpeditionRun Run; FIntPoint Body, Beyond;
    if (!TestTrue(TEXT("A revision 11 battle with a body in front of the acting hero"), ExpeditionTestSupport::BodyInFrontBattle(Run, Body, Beyond))) { return false; }
    FExpeditionSaveData Data; Data.Current = Run.Snapshot();
    ExpeditionInputHarness::FHarness Game(*this, TEXT("MovePreviewPassesBodies"), &Data);
    if (!TestTrue(TEXT("Input, board and HUD are ready"), Game.IsReady())) { return false; }
    Game.Frame();
    Game.Press(EKeys::O);
    if (!TestTrue(TEXT("The battle resumes"), Game.View->HasRun() && Game.View->InCombat())) { return false; }
    Game.Settle();
    Game.PartyTurn();
    Game.Press(EKeys::A);
    const FCombatBattle& Rules = Game.View->Rules();
    const int32 Actor = Game.Player->Actor();
    TestTrue(TEXT("The resumed battle moves through"), Rules.Snapshot().Encounter.bMoveThrough);
    const BoardOverlay::FMask& Mask = Game.View->OverlayMask();
    const TSet<FIntPoint> Drawn(Mask.Cells(BoardOverlay::Move)), Legal(Rules.LegalMoves(Actor));
    TestTrue(TEXT("The move contour is exactly the legal moves"), Drawn.Num() == Legal.Num() && Drawn.Includes(Legal));
    for (const FCombatantState& Unit : Rules.Snapshot().Encounter.Combatants)
    { if (Unit.Id != Actor) { TestFalse(FString::Printf(TEXT("%s's cell is not offered"), *Unit.Name), Mask.Has(Unit.Cell, BoardOverlay::Move)); } }
    TestTrue(TEXT("The cell beyond the body is offered"), Mask.Has(Beyond, BoardOverlay::Move));
    Game.Player->PreviewAim(true, Beyond);
    Game.Press(EKeys::A);
    TestEqual(TEXT("The ribbon is the rules' path"), Game.Player->PreviewPath(), Rules.PathTo(Actor, Beyond));
    TestTrue(TEXT("The ribbon crosses the body"), Game.Player->PreviewPath().Contains(Body) && Game.View->OverlayMask().Has(Body, BoardOverlay::Path));
    TestTrue(TEXT("The destination has its bracket"), Game.View->OverlayBars(BoardOverlay::EStyle::PathBracket) > 0);
    Game.Player->PreviewAim(true, Body);
    Game.Press(EKeys::A);
    TestTrue(TEXT("Hovering the body previews no path"), Game.Player->PreviewPath().IsEmpty());
    TestEqual(TEXT("and draws no destination bracket"), Game.View->OverlayBars(BoardOverlay::EStyle::PathBracket), 0);
    Game.Player->PreviewAim(false);
    return !HasAnyErrors();
}
#endif
