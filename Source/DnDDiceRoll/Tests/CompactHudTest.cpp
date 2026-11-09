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

namespace CompactHudMenuTest
{
const FName Toggle(TEXT("MenuToggle"));
const TCHAR* ItemNames[] = {TEXT("Save"), TEXT("Resume"), TEXT("Records"), TEXT("Speed"), TEXT("Help"), TEXT("Menu")};

bool HasAll(const ExpeditionInputHarness::FHarness& Game)
{
    const TArray<FName> Live = Game.Hud->MenuButtons();
    bool bAll = Live.Contains(Toggle);
    for (const TCHAR* Name : ItemNames) { bAll &= Live.Contains(FName(Name)); }
    return bAll;
}
bool HasAny(const ExpeditionInputHarness::FHarness& Game)
{
    const TArray<FName> Live = Game.Hud->MenuButtons();
    bool bAny = false;
    for (const TCHAR* Name : ItemNames) { bAny |= Live.Contains(FName(Name)); }
    return bAny;
}
}

/**
 * #146: the secondary controls live in one Menu. It opens by key (I) and by its button, lists every
 * control inside the viewport, closes by Esc, a second press or a choice, restores focus, and leaves
 * every hotkey working with the Menu closed or open. It is a transient layer: it never adds to the
 * persistent budget.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCompactHudMenuTest, "Project.Expedition.Presentation.CompactHudMenu",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FCompactHudMenuTest::RunTest(const FString& Parameters)
{
    using namespace CompactHudMenuTest;
    using ExpeditionInputHarness::FHarness;
    FHarness Game(*this, TEXT("CompactHudMenu"));
    if (!TestTrue(TEXT("Input, board and HUD are ready"), Game.IsReady())) { return false; }
    Game.Frame();
    Game.Press(EKeys::I);
    TestFalse(TEXT("The start screen has no Menu to open"), Game.Player->ShowingMenu() || Game.Hud->MenuButtons().Contains(Toggle));
    Game.Press(EKeys::Enter);
    if (!TestTrue(TEXT("Enter on Begin starts the expedition"), Game.View->HasRun())) { return false; }
    Game.Settle();

    // Closed: one button, none of the old strip.
    TestTrue(TEXT("The route screen shows the Menu button"), Game.Hud->MenuButtons().Contains(Toggle));
    TestFalse(TEXT("The secondary controls are not a persistent strip"), HasAny(Game));
    TestEqual(TEXT("The route screen starts on its primary choice"), Game.Player->MenuFocus(), FName(TEXT("Route0")));

    // By key: opens, lists everything on screen, focuses Save, closes by Esc and restores focus.
    Game.Press(EKeys::I);
    if (!TestTrue(TEXT("I opens the Menu"), Game.Player->ShowingMenu())) { return false; }
    TestFalse(TEXT("The Menu is not an overlay: gameplay input stays on"), Game.Player->OverlayOpen());
    TestTrue(TEXT("The open Menu registers the toggle and all six items"), HasAll(Game));
    TestEqual(TEXT("The open Menu focuses Save"), Game.Player->MenuFocus(), FName(TEXT("Save")));
    CheckInside(*this, Game, TEXT("Open menu"));
    Game.Press(EKeys::Tab);
    TestEqual(TEXT("Tab moves to Resume"), Game.Player->MenuFocus(), FName(TEXT("Resume")));
    Game.Press(EKeys::Escape);
    TestFalse(TEXT("Esc closes the Menu"), Game.Player->ShowingMenu());
    TestFalse(TEXT("The closed Menu drops its items"), HasAny(Game));
    TestEqual(TEXT("Closing restores the route focus"), Game.Player->MenuFocus(), FName(TEXT("Route0")));

    // A second press closes it, and the button does both.
    Game.Press(EKeys::I);
    Game.Press(EKeys::I);
    TestFalse(TEXT("A second I closes the Menu"), Game.Player->ShowingMenu());
    Game.Player->HandleButton(Toggle);
    Game.Frame();
    TestTrue(TEXT("The Menu button opens the Menu"), Game.Player->ShowingMenu());
    Game.Player->HandleButton(Toggle);
    Game.Frame();
    TestFalse(TEXT("The Menu button closes the Menu"), Game.Player->ShowingMenu());
    TestEqual(TEXT("The button restores the route focus too"), Game.Player->MenuFocus(), FName(TEXT("Route0")));

    // Choosing an item closes the Menu; Help then closes back to the route with its focus.
    Game.Press(EKeys::I);
    Game.Player->HandleButton(TEXT("Help"));
    Game.Frame();
    TestTrue(TEXT("Choosing Help opens Help and closes the Menu"), Game.Player->ShowingHelp() && !Game.Player->ShowingMenu());
    Game.Press(EKeys::Escape);
    TestTrue(TEXT("Closing Help does not bring the Menu back"), !Game.Player->ShowingHelp() && !Game.Player->ShowingMenu());
    TestEqual(TEXT("Help restores the route focus"), Game.Player->MenuFocus(), FName(TEXT("Route0")));

    // Keyboard only: Save, Resume, Records, then Speed with Enter.
    const EPlaybackSpeed Speed = Game.View->Speed();
    Game.Press(EKeys::I);
    for (int32 Step = 0; Step < 3; ++Step) { Game.Press(EKeys::Tab); }
    TestEqual(TEXT("Three Tabs reach Speed"), Game.Player->MenuFocus(), FName(TEXT("Speed")));
    Game.Press(EKeys::Enter);
    TestTrue(TEXT("Enter on Speed changes the speed and closes the Menu"), Game.View->Speed() != Speed && !Game.Player->ShowingMenu());
    TestEqual(TEXT("Choosing an item restores the route focus"), Game.Player->MenuFocus(), FName(TEXT("Route0")));

    // Hotkeys with the Menu closed.
    const EPlaybackSpeed Before = Game.View->Speed();
    Game.Press(EKeys::T);
    TestTrue(TEXT("T changes the speed with the Menu closed"), Game.View->Speed() != Before);
    Game.Press(EKeys::H);
    TestTrue(TEXT("H opens Records with the Menu closed"), Game.View->ShowingRecords());
    Game.Press(EKeys::H);
    Game.Press(EKeys::Slash);
    TestTrue(TEXT("? opens Help with the Menu closed"), Game.Player->ShowingHelp());
    Game.Press(EKeys::Escape);
    // ... and with it open.
    Game.Press(EKeys::I);
    const EPlaybackSpeed Open = Game.View->Speed();
    Game.Press(EKeys::T);
    TestTrue(TEXT("T still changes the speed with the Menu open"), Game.View->Speed() != Open);
    TestFalse(TEXT("The hotkey closes the Menu"), Game.Player->ShowingMenu());
    Game.Press(EKeys::I);
    Game.Press(EKeys::H);
    TestTrue(TEXT("H still opens Records with the Menu open"), Game.View->ShowingRecords() && !Game.Player->ShowingMenu());
    Game.Press(EKeys::H);
    TestEqual(TEXT("Records restores the route focus"), Game.Player->MenuFocus(), FName(TEXT("Route0")));

    // Combat: focus stays empty, the Menu takes it and gives it back, and coverage stays in budget.
    Game.Press(EKeys::Enter);
    if (!TestTrue(TEXT("The first route starts an encounter"), Game.View->InCombat())) { return false; }
    Game.PartyTurn();
    if (!TestTrue(TEXT("The party has the turn"), Game.View->InCombat() && Game.Player->ShowsTargeting())) { return false; }
    CheckInside(*this, Game, TEXT("Party turn"));
    for (const FIntPoint Size : {FIntPoint(1920, 1080), FIntPoint(1280, 720)})
    {
        Game.Target.Size = Size;
        Game.Frame();
        const double Closed = Game.Hud->PersistentCoverage();
        Game.Press(EKeys::I);
        if (!TestTrue(TEXT("I opens the Menu in combat"), Game.Player->ShowingMenu())) { break; }
        TestEqual(TEXT("The combat Menu focuses Save"), Game.Player->MenuFocus(), FName(TEXT("Save")));
        TestTrue(TEXT("The combat Menu lists every control"), HasAll(Game));
        const double Open = Game.Hud->PersistentCoverage();
        AddInfo(FString::Printf(TEXT("Persistent combat HUD at %dx%d: %.2f%% closed, %.2f%% with the Menu open"), Size.X, Size.Y, Closed * 100.0, Open * 100.0));
        TestTrue(FString::Printf(TEXT("The Menu button keeps persistent coverage within %.0f%% at %dx%d"), PersistentBudget * 100.0, Size.X, Size.Y), Closed > 0.0 && Closed <= PersistentBudget);
        TestTrue(TEXT("An open Menu is transient and adds nothing to persistent coverage"), Open <= Closed + 1e-6);
        CheckInside(*this, Game, TEXT("Open menu in combat"));
        Game.Press(EKeys::Escape);
        TestFalse(TEXT("Esc closes the combat Menu"), Game.Player->ShowingMenu());
        TestEqual(TEXT("Combat holds no menu focus after closing"), Game.Player->MenuFocus(), FName());
    }
    Game.Target.Size = FIntPoint(1280, 720);
    Game.Frame();
    // Enter inside the combat Menu acts on the focused item rather than the board cursor.
    Game.Press(EKeys::I);
    Game.Press(EKeys::Enter);
    TestFalse(TEXT("Enter on Save closes the combat Menu"), Game.Player->ShowingMenu());
    // The return question replaces the Menu.
    Game.Press(EKeys::I);
    Game.Press(EKeys::BackSpace);
    TestTrue(TEXT("Backspace with the Menu open asks to return"), Game.Player->ConfirmingReturn() && !Game.Player->ShowingMenu());
    Game.Press(EKeys::Escape);
    TestFalse(TEXT("Esc cancels the question"), Game.Player->ConfirmingReturn());
    return !HasAnyErrors();
}
#endif
