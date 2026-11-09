#pragma once
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "CanvasTypes.h"
#include "Engine/Canvas.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "HitProxies.h"
#include "InputKeyEventArgs.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Persistence/ExpeditionSave.h"
#include "RenderingThread.h"
#include "UObject/GarbageCollection.h"
#include "Presentation/ExpeditionBattleView.h"
#include "Presentation/ExpeditionHUD.h"
#include "Presentation/ExpeditionPlayerController.h"
#include "ExpeditionTestSupport.h"

/** Drives the real player-controller input path and HUD draw headlessly (#128, #132). */
namespace ExpeditionInputHarness
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
    /** The checkpoint file this game commits to. */
    FString Path;
    FOffscreenTarget Target{FIntPoint(1280, 720)};
    FNoHitProxies HitProxies;

    /** Saved writes a valid checkpoint first; RawCheckpoint writes these exact contents instead (for unreadable files). */
    FHarness(FAutomationTestBase& InTest, const TCHAR* Name, const FExpeditionSaveData* Saved = nullptr, const TCHAR* RawCheckpoint = nullptr) : Test(InTest)
    {
        Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation"), Name, FGuid::NewGuid().ToString());
        Path = FPaths::Combine(Directory, TEXT("current.json"));
        FString Error;
        if (Saved && !Test.TestTrue(TEXT("Fixture checkpoint writes"), FExpeditionSave::Write(Path, *Saved, Error))) { return; }
        if (RawCheckpoint && !Test.TestTrue(TEXT("Raw checkpoint writes"), FFileHelper::SaveStringToFile(RawCheckpoint, *Path))) { return; }
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
        if (World)
        {
            // Free the world's render resources now, not at the next GC: lingering harness worlds
            // push RHI reserved memory past its budget later in the suite (the same fix as #141's tests).
            World->DestroyWorld(false);
            FlushRenderingCommands();
            CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
        }
        IFileManager::Get().DeleteDirectory(*Directory, false, true);
    }
    /** The checkpoint file's exact bytes; empty when it does not exist. */
    TArray<uint8> CheckpointBytes() const
    {
        TArray<uint8> Bytes;
        FFileHelper::LoadFileToArray(Bytes, *Path, FILEREAD_Silent);
        return Bytes;
    }
    /** The live run as the view shows it, in checkpoint form. */
    FExpeditionSnapshot Live() const { return {View->Progress(), View->Rules().Snapshot()}; }
    /** Let enemies act until the party has the turn, then let playback finish. */
    void PartyTurn()
    {
        for (int32 Step = 0; Step < 200 && View->InCombat(); ++Step)
        {
            const FCombatantState* Active = View->Rules().Find(View->Rules().ActiveId());
            if (Active && Active->Side == ECombatSide::Party) { break; }
            View->SkipPresentation();
            View->Tick(1.0f);
            Frame();
        }
        Settle();
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
    /** Replays party commands found by ExpeditionTestSupport::FinishCombat, letting enemies act between them. */
    bool Replay(const TArray<FCombatCommand>& Commands)
    {
        for (const FCombatCommand& Command : Commands)
        {
            for (int32 Step = 0; Step < 200 && View->InCombat(); ++Step)
            {
                const FCombatantState* Active = View->Rules().Find(View->Rules().ActiveId());
                if (Active && Active->Side == ECombatSide::Party) { break; }
                View->SkipPresentation();
                View->Tick(1.0f);
                Frame();
            }
            if (!View->InCombat()) { break; }
            View->Execute(Command);
            Frame();
        }
        return !View->InCombat();
    }
    /** Help and Records each take focus while open and give the screen its focus back when closed. */
    void CheckOverlays(const FString& Screen, FName Primary)
    {
        Test.TestEqual(Screen + TEXT(" focuses its primary choice"), Player->MenuFocus(), Primary);
        Press(EKeys::Slash);
        Test.TestTrue(Screen + TEXT(": ? opens Help"), Player->ShowingHelp());
        Test.TestEqual(Screen + TEXT(": Help focuses its Close"), Player->MenuFocus(), FName(TEXT("Close")));
        Press(EKeys::Escape);
        Test.TestFalse(Screen + TEXT(": Esc closes Help"), Player->ShowingHelp());
        Test.TestEqual(Screen + TEXT(": closing Help restores the primary choice"), Player->MenuFocus(), Primary);
        Press(EKeys::H);
        Test.TestTrue(Screen + TEXT(": H opens Records"), View->ShowingRecords());
        Test.TestEqual(Screen + TEXT(": Records focuses its Close"), Player->MenuFocus(), FName(TEXT("Close")));
        Press(EKeys::H);
        Test.TestFalse(Screen + TEXT(": H closes Records"), View->ShowingRecords());
        Test.TestEqual(Screen + TEXT(": closing Records restores the primary choice"), Player->MenuFocus(), Primary);
    }
};

}
#endif
