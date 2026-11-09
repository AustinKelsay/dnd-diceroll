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
bool PartyCheckpoint(const TArray<FString>& Builds, int32 Seed, FExpeditionSaveData& Data)
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
    if (!Run.NewRun(Seed, Party, Error, FString::Printf(TEXT("action-tags-%d"), Seed))) { return false; }
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
            Test.TestEqual(Button + TEXT(": usage tag is the formatter's"), Tag.Usage, ExpeditionStyle::UsagePillText(Facts));
            if (Bar[Index] == EC::EndTurn) { Test.TestTrue(Button + TEXT(": End Turn has no tags"), Tag.Cost.IsEmpty() && Tag.Usage.IsEmpty()); }
            else { Test.TestTrue(Button + TEXT(": has both tags"), !Tag.Cost.IsEmpty() && !Tag.Usage.IsEmpty()); }
            if (Bar[Index] == EC::Attack)
            { Test.TestTrue(Button + TEXT(": the Move tag carries the movement left"), Tag.Cost.EndsWith(ExpeditionStyle::MoveBudgetText(Rules.Snapshot().MovementLeft))); }
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
    FActionTooltipRequest Expected;
    Expected.Command = Command;
    Expected.Headline = ExpeditionStyle::TooltipHeadline(Facts);
    Expected.Usage = ExpeditionStyle::UsageLines(Facts);
    Expected.Effect = Effect;
    const bool bBoth = Command == EC::Attack;
    if (!Facts.bAvailable && (!bBoth || !Move.bAvailable))
    {
        for (const FActionBlocker& Blocker : Facts.Blockers) { Expected.Reasons.AddUnique(ExpeditionStyle::BlockerText(Blocker)); }
        if (bBoth) { for (const FActionBlocker& Blocker : Move.Blockers) { Expected.Reasons.AddUnique(ExpeditionStyle::BlockerText(Blocker)); } }
    }
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
    // Catalog text only: the revision that saves carry is untouched.
    TestEqual(TEXT("The catalog revision is unchanged"), FBuildCatalog::Revision, 7);
    return !HasAnyErrors();
}

/**
 * The HUD code that draws the dock holds no cost or usage wording of its own: it asks the facts and the text helpers.
 * This reads the source because a hard-coded "Action" would equal the formatter's output and so pass the behavioural tests.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDockTextFromFactsTest, "Project.Expedition.Presentation.DockTextComesFromFacts",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FDockTextFromFactsTest::RunTest(const FString& Parameters)
{
    const FString Path = FPaths::Combine(FPaths::ProjectDir(), TEXT("Source/DnDDiceRoll/Private/Presentation/ExpeditionHUD.cpp"));
    FString Source;
    if (!TestTrue(TEXT("The HUD source is readable from the project"), FFileHelper::LoadFileToString(Source, *Path))) { return false; }
    const int32 Begin = Source.Find(TEXT("bool AExpeditionHUD::DockButton"));
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
        TEXT("FActionFactsQuery::BarFor"), TEXT("FActionFactsQuery::For"), TEXT("ExpeditionStyle::CostPillText"), TEXT("ExpeditionStyle::UsagePillText"),
        TEXT("ExpeditionStyle::AttackMovePillText"), TEXT("ExpeditionStyle::BlockerText"), TEXT("ExpeditionStyle::TooltipHeadline"), TEXT("ExpeditionStyle::UsageLines"),
        TEXT("ExpeditionStyle::CostName"), TEXT("ExpeditionStyle::MoveBudgetText")};
    for (const TCHAR* Word : Required)
    { TestTrue(FString::Printf(TEXT("The dock code uses %s"), Word), Dock.Contains(Word, ESearchCase::CaseSensitive)); }
    return !HasAnyErrors();
}
#endif
