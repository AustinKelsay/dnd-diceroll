#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Persistence/ExpeditionSave.h"
#include "ExpeditionTestSupport.h"

/**
 * Developer tool, not a behavior test: writes real checkpoints at each expedition phase
 * under Saved/CaptureFixtures/<phase>/current-v1.json so visual QA can resume straight into
 * that screen with -ExpeditionSaveRoot=<that folder>. It also writes the gallery fixtures that
 * `tools/dev.py gallery` renders (default-seed-route/-battle/-enemy-turn, board-16x11, board-18x12, board-vigil-pillars, board-vigil-walls), each with a
 * fixture.json sidecar naming the seed and board that were found. Run it explicitly:
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
    // Gallery fixtures: first encounters with known seeds and board sizes, party to act first.
    auto Describe = [&](const TCHAR* Name, const FExpeditionRun& Run, int32 Seed, int32 Offer)
    {
        const FCombatEncounter& Encounter = Run.Battle().Snapshot().Encounter;
        const int32 Width = Run.Progress().Phase == EExpeditionPhase::Combat ? Encounter.Width : 0;
        const int32 Height = Run.Progress().Phase == EExpeditionPhase::Combat ? Encounter.Height : 0;
        const FString Info = FString::Printf(TEXT("{\n  \"seed\": %d,\n  \"offer\": %d,\n  \"phase\": \"%s\",\n  \"boardWidth\": %d,\n  \"boardHeight\": %d\n}\n"),
            Seed, Offer, Run.Progress().Phase == EExpeditionPhase::Combat ? TEXT("combat") : TEXT("route"), Width, Height);
        TestTrue(FString::Printf(TEXT("Fixture %s writes its sidecar"), Name), FFileHelper::SaveStringToFile(Info, *FPaths::Combine(Root, Name, TEXT("fixture.json"))));
    };
    constexpr int32 DefaultSeed = 20261030;
    {
        FString Error;
        FExpeditionRun Run;
        Run.NewRun(DefaultSeed, Error, FString::Printf(TEXT("capture-%d"), DefaultSeed));
        Write(TEXT("default-seed-route"), Run);
        Describe(TEXT("default-seed-route"), Run, DefaultSeed, -1);
        TestTrue(TEXT("The default seed offers a route"), Run.ChooseRoute(Run.Progress().Offers[0].Id, Error));
        Write(TEXT("default-seed-battle"), Run);
        Describe(TEXT("default-seed-battle"), Run, DefaultSeed, 0);
        // Enemy turn: the party ends its turns until an enemy is active. Fixture preparation may play
        // legal commands; capture mode only resumes this checkpoint and holds the enemy's turn.
        for (int32 Step = 0; Step < 8 && Run.Progress().Phase == EExpeditionPhase::Combat; ++Step)
        {
            const FCombatantState* Active = Run.Battle().Find(Run.Battle().ActiveId());
            if (!Active || Active->Side != ECombatSide::Party) { break; }
            Run.Submit({ECombatCommandType::EndTurn, Active->Id});
        }
        const FCombatantState* Enemy = Run.Battle().Find(Run.Battle().ActiveId());
        if (TestTrue(TEXT("The default seed reaches an enemy turn"), Run.Progress().Phase == EExpeditionPhase::Combat && Enemy && Enemy->Side == ECombatSide::Enemy))
        {
            Write(TEXT("default-seed-enemy-turn"), Run);
            Describe(TEXT("default-seed-enemy-turn"), Run, DefaultSeed, 0);
        }
    }
    auto FindBoard = [&](const TCHAR* Name, int32 Width, int32 Height)
    {
        for (int32 Seed = 1; Seed < 2000; ++Seed)
        {
            for (int32 Offer = 0; Offer < 2; ++Offer)
            {
                FString Error;
                FExpeditionRun Run;
                Run.NewRun(Seed, Error, FString::Printf(TEXT("capture-%d"), Seed));
                const FExpeditionRoute& Route = Run.Progress().Offers[Offer];
                if (Route.Width != Width || Route.Height != Height || !Run.ChooseRoute(Route.Id, Error)) { continue; }
                const FCombatantState* Active = Run.Battle().Find(Run.Battle().ActiveId());
                if (!Active || Active->Side != ECombatSide::Party) { continue; }
                Write(Name, Run);
                Describe(Name, Run, Seed, Offer);
                return;
            }
        }
        AddError(FString::Printf(TEXT("No seed below 2000 gives a %dx%d board with the party first"), Width, Height));
    };
    FindBoard(TEXT("board-16x11"), 16, 11);
    FindBoard(TEXT("board-18x12"), 18, 12);
    // Drowned Vigil boards (the dangerous Watchtower road) with the architectural recipes: ruined pillars (1) and broken wall (2).
    // The wall recipe is the one whose adjacent blocked cells get the 2-cell arch, so these cover the pillar and wall variants and the arch.
    auto FindVigilRecipe = [&](const TCHAR* Name, int32 Recipe)
    {
        for (int32 Seed = 1; Seed < 2000; ++Seed)
        {
            FString Error;
            FExpeditionRun Run;
            Run.NewRun(Seed, Error, FString::Printf(TEXT("capture-%d"), Seed));
            const FExpeditionRoute& Route = Run.Progress().Offers[1];
            if (!Route.bDangerous || Route.Recipe != Recipe || !Run.ChooseRoute(Route.Id, Error)) { continue; }
            const FCombatantState* Active = Run.Battle().Find(Run.Battle().ActiveId());
            if (!Active || Active->Side != ECombatSide::Party) { continue; }
            Write(Name, Run);
            Describe(Name, Run, Seed, 1);
            return;
        }
        AddError(FString::Printf(TEXT("No seed below 2000 gives a dangerous route with recipe %d and the party first"), Recipe));
    };
    FindVigilRecipe(TEXT("board-vigil-pillars"), 1);
    FindVigilRecipe(TEXT("board-vigil-walls"), 2);
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
