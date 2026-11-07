#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Party/BuildCatalog.h"
#include "Persistence/ExpeditionSave.h"
#include "Presentation/ExpeditionPlayback.h"
#include "ExpeditionTestSupport.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMartialRosterTest, "Project.Expedition.Catalog.MartialRoster", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMartialRosterTest::RunTest(const FString& Parameters)
{
    FCombatantState Hero; FString Error;
    TestTrue(TEXT("Thief is an authored selectable build"), FBuildCatalog::Create(TEXT("rogue.thief"), 1, TEXT("Nix"), Hero));
    Hero.Id = 1; Hero.Name = TEXT("Nix"); Hero.Cell = {0,0}; Hero.InitiativeBonus = 30;
    FCombatantState Ally; Ally.Id = 2; Ally.Name = TEXT("Ally"); Ally.Cell = {2,0};
    FCombatantState Enemy; Enemy.Id = 20; Enemy.Name = TEXT("Enemy"); Enemy.Side = ECombatSide::Enemy; Enemy.Cell = {4,0}; Enemy.InitiativeBonus = -10;
    FCombatEncounter Encounter; Encounter.Combatants = {Hero, Ally, Enemy};
    FCombatBattle Battle;
    if (TestTrue(TEXT("Start worked Thief escape"), Battle.Start(Encounter, 42, Error)))
    {
        TestTrue(TEXT("Thief swaps with ally using bonus action"), Battle.Submit({ECombatCommandType::Special1, 1, {}, 2}).bAccepted);
        TestEqual(TEXT("Thief reaches ally cell"), Battle.Find(1)->Cell, FIntPoint(2,0));
        TestEqual(TEXT("Ally reaches original cell"), Battle.Find(2)->Cell, FIntPoint(0,0));
        TestTrue(TEXT("Escape retains basic action"), Battle.Snapshot().bActionAvailable);
    }
    for(int32 Constraint=0;Constraint<3;++Constraint)
    {
        auto Restricted=Encounter;
        if(Constraint==0) { Restricted.BlockedCells={{1,0}}; }
        if(Constraint==1) { Restricted.Combatants[1].Cell={7,7}; }
        if(Constraint==2) { Restricted.Combatants[1].HP=0; Restricted.Combatants[1].Life=ECombatLife::Dead; }
        FCombatBattle Rejected; TestTrue(TEXT("Start restricted exchange scenario"),Rejected.Start(Restricted,42,Error));
        const auto Before=Rejected.Snapshot();
        TestFalse(TEXT("Sight/range/dead constraint removes target"),Rejected.Forecast(1,ECombatCommandType::Special1,2).bValid);
        TestFalse(TEXT("Sight/range/dead exchange rejected"),Rejected.Submit({ECombatCommandType::Special1,1,{},2}).bAccepted);
        TestTrue(TEXT("Constraint rejection preserves complete state and RNG"),FCombatSnapshot::StaticStruct()->CompareScriptStruct(&Before,&Rejected.Snapshot(),0));
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMartialWorkedTest, "Project.Expedition.Catalog.MartialWorkedOutcomes", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMartialWorkedTest::RunTest(const FString& Parameters)
{
    // Literal expected outcomes authored independently of catalog facts. Same geometry
    // exposes sibling differences: two adjacent enemies, injured ally two cells away.
    struct FCase { const TCHAR* Id; int32 Target; int32 Loss; int32 Heal; int32 Guard; bool Swap; bool Push; bool Area; bool Bonus; };
    const FCase Cases[] = {
        {TEXT("rogue.thief"),2,0,0,0,true,false,false,true},
        {TEXT("rogue.assassin"),20,9,0,0,false,false,false,false},
        {TEXT("rogue.arcane-trickster"),1,0,0,3,false,false,false,true},
        {TEXT("rogue.phantom"),20,4,0,0,false,false,true,false},
        {TEXT("rogue.soulknife"),20,4,0,0,false,false,false,true},
        {TEXT("fighter.eldritch-knight"),1,0,0,3,false,false,false,true},
        {TEXT("fighter.psi-warrior"),2,0,0,3,false,false,false,true},
        {TEXT("fighter.rune-knight"),1,0,0,4,false,false,false,false},
        {TEXT("monk.open-hand"),20,5,0,0,false,true,false,true},
        {TEXT("monk.shadow"),2,0,0,0,true,false,false,true},
        {TEXT("monk.four-elements"),20,5,0,0,false,true,false,false},
        {TEXT("monk.mercy"),2,0,8,0,false,false,false,true},
        {TEXT("monk.astral-self"),20,5,0,0,false,false,false,true},
        {TEXT("paladin.devotion"),2,0,0,4,false,false,false,false},
        {TEXT("paladin.ancients"),20,5,0,0,false,true,false,false},
        {TEXT("paladin.vengeance"),20,8,0,0,false,false,false,true},
        {TEXT("paladin.glory"),2,0,0,0,true,false,false,true},
        {TEXT("paladin.watchers"),20,4,0,0,false,false,true,false},
        {TEXT("ranger.hunter"),20,4,0,0,false,false,true,false},
        {TEXT("ranger.beast-master"),20,4,0,0,false,true,false,false},
        {TEXT("ranger.fey-wanderer"),2,0,0,0,true,false,false,true},
        {TEXT("ranger.swarmkeeper"),20,3,0,0,false,false,true,true},
    };
    const FCase SecondCases[] = {
        {TEXT("rogue.thief"),2,0,6,0,false,false,false,true},
        {TEXT("rogue.assassin"),2,0,0,0,true,false,false,true},
        {TEXT("rogue.arcane-trickster"),20,3,0,0,false,true,false,false},
        {TEXT("rogue.phantom"),2,0,0,2,false,false,false,true},
        {TEXT("rogue.soulknife"),2,0,0,0,true,false,false,false},
        {TEXT("fighter.eldritch-knight"),20,4,0,0,false,false,true,false},
        {TEXT("fighter.psi-warrior"),20,5,0,0,false,true,false,false},
        {TEXT("fighter.rune-knight"),20,5,0,0,false,false,true,false},
        {TEXT("monk.open-hand"),1,0,0,3,false,false,false,false},
        {TEXT("monk.shadow"),2,0,0,3,false,false,false,false},
        {TEXT("monk.four-elements"),20,4,0,0,false,false,true,false},
        {TEXT("monk.mercy"),20,6,0,0,false,false,false,false},
        {TEXT("monk.astral-self"),1,0,0,3,false,false,false,true},
        {TEXT("paladin.devotion"),2,0,8,0,false,false,false,true},
        {TEXT("paladin.ancients"),2,0,0,3,false,false,false,true},
        {TEXT("paladin.vengeance"),2,0,0,0,true,false,false,false},
        {TEXT("paladin.glory"),2,0,8,0,false,false,false,false},
        {TEXT("paladin.watchers"),1,0,0,3,false,false,false,true},
        {TEXT("ranger.hunter"),20,4,0,0,false,false,false,true},
        {TEXT("ranger.beast-master"),2,0,0,3,false,false,false,true},
        {TEXT("ranger.fey-wanderer"),20,6,0,0,false,false,false,false},
        {TEXT("ranger.swarmkeeper"),20,5,0,0,false,true,false,false},
    };
    auto Same = [](const FCombatSnapshot& A,const FCombatSnapshot& B) { return FCombatSnapshot::StaticStruct()->CompareScriptStruct(&A,&B,0); };
    for (const auto& C : Cases)
    {
        AddInfo(C.Id); FString Error; FCombatantState Hero;
        if (!TestTrue(TEXT("Authored build"),FBuildCatalog::Create(C.Id,1,TEXT("Hero"),Hero))) { continue; }
        Hero.Cell={1,1}; Hero.InitiativeBonus=30;
        FCombatantState Ally; Ally.Id=2; Ally.Name=Ally.Role=TEXT("Ally"); Ally.Cell={1,2}; Ally.HP=5;
        FCombatantState Enemy; Enemy.Id=20; Enemy.Name=Enemy.Role=TEXT("Enemy"); Enemy.Side=ECombatSide::Enemy; Enemy.Cell={2,1}; Enemy.HP=Enemy.MaxHP=40; Enemy.InitiativeBonus=-10;
        auto Secondary=Enemy; Secondary.Id=21; Secondary.Name=TEXT("Secondary"); Secondary.Cell={2,2};
        FCombatEncounter Encounter; Encounter.Combatants={Hero,Ally,Enemy,Secondary};
        const FCase* Second = nullptr;
        for (const auto& Candidate : SecondCases) { if (FString(Candidate.Id) == C.Id) { Second = &Candidate; break; } }
        if (!TestNotNull(TEXT("Independent second-slot expectation"),Second)) { continue; }
        const auto& D = *Second;
        FCombatBattle Combo;
        if (TestTrue(TEXT("Start two-slot sibling combination"),Combo.Start(Encounter,42,Error)))
        {
            TestTrue(TEXT("First slot of combination"),Combo.Submit({ECombatCommandType::Special1,1,{},C.Target}).bAccepted);
            TestEqual(TEXT("First slot spends one shared use"),Combo.Find(1)->KitUses,1);
            if (C.Bonus == D.Bonus)
            {
                const auto Spent = Combo.Snapshot();
                TestFalse(TEXT("Same-budget second slot waits for next turn"),Combo.Submit({ECombatCommandType::Special2,1,{},D.Target}).bAccepted);
                TestTrue(TEXT("Spent-budget rejection preserves state and RNG"),Same(Spent,Combo.Snapshot()));
                Combo.Submit({ECombatCommandType::EndTurn,1});
                for (int32 Turn=0;Turn<8 && Combo.ActiveId()!=1;++Turn) { Combo.Submit({ECombatCommandType::EndTurn,Combo.ActiveId()}); }
            }
            const auto BeforeSecond = Combo.Snapshot();
            const int32 PrimaryBefore = Combo.Find(20)->HP, SecondaryBefore = Combo.Find(21)->HP, AllyBefore = Combo.Find(2)->HP;
            const auto PreviewSecond = Combo.Forecast(1,ECombatCommandType::Special2,D.Target);
            TestTrue(TEXT("Second slot forecast after first effect"),PreviewSecond.bValid);
            TestTrue(TEXT("Combination preview preserves snapshot and RNG"),Same(BeforeSecond,Combo.Snapshot()));
            const auto SecondResult = Combo.Submit({ECombatCommandType::Special2,1,{},D.Target});
            TestTrue(TEXT("Second slot commits using remaining shared use"),SecondResult.bAccepted);
            TestEqual(TEXT("Both slots deplete one common pool"),Combo.Find(1)->KitUses,0);
            TestEqual(TEXT("Second-slot primary HP loss"),PrimaryBefore-Combo.Find(20)->HP,D.Loss);
            TestEqual(TEXT("Second-slot ally healing"),Combo.Find(2)->HP-AllyBefore,D.Heal);
            // Water Whip moves the selected enemy away from its former neighbour.
            const int32 SecondaryLoss = D.Area && !C.Push ? D.Loss : 0;
            TestEqual(TEXT("Area membership follows the displaced primary"),SecondaryBefore-Combo.Find(21)->HP,SecondaryLoss);
            TestEqual(TEXT("Second-slot protection amount"),Combo.Find(1)->GuardAmount,D.Guard ? D.Guard : C.Bonus != D.Bonus ? C.Guard : 0);
            TestEqual(TEXT("Combination action budget"),Combo.Snapshot().bActionAvailable,C.Bonus == D.Bonus && D.Bonus);
            TestEqual(TEXT("Combination bonus budget"),Combo.Snapshot().bBonusActionAvailable,C.Bonus == D.Bonus && !D.Bonus);
            if (D.Loss) { TestEqual(TEXT("Second-slot forecast damage"),PreviewSecond.MinAmount,D.Loss); }
            if (D.Heal) { TestEqual(TEXT("Second-slot forecast healing"),PreviewSecond.MinAmount,D.Heal); }
            const FIntPoint UserCell = C.Swap != D.Swap ? FIntPoint(1,2) : FIntPoint(1,1);
            const FIntPoint AllyCell = C.Swap != D.Swap ? FIntPoint(1,1) : FIntPoint(1,2);
            const FIntPoint EnemyCell = C.Push || D.Push ? FIntPoint(3,1) : FIntPoint(2,1);
            TestEqual(TEXT("Combined hero position"),Combo.Find(1)->Cell,UserCell);
            TestEqual(TEXT("Combined ally position"),Combo.Find(2)->Cell,AllyCell);
            TestEqual(TEXT("Combined enemy position"),Combo.Find(20)->Cell,EnemyCell);
            FExpeditionPlayback NormalCombo,FastCombo,SkippedCombo;
            for (auto* Show : {&NormalCombo,&FastCombo,&SkippedCombo}) { Show->Reset(BeforeSecond); Show->Enqueue({SecondResult},Combo.Snapshot()); }
            for (int32 Tick=0;Tick<100;++Tick) { NormalCombo.Tick(0.05f); FastCombo.Tick(0.5f); } SkippedCombo.Finish();
            for (auto* Show : {&NormalCombo,&FastCombo,&SkippedCombo})
            {
                TestEqual(TEXT("Combination playback hero cell"),Show->Unit(1)->Position,FVector2D(UserCell));
                TestEqual(TEXT("Combination playback ally cell"),Show->Unit(2)->Position,FVector2D(AllyCell));
                TestEqual(TEXT("Combination playback enemy cell"),Show->Unit(20)->Position,FVector2D(EnemyCell));
            }
        }
        FCombatBattle Battle; if (!TestTrue(TEXT("Start worked scenario"),Battle.Start(Encounter,42,Error))) { continue; }
        const auto Before=Battle.Snapshot();
        const auto Preview=Battle.Forecast(1,ECombatCommandType::Special1,C.Target);
        TestTrue(TEXT("Legal forecast"),Preview.bValid);
        Battle.Forecast(1,ECombatCommandType::Special1,C.Target);
        TestTrue(TEXT("Repeated preview preserves full snapshot and RNG"),Same(Before,Battle.Snapshot()));
        TestFalse(TEXT("Dead/missing target rejected"),Battle.Submit({ECombatCommandType::Special1,1,{},99}).bAccepted);
        TestTrue(TEXT("Rejected target atomic"),Same(Before,Battle.Snapshot()));
        const auto Result=Battle.Submit({ECombatCommandType::Special1,1,{},C.Target});
        TestTrue(TEXT("Worked command accepted"),Result.bAccepted);
        TestEqual(TEXT("Primary HP loss"),40-Battle.Find(20)->HP,C.Loss);
        TestEqual(TEXT("Secondary HP loss only for area"),40-Battle.Find(21)->HP,C.Area?C.Loss:0);
        TestEqual(TEXT("Actual healing"),Battle.Find(2)->HP-5,C.Heal);
        TestEqual(TEXT("Owned protection amount"),Battle.Find(1)->GuardAmount,C.Guard);
        TestEqual(TEXT("Independent resource spent"),Battle.Find(1)->KitUses,1);
        TestEqual(TEXT("Action budget"),Battle.Snapshot().bActionAvailable,C.Bonus);
        TestEqual(TEXT("Bonus budget"),Battle.Snapshot().bBonusActionAvailable,!C.Bonus);
        TestEqual(TEXT("User cell"),Battle.Find(1)->Cell,C.Swap?FIntPoint(1,2):FIntPoint(1,1));
        TestEqual(TEXT("Displaced enemy cell"),Battle.Find(20)->Cell,C.Push?FIntPoint(3,1):FIntPoint(2,1));
        if(C.Loss) { TestEqual(TEXT("Forecast exact primary"),Preview.MinAmount,C.Loss); TestEqual(TEXT("Forecast max primary"),Preview.MaxAmount,C.Loss); }
        if(C.Loss)
        {
            auto Fragile=Encounter; Fragile.Combatants[2].HP=1;
            FCombatBattle Overkill; TestTrue(TEXT("Start one-HP target"),Overkill.Start(Fragile,42,Error));
            TestEqual(TEXT("Forecast caps actual HP loss"),Overkill.Forecast(1,ECombatCommandType::Special1,20).MinAmount,1);
            const auto Finish=Overkill.Submit({ECombatCommandType::Special1,1,{},20});
            const auto* Loss=Finish.Effects.FindByPredicate([](const auto& E) { return E.TargetId==20 && E.Effect==EAbilityEffect::Strike; });
            TestTrue(TEXT("Ordered primary fact records actual one HP"),Loss && Loss->Amount==1);
        }
        const auto After=Battle.Snapshot();
        FCombatBattle Resumed; TestTrue(TEXT("Detached restore"),Resumed.Restore(After,Error));
        auto WrongRole=After; WrongRole.Encounter.Combatants[0].AdventurerRole=EAdventurerRole::Wizard;
        TestFalse(TEXT("Kit pool cannot be restored under wrong role"),Resumed.Restore(WrongRole,Error));
        auto Bad=After; Bad.Encounter.Combatants[0].KitUses=3;
        TestFalse(TEXT("Resource cap rejected"),Resumed.Restore(Bad,Error));
        TestTrue(TEXT("Restore rejection atomic"),Same(After,Resumed.Snapshot()));
        Bad=After; Bad.Encounter.Combatants[0].GuardTargetId=99; Bad.Encounter.Combatants[0].GuardTurns=1; Bad.Encounter.Combatants[0].GuardAmount=3;
        TestFalse(TEXT("Malformed owner target rejected"),Resumed.Restore(Bad,Error));
        FExpeditionPlayback Normal,Fast,Skipped; Normal.Reset(Before); Fast.Reset(Before); Skipped.Reset(Before);
        Normal.Enqueue({Result},After); Fast.Enqueue({Result},After); Skipped.Enqueue({Result},After);
        for(int32 I=0;I<100;++I) { Normal.Tick(0.05f); Fast.Tick(0.5f); } Skipped.Finish();
        for(const auto& U:After.Encounter.Combatants)
        { TestEqual(TEXT("Normal playback HP"),Normal.Unit(U.Id)->HP,U.HP); TestEqual(TEXT("Fast playback HP"),Fast.Unit(U.Id)->HP,U.HP); TestEqual(TEXT("Finish playback HP"),Skipped.Unit(U.Id)->HP,U.HP); }
        TestTrue(TEXT("Continued command"),Battle.Submit({ECombatCommandType::EndTurn,1}).bAccepted);
        TestTrue(TEXT("Resumed command"),Resumed.Submit({ECombatCommandType::EndTurn,1}).bAccepted);
        TestTrue(TEXT("Identical continued RNG"),Same(Battle.Snapshot(),Resumed.Snapshot()));
        for(int32 I=0;I<8 && Battle.ActiveId()!=1;++I) { Battle.Submit({ECombatCommandType::EndTurn,Battle.ActiveId()}); }
        TestEqual(TEXT("Protection expires on owner's next turn"),Battle.Find(1)->GuardTargetId,INDEX_NONE);
        TestEqual(TEXT("No turn refill"),Battle.Find(1)->KitUses,1);
        if(C.Push) { TestTrue(TEXT("Approach displaced enemy"),Battle.Submit({ECombatCommandType::Move,1,{2,1}}).bAccepted); }
        TestTrue(TEXT("Second use commits"),Battle.Submit({ECombatCommandType::Special1,1,{},C.Target}).bAccepted);
        TestEqual(TEXT("Pool depleted"),Battle.Find(1)->KitUses,0);
        Battle.Submit({ECombatCommandType::EndTurn,1});
        for(int32 I=0;I<8 && Battle.ActiveId()!=1;++I) { Battle.Submit({ECombatCommandType::EndTurn,Battle.ActiveId()}); }
        const auto Empty=Battle.Snapshot();
        TestFalse(TEXT("Depleted pool cannot use special"),Battle.Submit({ECombatCommandType::Special1,1,{},C.Target}).bAccepted);
        TestTrue(TEXT("Depleted command preserves snapshot and RNG"),Same(Empty,Battle.Snapshot()));
    }
    TestEqual(TEXT("Complete implemented catalog"),FBuildCatalog::All().Num(),70);
    TestNull(TEXT("Interim Rogue removed"),FBuildCatalog::Find(TEXT("rogue.base")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMartialJsonTest, "Project.Expedition.Catalog.MartialJsonContinuity", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMartialJsonTest::RunTest(const FString& Parameters)
{
    FString Error;
    for (const auto* Build : {TEXT("rogue.thief"),TEXT("fighter.eldritch-knight"),TEXT("ranger.beast-master")})
    {
        AddInfo(Build); TArray<FPartyBuildSelection> Party;
        for(int32 Id=1;Id<=4;++Id) { FPartyBuildSelection S; S.Id=Id; S.Name=FString::Printf(TEXT("Hero %d"),Id); S.BuildId=Build; Party.Add(S); }
        FExpeditionRun Run; if(!TestTrue(TEXT("Duplicate martial party"),Run.NewRun(42,Party,Error,TEXT("martial-json")))) { continue; }
        TestTrue(TEXT("First route retains party"),Run.ChooseRoute(Run.Progress().Offers[0].Id,Error));
        while(Run.Battle().Find(Run.Battle().ActiveId())->Side==ECombatSide::Enemy) { Run.PlayEnemyTurn(); }
        const int32 Id=Run.Battle().ActiveId();
        // Companion Guard is the second slot; others exercise exchange / self protection.
        const auto Action=FString(Build)==TEXT("ranger.beast-master")?ECombatCommandType::Special2:ECombatCommandType::Special1;
        const auto Targets=Run.Battle().LegalTargets(Id,Action);
        if(!TestFalse(TEXT("Generated party has valid ally/self target"),Targets.IsEmpty())) { continue; }
        TestTrue(TEXT("Commit actual martial effect"),Run.Submit({Action,Id,{},Targets[0]}).bAccepted);
        const FString Directory=FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("Automation/Martial"),FGuid::NewGuid().ToString());
        const FString Path=FPaths::Combine(Directory,TEXT("current-v1.json"));
        ExpeditionTestSupport::ExpectFailedCheckpointWrites(*this,Path,1);
        FExpeditionSaveData Data; Data.Current=Run.Snapshot(); FString Json;
        TestTrue(TEXT("Write real checkpoint"),FExpeditionSave::Write(Path,Data,Error));
        FExpeditionSession Session(Path); TestTrue(TEXT("Session Resume owned effects"),Session.Resume(Error));
        const auto BeforeFailure=Session.Run().Snapshot();
        IFileManager::Get().MakeDirectory(*(Path+TEXT(".pending")),true);
        TestFalse(TEXT("Failed checkpoint rejects expiration command"),Session.Submit({ECombatCommandType::EndTurn,Id}).bAccepted);
        TestTrue(TEXT("Failure retains resources, protection, cells and RNG"),ExpeditionTestSupport::SameRun(BeforeFailure,Session.Run().Snapshot()));
        IFileManager::Get().DeleteDirectory(*(Path+TEXT(".pending")),false,true);
        TestTrue(TEXT("Retry commits"),Session.Submit({ECombatCommandType::EndTurn,Id}).bAccepted);
        IFileManager::Get().DeleteDirectory(*Directory,false,true);
        TestTrue(TEXT("Actual JSON encode spent resource and owned effect/cells"),FExpeditionSave::Encode(Data,Json,Error));
        FExpeditionSaveData Decoded;
        if(!TestTrue(TEXT("Actual JSON decode"),FExpeditionSave::Decode(Json,Decoded,Error))) { continue; }
        TestTrue(TEXT("Every reflected value and RNG preserved"),ExpeditionTestSupport::SameRun(Data.Current,Decoded.Current));
        // Malformed wire data must fail before installation, not only struct restore.
        for (int32 Mutation=0;Mutation<9;++Mutation)
        {
            TSharedPtr<FJsonObject> Object;
            if (!TestTrue(TEXT("Read generated JSON object"),FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json),Object))) { continue; }
            auto Unit = Object->GetObjectField(TEXT("Current"))->GetObjectField(TEXT("Battle"))->GetObjectField(TEXT("Encounter"))->GetArrayField(TEXT("Combatants"))[0]->AsObject();
            if (Mutation==0) { Unit->RemoveField(TEXT("KitUses")); }
            else if (Mutation==1) { Unit->SetNumberField(TEXT("KitUses"),0.5); }
            else if (Mutation==2) { Unit->SetStringField(TEXT("KitUses"),TEXT("1")); }
            else if (Mutation==3) { Unit->SetNumberField(TEXT("KitUses"),3); }
            else if (Mutation==4) { Unit->SetNumberField(TEXT("GuardTurns"),2); }
            else if (Mutation==5) { Unit->SetNumberField(TEXT("GuardAmount"),99); }
            else if (Mutation==6) { Unit->SetNumberField(TEXT("GuardTargetId"),999); }
            else if (Mutation==7) { Unit->SetNumberField(TEXT("SpellSlots"),1); }
            else { Unit->RemoveField(TEXT("GuardAmount")); }
            FString Malformed; FJsonSerializer::Serialize(Object.ToSharedRef(),TJsonWriterFactory<>::Create(&Malformed));
            auto Output=Decoded;
            TestFalse(TEXT("Malformed martial wire state rejected"),FExpeditionSave::Decode(Malformed,Output,Error));
            TestTrue(TEXT("Malformed JSON decode is atomic"),FExpeditionSaveData::StaticStruct()->CompareScriptStruct(&Decoded,&Output,0));
        }
        FExpeditionRun Resumed; TestTrue(TEXT("Install decoded checkpoint"),Resumed.Restore(Decoded.Current,Error));
        Run.Submit({ECombatCommandType::EndTurn,Id}); Resumed.Submit({ECombatCommandType::EndTurn,Id});
        TestTrue(TEXT("Identical continued command and RNG"),ExpeditionTestSupport::SameRun(Run.Snapshot(),Resumed.Snapshot()));
        auto Bad=Decoded.Current; Bad.Battle.Encounter.Combatants[0].KitUses=3;
        const auto Before=Resumed.Snapshot();
        TestFalse(TEXT("Malformed pool rejected"),Resumed.Restore(Bad,Error));
        TestTrue(TEXT("Failed restore atomic"),ExpeditionTestSupport::SameRun(Before,Resumed.Snapshot()));
        Bad=Decoded.Current; Bad.Battle.Encounter.Combatants[0].SpellSlots=1;
        TestFalse(TEXT("Wrong class pool rejected"),Resumed.Restore(Bad,Error));
        Bad=Decoded.Current;
        for(auto& Unit:Bad.Battle.Encounter.Combatants) { if(Unit.Side==ECombatSide::Enemy) { Unit.SpellSlots=1; break; } }
        TestFalse(TEXT("Enemy cannot acquire class spell pool"),Resumed.Restore(Bad,Error));
        Bad=Decoded.Current;
        for(auto& Unit:Bad.Battle.Encounter.Combatants) { if(Unit.Side==ECombatSide::Enemy) { Unit.SecondWindUses=1; break; } }
        TestFalse(TEXT("Enemy cannot acquire Wind pool"),Resumed.Restore(Bad,Error));
        Bad=Decoded.Current; auto& Wrong=Bad.Battle.Encounter.Combatants[0]; Wrong.GuardTurns=1; Wrong.GuardAmount=4; Wrong.GuardTargetId=999;
        TestFalse(TEXT("Invalid protection owner/target rejected"),Resumed.Restore(Bad,Error));
    }
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMartialProtectionTest, "Project.Expedition.Catalog.MartialProtection", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMartialProtectionTest::RunTest(const FString& Parameters)
{
    FString Error; FCombatantState Knight,Psi;
    FBuildCatalog::Create(TEXT("fighter.eldritch-knight"),1,TEXT("Knight"),Knight);
    FBuildCatalog::Create(TEXT("fighter.psi-warrior"),2,TEXT("Psi"),Psi);
    Knight.Cell={1,1}; Knight.InitiativeBonus=30; Knight.ArmorClass=1;
    Psi.Cell={1,2}; Psi.InitiativeBonus=10;
    FCombatantState Enemy; Enemy.Id=20; Enemy.Name=Enemy.Role=TEXT("Enemy"); Enemy.Side=ECombatSide::Enemy; Enemy.Cell={2,1}; Enemy.InitiativeBonus=-10; Enemy.AttackBonus=30; Enemy.DamageDie=1; Enemy.DamageBonus=4;
    FCombatEncounter Encounter; Encounter.Combatants={Knight,Psi,Enemy}; FCombatBattle Battle;
    if(!TestTrue(TEXT("Start protection scenario"),Battle.Start(Encounter,42,Error))) { return false; }
    TestTrue(TEXT("Self ward"),Battle.Submit({ECombatCommandType::Special1,1,{},1}).bAccepted);
    Battle.Submit({ECombatCommandType::EndTurn,1});
    TestEqual(TEXT("Supporter acts"),Battle.ActiveId(),2);
    TestTrue(TEXT("Second owner protects Knight"),Battle.Submit({ECombatCommandType::Special1,2,{},1}).bAccepted);
    Battle.Submit({ECombatCommandType::EndTurn,2});
    const auto Preview=Battle.Forecast(20,ECombatCommandType::Attack,1);
    TestEqual(TEXT("Two wards use strongest, not sum: five minus three"),Preview.MinAmount,2);
    const int32 Before=Battle.Find(1)->HP;
    const auto Hit=Battle.Submit({ECombatCommandType::Attack,20,{},1});
    TestTrue(TEXT("Worked incoming hit"),Hit.bHit);
    TestEqual(TEXT("Committed prevention is three"),Hit.DamagePrevented,3);
    TestEqual(TEXT("Actual HP loss matches committed result"),Before-Battle.Find(1)->HP,Hit.Damage);
    TestTrue(TEXT("Feedback explains prevention"),Hit.Feedback.Contains(TEXT("prevented 3")));
    // A downed owner must remove only its own protection; target loss clears all.
    auto Down=Battle.Snapshot(); auto& Owner=Down.Encounter.Combatants[1]; Owner.Life=ECombatLife::Downed; Owner.HP=0; Owner.RescueTurnsLeft=3;
    FCombatBattle Restored;
    TestFalse(TEXT("Downed owner cannot retain an effect"),Restored.Restore(Down,Error));
    auto Wrong=Battle.Snapshot(); Wrong.Encounter.Combatants[1].GuardTurns=2;
    TestFalse(TEXT("Expired/invalid duration rejected"),Restored.Restore(Wrong,Error));
    Battle.Submit({ECombatCommandType::EndTurn,20});
    TestEqual(TEXT("Knight's ward expires on own turn"),Battle.Find(1)->GuardTargetId,INDEX_NONE);
    TestEqual(TEXT("Other owner's ward retains independent lifetime"),Battle.Find(2)->GuardTargetId,1);
    auto Lethal=Encounter; Lethal.Combatants[0].HP=1; Lethal.Combatants[2].DamageBonus=100; Lethal.Combatants[2].HP=1;
    FCombatBattle Loss; TestTrue(TEXT("Start owner-loss scenario"),Loss.Start(Lethal,42,Error));
    Loss.Submit({ECombatCommandType::Special1,1,{},1}); Loss.Submit({ECombatCommandType::EndTurn,1});
    Loss.Submit({ECombatCommandType::Special1,2,{},1}); Loss.Submit({ECombatCommandType::EndTurn,2});
    const auto Downing=Loss.Submit({ECombatCommandType::Attack,20,{},1});
    TestTrue(TEXT("Actual committed downing"),Downing.Downed.Contains(1));
    TestEqual(TEXT("Downing clears owner's protection"),Loss.Find(1)->GuardTargetId,INDEX_NONE);
    TestEqual(TEXT("Downing clears other protection targeting victim"),Loss.Find(2)->GuardTargetId,INDEX_NONE);
    TestEqual(TEXT("Downing clears duration"),Loss.Find(1)->GuardTurns,0);
    Loss.Submit({ECombatCommandType::EndTurn,20});
    const auto Victory=Loss.Submit({ECombatCommandType::Special2,2,{},20});
    TestTrue(TEXT("Survivor resolves finale of fixture"),Victory.bAccepted);
    TestEqual(TEXT("Terminal victory"),Loss.Snapshot().Phase,EBattlePhase::Victory);
    TestEqual(TEXT("Unrescued permanent death"),Loss.Find(1)->Life,ECombatLife::Dead);
    TestEqual(TEXT("Terminal protection cleanup"),Loss.Find(2)->GuardTurns,0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMartialFighterTest, "Project.Expedition.Catalog.MartialFighterCriticals", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMartialFighterTest::RunTest(const FString& Parameters)
{
    for(const auto* Build:{TEXT("fighter.champion"),TEXT("fighter.battle-master")})
    {
        bool Found19=false; const bool Champion=FString(Build)==TEXT("fighter.champion");
        for(int32 Seed=0;Seed<300 && !Found19;++Seed)
        {
            FString Error; FCombatantState Hero; FBuildCatalog::Create(Build,1,TEXT("Fighter"),Hero); Hero.InitiativeBonus=30; Hero.Cell={0,0};
            FCombatantState Enemy; Enemy.Id=20; Enemy.Name=Enemy.Role=TEXT("Enemy"); Enemy.Side=ECombatSide::Enemy; Enemy.Cell={1,0}; Enemy.ArmorClass=40; Enemy.HP=Enemy.MaxHP=100; Enemy.InitiativeBonus=-10;
            FCombatEncounter Encounter; Encounter.Combatants={Hero,Enemy}; FCombatBattle Battle;
            TestTrue(TEXT("Catalog Fighter starts"),Battle.Start(Encounter,Seed,Error));
            const auto Action=Champion?ECombatCommandType::Attack:ECombatCommandType::PrecisionStrike;
            TestEqual(TEXT("Worked critical-only chance"),Battle.Forecast(1,Action,20).HitChancePercent,Champion?10:5);
            const auto Result=Battle.Submit({Action,1,{},20});
            if(Result.NaturalRoll==19)
            {
                Found19=true; TestEqual(TEXT("19 critical belongs only to Champion"),Result.bCritical,Champion);
                TestEqual(TEXT("19 against AC40 hit agrees with critical"),Result.bHit,Champion);
                if(!Champion) { TestEqual(TEXT("Precision spends maneuver even on miss"),Battle.Find(1)->ManeuverUses,1); TestEqual(TEXT("Precision accuracy +2"),Result.AttackBonus,7); }
            }
        }
        TestTrue(TEXT("Observed natural19 through public seeded commands"),Found19);
    }
    return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMartialRunTest, "Project.Expedition.Catalog.MartialRunProgression", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMartialRunTest::RunTest(const FString& Parameters)
{
    for(int32 Mode=0;Mode<4;++Mode)
    {
        const bool Duplicate=Mode==1;
        FString Error; TArray<FPartyBuildSelection> Party;
        const TCHAR* Melee[]={TEXT("fighter.champion"),TEXT("monk.open-hand"),TEXT("paladin.vengeance"),TEXT("barbarian.beast")};
        const TCHAR* SupportPoor[]={TEXT("rogue.assassin"),TEXT("rogue.soulknife"),TEXT("ranger.hunter"),TEXT("ranger.swarmkeeper")};
        const TCHAR* Mixed[]={TEXT("fighter.rune-knight"),TEXT("rogue.soulknife"),TEXT("paladin.devotion"),TEXT("ranger.hunter")};
        for(int32 Id=1;Id<=4;++Id) { FPartyBuildSelection P; P.Id=Id; P.Name=FString::Printf(TEXT("Slot %d"),Id); P.BuildId=Mode==2?Melee[Id-1]:Mode==3?SupportPoor[Id-1]:Duplicate?TEXT("ranger.hunter"):Mixed[Id-1]; Party.Add(P); }
        FExpeditionRun Run; TestTrue(TEXT("Construct martial run"),Run.NewRun(42,Party,Error,Duplicate?TEXT("duplicate-martial"):TEXT("mixed-martial")));
        for(int32 Stage=0;Stage<5;++Stage)
        {
            if(!TestTrue(TEXT("Choose actual generated route"),Run.ChooseRoute(Run.Progress().Offers[0].Id,Error))) { break; }
            for(const auto& U:Run.Battle().Snapshot().Encounter.Combatants) { if(U.Side==ECombatSide::Party && U.Life==ECombatLife::Conscious) { TestEqual(TEXT("Conscious hero enters earned level"),U.Level,Stage+1); } }
            if(!TestTrue(TEXT("Public player policy completes encounter"),ExpeditionTestSupport::FinishCombat(Run))) { break; }
            FExpeditionSaveData Data; Data.Current=Run.Snapshot();
            if(Run.Progress().Phase==EExpeditionPhase::Victory || Run.Progress().Phase==EExpeditionPhase::Defeat) { Data.Records.Add(FExpeditionSave::Ending(Run)); }
            FString Json; TestTrue(TEXT("Progress JSON encode"),FExpeditionSave::Encode(Data,Json,Error)); FExpeditionSaveData Decoded;
            TestTrue(TEXT("Progress JSON decode"),FExpeditionSave::Decode(Json,Decoded,Error));
            FExpeditionRun Resumed; TestTrue(TEXT("Progress Resume"),Resumed.Restore(Decoded.Current,Error));
            TestTrue(TEXT("Progress identical values/RNG"),ExpeditionTestSupport::SameRun(Run.Snapshot(),Resumed.Snapshot()));
            for(int32 Id=1;Id<=4;++Id) { TestEqual(TEXT("Slot identity/build survives progression"),Resumed.Battle().Find(Id)->BuildId,Party[Id-1].BuildId); }
            if(Run.Progress().Phase==EExpeditionPhase::Victory || Run.Progress().Phase==EExpeditionPhase::Defeat) { break; }
            int32 Survivor=1; while(Survivor<=4 && Run.Battle().Find(Survivor)->Life!=ECombatLife::Conscious) { ++Survivor; }
            if(!TestTrue(TEXT("Capped reward"),Run.ClaimReward(Survivor,Error))) { break; }
            if(Stage==0) { TestTrue(TEXT("One camp"),Run.Recover(true,Error)); }
        }
        TestEqual(TEXT("Five encounters completed by worked martial party"),Run.Progress().EncountersCompleted,5);
        int32 Rewards=0; for(const auto& Record:Run.Progress().History) { if(Record.RewardRecipient!=INDEX_NONE) { ++Rewards; } }
        TestEqual(TEXT("Four recorded rewards"),Rewards,4);
        TestTrue(TEXT("Single camp retained"),Run.Progress().bCampUsed);
        for(const auto& U:Run.Battle().Snapshot().Encounter.Combatants) { if(U.Side==ECombatSide::Party && U.Life==ECombatLife::Conscious) { TestEqual(TEXT("Conscious finale survivor level five"),U.Level,5); } }
        const auto Ending=FExpeditionSave::Ending(Run);
        TestFalse(TEXT("Actual ending record retains identity"),Ending.RunId.IsEmpty());
    }
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMartialCombatPoolsTest, "Project.Expedition.Catalog.MartialCombatPoolValidation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMartialCombatPoolsTest::RunTest(const FString& Parameters)
{
    FString Error; FCombatantState Hero;
    FBuildCatalog::Create(TEXT("rogue.thief"),1,TEXT("Thief"),Hero); Hero.Cell={1,1}; Hero.InitiativeBonus=30;
    FCombatantState Enemy; Enemy.Id=20; Enemy.Name=Enemy.Role=TEXT("Enemy"); Enemy.Side=ECombatSide::Enemy; Enemy.Cell={3,1};
    FCombatEncounter Encounter; Encounter.Combatants={Hero,Enemy}; FCombatBattle Battle;
    if (!TestTrue(TEXT("Start catalog pool validation"),Battle.Start(Encounter,42,Error))) { return false; }
    const auto Before=Battle.Snapshot();
    for (int32 Pool=0;Pool<2;++Pool)
    {
        auto Bad=Before;
        if (Pool==0) { Bad.Encounter.Combatants[0].SpellSlots=1; }
        else { Bad.Encounter.Combatants[0].SecondWindUses=1; }
        TestFalse(TEXT("Public combat Restore rejects a pool absent from selected build"),Battle.Restore(Bad,Error));
        TestTrue(TEXT("Wrong-pool combat Restore preserves state/RNG"),FCombatSnapshot::StaticStruct()->CompareScriptStruct(&Before,&Battle.Snapshot(),0));
    }
    // Public combat fixtures without a catalog identity retain their bounded stats.
    auto Custom=Encounter; auto& Fixture=Custom.Combatants[0];
    Fixture.BuildId.Reset(); Fixture.ClassId.Reset(); Fixture.SubclassId.Reset(); Fixture.AdventurerRole=EAdventurerRole::None; Fixture.KitUses=0;
    Fixture.SecondWindUses=1; Fixture.SpellSlots=1;
    TestTrue(TEXT("Bounded uncatalogued fixture still starts"),Battle.Start(Custom,42,Error));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMartialBlockedPushTest, "Project.Expedition.Catalog.MartialBlockedPush", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMartialBlockedPushTest::RunTest(const FString& Parameters)
{
    for (int32 Obstacle=0;Obstacle<3;++Obstacle)
    {
        FString Error; FCombatantState Hero;
        FBuildCatalog::Create(TEXT("monk.open-hand"),1,TEXT("Monk"),Hero); Hero.Cell={1,1}; Hero.InitiativeBonus=30;
        FCombatantState Enemy; Enemy.Id=20; Enemy.Name=Enemy.Role=TEXT("Enemy"); Enemy.Side=ECombatSide::Enemy; Enemy.Cell={2,1}; Enemy.HP=Enemy.MaxHP=20;
        FCombatEncounter Encounter; Encounter.Combatants={Hero,Enemy};
        if (Obstacle==0) { Encounter.BlockedCells={{3,1}}; }
        else if (Obstacle==1) { auto Occupant=Enemy; Occupant.Id=21; Occupant.Cell={3,1}; Encounter.Combatants.Add(Occupant); }
        else { Encounter.Width=3; }
        FCombatBattle Battle;
        if (!TestTrue(TEXT("Start blocked/occupied/edge push"),Battle.Start(Encounter,42,Error))) { continue; }
        const auto Before=Battle.Snapshot(); const auto Preview=Battle.Forecast(1,ECombatCommandType::Special1,20);
        TestEqual(TEXT("Blocked push still forecasts five damage"),Preview.MinAmount,5);
        const auto Result=Battle.Submit({ECombatCommandType::Special1,1,{},20});
        TestTrue(TEXT("Blocked push still commits"),Result.bAccepted);
        TestEqual(TEXT("Blocked victim loses five HP"),Battle.Find(20)->HP,15);
        TestEqual(TEXT("Blocked victim stays in original cell"),Battle.Find(20)->Cell,FIntPoint(2,1));
        TestEqual(TEXT("Only committed damage fact, no displacement"),Result.Effects.Num(),1);
        if (Result.Effects.Num()==1)
        {
            TestEqual(TEXT("Damage fact category"),Result.Effects[0].Effect,EAbilityEffect::Strike);
            TestEqual(TEXT("Damage fact amount"),Result.Effects[0].Amount,5);
        }
        if (Obstacle==1) { TestEqual(TEXT("Occupied destination is not damaged"),Battle.Find(21)->HP,20); }
        TestEqual(TEXT("Blocked push still spends one use"),Battle.Find(1)->KitUses,1);
        TestTrue(TEXT("Blocked push retains action"),Battle.Snapshot().bActionAvailable);
        TestFalse(TEXT("Blocked push spends bonus"),Battle.Snapshot().bBonusActionAvailable);
        FExpeditionPlayback Show; Show.Reset(Before); Show.Enqueue({Result},Battle.Snapshot()); Show.Finish();
        TestEqual(TEXT("Playback keeps blocked victim in place"),Show.Unit(20)->Position,FVector2D(2,1));
        TestEqual(TEXT("Playback records actual loss"),Show.Unit(20)->HP,15);
    }
    return true;
}
#endif
