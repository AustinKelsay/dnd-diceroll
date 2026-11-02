#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Party/BuildCatalog.h"
#include "Presentation/ExpeditionPlayback.h"
#include "Persistence/ExpeditionSave.h"
#include "ExpeditionTestSupport.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FArcaneRosterTest, "Project.Expedition.Catalog.ArcaneRoster", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FArcaneRosterTest::RunTest(const FString& Parameters)
{
    FCombatantState Hero;
    TestTrue(TEXT("Draconic Bloodline can begin a new expedition"), FBuildCatalog::Create(TEXT("sorcerer.draconic-bloodline"),1,TEXT("Ember"),Hero));
    auto Encounter = FCombatBattle::TutorialEncounter();
    Encounter.BlockedCells.Reset();
    Encounter.Combatants[3].InitiativeBonus = 30;
    Encounter.Combatants[3].Cell = {3,3};
    Encounter.Combatants[4].Cell = {4,3};
    Encounter.Combatants[5].Cell = {4,4};
    FCombatBattle Battle; FString Error;
    if (TestTrue(TEXT("Start familiar Wizard scenario"), Battle.Start(Encounter,42,Error)))
    {
        const int32 Before = Battle.Find(Encounter.Combatants[4].Id)->HP;
        const auto Result = Battle.Submit({ECombatCommandType::Special2,Encounter.Combatants[3].Id,{},Encounter.Combatants[4].Id});
        TestTrue(TEXT("Familiar Wizard casts Sculpted Burst"), Result.bAccepted);
        TestEqual(TEXT("Sculpted Burst removes exactly three primary HP"), Before-Battle.Find(Encounter.Combatants[4].Id)->HP,3);
    }
    return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FArcaneWorkedTest, "Project.Expedition.Catalog.ArcaneWorkedOutcomes", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FArcaneWorkedTest::RunTest(const FString& Parameters)
{
    // Literal worked outcomes: primary at (2,1), secondary at (2,2), ally at (1,2).
    struct FCase { const TCHAR* Build; int32 Slot, Target, Loss, Heal, Guard; bool Swap, Push, Area, Bonus, Surge; };
    const FCase Cases[] = {
        {TEXT("sorcerer.draconic-bloodline"),1,20,5,0,0,false,false,true,false,false},
        {TEXT("sorcerer.draconic-bloodline"),2,1,0,0,3,false,false,false,true,false},
        {TEXT("sorcerer.wild-magic"),1,20,0,0,0,false,false,false,false,true},
        {TEXT("sorcerer.wild-magic"),2,2,0,0,0,true,false,false,true,false},
        {TEXT("sorcerer.aberrant-mind"),1,20,5,0,0,false,true,false,false,false},
        {TEXT("sorcerer.aberrant-mind"),2,2,0,0,2,false,false,false,true,false},
        {TEXT("sorcerer.clockwork-soul"),1,2,0,0,4,false,false,false,true,false},
        {TEXT("sorcerer.clockwork-soul"),2,2,0,6,0,false,false,false,false,false},
        {TEXT("warlock.archfey"),1,20,4,0,0,false,true,false,true,false},
        {TEXT("warlock.archfey"),2,2,0,0,0,true,false,false,false,false},
        {TEXT("warlock.fiend"),1,20,8,0,0,false,false,false,false,false},
        {TEXT("warlock.fiend"),2,1,0,5,0,false,false,false,true,false},
        {TEXT("warlock.great-old-one"),1,2,0,0,3,false,false,false,false,false},
        {TEXT("warlock.great-old-one"),2,20,4,0,0,false,true,false,true,false},
        {TEXT("warlock.fathomless"),1,20,5,0,0,false,true,false,false,false},
        {TEXT("warlock.fathomless"),2,1,0,0,3,false,false,false,true,false},
        {TEXT("warlock.genie"),1,2,0,0,0,true,false,false,true,false},
        {TEXT("warlock.genie"),2,1,0,0,4,false,false,false,false,false},
        {TEXT("wizard.abjuration"),1,2,0,0,4,false,false,false,true,false},
        {TEXT("wizard.abjuration"),2,20,3,0,0,false,false,true,false,false},
        {TEXT("wizard.conjuration"),1,2,0,0,0,true,false,false,true,false},
        {TEXT("wizard.conjuration"),2,20,5,0,0,false,true,false,false,false},
        {TEXT("wizard.divination"),1,20,6,0,0,false,false,false,false,false},
        {TEXT("wizard.divination"),2,2,0,0,2,false,false,false,true,false},
        {TEXT("wizard.enchantment"),1,20,3,0,0,false,true,false,true,false},
        {TEXT("wizard.enchantment"),2,2,0,0,0,true,false,false,false,false},
        {TEXT("wizard.evocation"),1,20,12,0,0,false,false,false,false,false},
        {TEXT("wizard.evocation"),2,20,3,0,0,false,false,true,true,false},
        {TEXT("wizard.illusion"),1,1,0,0,4,false,false,false,true,false},
        {TEXT("wizard.illusion"),2,2,0,0,0,true,false,false,false,false},
        {TEXT("wizard.necromancy"),1,20,7,0,0,false,false,false,false,false},
        {TEXT("wizard.necromancy"),2,2,0,5,0,false,false,false,true,false},
        {TEXT("wizard.transmutation"),1,2,0,0,0,true,false,false,false,false},
        {TEXT("wizard.transmutation"),2,2,0,7,0,false,false,false,true,false},
        {TEXT("wizard.bladesinging"),1,20,7,0,0,false,false,false,true,false},
        {TEXT("wizard.bladesinging"),2,1,0,0,3,false,false,false,false,false},
        {TEXT("wizard.order-of-scribes"),1,20,3,0,0,false,false,true,true,false},
        {TEXT("wizard.order-of-scribes"),2,2,0,0,0,true,false,false,false,false},
    };
    FString Error;
    for (const auto& C : Cases)
    {
        AddInfo(FString::Printf(TEXT("%s slot %d"),C.Build,C.Slot));
        FCombatantState Hero;
        if (!TestTrue(TEXT("Authored subclass"),FBuildCatalog::Create(C.Build,1,TEXT("Hero"),Hero))) { continue; }
        Hero.Cell={1,1}; Hero.InitiativeBonus=30; Hero.HP=10;
        FCombatantState Ally; Ally.Id=2; Ally.Name=Ally.Role=TEXT("Ally"); Ally.Cell={1,2}; Ally.HP=5;
        FCombatantState Enemy; Enemy.Id=20; Enemy.Name=Enemy.Role=TEXT("Primary"); Enemy.Side=ECombatSide::Enemy; Enemy.Cell={2,1}; Enemy.HP=Enemy.MaxHP=40; Enemy.InitiativeBonus=-10;
        auto Secondary=Enemy; Secondary.Id=21; Secondary.Name=TEXT("Secondary"); Secondary.Cell={2,2};
        FCombatEncounter Encounter; Encounter.Combatants={Hero,Ally,Enemy,Secondary};
        FCombatBattle Battle;
        if (!TestTrue(TEXT("Start worked scenario"),Battle.Start(Encounter,42,Error))) { continue; }
        const auto Action=FBuildCatalog::Abilities(Hero)[C.Slot-1].Command;
        const auto Before=Battle.Snapshot();
        for (int32 Repeat=0;Repeat<3;++Repeat) { TestTrue(TEXT("Repeated preview legal"),Battle.Forecast(1,Action,C.Target).bValid); }
        TestTrue(TEXT("Repeated previews preserve every value including RNG"),FCombatSnapshot::StaticStruct()->CompareScriptStruct(&Before,&Battle.Snapshot(),0));
        FCombatBattle Continued; TestTrue(TEXT("Direct restore"),Continued.Restore(Before,Error));
        const auto Preview=Battle.Forecast(1,Action,C.Target);
        const auto Result=Battle.Submit({Action,1,{},C.Target});
        TestTrue(TEXT("Special commits"),Result.bAccepted);
        TestEqual(TEXT("Exactly one shared Kit use spent"),Battle.Find(1)->KitUses,1);
        if(C.Area)
        {
            TestEqual(TEXT("Area forecast target count"),Preview.Effects.Num(),2);
            TestEqual(TEXT("Forecast primary first"),Preview.Effects[0].TargetId,20);
            TestEqual(TEXT("Forecast secondary next"),Preview.Effects[1].TargetId,21);
            TestEqual(TEXT("Committed area fact count"),Result.Effects.Num(),2);
            for(int32 I=0;I<2;++I) { TestEqual(TEXT("Forecast agrees with ordered commit target"),Preview.Effects[I].TargetId,Result.Effects[I].TargetId); TestEqual(TEXT("Forecast agrees with actual per-target loss"),Preview.Effects[I].Amount,Result.Effects[I].Amount); }
        }
        if (C.Surge)
        {
            TestTrue(TEXT("Chaos Bolt removes exactly four or eight HP"),Battle.Find(20)->HP==36 || Battle.Find(20)->HP==32);
            FRandomStream Expected; Expected.Initialize(Before.RandomState); Expected.RandRange(1,2);
            TestEqual(TEXT("Exactly one committed RNG draw"),Battle.Snapshot().RandomState,Expected.GetCurrentSeed());
        }
        else { TestEqual(TEXT("Primary literal loss"),40-Battle.Find(20)->HP,C.Loss); }
        TestEqual(TEXT("Secondary literal loss"),40-Battle.Find(21)->HP,C.Area?C.Loss:0);
        TestEqual(TEXT("Literal healing"),Battle.Find(C.Target)->HP-(C.Target==1?10:C.Target==2?5:40),C.Target==20?-(C.Surge?40-Battle.Find(20)->HP:C.Loss):C.Heal);
        TestEqual(TEXT("Owned ward amount"),Battle.Find(1)->GuardAmount,C.Guard);
        TestEqual(TEXT("Safe area excludes adjacent ally"),Battle.Find(2)->HP,C.Target==2?5+C.Heal:5);
        TestEqual(TEXT("Exchange moves owner"),Battle.Find(1)->Cell,C.Swap?FIntPoint(1,2):FIntPoint(1,1));
        TestEqual(TEXT("Push moves surviving primary"),Battle.Find(20)->Cell,C.Push?FIntPoint(3,1):FIntPoint(2,1));
        TestEqual(TEXT("Action budget"),Battle.Snapshot().bActionAvailable,C.Bonus);
        TestEqual(TEXT("Bonus budget"),Battle.Snapshot().bBonusActionAvailable,!C.Bonus);
        Continued.Submit({Action,1,{},C.Target});
        TestTrue(TEXT("Restored continuation identical including RNG"),FCombatSnapshot::StaticStruct()->CompareScriptStruct(&Continued.Snapshot(),&Battle.Snapshot(),0));
        for(int32 Mode=0;Mode<3;++Mode)
        {
            FExpeditionPlayback Playback; Playback.Reset(Before); Playback.Enqueue({Result},Battle.Snapshot());
            if(Mode==2) { Playback.Finish(); } else { for(int32 Tick=0;Tick<1000 && Playback.IsBusy();++Tick) { Playback.Tick(Mode?0.5f:0.02f); } }
            for(const auto& U:Battle.Snapshot().Encounter.Combatants) { TestEqual(TEXT("Playback HP converges"),Playback.Unit(U.Id)->HP,U.HP); TestEqual(TEXT("Playback position converges"),Playback.Unit(U.Id)->Position,FVector2D(U.Cell)); }
        }
        for(int32 Constraint=0;Constraint<2;++Constraint)
        {
            auto Restricted=Encounter;
            auto* T=Restricted.Combatants.FindByPredicate([&](const auto& U){return U.Id==C.Target;});
            if(Constraint==0) { T->HP=0; T->Life=ECombatLife::Dead; }
            else { if(C.Target==1) { continue; } T->Cell={7,7}; }
            FCombatBattle R; if(!TestTrue(TEXT("Start dead or out-of-range scenario"),R.Start(Restricted,42,Error))) { continue; }
            // A dead owner cannot act. Other targets are rejected while the owner retains the turn.
            if(C.Target==1) { continue; }
            const auto Saved=R.Snapshot();
            TestFalse(TEXT("Dead/distant target has no forecast"),R.Forecast(1,Action,C.Target).bValid);
            TestFalse(TEXT("Dead/distant command rejected"),R.Submit({Action,1,{},C.Target}).bAccepted);
            TestTrue(TEXT("Rejected target preserves full state and RNG"),FCombatSnapshot::StaticStruct()->CompareScriptStruct(&Saved,&R.Snapshot(),0));
        }
        for (int32 BadTarget : {999, C.Target==20?2:20})
        {
            FCombatBattle Rejected; Rejected.Restore(Before,Error); const auto Saved=Rejected.Snapshot();
            TestFalse(TEXT("Wrong target rejected"),Rejected.Submit({Action,1,{},BadTarget}).bAccepted);
            TestTrue(TEXT("Rejected command fully atomic"),FCombatSnapshot::StaticStruct()->CompareScriptStruct(&Saved,&Rejected.Snapshot(),0));
        }
    }
    for (int32 Index=0;Index<UE_ARRAY_COUNT(Cases);Index+=2)
    {
        const auto& C=Cases[Index]; const auto& D=Cases[Index+1];
        FCombatantState Hero; FBuildCatalog::Create(C.Build,1,TEXT("Hero"),Hero); Hero.Cell={1,1}; Hero.HP=10; Hero.InitiativeBonus=30;
        FCombatantState Ally; Ally.Id=2; Ally.Name=Ally.Role=TEXT("Ally"); Ally.Cell={1,2}; Ally.HP=5;
        FCombatantState Enemy; Enemy.Id=20; Enemy.Name=Enemy.Role=TEXT("Enemy"); Enemy.Side=ECombatSide::Enemy; Enemy.Cell={2,1}; Enemy.HP=Enemy.MaxHP=40; Enemy.InitiativeBonus=-10;
        FCombatEncounter E; E.Combatants={Hero,Ally,Enemy}; FCombatBattle B;
        if(!TestTrue(TEXT("Start sibling combination"),B.Start(E,42,Error))) { continue; }
        TestTrue(TEXT("First sibling special"),B.Submit({FBuildCatalog::Abilities(Hero)[0].Command,1,{},C.Target}).bAccepted);
        TestTrue(TEXT("Second sibling special"),B.Submit({ECombatCommandType::Special2,1,{},D.Target}).bAccepted);
        TestEqual(TEXT("One common pool depleted"),B.Find(1)->KitUses,0);
        const auto Spent=B.Snapshot();
        TestFalse(TEXT("No third special from shared pool"),B.Submit({FBuildCatalog::Abilities(Hero)[0].Command,1,{},C.Target}).bAccepted);
        TestTrue(TEXT("Spent rejection includes RNG"),FCombatSnapshot::StaticStruct()->CompareScriptStruct(&Spent,&B.Snapshot(),0));
    }
    for(const auto* Build:{TEXT("sorcerer.wild-magic"),TEXT("wizard.evocation")})
    {
        FCombatantState Hero; FBuildCatalog::Create(Build,1,TEXT("Caster"),Hero); Hero.Cell={0,0}; Hero.InitiativeBonus=30;
        FCombatantState Enemy; Enemy.Id=20; Enemy.Name=Enemy.Role=TEXT("Primary"); Enemy.Side=ECombatSide::Enemy; Enemy.Cell={4,0}; Enemy.HP=Enemy.MaxHP=40; Enemy.InitiativeBonus=-10;
        FCombatEncounter E; E.Combatants={Hero,Enemy}; E.BlockedCells={{2,0}};
        const auto Action=FBuildCatalog::Abilities(Hero)[0].Command;
        const int32 Range=FBuildCatalog::Abilities(Hero)[0].Range;
        E.Combatants[1].Cell={Range,0};
        FCombatBattle B; TestTrue(TEXT("Start actual intervening-wall scenario"),B.Start(E,42,Error));
        const auto Before=B.Snapshot();
        TestFalse(TEXT("Wall blocks forecast at range boundary"),B.Forecast(1,Action,20).bValid);
        TestFalse(TEXT("Wall blocks commit"),B.Submit({Action,1,{},20}).bAccepted);
        TestTrue(TEXT("Occluded rejection retains RNG and full snapshot"),FCombatSnapshot::StaticStruct()->CompareScriptStruct(&Before,&B.Snapshot(),0));
        E.BlockedCells.Reset(); B.Start(E,42,Error);
        TestTrue(TEXT("Exact authored range boundary is legal"),B.Forecast(1,Action,20).bValid);
        E.Combatants[1].Cell={Range+1,0}; B.Start(E,42,Error);
        TestFalse(TEXT("One cell beyond range is illegal"),B.Forecast(1,Action,20).bValid);
    }
    TestEqual(TEXT("Exactly 70 implemented kits"),FBuildCatalog::All().Num(),70);
    TestNull(TEXT("Interim Wizard removed"),FBuildCatalog::Find(TEXT("wizard.base")));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FArcaneJsonTest, "Project.Expedition.Catalog.ArcaneJsonContinuity", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FArcaneJsonTest::RunTest(const FString& Parameters)
{
    FString Error;
    for (const auto* Build : {TEXT("wizard.abjuration"),TEXT("sorcerer.clockwork-soul"),TEXT("warlock.genie"),TEXT("sorcerer.wild-magic")})
    {
        AddInfo(Build); TArray<FPartyBuildSelection> Party;
        for(int32 Id=1;Id<=4;++Id) { FPartyBuildSelection S; S.Id=Id; S.Name=FString::Printf(TEXT("Hero %d"),Id); S.BuildId=Build; Party.Add(S); }
        FExpeditionRun Run; if(!TestTrue(TEXT("Duplicate arcane party"),Run.NewRun(42,Party,Error,TEXT("arcane-json")))) { continue; }
        TestTrue(TEXT("First route retains party"),Run.ChooseRoute(Run.Progress().Offers[0].Id,Error));
        while(Run.Battle().Find(Run.Battle().ActiveId())->Side==ECombatSide::Enemy) { Run.PlayEnemyTurn(); }
        const int32 Id=Run.Battle().ActiveId();
        // Companion Guard is the second slot; others exercise exchange / self protection.
        const auto Action=ECombatCommandType::Special1;
        if(FString(Build)==TEXT("sorcerer.wild-magic"))
        {
            // A valid spatial checkpoint fixture puts the active caster near a target;
            // public Restore validates it before the public command authors surge state.
            auto Fixture=Run.Snapshot();
            const auto* Enemy=Fixture.Battle.Encounter.Combatants.FindByPredicate([](const auto& U){return U.Side==ECombatSide::Enemy;});
            bool Placed=false;
            for(int32 Y=0;Y<Fixture.Battle.Encounter.Height && !Placed;++Y)
            for(int32 X=0;X<Fixture.Battle.Encounter.Width && !Placed;++X)
            {
                const FIntPoint Cell(X,Y);
                if(ExpeditionTestSupport::Distance(Cell,Enemy->Cell)>4 || Fixture.Battle.Encounter.BlockedCells.Contains(Cell) || Fixture.Battle.Encounter.Combatants.ContainsByPredicate([&](const auto& U){return U.Cell==Cell;})) { continue; }
                auto Candidate=Fixture;
                Candidate.Battle.Encounter.Combatants.FindByPredicate([&](const auto& U){return U.Id==Id;})->Cell=Cell;
                FExpeditionRun Probe;
                if(Probe.Restore(Candidate,Error) && !Probe.Battle().LegalTargets(Id,Action).IsEmpty()) { Run=MoveTemp(Probe); Placed=true; }
            }
            if(!TestTrue(TEXT("Restore valid in-range surge fixture"),Placed)) { continue; }
        }
        const auto Targets=Run.Battle().LegalTargets(Id,Action);
        if(!TestFalse(TEXT("Generated party has valid ally/self target"),Targets.IsEmpty())) { continue; }
        TestTrue(TEXT("Commit actual arcane effect"),Run.Submit({Action,Id,{},Targets[0]}).bAccepted);
        const FString Directory=FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("Automation/Arcane"),FGuid::NewGuid().ToString());
        const FString Path=FPaths::Combine(Directory,TEXT("current-v1.json"));
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
        for (int32 Mutation=0;Mutation<15;++Mutation)
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
            else if (Mutation==8) { Unit->RemoveField(TEXT("GuardAmount")); }
            else if (Mutation==9) { Unit->RemoveField(TEXT("LastSurge")); }
            else if (Mutation==10) { Unit->SetNumberField(TEXT("LastSurge"),0.5); }
            else if (Mutation==11) { Unit->SetStringField(TEXT("LastSurge"),TEXT("1")); }
            else if (Mutation==12) { Unit->SetNumberField(TEXT("LastSurge"),3); }
            else if(Mutation==13) { Unit->SetNumberField(TEXT("LastSurge"),FString(Build)==TEXT("sorcerer.wild-magic")?3:1); }
            else { Unit->SetNumberField(TEXT("KitUses"),2); Unit->SetNumberField(TEXT("LastSurge"),1); }
            FString Malformed; FJsonSerializer::Serialize(Object.ToSharedRef(),TJsonWriterFactory<>::Create(&Malformed));
            auto Output=Decoded;
            TestFalse(TEXT("Malformed arcane wire state rejected"),FExpeditionSave::Decode(Malformed,Output,Error));
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FArcaneRunTest, "Project.Expedition.Catalog.ArcaneRunProgression", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FArcaneRunTest::RunTest(const FString& Parameters)
{
    for(bool Duplicate : {false,true})
    {
        FString Error; TArray<FPartyBuildSelection> Party;
        const TCHAR* Mixed[]={TEXT("sorcerer.draconic-bloodline"),TEXT("warlock.fiend"),TEXT("wizard.abjuration"),TEXT("wizard.evocation")};
        for(int32 Id=1;Id<=4;++Id) { FPartyBuildSelection P; P.Id=Id; P.Name=FString::Printf(TEXT("Slot %d"),Id); P.BuildId=Duplicate?TEXT("wizard.evocation"):Mixed[Id-1]; Party.Add(P); }
        FExpeditionRun Run; TestTrue(TEXT("Construct arcane run"),Run.NewRun(42,Party,Error,Duplicate?TEXT("duplicate-arcane"):TEXT("mixed-arcane")));
        for(int32 Stage=0;Stage<5;++Stage)
        {
            if(!TestTrue(TEXT("Choose actual generated route"),Run.ChooseRoute(Run.Progress().Offers[0].Id,Error))) { break; }
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
        TestEqual(TEXT("Five encounters completed by worked arcane party"),Run.Progress().EncountersCompleted,5);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMissileRecoveryTest, "Project.Expedition.Catalog.MissileSpentPoolsAndRollback", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMissileRecoveryTest::RunTest(const FString& Parameters)
{
    FString Error; TArray<FPartyBuildSelection> Party;
    for(int32 Id=1;Id<=4;++Id) { FPartyBuildSelection P; P.Id=Id; P.Name=FString::Printf(TEXT("Hero %d"),Id); P.BuildId=Id==1?TEXT("wizard.evocation"):TEXT("cleric.light"); Party.Add(P); }
    FExpeditionRun Run;
    if(!TestTrue(TEXT("Create Missile party"),Run.NewRun(42,Party,Error,TEXT("missile-recovery"))) || !TestTrue(TEXT("First route"),Run.ChooseRoute(Run.Progress().Offers[0].Id,Error))) { return false; }
    const FString Directory=FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("Automation/Missile"),FGuid::NewGuid().ToString());
    const FString Path=FPaths::Combine(Directory,TEXT("current-v1.json"));
    int32 Casts=0;
    for(int32 Turn=0;Turn<80 && Casts<2 && Run.Progress().Phase==EExpeditionPhase::Combat;++Turn)
    {
        const auto& B=Run.Battle(); const int32 Id=B.ActiveId(); const auto* Actor=B.Find(Id);
        if(Actor->Side==ECombatSide::Enemy) { Run.PlayEnemyTurn(); continue; }
        if(Id==1)
        {
            auto Targets=B.LegalTargets(1,ECombatCommandType::MagicMissile);
            if(Targets.IsEmpty())
            {
                auto Moves=B.LegalMoves(1);
                Moves.Sort([&](FIntPoint A,FIntPoint C) {
                    auto Nearest=[&](FIntPoint Cell) { int32 Best=1000; for(const auto& U:B.Snapshot().Encounter.Combatants) { if(U.Side==ECombatSide::Enemy && U.Life==ECombatLife::Conscious) { Best=FMath::Min(Best,ExpeditionTestSupport::Distance(Cell,U.Cell)); } } return Best; };
                    return Nearest(A)==Nearest(C)?(A.Y==C.Y?A.X<C.X:A.Y<C.Y):Nearest(A)<Nearest(C);
                });
                if(!Moves.IsEmpty()) { Run.Submit({ECombatCommandType::Move,1,Moves[0]}); }
                Targets=B.LegalTargets(1,ECombatCommandType::MagicMissile);
            }
            if(!Targets.IsEmpty())
            {
                const auto Before=Run.Snapshot();
                TestFalse(TEXT("Retired Special1 alias rejected"),Run.Submit({ECombatCommandType::Special1,1,{},Targets[0]}).bAccepted);
                TestTrue(TEXT("Alias rejection retains every value/RNG"),ExpeditionTestSupport::SameRun(Before,Run.Snapshot()));
                FExpeditionSaveData Data; Data.Current=Before;
                if(!TestTrue(TEXT("Write pre-Missile checkpoint"),FExpeditionSave::Write(Path,Data,Error))) { return false; }
                FExpeditionSession Session(Path);
                if(!TestTrue(TEXT("Resume before actual Missile"),Session.Resume(Error))) { return false; }
                FString BytesBefore; TestTrue(TEXT("Read checkpoint bytes"),FFileHelper::LoadFileToString(BytesBefore,*Path));
                IFileManager::Get().MakeDirectory(*(Path+TEXT(".pending")),true);
                const FCombatCommand Command{ECombatCommandType::MagicMissile,1,{},Targets[0]};
                TestFalse(TEXT("Failed write rejects RNG-consuming Missile"),Session.Submit(Command).bAccepted);
                TestTrue(TEXT("Failed Missile retains all effects/deaths/resources/RNG"),ExpeditionTestSupport::SameRun(Before,Session.Run().Snapshot()));
                FString BytesAfter; TestTrue(TEXT("Read retained checkpoint bytes"),FFileHelper::LoadFileToString(BytesAfter,*Path));
                TestEqual(TEXT("Failed Missile preserves exact file bytes"),BytesAfter,BytesBefore);
                FExpeditionSaveData Disk; TestTrue(TEXT("Read unchanged checkpoint"),FExpeditionSave::Read(Path,Disk,Error));
                TestTrue(TEXT("Disk state unchanged"),ExpeditionTestSupport::SameRun(Before,Disk.Current));
                IFileManager::Get().DeleteDirectory(*(Path+TEXT(".pending")),false,true);
                TestTrue(TEXT("Actual Missile commits on public turn"),Run.Submit(Command).bAccepted);
                TestTrue(TEXT("Retry commits same Missile"),Session.Submit(Command).bAccepted);
                TestTrue(TEXT("Retry equals uninterrupted RNG/damage/state"),ExpeditionTestSupport::SameRun(Run.Snapshot(),Session.Run().Snapshot()));
                TestTrue(TEXT("Missile consumed RNG"),Run.Snapshot().Battle.RandomState!=Before.Battle.RandomState);
                ++Casts;
            }
        }
        if(Run.Progress().Phase==EExpeditionPhase::Combat) { Run.Submit({ECombatCommandType::EndTurn,Id}); }
    }
    if(!TestEqual(TEXT("Two real Missile turns"),Casts,2)) { return false; }
    TestEqual(TEXT("Missiles empty Kit"),Run.Battle().Find(1)->KitUses,0);
    TestEqual(TEXT("Missiles empty Slots"),Run.Battle().Find(1)->SpellSlots,0);
    if(!TestTrue(TEXT("Complete encounter"),ExpeditionTestSupport::FinishCombat(Run)) || !TestTrue(TEXT("Reward"),Run.ClaimReward(1,Error)) || !TestTrue(TEXT("One camp"),Run.Recover(true,Error))) { return false; }
    TestEqual(TEXT("Camp Kit below cap"),Run.Battle().Find(1)->KitUses,1);
    TestEqual(TEXT("Camp Slot below cap"),Run.Battle().Find(1)->SpellSlots,1);
    if(!TestTrue(TEXT("Next route"),Run.ChooseRoute(Run.Progress().Offers[0].Id,Error))) { return false; }
    FExpeditionSaveData Data; Data.Current=Run.Snapshot();
    TestTrue(TEXT("Write post-camp route"),FExpeditionSave::Write(Path,Data,Error));
    FExpeditionSession Fresh(Path); TestTrue(TEXT("Fresh Resume"),Fresh.Resume(Error));
    TestTrue(TEXT("Complete state/RNG survives route and Resume"),ExpeditionTestSupport::SameRun(Data.Current,Fresh.Run().Snapshot()));
    TestEqual(TEXT("Resume Kit remains one"),Fresh.Run().Battle().Find(1)->KitUses,1);
    TestEqual(TEXT("Resume Slots remain one"),Fresh.Run().Battle().Find(1)->SpellSlots,1);
    IFileManager::Get().DeleteDirectory(*Directory,false,true);
    return true;
}

#endif
