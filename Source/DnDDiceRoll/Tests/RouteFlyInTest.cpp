#if WITH_DEV_AUTOMATION_TESTS
#include "ExpeditionInputHarness.h"
#include "Presentation/ExpeditionCapture.h"
#include "Presentation/JourneyLayout.h"
#include "Presentation/RouteFlyIn.h"

/**
 * #249: choosing a route on the journey map plays a short move into the chosen diorama, then the board fades up at the fight's opening framing. These tests drive the
 * real controller, HUD and camera headlessly (the harness's key and pointer events):
 *  - the timeline and path are pure and end looking into the diorama from the battle camera's side;
 *  - the move starts on the committed route command, from a key, a card click or a diorama click, after the run and checkpoint are already written;
 *  - any key or click skips it, and the skip acts on nothing (not even a skip click's release over a live board cell);
 *  - it converges under Instant, Finish (skip), a command, Resume and the primitive look, always to the same run, checkpoint bytes and opening camera as today's cut.
 */
namespace RouteFlyInTest
{
using ExpeditionInputHarness::FHarness;

void Down(FHarness& Game, FKey Key) { Game.Player->InputKey(FInputKeyEventArgs::CreateSimulated(Key, IE_Pressed, 1.0f)); }
void Up(FHarness& Game, FKey Key) { Game.Player->InputKey(FInputKeyEventArgs::CreateSimulated(Key, IE_Released, 0.0f)); }
void Frames(FHarness& Game, int32 Count) { for (int32 Index = 0; Index < Count; ++Index) { Game.Frame(); } }

/** A fresh run's route screen whose offer `Offer` opens a fight the party acts first in (so a stray key or click would have something to act on). */
bool RouteScreen(FExpeditionSaveData& Data, int32& Offer)
{
    for (int32 Seed = 20261030; Seed < 20261030 + 64; ++Seed)
    {
        FString Error;
        FExpeditionRun Run;
        if (!Run.NewRun(Seed, Error, TEXT("route-flyin"))) { continue; }
        for (Offer = 0; Offer < FMath::Min(2, Run.Progress().Offers.Num()); ++Offer)
        {
            FExpeditionRun Taken = Run;
            if (!Taken.ChooseRoute(Run.Progress().Offers[Offer].Id, Error)) { continue; }
            const FCombatantState* First = Taken.Battle().Find(Taken.Battle().ActiveId());
            if (First && First->Side == ECombatSide::Party) { Data.Current = Run.Snapshot(); return true; }
        }
    }
    return false;
}

/** The route screen resumed through the real input path, the journey map showing. */
TUniquePtr<FHarness> Open(FAutomationTestBase& Test, const FExpeditionSaveData& Data)
{
    TUniquePtr<FHarness> Game = MakeUnique<FHarness>(Test, TEXT("RouteFlyIn"), &Data);
    if (!Game->IsReady()) { return nullptr; }
    Game->Frame();
    Game->Press(EKeys::O);
    Game->Settle();
    return Game->View->HasRun() && Game->View->Progress().Phase == EExpeditionPhase::Route && Game->View->ShowingJourney() ? MoveTemp(Game) : nullptr;
}

FKey RouteKey(int32 Offer) { return Offer == 0 ? EKeys::Z : EKeys::X; }

/** Where a fight comes to rest: the run, the checkpoint bytes and the settled camera. */
struct FOutcome
{
    FExpeditionSnapshot Run;
    TArray<uint8> Bytes;
    FTransform Camera;
};
FOutcome Rest(FHarness& Game)
{
    Frames(Game, 150);
    return {Game.Live(), Game.CheckpointBytes(), Game.View->CameraTransform()};
}
bool SameCamera(const FTransform& A, const FTransform& B)
{
    return FVector::Dist(A.GetLocation(), B.GetLocation()) <= 1.0 && FMath::RadiansToDegrees(A.GetRotation().AngularDistance(B.GetRotation())) <= 0.1;
}
bool Unchanged(const FCombatSnapshot& Was, const AExpeditionBattleView& View) { return FCombatSnapshot::StaticStruct()->CompareScriptStruct(&Was, &View.Rules().Snapshot(), 0); }

/** Today's cut, the reference every fly-in must converge to: the same route command with the fly-in switched off. */
bool Reference(FAutomationTestBase& Test, const FExpeditionSaveData& Data, int32 Offer, FOutcome& Out)
{
    TUniquePtr<FHarness> Game = Open(Test, Data);
    if (!Game) { return false; }
    Game->View->SetRouteFlyIn(false);
    Game->Press(RouteKey(Offer));
    if (!Game->View->InCombat() || Game->View->FlyingIn() || Game->View->ShowingOverview()) { return false; }
    Out = Rest(*Game);
    return true;
}

/** Plays the fly-in out frame by frame; true when it ended with the board up. */
bool PlayOut(FHarness& Game)
{
    for (int32 Index = 0; Index < 400 && Game.View->FlyingIn(); ++Index) { Game.Frame(); }
    return !Game.View->FlyingIn() && !Game.View->ShowingOverview();
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRouteFlyInTimelineTest, "Project.Expedition.Presentation.RouteFlyIn.TimelineAndPath",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRouteFlyInTimelineTest::RunTest(const FString& Parameters)
{
    using namespace RouteFlyIn;
    TestTrue(TEXT("The fly-in lasts about 1.5 to 2.5 s"), DurationSeconds >= 1.5f && DurationSeconds <= 2.5f);
    TestEqual(TEXT("No fade at the start"), Fade(0.0f), 0.0f);
    TestEqual(TEXT("No fade before the dip"), Fade(CutSeconds - FadeOutSeconds - 0.01f), 0.0f);
    TestEqual(TEXT("Dark at the cut"), Fade(CutSeconds), 1.0f);
    TestEqual(TEXT("No fade at the end"), Fade(DurationSeconds), 0.0f);
    bool bDips = true, bRises = true, bEyeForward = true;
    for (float T = CutSeconds - FadeOutSeconds; T + 0.01f < CutSeconds; T += 0.01f) { bDips &= Fade(T + 0.01f) >= Fade(T); }
    for (float T = CutSeconds; T + 0.01f < DurationSeconds; T += 0.01f) { bRises &= Fade(T + 0.01f) <= Fade(T); }
    for (float T = 0.0f; T + 0.01f <= CutSeconds; T += 0.01f) { bEyeForward &= EyeAlpha(T + 0.01f) >= EyeAlpha(T); }
    TestTrue(TEXT("The dip only darkens"), bDips);
    TestTrue(TEXT("The fade-up only lightens"), bRises);
    TestTrue(TEXT("The eye only moves forward"), bEyeForward);
    TestEqual(TEXT("The eye starts at the journey camera"), EyeAlpha(0.0f), 0.0);
    TestEqual(TEXT("The eye reaches the end at the cut"), EyeAlpha(CutSeconds), 1.0);
    TestTrue(TEXT("The eye is still moving into the cut (it plunges, not parks)"), EyeAlpha(CutSeconds) - EyeAlpha(CutSeconds - 0.05f) > 0.01);

    // The journey camera over a diorama-sized box on the left plinth.
    const FTransform Camera(JourneyLayout::CameraLook().Rotation(), JourneyLayout::CameraEye());
    const FBox Diorama(FVector(10.0, -125.0, 15.0), FVector(70.0, -75.0, 60.0));
    const FPath Path = PathFor(Camera, Diorama, JourneyLayout::CameraFieldOfView);
    const FTransform Start = At(Path, 0.0f), End = At(Path, CutSeconds);
    TestTrue(TEXT("The move starts at the journey camera's eye"), Start.GetLocation().Equals(Camera.GetLocation(), 0.01));
    TestTrue(TEXT("The move starts looking where the journey camera looks"), FMath::RadiansToDegrees(Start.GetRotation().AngularDistance(Camera.GetRotation())) < 0.01);
    TestTrue(TEXT("The move ends at its end eye"), End.GetLocation().Equals(Path.EndEye, 0.01));
    const FVector Look = End.GetRotation().GetForwardVector();
    TestTrue(TEXT("It ends looking at its aim point"), FVector::DotProduct(Look, (Path.EndAim - End.GetLocation()).GetSafeNormal()) > 0.9999);
    TestTrue(TEXT("which is low in the diorama, over its middle"), Diorama.IsInside(Path.EndAim) && FMath::IsNearlyEqual(Path.EndAim.X, Diorama.GetCenter().X, 0.01) &&
        Path.EndAim.Z < Diorama.GetCenter().Z);
    TestTrue(TEXT("It ends inside the diorama's footprint on screen, much nearer than it started"),
        FVector::Dist(End.GetLocation(), Diorama.GetCenter()) < 0.25 * FVector::Dist(Start.GetLocation(), Diorama.GetCenter()));
    TestTrue(TEXT("It ends on the side the camera looks from (the battle camera's home side, -Y)"), End.GetLocation().Y < Path.EndAim.Y);
    TestTrue(TEXT("It ends about the battle camera's pitch"), FMath::IsNearlyEqual(-End.Rotator().Pitch, EndPitchDegrees, 1.0));
    // The burst showed an end below the diorama's top flying through its wooden base: the end eye clears the top, so the straight path never enters the diorama.
    TestTrue(TEXT("The end eye is above the diorama's top"), Path.EndEye.Z >= Diorama.Max.Z + EndClearanceCm - 0.01);
    bool bOutside = true;
    for (float T = 0.0f; T <= CutSeconds; T += 0.01f) { bOutside &= !Diorama.IsInside(At(Path, T).GetLocation()); }
    TestTrue(TEXT("The camera never enters the diorama on the way"), bOutside);
    for (const FBox& Tall : {FBox(FVector(10.0, -125.0, 15.0), FVector(70.0, -75.0, 140.0)), FBox(FVector(-70.0, -130.0, 15.0), FVector(-10.0, -70.0, 30.0))})
    {
        const FPath Other = PathFor(Camera, Tall, JourneyLayout::CameraFieldOfView);
        bool bClear = Other.EndEye.Z >= Tall.Max.Z + EndClearanceCm - 0.01;
        for (float T = 0.0f; T <= CutSeconds; T += 0.01f) { bClear &= !Tall.IsInside(At(Other, T).GetLocation()); }
        TestTrue(TEXT("A tall or a flat diorama is never entered either"), bClear);
    }
    TestTrue(TEXT("The pose holds after the cut"), At(Path, DurationSeconds).GetLocation().Equals(End.GetLocation(), 0.01));

    // The capture mode: a burst of held moments, never a command.
    FCapturePoses Poses;
    FString Error;
    TestTrue(TEXT("flyin shots parse"), ExpeditionCapture::ParsePoses(TEXT(R"({"version":1,"shots":[{"name":"a","mode":"flyin","time":0.5},{"name":"b","mode":"flyin","time":2.05,"offer":1}]})"), Poses, Error));
    TestTrue(TEXT("Both are flyin shots with their time and offer"), Poses.Shots.Num() == 2 && Poses.Shots[0].Mode == ECaptureMode::FlyIn && Poses.Shots[0].Offer == 0 &&
        Poses.Shots[1].Offer == 1 && FMath::IsNearlyEqual(Poses.Shots[1].Time, 2.05f));
    for (const TCHAR* Wrong : {TEXT(R"({"version":1,"shots":[{"name":"a","mode":"flyin"}]})"), TEXT(R"({"version":1,"shots":[{"name":"a","mode":"flyin","time":3}]})"),
        TEXT(R"({"version":1,"shots":[{"name":"a","mode":"flyin","time":1,"offer":2}]})")})
    {
        FCapturePoses Refused;
        TestFalse(FString::Printf(TEXT("Refused: %s"), Wrong), ExpeditionCapture::ParsePoses(Wrong, Refused, Error));
    }
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRouteFlyInStartsTest, "Project.Expedition.Presentation.RouteFlyIn.StartsOnTheRouteCommand",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRouteFlyInStartsTest::RunTest(const FString& Parameters)
{
    using namespace RouteFlyInTest;
    FExpeditionSaveData Data;
    int32 Offer = 0;
    if (!TestTrue(TEXT("A route screen whose offer opens on the party's turn"), RouteScreen(Data, Offer))) { return false; }
    FOutcome Cut;
    if (!TestTrue(TEXT("Today's cut, the reference"), Reference(*this, Data, Offer, Cut))) { return false; }
    {
        // A bare view (the many view-level tests, tools) and a capture run cut straight to the board; only a watched view flies in.
        TUniquePtr<FHarness> Game = Open(*this, Data);
        if (!TestNotNull(TEXT("The route screen"), Game.Get())) { return false; }
        TestTrue(TEXT("The player's view plays the fly-in"), Game->View->PlaysRouteFlyIn());
        AExpeditionBattleView* Bare = Game->World->SpawnActor<AExpeditionBattleView>();
        TestTrue(TEXT("A bare view does not"), Bare && !Bare->PlaysRouteFlyIn());
    }
    enum class EWay : uint8 { Key, Card, Diorama };
    for (const EWay Way : {EWay::Key, EWay::Card, EWay::Diorama})
    {
        const FString Name = Way == EWay::Key ? TEXT("Key") : Way == EWay::Card ? TEXT("Card click") : TEXT("Diorama click");
        TUniquePtr<FHarness> Game = Open(*this, Data);
        if (!TestNotNull(TEXT("The route screen"), Game.Get())) { return false; }
        AExpeditionBattleView* View = Game->View;
        const TArray<uint8> Before = Game->CheckpointBytes();
        FBox Diorama;
        if (!TestTrue(Name + TEXT(": the offer has a raised diorama"), View->RaisedDioramaBounds(Offer, Diorama))) { return false; }
        const FTransform Journey = View->JourneyCameraTransform();
        if (Way == EWay::Key) { Game->Press(RouteKey(Offer)); }
        else
        {
            // A real click (press and release) on the card's or the diorama's middle, through the HUD's own hit test.
            const double S = ExpeditionLayout::ViewScale(Game->Target.Size.X, Game->Target.Size.Y);
            const double W = Game->Target.Size.X / S, H = Game->Target.Size.Y / S;
            ExpeditionLayout::Rect Box = JourneyLayout::CardRect(W, Offer);
            if (Way == EWay::Diorama && !TestTrue(Name + TEXT(": the diorama's box"), View->RaisedScreenBox(Offer, W, H, Box))) { return false; }
            const FVector2D Pixel((Box.Left + Box.Right) * 0.5 * S, (Box.Top + Box.Bottom) * 0.5 * S);
            FName Hit;
            const FName Wanted(*FString::Printf(TEXT("%s%d"), Way == EWay::Card ? TEXT("Route") : TEXT("Diorama"), Offer));
            Game->Player->SimulatePointer(Pixel, FIntPoint(0, 0));
            Game->Frame();
            if (!TestTrue(Name + TEXT(": the pointer is over its control"), Game->Hud->ButtonAt(Pixel, Hit) && Hit == Wanted)) { return false; }
            Down(*Game, EKeys::LeftMouseButton);
            Game->Frame();
            Up(*Game, EKeys::LeftMouseButton);
            Game->Frame();
        }
        // The route is committed before anything moves: the run and the checkpoint are already the cut's.
        if (!TestTrue(Name + TEXT(": the route command committed the fight"), View->InCombat())) { return false; }
        TestTrue(Name + TEXT(": the checkpoint was written by the command"), Game->CheckpointBytes() != Before);
        TestTrue(Name + TEXT(": the committed run is the cut's"), ExpeditionTestSupport::SameRun(Game->Live(), Cut.Run));
        TestTrue(Name + TEXT(": the checkpoint bytes are the cut's"), Game->CheckpointBytes() == Cut.Bytes);
        if (!TestTrue(Name + TEXT(": the move is under way"), View->FlyInStage() == AExpeditionBattleView::ERouteFlyInStage::Moving)) { return false; }
        TestEqual(Name + TEXT(": into the chosen offer's diorama"), View->FlyInOffer(), Offer);
        TestTrue(Name + TEXT(": the journey is still drawn (the board waits for the cut)"), View->ShowingJourney());
        TestTrue(Name + TEXT(": it starts from the journey camera"), View->FlyInPath().StartEye.Equals(Journey.GetLocation(), 0.01));
        TestTrue(Name + TEXT(": the HUD draws no control while it plays"), Game->Hud->MenuButtons().IsEmpty());
        TestTrue(Name + TEXT(": and the view counts as presenting (enemies, the music and the next screen wait)"), View->IsPresenting());
        const FCombatSnapshot Fight = View->Rules().Snapshot();
        double Last = FVector::Dist(View->CameraTransform().GetLocation(), Diorama.GetCenter());
        const double First = Last;
        bool bOnPath = true, bCloser = true, bStill = true;
        int32 Moving = 0;
        while (View->FlyInStage() == AExpeditionBattleView::ERouteFlyInStage::Moving && Moving < 400)
        {
            bOnPath &= View->CameraTransform().GetLocation().Equals(RouteFlyIn::At(View->FlyInPath(), View->FlyInTime()).GetLocation(), 0.01);
            Game->Frame();
            ++Moving;
            if (View->FlyInStage() != AExpeditionBattleView::ERouteFlyInStage::Moving) { break; }
            const double Now = FVector::Dist(View->CameraTransform().GetLocation(), Diorama.GetCenter());
            bCloser &= Now <= Last + 0.01;
            Last = Now;
            bStill &= Unchanged(Fight, *View);
        }
        TestTrue(Name + TEXT(": the camera is the pure path every frame"), bOnPath);
        TestTrue(Name + TEXT(": it closes on the diorama"), bCloser && Last < 0.3 * First);
        TestTrue(Name + TEXT(": the fight waits (no enemy, no rule moves) during the move"), bStill);
        if (!TestTrue(Name + TEXT(": the cut hands over to the board"), View->FlyInStage() == AExpeditionBattleView::ERouteFlyInStage::Arriving)) { return false; }
        TestFalse(Name + TEXT(": the board is up at the cut"), View->ShowingOverview());
        TestTrue(Name + TEXT(": under the dark"), View->FlyInFade() > 0.8f);
        TestTrue(Name + TEXT(": the move took about the cut's time at 30 fps steps"), FMath::Abs(Moving * RouteFlyIn::MaxStepSeconds - RouteFlyIn::CutSeconds) <= 3.0f * RouteFlyIn::MaxStepSeconds);
        if (!TestTrue(Name + TEXT(": the fade-up ends"), PlayOut(*Game))) { return false; }
        TestEqual(Name + TEXT(": the fade is gone"), View->FlyInFade(), 0.0f);
        TestTrue(Name + TEXT(": the encounter's banner shows once the world is up"), !View->Playback().Banner().Title.IsEmpty());
        const FOutcome Flown = Rest(*Game);
        TestTrue(Name + TEXT(": the run is the cut's"), ExpeditionTestSupport::SameRun(Flown.Run, Cut.Run));
        TestTrue(Name + TEXT(": the checkpoint bytes are unchanged by the fly-in"), Flown.Bytes == Cut.Bytes);
        TestTrue(Name + TEXT(": it hands over to the fight's opening framing"), SameCamera(Flown.Camera, Cut.Camera));
    }
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRouteFlyInSkipTest, "Project.Expedition.Presentation.RouteFlyIn.AnyKeyOrClickSkipsAndActsOnNothing",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRouteFlyInSkipTest::RunTest(const FString& Parameters)
{
    using namespace RouteFlyInTest;
    FExpeditionSaveData Data;
    int32 Offer = 0;
    if (!TestTrue(TEXT("A route screen whose offer opens on the party's turn"), RouteScreen(Data, Offer))) { return false; }
    FOutcome Cut;
    if (!TestTrue(TEXT("Today's cut, the reference"), Reference(*this, Data, Offer, Cut))) { return false; }
    // Keys that would act in the fight: End Turn, the role special's mode, Tab (Skip / focus), the Menu, Help, Records, Resume, Return to menu.
    for (const FKey& Key : {EKeys::SpaceBar, EKeys::F, EKeys::Tab, EKeys::I, EKeys::Slash, EKeys::H, EKeys::O, EKeys::BackSpace})
    {
        TUniquePtr<FHarness> Game = Open(*this, Data);
        if (!TestNotNull(TEXT("The route screen"), Game.Get())) { return false; }
        Game->Press(RouteKey(Offer));
        Frames(*Game, 5);
        const FString Name = Key.ToString();
        if (!TestTrue(Name + TEXT(": the move is under way"), Game->View->FlyInStage() == AExpeditionBattleView::ERouteFlyInStage::Moving)) { return false; }
        const FCombatSnapshot Fight = Game->View->Rules().Snapshot();
        Down(*Game, Key);
        TestFalse(Name + TEXT(": the press ends the fly-in at once"), Game->View->FlyingIn());
        TestFalse(Name + TEXT(": with the board up"), Game->View->ShowingOverview());
        Game->Frame();
        Up(*Game, Key);
        Frames(*Game, 3);
        TestTrue(Name + TEXT(": the key acted on nothing"), Unchanged(Fight, *Game->View) && Game->View->InCombat());
        TestFalse(Name + TEXT(": no overlay or Menu opened"), Game->Player->OverlayOpen() || Game->Player->ShowingMenu() || Game->Player->ConfirmingReturn());
        TestEqual(Name + TEXT(": the mode stays the default"), Game->Player->Action(), ECombatCommandType::Attack);
        TestTrue(Name + TEXT(": the checkpoint bytes are the cut's"), Game->CheckpointBytes() == Cut.Bytes);
        if (Key == EKeys::SpaceBar)
        {
            // The control: the same key, pressed again now, ends the hero's turn.
            Game->Press(EKeys::SpaceBar);
            TestFalse(TEXT("Space pressed after the skip ends the turn (it was live)"), Unchanged(Fight, *Game->View));
        }
    }
    {
        // A skip click over a live board cell: its release must not move the hero there (#221, #236), though that cell is previewed when the button comes up.
        TUniquePtr<FHarness> Game = Open(*this, Data);
        if (!TestNotNull(TEXT("The route screen"), Game.Get())) { return false; }
        Game->Press(RouteKey(Offer));
        Frames(*Game, 5);
        const int32 Hero = Game->View->Rules().ActiveId();
        const TArray<FIntPoint> Moves = Game->View->Rules().LegalMoves(Hero);
        if (!TestTrue(TEXT("The hero has a move"), !Moves.IsEmpty())) { return false; }
        const FIntPoint Cell = Moves[0];
        Game->Player->SimulatePointer(FVector2D(640, 300), Cell);
        Game->Frame();
        TestTrue(TEXT("The move is under way under the pointer"), Game->View->FlyInStage() == AExpeditionBattleView::ERouteFlyInStage::Moving);
        const FCombatSnapshot Fight = Game->View->Rules().Snapshot();
        Down(*Game, EKeys::LeftMouseButton);
        TestFalse(TEXT("The click's press ends the fly-in"), Game->View->FlyingIn());
        Frames(*Game, 10);
        FIntPoint Under;
        TestTrue(TEXT("While the button is held, the board previews the cell under the pointer"), Game->Player->PreviewedPointerCell(Under) && Under == Cell);
        Up(*Game, EKeys::LeftMouseButton);
        Frames(*Game, 3);
        TestTrue(TEXT("The skip click's release moved nobody"), Unchanged(Fight, *Game->View));
        TestTrue(TEXT("The checkpoint bytes are the cut's"), Game->CheckpointBytes() == Cut.Bytes);
        // The control: once the camera has come to rest, a fresh click at the same spot moves the hero.
        Frames(*Game, 150);
        Down(*Game, EKeys::LeftMouseButton);
        Game->Frame();
        Up(*Game, EKeys::LeftMouseButton);
        Frames(*Game, 3);
        TestFalse(TEXT("A new click there acts (the spot was live)"), Unchanged(Fight, *Game->View));
    }
    {
        // The wheel waits: it neither skips nor zooms the fight it reveals.
        TUniquePtr<FHarness> Game = Open(*this, Data);
        if (!TestNotNull(TEXT("The route screen"), Game.Get())) { return false; }
        Game->Press(RouteKey(Offer));
        Game->Press(EKeys::MouseScrollUp);
        TestTrue(TEXT("The wheel does not skip"), Game->View->FlyInStage() == AExpeditionBattleView::ERouteFlyInStage::Moving);
        if (TestTrue(TEXT("The fly-in plays out"), PlayOut(*Game)))
        {
            TestTrue(TEXT("and the wheel left the opening framing alone"), SameCamera(Rest(*Game).Camera, Cut.Camera));
        }
    }
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRouteFlyInConvergesTest, "Project.Expedition.Presentation.RouteFlyIn.ConvergesUnderInstantFinishAndResume",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRouteFlyInConvergesTest::RunTest(const FString& Parameters)
{
    using namespace RouteFlyInTest;
    FExpeditionSaveData Data;
    int32 Offer = 0;
    if (!TestTrue(TEXT("A route screen whose offer opens on the party's turn"), RouteScreen(Data, Offer))) { return false; }
    FOutcome Cut;
    if (!TestTrue(TEXT("Today's cut, the reference"), Reference(*this, Data, Offer, Cut))) { return false; }
    auto Converged = [&](FHarness& Game, const FString& Name, bool bCamera = true)
    {
        TestFalse(Name + TEXT(": no fly-in left"), Game.View->FlyingIn());
        TestFalse(Name + TEXT(": the board is up"), Game.View->ShowingOverview());
        TestEqual(Name + TEXT(": no fade left"), Game.View->FlyInFade(), 0.0f);
        const FOutcome Now = Rest(Game);
        TestFalse(Name + TEXT(": it never comes back"), Game.View->FlyingIn());
        TestTrue(Name + TEXT(": the run is the cut's"), ExpeditionTestSupport::SameRun(Now.Run, Cut.Run));
        TestTrue(Name + TEXT(": the checkpoint bytes are the cut's"), Now.Bytes == Cut.Bytes);
        if (bCamera) { TestTrue(Name + TEXT(": the camera is the opening framing"), SameCamera(Now.Camera, Cut.Camera)); }
    };
    {
        TUniquePtr<FHarness> Game = Open(*this, Data);
        if (!TestNotNull(TEXT("The route screen"), Game.Get())) { return false; }
        Game->View->CycleSpeed(); Game->View->CycleSpeed();
        TestEqual(TEXT("Instant speed"), Game->View->Speed(), EPlaybackSpeed::Instant);
        Game->Press(RouteKey(Offer));
        TestFalse(TEXT("Instant: no fly-in starts"), Game->View->FlyingIn());
        Converged(*Game, TEXT("Instant"));
    }
    {
        TUniquePtr<FHarness> Game = Open(*this, Data);
        if (!TestNotNull(TEXT("The route screen"), Game.Get())) { return false; }
        Game->Press(RouteKey(Offer));
        Frames(*Game, 10);
        TestTrue(TEXT("Finish: the move is under way"), Game->View->FlyInStage() == AExpeditionBattleView::ERouteFlyInStage::Moving);
        Game->View->SkipPresentation();
        Converged(*Game, TEXT("Finish"));
    }
    {
        TUniquePtr<FHarness> Game = Open(*this, Data);
        if (!TestNotNull(TEXT("The route screen"), Game.Get())) { return false; }
        Game->Press(RouteKey(Offer));
        while (Game->View->FlyInStage() == AExpeditionBattleView::ERouteFlyInStage::Moving) { Game->Frame(); }
        TestTrue(TEXT("Finish while arriving: the fade-up is under way"), Game->View->FlyInStage() == AExpeditionBattleView::ERouteFlyInStage::Arriving);
        Game->View->SkipPresentation();
        Converged(*Game, TEXT("Finish while arriving"));
    }
    {
        TUniquePtr<FHarness> Game = Open(*this, Data);
        if (!TestNotNull(TEXT("The route screen"), Game.Get())) { return false; }
        Game->Press(RouteKey(Offer));
        Frames(*Game, 10);
        Game->View->CycleSpeed();
        TestTrue(TEXT("Fast keeps the move"), Game->View->FlyingIn());
        Game->View->CycleSpeed();
        Converged(*Game, TEXT("Instant mid-move"));
    }
    {
        // A command arriving while the move plays (autoplay, a button pressed by code) finishes the move first, then acts on the board it names.
        TUniquePtr<FHarness> Game = Open(*this, Data);
        if (!TestNotNull(TEXT("The route screen"), Game.Get())) { return false; }
        Game->Press(RouteKey(Offer));
        Frames(*Game, 10);
        const int32 Hero = Game->View->Rules().ActiveId();
        TestTrue(TEXT("A command mid-move is accepted"), Game->View->Execute({ECombatCommandType::EndTurn, Hero}));
        TestFalse(TEXT("after the move finished"), Game->View->FlyingIn() || Game->View->ShowingOverview());
    }
    {
        TUniquePtr<FHarness> Game = Open(*this, Data);
        if (!TestNotNull(TEXT("The route screen"), Game.Get())) { return false; }
        Game->Press(RouteKey(Offer));
        Frames(*Game, 10);
        Game->View->Resume();
        TestTrue(TEXT("Resume mid-move restores the committed fight"), Game->View->InCombat());
        Converged(*Game, TEXT("Resume mid-move"));
    }
    {
        // A resumed fight never replays the fly-in: played out, put away, resumed.
        TUniquePtr<FHarness> Game = Open(*this, Data);
        if (!TestNotNull(TEXT("The route screen"), Game.Get())) { return false; }
        Game->Press(RouteKey(Offer));
        TestTrue(TEXT("The fly-in plays out"), PlayOut(*Game));
        Game->View->ReturnToMenu();
        Game->Frame();
        Game->Press(EKeys::O);
        TestTrue(TEXT("Resume enters the fight"), Game->View->InCombat());
        Converged(*Game, TEXT("Resumed fight"));
    }
    {
        // The primitive look (and any journey that fell back as a whole, #248) has no diorama to fly into: today's cut.
        TUniquePtr<FHarness> Game = Open(*this, Data);
        if (!TestNotNull(TEXT("The route screen"), Game.Get())) { return false; }
        Game->View->UsePrimitives();
        Game->Frame();
        TestFalse(TEXT("Primitives: no journey"), Game->View->ShowingJourney());
        Game->Press(RouteKey(Offer));
        TestFalse(TEXT("Primitives: no fly-in"), Game->View->FlyingIn());
        TestTrue(TEXT("Primitives: the run is the cut's"), ExpeditionTestSupport::SameRun(Game->Live(), Cut.Run) && Game->CheckpointBytes() == Cut.Bytes);
    }
    return !HasAnyErrors();
}
#endif
