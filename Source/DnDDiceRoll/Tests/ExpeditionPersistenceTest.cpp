#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Run/ExpeditionRun.h"
#include "Persistence/ExpeditionSave.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "ExpeditionTestSupport.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FExactRestoreTest, "Project.Expedition.Persistence.ExactRestoreWithoutRolls",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FExactRestoreTest::RunTest(const FString& Parameters)
{
    FExpeditionRun Original, Restored; FString Error;
    Original.NewRun(42, Error, TEXT("restore-fixture"));
    Original.ChooseRoute(Original.Progress().Offers[0].Id, Error);
    const auto Before = Original.Snapshot();
    TestTrue(TEXT("Public restore accepts an initialized active encounter"), Restored.Restore(Before, Error));
    const auto After = Restored.Snapshot();
    TestTrue(TEXT("Restore retains every value including both random streams"), FExpeditionSnapshot::StaticStruct()->CompareScriptStruct(&Before, &After, 0));
    return !HasAnyErrors();
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStrictRoundtripTest, "Project.Expedition.Persistence.StrictSerializationRoundtrip",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FStrictRoundtripTest::RunTest(const FString& Parameters)
{
    FExpeditionRun Run, Restored; FString Error, Json;
    Run.NewRun(42, Error, TEXT("serialization-fixture"));
    FExpeditionSaveData Data; Data.Current = Run.Snapshot();
    TestTrue(TEXT("Real JSON encoding"), FExpeditionSave::Encode(Data, Json, Error));
    FExpeditionSaveData Loaded;
    TestTrue(TEXT("Strict JSON decoding"), FExpeditionSave::Decode(Json, Loaded, Error));
    TestTrue(TEXT("Decoded state restores"), Restored.Restore(Loaded.Current, Error));
    const auto Before = Run.Snapshot(), After = Restored.Snapshot();
    TestTrue(TEXT("All reflected values retained"), FExpeditionSnapshot::StaticStruct()->CompareScriptStruct(&Before, &After, 0));
    return !HasAnyErrors();
}

namespace
{
bool SameSnapshot(const FExpeditionSnapshot& A, const FExpeditionSnapshot& B)
{ return FExpeditionSnapshot::StaticStruct()->CompareScriptStruct(&A, &B, 0); }
struct FSaveFixture
{
    FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation/Expedition"), FGuid::NewGuid().ToString());
    FString Path = FPaths::Combine(Directory, TEXT("current.json"));
    ~FSaveFixture() { IFileManager::Get().DeleteDirectory(*Directory, false, true); }
};
FCombatCommand NextPartyCommand(const FExpeditionRun& Run, bool bFight)
{
    const auto& B = Run.Battle(); const int32 Id = B.ActiveId();
    const auto* Active = B.Find(Id);
    if (bFight)
    {
        for (auto Action : {ECombatCommandType::SecondWind, ECombatCommandType::Potion, ECombatCommandType::CureWounds})
        { if ((Action==ECombatCommandType::SecondWind ? Active->HP<Active->MaxHP : Active->HP <= Active->MaxHP / 2) && B.LegalTargets(Id, Action).Contains(Id)) { return {Action, Id, {}, Id}; } }
        // Keep movement before a spent Action observable, without consuming extra random values.
        if (B.Snapshot().bActionAvailable && B.LegalTargets(Id).IsEmpty())
        {
            auto Moves = B.LegalMoves(Id);
            auto Distance = [&B](FIntPoint Cell)
            {
                int32 Best = 1000;
                for (const auto& U : B.Snapshot().Encounter.Combatants)
                { if (U.Side == ECombatSide::Enemy && U.Life == ECombatLife::Conscious) { Best = FMath::Min(Best, FMath::Abs(Cell.X-U.Cell.X)+FMath::Abs(Cell.Y-U.Cell.Y)); } }
                return Best;
            };
            Moves.Sort([&](FIntPoint A, FIntPoint C) { const int32 DA=Distance(A), DC=Distance(C); return DA==DC ? (A.Y==C.Y ? A.X<C.X : A.Y<C.Y) : DA<DC; });
            if (!Moves.IsEmpty() && Distance(Moves[0]) < Distance(Active->Cell)) { return {ECombatCommandType::Move, Id, Moves[0]}; }
        }
        auto Targets = B.LegalTargets(Id, ECombatCommandType::MagicMissile);
        if (!Targets.IsEmpty()) { return {ECombatCommandType::MagicMissile, Id, {}, Targets[0]}; }
        Targets = B.LegalTargets(Id);
        if (!Targets.IsEmpty()) { return {ECombatCommandType::Attack, Id, {}, Targets[0]}; }
    }
    return {ECombatCommandType::EndTurn, Id};
}
bool Roundtrip(const FExpeditionRun& Run, FExpeditionRun& Restored, FString& Error)
{
    FExpeditionSaveData Data, Loaded; FString Json;
    Data.Current = Run.Snapshot();
    if (Run.Progress().Phase == EExpeditionPhase::Victory || Run.Progress().Phase == EExpeditionPhase::Defeat) { Data.Records.Add(FExpeditionSave::Ending(Run)); }
    return FExpeditionSave::Encode(Data, Json, Error) && FExpeditionSave::Decode(Json, Loaded, Error) && Restored.Restore(Loaded.Current, Error);
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FResumeEveryPhaseTest, "Project.Expedition.Persistence.ContinueIdenticallyAcrossEveryPhase",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FResumeEveryPhaseTest::RunTest(const FString& Parameters)
{
    TSet<EExpeditionPhase> Phases; bool bSpentAction=false, bPartialMove=false, bBonus=false, bDowned=false, bDead=false;
    for (bool bFight : {true, false})
    {
        FExpeditionRun Run, Resumed; FString Error; Run.NewRun(42, Error, TEXT("phase-fixture"));
        for (int32 Step=0; Step<1600; ++Step)
        {
            if (!TestTrue(*Error, Roundtrip(Run, Resumed, Error))) { return false; }
            TestTrue(TEXT("Every field equals uninterrupted run"), SameSnapshot(Run.Snapshot(), Resumed.Snapshot()));
            const auto Phase=Run.Progress().Phase; Phases.Add(Phase);
            const auto& B=Run.Battle(); const auto* A=B.Find(B.ActiveId());
            if (Phase==EExpeditionPhase::Victory || Phase==EExpeditionPhase::Defeat) { break; }
            if (Phase==EExpeditionPhase::Route)
            { const int32 Id=Run.Progress().Offers[0].Id; Run.ChooseRoute(Id,Error); Resumed.ChooseRoute(Id,Error); }
            else if (Phase==EExpeditionPhase::Reward)
            {
                int32 Recipient=INDEX_NONE;
                for (const auto& U:B.Snapshot().Encounter.Combatants) { if (U.Side==ECombatSide::Party && U.Life==ECombatLife::Conscious) { Recipient=U.Id; break; } }
                Run.ClaimReward(Recipient,Error); Resumed.ClaimReward(Recipient,Error);
            }
            else if (Phase==EExpeditionPhase::Recovery) { Run.Recover(true,Error); Resumed.Recover(true,Error); }
            else
            {
                bSpentAction |= !B.Snapshot().bActionAvailable; bPartialMove |= B.Snapshot().MovementLeft>0 && B.Snapshot().MovementLeft<A->Speed;
                bBonus |= !B.Snapshot().bBonusActionAvailable;
                for (const auto& U:B.Snapshot().Encounter.Combatants)
                { bDowned |= U.Side==ECombatSide::Party && U.Life==ECombatLife::Downed; bDead |= U.Side==ECombatSide::Party && U.Life==ECombatLife::Dead; }
                if (A->Side==ECombatSide::Enemy) { Run.PlayEnemyTurn(); Resumed.PlayEnemyTurn(); }
                else { const auto Command=NextPartyCommand(Run,bFight); Run.Submit(Command); Resumed.Submit(Command); }
            }
            TestTrue(TEXT("Same next command retains outcomes and both RNG streams"), SameSnapshot(Run.Snapshot(),Resumed.Snapshot()));
        }
    }
    TestEqual(TEXT("All six phases serialized"),Phases.Num(),6);
    TestTrue(TEXT("Spent Action observed"),bSpentAction); TestTrue(TEXT("Partial movement observed"),bPartialMove);
    TestTrue(TEXT("Spent Bonus Action observed"),bBonus); TestTrue(TEXT("Downed deadline observed"),bDowned); TestTrue(TEXT("Lost member observed"),bDead);
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInvalidNestedRestoreTest, "Project.Expedition.Persistence.InvalidNestedStateRejectedAtomically",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FInvalidNestedRestoreTest::RunTest(const FString& Parameters)
{
    FExpeditionRun Run; FString Error; Run.NewRun(42,Error,TEXT("invalid-fixture")); Run.ChooseRoute(Run.Progress().Offers[0].Id,Error);
    const auto Original=Run.Snapshot();
    TArray<TFunction<void(FExpeditionSnapshot&)>> Corruptions={
        [](auto& S){S.Progress.RunId.Reset();}, [](auto& S){S.Progress.Phase=static_cast<EExpeditionPhase>(255);},
        [](auto& S){S.Battle.Phase=static_cast<EBattlePhase>(255);}, [](auto& S){S.Battle.Encounter.Combatants[0].Life=static_cast<ECombatLife>(255);},
        [](auto& S){S.Battle.Encounter.Combatants[0].Side=static_cast<ECombatSide>(255);}, [](auto& S){S.Battle.Encounter.Combatants[0].AdventurerRole=static_cast<EAdventurerRole>(255);},
        [](auto& S){S.Battle.Encounter.Combatants[1].Id=1;}, [](auto& S){S.Battle.Encounter.Combatants[0].HP=0;},
        [](auto& S){S.Battle.Encounter.Combatants[0].HP=1000;}, [](auto& S){S.Battle.Encounter.Combatants[0].RescueTurnsLeft=2;},
        [](auto& S){auto& U=S.Battle.Encounter.Combatants[0]; U.HP=0; U.Life=ECombatLife::Downed; U.RescueTurnsLeft=0;},
        [](auto& S){auto& U=S.Battle.Encounter.Combatants[0]; U.HP=1; U.Life=ECombatLife::Dead;},
        [](auto& S){S.Battle.Encounter.Combatants[0].Cell={99,99};}, [](auto& S){S.Battle.Encounter.Combatants[0].Cell=S.Battle.Encounter.Combatants[1].Cell;},
        [](auto& S){const auto Cell=S.Battle.Encounter.BlockedCells[0];S.Battle.Encounter.BlockedCells.Add(Cell);}, [](auto& S){S.Battle.Encounter.Width+=1;}, [](auto& S){S.Progress.History[0].Route.Width=9; S.Battle.Encounter.Width=9;},
        [](auto& S){S.Battle.TurnOrder[0]=999;}, [](auto& S){const int32 Id=S.Battle.TurnOrder[0];S.Battle.TurnOrder.Add(Id);},
        [](auto& S){S.Battle.TurnOrder.RemoveAt(0);}, [](auto& S){Swap(S.Battle.TurnOrder[0],S.Battle.TurnOrder[1]);},
        [](auto& S){S.Battle.TurnIndex=999;}, [](auto& S){S.Battle.Round=0;}, [](auto& S){S.Battle.MovementLeft=99;},
        [](auto& S){S.Battle.Encounter.Combatants[0].Initiative=100;}, [](auto& S){S.Battle.Encounter.Combatants[0].SpellSlots=3;},
        [](auto& S){S.Battle.Encounter.Combatants[0].SpellSlots=1;}, [](auto& S){S.Battle.Encounter.Combatants[0].WeaponRank=4;}, [](auto& S){S.Battle.Encounter.Combatants[0].WeaponRank=1;},
        [](auto& S){S.Progress.EncountersCompleted=2;}, [](auto& S){S.Progress.History.Reset();},
        [](auto& S){S.Progress.History[0].Outcome=EBattlePhase::Victory;}, [](auto& S){S.Progress.History[0].RewardRecipient=1;},
        [](auto& S){S.Progress.History[0].Route.Recipe=99;}, [](auto& S){S.Progress.History[0].Route.Preview=TEXT("no enemies and free loot");}, [](auto& S){S.Progress.History[0].Route.Name=TEXT("Wrong route");}, [](auto& S){S.Progress.History[0].Route.EnemyCount=3;},
        [](auto& S){S.Progress.History[0].Route.RewardPotions=0;}, [](auto& S){S.Progress.bCampUsed=true;},
        [](auto& S){S.Progress.bCampResolved=true;}, [](auto& S){S.Progress.bArtifactRecovered=true;},
        [](auto& S){S.Battle.Encounter.Combatants[0].MaxHP=31;}, [](auto& S){S.Battle.Encounter.Combatants.RemoveAt(0);}
    };
    for (const auto& Corrupt:Corruptions)
    {
        auto Bad=Original; Corrupt(Bad);
        TestFalse(TEXT("Bad nested snapshot rejected"),Run.Restore(Bad,Error)); TestFalse(TEXT("Clear reason"),Error.IsEmpty());
        TestTrue(TEXT("No partial live mutation or reroll"),SameSnapshot(Original,Run.Snapshot()));
    }
    FExpeditionSnapshot Empty;
    TestFalse(TEXT("Uninitialized snapshot rejected"),Run.Restore(Empty,Error));
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStrictMalformedSaveTest, "Project.Expedition.Persistence.MissingTruncatedTypedAndVersionFailures",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FStrictMalformedSaveTest::RunTest(const FString& Parameters)
{
    FExpeditionRun Run; FString Error, Json; Run.NewRun(42,Error,TEXT("malformed-fixture"));
    FExpeditionSaveData Data; Data.Current=Run.Snapshot(); FExpeditionSave::Encode(Data,Json,Error);
    TSharedPtr<FJsonObject> Root; FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json),Root);
    TArray<FString> Malformed={TEXT("{}"),TEXT(""),Json.Left(Json.Len()/2)};
    auto AddMutation=[&](TFunctionRef<void(TSharedPtr<FJsonObject>)> Mutate)
    {
        TSharedPtr<FJsonObject> Object; FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json),Object); Mutate(Object);
        FString Text; FJsonSerializer::Serialize(Object.ToSharedRef(),TJsonWriterFactory<>::Create(&Text)); Malformed.Add(Text);
    };
    AddMutation([](auto O){O->SetNumberField(TEXT("Version"),FExpeditionSave::Version+1);}); // future schema unsupported
    AddMutation([](auto O){O->SetNumberField(TEXT("Version"),3);}); // schema 3 and older remain unsupported
    AddMutation([](auto O){O->GetObjectField(TEXT("Current"))->GetObjectField(TEXT("Progress"))->GetArrayField(TEXT("Offers"))[0]->AsObject()->SetStringField(TEXT("Preview"),TEXT("No enemies, no cost"));});
    AddMutation([](auto O){O->GetObjectField(TEXT("Current"))->GetObjectField(TEXT("Battle"))->SetNumberField(TEXT("MovementLeft"),99);});
    AddMutation([](auto O){O->GetObjectField(TEXT("Current"))->SetObjectField(TEXT("Battle"),MakeShared<FJsonObject>());});
    AddMutation([](auto O){O->GetObjectField(TEXT("Current"))->GetObjectField(TEXT("Progress"))->RemoveField(TEXT("GenerationRandomState"));});
    AddMutation([](auto O){O->GetObjectField(TEXT("Current"))->GetObjectField(TEXT("Battle"))->SetField(TEXT("bActionAvailable"),MakeShared<FJsonValueNull>());});
    AddMutation([](auto O){O->GetObjectField(TEXT("Current"))->GetObjectField(TEXT("Battle"))->SetStringField(TEXT("RandomState"),TEXT("42"));});
    AddMutation([](auto O){O->GetObjectField(TEXT("Current"))->GetObjectField(TEXT("Battle"))->SetNumberField(TEXT("MovementLeft"),1.5);});
    AddMutation([](auto O){O->GetObjectField(TEXT("Current"))->GetObjectField(TEXT("Progress"))->SetNumberField(TEXT("Phase"),256);});
    AddMutation([](auto O){O->GetObjectField(TEXT("Current"))->GetObjectField(TEXT("Progress"))->SetNumberField(TEXT("Seed"),4294967296.0);});
    AddMutation([](auto O){O->GetObjectField(TEXT("Current"))->GetObjectField(TEXT("Battle"))->GetObjectField(TEXT("Encounter"))->GetArrayField(TEXT("Combatants"))[0]->AsObject()->RemoveField(TEXT("HP"));});
    for (const auto& Bad:Malformed)
    {
        auto Output=Data;
        TestFalse(TEXT("Malformed real JSON rejected"),FExpeditionSave::Decode(Bad,Output,Error));
        TestFalse(TEXT("Decode reason"),Error.IsEmpty());
        TestTrue(TEXT("Decode output unchanged"),FExpeditionSaveData::StaticStruct()->CompareScriptStruct(&Data,&Output,0));
    }
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCheckpointDiskTest, "Project.Expedition.Persistence.DiskCheckpointFailureDeathAndEndingRecords",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FCheckpointDiskTest::RunTest(const FString& Parameters)
{
    FSaveFixture Fixture; FExpeditionSession Session(Fixture.Path); FString Error;
    // Failed writes: route, death, ending, then new run.
    ExpeditionTestSupport::ExpectFailedCheckpointWrites(*this,Fixture.Path,4);
    TestFalse(TEXT("Startup does not create or overwrite any save"),FPaths::FileExists(Fixture.Path));
    TestTrue(TEXT("Fresh run checkpoint"),Session.NewRun(42,Error));
    const FString FirstId=Session.Run().Progress().RunId;
    FExpeditionSession FreshProcess(Fixture.Path);
    TestTrue(TEXT("Real disk resume"),FreshProcess.Resume(Error));
    TestTrue(TEXT("Exact initial disk state"),SameSnapshot(Session.Run().Snapshot(),FreshProcess.Run().Snapshot()));
    const auto Before=Session.Run().Snapshot();
    // A real directory at the temporary filename makes writing fail deterministically,
    // while leaving the existing checkpoint file independently readable.
    auto& Files=FPlatformFileManager::Get().GetPlatformFile(); Files.CreateDirectory(*(Fixture.Path+TEXT(".pending")));
    TestFalse(TEXT("Failed accepted route checkpoint rejects the command"),Session.ChooseRoute(Session.Run().Progress().Offers[0].Id,Error));
    TestTrue(TEXT("Failed checkpoint keeps entire live state"),SameSnapshot(Before,Session.Run().Snapshot()));
    TestTrue(TEXT("Prior valid disk checkpoint retained"),FreshProcess.Resume(Error));
    TestTrue(TEXT("Failed save retains disk state"),SameSnapshot(Before,FreshProcess.Run().Snapshot()));
    Files.DeleteDirectory(*(Fixture.Path+TEXT(".pending")));
    TestTrue(TEXT("Route commits after storage recovers"),Session.ChooseRoute(Session.Run().Progress().Offers[0].Id,Error));
    bool bObservedDeath=false, bObservedDowned=false, bRejectedDeath=false, bRejectedEnding=false;
    for(int32 Step=0;Step<1000 && Session.Run().Progress().Phase==EExpeditionPhase::Combat;++Step)
    {
        const auto& B=Session.Run().Battle(); const auto* A=B.Find(B.ActiveId());
        auto Predicted=Session.Run();
        if(A->Side==ECombatSide::Enemy){Predicted.PlayEnemyTurn();}else{Predicted.Submit({ECombatCommandType::EndTurn,A->Id});}
        const bool bNewDeath=Predicted.Battle().Snapshot().Encounter.Combatants.ContainsByPredicate([&](const auto& U)
        {return U.Side==ECombatSide::Party && U.Life==ECombatLife::Dead && B.Find(U.Id)->Life!=ECombatLife::Dead;});
        const bool bNewEnding=Predicted.Progress().Phase==EExpeditionPhase::Defeat;
        if((bNewDeath && !bRejectedDeath) || (bNewEnding && !bRejectedEnding))
        {
            const auto BeforeFailure=Session.Run().Snapshot();
            Files.CreateDirectory(*(Fixture.Path+TEXT(".pending")));
            if(A->Side==ECombatSide::Enemy)
            {const auto Results=Session.PlayEnemyTurn();TestTrue(TEXT("Failed enemy checkpoint rejects entire turn"),Results.Num()==1 && !Results[0].bAccepted);}
            else{TestFalse(TEXT("Failed death checkpoint rejects turn progression"),Session.Submit({ECombatCommandType::EndTurn,A->Id}).bAccepted);}
            TestTrue(TEXT("Failed death/ending leaves live state before the command"),SameSnapshot(BeforeFailure,Session.Run().Snapshot()));
            FreshProcess.Resume(Error);TestTrue(TEXT("Disk agrees with uncommitted death rejection"),SameSnapshot(BeforeFailure,FreshProcess.Run().Snapshot()));
            TArray<FExpeditionEnding> PriorRecords;Session.ReadRecords(PriorRecords,Error);TestTrue(TEXT("No ending written on failed outcome"),PriorRecords.IsEmpty());
            Files.DeleteDirectory(*(Fixture.Path+TEXT(".pending")));bRejectedDeath |= bNewDeath;bRejectedEnding |= bNewEnding;
        }
        if(A->Side==ECombatSide::Enemy){Session.PlayEnemyTurn();} else {Session.Submit({ECombatCommandType::EndTurn,A->Id});}
        if(!TestTrue(TEXT("Every accepted step survives fresh adapter"),FreshProcess.Resume(Error))){return false;}
        TestTrue(TEXT("Latest command/RNG/resources on disk"),SameSnapshot(Session.Run().Snapshot(),FreshProcess.Run().Snapshot()));
        for(const auto& U:Session.Run().Battle().Snapshot().Encounter.Combatants)
        {
            if(U.Side!=ECombatSide::Party){continue;}
            bObservedDowned |= U.Life==ECombatLife::Downed;
            if(U.Life==ECombatLife::Dead){bObservedDeath=true; TestTrue(TEXT("Resume cannot revive actual death"),FreshProcess.Run().Battle().Find(U.Id)->Life==ECombatLife::Dead);}
        }
    }
    TestTrue(TEXT("Real commands reach downed and dead"),bObservedDowned && bObservedDeath);
    TestTrue(TEXT("Write failure tested exactly at death and ending"),bRejectedDeath && bRejectedEnding);
    TestTrue(TEXT("Wipe terminal"),Session.Run().Progress().Phase==EExpeditionPhase::Defeat);
    TArray<FExpeditionEnding> Records; TestTrue(TEXT("Records readable"),Session.ReadRecords(Records,Error));
    TestEqual(TEXT("One durable ending"),Records.Num(),1);
    TestTrue(TEXT("Ending has same identity, seed, fate and zero survivors"),Records[0].RunId==FirstId && Records[0].Seed==42 && Records[0].Outcome==EExpeditionPhase::Defeat && Records[0].Survivors.IsEmpty());
    TestTrue(TEXT("Explicit save ending"),Session.Save(Error)); TestTrue(TEXT("Reload ending"),Session.Resume(Error)); TestTrue(TEXT("Save reloaded ending"),Session.Save(Error));
    Session.ReadRecords(Records,Error); TestEqual(TEXT("Ending idempotent"),Records.Num(),1);
    const auto Ending=Session.Run().Snapshot();
    Files.CreateDirectory(*(Fixture.Path+TEXT(".pending")));
    TestFalse(TEXT("New-run failed checkpoint preserves ending and history"),Session.NewRun(42,Error));
    TestTrue(TEXT("Failed new-run stays at ending"),SameSnapshot(Ending,Session.Run().Snapshot()));
    Session.ReadRecords(Records,Error); TestEqual(TEXT("History preserved on failure"),Records.Num(),1);
    Files.DeleteDirectory(*(Fixture.Path+TEXT(".pending")));
    TestTrue(TEXT("Fresh same-seed run"),Session.NewRun(42,Error));
    TestNotEqual(TEXT("Fresh same seed has distinct identity"),Session.Run().Progress().RunId,FirstId);
    for(const auto& U:Session.Run().Battle().Snapshot().Encounter.Combatants)
    {if(U.Side==ECombatSide::Party){TestTrue(TEXT("History grants no power"),U.HP==U.MaxHP && U.Life==ECombatLife::Conscious && U.WeaponRank==0 && U.Potions==1);}}
    Session.ChooseRoute(Session.Run().Progress().Offers[0].Id,Error);
    for(int32 Step=0;Step<1000 && Session.Run().Progress().Phase==EExpeditionPhase::Combat;++Step)
    {const auto& B=Session.Run().Battle();const auto* A=B.Find(B.ActiveId());if(A->Side==ECombatSide::Enemy){Session.PlayEnemyTurn();}else{Session.Submit({ECombatCommandType::EndTurn,A->Id});}}
    Session.ReadRecords(Records,Error); TestEqual(TEXT("Same-seed fresh endings remain distinct"),Records.Num(),2);
    // Corrupt input cannot change a live run; preserve fixtures only under Automation/.
    const auto Live=Session.Run().Snapshot(); FFileHelper::SaveStringToFile(TEXT("{truncated"),*Fixture.Path);
    TestFalse(TEXT("Invalid load fails"),Session.Resume(Error)); TestTrue(TEXT("Invalid load has no partial install"),SameSnapshot(Live,Session.Run().Snapshot()));
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStartupRunProtectionTest, "Project.Expedition.Persistence.StartupProtectsUnfinishedCheckpoint",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FStartupRunProtectionTest::RunTest(const FString& Parameters)
{
    FSaveFixture Fixture; FString Error, Before, After;
    FExpeditionSession Original(Fixture.Path);
    if (!TestTrue(TEXT("Create unfinished expedition"), Original.NewRun(42, Error))) { return false; }
    const auto Snapshot = Original.Run().Snapshot();
    FFileHelper::LoadFileToString(Before, *Fixture.Path);
    FExpeditionSession Restarted(Fixture.Path);
    TestFalse(TEXT("Restart cannot replace an unfinished checkpoint"), Restarted.NewRun(43, Error));
    TestFalse(TEXT("Rejected startup leaves no live run installed"), Restarted.HasRun());
    TestFalse(TEXT("Live session cannot abandon an unfinished run either"), Original.NewRun(43, Error));
    FFileHelper::LoadFileToString(After, *Fixture.Path);
    TestEqual(TEXT("Checkpoint bytes retained"), After, Before);
    TestTrue(TEXT("Existing run still resumes"), Restarted.Resume(Error));
    TestTrue(TEXT("Identity and all progress retained"), SameSnapshot(Snapshot, Restarted.Run().Snapshot()));
    FFileHelper::SaveStringToFile(TEXT("{"), *Fixture.Path);
    TestFalse(TEXT("Live run cannot quarantine a damaged checkpoint"), Original.NewRun(43, Error));
    FFileHelper::LoadFileToString(After, *Fixture.Path);
    TestEqual(TEXT("Live recovery preserves damaged file in place"), After, FString(TEXT("{")));
    TestTrue(TEXT("Live recovery preserves current run"), SameSnapshot(Snapshot, Original.Run().Snapshot()));
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUnreadableStartupRecoveryTest, "Project.Expedition.Persistence.NewRunPreservesUnreadableCheckpoint",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FUnreadableStartupRecoveryTest::RunTest(const FString& Parameters)
{
    FExpeditionRun Run; FString Error, Json;
    Run.NewRun(42, Error, TEXT("prior-version"));
    FExpeditionSaveData Data; Data.Current = Run.Snapshot();
    FExpeditionSave::Encode(Data, Json, Error);
    TSharedPtr<FJsonObject> Object;
    FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Object);
    Object->SetNumberField(TEXT("Version"), FExpeditionSave::Version + 1); // future schema: unreadable, preserve on NewRun
    FString Incompatible;
    FJsonSerializer::Serialize(Object.ToSharedRef(), TJsonWriterFactory<>::Create(&Incompatible));
    for (const FString& Unreadable : TArray<FString>{Json.Left(Json.Len()/2), Incompatible})
    {
        FSaveFixture Fixture;
        IFileManager::Get().MakeDirectory(*Fixture.Directory, true);
        FFileHelper::SaveStringToFile(Unreadable, *Fixture.Path);
        FExpeditionSession Session(Fixture.Path);
        TestFalse(TEXT("Resume rejects unreadable save"), Session.Resume(Error));
        if (!TestTrue(TEXT("Explicit fresh start can recover"), Session.NewRun(43, Error))) { continue; }
        TArray<FString> Preserved;
        IFileManager::Get().FindFiles(Preserved, *(Fixture.Path + TEXT(".unreadable-*")), true, false);
        if (TestEqual(TEXT("Exactly one original preserved"), Preserved.Num(), 1))
        {
            FString Original;
            FFileHelper::LoadFileToString(Original, *FPaths::Combine(Fixture.Directory, Preserved[0]));
            TestEqual(TEXT("Preserved original bytes"), Original, Unreadable);
        }
        FExpeditionSession Restarted(Fixture.Path);
        TestTrue(TEXT("Replacement run is resumable"), Restarted.Resume(Error));
        TestEqual(TEXT("Requested fresh seed"), Restarted.Run().Progress().Seed, 43);
    }
    FSaveFixture Failed;
    ExpeditionTestSupport::ExpectFailedCheckpointWrites(*this,Failed.Path,1);
    IFileManager::Get().MakeDirectory(*Failed.Directory, true);
    FFileHelper::SaveStringToFile(Incompatible, *Failed.Path);
    IFileManager::Get().MakeDirectory(*(Failed.Path + TEXT(".pending")), true);
    FExpeditionSession FailedSession(Failed.Path);
    FString PreservedPath, PreservedBytes;
    TestFalse(TEXT("Failed replacement does not install a recovered run"), FailedSession.NewRun(43, Error, FString(), &PreservedPath));
    TestFalse(TEXT("Failed recovery leaves startup state"), FailedSession.HasRun());
    TestTrue(TEXT("Failure reports that original was preserved"), Error.Contains(TEXT("preserved")));
    TestTrue(TEXT("Original still readable at reported path"), FFileHelper::LoadFileToString(PreservedBytes, *PreservedPath));
    TestEqual(TEXT("Failed replacement cannot destroy original"), PreservedBytes, Incompatible);
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPersistLostSurvivorTest, "Project.Expedition.Persistence.PermanentLossContinuesThroughCampAndNextEncounter",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FPersistLostSurvivorTest::RunTest(const FString& Parameters)
{
    FExpeditionRun Run, Restored; FString Error; Run.NewRun(42,Error,TEXT("lost-member-fixture"));Run.ChooseRoute(Run.Progress().Offers[0].Id,Error);
    for(int32 Step=0;Step<120 && Run.Progress().Phase==EExpeditionPhase::Combat && Run.Battle().Find(1)->Life!=ECombatLife::Dead;++Step)
    {
        const auto& B=Run.Battle();const auto* A=B.Find(B.ActiveId());
        if(A->Side==ECombatSide::Enemy){Run.PlayEnemyTurn();}else{Run.Submit({ECombatCommandType::EndTurn,A->Id});}
        if(!TestTrue(TEXT("Death countdown and shrinking party serialize"),Roundtrip(Run,Restored,Error))){return false;}
        TestTrue(TEXT("Shrinking party exact"),SameSnapshot(Run.Snapshot(),Restored.Snapshot()));
    }
    TestTrue(TEXT("Resumed survivors finish the same encounter"),ExpeditionTestSupport::FinishCombat(Run) && ExpeditionTestSupport::FinishCombat(Restored));
    TestTrue(TEXT("Full survivor continuation preserves outcomes/RNG"),SameSnapshot(Run.Snapshot(),Restored.Snapshot()));
    if(!TestTrue(TEXT("Survivor victory after permanent loss"),Run.Progress().Phase==EExpeditionPhase::Reward && Run.Battle().Find(1)->Life==ECombatLife::Dead)){return false;}
    TestEqual(TEXT("Dead fighter retains death-level"),Run.Battle().Find(1)->Level,1);
    int32 Recipient=INDEX_NONE;for(const auto& U:Run.Battle().Snapshot().Encounter.Combatants){if(U.Side==ECombatSide::Party && U.Life==ECombatLife::Conscious){Recipient=U.Id;break;}}
    Run.ClaimReward(Recipient,Error);TestTrue(TEXT("Reward exact"),Roundtrip(Run,Restored,Error));
    Run.Recover(true,Error);TestTrue(TEXT("Camp exact"),Roundtrip(Run,Restored,Error));
    Run.ChooseRoute(Run.Progress().Offers[0].Id,Error);TestTrue(TEXT("Next encounter exact"),Roundtrip(Run,Restored,Error));
    TestTrue(TEXT("Dead stable identity remains lost after serialized camp and route"),Restored.Battle().Find(1)->HP==0 && Restored.Battle().Find(1)->Life==ECombatLife::Dead && !Restored.Battle().Snapshot().TurnOrder.Contains(1));
    return !HasAnyErrors();
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProgressionSessionTest, "Project.Expedition.Persistence.SubclassAndMilestoneCheckpoint",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FProgressionSessionTest::RunTest(const FString& Parameters)
{
    const FString Path = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation"), FGuid::NewGuid().ToString(), TEXT("current-v1.json"));
    FExpeditionSession Session(Path), Resumed(Path); FString Error;
    TestTrue(TEXT("Atomic new run selects Battle Master"), Session.NewRun(42,Error,TEXT("bm-checkpoint"),nullptr,EFighterSubclass::BattleMaster));
    TestTrue(TEXT("Resume selected subclass"),Resumed.Resume(Error));
    TestTrue(TEXT("Subclass retained"),Resumed.Run().Battle().Find(1)->FighterSubclass==EFighterSubclass::BattleMaster);
    TestEqual(TEXT("Initial maneuver budget"),Resumed.Run().Battle().Find(1)->ManeuverUses,2);
    TestFalse(TEXT("Active run cannot respec through NewRun"),Resumed.NewRun(43,Error,TEXT("respec"),nullptr,EFighterSubclass::Champion));
    auto Invalid = Resumed.Run().Snapshot(); Invalid.Battle.Encounter.Combatants[0].Level=2;
    FExpeditionRun Candidate;
    TestFalse(TEXT("Unearned level rejected"),Candidate.Restore(Invalid,Error));
    Invalid=Resumed.Run().Snapshot(); Invalid.Battle.Encounter.Combatants[1].FighterSubclass=EFighterSubclass::Champion;
    TestFalse(TEXT("Subclass on another role rejected"),Candidate.Restore(Invalid,Error));
    return !HasAnyErrors();
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMilestoneFailureTest, "Project.Expedition.Persistence.FailedMilestoneIsAtomic",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FMilestoneFailureTest::RunTest(const FString& Parameters)
{
    FSaveFixture Fixture; FExpeditionSession Session(Fixture.Path), Disk(Fixture.Path); FString Error;
    ExpeditionTestSupport::ExpectFailedCheckpointWrites(*this,Fixture.Path,1);
    Session.NewRun(42,Error); Session.ChooseRoute(Session.Run().Progress().Offers[0].Id,Error);
    for (int32 Step=0; Step<1000 && Session.Run().Progress().Phase==EExpeditionPhase::Combat; ++Step)
    {
        const bool bEnemy=Session.Run().Battle().Find(Session.Run().Battle().ActiveId())->Side==ECombatSide::Enemy;
        const auto Command=NextPartyCommand(Session.Run(),true);
        auto Predicted=Session.Run();
        if(bEnemy){Predicted.PlayEnemyTurn();}else{Predicted.Submit(Command);}
        if(Predicted.Progress().Phase==EExpeditionPhase::Reward)
        {
            const auto Before=Session.Run().Snapshot();
            auto& Files=FPlatformFileManager::Get().GetPlatformFile();
            Files.CreateDirectory(*(Fixture.Path+TEXT(".pending")));
            if(bEnemy){Session.PlayEnemyTurn();}else{TestFalse(TEXT("Failed milestone rejects finishing action"),Session.Submit(Command).bAccepted);}
            TestTrue(TEXT("Failed milestone preserves levels deaths and RNG"),SameSnapshot(Before,Session.Run().Snapshot()));
            TestTrue(TEXT("Prior checkpoint readable"),Disk.Resume(Error));
            TestTrue(TEXT("Disk preserves pre-milestone state"),SameSnapshot(Before,Disk.Run().Snapshot()));
            Files.DeleteDirectory(*(Fixture.Path+TEXT(".pending")));
            if(bEnemy){Session.PlayEnemyTurn();}else{TestTrue(TEXT("Finishing action commits after recovery"),Session.Submit(Command).bAccepted);}
            TestEqual(TEXT("One milestone committed"),Session.Run().Battle().Find(1)->Level,2);
            TestTrue(TEXT("Committed milestone matches prediction"),SameSnapshot(Predicted.Snapshot(),Session.Run().Snapshot()));
            return !HasAnyErrors();
        }
        if(bEnemy){Session.PlayEnemyTurn();}else{Session.Submit(Command);}
    }
    AddError(TEXT("Policy did not reach first milestone")); return false;
}
#endif
