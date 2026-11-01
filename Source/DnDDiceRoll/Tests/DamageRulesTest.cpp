#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Combat/CombatBattle.h"

namespace
{
// Attacker (id 1, acts first) against one high-HP enemy whose Role selects an SRD defense profile.
FCombatEncounter DefenseDuel(int32 TutorialIndex, const TCHAR* Profile)
{
    const FCombatEncounter Tutorial = FCombatBattle::TutorialEncounter();
    FCombatEncounter Layout = Tutorial;
    Layout.BlockedCells.Reset();
    Layout.Combatants = {Tutorial.Combatants[TutorialIndex], Tutorial.Combatants[4]};
    FCombatantState& Hero = Layout.Combatants[0];
    Hero.Id = 1; Hero.Cell = {0, 0}; Hero.InitiativeBonus = 30; Hero.AttackBonus = 30;
    FCombatantState& Foe = Layout.Combatants[1];
    Foe.Id = 2; Foe.Role = Profile; Foe.Cell = Hero.Range > 1 ? FIntPoint(2, 0) : FIntPoint(1, 0);
    Foe.MaxHP = Foe.HP = 200; Foe.Speed = 0;
    return Layout;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDamageAdjustTest, "Project.Expedition.Rules.DamageResistanceVulnerabilityImmunity",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FDamageAdjustTest::RunTest(const FString& Parameters)
{
    FDamageDefenses None, Resist, Vulnerable, Both, Immune;
    Resist.Resist(EDamageType::Fire).Resist(EDamageType::Fire);
    Vulnerable.Vulnerable(EDamageType::Fire);
    Both.Resist(EDamageType::Fire).Vulnerable(EDamageType::Fire);
    Immune.Immune(EDamageType::Fire).Resist(EDamageType::Fire);
    TestEqual(TEXT("No defenses"), FDamageRules::Adjust(7, EDamageType::Fire, None), 7);
    TestEqual(TEXT("Resistance halves, rounding down; repeats count once"), FDamageRules::Adjust(7, EDamageType::Fire, Resist), 3);
    TestEqual(TEXT("Vulnerability doubles"), FDamageRules::Adjust(7, EDamageType::Fire, Vulnerable), 14);
    TestEqual(TEXT("Both: halve then double"), FDamageRules::Adjust(7, EDamageType::Fire, Both), 6);
    TestEqual(TEXT("Immunity zeroes and wins"), FDamageRules::Adjust(7, EDamageType::Fire, Immune), 0);
    TestEqual(TEXT("Other types unaffected"), FDamageRules::Adjust(7, EDamageType::Cold, Resist), 7);
    TestTrue(TEXT("Response names"), FDamageRules::Response(EDamageType::Fire, Immune) == EDamageResponse::Immune && FDamageRules::Response(EDamageType::Fire, Both) == EDamageResponse::ResistedAndVulnerable);
    const FDamageDefenses Mephit = FDamageRules::ProfileDefenses(TEXT("Ice Mephit"));
    TestEqual(TEXT("Ice Mephit vulnerable to fire"), FDamageRules::Adjust(5, EDamageType::Fire, Mephit), 10);
    TestEqual(TEXT("Ice Mephit immune to cold"), FDamageRules::Adjust(5, EDamageType::Cold, Mephit), 0);
    TestEqual(TEXT("Fire Elemental resists slashing"), FDamageRules::Adjust(9, EDamageType::Slashing, FDamageRules::ProfileDefenses(TEXT("Fire Elemental"))), 4);
    TestEqual(TEXT("Skeleton vulnerable to bludgeoning"), FDamageRules::Adjust(4, EDamageType::Bludgeoning, FDamageRules::ProfileDefenses(TEXT("Skeleton"))), 8);
    TestEqual(TEXT("Raiders have no defenses"), FDamageRules::ProfileDefenses(TEXT("Raider")).Resistances | FDamageRules::ProfileDefenses(TEXT("Raider")).Immunities, 0);
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDamageTypesInBattleTest, "Project.Expedition.Rules.DamageTypesInBattle",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FDamageTypesInBattleTest::RunTest(const FString& Parameters)
{
    struct FCase { int32 Hero; const TCHAR* Profile; EDamageType Type; EDamageResponse Response; };
    // Tutorial order: 0 Fighter, 1 Rogue, 3 Wizard.
    const FCase Cases[] = {
        {3, TEXT("Raider"), EDamageType::Fire, EDamageResponse::Normal},
        {3, TEXT("Ice Mephit"), EDamageType::Fire, EDamageResponse::Vulnerable},
        {3, TEXT("Fire Elemental"), EDamageType::Fire, EDamageResponse::Immune},
        {0, TEXT("Fire Elemental"), EDamageType::Slashing, EDamageResponse::Resisted},
        {1, TEXT("Fire Elemental"), EDamageType::Piercing, EDamageResponse::Resisted}};
    FString Error;
    for (const FCase& Case : Cases)
    {
        const FString What = FString::Printf(TEXT("hero %d vs %s"), Case.Hero, Case.Profile);
        FCombatBattle Plain, Battle;
        if (!TestTrue(What, Plain.Start(DefenseDuel(Case.Hero, TEXT("Raider")), 3, Error) && Battle.Start(DefenseDuel(Case.Hero, Case.Profile), 3, Error))) { return false; }
        const FCombatSnapshot Before = Battle.Snapshot();
        const FActionForecast Base = Plain.Forecast(1, ECombatCommandType::Attack, 2);
        const FActionForecast Forecast = Battle.Forecast(1, ECombatCommandType::Attack, 2);
        TestTrue(What + TEXT(": forecast valid, type and response"), Forecast.bValid && Forecast.DamageType == Case.Type && Forecast.DamageResponse == Case.Response);
        TestEqual(What + TEXT(": forecast min adjusted"), Forecast.MinAmount, FDamageRules::Adjust(Base.MinAmount, Case.Type, Battle.DamageDefenses(2)));
        TestEqual(What + TEXT(": forecast max adjusted"), Forecast.MaxAmount, FDamageRules::Adjust(Base.MaxAmount, Case.Type, Battle.DamageDefenses(2)));
        TestTrue(What + TEXT(": forecast spends no RNG"), FCombatSnapshot::StaticStruct()->CompareScriptStruct(&Before, &Battle.Snapshot(), 0));
        const FCombatResult Raw = Plain.Submit({ECombatCommandType::Attack, 1, {}, 2});
        const FCombatResult Result = Battle.Submit({ECombatCommandType::Attack, 1, {}, 2});
        TestTrue(What + TEXT(": hit"), Raw.bHit && Result.bHit);
        TestEqual(What + TEXT(": same dice before defenses"), Result.DamageBeforeDefenses, Raw.Damage);
        TestEqual(What + TEXT(": damage adjusted after all modifiers"), Result.Damage, FDamageRules::Adjust(Raw.Damage, Case.Type, Battle.DamageDefenses(2)));
        TestEqual(What + TEXT(": HP reflects adjusted damage"), Battle.Find(2)->HP, 200 - Result.Damage);
        TestTrue(What + TEXT(": result type"), Result.DamageType == Case.Type && Result.DamageResponse == Case.Response);
        TestEqual(What + TEXT(": RNG stream identical to undefended target"), Battle.Snapshot().RandomState, Plain.Snapshot().RandomState);
        TestEqual(What + TEXT(": feedback names response only when it applies"), Result.Feedback.Contains(FDamageRules::ResponseName(Case.Response)), Case.Response != EDamageResponse::Normal);
    }
    FCombatEncounter Missile = DefenseDuel(3, TEXT("Fire Elemental"));
    FCombatBattle Wizard;
    TestTrue(TEXT("Missile duel starts"), Wizard.Start(Missile, 4, Error));
    TestTrue(TEXT("Magic Missile is force damage, not resisted"), Wizard.DamageTypeOf(1, ECombatCommandType::MagicMissile) == EDamageType::Force &&
        Wizard.Forecast(1, ECombatCommandType::MagicMissile, 2).DamageResponse == EDamageResponse::Normal);
    return !HasAnyErrors();
}
#endif
