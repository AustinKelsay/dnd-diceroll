#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Run/ExpeditionRun.h"
#include "Persistence/ExpeditionSave.h"
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSchema4LoadsAsCompactTest, "Project.Expedition.Persistence.Schema4LoadsAsCompact",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSchema4LoadsAsCompactTest::RunTest(const FString& Parameters)
{
    FExpeditionRun Run; FString Error, Json;
    TestTrue(TEXT("Start compact run"), Run.NewRun(42, Error, TEXT("schema4-migrate"), EFighterSubclass::Champion, ERuleset::Compact));
    FExpeditionSaveData Data; Data.Current = Run.Snapshot();
    TestTrue(TEXT("Encode schema 5"), FExpeditionSave::Encode(Data, Json, Error));
    TestEqual(TEXT("Encoder writes version 5"), Data.Version, 5);

    TSharedPtr<FJsonObject> Object;
    FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Object);
    Object->SetNumberField(TEXT("Version"), 4);
    Object->GetObjectField(TEXT("Current"))->GetObjectField(TEXT("Progress"))->RemoveField(TEXT("Ruleset"));
    FString Schema4;
    FJsonSerializer::Serialize(Object.ToSharedRef(), TJsonWriterFactory<>::Create(&Schema4));

    FExpeditionSaveData Loaded;
    TestTrue(TEXT("Schema 4 decodes"), FExpeditionSave::Decode(Schema4, Loaded, Error));
    TestEqual(TEXT("Migrated version is 5"), Loaded.Version, 5);
    TestTrue(TEXT("Schema 4 becomes Compact"), Loaded.Current.Progress.Ruleset == ERuleset::Compact);
    FExpeditionRun Restored;
    TestTrue(TEXT("Migrated snapshot restores"), Restored.Restore(Loaded.Current, Error));
    TestTrue(TEXT("Restored ruleset Compact"), Restored.Progress().Ruleset == ERuleset::Compact);
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
    FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Object);
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
    // Proves Compact path still reaches Victory; fingerprint must stay stable vs staging c28b9cf battle math.
    FExpeditionRun Run; FString Error;
    TestTrue(TEXT("Compact NewRun"), Run.NewRun(42, Error, TEXT("seeded-fixture"), EFighterSubclass::Champion, ERuleset::Compact));
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
    // Golden Compact fingerprint for seed 42 / FinishCombat / ClaimReward(1) / Recover(true).
    // Captured on tip sharing Compact battle math with staging c28b9cf (Rules libraries unused in Compact combat).
    // Re-capture on a c28b9cf worktree if FinishCombat or Compact combat changes.
    const FString Golden = TEXT("rng=-958489057|partyHp=82|enemyHp=0|alive=4|phase=4|victories=5|ruleset=0|rounds=4|turn=4");
    TestEqual(TEXT("Compact fingerprint matches staging-shared golden"), Fingerprint, Golden);
    FExpeditionSaveData VictorySave; VictorySave.Current = Run.Snapshot();
    VictorySave.Records.Add(FExpeditionSave::Ending(Run));
    FString VictoryJson, VictoryError;
    FExpeditionRun VictoryRestored;
    const bool bRestored = VictoryRestored.Restore(Run.Snapshot(), VictoryError);
    TestTrue(FString::Printf(TEXT("Victory Restore: %s"), *VictoryError), bRestored);
    const bool bEncoded = FExpeditionSave::Encode(VictorySave, VictoryJson, VictoryError);
    TestTrue(FString::Printf(TEXT("Victory Encode: %s"), *VictoryError), bEncoded);
    FExpeditionRun Twin; FString TwinError;
    TestTrue(TEXT("Twin Compact NewRun"), Twin.NewRun(42, TwinError, TEXT("seeded-fixture"), EFighterSubclass::Champion, ERuleset::Compact));
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
#endif
