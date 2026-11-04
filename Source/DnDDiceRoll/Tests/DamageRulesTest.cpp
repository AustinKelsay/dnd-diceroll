#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Rules/DamageRules.h"
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

#endif
