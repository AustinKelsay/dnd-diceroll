#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Persistence/ExpeditionSave.h"
#include "Run/JourneyQuery.h"

#include "ExpeditionTestSupport.h"
using namespace ExpeditionTestSupport;

// FJourneyQuery (#247, spec #239): the whole five-stage expedition from saved fields. Each stage prints as one exact line, so a test names the
// full expected state of every stage: "<index> <kind> <state> <world>", then " taken=<id>:<name>:<risk>:<outcome>:<survivors>" for a route taken
// and " offers=<id>:<name>:<world>:<risk>|..." while the offers are open.
namespace JourneyQueryTest
{
const TCHAR* Name(EEncounterStage Kind) { return Kind == EEncounterStage::Approach ? TEXT("Approach") : Kind == EEncounterStage::Pressure ? TEXT("Pressure") : TEXT("Finale"); }
const TCHAR* Name(EJourneyState State)
{
    switch (State) { case EJourneyState::Cleared: return TEXT("Cleared"); case EJourneyState::Lost: return TEXT("Lost"); case EJourneyState::Current: return TEXT("Current"); default: return TEXT("Fogged"); }
}
const TCHAR* Name(EExpeditionWorld World)
{
    switch (World) { case EExpeditionWorld::Root: return TEXT("Root"); case EExpeditionWorld::Vigil: return TEXT("Vigil"); case EExpeditionWorld::Vault: return TEXT("Vault"); default: return TEXT("Unknown"); }
}
const TCHAR* Risk(ERouteRisk Value) { return Value == ERouteRisk::None ? TEXT("none") : FExpeditionRun::RiskWord(Value); }
const TCHAR* Name(EBattlePhase Outcome) { return Outcome == EBattlePhase::Victory ? TEXT("Victory") : Outcome == EBattlePhase::Defeat ? TEXT("Defeat") : TEXT("Fighting"); }

FString Line(const FJourneyStage& Stage)
{
    FString Out = FString::Printf(TEXT("%d %s %s %s"), Stage.Index, Name(Stage.Kind), Name(Stage.State), Name(Stage.World));
    if (Stage.bTaken)
    {
        Out += FString::Printf(TEXT(" taken=%d:%s:%s:%s:%d"), Stage.Taken.Id, *Stage.Taken.Name, Risk(Stage.Taken.Risk), Name(Stage.Outcome), Stage.Survivors);
    }
    if (!Stage.Offers.IsEmpty())
    {
        TArray<FString> Offers;
        for (const FJourneyRoute& Offer : Stage.Offers) { Offers.Add(FString::Printf(TEXT("%d:%s:%s:%s"), Offer.Id, *Offer.Name, Name(Offer.World), Risk(Offer.Risk))); }
        Out += TEXT(" offers=") + FString::Join(Offers, TEXT("|"));
    }
    return Out;
}

TArray<FString> Lines(const FJourney& Journey)
{
    TArray<FString> Out;
    for (const FJourneyStage& Stage : Journey.Stages) { Out.Add(Line(Stage)); }
    return Out;
}

/** The two offers of a stage, written from the contract tables rather than from the code under test. */
FString Offers(int32 Stage, bool bRisk)
{
    const bool bFinale = Stage == 4;
    const TCHAR* Prefix = bFinale ? TEXT("Artifact vault: ") : TEXT("");
    const TCHAR* Guarded = Stage < 2 ? TEXT("low") : Stage < 4 ? TEXT("moderate") : TEXT("high");
    const TCHAR* Perilous = Stage < 2 ? TEXT("high") : TEXT("severe");
    return FString::Printf(TEXT(" offers=%d:%sStream trail:%s:%s|%d:%sWatchtower road:%s:%s"), Stage * 2 + 1, Prefix, bFinale ? TEXT("Vault") : TEXT("Root"), bRisk ? Guarded : TEXT("none"),
        Stage * 2 + 2, Prefix, bFinale ? TEXT("Vault") : TEXT("Vigil"), bRisk ? Perilous : TEXT("none"));
}

FString Fogged(int32 Index)
{
    return FString::Printf(TEXT("%d %s Fogged Unknown"), Index, Index < 2 ? TEXT("Approach") : Index < 4 ? TEXT("Pressure") : TEXT("Finale"));
}

/** The line of a stage the party has cleared on the guarded (false) or dangerous (true) road. */
FString Cleared(const FExpeditionProgress& Progress, int32 Index, bool bRisk)
{
    const FExpeditionEncounterRecord& Record = Progress.History[Index];
    const bool bDangerous = Record.Route.bDangerous;
    const bool bFinale = Index == 4;
    const TCHAR* Guarded = Index < 2 ? TEXT("low") : Index < 4 ? TEXT("moderate") : TEXT("high");
    const TCHAR* Danger = Index < 2 ? TEXT("high") : TEXT("severe");
    const TCHAR* World = bFinale ? TEXT("Vault") : bDangerous ? TEXT("Vigil") : TEXT("Root");
    return FString::Printf(TEXT("%d %s Cleared %s taken=%d:%s%s:%s:Victory:%d"), Index, Index < 2 ? TEXT("Approach") : Index < 4 ? TEXT("Pressure") : TEXT("Finale"), World, Index * 2 + (bDangerous ? 2 : 1),
        bFinale ? TEXT("Artifact vault: ") : TEXT(""), bDangerous ? TEXT("Watchtower road") : TEXT("Stream trail"), bRisk ? (bDangerous ? Danger : Guarded) : TEXT("none"), Record.Survivors);
}

/** Plays one step of the scripted player: the route (the offer Choice(stage) picks), the fight, the first conscious survivor's reward, and the camp (bCamp). False if it cannot. */
bool Step(FExpeditionRun& Run, TFunctionRef<int32(int32)> Choice, bool bCamp)
{
    FString Error;
    switch (Run.Progress().Phase)
    {
    case EExpeditionPhase::Route: return Run.ChooseRoute(Run.Progress().Offers[Choice(Run.Progress().EncountersCompleted)].Id, Error);
    case EExpeditionPhase::Combat: return FinishCombat(Run);
    case EExpeditionPhase::Reward:
    {
        for (const FCombatantState& Unit : Run.Battle().Snapshot().Encounter.Combatants)
        { if (Unit.Side == ECombatSide::Party && Unit.Life == ECombatLife::Conscious) { return Run.ClaimReward(Unit.Id, Error); } }
        return false;
    }
    case EExpeditionPhase::Recovery: return Run.Recover(bCamp, Error);
    default: return false;
    }
}

/** Plays the scripted player until Done says stop. False if a step fails or the run ends first (unless Done wants the ending). */
bool PlayUntil(FExpeditionRun& Run, TFunctionRef<int32(int32)> Choice, bool bCamp, TFunctionRef<bool(const FExpeditionRun&)> Done)
{
    for (int32 Guard = 0; Guard < 64; ++Guard)
    {
        if (Done(Run)) { return true; }
        if (!Step(Run, Choice, bCamp)) { return false; }
    }
    return Done(Run);
}

auto DangerousRoads = [](int32) { return 1; };
auto AlternatingRoads = [](int32 Stage) { return Stage % 2; };

bool AtRoute(const FExpeditionRun& Run, int32 Stage) { return Run.Progress().Phase == EExpeditionPhase::Route && Run.Progress().EncountersCompleted == Stage; }

/** A seed from 42 on whose scripted run (the Choice roads, the camp) reaches Done on a Compact run of Revision or an SRD run. */
int32 SeedReaching(int32 Revision, ERuleset Ruleset, TFunctionRef<int32(int32)> Choice, TFunctionRef<bool(const FExpeditionRun&)> Done)
{
    return FirstSeedWhere([&](int32 Candidate)
    {
        FExpeditionRun Probe; FString Error;
        return Probe.NewRun(Candidate, Error, TEXT("journey-probe"), EFighterSubclass::Champion, Ruleset, Revision) && PlayUntil(Probe, Choice, true, Done);
    }, 42, 200);
}

/** The query changes no state and no random stream (the snapshot holds both) and answers the same twice. */
bool PureQuery(FAutomationTestBase& Test, const FExpeditionRun& Run, const FString& Where)
{
    const FExpeditionSnapshot Before = Run.Snapshot();
    const TArray<FString> First = Lines(FJourneyQuery::For(Run)), Second = Lines(FJourneyQuery::For(Run.Progress()));
    const bool bSame = Test.TestTrue(Where + TEXT(": the query changes no state or random stream"), SameRun(Before, Run.Snapshot()));
    return Test.TestTrue(Where + TEXT(": it answers the same twice, from the run or from its progress"), First == Second) && bSame;
}

void ExpectStages(FAutomationTestBase& Test, const FExpeditionRun& Run, const TArray<FString>& Expected, const FString& Where)
{
    PureQuery(Test, Run, Where);
    const FJourney Journey = FJourneyQuery::For(Run);
    const TArray<FString> Actual = Lines(Journey);
    if (!Test.TestEqual(Where + TEXT(": five stages"), Actual.Num(), FExpeditionRun::EncounterCount)) { return; }
    for (int32 Index = 0; Index < Expected.Num(); ++Index) { Test.TestEqual(FString::Printf(TEXT("%s: stage %d"), *Where, Index), Actual[Index], Expected[Index]); }
    Test.TestEqual(Where + TEXT(": the Vault is the goal"), Journey.Goal, EExpeditionWorld::Vault);
    Test.TestEqual(Where + TEXT(": the phase"), Journey.Phase, Run.Progress().Phase);
}
}
using namespace JourneyQueryTest;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJourneyFreshRunTest, "Project.Expedition.Journey.FreshRun", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FJourneyFreshRunTest::RunTest(const FString& Parameters)
{
    FExpeditionRun Run; FString Error;
    if (!TestTrue(TEXT("A fresh run starts"), Run.NewRun(42, Error))) { return false; }
    ExpectStages(*this, Run, {FString(TEXT("0 Approach Current Unknown")) + Offers(0, true), Fogged(1), Fogged(2), Fogged(3), Fogged(4)}, TEXT("Fresh run"));
    const FJourney Journey = FJourneyQuery::For(Run);
    TestEqual(TEXT("The party is at the first stage"), Journey.CurrentIndex, 0);
    TestFalse(TEXT("No camp has been used"), Journey.bCampUsed);
    TestEqual(TEXT("No camp position"), Journey.CampAfterStage, static_cast<int32>(INDEX_NONE));
    TestEqual(TEXT("The kinds are known all the way to the finale"), FString::JoinBy(Journey.Stages, TEXT(","), [](const FJourneyStage& S) { return FString(Name(S.Kind)); }), FString(TEXT("Approach,Approach,Pressure,Pressure,Finale")));
    // The offers are the run's own, not a second roll: a fresh run from the same seed offers the same routes, and the query matches them.
    TestEqual(TEXT("The offers are the run's offers"), Journey.Stages[0].Offers.Num(), Run.Progress().Offers.Num());
    for (int32 Index = 0; Index < Run.Progress().Offers.Num(); ++Index)
    {
        TestEqual(TEXT("Offer id"), Journey.Stages[0].Offers[Index].Id, Run.Progress().Offers[Index].Id);
        TestEqual(TEXT("Offer danger"), Journey.Stages[0].Offers[Index].bDangerous, Run.Progress().Offers[Index].bDangerous);
    }
    // A default progress (nothing started) is safe: the first stage is current with no offers, the rest fogged.
    const FJourney Empty = FJourneyQuery::For(FExpeditionProgress());
    TestEqual(TEXT("An empty progress: first stage current, no offers"), Line(Empty.Stages[0]), FString(TEXT("0 Approach Current Unknown")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJourneyMidRunTest, "Project.Expedition.Journey.MidRunAndCamp", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FJourneyMidRunTest::RunTest(const FString& Parameters)
{
    const int32 Seed = SeedReaching(FBuildCatalog::Revision, ERuleset::Compact, DangerousRoads, [](const FExpeditionRun& R) { return AtRoute(R, 2); });
    if (!TestTrue(TEXT("A seed reaches the second route choice on the dangerous road"), Seed != INDEX_NONE)) { return false; }
    FExpeditionRun Run; FString Error;
    Run.NewRun(Seed, Error, TEXT("journey"));
    // The fight under way: the first stage is current with the road taken and no offers; the rest are fogged.
    TestTrue(TEXT("Choose the dangerous road"), Run.ChooseRoute(Run.Progress().Offers[1].Id, Error));
    {
        const FExpeditionEncounterRecord& Record = Run.Progress().History[0];
        ExpectStages(*this, Run, {FString::Printf(TEXT("0 Approach Current Vigil taken=2:Watchtower road:high:Fighting:%d"), Record.Survivors), Fogged(1), Fogged(2), Fogged(3), Fogged(4)}, TEXT("In the first fight"));
        TestEqual(TEXT("The first fight is the current stage"), FJourneyQuery::For(Run).CurrentIndex, 0);
    }
    TestTrue(TEXT("Win the fight"), FinishCombat(Run));
    TestEqual(TEXT("The reward is pending"), Run.Progress().Phase, EExpeditionPhase::Reward);
    // The reward is pending: the first stage is cleared; the next is current with no offers (they are rolled on arrival).
    ExpectStages(*this, Run, {Cleared(Run.Progress(), 0, true), FString(TEXT("1 Approach Current Unknown")), Fogged(2), Fogged(3), Fogged(4)}, TEXT("Reward pending"));
    TestTrue(TEXT("Claim the reward"), Step(Run, DangerousRoads, true));
    TestEqual(TEXT("The camp is pending"), Run.Progress().Phase, EExpeditionPhase::Recovery);
    ExpectStages(*this, Run, {Cleared(Run.Progress(), 0, true), FString(TEXT("1 Approach Current Unknown")), Fogged(2), Fogged(3), Fogged(4)}, TEXT("Camp pending"));
    TestFalse(TEXT("The camp is not yet used"), FJourneyQuery::For(Run).bCampUsed);
    // After the camp the second stage's offers are open and the camp sits after the first fight.
    FExpeditionRun Skipped = Run;
    TestTrue(TEXT("Take the camp"), Run.Recover(true, Error));
    ExpectStages(*this, Run, {Cleared(Run.Progress(), 0, true), FString(TEXT("1 Approach Current Unknown")) + Offers(1, true), Fogged(2), Fogged(3), Fogged(4)}, TEXT("After the camp"));
    const FJourney Camped = FJourneyQuery::For(Run);
    TestTrue(TEXT("The camp is used"), Camped.bCampUsed);
    TestEqual(TEXT("The camp sits after the first fight"), Camped.CampAfterStage, 0);
    TestEqual(TEXT("The second stage is current"), Camped.CurrentIndex, 1);
    // Skipping the camp is not a camp position.
    TestTrue(TEXT("Skip the camp"), Skipped.Recover(false, Error));
    const FJourney Declined = FJourneyQuery::For(Skipped);
    TestFalse(TEXT("A skipped camp is not used"), Declined.bCampUsed);
    TestEqual(TEXT("A skipped camp has no position"), Declined.CampAfterStage, static_cast<int32>(INDEX_NONE));
    TestTrue(TEXT("Otherwise the same journey"), Lines(Declined) == Lines(Camped));
    // The second fight is also on the dangerous road (the probe seed wins it).
    TestTrue(TEXT("Choose the dangerous road again"), Run.ChooseRoute(Run.Progress().Offers[1].Id, Error));
    TestTrue(TEXT("Win it"), FinishCombat(Run));
    ExpectStages(*this, Run, {Cleared(Run.Progress(), 0, true), Cleared(Run.Progress(), 1, true), FString(TEXT("2 Pressure Current Unknown")), Fogged(3), Fogged(4)}, TEXT("After two fights"));
    TestTrue(TEXT("The camp stays reported"), FJourneyQuery::For(Run).bCampUsed);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJourneyFinaleTest, "Project.Expedition.Journey.FinaleOfferAndVictory", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FJourneyFinaleTest::RunTest(const FString& Parameters)
{
    const int32 Seed = SeedReaching(FBuildCatalog::Revision, ERuleset::Compact, AlternatingRoads, [](const FExpeditionRun& R) { return R.Progress().Phase == EExpeditionPhase::Victory; });
    if (!TestTrue(TEXT("A seed wins the run on alternating roads"), Seed != INDEX_NONE)) { return false; }
    FExpeditionRun Run; FString Error;
    Run.NewRun(Seed, Error, TEXT("journey"));
    TestTrue(TEXT("Reach the finale's offers"), PlayUntil(Run, AlternatingRoads, true, [](const FExpeditionRun& R) { return AtRoute(R, 4); }));
    const FExpeditionProgress& P = Run.Progress();
    ExpectStages(*this, Run, {Cleared(P, 0, true), Cleared(P, 1, true), Cleared(P, 2, true), Cleared(P, 3, true), FString(TEXT("4 Finale Current Unknown")) + Offers(4, true)}, TEXT("Finale offer"));
    TestTrue(TEXT("The roads were alternating: guarded, dangerous, guarded, dangerous"), !P.History[0].Route.bDangerous && P.History[1].Route.bDangerous && !P.History[2].Route.bDangerous && P.History[3].Route.bDangerous);
    TestEqual(TEXT("The finale is current"), FJourneyQuery::For(Run).CurrentIndex, 4);
    // Both finale offers lead to the Vault, whichever road.
    const FJourney AtFinale = FJourneyQuery::For(Run);
    for (const FJourneyRoute& Offer : AtFinale.Stages[4].Offers) { TestEqual(TEXT("A finale offer is the Vault"), Offer.World, EExpeditionWorld::Vault); }
    TestTrue(TEXT("Choose the guarded finale"), Run.ChooseRoute(Run.Progress().Offers[0].Id, Error));
    ExpectStages(*this, Run, {Cleared(P, 0, true), Cleared(P, 1, true), Cleared(P, 2, true), Cleared(P, 3, true),
        FString::Printf(TEXT("4 Finale Current Vault taken=9:Artifact vault: Stream trail:high:Fighting:%d"), Run.Progress().History[4].Survivors)}, TEXT("Finale fight"));
    TestTrue(TEXT("Win the finale"), PlayUntil(Run, AlternatingRoads, true, [](const FExpeditionRun& R) { return R.Progress().Phase == EExpeditionPhase::Victory; }));
    ExpectStages(*this, Run, {Cleared(P, 0, true), Cleared(P, 1, true), Cleared(P, 2, true), Cleared(P, 3, true), Cleared(P, 4, true)}, TEXT("Victory"));
    const FJourney Won = FJourneyQuery::For(Run);
    TestEqual(TEXT("A won run has no current stage"), Won.CurrentIndex, static_cast<int32>(INDEX_NONE));
    TestTrue(TEXT("The camp was used"), Won.bCampUsed);
    for (const FJourneyStage& Stage : Won.Stages) { TestTrue(TEXT("No stage offers anything"), Stage.Offers.IsEmpty()); }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJourneyDefeatTest, "Project.Expedition.Journey.Defeat", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FJourneyDefeatTest::RunTest(const FString& Parameters)
{
    FString Error;
    // Lost in the first fight: the stage is Lost with its road and no survivors the party can use; everything after stays fogged, nothing is current.
    {
        FExpeditionRun Run;
        Run.NewRun(42, Error, TEXT("journey"));
        TestTrue(TEXT("Choose the guarded road"), Run.ChooseRoute(Run.Progress().Offers[0].Id, Error));
        TestTrue(TEXT("The party passes every turn and is beaten"), FinishCombat(Run, false));
        TestEqual(TEXT("The run is lost"), Run.Progress().Phase, EExpeditionPhase::Defeat);
        const int32 Survivors = Run.Progress().History[0].Survivors;
        ExpectStages(*this, Run, {FString::Printf(TEXT("0 Approach Lost Root taken=1:Stream trail:low:Defeat:%d"), Survivors), Fogged(1), Fogged(2), Fogged(3), Fogged(4)}, TEXT("Lost the first fight"));
        const FJourney Lost = FJourneyQuery::For(Run);
        TestEqual(TEXT("A lost run has no current stage"), Lost.CurrentIndex, static_cast<int32>(INDEX_NONE));
        TestFalse(TEXT("No camp"), Lost.bCampUsed);
    }
    // Lost in the third fight, after the camp: two cleared stages, the lost stage, then fog.
    const int32 Seed = SeedReaching(FBuildCatalog::Revision, ERuleset::Compact, DangerousRoads, [](const FExpeditionRun& R) { return AtRoute(R, 2); });
    if (!TestTrue(TEXT("A seed wins two fights on dangerous roads"), Seed != INDEX_NONE)) { return false; }
    FExpeditionRun Run;
    Run.NewRun(Seed, Error, TEXT("journey"));
    TestTrue(TEXT("Reach the third fight's offers"), PlayUntil(Run, DangerousRoads, true, [](const FExpeditionRun& R) { return AtRoute(R, 2); }));
    TestTrue(TEXT("Choose the guarded road"), Run.ChooseRoute(Run.Progress().Offers[0].Id, Error));
    TestTrue(TEXT("Lose it"), FinishCombat(Run, false));
    TestEqual(TEXT("The run is lost"), Run.Progress().Phase, EExpeditionPhase::Defeat);
    ExpectStages(*this, Run, {Cleared(Run.Progress(), 0, true), Cleared(Run.Progress(), 1, true),
        FString::Printf(TEXT("2 Pressure Lost Root taken=5:Stream trail:moderate:Defeat:%d"), Run.Progress().History[2].Survivors), Fogged(3), Fogged(4)}, TEXT("Lost the third fight"));
    const FJourney Lost = FJourneyQuery::For(Run);
    TestEqual(TEXT("No current stage"), Lost.CurrentIndex, static_cast<int32>(INDEX_NONE));
    TestTrue(TEXT("The camp stays reported"), Lost.bCampUsed && Lost.CampAfterStage == 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJourneyResumeTest, "Project.Expedition.Journey.ResumeRoundTrip", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FJourneyResumeTest::RunTest(const FString& Parameters)
{
    // At every step of a won run, and of a lost one, the journey of the run saved (strict JSON) and restored equals the journey of the run itself;
    // one checkpoint also goes through the disk and the session's Resume.
    const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation/Journey"), FGuid::NewGuid().ToString());
    const int32 WinSeed = SeedReaching(FBuildCatalog::Revision, ERuleset::Compact, AlternatingRoads, [](const FExpeditionRun& R) { return R.Progress().Phase == EExpeditionPhase::Victory; });
    if (!TestTrue(TEXT("A seed wins on alternating roads"), WinSeed != INDEX_NONE)) { return false; }
    int32 Checkpoints = 0;
    for (const bool bLose : {false, true})
    {
        FExpeditionRun Run; FString Error;
        Run.NewRun(WinSeed, Error, TEXT("journey"));
        for (int32 Guard = 0; Guard < 80; ++Guard)
        {
            FExpeditionSaveData Data, Loaded; FString Json;
            Data.Current = Run.Snapshot();
            if (Run.Progress().Phase == EExpeditionPhase::Victory || Run.Progress().Phase == EExpeditionPhase::Defeat) { Data.Records.Add(FExpeditionSave::Ending(Run)); }
            FExpeditionRun Restored;
            const FString Where = FString::Printf(TEXT("%s checkpoint %d (phase %d)"), bLose ? TEXT("Lost run") : TEXT("Won run"), Checkpoints, static_cast<int32>(Run.Progress().Phase));
            if (!TestTrue(Where + TEXT(": saves and restores ") + Error, FExpeditionSave::Encode(Data, Json, Error) && FExpeditionSave::Decode(Json, Loaded, Error) && Restored.Restore(Loaded.Current, Error))) { return false; }
            TestTrue(Where + TEXT(": the restored journey equals the live one"), Lines(FJourneyQuery::For(Restored)) == Lines(FJourneyQuery::For(Run)));
            TestEqual(Where + TEXT(": camp"), FJourneyQuery::For(Restored).CampAfterStage, FJourneyQuery::For(Run).CampAfterStage);
            TestEqual(Where + TEXT(": current stage"), FJourneyQuery::For(Restored).CurrentIndex, FJourneyQuery::For(Run).CurrentIndex);
            PureQuery(*this, Restored, Where + TEXT(" (restored)"));
            if (Run.Progress().Phase != EExpeditionPhase::Combat && Run.Progress().Phase != EExpeditionPhase::Victory && Run.Progress().Phase != EExpeditionPhase::Defeat && Checkpoints % 3 == 0)
            {
                const FString Path = FPaths::Combine(Directory, FString::FromInt(Checkpoints), TEXT("current.json"));
                FExpeditionSaveData OnDisk; OnDisk.Current = Run.Snapshot();
                if (TestTrue(Where + TEXT(": written ") + Error, FExpeditionSave::Write(Path, OnDisk, Error)))
                {
                    FExpeditionSession Session(Path);
                    if (TestTrue(Where + TEXT(": resumes ") + Error, Session.Resume(Error))) { TestTrue(Where + TEXT(": the resumed journey equals the live one"), Lines(FJourneyQuery::For(Session.Run())) == Lines(FJourneyQuery::For(Run))); }
                }
            }
            ++Checkpoints;
            const EExpeditionPhase Phase = Run.Progress().Phase;
            if (Phase == EExpeditionPhase::Victory || Phase == EExpeditionPhase::Defeat) { break; }
            // The lost run wins its first fight and passes every turn of the second.
            if (bLose && Phase == EExpeditionPhase::Combat && Run.Progress().EncountersCompleted == 1) { FinishCombat(Run, false); continue; }
            if (!Step(Run, AlternatingRoads, true)) { return false; }
        }
        TestEqual(TEXT("The run ended as planned"), Run.Progress().Phase, bLose ? EExpeditionPhase::Defeat : EExpeditionPhase::Victory);
    }
    IFileManager::Get().DeleteDirectory(*Directory, false, true);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJourneyRevisionsTest, "Project.Expedition.Journey.EveryRevisionAndRuleset", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FJourneyRevisionsTest::RunTest(const FString& Parameters)
{
    // Revisions 7-10 (Compact) and an SRD run, each played to the finale's offers on alternating roads. The world of every road is the same in every
    // revision (the rules' stage and road); the risk word exists from revision 9 on Compact runs only (older route cards never showed one).
    struct FCase { int32 Revision; ERuleset Ruleset; };
    const FCase Cases[] = {{7, ERuleset::Compact}, {8, ERuleset::Compact}, {9, ERuleset::Compact}, {10, ERuleset::Compact}, {FBuildCatalog::Revision, ERuleset::SRD}};
    for (const FCase& Case : Cases)
    {
        const FString Which = FString::Printf(TEXT("Revision %d %s"), Case.Revision, Case.Ruleset == ERuleset::SRD ? TEXT("SRD") : TEXT("Compact"));
        const bool bRisk = Case.Ruleset == ERuleset::Compact && Case.Revision >= 9;
        const int32 Seed = SeedReaching(Case.Revision, Case.Ruleset, AlternatingRoads, [](const FExpeditionRun& R) { return AtRoute(R, 4); });
        if (!TestTrue(Which + TEXT(": a seed reaches the finale's offers"), Seed != INDEX_NONE)) { continue; }
        FExpeditionRun Run; FString Error;
        Run.NewRun(Seed, Error, TEXT("journey"), EFighterSubclass::Champion, Case.Ruleset, Case.Revision);
        TestEqual(Which + TEXT(": keeps its revision"), Run.Progress().CatalogRevision, Case.Revision);
        ExpectStages(*this, Run, {FString(TEXT("0 Approach Current Unknown")) + Offers(0, bRisk), Fogged(1), Fogged(2), Fogged(3), Fogged(4)}, Which + TEXT(" fresh"));
        TestTrue(Which + TEXT(": second route choice"), PlayUntil(Run, AlternatingRoads, true, [](const FExpeditionRun& R) { return AtRoute(R, 1); }));
        ExpectStages(*this, Run, {Cleared(Run.Progress(), 0, bRisk), FString(TEXT("1 Approach Current Unknown")) + Offers(1, bRisk), Fogged(2), Fogged(3), Fogged(4)}, Which + TEXT(" after fight one"));
        TestTrue(Which + TEXT(": the camp was used"), FJourneyQuery::For(Run).bCampUsed && FJourneyQuery::For(Run).CampAfterStage == 0);
        TestTrue(Which + TEXT(": finale offers"), PlayUntil(Run, AlternatingRoads, true, [](const FExpeditionRun& R) { return AtRoute(R, 4); }));
        const FExpeditionProgress& P = Run.Progress();
        ExpectStages(*this, Run, {Cleared(P, 0, bRisk), Cleared(P, 1, bRisk), Cleared(P, 2, bRisk), Cleared(P, 3, bRisk), FString(TEXT("4 Finale Current Unknown")) + Offers(4, bRisk)}, Which + TEXT(" finale offer"));
        // The same journey from the saved fields alone: a restored run answers identically.
        FExpeditionRun Restored;
        TestTrue(Which + TEXT(": restores"), Restored.Restore(Run.Snapshot(), Error));
        TestTrue(Which + TEXT(": the restored journey is the same"), Lines(FJourneyQuery::For(Restored)) == Lines(FJourneyQuery::For(Run)));
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJourneyWorldTest, "Project.Expedition.Journey.WorldIsTheRulesMapping", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FJourneyWorldTest::RunTest(const FString& Parameters)
{
    // The one mapping presentation and the query share (FEncounterIdentity::World).
    for (const bool bDangerous : {false, true})
    {
        TestEqual(TEXT("Approach world"), FExpeditionRun::WorldOf(EEncounterStage::Approach, bDangerous), bDangerous ? EExpeditionWorld::Vigil : EExpeditionWorld::Root);
        TestEqual(TEXT("Pressure world"), FExpeditionRun::WorldOf(EEncounterStage::Pressure, bDangerous), bDangerous ? EExpeditionWorld::Vigil : EExpeditionWorld::Root);
        TestEqual(TEXT("Finale world"), FExpeditionRun::WorldOf(EEncounterStage::Finale, bDangerous), EExpeditionWorld::Vault);
    }
    FExpeditionRun Run; FString Error;
    Run.NewRun(42, Error);
    for (const FExpeditionRoute& Offer : Run.Progress().Offers)
    {
        const FEncounterIdentity Identity = FExpeditionRun::Encounter(Run.Progress(), Offer);
        TestEqual(TEXT("An offer's identity carries its world"), Identity.World, Offer.bDangerous ? EExpeditionWorld::Vigil : EExpeditionWorld::Root);
        TestEqual(TEXT("and the risk word of the route card"), Identity.Risk, Offer.bDangerous ? ERouteRisk::High : ERouteRisk::Low);
        TestTrue(TEXT("whose text is the word the preview shows"), Offer.Preview.Contains(FString(TEXT("Risk: ")) + FExpeditionRun::RiskWord(Identity.Risk)));
    }
    return true;
}
#endif
