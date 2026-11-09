#if WITH_DEV_AUTOMATION_TESTS
#include "ExpeditionInputHarness.h"
#include "Presentation/BoardMetric.h"
#include "Presentation/BoardOverlay.h"

/**
 * The contextual grid and footprint rings (#142). The mask the board draws from must equal the authoritative
 * queries behind #50's targeting feedback, themed boards must not place per-cell tiles, the full-grid key must
 * touch presentation only, and the rings must follow the board metric.
 */
namespace
{
using BoardOverlay::EStyle;

bool ReachFirstPartyTurn(ExpeditionInputHarness::FHarness& Game)
{
    Game.Frame();
    Game.Press(EKeys::Enter);
    Game.Settle();
    Game.Press(EKeys::Enter);
    if (!Game.View->InCombat()) { return false; }
    Game.PartyTurn();
    return Game.View->InCombat();
}

bool SameCells(const TArray<FIntPoint>& A, const TArray<FIntPoint>& B)
{
    const TSet<FIntPoint> SetA(A), SetB(B);
    return SetA.Num() == SetB.Num() && SetA.Includes(SetB);
}

int32 Count(const TArray<BoardOverlay::FBar>& Bars, EStyle Style)
{
    int32 Result = 0;
    for (const BoardOverlay::FBar& Bar : Bars) { if (Bar.Style == Style) { ++Result; } }
    return Result;
}

/** The cells the forecast says an action would strike (what the HUD labels AREA). */
TArray<FIntPoint> ForecastVictims(const FCombatBattle& Rules, int32 Actor, ECombatCommandType Action, int32 Hover)
{
    TArray<FIntPoint> Cells;
    for (const FTargetEffect& Effect : Rules.Forecast(Actor, Action, Hover).Effects)
    {
        if (Effect.Effect != EAbilityEffect::Strike) { continue; }
        if (const FCombatantState* Unit = Rules.Find(Effect.TargetId)) { Cells.AddUnique(Unit->Cell); }
    }
    return Cells;
}

void CheckMaskMatchesRules(FAutomationTestBase& Test, const ExpeditionInputHarness::FHarness& Game, const FString& Context)
{
    const FCombatBattle& Rules = Game.View->Rules();
    const int32 Actor = Game.Player->Actor();
    const ECombatCommandType Action = Game.Player->Action();
    const BoardOverlay::FMask& Mask = Game.View->OverlayMask();
    const FActionReach Expected = Rules.ActionReach(Actor, Action);
    Test.TestTrue(Context + TEXT(": reach cells equal the authoritative visible reach"), SameCells(Mask.Cells(BoardOverlay::Reach), Expected.Visible));
    Test.TestTrue(Context + TEXT(": obstructed cells equal the authoritative blocked sight"), SameCells(Mask.Cells(BoardOverlay::Obstructed), Expected.Obstructed));
    const TArray<FIntPoint> Moves = Action == ECombatCommandType::Attack ? Rules.LegalMoves(Actor) : TArray<FIntPoint>();
    Test.TestTrue(Context + TEXT(": move cells equal the legal moves"), SameCells(Mask.Cells(BoardOverlay::Move), Moves));
    int32 HoverTarget = INDEX_NONE;
    FIntPoint Hover;
    if (Game.Player->HoverCell(Hover))
    {
        for (const FCombatantState& Unit : Rules.Snapshot().Encounter.Combatants)
        { if (Unit.Cell == Hover && Game.Player->Targets().Contains(Unit.Id)) { HoverTarget = Unit.Id; } }
    }
    Test.TestTrue(Context + TEXT(": victim cells equal the forecast's strike targets"),
        SameCells(Mask.Cells(BoardOverlay::Victim), ForecastVictims(Rules, Actor, Action, HoverTarget)));
    if (Game.View->DrawsContextualGrid())
    {
        Test.TestTrue(Context + TEXT(": a move region has a contour"), Moves.IsEmpty() || Game.View->OverlayBars(EStyle::MoveContour) > 0);
        Test.TestEqual(Context + TEXT(": one cross (two bars) per blocked-sight cell"), Game.View->OverlayBars(EStyle::Blocked), 2 * Expected.Obstructed.Num());
    }
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBoardOverlayMatchesRulesTest, "Project.Expedition.Presentation.ContextualGridMatchesRules",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FBoardOverlayMatchesRulesTest::RunTest(const FString& Parameters)
{
    ExpeditionInputHarness::FHarness Game(*this, TEXT("ContextualGrid"));
    if (!TestTrue(TEXT("Input, board and HUD are ready"), Game.IsReady())) { return false; }
    if (!TestTrue(TEXT("The party has the first turn"), ReachFirstPartyTurn(Game))) { return false; }

    // Every mode key #50's tests use, so every action type the actor has: reach, blocked sight and moves follow the rules.
    for (const FKey& Key : {EKeys::A, EKeys::F, EKeys::M, EKeys::B, EKeys::J, EKeys::R, EKeys::P, EKeys::A})
    {
        Game.Press(Key);
        CheckMaskMatchesRules(*this, Game, FString::Printf(TEXT("Action key %s"), *Key.ToString()));
    }

    // Put the keyboard cursor on each legal target of each action: the hover and the forecast's victims are marked.
    for (const FKey& Key : {EKeys::A, EKeys::F, EKeys::M, EKeys::B, EKeys::J, EKeys::R, EKeys::P})
    {
        Game.Player->PreviewAim(false);
        Game.Press(Key);
        const TArray<int32> Targets = Game.Player->Targets();
        for (const int32 Id : Targets)
        {
            const FCombatantState* Unit = Game.View->Rules().Find(Id);
            if (!Unit) { continue; }
            const FIntPoint Cell = Unit->Cell;
            Game.Player->PreviewAim(true, Cell);
            Game.Press(Key);
            CheckMaskMatchesRules(*this, Game, FString::Printf(TEXT("Cursor on %s under key %s"), *Unit->Name, *Key.ToString()));
            TestTrue(TEXT("The cursor cell is marked as hovered"), Game.View->OverlayMask().Has(Cell, BoardOverlay::Hover));
        }
    }
    Game.Player->PreviewAim(false);
    Game.Press(EKeys::A);

    // Nothing is marked once a playback or an overlay owns the screen.
    Game.Press(EKeys::Slash);
    TestTrue(TEXT("Help opens"), Game.Player->OverlayOpen());
    TestTrue(TEXT("Under Help the board marks nothing"), Game.View->OverlayMask().Cells(static_cast<uint8>(BoardOverlay::Move | BoardOverlay::Reach | BoardOverlay::Obstructed)).IsEmpty());
    Game.Press(EKeys::Escape);
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBoardOverlayGeometryTest, "Project.Expedition.Presentation.ContextualGridGeometry",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FBoardOverlayGeometryTest::RunTest(const FString& Parameters)
{
    TArray<BoardOverlay::FBar> Bars;
    FBoardHighlights Highlights;

    // One cell: four contour edges. Two side by side: still four (runs merge). An L of three: six.
    Highlights.Moves = {FIntPoint(1, 1)};
    BoardOverlay::BuildBars(BoardOverlay::BuildMask(5, 5, Highlights, {}), Highlights, false, Bars);
    TestEqual(TEXT("A single cell has a four-sided contour"), Count(Bars, EStyle::MoveContour), 4);
    TestEqual(TEXT("A single cell has two ticks at each of its four corners"), Count(Bars, EStyle::Tick), 8);
    Highlights.Moves = {FIntPoint(1, 1), FIntPoint(2, 1)};
    BoardOverlay::BuildBars(BoardOverlay::BuildMask(5, 5, Highlights, {}), Highlights, false, Bars);
    TestEqual(TEXT("Two neighbours merge into one rectangle contour"), Count(Bars, EStyle::MoveContour), 4);
    Highlights.Moves = {FIntPoint(1, 1), FIntPoint(2, 1), FIntPoint(1, 2)};
    BoardOverlay::BuildBars(BoardOverlay::BuildMask(5, 5, Highlights, {}), Highlights, false, Bars);
    TestEqual(TEXT("An L-shaped region has six contour runs"), Count(Bars, EStyle::MoveContour), 6);
    for (const BoardOverlay::FBar& Bar : Bars)
    {
        if (Bar.Style != EStyle::MoveContour) { continue; }
        TestTrue(TEXT("Contours are axis aligned"), FMath::IsNearlyEqual(Bar.A.X, Bar.B.X) || FMath::IsNearlyEqual(Bar.A.Y, Bar.B.Y));
    }

    // A region on the board edge is outlined along the edge too.
    Highlights.Moves = {FIntPoint(0, 0)};
    BoardOverlay::BuildBars(BoardOverlay::BuildMask(3, 3, Highlights, {}), Highlights, false, Bars);
    TestEqual(TEXT("A corner cell is outlined on all four sides"), Count(Bars, EStyle::MoveContour), 4);

    // The path ribbon: cardinal legs, one cost tick per step, a destination bracket.
    Highlights = FBoardHighlights();
    Highlights.bHasOrigin = true;
    Highlights.Origin = FIntPoint(1, 1);
    Highlights.Path = {FIntPoint(2, 1), FIntPoint(2, 2), FIntPoint(3, 2)};
    BoardOverlay::BuildBars(BoardOverlay::BuildMask(6, 6, Highlights, {}), Highlights, false, Bars);
    TestEqual(TEXT("A three-step path has three legs and three cost ticks"), Count(Bars, EStyle::Ribbon), 6);
    TestEqual(TEXT("The destination has four corner brackets (two arms each)"), Count(Bars, EStyle::PathBracket), 8);
    for (const BoardOverlay::FBar& Bar : Bars)
    {
        if (Bar.Style != EStyle::Ribbon) { continue; }
        TestTrue(TEXT("Ribbon bars are axis aligned: exact cardinal corners"), FMath::IsNearlyEqual(Bar.A.X, Bar.B.X) || FMath::IsNearlyEqual(Bar.A.Y, Bar.B.Y));
    }

    // Blocked sight, victims, hover and the full grid.
    Highlights = FBoardHighlights();
    Highlights.Reach.Obstructed = {FIntPoint(1, 1), FIntPoint(2, 2)};
    Highlights.bHasHover = true;
    Highlights.Hover = FIntPoint(3, 3);
    const TArray<FIntPoint> Victims = {FIntPoint(0, 3)};
    BoardOverlay::BuildBars(BoardOverlay::BuildMask(5, 4, Highlights, Victims), Highlights, true, Bars);
    TestEqual(TEXT("Each blocked cell has a two-bar cross"), Count(Bars, EStyle::Blocked), 4);
    TestEqual(TEXT("A victim has a four-bar frame"), Count(Bars, EStyle::Victim), 4);
    TestEqual(TEXT("The hover cell has corner brackets"), Count(Bars, EStyle::HoverBracket), 8);
    TestEqual(TEXT("The full grid is one line per cell edge of the board"), Count(Bars, EStyle::Grid), (5 + 1) + (4 + 1));
    BoardOverlay::BuildBars(BoardOverlay::BuildMask(5, 4, Highlights, Victims), Highlights, false, Bars);
    TestEqual(TEXT("Without the key there is no grid"), Count(Bars, EStyle::Grid), 0);
    // Cells off the board are ignored.
    Highlights = FBoardHighlights();
    Highlights.Moves = {FIntPoint(-1, 0), FIntPoint(9, 9)};
    TestTrue(TEXT("Off-board cells leave the mask empty"), BoardOverlay::BuildMask(3, 3, Highlights, {}).Cells(BoardOverlay::Move).IsEmpty());
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FThemedBoardHasNoTilesTest, "Project.Expedition.Presentation.ThemedBoardHasNoCellTiles",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FThemedBoardHasNoTilesTest::RunTest(const FString& Parameters)
{
    ExpeditionInputHarness::FHarness Game(*this, TEXT("ThemedNoTiles"));
    if (!TestTrue(TEXT("Input, board and HUD are ready"), Game.IsReady())) { return false; }
    if (!TestTrue(TEXT("The party has the first turn"), ReachFirstPartyTurn(Game))) { return false; }
    const FCombatEncounter& Encounter = Game.View->Rules().Snapshot().Encounter;
    if (Game.View->UsesTheme())
    {
        TestEqual(TEXT("A themed board places no per-cell tile meshes"), Game.View->TileCount(), 0);
        TestTrue(TEXT("A themed board draws the contextual grid"), Game.View->DrawsContextualGrid());
    }
    else { AddInfo(TEXT("No theme is loaded here; the themed half of this check was skipped.")); }
    Game.View->UsePrimitives();
    Game.Frame();
    const FCombatEncounter& Primitive = Game.View->Rules().Snapshot().Encounter;
    TestEqual(TEXT("A primitive board keeps one cube tile per cell"), Game.View->TileCount(), Primitive.Width * Primitive.Height);
    TestFalse(TEXT("A primitive board has no contextual grid"), Game.View->DrawsContextualGrid());
    TestTrue(TEXT("The board is the same size either way"), Primitive.Width == Encounter.Width && Primitive.Height == Encounter.Height);
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFullGridKeyTest, "Project.Expedition.Presentation.FullGridKeyIsPresentationOnly",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FFullGridKeyTest::RunTest(const FString& Parameters)
{
    ExpeditionInputHarness::FHarness Game(*this, TEXT("FullGridKey"));
    if (!TestTrue(TEXT("Input, board and HUD are ready"), Game.IsReady())) { return false; }
    if (!TestTrue(TEXT("The party has the first turn"), ReachFirstPartyTurn(Game))) { return false; }
    const FExpeditionSnapshot Before = Game.Live();
    const TArray<uint8> CheckpointBefore = Game.CheckpointBytes();
    const FCombatEncounter& Encounter = Game.View->Rules().Snapshot().Encounter;
    TestFalse(TEXT("The grid is hidden at rest"), Game.View->ShowingFullGrid());
    TestEqual(TEXT("No grid lines at rest"), Game.View->OverlayBars(EStyle::Grid), 0);

    Game.Player->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::LeftAlt, IE_Pressed, 1.0f));
    Game.Frame();
    TestTrue(TEXT("Holding Left Alt shows the full grid"), Game.View->ShowingFullGrid() && Game.Player->ShowsFullGrid());
    if (Game.View->DrawsContextualGrid())
    { TestEqual(TEXT("One line per cell edge across the board"), Game.View->OverlayBars(EStyle::Grid), (Encounter.Width + 1) + (Encounter.Height + 1)); }
    TestTrue(TEXT("Holding the key changes no rules state"), ExpeditionTestSupport::SameRun(Before, Game.Live()));
    TestTrue(TEXT("Holding the key writes no checkpoint"), CheckpointBefore == Game.CheckpointBytes());

    // Help owns the screen: the grid hides under it and returns after.
    Game.Press(EKeys::Slash);
    TestFalse(TEXT("The grid hides under Help"), Game.View->ShowingFullGrid());
    Game.Press(EKeys::Escape);
    Game.Frame();
    TestTrue(TEXT("The grid returns while the key is still down"), Game.View->ShowingFullGrid());

    Game.Player->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::LeftAlt, IE_Released, 0.0f));
    Game.Frame();
    TestFalse(TEXT("Releasing Left Alt hides the grid again"), Game.View->ShowingFullGrid());
    TestEqual(TEXT("The grid lines are gone"), Game.View->OverlayBars(EStyle::Grid), 0);
    TestTrue(TEXT("The round trip changes no rules state"), ExpeditionTestSupport::SameRun(Before, Game.Live()));
    TestTrue(TEXT("The round trip writes no checkpoint"), CheckpointBefore == Game.CheckpointBytes());
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFootprintRingTest, "Project.Expedition.Presentation.FootprintRingFollowsMetric",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FFootprintRingTest::RunTest(const FString& Parameters)
{
    ExpeditionInputHarness::FHarness Game(*this, TEXT("FootprintRing"));
    if (!TestTrue(TEXT("Input, board and HUD are ready"), Game.IsReady())) { return false; }
    if (!TestTrue(TEXT("The party has the first turn"), ReachFirstPartyTurn(Game))) { return false; }
    TestEqual(TEXT("The default 150 cm cell gives a 56 cm ring"), FBoardMetric(150.0f, 180.0f).FootprintDiameter(), 56.0f);

    for (const FBoardMetric& Metric : {Game.View->Metric(), FBoardMetric(100.0f, 170.0f), FBoardMetric(220.0f, 185.0f)})
    {
        Game.View->SetBoardMetric(Metric);
        Game.Frame();
        const float Expected = 56.0f * Metric.CellCm / 150.0f;
        int32 Rings = 0;
        for (const FCombatantState& Unit : Game.View->Rules().Snapshot().Encounter.Combatants)
        {
            const float Diameter = Game.View->FootprintRingDiameterCm(Unit.Id);
            TestTrue(FString::Printf(TEXT("%s has a footprint ring at %.0f cm"), *Unit.Name, Metric.CellCm), Diameter > 0.0f);
            TestTrue(FString::Printf(TEXT("%s ring is %.1f cm across at a %.0f cm cell (got %.2f)"), *Unit.Name, Expected, Metric.CellCm, Diameter),
                FMath::IsNearlyEqual(Diameter, Expected, 0.01f));
            ++Rings;
        }
        TestTrue(TEXT("The board has figures to ring"), Rings > 0);
    }
    return !HasAnyErrors();
}
#endif
