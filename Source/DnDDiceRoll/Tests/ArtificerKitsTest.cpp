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
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FArtificerRedTest,"Project.Expedition.Catalog.ArtificerStartup",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FArtificerRedTest::RunTest(const FString& Parameters)
{
    auto E=FCombatBattle::TutorialEncounter(); E.BlockedCells.Reset();
    auto& Hero=E.Combatants[0];
    FBuildCatalog::Create(TEXT("artificer.armorer"),1,TEXT("Arden"),Hero);
    Hero.InitiativeBonus=30;
    FCombatBattle B; FString Error;
    if(!TestTrue(TEXT("Successful encounter startup before desired command"),B.Start(E,42,Error))) { return false; }
    const auto R=B.Submit({ECombatCommandType::Special1,1,{},1});
    TestTrue(TEXT("Armorer activates Defensive Field"),R.bAccepted);
    TestEqual(TEXT("Concrete armor reduction four"),B.Find(1)->GuardAmount,4);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FArtificerWorkedTest, "Project.Expedition.Catalog.ArtificerWorkedOutcomes", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FArtificerWorkedTest::RunTest(const FString& Parameters)
{
    // Literal worked outcomes: primary at (2,1), secondary at (2,2), ally at (1,2).
    struct FCase { const TCHAR* Build; int32 Slot, Target, Loss, Heal, Guard; bool Push, Area, Bonus; };
    const FCase Cases[] = {
        {TEXT("artificer.alchemist"),1,2,0,8,0,false,false,false},
        {TEXT("artificer.alchemist"),2,20,3,0,0,false,true,true},
        {TEXT("artificer.armorer"),1,1,0,0,4,false,false,true},
        {TEXT("artificer.armorer"),2,20,6,0,0,true,false,false},
        {TEXT("artificer.artillerist"),1,20,5,0,0,true,false,false},
        {TEXT("artificer.artillerist"),2,2,0,0,2,false,false,true},
        {TEXT("artificer.battle-smith"),1,2,0,0,4,false,false,false},
        {TEXT("artificer.battle-smith"),2,20,5,0,0,false,false,true},
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
        const auto Action=C.Slot==1?ECombatCommandType::Special1:ECombatCommandType::Special2;
        const auto Before=Battle.Snapshot();
        for (int32 Repeat=0;Repeat<3;++Repeat) { TestTrue(TEXT("Repeated preview legal"),Battle.Forecast(1,Action,C.Target).bValid); }
        TestTrue(TEXT("Repeated previews preserve every value including RNG"),FCombatSnapshot::StaticStruct()->CompareScriptStruct(&Before,&Battle.Snapshot(),0));
        FCombatBattle Continued; TestTrue(TEXT("Direct restore"),Continued.Restore(Before,Error));
        const auto Preview=Battle.Forecast(1,Action,C.Target);
        if(C.Loss>0 || C.Heal>0)
        {
            TestEqual(TEXT("Literal minimum forecast"),Preview.MinAmount,C.Loss>0?C.Loss:C.Heal);
            TestEqual(TEXT("Literal maximum forecast"),Preview.MaxAmount,C.Loss>0?C.Loss:C.Heal);
            TestEqual(TEXT("Healing forecast classification"),Preview.bHealing,C.Heal>0);
        }
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
        TestEqual(TEXT("Primary literal loss"),40-Battle.Find(20)->HP,C.Loss);
        TestEqual(TEXT("Secondary literal loss"),40-Battle.Find(21)->HP,C.Area?C.Loss:0);
        TestEqual(TEXT("Literal healing"),Battle.Find(C.Target)->HP-(C.Target==1?10:C.Target==2?5:40),C.Target==20?-C.Loss:C.Heal);
        TestEqual(TEXT("Owned ward amount"),Battle.Find(1)->GuardAmount,C.Guard);
        TestEqual(TEXT("Safe area excludes adjacent ally"),Battle.Find(2)->HP,C.Target==2?5+C.Heal:5);
        TestEqual(TEXT("Owner stays in place"),Battle.Find(1)->Cell,FIntPoint(1,1));
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
        for(int32 Constraint=0;Constraint<(C.Heal>0?4:2);++Constraint)
        {
            auto Restricted=Encounter;
            auto* T=Restricted.Combatants.FindByPredicate([&](const auto& U){return U.Id==C.Target;});
            if(Constraint==0) { T->HP=0; T->Life=ECombatLife::Dead; }
            else if(Constraint==1) { if(C.Target==1) { continue; } T->Cell={7,7}; }
            else if(Constraint==2) { T->HP=0; T->Life=ECombatLife::Downed; T->RescueTurnsLeft=3; }
            else { T->HP=T->MaxHP; }
            FCombatBattle R; if(!TestTrue(TEXT("Start invalid target scenario"),R.Start(Restricted,42,Error))) { continue; }
            // A dead owner cannot act. Other targets are rejected while the owner retains the turn.
            if(C.Target==1) { continue; }
            const auto Saved=R.Snapshot();
            TestFalse(TEXT("Invalid target has no forecast"),R.Forecast(1,Action,C.Target).bValid);
            TestFalse(TEXT("Invalid target command rejected"),R.Submit({Action,1,{},C.Target}).bAccepted);
            TestTrue(TEXT("Rejected target preserves full state and RNG"),FCombatSnapshot::StaticStruct()->CompareScriptStruct(&Saved,&R.Snapshot(),0));
        }
        for(int32 Restriction=0;Restriction<2;++Restriction)
        {
            auto Restricted=Before;
            if(Restriction==0) { Restricted.Encounter.Combatants[0].KitUses=0; }
            else if(C.Bonus) { Restricted.bBonusActionAvailable=false; Restricted.Encounter.Combatants[0].KitUses=1; } else { Restricted.bActionAvailable=false; }
            FCombatBattle R; TestTrue(TEXT("Restore bounded unavailable scenario"),R.Restore(Restricted,Error)); const auto Saved=R.Snapshot();
            TestFalse(TEXT("Spent resource or budget rejects preview"),R.Forecast(1,Action,C.Target).bValid);
            TestFalse(TEXT("Spent resource or budget rejects command"),R.Submit({Action,1,{},C.Target}).bAccepted);
            TestTrue(TEXT("Unavailable rejection retains complete state/RNG"),FCombatSnapshot::StaticStruct()->CompareScriptStruct(&Saved,&R.Snapshot(),0));
        }
        if(C.Push)
        {
            auto Occupied=Encounter; Occupied.Combatants[3].Cell={3,1}; FCombatBattle R;
            TestTrue(TEXT("Occupied push destination encounter"),R.Start(Occupied,42,Error));
            TestTrue(TEXT("Occupied push still damages"),R.Submit({Action,1,{},20}).bAccepted);
            TestEqual(TEXT("Occupied cell prevents displacement"),R.Find(20)->Cell,FIntPoint(2,1));
            TestEqual(TEXT("Blocked push retains literal damage"),R.Find(20)->HP,40-C.Loss);
        }
        if(C.Target!=1 && !(FString(C.Build)==TEXT("artificer.armorer")))
        {
            auto Wall=Encounter;
            auto* Target=Wall.Combatants.FindByPredicate([&](const auto& U){return U.Id==C.Target;});
            Target->Cell={3,1}; if(C.Target==2) { Wall.Combatants[2].Cell={5,5}; }
            Wall.BlockedCells.Add({2,1}); if(C.Target==20) { Wall.Combatants[3].Cell={5,5}; }
            // An adjacent defender ward is range-rejected here; ranged tools additionally require sight.
            FCombatBattle R; if(TestTrue(TEXT("Wall restriction encounter"),R.Start(Wall,42,Error))) {
                const auto Saved=R.Snapshot(); TestFalse(TEXT("Wall/range rejects forecast"),R.Forecast(1,Action,C.Target).bValid);
                TestFalse(TEXT("Wall/range rejects command"),R.Submit({Action,1,{},C.Target}).bAccepted);
                TestTrue(TEXT("Wall rejection atomic"),FCombatSnapshot::StaticStruct()->CompareScriptStruct(&Saved,&R.Snapshot(),0));
            }
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
        TestTrue(TEXT("First sibling special"),B.Submit({ECombatCommandType::Special1,1,{},C.Target}).bAccepted);
        TestTrue(TEXT("Second sibling special"),B.Submit({ECombatCommandType::Special2,1,{},D.Target}).bAccepted);
        TestEqual(TEXT("One common pool depleted"),B.Find(1)->KitUses,0);
        const auto Spent=B.Snapshot();
        TestFalse(TEXT("No third special from shared pool"),B.Submit({ECombatCommandType::Special1,1,{},C.Target}).bAccepted);
        TestTrue(TEXT("Spent rejection includes RNG"),FCombatSnapshot::StaticStruct()->CompareScriptStruct(&Spent,&B.Snapshot(),0));
    }
    TSet<FString> Classes; int32 Artificers=0;
    for(const auto& B:FBuildCatalog::All()) { Classes.Add(B.Profile.ClassId); if(B.Profile.ClassId==TEXT("artificer")) { ++Artificers; TestEqual(TEXT("Appended stable role"),static_cast<int32>(B.Profile.AdventurerRole),13); TestEqual(TEXT("Two bounded commands"),B.Specials.Num(),2); TestEqual(TEXT("No spellbook pool"),B.SlotCap,0); } }
    TestEqual(TEXT("Exact thirteen class membership"),Classes.Num(),13); TestEqual(TEXT("Exact four Artificers"),Artificers,4);
    TestEqual(TEXT("Exactly 70 implemented kits"),FBuildCatalog::All().Num(),70);
    TestNull(TEXT("Interim Cleric removed"),FBuildCatalog::Find(TEXT("cleric.base")));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FArtificerJsonTest, "Project.Expedition.Catalog.ArtificerJsonContinuity", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FArtificerJsonTest::RunTest(const FString& Parameters)
{
    FString Error;
    for (const auto* Build : {TEXT("artificer.armorer"),TEXT("artificer.artillerist"),TEXT("artificer.battle-smith"),TEXT("artificer.alchemist")})
    {
        AddInfo(Build); TArray<FPartyBuildSelection> Party;
        for(int32 Id=1;Id<=4;++Id) { FPartyBuildSelection S; S.Id=Id; S.Name=FString::Printf(TEXT("Hero %d"),Id); S.BuildId=Build; Party.Add(S); }
        FExpeditionRun Run; if(!TestTrue(TEXT("Duplicate artificer party"),Run.NewRun(42,Party,Error,TEXT("artificer-json")))) { continue; }
        TestTrue(TEXT("First route retains party"),Run.ChooseRoute(Run.Progress().Offers[0].Id,Error));
        while(Run.Battle().Find(Run.Battle().ActiveId())->Side==ECombatSide::Enemy) { Run.PlayEnemyTurn(); }
        const int32 Id=Run.Battle().ActiveId();
        const auto Action=FString(Build)==TEXT("artificer.artillerist")?ECombatCommandType::Special2:ECombatCommandType::Special1;
        if(FString(Build)==TEXT("artificer.alchemist"))
        {
            auto Fixture=Run.Snapshot();
            Fixture.Battle.Encounter.Combatants.FindByPredicate([&](const auto& U){return U.Id!=Id && U.Side==ECombatSide::Party;})->HP=10;
            TestTrue(TEXT("Valid injured Elixir ally checkpoint fixture"),Run.Restore(Fixture,Error));
        }
        const auto Targets=Run.Battle().LegalTargets(Id,Action);
        if(!TestFalse(TEXT("Generated party has valid ally/self target"),Targets.IsEmpty())) { continue; }
        TestTrue(TEXT("Commit actual artificer effect"),Run.Submit({Action,Id,{},Targets[0]}).bAccepted);
        const FString Directory=FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("Automation/Artificer"),FGuid::NewGuid().ToString());
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
            else if (Mutation==7) { Unit->SetNumberField(TEXT("SpellSlots"),1); }
            else if (Mutation==8) { Unit->RemoveField(TEXT("GuardAmount")); }
            else if (Mutation==9) { Unit->RemoveField(TEXT("LastSurge")); }
            else if (Mutation==10) { Unit->SetNumberField(TEXT("LastSurge"),0.5); }
            else if (Mutation==11) { Unit->SetStringField(TEXT("LastSurge"),TEXT("1")); }
            else if (Mutation==12) { Unit->SetNumberField(TEXT("LastSurge"),3); }
            else if(Mutation==13) { Unit->SetNumberField(TEXT("LastSurge"),1); }
            else if(Mutation==14) { Unit->SetNumberField(TEXT("KitUses"),2); Unit->SetNumberField(TEXT("LastSurge"),1); }
            else { Object->GetObjectField(TEXT("Current"))->GetObjectField(TEXT("Battle"))->SetStringField(TEXT("bActionAvailable"),TEXT("true")); }
            FString Malformed; FJsonSerializer::Serialize(Object.ToSharedRef(),TJsonWriterFactory<>::Create(&Malformed));
            auto Output=Decoded;
            TestFalse(TEXT("Malformed artificer wire state rejected"),FExpeditionSave::Decode(Malformed,Output,Error));
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
        Bad=Decoded.Current; Bad.Battle.Encounter.Combatants[0].AdventurerRole=EAdventurerRole::Bard;
        TestFalse(TEXT("Artificer build cannot acquire another role"),Resumed.Restore(Bad,Error));
        Bad=Decoded.Current; Bad.Battle.Encounter.Combatants[0].AdventurerRole=static_cast<EAdventurerRole>(14);
        TestFalse(TEXT("Role beyond appended Artificer rejected"),Resumed.Restore(Bad,Error));
        TestTrue(TEXT("Role rejections retain complete state/RNG"),ExpeditionTestSupport::SameRun(Before,Resumed.Snapshot()));
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FArtificerRandomContinuityTest,"Project.Expedition.Catalog.ArtificerRandomCheckpointContinuity",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FArtificerRandomContinuityTest::RunTest(const FString& Parameters)
{
    FString Error; TArray<FPartyBuildSelection> Party;
    for(int32 Id=1;Id<=4;++Id) { FPartyBuildSelection P; P.Id=Id; P.Name=FString::Printf(TEXT("Hero %d"),Id); P.BuildId=TEXT("artificer.armorer"); Party.Add(P); }
    FExpeditionRun Run;
    if(!TestTrue(TEXT("Construct Armorer RNG run"),Run.NewRun(42,Party,Error,TEXT("artificer-rng"))) ||
       !TestTrue(TEXT("Enter actual route"),Run.ChooseRoute(Run.Progress().Offers[0].Id,Error))) { return false; }
    while(Run.Battle().Find(Run.Battle().ActiveId())->Side==ECombatSide::Enemy) { Run.PlayEnemyTurn(); }
    const int32 Id=Run.Battle().ActiveId();
    if(!TestTrue(TEXT("Commit owned armor before checkpoint"),Run.Submit({ECombatCommandType::Special1,Id,{},Id}).bAccepted)) { return false; }
    // Find a basic-attack opening using only legal moves and public command copies.
    if(Run.Battle().LegalTargets(Id).IsEmpty())
    {
        for(const auto& Cell:Run.Battle().LegalMoves(Id))
        {
            auto Candidate=Run;
            if(Candidate.Submit({ECombatCommandType::Move,Id,Cell}).bAccepted && !Candidate.Battle().LegalTargets(Id).IsEmpty()) { Run=MoveTemp(Candidate); break; }
        }
    }
    const auto Targets=Run.Battle().LegalTargets(Id);
    if(!TestFalse(TEXT("Generated geometry permits actual basic attack"),Targets.IsEmpty())) { return false; }
    const FCombatCommand Attack{ECombatCommandType::Attack,Id,{},Targets[0]};
    const auto Before=Run.Snapshot();
    const FString Directory=FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("Automation/Artificer"),FGuid::NewGuid().ToString());
    const FString Path=FPaths::Combine(Directory,TEXT("current-v1.json"));
    FExpeditionSaveData Data; Data.Current=Before;
    if(!TestTrue(TEXT("Write owned-effect checkpoint"),FExpeditionSave::Write(Path,Data,Error))) { return false; }
    FExpeditionSession Session(Path);
    TestTrue(TEXT("Fresh file Resume before random command"),Session.Resume(Error));
    TestTrue(TEXT("Resume retains every value including RNG/armor"),ExpeditionTestSupport::SameRun(Before,Session.Run().Snapshot()));
    TArray<uint8> BytesBefore,BytesAfter;
    TestTrue(TEXT("Read original disk bytes"),FFileHelper::LoadFileToArray(BytesBefore,*Path));
    const auto Expected=Run.Submit(Attack);
    TestTrue(TEXT("Reference attack accepted"),Expected.bAccepted);
    TestNotEqual(TEXT("Actual basic attack consumes RNG"),Run.Snapshot().Battle.RandomState,Before.Battle.RandomState);
    IFileManager::Get().MakeDirectory(*(Path+TEXT(".pending")),true);
    TestFalse(TEXT("Failed checkpoint rejects RNG-consuming attack"),Session.Submit(Attack).bAccepted);
    TestTrue(TEXT("Failed attack retains all live state/RNG/owned armor"),ExpeditionTestSupport::SameRun(Before,Session.Run().Snapshot()));
    TestTrue(TEXT("Read disk after refusal"),FFileHelper::LoadFileToArray(BytesAfter,*Path));
    TestTrue(TEXT("Failed attack preserves exact checkpoint bytes"),BytesBefore==BytesAfter);
    IFileManager::Get().DeleteDirectory(*(Path+TEXT(".pending")),false,true);
    const auto Actual=Session.Submit(Attack);
    TestTrue(TEXT("Retry attack commits"),Actual.bAccepted);
    TestEqual(TEXT("Continued natural roll matches uninterrupted run"),Actual.NaturalRoll,Expected.NaturalRoll);
    TestEqual(TEXT("Continued attack total"),Actual.AttackTotal,Expected.AttackTotal);
    TestEqual(TEXT("Continued damage"),Actual.Damage,Expected.Damage);
    TestEqual(TEXT("Continued hit"),Actual.bHit,Expected.bHit);
    TestEqual(TEXT("Continued critical"),Actual.bCritical,Expected.bCritical);
    TestTrue(TEXT("Post-attack complete state and RNG agree"),ExpeditionTestSupport::SameRun(Run.Snapshot(),Session.Run().Snapshot()));
    FExpeditionSession Disk(Path); TestTrue(TEXT("Resume committed retry from actual disk"),Disk.Resume(Error));
    TestTrue(TEXT("Committed file matches uninterrupted run"),ExpeditionTestSupport::SameRun(Run.Snapshot(),Disk.Run().Snapshot()));
    IFileManager::Get().DeleteDirectory(*Directory,false,true);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FArtificerRunTest, "Project.Expedition.Catalog.ArtificerRunProgression", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FArtificerRunTest::RunTest(const FString& Parameters)
{
    for(bool Duplicate : {false,true})
    {
        FString Error; TArray<FPartyBuildSelection> Party;
        const TCHAR* Mixed[]={TEXT("artificer.alchemist"),TEXT("artificer.armorer"),TEXT("artificer.artillerist"),TEXT("artificer.battle-smith")};
        for(int32 Id=1;Id<=4;++Id) { FPartyBuildSelection P; P.Id=Id; P.Name=FString::Printf(TEXT("Slot %d"),Id); P.BuildId=Duplicate?TEXT("artificer.armorer"):Mixed[Id-1]; Party.Add(P); }
        FExpeditionRun Run; TestTrue(TEXT("Construct artificer run"),Run.NewRun(42,Party,Error,Duplicate?TEXT("duplicate-artificer"):TEXT("mixed-artificer")));
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
        TestEqual(TEXT("Five encounters completed by worked artificer party"),Run.Progress().EncountersCompleted,5);
    }
    return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FArtificerProtectionTest, "Project.Expedition.Catalog.ArtificerProtection", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FArtificerProtectionTest::RunTest(const FString& Parameters)
{
    FString Error; FCombatantState Knight,Psi;
    FBuildCatalog::Create(TEXT("artificer.armorer"),1,TEXT("Knight"),Knight);
    FBuildCatalog::Create(TEXT("artificer.battle-smith"),2,TEXT("Psi"),Psi);
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
    TestEqual(TEXT("Two wards use strongest, not sum: five minus four"),Preview.MinAmount,1);
    const int32 Before=Battle.Find(1)->HP;
    const auto Hit=Battle.Submit({ECombatCommandType::Attack,20,{},1});
    TestTrue(TEXT("Worked incoming hit"),Hit.bHit);
    TestEqual(TEXT("Committed prevention is four"),Hit.DamagePrevented,4);
    TestEqual(TEXT("Actual HP loss matches committed result"),Before-Battle.Find(1)->HP,Hit.Damage);
    TestTrue(TEXT("Feedback explains prevention"),Hit.Feedback.Contains(TEXT("prevented 4")));
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


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FArtificerSpentRecoveryTest,"Project.Expedition.Catalog.ArtificerSpentPoolsAcrossProgression",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FArtificerSpentRecoveryTest::RunTest(const FString& Parameters)
{
    {
    AddInfo(TEXT("Two Defensive Fields on separate turns"));
    const int32 SpentSlots=0;
    FString Error; TArray<FPartyBuildSelection> Party;
    for(int32 Id=1;Id<=4;++Id) { FPartyBuildSelection P; P.Id=Id; P.Name=FString::Printf(TEXT("Hero %d"),Id); P.BuildId=TEXT("artificer.armorer"); Party.Add(P); }
    FExpeditionRun Run;
    if(!TestTrue(TEXT("Construct spent-pool run"),Run.NewRun(42,Party,Error,TEXT("armorer-spent-pools"))) ||
       !TestTrue(TEXT("Enter generated route"),Run.ChooseRoute(Run.Progress().Offers[0].Id,Error))) { return false; }
    for(int32 Turn=0;Turn<8 && Run.Battle().ActiveId()!=1;++Turn)
    {
        const int32 Id=Run.Battle().ActiveId();
        if(Run.Battle().Find(Id)->Side==ECombatSide::Enemy) { Run.PlayEnemyTurn(); }
        else { Run.Submit({ECombatCommandType::EndTurn,Id}); }
    }
    if(!TestEqual(TEXT("Armorer reaches own turn"),Run.Battle().ActiveId(),1)) { return false; }
    TestTrue(TEXT("Spend first Kit on Defensive Field"),Run.Submit({ECombatCommandType::Special1,1,{},1}).bAccepted);
        TestTrue(TEXT("End first field turn"),Run.Submit({ECombatCommandType::EndTurn,1}).bAccepted);
        for(int32 Turn=0;Turn<8 && Run.Battle().ActiveId()!=1;++Turn)
        {
            const int32 Id=Run.Battle().ActiveId();
            if(Run.Battle().Find(Id)->Side==ECombatSide::Enemy) { Run.PlayEnemyTurn(); }
            else { Run.Submit({ECombatCommandType::EndTurn,Id}); }
        }
        if(!TestEqual(TEXT("Armorer reaches second own turn"),Run.Battle().ActiveId(),1)) { return false; }
        TestTrue(TEXT("Spend second Kit on Defensive Field"),Run.Submit({ECombatCommandType::Special1,1,{},1}).bAccepted);
    TestEqual(TEXT("Two actual fields deplete shared Kit"),Run.Battle().Find(1)->KitUses,0);
    TestEqual(TEXT("Artificer has no Slots"),Run.Battle().Find(1)->SpellSlots,SpentSlots);
    if(!TestTrue(TEXT("Public combat policy wins"),ExpeditionTestSupport::FinishCombat(Run))) { return false; }
    TestEqual(TEXT("Victory reaches reward"),Run.Progress().Phase,EExpeditionPhase::Reward);
    TestEqual(TEXT("Conscious Armorer levels up"),Run.Battle().Find(1)->Level,2);
    TestEqual(TEXT("Victory and level do not refill Kit"),Run.Battle().Find(1)->KitUses,0);
    TestEqual(TEXT("Victory and level do not refill Slots"),Run.Battle().Find(1)->SpellSlots,SpentSlots);
    if(!TestTrue(TEXT("Claim actual reward"),Run.ClaimReward(1,Error)) ||
       !TestTrue(TEXT("Take explicit camp"),Run.Recover(true,Error))) { return false; }
    TestEqual(TEXT("Camp restores exactly one Kit"),Run.Battle().Find(1)->KitUses,1);
    TestEqual(TEXT("Camp cannot create Slots"),Run.Battle().Find(1)->SpellSlots,0);
    if(!TestTrue(TEXT("Enter next generated route"),Run.ChooseRoute(Run.Progress().Offers[0].Id,Error))) { return false; }
    TestEqual(TEXT("Route leaves spent Kit balance"),Run.Battle().Find(1)->KitUses,1);
    TestEqual(TEXT("Route leaves Slot balance"),Run.Battle().Find(1)->SpellSlots,0);
    const FString Directory=FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("Automation/Artificer"),FGuid::NewGuid().ToString());
    const FString Path=FPaths::Combine(Directory,TEXT("current-v1.json"));
    FExpeditionSaveData Data; Data.Current=Run.Snapshot();
    TestTrue(TEXT("Write actual post-camp checkpoint"),FExpeditionSave::Write(Path,Data,Error));
    FExpeditionSession Session(Path);
    TestTrue(TEXT("Fresh session Resume"),Session.Resume(Error));
    TestTrue(TEXT("Resume preserves all values and RNG"),ExpeditionTestSupport::SameRun(Data.Current,Session.Run().Snapshot()));
    TestEqual(TEXT("Resume cannot refill Kit"),Session.Run().Battle().Find(1)->KitUses,1);
    TestEqual(TEXT("Resume cannot refill Slots"),Session.Run().Battle().Find(1)->SpellSlots,0);
    IFileManager::Get().DeleteDirectory(*Directory,false,true);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCappedCampCheckpointTest,"Project.Expedition.Catalog.CappedCampAfterBonusVictory",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCappedCampCheckpointTest::RunTest(const FString& Parameters)
{
    FString Error; TArray<FPartyBuildSelection> Party;
    for(int32 Id=1;Id<=4;++Id) { FPartyBuildSelection P; P.Id=Id; P.Name=FString::Printf(TEXT("Hero %d"),Id); P.BuildId=TEXT("artificer.artillerist"); Party.Add(P); }
    FExpeditionRun Run;
    if(!TestTrue(TEXT("Create capped recovery run"),Run.NewRun(42,Party,Error,TEXT("capped-bonus-camp"))) ||
       !TestTrue(TEXT("Enter first generated route"),Run.ChooseRoute(Run.Progress().Offers[0].Id,Error))) { return false; }
    int32 Finisher=INDEX_NONE;
    for(int32 Turn=0;Turn<400 && Run.Progress().Phase==EExpeditionPhase::Combat;++Turn)
    {
        const int32 Id=Run.Battle().ActiveId();
        if(Run.Battle().Find(Id)->Side==ECombatSide::Enemy) { Run.PlayEnemyTurn(); continue; }
        auto Targets=Run.Battle().LegalTargets(Id);
        if(Targets.IsEmpty())
        {
            auto Moves=Run.Battle().LegalMoves(Id);
            auto Nearest=[&](FIntPoint Cell)
            {
                int32 Best=1000;
                for(const auto& U:Run.Battle().Snapshot().Encounter.Combatants)
                { if(U.Side==ECombatSide::Enemy && U.HP>0) { Best=FMath::Min(Best,ExpeditionTestSupport::Distance(Cell,U.Cell)); } }
                return Best;
            };
            Moves.Sort([&](FIntPoint A,FIntPoint B) { const int32 DA=Nearest(A),DB=Nearest(B); return DA==DB?(A.Y==B.Y?A.X<B.X:A.Y<B.Y):DA<DB; });
            if(!Moves.IsEmpty()) { Run.Submit({ECombatCommandType::Move,Id,Moves[0]}); }
            Targets=Run.Battle().LegalTargets(Id);
        }
        if(!Targets.IsEmpty())
        {
            const FCombatCommand Attack{ECombatCommandType::Attack,Id,{},Targets[0]};
            FExpeditionRun Trial=Run; Trial.Submit(Attack);
            const auto Allies=Run.Battle().LegalTargets(Id,ECombatCommandType::Special2);
            if(Trial.Progress().Phase==EExpeditionPhase::Reward && Run.Battle().Find(Id)->KitUses==2 && !Allies.IsEmpty())
            {
                if(!TestTrue(TEXT("Spend real Bonus protection before finishing blow"),Run.Submit({ECombatCommandType::Special2,Id,{},Allies[0]}).bAccepted)) { return false; }
                Finisher=Id;
            }
            if(!TestTrue(TEXT("Submit actual basic attack"),Run.Submit(Attack).bAccepted)) { return false; }
        }
        if(Run.Progress().Phase==EExpeditionPhase::Combat) { Run.Submit({ECombatCommandType::EndTurn,Id}); }
    }
    if(!TestTrue(TEXT("A real Bonus-spending hero wins"),Finisher!=INDEX_NONE && Run.Progress().Phase==EExpeditionPhase::Reward)) { return false; }
    TestFalse(TEXT("Victory retains spent Bonus budget"),Run.Battle().Snapshot().bBonusActionAvailable);
    TestEqual(TEXT("Victory retains one unspent Kit"),Run.Battle().Find(Finisher)->KitUses,1);
    const FString Directory=FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("Automation/CappedCamp"),FGuid::NewGuid().ToString());
    const FString Path=FPaths::Combine(Directory,TEXT("current-v1.json"));
    FExpeditionSaveData Data; Data.Current=Run.Snapshot();
    if(!TestTrue(TEXT("Write genuine victory checkpoint"),FExpeditionSave::Write(Path,Data,Error))) { return false; }
    FExpeditionSession Session(Path);
    if(!TestTrue(TEXT("Resume before reward"),Session.Resume(Error)) ||
       !TestTrue(TEXT("Commit actual reward"),Session.ClaimReward(Finisher,Error)) ||
       !TestTrue(TEXT("Commit camp recovery back to Kit cap"),Session.Recover(true,Error))) { return false; }
    TestEqual(TEXT("Camp restores exactly to two"),Session.Run().Battle().Find(Finisher)->KitUses,2);
    TestFalse(TEXT("Historical combat budget stays spent"),Session.Run().Battle().Snapshot().bBonusActionAvailable);
    FExpeditionSession Resumed(Path);
    TestTrue(TEXT("Fresh session resumes capped camp"),Resumed.Resume(Error));
    TestTrue(TEXT("Resume retains complete state and RNG"),ExpeditionTestSupport::SameRun(Session.Run().Snapshot(),Resumed.Run().Snapshot()));
    IFileManager::Get().DeleteDirectory(*Directory,false,true);
    return true;
}

#endif
