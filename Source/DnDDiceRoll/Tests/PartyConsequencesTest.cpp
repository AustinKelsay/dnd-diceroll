#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Combat/CombatBattle.h"
#include "Party/PartyContinuity.h"

namespace
{
FCombatEncounter RoleDuel(EAdventurerRole Role)
{
    FCombatEncounter E;
    FCombatantState A;
    A.Id = 1; A.Name = TEXT("Hero"); A.Cell = {0,0}; A.AdventurerRole = Role;
    A.InitiativeBonus = 30; A.HP = 10; A.MaxHP = 30; A.SecondWindUses = 2; A.SpellSlots = 2;
    A.Potions = 1; A.Range = 4;
    FCombatantState B;
    B.Id = 20; B.Name = TEXT("Enemy"); B.Side = ECombatSide::Enemy; B.Cell = {1,0};
    B.InitiativeBonus = -10; B.HP = B.MaxHP = 100; B.Speed = 0;
    E.Combatants = {A,B}; return E;
}
bool Same(const FCombatSnapshot& A, const FCombatSnapshot& B)
{
    return FCombatSnapshot::StaticStruct()->CompareScriptStruct(&A, &B, 0);
}
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSecondWindTest, "Project.Expedition.Party.SecondWindBudgetAndAtomicCosts",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSecondWindTest::RunTest(const FString& Parameters)
{
    FCombatBattle B; FString Error;
    TestTrue(TEXT("Starts"), B.Start(RoleDuel(EAdventurerRole::Fighter), 42, Error));
    TestTrue(TEXT("Self preview"), B.LegalTargets(1, ECombatCommandType::SecondWind).Contains(1));
    const auto R = B.Submit({ECombatCommandType::SecondWind, 1, {}, 1});
    TestTrue(TEXT("Second Wind is useful"), R.bAccepted && B.Find(1)->HP > 10);
    TestEqual(TEXT("Exactly one limited use spent"), B.Find(1)->SecondWindUses, 1);
    TestTrue(TEXT("Action remains"), B.Snapshot().bActionAvailable);
    TestFalse(TEXT("Bonus action spent"), B.Snapshot().bBonusActionAvailable);
    const auto After = B.Snapshot();
    TestFalse(TEXT("Repeat cannot spend twice"), B.Submit({ECombatCommandType::SecondWind, 1, {}, 1}).bAccepted);
    TestTrue(TEXT("Rejected repeat preserves RNG/state"), Same(After, B.Snapshot()));
    TestTrue(TEXT("Can still attack"), B.Submit({ECombatCommandType::Attack, 1, {}, 20}).bAccepted);
    return !HasAnyErrors();
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPartyTransitionTest, "Project.Expedition.Party.PersistentSurvivorsAndLoot",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FPartyTransitionTest::RunTest(const FString& Parameters)
{
    auto E = RoleDuel(EAdventurerRole::Wizard);
    E.Combatants[1].HP = 1;
    FCombatantState Fallen = E.Combatants[0];
    Fallen.Id = 2; Fallen.Cell = {0,1}; Fallen.HP = 0; Fallen.Life = ECombatLife::Dead;
    E.Combatants.Add(Fallen);
    FCombatBattle B; FString Error;
    TestTrue(TEXT("Starts"), B.Start(E, 42, Error));
    TestTrue(TEXT("Guaranteed spell finishes encounter"), B.Submit({ECombatCommandType::MagicMissile,1,{},20}).bAccepted);
    TestTrue(TEXT("Victory"), B.Snapshot().Phase == EBattlePhase::Victory);
    auto Layout = E;
    Layout.Combatants[1].HP = 100;
    const auto Before = B.Snapshot();
    TestFalse(TEXT("Dead reward recipient refused"), FPartyContinuity::Advance(B, Layout, 9, 2, Error));
    TestTrue(TEXT("Rejected transition atomic"), Same(Before, B.Snapshot()));
    TestTrue(TEXT("Survivors continue"), FPartyContinuity::Advance(B, Layout, 9, 1, Error));
    TestEqual(TEXT("Injury carried"), B.Find(1)->HP, 10);
    TestEqual(TEXT("Spent slot carried"), B.Find(1)->SpellSlots, 1);
    TestEqual(TEXT("Useful weapon reward"), B.Find(1)->WeaponRank, 1);
    TestEqual(TEXT("Owned potion reward"), B.Find(1)->Potions, 2);
    TestTrue(TEXT("Dead ID remains permanently dead"), B.Find(2)->Life == ECombatLife::Dead);
    TestFalse(TEXT("Dead ID gets no turn"), B.Snapshot().TurnOrder.Contains(2));
    const auto Next = B.Snapshot();
    TestFalse(TEXT("Repeated transition cannot duplicate reward"), FPartyContinuity::Advance(B, Layout, 9, 1, Error));
    TestTrue(TEXT("Repeated transition atomic"), Same(Next, B.Snapshot()));
    return !HasAnyErrors();
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSpellAndPotionTest, "Project.Expedition.Party.SpellsHealingInventoryAndRejections",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSpellAndPotionTest::RunTest(const FString& Parameters)
{
    FString Error;
    for (auto Role : {EAdventurerRole::Fighter,EAdventurerRole::Rogue,EAdventurerRole::Cleric,EAdventurerRole::Wizard})
    {
        FCombatBattle B; B.Start(RoleDuel(Role),42,Error);
        const auto Before = B.Snapshot();
        const TArray<FCombatCommand> Invalid = {
            {ECombatCommandType::CureWounds,1,{},20}, {ECombatCommandType::Potion,1,{},999},
            {ECombatCommandType::Rescue,1,{},1}, {ECombatCommandType::SecondWind,1,{},20},
            {ECombatCommandType::MagicMissile,1,{},1}};
        for (const auto& C : Invalid)
        {
            TestFalse(TEXT("Invalid ability/target rejected"),B.Submit(C).bAccepted);
            TestTrue(TEXT("No costs or dice on rejection"),Same(Before,B.Snapshot()));
        }
        if (Role != EAdventurerRole::Wizard)
        {
            TestFalse(TEXT("Wrong role cannot cast missile"),B.Submit({ECombatCommandType::MagicMissile,1,{},20}).bAccepted);
            TestTrue(TEXT("Wrong role rejection atomic"),Same(Before,B.Snapshot()));
        }
    }
    auto E = RoleDuel(EAdventurerRole::Cleric); E.Combatants[0].HP=29;
    FCombatBattle B; B.Start(E,42,Error);
    auto R = B.Submit({ECombatCommandType::CureWounds,1,{},1});
    TestTrue(TEXT("Touch self healing caps at maximum"),R.bAccepted && R.Healing==1 && B.Find(1)->HP==30);
    TestEqual(TEXT("Spell slot spent"),B.Find(1)->SpellSlots,1);
    auto After=B.Snapshot();
    TestFalse(TEXT("Action cannot be reused by potion"),B.Submit({ECombatCommandType::Potion,1,{},1}).bAccepted);
    TestTrue(TEXT("No inventory cost on rejection"),Same(After,B.Snapshot()));
    E=RoleDuel(EAdventurerRole::Wizard); B.Start(E,42,Error);
    R=B.Submit({ECombatCommandType::MagicMissile,1,{},20});
    TestTrue(TEXT("Three darts hit without attack or crit"),R.bAccepted && R.bHit && !R.bCritical && R.NaturalRoll==0 && R.Damage>=6 && R.Damage<=15);
    TestEqual(TEXT("One slot for all three darts"),B.Find(1)->SpellSlots,1);
    E=RoleDuel(EAdventurerRole::Rogue); B.Start(E,42,Error);
    R=B.Submit({ECombatCommandType::Potion,1,{},1});
    TestTrue(TEXT("Potion helps"),R.bAccepted && R.Healing>=4 && R.Healing<=10);
    TestEqual(TEXT("Bottle consumed exactly once"),B.Find(1)->Potions,0);
    After=B.Snapshot();
    TestFalse(TEXT("Repeated potion refused"),B.Submit({ECombatCommandType::Potion,1,{},1}).bAccepted);
    TestTrue(TEXT("Repeated potion atomic"),Same(After,B.Snapshot()));
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSneakAttackTest, "Project.Expedition.Party.SneakAttackQualification",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSneakAttackTest::RunTest(const FString& Parameters)
{
    FString Error;
    for (int32 Case=0;Case<4;++Case)
    {
        auto E=RoleDuel(EAdventurerRole::Rogue);
        E.Combatants[0].bSneakWeapon=Case!=1; E.Combatants[0].AttackBonus=30;
        FCombatantState Ally=E.Combatants[0]; Ally.Id=2; Ally.Cell=Case==2?FIntPoint(6,6):FIntPoint(1,1); Ally.InitiativeBonus=-10;
        if(Case==3) { Ally.Life=ECombatLife::Downed; Ally.HP=0; Ally.RescueTurnsLeft=3; }
        E.Combatants.Add(Ally);
        FCombatBattle B; B.Start(E,42,Error);
        const auto R=B.Submit({ECombatCommandType::Attack,1,{},20});
        TestTrue(TEXT("Worked attack hits"),R.bHit);
        TestEqual(TEXT("Requires qualifying weapon and conscious adjacent ally"),R.bSneakAttack,Case==0);
        const auto After=B.Snapshot();
        TestFalse(TEXT("No second sneak/action this turn"),B.Submit({ECombatCommandType::Attack,1,{},20}).bAccepted);
        TestTrue(TEXT("Duplicate preserves state"),Same(After,B.Snapshot()));
    }
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRescueTest, "Project.Expedition.Party.RescueDeadlineAndPermanentDeath",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRescueTest::RunTest(const FString& Parameters)
{
    FString Error;
    auto E=RoleDuel(EAdventurerRole::Cleric);
    FCombatantState Ally=E.Combatants[0]; Ally.Id=2; Ally.Cell={0,1}; Ally.HP=0;
    Ally.Life=ECombatLife::Downed; Ally.RescueTurnsLeft=3; Ally.InitiativeBonus=-10;
    E.Combatants.Add(Ally);
    for (auto Action : {ECombatCommandType::Rescue,ECombatCommandType::CureWounds,ECombatCommandType::Potion})
    {
        FCombatBattle B; B.Start(E,42,Error);
        TestTrue(TEXT("Downed ally is a legal adjacent target"),B.LegalTargets(1,Action).Contains(2));
        const auto R=B.Submit({Action,1,{},2});
        TestTrue(TEXT("Rescued with positive HP"),R.bAccepted && B.Find(2)->HP>0 && B.Find(2)->Life==ECombatLife::Conscious);
        TestEqual(TEXT("Rescue deadline cleared"),B.Find(2)->RescueTurnsLeft,0);
        TestFalse(TEXT("Rescue spends action"),B.Snapshot().bActionAvailable);
    }
    FCombatBattle B; B.Start(E,42,Error);
    for(int32 Tick=1;Tick<=3;++Tick)
    {
        const int32 Round=B.Snapshot().Round;
        // Only legal end-turn commands; no clock, frames or random rolls advance the deadline.
        do { B.Submit({ECombatCommandType::EndTurn,B.ActiveId()}); } while(B.Snapshot().Round==Round);
        TestEqual(TEXT("One countdown tick per own scheduled slot"),B.Find(2)->RescueTurnsLeft,3-Tick);
        TestTrue(TEXT("Distinct downed then dead"),B.Find(2)->Life==(Tick==3?ECombatLife::Dead:ECombatLife::Downed));
    }
    while(B.ActiveId()!=1) { B.Submit({ECombatCommandType::EndTurn,B.ActiveId()}); }
    const auto Before=B.Snapshot();
    for(auto Action:{ECombatCommandType::Rescue,ECombatCommandType::CureWounds,ECombatCommandType::Potion})
    {
        TestFalse(TEXT("Dead adventurer cannot be restored"),B.Submit({Action,1,{},2}).bAccepted);
        TestTrue(TEXT("Dead target rejection atomic"),Same(Before,B.Snapshot()));
    }
    TestFalse(TEXT("Dead ID cannot act"),B.Submit({ECombatCommandType::EndTurn,2}).bAccepted);
    // An actual hit creates the countdown rather than merely removing a turn.
    E=RoleDuel(EAdventurerRole::None);
    E.Combatants[0].InitiativeBonus=-10;
    E.Combatants[1].InitiativeBonus=30; E.Combatants[1].AttackBonus=30; E.Combatants[1].DamageBonus=100;
    Ally.Life=ECombatLife::Conscious; Ally.HP=20; Ally.RescueTurnsLeft=0; E.Combatants.Add(Ally);
    B.Start(E,0,Error);
    TestTrue(TEXT("Enemy downs hero"),B.Submit({ECombatCommandType::Attack,20,{},1}).bHit);
    TestTrue(TEXT("Zero HP is downed while a rescuer lives"),B.Find(1)->Life==ECombatLife::Downed && B.Find(1)->RescueTurnsLeft==3);
    TestTrue(TEXT("Other survivor keeps fight active"),B.Snapshot().Phase==EBattlePhase::Fighting);
    E.Combatants.RemoveAt(2); B.Start(E,0,Error);
    B.Submit({ECombatCommandType::Attack,20,{},1});
    TestTrue(TEXT("No conscious rescuer is a terminal wipe"),B.Snapshot().Phase==EBattlePhase::Defeat && B.Find(1)->Life==ECombatLife::Dead);
    const auto Wipe=B.Snapshot();
    TestFalse(TEXT("Wipe cannot transition"),FPartyContinuity::Advance(B,E,4,1,Error));
    TestTrue(TEXT("Wipe preserved"),Same(Wipe,B.Snapshot()));
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAbilityLimitsTest, "Project.Expedition.Party.RangeSightAndExhaustedResources",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FAbilityLimitsTest::RunTest(const FString& Parameters)
{
    FString Error;
    auto E=RoleDuel(EAdventurerRole::Wizard); E.Combatants[1].Cell={7,7};
    FCombatBattle B; B.Start(E,42,Error);
    auto Before=B.Snapshot();
    TestFalse(TEXT("Missile range is six cells"),B.Submit({ECombatCommandType::MagicMissile,1,{},20}).bAccepted);
    TestTrue(TEXT("Range rejection atomic"),Same(Before,B.Snapshot()));
    E.Combatants[1].Cell={2,0}; E.BlockedCells={{1,0}}; B.Start(E,42,Error); Before=B.Snapshot();
    TestTrue(TEXT("Obscured spell absent from preview"),B.LegalTargets(1,ECombatCommandType::MagicMissile).IsEmpty());
    TestFalse(TEXT("Missile requires sight"),B.Submit({ECombatCommandType::MagicMissile,1,{},20}).bAccepted);
    TestTrue(TEXT("Sight rejection atomic"),Same(Before,B.Snapshot()));
    E=RoleDuel(EAdventurerRole::Wizard); B.Start(E,42,Error);
    for(int32 Cast=0;Cast<2;++Cast)
    {
        TestTrue(TEXT("Available slot can be cast"),B.Submit({ECombatCommandType::MagicMissile,1,{},20}).bAccepted);
        B.Submit({ECombatCommandType::EndTurn,1}); B.Submit({ECombatCommandType::EndTurn,20});
    }
    Before=B.Snapshot();
    TestEqual(TEXT("Two slots exhausted"),B.Find(1)->SpellSlots,0);
    TestFalse(TEXT("Cannot cast without slot"),B.Submit({ECombatCommandType::MagicMissile,1,{},20}).bAccepted);
    TestTrue(TEXT("Exhaustion atomic"),Same(Before,B.Snapshot()));
    TestTrue(TEXT("Cantrip remains useful without a slot"),B.Submit({ECombatCommandType::Attack,1,{},20}).bAccepted);
    E=RoleDuel(EAdventurerRole::Cleric);
    FCombatantState Ally=E.Combatants[0]; Ally.Id=2; Ally.Cell={0,2}; Ally.HP=0; Ally.Life=ECombatLife::Downed; Ally.RescueTurnsLeft=3; Ally.InitiativeBonus=-10;
    E.Combatants.Add(Ally); B.Start(E,42,Error); Before=B.Snapshot();
    for(auto Action:{ECombatCommandType::CureWounds,ECombatCommandType::Rescue,ECombatCommandType::Potion})
    {
        TestFalse(TEXT("Touch actions cannot rescue at two cells"),B.Submit({Action,1,{},2}).bAccepted);
        TestTrue(TEXT("Rejected distant rescue atomic"),Same(Before,B.Snapshot()));
    }
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVictoryLossPolicyTest, "Project.Expedition.Party.VictoryResolvesDownedAndValidatesContinuation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FVictoryLossPolicyTest::RunTest(const FString& Parameters)
{
    FString Error;
    auto E=RoleDuel(EAdventurerRole::Wizard); E.Combatants[1].HP=1;
    FCombatantState Ally=E.Combatants[0]; Ally.Id=2; Ally.Cell={0,1}; Ally.HP=0; Ally.Life=ECombatLife::Downed; Ally.RescueTurnsLeft=3; Ally.InitiativeBonus=-10;
    E.Combatants.Add(Ally);
    FCombatBattle B; B.Start(E,42,Error); B.Submit({ECombatCommandType::MagicMissile,1,{},20});
    TestTrue(TEXT("Unresolved downed ally dies when victory ends rescue window"),B.Snapshot().Phase==EBattlePhase::Victory && B.Find(2)->Life==ECombatLife::Dead);
    const auto Before=B.Snapshot();
    auto Invalid=E; Invalid.Combatants.RemoveAt(2);
    TestFalse(TEXT("Cannot omit dead roster member"),FPartyContinuity::Advance(B,Invalid,7,1,Error));
    TestTrue(TEXT("Missing roster rejection atomic"),Same(Before,B.Snapshot()));
    Invalid=E; Invalid.Combatants[1].Cell=Invalid.Combatants[0].Cell;
    TestFalse(TEXT("Invalid layout rejected before loot"),FPartyContinuity::Advance(B,Invalid,7,1,Error));
    TestTrue(TEXT("Failed layout preserves loot and RNG"),Same(Before,B.Snapshot()));
    E.Combatants[0].HP=30; E.Combatants[0].SpellSlots=2;
    E.Combatants[2].HP=30; E.Combatants[2].Life=ECombatLife::Conscious; E.Combatants[2].RescueTurnsLeft=0;
    TestTrue(TEXT("Layout cannot restore injuries, slots or dead member"),FPartyContinuity::Advance(B,E,7,1,Error));
    TestEqual(TEXT("Carried injury"),B.Find(1)->HP,10);
    TestEqual(TEXT("Carried slot expenditure"),B.Find(1)->SpellSlots,1);
    TestTrue(TEXT("Layout resurrection refused by ownership"),B.Find(2)->Life==ECombatLife::Dead);
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEquipmentDamageTest, "Project.Expedition.Party.EquipmentHasCombatValue",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FEquipmentDamageTest::RunTest(const FString& Parameters)
{
    auto E=RoleDuel(EAdventurerRole::Fighter); E.Combatants[0].WeaponRank=2;
    FCombatBattle B; FString Error; B.Start(E,42,Error);
    const auto R=B.Submit({ECombatCommandType::Attack,1,{},20});
    TestEqual(TEXT("Worked seed: d6=4, modifier=3, owned gear=2"),R.Damage,9);
    TestEqual(TEXT("Gear not consumed on use"),B.Find(1)->WeaponRank,2);
    return !HasAnyErrors();
}

#endif
