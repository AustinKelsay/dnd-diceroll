#if WITH_DEV_AUTOMATION_TESTS
#include "ExpeditionInputHarness.h"
#include "Combat/ActionFacts.h"
#include "Party/BuildCatalog.h"
#include "Presentation/ActionText.h"
#include "Presentation/CombatHints.h"
#include "Presentation/HudCoverage.h"

/**
 * #158 (#112 slice 2): four contextual combat hints, each shown at most once per run. They are driven here through the
 * real player controller and HUD in a transient world, the way a player reaches them.
 */
namespace CombatHintsTest
{
using ExpeditionInputHarness::FHarness;
/** A viewport position over the open board (no panel there at 1280x720). */
const FVector2D BoardSpot(700, 300);
const FName GotIt(TEXT("HintGotIt"));
const FName Skip(TEXT("HintSkip"));
const FName Replay(TEXT("ReplayTips"));

/** Begin with Enter, take the first road and reach the party's first turn at rest. */
bool ToFirstTurn(FHarness& Game)
{
    Game.Frame();
    Game.Press(EKeys::Enter);
    if (!Game.View->HasRun()) { return false; }
    Game.Settle();
    Game.Press(EKeys::Enter);
    if (!Game.View->InCombat()) { return false; }
    Game.PartyTurn();
    return Game.Player->ShowsTargeting();
}

ECombatHint Visible(const FHarness& Game) { return Game.Player->VisibleHint(); }

/**
 * Some hero of the party spends a resource by keyboard: the mode key, the cursor on a legal target, Enter. Heroes with
 * nothing to spend this turn end it (Space) and the next hero is tried. Returns false if no hero could.
 */
bool SpendResource(FHarness& Game)
{
    for (int32 Hero = 0; Hero < 4 && Game.View->InCombat(); ++Hero)
    {
        const FCombatBattle& Rules = Game.View->Rules();
        const int32 Id = Game.Player->Actor();
        for (const ECombatCommandType Command : FActionFactsQuery::BarFor(Rules, Id))
        {
            const FActionFacts Facts = FActionFactsQuery::For(Rules, Id, Command);
            FKey Key;
            if (Command == Game.Player->SpecialAction(0) && Command != ECombatCommandType::Attack) { Key = EKeys::F; }
            else if (Command == Game.Player->SpecialAction(1) && Command != ECombatCommandType::Attack) { Key = EKeys::M; }
            else { continue; }
            if (!Facts.bAvailable || Facts.Resources.IsEmpty()) { continue; }
            const TArray<int32> Targets = Rules.LegalTargets(Id, Command);
            if (Targets.IsEmpty()) { continue; }
            const FCombatantState* Target = Rules.Find(Targets[0]);
            if (!Target) { continue; }
            Game.Player->PreviewAim(true, Target->Cell);
            Game.Press(Key);
            Game.Press(EKeys::Enter);
            Game.Settle();
            const FCombatantState* After = Game.View->Rules().Find(Id);
            const FActionFacts Now = FActionFactsQuery::For(Game.View->Rules(), Id, Command);
            bool bSpent = false;
            for (int32 Index = 0; Index < Now.Resources.Num() && Index < Facts.Resources.Num(); ++Index) { bSpent |= Now.Resources[Index].Held < Facts.Resources[Index].Held; }
            if (After && bSpent) { return true; }
        }
        // Nothing to spend: end this hero's turn and wait for the next one.
        Game.Press(EKeys::SpaceBar);
        Game.PartyTurn();
        if (!Game.Player->ShowsTargeting()) { return false; }
    }
    return false;
}

/** Walks the acting hero until no step is left, by keyboard: arrows are not needed, the cursor is placed and Enter moves. */
void SpendMovement(FHarness& Game)
{
    for (int32 Step = 0; Step < 30 && Game.View->Rules().Snapshot().MovementLeft > 0; ++Step)
    {
        const TArray<FIntPoint> Moves = Game.View->Rules().LegalMoves(Game.Player->Actor());
        if (Moves.IsEmpty()) { return; }
        Game.Player->PreviewAim(true, Moves.Last());
        Game.Press(EKeys::Enter);
        Game.Settle();
    }
}

/** The id of the hint button at a point, or none. */
FName ButtonAt(const FHarness& Game, FVector2D Point)
{
    FName Id;
    return Game.Hud->ButtonAt(Point, Id) ? Id : NAME_None;
}

/** The centre of a hint button, found by probing the callout's rectangle. */
bool FindButton(const FHarness& Game, FName Wanted, FVector2D& Centre)
{
    const FHintRequest* Hint = Game.Hud->HintRequested();
    if (!Hint) { return false; }
    for (double Y = Hint->Box.Min.Y; Y < Hint->Box.Max.Y; Y += 2.0)
    {
        for (double X = Hint->Box.Min.X; X < Hint->Box.Max.X; X += 2.0)
        { if (ButtonAt(Game, FVector2D(X, Y)) == Wanted) { Centre = FVector2D(X + 2, Y + 2); return true; } }
    }
    return false;
}

/** An empty cell the acting hero can walk to: where a click that leaked through the callout would move them. */
FIntPoint FreeStep(const FHarness& Game)
{
    const FCombatBattle& Rules = Game.View->Rules();
    for (const FIntPoint Cell : Rules.LegalMoves(Game.Player->Actor()))
    {
        bool bTaken = false;
        for (const FCombatantState& Unit : Rules.Snapshot().Encounter.Combatants) { bTaken |= Unit.Cell == Cell && Unit.Life != ECombatLife::Dead; }
        if (!bTaken) { return Cell; }
    }
    return Rules.Find(Game.Player->Actor())->Cell;
}

/**
 * A real pointer click through the controller's Click(): the pointer comes to Point (with a free board cell behind it, as a
 * pointer over the callout has) and the left button goes down. The simulated pointer is released afterwards.
 */
void ClickAt(FHarness& Game, FVector2D Point)
{
    Game.Player->SimulatePointer(Point, FreeStep(Game));
    Game.Frame();
    Game.Press(EKeys::LeftMouseButton);
    Game.Player->ReleaseSimulatedPointer();
}

struct FRunState
{
    FExpeditionSnapshot Run;
    TArray<uint8> Bytes;
    int32 Selection = INDEX_NONE;
    ECombatCommandType Action = ECombatCommandType::Attack;
    float Yaw = 0.0f, Distance = 0.0f;
    bool bCursor = false;
};
FRunState Capture(const FHarness& Game)
{
    return {Game.Live(), Game.CheckpointBytes(), Game.Player->Selection(), Game.Player->Action(), Game.View->CameraYaw(), Game.View->CameraDistance(), Game.Player->UsingKeyboardCursor()};
}
void ExpectSame(FAutomationTestBase& Test, const FString& Where, const FRunState& Before, const FRunState& After)
{
    Test.TestTrue(Where + TEXT(": the run is unchanged"), ExpeditionTestSupport::SameRun(Before.Run, After.Run));
    Test.TestTrue(Where + TEXT(": the checkpoint bytes are unchanged"), Before.Bytes == After.Bytes);
    Test.TestEqual(Where + TEXT(": the selection is unchanged"), After.Selection, Before.Selection);
    Test.TestEqual(Where + TEXT(": the chosen action is unchanged"), After.Action, Before.Action);
    Test.TestEqual(Where + TEXT(": the camera yaw is unchanged"), After.Yaw, Before.Yaw);
    Test.TestEqual(Where + TEXT(": the camera distance is unchanged"), After.Distance, Before.Distance);
    Test.TestEqual(Where + TEXT(": the keyboard cursor is unchanged"), After.bCursor, Before.bCursor);
}

/** A mid-battle checkpoint: the default seed's first battle, with the second hero Downed when asked. */
FExpeditionSaveData BattleCheckpoint(FAutomationTestBase& Test, bool bDowned)
{
    FString Error;
    FExpeditionRun Run;
    TArray<FPartyBuildSelection> Party;
    const TCHAR* Names[] = {TEXT("Arden"), TEXT("Bartholomew the Bold"), TEXT("Sera"), TEXT("Ivo")};
    const TCHAR* Builds[] = {TEXT("fighter.champion"), TEXT("rogue.thief"), TEXT("cleric.life"), TEXT("wizard.evocation")};
    for (int32 Index = 0; Index < 4; ++Index) { FPartyBuildSelection Slot; Slot.Id = Index + 1; Slot.Name = Names[Index]; Slot.BuildId = Builds[Index]; Party.Add(Slot); }
    Run.NewRun(20261030, Party, Error, TEXT("hints"));
    Run.ChooseRoute(Run.Progress().Offers[0].Id, Error);
    FExpeditionSnapshot Snapshot = Run.Snapshot();
    if (bDowned)
    {
        const int32 ActiveId = Run.Battle().ActiveId();
        for (FCombatantState& Unit : Snapshot.Battle.Encounter.Combatants)
        {
            if (Unit.Side == ECombatSide::Party && Unit.Id != ActiveId && (Unit.Name.StartsWith(TEXT("Bartholomew")) || ActiveId == 2))
            { Unit.HP = 0; Unit.Life = ECombatLife::Downed; Unit.RescueTurnsLeft = 2; break; }
        }
    }
    FExpeditionRun Restored;
    if (!Restored.Restore(Snapshot, Error)) { Test.AddError(TEXT("The Downed checkpoint does not restore: ") + Error); }
    FExpeditionSaveData Data;
    Data.Current = Restored.Snapshot();
    return Data;
}

/** The Wizard bursts and fires a missile (no Action, Bonus or Kit left), then walks every step: only End Turn remains. */
bool EndTurnOnlyCheckpoint(FExpeditionSaveData& Saved)
{
    TArray<FPartyBuildSelection> Party;
    const TCHAR* Builds[] = {TEXT("wizard.evocation"), TEXT("fighter.battle-master"), TEXT("cleric.life"), TEXT("rogue.thief")};
    for (int32 Index = 0; Index < 4; ++Index)
    { FPartyBuildSelection Slot; Slot.Id = Index + 1; Slot.Name = FString::Printf(TEXT("Hero %d"), Index + 1); Slot.BuildId = Builds[Index]; Party.Add(Slot); }
    for (int32 Seed = 1; Seed < 400; ++Seed)
    {
        FString Error;
        FExpeditionRun Run;
        if (!Run.NewRun(Seed, Party, Error, TEXT("hints-end")) || !Run.ChooseRoute(Run.Progress().Offers[0].Id, Error)) { continue; }
        const FCombatantState* Active = Run.Battle().Find(Run.Battle().ActiveId());
        if (!Active || Active->Side != ECombatSide::Party || Active->BuildId != TEXT("wizard.evocation")) { continue; }
        const int32 Id = Active->Id;
        bool bSpent = false;
        for (int32 Step = 0; Step < 4 && !bSpent; ++Step)
        {
            const TArray<int32> Burst = Run.Battle().LegalTargets(Id, ECombatCommandType::Special2);
            const TArray<int32> Missile = Run.Battle().LegalTargets(Id, ECombatCommandType::MagicMissile);
            if (!Burst.IsEmpty() && !Missile.IsEmpty())
            { bSpent = Run.Submit({ECombatCommandType::Special2, Id, {}, Burst[0]}).bAccepted && Run.Submit({ECombatCommandType::MagicMissile, Id, {}, Missile[0]}).bAccepted; break; }
            FIntPoint Best = Run.Battle().Find(Id)->Cell; int32 BestSteps = MAX_int32;
            for (const FIntPoint& Cell : Run.Battle().LegalMoves(Id))
            { for (const FCombatantState& Enemy : Run.Battle().Snapshot().Encounter.Combatants)
              { const int32 Steps = ExpeditionTestSupport::Distance(Cell, Enemy.Cell); if (Enemy.Side == ECombatSide::Enemy && Enemy.HP > 0 && Steps < BestSteps) { BestSteps = Steps; Best = Cell; } } }
            if (BestSteps == MAX_int32 || !Run.Submit({ECombatCommandType::Move, Id, Best}).bAccepted) { break; }
        }
        if (!bSpent) { continue; }
        for (int32 Step = 0; Step < 30 && Run.Battle().Snapshot().MovementLeft > 0; ++Step)
        {
            const TArray<FIntPoint> Moves = Run.Battle().LegalMoves(Id);
            if (Moves.IsEmpty() || !Run.Submit({ECombatCommandType::Move, Id, Moves.Last()}).bAccepted) { break; }
        }
        bool bOnly = Run.Battle().ActiveId() == Id;
        for (const ECombatCommandType Command : FActionFactsQuery::BarFor(Run.Battle(), Id))
        { if (Command != ECombatCommandType::EndTurn && FActionFactsQuery::For(Run.Battle(), Id, Command).bAvailable) { bOnly = false; } }
        if (!bOnly) { continue; }
        Saved.Current = Run.Snapshot();
        return true;
    }
    return false;
}

/**
 * The visible hint's callout at the four compact sizes: inside the viewport, clear of the board, the tallest log and every
 * other control, and the persistent budget still holds with it up (it is a persistent panel here, not a transient layer).
 */
void CheckHintFits(FHarness& Game, const FString& Name)
{
    FAutomationTestBase& Test = Game.Test;
    constexpr double PersistentBudget = 0.18;
    for (const FIntPoint Size : {FIntPoint(1280, 720), FIntPoint(1920, 1080), FIntPoint(2560, 1080), FIntPoint(1024, 576)})
    {
        Game.Target.Size = Size;
        Game.Frame();
        const FString Where = FString::Printf(TEXT("%s at %dx%d"), *Name, Size.X, Size.Y);
        const FHintRequest* Drawn = Game.Hud->HintRequested();
        if (!Test.TestNotNull(Where + TEXT(": the HUD drew the hint"), Drawn)) { continue; }
        const FBox2D Box = Drawn->Box;
        Test.TestTrue(Where + TEXT(": inside the viewport"), Box.Min.X >= 0 && Box.Min.Y >= 0 && Box.Max.X <= Size.X + 0.01 && Box.Max.Y <= Size.Y + 0.01);
        const double Scale = ExpeditionLayout::ViewScale(Size.X, Size.Y);
        const double W = Size.X / Scale, H = Size.Y / Scale;
        Test.TestTrue(Where + TEXT(": clear of the board's play area"), Box.Max.X <= ExpeditionLayout::BoardBounds(W, H).Left * Scale + 0.01);
        Test.TestTrue(Where + TEXT(": clear of the tallest combat log"), Box.Max.Y <= ExpeditionLayout::LogRect(W, H).Top * Scale + 0.01);
        int32 Overlaps = 0, Own = 0;
        for (const FBox2D& Other : Game.Hud->ButtonRects())
        {
            if (Other.Min.X >= Box.Min.X - 0.01 && Other.Max.X <= Box.Max.X + 0.01 && Other.Min.Y >= Box.Min.Y - 0.01 && Other.Max.Y <= Box.Max.Y + 0.01) { ++Own; }
            else if (Other.Intersect(Box)) { ++Overlaps; }
            Test.TestTrue(Where + TEXT(": every control is inside the viewport"), Other.Min.X >= -0.01 && Other.Min.Y >= -0.01 && Other.Max.X <= Size.X + 0.01 && Other.Max.Y <= Size.Y + 0.01);
        }
        Test.TestEqual(Where + TEXT(": its own buttons are the two inside it"), Own, 2);
        Test.TestEqual(Where + TEXT(": no other control overlaps it"), Overlaps, 0);
        const double Coverage = Game.Hud->PersistentCoverage();
        Test.AddInfo(FString::Printf(TEXT("%s: the persistent HUD covers %.2f%% with the hint up"), *Where, Coverage * 100.0));
        Test.TestTrue(Where + TEXT(": the persistent budget holds with the hint up"), Coverage > 0.0 && Coverage <= PersistentBudget);
    }
    Game.Target.Size = FIntPoint(1280, 720);
    Game.Frame();
}

/** Help's Replay tips by keyboard: ? opens Help, Tab moves to the button, Enter chooses it. */
void ReplayFromHelp(FHarness& Game)
{
    Game.Press(EKeys::Slash);
    for (int32 Tab = 0; Tab < 4 && Game.Player->MenuFocus() != Replay; ++Tab) { Game.Press(EKeys::Tab); }
    Game.Press(EKeys::Enter);
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHintsFollowFirstRunTest, "Project.Expedition.Presentation.HintsFollowFirstRun",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FHintsFollowFirstRunTest::RunTest(const FString& Parameters)
{
    using namespace CombatHintsTest;
    {
        FHarness Game(*this, TEXT("HintsFirstRun"));
        if (!TestTrue(TEXT("Input, board and HUD are ready"), Game.IsReady())) { return false; }
        Game.Frame();
        TestEqual(TEXT("The start screen shows no hint"), Visible(Game), ECombatHint::Count);
        if (!TestTrue(TEXT("A fresh run reaches the party's first turn"), ToFirstTurn(Game))) { return false; }
        // 1. Move, select, confirm: the first party turn of a fresh run.
        TestEqual(TEXT("The first party turn shows the move hint"), Visible(Game), ECombatHint::MoveSelectConfirm);
        if (const FHintRequest* Drawn = Game.Hud->HintRequested())
        {
            TestEqual(TEXT("The HUD draws the same hint"), Drawn->Hint, ECombatHint::MoveSelectConfirm);
            TestTrue(TEXT("It tells the player to click a tile and an enemy"), Drawn->Lines.Num() > 0 && FString::Join(Drawn->Lines, TEXT(" ")).Contains(TEXT("glowing tile")));
        }
        else { AddError(TEXT("The HUD drew no hint on the first turn")); }
        // Stays up on later frames: one hint at a time, not re-raised and not replaced.
        for (int32 Index = 0; Index < 5; ++Index) { Game.Frame(); }
        TestEqual(TEXT("The hint stays up until the player puts it away"), Visible(Game), ECombatHint::MoveSelectConfirm);
        Game.Press(EKeys::D);
        TestEqual(TEXT("D puts it away"), Visible(Game), ECombatHint::Count);
        for (int32 Index = 0; Index < 5; ++Index) { Game.Frame(); }
        TestEqual(TEXT("Nothing else is up before a resource is spent"), Visible(Game), ECombatHint::Count);
        TestFalse(TEXT("The scarce-resource hint has not been shown"), Game.Player->Hints().WasShown(ECombatHint::ScarceResources));

        // 2. Scarce resources: the first time a resource is spent. The potion is spent by keyboard.
        if (!TestTrue(TEXT("A hero spends a resource"), SpendResource(Game))) { return false; }
        TestEqual(TEXT("Spending a resource raises the resource hint"), Visible(Game), ECombatHint::ScarceResources);
        if (const FHintRequest* Drawn = Game.Hud->HintRequested())
        {
            const FString Words = FString::Join(Drawn->Lines, TEXT(" "));
            // Expected from the facts of the hero's own bar: the first resource below its cap.
            FActionResourceCost Cue;
            for (const ECombatCommandType Command : FActionFactsQuery::BarFor(Game.View->Rules(), Game.Player->Actor()))
            {
                for (const FActionResourceCost& Cost : FActionFactsQuery::For(Game.View->Rules(), Game.Player->Actor(), Command).Resources)
                { if (Cue.Resource == EActionResource::None && Cost.Resource != EActionResource::Potion && Cost.Cap > 0 && Cost.Held < Cost.Cap) { Cue = Cost; } }
            }
            TestTrue(TEXT("A spent resource is found in the hero's bar"), Cue.Resource != EActionResource::None);
            TestTrue(TEXT("It names the resource with its count and cap"), Words.Contains(FString::Printf(TEXT("%s %d/%d"), *ExpeditionStyle::ResourceName(Cue.Resource), Cue.Held, Cue.Cap)));
            TestTrue(TEXT("and what camp gives back, from the facts"), Words.Contains(ExpeditionStyle::CampRecoveryText(Cue)));
        }
        Game.Press(EKeys::D);
        for (int32 Index = 0; Index < 5; ++Index) { Game.Frame(); }
        TestEqual(TEXT("It is shown once: the next spend does not raise it again"), Visible(Game), ECombatHint::Count);

        // 4. Ending a turn: nothing left but End Turn. Walk the remaining movement.
        SpendMovement(Game);
        const FCombatBattle& Rules = Game.View->Rules();
        bool bOnlyEndTurn = true;
        for (const ECombatCommandType Command : FActionFactsQuery::BarFor(Rules, Game.Player->Actor()))
        { if (Command != ECombatCommandType::EndTurn && FActionFactsQuery::For(Rules, Game.Player->Actor(), Command).bAvailable) { bOnlyEndTurn = false; } }
        if (bOnlyEndTurn)
        {
            TestEqual(TEXT("A hero with only End Turn left sees the ending hint"), Visible(Game), ECombatHint::EndingTurn);
            Game.Press(EKeys::D);
            TestEqual(TEXT("D puts it away"), Visible(Game), ECombatHint::Count);
        }
        else { AddInfo(TEXT("This hero still has an action besides End Turn after the potion and every step; the ending hint is checked by Presentation.HintsEndTurnWhenNothingElseIsLeft.")); }
        // Space ends the turn; nothing is shown during the enemy turn or its playback.
        Game.Press(EKeys::SpaceBar);
        TestEqual(TEXT("No hint is up while the turn passes"), Visible(Game), ECombatHint::Count);
        for (int32 Index = 0; Index < 5; ++Index) { TestEqual(TEXT("No hint shows during playback or an enemy turn"), Game.Player->ShowsTargeting() ? ECombatHint::Count : Visible(Game), ECombatHint::Count); Game.Frame(); }
        Game.PartyTurn();
        TestFalse(TEXT("The move hint is not shown a second time in this run"), Visible(Game) == ECombatHint::MoveSelectConfirm);
    }
    // 3. Downed and Rescue, and "Resume does not arm hints": a resumed battle with a Downed ally shows nothing until Replay tips.
    {
        const FExpeditionSaveData Saved = BattleCheckpoint(*this, true);
        FHarness Game(*this, TEXT("HintsResumed"), &Saved);
        if (!TestTrue(TEXT("Input, board and HUD are ready"), Game.IsReady())) { return false; }
        Game.Frame();
        Game.Press(EKeys::O);
        if (!TestTrue(TEXT("Resume brings the battle back"), Game.View->HasRun() && Game.View->InCombat())) { return false; }
        Game.PartyTurn();
        if (!TestTrue(TEXT("The party has the turn"), Game.Player->ShowsTargeting())) { return false; }
        for (int32 Index = 0; Index < 10; ++Index) { Game.Frame(); }
        TestEqual(TEXT("A resumed run shows no hint, not even for a Downed ally"), Visible(Game), ECombatHint::Count);
        TestFalse(TEXT("A resumed run is not armed"), Game.Player->Hints().Armed());
        TestNull(TEXT("The HUD draws none"), Game.Hud->HintRequested());
        // Replay tips re-arms: the move hint first, then the Downed hint, in order.
        ReplayFromHelp(Game);
        TestFalse(TEXT("Replay tips closes Help"), Game.Player->ShowingHelp());
        TestEqual(TEXT("Replay tips shows the move hint first"), Visible(Game), ECombatHint::MoveSelectConfirm);
        Game.Press(EKeys::D);
        TestEqual(TEXT("Then the Downed hint"), Visible(Game), ECombatHint::DownedRescue);
        if (const FHintRequest* Drawn = Game.Hud->HintRequested())
        {
            const FString Words = Drawn->Title + TEXT(" ") + FString::Join(Drawn->Lines, TEXT(" "));
            TestTrue(TEXT("It gives Rescue (R) and the turns left from the snapshot"), Words.Contains(TEXT("Rescue (R)")) && Words.Contains(TEXT("2 turns")) && Words.Contains(TEXT("is Downed")));
        }
        Game.Press(EKeys::D);
        TestEqual(TEXT("Nothing more is raised for this state"), Visible(Game), ECombatHint::Count);
    }
    // New Run re-arms: a lost run is resumed (no hint), then the next party's first turn shows the move hint again.
    {
        FString Error;
        FExpeditionRun Lost;
        Lost.NewRun(3, Error, TEXT("hints-defeat"));
        Lost.ChooseRoute(Lost.Progress().Offers[1].Id, Error);
        ExpeditionTestSupport::FinishCombat(Lost, false);
        if (!TestEqual(TEXT("A passive party is defeated"), Lost.Progress().Phase, EExpeditionPhase::Defeat)) { return false; }
        FExpeditionSaveData Saved;
        Saved.Current = Lost.Snapshot();
        Saved.Records.Add(FExpeditionSave::Ending(Lost));
        FHarness Game(*this, TEXT("HintsNewRun"), &Saved);
        if (!TestTrue(TEXT("Input, board and HUD are ready"), Game.IsReady())) { return false; }
        Game.Frame();
        Game.Press(EKeys::O);
        if (!TestTrue(TEXT("The ending resumes"), Game.View->HasRun() && Game.View->Progress().Phase == EExpeditionPhase::Defeat)) { return false; }
        TestFalse(TEXT("Resuming the ending arms nothing"), Game.Player->Hints().Armed());
        Game.Press(EKeys::N);
        Game.Press(EKeys::Enter);
        if (!TestTrue(TEXT("New Run starts the next expedition"), Game.View->HasRun() && Game.View->Progress().Phase == EExpeditionPhase::Route)) { return false; }
        TestTrue(TEXT("New Run arms the hints"), Game.Player->Hints().Armed());
        Game.Settle();
        Game.Press(EKeys::Enter);
        Game.PartyTurn();
        TestEqual(TEXT("The new run's first party turn shows the move hint"), Visible(Game), ECombatHint::MoveSelectConfirm);
    }
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHintsEndingTurnTest, "Project.Expedition.Presentation.HintsEndTurnWhenNothingElseIsLeft",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FHintsEndingTurnTest::RunTest(const FString& Parameters)
{
    using namespace CombatHintsTest;
    FExpeditionSaveData Saved;
    if (!TestTrue(TEXT("A seed exists where the Wizard is left with only End Turn"), EndTurnOnlyCheckpoint(Saved))) { return false; }
    FHarness Game(*this, TEXT("HintsEndTurn"), &Saved);
    if (!TestTrue(TEXT("Input, board and HUD are ready"), Game.IsReady())) { return false; }
    Game.Frame();
    Game.Press(EKeys::O);
    if (!TestTrue(TEXT("The battle resumes"), Game.View->InCombat())) { return false; }
    Game.Settle();
    TestEqual(TEXT("A resumed run shows no hint"), Visible(Game), ECombatHint::Count);
    ReplayFromHelp(Game);
    // Each hint is its own one-time callout, in order, and each fits the HUD at the four sizes.
    TestEqual(TEXT("Replay shows the move hint first"), Visible(Game), ECombatHint::MoveSelectConfirm);
    CheckHintFits(Game, TEXT("Move hint"));
    Game.Press(EKeys::D);
    TestEqual(TEXT("Then the spent Kit"), Visible(Game), ECombatHint::ScarceResources);
    if (const FHintRequest* Drawn = Game.Hud->HintRequested())
    { TestTrue(TEXT("It reads the Kit count from the facts"), FString::Join(Drawn->Lines, TEXT(" ")).Contains(TEXT("Kit 0/"))); }
    CheckHintFits(Game, TEXT("Resource hint"));
    Game.Press(EKeys::D);
    TestEqual(TEXT("Then End Turn"), Visible(Game), ECombatHint::EndingTurn);
    if (const FHintRequest* Drawn = Game.Hud->HintRequested())
    { TestTrue(TEXT("It names Space"), FString::Join(Drawn->Lines, TEXT(" ")).Contains(TEXT("Space"))); }
    CheckHintFits(Game, TEXT("End Turn hint"));
    Game.Press(EKeys::D);
    TestEqual(TEXT("Then nothing"), Visible(Game), ECombatHint::Count);
    for (int32 Index = 0; Index < 5; ++Index) { Game.Frame(); }
    TestEqual(TEXT("Each is shown once"), Visible(Game), ECombatHint::Count);
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHintsSkipAndReplayTest, "Project.Expedition.Presentation.HintsSkipAndReplay",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FHintsSkipAndReplayTest::RunTest(const FString& Parameters)
{
    using namespace CombatHintsTest;
    FHarness Game(*this, TEXT("HintsSkip"));
    if (!TestTrue(TEXT("Input, board and HUD are ready"), Game.IsReady())) { return false; }
    if (!TestTrue(TEXT("A fresh run reaches the party's first turn"), ToFirstTurn(Game))) { return false; }
    TestEqual(TEXT("The move hint is up"), Visible(Game), ECombatHint::MoveSelectConfirm);
    // The callout's buttons are real controls with the labels the player sees.
    FVector2D GotItAt = FVector2D::ZeroVector, SkipAt = FVector2D::ZeroVector;
    if (!TestTrue(TEXT("Got it is a button"), FindButton(Game, GotIt, GotItAt)) || !TestTrue(TEXT("Skip tips is a button"), FindButton(Game, Skip, SkipAt))) { return false; }
    TestTrue(TEXT("The hint's buttons are not a Tab stop"), !Game.Hud->MenuButtons().Contains(GotIt) && !Game.Hud->MenuButtons().Contains(Skip));
    // Got it puts away this one; the next hint still comes.
    {
        const FRunState Before = Capture(Game);
        ClickAt(Game, GotItAt);
        TestEqual(TEXT("Got it puts the hint away"), Visible(Game), ECombatHint::Count);
        TestFalse(TEXT("Got it does not turn tips off"), Game.Player->Hints().TipsOff());
        ExpectSame(*this, TEXT("Got it"), Before, Capture(Game));
    }
    // Skip tips ends them for the session: a spent resource no longer raises anything.
    if (!TestTrue(TEXT("A hero spends a resource"), SpendResource(Game))) { return false; }
    TestEqual(TEXT("The resource hint is up"), Visible(Game), ECombatHint::ScarceResources);
    {
        if (!TestTrue(TEXT("Skip tips is a button on this hint too"), FindButton(Game, Skip, SkipAt))) { return false; }
        const FRunState Before = Capture(Game);
        ClickAt(Game, SkipAt);
        TestEqual(TEXT("Skip tips puts it away"), Visible(Game), ECombatHint::Count);
        TestTrue(TEXT("and turns tips off"), Game.Player->Hints().TipsOff());
        ExpectSame(*this, TEXT("Skip tips"), Before, Capture(Game));
    }
    SpendMovement(Game);
    for (int32 Index = 0; Index < 5; ++Index) { Game.Frame(); }
    TestEqual(TEXT("With tips off nothing is raised"), Visible(Game), ECombatHint::Count);
    TestNull(TEXT("and the HUD draws no callout"), Game.Hud->HintRequested());
    // Help gains Replay tips; it is a keyboard-reachable control that re-arms and closes Help without touching the game.
    {
        const FRunState Before = Capture(Game);
        Game.Press(EKeys::Slash);
        TestTrue(TEXT("Help lists Replay tips"), Game.Hud->MenuButtons().Contains(Replay));
        Game.Press(EKeys::Escape);
        TestFalse(TEXT("Closing Help without Replay tips re-arms nothing"), Game.Player->Hints().TipsOff() == false);
        ReplayFromHelp(Game);
        TestFalse(TEXT("Replay tips closes Help"), Game.Player->ShowingHelp());
        TestFalse(TEXT("and turns tips back on"), Game.Player->Hints().TipsOff());
        TestEqual(TEXT("The first hint is shown again"), Visible(Game), ECombatHint::MoveSelectConfirm);
        const FRunState After = Capture(Game);
        TestTrue(TEXT("Replay tips changes no game state"), ExpeditionTestSupport::SameRun(Before.Run, After.Run) && Before.Bytes == After.Bytes && Before.Selection == After.Selection);
        Game.Press(EKeys::D);
        TestEqual(TEXT("A resource already spent raises its hint again"), Visible(Game), ECombatHint::ScarceResources);
    }
    // Replay tips by the pointer path: the button is registered while Help is open.
    {
        Game.Press(EKeys::D);
        Game.Press(EKeys::Slash);
        FName Found = NAME_None;
        for (double Y = 0; Y < 720 && Found.IsNone(); Y += 4) { for (double X = 0; X < 1280 && Found.IsNone(); X += 4) { if (ButtonAt(Game, FVector2D(X, Y)) == Replay) { Found = Replay; } } }
        TestEqual(TEXT("The Replay tips button is on screen in Help"), Found, Replay);
        Game.Press(EKeys::Escape);
    }
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHintsNeverLeakInputTest, "Project.Expedition.Presentation.HintsNeverLeakInput",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FHintsNeverLeakInputTest::RunTest(const FString& Parameters)
{
    using namespace CombatHintsTest;
    FHarness Game(*this, TEXT("HintsInput"));
    if (!TestTrue(TEXT("Input, board and HUD are ready"), Game.IsReady())) { return false; }
    if (!TestTrue(TEXT("A fresh run reaches the party's first turn"), ToFirstTurn(Game))) { return false; }
    TestEqual(TEXT("The move hint is up"), Visible(Game), ECombatHint::MoveSelectConfirm);
    const FHintRequest* Drawn = Game.Hud->HintRequested();
    if (!TestNotNull(TEXT("The HUD drew it"), Drawn)) { return false; }
    const FBox2D Box = Drawn->Box;

    // Everything inside the callout belongs to the HUD: a click there never reaches the board.
    for (double Y = Box.Min.Y + 1; Y < Box.Max.Y; Y += 6)
    { for (double X = Box.Min.X + 1; X < Box.Max.X; X += 6) { TestTrue(TEXT("The callout swallows clicks over its whole area"), Game.Hud->CoversBoard(FVector2D(X, Y))); } }
    FVector2D GotItAt = FVector2D::ZeroVector, SkipAt = FVector2D::ZeroVector;
    if (!TestTrue(TEXT("Got it and Skip tips are buttons"), FindButton(Game, GotIt, GotItAt) && FindButton(Game, Skip, SkipAt))) { return false; }
    TestEqual(TEXT("Combat holds no menu focus: the hint adds no focus"), Game.Player->MenuFocus(), FName(NAME_None));
    Game.Press(EKeys::Tab);
    TestEqual(TEXT("Tab does not focus a hint button"), Game.Player->MenuFocus(), FName(NAME_None));

    // The dismiss key and the buttons change the hint and nothing else.
    {
        const FRunState Before = Capture(Game);
        Game.Press(EKeys::D);
        TestEqual(TEXT("D puts the visible hint away"), Visible(Game), ECombatHint::Count);
        ExpectSame(*this, TEXT("D with a hint"), Before, Capture(Game));
    }
    {
        const FRunState Before = Capture(Game);
        Game.Press(EKeys::D);
        for (int32 Index = 0; Index < 3; ++Index) { Game.Frame(); }
        ExpectSame(*this, TEXT("D with no hint"), Before, Capture(Game));
        TestEqual(TEXT("D with no hint is a no-op"), Visible(Game), ECombatHint::Count);
    }
    // Shift+D is the camera's pan, not the hint's key.
    ReplayFromHelp(Game);
    TestEqual(TEXT("Replay shows the hint again"), Visible(Game), ECombatHint::MoveSelectConfirm);
    {
        Game.Player->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::LeftShift, IE_Pressed, 1.0f));
        Game.Press(EKeys::D);
        Game.Player->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::LeftShift, IE_Released, 0.0f));
        Game.Frame();
        TestEqual(TEXT("Shift+D leaves the hint to the player"), Visible(Game), ECombatHint::MoveSelectConfirm);
    }

    // Hidden, frozen and unreachable under every overlay.
    struct FOverlay { FKey Open; FKey Close; const TCHAR* Name; };
    const FOverlay Overlays[] = {{EKeys::Slash, EKeys::Escape, TEXT("Help")}, {EKeys::H, EKeys::H, TEXT("Records")}, {EKeys::BackSpace, EKeys::Escape, TEXT("Return question")}, {EKeys::I, EKeys::Escape, TEXT("Menu")}};
    for (const FOverlay& Overlay : Overlays)
    {
        const FString Name = Overlay.Name;
        const FRunState Before = Capture(Game);
        Game.Press(Overlay.Open);
        TestEqual(Name + TEXT(": the hint is hidden"), Visible(Game), ECombatHint::Count);
        TestNull(Name + TEXT(": the HUD draws no callout"), Game.Hud->HintRequested());
        TestTrue(Name + TEXT(": its buttons are not live"), !Game.Hud->MenuButtons().Contains(GotIt) && !Game.Hud->MenuButtons().Contains(Skip));
        // Overlay keys never dismiss it and its buttons do nothing.
        Game.Press(EKeys::D);
        Game.Player->HandleButton(GotIt);
        Game.Player->HandleButton(Skip);
        // The same click where the buttons were, through the pointer path (the Menu is left out: a click beside it closes it).
        if (Overlay.Open != EKeys::I) { ClickAt(Game, GotItAt); ClickAt(Game, SkipAt); }
        TestEqual(Name + TEXT(": the hint is still held"), Game.Player->Hints().Held(), ECombatHint::MoveSelectConfirm);
        TestFalse(Name + TEXT(": Skip tips did not take effect"), Game.Player->Hints().TipsOff());
        for (int32 Index = 0; Index < 3; ++Index) { Game.Frame(); }
        TestFalse(Name + TEXT(": nothing else advanced"), Game.Player->Hints().WasShown(ECombatHint::ScarceResources) || Game.Player->Hints().WasShown(ECombatHint::EndingTurn));
        Game.Press(Overlay.Close);
        TestFalse(Name + TEXT(": closed again"), Game.Player->OverlayOpen() || Game.Player->ShowingMenu());
        TestEqual(Name + TEXT(": the same hint returns"), Visible(Game), ECombatHint::MoveSelectConfirm);
        ExpectSame(*this, Name, Before, Capture(Game));
    }

    // A click on a hint's button is the HUD's, not the board's: it never moves, attacks or selects.
    {
        const FRunState Before = Capture(Game);
        TestEqual(TEXT("The pointer over Got it finds Got it"), ButtonAt(Game, GotItAt), GotIt);
        Game.Hud->PreviewPointer(GotItAt);
        Game.Frame();
        TestTrue(TEXT("A hover over the callout is not over a board cell"), Game.Hud->CoversBoard(GotItAt));
        Game.Hud->PreviewPointer(FVector2D(-1, -1));
        ClickAt(Game, GotItAt);
        TestEqual(TEXT("A real click on Got it puts the hint away"), Visible(Game), ECombatHint::Count);
        ExpectSame(*this, TEXT("Got it"), Before, Capture(Game));
    }

    // During playback or an enemy turn, nothing shows, and the key and buttons do nothing.
    ReplayFromHelp(Game);
    TestEqual(TEXT("Replay shows the hint on the party's turn"), Visible(Game), ECombatHint::MoveSelectConfirm);
    if (!TestTrue(TEXT("A hero spends a resource"), SpendResource(Game))) { return false; }
    for (int32 Turn = 0; Turn < 8 && Game.View->InCombat() && Game.Player->ShowsTargeting(); ++Turn) { Game.Press(EKeys::SpaceBar); }
    bool bSawPlaybackOrEnemy = false;
    for (int32 Index = 0; Index < 400 && Game.View->InCombat() && !Game.Player->ShowsTargeting(); ++Index)
    {
        bSawPlaybackOrEnemy = true;
        TestEqual(TEXT("No hint while results play or an enemy acts"), Visible(Game), ECombatHint::Count);
        TestNull(TEXT("and none is drawn"), Game.Hud->HintRequested());
        Game.Player->HandleButton(GotIt);
        Game.Player->HandleButton(Skip);
        ClickAt(Game, GotItAt);
        ClickAt(Game, SkipAt);
        TestFalse(TEXT("The buttons are dead then"), Game.Player->Hints().TipsOff());
        Game.Frame(0.25f);
        Game.View->SkipPresentation();
    }
    TestTrue(TEXT("Playback or an enemy turn was observed"), bSawPlaybackOrEnemy);
    return !HasAnyErrors();
}

/**
 * Under #172's input rules a click on the callout and the D key are neither a pointer preview nor a keyboard confirmation:
 * the panel swallows the click before the preview or the keyboard-cursor handoff is touched, and D only puts the hint away.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHintsPointerRulesTest, "Project.Expedition.Presentation.HintsClickAndDKeyAreNeitherPreviewNorConfirm",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FHintsPointerRulesTest::RunTest(const FString& Parameters)
{
    using namespace CombatHintsTest;
    FHarness Game(*this, TEXT("HintsPointerRules"));
    if (!TestTrue(TEXT("Input, board and HUD are ready"), Game.IsReady())) { return false; }
    if (!TestTrue(TEXT("A fresh run reaches the party's first turn"), ToFirstTurn(Game))) { return false; }
    const FIntPoint Behind = FreeStep(Game), Start = Game.View->Rules().Find(Game.Player->Actor())->Cell;
    if (!TestTrue(TEXT("The hero has a cell a leaked click would move them to"), Behind != Start)) { return false; }
    FVector2D GotItAt = FVector2D::ZeroVector, SkipAt = FVector2D::ZeroVector;
    if (!TestTrue(TEXT("The callout has its buttons"), FindButton(Game, GotIt, GotItAt) && FindButton(Game, Skip, SkipAt))) { return false; }
    const FHintRequest* Drawn = Game.Hud->HintRequested();
    const FVector2D Panel((Drawn->Box.Min.X + Drawn->Box.Max.X) * 0.5, Drawn->Box.Min.Y + 8);
    TestFalse(TEXT("The point on the panel is no button"), !ButtonAt(Game, Panel).IsNone());
    FIntPoint Previewed;

    // 1. The pointer rests on the callout's panel, a free cell behind it: nothing is previewed, and a click on the panel does nothing.
    {
        Game.Player->SimulatePointer(Panel, Behind);
        for (int32 Index = 0; Index < 3; ++Index) { Game.Frame(); }
        TestFalse(TEXT("A pointer over the callout previews no board cell"), Game.Player->PreviewedPointerCell(Previewed));
        TestTrue(TEXT("The callout stays up under the pointer"), Visible(Game) == ECombatHint::MoveSelectConfirm);
        const FRunState Before = Capture(Game);
        Game.Press(EKeys::LeftMouseButton);
        Game.Press(EKeys::LeftMouseButton);
        ExpectSame(*this, TEXT("Clicks on the panel"), Before, Capture(Game));
        TestEqual(TEXT("The hero stays where they were"), Game.View->Rules().Find(Game.Player->Actor())->Cell, Start);
        TestFalse(TEXT("A click on the panel previews nothing"), Game.Player->PreviewedPointerCell(Previewed));
        TestEqual(TEXT("A click on the panel leaves the hint up"), Visible(Game), ECombatHint::MoveSelectConfirm);
    }

    // 2. Keyboard cursor showing, the pointer resting on Got it: a click on the button dismisses the hint and nothing else.
    // It does not spend the "first click only hides the cursor" rule, and it is not a confirmation.
    {
        Game.Player->SimulatePointer(GotItAt, Behind);
        Game.Frame();
        Game.Press(EKeys::Left);
        TestTrue(TEXT("The arrow shows the keyboard cursor"), Game.Player->UsingKeyboardCursor());
        const FIntPoint Cursor = Game.Player->KeyboardCursorCell();
        Game.Press(EKeys::LeftMouseButton);
        TestEqual(TEXT("Got it by the pointer puts the hint away"), Visible(Game), ECombatHint::Count);
        TestTrue(TEXT("The click did not hand the board back from the keyboard cursor"), Game.Player->UsingKeyboardCursor() && Game.Player->KeyboardCursorCell() == Cursor);
        TestEqual(TEXT("It acted on nothing"), Game.View->Rules().Find(Game.Player->Actor())->Cell, Start);
        TestFalse(TEXT("It previewed nothing"), Game.Player->PreviewedPointerCell(Previewed));
    }

    // 3. The pointer takes the board (cursor hidden), the callout is back: D only puts the hint away. It does not show the
    // keyboard cursor, so the first Enter afterwards still only shows it and a second one is needed to act.
    ReplayFromHelp(Game);
    TestEqual(TEXT("Replay tips raises the hint again"), Visible(Game), ECombatHint::MoveSelectConfirm);
    {
        Game.Player->SimulatePointer(BoardSpot, Behind);
        Game.Frame();
        TestFalse(TEXT("Pointer motion hid the keyboard cursor"), Game.Player->UsingKeyboardCursor());
        const FRunState Before = Capture(Game);
        Game.Press(EKeys::D);
        TestEqual(TEXT("D puts the hint away"), Visible(Game), ECombatHint::Count);
        TestFalse(TEXT("D did not show the keyboard cursor"), Game.Player->UsingKeyboardCursor());
        ExpectSame(*this, TEXT("D with the pointer on the board"), Before, Capture(Game));
        Game.Press(EKeys::Enter);
        TestTrue(TEXT("The first Enter only shows the keyboard cursor"), Game.Player->UsingKeyboardCursor());
        TestEqual(TEXT("... and acts on nothing"), Game.View->Rules().Find(Game.Player->Actor())->Cell, Start);
    }
    Game.Player->ReleaseSimulatedPointer();
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHintsFitTheHudTest, "Project.Expedition.Presentation.HintsFitTheHud",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FHintsFitTheHudTest::RunTest(const FString& Parameters)
{
    using namespace CombatHintsTest;
    // The move hint of a fresh run, then the Downed hint with a long name (the longest wording), at the four compact sizes.
    {
        FHarness Game(*this, TEXT("HintsFit"));
        if (!TestTrue(TEXT("Input, board and HUD are ready"), Game.IsReady()) || !TestTrue(TEXT("A fresh run reaches the party's first turn"), ToFirstTurn(Game))) { return false; }
        TestEqual(TEXT("The move hint is up"), Visible(Game), ECombatHint::MoveSelectConfirm);
        CheckHintFits(Game, TEXT("Fresh run, move hint"));
    }
    {
        const FExpeditionSaveData Saved = BattleCheckpoint(*this, true);
        FHarness Game(*this, TEXT("HintsFitDowned"), &Saved);
        if (!TestTrue(TEXT("Input, board and HUD are ready"), Game.IsReady())) { return false; }
        Game.Frame();
        Game.Press(EKeys::O);
        Game.PartyTurn();
        ReplayFromHelp(Game);
        Game.Press(EKeys::D);
        if (TestEqual(TEXT("The Downed hint is up"), Visible(Game), ECombatHint::DownedRescue)) { CheckHintFits(Game, TEXT("Downed hint")); }
    }
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHintsSkipThenNewRunTest, "Project.Expedition.Presentation.HintsSkipThenNewRunGivesHintsAgain",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FHintsSkipThenNewRunTest::RunTest(const FString& Parameters)
{
    using namespace CombatHintsTest;
    // Skip tips is for the session (never saved), but a New Run resets it with the shown flags (#158).
    FString Error;
    FExpeditionRun Lost;
    Lost.NewRun(3, Error, TEXT("hints-skip"));
    Lost.ChooseRoute(Lost.Progress().Offers[1].Id, Error);
    ExpeditionTestSupport::FinishCombat(Lost, false);
    if (!TestEqual(TEXT("A passive party is defeated"), Lost.Progress().Phase, EExpeditionPhase::Defeat)) { return false; }
    FExpeditionSaveData Saved;
    Saved.Current = Lost.Snapshot();
    Saved.Records.Add(FExpeditionSave::Ending(Lost));
    FHarness Game(*this, TEXT("HintsSkipNewRun"), &Saved);
    if (!TestTrue(TEXT("Input, board and HUD are ready"), Game.IsReady())) { return false; }
    Game.Frame();
    Game.Press(EKeys::O);
    Game.Press(EKeys::N);
    Game.Press(EKeys::Enter);
    if (!TestTrue(TEXT("New Run starts an expedition"), Game.View->HasRun() && Game.View->Progress().Phase == EExpeditionPhase::Route)) { return false; }
    Game.Settle();
    Game.Press(EKeys::Enter);
    Game.PartyTurn();
    if (!TestEqual(TEXT("The first run shows the move hint"), Visible(Game), ECombatHint::MoveSelectConfirm)) { return false; }
    FVector2D SkipAt;
    if (!TestTrue(TEXT("Skip tips is a button"), FindButton(Game, Skip, SkipAt))) { return false; }
    ClickAt(Game, SkipAt);
    TestTrue(TEXT("Skip tips turns tips off"), Game.Player->Hints().TipsOff());
    // Finish the run (a party that only ends turns is defeated), then start another.
    for (int32 Turn = 0; Turn < 400 && Game.View->InCombat(); ++Turn)
    {
        if (Game.Player->ShowsTargeting()) { Game.Press(EKeys::SpaceBar); } else { Game.Frame(0.25f); Game.View->SkipPresentation(); }
        TestEqual(TEXT("No hint appears while tips are off"), Visible(Game), ECombatHint::Count);
    }
    Game.Settle();
    if (!TestTrue(TEXT("The passive run ends"), Game.View->HasRun() && (Game.View->Progress().Phase == EExpeditionPhase::Defeat || Game.View->Progress().Phase == EExpeditionPhase::Reward))) { return false; }
    if (Game.View->Progress().Phase != EExpeditionPhase::Defeat) { AddInfo(TEXT("The battle was won; the run is not defeated, so New Run cannot start here.")); return !HasAnyErrors(); }
    Game.Press(EKeys::N);
    Game.Press(EKeys::Enter);
    if (!TestTrue(TEXT("The next New Run starts"), Game.View->HasRun() && Game.View->Progress().Phase == EExpeditionPhase::Route)) { return false; }
    TestFalse(TEXT("New Run resets the tips-off switch"), Game.Player->Hints().TipsOff());
    Game.Settle();
    Game.Press(EKeys::Enter);
    Game.PartyTurn();
    TestEqual(TEXT("The new run's first party turn gives the move hint again"), Visible(Game), ECombatHint::MoveSelectConfirm);
    return !HasAnyErrors();
}

/**
 * A resource is spent, not only below its cap: a hero with two Potions drinks one (2 to 1), every other resource full,
 * and the resource hint shows. The state is a real run (a won battle, a reward claimed) restored with full resources
 * and wounded heroes, then driven through Replay tips and the keyboard.
 */
namespace CombatHintsTest
{
bool TwoPotionCheckpoint(FAutomationTestBase& Test, FExpeditionSaveData& Saved, int32& RecipientId)
{
    for (int32 Seed = 1; Seed < 60; ++Seed)
    {
        FString Error;
        FExpeditionRun Run;
        if (!Run.NewRun(Seed, Error, TEXT("hints-potion")) || !Run.ChooseRoute(Run.Progress().Offers[0].Id, Error)) { continue; }
        if (!ExpeditionTestSupport::FinishCombat(Run) || Run.Progress().Phase != EExpeditionPhase::Reward) { continue; }
        int32 Survivor = INDEX_NONE;
        for (const FCombatantState& Unit : Run.Battle().Snapshot().Encounter.Combatants)
        { if (Survivor == INDEX_NONE && Unit.Side == ECombatSide::Party && Unit.Life == ECombatLife::Conscious) { Survivor = Unit.Id; } }
        if (Survivor == INDEX_NONE || !Run.ClaimReward(Survivor, Error) || !Run.Recover(false, Error) || !Run.ChooseRoute(Run.Progress().Offers[0].Id, Error)) { continue; }
        FExpeditionSnapshot Snapshot = Run.Snapshot();
        const FCombatantState* Active = Run.Battle().Find(Run.Battle().ActiveId());
        if (!Active || Active->Side != ECombatSide::Party) { continue; }
        bool bTwo = false;
        for (FCombatantState& Unit : Snapshot.Battle.Encounter.Combatants)
        {
            if (Unit.Side != ECombatSide::Party || Unit.Life != ECombatLife::Conscious) { continue; }
            if (const auto* Build = FBuildCatalog::Find(Unit.BuildId))
            { Unit.KitUses = Build->KitCap; Unit.SecondWindUses = Build->WindCap; Unit.SpellSlots = Build->SlotCap; Unit.ManeuverUses = Build->ManeuverCap; Unit.RageUses = Build->RageCap; }
            Unit.HP = FMath::Max(1, Unit.MaxHP - 3);
            bTwo |= Unit.Id == Survivor && Unit.Potions >= 2;
        }
        if (!bTwo) { continue; }
        FExpeditionRun Restored;
        if (!Restored.Restore(Snapshot, Error)) { Test.AddError(TEXT("The two-potion checkpoint does not restore: ") + Error); return false; }
        Saved.Current = Restored.Snapshot();
        RecipientId = Survivor;
        return true;
    }
    return false;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHintsPotionTest, "Project.Expedition.Presentation.HintsSpendingAPotionRaisesTheResourceHint",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FHintsPotionTest::RunTest(const FString& Parameters)
{
    using namespace CombatHintsTest;
    FExpeditionSaveData Saved;
    int32 Recipient = INDEX_NONE;
    if (!TestTrue(TEXT("A run exists where a hero holds two Potions in a fresh battle"), TwoPotionCheckpoint(*this, Saved, Recipient))) { return false; }
    FHarness Game(*this, TEXT("HintsPotion"), &Saved);
    if (!TestTrue(TEXT("Input, board and HUD are ready"), Game.IsReady())) { return false; }
    Game.Frame();
    Game.Press(EKeys::O);
    Game.PartyTurn();
    if (!TestTrue(TEXT("The party has the turn"), Game.Player->ShowsTargeting())) { return false; }
    ReplayFromHelp(Game);
    TestEqual(TEXT("Replay shows the move hint"), Visible(Game), ECombatHint::MoveSelectConfirm);
    Game.Press(EKeys::D);
    for (int32 Index = 0; Index < 3; ++Index) { Game.Frame(); }
    TestEqual(TEXT("With every resource full and two Potions nothing else is raised"), Visible(Game), ECombatHint::Count);
    // Let the turns pass (Space) until the hero with the Potions acts.
    for (int32 Turn = 0; Turn < 8 && Game.View->InCombat() && Game.Player->Actor() != Recipient; ++Turn) { Game.Press(EKeys::SpaceBar); Game.PartyTurn(); }
    if (!TestTrue(TEXT("The hero with the Potions has the turn"), Game.View->InCombat() && Game.Player->ShowsTargeting() && Game.Player->Actor() == Recipient)) { return false; }
    TestEqual(TEXT("Still nothing: no resource is spent"), Visible(Game), ECombatHint::Count);
    const FCombatBattle& Rules = Game.View->Rules();
    const TArray<int32> Targets = Rules.LegalTargets(Recipient, ECombatCommandType::Potion);
    if (!TestTrue(TEXT("A wounded hero can be given a Potion"), !Targets.IsEmpty())) { return false; }
    const FCombatantState* Target = Rules.Find(Targets[0]);
    const int32 Before = Rules.Find(Recipient)->Potions;
    TestTrue(TEXT("The hero holds at least two Potions"), Before >= 2);
    Game.Player->PreviewAim(true, Target->Cell);
    Game.Press(EKeys::P);
    Game.Press(EKeys::Enter);
    Game.Settle();
    TestEqual(TEXT("One Potion was drunk"), Game.View->Rules().Find(Recipient)->Potions, Before - 1);
    // The roll card (the Potion's dice) shares the left margin with the hint, so the hint waits until the card has gone (#190).
    for (int32 Index = 0; Index < 200 && Game.View->RollCardUp(); ++Index) { Game.Frame(0.05f); }
    TestEqual(TEXT("Spending a Potion raises the resource hint"), Visible(Game), ECombatHint::ScarceResources);
    if (const FHintRequest* Drawn = Game.Hud->HintRequested())
    {
        const FString Words = FString::Join(Drawn->Lines, TEXT(" "));
        TestTrue(TEXT("It names the Potion and that camp does not refill it, from the facts"), Words.Contains(TEXT("Potion")) && Words.Contains(ExpeditionStyle::CampRecoveryText(FActionResourceCost{EActionResource::Potion, 1, 1, 9, 0})));
    }
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHintsWordingTest, "Project.Expedition.Presentation.HintsWordingFromFacts",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FHintsWordingTest::RunTest(const FString& Parameters)
{
    // The state machine, with no battle: arm, order, once, tips off, replay, resume.
    FCombatHints Hints;
    FHintObservation Rest;
    Rest.bAtRest = true;
    Hints.Observe(Rest);
    TestEqual(TEXT("Nothing before a New Run arms the hints"), Hints.Visible(), ECombatHint::Count);
    Hints.Arm();
    Hints.Observe(Rest);
    TestEqual(TEXT("The move hint comes first"), Hints.Visible(), ECombatHint::MoveSelectConfirm);
    FHintObservation Busy = Rest;
    Busy.bAtRest = false;
    Hints.Observe(Busy);
    TestEqual(TEXT("Not at rest hides it"), Hints.Visible(), ECombatHint::Count);
    TestEqual(TEXT("but keeps it"), Hints.Held(), ECombatHint::MoveSelectConfirm);
    Hints.Observe(Rest);
    TestEqual(TEXT("and it returns"), Hints.Visible(), ECombatHint::MoveSelectConfirm);
    Hints.Dismiss();
    Hints.Observe(Rest);
    TestEqual(TEXT("Dismissed hints do not repeat"), Hints.Visible(), ECombatHint::Count);
    FHintObservation Everything = Rest;
    Everything.bResourceCue = true; Everything.Resource = {EActionResource::Kit, 1, 1, 2, 1};
    Everything.bDowned = true; Everything.DownedName = TEXT("Nix"); Everything.RescueTurnsLeft = 1; Everything.bOnlyEndTurn = true;
    for (const ECombatHint Expected : {ECombatHint::ScarceResources, ECombatHint::DownedRescue, ECombatHint::EndingTurn})
    {
        Hints.Observe(Everything);
        TestEqual(TEXT("The hints come in order, one at a time"), Hints.Visible(), Expected);
        Hints.Dismiss();
    }
    Hints.Observe(Everything);
    TestEqual(TEXT("All four shown: nothing more"), Hints.Visible(), ECombatHint::Count);
    TestFalse(TEXT("A finished set wants nothing"), Hints.Wants());
    Hints.Arm();
    Hints.SkipTips();
    Hints.Observe(Everything);
    TestEqual(TEXT("Skip tips shows nothing"), Hints.Visible(), ECombatHint::Count);
    TestTrue(TEXT("Skip tips turned them off"), Hints.TipsOff());
    Hints.Arm();
    TestFalse(TEXT("A New Run resets the tips-off switch with the shown flags"), Hints.TipsOff());
    Hints.Observe(Everything);
    TestEqual(TEXT("so the first hint is shown again"), Hints.Visible(), ECombatHint::MoveSelectConfirm);
    Hints.SkipTips();
    Hints.Replay();
    Hints.Observe(Everything);
    TestEqual(TEXT("Replay tips turns them on"), Hints.Visible(), ECombatHint::MoveSelectConfirm);
    Hints.Disarm();
    Hints.Observe(Everything);
    TestEqual(TEXT("Resume disarms"), Hints.Visible(), ECombatHint::Count);
    // A hint whose cause has passed is put away.
    Hints.Arm();
    FHintObservation Down = Rest; Down.bDowned = true; Down.DownedName = TEXT("Ivo"); Down.RescueTurnsLeft = 3;
    Hints.Observe(Rest); Hints.Dismiss();
    Hints.Observe(Down);
    TestEqual(TEXT("A Downed ally raises the Rescue hint"), Hints.Visible(), ECombatHint::DownedRescue);
    Hints.Observe(Rest);
    TestEqual(TEXT("Rescued: the hint goes"), Hints.Visible(), ECombatHint::Count);

    // Wording comes from the facts' formatters.
    FActionResourceCost Kit; Kit.Resource = EActionResource::Kit; Kit.Amount = 1; Kit.Held = 1; Kit.Cap = 2; Kit.CampRecovery = 1;
    FHintObservation Spent = Rest; Spent.bResourceCue = true; Spent.Resource = Kit;
    const FHintContent KitWords = FCombatHints::ContentFor(ECombatHint::ScarceResources, Spent);
    const FString KitText = FString::Join(KitWords.Lines, TEXT(" "));
    TestTrue(TEXT("The Kit hint reads the count and the cap"), KitText.Contains(TEXT("Kit 1/2")));
    TestTrue(TEXT("and the camp recovery text of the facts"), KitText.Contains(ExpeditionStyle::CampRecoveryText(Kit)));
    FActionResourceCost Potion; Potion.Resource = EActionResource::Potion; Potion.Amount = 1; Potion.Held = 0; Potion.Cap = 9; Potion.CampRecovery = 0;
    Spent.Resource = Potion;
    TestTrue(TEXT("A Potion hint says it is not refilled"), FString::Join(FCombatHints::ContentFor(ECombatHint::ScarceResources, Spent).Lines, TEXT(" ")).Contains(TEXT("Not refilled by camp")));
    FHintObservation OneTurn = Rest; OneTurn.bDowned = true; OneTurn.DownedName = TEXT("Ivo"); OneTurn.RescueTurnsLeft = 1;
    TestTrue(TEXT("The Rescue hint reads the turns left"), FString::Join(FCombatHints::ContentFor(ECombatHint::DownedRescue, OneTurn).Lines, TEXT(" ")).Contains(TEXT("1 turn,")));
    for (int32 Index = 0; Index < int32(ECombatHint::Count); ++Index)
    {
        const FHintContent Words = FCombatHints::ContentFor(ECombatHint(Index), Everything);
        TestTrue(FString::Printf(TEXT("Hint %d has a title and words"), Index), !Words.Title.IsEmpty() && Words.Lines.Num() > 0);
    }
    return !HasAnyErrors();
}
#endif
