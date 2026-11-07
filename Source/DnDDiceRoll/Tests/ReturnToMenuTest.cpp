#if WITH_DEV_AUTOMATION_TESTS
#include "ExpeditionInputHarness.h"

namespace ReturnToMenuTest
{
using ExpeditionInputHarness::FHarness;
const FName Menu(TEXT("Menu"));
const FName Confirm(TEXT("ReturnConfirm"));
const FName Resume(TEXT("Resume"));
const FName KeepPlaying(TEXT("KeepPlaying"));

/**
 * Leaves the expedition, checks the start screen and its New Run protection, then comes
 * back and checks the exact run and checkpoint bytes are restored.
 */
bool EnemyActive(const FHarness& Game)
{
    const FCombatantState* Active = Game.View->Rules().Find(Game.View->Rules().ActiveId());
    return Active && Active->Side == ECombatSide::Enemy;
}
/** Ends party turns until an enemy is about to act (it is still "thinking"). */
bool ToEnemyTurn(FHarness& Game)
{
    for (int32 Turn = 0; Turn < 12 && Game.View->InCombat() && !EnemyActive(Game); ++Turn)
    {
        Game.PartyTurn();
        Game.Press(EKeys::SpaceBar);
    }
    return Game.View->InCombat() && EnemyActive(Game);
}
/** Plays frames until the pending return opens its question (or the frames run out). */
void UntilConfirm(FHarness& Game)
{
    for (int32 Step = 0; Step < 2000 && Game.View->InCombat() && !Game.Player->ConfirmingReturn(); ++Step) { Game.Frame(); }
}

void ReturnAndResume(FHarness& Game, const FString& Phase, TFunctionRef<void()> Leave, TFunctionRef<void()> Back, FName Primary)
{
    FAutomationTestBase& Test = Game.Test;
    const FExpeditionSnapshot Before = Game.Live();
    const TArray<uint8> Bytes = Game.CheckpointBytes();
    Test.TestFalse(Phase + TEXT(": a checkpoint exists before returning"), Bytes.IsEmpty());
    Leave();
    Test.TestFalse(Phase + TEXT(": the run is put away"), Game.View->HasRun());
    Test.TestTrue(Phase + TEXT(": the start screen is shown"), Game.Player->ShowingBuilder());
    Test.TestFalse(Phase + TEXT(": no confirmation stays open"), Game.Player->ConfirmingReturn());
    Test.TestTrue(Phase + TEXT(": returning leaves the checkpoint bytes unchanged"), Game.CheckpointBytes() == Bytes);
    Test.TestEqual(Phase + TEXT(": the start screen focuses Resume"), Game.Player->MenuFocus(), Resume);
    Game.Press(EKeys::N);
    Test.TestFalse(Phase + TEXT(": New Run keeps protecting the unfinished expedition"), Game.View->HasRun());
    Test.TestTrue(Phase + TEXT(": the refused New Run leaves the checkpoint bytes unchanged"), Game.CheckpointBytes() == Bytes);
    Test.TestEqual(Phase + TEXT(": Resume keeps the focus after a refused New Run"), Game.Player->MenuFocus(), Resume);
    Back();
    Test.TestTrue(Phase + TEXT(": Resume brings the run back"), Game.View->HasRun());
    Test.TestTrue(Phase + TEXT(": Resume restores the identical run"), ExpeditionTestSupport::SameRun(Before, Game.Live()));
    Test.TestTrue(Phase + TEXT(": Resume leaves the checkpoint bytes unchanged"), Game.CheckpointBytes() == Bytes);
    Game.Settle();
    Test.TestEqual(Phase + TEXT(": the resumed screen focuses its primary choice"), Game.Player->MenuFocus(), Primary);
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReturnToMenuTest, "Project.Expedition.Presentation.ReturnToMenuFromEachPhase",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FReturnToMenuTest::RunTest(const FString& Parameters)
{
    using namespace ReturnToMenuTest;
    FHarness Game(*this, TEXT("ReturnToMenu"));
    if (!TestTrue(TEXT("Input, board and HUD are ready"), Game.IsReady())) { return false; }
    AExpeditionPlayerController* Player = Game.Player;
    AExpeditionBattleView* View = Game.View;
    Game.Frame();
    Game.Press(EKeys::Enter);
    if (!TestTrue(TEXT("Enter on Begin starts the expedition"), View->HasRun() && View->Progress().Phase == EExpeditionPhase::Route)) { return false; }
    Game.Settle();
    TestTrue(TEXT("The route screen shows a Menu button"), Game.Hud->MenuButtons().Contains(Menu));

    // Route: nothing is in progress, so Backspace returns at once; Enter on Resume comes back.
    ReturnAndResume(Game, TEXT("Route"), [&] { Game.Press(EKeys::BackSpace); }, [&] { Game.Press(EKeys::Enter); }, TEXT("Route0"));

    Game.Press(EKeys::Enter);
    if (!TestTrue(TEXT("Enter on the first route starts its encounter"), View->InCombat())) { return false; }
    Game.PartyTurn();
    if (!TestTrue(TEXT("The party has the turn"), View->InCombat())) { return false; }
    TestTrue(TEXT("The encounter shows a Menu button"), Game.Hud->MenuButtons().Contains(Menu));
    {
        // Mid-encounter, Backspace asks first, and the question blocks everything else.
        const FExpeditionSnapshot Before = Game.Live();
        const TArray<uint8> Bytes = Game.CheckpointBytes();
        Game.Press(EKeys::BackSpace);
        TestTrue(TEXT("Backspace mid-encounter asks for confirmation"), Player->ConfirmingReturn());
        TestTrue(TEXT("Asking keeps the run"), View->HasRun() && View->InCombat());
        TestEqual(TEXT("The confirmation focuses Return to menu"), Player->MenuFocus(), Confirm);
        const TArray<FName> Choices = Game.Hud->MenuButtons();
        TestTrue(TEXT("Only the confirmation's two choices are live"), Choices.Num() == 2 && Choices.Contains(Confirm) && Choices.Contains(KeepPlaying));
        Game.Press(EKeys::SpaceBar);
        Game.Press(EKeys::One);
        Game.Press(EKeys::Slash);
        Game.Press(EKeys::H);
        Game.Press(EKeys::BackSpace);
        TestFalse(TEXT("The confirmation blocks Help"), Player->ShowingHelp());
        TestFalse(TEXT("The confirmation blocks Records"), View->ShowingRecords());
        TestTrue(TEXT("A second Backspace does not confirm"), Player->ConfirmingReturn() && View->HasRun());
        TestTrue(TEXT("The confirmation blocks gameplay keys"), ExpeditionTestSupport::SameRun(Before, Game.Live()));
        Game.Press(EKeys::Escape);
        TestFalse(TEXT("Esc cancels the confirmation"), Player->ConfirmingReturn());
        TestTrue(TEXT("Cancelling keeps playing the same encounter"), View->InCombat() && ExpeditionTestSupport::SameRun(Before, Game.Live()));
        TestEqual(TEXT("Combat holds no menu focus after cancelling"), Player->MenuFocus(), FName());
        TestTrue(TEXT("Asking and cancelling leave the checkpoint bytes unchanged"), Game.CheckpointBytes() == Bytes);
    }
    ReturnAndResume(Game, TEXT("Encounter"), [&]
    {
        Game.Press(EKeys::BackSpace);
        Game.Press(EKeys::Enter);
    }, [&] { Game.Press(EKeys::O); }, FName());
    TestTrue(TEXT("Resume returns to the encounter"), View->InCombat());

    if (!TestTrue(TEXT("The party wins the first encounter"), Game.WinBattle())) { return false; }
    Game.Settle();
    if (!TestEqual(TEXT("Victory reaches the reward"), View->Progress().Phase, EExpeditionPhase::Reward)) { return false; }
    ReturnAndResume(Game, TEXT("Reward"), [&] { Game.Press(EKeys::BackSpace); }, [&] { Game.Press(EKeys::O); }, TEXT("Claim"));

    Game.Press(EKeys::One);
    Game.Press(EKeys::L);
    if (!TestEqual(TEXT("1 then L claims the reward"), View->Progress().Phase, EExpeditionPhase::Recovery)) { return false; }
    Game.Settle();
    // The mouse path: the Menu and Resume buttons dispatch the same commands as a click.
    ReturnAndResume(Game, TEXT("Recovery"), [&] { Player->HandleButton(Menu); Game.Frame(); }, [&] { Player->HandleButton(Resume); Game.Frame(); }, TEXT("Camp"));
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReturnToMenuBlockedTest, "Project.Expedition.Presentation.ReturnToMenuWaitsForOverlays",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FReturnToMenuBlockedTest::RunTest(const FString& Parameters)
{
    using namespace ReturnToMenuTest;
    FHarness Game(*this, TEXT("ReturnToMenuBlocked"));
    if (!TestTrue(TEXT("Input, board and HUD are ready"), Game.IsReady())) { return false; }
    AExpeditionPlayerController* Player = Game.Player;
    AExpeditionBattleView* View = Game.View;
    Game.Frame();
    Game.Press(EKeys::BackSpace);
    TestTrue(TEXT("Backspace on the start screen does nothing"), !View->HasRun() && !Player->ConfirmingReturn() && Player->ShowingBuilder());
    TestEqual(TEXT("The start screen still focuses Begin"), Player->MenuFocus(), FName(TEXT("New")));
    Game.Press(EKeys::Enter);
    Game.Settle();
    if (!TestTrue(TEXT("The expedition starts"), View->HasRun())) { return false; }
    const TArray<uint8> Bytes = Game.CheckpointBytes();

    Game.Press(EKeys::Slash);
    Game.Press(EKeys::BackSpace);
    Player->HandleButton(Menu);
    Game.Frame();
    TestTrue(TEXT("Help blocks returning to the menu"), View->HasRun() && Player->ShowingHelp() && !Player->ConfirmingReturn());
    Game.Press(EKeys::Escape);
    Game.Press(EKeys::H);
    Game.Press(EKeys::BackSpace);
    Player->HandleButton(Menu);
    Game.Frame();
    TestTrue(TEXT("Records blocks returning to the menu"), View->HasRun() && View->ShowingRecords() && !Player->ConfirmingReturn());
    Game.Press(EKeys::Escape);
    TestEqual(TEXT("Closing the overlays restores the route focus"), Player->MenuFocus(), FName(TEXT("Route0")));
    TestTrue(TEXT("Blocked returns leave the checkpoint bytes unchanged"), Game.CheckpointBytes() == Bytes);

    Game.Press(EKeys::Enter);
    Game.PartyTurn();
    if (!TestTrue(TEXT("The first encounter starts"), View->InCombat())) { return false; }
    const FExpeditionSnapshot Before = Game.Live();
    const TArray<uint8> Fight = Game.CheckpointBytes();
    Player->HandleButton(Menu);
    Game.Frame();
    TestTrue(TEXT("The Menu button asks for confirmation mid-encounter"), Player->ConfirmingReturn() && View->HasRun());
    Player->HandleButton(TEXT("EndTurn"));
    Player->HandleButton(TEXT("Help"));
    Player->HandleButton(TEXT("Records"));
    Game.Frame();
    TestTrue(TEXT("Buttons beneath the confirmation do nothing"), ExpeditionTestSupport::SameRun(Before, Game.Live()) && !Player->ShowingHelp() && !View->ShowingRecords());
    Game.Press(EKeys::Tab);
    TestEqual(TEXT("Tab moves to Keep playing"), Player->MenuFocus(), KeepPlaying);
    Game.Press(EKeys::Enter);
    TestTrue(TEXT("Enter on Keep playing stays in the encounter"), !Player->ConfirmingReturn() && View->InCombat());
    Player->HandleButton(Menu);
    Game.Frame();
    Player->HandleButton(Confirm);
    Game.Frame();
    TestTrue(TEXT("The confirm button returns to the start screen"), !View->HasRun() && Player->ShowingBuilder());
    TestEqual(TEXT("The start screen focuses Resume"), Player->MenuFocus(), Resume);
    TestTrue(TEXT("The mouse path leaves the checkpoint bytes unchanged"), Game.CheckpointBytes() == Fight);

    // An ending already leads to the next-party builder; there is no run to put away.
    Game.Press(EKeys::Enter);
    Game.PartyTurn();
    if (!TestTrue(TEXT("Resume returns to the encounter"), ExpeditionTestSupport::SameRun(Before, Game.Live()))) { return false; }
    for (int32 Step = 0; Step < 600 && View->InCombat(); ++Step)
    {
        View->SkipPresentation();
        Game.Frame();
        const FCombatantState* Active = View->Rules().Find(View->Rules().ActiveId());
        if (!Active) { break; }
        if (Active->Side == ECombatSide::Party) { Game.Press(EKeys::SpaceBar); }
        else { View->Tick(1.0f); }
    }
    if (!TestEqual(TEXT("A passive party is defeated"), View->Progress().Phase, EExpeditionPhase::Defeat)) { return false; }
    Game.Settle();
    TestFalse(TEXT("The ending shows no Menu button"), Game.Hud->MenuButtons().Contains(Menu));
    Game.Press(EKeys::BackSpace);
    TestTrue(TEXT("Backspace on the ending does nothing"), View->HasRun() && !Player->ConfirmingReturn() && !Player->ShowingBuilder());
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReturnToMenuEnemyTurnTest, "Project.Expedition.Presentation.ReturnToMenuWaitsForEnemyTurn",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FReturnToMenuEnemyTurnTest::RunTest(const FString& Parameters)
{
    using namespace ReturnToMenuTest;
    FHarness Game(*this, TEXT("ReturnToMenuEnemyTurn"));
    if (!TestTrue(TEXT("Input, board and HUD are ready"), Game.IsReady())) { return false; }
    AExpeditionPlayerController* Player = Game.Player;
    AExpeditionBattleView* View = Game.View;
    Game.Frame();
    Game.Press(EKeys::Enter);
    Game.Settle();
    Game.Press(EKeys::Enter);
    if (!TestTrue(TEXT("Enter, Enter starts the first encounter"), View->InCombat())) { return false; }
    if (!TestTrue(TEXT("An enemy is about to act"), ToEnemyTurn(Game))) { return false; }

    // Asked while the enemy thinks: the question waits until the party has the turn and
    // every committed result has played, so nothing resolves unseen behind it.
    Game.Press(EKeys::BackSpace);
    TestFalse(TEXT("Backspace on an enemy turn does not open the question yet"), Player->ConfirmingReturn());
    TestTrue(TEXT("The return is pending"), Player->ReturnPending() && View->HasRun());
    UntilConfirm(Game);
    if (!TestTrue(TEXT("The question opens once the party has the turn"), Player->ConfirmingReturn())) { return false; }
    TestFalse(TEXT("The enemy turn was shown in full first"), View->IsPresenting());
    TestFalse(TEXT("The question opens on the party's turn"), EnemyActive(Game));
    const FExpeditionSnapshot Asked = Game.Live();
    const TArray<uint8> AskedBytes = Game.CheckpointBytes();
    for (int32 Step = 0; Step < 60; ++Step) { View->Tick(1.0f); Game.Frame(); }
    TestTrue(TEXT("Nothing resolves while the question is open"), ExpeditionTestSupport::SameRun(Asked, Game.Live()) && Game.CheckpointBytes() == AskedBytes);
    TestTrue(TEXT("The question stays open"), Player->ConfirmingReturn());
    Game.Press(EKeys::Escape);
    TestFalse(TEXT("Esc cancels the question"), Player->ConfirmingReturn() || Player->ReturnPending());
    TestTrue(TEXT("Cancelling keeps the same turn"), ExpeditionTestSupport::SameRun(Asked, Game.Live()));
    Game.Press(EKeys::SpaceBar);
    TestFalse(TEXT("Play continues after cancelling"), ExpeditionTestSupport::SameRun(Asked, Game.Live()));

    if (!TestTrue(TEXT("An enemy is about to act again"), ToEnemyTurn(Game))) { return false; }
    Game.Press(EKeys::BackSpace);
    Game.Press(EKeys::BackSpace);
    TestFalse(TEXT("A second Backspace withdraws the pending return"), Player->ReturnPending());
    for (int32 Step = 0; Step < 400 && EnemyActive(Game); ++Step) { Game.Frame(); }
    Game.Settle();
    TestFalse(TEXT("A withdrawn return never asks"), Player->ConfirmingReturn());

    if (!TestTrue(TEXT("An enemy is about to act once more"), ToEnemyTurn(Game))) { return false; }
    Player->HandleButton(Menu);
    Game.Frame();
    TestTrue(TEXT("The Menu button also waits for the enemy"), Player->ReturnPending() && !Player->ConfirmingReturn());
    UntilConfirm(Game);
    if (!TestTrue(TEXT("The question opens on the party's turn again"), Player->ConfirmingReturn())) { return false; }
    const FExpeditionSnapshot Turn = Game.Live();
    const TArray<uint8> TurnBytes = Game.CheckpointBytes();
    Game.Press(EKeys::Enter);
    TestTrue(TEXT("Enter returns to the start screen"), !View->HasRun() && Player->ShowingBuilder());
    TestTrue(TEXT("Returning leaves the checkpoint bytes unchanged"), Game.CheckpointBytes() == TurnBytes);
    Game.Press(EKeys::Enter);
    TestTrue(TEXT("Enter on the focused Resume comes back to the same turn"), View->HasRun() && ExpeditionTestSupport::SameRun(Turn, Game.Live()));
    TestFalse(TEXT("Resume lands on the party's turn"), EnemyActive(Game));
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReturnToMenuPlaybackTest, "Project.Expedition.Presentation.ReturnToMenuWaitsForPlayback",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FReturnToMenuPlaybackTest::RunTest(const FString& Parameters)
{
    using namespace ReturnToMenuTest;
    FHarness Game(*this, TEXT("ReturnToMenuPlayback"));
    if (!TestTrue(TEXT("Input, board and HUD are ready"), Game.IsReady())) { return false; }
    AExpeditionPlayerController* Player = Game.Player;
    AExpeditionBattleView* View = Game.View;
    Game.Frame();
    Game.Press(EKeys::Enter);
    Game.Settle();
    Game.Press(EKeys::Enter);
    Game.PartyTurn();
    if (!TestTrue(TEXT("The party has the first turn of the encounter"), View->InCombat() && !EnemyActive(Game))) { return false; }

    // A party move is committed at once and then animates.
    const int32 Id = View->Rules().ActiveId();
    const TArray<FIntPoint> Moves = View->Rules().LegalMoves(Id);
    if (!TestFalse(TEXT("The active hero can move"), Moves.IsEmpty())) { return false; }
    View->Execute({ECombatCommandType::Move, Id, Moves.Last()});
    if (!TestTrue(TEXT("The move is still animating"), View->IsPresenting())) { return false; }
    const FExpeditionSnapshot Moved = Game.Live();
    const TArray<uint8> MovedBytes = Game.CheckpointBytes();
    Game.Press(EKeys::BackSpace);
    TestTrue(TEXT("Backspace during the move's playback waits"), Player->ReturnPending() && !Player->ConfirmingReturn() && View->HasRun());
    UntilConfirm(Game);
    TestTrue(TEXT("The question opens after the move has played"), Player->ConfirmingReturn() && !View->IsPresenting());
    TestTrue(TEXT("Waiting changed nothing"), ExpeditionTestSupport::SameRun(Moved, Game.Live()));
    Game.Press(EKeys::Enter);
    TestTrue(TEXT("Confirming returns with the checkpoint bytes unchanged"), !View->HasRun() && Game.CheckpointBytes() == MovedBytes);
    Game.Press(EKeys::O);
    Game.PartyTurn();
    TestTrue(TEXT("Resume restores the committed move"), ExpeditionTestSupport::SameRun(Moved, Game.Live()));

    // The victory is committed before its playback finishes; leaving waits for it, then needs no question.
    if (!TestTrue(TEXT("The party wins the encounter"), Game.WinBattle())) { return false; }
    if (!TestTrue(TEXT("The victory is still being presented on the reward"), View->IsPresenting() && View->Progress().Phase == EExpeditionPhase::Reward)) { return false; }
    const FExpeditionSnapshot Won = Game.Live();
    const TArray<uint8> WonBytes = Game.CheckpointBytes();
    Game.Press(EKeys::BackSpace);
    TestTrue(TEXT("Backspace during the victory playback waits"), View->HasRun() && Player->ReturnPending());
    Game.Settle();
    TestTrue(TEXT("Once the victory has played, the run is put away"), !View->HasRun() && Player->ShowingBuilder() && !Player->ReturnPending());
    TestTrue(TEXT("The checkpoint bytes are unchanged"), Game.CheckpointBytes() == WonBytes);
    Game.Press(EKeys::O);
    TestTrue(TEXT("Resume restores the committed victory"), ExpeditionTestSupport::SameRun(Won, Game.Live()));

    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStartScreenSavedFocusTest, "Project.Expedition.Presentation.StartScreenFocusesResumeForUnfinishedSave",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FStartScreenSavedFocusTest::RunTest(const FString& Parameters)
{
    FString Error;
    FExpeditionRun Unfinished;
    Unfinished.NewRun(20261030, Error, TEXT("start-focus-unfinished"));
    FExpeditionRun Ended;
    Ended.NewRun(1, Error, TEXT("start-focus-ended"));
    Ended.ChooseRoute(Ended.Progress().Offers[0].Id, Error);
    if (!TestTrue(TEXT("The ended fixture reaches an ending"), ExpeditionTestSupport::FinishCombat(Ended, false))) { return false; }
    {
        FExpeditionSaveData Data;
        Data.Current = Unfinished.Snapshot();
        ReturnToMenuTest::FHarness Game(*this, TEXT("StartFocusUnfinished"), &Data);
        if (!TestTrue(TEXT("Input, board and HUD are ready"), Game.IsReady())) { return false; }
        Game.Frame();
        TestEqual(TEXT("A cold launch with an unfinished save focuses Resume"), Game.Player->MenuFocus(), ReturnToMenuTest::Resume);
        Game.Press(EKeys::Enter);
        TestTrue(TEXT("Enter resumes the unfinished expedition"), Game.View->HasRun() && ExpeditionTestSupport::SameRun(Unfinished.Snapshot(), Game.Live()));
    }
    {
        FExpeditionSaveData Data;
        Data.Current = Ended.Snapshot();
        Data.Records.Add(FExpeditionSave::Ending(Ended));
        ReturnToMenuTest::FHarness Game(*this, TEXT("StartFocusEnded"), &Data);
        if (!TestTrue(TEXT("Input, board and HUD are ready"), Game.IsReady())) { return false; }
        Game.Frame();
        TestEqual(TEXT("A cold launch with a finished save focuses Begin"), Game.Player->MenuFocus(), FName(TEXT("New")));
    }
    return !HasAnyErrors();
}
#endif
