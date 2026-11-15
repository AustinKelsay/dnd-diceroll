#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Components/LightComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Persistence/ExpeditionSave.h"
#include "Presentation/BoardMetric.h"
#include "Presentation/CameraRig.h"
#include "Presentation/ExpeditionBattleView.h"
#include "Materials/MaterialInterface.h"

namespace CameraLightingTestSupport
{
struct FViewportSize { int32 X; int32 Y; };
const FViewportSize Viewports[] = {{1920, 1080}, {1280, 720}, {1024, 576}, {2560, 1080}};

/** The board's four outer tile corners at the board top, in the view's space. */
void BoardCorners(int32 Width, int32 Height, const FBoardMetric& Metric, FVector (&Corners)[4])
{
    const double C = Metric.CellCm, Z = Metric.BoardTop();
    Corners[0] = FVector(-0.5 * C, -0.5 * C, Z);
    Corners[1] = FVector((Width - 0.5) * C, -0.5 * C, Z);
    Corners[2] = FVector(-0.5 * C, (Height - 0.5) * C, Z);
    Corners[3] = FVector((Width - 0.5) * C, (Height - 0.5) * C, Z);
}

/** Whether every corner projects inside the safe rect; Where reports the first one that does not. */
bool CornersInside(const FVector (&Corners)[4], const FVector& Eye, const FVector& Forward, const ExpeditionCameraRig::FSafeRect& Safe, FString& Where)
{
    for (const FVector& Corner : Corners)
    {
        FVector2D Screen;
        if (!ExpeditionCameraRig::Project(Corner, Eye, Forward, Safe.Aspect, Screen)) { Where = TEXT("behind the camera"); return false; }
        if (!Safe.Holds(Screen, 1.0f))
        {
            Where = FString::Printf(TEXT("(%.3f, %.3f) outside x %.3f..%.3f, y %.3f..%.3f"), Screen.X, Screen.Y, Safe.Left, 1.0f - Safe.Right, Safe.Top, 1.0f - Safe.Bottom);
            return false;
        }
    }
    return true;
}

/** The acting unit's tile corners (at the board top) and head, in world space: what the default view must keep inside the safe rect. */
void UnitPoints(const AExpeditionBattleView& View, int32 Id, FVector (&Points)[5])
{
    const FBoardMetric& Metric = View.Metric();
    const FVector At = View.UnitPosition(Id, 0.0f);
    const double Half = 0.5 * Metric.CellCm, Z = Metric.BoardTop();
    const FTransform Actor = View.GetActorTransform();
    Points[0] = Actor.TransformPosition(FVector(At.X - Half, At.Y - Half, Z));
    Points[1] = Actor.TransformPosition(FVector(At.X + Half, At.Y - Half, Z));
    Points[2] = Actor.TransformPosition(FVector(At.X - Half, At.Y + Half, Z));
    Points[3] = Actor.TransformPosition(FVector(At.X + Half, At.Y + Half, Z));
    Points[4] = Actor.TransformPosition(FVector(At.X, At.Y, Z + Metric.FigureCm));
}

/** Whether the acting unit's tile and head project inside the safe rect through the real camera; Where reports the first point that does not. */
bool UnitInsideRect(const AExpeditionBattleView& View, const ExpeditionCameraRig::FSafeRect& Safe, FString& Where)
{
    FVector Points[5];
    UnitPoints(View, View.Rules().ActiveId(), Points);
    const FTransform Camera = View.CameraTransform();
    for (const FVector& Point : Points)
    {
        FVector2D Screen;
        if (!ExpeditionCameraRig::Project(Point, Camera.GetLocation(), Camera.GetRotation().GetForwardVector(), Safe.Aspect, Screen)) { Where = TEXT("behind the camera"); return false; }
        if (!Safe.Holds(Screen, 1.0f))
        {
            Where = FString::Printf(TEXT("(%.3f, %.3f) outside x %.3f..%.3f, y %.3f..%.3f"), Screen.X, Screen.Y, Safe.Left, 1.0f - Safe.Right, Safe.Top, 1.0f - Safe.Bottom);
            return false;
        }
    }
    return true;
}

/** How many pixels tall a standing figure at the camera focus is, projected through the real camera at a viewport height. */
float FigurePixelsAtFocus(const AExpeditionBattleView& View, const ExpeditionCameraRig::FSafeRect& Safe, int32 ViewportHeight)
{
    const FBoardMetric& Metric = View.Metric();
    const FVector Focus = View.CameraFocus();
    const FTransform Camera = View.CameraTransform();
    const FTransform Actor = View.GetActorTransform();
    FVector2D Feet, Head;
    if (!ExpeditionCameraRig::Project(Actor.TransformPosition(FVector(Focus.X, Focus.Y, Metric.BoardTop())), Camera.GetLocation(), Camera.GetRotation().GetForwardVector(), Safe.Aspect, Feet) ||
        !ExpeditionCameraRig::Project(Actor.TransformPosition(FVector(Focus.X, Focus.Y, Metric.BoardTop() + Metric.FigureCm)), Camera.GetLocation(), Camera.GetRotation().GetForwardVector(), Safe.Aspect, Head))
    { return 0.0f; }
    return static_cast<float>((Feet.Y - Head.Y) * ViewportHeight);
}

/** Writes a checkpoint at the first encounter whose board is Width x Height (BoardMetricTest and the capture fixtures search the same way). */
bool WriteCameraBoardCheckpoint(const FString& Path, int32 Width, int32 Height)
{
    for (int32 Seed = 1; Seed < 2000; ++Seed)
    {
        FString Error;
        FExpeditionRun Run;
        Run.NewRun(Seed, Error, FString::Printf(TEXT("camera-%d"), Seed));
        for (int32 Offer = 0; Offer < Run.Progress().Offers.Num(); ++Offer)
        {
            const FExpeditionRoute& Route = Run.Progress().Offers[Offer];
            if (Route.Width != Width || Route.Height != Height) { continue; }
            if (!Run.ChooseRoute(Route.Id, Error)) { continue; }
            FExpeditionSaveData Data;
            Data.Current = Run.Snapshot();
            return FExpeditionSave::Write(Path, Data, Error);
        }
    }
    return false;
}
}

namespace
{
UWorld* MakeCameraWorld()
{
    const UWorld::InitializationValues Values = UWorld::InitializationValues()
        .AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false);
    return UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
}

AExpeditionBattleView* StartBattle(UWorld* World, const FString& Directory)
{
    AExpeditionBattleView* View = World->SpawnActor<AExpeditionBattleView>();
    if (!View) { return nullptr; }
    View->ConfigureStorage(FPaths::Combine(Directory, TEXT("current.json")));
    View->DispatchBeginPlay();
    View->NewRun();
    View->ChooseRoute(0);
    View->HoldEnemyTurns(true);
    View->Tick(0.016f);
    return View;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCameraRigMathTest, "Project.Expedition.Presentation.CameraRigMath",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FCameraRigMathTest::RunTest(const FString& Parameters)
{
    using namespace ExpeditionCameraRig;
    TestEqual(TEXT("The battle field of view is 40"), FieldOfView, 40.0f);
    // The active-unit lean never pushes a corner out of the safe rect at rest when the centred board fits it, including
    // distances where the board fits the rect but not the 99% lean inset (the fallback must test the real rect).
    {
        const FBoardMetric Metric(150.0f, 180.0f);
        const FSafeRect Safe = FSafeRect::ForViewport(1920, 1080);
        const FVector2D Centre = BoardCentre(18, 12, Metric);
        const FVector2D Target(17 * 150.0f, 11 * 150.0f);
        for (const float Distance : {3300.0f, 3400.0f, 3507.4f, 3560.0f, 3649.0f, 3800.0f})
        {
            if (!BoardInside(18, 12, Metric, Centre, -90.0f, Distance, 45.0f, Safe, 1.0f)) { continue; }
            const FVector2D Leaned = LimitLean(Centre, Target, 18, 12, Metric, -90.0f, Distance, 45.0f, Safe);
            TestTrue(*FString::Printf(TEXT("Lean at distance %.1f keeps the 18x12 board inside the safe rect"), Distance),
                BoardInside(18, 12, Metric, Leaned, -90.0f, Distance, 45.0f, Safe, 1.0f));
        }
    }
    // Pitch is non-decreasing in zoom and hits 35 / 45 / 60 at near / framed / far.
    for (const float Framed : {1400.0f, 2300.0f, 2900.0f, MaxFramed})
    {
        float Last = -1.0f;
        for (float Distance = MinDistance; Distance <= MaxDistance; Distance += 25.0f)
        {
            const float Pitch = PitchFor(Distance, Framed);
            TestTrue(FString::Printf(TEXT("Pitch does not fall as the camera pulls back (framed %.0f, distance %.0f)"), Framed, Distance), Pitch >= Last - KINDA_SMALL_NUMBER);
            Last = Pitch;
        }
        TestEqual(TEXT("Nearest zoom is 35 degrees"), PitchFor(NearDistance, Framed), 35.0f, 0.01f);
        TestEqual(TEXT("The framed distance is 45 degrees"), PitchFor(Framed, Framed), 45.0f, 0.01f);
        TestEqual(TEXT("The far stop is 60 degrees"), PitchFor(FarStop(Framed), Framed), 60.0f, 0.01f);
        TestTrue(TEXT("The far stop is past the framed distance and inside the zoom range"), FarStop(Framed) > Framed && FarStop(Framed) <= MaxDistance);
        TestEqual(TEXT("The closest allowed zoom stays at the near pitch"), PitchFor(MinDistance, Framed), 35.0f, 0.01f);
    }
    TestEqual(TEXT("The pan margin is 1.5 cells past the board's outer tile edge"), PanMarginCells, 1.5f);
    // Project and Unproject are inverses on the board plane.
    {
        const FBoardMetric Metric(150.0f, 180.0f);
        const FSafeRect Safe = FSafeRect::ForViewport(1920, 1080);
        const FArm Pose = Arm(33.0f, 3100.0f, 44.0f, Metric.Scale(), Safe);
        const FVector Eye = Pose.Offset + Pose.Framing;
        const FVector Forward = -Pose.Offset.GetSafeNormal();
        for (const FVector2D Screen : {FVector2D(0.3, 0.2), FVector2D(0.5, 0.5), FVector2D(0.9, 0.8)})
        {
            FVector2D Ground, Back;
            if (TestTrue(TEXT("A screen point under the horizon hits the board plane"), Unproject(Screen, Eye, Forward, Safe.Aspect, Metric.BoardTop(), Ground)))
            {
                TestTrue(TEXT("Projecting the ground point returns the screen point"),
                    Project(FVector(Ground.X, Ground.Y, Metric.BoardTop()), Eye, Forward, Safe.Aspect, Back) && Back.Equals(Screen, 0.0005));
            }
        }
    }
    // The pan bound (#148 gate 2): the visible ground never reaches more than 1.5 cells past the board's outer tile edge; an
    // axis on which the view is wider than the board plus its margins is locked to the board centre.
    {
        using namespace CameraLightingTestSupport;
        const FBoardMetric Metric(150.0f, 180.0f);
        const double C = Metric.CellCm;
        int32 Checked = 0, Locked = 0, Reached = 0;
        for (const FViewportSize& Size : Viewports)
        {
            const FSafeRect Safe = FSafeRect::ForViewport(Size.X, Size.Y);
            for (const TPair<int32, int32>& Board : {TPair<int32, int32>(16, 11), TPair<int32, int32>(18, 12), TPair<int32, int32>(8, 7)})
            {
                const FFit Home = FitDistance(Board.Key, Board.Value, Metric, HomeYaw, 0.0f, Safe);
                const float Anchor = FMath::Clamp(Home.Distance, MinFramed, MaxFramed);
                const FVector2D Centre = BoardCentre(Board.Key, Board.Value, Metric);
                const double LowerX = -(0.5 + PanMarginCells) * C, UpperX = (Board.Key - 1 + 0.5 + PanMarginCells) * C;
                const double LowerY = -(0.5 + PanMarginCells) * C, UpperY = (Board.Value - 1 + 0.5 + PanMarginCells) * C;
                for (const float Yaw : {-90.0f, 0.0f, 33.0f, 90.0f, 180.0f})
                {
                    for (const float Distance : {1300.0f, 2000.0f, FigureDistance(Metric, Anchor, Safe), FarStop(Anchor)})
                    {
                        const FString Label = FString::Printf(TEXT("%dx%d at %dx%d yaw %.0f distance %.0f"), Board.Key, Board.Value, Size.X, Size.Y, Yaw, Distance);
                        const float Pitch = PitchFor(Distance, Anchor);
                        FVector2D Ground[4];
                        if (!TestTrue(*(Label + TEXT(": the safe rect corners have ground under them")), VisibleGround(Yaw, Distance, Pitch, Metric, Safe, Ground))) { continue; }
                        for (const FVector2D Wanted : {FVector2D(1.0e6, 1.0e6), FVector2D(-1.0e6, -1.0e6), FVector2D(1.0e6, -1.0e6), FVector2D(-1.0e6, 1.0e6)})
                        {
                            const FPanBound Bound = ClampPan(Wanted, Board.Key, Board.Value, Metric, Yaw, Distance, Pitch, Safe);
                            ++Checked;
                            double MinX = Ground[0].X, MaxX = Ground[0].X, MinY = Ground[0].Y, MaxY = Ground[0].Y;
                            for (const FVector2D& Point : Ground) { MinX = FMath::Min(MinX, Point.X); MaxX = FMath::Max(MaxX, Point.X); MinY = FMath::Min(MinY, Point.Y); MaxY = FMath::Max(MaxY, Point.Y); }
                            if (Bound.bLockedX)
                            {
                                ++Locked;
                                TestTrue(*(Label + TEXT(": a view wider than the board plus margin is centred on the board in X")), FMath::Abs(Bound.Focus.X + 0.5 * (MinX + MaxX) - Centre.X) < 0.01);
                                TestTrue(*(Label + TEXT(": a locked axis really is wider than the bound")), MaxX - MinX > UpperX - LowerX - 0.01);
                            }
                            else
                            {
                                TestTrue(*(Label + TEXT(": the visible ground stays within 1.5 cells of the board's X edges")),
                                    Bound.Focus.X + MinX >= LowerX - 0.5 && Bound.Focus.X + MaxX <= UpperX + 0.5);
                                // Pushed as far as the wanted focus asks, the view rests on the bound.
                                const bool bOnBound = FMath::Abs(Bound.Focus.X + MaxX - UpperX) < 0.5 || FMath::Abs(Bound.Focus.X + MinX - LowerX) < 0.5;
                                TestTrue(*(Label + TEXT(": a focus pushed past the bound rests on it (X)")), bOnBound);
                                Reached += bOnBound ? 1 : 0;
                            }
                            if (Bound.bLockedY)
                            {
                                ++Locked;
                                TestTrue(*(Label + TEXT(": a view wider than the board plus margin is centred on the board in Y")), FMath::Abs(Bound.Focus.Y + 0.5 * (MinY + MaxY) - Centre.Y) < 0.01);
                                TestTrue(*(Label + TEXT(": a locked axis really is wider than the bound (Y)")), MaxY - MinY > UpperY - LowerY - 0.01);
                            }
                            else
                            {
                                TestTrue(*(Label + TEXT(": the visible ground stays within 1.5 cells of the board's Y edges")),
                                    Bound.Focus.Y + MinY >= LowerY - 0.5 && Bound.Focus.Y + MaxY <= UpperY + 0.5);
                                const bool bOnBound = FMath::Abs(Bound.Focus.Y + MaxY - UpperY) < 0.5 || FMath::Abs(Bound.Focus.Y + MinY - LowerY) < 0.5;
                                TestTrue(*(Label + TEXT(": a focus pushed past the bound rests on it (Y)")), bOnBound);
                                Reached += bOnBound ? 1 : 0;
                            }
                            // A shake at a pan stop keeps the bound too (its height part is dropped).
                            for (const FVector& Shake : {FVector(14.0, 14.0, 14.0), FVector(-14.0, 14.0, -14.0), FVector(14.0, -14.0, 14.0), FVector(-14.0, -14.0, 0.0)})
                            {
                                const FVector2D Shaken = ShakenFocus(Bound.Focus, Shake * Metric.Scale(), Board.Key, Board.Value, Metric, Yaw, Distance, Pitch, Safe);
                                const bool bInX = Bound.bLockedX ? FMath::Abs(Shaken.X - Bound.Focus.X) < 0.01 : Shaken.X + MinX >= LowerX - 0.5 && Shaken.X + MaxX <= UpperX + 0.5;
                                const bool bInY = Bound.bLockedY ? FMath::Abs(Shaken.Y - Bound.Focus.Y) < 0.01 : Shaken.Y + MinY >= LowerY - 0.5 && Shaken.Y + MaxY <= UpperY + 0.5;
                                TestTrue(*(Label + TEXT(": a shake at the pan stop keeps the bound")), bInX && bInY);
                            }
                        }
                    }
                }
            }
        }
        TestTrue(TEXT("Some poses have room to pan, so the bound is exercised"), Reached > 0);
        TestTrue(TEXT("Some poses see more than the board plus margin, so locking is exercised"), Locked > 0);
        // A focus on the board centre is untouched wherever the axis is free (near zoom has room on a big board).
        const FSafeRect Safe = FSafeRect::ForViewport(1920, 1080);
        const FVector2D Centre = BoardCentre(18, 12, Metric);
        const float Anchor18 = FMath::Clamp(FitDistance(18, 12, Metric, HomeYaw, 0.0f, Safe).Distance, MinFramed, MaxFramed);
        const FPanBound Near = ClampPan(Centre, 18, 12, Metric, -90.0f, 1300.0f, PitchFor(1300.0f, Anchor18), Safe);
        TestTrue(TEXT("Near zoom on 18x12 leaves both axes free"), !Near.bLockedX && !Near.bLockedY);
        TestTrue(TEXT("A focus at the board centre is within the bound"), Near.Focus.Equals(Centre, 0.01));
        // A small board seen from afar is smaller than the view on both axes: nothing to pan to.
        const float Anchor8 = FMath::Clamp(FitDistance(8, 7, Metric, HomeYaw, 0.0f, Safe).Distance, MinFramed, MaxFramed);
        const FPanBound Small = ClampPan(FVector2D(1.0e6, 1.0e6), 8, 7, Metric, -90.0f, FarStop(Anchor8), PitchFor(FarStop(Anchor8), Anchor8), Safe);
        {
            const float FarAway = FarStop(Anchor8);
            FVector2D Ground[4];
            const bool bGround = VisibleGround(-90.0f, FarAway, PitchFor(FarAway, Anchor8), Metric, Safe, Ground);
            double MinX = Ground[0].X, MaxX = Ground[0].X, MinY = Ground[0].Y, MaxY = Ground[0].Y;
            for (const FVector2D& Point : Ground) { MinX = FMath::Min(MinX, Point.X); MaxX = FMath::Max(MaxX, Point.X); MinY = FMath::Min(MinY, Point.Y); MaxY = FMath::Max(MaxY, Point.Y); }
            const FVector2D ViewCentre = Small.Focus + FVector2D(0.5 * (MinX + MaxX), 0.5 * (MinY + MaxY));
            TestTrue(TEXT("A small board far away locks both axes with the view centred on it"), bGround && Small.bLockedX && Small.bLockedY && ViewCentre.Equals(BoardCentre(8, 7, Metric), 0.01));
        }
        // Locking is continuous: sweeping the zoom through the threshold where an axis locks moves the clamped focus smoothly
        // (locking to the board centre instead jumped about 65 cm on 18x12 at yaw -90). Steps of 2 reference units move it a few cm.
        for (const TPair<int32, int32>& Board : {TPair<int32, int32>(18, 11), TPair<int32, int32>(18, 12), TPair<int32, int32>(16, 11)})
        {
            const float Anchor = FMath::Clamp(FitDistance(Board.Key, Board.Value, Metric, HomeYaw, 0.0f, Safe).Distance, MinFramed, MaxFramed);
            for (const float Yaw : {-90.0f, 0.0f, 33.0f})
            {
                for (const FVector2D Wanted : {FVector2D(1.0e6, 1.0e6), FVector2D(-1.0e6, -1.0e6)})
                {
                    FPanBound Last = ClampPan(Wanted, Board.Key, Board.Value, Metric, Yaw, MinDistance, PitchFor(MinDistance, Anchor), Safe);
                    double Worst = 0.0;
                    int32 Locks = 0;
                    for (float Distance = MinDistance + 2.0f; Distance <= MaxDistance; Distance += 2.0f)
                    {
                        const FPanBound Next = ClampPan(Wanted, Board.Key, Board.Value, Metric, Yaw, Distance, PitchFor(Distance, Anchor), Safe);
                        Worst = FMath::Max(Worst, FVector2D::Distance(Next.Focus, Last.Focus));
                        Locks += (Next.bLockedX != Last.bLockedX) + (Next.bLockedY != Last.bLockedY);
                        Last = Next;
                    }
                    const FString Label = FString::Printf(TEXT("%dx%d yaw %.0f pushed (%.0e, %.0e)"), Board.Key, Board.Value, Yaw, Wanted.X, Wanted.Y);
                    TestTrue(*(Label + TEXT(": the sweep crosses a lock threshold")), Locks > 0);
                    TestTrue(*FString::Printf(TEXT("%s: the clamped focus moves at most 20 cm per 2-unit zoom step (worst %.1f)"), *Label, Worst), Worst < 20.0);
                }
            }
        }
    }
    // The default zoom targets figure size: a standing 180 cm figure at the focus is 88 px tall at 1080 viewport pixels (scaled with height),
    // within 10 %, on the boards the game generates; a board that already fits closer keeps its whole-board fit.
    {
        using namespace CameraLightingTestSupport;
        const FBoardMetric Metric(150.0f, 180.0f);
        for (const FViewportSize& Size : {FViewportSize{1920, 1080}, FViewportSize{1280, 720}})
        {
            const FSafeRect Safe = FSafeRect::ForViewport(Size.X, Size.Y);
            const float Target = FigureTargetPixels * Size.Y / FigureTargetViewportHeight;
            for (const TPair<int32, int32>& Board : {TPair<int32, int32>(16, 11), TPair<int32, int32>(17, 12), TPair<int32, int32>(18, 12)})
            {
                const float Anchor = FMath::Clamp(FitDistance(Board.Key, Board.Value, Metric, HomeYaw, 0.0f, Safe).Distance, MinFramed, MaxFramed);
                const float Figure = FigureDistance(Metric, Anchor, Safe);
                for (const float Yaw : {-90.0f, 0.0f, 90.0f, 180.0f})
                {
                    const FString Label = FString::Printf(TEXT("%dx%d at %dx%d yaw %.0f"), Board.Key, Board.Value, Size.X, Size.Y, Yaw);
                    const FFit Whole = FitBoard(Board.Key, Board.Value, Metric, Yaw, Safe);
                    const float Default = DefaultDistance(Whole.Distance, Figure);
                    TestTrue(*(Label + TEXT(": the default is never farther than the whole-board fit")), Default <= Whole.Distance + 0.01f);
                    TestTrue(*(Label + TEXT(": the default is inside the zoom range")), Default >= MinDistance && Default <= MaxDistance);
                    const float Pixels = FigureScreenHeight(Default, PitchFor(Default, Anchor), Metric, Safe) * Size.Y;
                    TestTrue(*FString::Printf(TEXT("%s: the default figure is %.1f px, target %.1f px (+/-10%%)"), *Label, Pixels, Target), FMath::Abs(Pixels - Target) <= 0.1f * Target);
                }
            }
            // Smaller boards that fit at the figure distance keep today's whole-board framing.
            const FFit Small = FitBoard(8, 7, Metric, HomeYaw, Safe);
            const float SmallAnchor = Small.Anchor;
            TestEqual(*FString::Printf(TEXT("An 8x7 board keeps its whole-board fit at %dx%d"), Size.X, Size.Y),
                DefaultDistance(Small.Distance, FigureDistance(Metric, SmallAnchor, Safe)), Small.Distance, 0.01f);
        }
        // The figure target scales with the viewport height and the figure shrinks as the camera pulls back.
        const FSafeRect Safe = FSafeRect::ForViewport(1920, 1080);
        const float Anchor18 = FMath::Clamp(FitDistance(18, 12, Metric, HomeYaw, 0.0f, Safe).Distance, MinFramed, MaxFramed);
        TestTrue(TEXT("A farther camera shows a smaller figure"),
            FigureScreenHeight(2000.0f, PitchFor(2000.0f, Anchor18), Metric, Safe) > FigureScreenHeight(4000.0f, PitchFor(4000.0f, Anchor18), Metric, Safe));
        // The unit hold: a unit at a board corner with the focus at the far corner is pulled into the safe rect, and a visible unit leaves the focus alone.
        const FVector2D Centre = BoardCentre(18, 12, Metric);
        const float Distance = FigureDistance(Metric, Anchor18, Safe);
        const float Pitch = PitchFor(Distance, Anchor18);
        for (const float Yaw : {-90.0f, 0.0f, 45.0f, 180.0f})
        {
            for (const FVector2D Unit : {FVector2D(0.0, 0.0), FVector2D(17 * 150.0, 0.0), FVector2D(0.0, 11 * 150.0), FVector2D(17 * 150.0, 11 * 150.0)})
            {
                const FVector2D Held = KeepUnitInside(Centre + (Centre - Unit) * 0.5, Unit, Metric, Yaw, Distance, Pitch, Safe);
                TestTrue(FString::Printf(TEXT("A held focus shows a corner unit at yaw %.0f"), Yaw), UnitInside(Unit, Metric, Held, Yaw, Distance, Pitch, Safe, UnitFill + 0.001f));
                TestTrue(TEXT("A focus on the unit shows it"), UnitInside(Unit, Metric, Unit, Yaw, Distance, Pitch, Safe, UnitFill));
                TestTrue(TEXT("A focus that already shows the unit is left alone"), KeepUnitInside(Unit, Unit, Metric, Yaw, Distance, Pitch, Safe).Equals(Unit, 0.001));
            }
        }
    }
    // The default zoom contains the board: every corner projects inside the HUD-safe rect, for every yaw and viewport.
    using namespace CameraLightingTestSupport;
    const FBoardMetric Metric(150.0f, 180.0f);
    const TPair<int32, int32> Boards[] = {{16, 11}, {18, 12}};
    float LastWidthFit = 0.0f;
    for (const FViewportSize& Size : Viewports)
    {
        const FSafeRect Safe = FSafeRect::ForViewport(Size.X, Size.Y);
        TestEqual(TEXT("The safe rect carries the viewport aspect"), Safe.Aspect, static_cast<float>(Size.X) / static_cast<float>(Size.Y), 0.0001f);
        // The party cards, the top strip and the action bar come from the shared HUD layout.
        const double Scale = ExpeditionLayout::ViewScale(Size.X, Size.Y);
        TestEqual(TEXT("The safe rect starts right of the party cards"), static_cast<float>(Safe.Left * Size.X), static_cast<float>(ExpeditionLayout::BoardLeft * Scale), 0.5f);
        TestEqual(TEXT("The safe rect ends above the action bar"), static_cast<float>(Safe.Bottom * Size.Y), static_cast<float>(ExpeditionLayout::BoardBottom * Scale), 0.5f);
        // The tallest combat log (DrawLog's footprint) lies outside the board's area, so a framed board never sits under it.
        {
            const double W = Size.X / Scale, H = Size.Y / Scale;
            const ExpeditionLayout::Rect Bounds = ExpeditionLayout::BoardBounds(W, H);
            const ExpeditionLayout::Rect Log = ExpeditionLayout::LogRect(W, H);
            TestFalse(FString::Printf(TEXT("The board area clears the combat log at %dx%d"), Size.X, Size.Y), ExpeditionLayout::Overlaps(Bounds, Log));
            TestTrue(TEXT("The safe rect's bottom edge is at or above the log's top"), (1.0 - Safe.Bottom) * H <= Log.Top + 0.01);
        }
        for (const TPair<int32, int32>& Board : Boards)
        {
            for (const float Yaw : {-90.0f, 0.0f, 90.0f, 180.0f})
            {
                const FString Label = FString::Printf(TEXT("%dx%d at %dx%d yaw %.0f"), Board.Key, Board.Value, Size.X, Size.Y, Yaw);
                const FFit Fit = FitBoard(Board.Key, Board.Value, Metric, Yaw, Safe);
                TestTrue(*(Label + TEXT(": the board fits inside the zoom range")), Fit.bFits);
                TestTrue(*(Label + TEXT(": the fit is inside the clamp")), Fit.Distance >= MinDistance && Fit.Distance <= MaxDistance);
                TestEqual(*(Label + TEXT(": the pitch follows the anchor")), Fit.Pitch, PitchFor(Fit.Distance, Fit.Anchor), 0.01f);
                const FVector2D Centre = BoardCentre(Board.Key, Board.Value, Metric);
                const FArm Pose = Arm(Yaw, Fit.Distance, Fit.Pitch, Metric.Scale(), Safe);
                const FVector Eye = FVector(Centre.X, Centre.Y, 0.0) + Pose.Offset + Pose.Framing;
                FVector Corners[4];
                BoardCorners(Board.Key, Board.Value, Metric, Corners);
                FString Where;
                const bool bInside = CornersInside(Corners, Eye, -Pose.Offset.GetSafeNormal(), Safe, Where);
                TestTrue(*(Label + TEXT(": all four corners are inside the safe rect ") + Where), bInside);
                // The lean toward an acting unit at a far corner stays inside the same rect.
                const FVector2D Target = Centre + FVector2D(0.4 * (Board.Key - 1) * 0.5 * Metric.CellCm, -0.4 * (Board.Value - 1) * 0.5 * Metric.CellCm);
                const FVector2D Leaned = LimitLean(Centre, Target, Board.Key, Board.Value, Metric, Yaw, Fit.Distance, Fit.Pitch, Safe);
                TestTrue(*(Label + TEXT(": a limited lean keeps the board inside the safe rect")), BoardInside(Board.Key, Board.Value, Metric, Leaned, Yaw, Fit.Distance, Fit.Pitch, Safe, 1.0f));
                // The far pose (the end of the pitch ramp, never nearer than this yaw's whole-board fit) still contains the whole board.
                const float FarPose = FMath::Max(FarStop(Fit.Anchor), Fit.Distance);
                TestTrue(*(Label + TEXT(": the far pose fits the whole board in the safe rect")),
                    BoardInside(Board.Key, Board.Value, Metric, Centre, Yaw, FarPose, PitchFor(FarPose, Fit.Anchor), Safe, 1.0f));
                if (Board.Key == 18 && Yaw == -90.0f) { LastWidthFit = Fit.Distance; }
            }
        }
    }
    TestTrue(TEXT("The widest board is framed from far enough that it needs more than the old 2900 cap"), LastWidthFit > 2900.0f);
    // A larger board is framed from farther away than a smaller one.
    const FSafeRect Safe;
    TestTrue(TEXT("A larger board is framed from farther away"),
        FitBoard(18, 12, Metric, -90.0f, Safe).Distance > FitBoard(10, 8, Metric, -90.0f, Safe).Distance);
    // A board too big for the zoom range reports that it does not fit instead of being cut off silently.
    TestFalse(TEXT("An enormous board reports bFits = false"), FitBoard(200, 150, Metric, 0.0f, Safe).bFits);
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBattleCameraTest, "Project.Expedition.Presentation.BattleCameraRig",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FBattleCameraTest::RunTest(const FString& Parameters)
{
    using namespace ExpeditionCameraRig;
    UWorld* World = MakeCameraWorld();
    if (!TestNotNull(TEXT("Transient world"), World)) { return false; }
    const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation/BattleCamera"), FGuid::NewGuid().ToString());
    AExpeditionBattleView* View = StartBattle(World, Directory);
    if (TestNotNull(TEXT("Battle view"), View))
    {
        TestEqual(TEXT("The battle camera's field of view is 40"), View->CameraFieldOfView(), 40.0f);
        const float Framed = View->FramedDistance();
        TestTrue(TEXT("The framed distance is inside the framed range"), Framed >= MinFramed && Framed <= MaxFramed);
        // The camera really looks down at the pitch the rig reports: 35 / 45 / 60.
        const TPair<float, float> Stops[] = {{NearDistance, 35.0f}, {Framed, 45.0f}, {FarStop(Framed), 60.0f}};
        for (const TPair<float, float>& Stop : Stops)
        {
            View->SnapCamera(-90.0f, Stop.Key);
            TestEqual(FString::Printf(TEXT("Rig pitch at %.0f"), Stop.Key), View->CameraPitch(), Stop.Value, 0.01f);
            TestEqual(FString::Printf(TEXT("Camera looks down %.0f degrees at %.0f"), Stop.Value, Stop.Key), static_cast<float>(-View->CameraTransform().Rotator().Pitch), Stop.Value, 0.05f);
        }
        float LastPitch = 0.0f;
        for (float Distance = MinDistance; Distance <= MaxDistance; Distance += 100.0f)
        {
            View->SnapCamera(-90.0f, Distance);
            TestTrue(TEXT("The camera pitch is monotonic in zoom"), View->CameraPitch() >= LastPitch - 0.001f);
            LastPitch = View->CameraPitch();
        }
        // Free orbit keeps the pitch.
        View->SnapCamera(-90.0f, View->FramedDistance(-90.0f));
        View->Orbit(45.0f);
        View->Tick(5.0f);
        // At the default zoom orbiting keeps the default for the new yaw; the pitch follows that distance along the ramp anchored at the home fit.
        TestEqual(TEXT("Orbit keeps the pitch on the zoom ramp"), static_cast<float>(-View->CameraTransform().Rotator().Pitch), PitchFor(View->CameraDistance(), Framed), 0.1f);
        // A zoom the player chose is kept through an orbit.
        View->SnapCamera(-90.0f, 1100.0f);
        View->Orbit(90.0f);
        TestEqual(TEXT("Orbit keeps a deliberate zoom"), View->CameraDistance(), 1100.0f, 0.01f);
        View->SnapCamera(-90.0f, FarStop(Framed));
        View->Orbit(90.0f);
        TestEqual(TEXT("Orbit keeps a deliberate far zoom"), View->CameraDistance(), FarStop(Framed), 0.01f);
        // Pan never shows more than 1.5 cells past the board's outer tile edge whatever the yaw, an axis the view is wider than stays at the
        // board centre, nothing pans while the whole board is in view, and a snap clears it. Checked through the real camera.
        const FCombatEncounter& Encounter = View->Rules().Snapshot().Encounter;
        const float Cell = View->Metric().CellCm;
        const FVector2D BoardMid = BoardCentre(Encounter.Width, Encounter.Height, View->Metric());
        const double LowerX = -(0.5 + PanMarginCells) * Cell, UpperX = (Encounter.Width - 1 + 0.5 + PanMarginCells) * Cell;
        const double LowerY = -(0.5 + PanMarginCells) * Cell, UpperY = (Encounter.Height - 1 + 0.5 + PanMarginCells) * Cell;
        // Returns whether the bound applied (it does not while the whole board is in view: then nothing pans).
        auto CheckPanBound = [&](const FString& Label) -> bool
        {
            using namespace CameraLightingTestSupport;
            if (View->WholeBoardInView()) { return false; }
            const FSafeRect Rect = View->SafeRect();
            const FTransform Camera = View->CameraTransform();
            const FTransform Actor = View->GetActorTransform();
            const double PlaneZ = Actor.GetLocation().Z + View->Metric().BoardTop();
            double MinX = TNumericLimits<double>::Max(), MaxX = TNumericLimits<double>::Lowest(), MinY = MinX, MaxY = MaxX;
            const FVector2D Corners[4] = {
                FVector2D(Rect.Left, Rect.Top), FVector2D(1.0 - Rect.Right, Rect.Top), FVector2D(Rect.Left, 1.0 - Rect.Bottom), FVector2D(1.0 - Rect.Right, 1.0 - Rect.Bottom)};
            for (const FVector2D& Corner : Corners)
            {
                FVector2D Ground;
                if (!TestTrue(*(Label + TEXT(": a safe rect corner has ground under it")),
                    Unproject(Corner, Camera.GetLocation(), Camera.GetRotation().GetForwardVector(), Rect.Aspect, static_cast<float>(PlaneZ), Ground))) { return true; }
                const FVector InView = Actor.InverseTransformPosition(FVector(Ground.X, Ground.Y, PlaneZ));
                MinX = FMath::Min(MinX, InView.X); MaxX = FMath::Max(MaxX, InView.X);
                MinY = FMath::Min(MinY, InView.Y); MaxY = FMath::Max(MaxY, InView.Y);
            }
            const FVector UnitAt = View->UnitPosition(View->Rules().ActiveId(), 0.0f);
            // The rendered focus: the arm's framing puts it in the middle of the safe rect (on the focus plane, the actor's height).
            FVector2D FocusGround;
            if (!TestTrue(*(Label + TEXT(": the safe rect centre has ground under it")),
                Unproject(FVector2D(0.5, 0.5) + Rect.CentreOffset(), Camera.GetLocation(), Camera.GetRotation().GetForwardVector(), Rect.Aspect, static_cast<float>(Actor.GetLocation().Z), FocusGround))) { return true; }
            const FVector Rendered = Actor.InverseTransformPosition(FVector(FocusGround.X, FocusGround.Y, Actor.GetLocation().Z));
            FString Where;
            const bool bUnitIn = UnitInsideRect(*View, Rect, Where);
            // A locked axis centres the view on the board exactly. The only exception is the unpanned unit hold: it moves the focus
            // from the centring focus toward the acting unit (never past it), and then the unit is in view.
            auto Locked = [&](const TCHAR* Axis, double ViewMid, double BoardMidAxis, double FocusAxis, double UnitAxis)
            {
                const double Off = ViewMid - BoardMidAxis;
                if (FMath::Abs(Off) <= 1.0) { return; }
                const double Centring = FocusAxis - Off;
                const bool bTowardUnit = FocusAxis >= FMath::Min(Centring, UnitAxis) - 1.0 && FocusAxis <= FMath::Max(Centring, UnitAxis) + 1.0;
                TestTrue(*FString::Printf(TEXT("%s: a locked %s axis is off centre by %.1f cm only for the unit hold (focus %.1f, centring %.1f, unit %.1f)"), *Label, Axis, Off, FocusAxis, Centring, UnitAxis), bTowardUnit && bUnitIn);
            };
            if (MaxX - MinX > UpperX - LowerX) { Locked(TEXT("X"), 0.5 * (MinX + MaxX), BoardMid.X, Rendered.X, UnitAt.X); }
            else { TestTrue(*(Label + TEXT(": the view stays within 1.5 cells of the board in X")), MinX >= LowerX - 1.0 && MaxX <= UpperX + 1.0); }
            if (MaxY - MinY > UpperY - LowerY) { Locked(TEXT("Y"), 0.5 * (MinY + MaxY), BoardMid.Y, Rendered.Y, UnitAt.Y); }
            else { TestTrue(*(Label + TEXT(": the view stays within 1.5 cells of the board in Y")), MinY >= LowerY - 1.0 && MaxY <= UpperY + 1.0); }
            return true;
        };
        for (const float Yaw : {-90.0f, 0.0f, 90.0f, 180.0f, 33.0f})
        {
            for (const float Zoom : {View->FramedDistance(Yaw), 1300.0f, 2000.0f})
            {
                const FString Label = FString::Printf(TEXT("Yaw %.0f at zoom %.0f"), Yaw, Zoom);
                View->SnapCamera(Yaw, Zoom);
                for (int32 Step = 0; Step < 40; ++Step) { View->Pan(900.0f, 700.0f); }
                View->Tick(0.016f);
                CheckPanBound(Label + TEXT(" panned one way"));
                for (int32 Step = 0; Step < 80; ++Step) { View->Pan(-900.0f, -700.0f); }
                View->Tick(0.016f);
                CheckPanBound(Label + TEXT(" panned the other way"));
            }
        }
        // Near zoom has room on every board: the pan really moves the focus there (both ways, so it cannot start at the bound), and a snap clears it.
        {
            View->SnapCamera(-90.0f, 1300.0f);
            const FVector Before = View->CameraFocus();
            for (int32 Step = 0; Step < 6; ++Step) { View->Pan(300.0f, 0.0f); }
            View->Tick(0.016f);
            const FVector One = View->CameraFocus();
            for (int32 Step = 0; Step < 12; ++Step) { View->Pan(-300.0f, 0.0f); }
            View->Tick(0.016f);
            TestTrue(TEXT("A pan at near zoom moves the focus"), FVector::Dist2D(View->CameraFocus(), One) > 0.5 * Cell);
            View->SnapCamera(-90.0f, 1300.0f);
            TestTrue(TEXT("A snap clears the pan"), View->CameraFocus().Equals(Before, 0.01));
        }
        // The bound holds on every rendered frame, not only once settled: zooming out or orbiting from a pan stop eases through poses
        // that see more ground than the one the pan was clamped for (an 18x12 board at yaw -90 overran by about 50 cm before the fix).
        const TPair<float, float> Pushes[] = {{900.0f, 0.0f}, {0.0f, 900.0f}, {-900.0f, -700.0f}};
        for (const TPair<float, float>& Push : Pushes)
        {
            const FString At = FString::Printf(TEXT("pan stop (%.0f, %.0f)"), Push.Key, Push.Value);
            View->SnapCamera(-90.0f, 1300.0f);
            for (int32 Step = 0; Step < 40; ++Step) { View->Pan(Push.Key, Push.Value); View->Tick(1.0f / 60.0f); }
            // Out to just short of the whole-board fit (beyond it nothing pans and the bound no longer applies), through any lock threshold on the way.
            View->Zoom(View->WholeBoardDistance(-90.0f) - 20.0f - 1300.0f);
            int32 Bounded = 0;
            for (int32 Frame = 0; Frame < 90; ++Frame) { View->Tick(1.0f / 60.0f); Bounded += CheckPanBound(FString::Printf(TEXT("Zooming out from the %s, frame %d"), *At, Frame)) ? 1 : 0; }
            TestTrue(*FString::Printf(TEXT("Zooming out from the %s checks the bound on most frames before the whole board is in view (%d)"), *At, Bounded), Bounded >= 15);
            View->SnapCamera(-90.0f, 1300.0f);
            for (int32 Step = 0; Step < 40; ++Step) { View->Pan(Push.Key, Push.Value); View->Tick(1.0f / 60.0f); }
            View->Orbit(90.0f);
            for (int32 Frame = 0; Frame < 90; ++Frame)
            {
                View->Tick(1.0f / 60.0f);
                TestTrue(*FString::Printf(TEXT("Orbiting from the %s at near zoom, frame %d, the bound applies"), *At, Frame), CheckPanBound(FString::Printf(TEXT("Orbiting from the %s, frame %d"), *At, Frame)));
            }
            // A shake at the pan stop (a crit's 1.0) keeps the bound on every frame through the rendered camera, and the camera really shakes.
            View->SnapCamera(-90.0f, 1300.0f);
            for (int32 Step = 0; Step < 40; ++Step) { View->Pan(Push.Key, Push.Value); View->Tick(1.0f / 60.0f); }
            const FVector Still = View->CameraTransform().GetLocation();
            View->Shake(1.0f);
            double Moved = 0.0;
            for (int32 Frame = 0; Frame < 40; ++Frame)
            {
                View->Tick(1.0f / 60.0f);
                CheckPanBound(FString::Printf(TEXT("Shaking at the %s, frame %d"), *At, Frame));
                Moved = FMath::Max(Moved, FVector::Dist(View->CameraTransform().GetLocation(), Still));
            }
            TestTrue(*FString::Printf(TEXT("A shake at the %s still moves the camera (%.1f cm)"), *At, Moved), Moved > 2.0);
        }
        // Unpanned at the default zoom (the whole board not in view), a shake never takes the acting unit out of the safe rect (the unit hold wins over the bound).
        {
            View->SnapCamera(-90.0f, View->FramedDistance(-90.0f));
            View->Tick(1.0f / 60.0f);
            View->Shake(1.0f);
            for (int32 Frame = 0; Frame < 40; ++Frame)
            {
                View->Tick(1.0f / 60.0f);
                FString Where;
                const bool bInside = CameraLightingTestSupport::UnitInsideRect(*View, View->SafeRect(), Where);
                TestTrue(*FString::Printf(TEXT("Shaking unpanned at the default zoom, frame %d: the acting unit stays in the safe rect %s"), Frame, *Where), bInside);
            }
        }
        // With the whole board in view the bound does not apply, and a shake moves the camera as before, height included.
        {
            View->SnapCamera(-90.0f, View->FarFramedDistance(-90.0f));
            View->Tick(1.0f / 60.0f);
            TestTrue(TEXT("The far stop shows the whole board"), View->WholeBoardInView());
            const FVector Still = View->CameraTransform().GetLocation();
            View->Shake(1.0f);
            double Rise = 0.0;
            for (int32 Frame = 0; Frame < 20; ++Frame) { View->Tick(1.0f / 60.0f); Rise = FMath::Max(Rise, FMath::Abs(View->CameraTransform().GetLocation().Z - Still.Z)); }
            TestTrue(*FString::Printf(TEXT("With the whole board in view a shake still moves the camera's height (%.1f cm)"), Rise), Rise > 1.0);
            View->Tick(1.0f); // let the shake die out before the checks below
        }
        // A snap is a fixed pose: no pan, no drift.
        View->SnapCamera(90.0f, 1100.0f);
        View->Tick(0.016f);
        const FVector First = View->CameraTransform().GetLocation();
        View->Tick(0.5f);
        TestTrue(TEXT("The camera does not drift after a snap"), View->CameraTransform().GetLocation().Equals(First, 0.01f));
        // At the whole-board fit (the old default, and where the far pose starts) the real camera frames the whole board inside the HUD-safe rect, with the unit lean on.
        // The default zoom (#148 gate 2) is the closer of that fit and the figure-sized distance: the whole board is inside only where the fit is the closer one,
        // and the acting unit is inside the safe rect either way.
        {
            using namespace CameraLightingTestSupport;
            const FBoardMetric& Metric = View->Metric();
            FVector Corners[4];
            BoardCorners(Encounter.Width, Encounter.Height, Metric, Corners);
            for (FVector& Corner : Corners) { Corner = View->GetActorTransform().TransformPosition(Corner); }
            for (const FViewportSize& Size : Viewports)
            {
                const FSafeRect Rect = FSafeRect::ForViewport(Size.X, Size.Y);
                View->SetSafeRect(Rect);
                for (const float Yaw : {-90.0f, 0.0f, 90.0f, 180.0f})
                {
                    const FString Label = FString::Printf(TEXT("%dx%d board at %dx%d yaw %.0f"), Encounter.Width, Encounter.Height, Size.X, Size.Y, Yaw);
                    TestTrue(*(Label + TEXT(": the whole-board fit is inside the zoom range")), View->FramedFits(Yaw));
                    View->SnapCamera(Yaw, View->WholeBoardDistance(Yaw));
                    FTransform Camera = View->CameraTransform();
                    FString Where;
                    bool bInside = CornersInside(Corners, Camera.GetLocation(), Camera.GetRotation().GetForwardVector(), Rect, Where);
                    TestTrue(*(Label + TEXT(": at the whole-board fit the corners are inside the safe rect ") + Where), bInside);
                    // Far still fits the whole board.
                    View->SnapCamera(Yaw, View->FarFramedDistance(Yaw));
                    Camera = View->CameraTransform();
                    bInside = CornersInside(Corners, Camera.GetLocation(), Camera.GetRotation().GetForwardVector(), Rect, Where);
                    TestTrue(*(Label + TEXT(": the far pose has the corners inside the safe rect ") + Where), bInside);
                    TestTrue(*(Label + TEXT(": the far pose is not nearer than the whole-board fit")), View->FarFramedDistance(Yaw) >= View->WholeBoardDistance(Yaw) - 0.01f);
                    // The default.
                    const float Default = View->FramedDistance(Yaw);
                    TestTrue(*(Label + TEXT(": the default is never farther than the whole-board fit")), Default <= View->WholeBoardDistance(Yaw) + 0.01f);
                    View->SnapCamera(Yaw, Default);
                    View->Tick(0.016f);
                    Camera = View->CameraTransform();
                    Where.Reset();
                    const bool bUnit = UnitInsideRect(*View, Rect, Where);
                    TestTrue(*(Label + TEXT(": at the default the acting unit is inside the safe rect ") + Where), bUnit);
                    if (FMath::IsNearlyEqual(Default, View->WholeBoardDistance(Yaw), 0.5f))
                    {
                        bInside = CornersInside(Corners, Camera.GetLocation(), Camera.GetRotation().GetForwardVector(), Rect, Where);
                        TestTrue(*(Label + TEXT(": where the default is the whole-board fit the corners are inside the safe rect ") + Where), bInside);
                    }
                }
            }
            // Orbiting from the default zoom keeps the default at every yaw it passes through (not just a snapped pose).
            for (const FViewportSize& Size : Viewports)
            {
                const FSafeRect Rect = FSafeRect::ForViewport(Size.X, Size.Y);
                View->SetSafeRect(Rect);
                View->SnapCamera(-90.0f, View->FramedDistance(-90.0f));
                for (int32 Quarter = 1; Quarter <= 3; ++Quarter)
                {
                    View->Orbit(90.0f);
                    View->Tick(5.0f);
                    const float Yaw = -90.0f + 90.0f * Quarter;
                    const FString Label = FString::Printf(TEXT("%dx%d board orbited to yaw %.0f at %dx%d"), Encounter.Width, Encounter.Height, Yaw, Size.X, Size.Y);
                    TestEqual(*(Label + TEXT(": the distance follows the default")), View->CameraDistance(), View->FramedDistance(Yaw), 0.5f);
                    const FTransform Camera = View->CameraTransform();
                    FString Where;
                    const bool bUnit = UnitInsideRect(*View, Rect, Where);
                    TestTrue(*(Label + TEXT(": the acting unit is inside the safe rect ") + Where), bUnit);
                    if (FMath::IsNearlyEqual(View->FramedDistance(Yaw), View->WholeBoardDistance(Yaw), 0.5f))
                    {
                        const bool bInside = CornersInside(Corners, Camera.GetLocation(), Camera.GetRotation().GetForwardVector(), Rect, Where);
                        TestTrue(*(Label + TEXT(": the corners are inside the safe rect ") + Where), bInside);
                    }
                }
            }
            // Orbits keep the acting unit inside the safe rect on EVERY frame of the easing, not just once it settles, and the whole board too
            // wherever the whole-board fit is the closer default: the yaw, distance and focus ease independently, so the rendered fit must
            // follow the rendered yaw. Includes consecutive orbits issued before the first one finished. The live board is whatever the
            // route generated (see the label); DefaultFraming below runs the same orbits on 16x11 and 18x12 boards.
            // Consecutive orbits are issued mid-ease (12 frames apart), so they retarget a camera that is still turning.
            struct FOrbitCase { const TCHAR* Name; TArray<float> Orbits; float Start; float FinalDelta; };
            const FOrbitCase Cases[] = {
                {TEXT("+90"), {90.0f}, -90.0f, 90.0f}, {TEXT("-90"), {-90.0f}, -90.0f, -90.0f}, {TEXT("+90 then -90"), {90.0f, -90.0f}, -90.0f, 0.0f},
                {TEXT("+90 then +90"), {90.0f, 90.0f}, -90.0f, 180.0f}, {TEXT("-90 x3"), {-90.0f, -90.0f, -90.0f}, 0.0f, -270.0f}};
            constexpr int32 GapFrames = 12, SettleFrames = 150;
            auto RenderedYaw = [&View]() { return static_cast<float>(View->CameraTransform().Rotator().Yaw); };
            for (const FViewportSize& Size : Viewports)
            {
                const FSafeRect Rect = FSafeRect::ForViewport(Size.X, Size.Y);
                View->SetSafeRect(Rect);
                for (const FOrbitCase& Case : Cases)
                {
                    View->SnapCamera(Case.Start, View->FramedDistance(Case.Start));
                    View->Tick(1.0f / 60.0f);
                    const float StartYaw = RenderedYaw();
                    float MaxDeparture = 0.0f;
                    bool bFailed = false;
                    for (int32 Command = 0; Command < Case.Orbits.Num() && !bFailed; ++Command)
                    {
                        View->Orbit(Case.Orbits[Command]);
                        const int32 Frames = Command + 1 < Case.Orbits.Num() ? GapFrames : SettleFrames;
                        for (int32 Frame = 0; Frame < Frames; ++Frame)
                        {
                            View->Tick(1.0f / 60.0f);
                            MaxDeparture = FMath::Max(MaxDeparture, FMath::Abs(FMath::FindDeltaAngleDegrees(StartYaw, RenderedYaw())));
                            const FTransform Camera = View->CameraTransform();
                            FString Where;
                            // The camera heading is the orbit yaw turned half a circle, and the board is symmetric under that turn.
                            const bool bWholeFramed = View->WholeBoardDistance(RenderedYaw()) < View->FigureDistance() - 25.0f;
                            bool bInside = UnitInsideRect(*View, Rect, Where);
                            if (bInside && bWholeFramed) { bInside = CornersInside(Corners, Camera.GetLocation(), Camera.GetRotation().GetForwardVector(), Rect, Where); }
                            if (!bInside)
                            {
                                AddError(FString::Printf(TEXT("%dx%d board, orbit %s at %dx%d: command %d, frame %d leaves the safe rect %s"), Encounter.Width, Encounter.Height, Case.Name, Size.X, Size.Y, Command, Frame, *Where));
                                bFailed = true;
                                break;
                            }
                        }
                    }
                    // The camera really turned, and (for +90 then -90) came back rather than cancelling before moving.
                    TestTrue(FString::Printf(TEXT("Orbit %s at %dx%d: the camera departed from its start yaw"), Case.Name, Size.X, Size.Y), MaxDeparture > 5.0f);
                    TestTrue(FString::Printf(TEXT("Orbit %s at %dx%d: it settles at the start yaw plus the orbits"), Case.Name, Size.X, Size.Y),
                        FMath::Abs(FMath::FindDeltaAngleDegrees(StartYaw + Case.FinalDelta, RenderedYaw())) < 1.0f);
                }
            }
            // A deliberate zoom-in survives the orbit and its easing.
            {
                View->SetSafeRect(FSafeRect::ForViewport(1920, 1080));
                View->SnapCamera(-90.0f, View->FramedDistance(-90.0f));
                View->Tick(1.0f / 60.0f);
                View->Zoom(-600.0f);
                const float Chosen = View->CameraDistance();
                View->Orbit(90.0f);
                View->Orbit(90.0f);
                for (int32 Frame = 0; Frame < 120; ++Frame) { View->Tick(1.0f / 60.0f); }
                TestEqual(TEXT("A deliberate zoom-in is kept through consecutive orbits"), View->CameraDistance(), Chosen, 0.01f);
                TestEqual(TEXT("The rendered pitch still follows the chosen zoom"), static_cast<float>(-View->CameraTransform().Rotator().Pitch), PitchFor(Chosen, Framed), 0.2f);
            }
            View->SetSafeRect(FSafeRect());
        }
        // The focus lands in the middle of the free screen area: right of the screen centre, clear of the left HUD.
        View->SnapCamera(-90.0f, Framed);
        const FVector Focus = View->GetActorTransform().TransformPosition(View->CameraFocus());
        const FVector Local = View->CameraTransform().InverseTransformPosition(Focus);
        TestTrue(TEXT("The focus sits right of the screen centre"), Local.Y > 0.0f);
    }
    World->DestroyWorld(false);
    IFileManager::Get().DeleteDirectory(*Directory, false, true);
    return !HasAnyErrors();
}

/** #148 gate 2: on real 16x11 and 18x12 boards the default zoom is figure-sized, far still fits the board, and the acting unit never leaves the safe rect. */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDefaultFramingTest, "Project.Expedition.Presentation.DefaultFraming",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FDefaultFramingTest::RunTest(const FString& Parameters)
{
    using namespace ExpeditionCameraRig;
    using namespace CameraLightingTestSupport;
    const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation/DefaultFraming"), FGuid::NewGuid().ToString());
    for (const FIntPoint Board : {FIntPoint(16, 11), FIntPoint(18, 12)})
    {
        const FString Path = FPaths::Combine(Directory, FString::Printf(TEXT("%dx%d"), Board.X, Board.Y), TEXT("current.json"));
        if (!TestTrue(FString::Printf(TEXT("A seed gives a %dx%d board"), Board.X, Board.Y), WriteCameraBoardCheckpoint(Path, Board.X, Board.Y))) { continue; }
        UWorld* World = MakeCameraWorld();
        if (!TestNotNull(TEXT("Transient world"), World)) { continue; }
        AExpeditionBattleView* View = World->SpawnActor<AExpeditionBattleView>();
        if (TestNotNull(TEXT("Battle view"), View))
        {
            View->ConfigureStorage(Path);
            View->DispatchBeginPlay();
            View->Resume();
            View->HoldEnemyTurns(true);
            View->Tick(0.016f);
            const FCombatEncounter& Encounter = View->Rules().Snapshot().Encounter;
            TestEqual(TEXT("Board width"), Encounter.Width, Board.X);
            TestEqual(TEXT("Board height"), Encounter.Height, Board.Y);
            FVector Corners[4];
            BoardCorners(Encounter.Width, Encounter.Height, View->Metric(), Corners);
            for (FVector& Corner : Corners) { Corner = View->GetActorTransform().TransformPosition(Corner); }
            for (const FViewportSize& Size : {FViewportSize{1920, 1080}, FViewportSize{1280, 720}})
            {
                const FSafeRect Rect = FSafeRect::ForViewport(Size.X, Size.Y);
                View->SetSafeRect(Rect);
                const float Target = FigureTargetPixels * Size.Y / FigureTargetViewportHeight;
                for (const float Yaw : {-90.0f, 0.0f, 90.0f, 180.0f})
                {
                    const FString Label = FString::Printf(TEXT("%dx%d at %dx%d yaw %.0f"), Board.X, Board.Y, Size.X, Size.Y, Yaw);
                    // The default: a standing 180 cm figure at the focus is 88 px tall at 1080 (about 59 at 720), within 10 %.
                    View->SnapCamera(Yaw, View->FramedDistance(Yaw));
                    View->Tick(0.016f);
                    const float Pixels = FigurePixelsAtFocus(*View, Rect, Size.Y);
                    TestTrue(*FString::Printf(TEXT("%s: the default figure is %.1f px tall, target %.1f px (+/-10%%)"), *Label, Pixels, Target), FMath::Abs(Pixels - Target) <= 0.1f * Target);
                    FString Where;
                    const bool bUnit = UnitInsideRect(*View, Rect, Where);
                    TestTrue(*(Label + TEXT(": the acting unit is inside the safe rect at the default ") + Where), bUnit);
                    // Far still reaches the whole board.
                    View->SnapCamera(Yaw, View->FarFramedDistance(Yaw));
                    View->Tick(0.016f);
                    const FTransform Camera = View->CameraTransform();
                    const bool bFar = CornersInside(Corners, Camera.GetLocation(), Camera.GetRotation().GetForwardVector(), Rect, Where);
                    TestTrue(*(Label + TEXT(": far fits the whole board in the safe rect ") + Where), bFar);
                    TestTrue(*(Label + TEXT(": far is farther than the default")), View->FarFramedDistance(Yaw) > View->FramedDistance(Yaw));
                }
                // Orbit easing at the default zoom keeps the acting unit inside the safe rect on every frame, as a swept pair of orbits and a reversal.
                struct FOrbit { const TCHAR* Name; TArray<float> Turns; };
                const FOrbit Orbits[] = {{TEXT("+90"), {90.0f}}, {TEXT("-90"), {-90.0f}}, {TEXT("+90 then -90"), {90.0f, -90.0f}}, {TEXT("+90 then +90"), {90.0f, 90.0f}}};
                for (const FOrbit& Orbit : Orbits)
                {
                    View->SnapCamera(-90.0f, View->FramedDistance(-90.0f));
                    View->Tick(1.0f / 60.0f);
                    bool bFailed = false;
                    for (int32 Command = 0; Command < Orbit.Turns.Num() && !bFailed; ++Command)
                    {
                        View->Orbit(Orbit.Turns[Command]);
                        const int32 Frames = Command + 1 < Orbit.Turns.Num() ? 12 : 150;
                        for (int32 Frame = 0; Frame < Frames; ++Frame)
                        {
                            View->Tick(1.0f / 60.0f);
                            FString Where;
                            if (!UnitInsideRect(*View, Rect, Where))
                            {
                                AddError(FString::Printf(TEXT("%dx%d board, orbit %s at %dx%d: command %d, frame %d leaves the acting unit outside the safe rect %s"), Board.X, Board.Y, Orbit.Name, Size.X, Size.Y, Command, Frame, *Where));
                                bFailed = true;
                                break;
                            }
                        }
                    }
                    TestEqual(FString::Printf(TEXT("%dx%d orbit %s at %dx%d settles on the default"), Board.X, Board.Y, Orbit.Name, Size.X, Size.Y),
                        View->CameraDistance(), View->FramedDistance(View->CameraYaw()), 0.5f);
                }
            }
            View->SetSafeRect(FSafeRect());
        }
        World->DestroyWorld(false);
    }
    IFileManager::Get().DeleteDirectory(*Directory, false, true);
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOverviewCameraTest, "Project.Expedition.Presentation.OverviewCameraUnchanged",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FOverviewCameraTest::RunTest(const FString& Parameters)
{
    UWorld* World = MakeCameraWorld();
    if (!TestNotNull(TEXT("Transient world"), World)) { return false; }
    const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation/OverviewCamera"), FGuid::NewGuid().ToString());
    AExpeditionBattleView* View = World->SpawnActor<AExpeditionBattleView>();
    if (TestNotNull(TEXT("Battle view"), View))
    {
        View->ConfigureStorage(FPaths::Combine(Directory, TEXT("current.json")));
        View->DispatchBeginPlay();
        View->Tick(0.016f);
        if (TestTrue(TEXT("The shipped theme loads, so the tabletop overview exists"), View->UsesTheme()))
        {
            TestEqual(TEXT("The overview keeps its 50 degree field of view"), View->CameraFieldOfView(), 50.0f);
            TestEqual(TEXT("The overview keeps its fixed distance"), View->CameraDistance(), 300.0f);
            const FTransform Before = View->CameraTransform();
            View->Orbit(90.0f); View->Zoom(500.0f); View->Pan(500.0f, 500.0f);
            View->Tick(1.0f);
            TestTrue(TEXT("Orbit, zoom and pan do not move the tabletop camera"), View->CameraTransform().Equals(Before, 0.01f));
            TestEqual(TEXT("The overview still has its 50 degree field of view"), View->CameraFieldOfView(), 50.0f);
        }
    }
    World->DestroyWorld(false);
    IFileManager::Get().DeleteDirectory(*Directory, false, true);
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBattleLightingTest, "Project.Expedition.Presentation.BattleLighting",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FBattleLightingTest::RunTest(const FString& Parameters)
{
    UWorld* World = MakeCameraWorld();
    if (!TestNotNull(TEXT("Transient world"), World)) { return false; }
    ADirectionalLight* Sun = World->SpawnActor<ADirectionalLight>();
    const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation/BattleLighting"), FGuid::NewGuid().ToString());
    AExpeditionBattleView* View = StartBattle(World, Directory);
    if (TestNotNull(TEXT("Sun"), Sun) && TestNotNull(TEXT("Battle view"), View))
    {
        // Rebuild so the sun is themed too; it joins the count.
        View->SetBoardMetric(FBoardMetric(150.0f, 180.0f));
        TestTrue(TEXT("The key light casts shadows"), Sun->GetLightComponent()->CastShadows);
        const int32 Shadowed = View->ShadowCastingLights();
        TestTrue(FString::Printf(TEXT("At most the sun plus two local lights cast shadows (%d)"), Shadowed), Shadowed >= 1 && Shadowed <= 3);
        // Light radii scale with the board metric.
        View->SetBoardMetric(FBoardMetric(110.0f, 180.0f));
        const float Reference = View->FlameLightRadius();
        View->SetBoardMetric(FBoardMetric(165.0f, 180.0f));
        const float Large = View->FlameLightRadius();
        if (TestTrue(TEXT("The board has flame lights"), Reference > 0.0f))
        { TestEqual(TEXT("Light radii scale with the metric (110 to 165 cm is 1.5x)"), Large / Reference, 1.5f, 0.001f); }
        TestTrue(TEXT("The cap holds at the larger metric"), View->ShadowCastingLights() <= 3);
        // The choice is deterministic: the same rebuild shadows the same lights.
        const int32 Again = View->ShadowCastingLights();
        const TArray<FVector> Chosen = View->ShadowedLightPositions();
        View->SetBoardMetric(FBoardMetric(165.0f, 180.0f));
        TestEqual(TEXT("A repeated rebuild shadows the same number of lights"), View->ShadowCastingLights(), Again);
        const TArray<FVector> Repeated = View->ShadowedLightPositions();
        if (TestEqual(TEXT("A repeated rebuild shadows the same number of local lights"), Repeated.Num(), Chosen.Num()))
        {
            for (int32 Index = 0; Index < Chosen.Num(); ++Index)
            { TestTrue(FString::Printf(TEXT("Shadowed light %d is the same light after a rebuild"), Index), Repeated[Index].Equals(Chosen[Index], 0.01)); }
        }
    }
    World->DestroyWorld(false);
    IFileManager::Get().DeleteDirectory(*Directory, false, true);
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFigureSilhouetteTest, "Project.Expedition.Presentation.FigureSilhouettes",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FFigureSilhouetteTest::RunTest(const FString& Parameters)
{
    UWorld* World = MakeCameraWorld();
    if (!TestNotNull(TEXT("Transient world"), World)) { return false; }
    const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation/FigureSilhouettes"), FGuid::NewGuid().ToString());
    AExpeditionBattleView* View = StartBattle(World, Directory);
    if (TestNotNull(TEXT("Battle view"), View))
    {
        int32 Party = 0, Enemies = 0;
        for (const FCombatantState& Unit : View->Rules().Snapshot().Encounter.Combatants)
        {
            if (Unit.Life == ECombatLife::Dead) { continue; }
            const bool bEnemy = Unit.Side == ECombatSide::Enemy;
            (bEnemy ? Enemies : Party)++;
            TestEqual(FString::Printf(TEXT("%s renders custom depth with its side's stencil"), *Unit.Name),
                View->FigureStencil(Unit.Id), bEnemy ? AExpeditionBattleView::EnemyStencil : AExpeditionBattleView::PartyStencil);
            // Only figure parts: the footprint ring and scenery never write the stencil.
            if (USceneComponent* Root = View->UnitRigRoot(Unit.Id))
            {
                TArray<USceneComponent*> Children;
                Root->GetChildrenComponents(true, Children);
                for (USceneComponent* Child : Children)
                {
                    const UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(Child);
                    if (Primitive) { TestTrue(FString::Printf(TEXT("%s part writes custom depth"), *Unit.Name), Primitive->bRenderCustomDepth); }
                }
            }
        }
        TestTrue(TEXT("The fixture has both sides"), Party > 0 && Enemies > 0);
        // The blendable is added when the asset exists and silently skipped when it does not.
        const bool bAsset = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/DnDDiceRoll/Theme/Materials/M_FigureSilhouette.M_FigureSilhouette"), nullptr, LOAD_Quiet | LOAD_NoWarn) != nullptr;
        TestEqual(TEXT("The silhouette blendable is present exactly when M_FigureSilhouette exists"), View->HasSilhouetteBlendable(), bAsset);
        // A rebuild never adds it twice.
        View->SetBoardMetric(FBoardMetric(150.0f, 180.0f));
        TestEqual(TEXT("A rebuild keeps one blendable"), View->HasSilhouetteBlendable(), bAsset);
    }
    World->DestroyWorld(false);
    IFileManager::Get().DeleteDirectory(*Directory, false, true);
    return !HasAnyErrors();
}

/**
 * #209: the camera never shows ground beyond an authored set, by zoom alone. SetCovers and SetCoverDistance give the upper bound for a yaw (the footprint
 * is no wider than the set); SetCoverDistanceAt gives, for a focus the pan bound, the locked axes and the unit hold already decided, the farthest zoom at
 * which the ground around that focus fits, never nearer than the default and never moving the focus. Pure rig, at the aspects the game runs at.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSetBoundRigTest, "Project.Expedition.Presentation.CameraRigKeepsTheViewInsideTheSet",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSetBoundRigTest::RunTest(const FString& Parameters)
{
    using namespace ExpeditionCameraRig;
    using namespace CameraLightingTestSupport;
    const FBoardMetric Metric(150.0f, 180.0f);
    const float Half = SetHalfExtentCells * Metric.CellCm;
    int32 Capped = 0, Free = 0, Reported = 0, Moved = 0;
    for (const FViewportSize& Size : Viewports)
    {
        const FSafeRect Safe = FSafeRect::ForViewport(Size.X, Size.Y);
        for (const TPair<int32, int32>& Board : {TPair<int32, int32>(16, 11), TPair<int32, int32>(18, 12)})
        {
            const float Anchor = FMath::Clamp(FitDistance(Board.Key, Board.Value, Metric, HomeYaw, 0.0f, Safe).Distance, MinFramed, MaxFramed);
            const FVector2D Centre = BoardCentre(Board.Key, Board.Value, Metric);
            // The centre, and a focus at each board corner (where a unit at the edge puts it).
            const FVector2D Foci[5] = {Centre, FVector2D(0, 0), FVector2D((Board.Key - 1) * Metric.CellCm, 0), FVector2D(0, (Board.Value - 1) * Metric.CellCm),
                FVector2D((Board.Key - 1) * Metric.CellCm, (Board.Value - 1) * Metric.CellCm)};
            for (float Yaw = -180.0f; Yaw <= 180.0f; Yaw += 15.0f)
            {
                const FString Label = FString::Printf(TEXT("%dx%d at %dx%d yaw %.0f"), Board.Key, Board.Value, Size.X, Size.Y, Yaw);
                const float Default = DefaultDistance(FitBoard(Board.Key, Board.Value, Metric, Yaw, Safe).Distance, FigureDistance(Metric, Anchor, Safe));
                const float Far = FMath::Min(MaxDistance, FMath::Max(FarStop(Anchor), FitDistance(Board.Key, Board.Value, Metric, Yaw, Anchor, Safe).Distance));
                const float Cover = SetCoverDistance(Yaw, Anchor, Metric, Safe, Half, Default, Far);
                TestTrue(*(Label + TEXT(": the yaw's cover is between the default and the far pose")), Cover >= Default - 0.01f && Cover <= Far + 0.01f);
                for (const FVector2D& Focus : Foci)
                {
                    bool bFits = true;
                    const float At = SetCoverDistanceAt(Focus, Yaw, Anchor, Metric, Safe, Centre, Half, Default, Far, bFits);
                    TestTrue(*(Label + TEXT(": the cover at a focus is between the default and the far pose")), At >= Default - 0.01f && At <= Far + 0.01f);
                    TestTrue(*(Label + TEXT(": a focus never allows more than the yaw's own cover (the bound is on the footprint's size)")), At <= FMath::Max(Cover, Default) + 0.5f);
                    if (bFits)
                    {
                        TestTrue(*(Label + TEXT(": the ground around the focus fits at the cover")), SetContainsAt(Focus, Yaw, At, PitchFor(At, Anchor), Metric, Safe, Centre, Half));
                        if (At < Far - 0.5f)
                        {
                            ++Capped;
                            TestFalse(*(Label + TEXT(": and does not a little farther")), SetContainsAt(Focus, Yaw, At + 2.0f, PitchFor(At + 2.0f, Anchor), Metric, Safe, Centre, Half));
                        }
                        else { ++Free; }
                        if (!(Focus - Centre).IsNearlyZero() && At < Cover - 0.5f) { ++Moved; }
                    }
                    else
                    {
                        // Even the default shows more than the set around this focus: the cover is the default, to be reported, and the focus is not asked to move.
                        ++Reported;
                        TestEqual(*(Label + TEXT(": where even the default does not fit at the focus the cover is the default")), At, Default, 0.01f);
                        TestFalse(*(Label + TEXT(": and it really does not fit")), SetContainsAt(Focus, Yaw, Default, PitchFor(Default, Anchor), Metric, Safe, Centre, Half));
                    }
                }
            }
        }
    }
    TestTrue(TEXT("Some poses are capped below the far pose"), Capped > 0);
    TestTrue(TEXT("An off-centre focus lowers the cover for some yaw"), Moved > 0);
    AddInfo(FString::Printf(TEXT("%d focus and yaw cases fit as far as the far pose, %d are capped, %d report that even the default does not fit"), Free, Capped, Reported));
    return !HasAnyErrors();
}
#endif
