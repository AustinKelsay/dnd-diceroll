#if WITH_DEV_AUTOMATION_TESTS
#include "ExpeditionInputHarness.h"

namespace PhaseFocusTest
{
using ExpeditionInputHarness::FHarness;

int32 Survivor(const FExpeditionRun& Run)
{
    for (const FCombatantState& Unit : Run.Battle().Snapshot().Encounter.Combatants)
    { if (Unit.Side == ECombatSide::Party && Unit.Life == ECombatLife::Conscious) { return Unit.Id; } }
    return INDEX_NONE;
}
/** A checkpoint at the start of the final battle and party commands that win it. */
bool FinalBattle(FExpeditionSnapshot& Checkpoint, TArray<FCombatCommand>& Commands)
{
    for (int32 Seed = 1; Seed < 60; ++Seed)
    {
        FString Error;
        FExpeditionRun Run;
        Run.NewRun(Seed, Error, FString::Printf(TEXT("phase-focus-victory-%d"), Seed));
        while (Run.Progress().Phase == EExpeditionPhase::Route && Run.Progress().EncountersCompleted < FExpeditionRun::EncounterCount - 1)
        {
            if (!Run.ChooseRoute(Run.Progress().Offers[0].Id, Error) || !ExpeditionTestSupport::FinishCombat(Run) || Run.Progress().Phase != EExpeditionPhase::Reward) { break; }
            if (!Run.ClaimReward(Survivor(Run), Error)) { break; }
            if (Run.Progress().Phase == EExpeditionPhase::Recovery) { Run.Recover(true, Error); }
        }
        if (Run.Progress().Phase != EExpeditionPhase::Route || Run.Progress().EncountersCompleted != FExpeditionRun::EncounterCount - 1) { continue; }
        if (!Run.ChooseRoute(Run.Progress().Offers[0].Id, Error)) { continue; }
        Checkpoint = Run.Snapshot();
        Commands.Reset();
        if (ExpeditionTestSupport::FinishCombat(Run, true, &Commands) && Run.Progress().Phase == EExpeditionPhase::Victory) { return true; }
    }
    return false;
}
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
    Game.CheckOverlays(TEXT("Startup builder"), TEXT("New"));
    Game.Press(EKeys::Enter);
    if (!TestTrue(TEXT("Enter on Begin starts the expedition"), View->HasRun() && View->Progress().Phase == EExpeditionPhase::Route)) { return false; }
    Game.Settle();
    Game.CheckOverlays(TEXT("First route"), TEXT("Route0"));
    Game.Press(EKeys::Enter);
    if (!TestTrue(TEXT("Enter on the first route starts its encounter"), View->InCombat())) { return false; }
    TestEqual(TEXT("Combat holds no menu focus"), Player->MenuFocus(), FName());

    if (!TestTrue(TEXT("The party wins the first encounter"), Game.WinBattle())) { return false; }
    TestEqual(TEXT("Victory reaches the reward"), View->Progress().Phase, EExpeditionPhase::Reward);
    TestTrue(TEXT("The victory is still being presented"), View->IsPresenting());
    // Open and close Help while the victory plays: Help must not keep focus afterwards.
    Game.Press(EKeys::Slash);
    TestEqual(TEXT("Help focuses its own Close"), Player->MenuFocus(), FName(TEXT("Close")));
    Game.Press(EKeys::Escape);
    Game.Settle();
    TestFalse(TEXT("Help closed"), Player->ShowingHelp());
    TestEqual(TEXT("Reward focuses Claim"), Player->MenuFocus(), FName(TEXT("Claim")));

    Game.Press(EKeys::One);
    Game.Press(EKeys::L);
    if (!TestEqual(TEXT("1 then L claims the reward"), View->Progress().Phase, EExpeditionPhase::Recovery)) { return false; }
    Game.CheckOverlays(TEXT("Recovery"), TEXT("Camp"));
    // A control the player chose with Tab survives Help on the same screen; Shift-Tab returns.
    Game.Press(EKeys::Tab);
    const FName Chosen = Player->MenuFocus();
    TestTrue(TEXT("Tab moves focus off the recovery choice"), Chosen != FName() && Chosen != FName(TEXT("Camp")));
    Game.Press(EKeys::Slash);
    Game.Press(EKeys::Escape);
    TestEqual(TEXT("Closing Help restores the chosen control"), Player->MenuFocus(), Chosen);
    Player->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::LeftShift, IE_Pressed, 1.0f));
    Game.Press(EKeys::Tab);
    Player->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::LeftShift, IE_Released, 0.0f));
    Game.Frame();
    TestEqual(TEXT("Shift-Tab returns to the recovery choice"), Player->MenuFocus(), FName(TEXT("Camp")));
    Game.Press(EKeys::Enter);
    TestFalse(TEXT("Enter on recovery does not open Help"), Player->ShowingHelp());
    if (!TestEqual(TEXT("Enter on recovery takes the rest"), View->Progress().Phase, EExpeditionPhase::Route)) { return false; }
    TestTrue(TEXT("The rest was taken, not forfeited"), View->Progress().bCampUsed);

    Game.Settle();
    Game.CheckOverlays(TEXT("Next route"), TEXT("Route0"));
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
        Game.CheckOverlays(TEXT("Defeat ending"), TEXT("Builder"));
        Game.Press(EKeys::Enter);
        TestFalse(TEXT("Enter on the ending does not open Help"), Game.Player->ShowingHelp());
        TestTrue(TEXT("Enter on the ending opens the next-party builder"), Game.Player->ShowingBuilder());
        Game.CheckOverlays(TEXT("Next-party builder after defeat"), TEXT("New"));
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
        Game.CheckOverlays(FString::Printf(TEXT("Resumed %s"), Case.Name), Case.Primary);
        Game.Press(EKeys::Enter);
        TestFalse(FString::Printf(TEXT("Enter on resumed %s does not open Help"), Case.Name), Game.Player->ShowingHelp());
    }
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVictoryFocusTest, "Project.Expedition.Presentation.VictoryEndingFocusesNextParty",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FVictoryFocusTest::RunTest(const FString& Parameters)
{
    FExpeditionSaveData Data;
    TArray<FCombatCommand> Commands;
    if (!TestTrue(TEXT("A seed reaches a winnable final battle"), PhaseFocusTest::FinalBattle(Data.Current, Commands))) { return false; }
    PhaseFocusTest::FHarness Game(*this, TEXT("VictoryFocus"), &Data);
    if (!TestTrue(TEXT("Input, board and HUD are ready"), Game.IsReady())) { return false; }
    Game.Frame();
    Game.Press(EKeys::O);
    Game.Settle();
    if (!TestTrue(TEXT("O resumes the final battle"), Game.View->InCombat())) { return false; }
    // The winning commands play through the view; the final blow's playback is still pending.
    TestTrue(TEXT("The party wins the final battle"), Game.Replay(Commands));
    if (!TestEqual(TEXT("The expedition is won"), Game.View->Progress().Phase, EExpeditionPhase::Victory)) { return false; }
    Game.Settle();
    Game.CheckOverlays(TEXT("Victory ending"), TEXT("Builder"));
    Game.Press(EKeys::Enter);
    TestFalse(TEXT("Enter on the victory ending does not open Help"), Game.Player->ShowingHelp());
    TestTrue(TEXT("Enter on the victory ending opens the next-party builder"), Game.Player->ShowingBuilder());
    Game.CheckOverlays(TEXT("Next-party builder after victory"), TEXT("New"));
    Game.Press(EKeys::Escape);
    TestFalse(TEXT("Esc returns from the draft to the result"), Game.Player->ShowingBuilder());
    TestEqual(TEXT("The victory ending focuses Choose next party again"), Game.Player->MenuFocus(), FName(TEXT("Builder")));
    return !HasAnyErrors();
}
#endif
