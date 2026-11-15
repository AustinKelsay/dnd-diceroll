#if WITH_DEV_AUTOMATION_TESTS
#include "ExpeditionInputHarness.h"

/**
 * #209: the framing limits hold for keyboard panning, through the real controller. Shift with W, A, S and D pans the focus every frame; the on-screen
 * rig could not press Shift, so this drives the controller's own input path (key events into UPlayerInput, the controller's tick) at 30 Hz and
 * samples the rendered camera each frame: on the Vault, the viewport's ground never leaves the authored set, at the far cap and at the default zoom; on
 * the Vigil, the view never passes the pan bound and every point of the rose in the bottom third of the frame is faded, zoomed in and out.
 */
namespace CameraFramingInputTest
{
using ExpeditionInputHarness::FHarness;
const float Dt = 1.0f / 30.0f;

void Down(FHarness& Game, FKey Key) { Game.Player->InputKey(FInputKeyEventArgs::CreateSimulated(Key, IE_Pressed, 1.0f)); }
void Up(FHarness& Game, FKey Key) { Game.Player->InputKey(FInputKeyEventArgs::CreateSimulated(Key, IE_Released, 0.0f)); }
void Tick(FHarness& Game, int32 Frames) { for (int32 Index = 0; Index < Frames; ++Index) { Game.Frame(Dt); } }

/** A fight the party starts: the Vault finale, or the first dangerous road (the Drowned Vigil). */
bool Checkpoint(FExpeditionSaveData& Data, bool bVault)
{
    FString Error;
    FExpeditionRun Run;
    if (bVault)
    {
        const int32 Seed = ExpeditionTestSupport::FirstSeedWhere([&Run](int32 Candidate) { Run = FExpeditionRun(); return ExpeditionTestSupport::ReachFinale(Run, Candidate, -1, true); }, 1, 400);
        if (Seed == INDEX_NONE) { return false; }
    }
    else
    {
        if (!Run.NewRun(20261030, Error, TEXT("framing-vigil"))) { return false; }
        const FExpeditionRoute* Dangerous = Run.Progress().Offers.FindByPredicate([](const FExpeditionRoute& Offer) { return Offer.bDangerous; });
        if (!Dangerous || !Run.ChooseRoute(Dangerous->Id, Error)) { return false; }
    }
    Data.Current = Run.Snapshot();
    return true;
}

bool Resume(FHarness& Game)
{
    Game.Frame();
    Game.Press(EKeys::O);
    if (!Game.View->InCombat()) { return false; }
    Game.PartyTurn();
    Tick(Game, 90);
    return Game.View->InCombat();
}

/** The ground under the four corners of the viewport (or the safe rect) through the rendered camera, in the board's view space. */
bool Ground(const AExpeditionBattleView& View, bool bWholeViewport, FVector2D (&Out)[4])
{
    using namespace ExpeditionCameraRig;
    const FSafeRect Rect = View.SafeRect();
    const FTransform Camera = View.CameraTransform();
    const FTransform Actor = View.GetActorTransform();
    const double PlaneZ = Actor.GetLocation().Z + View.Metric().BoardTop();
    const FVector2D Corners[4] = {
        bWholeViewport ? FVector2D(0, 0) : FVector2D(Rect.Left, Rect.Top), bWholeViewport ? FVector2D(1, 0) : FVector2D(1.0 - Rect.Right, Rect.Top),
        bWholeViewport ? FVector2D(0, 1) : FVector2D(Rect.Left, 1.0 - Rect.Bottom), bWholeViewport ? FVector2D(1, 1) : FVector2D(1.0 - Rect.Right, 1.0 - Rect.Bottom)};
    for (int32 Index = 0; Index < 4; ++Index)
    {
        FVector2D Hit;
        if (!Unproject(Corners[Index], Camera.GetLocation(), Camera.GetRotation().GetForwardVector(), Rect.Aspect, static_cast<float>(PlaneZ), Hit)) { return false; }
        const FVector InView = Actor.InverseTransformPosition(FVector(Hit.X, Hit.Y, PlaneZ));
        Out[Index] = FVector2D(InView.X, InView.Y);
    }
    return true;
}

/** Holds Shift and one of W, A, S, D for Frames frames, calling Each after every frame; returns whether the camera moved at all. */
bool HoldPan(FHarness& Game, FKey Key, int32 Frames, TFunctionRef<void()> Each)
{
    const FVector Before = Game.View->CameraTransform().GetLocation();
    Down(Game, EKeys::LeftShift);
    Down(Game, Key);
    for (int32 Index = 0; Index < Frames; ++Index) { Game.Frame(Dt); Each(); }
    Up(Game, Key);
    Up(Game, EKeys::LeftShift);
    Tick(Game, 20);
    Each();
    return FVector::Dist(Before, Game.View->CameraTransform().GetLocation()) > 1.0;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVaultKeyboardPanStaysInTheSetTest, "Project.Expedition.Presentation.VaultKeyboardPanStaysInsideTheSet",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FVaultKeyboardPanStaysInTheSetTest::RunTest(const FString& Parameters)
{
    using namespace CameraFramingInputTest;
    FExpeditionSaveData Data;
    if (!TestTrue(TEXT("A finale checkpoint exists"), Checkpoint(Data, true))) { return false; }
    FHarness Game(*this, TEXT("VaultKeyboardPan"), &Data);
    if (!TestTrue(TEXT("Input, board and HUD are ready"), Game.IsReady())) { return false; }
    if (!TestTrue(TEXT("The party holds the turn"), Resume(Game))) { return false; }
    TestTrue(TEXT("It is the Vault"), Game.View->UsesVaultKit() && Game.View->BoundsSet());
    const float Half = ExpeditionCameraRig::SetHalfExtentCells * Game.View->Metric().CellCm;
    const FCombatEncounter& Board = Game.View->Rules().Snapshot().Encounter;
    const FVector2D Centre = ExpeditionCameraRig::BoardCentre(Board.Width, Board.Height, Game.View->Metric());
    int32 Moved = 0, Samples = 0;
    for (const bool bFar : {true, false})
    {
        for (const float Yaw : {-90.0f, -45.0f, 0.0f, 45.0f})
        {
            // The wheel, through the controller, to the stop (far) or back to the default; Q/E to the yaw.
            Game.View->SnapCamera(Yaw, Game.View->FramedDistance(Yaw));
            Tick(Game, 10);
            if (bFar) { for (int32 Notch = 0; Notch < 80; ++Notch) { Game.Press(EKeys::MouseScrollDown); } Tick(Game, 90); }
            const FString Label = FString::Printf(TEXT("%s yaw %.0f"), bFar ? TEXT("Far cap") : TEXT("Default zoom"), Yaw);
            if (bFar) { TestEqual(*(Label + TEXT(": the wheel stopped at the cap")), Game.View->CameraDistance(), Game.View->MaxZoomFor(Yaw), 0.5f); }
            for (const FKey& Key : {EKeys::W, EKeys::S, EKeys::A, EKeys::D})
            {
                const bool bMoved = HoldPan(Game, Key, 60, [&]()
                {
                    FVector2D P[4];
                    ++Samples;
                    if (Game.View->ShowsPastSet() || !Ground(*Game.View, true, P)) { return; }
                    for (const FVector2D& Corner : P)
                    {
                        if (FMath::Abs(Corner.X - Centre.X) > Half + 5.0 || FMath::Abs(Corner.Y - Centre.Y) > Half + 5.0)
                        { AddError(Label + FString::Printf(TEXT(", Shift+%s: a viewport corner lands %.0f, %.0f cm from the board centre (set %.0f)"), *Key.ToString(), Corner.X - Centre.X, Corner.Y - Centre.Y, Half)); break; }
                    }
                });
                Moved += bMoved ? 1 : 0;
            }
        }
    }
    AddInfo(FString::Printf(TEXT("%d frames sampled under Shift+W/A/S/D; %d of the holds moved the camera"), Samples, Moved));
    TestTrue(TEXT("Keyboard panning reached the camera (Shift+W/A/S/D moved it at least once)"), Moved > 0);
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVigilKeyboardPanTest, "Project.Expedition.Presentation.VigilKeyboardPanKeepsTheBoundAndTheRoseFaded",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FVigilKeyboardPanTest::RunTest(const FString& Parameters)
{
    using namespace CameraFramingInputTest;
    using namespace ExpeditionCameraRig;
    FExpeditionSaveData Data;
    if (!TestTrue(TEXT("A Vigil checkpoint exists"), Checkpoint(Data, false))) { return false; }
    FHarness Game(*this, TEXT("VigilKeyboardPan"), &Data);
    if (!TestTrue(TEXT("Input, board and HUD are ready"), Game.IsReady())) { return false; }
    if (!TestTrue(TEXT("The party holds the turn"), Resume(Game))) { return false; }
    if (!TestTrue(TEXT("It is the Vigil with its occluder"), Game.View->UsesVigilKit() && Game.View->HasOccluder())) { return false; }
    const FCombatEncounter& Board = Game.View->Rules().Snapshot().Encounter;
    const double Cell = Game.View->Metric().CellCm;
    const double Lower = -(0.5 + PanMarginCells) * Cell, UpperX = (Board.Width - 1 + 0.5 + PanMarginCells) * Cell, UpperY = (Board.Height - 1 + 0.5 + PanMarginCells) * Cell;
    const FVector Centre = Game.View->GetActorTransform().TransformPosition(Game.View->BoardPosition(FVector2D((Board.Width - 1) * 0.5f, (Board.Height - 1) * 0.5f)));
    FVector2D MinXY, MaxXY;
    float PlaneZ = 0.0f;
    Game.View->OccluderBoard(MinXY, MaxXY, PlaneZ);
    TArray<FVector> Rose;
    for (const double X : {-390.0, 0.0, 390.0}) for (const double Up : {580.0, 1100.0, 1370.0}) { Rose.Add(FVector(Centre.X + X, Centre.Y - 2175.0, PlaneZ + Up)); }
    int32 Moved = 0, Samples = 0, RoseChecked = 0;
    // Zoomed in (the wheel up, through the controller), at the default, and zoomed out; each panned with Shift+W/A/S/D to every stop.
    for (const int32 Notches : {-14, 0, 12})
    {
        for (const float Yaw : {-90.0f, -45.0f, 0.0f})
        {
            Game.View->SnapCamera(Yaw, Game.View->FramedDistance(Yaw));
            Tick(Game, 10);
            for (int32 Notch = 0; Notch < FMath::Abs(Notches); ++Notch) { Game.Press(Notches < 0 ? EKeys::MouseScrollUp : EKeys::MouseScrollDown); }
            Tick(Game, 60);
            const FString Label = FString::Printf(TEXT("%d wheel notches %s from the default, yaw %.0f"), FMath::Abs(Notches), Notches < 0 ? TEXT("in") : TEXT("out"), Yaw);
            for (const FKey& Key : {EKeys::S, EKeys::W, EKeys::A, EKeys::D})
            {
                const bool bMoved = HoldPan(Game, Key, 90, [&]()
                {
                    ++Samples;
                    if (!Game.View->WholeBoardInView())
                    {
                        FVector2D P[4];
                        if (Ground(*Game.View, false, P))
                        {
                            double MinX = P[0].X, MaxX = P[0].X, MinY = P[0].Y, MaxY = P[0].Y;
                            for (const FVector2D& Corner : P) { MinX = FMath::Min(MinX, Corner.X); MaxX = FMath::Max(MaxX, Corner.X); MinY = FMath::Min(MinY, Corner.Y); MaxY = FMath::Max(MaxY, Corner.Y); }
                            // An axis wider than the bound is locked and centred; the others stop at the margin.
                            if (MaxX - MinX <= UpperX - Lower && (MinX < Lower - 2.0 || MaxX > UpperX + 2.0)) { AddError(Label + FString::Printf(TEXT(", Shift+%s: the view passed the pan bound in X"), *Key.ToString())); }
                            if (MaxY - MinY <= UpperY - Lower && (MinY < Lower - 2.0 || MaxY > UpperY + 2.0)) { AddError(Label + FString::Printf(TEXT(", Shift+%s: the view passed the pan bound in Y"), *Key.ToString())); }
                        }
                    }
                    const FTransform Camera = Game.View->CameraTransform();
                    for (const FVector& Point : Rose)
                    {
                        FVector2D Screen;
                        if (!Project(Point, Camera.GetLocation(), Camera.GetRotation().GetForwardVector(), Game.View->SafeRect().Aspect, Screen)) { continue; }
                        if (Screen.X < 0.2 || Screen.X > 0.8 || Screen.Y < 0.66 || Screen.Y > 1.0) { continue; }
                        ++RoseChecked;
                        if (Game.View->OccluderBoardOpacityAt(Point) > Game.View->OccluderMinOpacity() + 0.001f)
                        { AddError(Label + FString::Printf(TEXT(", Shift+%s: a point of the rose at screen (%.2f, %.2f) in the bottom third is not faded"), *Key.ToString(), Screen.X, Screen.Y)); }
                    }
                });
                Moved += bMoved ? 1 : 0;
            }
        }
    }
    AddInfo(FString::Printf(TEXT("%d frames sampled under Shift+W/A/S/D; %d of the holds moved the camera; %d rose points in the bottom third checked"), Samples, Moved, RoseChecked));
    TestTrue(TEXT("Keyboard panning reached the camera (Shift+W/A/S/D moved it at least once)"), Moved > 0);
    return !HasAnyErrors();
}
#endif
