#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Party/BuildCatalog.h"
#include "Persistence/ExpeditionSave.h"
#include "Presentation/ExpeditionPlayback.h"
#include "ExpeditionTestSupport.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"

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
    TestEqual(TEXT("Complete implemented catalog"),FBuildCatalog::All().Num(),30);
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
    for(bool Duplicate : {false,true})
    {
        FString Error; TArray<FPartyBuildSelection> Party;
        const TCHAR* Mixed[]={TEXT("fighter.rune-knight"),TEXT("rogue.soulknife"),TEXT("paladin.devotion"),TEXT("ranger.hunter")};
        for(int32 Id=1;Id<=4;++Id) { FPartyBuildSelection P; P.Id=Id; P.Name=FString::Printf(TEXT("Slot %d"),Id); P.BuildId=Duplicate?TEXT("ranger.hunter"):Mixed[Id-1]; Party.Add(P); }
        FExpeditionRun Run; TestTrue(TEXT("Construct martial run"),Run.NewRun(42,Party,Error,Duplicate?TEXT("duplicate-martial"):TEXT("mixed-martial")));
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
        TestEqual(TEXT("Five encounters completed by worked martial party"),Run.Progress().EncountersCompleted,5);
    }
    return true;
}
#endif
