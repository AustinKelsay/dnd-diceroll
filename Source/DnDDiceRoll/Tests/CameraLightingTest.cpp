#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/DirectionalLight.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Presentation/BoardMetric.h"
#include "Presentation/CameraRig.h"
#include "Presentation/ExpeditionBattleView.h"

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
    for (const float Framed : {1400.0f, 2300.0f, 2900.0f})
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
        TestEqual(TEXT("The far stop is 60 degrees"), PitchFor(FarDistance, Framed), 60.0f, 0.01f);
        TestEqual(TEXT("The closest allowed zoom stays at the near pitch"), PitchFor(MinDistance, Framed), 35.0f, 0.01f);
    }
    // Pan clamp: far outside the board lands on the board box grown by the margin.
    const FVector2D Far = ClampFocus(FVector2D(1.0e6f, -1.0e6f), 16, 11, 150.0f);
    TestEqual(TEXT("Pan clamps X to the board plus margin"), Far.X, 15 * 150.0f + PanMarginCells * 150.0f);
    TestEqual(TEXT("Pan clamps Y to the board plus margin"), Far.Y, -PanMarginCells * 150.0f);
    TestTrue(TEXT("A focus inside the board is untouched"), ClampFocus(FVector2D(300.0f, 600.0f), 16, 11, 150.0f).Equals(FVector2D(300.0f, 600.0f)));
    // The fit grows with the board and stays in reference units (the metric scale cancels out).
    const FSafeRect Safe;
    const float Small = FitDistance(10, 8, 150.0f, 180.0f, 150.0f / 110.0f, Safe);
    const float Large = FitDistance(18, 12, 150.0f, 180.0f, 150.0f / 110.0f, Safe);
    TestTrue(TEXT("A larger board is framed from farther away"), Large > Small);
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
        const TPair<float, float> Stops[] = {{NearDistance, 35.0f}, {Framed, 45.0f}, {FarDistance, 60.0f}};
        for (const TPair<float, float>& Stop : Stops)
        {
            View->SnapCamera(-90.0f, Stop.Key);
            TestEqual(FString::Printf(TEXT("Rig pitch at %.0f"), Stop.Key), View->CameraPitch(), Stop.Value, 0.01f);
            TestEqual(FString::Printf(TEXT("Camera looks down %.0f degrees at %.0f"), Stop.Value, Stop.Key), -View->CameraTransform().Rotator().Pitch, Stop.Value, 0.05f);
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
        TestEqual(TEXT("Orbit leaves the pitch alone"), -View->CameraTransform().Rotator().Pitch, 45.0f, 0.1f);
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
        View->SetBoardMetric(FBoardMetric(165.0f, 180.0f));
        TestEqual(TEXT("A repeated rebuild shadows the same number of lights"), View->ShadowCastingLights(), Again);
    }
    World->DestroyWorld(false);
    IFileManager::Get().DeleteDirectory(*Directory, false, true);
    return !HasAnyErrors();
}
#endif
