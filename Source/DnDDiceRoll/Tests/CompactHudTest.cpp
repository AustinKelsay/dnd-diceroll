#if WITH_DEV_AUTOMATION_TESTS
#include "ExpeditionInputHarness.h"
#include "Party/BuildCatalog.h"
#include "Presentation/HudCoverage.h"

/**
 * #146: the compact HUD gives the world more of the screen without losing a control. Every button
 * the HUD registers must lie inside the viewport, and the always-on combat HUD (the panels and
 * buttons it registered, overlaps counted once) must stay within the persistent-area budget.
 */
namespace
{
constexpr double PersistentBudget = 0.18;
const FIntPoint Viewports[] = {FIntPoint(1280, 720), FIntPoint(1920, 1080), FIntPoint(2560, 1080), FIntPoint(1024, 576)};

void CheckInside(FAutomationTestBase& Test, ExpeditionInputHarness::FHarness& Game, const FString& Screen)
{
    for (const FIntPoint Size : Viewports)
    {
        Game.Target.Size = Size;
        Game.Frame();
        const TArray<FBox2D> Rects = Game.Hud->ButtonRects();
        Test.TestTrue(FString::Printf(TEXT("%s at %dx%d registers controls"), *Screen, Size.X, Size.Y), Rects.Num() > 0);
        for (const FBox2D& Box : Rects)
        {
            Test.TestTrue(FString::Printf(TEXT("%s at %dx%d: button %s stays inside the viewport"), *Screen, Size.X, Size.Y, *Box.ToString()),
                Box.Min.X >= -0.01 && Box.Min.Y >= -0.01 && Box.Max.X <= Size.X + 0.01 && Box.Max.Y <= Size.Y + 0.01 && Box.Max.X > Box.Min.X && Box.Max.Y > Box.Min.Y);
        }
    }
    Game.Target.Size = FIntPoint(1280, 720);
    Game.Frame();
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUnionAreaTest, "Project.Expedition.Presentation.HudCoverageArithmetic",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FUnionAreaTest::RunTest(const FString& Parameters)
{
    using LayoutRect = ExpeditionLayout::Rect;
    const LayoutRect Screen = {0, 0, 100, 100};
    auto Area = [&Screen](std::initializer_list<LayoutRect> Boxes) { return ExpeditionLayout::UnionArea(std::vector<LayoutRect>(Boxes), Screen); };
    TestEqual(TEXT("Nothing covers nothing"), Area({}), 0.0);
    TestEqual(TEXT("One box"), Area({LayoutRect{10, 10, 30, 20}}), 200.0);
    TestEqual(TEXT("A box inside another counts once"), Area({LayoutRect{0, 0, 50, 50}, LayoutRect{10, 10, 20, 20}}), 2500.0);
    TestEqual(TEXT("Overlapping boxes count the overlap once"), Area({LayoutRect{0, 0, 20, 20}, LayoutRect{10, 10, 30, 30}}), 700.0);
    TestEqual(TEXT("Disjoint boxes add"), Area({LayoutRect{0, 0, 10, 10}, LayoutRect{50, 50, 60, 70}}), 300.0);
    TestEqual(TEXT("Anything outside the viewport is clipped"), Area({LayoutRect{-50, -50, 10, 10}, LayoutRect{90, 90, 200, 200}}), 200.0);
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCompactHudTest, "Project.Expedition.Presentation.CompactHudBudgetAndBounds",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FCompactHudTest::RunTest(const FString& Parameters)
{
    using ExpeditionInputHarness::FHarness;
    FHarness Game(*this, TEXT("CompactHud"));
    if (!TestTrue(TEXT("Input, board and HUD are ready"), Game.IsReady())) { return false; }
    Game.Frame();
    CheckInside(*this, Game, TEXT("Start screen"));
    // Every class's full subclass list registers and stays on screen (cleric and wizard have ten).
    {
        const FString Original = Game.Player->NextParty()[Game.Player->BuilderSlot()].BuildId;
        TSet<FString> Seen;
        for (const auto& Build : FBuildCatalog::All())
        {
            const FString ClassId = Build.Profile.ClassId;
            if (Seen.Contains(ClassId)) { continue; }
            Seen.Add(ClassId);
            Game.Player->HandleButton(FName(*(TEXT("Class:") + ClassId)));
            Game.Frame();
            int32 Expected = 0, Shown = 0;
            for (const auto& Other : FBuildCatalog::All()) { Expected += Other.Profile.ClassId == ClassId ? 1 : 0; }
            for (const FName Id : Game.Hud->MenuButtons()) { Shown += Id.ToString().StartsWith(TEXT("Build:")) ? 1 : 0; }
            TestEqual(FString::Printf(TEXT("Start screen, %s: every subclass has a control"), *ClassId), Shown, Expected);
            CheckInside(*this, Game, FString::Printf(TEXT("Start screen, %s"), *ClassId));
        }
        TestTrue(TEXT("The catalog has more than one class"), Seen.Num() > 1);
        // Restore the starting build so the rest of the test plays the default party.
        if (const auto* Start = FBuildCatalog::Find(Original)) { Game.Player->HandleButton(FName(*(TEXT("Class:") + Start->Profile.ClassId))); }
        Game.Player->HandleButton(FName(*(TEXT("Build:") + Original)));
        Game.Frame();
        TestEqual(TEXT("The starting build is restored"), Game.Player->NextParty()[Game.Player->BuilderSlot()].BuildId, Original);
    }
    // The start screen is two drawers: the centre of the screen stays open for the tabletop.
    {
        Game.Target.Size = FIntPoint(1920, 1080);
        Game.Frame();
        const FVector2D Size = Game.Hud->ViewportSize();
        bool bCentreOpen = true;
        for (const FBox2D& Box : Game.Hud->ButtonRects()) { bCentreOpen &= !(Box.Min.X < Size.X * 0.5 && Box.Max.X > Size.X * 0.5); }
        TestTrue(TEXT("No start-screen control crosses the centre line"), bCentreOpen);
        Game.Target.Size = FIntPoint(1280, 720);
        Game.Frame();
    }

    Game.Press(EKeys::Enter);
    if (!TestTrue(TEXT("Enter on Begin starts the expedition"), Game.View->HasRun())) { return false; }
    Game.Settle();
    CheckInside(*this, Game, TEXT("Route"));
    Game.Press(EKeys::Enter);
    if (!TestTrue(TEXT("The first route starts an encounter"), Game.View->InCombat())) { return false; }
    Game.PartyTurn();
    if (!TestTrue(TEXT("The party has the turn"), Game.View->InCombat() && Game.Player->ShowsTargeting())) { return false; }
    CheckInside(*this, Game, TEXT("Party turn"));
    // The acting hero's full status is keyboard reachable: choosing them again toggles it.
    {
        const int32 Active = Game.View->Rules().ActiveId();
        int32 Index = 0, ActiveIndex = INDEX_NONE;
        for (const FCombatantState& Unit : Game.View->Rules().Snapshot().Encounter.Combatants)
        {
            if (Unit.Side != ECombatSide::Party) { continue; }
            if (Unit.Id == Active) { ActiveIndex = Index; }
            ++Index;
        }
        const FKey Keys[] = {EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four};
        if (TestTrue(TEXT("A party hero acts"), ActiveIndex >= 0 && ActiveIndex < 4))
        {
            Game.Frame();
            TestEqual(TEXT("The acting hero's details stay closed by default"), Game.Hud->PartyDetailUnit(), int32(INDEX_NONE));
            Game.Press(Keys[ActiveIndex]);
            TestEqual(TEXT("Choosing the acting hero again opens their details"), Game.Hud->PartyDetailUnit(), Active);
            Game.Press(Keys[ActiveIndex]);
            TestEqual(TEXT("Choosing them once more closes the details"), Game.Hud->PartyDetailUnit(), int32(INDEX_NONE));
        }
    }

    // Persistent area: the party turn is the busiest combat state (cards, strips, dock and log all up).
    for (const FIntPoint Size : {FIntPoint(1920, 1080), FIntPoint(1280, 720)})
    {
        Game.Target.Size = Size;
        Game.Frame();
        const double Coverage = Game.Hud->PersistentCoverage();
        AddInfo(FString::Printf(TEXT("Persistent combat HUD at %dx%d covers %.2f%% of the screen"), Size.X, Size.Y, Coverage * 100.0));
        TestTrue(FString::Printf(TEXT("Persistent combat HUD at %dx%d stays within %.0f%%"), Size.X, Size.Y, PersistentBudget * 100.0), Coverage > 0.0 && Coverage <= PersistentBudget);
        const FVector2D Viewport = Game.Hud->ViewportSize();
        TestTrue(TEXT("The HUD reports the viewport it drew into"), FMath::IsNearlyEqual(Viewport.X / Viewport.Y, double(Size.X) / Size.Y, 0.01));
    }
    // Targeting adds chips and markers, never persistent panels.
    Game.Press(EKeys::F);
    for (const FIntPoint Size : {FIntPoint(1920, 1080), FIntPoint(1280, 720)})
    {
        Game.Target.Size = Size;
        Game.Frame();
        TestTrue(FString::Printf(TEXT("Persistent HUD with a selected action at %dx%d stays within the budget"), Size.X, Size.Y), Game.Hud->PersistentCoverage() <= PersistentBudget);
    }
    Game.Target.Size = FIntPoint(1280, 720);
    Game.Frame();

    struct FOverlay { FKey Open; FKey Close; const TCHAR* Name; };
    const FOverlay Overlays[] = {{EKeys::Slash, EKeys::Escape, TEXT("Help")}, {EKeys::H, EKeys::H, TEXT("Records")}, {EKeys::BackSpace, EKeys::Escape, TEXT("Return question")}};
    for (const FOverlay& Overlay : Overlays)
    {
        Game.Press(Overlay.Open);
        CheckInside(*this, Game, Overlay.Name);
        Game.Press(Overlay.Close);
    }
    TestFalse(TEXT("The overlays are closed again"), Game.Player->OverlayOpen());
    return !HasAnyErrors();
}
#endif
