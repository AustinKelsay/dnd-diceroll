#if WITH_DEV_AUTOMATION_TESTS
#include "ExpeditionInputHarness.h"
#include "Combat/ActionFacts.h"
#include "Presentation/TargetChipLayout.h"

/**
 * #171: pointer and keyboard share the board. These tests drive the real controller and HUD through the
 * harness's simulated pointer (a transient world has no viewport to project a real one through).
 */
namespace PointerInputTest
{
using ExpeditionInputHarness::FHarness;

/** The first party turn of the first encounter, in the real input path. */
bool FirstPartyTurn(FHarness& Game)
{
    Game.Frame();
    Game.Press(EKeys::Enter);
    Game.Settle();
    Game.Press(EKeys::Enter);
    if (!Game.View->InCombat()) { return false; }
    Game.PartyTurn();
    return Game.View->InCombat() && Game.Player->ShowsTargeting();
}

bool Occupied(const FCombatBattle& Rules, FIntPoint Cell)
{
    for (const FCombatantState& Unit : Rules.Snapshot().Encounter.Combatants) { if (Unit.Cell == Cell && Unit.Life != ECombatLife::Dead) { return true; } }
    return false;
}

/** Empty cells the acting hero can walk to, farthest first so a move is a clear change. */
TArray<FIntPoint> FreeMoves(const FHarness& Game)
{
    TArray<FIntPoint> Cells;
    for (const FIntPoint Cell : Game.View->Rules().LegalMoves(Game.Player->Actor())) { if (!Occupied(Game.View->Rules(), Cell)) { Cells.Add(Cell); } }
    return Cells;
}

FIntPoint ActorCell(const FHarness& Game) { return Game.View->Rules().Find(Game.Player->Actor())->Cell; }

/** Two viewport positions over the open board (no panel there at 1280x720). */
const FVector2D BoardSpotA(700, 300), BoardSpotB(760, 340);
}

/**
 * The defect: after an arrow key the pointer was ignored until a click, and that click acted on a cell the player never saw
 * previewed. Pointer motion hands the board back and previews the pointed cell; a click only ever acts on a previewed cell;
 * the keyboard still takes the board back from the pointer.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPointerAfterKeyboardTest, "Project.Expedition.Presentation.PointerAfterKeyboardPreviewsBeforeCommit",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FPointerAfterKeyboardTest::RunTest(const FString& Parameters)
{
    using namespace PointerInputTest;
    FHarness Game(*this, TEXT("PointerAfterKeyboard"));
    if (!TestTrue(TEXT("Input, board and HUD are ready"), Game.IsReady())) { return false; }
    if (!TestTrue(TEXT("The party has the first turn"), FirstPartyTurn(Game))) { return false; }
    const TArray<FIntPoint> Free = FreeMoves(Game);
    if (!TestTrue(TEXT("The hero has two cells to walk to"), Free.Num() >= 2)) { return false; }
    const FIntPoint A = Free[0], B = Free.Last();
    const FIntPoint Start = ActorCell(Game);

    // The pointer rests over cell A; the player then uses the keyboard.
    Game.Player->SimulatePointer(BoardSpotA, A);
    Game.Frame();
    TestFalse(TEXT("The pointer alone does not show the keyboard cursor"), Game.Player->UsingKeyboardCursor());
    Game.Press(EKeys::Left);
    Game.Press(EKeys::Left);
    TestTrue(TEXT("Arrows show the keyboard cursor"), Game.Player->UsingKeyboardCursor());
    for (int32 Index = 0; Index < 5; ++Index) { Game.Frame(); }
    TestTrue(TEXT("A pointer that does not move leaves the keyboard cursor alone"), Game.Player->UsingKeyboardCursor());

    // A pointer that jitters within a couple of pixels is not motion either.
    Game.Player->SimulatePointer(BoardSpotA + FVector2D(1, 1), A);
    Game.Frame();
    TestTrue(TEXT("A one-pixel wobble leaves the keyboard cursor alone"), Game.Player->UsingKeyboardCursor());

    // Pointer motion hands the board back, and the pointed cell is previewed (path and bracket) at once.
    Game.Player->SimulatePointer(BoardSpotB, B);
    Game.Frame();
    TestFalse(TEXT("Pointer motion hides the keyboard cursor"), Game.Player->UsingKeyboardCursor());
    TestFalse(TEXT("The HUD no longer draws keyboard brackets"), Game.Hud->KeyboardCursorRequested());
    FIntPoint Previewed;
    TestTrue(TEXT("The pointed cell is previewed"), Game.Player->PreviewedPointerCell(Previewed) && Previewed == B);
    TestTrue(TEXT("The preview is the path to the pointed cell"), !Game.Player->PreviewPath().IsEmpty() && Game.Player->PreviewPath().Last() == B);
    TestEqual(TEXT("Previewing acts on nothing"), ActorCell(Game), Start);

    // The keyboard takes the board back from the pointer, and a resting pointer does not take it again.
    Game.Press(EKeys::Right);
    TestTrue(TEXT("An arrow after pointer use shows the keyboard cursor again"), Game.Player->UsingKeyboardCursor());
    for (int32 Index = 0; Index < 5; ++Index) { Game.Frame(); }
    TestTrue(TEXT("... and it stays while the pointer rests"), Game.Player->UsingKeyboardCursor());
    FIntPoint Cursor;
    TestTrue(TEXT("The keyboard cursor owns the hover cell"), Game.Player->KeyboardCell(Cursor) && Game.Player->HoverCell(Cursor) && Cursor == Game.Player->KeyboardCursorCell());
    TestFalse(TEXT("While it does, no pointer cell is previewed"), Game.Player->PreviewedPointerCell(Previewed));

    // A click while the keyboard cursor still owns the board only hands it back: the pointed cell was never previewed.
    Game.Press(EKeys::LeftMouseButton);
    TestFalse(TEXT("A click with the keyboard cursor showing hides it"), Game.Player->UsingKeyboardCursor());
    TestEqual(TEXT("... and acts on nothing"), ActorCell(Game), Start);
    TestTrue(TEXT("The pointed cell is now previewed"), Game.Player->PreviewedPointerCell(Previewed) && Previewed == B);

    // Now the click acts on exactly the previewed cell.
    Game.Press(EKeys::LeftMouseButton);
    Game.Settle();
    TestEqual(TEXT("The next click moves to the previewed cell"), ActorCell(Game), B);
    return !HasAnyErrors();
}

/** A click is never faster than the preview: one that lands before the pointed cell was shown does nothing. */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FClickNeedsPreviewTest, "Project.Expedition.Presentation.ClickActsOnlyOnPreviewedCell",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FClickNeedsPreviewTest::RunTest(const FString& Parameters)
{
    using namespace PointerInputTest;
    FHarness Game(*this, TEXT("ClickNeedsPreview"));
    if (!TestTrue(TEXT("Input, board and HUD are ready"), Game.IsReady())) { return false; }
    if (!TestTrue(TEXT("The party has the first turn"), FirstPartyTurn(Game))) { return false; }
    const TArray<FIntPoint> Free = FreeMoves(Game);
    if (!TestFalse(TEXT("The hero has a cell to walk to"), Free.IsEmpty())) { return false; }
    const FIntPoint Target = Free[0], Start = ActorCell(Game);

    // The pointer arrives and clicks within one frame, before the cell was ever drawn as previewed.
    Game.Player->SimulatePointer(BoardSpotA, Target);
    Game.Press(EKeys::LeftMouseButton);
    TestEqual(TEXT("A click before the preview acts on nothing"), ActorCell(Game), Start);
    FIntPoint Previewed;
    TestTrue(TEXT("The cell is previewed by the next frame"), Game.Player->PreviewedPointerCell(Previewed) && Previewed == Target);
    Game.Press(EKeys::LeftMouseButton);
    Game.Settle();
    TestEqual(TEXT("A click on the previewed cell moves there"), ActorCell(Game), Target);
    return !HasAnyErrors();
}

/**
 * The same pointer rules over the full-status cards: clicking a party card keeps its hover card while the pointer is on it.
 * Reported on screen as the card vanishing after a click on a hero who is not acting.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPartyCardClickKeepsHoverTest, "Project.Expedition.Presentation.PartyCardClickKeepsHoverCard",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FPartyCardClickKeepsHoverTest::RunTest(const FString& Parameters)
{
    using namespace PointerInputTest;
    FHarness Game(*this, TEXT("PartyCardClick"));
    if (!TestTrue(TEXT("Input, board and HUD are ready"), Game.IsReady())) { return false; }
    if (!TestTrue(TEXT("The party has the first turn"), FirstPartyTurn(Game))) { return false; }
    const FCombatBattle& Rules = Game.View->Rules();
    TArray<int32> Party;
    for (const FCombatantState& Unit : Rules.Snapshot().Encounter.Combatants) { if (Unit.Side == ECombatSide::Party) { Party.Add(Unit.Id); } }
    const int32 ActiveId = Rules.ActiveId();
    const int32 ActiveIndex = Party.IndexOfByKey(ActiveId);
    if (!TestTrue(TEXT("A party member acts"), ActiveIndex != INDEX_NONE && Party.Num() >= 3)) { return false; }
    const int32 OtherIndex = ActiveIndex == 0 ? 1 : 0;
    const double Scale = ExpeditionLayout::ViewScale(Game.Target.Size.X, Game.Target.Size.Y);
    auto CardCentre = [Scale](int32 Index) { return FVector2D(108.0, 54.0 + Index * 68.0 + 32.0) * Scale; };
    auto Shown = [&Game](int32 Id) { return Game.Hud->PartyDetailUnits().Contains(Id); };

    // The acting hero's details are open, as when the player pressed their number.
    Game.Press(ActiveIndex == 0 ? EKeys::One : ActiveIndex == 1 ? EKeys::Two : ActiveIndex == 2 ? EKeys::Three : EKeys::Four);
    TestTrue(TEXT("The acting hero's details are open"), Game.Player->Inspected() == ActiveId && Shown(ActiveId));

    Game.Player->SimulatePointer(CardCentre(OtherIndex), FIntPoint(0, 0));
    Game.Frame();
    TestTrue(TEXT("Hovering a hero who is not acting shows their card"), Shown(Party[OtherIndex]));
    Game.Press(EKeys::LeftMouseButton);
    Game.Frame();
    TestEqual(TEXT("The click chooses that hero"), Game.Player->Selection(), Party[OtherIndex]);
    TestTrue(TEXT("Their card is still up with the pointer on it"), Shown(Party[OtherIndex]));
    Game.Press(EKeys::LeftMouseButton);
    Game.Frame();
    TestTrue(TEXT("A second click keeps it"), Shown(Party[OtherIndex]));
    Game.Press(EKeys::LeftMouseButton);
    Game.Frame();
    TestTrue(TEXT("A third click keeps it"), Shown(Party[OtherIndex]));
    return !HasAnyErrors();
}

/** The legend gets a backing panel in the top band, clear of every status card and button, at every size. */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLegendPlacementTest, "Project.Expedition.Presentation.TargetingLegendIsBackedAndClear",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FLegendPlacementTest::RunTest(const FString& Parameters)
{
    using namespace PointerInputTest;
    FHarness Game(*this, TEXT("LegendPlacement"));
    if (!TestTrue(TEXT("Input, board and HUD are ready"), Game.IsReady())) { return false; }
    if (!TestTrue(TEXT("The party has the first turn"), FirstPartyTurn(Game))) { return false; }
    const FCombatBattle& Rules = Game.View->Rules();
    TArray<int32> Party;
    for (const FCombatantState& Unit : Rules.Snapshot().Encounter.Combatants) { if (Unit.Side == ECombatSide::Party) { Party.Add(Unit.Id); } }
    for (const FIntPoint Size : {FIntPoint(1280, 720), FIntPoint(1920, 1080), FIntPoint(1600, 600), FIntPoint(1024, 576), FIntPoint(2560, 1080)})
    {
        const FString Where = FString::Printf(TEXT("%dx%d"), Size.X, Size.Y);
        Game.Target.Size = Size;
        const double Scale = ExpeditionLayout::ViewScale(Size.X, Size.Y);
        // Open as many full-status cards as can be up at once: the acting hero's, another hero's by hover.
        Game.Player->SimulatePointer(FVector2D(108.0, 54.0 + 68.0 * 3 + 32.0) * Scale, FIntPoint(0, 0));
        Game.Frame();
        const FBox2D* Legend = Game.Hud->LegendRequested();
        if (!TestNotNull(Where + TEXT(": targeting shows the legend"), Legend)) { continue; }
        const FBox2D Box = *Legend;
        TestTrue(Where + TEXT(": the legend is inside the viewport"), Box.Min.X >= 0 && Box.Min.Y >= 0 && Box.Max.X <= Size.X && Box.Max.Y <= Size.Y);
        TestTrue(Where + TEXT(": the legend is a real panel, not a bare line"), Box.GetSize().X > 120 * Scale && Box.GetSize().Y >= 18 * Scale);
        TestFalse(Where + TEXT(": a hovered card is up"), Game.Hud->PartyDetailRects().IsEmpty());
        for (const FBox2D& Card : Game.Hud->PartyDetailRects()) { TestFalse(Where + TEXT(": the legend is not under a full-status card"), Box.Intersect(Card)); }
        for (const FBox2D& Button : Game.Hud->ButtonRects()) { TestFalse(Where + TEXT(": the legend is not under a button"), Box.Intersect(Button)); }
    }
    return !HasAnyErrors();
}

/** The path-cost number's plate is moved clear of the cell panel, the chips and the HUD, wherever the path ends. */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPathCostPlateTest, "Project.Expedition.Presentation.PathCostPlateStaysClear",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FPathCostPlateTest::RunTest(const FString& Parameters)
{
    using ExpeditionLayout::PathCostPlate; using ExpeditionLayout::Overlaps; using ExpeditionLayout::Point;
    using Rect = ExpeditionLayout::Rect;
    using ExpeditionLayout::DesignWidth; using ExpeditionLayout::DesignHeight;
    const Rect Bounds = {8, 52, DesignWidth - 8, DesignHeight - 112};
    const double Width = 26, Height = 24;
    auto Clear = [&](const Rect& Plate, const std::vector<Rect>& Occupied, const FString& Where)
    {
        TestTrue(Where + TEXT(": inside the board area"), Plate.Left >= Bounds.Left && Plate.Top >= Bounds.Top && Plate.Right <= Bounds.Right && Plate.Bottom <= Bounds.Bottom);
        for (const Rect& Other : Occupied) { TestFalse(Where + TEXT(": clear of a panel"), Overlaps(Plate, Other)); }
    };
    // Nothing near: the plate sits just above the end of the path.
    {
        const Rect Plate = PathCostPlate({640, 400}, Width, Height, Bounds, {});
        TestTrue(TEXT("Alone, the plate is centred above the endpoint"), FMath::IsNearlyEqual(float((Plate.Left + Plate.Right) * 0.5), 640.0f, 0.5f) && Plate.Bottom < 400);
    }
    // The reported case: the cell panel (the forecast) hangs to the lower right of the pointer, over the endpoint's plate.
    for (const Point End : {Point{900, 330}, Point{1100, 300}, Point{700, 500}, Point{1230, 460}, Point{300, 200}})
    {
        const double PanelX = std::min(End.X + 20, DesignWidth - 400 - 8), PanelY = std::min(End.Y + 18, DesignHeight - 12 - 32 - 100);
        std::vector<Rect> Occupied = {{PanelX, PanelY, PanelX + 400, PanelY + 32},          // The cell panel under the pointer.
            {End.X - 120, End.Y - 70, End.X + 60, End.Y - 46}, {End.X - 40, End.Y + 6, End.X + 150, End.Y + 30}, // Target chips around it.
            {8, 54, 208, 318}};                                                           // The party cards.
        const Rect Plate = PathCostPlate(End, Width, Height, Bounds, Occupied);
        Clear(Plate, Occupied, FString::Printf(TEXT("End %.0f,%.0f"), End.X, End.Y));
    }
    return !HasAnyErrors();
}

/**
 * The attack's reach outline describes an action the hero can still take. With the Action spent it must not show,
 * though the rules still know the geometry; before it is spent it does.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSpentActionReachTest, "Project.Expedition.Presentation.ReachContourHiddenWhenActionSpent",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSpentActionReachTest::RunTest(const FString& Parameters)
{
    using namespace PointerInputTest;
    FHarness Game(*this, TEXT("SpentActionReach"));
    if (!TestTrue(TEXT("Input, board and HUD are ready"), Game.IsReady())) { return false; }
    if (!TestTrue(TEXT("The party has the first turn"), FirstPartyTurn(Game))) { return false; }
    int32 Spent = 0;
    for (int32 Turn = 0; Turn < 24 && Game.View->InCombat() && Spent < 2; ++Turn)
    {
        Game.PartyTurn();
        if (!Game.View->InCombat()) { break; }
        const FCombatBattle& Rules = Game.View->Rules();
        const int32 Id = Rules.ActiveId();
        Game.Press(EKeys::A);
        if (!Game.Player->ShowsTargeting()) { break; }
        if (Rules.Snapshot().bActionAvailable)
        {
            TestFalse(TEXT("With the Action available the reach outline shows"), Game.Player->Reach().Visible.IsEmpty() && Game.Player->Reach().Obstructed.IsEmpty());
            const TArray<int32> Targets = Rules.LegalTargets(Id);
            if (!Targets.IsEmpty())
            {
                Game.View->Execute({ECombatCommandType::Attack, Id, {}, Targets[0]});
                Game.Settle();
                if (!Game.View->InCombat() || Game.View->Rules().ActiveId() != Id) { continue; }
                Game.Press(EKeys::A);
                ++Spent;
                TestFalse(TEXT("The Action is spent"), Game.View->Rules().Snapshot().bActionAvailable);
                TestTrue(TEXT("Targeting is still on"), Game.Player->ShowsTargeting());
                TestTrue(TEXT("The rules still know the geometry"), !Game.View->Rules().ActionReach(Id, ECombatCommandType::Attack).Visible.IsEmpty());
                TestTrue(TEXT("The outline is not shown for an Action already spent"), Game.Player->Reach().Visible.IsEmpty() && Game.Player->Reach().Obstructed.IsEmpty());
                TestEqual(TEXT("... and no reach marker is requested"), Game.Hud->ReachMarkersRequested(), 0);
            }
            else
            {
                // Close in on the nearest enemy so a later turn has a target.
                const TArray<FIntPoint> Moves = Rules.LegalMoves(Id);
                FIntPoint Best = Rules.Find(Id)->Cell;
                int32 BestDistance = 1000;
                for (const FIntPoint Cell : Moves)
                {
                    for (const FCombatantState& Enemy : Rules.Snapshot().Encounter.Combatants)
                    {
                        if (Enemy.Side != ECombatSide::Enemy || Enemy.Life == ECombatLife::Dead) { continue; }
                        const int32 Distance = FMath::Max(FMath::Abs(Cell.X - Enemy.Cell.X), FMath::Abs(Cell.Y - Enemy.Cell.Y));
                        if (Distance < BestDistance) { BestDistance = Distance; Best = Cell; }
                    }
                }
                if (!Moves.IsEmpty()) { Game.View->Execute({ECombatCommandType::Move, Id, Best}); Game.Settle(); }
                const TArray<int32> After = Game.View->InCombat() && Game.View->Rules().ActiveId() == Id ? Game.View->Rules().LegalTargets(Id) : TArray<int32>();
                if (!After.IsEmpty())
                {
                    Game.View->Execute({ECombatCommandType::Attack, Id, {}, After[0]});
                    Game.Settle();
                    if (Game.View->InCombat() && Game.View->Rules().ActiveId() == Id)
                    {
                        Game.Press(EKeys::A);
                        ++Spent;
                        TestTrue(TEXT("The outline is not shown for an Action already spent"), Game.Player->Reach().Visible.IsEmpty() && Game.Player->Reach().Obstructed.IsEmpty());
                        TestEqual(TEXT("... and no reach marker is requested"), Game.Hud->ReachMarkersRequested(), 0);
                    }
                }
            }
        }
        if (Game.View->InCombat() && Game.View->Rules().ActiveId() == Id) { Game.Press(EKeys::SpaceBar); }
    }
    TestTrue(TEXT("An Action was spent and checked"), Spent > 0);
    return !HasAnyErrors();
}
#endif
