#if WITH_DEV_AUTOMATION_TESTS
#include "ExpeditionInputHarness.h"
#include "Combat/ActionFacts.h"
#include "Party/BuildCatalog.h"
#include "Presentation/ActionText.h"
#include "Presentation/ExpeditionPlayback.h"

/**
 * #58: the action dock shows each action's cost and usage tags and a tooltip, and every word of them comes from the
 * action facts through ExpeditionStyle's helpers. These tests read what the HUD asked to draw, through its public
 * seam, and compare it with the facts the rules give for the same battle.
 */
namespace ActionTagsTest
{
using EC = ECombatCommandType;
const FIntPoint Viewports[] = {FIntPoint(1280, 720), FIntPoint(1920, 1080), FIntPoint(2560, 1080), FIntPoint(1024, 576)};

/** A checkpoint at the first route's encounter for a party of the given builds (slots 1-4, in order). */
bool PartyCheckpoint(const TArray<FString>& Builds, int32 Seed, FExpeditionSaveData& Data, ERuleset Ruleset = ERuleset::Compact)
{
    TArray<FPartyBuildSelection> Party;
    for (int32 Index = 0; Index < Builds.Num(); ++Index)
    {
        FPartyBuildSelection Slot;
        Slot.Id = Index + 1;
        Slot.Name = FString::Printf(TEXT("Hero %d"), Index + 1);
        Slot.BuildId = Builds[Index];
        Party.Add(Slot);
    }
    FString Error;
    FExpeditionRun Run;
    if (!Run.NewRun(Seed, Party, Error, FString::Printf(TEXT("action-tags-%d"), Seed), Ruleset)) { return false; }
    if (Run.Progress().Offers.IsEmpty() || !Run.ChooseRoute(Run.Progress().Offers[0].Id, Error)) { return false; }
    Data.Current = Run.Snapshot();
    return true;
}

/** The dock's buttons for the active hero: the facts' bar, with Move sharing the Attack button. */
TArray<EC> DockCommands(const FCombatBattle& Rules)
{
    TArray<EC> Bar = FActionFactsQuery::BarFor(Rules, Rules.ActiveId());
    Bar.Remove(EC::Move);
    return Bar;
}

/** What the facts say the cost pill of a dock button reads. */
FString ExpectedCost(const FCombatBattle& Rules, EC Command)
{
    const int32 Id = Rules.ActiveId();
    const FActionFacts Facts = FActionFactsQuery::For(Rules, Id, Command);
    if (Command == EC::Attack)
    { return ExpeditionStyle::AttackMovePillText(Facts, FActionFactsQuery::For(Rules, Id, EC::Move), Rules.Snapshot().MovementLeft); }
    return ExpeditionStyle::CostPillText(Facts);
}

/** The active hero's dock, at every compact HUD size: one tag pair per button, equal to the formatter's words. */
int32 CheckTags(FAutomationTestBase& Test, ExpeditionInputHarness::FHarness& Game, const FString& Context)
{
    int32 Checked = 0;
    for (const FIntPoint Size : Viewports)
    {
        Game.Target.Size = Size;
        Game.Frame();
        const FCombatBattle& Rules = Game.View->Rules();
        const int32 Id = Rules.ActiveId();
        const FString Where = FString::Printf(TEXT("%s at %dx%d"), *Context, Size.X, Size.Y);
        const TArray<EC> Bar = DockCommands(Rules);
        const TArray<FActionTagRequest>& Tags = Game.Hud->ActionTagsRequested();
        if (!Test.TestEqual(Where + TEXT(": one tag pair per dock button"), Tags.Num(), Bar.Num())) { continue; }
        for (int32 Index = 0; Index < Bar.Num(); ++Index)
        {
            const FActionTagRequest& Tag = Tags[Index];
            const FActionFacts Facts = FActionFactsQuery::For(Rules, Id, Bar[Index]);
            const FString Button = Where + TEXT(", ") + Tag.ButtonId.ToString();
            Test.TestEqual(Button + TEXT(": the tag belongs to its command"), Tag.Command, Bar[Index]);
            Test.TestEqual(Button + TEXT(": cost tag is the formatter's"), Tag.Cost, ExpectedCost(Rules, Bar[Index]));
            Test.TestEqual(Button + TEXT(": usage tag is the formatter's"), Tag.Usage, ExpeditionStyle::UsageTagText(Facts));
            if (Bar[Index] == EC::EndTurn) { Test.TestTrue(Button + TEXT(": End Turn has no tags"), Tag.Cost.IsEmpty() && Tag.Usage.IsEmpty()); }
            else { Test.TestTrue(Button + TEXT(": has both tags"), !Tag.Cost.IsEmpty() && !Tag.Usage.IsEmpty()); }
            if (Bar[Index] == EC::Attack)
            { Test.TestTrue(Button + TEXT(": the Move tag carries the movement left"), Tag.Cost.EndsWith(ExpeditionStyle::MoveBudgetText(Rules.Snapshot().MovementLeft))); }
            // Text is never shrunk below the dock's minimum: a name or tag that would need it is cut instead, and that is a failure here.
            Test.TestFalse(Button + TEXT(": no dock text needs more shrinking than the minimum scale allows"), Tag.bClipped);
            Test.TestTrue(Button + TEXT(": the label scale is at least the minimum"), Tag.LabelScale >= AExpeditionHUD::MinDockLabelScale - KINDA_SMALL_NUMBER);
            Test.TestTrue(Button + TEXT(": the tag scales are at least the minimum"),
                (Tag.CostScale == 0.0f || Tag.CostScale >= AExpeditionHUD::MinDockTagScale - KINDA_SMALL_NUMBER) && (Tag.UsageScale == 0.0f || Tag.UsageScale >= AExpeditionHUD::MinDockTagScale - KINDA_SMALL_NUMBER));
            Test.TestTrue(Button + TEXT(": the label is the button's name"), !Tag.Label.IsEmpty());
            Test.TestTrue(Button + TEXT(": the button is inside the viewport"),
                Tag.Box.Min.X >= -0.01 && Tag.Box.Min.Y >= -0.01 && Tag.Box.Max.X <= Size.X + 0.01 && Tag.Box.Max.Y <= Size.Y + 0.01);
            ++Checked;
        }
    }
    Game.Target.Size = FIntPoint(1280, 720);
    Game.Frame();
    return Checked;
}

/** What the facts say a tooltip for this command reads. */
FActionTooltipRequest ExpectedTooltip(const FCombatBattle& Rules, EC Command, const FString& Effect)
{
    const int32 Id = Rules.ActiveId();
    const FActionFacts Facts = FActionFactsQuery::For(Rules, Id, Command);
    const FActionFacts Move = FActionFactsQuery::For(Rules, Id, EC::Move);
    const ExpeditionStyle::FActionTooltipText Words = ExpeditionStyle::TooltipText(Facts, Command == EC::Attack ? &Move : nullptr);
    FActionTooltipRequest Expected;
    Expected.Command = Command;
    Expected.Headline = Words.Headline;
    Expected.Usage = Words.Usage;
    Expected.Effect = Effect;
    Expected.Reasons = Words.Reasons;
    return Expected;
}

FKey KeyFor(FName ButtonId)
{
    if (ButtonId == TEXT("ModeAttack")) { return EKeys::A; }
    if (ButtonId == TEXT("ModeRole")) { return EKeys::F; }
    if (ButtonId == TEXT("Maneuver")) { return EKeys::M; }
    if (ButtonId == TEXT("ModeRescue")) { return EKeys::R; }
    return EKeys::P;
}

struct FParity { int32 Tooltips = 0; int32 Unavailable = 0; int32 Spent = 0; };

/** Every dock button's tooltip by hover and by its key: the same words, from the facts, with the exact reasons when unavailable. */
void CheckTooltips(FAutomationTestBase& Test, ExpeditionInputHarness::FHarness& Game, const FString& Context, FParity& Parity)
{
    const TArray<FActionTagRequest> Tags = Game.Hud->ActionTagsRequested();
    const FCombatBattle& Rules = Game.View->Rules();
    const FCombatantState* Hero = Rules.Find(Rules.ActiveId());
    if (!Hero) { return; }
    const TArray<FAbilityFacts> Abilities = FBuildCatalog::Abilities(*Hero);
    for (const FActionTagRequest& Tag : Tags)
    {
        if (Tag.Command == EC::EndTurn) { continue; }
        const FString Where = Context + TEXT(", ") + Tag.ButtonId.ToString();
        FString Effect = FCombatBattle::DescribeAction(Tag.Command);
        for (const FAbilityFacts& Ability : Abilities) { if (Ability.Command == Tag.Command) { Effect = Ability.Description; } }
        const FActionTooltipRequest Expected = ExpectedTooltip(Rules, Tag.Command, Effect);

        Game.Player->PreviewAim(false);
        Game.Hud->PreviewPointer(Tag.Box.GetCenter());
        Game.Frame();
        const FActionTooltipRequest* Hover = Game.Hud->ActionTooltipRequested();
        if (!Test.TestNotNull(Where + TEXT(": hovering shows a tooltip, even when the action is unavailable"), Hover)) { Game.Hud->PreviewPointer(FVector2D(-1, -1)); continue; }
        const FActionTooltipRequest ByHover = *Hover;
        Game.Hud->PreviewPointer(FVector2D(-1, -1));
        Game.Frame();
        Test.TestNull(Where + TEXT(": no hover and no mode key means no tooltip"), Game.Hud->ActionTooltipRequested());

        Game.Press(KeyFor(Tag.ButtonId));
        const FActionTooltipRequest* ByKey = Game.Hud->ActionTooltipRequested();
        if (Test.TestNotNull(Where + TEXT(": choosing the mode by key shows a tooltip"), ByKey))
        { Test.TestTrue(Where + TEXT(": the key shows the same tooltip as hover"), *ByKey == ByHover); }
        Test.TestEqual(Where + TEXT(": headline is the facts' cost headline"), ByHover.Headline, Expected.Headline);
        Test.TestEqual(Where + TEXT(": usage lines are the facts'"), ByHover.Usage, Expected.Usage);
        Test.TestEqual(Where + TEXT(": effect is the catalog's effect-only text"), ByHover.Effect, Expected.Effect);
        Test.TestEqual(Where + TEXT(": the reasons are the facts' blockers, in order"), ByHover.Reasons, Expected.Reasons);
        ++Parity.Tooltips;
        if (!Expected.Reasons.IsEmpty()) { ++Parity.Unavailable; }
        if (Expected.Reasons.Contains(ExpeditionStyle::BlockerText({EActionBlocker::ActionSpent}))) { ++Parity.Spent; }
        Game.Player->PreviewAim(false);
    }
    Game.Frame();
}

/** Move toward the nearest enemy so attacks come into reach. */
void CloseIn(ExpeditionInputHarness::FHarness& Game)
{
    const FCombatBattle& Rules = Game.View->Rules();
    const int32 Id = Rules.ActiveId();
    const FCombatantState* Hero = Rules.Find(Id);
    if (!Hero) { return; }
    auto Nearest = [&Rules](FIntPoint Cell)
    {
        int32 Best = 1000;
        for (const FCombatantState& Enemy : Rules.Snapshot().Encounter.Combatants)
        { if (Enemy.Side == ECombatSide::Enemy && Enemy.HP > 0) { Best = FMath::Min(Best, ExpeditionTestSupport::Distance(Cell, Enemy.Cell)); } }
        return Best;
    };
    FIntPoint Best = Hero->Cell;
    bool bFound = false;
    for (const FIntPoint& Cell : Rules.LegalMoves(Id)) { if (!bFound || Nearest(Cell) < Nearest(Best)) { Best = Cell; bFound = true; } }
    if (bFound) { Game.View->Execute({EC::Move, Id, Best}); }
    Game.Settle();
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FActionTagsFromFactsTest, "Project.Expedition.Presentation.ActionTagsFromFacts",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FActionTagsFromFactsTest::RunTest(const FString& Parameters)
{
    using namespace ActionTagsTest;
    using ExpeditionInputHarness::FHarness;
    TSet<FString> Remaining;
    for (const FBuildDefinition& Build : FBuildCatalog::All()) { Remaining.Add(Build.Id); }
    const int32 Kits = Remaining.Num();
    int32 Checked = 0;
    // Four heroes to a party; a hero who is downed before their turn is retried in the next pass.
    for (int32 Pass = 0; Pass < 4 && !Remaining.IsEmpty(); ++Pass)
    {
        TArray<FString> Ids = Remaining.Array();
        Ids.Sort();
        for (int32 First = 0; First < Ids.Num(); First += 4)
        {
            TArray<FString> Party;
            for (int32 Slot = 0; Slot < 4; ++Slot) { Party.Add(Ids.IsValidIndex(First + Slot) ? Ids[First + Slot] : TEXT("fighter.champion")); }
            FExpeditionSaveData Data;
            if (!TestTrue(TEXT("A party checkpoint is made"), PartyCheckpoint(Party, 100 + Pass * 40 + First, Data))) { continue; }
            FHarness Game(*this, TEXT("ActionTags"), &Data);
            if (!TestTrue(TEXT("Input, board and HUD are ready"), Game.IsReady())) { continue; }
            Game.Frame();
            Game.Press(EKeys::O);
            if (!TestTrue(TEXT("The party resumes in combat"), Game.View->HasRun() && Game.View->InCombat())) { continue; }
            Game.Settle();
            for (int32 Turn = 0; Turn < 8 && Game.View->InCombat(); ++Turn)
            {
                Game.PartyTurn();
                if (!Game.View->InCombat()) { break; }
                const FCombatantState* Hero = Game.View->Rules().Find(Game.View->Rules().ActiveId());
                if (!Hero) { break; }
                const FString BuildId = Hero->BuildId;
                Checked += CheckTags(*this, Game, BuildId);
                Remaining.Remove(BuildId);
                Game.Press(EKeys::SpaceBar);
            }
        }
    }
    AddInfo(FString::Printf(TEXT("Checked %d dock buttons across %d kits at %d sizes"), Checked, Kits - Remaining.Num(), static_cast<int32>(UE_ARRAY_COUNT(Viewports))));
    TestTrue(FString::Printf(TEXT("Every kit's dock was checked (missing: %s)"), *FString::Join(Remaining.Array(), TEXT(", "))), Remaining.IsEmpty());
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FActionTagsHiddenTest, "Project.Expedition.Presentation.ActionTagsHiddenUnderOverlays",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FActionTagsHiddenTest::RunTest(const FString& Parameters)
{
    using namespace ActionTagsTest;
    using ExpeditionInputHarness::FHarness;
    FHarness Game(*this, TEXT("ActionTagsHidden"));
    if (!TestTrue(TEXT("Input, board and HUD are ready"), Game.IsReady())) { return false; }
    Game.Frame();
    Game.Press(EKeys::Enter);
    Game.Settle();
    Game.Press(EKeys::Enter);
    if (!TestTrue(TEXT("The first route starts an encounter"), Game.View->InCombat())) { return false; }
    Game.PartyTurn();
    if (!TestTrue(TEXT("The party has the turn"), Game.View->InCombat() && Game.Player->ShowsTargeting())) { return false; }
    Game.Press(EKeys::F);
    TestFalse(TEXT("A party turn shows tags"), Game.Hud->ActionTagsRequested().IsEmpty());
    TestNotNull(TEXT("Choosing a mode by key shows its tooltip"), Game.Hud->ActionTooltipRequested());

    struct FOverlay { FKey Open; FKey Close; const TCHAR* Name; };
    const FOverlay Overlays[] = {{EKeys::Slash, EKeys::Escape, TEXT("Help")}, {EKeys::H, EKeys::H, TEXT("Records")}, {EKeys::BackSpace, EKeys::Escape, TEXT("Return question")}};
    for (const FOverlay& Overlay : Overlays)
    {
        Game.Press(Overlay.Open);
        TestTrue(FString::Printf(TEXT("%s is open"), Overlay.Name), Game.Player->OverlayOpen());
        TestTrue(FString::Printf(TEXT("Under %s no tag is requested"), Overlay.Name), Game.Hud->ActionTagsRequested().IsEmpty());
        TestNull(FString::Printf(TEXT("Under %s no tooltip is requested"), Overlay.Name), Game.Hud->ActionTooltipRequested());
        Game.Press(Overlay.Close);
        TestFalse(FString::Printf(TEXT("%s is closed"), Overlay.Name), Game.Player->OverlayOpen());
        TestFalse(FString::Printf(TEXT("Closing %s brings the tags back"), Overlay.Name), Game.Hud->ActionTagsRequested().IsEmpty());
    }
    // The tooltip belongs to the mode chosen by key; the turn change puts it away.
    Game.Press(EKeys::A);
    TestNotNull(TEXT("The Attack key shows its tooltip"), Game.Hud->ActionTooltipRequested());
    Game.Press(EKeys::SpaceBar);
    TestNull(TEXT("Ending the turn puts the tooltip away"), Game.Hud->ActionTooltipRequested());
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FActionTagsDuringPlaybackTest, "Project.Expedition.Presentation.ActionTagsHiddenDuringPlayback",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FActionTagsDuringPlaybackTest::RunTest(const FString& Parameters)
{
    using namespace ActionTagsTest;
    using ExpeditionInputHarness::FHarness;
    FHarness Game(*this, TEXT("ActionTagsPlayback"));
    if (!TestTrue(TEXT("Input, board and HUD are ready"), Game.IsReady())) { return false; }
    Game.Frame();
    Game.Press(EKeys::Enter);
    Game.Settle();
    Game.Press(EKeys::Enter);
    if (!TestTrue(TEXT("The first route starts an encounter"), Game.View->InCombat())) { return false; }
    Game.PartyTurn();
    if (!TestTrue(TEXT("The party has the turn"), Game.View->InCombat() && Game.Player->ShowsTargeting())) { return false; }
    const int32 Actor = Game.Player->Actor();
    TestFalse(TEXT("Tags are up before the command"), Game.Hud->ActionTagsRequested().IsEmpty());
    Game.Press(EKeys::A);
    TestNotNull(TEXT("The mode key shows a tooltip before the command"), Game.Hud->ActionTooltipRequested());
    // A move plays out on the board; no frame of that playback may request a tag or a tooltip.
    const TArray<FIntPoint> Moves = Game.View->Rules().LegalMoves(Actor);
    if (!TestFalse(TEXT("The actor has a move to play"), Moves.IsEmpty())) { return false; }
    Game.View->Execute({EC::Move, Actor, Moves.Last()});
    Game.Frame();
    int32 PlaybackFrames = 0, LeakedFrames = 0;
    for (int32 Step = 0; Step < 2000 && Game.View->IsPresenting(); ++Step)
    {
        ++PlaybackFrames;
        if (!Game.Hud->ActionTagsRequested().IsEmpty() || Game.Hud->ActionTooltipRequested() != nullptr) { ++LeakedFrames; }
        Game.Frame();
    }
    TestTrue(TEXT("Playback ran for the move"), PlaybackFrames > 0);
    TestEqual(TEXT("No frame of playback requested a tag or a tooltip"), LeakedFrames, 0);
    Game.Settle();
    if (!TestTrue(TEXT("The same hero can act again once playback is over"), Game.View->InCombat() && Game.Player->ShowsTargeting() && Game.Player->Actor() == Actor)) { return false; }
    // The move spent movement, so the tags are read afresh from the facts: the Move tag shows what is left.
    TestTrue(TEXT("The tags return after playback and follow the facts"), CheckTags(*this, Game, TEXT("After playback")) > 0);
    // A turn change gives the new hero's tags and no tooltip left over from the previous one.
    Game.Press(EKeys::SpaceBar);
    Game.PartyTurn();
    if (!TestTrue(TEXT("The party has the turn again"), Game.View->InCombat() && Game.Player->ShowsTargeting())) { return false; }
    TestTrue(TEXT("The new turn's tags follow its facts"), CheckTags(*this, Game, TEXT("After the turn change")) > 0);
    TestNull(TEXT("No tooltip is left over from the previous turn"), Game.Hud->ActionTooltipRequested());
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FActionTooltipTest, "Project.Expedition.Presentation.ActionTooltipsMatchHoverAndKeys",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FActionTooltipTest::RunTest(const FString& Parameters)
{
    using namespace ActionTagsTest;
    const TArray<TArray<FString>> Parties = {
        {TEXT("fighter.battle-master"), TEXT("rogue.phantom"), TEXT("wizard.evocation"), TEXT("cleric.life")},
        {TEXT("barbarian.wild-magic"), TEXT("warlock.fiend"), TEXT("artificer.armorer"), TEXT("monk.four-elements")},
    };
    FParity Parity;
    for (int32 PartyIndex = 0; PartyIndex < Parties.Num(); ++PartyIndex)
    {
        FExpeditionSaveData Data;
        if (!TestTrue(FString::Printf(TEXT("Party %d starts the first route"), PartyIndex), PartyCheckpoint(Parties[PartyIndex], 31 + PartyIndex, Data))) { continue; }
        ExpeditionInputHarness::FHarness Game(*this, TEXT("ActionTooltips"), &Data);
        if (!TestTrue(TEXT("Input, board and HUD are ready"), Game.IsReady())) { continue; }
        Game.Frame();
        Game.Press(EKeys::O);
        if (!TestTrue(FString::Printf(TEXT("Party %d resumes in combat"), PartyIndex), Game.View->HasRun() && Game.View->InCombat())) { continue; }
        Game.Settle();
        for (int32 Turn = 0; Turn < 8 && Game.View->InCombat(); ++Turn)
        {
            Game.PartyTurn();
            if (!Game.View->InCombat()) { break; }
            const int32 Id = Game.View->Rules().ActiveId();
            const FString Build = Game.View->Rules().Find(Id) ? Game.View->Rules().Find(Id)->BuildId : FString();
            CheckTooltips(*this, Game, Build + TEXT(", fresh turn"), Parity);
            // Spend the Action on an attack when one is in reach, then every Action-cost tooltip names it.
            CloseIn(Game);
            if (!Game.View->InCombat() || Game.View->Rules().ActiveId() != Id) { continue; }
            const TArray<int32> Targets = Game.View->Rules().LegalTargets(Id);
            if (!Targets.IsEmpty())
            {
                Game.View->Execute({EC::Attack, Id, {}, Targets[0]});
                Game.Settle();
                if (Game.View->InCombat() && Game.View->Rules().ActiveId() == Id) { CheckTooltips(*this, Game, Build + TEXT(", Action spent"), Parity); }
            }
            if (Game.View->InCombat() && Game.View->Rules().ActiveId() == Id) { Game.Press(EKeys::SpaceBar); }
        }
    }
    AddInfo(FString::Printf(TEXT("Compared %d tooltips by hover and by key; %d unavailable, %d with the Action spent"), Parity.Tooltips, Parity.Unavailable, Parity.Spent));
    TestTrue(TEXT("Many tooltips were compared"), Parity.Tooltips >= 12);
    TestTrue(TEXT("Some unavailable action showed its reasons"), Parity.Unavailable > 0);
    TestTrue(TEXT("Some Action-spent state showed its reason"), Parity.Spent > 0);
    return !HasAnyErrors();
}

namespace ActionTagsTest
{
FActionResourceCost Spend(EActionResource Resource, int32 Amount, int32 Held, int32 Cap, int32 Recovery)
{
    FActionResourceCost Cost;
    Cost.Resource = Resource; Cost.Amount = Amount; Cost.Held = Held; Cost.Cap = Cap; Cost.CampRecovery = Recovery;
    return Cost;
}
}

/**
 * The real guarantee that dock and tooltip words come from the facts: facts built by hand, changed one field at a time,
 * change the words the HUD's helpers give, in exactly the way the fact changed.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FActionTextFromFactsTest, "Project.Expedition.Presentation.ActionTextFromFacts",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FActionTextFromFactsTest::RunTest(const FString& Parameters)
{
    using namespace ActionTagsTest;
    using namespace ExpeditionStyle;
    FActionFacts Facts;
    Facts.Command = EC::Special1; Facts.Name = TEXT("Flurry"); Facts.Cost = EActionCostKind::Action; Facts.bAvailable = true;
    Facts.Resources = {Spend(EActionResource::Kit, 1, 2, 3, 1)};
    const FString Cost = CostPillText(Facts), Usage = UsageTagText(Facts);
    TestEqual(TEXT("Cost tag"), Cost, FString(TEXT("Action")));
    TestEqual(TEXT("Usage tag: the resource with held and cap"), Usage, FString(TEXT("Kit 2/3")));

    Facts.Cost = EActionCostKind::BonusAction;
    TestEqual(TEXT("A changed cost changes the cost tag"), CostPillText(Facts), FString(TEXT("Bonus")));
    TestEqual(TEXT("... and the headline"), TooltipText(Facts).Headline, FString(TEXT("Flurry · Bonus action")));
    Facts.Resources[0].Held = 0;
    TestEqual(TEXT("A changed count changes the usage tag"), UsageTagText(Facts), FString(TEXT("Kit 0/3")));
    Facts.Resources[0].Amount = 2;
    TestEqual(TEXT("A changed cost amount shows"), UsageTagText(Facts), FString(TEXT("2 Kit 0/3")));
    Facts.Resources[0].Cap = INDEX_NONE;
    TestEqual(TEXT("Without a cap only the count shows"), UsageTagText(Facts), FString(TEXT("2 Kit 0")));
    Facts.Resources = {Spend(EActionResource::Kit, 1, 1, 1, 1), Spend(EActionResource::Slot, 1, 2, 2, 2)};
    TestEqual(TEXT("Two resources are both on the button, shortened to fit"), UsageTagText(Facts), FString(TEXT("Kit 1/1 + Slot 2/2")));
    TestTrue(TEXT("... and the tooltip keeps their full detail, one line each"),
        UsageLines(Facts) == TArray<FString>({TEXT("1 Kit · 1/1 · Camp restores all"), TEXT("1 Slot · 2/2 · Camp restores all")}));
    Facts.Resources.Reset();
    TestEqual(TEXT("No resource is at will"), UsageTagText(Facts), FString(TEXT("At will")));
    Facts.bRequiresRage = true;
    TestEqual(TEXT("A stance requirement shows when it is the only usage"), UsageTagText(Facts), FString(TEXT("Needs Rage")));
    Facts.Resources = {Spend(EActionResource::Rage, 1, 2, 2, 1)};
    Facts.bRequiresRage = false; Facts.bForbidsRage = true;
    TestEqual(TEXT("With a resource the button shows the resource, short"), UsageTagText(Facts), FString(TEXT("Rage 2/2")));
    TestTrue(TEXT("... and the tooltip carries the stance"), UsageLines(Facts).Contains(TEXT("Not while raging")));
    Facts.Cost = EActionCostKind::None;
    TestTrue(TEXT("No cost means no tags"), CostPillText(Facts).IsEmpty() && UsageTagText(Facts).IsEmpty() && UsageLines(Facts).IsEmpty());

    // Blockers become the tooltip's reasons, in order and unchanged; an available action has none.
    FActionFacts Blocked;
    Blocked.Command = EC::MagicMissile; Blocked.Cost = EActionCostKind::Action;
    FActionBlocker Spent; Spent.Reason = EActionBlocker::ActionSpent;
    FActionBlocker Short; Short.Reason = EActionBlocker::ResourceShort; Short.Resource = EActionResource::Slot; Short.Held = 0; Short.Needed = 1;
    FActionBlocker Far; Far.Reason = EActionBlocker::OutOfRange; Far.Range = 6;
    Blocked.Blockers = {Spent, Short, Far};
    TestTrue(TEXT("The reasons are the blockers' text, in order"),
        TooltipText(Blocked).Reasons == TArray<FString>({BlockerText(Spent), BlockerText(Short), BlockerText(Far)}));
    Blocked.Blockers = {Far, Spent};
    TestTrue(TEXT("A different order gives a different order"), TooltipText(Blocked).Reasons == TArray<FString>({BlockerText(Far), BlockerText(Spent)}));
    Blocked.bAvailable = true;
    TestTrue(TEXT("An available action gives no reasons"), TooltipText(Blocked).Reasons.IsEmpty());

    // The Attack / Move button explains Attack even while Move is possible, and each reason once.
    FActionFacts Attack, Move;
    Attack.Command = EC::Attack; Attack.Cost = EActionCostKind::Action; Attack.Blockers = {Spent};
    Move.Command = EC::Move; Move.Cost = EActionCostKind::Movement; Move.bAvailable = true;
    TestTrue(TEXT("With movement left the Attack blocker still shows"), TooltipText(Attack, &Move).Reasons == TArray<FString>({BlockerText(Spent)}));
    FActionBlocker NoMove; NoMove.Reason = EActionBlocker::NoMovementLeft;
    Move.bAvailable = false; Move.Blockers = {Spent, NoMove};
    TestTrue(TEXT("Both lists show, the shared reason once"), TooltipText(Attack, &Move).Reasons == TArray<FString>({BlockerText(Spent), BlockerText(NoMove)}));
    Attack.Cost = EActionCostKind::Action;
    TestEqual(TEXT("The Attack / Move pill carries the movement left"), AttackMovePillText(Attack, Move, 6), FString(TEXT("Action · Move 6")));
    TestEqual(TEXT("... and follows it"), AttackMovePillText(Attack, Move, 0), FString(TEXT("Action · Move 0")));
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FActionDockCacheTest, "Project.Expedition.Presentation.ActionDockRebuildsOnlyOnStateChange",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FActionDockCacheTest::RunTest(const FString& Parameters)
{
    using namespace ActionTagsTest;
    using ExpeditionInputHarness::FHarness;
    FHarness Game(*this, TEXT("ActionDockCache"));
    if (!TestTrue(TEXT("Input, board and HUD are ready"), Game.IsReady())) { return false; }
    Game.Frame();
    Game.Press(EKeys::Enter);
    Game.Settle();
    Game.Press(EKeys::Enter);
    if (!TestTrue(TEXT("The first route starts an encounter"), Game.View->InCombat())) { return false; }
    Game.PartyTurn();
    if (!TestTrue(TEXT("The party has the turn"), Game.View->InCombat() && Game.Player->ShowsTargeting())) { return false; }
    Game.Frame();
    const int32 Before = Game.Hud->DockRebuilds();
    for (int32 Frame = 0; Frame < 20; ++Frame) { Game.Frame(); }
    Game.Press(EKeys::A); Game.Press(EKeys::F); Game.Hud->PreviewPointer(FVector2D(20, 20)); Game.Frame(); Game.Hud->PreviewPointer(FVector2D(-1, -1));
    TestEqual(TEXT("Frames, mode keys and hover rebuild nothing"), Game.Hud->DockRebuilds(), Before);
    const int32 Actor = Game.Player->Actor();
    const TArray<FIntPoint> Moves = Game.View->Rules().LegalMoves(Actor);
    if (!TestFalse(TEXT("The actor has a move"), Moves.IsEmpty())) { return false; }
    Game.View->Execute({EC::Move, Actor, Moves.Last()});
    Game.Settle();
    TestTrue(TEXT("A command changes the state, so the dock rebuilds"), Game.Hud->DockRebuilds() > Before);
    // The rebuilt dock is the new state's: the Move tag follows the movement spent (CheckTags compares every tag with the live facts).
    TestTrue(TEXT("The rebuilt tags follow the facts"), CheckTags(*this, Game, TEXT("After the move")) > 0);
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRejectedCommandTest, "Project.Expedition.Presentation.RejectedCommandsNameTheirBlocker",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRejectedCommandTest::RunTest(const FString& Parameters)
{
    using namespace ActionTagsTest;
    FExpeditionSaveData Data;
    if (!TestTrue(TEXT("A Barbarian party checkpoint"), PartyCheckpoint({TEXT("barbarian.berserker"), TEXT("fighter.battle-master"), TEXT("cleric.life"), TEXT("rogue.thief")}, 41, Data))) { return false; }
    ExpeditionInputHarness::FHarness Game(*this, TEXT("RejectedCommand"), &Data);
    if (!TestTrue(TEXT("Input, board and HUD are ready"), Game.IsReady())) { return false; }
    Game.Frame();
    Game.Press(EKeys::O);
    if (!TestTrue(TEXT("The party resumes in combat"), Game.View->HasRun() && Game.View->InCombat())) { return false; }
    Game.Settle();
    bool bBarbarian = false;
    for (int32 Turn = 0; Turn < 8 && Game.View->InCombat(); ++Turn)
    {
        Game.PartyTurn();
        const FCombatantState* Hero = Game.View->InCombat() ? Game.View->Rules().Find(Game.View->Rules().ActiveId()) : nullptr;
        if (!Hero) { break; }
        if (Hero->BuildId == TEXT("barbarian.berserker")) { bBarbarian = true; break; }
        Game.Press(EKeys::SpaceBar);
    }
    if (!TestTrue(TEXT("The Berserker has a turn"), bBarbarian)) { return false; }
    const FCombatBattle& Rules = Game.View->Rules();
    const int32 Id = Rules.ActiveId();
    TestEqual(TEXT("The Berserker is not raging"), Rules.Find(Id)->RageTurns, 0);
    const FCombatantState* Foe = nullptr;
    for (const FCombatantState& Unit : Rules.Snapshot().Encounter.Combatants) { if (Unit.Side == ECombatSide::Enemy && Unit.HP > 0) { Foe = &Unit; break; } }
    if (!TestNotNull(TEXT("An enemy stands"), Foe)) { return false; }
    const FExpeditionSnapshot Before = Game.Live();

    // Frenzy (M) needs Rage: choose it by key, aim at an enemy, confirm. The rules refuse; the log says why, and the tooltip stays.
    Game.Player->PreviewAim(true, Foe->Cell);
    Game.Press(EKeys::M);
    const FActionTooltipRequest* Tip = Game.Hud->ActionTooltipRequested();
    if (!TestNotNull(TEXT("M shows Frenzy's tooltip"), Tip)) { return false; }
    TestEqual(TEXT("It is Frenzy's"), Tip->Command, EC::Special2);
    const FActionFacts Facts = FActionFactsQuery::For(Rules, Id, EC::Special2);
    if (!TestFalse(TEXT("Frenzy has a blocker"), Facts.Blockers.IsEmpty())) { return false; }
    TestEqual(TEXT("The first blocker is the stance"), Facts.Blockers[0].Reason, EActionBlocker::StanceRequired);
    Game.Press(EKeys::Enter);
    TestTrue(TEXT("The rules refused: the run is unchanged"), ExpeditionTestSupport::SameRun(Before, Game.Live()));
    if (TestFalse(TEXT("The log has a message"), Game.View->Messages().IsEmpty()))
    {
        const FString Message = Game.View->Messages().Last();
        TestEqual(TEXT("The log gives the facts' first blocker"), Message, TEXT("Cannot act: ") + ExpeditionStyle::BlockerText(Facts.Blockers[0]));
        TestFalse(TEXT("... not the generic list"), Message.Contains(TEXT("budget, stance")));
    }
    TestTrue(TEXT("A rejected command keeps the mode tooltip up"), Game.Player->ModeTipActive() && Game.Hud->ActionTooltipRequested() != nullptr);
    if (const FActionTooltipRequest* Kept = Game.Hud->ActionTooltipRequested()) { TestTrue(TEXT("... showing the same reason"), Kept->Reasons.Contains(ExpeditionStyle::BlockerText(Facts.Blockers[0]))); }

    // A target-specific refusal the action-wide facts cannot explain keeps the rules' own text: a move onto an enemy's cell.
    Game.Player->PreviewAim(false);
    TestTrue(TEXT("Moving is possible, so the facts have no blocker for it"), FActionFactsQuery::For(Rules, Id, EC::Move).Blockers.IsEmpty());
    Game.View->Execute({EC::Move, Id, Foe->Cell});
    if (TestFalse(TEXT("The refusal is logged"), Game.View->Messages().IsEmpty()))
    {
        const FString Message = Game.View->Messages().Last();
        TestTrue(TEXT("It is the rules' text, prefixed"), Message.StartsWith(TEXT("Cannot act: ")) && Message.Len() > 12);
        for (uint8 Reason = 0; Reason < static_cast<uint8>(EActionBlocker::Count); ++Reason)
        {
            FActionBlocker Blocker; Blocker.Reason = static_cast<EActionBlocker>(Reason); Blocker.Resource = EActionResource::Slot; Blocker.Needed = 1; Blocker.Range = 1;
            TestNotEqual(TEXT("It is not one of the facts' phrases"), Message, TEXT("Cannot act: ") + ExpeditionStyle::BlockerText(Blocker));
        }
    }
    TestTrue(TEXT("Nothing changed by either refusal"), ExpeditionTestSupport::SameRun(Before, Game.Live()));
    return !HasAnyErrors();
}

/** What the build picker should say for a kit: each ability's name, then its cost and usage from the facts without a battle. */
/**
 * An SRD run uses the same dock. The fact query knows only the Compact commands, so Grapple, Shove and Escape have no
 * button and no facts (SRD facts belong to the rules work); everything the dock does draw must still come from the facts,
 * and a rejected SRD-only command must keep the rules' own text rather than a wrong fact reason.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FActionDockSrdTest, "Project.Expedition.Presentation.ActionDockDegradesInSrdMode",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FActionDockSrdTest::RunTest(const FString& Parameters)
{
    using namespace ActionTagsTest;
    FExpeditionSaveData Data;
    if (!TestTrue(TEXT("An SRD party checkpoint"), PartyCheckpoint({TEXT("fighter.battle-master"), TEXT("wizard.evocation"), TEXT("cleric.life"), TEXT("barbarian.berserker")}, 51, Data, ERuleset::SRD))) { return false; }
    ExpeditionInputHarness::FHarness Game(*this, TEXT("ActionDockSrd"), &Data);
    if (!TestTrue(TEXT("Input, board and HUD are ready"), Game.IsReady())) { return false; }
    Game.Frame();
    Game.Press(EKeys::O);
    if (!TestTrue(TEXT("The SRD party resumes in combat"), Game.View->HasRun() && Game.View->InCombat() && Game.View->Rules().Snapshot().Encounter.Ruleset == ERuleset::SRD)) { return false; }
    Game.Settle();
    FParity Parity;
    int32 Heroes = 0;
    for (int32 Turn = 0; Turn < 6 && Game.View->InCombat(); ++Turn)
    {
        Game.PartyTurn();
        if (!Game.View->InCombat()) { break; }
        const FCombatBattle& Rules = Game.View->Rules();
        const int32 Id = Rules.ActiveId();
        TestTrue(TEXT("The SRD dock draws tags that follow the facts"), CheckTags(*this, Game, TEXT("SRD")) > 0);
        CheckTooltips(*this, Game, TEXT("SRD"), Parity);
        // No dock button stands for the SRD-only commands.
        for (const FActionTagRequest& Tag : Game.Hud->ActionTagsRequested())
        { TestFalse(TEXT("No dock button is a Grapple, Shove or Escape"), Tag.Command == EC::Grapple || Tag.Command == EC::Shove || Tag.Command == EC::Escape); }
        // The facts for them degrade to "not in this kit", with no cost and no text that pretends otherwise.
        for (const EC Command : {EC::Grapple, EC::Shove, EC::Escape})
        {
            const FActionFacts Facts = FActionFactsQuery::For(Rules, Id, Command);
            TestFalse(TEXT("An SRD-only command is not reported available"), Facts.bAvailable);
            TestTrue(TEXT("It has no cost and no resources"), Facts.Cost == EActionCostKind::None && Facts.Resources.IsEmpty());
            TestTrue(TEXT("It reports one blocker, with text"), Facts.Blockers.Num() == 1 && Facts.Blockers[0].Reason == EActionBlocker::NotInKit && !ExpeditionStyle::BlockerText(Facts.Blockers[0]).IsEmpty());
            TestTrue(TEXT("Its tags are empty"), ExpeditionStyle::CostPillText(Facts).IsEmpty() && ExpeditionStyle::UsageTagText(Facts).IsEmpty());
        }
        // A rejected Grapple keeps the rules' own message.
        const FCombatantState* Foe = nullptr;
        for (const FCombatantState& Unit : Rules.Snapshot().Encounter.Combatants) { if (Unit.Side == ECombatSide::Enemy && Unit.HP > 0) { Foe = &Unit; break; } }
        if (Foe && ExpeditionTestSupport::Distance(Rules.Find(Id)->Cell, Foe->Cell) > 1)
        {
            const FExpeditionSnapshot Before = Game.Live();
            TestFalse(TEXT("A Grapple at range is rejected"), Game.View->Execute({EC::Grapple, Id, {}, Foe->Id}));
            if (!Game.View->Messages().IsEmpty())
            {
                const FString Message = Game.View->Messages().Last();
                TestTrue(TEXT("The log keeps the rules' text"), Message.StartsWith(TEXT("Cannot act: ")) && !Message.Contains(TEXT("Not in this kit")));
                AddInfo(TEXT("SRD Grapple rejection reads: ") + Message);
            }
            TestTrue(TEXT("The rejection changed nothing"), ExpeditionTestSupport::SameRun(Before, Game.Live()));
        }
        ++Heroes;
        Game.Press(EKeys::SpaceBar);
    }
    TestTrue(TEXT("Several SRD heroes were checked"), Heroes >= 3);
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBuildPickerFactsTest, "Project.Expedition.Presentation.BuildPickerShowsAbilityCosts",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FBuildPickerFactsTest::RunTest(const FString& Parameters)
{
    using namespace ActionTagsTest;
    using ExpeditionInputHarness::FHarness;
    FHarness Game(*this, TEXT("BuildPicker"));
    if (!TestTrue(TEXT("Input, board and HUD are ready"), Game.IsReady())) { return false; }
    Game.Frame();
    int32 Checked = 0;
    for (const FBuildDefinition& Build : FBuildCatalog::All())
    {
        if (!TestTrue(Build.Id + TEXT(" can be selected in the picker"), Game.Player->SelectBuild(Build.Id))) { continue; }
        Game.Frame();
        TArray<FString> Expected;
        for (const FAbilityFacts& Ability : FBuildCatalog::Abilities(Build.Profile))
        {
            Expected.Add(Ability.Name);
            Expected.Add(ExpeditionStyle::AbilitySummaryText(FActionFactsQuery::ForBuild(Build.Profile, Ability.Command)));
        }
        TestTrue(Build.Id + TEXT(": the picker draws every ability's name and its facts-derived cost line"), Game.Hud->BuildPickerLinesRequested() == Expected);
        ++Checked;
    }
    TestEqual(TEXT("Every kit was shown"), Checked, FBuildCatalog::All().Num());
    // The two the reviewer named: Frenzy says Bonus and Rage, Magic Missile says Action, Kit and Slot.
    if (Game.Player->SelectBuild(TEXT("barbarian.berserker")))
    {
        Game.Frame();
        const TArray<FString>& Lines = Game.Hud->BuildPickerLinesRequested();
        const int32 At = Lines.IndexOfByKey(TEXT("Frenzy"));
        if (TestTrue(TEXT("Berserker shows Frenzy"), At != INDEX_NONE && Lines.IsValidIndex(At + 1)))
        { TestTrue(TEXT("Frenzy needs Rage and spends a Bonus action"), Lines[At + 1].Contains(TEXT("Bonus action")) && Lines[At + 1].Contains(TEXT("Needs Rage"))); }
    }
    if (Game.Player->SelectBuild(TEXT("wizard.evocation")))
    {
        Game.Frame();
        const TArray<FString>& Lines = Game.Hud->BuildPickerLinesRequested();
        const int32 At = Lines.IndexOfByKey(TEXT("Magic Missile"));
        if (TestTrue(TEXT("Evocation shows Magic Missile"), At != INDEX_NONE && Lines.IsValidIndex(At + 1)))
        { TestTrue(TEXT("Magic Missile costs an Action, a Kit and a Slot"), Lines[At + 1].Contains(TEXT("Action")) && Lines[At + 1].Contains(TEXT("1 Kit")) && Lines[At + 1].Contains(TEXT("1 Slot"))); }
    }
    return !HasAnyErrors();
}

/** ForBuild runs the same shape code as For: on a fresh battle they agree for every action of every kit. */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FForBuildMatchesForTest, "Project.Expedition.Combat.ActionFactsForBuildMatchesFor",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FForBuildMatchesForTest::RunTest(const FString& Parameters)
{
    using EC = ECombatCommandType;
    int32 Commands = 0;
    for (const FBuildDefinition& Build : FBuildCatalog::All())
    {
        FCombatEncounter Encounter;
        Encounter.Width = Encounter.Height = 8;
        FCombatantState Hero = Build.Profile;
        Hero.Id = 10; Hero.Name = TEXT("Actor"); Hero.Cell = {1, 1}; Hero.InitiativeBonus = 30;
        FCombatantState Foe;
        Foe.Id = 20; Foe.Name = TEXT("Target"); Foe.Role = TEXT("Raider"); Foe.Side = ECombatSide::Enemy; Foe.Cell = {2, 1}; Foe.InitiativeBonus = -10;
        Encounter.Combatants = {Hero, Foe};
        FCombatBattle Battle;
        FString Error;
        if (!TestTrue(Build.Id + TEXT(" starts"), Battle.Start(Encounter, 42, Error))) { continue; }
        for (const EC Command : FActionFactsQuery::BarFor(Battle, 10))
        {
            const FActionFacts Live = FActionFactsQuery::For(Battle, 10, Command);
            const FActionFacts Static = FActionFactsQuery::ForBuild(Build.Profile, Command);
            const FString Where = FString::Printf(TEXT("%s / command %d"), *Build.Id, static_cast<int32>(Command));
            TestEqual(Where + TEXT(": command"), Static.Command, Live.Command);
            TestEqual(Where + TEXT(": name"), Static.Name, Live.Name);
            TestEqual(Where + TEXT(": cost"), Static.Cost, Live.Cost);
            TestEqual(Where + TEXT(": target kind"), Static.Target, Live.Target);
            TestEqual(Where + TEXT(": range"), Static.Range, Live.Range);
            TestEqual(Where + TEXT(": needs sight"), Static.bNeedsSight, Live.bNeedsSight);
            TestEqual(Where + TEXT(": needs a wounded target"), Static.bNeedsWounded, Live.bNeedsWounded);
            TestEqual(Where + TEXT(": requires Rage"), Static.bRequiresRage, Live.bRequiresRage);
            TestEqual(Where + TEXT(": forbids Rage"), Static.bForbidsRage, Live.bForbidsRage);
            if (TestEqual(Where + TEXT(": resource count"), Static.Resources.Num(), Live.Resources.Num()))
            {
                for (int32 Index = 0; Index < Live.Resources.Num(); ++Index)
                {
                    TestEqual(Where + TEXT(": resource"), Static.Resources[Index].Resource, Live.Resources[Index].Resource);
                    TestEqual(Where + TEXT(": amount"), Static.Resources[Index].Amount, Live.Resources[Index].Amount);
                    TestEqual(Where + TEXT(": held"), Static.Resources[Index].Held, Live.Resources[Index].Held);
                    TestEqual(Where + TEXT(": cap"), Static.Resources[Index].Cap, Live.Resources[Index].Cap);
                    TestEqual(Where + TEXT(": camp recovery"), Static.Resources[Index].CampRecovery, Live.Resources[Index].CampRecovery);
                }
            }
            TestTrue(Where + TEXT(": a static form carries no availability"), !Static.bAvailable && Static.Blockers.IsEmpty());
            ++Commands;
        }
        const bool bHasSecond = FBuildCatalog::Abilities(Build.Profile).ContainsByPredicate([](const FAbilityFacts& A) { return A.Command == EC::Special2; });
        TestEqual(Build.Id + TEXT(": a command the kit lacks has no cost"), FActionFactsQuery::ForBuild(Build.Profile, EC::Special2).Cost != EActionCostKind::None, bHasSecond);
    }
    AddInfo(FString::Printf(TEXT("Compared %d commands"), Commands));
    TestTrue(TEXT("Every kit's bar was compared"), Commands > 70 * 4);
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDescriptionsNoCostWordsTest, "Project.Expedition.Presentation.DescriptionsHaveNoCostWords",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FDescriptionsNoCostWordsTest::RunTest(const FString& Parameters)
{
    using namespace ActionTagsTest;
    // What the facts now supply: the cost kind, the resources and their names, camp recovery and the Rage stance.
    TArray<FString> Banned = {TEXT("action"), TEXT("bonus"), TEXT("camp"), TEXT("raging"), TEXT(" use"), TEXT("no item"), TEXT("bottle"), TEXT("refill"), TEXT("cost")};
    for (uint8 Resource = 1; Resource <= static_cast<uint8>(EActionResource::Potion); ++Resource)
    { Banned.Add(ExpeditionStyle::ResourceName(static_cast<EActionResource>(Resource)).ToLower()); }
    int32 Texts = 0;
    auto Check = [&](const FString& Where, const FString& Text)
    {
        ++Texts;
        TestFalse(Where + TEXT(" has text"), Text.IsEmpty());
        for (const FString& Word : Banned) { TestFalse(FString::Printf(TEXT("%s (\"%s\") does not say \"%s\""), *Where, *Text, *Word), Text.ToLower().Contains(Word)); }
    };
    for (const FBuildDefinition& Build : FBuildCatalog::All())
    {
        for (const FAbilityFacts& Ability : Build.Abilities) { Check(Build.Id + TEXT(" / ") + Ability.Name, Ability.Description); }
        // Kits without authored abilities are described from DescribeAction.
        for (const FAbilityFacts& Ability : FBuildCatalog::Abilities(Build.Profile)) { Check(Build.Id + TEXT(" / ") + Ability.Name + TEXT(" (as the HUD reads it)"), Ability.Description); }
    }
    for (const EC Command : {EC::Attack, EC::Move, EC::SecondWind, EC::CureWounds, EC::MagicMissile, EC::Rescue, EC::Potion, EC::PrecisionStrike, EC::Special1, EC::Special2})
    { Check(FString::Printf(TEXT("DescribeAction(%d)"), static_cast<int32>(Command)), FCombatBattle::DescribeAction(Command)); }
    AddInfo(FString::Printf(TEXT("Checked %d description texts"), Texts));
    TestTrue(TEXT("Every kit's descriptions were read"), Texts > 70 * 2);
    // Catalog text only: descriptions do not move the revision saves carry (8 since the #184 Compact enemy tuning, 9 since the
    // #188 encounter identity, 10 since the #218 reward cards).
    TestEqual(TEXT("The catalog revision is the tuning's, not the text's"), FBuildCatalog::Revision, 10);
    return !HasAnyErrors();
}

/**
 * A supplementary lint, not the guarantee. The guarantee is ActionTextFromFacts below: it builds facts by hand, changes
 * their cost, resources, stance and blockers, and checks that the text a dock button and tooltip show changes with them,
 * so text that did not come from the facts could not follow. This only reads the HUD source for the obvious ways of
 * hard-coding it again (a literal "Action", a resource name, "At will"), which would pass a comparison with the formatter.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDockTextFromFactsTest, "Project.Expedition.Presentation.DockTextComesFromFacts",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FDockTextFromFactsTest::RunTest(const FString& Parameters)
{
    const FString Path = FPaths::Combine(FPaths::ProjectDir(), TEXT("Source/DnDDiceRoll/Private/Presentation/ExpeditionHUD.cpp"));
    FString Source;
    if (!TestTrue(TEXT("The HUD source is readable from the project"), FFileHelper::LoadFileToString(Source, *Path))) { return false; }
    const int32 Begin = Source.Find(TEXT("float AExpeditionHUD::FitScale"));
    const int32 End = Source.Find(TEXT("void AExpeditionHUD::DrawForecast"));
    if (!TestTrue(TEXT("The dock code is found"), Begin != INDEX_NONE && End > Begin)) { return false; }
    // Comments may say anything; only code counts.
    TArray<FString> Lines;
    Source.Mid(Begin, End - Begin).ParseIntoArrayLines(Lines, false);
    FString Dock;
    for (FString& Line : Lines)
    {
        const int32 Comment = Line.Find(TEXT("//"));
        Dock += (Comment == INDEX_NONE ? Line : Line.Left(Comment)) + TEXT("\n");
    }
    const TCHAR* Banned[] = {
        TEXT("TEXT(\"Action"), TEXT("TEXT(\"Bonus"), TEXT("TEXT(\"Move %d"), TEXT("TEXT(\"Move "), TEXT("At will"), TEXT("\"Kit"), TEXT("\"Slot"), TEXT("\"Wind"), TEXT("\"Rage"), TEXT("\"Potion %d"),
        TEXT(" left"), TEXT(" none"), TEXT("bottle"), TEXT("TEXT(\"unavailable"), TEXT("LegalTargets"), TEXT("LegalMoves"), TEXT("ResourceCost"), TEXT("bResourceAvailable"), TEXT("Ability->bBonus")};
    for (const TCHAR* Word : Banned)
    { TestFalse(FString::Printf(TEXT("The dock code does not hard-code \"%s\""), Word), Dock.Contains(Word, ESearchCase::CaseSensitive)); }
    const TCHAR* Required[] = {
        TEXT("FActionFactsQuery::BarFor"), TEXT("FActionFactsQuery::For"), TEXT("ExpeditionStyle::CostPillText"), TEXT("ExpeditionStyle::UsageTagText"),
        TEXT("ExpeditionStyle::AttackMovePillText"), TEXT("ExpeditionStyle::TooltipText"),
        TEXT("ExpeditionStyle::CostName"), TEXT("ExpeditionStyle::MoveBudgetText")};
    for (const TCHAR* Word : Required)
    { TestTrue(FString::Printf(TEXT("The dock code uses %s"), Word), Dock.Contains(Word, ESearchCase::CaseSensitive)); }
    return !HasAnyErrors();
}
#endif
