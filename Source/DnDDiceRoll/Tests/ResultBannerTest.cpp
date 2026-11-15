#if WITH_DEV_AUTOMATION_TESTS
#include "ExpeditionInputHarness.h"

/**
 * #251: the victory and defeat banners are drawn after the fight, on a frame of their own before the reward or ending screen, and the reward's
 * claim line only mentions the camp when the camp is next.
 */
namespace ResultBannerTest
{
using namespace ExpeditionTestSupport;

int32 FirstSurvivor(const FExpeditionRun& Run)
{
    for (const FCombatantState& U : Run.Battle().Snapshot().Encounter.Combatants) { if (U.Side == ECombatSide::Party && U.Life == ECombatLife::Conscious) { return U.Id; } }
    return INDEX_NONE;
}

/** A checkpoint at the start of the first fight, resumed in the harness. */
bool ResumeInFight(FAutomationTestBase& Test, ExpeditionInputHarness::FHarness& Game, const TCHAR* Where)
{
    if (!Test.TestTrue(FString(Where) + TEXT(": input, board and HUD are ready"), Game.IsReady())) { return false; }
    Game.Frame();
    Game.Press(EKeys::O);
    Game.Settle();
    return Test.TestTrue(FString(Where) + TEXT(": the fight resumes"), Game.View->InCombat());
}

FExpeditionSaveData FightStart(int32 Seed)
{
    FExpeditionSaveData Data; FString Error;
    FExpeditionRun Run;
    if (Run.NewRun(Seed, Error, FString::Printf(TEXT("result-banner-%d"), Seed)) && Run.ChooseRoute(Run.Progress().Offers[0].Id, Error)) { Data.Current = Run.Snapshot(); }
    return Data;
}

/** The party passes every turn until the enemies win, leaving the defeat's playback pending. */
bool LoseBattle(ExpeditionInputHarness::FHarness& Game)
{
    AExpeditionBattleView* View = Game.View;
    for (int32 Step = 0; Step < 900 && View->InCombat(); ++Step)
    {
        View->SkipPresentation();
        Game.Frame();
        const FCombatantState* Active = View->Rules().Find(View->Rules().ActiveId());
        if (!Active || !View->InCombat()) { break; }
        if (Active->Side == ECombatSide::Enemy) { View->Tick(1.0f); continue; }
        View->Execute({ECombatCommandType::EndTurn, Active->Id});
    }
    return !View->InCombat();
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FResultBannersTest, "Project.Expedition.Presentation.ResultBannersDrawAfterTheFight",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FResultBannersTest::RunTest(const FString& Parameters)
{
    using namespace ResultBannerTest;
    const FExpeditionSaveData Data = FightStart(42);
    if (!TestTrue(TEXT("A seed reaches the first fight"), Data.Current.Progress.Phase == EExpeditionPhase::Combat)) { return false; }
    {
        ExpeditionInputHarness::FHarness Game(*this, TEXT("VictoryBanner"), &Data);
        if (!ResumeInFight(*this, Game, TEXT("Victory")) || !TestTrue(TEXT("The party wins"), Game.WinBattle())) { return false; }
        if (!TestTrue(TEXT("The reward phase is committed and the victory is presenting"), Game.View->Progress().Phase == EExpeditionPhase::Reward && Game.View->IsPresenting())) { return false; }
        // The reward screen waits for the banner: no reward card is drawn on any frame that draws the banner.
        bool bBanner = false, bCardsWithBanner = false;
        for (int32 Frame = 0; Frame < 600 && Game.View->IsPresenting(); ++Frame)
        {
            Game.Frame(0.05f);
            if (Game.Hud->BannerDrawn() == TEXT("VICTORY")) { bBanner = true; bCardsWithBanner |= !Game.Hud->ProgressionDrawn().Cards.IsEmpty(); }
        }
        Game.Frame(0.0f);
        TestTrue(TEXT("The VICTORY banner is drawn after the fight"), bBanner);
        TestFalse(TEXT("The reward cards never draw under it"), bCardsWithBanner);
        TestTrue(TEXT("The reward screen then takes over, with no banner"), Game.Hud->BannerDrawn().IsEmpty() && Game.Hud->ProgressionDrawn().Cards.Num() == 3);
    }
    {
        ExpeditionInputHarness::FHarness Game(*this, TEXT("DefeatBanner"), &Data);
        if (!ResumeInFight(*this, Game, TEXT("Defeat")) || !TestTrue(TEXT("The party loses"), LoseBattle(Game))) { return false; }
        if (!TestTrue(TEXT("The defeat is committed and presenting"), Game.View->Progress().Phase == EExpeditionPhase::Defeat && Game.View->IsPresenting())) { return false; }
        bool bBanner = false, bButtonsWithBanner = false;
        for (int32 Frame = 0; Frame < 600 && Game.View->IsPresenting(); ++Frame)
        {
            Game.Frame(0.05f);
            if (Game.Hud->BannerDrawn() == TEXT("DEFEAT")) { bBanner = true; bButtonsWithBanner |= Game.Hud->MenuButtons().Contains(FName(TEXT("Builder"))); }
        }
        Game.Frame(0.0f);
        TestTrue(TEXT("The DEFEAT banner is drawn after the fight"), bBanner);
        TestFalse(TEXT("The ending screen never draws under it"), bButtonsWithBanner);
        TestTrue(TEXT("The ending screen then takes over, with no banner"), Game.Hud->BannerDrawn().IsEmpty() && Game.Hud->MenuButtons().Contains(FName(TEXT("Builder"))));
    }
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRewardLineAfterCampTest, "Project.Expedition.Presentation.RewardLineNamesTheCampOnlyWhenOffered",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRewardLineAfterCampTest::RunTest(const FString& Parameters)
{
    using namespace ResultBannerTest;
    // A seed whose scripted run reaches reward 1 (the camp is next) and, after the camp, reward 2 (the camp is spent).
    FExpeditionSaveData First, Second;
    bool bFound = false;
    for (int32 Seed = 42; Seed < 300 && !bFound; ++Seed)
    {
        FExpeditionRun Run; FString Error;
        if (!Run.NewRun(Seed, Error, FString::Printf(TEXT("result-line-%d"), Seed)) || !Run.ChooseRoute(Run.Progress().Offers[0].Id, Error) || !FinishCombat(Run) || Run.Progress().Phase != EExpeditionPhase::Reward) { continue; }
        First.Current = Run.Snapshot();
        if (!Run.ClaimReward(FirstSurvivor(Run), Error) || !Run.Recover(true, Error) || !Run.ChooseRoute(Run.Progress().Offers[0].Id, Error) || !FinishCombat(Run) || Run.Progress().Phase != EExpeditionPhase::Reward) { continue; }
        Second.Current = Run.Snapshot();
        bFound = true;
    }
    if (!TestTrue(TEXT("A seed reaches reward 1 and reward 2"), bFound)) { return false; }
    {
        ExpeditionInputHarness::FHarness Game(*this, TEXT("RewardLine1"), &First);
        Game.Frame(); Game.Press(EKeys::O); Game.Settle();
        if (!TestTrue(TEXT("Reward 1 resumes"), Game.IsReady() && Game.View->Progress().Phase == EExpeditionPhase::Reward)) { return false; }
        Game.View->ClaimReward(FirstSurvivor(Game.View->CurrentRun()));
        TestEqual(TEXT("The first claim leads to the camp"), Game.View->Progress().Phase, EExpeditionPhase::Recovery);
        TestTrue(TEXT("and the line says so"), !Game.View->Messages().IsEmpty() && Game.View->Messages().Last().Contains(TEXT("Continue at camp")));
    }
    {
        ExpeditionInputHarness::FHarness Game(*this, TEXT("RewardLine2"), &Second);
        Game.Frame(); Game.Press(EKeys::O); Game.Settle();
        if (!TestTrue(TEXT("Reward 2 resumes"), Game.IsReady() && Game.View->Progress().Phase == EExpeditionPhase::Reward)) { return false; }
        Game.View->ClaimReward(FirstSurvivor(Game.View->CurrentRun()));
        TestEqual(TEXT("A later claim leads to the next route"), Game.View->Progress().Phase, EExpeditionPhase::Route);
        const FString Line = Game.View->Messages().IsEmpty() ? FString() : Game.View->Messages().Last();
        TestTrue(TEXT("and the line does not offer a camp that is spent"), !Line.IsEmpty() && !Line.Contains(TEXT("camp")) && Line.Contains(TEXT("Choose the next route")));
    }
    return !HasAnyErrors();
}
#endif
