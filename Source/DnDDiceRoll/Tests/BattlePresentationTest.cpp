#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Presentation/ExpeditionBattleView.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnemyInputStarvationTest, "Project.Expedition.Presentation.RejectedInputCannotStallEnemy",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FEnemyInputStarvationTest::RunTest(const FString& Parameters)
{
    const UWorld::InitializationValues Values = UWorld::InitializationValues()
        .AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
    if (!TestNotNull(TEXT("Transient game world"), World)) { return false; }
    const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation/Presentation"), FGuid::NewGuid().ToString());
    AExpeditionBattleView* View = World->SpawnActor<AExpeditionBattleView>();
    if (TestNotNull(TEXT("Runtime board adapter"), View))
    {
        View->ConfigureStorage(FPaths::Combine(Directory, TEXT("current.json")));
        View->DispatchBeginPlay();
        View->NewRun();
        View->ChooseRoute(0);
        for (int32 Turn = 0; Turn < 4; ++Turn)
        {
            const auto* Unit = View->Rules().Find(View->Rules().ActiveId());
            if (!Unit || Unit->Side == ECombatSide::Enemy) { break; }
            View->Execute({ECombatCommandType::EndTurn, Unit->Id});
        }
        const int32 FirstEnemy = View->Rules().ActiveId();
        const FCombatantState* Active = View->Rules().Find(FirstEnemy);
        if (TestNotNull(TEXT("Battle initialized"), Active))
        {
            TestTrue(TEXT("Generated battle reaches enemy initiative"), Active->Side == ECombatSide::Enemy);
            // Simulate repeated rejected player input between presentation frames.
            for (int32 Frame = 0; Frame < 3; ++Frame)
            {
                View->Execute({ECombatCommandType::EndTurn, 1});
                View->Tick(0.4f);
            }
            TestNotEqual(TEXT("Enemy completes within its normal presentation delay despite rejected input"), View->Rules().ActiveId(), FirstEnemy);
        }
    }
    World->DestroyWorld(false);
    IFileManager::Get().DeleteDirectory(*Directory, false, true);
    return !HasAnyErrors();
}
#endif
