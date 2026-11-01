#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "Components/PrimitiveComponent.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Presentation/ExpeditionBattleView.h"
#include "Presentation/ExpeditionPlayerController.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOverlayInputTest, "Project.Expedition.Presentation.OverlaysBlockGameplayShortcuts",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FOverlayInputTest::RunTest(const FString& Parameters)
{
    const UWorld::InitializationValues Values = UWorld::InitializationValues()
        .AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
    if (!TestNotNull(TEXT("Transient input world"), World)) { return false; }
    const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation/OverlayInput"), FGuid::NewGuid().ToString());
    AExpeditionPlayerController* Player = World->SpawnActor<AExpeditionPlayerController>();
    if (TestNotNull(TEXT("Player input adapter"), Player))
    {
        Player->DispatchBeginPlay();
        AExpeditionBattleView* View = Player->GetBattleView();
        if (TestNotNull(TEXT("Player owns a battle view"), View))
        {
            View->ConfigureStorage(FPaths::Combine(Directory, TEXT("current.json")));
            View->DispatchBeginPlay();
            Player->HandleButton(TEXT("New"));
            TestEqual(TEXT("New run offers routes"), View->Progress().Phase, EExpeditionPhase::Route);
            Player->HandleButton(TEXT("Help"));
            Player->HandleButton(TEXT("Route0"));
            TestEqual(TEXT("Help blocks the route shortcut"), View->Progress().Phase, EExpeditionPhase::Route);
            Player->HandleButton(TEXT("Close"));
            Player->HandleButton(TEXT("Route0"));
            TestTrue(TEXT("Closing help restores route input"), View->InCombat());
            for (int32 Turn = 0; Turn < 20; ++Turn)
            {
                const FCombatantState* Active = View->Rules().Find(View->Rules().ActiveId());
                if (!Active || Active->Side == ECombatSide::Party) { break; }
                View->Tick(1.0f);
                View->SkipPresentation();
            }
            const FCombatantState* Active = View->Rules().Find(View->Rules().ActiveId());
            TestTrue(TEXT("A party turn is ready"), Active && Active->Side == ECombatSide::Party);
            const FCombatSnapshot Before = View->Rules().Snapshot();
            auto Unchanged = [&]() { return FCombatSnapshot::StaticStruct()->CompareScriptStruct(&Before, &View->Rules().Snapshot(), 0); };
            Player->HandleButton(TEXT("Help"));
            Player->HandleButton(TEXT("EndTurn"));
            TestTrue(TEXT("Help blocks the end-turn shortcut"), Unchanged());
            Player->HandleButton(TEXT("Close"));
            Player->HandleButton(TEXT("Records"));
            Player->HandleButton(TEXT("EndTurn"));
            TestTrue(TEXT("Records blocks the end-turn shortcut"), Unchanged());
            Player->HandleButton(TEXT("Close"));
            Player->HandleButton(TEXT("EndTurn"));
            TestFalse(TEXT("Closing records restores end-turn input"), Unchanged());
        }
    }
    World->DestroyWorld(false);
    IFileManager::Get().DeleteDirectory(*Directory, false, true);
    return !HasAnyErrors();
}

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
        View->NewRun();
        TestFalse(TEXT("Startup new-run input cannot replace saved progress"),View->HasRun());
        TestTrue(TEXT("Player is directed to resume"),!View->Messages().IsEmpty() && View->Messages().Last().Contains(TEXT("Press O")));
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

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
bool DisplayMatchesRules(const AExpeditionBattleView& View)
{
    for (const FCombatantState& Unit : View.Rules().Snapshot().Encounter.Combatants)
    {
        const FUnitDisplay* Display = View.Playback().Unit(Unit.Id);
        if (!Display || Display->Position != FVector2D(Unit.Cell.X, Unit.Cell.Y) || Display->HP != Unit.HP || Display->Life != Unit.Life) { return false; }
    }
    return true;
}
void Settle(AExpeditionBattleView& View)
{
    for (int32 Frame = 0; Frame < 400 && View.IsPresenting(); ++Frame) { View.Tick(0.05f); }
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPlaybackConvergenceTest, "Project.Expedition.Presentation.PlaybackConvergesOnCommittedState",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FPlaybackConvergenceTest::RunTest(const FString& Parameters)
{
    const UWorld::InitializationValues Values = UWorld::InitializationValues()
        .AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
    if (!TestNotNull(TEXT("Transient game world"), World)) { return false; }
    const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation/Playback"), FGuid::NewGuid().ToString());
    AExpeditionBattleView* View = World->SpawnActor<AExpeditionBattleView>();
    if (TestNotNull(TEXT("Runtime board adapter"), View))
    {
        View->ConfigureStorage(FPaths::Combine(Directory, TEXT("current.json")));
        View->DispatchBeginPlay();
        View->NewRun();
        View->ChooseRoute(0);
        TestTrue(TEXT("A fresh board shows the committed state"), DisplayMatchesRules(*View));
        bool bMoved = false, bEnemyShown = false;
        for (int32 Turn = 0; Turn < 40 && View->InCombat() && !(bMoved && bEnemyShown); ++Turn)
        {
            const FCombatantState* Active = View->Rules().Find(View->Rules().ActiveId());
            if (!Active) { break; }
            if (Active->Side == ECombatSide::Enemy)
            {
                const int32 Enemy = Active->Id;
                for (int32 Frame = 0; Frame < 40 && View->Rules().ActiveId() == Enemy; ++Frame) { View->Tick(0.1f); }
                TestNotEqual(TEXT("Enemy turn completes"), View->Rules().ActiveId(), Enemy);
                if (View->IsPresenting())
                {
                    bEnemyShown = true;
                    const float PendingAge = View->Playback().Banner().Age;
                    View->ToggleRecords();
                    View->Tick(0.2f);
                    TestEqual(TEXT("Records pauses the pending enemy presentation"), View->Playback().Banner().Age, PendingAge);
                    TestTrue(TEXT("Records retains pending enemy steps"), View->IsPresenting());
                    View->ToggleRecords();
                    View->Tick(0.05f);
                    TestTrue(TEXT("Closing records continues the same playback"), View->Playback().Banner().Age > PendingAge);
                    View->SkipPresentation();
                    TestFalse(TEXT("Skip finishes the enemy's steps"), View->IsPresenting());
                    TestTrue(TEXT("Skipping converges on the committed state"), DisplayMatchesRules(*View));
                }
                continue;
            }
            const TArray<FIntPoint> Moves = View->Rules().LegalMoves(Active->Id);
            if (!bMoved && !Moves.IsEmpty())
            {
                const FVector2D Start(Active->Cell.X, Active->Cell.Y);
                const int32 Id = Active->Id;
                View->Execute({ECombatCommandType::Move, Id, Moves.Last()});
                TestTrue(TEXT("Accepted move is presented over time"), View->IsPresenting());
                TestEqual(TEXT("Display starts where the unit was"), View->Playback().Unit(Id)->Position, Start);
                const FCombatSnapshot Committed = View->Rules().Snapshot();
                View->Execute({ECombatCommandType::Move, Id, FIntPoint(-5, -5)});
                TestTrue(TEXT("Rejected input during playback leaves rules unchanged"),
                    FCombatSnapshot::StaticStruct()->CompareScriptStruct(&Committed, &View->Rules().Snapshot(), 0));
                TestTrue(TEXT("Rejected input does not skip the animation"), View->IsPresenting());
                View->ClaimReward(Id);
                TestTrue(TEXT("Rejected reward does not skip the animation"), View->IsPresenting());
                TestTrue(TEXT("Rejected reward preserves the committed state"),
                    FCombatSnapshot::StaticStruct()->CompareScriptStruct(&Committed, &View->Rules().Snapshot(), 0));
                View->Tick(0.05f);
                TestFalse(TEXT("The display is still on its way"), DisplayMatchesRules(*View));
                Settle(*View);
                TestFalse(TEXT("Ticking finishes playback"), View->IsPresenting());
                TestTrue(TEXT("Playback ends on the committed state"), DisplayMatchesRules(*View));
                bMoved = true;
            }
            View->Execute({ECombatCommandType::EndTurn, Active->Id});
            Settle(*View);
        }
        TestTrue(TEXT("A party move was presented"), bMoved);
        TestTrue(TEXT("An enemy turn was presented"), bEnemyShown);
        // Resume mid-presentation replaces the show with the saved checkpoint.
        View->Save();
        const FCombatantState* Active = View->Rules().Find(View->Rules().ActiveId());
        if (Active && Active->Side == ECombatSide::Party && !View->Rules().LegalMoves(Active->Id).IsEmpty())
        {
            View->Execute({ECombatCommandType::Move, Active->Id, View->Rules().LegalMoves(Active->Id)[0]});
        }
        View->Resume();
        TestFalse(TEXT("Resume clears pending playback"), View->IsPresenting());
        TestTrue(TEXT("Resume shows the checkpoint"), DisplayMatchesRules(*View));
    }
    World->DestroyWorld(false);
    IFileManager::Get().DeleteDirectory(*Directory, false, true);
    return !HasAnyErrors();
}
#endif

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCorpseVisibilityTest, "Project.Expedition.Presentation.OccupiedCorpseCellsStayVisible",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FCorpseVisibilityTest::RunTest(const FString& Parameters)
{
    FString Error;
    FExpeditionRun Run;
    Run.NewRun(1, Error, TEXT("corpse-visibility"));
    Run.ChooseRoute(Run.Progress().Offers[0].Id, Error);
    TestTrue(TEXT("Real commands finish the encounter"), ExpeditionTestSupport::FinishCombat(Run));
    TestEqual(TEXT("Winning fixture"), Run.Progress().Phase, EExpeditionPhase::Reward);
    const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation/CorpseVisibility"), FGuid::NewGuid().ToString());
    const FString Path = FPaths::Combine(Directory, TEXT("current.json"));
    FExpeditionSaveData Data;
    Data.Current = Run.Snapshot();
    TestTrue(TEXT("Checkpoint writes"), FExpeditionSave::Write(Path, Data, Error));
    const UWorld::InitializationValues Values = UWorld::InitializationValues()
        .AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
    if (!TestNotNull(TEXT("Transient corpse world"), World)) { return false; }
    AExpeditionBattleView* View = World->SpawnActor<AExpeditionBattleView>();
    if (TestNotNull(TEXT("Corpse view"), View))
    {
        View->ConfigureStorage(Path);
        View->DispatchBeginPlay();
        View->Resume();
        View->Tick(0.1f);
        TArray<USceneComponent*> Components;
        View->GetComponents(Components);
        int32 Corpses = 0;
        for (const FCombatantState& Unit : View->Rules().Snapshot().Encounter.Combatants)
        {
            if (Unit.Side != ECombatSide::Enemy || Unit.Life != ECombatLife::Dead) { continue; }
            ++Corpses;
            const FVector Cell = View->CellPosition(Unit.Cell);
            bool bVisibleBody = false;
            for (USceneComponent* Component : Components)
            {
                if (Component->GetAttachParent() != View->GetRootComponent() || Component->GetNumChildrenComponents() < 4 ||
                    !FVector2D(Component->GetRelativeLocation()).Equals(FVector2D(Cell), 0.1f)) { continue; }
                TArray<USceneComponent*> Children;
                Component->GetChildrenComponents(true, Children);
                for (USceneComponent* Child : Children)
                {
                    const UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(Child);
                    if (Primitive && Primitive->IsVisible() && Primitive->Bounds.GetBox().Max.Z > Cell.Z + 5.0f) { bVisibleBody = true; }
                }
            }
            TestTrue(FString::Printf(TEXT("Occupied corpse cell for %s has a visible body above the board"), *Unit.Name), bVisibleBody);
        }
        TestTrue(TEXT("Fixture contains slain enemies"), Corpses > 0);
    }
    World->DestroyWorld(false);
    IFileManager::Get().DeleteDirectory(*Directory, false, true);
    return !HasAnyErrors();
}
#endif
