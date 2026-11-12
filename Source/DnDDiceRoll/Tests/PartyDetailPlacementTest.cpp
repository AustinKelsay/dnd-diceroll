#if WITH_DEV_AUTOMATION_TESTS
#include "ExpeditionInputHarness.h"
#include "Presentation/OverviewLayout.h"

/**
 * #200: a hero's details panel (DrawPartyDetail) used to be drawn before the route banner, the route cards and the reward and camp panels, which then painted over
 * it, and at the design width it also lay under them by geometry. Now it keeps clear of them, derived from their own rectangles (TargetChipLayout.h).
 */
namespace PartyDetailPlacementTest
{
using FDesignRect = ExpeditionLayout::Rect;
using FNamed = TPair<FString, FDesignRect>;

// 1920x1080 and 1280x720 are the same 1280x720 design space at two scales, 1600x600 is a short, wide window (1920x720 design units).
const FIntPoint Sizes[] = {FIntPoint(1920, 1080), FIntPoint(1280, 720), FIntPoint(1600, 600)};
constexpr double Clearance = 4.0;

FDesignRect ToDesign(const FBox2D& Box, double Scale) { return {Box.Min.X / Scale, Box.Min.Y / Scale, Box.Max.X / Scale, Box.Max.Y / Scale}; }

bool KeepsClear(FAutomationTestBase& Test, const FString& Where, const FDesignRect& Panel, double W, double H, const TArray<FNamed>& Others)
{
    bool bOk = Test.TestTrue(Where + TEXT(": on screen"), Panel.Left >= 0 && Panel.Top >= 0 && Panel.Right <= W && Panel.Bottom <= H);
    for (const TPair<FString, FDesignRect>& Other : Others)
    { bOk &= Test.TestFalse(FString::Printf(TEXT("%s: clear of %s (panel %.0f,%.0f to %.0f,%.0f)"), *Where, *Other.Key, Panel.Left, Panel.Top, Panel.Right, Panel.Bottom), ExpeditionLayout::Overlaps(Panel, Other.Value, Clearance)); }
    return bOk;
}

TArray<FNamed> PartyCards(int32 Skip = INDEX_NONE)
{
    TArray<FNamed> Out;
    for (int32 Slot = 0; Slot < 4; ++Slot)
    {
        const double Top = ExpeditionLayout::PartyCardTop + Slot * ExpeditionLayout::PartyCardPitch;
        if (Slot != Skip) { Out.Add(FNamed(FString::Printf(TEXT("party card %d"), Slot + 1), FDesignRect{ExpeditionLayout::PartyCardX, Top, ExpeditionLayout::PartyCardX + ExpeditionLayout::PartyCardWidth, Top + ExpeditionLayout::PartyCardHeight})); }
    }
    return Out;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPartyDetailPlacementGeometryTest, "Project.Expedition.Presentation.PartyDetailsKeepClearOfPhasePanels",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FPartyDetailPlacementGeometryTest::RunTest(const FString& Parameters)
{
    using namespace PartyDetailPlacementTest;
    // The pure placement: every slot, from the shortest panel to the tallest, on the route screen, the reward and the camp, at the three sizes.
    for (const FIntPoint Size : Sizes)
    {
        const double Scale = ExpeditionLayout::ViewScale(Size.X, Size.Y), W = Size.X / Scale, H = Size.Y / Scale;
        const TArray<FNamed> Blocks = {{TEXT("route banner"), ExpeditionLayout::RouteBannerRect(W, H)}, {TEXT("route card 1"), ExpeditionLayout::RouteCardRect(W, H, 0)},
            {TEXT("route card 2"), ExpeditionLayout::RouteCardRect(W, H, 1)}, {TEXT("reward panel"), ExpeditionLayout::RewardPanelRect(W, H)}, {TEXT("camp panel"), ExpeditionLayout::CampPanelRect(W, H)}};
        const FDesignRect Route = ExpeditionLayout::RouteBlockRect(W, H);
        for (int32 Which = 0; Which < 3; ++Which)
        {
            const FDesignRect Block = Which == 0 ? Route : Which == 1 ? ExpeditionLayout::RewardPanelRect(W, H) : ExpeditionLayout::CampPanelRect(W, H);
            for (int32 Slot = 0; Slot < 4; ++Slot)
            {
                for (int32 Lines = 3; Lines <= ExpeditionLayout::PartyDetailMaxLines; ++Lines)
                {
                    const FString Where = FString::Printf(TEXT("%dx%d, %s, slot %d, %d lines"), Size.X, Size.Y, Which == 0 ? TEXT("route") : Which == 1 ? TEXT("reward") : TEXT("camp"), Slot + 1, Lines);
                    const FDesignRect Placed = ExpeditionLayout::PartyDetailClearOf(ExpeditionLayout::PartyDetailRect(Slot, Lines), Block, W, H);
                    TArray<FNamed> Others;
                    Others.Add({TEXT("the phase's block"), Block});
                    if (Which == 0) { Others.Append({Blocks[0], Blocks[1], Blocks[2]}); }
                    Others.Append(PartyCards());
                    KeepsClear(*this, Where, Placed, W, H, Others);
                    TestEqual(Where + TEXT(": the panel keeps its size"), Placed.Bottom - Placed.Top, ExpeditionLayout::PartyDetailRect(Slot, Lines).Bottom - ExpeditionLayout::PartyDetailRect(Slot, Lines).Top);
                }
            }
        }
        // The overview's HUD rectangles hold where the panels really go, so the dioramas cannot be placed under a moved panel either.
        const TArray<FDesignRect> Hud = OverviewLayout::HudRects(true, W, H);
        for (int32 Slot = 0; Slot < 4; ++Slot)
        {
            const FDesignRect Placed = ExpeditionLayout::PartyDetailClearOf(ExpeditionLayout::PartyDetailRect(Slot, ExpeditionLayout::PartyDetailMaxLines), Route, W, H);
            bool bListed = false;
            for (const FDesignRect& Listed : Hud) { bListed |= Listed.Left == Placed.Left && Listed.Top == Placed.Top && Listed.Right == Placed.Right && Listed.Bottom == Placed.Bottom; }
            TestTrue(FString::Printf(TEXT("%dx%d: the overview's HUD rectangles hold slot %d's placed details"), Size.X, Size.Y, Slot + 1), bListed);
        }
    }
    // The bug is real at the design width: unplaced, slots 2 to 4 lie under the banner or the left card.
    {
        const FDesignRect Route = ExpeditionLayout::RouteBlockRect(ExpeditionLayout::DesignWidth, ExpeditionLayout::DesignHeight);
        for (int32 Slot = 1; Slot < 4; ++Slot)
        { TestTrue(FString::Printf(TEXT("Unplaced, slot %d's details would meet the route block at the design width"), Slot + 1), ExpeditionLayout::Overlaps(ExpeditionLayout::PartyDetailRect(Slot, 5), Route)); }
    }
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPartyDetailPlacementHudTest, "Project.Expedition.Presentation.RouteHeroDetailsDrawClearOfBannerAndCards",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FPartyDetailPlacementHudTest::RunTest(const FString& Parameters)
{
    using namespace PartyDetailPlacementTest;
    // The real HUD on the route, reward and camp screens: select each hero in turn (the details follow the selection) at each size and check the panel the HUD drew,
    // at its natural height and at the tallest the overview layout allows for.
    FString Error;
    FExpeditionRun Start;
    Start.NewRun(20261030, Error, TEXT("party-detail-placement"));
    struct FCase { const TCHAR* Name; FExpeditionSnapshot Checkpoint; int32 Which; };
    TArray<FCase> Cases;
    Cases.Add({TEXT("route"), Start.Snapshot(), 0});
    for (int32 Seed = 1; Seed < 60 && Cases.Num() < 3; ++Seed)
    {
        FExpeditionRun Won;
        Won.NewRun(Seed, Error, FString::Printf(TEXT("party-detail-%d"), Seed));
        Won.ChooseRoute(Won.Progress().Offers[0].Id, Error);
        if (!ExpeditionTestSupport::FinishCombat(Won) || Won.Progress().Phase != EExpeditionPhase::Reward) { continue; }
        Cases.Add({TEXT("reward"), Won.Snapshot(), 1});
        for (const FCombatantState& Unit : Won.Battle().Snapshot().Encounter.Combatants)
        { if (Unit.Side == ECombatSide::Party && Unit.Life == ECombatLife::Conscious) { Won.ClaimReward(Unit.Id, Error); break; } }
        if (Won.Progress().Phase == EExpeditionPhase::Recovery) { Cases.Add({TEXT("camp"), Won.Snapshot(), 2}); }
    }
    if (!TestEqual(TEXT("Route, reward and camp checkpoints exist"), Cases.Num(), 3)) { return false; }
    for (const FCase& Case : Cases)
    {
        FExpeditionSaveData Data;
        Data.Current = Case.Checkpoint;
        ExpeditionInputHarness::FHarness Game(*this, TEXT("PartyDetailPlacement"), &Data);
        if (!TestTrue(FString::Printf(TEXT("%s: input, board and HUD are ready"), Case.Name), Game.IsReady())) { continue; }
        Game.Frame();
        Game.Press(EKeys::O);
        if (!TestTrue(FString::Printf(TEXT("%s: the checkpoint resumes"), Case.Name), Game.View->HasRun() && !Game.View->InCombat())) { continue; }
        Game.Settle();
        for (const bool bTallest : {false, true})
        {
            Game.Hud->PreviewDetailLines(bTallest ? ExpeditionLayout::PartyDetailMaxLines : 0);
            for (const FIntPoint Size : Sizes)
            {
                Game.Target.Size = Size;
                const double Scale = ExpeditionLayout::ViewScale(Size.X, Size.Y), W = Size.X / Scale, H = Size.Y / Scale;
                TArray<FNamed> Others;
                if (Case.Which == 0)
                {
                    Others.Add({TEXT("the route banner"), ExpeditionLayout::RouteBannerRect(W, H)});
                    Others.Add({TEXT("route card 1"), ExpeditionLayout::RouteCardRect(W, H, 0)});
                    Others.Add({TEXT("route card 2"), ExpeditionLayout::RouteCardRect(W, H, 1)});
                }
                else { Others.Add({Case.Which == 1 ? TEXT("the reward panel") : TEXT("the camp panel"), Case.Which == 1 ? ExpeditionLayout::RewardPanelRect(W, H) : ExpeditionLayout::CampPanelRect(W, H)}); }
                Others.Append(PartyCards());
                for (int32 Hero = 0; Hero < 4; ++Hero)
                {
                    const FString Where = FString::Printf(TEXT("%s, %dx%d, hero %d%s"), Case.Name, Size.X, Size.Y, Hero + 1, bTallest ? TEXT(", tallest") : TEXT(""));
                    Game.Player->SelectPartyIndex(Hero, false);
                    Game.Frame(0.0f);
                    Game.Frame(0.0f);
                    const TArray<FBox2D>& Panels = Game.Hud->PartyDetailRects();
                    if (!TestEqual(Where + TEXT(": one details panel is drawn"), Panels.Num(), 1)) { continue; }
                    KeepsClear(*this, Where, ToDesign(Panels[0], Scale), W, H, Others);
                }
                // One hero selected, another's card hovered: the hovered card's details are the one panel shown, so the selected hero's cannot take the same spot and
                // hide them (the review of #200).
                TArray<int32> PartyIds;
                for (const FCombatantState& Unit : Game.View->Rules().Snapshot().Encounter.Combatants) { if (Unit.Side == ECombatSide::Party) { PartyIds.Add(Unit.Id); } }
                for (int32 Selected = 0; Selected < PartyIds.Num(); ++Selected)
                {
                    for (int32 Hovered = 0; Hovered < PartyIds.Num(); ++Hovered)
                    {
                        if (Hovered == Selected) { continue; }
                        const FString Where = FString::Printf(TEXT("%s, %dx%d, hero %d selected and hero %d hovered%s"), Case.Name, Size.X, Size.Y, Selected + 1, Hovered + 1, bTallest ? TEXT(", tallest") : TEXT(""));
                        Game.Player->SelectPartyIndex(Selected, false);
                        Game.Player->SimulatePointer(FVector2D(ExpeditionLayout::PartyCardX + 100.0, ExpeditionLayout::PartyCardTop + Hovered * ExpeditionLayout::PartyCardPitch + 32.0) * Scale, FIntPoint(0, 0));
                        Game.Frame(0.0f);
                        Game.Frame(0.0f);
                        const TArray<FBox2D>& Panels = Game.Hud->PartyDetailRects();
                        if (!TestEqual(Where + TEXT(": one details panel is drawn, not two on one spot"), Panels.Num(), 1)) { continue; }
                        TestEqual(Where + TEXT(": it is the hovered hero's"), Game.Hud->PartyDetailUnit(), PartyIds[Hovered]);
                        KeepsClear(*this, Where, ToDesign(Panels[0], Scale), W, H, Others);
                    }
                }
                Game.Player->ReleaseSimulatedPointer();
                Game.Player->SelectPartyIndex(0, false);
            }
        }
        Game.Hud->PreviewDetailLines(0);
    }
    return !HasAnyErrors();
}
#endif
