#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/PackageName.h"
#include "Engine/World.h"
#include "Engine/Level.h"
#include "Engine/StaticMeshActor.h"
#include "GameFramework/PlayerStart.h"
#include "Components/StaticMeshComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStarterSandboxTest, "Project.Starter.SandboxAssets",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FStarterSandboxTest::RunTest(const FString& Parameters)
{
    FString StartupMap;
    FString DefaultMap;
    GConfig->GetString(TEXT("/Script/EngineSettings.GameMapsSettings"), TEXT("EditorStartupMap"), StartupMap, GEngineIni);
    GConfig->GetString(TEXT("/Script/EngineSettings.GameMapsSettings"), TEXT("GameDefaultMap"), DefaultMap, GEngineIni);
    if (!TestFalse(TEXT("Editor startup map is configured"), StartupMap.IsEmpty()) ||
        !TestTrue(TEXT("Packaged default map exists"), FPackageName::DoesPackageExist(DefaultMap)))
    {
        return false;
    }
    UWorld* World = LoadObject<UWorld>(nullptr, *StartupMap);
    if (!TestNotNull(TEXT("Startup map loads"), World) || !TestNotNull(TEXT("Persistent level exists"), World->PersistentLevel.Get()))
    {
        return false;
    }
    int32 Floors = 0;
    int32 Starts = 0;
    for (AActor* Actor : World->PersistentLevel->Actors)
    {
        if (!Actor) { continue; }
        Starts += Actor->IsA<APlayerStart>() ? 1 : 0;
        if (Actor->GetActorLabel() == TEXT("SandboxFloor"))
        {
            ++Floors;
            AStaticMeshActor* Floor = Cast<AStaticMeshActor>(Actor);
            if (TestNotNull(TEXT("Floor is a static mesh actor"), Floor))
            {
                UStaticMeshComponent* Mesh = Floor->GetStaticMeshComponent();
                TestNotNull(TEXT("Floor has a mesh"), Mesh->GetStaticMesh().Get());
                TestTrue(TEXT("Floor has collision"), Mesh->GetCollisionEnabled() != ECollisionEnabled::NoCollision);
            }
        }
    }
    TestEqual(TEXT("Exactly one sandbox floor"), Floors, 1);
    TestTrue(TEXT("At least one player start"), Starts > 0);
    return !HasAnyErrors();
}
#endif
