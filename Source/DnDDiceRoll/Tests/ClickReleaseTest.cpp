#if WITH_DEV_AUTOMATION_TESTS
#include "ExpeditionInputHarness.h"

/**
 * #220: a left click on the board is a press and its release, and the release acts only if nothing changed under the press. These tests drive the real controller (the
 * harness's key and pointer events, the real HUD and camera):
 *  - every action the board click can take (a move, an attack, both specials, a rescue and a potion) commits on a plain press and release and not when the camera moved,
 *    a key or button was pressed, or the pointer dragged while the button was down;
 *  - a press is spent for good by a change of its context (mode, overlays, the Menu, the battle, playback, the enemy turn, Resume, Return to menu, focus loss);
 *  - a press and release either side of a HUD edge commits nothing.
 */
namespace ClickReleaseTest
{
using ExpeditionInputHarness::FHarness;

const FIntPoint Viewport(1280, 720);
/** Where the pointer rests on the board in the fixtures: clear of every HUD panel. */
const FVector2D Aim(640, 300);

/** What a press and release that acts on nothing leaves exactly as it found it. */
struct FStamp
{
    FExpeditionSnapshot Run;
    int32 ActiveId = INDEX_NONE, Actor = INDEX_NONE, TurnIndex = 0, Round = 0, MovementLeft = 0, Messages = 0;
    TArray<uint8> Checkpoint;
};
FStamp Stamp(const FHarness& Game)
{
    FStamp Out;
    Out.Run = Game.Live();
    Out.ActiveId = Game.View->Rules().ActiveId();
    Out.Actor = Game.Player->Actor();
    Out.TurnIndex = Game.View->Rules().Snapshot().TurnIndex;
    Out.Round = Game.View->Rules().Snapshot().Round;
    Out.MovementLeft = Game.View->Rules().Snapshot().MovementLeft;
    Out.Messages = Game.View->Messages().Num();
    Out.Checkpoint = Game.CheckpointBytes();
    return Out;
}
bool Same(const FStamp& Was, const FStamp& Now)
{
    return Was.ActiveId == Now.ActiveId && Was.TurnIndex == Now.TurnIndex && Was.Round == Now.Round && Was.Actor == Now.Actor && Was.MovementLeft == Now.MovementLeft &&
        Was.Messages == Now.Messages && Was.Checkpoint == Now.Checkpoint && ExpeditionTestSupport::SameRun(Was.Run, Now.Run);
}

void Down(FHarness& Game, FKey Key) { Game.Player->InputKey(FInputKeyEventArgs::CreateSimulated(Key, IE_Pressed, 1.0f)); }
void Up(FHarness& Game, FKey Key) { Game.Player->InputKey(FInputKeyEventArgs::CreateSimulated(Key, IE_Released, 0.0f)); }
void Frames(FHarness& Game, int32 Count, TFunctionRef<void(int32)> Each = [](int32) {})
{
    for (int32 Index = 0; Index < Count; ++Index) { Each(Index); Game.Frame(1.0f / 30.0f); }
}
void Tap(FHarness& Game, FKey Key, int32 Settle = 1) { Down(Game, Key); Game.Frame(1.0f / 30.0f); Up(Game, Key); Frames(Game, Settle); }
/** The pointer at a pixel, over a given cell: the cell stays whatever the camera does, so only the click's own guards decide. */
void Point(FHarness& Game, FVector2D Pixel, FIntPoint Cell) { Game.Player->SimulatePointer(Pixel, Cell); }

/** The first fight with the party to act: a checkpoint, optionally with the acting hero changed and the neighbours loaded for an attack, a rescue and a potion. */
bool Fight(FExpeditionSaveData& Data, int32 Hero, bool bLoaded)
{
    FString Error;
    FExpeditionRun Run;
    if (!Run.NewRun(20261030, Error, TEXT("click-release")) || !Run.ChooseRoute(Run.Progress().Offers[0].Id, Error)) { return false; }
    Data.Current = Run.Snapshot();
    if (Hero < 0) { return true; }
    FCombatSnapshot& Battle = Data.Current.Battle;
    TArray<int32> Party;
    for (const FCombatantState& Unit : Battle.Encounter.Combatants) { if (Unit.Side == ECombatSide::Party) { Party.Add(Unit.Id); } }
    if (!Party.IsValidIndex(Hero)) { return false; }
    const int32 Slot = Battle.TurnOrder.IndexOfByKey(Party[Hero]);
    if (Slot == INDEX_NONE) { return false; }
    Battle.TurnIndex = Slot;
    if (!bLoaded) { return true; }
    FCombatantState* Actor = nullptr;
    for (FCombatantState& Unit : Battle.Encounter.Combatants) { if (Unit.Id == Party[Hero]) { Actor = &Unit; } }
    if (!Actor) { return false; }
    auto Free = [&Battle](FIntPoint Cell)
    {
        for (const FCombatantState& Unit : Battle.Encounter.Combatants) { if (Unit.Cell == Cell && Unit.Life != ECombatLife::Dead) { return false; } }
        return Cell.X >= 0 && Cell.Y >= 0;
    };
    TArray<FIntPoint> Around;
    for (int32 DX = -1; DX <= 1; ++DX) for (int32 DY = -1; DY <= 1; ++DY) { const FIntPoint Cell = Actor->Cell + FIntPoint(DX, DY); if ((DX || DY) && Free(Cell)) { Around.Add(Cell); } }
    if (Around.Num() < 2) { return false; }
    // An enemy next to the actor (attack, specials), a downed ally next to it (rescue, potion).
    for (FCombatantState& Unit : Battle.Encounter.Combatants) { if (Unit.Side == ECombatSide::Enemy && Unit.Life == ECombatLife::Conscious) { Unit.Cell = Around[0]; break; } }
    for (FCombatantState& Unit : Battle.Encounter.Combatants)
    {
        if (Unit.Side == ECombatSide::Party && Unit.Id != Actor->Id) { Unit.Cell = Around[1]; Unit.Life = ECombatLife::Downed; Unit.HP = 0; Unit.RescueTurnsLeft = 2; break; }
    }
    return true;
}

/** Resumed on `Data`, the party holding the turn with targeting visible. */
TUniquePtr<FHarness> Resumed(FAutomationTestBase& Test, const FExpeditionSaveData& Data)
{
    TUniquePtr<FHarness> Game = MakeUnique<FHarness>(Test, TEXT("ClickRelease"), &Data);
    if (!Game->IsReady()) { return nullptr; }
    Game->Frame();
    Game->Press(EKeys::O);
    if (!Game->View->InCombat()) { return nullptr; }
    Game->PartyTurn();
    Frames(*Game, 90);
    if (!Game->View->InCombat() || !Game->Player->ShowsTargeting() || Game->Player->Actor() != Game->View->Rules().ActiveId()) { return nullptr; }
    const FCombatantState* Active = Game->View->Rules().Find(Game->View->Rules().ActiveId());
    return Active && Active->Side == ECombatSide::Party ? MoveTemp(Game) : nullptr;
}

/** A press and release at `Pixel` over `Cell`, nothing moving. True if it committed something. */
bool PlainClick(FHarness& Game, FVector2D Pixel, FIntPoint Cell)
{
    Point(Game, Pixel, Cell);
    Frames(Game, 3);
    const FStamp Was = Stamp(Game);
    Down(Game, EKeys::LeftMouseButton);
    Game.Frame(1.0f / 30.0f);
    Up(Game, EKeys::LeftMouseButton);
    Frames(Game, 3);
    Game.Settle();
    return !Same(Was, Stamp(Game));
}

/** The first cell the acting hero may walk to that nobody stands on (a click on a unit's cell is an attack or an inspection, not a move). */
bool FreeMove(const FHarness& Game, FIntPoint& Cell)
{
    const FCombatBattle& Rules = Game.View->Rules();
    for (const FIntPoint& Move : Rules.LegalMoves(Game.Player->Actor()))
    {
        bool bTaken = false;
        for (const FCombatantState& Unit : Rules.Snapshot().Encounter.Combatants) { bTaken |= Unit.Cell == Move && Unit.Life != ECombatLife::Dead; }
        if (!bTaken) { Cell = Move; return true; }
    }
    return false;
}

enum class EClick : uint8 { Move, Attack, Special1, Special2, Rescue, Potion, Count };
const TCHAR* ClickNames[] = {TEXT("move"), TEXT("attack"), TEXT("special 1"), TEXT("special 2"), TEXT("rescue"), TEXT("potion")};

/** The mode and a cell where this click acts now, or false when the acting hero has no such target. */
bool Candidate(FHarness& Game, EClick Click, ECombatCommandType& Mode, FIntPoint& Cell)
{
    const FCombatBattle& Rules = Game.View->Rules();
    const int32 Actor = Game.Player->Actor();
    auto CellOf = [&Rules](int32 Id) { const FCombatantState* Unit = Rules.Find(Id); return Unit ? Unit->Cell : FIntPoint(-1, -1); };
    switch (Click)
    {
    case EClick::Move: Mode = ECombatCommandType::Attack; return FreeMove(Game, Cell);
    case EClick::Attack: { Mode = ECombatCommandType::Attack; const TArray<int32> Targets = Rules.LegalTargets(Actor); if (Targets.IsEmpty()) { return false; } Cell = CellOf(Targets[0]); return true; }
    case EClick::Rescue: case EClick::Potion:
    {
        Mode = Click == EClick::Rescue ? ECombatCommandType::Rescue : ECombatCommandType::Potion;
        const TArray<int32> Targets = Rules.LegalTargets(Actor, Mode);
        if (Targets.IsEmpty()) { return false; }
        Cell = CellOf(Targets[0]);
        return true;
    }
    default:
    {
        Mode = Game.Player->SpecialAction(Click == EClick::Special1 ? 0 : 1);
        if (Mode == ECombatCommandType::Attack) { return false; }
        const TArray<int32> Targets = Rules.LegalTargets(Actor, Mode);
        if (!Targets.IsEmpty()) { Cell = CellOf(Targets[0]); return true; }
        // A special with no target of its own is cast from anywhere: any free cell will do.
        return FreeMove(Game, Cell);
    }
    }
}

/** What disturbs a click while the button is held. The first group goes through key and button events (which spend a press at once); the second moves the camera or the pointer with no event at all. */
enum class EDisturb : uint8 { OrbitKey, WheelKey, PanKeys, RightDrag, MiddleDrag, ViewOrbit, ViewZoom, ViewPan, PointerDrag, Count };
const TCHAR* DisturbNames[] = {TEXT("an orbit key"), TEXT("the wheel"), TEXT("a pan key"), TEXT("a right drag"), TEXT("a middle drag"), TEXT("the camera orbiting"), TEXT("the camera zooming"),
    TEXT("the camera panning"), TEXT("the pointer dragging")};

void Disturb(FHarness& Game, EDisturb Kind, FVector2D Pixel, FIntPoint Cell)
{
    switch (Kind)
    {
    case EDisturb::OrbitKey: Tap(Game, EKeys::Q, 40); break;
    case EDisturb::WheelKey: for (int32 Index = 0; Index < 6; ++Index) { Tap(Game, EKeys::MouseScrollDown, 2); } Frames(Game, 30); break;
    case EDisturb::PanKeys: Down(Game, EKeys::LeftShift); Down(Game, EKeys::W); Frames(Game, 30); Up(Game, EKeys::W); Up(Game, EKeys::LeftShift); Frames(Game, 10); break;
    case EDisturb::RightDrag: Down(Game, EKeys::RightMouseButton); Frames(Game, 20, [&](int32 Index) { Point(Game, Pixel + FVector2D(Index * 5.0f, 0.0f), Cell); }); Up(Game, EKeys::RightMouseButton); Point(Game, Pixel, Cell); Frames(Game, 20); break;
    case EDisturb::MiddleDrag: Down(Game, EKeys::MiddleMouseButton); Frames(Game, 20, [&](int32 Index) { Point(Game, Pixel + FVector2D(Index * 5.0f, 0.0f), Cell); }); Up(Game, EKeys::MiddleMouseButton); Point(Game, Pixel, Cell); Frames(Game, 20); break;
    case EDisturb::ViewOrbit: Game.View->Orbit(25.0f); Frames(Game, 40); break;
    case EDisturb::ViewZoom: Game.View->Zoom(-400.0f); Frames(Game, 40); break;
    case EDisturb::ViewPan: Game.View->Pan(80.0f, 80.0f); Frames(Game, 40); break;
    case EDisturb::PointerDrag: Frames(Game, 6, [&](int32 Index) { Point(Game, Pixel + FVector2D(Index * 6.0f, 0.0f), Cell); }); break;
    default: break;
    }
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLeftClickEveryActionTest, "Project.Expedition.Presentation.LeftReleaseActsForEveryAction",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FLeftClickEveryActionTest::RunTest(const FString& Parameters)
{
    using namespace ClickReleaseTest;
    // Fixtures: each hero acting, plain and with an enemy and a downed ally beside them. For every action the first fixture where the hero has a legal target is used: the
    // disturbed presses first (they must leave everything as it was), then the plain press and release, which must commit, so the disturbed ones were against a legal target.
    bool bCovered[static_cast<int32>(EClick::Count)] = {};
    int32 Disturbed = 0, ViewCalls = 0;
    for (int32 Variant = 0; Variant < 8; ++Variant)
    {
        FExpeditionSaveData Data;
        if (!Fight(Data, Variant / 2, (Variant & 1) != 0)) { continue; }
        // Which actions this fixture offers.
        TArray<EClick> Offers;
        {
            TUniquePtr<FHarness> Probe = Resumed(*this, Data);
            if (!Probe) { continue; }
            for (int32 Index = 0; Index < static_cast<int32>(EClick::Count); ++Index)
            {
                ECombatCommandType Mode; FIntPoint Cell;
                if (!bCovered[Index] && Candidate(*Probe, static_cast<EClick>(Index), Mode, Cell)) { Offers.Add(static_cast<EClick>(Index)); }
            }
        }
        for (const EClick Click : Offers)
        {
            TUniquePtr<FHarness> Game = Resumed(*this, Data);
            if (!Game) { continue; }
            ECombatCommandType Mode; FIntPoint Cell;
            if (!Candidate(*Game, Click, Mode, Cell)) { continue; }
            const FString Name = ClickNames[static_cast<int32>(Click)];
            Game->Player->PreviewMode(Mode);
            Frames(*Game, 3);
            for (int32 Kind = 0; Kind < static_cast<int32>(EDisturb::Count); ++Kind)
            {
                const FString Where = FString::Printf(TEXT("%s (hero %d, fixture %d), %s mid-press"), *Name, Variant / 2 + 1, Variant & 1, DisturbNames[Kind]);
                // A camera call that moves nothing here (a pan at the locked default zoom, a zoom at its stop) disturbs nothing, so the click it is held against is a plain one:
                // try it with the button up first and leave it out when it does not move the camera.
                const bool bViewCall = Kind >= static_cast<int32>(EDisturb::ViewOrbit) && Kind <= static_cast<int32>(EDisturb::ViewPan);
                if (bViewCall)
                {
                    const FTransform Rest = Game->View->CameraTransform();
                    Disturb(*Game, static_cast<EDisturb>(Kind), Aim, Cell);
                    const FTransform Moved = Game->View->CameraTransform();
                    const bool bMoves = FVector::Dist(Rest.GetLocation(), Moved.GetLocation()) > 1.0 || Rest.GetRotation().AngularDistance(Moved.GetRotation()) > 0.001;
                    Game->View->SnapCameraToTargets();
                    Frames(*Game, 30);
                    if (!bMoves) { continue; }
                }
                Point(*Game, Aim, Cell);
                Frames(*Game, 3);
                const FStamp Was = Stamp(*Game);
                Down(*Game, EKeys::LeftMouseButton);
                Game->Frame(1.0f / 30.0f);
                Disturb(*Game, static_cast<EDisturb>(Kind), Aim, Cell);
                Point(*Game, Aim, Cell);
                Frames(*Game, 3);
                Up(*Game, EKeys::LeftMouseButton);
                Frames(*Game, 3);
                if (bViewCall) { ++ViewCalls; }
                ++Disturbed;
                const bool bSame = Same(Was, Stamp(*Game));
                TestTrue(Where + TEXT(": the release committed nothing"), bSame);
                if (!bSame) { return false; }
                // Put the camera back for the next sequence.
                Game->View->SnapCameraToTargets();
                Frames(*Game, 30);
            }
            Game->Player->PreviewMode(Mode);
            const bool bActed = PlainClick(*Game, Aim, Cell);
            TestTrue(FString::Printf(TEXT("%s (hero %d, fixture %d): a plain press and release commits"), *Name, Variant / 2 + 1, Variant & 1), bActed);
            bCovered[static_cast<int32>(Click)] = bActed;
        }
    }
    for (int32 Index = 0; Index < static_cast<int32>(EClick::Count); ++Index)
    { TestTrue(FString::Printf(TEXT("A fixture had a legal %s target, committed by a plain click and held back when disturbed"), ClickNames[Index]), bCovered[Index]); }
    TestTrue(TEXT("Many disturbed presses were tried"), Disturbed >= 6 * 5);
    TestTrue(TEXT("The camera was moved with no key or button event in some of them"), ViewCalls >= 6);
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLeftPressSpentByContextTest, "Project.Expedition.Presentation.LeftPressIsSpentByAContextChange",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FLeftPressSpentByContextTest::RunTest(const FString& Parameters)
{
    using namespace ClickReleaseTest;
    FExpeditionSaveData Data;
    if (!TestTrue(TEXT("A fight checkpoint exists"), Fight(Data, -1, false))) { return false; }
    // Each case holds the left button on a legal move cell, changes the context, puts everything it can back, waits for the pointer's preview to return, and releases:
    // nothing may be committed. A plain click right after (the control) does.
    struct FCase { const TCHAR* Name; TFunction<void(FHarness&, FVector2D, FIntPoint)> During; };
    const TArray<FCase> Cases = {
        {TEXT("the mode switched away and back"), [](FHarness& G, FVector2D, FIntPoint) { G.Player->PreviewMode(ECombatCommandType::Rescue); G.Player->PreviewMode(ECombatCommandType::Attack); }},
        {TEXT("the mode keys F then A"), [](FHarness& G, FVector2D, FIntPoint) { G.Press(EKeys::F); G.Press(EKeys::A); }},
        {TEXT("Help opened and closed"), [](FHarness& G, FVector2D, FIntPoint) { G.Press(EKeys::Slash); G.Press(EKeys::Escape); }},
        {TEXT("Records opened and closed"), [](FHarness& G, FVector2D, FIntPoint) { G.Press(EKeys::H); G.Press(EKeys::H); }},
        {TEXT("the Return to menu question opened and dismissed"), [](FHarness& G, FVector2D, FIntPoint) { G.Press(EKeys::BackSpace); G.Press(EKeys::Escape); }},
        {TEXT("the Menu opened and closed"), [](FHarness& G, FVector2D, FIntPoint) { G.Press(EKeys::I); G.Press(EKeys::I); }},
        {TEXT("an arrow key (the keyboard cursor) pressed"), [](FHarness& G, FVector2D, FIntPoint) { G.Press(EKeys::Right); }},
        {TEXT("a command from elsewhere changing the battle"), [](FHarness& G, FVector2D, FIntPoint)
            {
                const int32 Actor = G.Player->Actor();
                const TArray<FIntPoint> Moves = G.View->Rules().LegalMoves(Actor);
                if (Moves.Num() > 1) { G.View->Execute({ECombatCommandType::Move, Actor, Moves.Last()}); }
                G.View->SkipPresentation();
            }},
        {TEXT("playback running"), [](FHarness& G, FVector2D, FIntPoint)
            {
                const int32 Actor = G.Player->Actor();
                const TArray<FIntPoint> Moves = G.View->Rules().LegalMoves(Actor);
                if (Moves.Num() > 1) { G.View->Execute({ECombatCommandType::Move, Actor, Moves.Last()}); }
                Frames(G, 2);
            }},
        {TEXT("the enemy turn passing"), [](FHarness& G, FVector2D, FIntPoint)
            {
                G.View->Execute({ECombatCommandType::EndTurn, G.Player->Actor()});
                G.PartyTurn();
            }},
        {TEXT("Resume"), [](FHarness& G, FVector2D, FIntPoint) { G.View->Resume(); G.Settle(); }},
        {TEXT("Return to menu then Resume"), [](FHarness& G, FVector2D, FIntPoint) { if (G.View->CanReturnToMenu()) { G.View->ReturnToMenu(); } G.View->Resume(); G.Settle(); }},
        {TEXT("the window losing focus"), [](FHarness& G, FVector2D, FIntPoint) { G.Player->NotifyWindowFocusLost(); }},
    };
    for (const FCase& Case : Cases)
    {
        TUniquePtr<FHarness> Game = Resumed(*this, Data);
        if (!TestTrue(FString(Case.Name) + TEXT(": the party holds the turn"), Game.IsValid())) { continue; }
        FIntPoint Cell;
        if (!TestTrue(FString(Case.Name) + TEXT(": the hero has a cell to walk to"), FreeMove(*Game, Cell))) { continue; }
        Point(*Game, Aim, Cell);
        Frames(*Game, 3);
        Down(*Game, EKeys::LeftMouseButton);
        Game->Frame(1.0f / 30.0f);
        Case.During(*Game, Aim, Cell);
        // Back over a legal cell (whatever the case did to the hero) and let the preview return before the release.
        FIntPoint After = Cell;
        FreeMove(*Game, After);
        Point(*Game, Aim, After);
        Frames(*Game, 3);
        const FStamp Before = Stamp(*Game);
        Up(*Game, EKeys::LeftMouseButton);
        Frames(*Game, 3);
        TestTrue(FString(Case.Name) + TEXT(" mid-press: the release committed nothing"), Same(Before, Stamp(*Game)));
        // The control: a plain click now acts, so the case was holding back a click that would otherwise commit. (A case that spent the hero's movement has none to try.)
        Game->Player->PreviewMode(ECombatCommandType::Attack);
        Point(*Game, Aim + FVector2D(12, 0), After);
        Frames(*Game, 2);
        Point(*Game, Aim, After);
        Frames(*Game, 3);
        FIntPoint Next;
        if (FreeMove(*Game, Next)) { TestTrue(FString(Case.Name) + TEXT(": a plain click afterwards commits"), PlainClick(*Game, Aim, Next)); }
    }
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLeftReleaseHudBoundaryTest, "Project.Expedition.Presentation.LeftReleaseAtAHudEdgeCommitsNothing",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FLeftReleaseHudBoundaryTest::RunTest(const FString& Parameters)
{
    using namespace ClickReleaseTest;
    FExpeditionSaveData Data;
    if (!TestTrue(TEXT("A fight checkpoint exists"), Fight(Data, -1, false))) { return false; }
    TUniquePtr<FHarness> Game = Resumed(*this, Data);
    if (!TestTrue(TEXT("The party holds the turn"), Game.IsValid())) { return false; }
    // HUD edges: along rows of the screen, a pixel the board owns beside one a HUD button or panel owns, at most 5 pixels apart (inside the click slop).
    struct FEdge { FVector2D Board, Hud; bool bButton; };
    TArray<FEdge> Edges;
    auto Owned = [&](float X, float Y, bool& bButton)
    {
        FName Id;
        bButton = Game->Hud->ButtonAt(FVector2D(X, Y), Id);
        return bButton || Game->Hud->CoversBoard(FVector2D(X, Y));
    };
    bool bHaveButtonEdge = false, bHavePanelEdge = false;
    for (float Y = 4; Y < Viewport.Y; Y += 6)
    {
        bool bPrevButton = false;
        bool bPrev = Owned(0, Y, bPrevButton);
        for (float X = 1; X < Viewport.X; X += 1)
        {
            bool bButton = false;
            const bool bOwned = Owned(X, Y, bButton);
            if (bOwned != bPrev)
            {
                const bool bHudSide = bOwned;
                FEdge Edge{bOwned ? FVector2D(X - 1, Y) : FVector2D(X, Y), bOwned ? FVector2D(X, Y) : FVector2D(X - 1, Y), bOwned ? bButton : bPrevButton};
                if ((Edge.bButton && !bHaveButtonEdge) || (!Edge.bButton && !bHavePanelEdge) || FMath::RandRange(0, 40) == 0) { Edges.Add(Edge); }
                bHaveButtonEdge |= Edge.bButton; bHavePanelEdge |= !Edge.bButton;
                (void)bHudSide;
            }
            bPrev = bOwned; bPrevButton = bButton;
        }
    }
    if (!TestTrue(TEXT("The HUD has an edge with a button and an edge with a panel"), bHaveButtonEdge && bHavePanelEdge)) { return false; }
    int32 BoardToHud = 0, HudToBoard = 0;
    for (const FEdge& Edge : Edges)
    {
        FIntPoint Cell;
        if (!FreeMove(*Game, Cell)) { break; }
        const FString Where = FString::Printf(TEXT("board (%.0f, %.0f) and %s (%.0f, %.0f)"), Edge.Board.X, Edge.Board.Y, Edge.bButton ? TEXT("button") : TEXT("panel"), Edge.Hud.X, Edge.Hud.Y);
        // Board to HUD, released before the next preview: the old preview is still the board cell's.
        Point(*Game, Edge.Board, Cell);
        Frames(*Game, 3);
        FStamp Was = Stamp(*Game);
        Down(*Game, EKeys::LeftMouseButton);
        Game->Frame(1.0f / 30.0f);
        Point(*Game, Edge.Hud, Cell);
        Up(*Game, EKeys::LeftMouseButton);
        TestTrue(Where + TEXT(": a press on the board released on the HUD, before another frame, commits nothing"), Same(Was, Stamp(*Game)));
        Frames(*Game, 3);
        TestTrue(Where + TEXT(": ... and nothing follows a frame later"), Same(Was, Stamp(*Game)));
        ++BoardToHud;
        // The same with a frame on the HUD before the release.
        Point(*Game, Edge.Board, Cell);
        Frames(*Game, 3);
        Was = Stamp(*Game);
        Down(*Game, EKeys::LeftMouseButton);
        Game->Frame(1.0f / 30.0f);
        Point(*Game, Edge.Hud, Cell);
        Frames(*Game, 3);
        Up(*Game, EKeys::LeftMouseButton);
        Frames(*Game, 3);
        TestTrue(Where + TEXT(": released on the HUD after a frame commits nothing"), Same(Was, Stamp(*Game)));
        // HUD to board: a press on a panel (a button would act on the press) released on the board beside it.
        if (!Edge.bButton)
        {
            Point(*Game, Edge.Hud, Cell);
            Frames(*Game, 3);
            Was = Stamp(*Game);
            Down(*Game, EKeys::LeftMouseButton);
            Game->Frame(1.0f / 30.0f);
            Point(*Game, Edge.Board, Cell);
            Frames(*Game, 3);
            Up(*Game, EKeys::LeftMouseButton);
            Frames(*Game, 3);
            TestTrue(Where + TEXT(": a press on a HUD panel released on the board commits nothing"), Same(Was, Stamp(*Game)));
            ++HudToBoard;
        }
    }
    TestTrue(TEXT("Board to HUD edges were tried"), BoardToHud >= 2);
    TestTrue(TEXT("HUD to board edges were tried"), HudToBoard >= 1);
    // The control: on the board, away from the HUD, the same press and release acts.
    FIntPoint Last;
    if (TestTrue(TEXT("A cell is left to walk to"), FreeMove(*Game, Last))) { TestTrue(TEXT("A plain press and release on the board commits"), PlainClick(*Game, Aim, Last)); }
    return !HasAnyErrors();
}
#endif
