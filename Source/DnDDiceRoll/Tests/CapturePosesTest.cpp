#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Presentation/ExpeditionBattleView.h"
#include "Presentation/ExpeditionCapture.h"
#include "Presentation/ExpeditionPlayerController.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCapturePosesTest, "Project.Expedition.Presentation.CapturePoses",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FCapturePosesTest::RunTest(const FString& Parameters)
{
    FCapturePoses Poses;
    FString Error;
    const FString Valid = TEXT("{\"version\":1,\"settleFrames\":12,\"shots\":["
        "{\"name\":\"battle-a\",\"mode\":\"battle\",\"yaw\":-90,\"distance\":1300},"
        "{\"name\":\"battle_b\",\"mode\":\"targeting\",\"yaw\":180.5,\"distance\":\"default\"},"
        "{\"name\":\"table\",\"mode\":\"overview\"},{\"name\":\"menu\",\"mode\":\"start\"},"
        "{\"name\":\"tip\",\"mode\":\"tooltip\",\"action\":\"rescue\",\"yaw\":0,\"distance\":1300},"
        "{\"name\":\"pick\",\"mode\":\"start\",\"build\":\"barbarian.berserker\"},"
        "{\"name\":\"battle-far\",\"mode\":\"battle\",\"yaw\":0,\"distance\":\"far\"}]}");
    if (TestTrue(TEXT("A valid file parses"), ExpeditionCapture::ParsePoses(Valid, Poses, Error)))
    {
        TestEqual(TEXT("Shot count"), Poses.Shots.Num(), 7);
        TestEqual(TEXT("Settle frames"), Poses.SettleFrames, 12);
        TestEqual(TEXT("Explicit distance"), Poses.Shots[0].Distance, 1300.0f);
        TestEqual(TEXT("Default distance is zero"), Poses.Shots[1].Distance, 0.0f);
        TestEqual(TEXT("Far distance is the far sentinel"), Poses.Shots[6].Distance, ExpeditionCapture::FarDistance);
        TestEqual(TEXT("Yaw"), Poses.Shots[1].Yaw, 180.5f);
        TestTrue(TEXT("Battle shots carry a camera"), Poses.Shots[0].bHasCamera);
        TestFalse(TEXT("Overview shots do not"), Poses.Shots[2].bHasCamera);
        TestEqual(TEXT("Mode"), Poses.Shots[3].Mode, ECaptureMode::Start);
        TestEqual(TEXT("A tooltip shot names its action"), Poses.Shots[4].Action, ECaptureAction::Rescue);
        TestEqual(TEXT("A start shot can name a build"), Poses.Shots[5].Build, FString(TEXT("barbarian.berserker")));
        TestTrue(TEXT("A tooltip shot carries a camera"), Poses.Shots[4].bHasCamera && Poses.Shots[4].Mode == ECaptureMode::Tooltip);
    }
    // A hint shot (#158) names its hint and carries a camera.
    {
        FCapturePoses Hinted;
        TestTrue(TEXT("A hint shot parses"), ExpeditionCapture::ParsePoses(TEXT("{\"version\":1,\"shots\":[{\"name\":\"h\",\"mode\":\"hint\",\"hint\":\"endturn\",\"yaw\":0,\"distance\":\"default\"}]}"), Hinted, Error));
        if (Hinted.Shots.Num() == 1)
        {
            TestEqual(TEXT("A hint shot names its hint"), Hinted.Shots[0].Hint, ECaptureHint::EndTurn);
            TestTrue(TEXT("A hint shot carries a camera"), Hinted.Shots[0].bHasCamera && Hinted.Shots[0].Mode == ECaptureMode::Hint);
        }
    }
    // Capture runs bind no player input; only a real -ExpeditionCapture= value turns that on.
    TestTrue(TEXT("Capture is requested by a poses path"), ExpeditionCapture::Requested(TEXT("-game -ExpeditionCapture=/tmp/poses.json -ExpeditionSaveRoot=/tmp/s")));
    TestFalse(TEXT("A normal run is not a capture run"), ExpeditionCapture::Requested(TEXT("-game -ExpeditionSaveRoot=/tmp/s")));
    TestFalse(TEXT("Capture output alone is not a capture run"), ExpeditionCapture::Requested(TEXT("-ExpeditionCaptureOut=/tmp/o -ExpeditionCaptureFixture=f")));
    TestFalse(TEXT("An empty poses path is not a capture run"), ExpeditionCapture::Requested(TEXT("-ExpeditionCapture=")));
    TestFalse(TEXT("No command line is not a capture run"), ExpeditionCapture::Requested(nullptr));

    FCapturePoses Defaults;
    TestTrue(TEXT("Settle frames default"), ExpeditionCapture::ParsePoses(TEXT("{\"version\":1,\"shots\":[{\"name\":\"m\",\"mode\":\"start\"}]}"), Defaults, Error) && Defaults.SettleFrames == 60);

    const TCHAR* Bad[] = {
        TEXT("not json"), TEXT("[]"), TEXT("{}"),
        TEXT("{\"version\":2,\"shots\":[{\"name\":\"m\",\"mode\":\"start\"}]}"),
        TEXT("{\"version\":1}"), TEXT("{\"version\":1,\"shots\":[]}"), TEXT("{\"version\":1,\"shots\":[3]}"),
        TEXT("{\"version\":1,\"settleFrames\":0,\"shots\":[{\"name\":\"m\",\"mode\":\"start\"}]}"),
        TEXT("{\"version\":1,\"settleFrames\":2.5,\"shots\":[{\"name\":\"m\",\"mode\":\"start\"}]}"),
        TEXT("{\"version\":1,\"shots\":[{\"mode\":\"start\"}]}"),
        TEXT("{\"version\":1,\"shots\":[{\"name\":\"a/b\",\"mode\":\"start\"}]}"),
        TEXT("{\"version\":1,\"shots\":[{\"name\":\"m\"}]}"),
        TEXT("{\"version\":1,\"shots\":[{\"name\":\"m\",\"mode\":\"orbit\"}]}"),
        TEXT("{\"version\":1,\"shots\":[{\"name\":\"m\",\"mode\":\"start\"},{\"name\":\"m\",\"mode\":\"start\"}]}"),
        TEXT("{\"version\":1,\"shots\":[{\"name\":\"m\",\"mode\":\"battle\",\"distance\":1300}]}"),
        TEXT("{\"version\":1,\"shots\":[{\"name\":\"m\",\"mode\":\"battle\",\"yaw\":0}]}"),
        TEXT("{\"version\":1,\"shots\":[{\"name\":\"m\",\"mode\":\"battle\",\"yaw\":999,\"distance\":1300}]}"),
        TEXT("{\"version\":1,\"shots\":[{\"name\":\"m\",\"mode\":\"battle\",\"yaw\":0,\"distance\":100}]}"),
        TEXT("{\"version\":1,\"shots\":[{\"name\":\"m\",\"mode\":\"battle\",\"yaw\":0,\"distance\":9000}]}"),
        TEXT("{\"version\":1,\"shots\":[{\"name\":\"m\",\"mode\":\"targeting\",\"yaw\":0,\"distance\":\"near\"}]}"),
        TEXT("{\"version\":1,\"shots\":[{\"name\":\"m\",\"mode\":\"battle\",\"yaw\":0,\"distance\":\"Far\"}]}"),
        TEXT("{\"version\":1,\"shots\":[{\"name\":\"m\",\"mode\":\"battle\",\"yaw\":\"0\",\"distance\":1300}]}"),
        TEXT("{\"version\":1,\"shots\":[{\"name\":\"m\",\"mode\":\"tooltip\",\"yaw\":0,\"distance\":1300}]}"),
        TEXT("{\"version\":1,\"shots\":[{\"name\":\"m\",\"mode\":\"start\",\"build\":\"Wizard\"}]}"),
        TEXT("{\"version\":1,\"shots\":[{\"name\":\"m\",\"mode\":\"start\",\"build\":7}]}"),
        TEXT("{\"version\":1,\"shots\":[{\"name\":\"m\",\"mode\":\"tooltip\",\"action\":\"jump\",\"yaw\":0,\"distance\":1300}]}"),
        TEXT("{\"version\":1,\"shots\":[{\"name\":\"m\",\"mode\":\"hint\",\"yaw\":0,\"distance\":1300}]}"),
        TEXT("{\"version\":1,\"shots\":[{\"name\":\"m\",\"mode\":\"hint\",\"hint\":\"tips\",\"yaw\":0,\"distance\":1300}]}"),
    };
    for (const TCHAR* Text : Bad)
    {
        FCapturePoses Rejected;
        FString Reason;
        TestFalse(FString::Printf(TEXT("Rejects %s"), Text), ExpeditionCapture::ParsePoses(Text, Rejected, Reason));
        TestFalse(FString::Printf(TEXT("Explains %s"), Text), Reason.IsEmpty());
    }
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCaptureSnapCameraTest, "Project.Expedition.Presentation.CaptureCameraSnaps",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FCaptureSnapCameraTest::RunTest(const FString& Parameters)
{
    const UWorld::InitializationValues Values = UWorld::InitializationValues()
        .AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
    if (!TestNotNull(TEXT("Transient world"), World)) { return false; }
    const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation/CaptureSnap"), FGuid::NewGuid().ToString());
    AExpeditionBattleView* View = World->SpawnActor<AExpeditionBattleView>();
    if (TestNotNull(TEXT("Battle view"), View))
    {
        View->ConfigureStorage(FPaths::Combine(Directory, TEXT("current.json")));
        View->DispatchBeginPlay();
        View->NewRun();
        View->ChooseRoute(0);
        View->HoldEnemyTurns(true);
        View->Tick(0.016f);
        View->SnapCamera(90.0f, 1100.0f);
        View->Tick(0.016f);
        TestEqual(TEXT("Yaw is the requested yaw at once"), View->CameraYaw(), 90.0f);
        TestEqual(TEXT("Distance is the requested distance at once"), View->CameraDistance(), 1100.0f);
        const FVector First = View->CameraTransform().GetLocation();
        View->Tick(0.016f);
        View->Tick(0.5f);
        TestTrue(TEXT("The camera does not drift after a snap"), View->CameraTransform().GetLocation().Equals(First, 0.01f));
        View->SnapCamera(0.0f, 99999.0f);
        TestEqual(TEXT("Distance keeps the battle clamp"), View->CameraDistance(), ExpeditionCapture::MaxDistance);
        TestTrue(TEXT("The framed distance is inside the clamp"), View->FramedDistance() >= ExpeditionCapture::MinDistance && View->FramedDistance() <= ExpeditionCapture::MaxDistance);
    }
    World->DestroyWorld(false);
    IFileManager::Get().DeleteDirectory(*Directory, false, true);
    return !HasAnyErrors();
}
#endif
