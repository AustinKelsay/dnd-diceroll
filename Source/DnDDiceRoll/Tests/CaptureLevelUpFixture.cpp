#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Persistence/ExpeditionSave.h"
#include "ExpeditionTestSupport.h"
#include "Rules/AdvancementRules.h"

/** Developer tool: Saved/CaptureFixtures/levelup-reward/current-v1.json (SRD Reward + Level Up). */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCaptureLevelUpRewardFixture, "Project.Tools.CaptureLevelUpReward",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FCaptureLevelUpRewardFixture::RunTest(const FString& Parameters)
{
    FString Error;
    FExpeditionRun Run;
    bool bWrote = false;
    for (int32 Seed = 1; Seed < 80 && !bWrote; ++Seed)
    {
        if (!Run.NewRun(Seed, Error, FString::Printf(TEXT("levelup-cap-%d"), Seed), EFighterSubclass::Champion, ERuleset::SRD)) { continue; }
        if (!Run.ChooseRoute(Run.Progress().Offers[0].Id, Error)) { continue; }
        if (!ExpeditionTestSupport::FinishCombat(Run) || Run.Progress().Phase != EExpeditionPhase::Reward) { continue; }

        FExpeditionSnapshot Bumped = Run.Snapshot();
        for (FCombatantState& U : Bumped.Battle.Encounter.Combatants)
        {
            if (U.Side != ECombatSide::Party || U.Life != ECombatLife::Conscious) { continue; }
            U.ExperiencePoints = FMath::Max(U.ExperiencePoints, FAdvancementRules::XpForLevel(2));
            U.bLevelUpAvailable = true;
        }
        FExpeditionRun Probe;
        if (!Probe.Restore(Bumped, Error))
        {
            AddError(FString::Printf(TEXT("Restore bumped seed %d: %s"), Seed, *Error));
            continue;
        }
        FExpeditionSaveData Out;
        Out.Current = Bumped;
        const FString Root = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("CaptureFixtures"), TEXT("levelup-reward"));
        if (!FExpeditionSave::Write(FPaths::Combine(Root, TEXT("current-v1.json")), Out, Error))
        {
            AddError(FString::Printf(TEXT("Write seed %d: %s"), Seed, *Error));
            continue;
        }
        bWrote = true;
    }
    TestTrue(TEXT("Wrote SRD level-up reward fixture"), bWrote);
    return !HasAnyErrors();
}
#endif
