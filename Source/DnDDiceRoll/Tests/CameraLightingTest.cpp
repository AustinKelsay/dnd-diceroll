#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Components/LightComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Presentation/BoardMetric.h"
#include "Presentation/CameraRig.h"
#include "Presentation/ExpeditionBattleView.h"

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
    // Pan clamp: far outside the board lands on the board box grown by the margin.
    const FVector2D Far = ClampFocus(FVector2D(1.0e6f, -1.0e6f), 16, 11, 150.0f);
    TestEqual(TEXT("Pan clamps X to the board plus margin"), static_cast<float>(Far.X), 15 * 150.0f + PanMarginCells * 150.0f);
    TestEqual(TEXT("Pan clamps Y to the board plus margin"), static_cast<float>(Far.Y), -PanMarginCells * 150.0f);
    TestTrue(TEXT("A focus inside the board is untouched"), ClampFocus(FVector2D(300.0f, 600.0f), 16, 11, 150.0f).Equals(FVector2D(300.0f, 600.0f)));
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
        View->SnapCamera(-90.0f, Framed);
        View->Orbit(45.0f);
        View->Tick(2.0f);
        TestEqual(TEXT("Orbit leaves the pitch alone"), static_cast<float>(-View->CameraTransform().Rotator().Pitch), 45.0f, 0.1f);
        // Pan stays inside the board plus margin whatever the yaw, and a snap clears it.
        const FCombatEncounter& Encounter = View->Rules().Snapshot().Encounter;
        const float Cell = View->Metric().CellCm;
        for (const float Yaw : {-90.0f, 0.0f, 90.0f, 180.0f, 33.0f})
        {
            View->SnapCamera(Yaw, Framed);
            for (int32 Step = 0; Step < 40; ++Step) { View->Pan(900.0f, 700.0f); }
            View->Tick(0.016f);
            FVector Focus = View->CameraFocus();
            TestTrue(TEXT("The pan keeps the focus within the board plus margin"),
                Focus.X >= -PanMarginCells * Cell - 0.01f && Focus.X <= (Encounter.Width - 1) * Cell + PanMarginCells * Cell + 0.01f &&
                Focus.Y >= -PanMarginCells * Cell - 0.01f && Focus.Y <= (Encounter.Height - 1) * Cell + PanMarginCells * Cell + 0.01f);
            for (int32 Step = 0; Step < 80; ++Step) { View->Pan(-900.0f, -700.0f); }
            View->Tick(0.016f);
            Focus = View->CameraFocus();
            TestTrue(TEXT("The pan clamps in the other direction too"),
                Focus.X >= -PanMarginCells * Cell - 0.01f && Focus.X <= (Encounter.Width - 1) * Cell + PanMarginCells * Cell + 0.01f &&
                Focus.Y >= -PanMarginCells * Cell - 0.01f && Focus.Y <= (Encounter.Height - 1) * Cell + PanMarginCells * Cell + 0.01f);
        }
        // A snap is a fixed pose: no pan, no drift.
        View->SnapCamera(90.0f, 1100.0f);
        View->Tick(0.016f);
        const FVector First = View->CameraTransform().GetLocation();
        View->Tick(0.5f);
        TestTrue(TEXT("The camera does not drift after a snap"), View->CameraTransform().GetLocation().Equals(First, 0.01f));
        // At the default zoom the real camera frames the whole board inside the HUD-safe rect, with the unit lean on.
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
                    TestTrue(*(Label + TEXT(": the default zoom fits the zoom range")), View->FramedFits(Yaw));
                    View->SnapCamera(Yaw, View->FramedDistance(Yaw));
                    const FTransform Camera = View->CameraTransform();
                    FString Where;
                    const bool bInside = CornersInside(Corners, Camera.GetLocation(), Camera.GetRotation().GetForwardVector(), Rect, Where);
                    TestTrue(*(Label + TEXT(": corners inside the safe rect ") + Where), bInside);
                }
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
#endif
