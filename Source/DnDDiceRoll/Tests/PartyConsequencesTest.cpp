#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Combat/CombatBattle.h"
#include "Party/PartyContinuity.h"

namespace
{
FCombatEncounter RoleDuel(EAdventurerRole Role)
{
    FCombatEncounter Encounter;
    FCombatantState Hero;
    Hero.Id = 1; Hero.Name = TEXT("Hero"); Hero.Cell = {0,0}; Hero.AdventurerRole = Role;
    Hero.InitiativeBonus = 30; Hero.HP = 10; Hero.MaxHP = 30; Hero.SecondWindUses = 2; Hero.SpellSlots = 2;
    Hero.Potions = 1; Hero.Range = 4;
    FCombatantState Enemy;
    Enemy.Id = 20; Enemy.Name = TEXT("Enemy"); Enemy.Side = ECombatSide::Enemy; Enemy.Cell = {1,0};
    Enemy.InitiativeBonus = -10; Enemy.HP = Enemy.MaxHP = 100; Enemy.Speed = 0;
    Encounter.Combatants = {Hero,Enemy}; return Encounter;
}
bool Same(const FCombatSnapshot& Expected, const FCombatSnapshot& Actual)
{
    return FCombatSnapshot::StaticStruct()->CompareScriptStruct(&Expected, &Actual, 0);
}
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSecondWindTest, "Project.Expedition.Party.SecondWindBudgetAndAtomicCosts",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSecondWindTest::RunTest(const FString& Parameters)
{
    FCombatBattle Battle; FString Error;
    TestTrue(TEXT("Starts"), Battle.Start(RoleDuel(EAdventurerRole::Fighter), 42, Error));
    TestTrue(TEXT("Self preview"), Battle.LegalTargets(1, ECombatCommandType::SecondWind).Contains(1));
    const auto Result = Battle.Submit({ECombatCommandType::SecondWind, 1, {}, 1});
    TestTrue(TEXT("Second Wind is useful"), Result.bAccepted && Battle.Find(1)->HP > 10);
    TestEqual(TEXT("Exactly one limited use spent"), Battle.Find(1)->SecondWindUses, 1);
    TestTrue(TEXT("Action remains"), Battle.Snapshot().bActionAvailable);
    TestFalse(TEXT("Bonus action spent"), Battle.Snapshot().bBonusActionAvailable);
    const auto After = Battle.Snapshot();
    TestFalse(TEXT("Repeat cannot spend twice"), Battle.Submit({ECombatCommandType::SecondWind, 1, {}, 1}).bAccepted);
    TestTrue(TEXT("Rejected repeat preserves RNG/state"), Same(After, Battle.Snapshot()));
    TestTrue(TEXT("Can still attack"), Battle.Submit({ECombatCommandType::Attack, 1, {}, 20}).bAccepted);
    return !HasAnyErrors();
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPartyTransitionTest, "Project.Expedition.Party.PersistentSurvivorsAndLoot",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FPartyTransitionTest::RunTest(const FString& Parameters)
{
    auto Encounter = RoleDuel(EAdventurerRole::Wizard);
    Encounter.Combatants[1].HP = 1;
    FCombatantState Fallen = Encounter.Combatants[0];
    Fallen.Id = 2; Fallen.Cell = {0,1}; Fallen.HP = 0; Fallen.Life = ECombatLife::Dead;
    Encounter.Combatants.Add(Fallen);
    FCombatBattle Battle; FString Error;
    TestTrue(TEXT("Starts"), Battle.Start(Encounter, 42, Error));
    TestTrue(TEXT("Guaranteed spell finishes encounter"), Battle.Submit({ECombatCommandType::MagicMissile,1,{},20}).bAccepted);
    TestTrue(TEXT("Victory"), Battle.Snapshot().Phase == EBattlePhase::Victory);
    auto Layout = Encounter;
    Layout.Combatants[1].HP = 100;
    const auto Before = Battle.Snapshot();
    TestFalse(TEXT("Dead reward recipient refused"), FPartyContinuity::Advance(Battle, Layout, 9, 2, Error));
    TestTrue(TEXT("Rejected transition atomic"), Same(Before, Battle.Snapshot()));
    TestTrue(TEXT("Survivors continue"), FPartyContinuity::Advance(Battle, Layout, 9, 1, Error));
    TestEqual(TEXT("Injury carried"), Battle.Find(1)->HP, 10);
    TestEqual(TEXT("Spent slot carried"), Battle.Find(1)->SpellSlots, 1);
    TestEqual(TEXT("Useful weapon reward"), Battle.Find(1)->WeaponRank, 1);
    TestEqual(TEXT("Owned potion reward"), Battle.Find(1)->Potions, 2);
    TestTrue(TEXT("Dead ID remains permanently dead"), Battle.Find(2)->Life == ECombatLife::Dead);
    TestFalse(TEXT("Dead ID gets no turn"), Battle.Snapshot().TurnOrder.Contains(2));
    const auto Next = Battle.Snapshot();
    TestFalse(TEXT("Repeated transition cannot duplicate reward"), FPartyContinuity::Advance(Battle, Layout, 9, 1, Error));
    TestTrue(TEXT("Repeated transition atomic"), Same(Next, Battle.Snapshot()));
    return !HasAnyErrors();
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSpellAndPotionTest, "Project.Expedition.Party.SpellsHealingInventoryAndRejections",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSpellAndPotionTest::RunTest(const FString& Parameters)
{
    FString Error;
    for (auto Role : {EAdventurerRole::Fighter,EAdventurerRole::Rogue,EAdventurerRole::Cleric,EAdventurerRole::Wizard})
    {
        FCombatBattle Battle; Battle.Start(RoleDuel(Role),42,Error);
        const auto Before = Battle.Snapshot();
        const TArray<FCombatCommand> Invalid = {
            {ECombatCommandType::CureWounds,1,{},20}, {ECombatCommandType::Potion,1,{},999},
            {ECombatCommandType::Rescue,1,{},1}, {ECombatCommandType::SecondWind,1,{},20},
            {ECombatCommandType::MagicMissile,1,{},1}};
        for (const auto& C : Invalid)
        {
            TestFalse(TEXT("Invalid ability/target rejected"),Battle.Submit(C).bAccepted);
            TestTrue(TEXT("No costs or dice on rejection"),Same(Before,Battle.Snapshot()));
        }
        if (Role != EAdventurerRole::Wizard)
        {
            TestFalse(TEXT("Wrong role cannot cast missile"),Battle.Submit({ECombatCommandType::MagicMissile,1,{},20}).bAccepted);
            TestTrue(TEXT("Wrong role rejection atomic"),Same(Before,Battle.Snapshot()));
        }
    }
    auto Encounter = RoleDuel(EAdventurerRole::Cleric); Encounter.Combatants[0].HP=29;
    FCombatBattle Battle; Battle.Start(Encounter,42,Error);
    auto Result = Battle.Submit({ECombatCommandType::CureWounds,1,{},1});
    TestTrue(TEXT("Touch self healing caps at maximum"),Result.bAccepted && Result.Healing==1 && Battle.Find(1)->HP==30);
    TestEqual(TEXT("Spell slot spent"),Battle.Find(1)->SpellSlots,1);
    auto After=Battle.Snapshot();
    TestFalse(TEXT("Action cannot be reused by potion"),Battle.Submit({ECombatCommandType::Potion,1,{},1}).bAccepted);
    TestTrue(TEXT("No inventory cost on rejection"),Same(After,Battle.Snapshot()));
    Encounter=RoleDuel(EAdventurerRole::Wizard); Battle.Start(Encounter,42,Error);
    Result=Battle.Submit({ECombatCommandType::MagicMissile,1,{},20});
    TestTrue(TEXT("Three darts hit without attack or crit"),Result.bAccepted && Result.bHit && !Result.bCritical && Result.NaturalRoll==0 && Result.Damage>=6 && Result.Damage<=15);
    TestEqual(TEXT("One slot for all three darts"),Battle.Find(1)->SpellSlots,1);
    Encounter=RoleDuel(EAdventurerRole::Rogue); Battle.Start(Encounter,42,Error);
    Result=Battle.Submit({ECombatCommandType::Potion,1,{},1});
    TestTrue(TEXT("Potion helps"),Result.bAccepted && Result.Healing>=4 && Result.Healing<=10);
    TestEqual(TEXT("Bottle consumed exactly once"),Battle.Find(1)->Potions,0);
    After=Battle.Snapshot();
    TestFalse(TEXT("Repeated potion refused"),Battle.Submit({ECombatCommandType::Potion,1,{},1}).bAccepted);
    TestTrue(TEXT("Repeated potion atomic"),Same(After,Battle.Snapshot()));
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSneakAttackTest, "Project.Expedition.Party.SneakAttackQualification",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSneakAttackTest::RunTest(const FString& Parameters)
{
    FString Error;
    for (int32 Case=0;Case<4;++Case)
    {
        auto Encounter=RoleDuel(EAdventurerRole::Rogue);
        Encounter.Combatants[0].bSneakWeapon=Case!=1; Encounter.Combatants[0].AttackBonus=30;
        FCombatantState Ally=Encounter.Combatants[0]; Ally.Id=2; Ally.Cell=Case==2?FIntPoint(6,6):FIntPoint(1,1); Ally.InitiativeBonus=-10;
        if(Case==3) { Ally.Life=ECombatLife::Downed; Ally.HP=0; Ally.RescueTurnsLeft=3; }
        Encounter.Combatants.Add(Ally);
        FCombatBattle Battle; Battle.Start(Encounter,42,Error);
        const auto Result=Battle.Submit({ECombatCommandType::Attack,1,{},20});
        TestTrue(TEXT("Worked attack hits"),Result.bHit);
        if (Case == 0) { TestEqual(TEXT("Seed 42: base d6=5, modifier=3, sneak d6=3"), Result.Damage, 11); }
        if (Case == 1) { TestEqual(TEXT("Same seed without qualifying weapon has only base damage"), Result.Damage, 8); }
        TestEqual(TEXT("Requires qualifying weapon and conscious adjacent ally"),Result.bSneakAttack,Case==0);
        const auto After=Battle.Snapshot();
        TestFalse(TEXT("No second sneak/action this turn"),Battle.Submit({ECombatCommandType::Attack,1,{},20}).bAccepted);
        TestTrue(TEXT("Duplicate preserves state"),Same(After,Battle.Snapshot()));
    }
    auto CriticalEncounter = RoleDuel(EAdventurerRole::Rogue);
    CriticalEncounter.Combatants[0].bSneakWeapon = true;
    CriticalEncounter.Combatants[1].HP = 1;
    FCombatantState Ally = CriticalEncounter.Combatants[0];
    Ally.Id = 2; Ally.Cell = {1,1}; Ally.InitiativeBonus = -10;
    CriticalEncounter.Combatants.Add(Ally);
    FCombatBattle CriticalBattle;
    CriticalBattle.Start(CriticalEncounter, 0, Error);
    const auto CriticalResult = CriticalBattle.Submit({ECombatCommandType::Attack,1,{},20});
    TestTrue(TEXT("Seed 0 after three initiative draws is a qualified critical"), CriticalResult.bCritical && CriticalResult.bSneakAttack);
    TestEqual(TEXT("Critical base 2+2, sneak 4+1, modifier once +3"), CriticalResult.Damage, 12);
    TestTrue(TEXT("Finishing sneak still reports both extra dice and death"), CriticalResult.Feedback.Contains(TEXT("SNEAK")) && CriticalResult.Feedback.Contains(TEXT("DEAD")));
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRescueTest, "Project.Expedition.Party.RescueDeadlineAndPermanentDeath",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRescueTest::RunTest(const FString& Parameters)
{
    FString Error;
    auto Encounter=RoleDuel(EAdventurerRole::Cleric);
    FCombatantState Ally=Encounter.Combatants[0]; Ally.Id=2; Ally.Cell={0,1}; Ally.HP=0;
    Ally.Life=ECombatLife::Downed; Ally.RescueTurnsLeft=3; Ally.InitiativeBonus=-10;
    Encounter.Combatants.Add(Ally);
    for (auto Action : {ECombatCommandType::Rescue,ECombatCommandType::CureWounds,ECombatCommandType::Potion})
    {
        FCombatBattle Battle; Battle.Start(Encounter,42,Error);
        TestTrue(TEXT("Downed ally is a legal adjacent target"),Battle.LegalTargets(1,Action).Contains(2));
        const auto Result=Battle.Submit({Action,1,{},2});
        TestTrue(TEXT("Rescued with positive HP"),Result.bAccepted && Battle.Find(2)->HP>0 && Battle.Find(2)->Life==ECombatLife::Conscious);
        TestEqual(TEXT("Rescue deadline cleared"),Battle.Find(2)->RescueTurnsLeft,0);
        TestFalse(TEXT("Rescue spends action"),Battle.Snapshot().bActionAvailable);
    }
    FCombatBattle Battle; Battle.Start(Encounter,42,Error);
    for(int32 Tick=1;Tick<=3;++Tick)
    {
        const int32 Round=Battle.Snapshot().Round;
        // Only legal end-turn commands; no clock, frames or random rolls advance the deadline.
        do { Battle.Submit({ECombatCommandType::EndTurn,Battle.ActiveId()}); } while(Battle.Snapshot().Round==Round);
        TestEqual(TEXT("One countdown tick per own scheduled slot"),Battle.Find(2)->RescueTurnsLeft,3-Tick);
        TestTrue(TEXT("Distinct downed then dead"),Battle.Find(2)->Life==(Tick==3?ECombatLife::Dead:ECombatLife::Downed));
    }
    while(Battle.ActiveId()!=1) { Battle.Submit({ECombatCommandType::EndTurn,Battle.ActiveId()}); }
    const auto Before=Battle.Snapshot();
    for(auto Action:{ECombatCommandType::Rescue,ECombatCommandType::CureWounds,ECombatCommandType::Potion})
    {
        TestFalse(TEXT("Dead adventurer cannot be restored"),Battle.Submit({Action,1,{},2}).bAccepted);
        TestTrue(TEXT("Dead target rejection atomic"),Same(Before,Battle.Snapshot()));
    }
    TestFalse(TEXT("Dead ID cannot act"),Battle.Submit({ECombatCommandType::EndTurn,2}).bAccepted);
    // An actual hit creates the countdown rather than merely removing a turn.
    Encounter=RoleDuel(EAdventurerRole::None);
    Encounter.Combatants[0].InitiativeBonus=-10;
    Encounter.Combatants[1].InitiativeBonus=30; Encounter.Combatants[1].AttackBonus=30; Encounter.Combatants[1].DamageBonus=100;
    Ally.Life=ECombatLife::Conscious; Ally.HP=20; Ally.RescueTurnsLeft=0; Encounter.Combatants.Add(Ally);
    Battle.Start(Encounter,0,Error);
    TestTrue(TEXT("Enemy downs hero"),Battle.Submit({ECombatCommandType::Attack,20,{},1}).bHit);
    TestTrue(TEXT("Zero HP is downed while a rescuer lives"),Battle.Find(1)->Life==ECombatLife::Downed && Battle.Find(1)->RescueTurnsLeft==3);
    TestTrue(TEXT("Other survivor keeps fight active"),Battle.Snapshot().Phase==EBattlePhase::Fighting);
    Encounter.Combatants.RemoveAt(2); Battle.Start(Encounter,0,Error);
    const auto WipeResult = Battle.Submit({ECombatCommandType::Attack,20,{},1});
    TestTrue(TEXT("Wipe feedback reports permanent death, not an available rescue window"), WipeResult.Feedback.Contains(TEXT("DEAD")));
    TestTrue(TEXT("No conscious rescuer is a terminal wipe"),Battle.Snapshot().Phase==EBattlePhase::Defeat && Battle.Find(1)->Life==ECombatLife::Dead);
    const auto Wipe=Battle.Snapshot();
    TestFalse(TEXT("Wipe cannot transition"),FPartyContinuity::Advance(Battle,Encounter,4,1,Error));
    TestTrue(TEXT("Wipe preserved"),Same(Wipe,Battle.Snapshot()));
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAbilityLimitsTest, "Project.Expedition.Party.RangeSightAndExhaustedResources",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FAbilityLimitsTest::RunTest(const FString& Parameters)
{
    FString Error;
    auto Encounter=RoleDuel(EAdventurerRole::Wizard); Encounter.Combatants[1].Cell={7,7};
    FCombatBattle Battle; Battle.Start(Encounter,42,Error);
    auto Before=Battle.Snapshot();
    TestFalse(TEXT("Missile range is six cells"),Battle.Submit({ECombatCommandType::MagicMissile,1,{},20}).bAccepted);
    TestTrue(TEXT("Range rejection atomic"),Same(Before,Battle.Snapshot()));
    Encounter.Combatants[1].Cell={2,0}; Encounter.BlockedCells={{1,0}}; Battle.Start(Encounter,42,Error); Before=Battle.Snapshot();
    TestTrue(TEXT("Obscured spell absent from preview"),Battle.LegalTargets(1,ECombatCommandType::MagicMissile).IsEmpty());
    TestFalse(TEXT("Missile requires sight"),Battle.Submit({ECombatCommandType::MagicMissile,1,{},20}).bAccepted);
    TestTrue(TEXT("Sight rejection atomic"),Same(Before,Battle.Snapshot()));
    Encounter=RoleDuel(EAdventurerRole::Wizard); Battle.Start(Encounter,42,Error);
    for(int32 Cast=0;Cast<2;++Cast)
    {
        TestTrue(TEXT("Available slot can be cast"),Battle.Submit({ECombatCommandType::MagicMissile,1,{},20}).bAccepted);
        Battle.Submit({ECombatCommandType::EndTurn,1}); Battle.Submit({ECombatCommandType::EndTurn,20});
    }
    Before=Battle.Snapshot();
    TestEqual(TEXT("Two slots exhausted"),Battle.Find(1)->SpellSlots,0);
    TestFalse(TEXT("Cannot cast without slot"),Battle.Submit({ECombatCommandType::MagicMissile,1,{},20}).bAccepted);
    TestTrue(TEXT("Exhaustion atomic"),Same(Before,Battle.Snapshot()));
    TestTrue(TEXT("Cantrip remains useful without a slot"),Battle.Submit({ECombatCommandType::Attack,1,{},20}).bAccepted);
    Encounter=RoleDuel(EAdventurerRole::Cleric);
    FCombatantState Ally=Encounter.Combatants[0]; Ally.Id=2; Ally.Cell={0,2}; Ally.HP=0; Ally.Life=ECombatLife::Downed; Ally.RescueTurnsLeft=3; Ally.InitiativeBonus=-10;
    Encounter.Combatants.Add(Ally); Battle.Start(Encounter,42,Error); Before=Battle.Snapshot();
    for(auto Action:{ECombatCommandType::CureWounds,ECombatCommandType::Rescue,ECombatCommandType::Potion})
    {
        TestFalse(TEXT("Touch actions cannot rescue at two cells"),Battle.Submit({Action,1,{},2}).bAccepted);
        TestTrue(TEXT("Rejected distant rescue atomic"),Same(Before,Battle.Snapshot()));
    }
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVictoryLossPolicyTest, "Project.Expedition.Party.VictoryResolvesDownedAndValidatesContinuation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FVictoryLossPolicyTest::RunTest(const FString& Parameters)
{
    FString Error;
    auto Encounter=RoleDuel(EAdventurerRole::Wizard); Encounter.Combatants[1].HP=1;
    FCombatantState Ally=Encounter.Combatants[0]; Ally.Id=2; Ally.Cell={0,1}; Ally.HP=0; Ally.Life=ECombatLife::Downed; Ally.RescueTurnsLeft=3; Ally.InitiativeBonus=-10;
    Encounter.Combatants.Add(Ally);
    FCombatBattle Battle; Battle.Start(Encounter,42,Error); Battle.Submit({ECombatCommandType::MagicMissile,1,{},20});
    TestTrue(TEXT("Unresolved downed ally dies when victory ends rescue window"),Battle.Snapshot().Phase==EBattlePhase::Victory && Battle.Find(2)->Life==ECombatLife::Dead);
    const auto Before=Battle.Snapshot();
    auto Invalid=Encounter; Invalid.Combatants.RemoveAt(2);
    TestFalse(TEXT("Cannot omit dead roster member"),FPartyContinuity::Advance(Battle,Invalid,7,1,Error));
    TestTrue(TEXT("Missing roster rejection atomic"),Same(Before,Battle.Snapshot()));
    Invalid=Encounter; Invalid.Combatants[1].Cell=Invalid.Combatants[0].Cell;
    TestFalse(TEXT("Invalid layout rejected before loot"),FPartyContinuity::Advance(Battle,Invalid,7,1,Error));
    TestTrue(TEXT("Failed layout preserves loot and RNG"),Same(Before,Battle.Snapshot()));
    Encounter.Combatants[0].HP=30; Encounter.Combatants[0].SpellSlots=2;
    Encounter.Combatants[2].HP=30; Encounter.Combatants[2].Life=ECombatLife::Conscious; Encounter.Combatants[2].RescueTurnsLeft=0;
    TestTrue(TEXT("Layout cannot restore injuries, slots or dead member"),FPartyContinuity::Advance(Battle,Encounter,7,1,Error));
    TestEqual(TEXT("Carried injury"),Battle.Find(1)->HP,10);
    TestEqual(TEXT("Carried slot expenditure"),Battle.Find(1)->SpellSlots,1);
    TestTrue(TEXT("Layout resurrection refused by ownership"),Battle.Find(2)->Life==ECombatLife::Dead);
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEquipmentDamageTest, "Project.Expedition.Party.EquipmentHasCombatValue",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FEquipmentDamageTest::RunTest(const FString& Parameters)
{
    auto Encounter=RoleDuel(EAdventurerRole::Fighter); Encounter.Combatants[0].WeaponRank=2;
    FCombatBattle Battle; FString Error; Battle.Start(Encounter,42,Error);
    const auto Result=Battle.Submit({ECombatCommandType::Attack,1,{},20});
    TestEqual(TEXT("Worked seed: d6=4, modifier=3, owned gear=2"),Result.Damage,9);
    TestEqual(TEXT("Gear not consumed on use"),Battle.Find(1)->WeaponRank,2);
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSpentInventoryPersistenceTest, "Project.Expedition.Party.SpentInventorySurvivesTransition",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSpentInventoryPersistenceTest::RunTest(const FString& Parameters)
{
    auto Encounter = RoleDuel(EAdventurerRole::Fighter);
    Encounter.Combatants[0].HP = 1;
    Encounter.Combatants[0].MaxHP = 100;
    Encounter.Combatants[0].WeaponRank = 2;
    Encounter.Combatants[0].AttackBonus = 30;
    Encounter.Combatants[1].HP = 1;
    FCombatBattle Battle;
    FString Error;
    TestTrue(TEXT("Starts"), Battle.Start(Encounter, 42, Error));
    TestTrue(TEXT("First wind"), Battle.Submit({ECombatCommandType::SecondWind, 1, {}, 1}).bAccepted);
    TestTrue(TEXT("Owned potion consumed"), Battle.Submit({ECombatCommandType::Potion, 1, {}, 1}).bAccepted);
    Battle.Submit({ECombatCommandType::EndTurn, 1});
    Battle.Submit({ECombatCommandType::EndTurn, 20});
    TestTrue(TEXT("Next turn grants a new bonus action"), Battle.Submit({ECombatCommandType::SecondWind, 1, {}, 1}).bAccepted);
    Battle.Submit({ECombatCommandType::EndTurn, 1});
    Battle.Submit({ECombatCommandType::EndTurn, 20});
    const auto Before = Battle.Snapshot();
    TestFalse(TEXT("No third use despite new bonus budget"), Battle.Submit({ECombatCommandType::SecondWind, 1, {}, 1}).bAccepted);
    TestTrue(TEXT("Exhausted use preserves RNG and budgets"), Same(Before, Battle.Snapshot()));
    for (int32 Turn = 0; Turn < 20 && Battle.Snapshot().Phase == EBattlePhase::Fighting; ++Turn)
    {
        Battle.Submit({ECombatCommandType::Attack, 1, {}, 20});
        if (Battle.Snapshot().Phase == EBattlePhase::Fighting)
        {
            Battle.Submit({ECombatCommandType::EndTurn, 1});
            Battle.Submit({ECombatCommandType::EndTurn, 20});
        }
    }
    TestTrue(TEXT("Encounter won"), Battle.Snapshot().Phase == EBattlePhase::Victory);
    const int32 CarriedHP = Battle.Find(1)->HP;
    TestTrue(TEXT("Continue after actual resource spending"), FPartyContinuity::Advance(Battle, Encounter, 7, 1, Error));
    TestEqual(TEXT("Used winds stay exhausted"), Battle.Find(1)->SecondWindUses, 0);
    TestEqual(TEXT("Consumed potion stays consumed; only one reward added"), Battle.Find(1)->Potions, 1);
    TestEqual(TEXT("Existing equipment advances to cap"), Battle.Find(1)->WeaponRank, 3);
    TestEqual(TEXT("Transition supplies no health restoration"), Battle.Find(1)->HP, CarriedHP);
    return !HasAnyErrors();
}

#endif
