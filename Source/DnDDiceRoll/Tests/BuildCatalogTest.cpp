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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBarbarianTest, "Project.Expedition.Catalog.Barbarian", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBarbarianTest::RunTest(const FString& Parameters)
{
    FString Error;
    for (const auto* Path : {TEXT("berserker"), TEXT("totem-warrior"), TEXT("beast"), TEXT("wild-magic")})
    {
        FCombatantState Hero; TestTrue(TEXT("Build exists"), FBuildCatalog::Create(FString(TEXT("barbarian.")) + Path, 1, TEXT("Hero"), Hero));
        Hero.Cell = {0,0}; Hero.InitiativeBonus = 30;
        FCombatantState Enemy; Enemy.Id = 20; Enemy.Side = ECombatSide::Enemy; Enemy.AttackBonus = 30; Enemy.DamageDie = 1; Enemy.DamageBonus = 4; Enemy.Name = Enemy.Role = TEXT("Enemy"); Enemy.Cell = {2,0}; Enemy.InitiativeBonus = -10;
        FCombatantState Ally; Ally.Id = 2; Ally.Name = Ally.Role = TEXT("Ally"); Ally.Cell = {0,1}; Ally.InitiativeBonus = -10;
        FCombatEncounter Duel; Duel.Combatants = {Hero, Enemy, Ally};
        FCombatBattle Battle; if (!TestTrue(TEXT("Start"), Battle.Start(Duel, 42, Error))) { return false; }
        TestTrue(TEXT("Rage forecast"), Battle.Forecast(1, ECombatCommandType::Special1, 1).bValid);
        TestTrue(TEXT("Rage activates"), Battle.Submit({ECombatCommandType::Special1, 1, {}, 1}).bAccepted);
        TestEqual(TEXT("Rage spent"), Battle.Find(1)->RageUses, 1);
        if (FString(Path) == TEXT("beast"))
        {
            const auto Before = Battle.Snapshot();
            TestFalse(TEXT("Wrong target rejected"), Battle.Submit({ECombatCommandType::Special2, 1, {}, 2}).bAccepted);
            TestTrue(TEXT("Wrong target snapshot unchanged"), FCombatSnapshot::StaticStruct()->CompareScriptStruct(&Before, &Battle.Snapshot(), 0));
            auto Obscured = Before; Obscured.Encounter.BlockedCells.Add({1,0}); FCombatBattle Wall;
            TestTrue(TEXT("Obscured fixture restores"), Wall.Restore(Obscured, Error));
            TestFalse(TEXT("Reach requires sight"), Wall.Submit({ECombatCommandType::Special2, 1, {}, 20}).bAccepted);
            TestEqual(TEXT("Obscured preserves RNG"), Wall.Snapshot().RandomState, Obscured.RandomState);
        }
        auto Same = [](const FCombatSnapshot& A, const FCombatSnapshot& B) { return FCombatSnapshot::StaticStruct()->CompareScriptStruct(&A, &B, 0); };
        const auto Activated = Battle.Snapshot();
        TestFalse(TEXT("Bonus budget rejection"), Battle.Submit({ECombatCommandType::Special1, 1, {}, 1}).bAccepted);
        TestTrue(TEXT("Rejection preserves state RNG"), Same(Activated, Battle.Snapshot()));
        FCombatBattle Resume; TestTrue(TEXT("Active rage restores"), Resume.Restore(Activated, Error));
        const bool Totem = FString(Path) == TEXT("totem-warrior"), Berserker = FString(Path) == TEXT("berserker");
        if (Berserker)
        {
            // Advance via public commands, preserving rage for the following turn.
            Battle.Submit({ECombatCommandType::EndTurn, 1});
            while (Battle.ActiveId() != 1) { Battle.Submit({ECombatCommandType::EndTurn, Battle.ActiveId()}); }
        }
        if (Berserker)
        {
            TestFalse(TEXT("Frenzy cannot reach two cells"), Battle.Forecast(1, ECombatCommandType::Special2, 20).bValid);
            TestTrue(TEXT("Approach"), Battle.Submit({ECombatCommandType::Move, 1, {1,0}}).bAccepted);
        }
        const int32 Target = Totem ? 2 : 20;
        const int32 RandomBefore = Battle.Snapshot().RandomState;
        const auto Preview = Battle.Forecast(1, ECombatCommandType::Special2, Target);
        TestTrue(TEXT("Path special legal"), Preview.bValid);
        TestEqual(TEXT("Preview never rolls"), Battle.Snapshot().RandomState, RandomBefore);
        TestFalse(TEXT("Special is not healing"), Preview.bHealing);
        TestEqual(TEXT("Worked minimum"), Preview.MinAmount, Totem ? 0 : Berserker ? 4 : FString(Path) == TEXT("beast") ? 6 : 4);
        TestEqual(TEXT("Worked maximum"), Preview.MaxAmount, Totem ? 0 : Berserker ? 4 : FString(Path) == TEXT("beast") ? 6 : 8);
        if (!Totem)
        {
            auto Terminal = Battle.Snapshot(); Terminal.Encounter.Combatants[1].HP = 1;
            FCombatBattle Finish; TestTrue(TEXT("Terminal fixture restores"), Finish.Restore(Terminal, Error));
            TestTrue(TEXT("Special ends encounter"), Finish.Submit({ECombatCommandType::Special2, 1, {}, 20}).bAccepted);
            TestEqual(TEXT("Terminal clears stance"), Finish.Find(1)->RageTurns, 0);
            TestEqual(TEXT("Terminal clears surge"), Finish.Find(1)->LastSurge, 0);
        }
        Resume.Restore(Battle.Snapshot(), Error);
        const FCombatCommand Special{ECombatCommandType::Special2, 1, {}, Target};
        const auto Result = Battle.Submit(Special); const auto Repeated = Resume.Submit(Special);
        TestTrue(TEXT("Special committed"), Result.bAccepted);
        TestEqual(TEXT("Ordered effect target"), Result.Effects.Num() ? Result.Effects[0].TargetId : -1, Target);
        TestEqual(TEXT("Continued result matches"), Result.Damage, Repeated.Damage);
        TestTrue(TEXT("Continued snapshot matches"), Same(Battle.Snapshot(), Resume.Snapshot()));
        if (Totem) { TestEqual(TEXT("Ally protected"), Battle.Find(1)->GuardTargetId, 2); TestTrue(TEXT("Protection description"), Preview.Formula.Contains(TEXT("reduce damage by 3"))); }
        else { TestTrue(TEXT("Damage matches forecast"), Result.Damage >= Preview.MinAmount && Result.Damage <= Preview.MaxAmount); }
        if (FString(Path) == TEXT("wild-magic")) { TestEqual(TEXT("Seed 42 worked surge outcome"), Result.SurgeOutcome, 2); TestEqual(TEXT("Saved surge"), Battle.Find(1)->LastSurge, Result.SurgeOutcome); TestTrue(TEXT("Seed advanced only on commit"), RandomBefore != Battle.Snapshot().RandomState); }
        if (Totem)
        {
            auto Fragile = Battle.Snapshot(); Fragile.Encounter.Combatants[0].HP = 1;
            FCombatBattle Loss; TestTrue(TEXT("Fragile protector restores"), Loss.Restore(Fragile, Error));
            Loss.Submit({ECombatCommandType::EndTurn, 1});
            while (Loss.ActiveId() != 20) { Loss.Submit({ECombatCommandType::EndTurn, Loss.ActiveId()}); }
            TestTrue(TEXT("Enemy approaches protector"), Loss.Submit({ECombatCommandType::Move, 20, {1,0}}).bAccepted);
            const auto Hit = Loss.Submit({ECombatCommandType::Attack, 20, {}, 1});
            TestTrue(TEXT("Protector downed"), Hit.Downed.Contains(1));
            TestEqual(TEXT("Downed clears stance"), Loss.Find(1)->RageTurns, 0);
            TestEqual(TEXT("Downed clears protection"), Loss.Find(1)->GuardTargetId, INDEX_NONE);
        }
        auto Invalid = Battle.Snapshot(); Invalid.Encounter.Combatants[0].RageTurns = 3;
        const auto BeforeReject = Resume.Snapshot();
        TestFalse(TEXT("Duration corruption rejected"), Resume.Restore(Invalid, Error));
        TestTrue(TEXT("Restore rejection atomic"), Same(BeforeReject, Resume.Snapshot()));
        Invalid = Battle.Snapshot(); Invalid.Encounter.Combatants[0].RageUses = 3;
        TestFalse(TEXT("Cap corruption rejected"), Resume.Restore(Invalid, Error));
        Invalid = Battle.Snapshot(); Invalid.Encounter.Combatants[0].BuildId = TEXT("wizard.base"); Invalid.Encounter.Combatants[0].AdventurerRole = EAdventurerRole::Wizard;
        TestFalse(TEXT("Wrong build state rejected"), Resume.Restore(Invalid, Error));
        Battle.Submit({ECombatCommandType::EndTurn, 1});
        while (Battle.ActiveId() != 1)
        {
            if (Totem && Battle.ActiveId() == 20)
            {
                TestTrue(TEXT("Enemy approaches protected ally"), Battle.Submit({ECombatCommandType::Move, 20, {1,1}}).bAccepted);
                const auto P = Battle.Forecast(20, ECombatCommandType::Attack, 2);
                TestEqual(TEXT("Five incoming minus three protection"), P.MinAmount, 2);
                const auto Hit = Battle.Submit({ECombatCommandType::Attack, 20, {}, 2});
                TestTrue(TEXT("Worked protected hit"), Hit.bHit);
                TestEqual(TEXT("Protection changes committed damage"), Hit.Damage, Hit.bCritical ? 3 : 2);
            }
            Battle.Submit({ECombatCommandType::EndTurn, Battle.ActiveId()});
        }
        if (!Berserker) { Battle.Submit({ECombatCommandType::EndTurn, 1}); }
        TestEqual(TEXT("Own-turn expiration"), Battle.Find(1)->RageTurns, 0);
        TestEqual(TEXT("Protection expires"), Battle.Find(1)->GuardTargetId, INDEX_NONE);
        TestEqual(TEXT("No turn refill"), Battle.Find(1)->RageUses, 1);
        while (Battle.ActiveId() != 1) { Battle.Submit({ECombatCommandType::EndTurn, Battle.ActiveId()}); }
        auto Empty = Battle.Snapshot(); Empty.Encounter.Combatants[0].RageUses = 0; Empty.bBonusActionAvailable = true;
        TestTrue(TEXT("Spent pool restore"), Resume.Restore(Empty, Error));
        TestFalse(TEXT("No resource rejection"), Resume.Submit({ECombatCommandType::Special1, 1, {}, 1}).bAccepted);
        TestTrue(TEXT("Empty pool rejection unchanged"), Same(Empty, Resume.Snapshot()));
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBarbarianPersistenceTest, "Project.Expedition.Catalog.BarbarianPersistence", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBarbarianPersistenceTest::RunTest(const FString& Parameters)
{
    FString Error;
    TArray<FPartyBuildSelection> Party;
    for (int32 I = 1; I <= 4; ++I) { FPartyBuildSelection S; S.Id = I; S.Name = FString::Printf(TEXT("Barbarian %d"), I); S.BuildId = TEXT("barbarian.wild-magic"); Party.Add(S); }
    const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation/Barbarians"), FGuid::NewGuid().ToString());
    const FString Path = FPaths::Combine(Directory, TEXT("current-v1.json"));
    FExpeditionSession Session(Path);
    if (!TestTrue(TEXT("New duplicate Barbarians"), Session.NewRun(42, Party, Error, TEXT("barbarians")))) { return false; }
    TestTrue(TEXT("First route"), Session.ChooseRoute(Session.Run().Progress().Offers[0].Id, Error));
    while (Session.Run().Battle().Find(Session.Run().Battle().ActiveId())->Side == ECombatSide::Enemy) { Session.PlayEnemyTurn(); }
    const int32 Id = Session.Run().Battle().ActiveId();
    TestTrue(TEXT("Checkpoint rage"), Session.Submit({ECombatCommandType::Special1, Id, {}, Id}).bAccepted);
    auto Moves = Session.Run().Battle().LegalMoves(Id);
    auto Nearest = [&](FIntPoint Cell) { int32 D = 1000; for (const auto& U : Session.Run().Battle().Snapshot().Encounter.Combatants) { if (U.Side == ECombatSide::Enemy && U.Life == ECombatLife::Conscious) { D = FMath::Min(D, ExpeditionTestSupport::Distance(Cell, U.Cell)); } } return D; };
    Moves.Sort([&](FIntPoint A, FIntPoint B) { return Nearest(A) == Nearest(B) ? (A.Y == B.Y ? A.X < B.X : A.Y < B.Y) : Nearest(A) < Nearest(B); });
    if (!Moves.IsEmpty()) { Session.Submit({ECombatCommandType::Move, Id, Moves[0]}); }
    const auto Targets = Session.Run().Battle().LegalTargets(Id, ECombatCommandType::Special2);
    if (TestFalse(TEXT("Generated route permits surge approach"), Targets.IsEmpty()))
    { TestTrue(TEXT("Checkpoint actual surge"), Session.Submit({ECombatCommandType::Special2, Id, {}, Targets[0]}).bAccepted); }
    FExpeditionSaveData Checkpoint;
    TestTrue(TEXT("Read active state JSON"), FExpeditionSave::Read(Path, Checkpoint, Error));
    const FString ClonePath = FPaths::Combine(Directory, TEXT("continued.json"));
    TestTrue(TEXT("Independent continued checkpoint"), FExpeditionSave::Write(ClonePath, Checkpoint, Error));
    FExpeditionSession Resumed(ClonePath); TestTrue(TEXT("Resume active stance JSON"), Resumed.Resume(Error));
    TestTrue(TEXT("Exact JSON state"), ExpeditionTestSupport::SameRun(Session.Run().Snapshot(), Resumed.Run().Snapshot()));
    Session.Submit({ECombatCommandType::EndTurn, Id}); Resumed.Submit({ECombatCommandType::EndTurn, Id});
    TestTrue(TEXT("Resume latest committed commands"), Resumed.Resume(Error));
    TestTrue(TEXT("Exact continued state"), ExpeditionTestSupport::SameRun(Session.Run().Snapshot(), Resumed.Run().Snapshot()));
    FExpeditionRun Run; Run.Restore(Session.Run().Snapshot(), Error);
    TestTrue(TEXT("Complete first encounter"), ExpeditionTestSupport::FinishCombat(Run));
    if (Run.Progress().Phase == EExpeditionPhase::Reward)
    {
        TestEqual(TEXT("Level without refill"), Run.Battle().Find(Id)->RageUses, 1);
        TestEqual(TEXT("Terminal stance cleanup"), Run.Battle().Find(Id)->RageTurns, 0);
        int32 Recipient = 1; while (Run.Battle().Find(Recipient)->Life != ECombatLife::Conscious) { ++Recipient; }
        TestTrue(TEXT("Reward"), Run.ClaimReward(Recipient, Error)); TestTrue(TEXT("Camp"), Run.Recover(true, Error));
        if (Run.Battle().Find(Id)->Life == ECombatLife::Conscious) { TestEqual(TEXT("Camp bounded refill"), Run.Battle().Find(Id)->RageUses, 2); }
        TestTrue(TEXT("Later route"), Run.ChooseRoute(Run.Progress().Offers[0].Id, Error));
        TestEqual(TEXT("Later build retained"), Run.Battle().Find(Id)->BuildId, FString(TEXT("barbarian.wild-magic")));
    }
    while (Run.Progress().Phase == EExpeditionPhase::Combat || Run.Progress().Phase == EExpeditionPhase::Reward || Run.Progress().Phase == EExpeditionPhase::Route)
    {
        if (Run.Progress().Phase == EExpeditionPhase::Combat) { if (!ExpeditionTestSupport::FinishCombat(Run)) { break; } }
        else if (Run.Progress().Phase == EExpeditionPhase::Reward)
        {
            int32 Survivor = 1; while (Survivor <= 4 && Run.Battle().Find(Survivor)->Life != ECombatLife::Conscious) { ++Survivor; }
            if (Survivor > 4 || !Run.ClaimReward(Survivor, Error)) { break; }
        }
        else if (!Run.ChooseRoute(Run.Progress().Offers[0].Id, Error)) { break; }
    }
    TestEqual(TEXT("Barbarian party finishes five fights"), Run.Progress().EncountersCompleted, 5);
    for (int32 I = 1; I <= 4; ++I) { TestEqual(TEXT("Five-fight build retained"), Run.Battle().Find(I)->BuildId, Party[I-1].BuildId); }
    IFileManager::Get().DeleteDirectory(*Directory, false, true);
    return true;
}

#endif
