#if WITH_DEV_AUTOMATION_TESTS
#include "ExpeditionInputHarness.h"

/**
 * #208: camera-only input never changes the battle. The on-screen pass of the release candidate saw the party's turns pass in
 * the Vault finale while a rig sent only wheel, Q/E, a middle-drag and a stray left-button drag. These tests drive the real
 * controller, camera and HUD (the harness's key events, the real projection through the eased camera) and pin the invariant
 * in the first fight and in the finale: nothing the camera does submits a command, and a left click ends a turn only on the
 * End Turn button, never on the board.
 */
namespace CameraInputTest
{
using ExpeditionInputHarness::FHarness;

/** What a camera-only input must leave exactly as it found it. */
struct FBattleStamp
{
    FExpeditionSnapshot Run;
    int32 ActiveId = INDEX_NONE;
    int32 Actor = INDEX_NONE;
    int32 TurnIndex = 0;
    int32 Round = 0;
    int32 MovementLeft = 0;
    int32 Messages = 0;
    TArray<uint8> Checkpoint;
};

FBattleStamp Stamp(const FHarness& Game)
{
    FBattleStamp Stamp;
    Stamp.Run = Game.Live();
    Stamp.ActiveId = Game.View->Rules().ActiveId();
    Stamp.Actor = Game.Player->Actor();
    Stamp.TurnIndex = Game.View->Rules().Snapshot().TurnIndex;
    Stamp.Round = Game.View->Rules().Snapshot().Round;
    Stamp.MovementLeft = Game.View->Rules().Snapshot().MovementLeft;
    Stamp.Messages = Game.View->Messages().Num();
    Stamp.Checkpoint = Game.CheckpointBytes();
    return Stamp;
}

/** True when the turn and the acting hero are the ones of `Was` (a move or an attack may change everything else). */
bool SameTurn(const FBattleStamp& Was, const FBattleStamp& Now)
{
    return Was.ActiveId == Now.ActiveId && Was.TurnIndex == Now.TurnIndex && Was.Round == Now.Round;
}

/** True when nothing about the battle, the run, the log or the checkpoint file differs. */
bool SameEverything(const FBattleStamp& Was, const FBattleStamp& Now)
{
    return SameTurn(Was, Now) && Was.Actor == Now.Actor && Was.MovementLeft == Now.MovementLeft && Was.Messages == Now.Messages &&
        Was.Checkpoint == Now.Checkpoint && ExpeditionTestSupport::SameRun(Was.Run, Now.Run);
}

const FIntPoint Viewport(1280, 720);

/** A harness resumed on a checkpoint of `Run`, the party holding the turn and playback finished. */
bool Resume(FHarness& Game)
{
    Game.Frame();
    Game.Press(EKeys::O);
    if (!Game.View->InCombat()) { return false; }
    Game.PartyTurn();
    for (int32 Index = 0; Index < 90; ++Index) { Game.Frame(1.0f / 30.0f); }
    return Game.View->InCombat() && Game.Player->ShowsTargeting();
}

void Point(FHarness& Game, FVector2D Pixel) { Game.Player->SimulatePointerThroughCamera(Pixel, Viewport); }
void Down(FHarness& Game, FKey Key) { Game.Player->InputKey(FInputKeyEventArgs::CreateSimulated(Key, IE_Pressed, 1.0f)); }
void Up(FHarness& Game, FKey Key) { Game.Player->InputKey(FInputKeyEventArgs::CreateSimulated(Key, IE_Released, 0.0f)); }
void Frames(FHarness& Game, int32 Count, TFunctionRef<void(int32)> Each = [](int32) {})
{
    for (int32 Index = 0; Index < Count; ++Index) { Each(Index); Game.Frame(1.0f / 30.0f); }
}
void Tap(FHarness& Game, FKey Key, int32 Settle = 1)
{
    Down(Game, Key);
    Game.Frame(1.0f / 30.0f);
    Up(Game, Key);
    Frames(Game, Settle);
}

/** A pixel the HUD reports as the End Turn button, or false. */
bool EndTurnPixel(FHarness& Game, FVector2D& Pixel)
{
    for (float Y = 0; Y < Viewport.Y; Y += 4) for (float X = 0; X < Viewport.X; X += 4)
    {
        FName Button;
        if (Game.Hud->ButtonAt(FVector2D(X, Y), Button) && Button == FName(TEXT("EndTurn"))) { Pixel = FVector2D(X, Y); return true; }
    }
    return false;
}

/** Everything the camera can be asked to do, one input at a time, as a real player and the on-screen rig send it. */
void ExerciseCamera(FHarness& Game, FAutomationTestBase& Test, const FString& Where, const FBattleStamp& Was)
{
    int32 Step = 0;
    auto Check = [&](const TCHAR* What)
    {
        ++Step;
        const FBattleStamp Now = Stamp(Game);
        Test.TestTrue(FString::Printf(TEXT("%s: after %s the battle, the run, the log and the checkpoint are unchanged"), *Where, What), SameEverything(Was, Now));
    };
    const FVector2D Board(640, 300);

    // Q and E, a quarter turn each way and back, letting the easing play out.
    for (int32 Index = 0; Index < 3; ++Index) { Tap(Game, EKeys::Q, 40); }
    Check(TEXT("Q x3"));
    for (int32 Index = 0; Index < 6; ++Index) { Tap(Game, EKeys::E, 40); }
    Check(TEXT("E x6"));
    for (int32 Index = 0; Index < 3; ++Index) { Tap(Game, EKeys::Q, 40); }
    Check(TEXT("Q x3 back"));
    // Q and E one after another with no pause, the camera still easing.
    for (int32 Index = 0; Index < 8; ++Index) { Tap(Game, Index % 3 ? EKeys::E : EKeys::Q, 2); }
    Frames(Game, 60);
    Check(TEXT("Q/E mashed mid-ease"));

    // The wheel, both ways, to the stops and back, with the pointer over the board and then over the End Turn button.
    Point(Game, Board);
    for (int32 Index = 0; Index < 40; ++Index) { Tap(Game, EKeys::MouseScrollDown, 2); }
    Frames(Game, 30);
    Check(TEXT("wheel out to the far stop"));
    for (int32 Index = 0; Index < 80; ++Index) { Tap(Game, EKeys::MouseScrollUp, 2); }
    Frames(Game, 30);
    Check(TEXT("wheel in to the near stop"));
    for (int32 Index = 0; Index < 30; ++Index) { Tap(Game, EKeys::MouseScrollDown, 2); }
    Frames(Game, 30);
    Check(TEXT("wheel back out"));

    // Shift with W, A, S and D pans the focus (A is also the attack mode and D puts a hint away, neither of which acts).
    Down(Game, EKeys::LeftShift);
    for (const FKey& Key : {EKeys::W, EKeys::A, EKeys::S, EKeys::D, EKeys::W, EKeys::D})
    {
        Down(Game, Key);
        Frames(Game, 45);
        Up(Game, Key);
        Frames(Game, 4);
    }
    Up(Game, EKeys::LeftShift);
    Frames(Game, 10);
    Check(TEXT("Shift + W/A/S/D panning"));

    // Middle-drag pans, right-drag orbits, with the pointer moving frame by frame.
    Point(Game, Board);
    Frames(Game, 3);
    Down(Game, EKeys::MiddleMouseButton);
    Frames(Game, 40, [&](int32 Index) { Point(Game, Board + FVector2D(Index * 6.0f, Index * -3.0f)); });
    Up(Game, EKeys::MiddleMouseButton);
    Frames(Game, 10);
    Check(TEXT("a middle-button drag"));
    Point(Game, Board);
    Frames(Game, 3);
    Down(Game, EKeys::RightMouseButton);
    Frames(Game, 60, [&](int32 Index) { Point(Game, Board + FVector2D(Index * 7.0f, 0.0f)); });
    Up(Game, EKeys::RightMouseButton);
    Frames(Game, 90);
    Check(TEXT("a right-button drag"));

    // The pointer resting on the End Turn button while every camera input runs: hovering is not a click.
    FVector2D EndTurn;
    if (Test.TestTrue(Where + TEXT(": the HUD draws an End Turn button"), EndTurnPixel(Game, EndTurn)))
    {
        Point(Game, EndTurn);
        Frames(Game, 5);
        Tap(Game, EKeys::E, 40);
        Tap(Game, EKeys::Q, 40);
        for (int32 Index = 0; Index < 20; ++Index) { Tap(Game, EKeys::MouseScrollDown, 1); }
        for (int32 Index = 0; Index < 20; ++Index) { Tap(Game, EKeys::MouseScrollUp, 1); }
        Down(Game, EKeys::LeftShift); Down(Game, EKeys::D); Frames(Game, 30); Up(Game, EKeys::D); Up(Game, EKeys::LeftShift);
        Down(Game, EKeys::MiddleMouseButton); Frames(Game, 20); Up(Game, EKeys::MiddleMouseButton);
        Down(Game, EKeys::RightMouseButton); Frames(Game, 20); Up(Game, EKeys::RightMouseButton);
        Frames(Game, 60);
        Check(TEXT("every camera input with the pointer resting on End Turn"));
    }

    // And a long while with nothing pressed: the party's turn does not pass by itself.
    Point(Game, Board);
    Frames(Game, 600);
    Check(TEXT("ten seconds idle"));
    Test.TestTrue(Where + TEXT(": the party still holds the turn"), Game.Player->ShowsTargeting() && Game.View->Rules().ActiveId() == Was.ActiveId);
}

/** The party's fight in the first encounter or the finale, saved in `Data` for the harness to resume. */
bool FightCheckpoint(FExpeditionSaveData& Data, bool bFinale)
{
    FString Error;
    FExpeditionRun Run;
    if (!bFinale)
    {
        if (!Run.NewRun(20261030, Error, TEXT("camera-input-first")) || !Run.ChooseRoute(Run.Progress().Offers[0].Id, Error)) { return false; }
        const FCombatantState* Active = Run.Battle().Find(Run.Battle().ActiveId());
        Data.Current = Run.Snapshot();
        return Active != nullptr;
    }
    // Any finale the party opens, with the party moving first as it does when the finale is played on screen.
    const int32 Seed = ExpeditionTestSupport::FirstSeedWhere([&Run](int32 Candidate) { Run = FExpeditionRun(); return ExpeditionTestSupport::ReachFinale(Run, Candidate, -1, true); }, 1, 400);
    if (Seed == INDEX_NONE) { return false; }
    Data.Current = Run.Snapshot();
    return true;
}

}

/**
 * Wheel, Q/E, Shift+W/A/S/D, middle- and right-drags and a resting pointer, including one resting on End Turn, change nothing
 * about the battle, the run, the log or the checkpoint, in the first fight and in the Vault finale.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCameraOnlyInputTest, "Project.Expedition.Presentation.CameraInputNeverChangesBattle",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FCameraOnlyInputTest::RunTest(const FString& Parameters)
{
    using namespace CameraInputTest;
    for (const bool bFinale : {false, true})
    {
        const FString Where = bFinale ? TEXT("Vault finale") : TEXT("First fight");
        FExpeditionSaveData Data;
        if (!TestTrue(Where + TEXT(": a fight checkpoint exists"), FightCheckpoint(Data, bFinale))) { return false; }
        FHarness Game(*this, TEXT("CameraOnlyInput"), &Data);
        if (!TestTrue(Where + TEXT(": input, board and HUD are ready"), Game.IsReady())) { return false; }
        if (!TestTrue(Where + TEXT(": the party holds the turn"), Resume(Game))) { return false; }
        if (bFinale) { TestTrue(Where + TEXT(": it is the fifth fight"), Game.View->Progress().History.Num() >= FExpeditionRun::EncounterCount); }
        const FBattleStamp Was = Stamp(Game);
        ExerciseCamera(Game, *this, Where, Was);
    }
    return !HasAnyErrors();
}

/**
 * A left press, alone or as the start of a drag, never ends a turn except on the End Turn button: it is swept over the whole
 * screen with the camera still and with the camera easing, in the first fight and in the finale. The button does end it.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLeftClickEndsTurnOnlyOnButtonTest, "Project.Expedition.Presentation.LeftClickEndsTurnOnlyOnTheButton",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FLeftClickEndsTurnOnlyOnButtonTest::RunTest(const FString& Parameters)
{
    using namespace CameraInputTest;
    for (const bool bFinale : {false, true})
    {
        const FString Where = bFinale ? TEXT("Vault finale") : TEXT("First fight");
        FExpeditionSaveData Data;
        if (!TestTrue(Where + TEXT(": a fight checkpoint exists"), FightCheckpoint(Data, bFinale))) { return false; }
        FHarness Game(*this, TEXT("LeftClickEndsTurn"), &Data);
        if (!TestTrue(Where + TEXT(": input, board and HUD are ready"), Game.IsReady())) { return false; }
        if (!TestTrue(Where + TEXT(": the party holds the turn"), Resume(Game))) { return false; }

        int32 Presses = 0, Skipped = 0, Acted = 0, Easing = 0;
        // Camera still: a preview frame, then the press, over every 48 px cell of the screen.
        // Camera easing: the same, with a quarter turn under way, where a press is voided by the camera guard.
        for (const bool bEase : {false, true})
        {
            for (float Y = 12; Y < Viewport.Y && Game.View->InCombat(); Y += 48) for (float X = 12; X < Viewport.X; X += 48)
            {
                FName Button;
                const FVector2D Pixel(X, Y);
                if (Game.Hud->ButtonAt(Pixel, Button)) { ++Skipped; continue; }
                Point(Game, Pixel);
                Game.Frame(1.0f / 30.0f);
                if (bEase) { Tap(Game, EKeys::Q, 0); ++Easing; }
                const FBattleStamp Before = Stamp(Game);
                Down(Game, EKeys::LeftMouseButton);
                Game.Frame(1.0f / 30.0f);
                // A drag: the pointer moves across a few pixels with the button down before it is released.
                Point(Game, Pixel + FVector2D(12, 6));
                Game.Frame(1.0f / 30.0f);
                Up(Game, EKeys::LeftMouseButton);
                Game.Frame(1.0f / 30.0f);
                ++Presses;
                const FBattleStamp After = Stamp(Game);
                if (!SameTurn(Before, After))
                {
                    AddError(FString::Printf(TEXT("%s: a left press at (%.0f, %.0f) passed the turn (active %d to %d, turn %d to %d)"), *Where, X, Y,
                        Before.ActiveId, After.ActiveId, Before.TurnIndex, After.TurnIndex));
                    return false;
                }
                if (After.Messages != Before.Messages) { ++Acted; }
                Game.Settle();
                if (bEase) { Frames(Game, 30); Tap(Game, EKeys::E, 30); }
            }
        }
        AddInfo(FString::Printf(TEXT("%s: %d left presses (%d of them with the camera easing), %d pixels on a button skipped, %d presses acted without ending a turn"), *Where, Presses, Easing, Skipped, Acted));
        TestTrue(Where + TEXT(": the sweep pressed the board many times"), Presses > 400);

        // The control: the End Turn button, pressed with the left button, does pass the turn.
        if (!Game.View->InCombat() || !Game.Player->ShowsTargeting()) { continue; }
        FVector2D EndTurn;
        if (!TestTrue(Where + TEXT(": the HUD draws an End Turn button"), EndTurnPixel(Game, EndTurn))) { return false; }
        const FBattleStamp Before = Stamp(Game);
        Point(Game, EndTurn);
        Game.Frame(1.0f / 30.0f);
        Tap(Game, EKeys::LeftMouseButton, 1);
        TestFalse(Where + TEXT(": a left press on the End Turn button passes the turn"), SameTurn(Before, Stamp(Game)));
    }
    return !HasAnyErrors();
}
#endif
