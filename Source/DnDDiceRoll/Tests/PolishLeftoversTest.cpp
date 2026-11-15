#if WITH_DEV_AUTOMATION_TESTS
#include "ExpeditionInputHarness.h"
#include "Presentation/TargetChipLayout.h"
#include "Run/ProgressionQuery.h"

/** #252 (from #209): the SLAIN text does not linger on the reward screen, and the turn banner does not dim the hero cards. */
namespace PolishLeftoversTest
{
FExpeditionSaveData FightStart(int32 Seed)
{
    FExpeditionSaveData Data; FString Error;
    FExpeditionRun Run;
    if (Run.NewRun(Seed, Error, FString::Printf(TEXT("polish-%d"), Seed)) && Run.ChooseRoute(Run.Progress().Offers[0].Id, Error)) { Data.Current = Run.Snapshot(); }
    return Data;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSlainTextClearsAtRewardTest, "Project.Expedition.Presentation.SlainTextDoesNotLingerOnTheRewardScreen",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSlainTextClearsAtRewardTest::RunTest(const FString& Parameters)
{
    const FExpeditionSaveData Data = PolishLeftoversTest::FightStart(42);
    if (!TestTrue(TEXT("A seed reaches the first fight"), Data.Current.Progress.Phase == EExpeditionPhase::Combat)) { return false; }
    ExpeditionInputHarness::FHarness Game(*this, TEXT("SlainText"), &Data);
    if (!TestTrue(TEXT("Input, board and HUD are ready"), Game.IsReady())) { return false; }
    Game.Frame(); Game.Press(EKeys::O); Game.Settle();
    if (!TestTrue(TEXT("The fight resumes and the party wins it"), Game.View->InCombat() && Game.WinBattle())) { return false; }
    bool bSlain = false;
    for (int32 Frame = 0; Frame < 600 && Game.View->IsPresenting(); ++Frame)
    {
        Game.Frame(0.05f);
        for (const FFloatingText& Floater : Game.View->Playback().Texts()) { bSlain |= Floater.Text == TEXT("SLAIN"); }
    }
    Game.Frame(0.0f);
    TestTrue(TEXT("The kill showed its SLAIN text during the fight"), bSlain);
    TestTrue(TEXT("The reward screen is up"), Game.Hud->ProgressionDrawn().Cards.Num() == 3);
    TestTrue(TEXT("and no floating text from the fight is still on it"), Game.View->Playback().Texts().IsEmpty());
    // A claim's own texts still show on the screens that follow.
    Game.View->ClaimReward(Game.View->Rules().Snapshot().Encounter.Combatants.FindByPredicate([](const FCombatantState& U) { return U.Side == ECombatSide::Party && U.Life == ECombatLife::Conscious; })->Id);
    TestFalse(TEXT("The claim's gain text is shown"), Game.View->Playback().Texts().IsEmpty());
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBannerKeepsOffHeroCardsTest, "Project.Expedition.Presentation.TurnBannerDoesNotDimTheHeroCards",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FBannerKeepsOffHeroCardsTest::RunTest(const FString& Parameters)
{
    const FExpeditionSaveData Data = PolishLeftoversTest::FightStart(42);
    if (!TestTrue(TEXT("A seed reaches the first fight"), Data.Current.Progress.Phase == EExpeditionPhase::Combat)) { return false; }
    for (const FIntPoint Size : {FIntPoint(1280, 720), FIntPoint(1920, 1080), FIntPoint(1024, 768)})
    {
        ExpeditionInputHarness::FHarness Game(*this, TEXT("BannerCards"), &Data);
        if (!TestTrue(TEXT("Input, board and HUD are ready"), Game.IsReady())) { return false; }
        Game.Target.Size = Size;
        Game.Frame(); Game.Press(EKeys::O);
        const FString Where = FString::Printf(TEXT("%dx%d"), Size.X, Size.Y);
        const FBox2D* Band = nullptr;
        for (int32 Frame = 0; Frame < 4 && !(Band = Game.Hud->BannerRequested()); ++Frame) { Game.Frame(0.05f); }
        if (!TestTrue(Where + TEXT(": a banner is drawn on the resumed fight"), Band != nullptr)) { continue; }
        const double Scale = ExpeditionLayout::ViewScale(Size.X, Size.Y);
        const FBox2D Dimmed = *Band;
        const int32 Heroes = Game.View->Rules().Snapshot().Encounter.Combatants.FilterByPredicate([](const FCombatantState& U) { return U.Side == ECombatSide::Party; }).Num();
        for (int32 Slot = 0; Slot < Heroes; ++Slot)
        {
            const FBox2D Card(FVector2D(ExpeditionLayout::PartyCardX, ExpeditionLayout::PartyCardTop + Slot * ExpeditionLayout::PartyCardPitch) * Scale,
                FVector2D(ExpeditionLayout::PartyCardX + ExpeditionLayout::PartyCardWidth, ExpeditionLayout::PartyCardTop + Slot * ExpeditionLayout::PartyCardPitch + ExpeditionLayout::PartyCardHeight) * Scale);
            TestFalse(FString::Printf(TEXT("%s: the banner's dimmed band stays off hero card %d"), *Where, Slot + 1), Dimmed.Intersect(Card));
        }
    }
    return !HasAnyErrors();
}
#endif
