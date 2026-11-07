#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "CanvasTypes.h"
#include "Engine/Canvas.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "HitProxies.h"
#include "InputKeyEventArgs.h"
#include "Misc/Paths.h"
#include "Persistence/ExpeditionSave.h"
#include "Presentation/ExpeditionBattleView.h"
#include "Presentation/ExpeditionHUD.h"
#include "Presentation/ExpeditionPlayerController.h"
#include "ExpeditionTestSupport.h"

namespace PhaseFocusTest
{
struct FOffscreenTarget final : FRenderTarget
{
    FIntPoint Size;
    explicit FOffscreenTarget(FIntPoint InSize) : Size(InSize) {}
    virtual FIntPoint GetSizeXY() const override { return Size; }
};
struct FNoHitProxies final : FHitProxyConsumer { virtual void AddHitProxy(HHitProxy*) override {} };

/**
 * The real input path in a transient world: key events enter UPlayerInput, the
 * controller's own bindings dispatch them on PlayerTick, and the real HUD draws each
 * frame into an offscreen canvas so menu focus sees exactly the controls a player sees.
 */
struct FHarness
{
    FAutomationTestBase& Test;
    UWorld* World = nullptr;
    AExpeditionPlayerController* Player = nullptr;
    AExpeditionBattleView* View = nullptr;
    AExpeditionHUD* Hud = nullptr;
    FString Directory;
    FOffscreenTarget Target{FIntPoint(1280, 720)};
    FNoHitProxies HitProxies;

    FHarness(FAutomationTestBase& InTest, const TCHAR* Name, const FExpeditionSaveData* Saved = nullptr) : Test(InTest)
    {
        Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation"), Name, FGuid::NewGuid().ToString());
        const FString Path = FPaths::Combine(Directory, TEXT("current.json"));
        FString Error;
        if (Saved && !Test.TestTrue(TEXT("Fixture checkpoint writes"), FExpeditionSave::Write(Path, *Saved, Error))) { return; }
        const auto Values = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false);
        World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
        if (!World) { return; }
        Player = World->SpawnActor<AExpeditionPlayerController>();
        if (!Player) { return; }
        Player->InitInputSystem();
        Player->DispatchBeginPlay();
        View = Player->GetBattleView();
        if (!View) { return; }
        View->ConfigureStorage(Path);
        View->DispatchBeginPlay();
        FActorSpawnParameters Spawn;
        Spawn.Owner = Player;
        Hud = World->SpawnActor<AExpeditionHUD>(Spawn);
        if (!Hud) { return; }
        // This uninitialized transient world skips PostInitializeComponents, which normally
        // makes the spawning controller the HUD's owner and the controller's HUD.
        Hud->PlayerOwner = Player;
        Player->MyHUD = Hud;
    }
    ~FHarness()
    {
        if (World) { World->DestroyWorld(false); }
        IFileManager::Get().DeleteDirectory(*Directory, false, true);
    }
    bool IsReady() const { return World && Player && View && Hud && Player->PlayerInput; }

    void Draw()
    {
        FCanvas Canvas(&Target, &HitProxies, FGameTime(), ERHIFeatureLevel::SM5);
        UCanvas* Surface = NewObject<UCanvas>(GetTransientPackage());
        Surface->Init(Target.Size.X, Target.Size.Y, nullptr, &Canvas);
        Hud->SetCanvas(Surface, Surface);
        Hud->DrawHUD();
        Hud->SetCanvas(nullptr, nullptr);
    }
    /** One game frame: controller (input), board, then the HUD. */
    void Frame(float DeltaSeconds = 0.05f)
    {
        Player->PlayerTick(DeltaSeconds);
        View->Tick(DeltaSeconds);
        Draw();
    }
    void Press(FKey Key)
    {
        Player->InputKey(FInputKeyEventArgs::CreateSimulated(Key, IE_Pressed, 1.0f));
        Frame();
        Player->InputKey(FInputKeyEventArgs::CreateSimulated(Key, IE_Released, 0.0f));
        Frame();
    }
    void Settle()
    {
        for (int32 Index = 0; Index < 600 && View->IsPresenting(); ++Index) { Frame(); }
        Frame();
    }
    /**
     * Win the current battle through view commands, leaving the battle-ending playback
     * pending and drawing frames throughout, as a player watching the victory would.
     */
    bool WinBattle()
    {
        for (int32 Step = 0; Step < 600 && View->InCombat(); ++Step)
        {
            View->SkipPresentation();
            Frame();
            const FCombatBattle& Battle = View->Rules();
            const FCombatantState* Active = Battle.Find(Battle.ActiveId());
            if (!Active) { return false; }
            if (Active->Side == ECombatSide::Enemy) { View->Tick(1.0f); continue; }
            const int32 Id = Active->Id;
            if (Battle.LegalTargets(Id).IsEmpty())
            {
                TArray<FIntPoint> Moves = Battle.LegalMoves(Id);
                auto Nearest = [&Battle](FIntPoint Cell)
                {
                    int32 Best = 1000;
                    for (const auto& Enemy : Battle.Snapshot().Encounter.Combatants)
                    { if (Enemy.Side == ECombatSide::Enemy && Enemy.HP > 0) { Best = FMath::Min(Best, ExpeditionTestSupport::Distance(Cell, Enemy.Cell)); } }
                    return Best;
                };
                Moves.Sort([&Nearest](FIntPoint A, FIntPoint B) { const int32 DA = Nearest(A), DB = Nearest(B); return DA == DB ? (A.Y == B.Y ? A.X < B.X : A.Y < B.Y) : DA < DB; });
                if (!Moves.IsEmpty()) { View->Execute({ECombatCommandType::Move, Id, Moves[0]}); }
            }
            if (!View->InCombat()) { break; }
            const TArray<int32> Targets = View->Rules().LegalTargets(Id);
            if (!Targets.IsEmpty()) { View->Execute({ECombatCommandType::Attack, Id, {}, Targets[0]}); }
            if (View->InCombat() && View->Rules().ActiveId() == Id) { View->Execute({ECombatCommandType::EndTurn, Id}); }
        }
        return !View->InCombat();
    }
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPhaseFocusTest, "Project.Expedition.Presentation.PhaseChangesFocusPrimaryChoice",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FPhaseFocusTest::RunTest(const FString& Parameters)
{
    PhaseFocusTest::FHarness Game(*this, TEXT("PhaseFocus"));
    if (!TestTrue(TEXT("Input, board and HUD are ready"), Game.IsReady())) { return false; }
    AExpeditionPlayerController* Player = Game.Player;
    AExpeditionBattleView* View = Game.View;

    Game.Frame();
    if (!TestTrue(TEXT("The HUD registers the startup controls"), !Game.Hud->MenuButtons().IsEmpty())) { return false; }
    TestEqual(TEXT("Startup builder focuses Begin"), Player->MenuFocus(), FName(TEXT("New")));
    Game.Press(EKeys::Enter);
    if (!TestTrue(TEXT("Enter on Begin starts the expedition"), View->HasRun() && View->Progress().Phase == EExpeditionPhase::Route)) { return false; }
    Game.Settle();
    TestEqual(TEXT("New run focuses the first route"), Player->MenuFocus(), FName(TEXT("Route0")));
    Game.Press(EKeys::Enter);
    if (!TestTrue(TEXT("Enter on the first route starts its encounter"), View->InCombat())) { return false; }
    TestEqual(TEXT("Combat holds no menu focus"), Player->MenuFocus(), FName());

    if (!TestTrue(TEXT("The party wins the first encounter"), Game.WinBattle())) { return false; }
    TestEqual(TEXT("Victory reaches the reward"), View->Progress().Phase, EExpeditionPhase::Reward);
    TestTrue(TEXT("The victory is still being presented"), View->IsPresenting());
    // Open and close Help while the victory plays: Help must not keep focus afterwards.
    Game.Press(EKeys::Slash);
    TestEqual(TEXT("Help focuses its own Back"), Player->MenuFocus(), FName(TEXT("Close")));
    Game.Press(EKeys::Escape);
    Game.Settle();
    TestFalse(TEXT("Help closed"), Player->ShowingHelp());
    TestEqual(TEXT("Reward focuses Claim"), Player->MenuFocus(), FName(TEXT("Claim")));

    Game.Press(EKeys::One);
    Game.Press(EKeys::L);
    if (!TestEqual(TEXT("1 then L claims the reward"), View->Progress().Phase, EExpeditionPhase::Recovery)) { return false; }
    TestEqual(TEXT("Recovery focuses Rest by the fire"), Player->MenuFocus(), FName(TEXT("Camp")));
    Game.Press(EKeys::Slash);
    TestEqual(TEXT("Help over recovery focuses Back"), Player->MenuFocus(), FName(TEXT("Close")));
    Game.Press(EKeys::Escape);
    TestEqual(TEXT("Closing Help restores the recovery choice"), Player->MenuFocus(), FName(TEXT("Camp")));
    Game.Press(EKeys::H);
    TestEqual(TEXT("Records over recovery focuses Close"), Player->MenuFocus(), FName(TEXT("Close")));
    Game.Press(EKeys::H);
    TestEqual(TEXT("Closing Records restores the recovery choice"), Player->MenuFocus(), FName(TEXT("Camp")));
    Game.Press(EKeys::Enter);
    TestFalse(TEXT("Enter on recovery does not open Help"), Player->ShowingHelp());
    if (!TestEqual(TEXT("Enter on recovery takes the rest"), View->Progress().Phase, EExpeditionPhase::Route)) { return false; }
    TestTrue(TEXT("The rest was taken, not forfeited"), View->Progress().bCampUsed);

    Game.Settle();
    TestEqual(TEXT("Next route focuses the first route"), Player->MenuFocus(), FName(TEXT("Route0")));
    Game.Press(EKeys::Enter);
    TestFalse(TEXT("Enter on the route does not open Help"), Player->ShowingHelp());
    TestTrue(TEXT("Enter on the route starts the second encounter"), View->InCombat());
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEndingFocusTest, "Project.Expedition.Presentation.EndingAndResumeFocusPrimaryChoice",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FEndingFocusTest::RunTest(const FString& Parameters)
{
    {
        // A defeat ends the expedition with its final blow still playing.
        PhaseFocusTest::FHarness Game(*this, TEXT("EndingFocus"));
        if (!TestTrue(TEXT("Input, board and HUD are ready"), Game.IsReady())) { return false; }
        Game.Frame();
        Game.Press(EKeys::Enter);
        Game.Settle();
        Game.Press(EKeys::Enter);
        if (!TestTrue(TEXT("Enter, Enter starts the first encounter"), Game.View->InCombat())) { return false; }
        for (int32 Step = 0; Step < 600 && Game.View->InCombat(); ++Step)
        {
            Game.View->SkipPresentation();
            Game.Frame();
            const FCombatantState* Active = Game.View->Rules().Find(Game.View->Rules().ActiveId());
            if (!Active) { break; }
            if (Active->Side == ECombatSide::Party) { Game.Press(EKeys::SpaceBar); }
            else { Game.View->Tick(1.0f); }
        }
        TestEqual(TEXT("A passive party is defeated"), Game.View->Progress().Phase, EExpeditionPhase::Defeat);
        Game.Settle();
        TestEqual(TEXT("Ending focuses Choose next party"), Game.Player->MenuFocus(), FName(TEXT("Builder")));
        Game.Press(EKeys::Enter);
        TestFalse(TEXT("Enter on the ending does not open Help"), Game.Player->ShowingHelp());
        TestTrue(TEXT("Enter on the ending opens the next-party builder"), Game.Player->ShowingBuilder());
        TestEqual(TEXT("Next-party builder focuses Begin"), Game.Player->MenuFocus(), FName(TEXT("New")));
    }
    // Resume from the startup builder straight into each between-battle screen.
    FString Error;
    FExpeditionRun Run;
    Run.NewRun(20261030, Error, TEXT("phase-focus-resume"));
    struct FResumeCase { const TCHAR* Name; FExpeditionSnapshot Checkpoint; FName Primary; };
    TArray<FResumeCase> Cases;
    Cases.Add({TEXT("route"), Run.Snapshot(), TEXT("Route0")});
    for (int32 Seed = 1; Seed < 60 && Cases.Num() < 3; ++Seed)
    {
        FExpeditionRun Won;
        Won.NewRun(Seed, Error, FString::Printf(TEXT("phase-focus-%d"), Seed));
        Won.ChooseRoute(Won.Progress().Offers[0].Id, Error);
        if (!ExpeditionTestSupport::FinishCombat(Won) || Won.Progress().Phase != EExpeditionPhase::Reward) { continue; }
        Cases.Add({TEXT("reward"), Won.Snapshot(), TEXT("Claim")});
        for (const FCombatantState& Unit : Won.Battle().Snapshot().Encounter.Combatants)
        { if (Unit.Side == ECombatSide::Party && Unit.Life == ECombatLife::Conscious) { Won.ClaimReward(Unit.Id, Error); break; } }
        if (Won.Progress().Phase == EExpeditionPhase::Recovery) { Cases.Add({TEXT("recovery"), Won.Snapshot(), TEXT("Camp")}); }
    }
    TestEqual(TEXT("Route, reward and recovery fixtures exist"), Cases.Num(), 3);
    for (const FResumeCase& Case : Cases)
    {
        FExpeditionSaveData Data;
        Data.Current = Case.Checkpoint;
        PhaseFocusTest::FHarness Game(*this, TEXT("ResumeFocus"), &Data);
        if (!TestTrue(TEXT("Resume harness is ready"), Game.IsReady())) { continue; }
        Game.Frame();
        Game.Press(EKeys::O);
        TestTrue(FString::Printf(TEXT("O resumes the %s checkpoint"), Case.Name), Game.View->HasRun());
        Game.Settle();
        TestEqual(FString::Printf(TEXT("Resumed %s focuses its primary choice"), Case.Name), Game.Player->MenuFocus(), Case.Primary);
        Game.Press(EKeys::Enter);
        TestFalse(FString::Printf(TEXT("Enter on resumed %s does not open Help"), Case.Name), Game.Player->ShowingHelp());
    }
    return !HasAnyErrors();
}
#endif
