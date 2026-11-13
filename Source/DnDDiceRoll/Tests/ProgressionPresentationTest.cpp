#if WITH_DEV_AUTOMATION_TESTS
#include "ExpeditionInputHarness.h"
#include "Presentation/ProgressionText.h"
#include "Presentation/TargetChipLayout.h"
#include "Run/ProgressionQuery.h"

/**
 * #218 PR B (contract 13 sections 7 and 8): the reward cards, the camp lines and the card keys, on the real HUD and controller.
 * Every word the HUD shows of progression must be the query's facts formatted by ProgressionText.
 */
namespace ProgressionPresentationTest
{
using namespace ExpeditionTestSupport;

TArray<FPartyBuildSelection> Party()
{
    const TCHAR* Builds[] = {TEXT("rogue.thief"), TEXT("cleric.life"), TEXT("barbarian.berserker"), TEXT("fighter.battle-master")};
    TArray<FPartyBuildSelection> Result;
    for (int32 I = 0; I < 4; ++I) { FPartyBuildSelection Slot; Slot.Id = I + 1; Slot.Name = FString::Printf(TEXT("Hero %d"), I + 1); Slot.BuildId = Builds[I]; Result.Add(Slot); }
    return Result;
}

int32 FirstSurvivor(const FExpeditionRun& Run)
{
    for (const FCombatantState& U : Run.Battle().Snapshot().Encounter.Combatants) { if (U.Side == ECombatSide::Party && U.Life == ECombatLife::Conscious) { return U.Id; } }
    return INDEX_NONE;
}

/** The scripted player to the Reward phase of victory Victory; Claim decides each earlier reward. */
bool ReachReward(FExpeditionRun& Run, int32 Seed, int32 Victory, TFunctionRef<bool(FExpeditionRun&)> Claim, int32 Revision = FBuildCatalog::Revision)
{
    FString Error;
    if (!Run.NewRun(Seed, Party(), Error, FString::Printf(TEXT("reward-ui-%d"), Seed), ERuleset::Compact, Revision)) { return false; }
    for (int32 Guard = 0; Guard < 16; ++Guard)
    {
        if (Run.Progress().Phase == EExpeditionPhase::Recovery && !Run.Recover(true, Error)) { return false; }
        if (Run.Progress().Phase == EExpeditionPhase::Route && (!Run.ChooseRoute(Run.Progress().Offers[0].Id, Error) || !FinishCombat(Run))) { return false; }
        if (Run.Progress().Phase != EExpeditionPhase::Reward) { return false; }
        if (Run.Progress().EncountersCompleted == Victory) { return true; }
        if (!Claim(Run)) { return false; }
    }
    return false;
}

bool Alive(const FExpeditionRun& Run, int32 Id) { const FCombatantState* U = Run.Battle().Find(Id); return U && U->Life == ECombatLife::Conscious; }

/** Hero 4 takes Reserve at reward 1 and Honed at reward 2 (the last slot is the one enemies reach last). */
bool Upgrades(FExpeditionRun& R)
{
    FString E;
    if (R.Progress().EncountersCompleted <= 2 && Alive(R, 4)) { return R.ClaimReward(4, R.Progress().EncountersCompleted == 1 ? ERewardCard::Reserve : ERewardCard::Honed, E); }
    return R.ClaimReward(FirstSurvivor(R), E);
}

FExpeditionSaveData Checkpoint(const FExpeditionRun& Run) { FExpeditionSaveData Data; Data.Current = Run.Snapshot(); return Data; }

/** The card lines the HUD must draw for one card: its before -> after lines, then its note. */
TArray<FString> Expected(const FRewardCardFacts& Card)
{
    TArray<FString> Lines = ProgressionText::CardLines(Card);
    const FString Note = ProgressionText::NoteText(Card.Note);
    if (!Note.IsEmpty()) { Lines.Add(Note); }
    return Lines;
}

bool Resume(FAutomationTestBase& Test, ExpeditionInputHarness::FHarness& Game, const FString& Where)
{
    if (!Test.TestTrue(Where + TEXT(": input, board and HUD are ready"), Game.IsReady())) { return false; }
    Game.Frame();
    Game.Press(EKeys::O);
    Game.Settle();
    return Test.TestTrue(Where + TEXT(": the checkpoint resumes"), Game.View->HasRun() && !Game.View->InCombat());
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRewardCardsShowFactsTest, "Project.Expedition.Presentation.RewardCardsShowProgressionFacts",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRewardCardsShowFactsTest::RunTest(const FString& Parameters)
{
    using namespace ProgressionPresentationTest;
    // Reward 2 after Reserve (Reserve taken), reward 3 after Honed too (Already honed), a revision 9 reward (Gear only in this run), and reward 4 after
    // three Gear claims (Gear at rank 3: potions only). A fallen hero shows Fallen on every card wherever one exists.
    struct FCase { const TCHAR* Name; FExpeditionSnapshot Snapshot; };
    TArray<FCase> Cases;
    auto Add = [&](const TCHAR* Name, int32 Victory, TFunctionRef<bool(FExpeditionRun&)> Claim, int32 Revision, TFunctionRef<bool(const FExpeditionRun&)> Accept)
    {
        const int32 Seed = FirstSeedWhere([&](int32 S) { FExpeditionRun Probe; return ReachReward(Probe, S, Victory, Claim, Revision) && Accept(Probe); }, 42, 300);
        FExpeditionRun Run;
        if (TestTrue(FString(Name) + TEXT(": a seed reaches it"), Seed != INDEX_NONE && ReachReward(Run, Seed, Victory, Claim, Revision))) { Cases.Add({Name, Run.Snapshot()}); }
    };
    Add(TEXT("reserve taken"), 2, Upgrades, FBuildCatalog::Revision, [](const FExpeditionRun& R) { return Alive(R, 4) && R.Battle().Find(4)->bReserve; });
    Add(TEXT("already honed"), 3, Upgrades, FBuildCatalog::Revision, [](const FExpeditionRun& R) { return Alive(R, 4) && R.Battle().Find(4)->bHoned; });
    Add(TEXT("gear only"), 1, [](FExpeditionRun& R) { FString E; return R.ClaimReward(FirstSurvivor(R), E); }, 9, [](const FExpeditionRun&) { return true; });
    Add(TEXT("gear at rank 3"), 4, [](FExpeditionRun& R) { FString E; return R.ClaimReward(4, E); }, FBuildCatalog::Revision, [](const FExpeditionRun& R) { return Alive(R, 4) && R.Battle().Find(4)->WeaponRank == 3; });
    TSet<ERewardCardNote> Notes;
    for (const FCase& Case : Cases)
    {
        FExpeditionRun Run; FString Error;
        Run.Restore(Case.Snapshot, Error);
        const FExpeditionSaveData Data = Checkpoint(Run);
        ExpeditionInputHarness::FHarness Game(*this, TEXT("RewardCards"), &Data);
        if (!Resume(*this, Game, Case.Name)) { continue; }
        const FProgressionFacts Facts = FProgressionQuery::For(Game.View->CurrentRun());
        for (int32 Slot = 0; Slot < 4; ++Slot)
        {
            const FHeroProgressFacts& Hero = Facts.Heroes[Slot];
            Game.Player->SelectPartyIndex(Slot, false);
            Game.Frame(0.0f); Game.Frame(0.0f);
            // A fallen hero cannot be selected for the reward: the controller keeps the selection on a survivor.
            if (Game.Player->Selection() != Hero.Id) { TestTrue(FString(Case.Name) + TEXT(": only the fallen are skipped"), Hero.Life != ECombatLife::Conscious); continue; }
            const AExpeditionHUD::FProgressionDrawn& Drawn = Game.Hud->ProgressionDrawn();
            const FString Where = FString::Printf(TEXT("%s, hero %d"), Case.Name, Slot + 1);
            TestEqual(Where + TEXT(": the title"), Drawn.RewardTitle, ProgressionText::RewardTitle(Facts));
            if (!TestEqual(Where + TEXT(": three cards"), Drawn.CardLines.Num(), 3)) { continue; }
            for (int32 Card = 0; Card < 3; ++Card)
            {
                TestTrue(Where + FString::Printf(TEXT(": card %d text is the query's"), Card), Drawn.CardLines[Card] == Expected(Hero.Cards[Card]));
                TestEqual(Where + FString::Printf(TEXT(": card %d availability"), Card), Drawn.CardAvailable[Card], Hero.Cards[Card].bAvailable);
                Notes.Add(Hero.Cards[Card].Note);
            }
            for (const TPair<FKey, int32>& Key : {TPair<FKey, int32>(EKeys::G, 0), TPair<FKey, int32>(EKeys::R, 1), TPair<FKey, int32>(EKeys::U, 2)})
            {
                Game.Press(Key.Key); Game.Frame(0.0f);
                TestEqual(Where + TEXT(": the Claim button names the card and the hero, or its blocker"), Game.Hud->ProgressionDrawn().Claim, ProgressionText::ClaimLabel(Hero, Hero.Cards[Key.Value]));
                TestEqual(Where + TEXT(": the Claim button is enabled exactly when the claim is legal"), Game.Hud->ProgressionDrawn().bClaimEnabled, Hero.Cards[Key.Value].bAvailable);
            }
            Game.Press(EKeys::G);
        }
        // Fallen heroes: the facts say Fallen on every card.
        for (const FHeroProgressFacts& Hero : Facts.Heroes)
        { if (Hero.Life == ECombatLife::Dead) { for (const FRewardCardFacts& Card : Hero.Cards) { TestTrue(TEXT("Fallen"), !Card.bAvailable && Card.Note == ERewardCardNote::Fallen); Notes.Add(Card.Note); } } }
    }
    for (const ERewardCardNote Note : {ERewardCardNote::ReserveTaken, ERewardCardNote::AlreadyHoned, ERewardCardNote::GearOnlyRun, ERewardCardNote::GearAtCap, ERewardCardNote::None})
    { TestTrue(FString::Printf(TEXT("Note %d was shown"), static_cast<int32>(Note)), Notes.Contains(Note)); }
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCampTextUsesCapTest, "Project.Expedition.Presentation.CampTextUsesEachHerosCap",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FCampTextUsesCapTest::RunTest(const FString& Parameters)
{
    using namespace ProgressionPresentationTest;
    // Reserve at reward 1 to hero 4 (Battle Master: Wind and Maneuver), then the camp: its lines are the query's, at each hero's own cap.
    const int32 Seed = FirstSeedWhere([](int32 S) { FExpeditionRun Probe; return ReachReward(Probe, S, 1, Upgrades) && Alive(Probe, 4); }, 42, 300);
    FExpeditionRun Run; FString Error;
    if (!TestTrue(TEXT("A seed keeps hero 4 standing at reward 1"), Seed != INDEX_NONE && ReachReward(Run, Seed, 1, Upgrades))) { return false; }
    TestTrue(TEXT("Reserve at reward 1"), Upgrades(Run) && Run.Progress().Phase == EExpeditionPhase::Recovery);
    const FExpeditionSaveData Data = Checkpoint(Run);
    ExpeditionInputHarness::FHarness Game(*this, TEXT("CampText"), &Data);
    if (!Resume(*this, Game, TEXT("camp"))) { return false; }
    Game.Frame(0.0f);
    const FProgressionFacts Facts = FProgressionQuery::For(Game.View->CurrentRun());
    TArray<FString> Expected;
    for (const FHeroProgressFacts& Hero : Facts.Heroes) { if (Hero.bCamp) { Expected.Add(ProgressionText::CampLine(Hero)); } }
    TestTrue(TEXT("The camp's lines are the query's"), Game.Hud->ProgressionDrawn().CampLines == Expected);
    TestTrue(TEXT("The Reserve hero recovers to its own cap of 3"), Facts.Heroes[3].bCamp && Facts.Heroes[3].Camp.ContainsByPredicate([](const FPoolChange& P) { return P.CapAfter == 3; }));
    for (const FHeroProgressFacts& Hero : Facts.Heroes)
    { if (Hero.Id != 4) { for (const FPoolChange& P : Hero.Camp) { TestEqual(TEXT("Heroes without Reserve stop at 2"), P.CapAfter, 2); } } }
    TestEqual(TEXT("The camp text says each hero's own cap"), FExpeditionRun::RecoveryDescription(), FString(TEXT("Survivors: +10 HP, +1 Wind/slot/Rage/Kit up to each hero's own cap. No revival or potions.")));
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRewardCardKeysTest, "Project.Expedition.Presentation.RewardCardKeysClaimAndStayGated",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRewardCardKeysTest::RunTest(const FString& Parameters)
{
    using namespace ProgressionPresentationTest;
    const int32 Seed = FirstSeedWhere([](int32 S) { FExpeditionRun Probe; return ReachReward(Probe, S, 1, Upgrades) && Alive(Probe, 4); }, 42, 300);
    FExpeditionRun Run;
    if (!TestTrue(TEXT("A seed keeps hero 4 standing at reward 1"), Seed != INDEX_NONE && ReachReward(Run, Seed, 1, Upgrades))) { return false; }
    const FExpeditionSaveData Data = Checkpoint(Run);
    ExpeditionInputHarness::FHarness Game(*this, TEXT("RewardKeys"), &Data);
    if (!Resume(*this, Game, TEXT("reward"))) { return false; }
    Game.Frame(0.0f);
    TestTrue(TEXT("The Reward phase starts on Gear, with its card focused"), Game.Player->RewardCard() == ERewardCard::Gear && Game.Player->MenuFocus() == FName(TEXT("CardGear")));
    Game.Press(EKeys::R); TestTrue(TEXT("R picks Reserve"), Game.Player->RewardCard() == ERewardCard::Reserve);
    Game.Press(EKeys::Right); TestTrue(TEXT("Right moves to Honed"), Game.Player->RewardCard() == ERewardCard::Honed);
    Game.Press(EKeys::Right); TestTrue(TEXT("and wraps to Gear"), Game.Player->RewardCard() == ERewardCard::Gear);
    Game.Press(EKeys::Left); TestTrue(TEXT("Left goes back to Honed"), Game.Player->RewardCard() == ERewardCard::Honed);
    // An explicit card keeps the focus on later frames, so the outline, Tab and Enter all follow it (#218 review): by letter, by arrow and by click.
    auto Holds = [&](const TCHAR* How, const TCHAR* Card, ERewardCard Expected)
    {
        Game.Frame(0.0f); Game.Frame(0.0f);
        TestTrue(FString(How) + TEXT(": the chosen card keeps the focus on later frames"), Game.Player->MenuFocus() == FName(Card) && Game.Player->RewardCard() == Expected);
    };
    Holds(TEXT("Left"), TEXT("CardHoned"), ERewardCard::Honed);
    Game.Press(EKeys::R); Holds(TEXT("R"), TEXT("CardReserve"), ERewardCard::Reserve);
    Game.Press(EKeys::Tab); Holds(TEXT("Tab from Reserve"), TEXT("CardHoned"), ERewardCard::Honed);
    // A real pointer click on the Reserve card: a HUD button acts at the press and its release dispatches nothing (#221's rules), and claims nothing.
    {
        const double Scale = ExpeditionLayout::ViewScale(Game.Target.Size.X, Game.Target.Size.Y);
        const double W = Game.Target.Size.X / Scale, H = Game.Target.Size.Y / Scale;
        const ExpeditionLayout::Rect Panel = ExpeditionLayout::RewardPanelRect(W, H);
        // The middle of the panel's second card (DrawReward: 16 in, cards 10 apart, from 88 below the top, 150 high).
        const double CardWidth = (Panel.Right - Panel.Left - 32 - 20) / 3;
        Game.Player->SimulatePointer(FVector2D(Panel.Left + 16 + CardWidth + 10 + CardWidth * 0.5, Panel.Top + 88 + 75) * Scale, FIntPoint(0, 0));
        Game.Frame(0.0f);
        const TArray<uint8> BeforeClick = Game.CheckpointBytes();
        Game.Press(EKeys::LeftMouseButton);
        Game.Player->ReleaseSimulatedPointer();
        TestTrue(TEXT("The click claimed nothing"), Game.View->Progress().Phase == EExpeditionPhase::Reward && Game.CheckpointBytes() == BeforeClick);
    }
    Holds(TEXT("A click on Reserve"), TEXT("CardReserve"), ERewardCard::Reserve);
    Game.Press(EKeys::Slash); Game.Frame(0.0f); Game.Press(EKeys::Escape); Holds(TEXT("Help opened and closed"), TEXT("CardReserve"), ERewardCard::Reserve);
    Game.Press(EKeys::G); Game.Frame(0.0f);
    Game.Press(EKeys::Tab); Game.Frame(0.0f);
    TestTrue(TEXT("Tab keeps the screen's focus order: Gear, then Reserve"), Game.Player->MenuFocus() == FName(TEXT("CardReserve")) && Game.Player->RewardCard() == ERewardCard::Reserve);
    // Help and Records own the keys: no card changes under them, and nothing is claimed.
    const TArray<uint8> Bytes = Game.CheckpointBytes();
    for (const FKey& Overlay : {EKeys::Slash, EKeys::H})
    {
        Game.Press(Overlay); Game.Frame(0.0f);
        TestTrue(TEXT("The overlay is open"), Game.Player->OverlayOpen());
        for (const FKey& Key : {EKeys::G, EKeys::U, EKeys::Left, EKeys::L, EKeys::Enter}) { Game.Press(Key); Game.Frame(0.0f); }
        TestTrue(TEXT("No card input leaks under the overlay"), Game.Player->RewardCard() == ERewardCard::Reserve);
        TestTrue(TEXT("Nothing is claimed under it"), Game.View->Progress().Phase == EExpeditionPhase::Reward && Game.CheckpointBytes() == Bytes);
        Game.Press(EKeys::Escape); Game.Frame(0.0f);
    }
    // Return to menu at the reward writes nothing, and Resume starts on Gear again.
    Game.Press(EKeys::BackSpace); Game.Settle();
    TestTrue(TEXT("Return to menu at the reward writes nothing"), Game.CheckpointBytes() == Bytes);
    Game.Press(EKeys::O); Game.Settle(); Game.Frame(0.0f);
    TestTrue(TEXT("Resume returns to the reward on Gear"), Game.View->Progress().Phase == EExpeditionPhase::Reward && Game.Player->RewardCard() == ERewardCard::Gear);
    // Keyboard claim: 4 selects the hero, U picks Honed, Enter claims.
    Game.Press(EKeys::Four); Game.Press(EKeys::U); Game.Frame(0.0f);
    Game.Press(EKeys::Enter); Game.Settle();
    const FExpeditionProgress& P = Game.View->Progress();
    TestTrue(TEXT("Enter claims the chosen card for the chosen hero"), P.History.Num() >= 1 && P.History[0].RewardCard == ERewardCard::Honed && P.History[0].RewardRecipient == 4);
    TestTrue(TEXT("and the claim is committed"), Game.CheckpointBytes() != Bytes && Game.View->Rules().Find(4)->bHoned);
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHeroDetailsInCombatTest, "Project.Expedition.Presentation.HeroDetailsShowProgressionInCombat",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FHeroDetailsInCombatTest::RunTest(const FString& Parameters)
{
    using namespace ProgressionPresentationTest;
    // A Totem Warrior (hero 4) takes Honed at reward 1, then rages and shelters an ally in fight 2: the next fight, resumed from its checkpoint, still shows
    // its basic-damage breakdown with the Rage part and its upgrades beside the protection line, on screen at every size (#218 review).
    const TCHAR* Builds[] = {TEXT("rogue.thief"), TEXT("cleric.life"), TEXT("fighter.champion"), TEXT("barbarian.totem-warrior")};
    TArray<FPartyBuildSelection> Selection;
    for (int32 I = 0; I < 4; ++I) { FPartyBuildSelection Slot; Slot.Id = I + 1; Slot.Name = FString::Printf(TEXT("Hero %d"), I + 1); Slot.BuildId = Builds[I]; Selection.Add(Slot); }
    auto Play = [&](FExpeditionRun& Run, int32 Seed) -> bool
    {
        FString E;
        if (!Run.NewRun(Seed, Selection, E, TEXT("details-in-combat")) || !Run.ChooseRoute(Run.Progress().Offers[0].Id, E) || !FinishCombat(Run) ||
            Run.Progress().Phase != EExpeditionPhase::Reward || !Alive(Run, 4) || !Run.ClaimReward(4, ERewardCard::Honed, E) || !Run.Recover(true, E) ||
            !Run.ChooseRoute(Run.Progress().Offers[0].Id, E)) { return false; }
        for (int32 Steps = 0; Steps < 400 && Run.Progress().Phase == EExpeditionPhase::Combat; ++Steps)
        {
            const int32 Id = Run.Battle().ActiveId();
            if (Run.Battle().Find(Id)->Side == ECombatSide::Enemy) { Run.PlayEnemyTurn(); continue; }
            if (Id == 4)
            {
                if (Run.Battle().LegalTargets(4, ECombatCommandType::Special1).Contains(4)) { Run.Submit({ECombatCommandType::Special1, 4, {}, 4}); }
                const TArray<int32> Allies = Run.Battle().LegalTargets(4, ECombatCommandType::Special2);
                if (!Allies.IsEmpty()) { return Run.Submit({ECombatCommandType::Special2, 4, {}, Allies[0]}).bAccepted; }
            }
            Run.Submit({ECombatCommandType::EndTurn, Id});
        }
        return false;
    };
    const int32 Seed = FirstSeedWhere([&](int32 S) { FExpeditionRun Probe; return Play(Probe, S); }, 42, 300);
    FExpeditionRun Run;
    if (!TestTrue(TEXT("A seed where a honed Totem Warrior rages and shelters in fight 2"), Seed != INDEX_NONE && Play(Run, Seed))) { return false; }
    // The checkpoint is the Totem Warrior's own turn, with Rage and Shelter up.
    const FExpeditionSaveData Data = Checkpoint(Run);
    ExpeditionInputHarness::FHarness Game(*this, TEXT("DetailsInCombat"), &Data);
    if (!TestTrue(TEXT("Input, board and HUD are ready"), Game.IsReady())) { return false; }
    Game.Frame();
    Game.Press(EKeys::O);
    Game.Settle();
    if (!TestTrue(TEXT("The fight resumes"), Game.View->HasRun() && Game.View->InCombat())) { return false; }
    const FCombatantState& Totem = *Game.View->Rules().Find(4);
    TestTrue(TEXT("The Totem Warrior is honed, raging and guarding"), Totem.bHoned && Totem.RageTurns > 0 && Totem.GuardTargetId != INDEX_NONE);
    FHeroProgressFacts Expected;
    for (const FHeroProgressFacts& Hero : FProgressionQuery::For(Game.View->CurrentRun()).Heroes) { if (Hero.Id == 4) { Expected = Hero; } }
    for (const FIntPoint Size : {FIntPoint(1920, 1080), FIntPoint(1280, 720), FIntPoint(1600, 600)})
    {
        Game.Target.Size = Size;
        const double Scale = ExpeditionLayout::ViewScale(Size.X, Size.Y);
        Game.Player->SimulatePointer(FVector2D(108.0, ExpeditionLayout::PartyCardTop + 3 * ExpeditionLayout::PartyCardPitch + 32.0) * Scale, FIntPoint(0, 0));
        Game.Frame(0.0f); Game.Frame(0.0f);
        const FString Where = FString::Printf(TEXT("%dx%d"), Size.X, Size.Y);
        const TArray<FString>& Lines = Game.Hud->ProgressionDrawn().DetailLines;
        TestTrue(Where + TEXT(": the breakdown shows the Rage part in combat"), Lines.Contains(ProgressionText::DamageBreakdown(Expected)) && Expected.Damage.Rage > 0);
        TestTrue(Where + TEXT(": the upgrades show in combat"), Lines.Contains(TEXT("Upgrades: Honed")));
        TestTrue(Where + TEXT(": no level marker during a fight"), Game.Hud->ProgressionDrawn().PartyMarkers.IsEmpty());
        for (const FBox2D& Panel : Game.Hud->PartyDetailRects())
        { TestTrue(Where + TEXT(": the details, with the protection line, stay on screen"), Panel.Min.X >= 0 && Panel.Min.Y >= 0 && Panel.Max.X <= Size.X && Panel.Max.Y <= Size.Y); }
        TestFalse(Where + TEXT(": a details panel is drawn"), Game.Hud->PartyDetailRects().IsEmpty());
    }
    Game.Player->ReleaseSimulatedPointer();
    return !HasAnyErrors();
}
#endif
