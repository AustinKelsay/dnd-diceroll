#if WITH_DEV_AUTOMATION_TESTS
#include "ExpeditionInputHarness.h"
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
    const double Width = 26, Height = 24;
    // The design space each viewport size gives the HUD (the layout scales to the smaller of the two ratios).
    for (const FIntPoint Size : {FIntPoint(1280, 720), FIntPoint(1920, 1080), FIntPoint(1600, 600)})
    {
        const double Scale = ExpeditionLayout::ViewScale(Size.X, Size.Y);
        const double W = Size.X / Scale, H = Size.Y / Scale;
        const FString Where = FString::Printf(TEXT("%dx%d"), Size.X, Size.Y);
        const Rect Bounds = {8, 52, W - 8, H - 112};
        auto Clear = [&](const Rect& Plate, const std::vector<Rect>& Occupied, const FString& Context)
        {
            TestTrue(Where + Context + TEXT(": inside the board area"), Plate.Left >= Bounds.Left && Plate.Top >= Bounds.Top && Plate.Right <= Bounds.Right && Plate.Bottom <= Bounds.Bottom);
            for (const Rect& Other : Occupied) { TestFalse(Where + Context + TEXT(": clear of a panel"), Overlaps(Plate, Other)); }
        };
        // Nothing near: the plate sits just above the end of the path.
        const Rect Alone = PathCostPlate({W * 0.5, H * 0.5}, Width, Height, Bounds, {});
        TestTrue(Where + TEXT(": alone, the plate is centred above the endpoint"), FMath::IsNearlyEqual(float((Alone.Left + Alone.Right) * 0.5), float(W * 0.5), 0.5f) && Alone.Bottom < H * 0.5);
        // The reported case: the cell panel (the forecast) hangs to the lower right of the pointer, over the endpoint's plate.
        // Ends in the middle and on every edge and corner of the board area.
        const Point Ends[] = {{W * 0.7, H * 0.45}, {W * 0.85, H * 0.4}, {W * 0.5, H * 0.7}, {W - 12, H * 0.6}, {280, 200},
            {Bounds.Left + 2, Bounds.Top + 2}, {Bounds.Right - 2, Bounds.Top + 2}, {Bounds.Left + 2, Bounds.Bottom - 2}, {Bounds.Right - 2, Bounds.Bottom - 2},
            {W * 0.5, Bounds.Top + 2}, {W * 0.5, Bounds.Bottom - 2}, {Bounds.Left + 2, H * 0.5}, {Bounds.Right - 2, H * 0.5}};
        for (const Point End : Ends)
        {
            const double PanelX = std::min(End.X + 20, W - 400 - 8), PanelY = std::min(End.Y + 18, H - 12 - 32 - 100);
            std::vector<Rect> Occupied = {{PanelX, PanelY, PanelX + 400, PanelY + 32},          // The cell panel under the pointer.
                {End.X - 120, End.Y - 70, End.X + 60, End.Y - 46}, {End.X - 40, End.Y + 6, End.X + 150, End.Y + 30}, // Target chips around it.
                {8, 54, 208, 318}};                                                           // The party cards.
            const Rect Plate = PathCostPlate(End, Width, Height, Bounds, Occupied);
            Clear(Plate, Occupied, FString::Printf(TEXT(" end %.0f,%.0f"), End.X, End.Y));
        }
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

/**
 * #171 review: a preview exists only while targeting is visible. Moving one square starts playback; a cell hovered for a
 * frame during it was never shown as a target, so a click on it must not commit (Execute would accept it).
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPlaybackPointerTest, "Project.Expedition.Presentation.NoPointerCommitDuringPlayback",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FPlaybackPointerTest::RunTest(const FString& Parameters)
{
    using namespace PointerInputTest;
    FHarness Game(*this, TEXT("PlaybackPointer"));
    if (!TestTrue(TEXT("Input, board and HUD are ready"), Game.IsReady())) { return false; }
    if (!TestTrue(TEXT("The party has the first turn"), FirstPartyTurn(Game))) { return false; }
    const int32 Actor = Game.Player->Actor();
    const TArray<FIntPoint> Free = FreeMoves(Game);
    if (!TestFalse(TEXT("The hero has a cell to walk to"), Free.IsEmpty())) { return false; }
    const FIntPoint First = Free[0];
    Game.Player->SimulatePointer(BoardSpotA, First);
    Game.Frame();
    Game.Press(EKeys::LeftMouseButton);
    if (!TestEqual(TEXT("The first click moves the hero one step"), ActorCell(Game), First)) { return false; }
    if (!TestTrue(TEXT("The move is playing back"), Game.View->IsPresenting())) { return false; }
    TArray<FIntPoint> Next = FreeMoves(Game);
    Next.Remove(First);
    if (!TestFalse(TEXT("Another cell is still reachable"), Next.IsEmpty())) { return false; }
    const FIntPoint Second = Next[0];
    const int32 MovementLeft = Game.View->Rules().Snapshot().MovementLeft;

    // Hover another cell for one frame during playback (no preview is drawn: targeting is hidden), then click it.
    Game.Player->SimulatePointer(BoardSpotB, Second);
    Game.View->Tick(0.01f);
    Game.Player->PlayerTick(0.01f);
    if (!TestTrue(TEXT("Playback is still running at the click"), Game.View->IsPresenting())) { return false; }
    TestFalse(TEXT("No targeting shows during playback"), Game.Player->ShowsTargeting());
    FIntPoint Previewed;
    TestFalse(TEXT("No cell is previewed during playback"), Game.Player->PreviewedPointerCell(Previewed));
    Game.Player->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::LeftMouseButton, IE_Pressed, 1.0f));
    Game.Player->PlayerTick(0.01f);
    Game.Player->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::LeftMouseButton, IE_Released, 0.0f));
    TestEqual(TEXT("The click during playback moves nobody"), ActorCell(Game), First);
    TestEqual(TEXT("... and spends no movement"), Game.View->Rules().Snapshot().MovementLeft, MovementLeft);
    TestEqual(TEXT("The same hero is still acting"), Game.Player->Actor(), Actor);

    // Once playback is over and the cell has been previewed, the click acts.
    Game.Settle();
    Game.Player->SimulatePointer(BoardSpotB, Second);
    Game.Frame();
    TestTrue(TEXT("The cell is previewed after playback"), Game.Player->PreviewedPointerCell(Previewed) && Previewed == Second);
    Game.Press(EKeys::LeftMouseButton);
    Game.Settle();
    TestEqual(TEXT("A click on the previewed cell then moves there"), ActorCell(Game), Second);
    return !HasAnyErrors();
}

/** A preview belongs to the state it was drawn for: a change of battle state or mode before the click voids it. */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPreviewInvalidatedTest, "Project.Expedition.Presentation.PreviewVoidedByStateChange",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FPreviewInvalidatedTest::RunTest(const FString& Parameters)
{
    using namespace PointerInputTest;
    FHarness Game(*this, TEXT("PreviewInvalidated"));
    if (!TestTrue(TEXT("Input, board and HUD are ready"), Game.IsReady())) { return false; }
    if (!TestTrue(TEXT("The party has the first turn"), FirstPartyTurn(Game))) { return false; }
    const int32 Actor = Game.Player->Actor();
    const FIntPoint Start = ActorCell(Game);
    TArray<FIntPoint> Free = FreeMoves(Game);
    auto Distance = [&Start](FIntPoint Cell) { return FMath::Max(FMath::Abs(Cell.X - Start.X), FMath::Abs(Cell.Y - Start.Y)); };
    Free.Sort([&Distance](FIntPoint A, FIntPoint B) { return Distance(A) == Distance(B) ? (A.Y == B.Y ? A.X < B.X : A.Y < B.Y) : Distance(A) < Distance(B); });
    if (!TestTrue(TEXT("The hero has two cells to walk to"), Free.Num() >= 2)) { return false; }
    // The pointer rests on the hero's own cell; after the hero steps away that cell is a legal move back.
    const FIntPoint Elsewhere = Free[0], Pointed = Start;
    FIntPoint Previewed;

    // The battle state changes between the preview and the click (a command from elsewhere).
    Game.Player->SimulatePointer(BoardSpotA, Pointed);
    Game.Frame();
    TestTrue(TEXT("The cell is previewed"), Game.Player->PreviewedPointerCell(Previewed) && Previewed == Pointed);
    Game.View->Execute({ECombatCommandType::Move, Actor, Elsewhere});
    Game.View->SkipPresentation();
    if (!TestTrue(TEXT("The pointed cell is still a legal move, so only the stale preview stops the click"), Game.View->Rules().LegalMoves(Actor).Contains(Pointed))) { return false; }
    Game.Player->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::LeftMouseButton, IE_Pressed, 1.0f));
    Game.Player->PlayerTick(0.01f);
    Game.Player->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::LeftMouseButton, IE_Released, 0.0f));
    TestEqual(TEXT("A click on a preview from before the state changed moves nobody"), ActorCell(Game), Elsewhere);

    // The mode changes between the preview and the click: nothing is attempted (a rejected command would be logged).
    Game.Settle();
    Game.Player->SimulatePointer(BoardSpotB, Pointed);
    Game.Frame();
    TestTrue(TEXT("The cell is previewed again"), Game.Player->PreviewedPointerCell(Previewed) && Previewed == Pointed);
    const FString LastMessage = Game.View->Messages().IsEmpty() ? FString() : Game.View->Messages().Last();
    Game.Player->PreviewMode(ECombatCommandType::Rescue);
    TestFalse(TEXT("A changed mode voids the preview"), Game.Player->PreviewedPointerCell(Previewed));
    Game.Player->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::LeftMouseButton, IE_Pressed, 1.0f));
    Game.Player->PlayerTick(0.01f);
    Game.Player->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::LeftMouseButton, IE_Released, 0.0f));
    TestEqual(TEXT("A click after a mode change moves nobody"), ActorCell(Game), Elsewhere);
    TestEqual(TEXT("... and attempts no command"), Game.View->Messages().IsEmpty() ? FString() : Game.View->Messages().Last(), LastMessage);
    return !HasAnyErrors();
}

/**
 * #171 review: Enter acts on the keyboard cell, so it must be the cell the player last saw marked. With the keyboard
 * cursor hidden (pointer took over), the first Enter shows it again and acts on nothing; the next Enter acts.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnterNeedsVisibleCursorTest, "Project.Expedition.Presentation.EnterNeedsVisibleKeyboardCursor",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FEnterNeedsVisibleCursorTest::RunTest(const FString& Parameters)
{
    using namespace PointerInputTest;
    FHarness Game(*this, TEXT("EnterNeedsCursor"));
    if (!TestTrue(TEXT("Input, board and HUD are ready"), Game.IsReady())) { return false; }
    if (!TestTrue(TEXT("The party has the first turn"), FirstPartyTurn(Game))) { return false; }
    const TArray<FIntPoint> Free = FreeMoves(Game);
    if (!TestTrue(TEXT("The hero has two cells to walk to"), Free.Num() >= 2)) { return false; }
    const FIntPoint A = Free[0], B = Free.Last();
    const FIntPoint Start = ActorCell(Game);

    // The keyboard cursor is on A; the pointer then moves onto B, and B is previewed.
    Game.Player->SimulatePointer(BoardSpotA, B);
    Game.Frame();
    Game.Player->PreviewAim(true, A);
    Game.Frame();
    TestTrue(TEXT("The keyboard cursor is on A"), Game.Player->UsingKeyboardCursor() && Game.Player->KeyboardCursorCell() == A);
    Game.Player->SimulatePointer(BoardSpotB, B);
    Game.Frame();
    FIntPoint Previewed;
    TestFalse(TEXT("Pointer motion hid the keyboard cursor"), Game.Player->UsingKeyboardCursor());
    TestTrue(TEXT("B is previewed"), Game.Player->PreviewedPointerCell(Previewed) && Previewed == B);

    Game.Press(EKeys::Enter);
    Game.Settle();
    TestEqual(TEXT("The first Enter moves nobody"), ActorCell(Game), Start);
    TestTrue(TEXT("... it shows the keyboard cursor on its cell"), Game.Player->UsingKeyboardCursor() && Game.Player->KeyboardCursorCell() == A);
    Game.Press(EKeys::Enter);
    Game.Settle();
    TestEqual(TEXT("The next Enter acts on the cell that was shown"), ActorCell(Game), A);

    // A cursor that was never shown is no different: Enter shows it first.
    Game.Player->PreviewAim(false);
    Game.Frame();
    const FIntPoint Now = ActorCell(Game);
    Game.Press(EKeys::Enter);
    Game.Settle();
    TestEqual(TEXT("Enter with no cursor showing moves nobody"), ActorCell(Game), Now);
    TestTrue(TEXT("... and shows the cursor"), Game.Player->UsingKeyboardCursor());
    return !HasAnyErrors();
}

/**
 * #171 round 2: Enter commits only a cell the last frame drew marked with targeting visible. During playback nothing is
 * marked, so Enter there must not commit; a cursor moved since the last frame waits for the next one; with the cursor
 * showing as it will act, one Enter acts.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnterDuringPlaybackTest, "Project.Expedition.Presentation.NoEnterCommitDuringPlayback",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FEnterDuringPlaybackTest::RunTest(const FString& Parameters)
{
    using namespace PointerInputTest;
    FHarness Game(*this, TEXT("EnterDuringPlayback"));
    if (!TestTrue(TEXT("Input, board and HUD are ready"), Game.IsReady())) { return false; }
    if (!TestTrue(TEXT("The party has the first turn"), FirstPartyTurn(Game))) { return false; }
    const int32 Actor = Game.Player->Actor();
    const FIntPoint Start = ActorCell(Game);
    TArray<FIntPoint> Free = FreeMoves(Game);
    auto Distance = [&Start](FIntPoint Cell) { return FMath::Max(FMath::Abs(Cell.X - Start.X), FMath::Abs(Cell.Y - Start.Y)); };
    Free.Sort([&Distance](FIntPoint A, FIntPoint B) { return Distance(A) == Distance(B) ? (A.Y == B.Y ? A.X < B.X : A.Y < B.Y) : Distance(A) < Distance(B); });
    if (!TestTrue(TEXT("The hero has cells to walk to"), Free.Num() >= 2)) { return false; }
    const FIntPoint First = Free[0];

    // One Enter acts when the cursor is showing on the cell as it will act.
    Game.Player->PreviewAim(true, First);
    Game.Frame();
    TestTrue(TEXT("The cursor shows on the cell"), Game.Player->UsingKeyboardCursor() && Game.Player->KeyboardCursorCell() == First);
    Game.Press(EKeys::Enter);
    if (!TestEqual(TEXT("One Enter moves the hero to the marked cell"), ActorCell(Game), First)) { return false; }
    if (!TestTrue(TEXT("The move is playing back"), Game.View->IsPresenting())) { return false; }

    // During playback the cursor is moved to another reachable cell (the way back); nothing is marked, so Enter commits nothing.
    if (!TestTrue(TEXT("The way back is a legal move"), Game.View->Rules().LegalMoves(Actor).Contains(Start))) { return false; }
    const int32 MovementLeft = Game.View->Rules().Snapshot().MovementLeft;
    Game.Player->PreviewAim(true, Start);
    Game.View->Tick(0.01f);
    Game.Player->PlayerTick(0.01f);
    if (!TestTrue(TEXT("Playback is still running at Enter"), Game.View->IsPresenting())) { return false; }
    TestFalse(TEXT("No targeting shows during playback"), Game.Player->ShowsTargeting());
    Game.Player->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::Enter, IE_Pressed, 1.0f));
    Game.Player->PlayerTick(0.01f);
    Game.Player->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::Enter, IE_Released, 0.0f));
    TestEqual(TEXT("Enter during playback moves nobody"), ActorCell(Game), First);
    TestEqual(TEXT("... and spends no movement"), Game.View->Rules().Snapshot().MovementLeft, MovementLeft);

    // Once playback is over the cursor is drawn marked; a cursor moved since the last frame is not acted on until drawn.
    Game.Settle();
    TestTrue(TEXT("The cursor is drawn again after playback"), Game.Player->UsingKeyboardCursor() && Game.Player->KeyboardCursorCell() == Start);
    TArray<FIntPoint> More = FreeMoves(Game);
    More.Remove(Start);
    Game.Player->PreviewAim(true, Start);
    Game.Frame();
    if (!More.IsEmpty())
    {
        const FIntPoint Later = More[0];
        Game.Player->PreviewAim(true, Later);
        Game.Player->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::Enter, IE_Pressed, 1.0f));
        Game.Player->PlayerTick(0.01f);
        Game.Player->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::Enter, IE_Released, 0.0f));
        TestEqual(TEXT("Enter on a cursor moved since the last frame acts on nothing"), ActorCell(Game), First);
    }
    Game.Player->PreviewAim(true, Start);
    Game.Frame();
    Game.Press(EKeys::Enter);
    Game.Settle();
    TestEqual(TEXT("With the cursor drawn on it, one Enter moves there"), ActorCell(Game), Start);
    return !HasAnyErrors();
}

/**
 * #171 round 2: the camera eases on its own, so the cell under a resting pointer can change after the preview was drawn.
 * SimulatePointer with the same position and another cell stands for that changed projection.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectionChangeTest, "Project.Expedition.Presentation.ClickNeedsUnchangedProjection",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FProjectionChangeTest::RunTest(const FString& Parameters)
{
    using namespace PointerInputTest;
    FHarness Game(*this, TEXT("ProjectionChange"));
    if (!TestTrue(TEXT("Input, board and HUD are ready"), Game.IsReady())) { return false; }
    if (!TestTrue(TEXT("The party has the first turn"), FirstPartyTurn(Game))) { return false; }
    const TArray<FIntPoint> Free = FreeMoves(Game);
    if (!TestTrue(TEXT("The hero has two cells to walk to"), Free.Num() >= 2)) { return false; }
    const FIntPoint A = Free[0], B = Free.Last();
    const FIntPoint Start = ActorCell(Game);
    FIntPoint Previewed;

    Game.Player->SimulatePointer(BoardSpotA, A);
    Game.Frame();
    TestTrue(TEXT("A is previewed"), Game.Player->PreviewedPointerCell(Previewed) && Previewed == A);
    // The pointer does not move, but the camera does: cell B is now under it. Combat state is unchanged.
    Game.Player->SimulatePointer(BoardSpotA, B);
    Game.Player->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::LeftMouseButton, IE_Pressed, 1.0f));
    Game.Player->PlayerTick(0.01f);
    Game.Player->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::LeftMouseButton, IE_Released, 0.0f));
    TestEqual(TEXT("A click on the old preview acts on neither cell"), ActorCell(Game), Start);
    Game.Frame();
    TestTrue(TEXT("The next frame previews the cell now under the pointer"), Game.Player->PreviewedPointerCell(Previewed) && Previewed == B);
    Game.Press(EKeys::LeftMouseButton);
    Game.Settle();
    TestEqual(TEXT("A click on it then moves there"), ActorCell(Game), B);
    return !HasAnyErrors();
}

namespace PointerInputTest
{
/** One game frame at 30 fps with a key pressed, then released, as a player's tap. */
void Tap(FHarness& Game, FKey Key)
{
    Game.Player->InputKey(FInputKeyEventArgs::CreateSimulated(Key, IE_Pressed, 1.0f));
    Game.Frame(1.0f / 30.0f);
    Game.Player->InputKey(FInputKeyEventArgs::CreateSimulated(Key, IE_Released, 0.0f));
}
bool PoseMoved(const FTransform& A, const FTransform& B)
{
    return FVector::Dist(A.GetLocation(), B.GetLocation()) > 0.5f || FMath::RadiansToDegrees(A.GetRotation().AngularDistance(B.GetRotation())) > 0.02f;
}
/** The cell under a viewport pixel through the real camera, or false. */
bool CellAt(FHarness& Game, FVector2D Pixel, FIntPoint& Cell)
{
    Game.Player->SimulatePointerThroughCamera(Pixel, Game.Target.Size);
    return Game.Player->HoverCell(Cell);
}
}

/**
 * #171 round 3: the camera eases on its own under a resting pointer. This drives the real camera (Q through the controller,
 * then ticks of the easing) and the real projection through it: no click may commit while the camera is still moving, and
 * never on a cell that is not the one under the pointer; once it has settled and the new cell is previewed, a click acts.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCameraEaseClickTest, "Project.Expedition.Presentation.ClickNeedsStillCamera",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FCameraEaseClickTest::RunTest(const FString& Parameters)
{
    using namespace PointerInputTest;
    FHarness Game(*this, TEXT("CameraEaseClick"));
    if (!TestTrue(TEXT("Input, board and HUD are ready"), Game.IsReady())) { return false; }
    if (!TestTrue(TEXT("The party has the first turn"), FirstPartyTurn(Game))) { return false; }
    for (int32 Index = 0; Index < 90; ++Index) { Game.Frame(1.0f / 30.0f); }
    const FIntPoint Start = ActorCell(Game);
    const TArray<FIntPoint> Free = FreeMoves(Game);
    auto IsFree = [&Free](FIntPoint Cell) { return Free.Contains(Cell); };

    // Find a pixel over a free reachable cell A that, after a quarter turn, lies over another free reachable cell B.
    TArray<TPair<FVector2D, FIntPoint>> Candidates;
    for (float Y = 160; Y < 560; Y += 12) for (float X = 300; X < 1200; X += 12)
    {
        FIntPoint Cell;
        if (CellAt(Game, FVector2D(X, Y), Cell) && IsFree(Cell) && !Game.Hud->CoversBoard(FVector2D(X, Y))) { Candidates.Add({FVector2D(X, Y), Cell}); }
    }
    if (!TestFalse(TEXT("Some pixels lie over free reachable cells"), Candidates.IsEmpty())) { return false; }
    Tap(Game, EKeys::Q);
    for (int32 Index = 0; Index < 150; ++Index) { Game.Frame(1.0f / 30.0f); }
    FVector2D Pixel = FVector2D::ZeroVector;
    FIntPoint A, B;
    bool bFound = false;
    for (const TPair<FVector2D, FIntPoint>& Candidate : Candidates)
    {
        FIntPoint After;
        if (CellAt(Game, Candidate.Key, After) && IsFree(After) && After != Candidate.Value && !Game.Hud->CoversBoard(Candidate.Key)) { Pixel = Candidate.Key; A = Candidate.Value; B = After; bFound = true; break; }
    }
    Tap(Game, EKeys::E);
    for (int32 Index = 0; Index < 150; ++Index) { Game.Frame(1.0f / 30.0f); }
    if (!TestTrue(TEXT("A pixel changes from one free reachable cell to another across a quarter turn"), bFound)) { return false; }
    FIntPoint Here;
    if (!TestTrue(TEXT("The camera is back where it started"), CellAt(Game, Pixel, Here) && Here == A)) { return false; }

    // The pointer rests on A and A is previewed.
    Game.Frame(1.0f / 30.0f);
    FIntPoint Previewed;
    TestTrue(TEXT("A is previewed"), Game.Player->PreviewedPointerCell(Previewed) && Previewed == A);
    Tap(Game, EKeys::Q);
    int32 EasingFrames = 0, Commits = 0;
    FTransform Before = Game.View->CameraTransform();
    for (int32 Frame = 0; Frame < 120 && !Commits; ++Frame)
    {
        // A click arrives with this frame's input, before the camera moves on.
        const FTransform Now = Game.View->CameraTransform();
        const bool bMoving = PoseMoved(Before, Now);
        FIntPoint Under;
        const bool bOver = CellAt(Game, Pixel, Under);
        Game.Player->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::LeftMouseButton, IE_Pressed, 1.0f));
        Game.Frame(1.0f / 30.0f);
        Game.Player->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::LeftMouseButton, IE_Released, 0.0f));
        if (bMoving) { ++EasingFrames; }
        if (ActorCell(Game) != Start)
        {
            ++Commits;
            TestFalse(FString::Printf(TEXT("Frame %d: no click commits while the camera is still moving"), Frame), bMoving);
            TestTrue(TEXT("A commit only goes to the cell that was under the pointer"), bOver && ActorCell(Game) == Under);
            TestNotEqual(TEXT("... never to the cell previewed before the camera moved"), ActorCell(Game), A);
        }
        Before = Now;
    }
    TestTrue(TEXT("The camera really did ease under the resting pointer"), EasingFrames >= 3);
    Game.Settle();
    if (Commits == 0)
    {
        for (int32 Index = 0; Index < 6; ++Index) { Game.Frame(1.0f / 30.0f); }
        TestTrue(TEXT("Once settled, the cell now under the pointer is previewed"), Game.Player->PreviewedPointerCell(Previewed) && Previewed == B);
        Game.Press(EKeys::LeftMouseButton);
        Game.Settle();
        TestEqual(TEXT("... and a click then moves there"), ActorCell(Game), B);
    }
    return !HasAnyErrors();
}

/**
 * #171 round 3: after a committed move, a second Enter (with an arrow in between) never commits while the first move is
 * still playing back; it commits only after playback ends, when it is a legitimate second move. Real arrow keys, 30 fps ticks.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSecondEnterTimingTest, "Project.Expedition.Presentation.SecondEnterOnlyAfterPlayback",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSecondEnterTimingTest::RunTest(const FString& Parameters)
{
    using namespace PointerInputTest;
    int32 DuringPlayback = 0, AfterPlayback = 0;
    for (const int32 Gap : {2, 4, 6, 8, 12, 18, 30, 60})
    {
        FHarness Game(*this, TEXT("SecondEnter"));
        if (!TestTrue(TEXT("Input, board and HUD are ready"), Game.IsReady())) { return false; }
        if (!TestTrue(TEXT("The party has the first turn"), FirstPartyTurn(Game))) { return false; }
        for (int32 Index = 0; Index < 60; ++Index) { Game.Frame(1.0f / 30.0f); }
        const FIntPoint Start = ActorCell(Game);
        TArray<FIntPoint> Free = FreeMoves(Game);
        auto Distance = [&Start](FIntPoint Cell) { return FMath::Max(FMath::Abs(Cell.X - Start.X), FMath::Abs(Cell.Y - Start.Y)); };
        Free.Sort([&Distance](FIntPoint A, FIntPoint B) { return Distance(A) == Distance(B) ? (A.Y == B.Y ? A.X < B.X : A.Y < B.Y) : Distance(A) < Distance(B); });
        // Three squares away so the playback is a real walk.
        FIntPoint First = Free.Last();
        for (const FIntPoint Cell : Free) { if (Distance(Cell) == 3) { First = Cell; break; } }
        const FString Where = FString::Printf(TEXT("gap %d frames"), Gap);
        Game.Player->PreviewAim(true, First);
        Game.Frame(1.0f / 30.0f);
        Tap(Game, EKeys::Enter);
        if (!TestEqual(Where + TEXT(": the first Enter moves to the cursor cell"), ActorCell(Game), First)) { return false; }
        const bool bPlaying = Game.View->IsPresenting();
        TestTrue(Where + TEXT(": the move plays back"), bPlaying);
        // The gap, then a real arrow and the second Enter.
        for (int32 Index = 0; Index < Gap; ++Index) { Game.Frame(1.0f / 30.0f); }
        Tap(Game, EKeys::Left);
        const bool bPresentingAtEnter = Game.View->IsPresenting();
        const FIntPoint Cursor = Game.Player->KeyboardCursorCell();
        const int32 MovementLeft = Game.View->Rules().Snapshot().MovementLeft;
        Tap(Game, EKeys::Enter);
        const bool bMoved = Game.View->Rules().Snapshot().MovementLeft != MovementLeft;
        if (bPresentingAtEnter)
        {
            ++DuringPlayback;
            TestFalse(Where + TEXT(": a second Enter during playback commits nothing"), bMoved);
            TestEqual(Where + TEXT(": ... the hero stays"), ActorCell(Game), First);
        }
        else
        {
            ++AfterPlayback;
            if (bMoved) { TestEqual(Where + TEXT(": a second Enter after playback acts on the cursor cell"), ActorCell(Game), Cursor); }
        }
    }
    AddInfo(FString::Printf(TEXT("Second Enter landed during playback in %d runs and after it in %d"), DuringPlayback, AfterPlayback));
    TestTrue(TEXT("Some second Enter landed inside the playback"), DuringPlayback > 0);
    return !HasAnyErrors();
}

/** Closing an overlay does not restore a preview by itself: a click in the same frame does nothing, the next frame previews, then a click acts. */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOverlayCloseClickTest, "Project.Expedition.Presentation.ClickAfterOverlayNeedsFreshPreview",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FOverlayCloseClickTest::RunTest(const FString& Parameters)
{
    using namespace PointerInputTest;
    FHarness Game(*this, TEXT("OverlayCloseClick"));
    if (!TestTrue(TEXT("Input, board and HUD are ready"), Game.IsReady())) { return false; }
    if (!TestTrue(TEXT("The party has the first turn"), FirstPartyTurn(Game))) { return false; }
    const TArray<FIntPoint> Free = FreeMoves(Game);
    if (!TestFalse(TEXT("The hero has a cell to walk to"), Free.IsEmpty())) { return false; }
    const FIntPoint Target = Free[0], Start = ActorCell(Game);
    Game.Player->SimulatePointer(BoardSpotA, Target);
    Game.Frame();
    FIntPoint Previewed;
    TestTrue(TEXT("The cell is previewed"), Game.Player->PreviewedPointerCell(Previewed) && Previewed == Target);
    Game.Press(EKeys::Slash);
    TestTrue(TEXT("Help is open"), Game.Player->OverlayOpen());
    Game.Frame();
    TestFalse(TEXT("No preview while Help is open"), Game.Player->PreviewedPointerCell(Previewed));
    // Esc and a click arrive in the same frame, in either order.
    Game.Player->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::Escape, IE_Pressed, 1.0f));
    Game.Player->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::LeftMouseButton, IE_Pressed, 1.0f));
    Game.Frame(1.0f / 30.0f);
    Game.Player->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::Escape, IE_Released, 0.0f));
    Game.Player->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::LeftMouseButton, IE_Released, 0.0f));
    TestFalse(TEXT("Help is closed"), Game.Player->OverlayOpen());
    TestEqual(TEXT("The click in the closing frame moves nobody"), ActorCell(Game), Start);
    // The HUD's panel rectangles are those of the frame drawn with Help open, so the pointer still reads as over a panel for one frame.
    Game.Frame(1.0f / 30.0f);
    TestTrue(TEXT("The cell is previewed again within a frame or two"), Game.Player->PreviewedPointerCell(Previewed) && Previewed == Target);
    Game.Press(EKeys::LeftMouseButton);
    Game.Settle();
    TestEqual(TEXT("A click then moves there"), ActorCell(Game), Target);
    return !HasAnyErrors();
}
#endif
