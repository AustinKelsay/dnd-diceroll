#if WITH_DEV_AUTOMATION_TESTS
#include "ExpeditionInputHarness.h"

/**
 * Help, Records and the "Return to menu?" question each own the screen. Targeting chips and
 * reach markers belong to the board underneath, so none may be requested while one is open,
 * and all return when it closes (#50, found by the on-screen check of #132's confirmation).
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTargetingHiddenUnderOverlaysTest, "Project.Expedition.Presentation.TargetingHiddenUnderOverlays",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FTargetingHiddenUnderOverlaysTest::RunTest(const FString& Parameters)
{
    using ExpeditionInputHarness::FHarness;
    FHarness Game(*this, TEXT("TargetingOverlays"));
    if (!TestTrue(TEXT("Input, board and HUD are ready"), Game.IsReady())) { return false; }
    Game.Frame();
    Game.Press(EKeys::Enter);
    Game.Settle();
    Game.Press(EKeys::Enter);
    if (!TestTrue(TEXT("The first route starts an encounter"), Game.View->InCombat())) { return false; }
    Game.PartyTurn();
    if (!TestTrue(TEXT("The party has the turn"), Game.View->InCombat())) { return false; }

    // Some mode key selects an action with targets on the party's first turn (an ally swap, protection, a spell).
    bool bFound = false;
    for (const FKey& Key : {EKeys::F, EKeys::M, EKeys::B, EKeys::J, EKeys::R, EKeys::P, EKeys::A})
    {
        Game.Press(Key);
        if (Game.Hud->TargetChipsRequested() > 0 && Game.Hud->ReachMarkersRequested() > 0) { bFound = true; break; }
    }
    if (!TestTrue(TEXT("A selected action shows target chips and reach markers"), bFound)) { return false; }
    const int32 Chips = Game.Hud->TargetChipsRequested();
    const int32 Reach = Game.Hud->ReachMarkersRequested();
    const ECombatCommandType Action = Game.Player->Action();

    struct FOverlay { const TCHAR* Name; FKey Open; FKey Close; };
    const FOverlay Overlays[] = {
        {TEXT("Help"), EKeys::Slash, EKeys::Escape},
        {TEXT("Records"), EKeys::H, EKeys::H},
        {TEXT("Return to menu question"), EKeys::BackSpace, EKeys::Escape},
    };
    for (const FOverlay& Overlay : Overlays)
    {
        const FString Name = Overlay.Name;
        Game.Press(Overlay.Open);
        if (!TestTrue(Name + TEXT(" opens over the board"), Game.Player->OverlayOpen())) { continue; }
        TestEqual(Name + TEXT(" hides every target chip"), Game.Hud->TargetChipsRequested(), 0);
        TestEqual(Name + TEXT(" hides every reach marker"), Game.Hud->ReachMarkersRequested(), 0);
        Game.Press(Overlay.Close);
        TestFalse(Name + TEXT(" closes"), Game.Player->OverlayOpen());
        TestEqual(Name + TEXT(" leaves the selected action alone"), Game.Player->Action(), Action);
        TestEqual(Name + TEXT(": closing brings the target chips back"), Game.Hud->TargetChipsRequested(), Chips);
        TestEqual(Name + TEXT(": closing brings the reach markers back"), Game.Hud->ReachMarkersRequested(), Reach);
    }
    return !HasAnyErrors();
}

namespace
{
/** The first party turn of the first encounter, in the real input path. */
bool ReachFirstPartyTurn(ExpeditionInputHarness::FHarness& Game)
{
    Game.Frame();
    Game.Press(EKeys::Enter);
    Game.Settle();
    Game.Press(EKeys::Enter);
    if (!Game.View->InCombat()) { return false; }
    Game.PartyTurn();
    return Game.View->InCombat();
}
/** What the HUD asked to place must be exactly what the controller says is targetable. */
void CheckChipsMatchSelection(FAutomationTestBase& Test, const ExpeditionInputHarness::FHarness& Game, const FString& Context)
{
    const FActionReach& Reach = Game.Player->Reach();
    const FCombatBattle& Rules = Game.View->Rules();
    const FActionReach ExpectedReach = Rules.ActionReach(Game.Player->Actor(), Game.Player->Action());
    Test.TestTrue(Context + TEXT(": target cache matches authoritative rules"),
        Game.Player->Targets() == Rules.LegalTargets(Game.Player->Actor(), Game.Player->Action()));
    Test.TestTrue(Context + TEXT(": visible reach cache matches authoritative rules"), Reach.Visible == ExpectedReach.Visible);
    Test.TestTrue(Context + TEXT(": obstructed reach cache matches authoritative rules"), Reach.Obstructed == ExpectedReach.Obstructed);
    Test.TestEqual(Context + TEXT(": one chip per legal target"), Game.Hud->TargetChipsRequested(), Game.Player->Targets().Num());
    Test.TestEqual(Context + TEXT(": one marker per reach cell"), Game.Hud->ReachMarkersRequested(), Reach.Visible.Num() + Reach.Obstructed.Num());
    for (const int32 Id : Game.Player->Targets())
    { Test.TestFalse(Context + TEXT(": every chip has a forecast"), ExpeditionStyle::TargetChip(Game.View->Rules(), Game.Player->Actor(), Game.Player->Action(), Id).IsEmpty()); }
}
FString Signature(const ExpeditionInputHarness::FHarness& Game)
{
    FString Value = FString::Printf(TEXT("%d/%d:"), static_cast<int32>(Game.Player->Action()), Game.Player->Actor());
    for (const int32 Id : Game.Player->Targets())
    { Value += FString::Printf(TEXT("[%d %s]"), Id, *ExpeditionStyle::TargetChip(Game.View->Rules(), Game.Player->Actor(), Game.Player->Action(), Id)); }
    return Value + FString::Printf(TEXT(" reach=%d"), Game.Hud->ReachMarkersRequested());
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTargetChipsFollowSelectionTest, "Project.Expedition.Presentation.TargetChipsFollowActionAndActor",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FTargetChipsFollowSelectionTest::RunTest(const FString& Parameters)
{
    using ExpeditionInputHarness::FHarness;
    FHarness Game(*this, TEXT("TargetChipsFollow"));
    if (!TestTrue(TEXT("Input, board and HUD are ready"), Game.IsReady())) { return false; }
    if (!TestTrue(TEXT("The party has the first turn"), ReachFirstPartyTurn(Game))) { return false; }

    // Each mode key re-aims the actor; the chips must follow whatever it selects.
    TSet<FString> Seen;
    for (const FKey& Key : {EKeys::A, EKeys::F, EKeys::M, EKeys::B, EKeys::J, EKeys::R, EKeys::P, EKeys::A})
    {
        Game.Press(Key);
        CheckChipsMatchSelection(*this, Game, FString::Printf(TEXT("Action key %s"), *Key.ToString()));
        Seen.Add(Signature(Game));
    }
    TestTrue(TEXT("Different actions show different chips and reach"), Seen.Num() >= 3);

    // The next hero's turn re-aims at Attack for that hero, with that hero's own chips.
    const int32 FirstActor = Game.Player->Actor();
    const FString FirstAttack = Signature(Game);
    Game.Press(EKeys::SpaceBar);
    Game.PartyTurn();
    if (!TestTrue(TEXT("A party member has the turn again"), Game.View->InCombat() && Game.Player->ShowsTargeting())) { return false; }
    TestNotEqual(TEXT("A different hero acts"), Game.Player->Actor(), FirstActor);
    TestEqual(TEXT("The new turn starts on Attack"), Game.Player->Action(), ECombatCommandType::Attack);
    CheckChipsMatchSelection(*this, Game, TEXT("Next hero"));
    TestNotEqual(TEXT("The next hero's chips are its own"), Signature(Game), FirstAttack);
    Game.Press(EKeys::F);
    CheckChipsMatchSelection(*this, Game, TEXT("Next hero role action"));
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTargetingHiddenDuringPlaybackTest, "Project.Expedition.Presentation.TargetingHiddenDuringPlayback",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FTargetingHiddenDuringPlaybackTest::RunTest(const FString& Parameters)
{
    using ExpeditionInputHarness::FHarness;
    FHarness Game(*this, TEXT("TargetingPlayback"));
    if (!TestTrue(TEXT("Input, board and HUD are ready"), Game.IsReady())) { return false; }
    if (!TestTrue(TEXT("The party has the first turn"), ReachFirstPartyTurn(Game))) { return false; }
    Game.Press(EKeys::A);
    TestTrue(TEXT("Targeting shows before the turn ends"), Game.Hud->ReachMarkersRequested() > 0 && Game.Player->ShowsTargeting());

    // A move plays out on the board; none of that playback may show targeting, and it returns after.
    const int32 Actor = Game.Player->Actor();
    const TArray<FIntPoint> Moves = Game.View->Rules().LegalMoves(Actor);
    if (!TestFalse(TEXT("The actor has a move to play"), Moves.IsEmpty())) { return false; }
    Game.View->Execute({ECombatCommandType::Move, Actor, Moves.Last()});
    Game.Frame();
    int32 PlaybackFrames = 0, LeakedFrames = 0;
    for (int32 Step = 0; Step < 2000 && Game.View->IsPresenting(); ++Step)
    {
        ++PlaybackFrames;
        if (Game.Player->ShowsTargeting() || Game.Hud->TargetChipsRequested() != 0 || Game.Hud->ReachMarkersRequested() != 0) { ++LeakedFrames; }
        Game.Frame();
    }
    TestTrue(TEXT("Playback ran for the move"), PlaybackFrames > 0);
    TestEqual(TEXT("No frame of playback requested chips or reach markers"), LeakedFrames, 0);
    Game.Settle();
    if (!TestTrue(TEXT("The same hero can aim again once playback is over"), Game.View->InCombat() && Game.Player->ShowsTargeting() && Game.Player->Actor() == Actor)) { return false; }
    CheckChipsMatchSelection(*this, Game, TEXT("After playback"));
    TestTrue(TEXT("Reach returns when playback is over"), Game.Hud->ReachMarkersRequested() > 0);
    return !HasAnyErrors();
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKeyboardCursorDuringEnemyThinkTest, "Project.Expedition.Presentation.KeyboardCursorDuringEnemyThink",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FKeyboardCursorDuringEnemyThinkTest::RunTest(const FString& Parameters)
{
    ExpeditionInputHarness::FHarness Game(*this, TEXT("CursorEnemyThink"));
    if (!TestTrue(TEXT("Input, board and HUD are ready"), Game.IsReady())) { return false; }
    if (!TestTrue(TEXT("The party has the first turn"), ReachFirstPartyTurn(Game))) { return false; }
    Game.Press(EKeys::Right);
    TestTrue(TEXT("Keyboard inspection starts with visible brackets"), Game.Hud->KeyboardCursorRequested());

    // End party turns without advancing the enemy decision clock. No private state edits.
    for (int32 Turn = 0; Turn < 4; ++Turn)
    {
        const FCombatantState* Active = Game.View->Rules().Find(Game.View->Rules().ActiveId());
        if (Active && Active->Side == ECombatSide::Enemy) { break; }
        Game.View->Execute({ECombatCommandType::EndTurn, Game.View->Rules().ActiveId()});
        Game.View->SkipPresentation();
        Game.Frame(0.0f);
    }
    const int32 EnemyId = Game.View->Rules().ActiveId();
    const FCombatantState* Enemy = Game.View->Rules().Find(EnemyId);
    if (!TestTrue(TEXT("An enemy is deciding, with no playback"), Enemy && Enemy->Side == ECombatSide::Enemy && !Game.View->IsPresenting())) { return false; }
    TestFalse(TEXT("Enemy inspection is not action targeting"), Game.Player->ShowsTargeting());
    TestTrue(TEXT("Brackets remain visible during the enemy decision beat"), Game.Hud->KeyboardCursorRequested());

    const FIntPoint Before = Game.Player->KeyboardCursorCell();
    Game.Press(EKeys::Right);
    if (Game.Player->KeyboardCursorCell() == Before) { Game.Press(EKeys::Left); }
    TestNotEqual(TEXT("Arrows move the inspection cursor during the decision beat"), Game.Player->KeyboardCursorCell(), Before);
    TestEqual(TEXT("The same enemy is still deciding"), Game.View->Rules().ActiveId(), EnemyId);
    TestFalse(TEXT("The decision beat has not entered playback"), Game.View->IsPresenting());
    TestTrue(TEXT("The moved cursor still requests brackets"), Game.Hud->KeyboardCursorRequested());
    TestEqual(TEXT("Enemy inspection requests no target chips"), Game.Hud->TargetChipsRequested(), 0);
    TestEqual(TEXT("Enemy inspection requests no reach markers"), Game.Hud->ReachMarkersRequested(), 0);

    // An overlay still owns inspection as well as targeting.
    Game.Press(EKeys::H);
    TestTrue(TEXT("Records opens"), Game.Player->OverlayOpen());
    TestFalse(TEXT("Records hides the inspection brackets"), Game.Hud->KeyboardCursorRequested());
    Game.Press(EKeys::H);
    TestFalse(TEXT("Records closes"), Game.Player->OverlayOpen());
    TestTrue(TEXT("Closing Records restores inspection brackets"), Game.Hud->KeyboardCursorRequested());
    return !HasAnyErrors();
}
#endif
