#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Run/ExpeditionRun.h"
#include "Persistence/ExpeditionSave.h"
#include "Misc/FileHelper.h"
#include "ExpeditionTestSupport.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

using namespace ExpeditionTestSupport;

namespace
{
/** Compact battle fingerprint: does not round-trip the save (Victory encodes are covered elsewhere). */
FString CompactBattleFingerprint(const FExpeditionSnapshot& Snapshot)
{
    int32 PartyHp = 0, EnemyHp = 0, PartyAlive = 0;
    for (const auto& Unit : Snapshot.Battle.Encounter.Combatants)
    {
        if (Unit.Side == ECombatSide::Party) { PartyHp += Unit.HP; if (Unit.Life == ECombatLife::Conscious) { ++PartyAlive; } }
        else { EnemyHp += Unit.HP; }
    }
    return FString::Printf(TEXT("rng=%d|partyHp=%d|enemyHp=%d|alive=%d|phase=%d|victories=%d|ruleset=%d|rounds=%d|turn=%d"),
        Snapshot.Battle.RandomState, PartyHp, EnemyHp, PartyAlive,
        static_cast<int32>(Snapshot.Progress.Phase), Snapshot.Progress.EncountersCompleted,
        static_cast<int32>(Snapshot.Progress.Ruleset), Snapshot.Battle.Round, Snapshot.Battle.TurnIndex);
}
}

// Real format 4 files from staging d9eac88, catalog revision 7. A revision 7 run keeps revision 7 enemies for its whole
// lifetime (#184), so both still load as Compact and resume exactly as staging continued them.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSchema4LoadsAsCompactTest, "Project.Expedition.Persistence.Schema4LoadsAsCompact",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSchema4LoadsAsCompactTest::RunTest(const FString& Parameters)
{
    FString Error;
    const FString Fixtures = FPaths::ProjectDir() / TEXT("Source/DnDDiceRoll/Tests/Fixtures/Schema4");
    FExpeditionSaveData Expected;
    if (!TestTrue(TEXT("Read staging expected victory"), FExpeditionSave::Read(Fixtures / TEXT("expected-victory-v4.json"), Expected, Error))) { return false; }
    FExpeditionSaveData ExpectedMid;
    if (!TestTrue(TEXT("Read staging expected mid continuation"), FExpeditionSave::Read(Fixtures / TEXT("expected-mid-victory-v4.json"), ExpectedMid, Error))) { return false; }
    for (const TCHAR* Name : {TEXT("mid-battle-v4.json"), TEXT("between-battles-v4.json")})
    {
        const bool bMid = FCString::Strcmp(Name, TEXT("mid-battle-v4.json")) == 0;
        FExpeditionSaveData Loaded;
        if (!TestTrue(Name, FExpeditionSave::Read(Fixtures / Name, Loaded, Error))) { return false; }
        TestEqual(TEXT("Version upgraded"), Loaded.Version, FExpeditionSave::Version);
        TestTrue(TEXT("Progress and encounter are Compact"), Loaded.Current.Progress.Ruleset == ERuleset::Compact && Loaded.Current.Battle.Encounter.Ruleset == ERuleset::Compact);
        for (const auto& U : Loaded.Current.Battle.Encounter.Combatants)
        {
            TestTrue(TEXT("Combatant defaults"), U.bReactionAvailable && U.DeathSaveSuccesses == 0 && U.DeathSaveFailures == 0 && U.TemporaryHP == 0 &&
                !U.bConcentrating && U.Size == 2 && !U.bSurprised && !U.bDisengaged && !U.bProne && U.GrapplerId == INDEX_NONE);
        }
        TestTrue(TEXT("Encounter defaults"), !Loaded.Current.Battle.Encounter.bAmbush && Loaded.Current.Battle.Encounter.AmbushStealthTotals.IsEmpty());
        FExpeditionRun Run;
        if (!TestTrue(TEXT("Restore migrated snapshot"), Run.Restore(Loaded.Current, Error))) { return false; }
        TestTrue(TEXT("Restore consumes no RNG or state"), SameRun(Loaded.Current, Run.Snapshot()));
        for (int32 Guard = 0; Guard < 30 && Run.Progress().Phase != EExpeditionPhase::Victory && Run.Progress().Phase != EExpeditionPhase::Defeat; ++Guard)
        {
            switch (Run.Progress().Phase)
            {
            case EExpeditionPhase::Route: TestTrue(TEXT("Choose"), Run.ChooseRoute(Run.Progress().Offers[0].Id, Error)); break;
            case EExpeditionPhase::Combat: TestTrue(TEXT("Finish combat"), FinishCombat(Run)); break;
            case EExpeditionPhase::Reward: TestTrue(TEXT("Reward"), Run.ClaimReward(1, Error)); break;
            case EExpeditionPhase::Recovery: TestTrue(TEXT("Recover"), Run.Recover(true, Error)); break;
            default: return false;
            }
        }
        TestTrue(TEXT("Full resumed snapshot equals staging continuation, including RNG"), SameRun((bMid ? ExpectedMid : Expected).Current, Run.Snapshot()));
    }
    return !HasAnyErrors();
}

// The 4 -> 5 upgrade on current runs (#184; since #188 revision 8 and revision 9 runs): a run written as format 4 (the fields format 5 added
// removed) decodes with the Compact defaults, restores exactly and continues identically. The fixtures above cover
// real format 4 files, which are revision 7.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSchema4UpgradeRoundTripTest, "Project.Expedition.Persistence.Schema4UpgradeRoundTripsCurrentRuns",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSchema4UpgradeRoundTripTest::RunTest(const FString& Parameters)
{
    FString Error;
    // A revision 8 run and a current-revision run (9 since #188), each mid-battle (after enemy turns and one party move) and
    // between battles (reward and camp taken).
    TArray<FExpeditionRun> Runs;
    for (const int32 Revision : {8, FBuildCatalog::Revision})
    {
        FExpeditionRun Mid; Mid.NewRun(42, Error, TEXT("seeded-fixture"), EFighterSubclass::Champion, ERuleset::Compact, Revision);
        Mid.ChooseRoute(Mid.Progress().Offers[0].Id, Error);
        while (Mid.Battle().Find(Mid.Battle().ActiveId())->Side == ECombatSide::Enemy) { Mid.PlayEnemyTurn(); }
        const int32 Mover = Mid.Battle().ActiveId();
        const TArray<FIntPoint> Moves = Mid.Battle().LegalMoves(Mover);
        if (TestFalse(TEXT("The first hero can move"), Moves.IsEmpty())) { TestTrue(TEXT("One party move"), Mid.Submit({ECombatCommandType::Move, Mover, Moves[0]}).bAccepted); }
        FExpeditionRun Between; Between.NewRun(42, Error, TEXT("seeded-fixture"), EFighterSubclass::Champion, ERuleset::Compact, Revision);
        Between.ChooseRoute(Between.Progress().Offers[0].Id, Error);
        if (!TestTrue(TEXT("First encounter won"), FinishCombat(Between) && Between.Progress().Phase == EExpeditionPhase::Reward)) { return false; }
        TestTrue(TEXT("Reward"), Between.ClaimReward(1, Error)); TestTrue(TEXT("Camp"), Between.Recover(true, Error));
        TestEqual(TEXT("Mid-battle run keeps its revision"), Mid.Progress().CatalogRevision, Revision);
        TestEqual(TEXT("Between-battles run keeps its revision"), Between.Progress().CatalogRevision, Revision);
        Runs.Add(Mid); Runs.Add(Between);
    }
    for (FExpeditionRun& Run : Runs)
    {
        FExpeditionRun* Original = &Run;
        FExpeditionSaveData Data; Data.Current = Original->Snapshot();
        FString Json;
        if (!TestTrue(TEXT("Encode format 5"), FExpeditionSave::Encode(Data, Json, Error))) { return false; }
        TSharedPtr<FJsonObject> Object;
        if (!TestTrue(TEXT("Parse"), FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Object) && Object.IsValid())) { return false; }
        // Write it as format 4: the version, and none of the fields the 4 -> 5 upgrade adds.
        Object->SetNumberField(TEXT("Version"), 4);
        const TSharedPtr<FJsonObject> Current = Object->GetObjectField(TEXT("Current"));
        Current->GetObjectField(TEXT("Progress"))->RemoveField(TEXT("Ruleset"));
        const TSharedPtr<FJsonObject> Encounter = Current->GetObjectField(TEXT("Battle"))->GetObjectField(TEXT("Encounter"));
        for (const TCHAR* Field : {TEXT("Ruleset"), TEXT("bAmbush"), TEXT("AmbushStealthTotals")}) { Encounter->RemoveField(Field); }
        for (const auto& Unit : Encounter->GetArrayField(TEXT("Combatants")))
        {
            for (const TCHAR* Field : {TEXT("bReactionAvailable"), TEXT("DeathSaveSuccesses"), TEXT("DeathSaveFailures"), TEXT("TemporaryHP"), TEXT("bConcentrating"),
                TEXT("Size"), TEXT("bSurprised"), TEXT("bDisengaged"), TEXT("bProne"), TEXT("GrapplerId")}) { Unit->AsObject()->RemoveField(Field); }
        }
        FString Format4; FJsonSerializer::Serialize(Object.ToSharedRef(), TJsonWriterFactory<>::Create(&Format4));
        FExpeditionSaveData Loaded;
        if (!TestTrue(TEXT("Format 4 document decodes through the upgrade: ") + Error, FExpeditionSave::Decode(Format4, Loaded, Error))) { return false; }
        TestEqual(TEXT("Version upgraded"), Loaded.Version, FExpeditionSave::Version);
        TestTrue(TEXT("Progress and encounter are Compact"), Loaded.Current.Progress.Ruleset == ERuleset::Compact && Loaded.Current.Battle.Encounter.Ruleset == ERuleset::Compact);
        for (const auto& U : Loaded.Current.Battle.Encounter.Combatants)
        {
            TestTrue(TEXT("Combatant defaults"), U.bReactionAvailable && U.DeathSaveSuccesses == 0 && U.DeathSaveFailures == 0 && U.TemporaryHP == 0 &&
                !U.bConcentrating && U.Size == 2 && !U.bSurprised && !U.bDisengaged && !U.bProne && U.GrapplerId == INDEX_NONE);
        }
        TestTrue(TEXT("Encounter defaults"), !Loaded.Current.Battle.Encounter.bAmbush && Loaded.Current.Battle.Encounter.AmbushStealthTotals.IsEmpty());
        TestTrue(TEXT("Upgraded snapshot equals the run, RNG included"), SameRun(Loaded.Current, Original->Snapshot()));
        FExpeditionRun Resumed;
        if (!TestTrue(TEXT("Restore upgraded snapshot"), Resumed.Restore(Loaded.Current, Error))) { return false; }
        FinishRunScripted(*Original); FinishRunScripted(Resumed);
        TestTrue(TEXT("The resumed run continues exactly as the original, RNG included"), SameRun(Original->Snapshot(), Resumed.Snapshot()));
    }
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRulesetPersistsAndDefaultsTest, "Project.Expedition.Persistence.RulesetPersistsAndDefaultsCompact",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRulesetPersistsAndDefaultsTest::RunTest(const FString& Parameters)
{
    FExpeditionRun CompactRun, SrdRun; FString Error, Json;
    TestTrue(TEXT("Default NewRun is Compact"), CompactRun.NewRun(7, Error, TEXT("ruleset-default")));
    TestTrue(TEXT("Default ruleset Compact"), CompactRun.Progress().Ruleset == ERuleset::Compact);
    TestTrue(TEXT("Explicit SRD NewRun"), SrdRun.NewRun(7, Error, TEXT("ruleset-srd"), EFighterSubclass::Champion, ERuleset::SRD));
    TestTrue(TEXT("SRD ruleset stored"), SrdRun.Progress().Ruleset == ERuleset::SRD);

    FExpeditionSaveData Data; Data.Current = SrdRun.Snapshot();
    TestTrue(TEXT("Encode SRD"), FExpeditionSave::Encode(Data, Json, Error));
    FExpeditionSaveData Loaded;
    TestTrue(TEXT("Decode SRD"), FExpeditionSave::Decode(Json, Loaded, Error));
    TestTrue(TEXT("SRD survives roundtrip"), Loaded.Current.Progress.Ruleset == ERuleset::SRD);

    TSharedPtr<FJsonObject> Object;
    if (!TestTrue(TEXT("SRD JSON parses"), FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Object) && Object.IsValid())) { return false; }
    Object->SetNumberField(TEXT("Version"), FExpeditionSave::Version + 1);
    FString Future; FJsonSerializer::Serialize(Object.ToSharedRef(), TJsonWriterFactory<>::Create(&Future));
    FExpeditionSaveData FutureData = Data;
    TestFalse(TEXT("Future schema rejected"), FExpeditionSave::Decode(Future, FutureData, Error));
    TestTrue(TEXT("Future decode leaves output unchanged"), FExpeditionSaveData::StaticStruct()->CompareScriptStruct(&Data, &FutureData, 0));
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCompactSeedRegressionTest, "Project.Expedition.Run.CompactSeedIdenticalFingerprint",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FCompactSeedRegressionTest::RunTest(const FString& Parameters)
{
    // Compact full-run seed 42 — same policy as FiveVictoriesRecoverArtifact.
    // Proves the Compact path still reaches Victory and pins its battle math to a golden fingerprint.
    // A revision 8 run, created explicitly since revision 9 (#188): the golden below is unchanged, which proves the revision 8
    // path is byte-identical. Project.Expedition.Run.CompactRevision9SeedFingerprint pins the revision 9 run of the same seed.
    FExpeditionRun Run; FString Error;
    TestTrue(TEXT("Compact NewRun"), Run.NewRun(42, Error, TEXT("seeded-fixture"), EFighterSubclass::Champion, ERuleset::Compact, 8));
    TestTrue(TEXT("Ruleset Compact"), Run.Progress().Ruleset == ERuleset::Compact);
    for (int32 Encounter = 0; Encounter < 5; ++Encounter)
    {
        TestTrue(TEXT("Choose route"), Run.ChooseRoute(Run.Progress().Offers[0].Id, Error));
        if (!TestTrue(TEXT("Finish combat"), FinishCombat(Run))) { return false; }
        if (Encounter < 4)
        {
            if (!TestTrue(TEXT("Reward phase"), Run.Progress().Phase == EExpeditionPhase::Reward)) { return false; }
            TestTrue(TEXT("Claim reward"), Run.ClaimReward(1, Error));
            if (Encounter == 0)
            {
                TestTrue(TEXT("Camp phase"), Run.Progress().Phase == EExpeditionPhase::Recovery);
                TestTrue(TEXT("Recover"), Run.Recover(true, Error));
            }
        }
    }
    TestTrue(TEXT("Victory"), Run.Progress().Phase == EExpeditionPhase::Victory && Run.Progress().bArtifactRecovered);
    TestEqual(TEXT("Five victories"), Run.Progress().EncountersCompleted, 5);

    const FString Fingerprint = CompactBattleFingerprint(Run.Snapshot());
    TestFalse(TEXT("Fingerprint produced"), Fingerprint.IsEmpty());
    // Golden Compact fingerprint for seed 42 / FinishCombat / ClaimReward(1) / Recover(true), catalog revision 8.
    // Recaptured deliberately for the #184 Compact enemy tuning (stronger enemies: the scripted party now ends with three
    // survivors and 36 HP after a 10-round finale). The revision 7 golden, from staging d9eac88, was
    // "rng=-958489057|partyHp=82|enemyHp=0|alive=4|phase=4|victories=5|ruleset=0|rounds=4|turn=4".
    // SRD runs keep their numbers; Project.Expedition.Run.SrdSeedUnchangedByCompactTuning pins one.
    const FString Golden = TEXT("rng=-1674769977|partyHp=36|enemyHp=0|alive=3|phase=4|victories=5|ruleset=0|rounds=10|turn=1");
    TestEqual(TEXT("Compact fingerprint matches staging-shared golden"), Fingerprint, Golden);
    FExpeditionSaveData VictorySave; VictorySave.Current = Run.Snapshot();
    VictorySave.Records.Add(FExpeditionSave::Ending(Run));
    FString VictoryJson, VictoryError;
    FExpeditionRun VictoryRestored;
    const bool bRestored = VictoryRestored.Restore(Run.Snapshot(), VictoryError);
    TestTrue(FString::Printf(TEXT("Victory Restore: %s"), *VictoryError), bRestored);
    const bool bEncoded = FExpeditionSave::Encode(VictorySave, VictoryJson, VictoryError);
    TestTrue(FString::Printf(TEXT("Victory Encode: %s"), *VictoryError), bEncoded);
    FExpeditionSaveData BareVictory; BareVictory.Current = Run.Snapshot();
    FString BareJson, BareError;
    TestFalse(TEXT("Victory Encode without Ending records fails"), FExpeditionSave::Encode(BareVictory, BareJson, BareError));
    TestTrue(TEXT("Bare Victory Encode names incomplete ending records"), BareError.Contains(TEXT("ending records")));
    FExpeditionRun Twin; FString TwinError;
    TestTrue(TEXT("Twin Compact NewRun"), Twin.NewRun(42, TwinError, TEXT("seeded-fixture"), EFighterSubclass::Champion, ERuleset::Compact, 8));
    for (int32 Encounter = 0; Encounter < 5; ++Encounter)
    {
        TestTrue(TEXT("Twin choose"), Twin.ChooseRoute(Twin.Progress().Offers[0].Id, TwinError));
        if (!TestTrue(TEXT("Twin finish"), FinishCombat(Twin))) { return false; }
        if (Encounter < 4)
        {
            TestTrue(TEXT("Twin reward"), Twin.ClaimReward(1, TwinError));
            if (Encounter == 0) { TestTrue(TEXT("Twin recover"), Twin.Recover(true, TwinError)); }
        }
    }
    TestEqual(TEXT("Twin Compact fingerprint (seed identity)"), Fingerprint, CompactBattleFingerprint(Twin.Snapshot()));
    TestTrue(TEXT("Twin snapshots byte-identical"), SameRun(Run.Snapshot(), Twin.Snapshot()));
    return !HasAnyErrors();
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSrdSeedUnchangedTest, "Project.Expedition.Run.SrdSeedUnchangedByCompactTuning",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSrdSeedUnchangedTest::RunTest(const FString& Parameters)
{
    // #184 tunes Compact only. An SRD run of seed 42 with the scripted player must end exactly as it did under catalog
    // revision 7 (golden captured on staging bd02ede with this same test), and its enemies keep the revision 7 stats.
    FExpeditionRun Run; FString Error;
    if (!TestTrue(TEXT("SRD NewRun"), Run.NewRun(42, Error, TEXT("srd-fixture"), EFighterSubclass::Champion, ERuleset::SRD))) { return false; }
    TestTrue(TEXT("SRD first route"), Run.ChooseRoute(Run.Progress().Offers[1].Id, Error));
    for (const auto& U : Run.Battle().Snapshot().Encounter.Combatants)
    {
        if (U.Side != ECombatSide::Enemy) { continue; }
        TestTrue(TEXT("SRD Dangerous fight 1 enemy keeps the revision 7 stats"), U.MaxHP == 12 && U.ArmorClass == 12 && U.AttackBonus == 2 && U.DamageDie == 6 && U.DamageBonus == 1);
    }
    FinishRunScripted(Run);
    TestEqual(TEXT("SRD fingerprint unchanged by the Compact tuning"), CompactBattleFingerprint(Run.Snapshot()), FString(TEXT("rng=-2011953773|partyHp=82|enemyHp=0|alive=4|phase=4|victories=5|ruleset=1|rounds=8|turn=0")));
    return !HasAnyErrors();
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSrdRevision7ResumeTest, "Project.Expedition.Persistence.SrdRevision7Resumes",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSrdRevision7ResumeTest::RunTest(const FString& Parameters)
{
    // An SRD checkpoint saved under catalog revision 7 resumes through the session and plays exactly as a current-revision SRD
    // run (#184, #188: SRD numbers and generation are the same in every revision); it keeps its revision.
    FString Error;
    FExpeditionRun Original;
    if (!TestTrue(TEXT("SRD NewRun"), Original.NewRun(42, Error, TEXT("srd-revision-7"), EFighterSubclass::Champion, ERuleset::SRD))) { return false; }
    TestTrue(TEXT("Route"), Original.ChooseRoute(Original.Progress().Offers[1].Id, Error));
    while (Original.Battle().Find(Original.Battle().ActiveId())->Side == ECombatSide::Enemy) { Original.PlayEnemyTurn(); }
    const int32 Mover = Original.Battle().ActiveId();
    const TArray<FIntPoint> Moves = Original.Battle().LegalMoves(Mover);
    if (!Moves.IsEmpty()) { Original.Submit({ECombatCommandType::Move, Mover, Moves[0]}); }
    FExpeditionSaveData Data; Data.Current = Original.Snapshot(); Data.Current.Progress.CatalogRevision = 7;
    const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation/SrdRevision7"), FGuid::NewGuid().ToString());
    const FString Path = FPaths::Combine(Directory, TEXT("current-v1.json"));
    if (!TestTrue(TEXT("Write a revision 7 SRD checkpoint: ") + Error, FExpeditionSave::Write(Path, Data, Error))) { return false; }
    FExpeditionSession Session(Path);
    if (!TestTrue(TEXT("Resume the revision 7 SRD checkpoint: ") + Error, Session.Resume(Error))) { IFileManager::Get().DeleteDirectory(*Directory, false, true); return false; }
    TestEqual(TEXT("The run keeps revision 7"), Session.Run().Progress().CatalogRevision, 7);
    TestTrue(TEXT("Resumed exactly, RNG included"), SameRun(Data.Current, Session.Run().Snapshot()));
    FExpeditionRun Resumed = Session.Run();
    FinishRunScripted(Original); FinishRunScripted(Resumed);
    TestEqual(TEXT("Still revision 7 at the end"), Resumed.Progress().CatalogRevision, 7);
    FExpeditionSnapshot Compared = Resumed.Snapshot(); Compared.Progress.CatalogRevision = Original.Progress().CatalogRevision;
    TestTrue(TEXT("Revision 7 and current-revision SRD runs play identically"), SameRun(Original.Snapshot(), Compared));
    IFileManager::Get().DeleteDirectory(*Directory, false, true);
    return !HasAnyErrors();
}
#endif
