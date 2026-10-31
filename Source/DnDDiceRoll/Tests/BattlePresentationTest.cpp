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

#if WITH_DEV_AUTOMATION_TESTS
#include "ExpeditionTestSupport.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRecordPagingTest, "Project.Expedition.Presentation.StartupResumeAndAllHistoryPages",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRecordPagingTest::RunTest(const FString& Parameters)
{
    const FString Directory=FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("Automation/HistoryView"),FGuid::NewGuid().ToString());
    const FString Path=FPaths::Combine(Directory,TEXT("current.json"));
    FString Error; FExpeditionRun Live, Ended;
    Live.NewRun(42,Error,TEXT("history-current")); Ended.NewRun(7,Error,TEXT("history-ended"));
    Ended.ChooseRoute(Ended.Progress().Offers[1].Id,Error); ExpeditionTestSupport::FinishCombat(Ended,false);
    FExpeditionSaveData Data; Data.Current=Live.Snapshot();
    for(int32 I=0;I<10;++I){auto Ending=FExpeditionSave::Ending(Ended);Ending.RunId=FString::Printf(TEXT("history-%d"),I);Data.Records.Add(Ending);}
    TestTrue(TEXT("Real history fixture writes"),FExpeditionSave::Write(Path,Data,Error));
    const UWorld::InitializationValues Values=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false);
    UWorld* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Values);
    if(!TestNotNull(TEXT("Transient world"),World)){return false;}
    auto* View=World->SpawnActor<AExpeditionBattleView>();
    if(TestNotNull(TEXT("History view"),View))
    {
        View->ConfigureStorage(Path);View->DispatchBeginPlay();
        TestFalse(TEXT("Startup waits for a player choice"),View->HasRun());
        View->ToggleRecords();TestTrue(TEXT("Records accessible before choosing a run"),View->ShowingRecords());
        TestEqual(TEXT("All records available"),View->Records().Num(),10);TestEqual(TEXT("Newest page first"),View->RecordOffset(),0);
        View->PageRecords(1);TestEqual(TEXT("Older page"),View->RecordOffset(),4);
        View->PageRecords(1);View->PageRecords(1);TestEqual(TEXT("Oldest partial page bounded"),View->RecordOffset(),8);
        View->PageRecords(-1);View->PageRecords(-1);View->PageRecords(-1);TestEqual(TEXT("Return to newest bounded"),View->RecordOffset(),0);
        View->Resume();TestTrue(TEXT("Resume succeeds and closes history"),View->HasRun() && !View->ShowingRecords());
        const FExpeditionSnapshot After={View->Progress(),View->Rules().Snapshot()};
        TestTrue(TEXT("Boot/history never overwrote the saved expedition"),FExpeditionSnapshot::StaticStruct()->CompareScriptStruct(&Data.Current,&After,0));
        TestTrue(TEXT("Resume rebuilds presentation"),View->PresentationRevision()>0);
    }
    World->DestroyWorld(false);IFileManager::Get().DeleteDirectory(*Directory,false,true);
    return !HasAnyErrors();
}
#endif
