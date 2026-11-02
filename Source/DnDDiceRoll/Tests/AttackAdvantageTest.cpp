#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Combat/CombatBattle.h"

namespace
{
FCombatEncounter RangedLayout(bool bHostileAdjacent)
{
    const FCombatEncounter Tutorial = FCombatBattle::TutorialEncounter();
    FCombatEncounter Layout = Tutorial;
    Layout.BlockedCells.Reset();
    Layout.Combatants = {Tutorial.Combatants[1], Tutorial.Combatants[0], Tutorial.Combatants[4], Tutorial.Combatants[5]};
    FCombatantState& Rogue = Layout.Combatants[0];
    Rogue.Id = 1; Rogue.Cell = {0, 0}; Rogue.Range = 6; Rogue.AttackBonus = 5; Rogue.InitiativeBonus = 30; Rogue.bSneakWeapon = true;
    FCombatantState& Ally = Layout.Combatants[1];
    Ally.Id = 2; Ally.Cell = {5, 0};
    FCombatantState& Near = Layout.Combatants[2];
    Near.Id = 3; Near.Cell = bHostileAdjacent ? FIntPoint(1, 0) : FIntPoint(0, 5);
    FCombatantState& Target = Layout.Combatants[3];
    Target.Id = 4; Target.Cell = {4, 0}; Target.ArmorClass = 12; Target.MaxHP = Target.HP = 200;
    return Layout;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAttackAdvantageForecastTest, "Project.Expedition.Rules.AttackDisadvantageForecast",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FAttackAdvantageForecastTest::RunTest(const FString& Parameters)
{
    FString Error;
    FCombatBattle Clear;
    TestTrue(TEXT("Clear layout starts"), Clear.Start(RangedLayout(false), 5, Error));
    const FActionForecast Normal = Clear.Forecast(1, ECombatCommandType::Attack, 4);
    TestTrue(TEXT("No adjacent hostile: normal roll"), Normal.bValid && Normal.RollMode == ERollMode::Normal && Normal.DisadvantageSources.IsEmpty());
    TestTrue(TEXT("Normal chance unchanged: 14 faces"), Normal.HitChanceOutOf400 == 280 && Normal.HitChancePercent == 70);
    TestTrue(TEXT("Sneak Attack from the adjacent ally"), Normal.bSneakAttack);

    FCombatBattle Pressed;
    TestTrue(TEXT("Pressed layout starts"), Pressed.Start(RangedLayout(true), 5, Error));
    const FCombatSnapshot Before = Pressed.Snapshot();
    const FAttackRollMode Mode = Pressed.AttackRollMode(1, 4);
    TestTrue(TEXT("Ranged with a hostile adjacent has disadvantage"), Mode.Mode == ERollMode::Disadvantage && Mode.Disadvantage.Num() == 1 && Mode.Advantage.IsEmpty());
    const FActionForecast Forecast = Pressed.Forecast(1, ECombatCommandType::Attack, 4);
    TestTrue(TEXT("Forecast names the source"), Forecast.RollMode == ERollMode::Disadvantage && Forecast.DisadvantageSources.Num() == 1);
    TestTrue(TEXT("Disadvantage chance is exact: 14x14 of 400"), Forecast.HitChanceOutOf400 == 196 && Forecast.HitChancePercent == 49);
    TestFalse(TEXT("Disadvantage denies Sneak Attack"), Forecast.bSneakAttack);
    TestEqual(TEXT("Melee attack on the adjacent hostile is normal"), (int32)Pressed.AttackRollMode(3, 1).Mode, (int32)ERollMode::Normal);
    TestTrue(TEXT("Queries spend no RNG"), FCombatSnapshot::StaticStruct()->CompareScriptStruct(&Before, &Pressed.Snapshot(), 0));
    // SRD 5.2.1: only an adjacent enemy that can see the attacker and isn't Incapacitated imposes disadvantage.
    for (ECombatCondition Blocker : {ECombatCondition::Incapacitated, ECombatCondition::Stunned, ECombatCondition::Blinded})
    {
        FCombatEncounter Layout = RangedLayout(true);
        FCombatCondition C; C.Condition = Blocker; Layout.Combatants[2].Conditions.Add(C);
        FCombatBattle Battle;
        TestTrue(TEXT("Blocked layout starts"), Battle.Start(Layout, 5, Error));
        TestTrue(FString::Printf(TEXT("%s neighbour imposes no ranged disadvantage"), FCombatBattle::ConditionName(Blocker)),
            Battle.AttackRollMode(1, 4).Disadvantage.IsEmpty());
    }
    FCombatEncounter Hidden = RangedLayout(true);
    FCombatCondition Invisible; Invisible.Condition = ECombatCondition::Invisible; Hidden.Combatants[0].Conditions.Add(Invisible);
    FCombatBattle Unseen;
    TestTrue(TEXT("Invisible archer layout starts"), Unseen.Start(Hidden, 5, Error));
    TestFalse(TEXT("Neighbour that can't see an invisible archer imposes no disadvantage"),
        Unseen.AttackRollMode(1, 4).Disadvantage.ContainsByPredicate([](const FString& S) { return S.Contains(TEXT("adjacent")); }));
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAttackAdvantageResolveTest, "Project.Expedition.Rules.AttackDisadvantageResolution",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FAttackAdvantageResolveTest::RunTest(const FString& Parameters)
{
    FString Error;
    for (int32 Seed = 1; Seed <= 20; ++Seed)
    {
        for (bool bPressed : {false, true})
        {
            FCombatBattle Battle;
            if (!TestTrue(TEXT("Layout starts"), Battle.Start(RangedLayout(bPressed), Seed, Error))) { return false; }
            if (!TestEqual(TEXT("Rogue acts first"), Battle.ActiveId(), 1)) { return false; }
            FRandomStream Stream(Battle.Snapshot().RandomState);
            const int32 First = Stream.RandRange(1, 20);
            const int32 Second = Stream.RandRange(1, 20);
            const FCombatResult Result = Battle.Submit({ECombatCommandType::Attack, 1, {}, 4});
            TestTrue(TEXT("Attack accepted"), Result.bAccepted);
            if (!bPressed)
            {
                TestTrue(TEXT("Normal attack rolls one d20"), Result.AttackRolls.Num() == 1 && Result.NaturalRoll == First && Result.RollMode == ERollMode::Normal);
                continue;
            }
            TestTrue(TEXT("Disadvantage rolls two d20 from the seeded stream"), Result.AttackRolls.Num() == 2 && Result.AttackRolls[0] == First && Result.AttackRolls[1] == Second);
            TestEqual(TEXT("Keeps the lower die"), Result.NaturalRoll, FMath::Min(First, Second));
            TestTrue(TEXT("Reports mode and source"), Result.RollMode == ERollMode::Disadvantage && Result.RollModeSources.Num() == 1);
            TestFalse(TEXT("No Sneak Attack with disadvantage"), Result.bSneakAttack);
            TestTrue(TEXT("Feedback names disadvantage"), Result.Feedback.Contains(TEXT("disadvantage")));
        }
    }
    return !HasAnyErrors();
}
#endif
