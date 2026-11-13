#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Combat/ActionFacts.h"
#include "Party/BuildCatalog.h"
#include "Party/PartyContinuity.h"
#include "Persistence/ExpeditionSave.h"
#include "Run/ExpeditionRun.h"
#include "Run/ProgressionQuery.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/FileHelper.h"
#include "ExpeditionTestSupport.h"

/**
 * Slice 5 (#218, docs/runs/gameplay-depth/13-progression-contract.md section 7): reward cards, the honed and Reserve seams, progression
 * facts, catalog revision 10 and save schema 6, through public seams only.
 */
namespace ProgressionTest
{
using namespace ExpeditionTestSupport;
using EC = ECombatCommandType;

TArray<FPartyBuildSelection> Party(const TArray<const TCHAR*>& Builds)
{
    TArray<FPartyBuildSelection> Result;
    for (int32 I = 0; I < Builds.Num(); ++I)
    {
        FPartyBuildSelection Slot; Slot.Id = I + 1; Slot.Name = FString::Printf(TEXT("Hero %d"), I + 1); Slot.BuildId = Builds[I];
        Result.Add(Slot);
    }
    return Result;
}

/** Kit, Kit + Slot, Rage, and Wind + Maneuver. */
TArray<FPartyBuildSelection> PoolParty() { return Party({TEXT("rogue.thief"), TEXT("cleric.life"), TEXT("barbarian.berserker"), TEXT("fighter.battle-master")}); }

int32 FirstSurvivor(const FExpeditionRun& Run)
{
    for (const FCombatantState& U : Run.Battle().Snapshot().Encounter.Combatants) { if (U.Side == ECombatSide::Party && U.Life == ECombatLife::Conscious) { return U.Id; } }
    return INDEX_NONE;
}

bool GearToFirst(FExpeditionRun& Run) { FString Error; return Run.ClaimReward(FirstSurvivor(Run), Error); }

bool Conscious(const FExpeditionRun& Run, int32 Id) { const FCombatantState* U = Run.Battle().Find(Id); return U && U->Life == ECombatLife::Conscious; }

/** NewRun, then the scripted player (first offer, FinishCombat, Claim for each earlier reward, the camp) to the Reward phase of victory Victory. */
bool ReachReward(FExpeditionRun& Run, int32 Seed, const TArray<FPartyBuildSelection>& Selection, int32 Victory, TFunctionRef<bool(FExpeditionRun&)> Claim,
    int32 Revision = FBuildCatalog::Revision, ERuleset Ruleset = ERuleset::Compact)
{
    FString Error;
    if (!Run.NewRun(Seed, Selection, Error, FString::Printf(TEXT("progression-%d"), Seed), Ruleset, Revision)) { return false; }
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

/** A simple player that reads only the battle: attack the first legal target, else step toward the nearest foe, else end the turn. It never
 * reads the revision or any preview text, so two runs that differ only in those take the same commands. */
FCombatCommand PartyCommand(const FCombatBattle& B, int32 Id)
{
    const FCombatSnapshot& S = B.Snapshot();
    const FCombatantState* Me = B.Find(Id);
    if (S.bActionAvailable)
    {
        const TArray<int32> Targets = B.LegalTargets(Id);
        if (!Targets.IsEmpty()) { return {EC::Attack, Id, {}, Targets[0]}; }
        auto Nearest = [&S](FIntPoint Cell)
        {
            int32 Best = 1000;
            for (const FCombatantState& U : S.Encounter.Combatants) { if (U.Side == ECombatSide::Enemy && U.Life == ECombatLife::Conscious) { Best = FMath::Min(Best, Distance(Cell, U.Cell)); } }
            return Best;
        };
        TArray<FIntPoint> Moves = B.LegalMoves(Id);
        Moves.Sort([&Nearest](FIntPoint A, FIntPoint C) { const int32 DA = Nearest(A), DC = Nearest(C); return DA == DC ? (A.Y == C.Y ? A.X < C.X : A.Y < C.Y) : DA < DC; });
        if (Me && !Moves.IsEmpty() && Nearest(Moves[0]) < Nearest(Me->Cell)) { return {EC::Move, Id, Moves[0]}; }
    }
    return {EC::EndTurn, Id};
}

/** One step of that player for the whole run. Reward claims come from Claim. */
bool Step(FExpeditionRun& Run, TFunctionRef<bool(FExpeditionRun&)> Claim, FCombatCommand* OutCommand = nullptr, FCombatBattle* OutBefore = nullptr)
{
    FString Error;
    switch (Run.Progress().Phase)
    {
    case EExpeditionPhase::Route: return Run.ChooseRoute(Run.Progress().Offers[0].Id, Error);
    case EExpeditionPhase::Reward: return Claim(Run);
    case EExpeditionPhase::Recovery: return Run.Recover(true, Error);
    case EExpeditionPhase::Combat:
    {
        const FCombatBattle& B = Run.Battle();
        const FCombatantState* Active = B.Find(B.ActiveId());
        if (!Active) { return false; }
        if (Active->Side == ECombatSide::Enemy) { return !Run.PlayEnemyTurn().IsEmpty(); }
        const FCombatCommand Command = PartyCommand(B, Active->Id);
        if (OutCommand) { *OutCommand = Command; }
        if (OutBefore) { *OutBefore = B; }
        return Run.Submit(Command).bAccepted;
    }
    default: return false;
    }
}

bool Ended(const FExpeditionRun& Run) { return Run.Progress().Phase == EExpeditionPhase::Victory || Run.Progress().Phase == EExpeditionPhase::Defeat; }

/** Every preview string of the run's offers and history. */
TArray<FString*> Previews(FExpeditionSnapshot& S)
{
    TArray<FString*> Result;
    for (FExpeditionRoute& R : S.Progress.Offers) { Result.Add(&R.Preview); }
    for (FExpeditionEncounterRecord& H : S.Progress.History) { Result.Add(&H.Route.Preview); }
    return Result;
}

FString Encoded(FAutomationTestBase& Test, const FExpeditionRun& Run)
{
    FExpeditionSaveData Data; Data.Current = Run.Snapshot();
    if (Ended(Run)) { Data.Records.Add(FExpeditionSave::Ending(Run)); }
    FString Json, Error;
    Test.TestTrue(TEXT("Encode: ") + Error, FExpeditionSave::Encode(Data, Json, Error));
    return Json;
}

TSharedPtr<FJsonObject> Parse(const FString& Json)
{
    TSharedPtr<FJsonObject> Object;
    FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Object);
    return Object;
}

FString Serialize(const TSharedPtr<FJsonObject>& Object)
{
    FString Json;
    FJsonSerializer::Serialize(Object.ToSharedRef(), TJsonWriterFactory<>::Create(&Json));
    return Json;
}

/** The pools a build has, as (held, cap) of a hero. */
TArray<TPair<int32, int32>> PoolValues(const FCombatantState& U)
{
    const FResourceCaps C = FBuildCatalog::Caps(U);
    TArray<TPair<int32, int32>> Result;
    if (C.Kit) { Result.Add({U.KitUses, C.Kit}); }
    if (C.Wind) { Result.Add({U.SecondWindUses, C.Wind}); }
    if (C.Slot) { Result.Add({U.SpellSlots, C.Slot}); }
    if (C.Maneuver) { Result.Add({U.ManeuverUses, C.Maneuver}); }
    if (C.Rage) { Result.Add({U.RageUses, C.Rage}); }
    return Result;
}

void SetPools(FCombatantState& U, int32 Kit, int32 Slot, int32 Wind, int32 Maneuver, int32 Rage)
{
    const FResourceCaps C = FBuildCatalog::Caps(U);
    if (C.Kit) { U.KitUses = Kit; }
    if (C.Slot) { U.SpellSlots = Slot; }
    if (C.Wind) { U.SecondWindUses = Wind; }
    if (C.Maneuver) { U.ManeuverUses = Maneuver; }
    if (C.Rage) { U.RageUses = Rage; }
}

FCombatantState* Hero(FExpeditionSnapshot& S, int32 Id)
{ return S.Battle.Encounter.Combatants.FindByPredicate([Id](const FCombatantState& U) { return U.Id == Id; }); }
}

// ---------------------------------------------------------------------------------------------------------------------------------- Run

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRewardCardsApplyOnceTest, "Project.Expedition.Run.RewardCardsApplyOnce",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRewardCardsApplyOnceTest::RunTest(const FString& Parameters)
{
    using namespace ProgressionTest;
    FString Error;
    const TArray<FPartyBuildSelection> Selection = Party({TEXT("fighter.battle-master"), TEXT("cleric.life"), TEXT("barbarian.totem-warrior"), TEXT("ranger.hunter")});
    // Hero 4 (the ranger; enemies attack the first legal target in roster order, so the last slot lasts longest) takes Reserve at reward 1
    // and Honed at reward 2 and is still standing at reward 3.
    auto Plan = [](FExpeditionRun& R) { FString E; return R.ClaimReward(4, R.Progress().EncountersCompleted == 1 ? ERewardCard::Reserve : ERewardCard::Honed, E); };
    const int32 Seed = FirstSeedWhere([&](int32 S) { FExpeditionRun First, Probe; return ReachReward(First, S, Selection, 1, GearToFirst) && Conscious(First, 4) &&
        ReachReward(Probe, S, Selection, 3, Plan) && Conscious(Probe, 4); }, 42, 400);
    if (!TestTrue(TEXT("A seed keeps hero 4 standing to reward 3"), Seed != INDEX_NONE)) { return false; }
    FExpeditionRun Run;
    if (!TestTrue(TEXT("Reach reward 1"), ReachReward(Run, Seed, Selection, 1, GearToFirst))) { return false; }
    TestEqual(TEXT("A revision 10 Compact run"), Run.Progress().CatalogRevision, 10);

    // Reserve: +1 cap and +1 use on each pool its specials spend, never a refill; the potions as always; nothing else changes.
    const FCombatantState Before = *Run.Battle().Find(4);
    const int32 Potions = Run.Progress().History.Last().Route.RewardPotions;
    if (!TestTrue(TEXT("Claim Reserve: ") + Error, Run.ClaimReward(4, ERewardCard::Reserve, Error))) { return false; }
    const FCombatantState Reserved = *Run.Battle().Find(4);
    TestTrue(TEXT("Reserve recorded on the hero"), Reserved.bReserve && !Reserved.bHoned);
    TestTrue(TEXT("Reserve recorded in history"), Run.Progress().History[0].RewardCard == ERewardCard::Reserve && Run.Progress().History[0].RewardRecipient == 4);
    TestEqual(TEXT("Kit cap 2 -> 3"), FBuildCatalog::Caps(Reserved).Kit, 3);
    TestEqual(TEXT("Kit use +1"), Reserved.KitUses, Before.KitUses + 1);
    TestEqual(TEXT("No other pool"), FBuildCatalog::Caps(Reserved).Slot + FBuildCatalog::Caps(Reserved).Wind + FBuildCatalog::Caps(Reserved).Rage, 0);
    TestEqual(TEXT("Gear unchanged"), Reserved.WeaponRank, Before.WeaponRank);
    TestEqual(TEXT("The route's potions"), Reserved.Potions, FMath::Min(9, Before.Potions + Potions));
    TestTrue(TEXT("No HP or profile change"), Reserved.HP == Before.HP && Reserved.MaxHP == Before.MaxHP && Reserved.DamageBonus == Before.DamageBonus && Reserved.Level == Before.Level);

    // Reward 2: Reserve is once per hero; Honed hones both specials.
    TestTrue(TEXT("Camp"), Run.Recover(true, Error));
    TestTrue(TEXT("Fight 2"), Run.ChooseRoute(Run.Progress().Offers[0].Id, Error) && FinishCombat(Run) && Run.Progress().Phase == EExpeditionPhase::Reward && Conscious(Run, 4));
    FExpeditionSnapshot Snapshot = Run.Snapshot();
    TestFalse(TEXT("Second Reserve refused"), Run.ClaimReward(4, ERewardCard::Reserve, Error));
    TestEqual(TEXT("named"), Error, FString(TEXT("Reserve taken.")));
    TestTrue(TEXT("Refusal changes nothing"), SameRun(Snapshot, Run.Snapshot()));
    const TArray<FAbilityFacts> Plain = FBuildCatalog::Abilities(*Run.Battle().Find(4));
    if (!TestTrue(TEXT("Claim Honed: ") + Error, Run.ClaimReward(4, ERewardCard::Honed, Error))) { return false; }
    const TArray<FAbilityFacts> Sharp = FBuildCatalog::Abilities(*Run.Battle().Find(4));
    TestTrue(TEXT("Honed recorded"), Run.Battle().Find(4)->bHoned && Run.Progress().History[1].RewardCard == ERewardCard::Honed);
    if (TestEqual(TEXT("Two specials"), Sharp.Num(), 2))
    {
        TestEqual(TEXT("Volley (Area): the honed row"), Sharp[0].Amount, Plain[0].Amount + UpgradeRules::HonedArea);
        TestEqual(TEXT("Hunter's Finish (Strike): the honed row"), Sharp[1].Amount, Plain[1].Amount + UpgradeRules::HonedStrike);
    }
    // Reward 3: Honed is once per hero; Gear still adds a rank.
    TestTrue(TEXT("Fight 3"), Run.ChooseRoute(Run.Progress().Offers[0].Id, Error) && FinishCombat(Run) && Run.Progress().Phase == EExpeditionPhase::Reward && Conscious(Run, 4));
    Snapshot = Run.Snapshot();
    TestFalse(TEXT("Second Honed refused"), Run.ClaimReward(4, ERewardCard::Honed, Error));
    TestEqual(TEXT("named"), Error, FString(TEXT("Already honed.")));
    TestTrue(TEXT("Refusal changes nothing"), SameRun(Snapshot, Run.Snapshot()));
    const int32 Rank = Run.Battle().Find(4)->WeaponRank;
    TestTrue(TEXT("Claim Gear"), Run.ClaimReward(4, ERewardCard::Gear, Error));
    TestEqual(TEXT("Gear +1"), Run.Battle().Find(4)->WeaponRank, Rank + 1);
    TestTrue(TEXT("Gear recorded"), Run.Progress().History[2].RewardCard == ERewardCard::Gear);

    // Gear at rank 3 stays legal and gives the potions only.
    const int32 GearSeed = FirstSeedWhere([&](int32 S) { FExpeditionRun Probe; return ReachReward(Probe, S, Selection, 4, [](FExpeditionRun& R) { FString E; return R.ClaimReward(4, E); }) && Conscious(Probe, 4); }, 42, 400);
    if (!TestTrue(TEXT("A seed gives hero 4 three Gear claims and reward 4"), GearSeed != INDEX_NONE)) { return false; }
    FExpeditionRun Capped;
    ReachReward(Capped, GearSeed, Selection, 4, [](FExpeditionRun& R) { FString E; return R.ClaimReward(4, E); });
    const FCombatantState AtCap = *Capped.Battle().Find(4);
    TestEqual(TEXT("Rank 3"), AtCap.WeaponRank, 3);
    const FProgressionFacts Facts = FProgressionQuery::For(Capped);
    TestTrue(TEXT("The query says potions only"), Facts.Heroes[3].Cards[0].bAvailable && Facts.Heroes[3].Cards[0].Note == ERewardCardNote::GearAtCap && Facts.Heroes[3].Cards[0].GearAfter == 3);
    TestTrue(TEXT("Gear at rank 3 is accepted"), Capped.ClaimReward(4, ERewardCard::Gear, Error));
    TestEqual(TEXT("still rank 3"), Capped.Battle().Find(4)->WeaponRank, 3);
    TestEqual(TEXT("potions only"), Capped.Battle().Find(4)->Potions, FMath::Min(9, AtCap.Potions + Capped.Progress().History[3].Route.RewardPotions));
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRewardCardRejectionsTest, "Project.Expedition.Run.RewardCardRejectionsLeaveStateUnchanged",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRewardCardRejectionsTest::RunTest(const FString& Parameters)
{
    using namespace ProgressionTest;
    const TArray<FPartyBuildSelection> Selection = PoolParty();
    auto Refused = [this](FExpeditionRun& Run, int32 Id, ERewardCard Card, const FString& Expected, const FString& What)
    {
        const FExpeditionSnapshot Before = Run.Snapshot();
        FString Error;
        TestFalse(What + TEXT(" is refused"), Run.ClaimReward(Id, Card, Error));
        TestTrue(What + TEXT(": named (") + Error + TEXT(")"), Error.StartsWith(Expected));
        TestTrue(What + TEXT(": the full snapshot, both random streams included, is unchanged"), SameRun(Before, Run.Snapshot()));
    };
    // Hero 1 takes Reserve at reward 1 and Honed at reward 2 and stands at reward 3.
    auto Plan = [](FExpeditionRun& R) { FString E; return R.ClaimReward(1, R.Progress().EncountersCompleted == 1 ? ERewardCard::Reserve : ERewardCard::Honed, E); };
    const int32 Seed = FirstSeedWhere([&](int32 S) { FExpeditionRun Probe; return ReachReward(Probe, S, Selection, 3, Plan) && Conscious(Probe, 1); }, 42, 200);
    if (!TestTrue(TEXT("A seed reaches reward 3 with hero 1"), Seed != INDEX_NONE)) { return false; }
    FExpeditionRun Run;
    ReachReward(Run, Seed, Selection, 3, Plan);
    Refused(Run, 1, ERewardCard::Reserve, TEXT("Reserve taken."), TEXT("A second Reserve"));
    Refused(Run, 1, ERewardCard::Honed, TEXT("Already honed."), TEXT("A second Honed"));
    Refused(Run, 1, static_cast<ERewardCard>(3), TEXT("Choose Gear, Reserve or Honed."), TEXT("A card out of range"));
    for (const int32 Id : {0, 5, 20, static_cast<int32>(INDEX_NONE)}) { Refused(Run, Id, ERewardCard::Gear, TEXT("Select a conscious survivor"), FString::Printf(TEXT("Recipient %d"), Id)); }
    for (const FCombatantState& U : Run.Battle().Snapshot().Encounter.Combatants)
    { if (U.Side == ECombatSide::Party && U.Life != ECombatLife::Conscious) { Refused(Run, U.Id, ERewardCard::Reserve, TEXT("Select a conscious survivor"), TEXT("A fallen hero")); } }
    FString Error;
    TestTrue(TEXT("Gear to hero 1"), Run.ClaimReward(1, ERewardCard::Gear, Error));
    Refused(Run, 1, ERewardCard::Gear, TEXT("Loot can be claimed once"), TEXT("A claim outside the Reward phase"));
    // Gear only in this run: SRD, and revisions 7-9.
    for (const int32 Revision : {7, 8, 9})
    {
        FExpeditionRun Old;
        const int32 OldSeed = FirstSeedWhere([&](int32 S) { FExpeditionRun Probe; return ReachReward(Probe, S, Selection, 1, GearToFirst, Revision); });
        if (!TestTrue(TEXT("An older-revision reward"), OldSeed != INDEX_NONE && ReachReward(Old, OldSeed, Selection, 1, GearToFirst, Revision))) { continue; }
        for (const ERewardCard Card : {ERewardCard::Reserve, ERewardCard::Honed})
        { Refused(Old, FirstSurvivor(Old), Card, TEXT("Gear only in this run."), FString::Printf(TEXT("Revision %d %s"), Revision, Card == ERewardCard::Reserve ? TEXT("Reserve") : TEXT("Honed"))); }
    }
    FExpeditionRun Srd;
    const int32 SrdSeed = FirstSeedWhere([&](int32 S) { FExpeditionRun Probe; return ReachReward(Probe, S, Selection, 1, GearToFirst, FBuildCatalog::Revision, ERuleset::SRD); });
    if (TestTrue(TEXT("An SRD reward"), SrdSeed != INDEX_NONE && ReachReward(Srd, SrdSeed, Selection, 1, GearToFirst, FBuildCatalog::Revision, ERuleset::SRD)))
    {
        Refused(Srd, FirstSurvivor(Srd), ERewardCard::Reserve, TEXT("Gear only in this run."), TEXT("SRD Reserve"));
        Refused(Srd, FirstSurvivor(Srd), ERewardCard::Honed, TEXT("Gear only in this run."), TEXT("SRD Honed"));
    }
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGearOnlyRevision10Test, "Project.Expedition.Run.GearOnlyRevision10MatchesRevision9",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FGearOnlyRevision10Test::RunTest(const FString& Parameters)
{
    using namespace ProgressionTest;
    // A revision 10 run claiming Gear at every reward against a revision 9 run of the same seed and party, command for command. Every snapshot
    // field is equal except the revision and the Preview strings, and those differ only in the reward phrase.
    int32 Compared = 0, Changed = 0;
    for (const TArray<FPartyBuildSelection>& Selection : {Party({TEXT("fighter.champion"), TEXT("rogue.thief"), TEXT("cleric.life"), TEXT("wizard.evocation")}), PoolParty()})
    {
        for (const int32 Seed : {42, 20261109, 7})
        {
            FExpeditionRun New, Old; FString Error;
            TestTrue(TEXT("Revision 10"), New.NewRun(Seed, Selection, Error, TEXT("lockstep"), ERuleset::Compact, 10));
            TestTrue(TEXT("Revision 9"), Old.NewRun(Seed, Selection, Error, TEXT("lockstep"), ERuleset::Compact, 9));
            auto Same = [&](const TCHAR* When)
            {
                FExpeditionSnapshot A = New.Snapshot(), B = Old.Snapshot();
                ++Compared;
                TestEqual(FString(When) + TEXT(": revisions"), A.Progress.CatalogRevision, 10);
                A.Progress.CatalogRevision = B.Progress.CatalogRevision;
                const TArray<FString*> PA = Previews(A), PB = Previews(B);
                if (!TestEqual(FString(When) + TEXT(": as many previews"), PA.Num(), PB.Num())) { return false; }
                for (int32 I = 0; I < PA.Num(); ++I)
                {
                    const FString Expected = PB[I]->Replace(TEXT("| +1 gear, +"), TEXT("| 1 upgrade choice, +"));
                    Changed += Expected != *PB[I] ? 1 : 0;
                    TestEqual(FString(When) + TEXT(": only the reward phrase differs"), *PA[I], Expected);
                    *PA[I] = *PB[I];
                }
                return TestTrue(FString(When) + TEXT(": every other field, both random streams included"), SameRun(A, B));
            };
            if (!Same(TEXT("new run"))) { continue; }
            auto Gear = [](FExpeditionRun& R) { FString E; return R.ClaimReward(FirstSurvivor(R), ERewardCard::Gear, E); };
            for (int32 Steps = 0; Steps < 20000 && !Ended(Old); ++Steps)
            {
                const bool bOld = Step(Old, Gear), bNew = Step(New, Gear);
                if (!TestTrue(TEXT("Both runs accept the same step"), bOld && bNew) || !Same(TEXT("after a step"))) { break; }
            }
            TestTrue(TEXT("The lockstep run ended"), Ended(Old) && Ended(New));
        }
    }
    TestTrue(TEXT("Many states were compared"), Compared > 300);
    TestTrue(TEXT("The reward phrase was seen"), Changed > 0);
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRewardPreviewsFollowRevisionTest, "Project.Expedition.Run.RewardPreviewsFollowRevision",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRewardPreviewsFollowRevisionTest::RunTest(const FString& Parameters)
{
    using namespace ProgressionTest;
    FString Error;
    const TArray<FPartyBuildSelection> Selection = PoolParty();
    // Revision 10 Compact: every route of fights 1-4 offers "1 upgrade choice, +N potion(s)", secured supplies kept; the finale is unchanged.
    for (const int32 Pattern : {0, 31, 0b01010})
    {
        const int32 Seed = FirstSeedWhere([&](int32 S)
        {
            FExpeditionRun Probe; Probe.NewRun(S, Selection, Error, TEXT("previews"));
            for (int32 Stage = 0; Stage < 4; ++Stage)
            {
                if (!Probe.ChooseRoute(Probe.Progress().Offers[(Pattern >> Stage) & 1].Id, Error) || !FinishCombat(Probe) || Probe.Progress().Phase != EExpeditionPhase::Reward || !GearToFirst(Probe)) { return false; }
                if (Probe.Progress().Phase == EExpeditionPhase::Recovery) { Probe.Recover(true, Error); }
            }
            return Probe.Progress().Phase == EExpeditionPhase::Route;
        }, 42, 200);
        if (!TestTrue(FString::Printf(TEXT("A seed reaches the finale on pattern %d"), Pattern), Seed != INDEX_NONE)) { continue; }
        FExpeditionRun Run; Run.NewRun(Seed, Selection, Error, TEXT("previews"));
        for (int32 Stage = 0; Stage < 5; ++Stage)
        {
            for (const FExpeditionRoute& Route : Run.Progress().Offers)
            {
                const FString Where = FString::Printf(TEXT("pattern %d fight %d %s: %s"), Pattern, Stage + 1, Route.bDangerous ? TEXT("Dangerous") : TEXT("Guarded"), *Route.Preview);
                if (Stage == 4) { TestTrue(Where, Route.Preview.EndsWith(TEXT("| Recover the artifact; no loot"))); continue; }
                const bool bSupplies = !Run.Progress().History.IsEmpty() && Run.Progress().History.Last().Route.bDangerous && !Route.bDangerous;
                TestTrue(Where, Route.Preview.EndsWith(FString::Printf(TEXT("| 1 upgrade choice, +%d potion%s%s"), Route.RewardPotions, Route.RewardPotions == 1 ? TEXT("") : TEXT("s"), bSupplies ? TEXT(" (secured supplies)") : TEXT(""))));
                TestFalse(Where + TEXT(": no gear promise"), Route.Preview.Contains(TEXT("+1 gear")));
            }
            if (Stage == 4) { break; }
            Run.ChooseRoute(Run.Progress().Offers[(Pattern >> Stage) & 1].Id, Error); FinishCombat(Run); GearToFirst(Run);
            if (Run.Progress().Phase == EExpeditionPhase::Recovery) { Run.Recover(true, Error); }
        }
    }
    // Revisions 7-9 and SRD keep the wording of the base build: their real schema 5 checkpoints (written by staging after #214) restore,
    // and restore rebuilds every saved route with the run's own wording and compares the whole struct.
    const FString Fixtures = FPaths::ProjectDir() / TEXT("Source/DnDDiceRoll/Tests/Fixtures/Schema5");
    for (const TCHAR* Name : {TEXT("route-r8-v5.json"), TEXT("route-r9-v5.json"), TEXT("reward-r9-v5.json")})
    {
        FExpeditionSaveData Data; FExpeditionRun Restored;
        TestTrue(FString(Name) + TEXT(" restores with its own wording: ") + Error, FExpeditionSave::Read(Fixtures / Name, Data, Error) && Restored.Restore(Data.Current, Error));
        for (const FExpeditionRoute& Route : Data.Current.Progress.Offers) { TestFalse(FString(Name) + TEXT(": no upgrade wording"), Route.Preview.Contains(TEXT("upgrade"))); }
    }
    // Restore accepts each run's own wording and rejects the other revision's.
    auto Swapped = [&](int32 Revision, ERuleset Ruleset, const TCHAR* From, const TCHAR* To)
    {
        FExpeditionRun Run;
        TestTrue(TEXT("New run"), Run.NewRun(42, Selection, Error, TEXT("wording"), Ruleset, Revision));
        FExpeditionSnapshot S = Run.Snapshot();
        FExpeditionRun Probe;
        TestTrue(FString::Printf(TEXT("Revision %d (%s) accepts its own wording"), Revision, Ruleset == ERuleset::SRD ? TEXT("SRD") : TEXT("Compact")), Probe.Restore(S, Error));
        bool bAny = false;
        for (FExpeditionRoute& Route : S.Progress.Offers) { const FString Was = Route.Preview; Route.Preview.ReplaceInline(From, To); bAny |= Was != Route.Preview; }
        if (!bAny) { for (FExpeditionRoute& Route : S.Progress.Offers) { Route.Preview += FString(TEXT(" | ")) + To; } }
        TestFalse(FString::Printf(TEXT("Revision %d (%s) rejects \"%s\""), Revision, Ruleset == ERuleset::SRD ? TEXT("SRD") : TEXT("Compact"), To), Probe.Restore(S, Error));
    };
    Swapped(10, ERuleset::Compact, TEXT("1 upgrade choice, +"), TEXT("+1 gear, +"));
    Swapped(9, ERuleset::Compact, TEXT("+1 gear, +"), TEXT("1 upgrade choice, +"));
    Swapped(10, ERuleset::SRD, TEXT("+1 gear (rank 3 cap)"), TEXT("1 upgrade choice"));
    // SRD and revisions 7-8 keep today's (pre-identity) wording.
    for (const TPair<int32, ERuleset>& Kind : {TPair<int32, ERuleset>(7, ERuleset::Compact), TPair<int32, ERuleset>(8, ERuleset::Compact), TPair<int32, ERuleset>(10, ERuleset::SRD)})
    {
        FExpeditionRun Run; Run.NewRun(42, Selection, Error, TEXT("wording"), Kind.Value, Kind.Key);
        for (const FExpeditionRoute& Route : Run.Progress().Offers) { TestTrue(TEXT("Pre-identity gear wording: ") + Route.Preview, Route.Preview.Contains(TEXT("| +1 gear (rank 3 cap) | +"))); }
    }
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOlderRevisionsGearOnlyTest, "Project.Expedition.Run.OlderRevisionsAndSrdRewardGearOnly",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FOlderRevisionsGearOnlyTest::RunTest(const FString& Parameters)
{
    using namespace ProgressionTest;
    const TArray<FPartyBuildSelection> Selection = PoolParty();
    for (const TPair<int32, ERuleset>& Kind : {TPair<int32, ERuleset>(7, ERuleset::Compact), TPair<int32, ERuleset>(8, ERuleset::Compact), TPair<int32, ERuleset>(9, ERuleset::Compact),
        TPair<int32, ERuleset>(10, ERuleset::SRD)})
    {
        const FString Where = FString::Printf(TEXT("revision %d %s"), Kind.Key, Kind.Value == ERuleset::SRD ? TEXT("SRD") : TEXT("Compact"));
        TestFalse(Where + TEXT(" offers no upgrade cards"), FExpeditionRun::OffersUpgradeCards(Kind.Value, Kind.Key));
        const int32 Seed = FirstSeedWhere([&](int32 S) { FExpeditionRun Probe; return ReachReward(Probe, S, Selection, 2, GearToFirst, Kind.Key, Kind.Value); }, 42, 200);
        FExpeditionRun Run;
        if (!TestTrue(Where + TEXT(": reward 2"), Seed != INDEX_NONE && ReachReward(Run, Seed, Selection, 2, GearToFirst, Kind.Key, Kind.Value))) { continue; }
        const FProgressionFacts Facts = FProgressionQuery::For(Run);
        TestFalse(Where + TEXT(": the query offers Gear only"), Facts.bUpgradeCards);
        for (const FHeroProgressFacts& H : Facts.Heroes)
        {
            if (H.Life != ECombatLife::Conscious) { continue; }
            TestTrue(Where + TEXT(": Gear available"), H.Cards[0].bAvailable);
            TestTrue(Where + TEXT(": Reserve and Honed are gear-only"), !H.Cards[1].bAvailable && !H.Cards[2].bAvailable &&
                H.Cards[1].Note == ERewardCardNote::GearOnlyRun && H.Cards[2].Note == ERewardCardNote::GearOnlyRun);
        }
        FString Error;
        TestTrue(Where + TEXT(": Gear accepted"), Run.ClaimReward(FirstSurvivor(Run), ERewardCard::Gear, Error));
        for (const FExpeditionEncounterRecord& H : Run.Progress().History) { TestTrue(Where + TEXT(": every record holds Gear"), H.RewardCard == ERewardCard::Gear); }
        for (const FCombatantState& U : Run.Battle().Snapshot().Encounter.Combatants) { TestTrue(Where + TEXT(": no upgrades"), !U.bReserve && !U.bHoned); }
    }
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReserveBeforeCampTest, "Project.Expedition.Run.ReserveBeforeCampRecoversToTheNewCap",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FReserveBeforeCampTest::RunTest(const FString& Parameters)
{
    using namespace ProgressionTest;
    FString Error;
    const TArray<FPartyBuildSelection> Selection = PoolParty();
    TestEqual(TEXT("The camp says each hero's own cap"), FExpeditionRun::RecoveryDescription(), FString(TEXT("Survivors: +10 HP, +1 Wind/slot/Rage/Kit up to each hero's own cap. No revival or potions.")));
    const int32 Seed = FirstSeedWhere([&](int32 S)
    {
        FExpeditionRun Probe;
        if (!ReachReward(Probe, S, Selection, 1, GearToFirst)) { return false; }
        for (int32 Id = 1; Id <= 4; ++Id) { if (!Conscious(Probe, Id)) { return false; } }
        return true;
    }, 42, 200);
    if (!TestTrue(TEXT("A seed keeps all four standing at reward 1"), Seed != INDEX_NONE)) { return false; }
    FExpeditionRun Base;
    ReachReward(Base, Seed, Selection, 1, GearToFirst);
    // Each build is its own run: the Reserve recipient spent every pool; the others hold full pools.
    for (int32 Recipient = 1; Recipient <= 4; ++Recipient)
    {
        FExpeditionSnapshot S = Base.Snapshot();
        for (int32 Id = 1; Id <= 4; ++Id)
        {
            FCombatantState& U = *Hero(S, Id);
            const FResourceCaps C = FBuildCatalog::Caps(U);
            if (Id == Recipient) { SetPools(U, 0, 0, 0, 0, 0); } else { SetPools(U, C.Kit, C.Slot, C.Wind, C.Maneuver, C.Rage); }
        }
        FExpeditionRun Run;
        if (!TestTrue(TEXT("Spent pools restore"), Run.Restore(S, Error))) { continue; }
        const FString Build = Run.Battle().Find(Recipient)->BuildId;
        if (!TestTrue(Build + TEXT(": Reserve"), Run.ClaimReward(Recipient, ERewardCard::Reserve, Error))) { continue; }
        for (const TPair<int32, int32>& Pool : PoolValues(*Run.Battle().Find(Recipient))) { TestTrue(Build + TEXT(": 0/2 -> 1/3"), Pool.Key == 1 && Pool.Value == 3); }
        const FProgressionFacts Camp = FProgressionQuery::For(Run);
        TestTrue(Build + TEXT(": Recovery"), Run.Progress().Phase == EExpeditionPhase::Recovery);
        TestTrue(Build + TEXT(": Camp"), Run.Recover(true, Error));
        const FCombatantState& After = *Run.Battle().Find(Recipient);
        const FBuildDefinition& B = *FBuildCatalog::Find(Build);
        // min(cap + 1, held + 1 + recovery): Kit, Slot, Rage and Wind recover 1; Maneuver recovers 0, so a spent pool stays 1/3.
        if (B.KitCap) { TestEqual(Build + TEXT(": Kit"), After.KitUses, FMath::Min(3, 1 + B.KitRecovery)); }
        if (B.SlotCap) { TestEqual(Build + TEXT(": Slot"), After.SpellSlots, FMath::Min(3, 1 + B.SlotRecovery)); }
        if (B.RageCap) { TestEqual(Build + TEXT(": Rage"), After.RageUses, FMath::Min(3, 1 + B.RageRecovery)); }
        if (B.WindCap) { TestEqual(Build + TEXT(": Wind"), After.SecondWindUses, FMath::Min(3, 1 + B.WindRecovery)); }
        if (B.ManeuverCap) { TestEqual(Build + TEXT(": Maneuver stays 1/3"), After.ManeuverUses, 1); TestEqual(TEXT("Maneuver recovers 0"), B.ManeuverRecovery, 0); }
        for (int32 Id = 1; Id <= 4; ++Id)
        {
            const FCombatantState& U = *Run.Battle().Find(Id);
            if (Id != Recipient) { for (const TPair<int32, int32>& Pool : PoolValues(U)) { TestTrue(U.BuildId + TEXT(": a hero without Reserve stops at its own cap"), Pool.Key == Pool.Value && Pool.Value == 2); } }
            // The query's camp line is the camp's result.
            const FHeroProgressFacts& H = Camp.Heroes[Id - 1];
            TestTrue(TEXT("Camp facts for each survivor"), H.bCamp && H.HpAfter == U.HP);
            const TArray<TPair<int32, int32>> Now = PoolValues(U);
            if (TestEqual(TEXT("Camp pool facts"), H.Camp.Num(), Now.Num()))
            { for (int32 I = 0; I < Now.Num(); ++I) { TestTrue(TEXT("Camp pool after"), H.Camp[I].HeldAfter == Now[I].Key && H.Camp[I].CapAfter == Now[I].Value); } }
        }
        // Recovery amounts are the build's and never change.
        TestTrue(Build + TEXT(": recovery amounts unchanged"), FActionFactsQuery::ForBuild(After, B.Specials[0]).Resources.ContainsByPredicate([&](const FActionResourceCost& C) { return C.CampRecovery >= 0 && C.Cap == 3; }));
    }
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLevelGainsAreFactsOnlyTest, "Project.Expedition.Run.LevelGainsAreFactsOnly",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FLevelGainsAreFactsOnlyTest::RunTest(const FString& Parameters)
{
    using namespace ProgressionTest;
    const TArray<FPartyBuildSelection> Selection = PoolParty();
    // Reserve at reward 1 to the last survivor; Gear to the first after that. The seed: a Reserve hero dies before a later reward.
    auto Claims = [](FExpeditionRun& R)
    {
        FString E;
        if (R.Progress().EncountersCompleted > 1) { return R.ClaimReward(FirstSurvivor(R), E); }
        int32 Last = INDEX_NONE;
        for (const FCombatantState& U : R.Battle().Snapshot().Encounter.Combatants) { if (U.Side == ECombatSide::Party && U.Life == ECombatLife::Conscious) { Last = U.Id; } }
        return R.ClaimReward(Last, ERewardCard::Reserve, E);
    };
    auto FallenReserve = [](const FExpeditionRun& R)
    { return R.Battle().Snapshot().Encounter.Combatants.ContainsByPredicate([](const FCombatantState& U) { return U.Side == ECombatSide::Party && U.Life == ECombatLife::Dead && U.bReserve; }); };
    const int32 Seed = FirstSeedWhere([&](int32 S)
    {
        FExpeditionRun Probe; FString E;
        if (!Probe.NewRun(S, Selection, E, TEXT("levels"))) { return false; }
        for (int32 Steps = 0; Steps < 20000 && !Ended(Probe); ++Steps)
        {
            if (Probe.Progress().Phase == EExpeditionPhase::Reward && FallenReserve(Probe)) { return true; }
            if (!Step(Probe, Claims)) { return false; }
        }
        return false;
    }, 42, 400);
    if (!TestTrue(TEXT("A seed where a Reserve hero falls before a later reward"), Seed != INDEX_NONE)) { return false; }
    FExpeditionRun Run; FString Error;
    Run.NewRun(Seed, Selection, Error, TEXT("levels"));
    int32 Rewards = 0;
    for (int32 Steps = 0; Steps < 20000 && !Ended(Run); ++Steps)
    {
        FCombatCommand Last; FCombatBattle Before;
        const bool bCombat = Run.Progress().Phase == EExpeditionPhase::Combat;
        if (!TestTrue(TEXT("Step"), Step(Run, Claims, &Last, &Before))) { return false; }
        if (!bCombat || Run.Progress().Phase != EExpeditionPhase::Reward) { continue; }
        ++Rewards;
        const int32 Victories = Run.Progress().EncountersCompleted;
        // The battle the last command produced, without the run's level-ups: a level adds +1 basic damage and nothing else.
        FCombatBattle Plain = Before;
        Plain.Submit(Last);
        const FProgressionFacts Facts = FProgressionQuery::For(Run);
        TestEqual(TEXT("Survivors reach the next level"), Facts.SurvivorLevel, Victories + 1);
        TestTrue(TEXT("Facts are a pure query"), SameRun(Run.Snapshot(), Run.Snapshot()));
        for (const FHeroProgressFacts& H : Facts.Heroes)
        {
            const FCombatantState& U = *Run.Battle().Find(H.Id);
            const FCombatantState& P = *Plain.Find(H.Id);
            const FString Where = FString::Printf(TEXT("victory %d hero %d"), Victories, H.Id);
            TestTrue(Where + TEXT(": basic damage parts"), H.Damage.Total == FBuildCatalog::AttackModifier(U) && H.Damage.Total == H.Damage.Build + H.Damage.Levels + H.Damage.Gear + H.Damage.Rage &&
                H.Damage.Levels == U.Level - 1 && H.Damage.Gear == U.WeaponRank && H.Damage.Rage == 0);
            TestTrue(Where + TEXT(": caps and upgrades"), H.bReserve == U.bReserve && H.bHoned == U.bHoned && H.Caps.Kit == FBuildCatalog::Caps(U).Kit && H.Pools.Num() == PoolValues(U).Num());
            if (U.Life == ECombatLife::Conscious)
            {
                TestTrue(Where + TEXT(": levelled"), H.bLevelled && H.Level == Victories + 1 && H.LevelBefore == Victories && H.DamageBeforeLevel == H.Damage.Total - 1);
                TestTrue(Where + TEXT(": the level changed only level and basic damage"), U.Level == P.Level + 1 && U.DamageBonus == P.DamageBonus + 1 && U.HP == P.HP && U.MaxHP == P.MaxHP &&
                    U.KitUses == P.KitUses && U.SpellSlots == P.SpellSlots && U.SecondWindUses == P.SecondWindUses && U.ManeuverUses == P.ManeuverUses && U.RageUses == P.RageUses && U.Potions == P.Potions);
                // Each card's facts are what the claim commits.
                for (const FRewardCardFacts& Card : H.Cards)
                {
                    FExpeditionRun Claimed = Run; FString E;
                    const bool bAccepted = Claimed.ClaimReward(H.Id, Card.Card, E);
                    TestEqual(Where + TEXT(": availability is the claim's"), Card.bAvailable, bAccepted);
                    if (!bAccepted) { continue; }
                    const FCombatantState& A = *Claimed.Battle().Find(H.Id);
                    TestTrue(Where + TEXT(": card after-values are the claim's"), Card.DamageAfter.Total == FBuildCatalog::AttackModifier(A) && Card.GearAfter == A.WeaponRank && Card.PotionsAfter == A.Potions);
                    const TArray<TPair<int32, int32>> Pools = PoolValues(A);
                    for (int32 I = 0; I < Pools.Num() && I < Card.Pools.Num(); ++I) { TestTrue(Where + TEXT(": card pools"), Card.Pools[I].HeldAfter == Pools[I].Key && Card.Pools[I].CapAfter == Pools[I].Value); }
                }
            }
            else
            {
                TestTrue(Where + TEXT(": a dead hero keeps its death level and upgrades"), !H.bLevelled && H.Level == U.Level && H.LevelBefore == U.Level && U.Level <= Victories && U.Level == P.Level);
                for (const FRewardCardFacts& Card : H.Cards) { TestTrue(Where + TEXT(": every card says Fallen"), !Card.bAvailable && Card.Note == ERewardCardNote::Fallen); }
                if (U.bReserve) { TestTrue(Where + TEXT(": Reserve's caps outlive the hero"), FBuildCatalog::Caps(U).Kit == 3 || FBuildCatalog::Caps(U).Rage == 3 || FBuildCatalog::Caps(U).Wind == 3); }
            }
        }
    }
    TestTrue(TEXT("Rewards were checked"), Rewards >= 2);
    return !HasAnyErrors();
}

// ------------------------------------------------------------------------------------------------------------------------------ Catalog

namespace ProgressionTest
{
/** The hero (10) acts first. Others act at -10: an ally (11), two enemies (20, 21) with 100 HP that always hit hard, and a far ally (12). */
FCombatEncounter Fixture(const FBuildDefinition& Build, bool bHoned)
{
    FCombatEncounter E; E.Width = E.Height = 12;
    FCombatantState Hero = Build.Profile;
    Hero.Id = 10; Hero.Name = TEXT("Hero"); Hero.Cell = {1, 1}; Hero.InitiativeBonus = 30; Hero.HP = 1; Hero.Potions = 0; Hero.bHoned = bHoned;
    FCombatantState Ally = Build.Profile;
    Ally.Id = 11; Ally.Name = TEXT("Ally"); Ally.Cell = {1, 2}; Ally.InitiativeBonus = -10; Ally.HP = 1;
    FCombatantState Far = Ally; Far.Id = 12; Far.Name = TEXT("Far"); Far.Cell = {11, 11};
    FCombatantState Enemy;
    Enemy.Id = 20; Enemy.Name = TEXT("Target"); Enemy.Role = TEXT("Raider"); Enemy.Side = ECombatSide::Enemy; Enemy.Cell = {2, 1}; Enemy.InitiativeBonus = -10;
    Enemy.MaxHP = Enemy.HP = 100; Enemy.ArmorClass = 1; Enemy.AttackBonus = 30; Enemy.DamageBonus = 10;
    FCombatantState Other = Enemy; Other.Id = 21; Other.Name = TEXT("Other"); Other.Cell = {3, 1};
    E.Combatants = {Hero, Ally, Far, Enemy, Other};
    return E;
}

FCombatantState& Unit(FCombatEncounter& E, int32 Id) { return *E.Combatants.FindByPredicate([Id](const FCombatantState& U) { return U.Id == Id; }); }

/** Ends turns (the party's) and plays enemy turns until enemy 20 has acted; returns its results. The hero acts first, so it never acts again before. */
TArray<FCombatResult> UntilEnemyActs(FCombatBattle& B)
{
    B.Submit({EC::EndTurn, 10});
    for (int32 Guard = 0; Guard < 10 && B.Snapshot().Phase == EBattlePhase::Fighting; ++Guard)
    {
        const int32 Id = B.ActiveId();
        if (Id == 20) { return B.PlayEnemyTurn(); }
        if (B.Find(Id)->Side == ECombatSide::Enemy) { B.PlayEnemyTurn(); } else { B.Submit({EC::EndTurn, Id}); }
    }
    return {};
}

/** The honed row of contract 13 section 3 for one ability, compared with the same ability unhoned. */
void ExpectRow(FAutomationTestBase& Test, const FString& Where, const FCombatantState& Honed, const FAbilityFacts& A, const FAbilityFacts& Base)
{
    using namespace UpgradeRules;
    const int32 Bonus = FBuildCatalog::HonedBonus(Honed, A.Command);
    switch (A.Effect)
    {
    case EAbilityEffect::Strike: case EAbilityEffect::Push: Test.TestEqual(Where + TEXT(": +2 damage"), A.Amount, Base.Amount + HonedStrike); break;
    case EAbilityEffect::Area: Test.TestEqual(Where + TEXT(": +1 per victim"), A.Amount, Base.Amount + HonedArea); break;
    case EAbilityEffect::Surge: Test.TestEqual(Where + TEXT(": +1 base"), A.Amount, Base.Amount + HonedSurge); break;
    case EAbilityEffect::Protect: Test.TestEqual(Where + TEXT(": +1 reduction"), A.Amount, Base.Amount + HonedProtect); Test.TestTrue(Where + TEXT(": within the cap"), A.Amount <= MaxProtect); break;
    case EAbilityEffect::Swap: Test.TestEqual(Where + TEXT(": +2 range"), A.Range, Base.Range + HonedSwapRange); break;
    case EAbilityEffect::Healing:
        if (A.bCureDice || A.Command == EC::CureWounds) { Test.TestEqual(Where + TEXT(": Cure Wounds +3"), Bonus, HonedCure); }
        else if (A.Command == EC::SecondWind) { Test.TestEqual(Where + TEXT(": Second Wind +3"), Bonus, HonedWind); }
        else { Test.TestEqual(Where + TEXT(": +3 HP"), A.Amount, Base.Amount + HonedKitHealing); }
        break;
    case EAbilityEffect::Missile: Test.TestEqual(Where + TEXT(": Magic Missile +2"), Bonus, HonedMissile); break;
    case EAbilityEffect::Rage: Test.TestEqual(Where + TEXT(": Rage +1"), Bonus, HonedRage); break;
    case EAbilityEffect::WeaponAttack: Test.TestEqual(Where + TEXT(": Precision Strike +1"), Bonus, HonedPrecision); break;
    case EAbilityEffect::Mitigation: break;
    }
    if (A.Effect != EAbilityEffect::Swap) { Test.TestEqual(Where + TEXT(": range unchanged"), A.Range, Base.Range); }
    Test.TestNotEqual(Where + TEXT(": the honed description is regenerated"), A.Description, Base.Description);
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHonedFactsMatchTest, "Project.Expedition.Catalog.HonedFactsMatchForecastAndResolution",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FHonedFactsMatchTest::RunTest(const FString& Parameters)
{
    using namespace ProgressionTest;
    int32 Checked = 0;
    for (const FBuildDefinition& Build : FBuildCatalog::All())
    {
        FCombatantState Honed = Build.Profile; Honed.bHoned = true;
        const TArray<FAbilityFacts> Sharp = FBuildCatalog::Abilities(Honed), Plain = FBuildCatalog::Abilities(Build.Profile);
        if (!TestEqual(Build.Id + TEXT(": two specials or the legacy ones"), Sharp.Num(), Plain.Num())) { continue; }
        for (int32 K = 0; K < Sharp.Num(); ++K)
        {
            const FAbilityFacts& A = Sharp[K];
            const FString Where = Build.Id + TEXT(" / ") + A.Name;
            ExpectRow(*this, Where, Honed, A, Plain[K]);
            const int32 Bonus = FBuildCatalog::HonedBonus(Honed, A.Command);
            ++Checked;
            // Retry the fixture's seed only for a natural-1 miss of a hit this check needs; every other result is deterministic.
            bool bDone = false;
            for (int32 Seed = 42; Seed < 82 && !bDone; ++Seed)
            {
                FCombatEncounter E = Fixture(Build, true);
                const bool bRageSpecial = Build.RageCap > 0 && A.Effect != EAbilityEffect::Rage;
                if (bRageSpecial) { Unit(E, 10).RageUses = 1; Unit(E, 10).RageTurns = 2; }
                const bool bProtectAlly = A.Effect == EAbilityEffect::Protect && A.Target != EAbilityTarget::Self;
                if (A.Effect == EAbilityEffect::Protect) { Unit(E, 21).Cell = {10, 10}; Unit(E, 20).Cell = bProtectAlly ? FIntPoint(1, 3) : FIntPoint(2, 1); }
                if (A.Effect == EAbilityEffect::Swap) { Unit(E, 12).Cell = {1, 1 + A.Range}; }
                FCombatBattle B; FString Error;
                if (!TestTrue(Where + TEXT(": fixture starts: ") + Error, B.Start(E, Seed, Error))) { bDone = true; break; }
                const int32 Target = A.Target == EAbilityTarget::Self ? 10 : A.Target == EAbilityTarget::Enemy ? 20 : A.Effect == EAbilityEffect::Swap ? 12 : 11;
                if (A.Effect == EAbilityEffect::Rage)
                {
                    // Rage: the basic attack's forecast and hit use the honed bonus.
                    TestTrue(Where + TEXT(": rage"), B.Submit({A.Command, 10, {}, 10}).bAccepted);
                    const int32 Modifier = Build.Profile.DamageBonus + BuildRules::RageBonus + Bonus;
                    TestEqual(Where + TEXT(": attack modifier while raging"), FBuildCatalog::AttackModifier(*B.Find(10)), Modifier);
                    const FActionForecast F = B.Forecast(10, EC::Attack, 20);
                    TestEqual(Where + TEXT(": forecast minimum"), F.MinAmount, 1 + Modifier);
                    const FCombatResult R = B.Submit({EC::Attack, 10, {}, 20});
                    if (!R.bHit) { continue; }
                    TestEqual(Where + TEXT(": the hit's modifier"), R.EffectModifier, Modifier);
                    TestTrue(Where + TEXT(": text"), A.Description.Contains(FString::Printf(TEXT("+%d basic damage"), BuildRules::RageBonus + Bonus)));
                    bDone = true; break;
                }
                const TArray<int32> Legal = B.LegalTargets(10, A.Command);
                if (!TestTrue(Where + TEXT(": the target is legal"), Legal.Contains(Target))) { bDone = true; break; }
                const FActionForecast F = B.Forecast(10, A.Command, Target);
                const FCombatResult R = B.Submit({A.Command, 10, {}, Target});
                if (!TestTrue(Where + TEXT(": committed"), R.bAccepted)) { bDone = true; break; }
                int32 Dice = 0;
                for (const FDieRoll& Die : R.EffectDice) { Dice += Die.Value; }
                switch (A.Effect)
                {
                case EAbilityEffect::Strike: case EAbilityEffect::Push:
                    TestTrue(Where + TEXT(": facts = forecast = committed"), F.MinAmount == A.Amount && F.MaxAmount == A.Amount && R.Damage == A.Amount);
                    TestTrue(Where + TEXT(": text"), A.Description.Contains(FString::FromInt(A.Amount)));
                    break;
                case EAbilityEffect::Surge:
                    TestTrue(Where + TEXT(": facts = forecast = committed"), F.MinAmount == A.Amount && F.MaxAmount == 2 * A.Amount && (R.Damage == A.Amount || R.Damage == 2 * A.Amount));
                    TestTrue(Where + TEXT(": text"), A.Description.Contains(FString::FromInt(A.Amount)));
                    break;
                case EAbilityEffect::Area:
                {
                    TArray<int32> Forecast, Committed;
                    for (const FTargetEffect& X : F.Effects) { if (X.Effect == EAbilityEffect::Strike) { Forecast.Add(X.Amount); } }
                    for (const FTargetEffect& X : R.Effects) { if (X.Effect == EAbilityEffect::Strike) { Committed.Add(X.Amount); } }
                    TestTrue(Where + TEXT(": two victims at the honed amount, forecast = committed"), Forecast == Committed && Forecast.Num() == 2 && Forecast[0] == A.Amount && Forecast[1] == A.Amount);
                    TestTrue(Where + TEXT(": text"), A.Description.Contains(FString::FromInt(A.Amount)));
                    break;
                }
                case EAbilityEffect::Missile:
                {
                    const int32 Modifier = BuildRules::MissileDarts * BuildRules::DartBonus + Bonus;
                    TestTrue(Where + TEXT(": facts = forecast = committed"), F.MinAmount == BuildRules::MissileDarts + Modifier && F.MaxAmount == BuildRules::MissileDarts * BuildRules::MissileDie + Modifier &&
                        R.EffectModifier == Modifier && R.Damage == Dice + Modifier);
                    TestTrue(Where + TEXT(": text"), A.Description.Contains(FString::Printf(TEXT("3d4+%d"), Modifier)));
                    break;
                }
                case EAbilityEffect::Healing:
                {
                    if (A.bCureDice || A.Command == EC::CureWounds || A.Command == EC::SecondWind)
                    {
                        const bool bWind = A.Command == EC::SecondWind;
                        const int32 Count = bWind ? 1 : BuildRules::CureDice, Sides = bWind ? BuildRules::WindDie : BuildRules::CureDie;
                        const int32 Modifier = (bWind ? BuildRules::FighterLevel : BuildRules::HealingModifier) + Bonus;
                        const int32 Missing = B.Find(Target)->MaxHP - (B.Find(Target)->HP - R.Healing);
                        TestTrue(Where + TEXT(": facts = forecast = committed"), F.MinAmount == FMath::Min(Missing, Count + Modifier) && F.MaxAmount == FMath::Min(Missing, Count * Sides + Modifier) &&
                            R.EffectModifier == Modifier && R.Healing == FMath::Min(Missing, Dice + Modifier));
                        TestTrue(Where + TEXT(": text"), A.Description.Contains(FString::Printf(TEXT("d%d+%d"), Sides, Modifier)));
                    }
                    else
                    {
                        TestTrue(Where + TEXT(": facts = forecast = committed"), F.MinAmount == A.Amount && F.MaxAmount == A.Amount && R.Healing == A.Amount);
                        TestTrue(Where + TEXT(": text"), A.Description.Contains(FString::FromInt(A.Amount)));
                    }
                    break;
                }
                case EAbilityEffect::WeaponAttack:
                {
                    const int32 Extra = BuildRules::PrecisionBonus + Bonus;
                    TestEqual(Where + TEXT(": accuracy"), R.AttackBonus, Build.Profile.AttackBonus + Extra);
                    TestEqual(Where + TEXT(": forecast damage"), F.MinAmount, 1 + Build.Profile.DamageBonus + Extra);
                    TestTrue(Where + TEXT(": text"), A.Description.Contains(FString::Printf(TEXT("+%d to hit and +%d damage"), Extra, Extra)));
                    if (!R.bHit) { continue; }
                    TestEqual(Where + TEXT(": committed damage modifier"), R.EffectModifier, Build.Profile.DamageBonus + Extra);
                    break;
                }
                case EAbilityEffect::Swap:
                {
                    TestEqual(Where + TEXT(": swapped across the honed range"), B.Find(10)->Cell, FIntPoint(1, 1 + A.Range));
                    FCombatBattle Unhoned; FString E2;
                    Unhoned.Start(Fixture(Build, false), Seed, E2);
                    FCombatEncounter Plain2 = Fixture(Build, false); Unit(Plain2, 12).Cell = {1, 1 + A.Range};
                    if (Build.RageCap > 0) { Unit(Plain2, 10).RageUses = 1; Unit(Plain2, 10).RageTurns = 2; }
                    Unhoned.Start(Plain2, Seed, E2);
                    TestFalse(Where + TEXT(": beyond the unhoned range"), Unhoned.LegalTargets(10, A.Command).Contains(12));
                    TestTrue(Where + TEXT(": text"), A.Description.Contains(FString::Printf(TEXT("range %d"), A.Range)));
                    break;
                }
                case EAbilityEffect::Protect:
                {
                    // The committed effect, the protection query (amount and source) and a real enemy hit all hold the honed amount.
                    const FTargetEffect* Effect = R.Effects.FindByPredicate([](const FTargetEffect& X) { return X.Effect == EAbilityEffect::Protect; });
                    TestTrue(Where + TEXT(": committed protect effect"), Effect && Effect->Amount == A.Amount && Effect->TargetId == Target);
                    const FProtectionFacts P = B.Protection(Target);
                    TestTrue(Where + TEXT(": FCombatBattle::Protection"), P.Amount == A.Amount && P.SourceId == 10);
                    TestTrue(Where + TEXT(": text"), A.Description.Contains(FString::FromInt(A.Amount)));
                    const TArray<FCombatResult> Hits = UntilEnemyActs(B);
                    const FCombatResult* Hit = Hits.FindByPredicate([Target](const FCombatResult& X) { return X.Type == EC::Attack && X.TargetId == Target; });
                    if (!Hit || !Hit->bHit) { continue; }
                    TestEqual(Where + TEXT(": a real hit prevents the honed amount"), Hit->DamagePrevented, A.Amount);
                    break;
                }
                case EAbilityEffect::Rage: case EAbilityEffect::Mitigation: break;
                }
                bDone = true;
            }
            TestTrue(Where + TEXT(": a hit landed within the seed range"), bDone);
        }
    }
    TestTrue(TEXT("Every kit's specials were checked (two per kit, one for the Champion)"), Checked == 70 * 2 - 1);
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHonedShelterTest, "Project.Expedition.Catalog.HonedShelterProtectsThroughRage",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FHonedShelterTest::RunTest(const FString& Parameters)
{
    using namespace ProgressionTest;
    const FBuildDefinition& Totem = *FBuildCatalog::Find(TEXT("barbarian.totem-warrior"));
    // Raging (RageTurns RageLeft), Shelter on the ally (11) whom only enemy 20 can reach, then the enemies act.
    auto Shelter = [&](bool bHoned, int32 RageLeft, int32 Seed, FCombatBattle& B)
    {
        FCombatEncounter E = Fixture(Totem, bHoned);
        Unit(E, 10).RageUses = 1; Unit(E, 10).RageTurns = RageLeft;
        Unit(E, 20).Cell = {1, 3}; Unit(E, 21).Cell = {10, 10};
        FString Error;
        return B.Start(E, Seed, Error) && B.Submit({EC::Special2, 10, {}, 11}).bAccepted;
    };
    for (const bool bHoned : {true, false})
    {
        const int32 Amount = bHoned ? 4 : 3;
        const FString Where = bHoned ? TEXT("honed") : TEXT("unhoned");
        bool bHit = false;
        for (int32 Seed = 42; Seed < 82 && !bHit; ++Seed)
        {
            FCombatBattle B;
            if (!TestTrue(Where + TEXT(": Shelter"), Shelter(bHoned, 2, Seed, B))) { return false; }
            const FProtectionFacts P = B.Protection(11);
            TestTrue(Where + TEXT(": Protection names the Barbarian"), P.Amount == Amount && P.SourceId == 10);
            TestEqual(Where + TEXT(": the seam's amount"), FBuildCatalog::GuardAmount(*B.Find(10)), Amount);
            TestTrue(Where + TEXT(": status shows the amount and the lifetime: ") + FBuildCatalog::Status(*B.Find(10)),
                FBuildCatalog::Status(*B.Find(10)).Contains(FString::Printf(TEXT("Protection -> 11 (%d, until next own turn or Rage ends)"), Amount)));
            // A checkpoint with Shelter active resumes with the same protection and continues identically.
            FCombatBattle Resumed; FString Error;
            TestTrue(Where + TEXT(": the checkpoint restores"), Resumed.Restore(B.Snapshot(), Error));
            TestTrue(Where + TEXT(": the same protection"), Resumed.Protection(11).Amount == Amount && Resumed.Protection(11).SourceId == 10);
            const TArray<FCombatResult> Hits = UntilEnemyActs(B);
            const TArray<FCombatResult> Again = UntilEnemyActs(Resumed);
            TestTrue(Where + TEXT(": continues identically"), FCombatSnapshot::StaticStruct()->CompareScriptStruct(&B.Snapshot(), &Resumed.Snapshot(), 0));
            const FCombatResult* Hit = Hits.FindByPredicate([](const FCombatResult& X) { return X.Type == EC::Attack && X.TargetId == 11; });
            if (!Hit || !Hit->bHit) { continue; }
            bHit = true;
            TestEqual(Where + TEXT(": an enemy hit on the ally prevents the amount"), Hit->DamagePrevented, Amount);
        }
        TestTrue(Where + TEXT(": a hit landed"), bHit);
        // The guard ends when Rage ends: Shelter cast on Rage's last turn ends with that turn.
        FCombatBattle Last;
        if (TestTrue(Where + TEXT(": Shelter on the last Rage turn"), Shelter(bHoned, 1, 42, Last)))
        {
            Last.Submit({EC::EndTurn, 10});
            TestTrue(Where + TEXT(": Rage ended, and the guard with it"), Last.Find(10)->RageTurns == 0 && Last.Find(10)->GuardTargetId == INDEX_NONE && Last.Protection(11).Amount == 0);
        }
        // Shelter cast on Rage's first turn ends when the Barbarian's next own turn starts, while Rage still has a turn left.
        FCombatBattle Next;
        if (TestTrue(Where + TEXT(": Shelter on the first Rage turn"), Shelter(bHoned, 2, 42, Next)))
        {
            UntilEnemyActs(Next);
            for (int32 Guard = 0; Guard < 10 && Next.Snapshot().Phase == EBattlePhase::Fighting && Next.ActiveId() != 10; ++Guard)
            { if (Next.Find(Next.ActiveId())->Side == ECombatSide::Enemy) { Next.PlayEnemyTurn(); } else { Next.Submit({EC::EndTurn, Next.ActiveId()}); } }
            TestEqual(Where + TEXT(": the Barbarian's next turn"), Next.ActiveId(), 10);
            TestTrue(Where + TEXT(": still raging, the guard ended"), Next.Find(10)->RageTurns == 1 && Next.Find(10)->GuardTargetId == INDEX_NONE && Next.Protection(11).Amount == 0);
        }
    }
    // Through a real save: a revision 10 run whose Totem Warrior took Honed at reward 1 casts Shelter in fight 2; the checkpoint resumes with it.
    const TArray<FPartyBuildSelection> Selection = Party({TEXT("barbarian.totem-warrior"), TEXT("fighter.champion"), TEXT("cleric.life"), TEXT("wizard.evocation")});
    auto Honed = [](FExpeditionRun& R) { FString E; return R.Progress().EncountersCompleted == 1 && Conscious(R, 1) ? R.ClaimReward(1, ERewardCard::Honed, E) : GearToFirst(R); };
    auto ShelterUp = [&](FExpeditionRun& Run) -> bool
    {
        for (int32 Steps = 0; Steps < 4000 && Run.Progress().Phase == EExpeditionPhase::Combat; ++Steps)
        {
            const int32 Id = Run.Battle().ActiveId();
            if (Id == 1 && Run.Battle().Find(1)->bHoned)
            {
                if (Run.Battle().LegalTargets(1, EC::Special1).Contains(1)) { Run.Submit({EC::Special1, 1, {}, 1}); }
                const TArray<int32> Allies = Run.Battle().LegalTargets(1, EC::Special2);
                if (!Allies.IsEmpty()) { return Run.Submit({EC::Special2, 1, {}, Allies[0]}).bAccepted; }
            }
            if (!Step(Run, GearToFirst)) { return false; }
        }
        return false;
    };
    const int32 Seed = FirstSeedWhere([&](int32 S)
    {
        FExpeditionRun Probe; FString E;
        return ReachReward(Probe, S, Selection, 1, GearToFirst) && Conscious(Probe, 1) && Honed(Probe) && Probe.Recover(true, E) &&
            Probe.ChooseRoute(Probe.Progress().Offers[0].Id, E) && ShelterUp(Probe);
    }, 42, 200);
    if (!TestTrue(TEXT("A seed where a honed Totem Warrior shelters in fight 2"), Seed != INDEX_NONE)) { return false; }
    FExpeditionRun Run; FString Error;
    ReachReward(Run, Seed, Selection, 1, GearToFirst); Honed(Run); Run.Recover(true, Error); Run.ChooseRoute(Run.Progress().Offers[0].Id, Error);
    TestTrue(TEXT("Shelter is up"), ShelterUp(Run));
    const int32 Ally = Run.Battle().Find(1)->GuardTargetId;
    TestEqual(TEXT("Honed Shelter protects for 4"), Run.Battle().Protection(Ally).Amount, 4);
    FExpeditionSaveData Loaded;
    TestTrue(TEXT("The checkpoint decodes"), FExpeditionSave::Decode(Encoded(*this, Run), Loaded, Error));
    FExpeditionRun Resumed;
    TestTrue(TEXT("and restores: ") + Error, Resumed.Restore(Loaded.Current, Error));
    TestTrue(TEXT("with the same protection"), Resumed.Battle().Protection(Ally).Amount == 4 && Resumed.Battle().Protection(Ally).SourceId == 1);
    FinishRunScripted(Run); FinishRunScripted(Resumed);
    TestTrue(TEXT("and continues identically"), SameRun(Run.Snapshot(), Resumed.Snapshot()));
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReserveCapsEverywhereTest, "Project.Expedition.Catalog.ReserveCapsEverywhere",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FReserveCapsEverywhereTest::RunTest(const FString& Parameters)
{
    using namespace ProgressionTest;
    // Every build's pools are exactly the pools its specials spend, so Reserve's +1 lands on those pools and no other.
    for (const FBuildDefinition& Build : FBuildCatalog::All())
    {
        TSet<EActionResource> Spent;
        for (const ECombatCommandType Command : Build.Specials)
        { for (const FActionResourceCost& C : FActionFactsQuery::ForBuild(Build.Profile, Command).Resources) { Spent.Add(C.Resource); } }
        TSet<EActionResource> Pools;
        if (Build.KitCap) { Pools.Add(EActionResource::Kit); }
        if (Build.SlotCap) { Pools.Add(EActionResource::Slot); }
        if (Build.WindCap) { Pools.Add(EActionResource::Wind); }
        if (Build.ManeuverCap) { Pools.Add(EActionResource::Maneuver); }
        if (Build.RageCap) { Pools.Add(EActionResource::Rage); }
        TestTrue(Build.Id + TEXT(": the pools are the ones its specials spend"), Spent.Num() == Pools.Num() && Spent.Includes(Pools));
    }
    for (const TCHAR* Id : {TEXT("rogue.thief"), TEXT("cleric.life"), TEXT("wizard.evocation"), TEXT("barbarian.berserker"), TEXT("fighter.champion"), TEXT("fighter.battle-master")})
    {
        const FBuildDefinition& Build = *FBuildCatalog::Find(Id);
        FCombatantState Reserved = Build.Profile; Reserved.bReserve = true;
        const FResourceCaps C = FBuildCatalog::Caps(Reserved), Plain = FBuildCatalog::Caps(Build.Profile);
        TestTrue(FString(Id) + TEXT(": +1 on each pool, none elsewhere"), C.Kit == (Build.KitCap ? Build.KitCap + 1 : 0) && C.Slot == (Build.SlotCap ? Build.SlotCap + 1 : 0) &&
            C.Wind == (Build.WindCap ? Build.WindCap + 1 : 0) && C.Maneuver == (Build.ManeuverCap ? Build.ManeuverCap + 1 : 0) && C.Rage == (Build.RageCap ? Build.RageCap + 1 : 0));
        TestTrue(FString(Id) + TEXT(": unreserved caps are the build's"), Plain.Kit == Build.KitCap && Plain.Slot == Build.SlotCap && Plain.Wind == Build.WindCap && Plain.Maneuver == Build.ManeuverCap && Plain.Rage == Build.RageCap);
        SetPools(Reserved, C.Kit, C.Slot, C.Wind, C.Maneuver, C.Rage);
        // Facts (and the usage tags and tooltips that format them) show the hero's own cap.
        for (const ECombatCommandType Command : Build.Specials)
        {
            for (const FActionResourceCost& Cost : FActionFactsQuery::ForBuild(Reserved, Command).Resources)
            { TestTrue(FString(Id) + TEXT(": facts hold the full Reserve pool"), Cost.Cap == 3 && Cost.Held == 3); }
        }
        // Validation: a full Reserve pool is legal; one more is not; the same pool without Reserve is not.
        FCombatEncounter E = Fixture(Build, false);
        { FCombatantState& H = Unit(E, 10); H = Reserved; H.Id = 10; H.Name = TEXT("Hero"); H.Cell = {1, 1}; H.InitiativeBonus = 30; }
        FCombatBattle B; FString Error;
        TestTrue(FString(Id) + TEXT(": a full Reserve pool starts: ") + Error, B.Start(E, 42, Error));
        TestTrue(FString(Id) + TEXT(": and its facts say so"), FActionFactsQuery::For(B, 10, Build.Specials[0]).Resources.ContainsByPredicate([](const FActionResourceCost& X) { return X.Cap == 3; }));
        FCombatEncounter Over = E; SetPools(Unit(Over, 10), C.Kit + 1, C.Slot + 1, C.Wind + 1, C.Maneuver + 1, C.Rage + 1);
        TestFalse(FString(Id) + TEXT(": above the cap is refused"), FCombatBattle().Start(Over, 42, Error));
        FCombatEncounter Unearned = E; Unit(Unearned, 10).bReserve = false;
        TestFalse(FString(Id) + TEXT(": the extra use without Reserve is refused"), FCombatBattle().Start(Unearned, 42, Error));
        FCombatEncounter Enemy = Fixture(Build, false); Unit(Enemy, 20).bReserve = true;
        TestFalse(FString(Id) + TEXT(": an enemy cannot hold Reserve"), FCombatBattle().Start(Enemy, 42, Error));
        FCombatEncounter Srd = E; Srd.Ruleset = ERuleset::SRD;
        TestFalse(FString(Id) + TEXT(": an SRD hero cannot hold Reserve"), FCombatBattle().Start(Srd, 42, Error));
    }
    // Camp: ReserveBeforeCampRecoversToTheNewCap plays each pool kind through a real camp. Here: a won battle recovers up to the hero's own cap.
    const FBuildDefinition& Rogue = *FBuildCatalog::Find(TEXT("rogue.thief"));
    FCombatEncounter E = Fixture(Rogue, false);
    Unit(E, 10).bReserve = true; Unit(E, 10).KitUses = 2; Unit(E, 11).KitUses = 2;
    FCombatBattle B; FString Error;
    if (TestTrue(TEXT("Camp fixture: ") + Error, B.Start(E, 42, Error)))
    {
        FCombatSnapshot Won = B.Snapshot();
        for (FCombatantState& U : Won.Encounter.Combatants) { if (U.Side == ECombatSide::Enemy) { U.Life = ECombatLife::Dead; U.HP = 0; } }
        Won.Phase = EBattlePhase::Victory;
        FCombatBattle Victory;
        if (TestTrue(TEXT("A won battle restores: ") + Error, Victory.Restore(Won, Error)) && TestTrue(TEXT("Camp"), FPartyContinuity::Recover(Victory, Error)))
        {
            TestEqual(TEXT("The Reserve hero recovers to 3"), Victory.Find(10)->KitUses, 3);
            TestEqual(TEXT("The other stops at 2"), Victory.Find(11)->KitUses, 2);
        }
    }
    return !HasAnyErrors();
}

// -------------------------------------------------------------------------------------------------------------------------- Persistence

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSchema5LoadsAsGearOnlyTest, "Project.Expedition.Persistence.Schema5LoadsAsGearOnly",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSchema5LoadsAsGearOnlyTest::RunTest(const FString& Parameters)
{
    using namespace ProgressionTest;
    // Real schema 5 files written by unmodified staging 6ee589d (#214 merged): revisions 8 and 9, route, reward and mid-battle (Fixtures/Schema5/README.md).
    const FString Fixtures = FPaths::ProjectDir() / TEXT("Source/DnDDiceRoll/Tests/Fixtures/Schema5");
    int32 Loaded = 0;
    for (const TCHAR* Revision : {TEXT("r8"), TEXT("r9")})
    {
        for (const TCHAR* Phase : {TEXT("route"), TEXT("reward"), TEXT("mid-battle")})
        {
            const FString Name = FString::Printf(TEXT("%s-%s-v5.json"), Phase, Revision);
            FString Raw, Error;
            if (!TestTrue(Name + TEXT(" exists"), FFileHelper::LoadFileToString(Raw, *(Fixtures / Name)))) { continue; }
            TestEqual(Name + TEXT(" is a real schema 5 file"), Parse(Raw)->GetIntegerField(TEXT("Version")), 5);
            TestFalse(Name + TEXT(" has no schema 6 fields"), Raw.Contains(TEXT("RewardCard")) || Raw.Contains(TEXT("bReserve")) || Raw.Contains(TEXT("bHoned")));
            FExpeditionSaveData Data, Expected;
            if (!TestTrue(Name + TEXT(" decodes through 5 -> 6: ") + Error, FExpeditionSave::Read(Fixtures / Name, Data, Error))) { continue; }
            if (!TestTrue(Name + TEXT(" staging's continuation decodes"), FExpeditionSave::Read(Fixtures / (TEXT("expected-") + Name), Expected, Error))) { continue; }
            ++Loaded;
            TestEqual(Name + TEXT(": version"), Data.Version, FExpeditionSave::Version);
            for (const FExpeditionEncounterRecord& H : Data.Current.Progress.History) { TestTrue(Name + TEXT(": every claim was Gear"), H.RewardCard == ERewardCard::Gear); }
            for (const FCombatantState& U : Data.Current.Battle.Encounter.Combatants) { TestTrue(Name + TEXT(": no upgrades"), !U.bReserve && !U.bHoned); }
            FExpeditionRun Run;
            if (!TestTrue(Name + TEXT(" restores: ") + Error, Run.Restore(Data.Current, Error))) { continue; }
            TestTrue(Name + TEXT(": restore rolls nothing"), SameRun(Data.Current, Run.Snapshot()));
            TestFalse(Name + TEXT(": gear only for life"), FExpeditionRun::OffersUpgradeCards(Run.Progress().Ruleset, Run.Progress().CatalogRevision));
            FinishRunScripted(Run);
            TestTrue(Name + TEXT(": continues exactly as staging continued it, RNG included"), SameRun(Expected.Current, Run.Snapshot()));
        }
    }
    TestEqual(TEXT("Six fixtures"), Loaded, 6);
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUpgradeChoicesRoundTripTest, "Project.Expedition.Persistence.UpgradeChoicesRoundTrip",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FUpgradeChoicesRoundTripTest::RunTest(const FString& Parameters)
{
    using namespace ProgressionTest;
    const TArray<FPartyBuildSelection> Selection = Party({TEXT("barbarian.totem-warrior"), TEXT("cleric.life"), TEXT("ranger.hunter"), TEXT("fighter.battle-master")});
    // Reserve, then Honed, then Gear, each to the last survivor (the slot enemies reach last).
    auto Cards = [](FExpeditionRun& R)
    {
        FString E;
        int32 Id = INDEX_NONE;
        for (const FCombatantState& U : R.Battle().Snapshot().Encounter.Combatants) { if (U.Side == ECombatSide::Party && U.Life == ECombatLife::Conscious) { Id = U.Id; } }
        const FCombatantState& U = *R.Battle().Find(Id);
        return R.ClaimReward(Id, !U.bReserve ? ERewardCard::Reserve : !U.bHoned ? ERewardCard::Honed : ERewardCard::Gear, E);
    };
    const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation/Progression"), FGuid::NewGuid().ToString());
    const FString Path = Directory / TEXT("current.json");
    TSet<EExpeditionPhase> Phases;
    int32 Checks = 0, Upgraded = 0;
    for (const int32 Seed : {42, 43, 44, 45, 46, 47})
    {
        FExpeditionRun Run; FString Error;
        Run.NewRun(Seed, Selection, Error, TEXT("round-trip"));
        for (int32 Steps = 0; Steps < 20000; ++Steps)
        {
            const EExpeditionPhase Phase = Run.Progress().Phase;
            // Every phase transition, and one mid-battle state per fight.
            const bool bCheck = !Phases.Contains(Phase) || Steps % 97 == 0 || Ended(Run);
            if (bCheck)
            {
                Phases.Add(Phase); ++Checks;
                Upgraded += Run.Battle().Snapshot().Encounter.Combatants.ContainsByPredicate([](const FCombatantState& U) { return U.bReserve || U.bHoned; }) ? 1 : 0;
                // Write, read and Resume through the session's own file.
                FExpeditionSaveData Data; Data.Current = Run.Snapshot();
                if (Ended(Run)) { Data.Records.Add(FExpeditionSave::Ending(Run)); }
                TestTrue(TEXT("Write"), FExpeditionSave::Write(Path, Data, Error));
                FExpeditionSession Session(Path);
                if (!TestTrue(TEXT("Resume: ") + Error, Session.Resume(Error))) { break; }
                TestTrue(TEXT("The resumed run is the run"), SameRun(Run.Snapshot(), Session.Run().Snapshot()));
                FExpeditionRun A = Run, B = Session.Run();
                for (int32 More = 0; More < 400 && !Ended(A); ++More) { const bool bA = Step(A, Cards), bB = Step(B, Cards); if (!bA || !bB) { break; } }
                TestTrue(TEXT("and continues identically"), SameRun(A.Snapshot(), B.Snapshot()));
            }
            if (Ended(Run) || !Step(Run, Cards)) { break; }
        }
    }
    IFileManager::Get().DeleteDirectory(*Directory, false, true);
    for (const EExpeditionPhase Phase : {EExpeditionPhase::Route, EExpeditionPhase::Combat, EExpeditionPhase::Reward, EExpeditionPhase::Recovery})
    { TestTrue(FString::Printf(TEXT("Phase %d round-tripped"), static_cast<int32>(Phase)), Phases.Contains(Phase)); }
    TestTrue(TEXT("Victory or Defeat round-tripped"), Phases.Contains(EExpeditionPhase::Victory) || Phases.Contains(EExpeditionPhase::Defeat));
    TestTrue(TEXT("Upgraded states round-tripped"), Upgraded > 5 && Checks > 20);
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRestoreRejectsUnearnedTest, "Project.Expedition.Persistence.RestoreRejectsUnearnedUpgrades",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRestoreRejectsUnearnedTest::RunTest(const FString& Parameters)
{
    using namespace ProgressionTest;
    FString Error;
    const TArray<FPartyBuildSelection> Selection = PoolParty();
    // Hero 4 took Reserve at reward 1 and hero 3 Honed at reward 2; route phase before fight 3.
    auto Plan = [](FExpeditionRun& R) { FString E; return R.Progress().EncountersCompleted == 1 ? R.ClaimReward(4, ERewardCard::Reserve, E) : R.ClaimReward(3, ERewardCard::Honed, E); };
    const int32 Seed = FirstSeedWhere([&](int32 S) { FExpeditionRun First, Probe; return ReachReward(First, S, Selection, 1, GearToFirst) && Conscious(First, 4) && ReachReward(Probe, S, Selection, 2, Plan) && Conscious(Probe, 3); }, 42, 400);
    if (!TestTrue(TEXT("A seed for the plan"), Seed != INDEX_NONE)) { return false; }
    FExpeditionRun Run;
    ReachReward(Run, Seed, Selection, 2, Plan);
    const FExpeditionSnapshot Pending = Run.Snapshot();
    TestTrue(TEXT("Honed at reward 2"), Plan(Run));
    const FExpeditionSnapshot Good = Run.Snapshot();
    FExpeditionRun Probe;
    TestTrue(TEXT("The earned state restores: ") + Error, Probe.Restore(Good, Error));
    auto Refused = [&](const TCHAR* What, TFunctionRef<void(FExpeditionSnapshot&)> Edit)
    {
        FExpeditionSnapshot Bad = Good; Edit(Bad);
        FExpeditionRun Target; Target.Restore(Good, Error);
        TestFalse(FString(What) + TEXT(" is refused"), Target.Restore(Bad, Error));
        TestTrue(FString(What) + TEXT(": the refusal leaves the run unchanged"), SameRun(Good, Target.Snapshot()));
    };
    Refused(TEXT("Reserve without history"), [](FExpeditionSnapshot& S) { Hero(S, 1)->bReserve = true; });
    Refused(TEXT("Honed without history"), [](FExpeditionSnapshot& S) { Hero(S, 2)->bHoned = true; });
    Refused(TEXT("History without the Honed flag"), [](FExpeditionSnapshot& S) { Hero(S, 3)->bHoned = false; });
    Refused(TEXT("History without the Reserve flag"), [](FExpeditionSnapshot& S) { Hero(S, 4)->bReserve = false; SetPools(*Hero(S, 4), 0, 0, 0, 0, 0); });
    Refused(TEXT("Two Reserve claims"), [](FExpeditionSnapshot& S) { S.Progress.History[1].RewardCard = ERewardCard::Reserve; S.Progress.History[1].RewardRecipient = 4; Hero(S, 3)->bHoned = false; });
    {
        // A card on the record whose reward is still pending (the Reward phase): no recipient yet, so it must hold Gear.
        FExpeditionSnapshot Bad = Pending; Bad.Progress.History.Last().RewardCard = ERewardCard::Honed;
        FExpeditionRun Target;
        TestTrue(TEXT("The pending reward restores"), Target.Restore(Pending, Error));
        TestFalse(TEXT("A card on the unclaimed record is refused"), Target.Restore(Bad, Error));
        TestTrue(TEXT("and changes nothing"), SameRun(Pending, Target.Snapshot()));
    }
    Refused(TEXT("A card out of range"), [](FExpeditionSnapshot& S) { S.Progress.History[0].RewardCard = static_cast<ERewardCard>(3); });
    Refused(TEXT("A pool above the Reserve cap"), [](FExpeditionSnapshot& S) { Hero(S, 4)->ManeuverUses = 4; });
    Refused(TEXT("A pool above an unreserved cap"), [](FExpeditionSnapshot& S) { Hero(S, 1)->KitUses = 3; });
    Refused(TEXT("Revision 9 wording on a revision 10 run"), [](FExpeditionSnapshot& S) { for (FString* P : Previews(S)) { P->ReplaceInline(TEXT("1 upgrade choice, +"), TEXT("+1 gear, +")); } });
    Refused(TEXT("Gear rank from a non-Gear claim"), [](FExpeditionSnapshot& S) { Hero(S, 4)->WeaponRank += 1; });
    // A non-Gear claim on revision 9 or SRD.
    for (const TPair<int32, ERuleset>& Kind : {TPair<int32, ERuleset>(9, ERuleset::Compact), TPair<int32, ERuleset>(10, ERuleset::SRD)})
    {
        FExpeditionRun Old;
        const int32 OldSeed = FirstSeedWhere([&](int32 S) { FExpeditionRun P; return ReachReward(P, S, Selection, 1, GearToFirst, Kind.Key, Kind.Value) && Conscious(P, 1); });
        if (!TestTrue(TEXT("Older reward"), OldSeed != INDEX_NONE && ReachReward(Old, OldSeed, Selection, 1, GearToFirst, Kind.Key, Kind.Value))) { continue; }
        TestTrue(TEXT("Gear to hero 1"), Old.ClaimReward(1, Error));
        FExpeditionSnapshot Bad = Old.Snapshot();
        Bad.Progress.History[0].RewardCard = ERewardCard::Reserve;
        Hero(Bad, 1)->WeaponRank -= 1;
        TestFalse(TEXT("A Reserve record on a gear-only run is refused"), FExpeditionRun().Restore(Bad, Error));
        Hero(Bad, 1)->bReserve = Kind.Value == ERuleset::Compact;
        TestFalse(TEXT("even with the flag"), FExpeditionRun().Restore(Bad, Error));
        FExpeditionSnapshot Wording = Old.Snapshot();
        for (FString* P : Previews(Wording)) { P->ReplaceInline(TEXT("+1 gear, +"), TEXT("1 upgrade choice, +")); P->ReplaceInline(TEXT("+1 gear (rank 3 cap)"), TEXT("1 upgrade choice")); }
        TestFalse(TEXT("Revision 10 wording on a gear-only run is refused"), FExpeditionRun().Restore(Wording, Error));
    }
    // Strict schema 6 import.
    const FString Json = Encoded(*this, Run);
    FExpeditionSaveData Out;
    TestTrue(TEXT("The schema 6 document decodes"), FExpeditionSave::Decode(Json, Out, Error));
    auto Document = [&](const TCHAR* What, TFunctionRef<void(const TSharedPtr<FJsonObject>&)> Edit)
    {
        const TSharedPtr<FJsonObject> Object = Parse(Json); Edit(Object);
        FExpeditionSaveData Unchanged = Out;
        TestFalse(FString(What) + TEXT(" is refused"), FExpeditionSave::Decode(Serialize(Object), Unchanged, Error));
        TestTrue(FString(What) + TEXT(": the output is untouched"), FExpeditionSaveData::StaticStruct()->CompareScriptStruct(&Out, &Unchanged, 0));
    };
    auto Current = [](const TSharedPtr<FJsonObject>& O) { return O->GetObjectField(TEXT("Current")); };
    Document(TEXT("A schema 6 record without RewardCard"), [&](const TSharedPtr<FJsonObject>& O) { Current(O)->GetObjectField(TEXT("Progress"))->GetArrayField(TEXT("History"))[0]->AsObject()->RemoveField(TEXT("RewardCard")); });
    Document(TEXT("A schema 6 hero without bHoned"), [&](const TSharedPtr<FJsonObject>& O) { Current(O)->GetObjectField(TEXT("Battle"))->GetObjectField(TEXT("Encounter"))->GetArrayField(TEXT("Combatants"))[0]->AsObject()->RemoveField(TEXT("bHoned")); });
    Document(TEXT("A fractional version"), [](const TSharedPtr<FJsonObject>& O) { O->SetNumberField(TEXT("Version"), 5.5); });
    Document(TEXT("A version as text"), [](const TSharedPtr<FJsonObject>& O) { O->SetStringField(TEXT("Version"), TEXT("6")); });
    Document(TEXT("A schema 5 document that already has schema 6 fields"), [](const TSharedPtr<FJsonObject>& O) { O->SetNumberField(TEXT("Version"), 5); });
    Document(TEXT("Version 7"), [](const TSharedPtr<FJsonObject>& O) { O->SetNumberField(TEXT("Version"), 7); });
    // Version 7 is refused, stays discoverable, and startup New Run preserves it beside the checkpoint.
    const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation/Progression"), FGuid::NewGuid().ToString());
    const FString Path = Directory / TEXT("current-v1.json");
    const TSharedPtr<FJsonObject> Future = Parse(Json); Future->SetNumberField(TEXT("Version"), 7);
    const FString FutureJson = Serialize(Future);
    TestTrue(TEXT("Write a version 7 file"), FFileHelper::SaveStringToFile(FutureJson, *Path));
    FExpeditionSession Session(Path);
    TestFalse(TEXT("Version 7 does not resume"), Session.Resume(Error));
    TestTrue(TEXT("and is named unsupported"), Error.Contains(TEXT("unsupported version")));
    FString Preserved;
    TestTrue(TEXT("New Run starts beside it: ") + Error, Session.NewRun(42, Selection, Error, FString(), &Preserved));
    FString Kept;
    TestTrue(TEXT("The version 7 file is preserved byte for byte"), !Preserved.IsEmpty() && FFileHelper::LoadFileToString(Kept, *Preserved) && Kept == FutureJson);
    IFileManager::Get().DeleteDirectory(*Directory, false, true);
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFailedUpgradeClaimTest, "Project.Expedition.Persistence.FailedUpgradeClaimIsAtomic",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FFailedUpgradeClaimTest::RunTest(const FString& Parameters)
{
    using namespace ProgressionTest;
    const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation/Progression"), FGuid::NewGuid().ToString());
    const FString Path = Directory / TEXT("current.json");
    FString Error;
    const TArray<FPartyBuildSelection> Selection = PoolParty();
    const int32 Seed = FirstSeedWhere([&](int32 S) { FExpeditionRun Probe; return ReachReward(Probe, S, Selection, 1, GearToFirst); });
    FExpeditionRun Run;
    if (!TestTrue(TEXT("Reward 1"), Seed != INDEX_NONE && ReachReward(Run, Seed, Selection, 1, GearToFirst))) { return false; }
    FExpeditionSaveData Data; Data.Current = Run.Snapshot();
    TestTrue(TEXT("Checkpoint the reward"), FExpeditionSave::Write(Path, Data, Error));
    FExpeditionSession Session(Path);
    TestTrue(TEXT("Resume it"), Session.Resume(Error));
    const int32 Id = FirstSurvivor(Session.Run());
    for (const ERewardCard Card : {ERewardCard::Reserve, ERewardCard::Honed})
    {
        TArray<uint8> Before, After;
        FFileHelper::LoadFileToArray(Before, *Path);
        const FExpeditionSnapshot Live = Session.Run().Snapshot();
        FailNextCheckpointWrite(*this, Path);
        TestFalse(TEXT("The claim is rejected when its checkpoint cannot be written"), Session.ClaimReward(Id, Card, Error));
        AllowCheckpointWrites(Path);
        FFileHelper::LoadFileToArray(After, *Path);
        TestTrue(TEXT("The live state and both random streams are unchanged"), SameRun(Live, Session.Run().Snapshot()));
        TestTrue(TEXT("The file bytes are unchanged"), Before == After);
    }
    TestTrue(TEXT("The claim then commits: ") + Error, Session.ClaimReward(Id, ERewardCard::Reserve, Error));
    FExpeditionSession Fresh(Path);
    TestTrue(TEXT("and resumes with Reserve"), Fresh.Resume(Error) && Fresh.Run().Battle().Find(Id)->bReserve && Fresh.Run().Progress().History[0].RewardCard == ERewardCard::Reserve);
    IFileManager::Get().DeleteDirectory(*Directory, false, true);
    return !HasAnyErrors();
}
#endif
