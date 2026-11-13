#if WITH_DEV_AUTOMATION_TESTS
#include "ExpeditionInputHarness.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Persistence/ExpeditionSave.h"

// Abandon run from the start screen (#224): a separate command with a confirmation that defaults to keeping the run.
namespace AbandonRunTest
{
/** A finished run's record, valid for the records format: a defeat after two victories, no survivors. */
FExpeditionEnding PastDefeat()
{
    FExpeditionEnding Record;
    Record.RunId = TEXT("abandon-test-past-run");
    Record.Seed = 7;
    Record.Outcome = EExpeditionPhase::Defeat;
    Record.Victories = 2;
    Record.Summary = TEXT("Party lost | 2/5 victories | Survivors: none");
    return Record;
}

/** An unfinished checkpoint (fight 1 under way) with one past record, written at Path. */
bool WriteUnfinished(FAutomationTestBase& Test, const FString& Path, FExpeditionSnapshot& OutSnapshot)
{
    FExpeditionRun Run; FString Error;
    if (!Test.TestTrue(TEXT("A run starts"), Run.NewRun(42, Error, TEXT("abandon-test-run")) && Run.ChooseRoute(Run.Progress().Offers[0].Id, Error))) { return false; }
    FExpeditionSaveData Data;
    Data.Current = Run.Snapshot();
    Data.Records.Add(PastDefeat());
    OutSnapshot = Data.Current;
    return Test.TestTrue(TEXT("The unfinished checkpoint writes: ") + Error, FExpeditionSave::Write(Path, Data, Error));
}

TArray<uint8> Bytes(const FString& Path) { TArray<uint8> Out; FFileHelper::LoadFileToArray(Out, *Path, FILEREAD_Silent); return Out; }

/** The id of the drawn button whose box centre hits Id (the pointer's path: ButtonAt, then HandleButton). */
bool PointerFinds(const AExpeditionHUD& Hud, FName Id)
{
    for (const FBox2D& Box : Hud.ButtonRects())
    {
        FName Hit;
        if (Hud.ButtonAt(Box.GetCenter(), Hit) && Hit == Id) { return true; }
    }
    return false;
}
}
using namespace AbandonRunTest;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAbandonRunSessionTest, "Project.Expedition.Persistence.AbandonRunThenNewRun",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FAbandonRunSessionTest::RunTest(const FString& Parameters)
{
    const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation/AbandonRun"), FGuid::NewGuid().ToString());
    const FString Path = FPaths::Combine(Directory, TEXT("current-v1.json"));
    FExpeditionSnapshot Saved;
    if (!WriteUnfinished(*this, Path, Saved)) { return false; }
    const TArray<uint8> Original = Bytes(Path);
    FString Error;
    {
        // Nothing to abandon without a checkpoint, and never with a live run.
        FExpeditionSession Empty(FPaths::Combine(Directory, TEXT("none.json")));
        TestFalse(TEXT("No checkpoint: nothing to abandon"), Empty.Abandon(Error));
        FExpeditionSession Live(Path);
        TestTrue(TEXT("Resume the run"), Live.Resume(Error));
        TestFalse(TEXT("A live run is not abandoned"), Live.Abandon(Error));
        TestTrue(TEXT("Refusals change nothing"), Bytes(Path) == Original && !FPaths::FileExists(Live.AbandonMarkerPath()));
    }
    {
        // A failed write changes nothing and says so (the marker's temporary path is a directory, so the write fails).
        FExpeditionSession Failing(Path);
        ExpeditionTestSupport::FailNextCheckpointWrite(*this, Failing.AbandonMarkerPath());
        TestFalse(TEXT("A failed write does not abandon"), Failing.Abandon(Error));
        TestTrue(TEXT("It says the run is unchanged: ") + Error, Error.Contains(TEXT("unchanged")));
        ExpeditionTestSupport::AllowCheckpointWrites(Failing.AbandonMarkerPath());
        TestTrue(TEXT("The checkpoint bytes are unchanged after the failure"), Bytes(Path) == Original);
        TestTrue(TEXT("No marker after the failure"), !FPaths::FileExists(Failing.AbandonMarkerPath()));
        TestTrue(TEXT("The run is still unfinished"), Failing.HasUnfinishedCheckpoint());
        TestTrue(TEXT("It still resumes"), Failing.Resume(Error) && ExpeditionTestSupport::SameRun(Saved, Failing.Run().Snapshot()));
    }
    FExpeditionSession Session(Path);
    TestTrue(TEXT("The checkpoint is unfinished"), Session.HasUnfinishedCheckpoint());
    TestFalse(TEXT("New Run still refuses it before an abandon"), Session.NewRun(43, Error));
    TestTrue(TEXT("Abandon succeeds: ") + Error, Session.Abandon(Error));
    TestTrue(TEXT("Abandon never writes the checkpoint"), Bytes(Path) == Original);
    TestFalse(TEXT("No unfinished checkpoint after the abandon"), Session.HasUnfinishedCheckpoint());
    TestFalse(TEXT("Abandon cannot repeat"), Session.Abandon(Error));
    {
        FExpeditionSession Other(Path);
        TestFalse(TEXT("The abandoned run cannot be resumed"), Other.Resume(Error));
        TestTrue(TEXT("Resume names the abandon: ") + Error, Error.Contains(TEXT("abandoned")));
        TestFalse(TEXT("A later start screen sees no unfinished checkpoint"), Other.HasUnfinishedCheckpoint());
    }
    TestTrue(TEXT("New Run then starts a fresh run: ") + Error, Session.NewRun(43, Error, TEXT("abandon-test-next")));
    TestTrue(TEXT("The fresh run is at its first route"), Session.HasRun() && Session.Run().Progress().Phase == EExpeditionPhase::Route && Session.Run().Progress().EncountersCompleted == 0);
    TestEqual(TEXT("It is the new run"), Session.Run().Progress().RunId, FString(TEXT("abandon-test-next")));
    TestFalse(TEXT("The marker is gone"), FPaths::FileExists(Session.AbandonMarkerPath()));
    TArray<FExpeditionEnding> Records;
    TestTrue(TEXT("Records read"), Session.ReadRecords(Records, Error));
    TestTrue(TEXT("The finished run's record is kept, and the abandoned run is not recorded"), Records.Num() == 1 && Records[0].RunId == PastDefeat().RunId);
    TestTrue(TEXT("The new run is unfinished and protected again"), Session.HasUnfinishedCheckpoint());
    IFileManager::Get().DeleteDirectory(*Directory, false, true);
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAbandonRunScreenTest, "Project.Expedition.Presentation.AbandonRunConfirmation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FAbandonRunScreenTest::RunTest(const FString& Parameters)
{
    using ExpeditionInputHarness::FHarness;
    const FName Abandon(TEXT("Abandon")), Cancel(TEXT("AbandonCancel")), Confirm(TEXT("AbandonConfirm")), Resume(TEXT("Resume")), New(TEXT("New"));
    FExpeditionSnapshot Saved;
    FExpeditionSaveData Data;
    {
        FExpeditionRun Run; FString Error;
        Run.NewRun(42, Error, TEXT("abandon-screen-run")); Run.ChooseRoute(Run.Progress().Offers[0].Id, Error);
        Data.Current = Run.Snapshot(); Data.Records.Add(PastDefeat());
        Saved = Data.Current;
    }
    FHarness Game(*this, TEXT("AbandonRunScreen"), &Data);
    if (!TestTrue(TEXT("Input, board and HUD are ready"), Game.IsReady())) { return false; }
    AExpeditionPlayerController* Player = Game.Player;
    AExpeditionBattleView* View = Game.View;
    Game.Frame();
    const TArray<uint8> Original = Game.CheckpointBytes();
    TestTrue(TEXT("The start screen offers Abandon run"), Game.Hud->MenuButtons().Contains(Abandon));
    TestEqual(TEXT("Resume stays the focused primary choice"), Player->MenuFocus(), Resume);
    // Keyboard: Tab to Abandon run, Enter opens the question, which focuses Keep the run; Enter keeps it.
    for (int32 Step = 0; Step < 12 && Player->MenuFocus() != Abandon; ++Step) { Game.Press(EKeys::Tab); }
    TestEqual(TEXT("Tab reaches Abandon run"), Player->MenuFocus(), Abandon);
    Game.Press(EKeys::Enter);
    TestTrue(TEXT("Enter opens the question"), Player->ConfirmingAbandon());
    TestEqual(TEXT("The question defaults to keeping the run"), Player->MenuFocus(), Cancel);
    Game.Press(EKeys::N);
    TestTrue(TEXT("The question owns the keys: N does nothing"), Player->ConfirmingAbandon() && !View->HasRun());
    Game.Press(EKeys::Enter);
    TestFalse(TEXT("Enter on the default keeps the run"), Player->ConfirmingAbandon());
    TestTrue(TEXT("Cancel leaves the checkpoint bytes identical"), Game.CheckpointBytes() == Original);
    TestTrue(TEXT("Cancel leaves the run unfinished"), View->HasUnfinishedCheckpoint() && !FPaths::FileExists(Game.Path + TEXT(".abandoned")));
    // Esc cancels too, and the pointer reaches both the command and the question's choices.
    Game.Frame();
    TestTrue(TEXT("The pointer finds Abandon run"), PointerFinds(*Game.Hud, Abandon));
    Player->HandleButton(Abandon);
    Game.Frame();
    TestTrue(TEXT("The pointer finds Keep the run"), PointerFinds(*Game.Hud, Cancel));
    TestTrue(TEXT("The pointer finds Abandon run in the question"), PointerFinds(*Game.Hud, Confirm));
    TestFalse(TEXT("Buttons under the question are not clickable"), PointerFinds(*Game.Hud, Resume));
    Game.Press(EKeys::Escape);
    TestFalse(TEXT("Esc keeps the run"), Player->ConfirmingAbandon());
    TestTrue(TEXT("Still identical bytes"), Game.CheckpointBytes() == Original);
    Game.Frame();
    TestEqual(TEXT("Focus returns to the start screen's choice"), Player->MenuFocus(), Abandon);
    // A failed write changes nothing and tells the player.
    Player->HandleButton(Abandon);
    ExpeditionTestSupport::FailNextCheckpointWrite(*this, Game.Path + TEXT(".abandoned"));
    Player->HandleButton(Confirm);
    ExpeditionTestSupport::AllowCheckpointWrites(Game.Path + TEXT(".abandoned"));
    Game.Frame();
    TestTrue(TEXT("The failure is reported: ") + View->Messages().Last(), View->Messages().Last().Contains(TEXT("Could not abandon")));
    TestTrue(TEXT("The failure leaves the bytes identical"), Game.CheckpointBytes() == Original);
    TestTrue(TEXT("The run is still offered"), View->HasUnfinishedCheckpoint() && Game.Hud->MenuButtons().Contains(Abandon));
    // Resume after all of that is the identical run.
    {
        FExpeditionSession Check(Game.Path);
        FString Error;
        TestTrue(TEXT("Resume after cancel restores the identical run"), Check.Resume(Error) && ExpeditionTestSupport::SameRun(Saved, Check.Run().Snapshot()));
    }
    // Confirm: the run is abandoned, Resume and Abandon go, Begin is focused and starts a fresh run.
    Player->HandleButton(Abandon);
    Player->HandleButton(Confirm);
    Game.Frame(); Game.Frame();
    TestFalse(TEXT("No unfinished checkpoint after Abandon"), View->HasUnfinishedCheckpoint());
    TestFalse(TEXT("Abandon run is no longer offered"), Game.Hud->MenuButtons().Contains(Abandon));
    TestEqual(TEXT("Begin is the primary choice again"), Player->MenuFocus(), New);
    Game.Press(EKeys::Enter);
    TestTrue(TEXT("Begin starts a fresh run"), View->HasRun() && View->Progress().Phase == EExpeditionPhase::Route && View->Progress().EncountersCompleted == 0 && View->Progress().RunId != Saved.Progress.RunId);
    return !HasAnyErrors();
}
#endif
