#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Party/BuildCatalog.h"
#include "Presentation/ExpeditionPlayback.h"
#include "Persistence/ExpeditionSave.h"
#include "ExpeditionTestSupport.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSupportRosterTest,"Project.Expedition.Catalog.SupportRoster",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSupportRosterTest::RunTest(const FString& Parameters)
{
    FCombatantState Hero;
    TestTrue(TEXT("Life is an authored selectable build"),FBuildCatalog::Create(TEXT("cleric.life"),1,TEXT("Sera"),Hero));
    TestEqual(TEXT("All approved support kits available"),FBuildCatalog::All().Num(),66);
    auto E=FCombatBattle::TutorialEncounter(); E.BlockedCells.Reset(); E.Combatants[2].InitiativeBonus=30;
    FCombatBattle B; FString Error;
    if(TestTrue(TEXT("Start support behavior"),B.Start(E,42,Error))) {
        const auto R=B.Submit({ECombatCommandType::Special2,3,{},1});
        TestTrue(TEXT("Life grants an ally Sanctuary"),R.bAccepted);
        TestEqual(TEXT("Sanctuary grants concrete reduction"),B.Find(3)->GuardAmount,3);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSupportWorkedTest, "Project.Expedition.Catalog.SupportWorkedOutcomes", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSupportWorkedTest::RunTest(const FString& Parameters)
{
    // Literal worked outcomes: primary at (2,1), secondary at (2,2), ally at (1,2).
    struct FCase { const TCHAR* Build; int32 Slot, Target, Loss, Heal, Guard; bool Swap, Push, Area, Bonus, Cure; };
    const FCase Cases[] = {
        {TEXT("cleric.knowledge"),1,2,0,0,0,true,false,false,true,false},
        {TEXT("cleric.knowledge"),2,20,6,0,0,false,false,false,false,false},
        {TEXT("cleric.life"),1,1,0,0,0,false,false,false,false,true},
        {TEXT("cleric.life"),2,2,0,0,3,false,false,false,true,false},
        {TEXT("cleric.light"),1,20,5,0,0,false,false,true,false,false},
        {TEXT("cleric.light"),2,1,0,0,3,false,false,false,true,false},
        {TEXT("cleric.nature"),1,20,4,0,0,false,true,false,false,false},
        {TEXT("cleric.nature"),2,2,0,7,0,false,false,false,true,false},
        {TEXT("cleric.tempest"),1,20,5,0,0,false,true,false,true,false},
        {TEXT("cleric.tempest"),2,20,4,0,0,false,false,true,false,false},
        {TEXT("cleric.trickery"),1,1,0,0,4,false,false,false,true,false},
        {TEXT("cleric.trickery"),2,2,0,0,0,true,false,false,false,false},
        {TEXT("cleric.war"),1,20,7,0,0,false,false,false,true,false},
        {TEXT("cleric.war"),2,2,0,0,3,false,false,false,false,false},
        {TEXT("cleric.order"),1,2,0,0,0,true,false,false,false,false},
        {TEXT("cleric.order"),2,2,0,0,3,false,false,false,true,false},
        {TEXT("cleric.peace"),1,2,0,8,0,false,false,false,false,false},
        {TEXT("cleric.peace"),2,2,0,0,0,true,false,false,true,false},
        {TEXT("cleric.twilight"),1,2,0,0,4,false,false,false,false,false},
        {TEXT("cleric.twilight"),2,20,3,0,0,false,false,true,true,false},
        {TEXT("druid.land"),1,20,4,0,0,false,false,true,false,false},
        {TEXT("druid.land"),2,2,0,0,2,false,false,false,true,false},
        {TEXT("druid.moon"),1,1,0,0,4,false,false,false,true,false},
        {TEXT("druid.moon"),2,20,7,0,0,false,false,false,false,false},
        {TEXT("druid.spores"),1,20,4,0,0,false,false,true,true,false},
        {TEXT("druid.spores"),2,1,0,7,0,false,false,false,false,false},
        {TEXT("druid.stars"),1,20,4,0,0,false,false,false,true,false},
        {TEXT("druid.stars"),2,2,0,6,0,false,false,false,false,false},
        {TEXT("druid.wildfire"),1,20,4,0,0,false,true,false,false,false},
        {TEXT("druid.wildfire"),2,2,0,0,0,true,false,false,true,false},
        {TEXT("bard.lore"),1,20,3,0,0,false,true,false,true,false},
        {TEXT("bard.lore"),2,2,0,7,0,false,false,false,false,false},
        {TEXT("bard.valor"),1,2,0,0,4,false,false,false,true,false},
        {TEXT("bard.valor"),2,20,6,0,0,false,false,false,false,false},
        {TEXT("bard.creation"),1,2,0,0,0,true,false,false,true,false},
        {TEXT("bard.creation"),2,20,5,0,0,false,false,true,false,false},
        {TEXT("bard.eloquence"),1,2,0,0,2,false,false,false,false,false},
        {TEXT("bard.eloquence"),2,20,5,0,0,false,false,false,true,false},
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
        const auto Action=C.Cure?ECombatCommandType::CureWounds:C.Slot==1?ECombatCommandType::Special1:ECombatCommandType::Special2;
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
        if (C.Cure)
        {
            TestEqual(TEXT("Seed42 after four initiatives: Cure 6+4+3 clips to twelve missing HP"),Battle.Find(1)->HP,22);
            TestEqual(TEXT("Cure uses two d8"),Result.EffectDice.Num(),2);
            if(Result.EffectDice.Num()==2) { TestEqual(TEXT("First literal d8"),Result.EffectDice[0].Value,6); TestEqual(TEXT("Second literal d8"),Result.EffectDice[1].Value,4); }
            FRandomStream Expected; Expected.Initialize(Before.RandomState); Expected.RandRange(1,8); Expected.RandRange(1,8);
            TestEqual(TEXT("Exactly two committed RNG draws"),Battle.Snapshot().RandomState,Expected.GetCurrentSeed());
        }
        else { TestEqual(TEXT("Primary literal loss"),40-Battle.Find(20)->HP,C.Loss); }
        TestEqual(TEXT("Secondary literal loss"),40-Battle.Find(21)->HP,C.Area?C.Loss:0);
        TestEqual(TEXT("Literal healing"),Battle.Find(C.Target)->HP-(C.Target==1?10:C.Target==2?5:40),C.Target==20?-C.Loss:C.Cure?Result.Healing:C.Heal);
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
        TestTrue(TEXT("First sibling special"),B.Submit({C.Cure?ECombatCommandType::CureWounds:ECombatCommandType::Special1,1,{},C.Target}).bAccepted);
        TestTrue(TEXT("Second sibling special"),B.Submit({ECombatCommandType::Special2,1,{},D.Target}).bAccepted);
        TestEqual(TEXT("One common pool depleted"),B.Find(1)->KitUses,0);
        if(C.Cure) { TestEqual(TEXT("Only Cure spends a Slot"),B.Find(1)->SpellSlots,1); const auto Saved=B.Snapshot(); TestFalse(TEXT("No hidden generic first-slot healing"),B.Submit({ECombatCommandType::Special1,1,{},1}).bAccepted); TestTrue(TEXT("Hidden-slot rejection is atomic"),FCombatSnapshot::StaticStruct()->CompareScriptStruct(&Saved,&B.Snapshot(),0)); }
        const auto Spent=B.Snapshot();
        TestFalse(TEXT("No third special from shared pool"),B.Submit({C.Cure?ECombatCommandType::CureWounds:ECombatCommandType::Special1,1,{},C.Target}).bAccepted);
        TestTrue(TEXT("Spent rejection includes RNG"),FCombatSnapshot::StaticStruct()->CompareScriptStruct(&Spent,&B.Snapshot(),0));
    }
    TestEqual(TEXT("Exactly 66 implemented kits"),FBuildCatalog::All().Num(),66);
    TestNull(TEXT("Interim Cleric removed"),FBuildCatalog::Find(TEXT("cleric.base")));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSupportJsonTest, "Project.Expedition.Catalog.SupportJsonContinuity", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSupportJsonTest::RunTest(const FString& Parameters)
{
    FString Error;
    for (const auto* Build : {TEXT("cleric.order"),TEXT("druid.moon"),TEXT("bard.creation"),TEXT("cleric.life")})
    {
        AddInfo(Build); TArray<FPartyBuildSelection> Party;
        for(int32 Id=1;Id<=4;++Id) { FPartyBuildSelection S; S.Id=Id; S.Name=FString::Printf(TEXT("Hero %d"),Id); S.BuildId=Build; Party.Add(S); }
        FExpeditionRun Run; if(!TestTrue(TEXT("Duplicate support party"),Run.NewRun(42,Party,Error,TEXT("support-json")))) { continue; }
        TestTrue(TEXT("First route retains party"),Run.ChooseRoute(Run.Progress().Offers[0].Id,Error));
        while(Run.Battle().Find(Run.Battle().ActiveId())->Side==ECombatSide::Enemy) { Run.PlayEnemyTurn(); }
        const int32 Id=Run.Battle().ActiveId();
        const bool Life=FString(Build)==TEXT("cleric.life");
        const auto Action=Life?ECombatCommandType::CureWounds:ECombatCommandType::Special1;
        if(Life)
        {
            auto Fixture=Run.Snapshot();
            Fixture.Battle.Encounter.Combatants.FindByPredicate([&](const auto& U){return U.Id==Id;})->HP=10;
            TestTrue(TEXT("Valid injured Life checkpoint fixture"),Run.Restore(Fixture,Error));
        }
        const auto Targets=Run.Battle().LegalTargets(Id,Action);
        if(!TestFalse(TEXT("Generated party has valid ally/self target"),Targets.IsEmpty())) { continue; }
        TestTrue(TEXT("Commit actual support effect"),Run.Submit({Action,Id,{},Targets[0]}).bAccepted);
        const FString Directory=FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("Automation/Support"),FGuid::NewGuid().ToString());
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
        for (int32 Mutation=0;Mutation<16;++Mutation)
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
            else if (Mutation==7) { Unit->SetNumberField(TEXT("SpellSlots"),Life?3:1); }
            else if (Mutation==8) { Unit->RemoveField(TEXT("GuardAmount")); }
            else if (Mutation==9) { Unit->RemoveField(TEXT("LastSurge")); }
            else if (Mutation==10) { Unit->SetNumberField(TEXT("LastSurge"),0.5); }
            else if (Mutation==11) { Unit->SetStringField(TEXT("LastSurge"),TEXT("1")); }
            else if (Mutation==12) { Unit->SetNumberField(TEXT("LastSurge"),3); }
            else if(Mutation==13) { Unit->SetNumberField(TEXT("LastSurge"),FString(Build)==TEXT("sorcerer.wild-magic")?3:1); }
            else if(Mutation==14) { Unit->SetNumberField(TEXT("KitUses"),2); Unit->SetNumberField(TEXT("LastSurge"),1); }
            else { Object->GetObjectField(TEXT("Current"))->GetObjectField(TEXT("Battle"))->SetStringField(TEXT("bActionAvailable"),TEXT("true")); }
            FString Malformed; FJsonSerializer::Serialize(Object.ToSharedRef(),TJsonWriterFactory<>::Create(&Malformed));
            auto Output=Decoded;
            TestFalse(TEXT("Malformed support wire state rejected"),FExpeditionSave::Decode(Malformed,Output,Error));
            TestTrue(TEXT("Malformed JSON decode is atomic"),FExpeditionSaveData::StaticStruct()->CompareScriptStruct(&Decoded,&Output,0));
        }
        FExpeditionRun Resumed; TestTrue(TEXT("Install decoded checkpoint"),Resumed.Restore(Decoded.Current,Error));
        Run.Submit({ECombatCommandType::EndTurn,Id}); Resumed.Submit({ECombatCommandType::EndTurn,Id});
        TestTrue(TEXT("Identical continued command and RNG"),ExpeditionTestSupport::SameRun(Run.Snapshot(),Resumed.Snapshot()));
        auto Bad=Decoded.Current; Bad.Battle.Encounter.Combatants[0].KitUses=3;
        const auto Before=Resumed.Snapshot();
        TestFalse(TEXT("Malformed pool rejected"),Resumed.Restore(Bad,Error));
        TestTrue(TEXT("Failed restore atomic"),ExpeditionTestSupport::SameRun(Before,Resumed.Snapshot()));
        Bad=Decoded.Current; Bad.Battle.Encounter.Combatants[0].SpellSlots=Life?3:1;
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSupportRunTest, "Project.Expedition.Catalog.SupportRunProgression", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSupportRunTest::RunTest(const FString& Parameters)
{
    for(bool Duplicate : {false,true})
    {
        FString Error; TArray<FPartyBuildSelection> Party;
        const TCHAR* Mixed[]={TEXT("cleric.life"),TEXT("druid.stars"),TEXT("bard.valor"),TEXT("cleric.light")};
        for(int32 Id=1;Id<=4;++Id) { FPartyBuildSelection P; P.Id=Id; P.Name=FString::Printf(TEXT("Slot %d"),Id); P.BuildId=Duplicate?TEXT("cleric.light"):Mixed[Id-1]; Party.Add(P); }
        FExpeditionRun Run; TestTrue(TEXT("Construct support run"),Run.NewRun(42,Party,Error,Duplicate?TEXT("duplicate-support"):TEXT("mixed-support")));
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
        TestEqual(TEXT("Five encounters completed by worked support party"),Run.Progress().EncountersCompleted,5);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSupportLifeTargetsTest,"Project.Expedition.Catalog.LifeConsciousTargets",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSupportLifeTargetsTest::RunTest(const FString& Parameters)
{
    FString Error; FCombatantState Hero;
    FBuildCatalog::Create(TEXT("cleric.life"),1,TEXT("Healer"),Hero); Hero.Cell={1,1}; Hero.InitiativeBonus=30;
    FCombatantState Ally; Ally.Id=2; Ally.Name=Ally.Role=TEXT("Any ally"); Ally.HP=5; Ally.Cell={2,1};
    FCombatantState Enemy; Enemy.Id=20; Enemy.Name=Enemy.Role=TEXT("Enemy"); Enemy.Side=ECombatSide::Enemy; Enemy.Cell={6,6};
    FCombatEncounter E; E.Combatants={Hero,Ally,Enemy}; FCombatBattle B;
    if(!TestTrue(TEXT("Start allied healing"),B.Start(E,42,Error))) { return false; }
    const auto R=B.Submit({ECombatCommandType::CureWounds,1,{},2});
    TestTrue(TEXT("Cure accepts a different class ally"),R.bAccepted);
    TestEqual(TEXT("Seed42 three initiatives then dice5+6+3 restore14"),B.Find(2)->HP,19);
    TestEqual(TEXT("One Kit use"),B.Find(1)->KitUses,1); TestEqual(TEXT("One Slot"),B.Find(1)->SpellSlots,1);
    for(int32 Case=0;Case<3;++Case)
    {
        auto Restricted=E; auto& T=Restricted.Combatants[1];
        if(Case==0) { T.Life=ECombatLife::Downed; T.HP=0; T.RescueTurnsLeft=3; }
        if(Case==1) { T.Life=ECombatLife::Dead; T.HP=0; }
        if(Case==2) { T.Cell={3,1}; }
        if(!TestTrue(TEXT("Start restricted Life target"),B.Start(Restricted,42,Error))) { continue; }
        const auto Before=B.Snapshot();
        TestFalse(TEXT("No downed/dead/distant heal preview"),B.Forecast(1,ECombatCommandType::CureWounds,2).bValid);
        TestFalse(TEXT("No downed/dead/distant healing"),B.Submit({ECombatCommandType::CureWounds,1,{},2}).bAccepted);
        TestTrue(TEXT("Every value/RNG retained"),FCombatSnapshot::StaticStruct()->CompareScriptStruct(&Before,&B.Snapshot(),0));
    }
    return true;
}
#endif
