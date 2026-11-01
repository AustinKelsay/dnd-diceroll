#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Party/BuildCatalog.h"
#include "Persistence/ExpeditionSave.h"
#include "ExpeditionTestSupport.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBuildCatalogTest, "Project.Expedition.Catalog.CurrentProfiles", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBuildCatalogTest::RunTest(const FString& Parameters)
{
    FString Error;
    FCombatantState Hero;
    TestTrue(TEXT("Build accepts independent instance identity"), FBuildCatalog::Create(TEXT("fighter.battle-master"), 4, TEXT("Nix"), Hero));
    TestEqual(TEXT("Fighter HP"), Hero.MaxHP, 30);
    TestEqual(TEXT("Fighter damage"), Hero.DamageBonus, 3);
    TestEqual(TEXT("Two maneuvers"), Hero.ManeuverUses, 2);
    TestEqual(TEXT("Two specials"), FBuildCatalog::Find(Hero.BuildId)->Specials.Num(), 2);
    TestNull(TEXT("Invalid class/subclass pair"), FBuildCatalog::Find(TEXT("wizard.champion")));
    auto Before = Hero;
    TestFalse(TEXT("Invalid construction"), FBuildCatalog::Create(TEXT("wizard.champion"), 4, TEXT("Nix"), Hero));
    TestTrue(TEXT("Invalid construction installs nothing"), FCombatantState::StaticStruct()->CompareScriptStruct(&Before, &Hero, 0));
    FExpeditionRun Run;
    TestTrue(TEXT("New run"), Run.NewRun(42, Error, TEXT("catalog"), EFighterSubclass::BattleMaster));
    TestTrue(TEXT("First route"), Run.ChooseRoute(Run.Progress().Offers[0].Id, Error));
    TestEqual(TEXT("First route retains build"), Run.Battle().Find(1)->BuildId, FString(TEXT("fighter.battle-master")));
    FExpeditionRun Restored;
    TestTrue(TEXT("Snapshot round trip"), Restored.Restore(Run.Snapshot(), Error));
    auto Bad = Run.Snapshot(); Bad.Battle.Encounter.Combatants[0].MaxHP++;
    TestFalse(TEXT("Profile mutation rejected"), Restored.Restore(Bad, Error));
    TestTrue(TEXT("Failed restore installs nothing"), ExpeditionTestSupport::SameRun(Run.Snapshot(), Restored.Snapshot()));
    Bad = Run.Snapshot(); Bad.Battle.Encounter.Combatants[1].SpellSlots = 1;
    TestFalse(TEXT("Wrong pool rejected"), Restored.Restore(Bad, Error));
    Bad = Run.Snapshot(); Bad.Battle.Encounter.Combatants[0].Name = TEXT("Impostor");
    TestFalse(TEXT("Identity mutation rejected against selection"), Restored.Restore(Bad, Error));
    Bad = Run.Snapshot(); Bad.Progress.CatalogRevision++;
    TestFalse(TEXT("Unknown catalog revision rejected"), Restored.Restore(Bad, Error));
    Bad = Run.Snapshot(); Bad.Battle.Encounter.Combatants[0].ClassId = TEXT("wizard");
    TestFalse(TEXT("Mismatched class rejected"), Restored.Restore(Bad, Error));
    FExpeditionSaveData Data; Data.Current = Run.Snapshot(); FString Json;
    TestEqual(TEXT("Reflected schema default"), Data.Version, 4);
    TestTrue(TEXT("Encode catalog state"), FExpeditionSave::Encode(Data, Json, Error));
    FExpeditionSaveData Decoded;
    TestTrue(TEXT("Decode catalog state"), FExpeditionSave::Decode(Json, Decoded, Error));
    TestTrue(TEXT("Exact JSON roundtrip"), ExpeditionTestSupport::SameRun(Data.Current, Decoded.Current));
    for (const auto* Id : {TEXT("rogue.base"), TEXT("cleric.base"), TEXT("wizard.base")})
    {
        TestTrue(TEXT("Construct current kit"), FBuildCatalog::Create(Id, 1, TEXT("Hero"), Hero));
        TestEqual(TEXT("Current non-Fighter health"), Hero.MaxHP, 22);
        TestEqual(TEXT("Current attack bonus"), Hero.AttackBonus, 5);
    }
    // Independently worked previews of existing source literals; custom combat seam
    // permits initiative/HP setup without changing catalog tuning.
    for (auto Action : {ECombatCommandType::SecondWind, ECombatCommandType::CureWounds, ECombatCommandType::MagicMissile})
    {
        const auto* Id = Action == ECombatCommandType::SecondWind ? TEXT("fighter.champion") : Action == ECombatCommandType::CureWounds ? TEXT("cleric.base") : TEXT("wizard.base");
        FBuildCatalog::Create(Id, 1, TEXT("Hero"), Hero);
        Hero.Cell = {0,0}; Hero.HP = 1; Hero.InitiativeBonus = 30;
        FCombatantState Enemy; Enemy.Id = 20; Enemy.Side = ECombatSide::Enemy; Enemy.Name = TEXT("Enemy"); Enemy.Cell = {1,0}; Enemy.InitiativeBonus = -10;
        FCombatEncounter Duel; Duel.Combatants = {Hero, Enemy};
        FCombatBattle Battle; TestTrue(TEXT("Start current kit scenario"), Battle.Start(Duel, 42, Error));
        const int32 Target = Action == ECombatCommandType::MagicMissile ? 20 : 1;
        auto Forecast = Battle.Forecast(1, Action, Target);
        TestTrue(TEXT("Catalog special legal"), Forecast.bValid);
        TestEqual(TEXT("Worked minimum"), Forecast.MinAmount, Action == ECombatCommandType::SecondWind ? 2 : Action == ECombatCommandType::CureWounds ? 5 : 6);
        TestEqual(TEXT("Worked maximum"), Forecast.MaxAmount, Action == ECombatCommandType::SecondWind ? 11 : Action == ECombatCommandType::CureWounds ? 19 : 15);
        TestTrue(TEXT("Catalog special executes"), Battle.Submit({Action, 1, {}, Target}).bAccepted);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSelectedPartyTest, "Project.Expedition.Catalog.SelectedParty", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSelectedPartyTest::RunTest(const FString& Parameters)
{
    FString Error;
    TArray<FPartyBuildSelection> Party;
    for (int32 I = 1; I <= 4; ++I)
    {
        FPartyBuildSelection S; S.Id = I; S.Name = FString::Printf(TEXT("Slot %d"), I);
        S.BuildId = TEXT("fighter.battle-master"); Party.Add(S);
    }
    FExpeditionRun Run;
    TestTrue(TEXT("Four duplicate builds start"), Run.NewRun(42, Party, Error, TEXT("duplicates")));
    for (int32 I = 1; I <= 4; ++I)
    {
        TestEqual(TEXT("Independent slot name"), Run.Battle().Find(I)->Name, Party[I-1].Name);
        TestEqual(TEXT("Chosen build"), Run.Battle().Find(I)->BuildId, FString(TEXT("fighter.battle-master")));
    }
    const auto Before = Run.Snapshot();
    auto Invalid = Party; Invalid.Pop();
    TestFalse(TEXT("Three slots rejected"), Run.NewRun(42, Invalid, Error));
    TestTrue(TEXT("Failed construction atomic"), ExpeditionTestSupport::SameRun(Before, Run.Snapshot()));
    Invalid = Party; Invalid[3].Id = 1;
    TestFalse(TEXT("Repeated instance identity rejected"), Run.NewRun(42, Invalid, Error));
    Invalid = Party; Invalid[2].BuildId = TEXT("wizard.champion");
    TestFalse(TEXT("Invalid build rejected"), Run.NewRun(42, Invalid, Error));
    FExpeditionRun Restored;
    TestTrue(TEXT("Selected startup restores"), Restored.Restore(Before, Error));
    TestTrue(TEXT("Selected first route"), Run.ChooseRoute(Run.Progress().Offers[0].Id, Error));
    for (int32 I = 1; I <= 4; ++I)
    { TestEqual(TEXT("Route keeps duplicate build"), Run.Battle().Find(I)->BuildId, FString(TEXT("fighter.battle-master"))); }
    TestTrue(TEXT("Selected combat restores"), Restored.Restore(Run.Snapshot(), Error));
    TestTrue(TEXT("Exact selected combat"), ExpeditionTestSupport::SameRun(Run.Snapshot(), Restored.Snapshot()));
    const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation/SelectedParty"), FGuid::NewGuid().ToString());
    const FString Path = FPaths::Combine(Directory, TEXT("current-v1.json"));
    FExpeditionSession Session(Path);
    TestTrue(TEXT("Session accepts duplicate selection"), Session.NewRun(42, Party, Error, TEXT("selected-session")));
    const auto Saved = Session.Run().Snapshot();
    FExpeditionSession Resumed(Path);
    TestTrue(TEXT("Session resume"), Resumed.Resume(Error));
    TestTrue(TEXT("Resume exact selection and RNG"), ExpeditionTestSupport::SameRun(Saved, Resumed.Run().Snapshot()));
    TestFalse(TEXT("Startup selection cannot overwrite unfinished save"), Resumed.NewRun(7, Error));
    TestTrue(TEXT("Rejected respec unchanged"), ExpeditionTestSupport::SameRun(Saved, Resumed.Run().Snapshot()));
    IFileManager::Get().DeleteDirectory(*Directory, false, true);
    return true;
}

#endif
