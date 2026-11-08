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
#endif
