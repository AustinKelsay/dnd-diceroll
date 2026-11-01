#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Persistence/ExpeditionSave.h"
#include "ExpeditionTestSupport.h"

/**
 * Developer tool, not a behavior test: writes real checkpoints at each expedition phase
 * under Saved/CaptureFixtures/<phase>/current-v1.json so visual QA can resume straight into
 * that screen with -ExpeditionSaveRoot=<that folder>. Run it explicitly:
 *   tools/dev.py test --filter Project.Tools.CaptureFixtures
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FExpeditionCaptureFixtures, "Project.Tools.CaptureFixtures",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FExpeditionCaptureFixtures::RunTest(const FString& Parameters)
{
    const FString Root = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("CaptureFixtures"));
    auto Write = [&](const TCHAR* Name, const FExpeditionRun& Run)
    {
        FExpeditionSaveData Data;
        Data.Current = Run.Snapshot();
        const EExpeditionPhase Phase = Run.Progress().Phase;
        if (Phase == EExpeditionPhase::Victory || Phase == EExpeditionPhase::Defeat) { Data.Records.Add(FExpeditionSave::Ending(Run)); }
        FString Error;
        const bool bWritten = FExpeditionSave::Write(FPaths::Combine(Root, Name, TEXT("current-v1.json")), Data, Error);
        TestTrue(FString::Printf(TEXT("Fixture %s writes %s"), Name, *Error), bWritten);
    };
    auto Survivor = [](const FExpeditionRun& Run) -> int32
    {
        for (const FCombatantState& Unit : Run.Battle().Snapshot().Encounter.Combatants)
        { if (Unit.Side == ECombatSide::Party && Unit.Life == ECombatLife::Conscious) { return Unit.Id; } }
        return INDEX_NONE;
    };
    bool bVictoryWritten = false;
    for (int32 Seed = 1; Seed < 60 && !bVictoryWritten; ++Seed)
    {
        FString Error;
        FExpeditionRun Run;
        Run.NewRun(Seed, Error, FString::Printf(TEXT("capture-%d"), Seed));
        Run.ChooseRoute(Run.Progress().Offers[0].Id, Error);
        Write(TEXT("first-battle"), Run);
        if (!ExpeditionTestSupport::FinishCombat(Run) || Run.Progress().Phase != EExpeditionPhase::Reward) { continue; }
        Write(TEXT("reward"), Run);
        Run.ClaimReward(Survivor(Run), Error);
        Write(TEXT("camp"), Run);
        Run.Recover(true, Error);
        Run.ChooseRoute(Run.Progress().Offers[1].Id, Error);
        if (!ExpeditionTestSupport::FinishCombat(Run) || Run.Progress().Phase != EExpeditionPhase::Reward) { continue; }
        Run.ClaimReward(Survivor(Run), Error);
        while (Run.Progress().Phase == EExpeditionPhase::Route && Run.Progress().EncountersCompleted < 4)
        {
            if (!Run.ChooseRoute(Run.Progress().Offers[0].Id, Error) || !ExpeditionTestSupport::FinishCombat(Run) || Run.Progress().Phase != EExpeditionPhase::Reward) { break; }
            if (Run.Progress().EncountersCompleted == 4) { Write(TEXT("fourth-reward"), Run); }
            if (!Run.ClaimReward(Survivor(Run), Error)) { break; }
        }
        if (Run.Progress().Phase != EExpeditionPhase::Route || Run.Progress().EncountersCompleted != 4) { continue; }
        Write(TEXT("final-route"), Run);
        Run.ChooseRoute(Run.Progress().Offers[0].Id, Error);
        Write(TEXT("vault"), Run);
        if (!ExpeditionTestSupport::FinishCombat(Run) || Run.Progress().Phase != EExpeditionPhase::Victory) { continue; }
        Write(TEXT("victory"), Run);
        bVictoryWritten = true;
    }
    TestTrue(TEXT("A seed reaches victory"), bVictoryWritten);
    FString Error;
    FExpeditionRun Lost;
    Lost.NewRun(3, Error, TEXT("capture-defeat"));
    Lost.ChooseRoute(Lost.Progress().Offers[1].Id, Error);
    ExpeditionTestSupport::FinishCombat(Lost, false);
    TestEqual(TEXT("A passive party is defeated"), Lost.Progress().Phase, EExpeditionPhase::Defeat);
    Write(TEXT("defeat"), Lost);
    return !HasAnyErrors();
}
#endif
