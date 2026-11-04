#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Rules/WeaponRules.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWeaponPropertiesTest, "Project.Expedition.Rules.WeaponProperties",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FWeaponPropertiesTest::RunTest(const FString& Parameters)
{
    const FWeaponProfile& Rapier = FWeaponRules::Profile(EWeapon::Rapier);
    const FWeaponProfile& Longsword = FWeaponRules::Profile(EWeapon::Longsword);
    const FWeaponProfile& Longbow = FWeaponRules::Profile(EWeapon::Longbow);
    const FWeaponProfile& Dagger = FWeaponRules::Profile(EWeapon::Dagger);
    const FWeaponProfile& Glaive = FWeaponRules::Profile(EWeapon::Glaive);
    const FWeaponProfile& HandCrossbow = FWeaponRules::Profile(EWeapon::HandCrossbow);
    const FWeaponProfile& Shortsword = FWeaponRules::Profile(EWeapon::Shortsword);
    FAbilityScores Nimble; Nimble.Strength = 10; Nimble.Dexterity = 16;
    FAbilityScores Strong; Strong.Strength = 16; Strong.Dexterity = 12;
    TestTrue(TEXT("Table: longsword 1d8 slashing, versatile 1d10"), Longsword.DieCount == 1 && Longsword.DieSides == 8 && Longsword.DamageType == EDamageType::Slashing && Longsword.VersatileSides == 10);
    TestTrue(TEXT("Table: greatsword 2d6"), FWeaponRules::Profile(EWeapon::Greatsword).DieCount == 2 && FWeaponRules::Profile(EWeapon::Greatsword).DieSides == 6);
    // Finesse
    TestTrue(TEXT("Finesse uses DEX when higher"), FWeaponRules::AttackAbility(Rapier, Nimble) == EAbility::Dexterity);
    TestTrue(TEXT("Finesse uses STR when higher"), FWeaponRules::AttackAbility(Rapier, Strong) == EAbility::Strength);
    TestTrue(TEXT("Non-finesse melee uses STR"), FWeaponRules::AttackAbility(Longsword, Nimble) == EAbility::Strength);
    TestTrue(TEXT("Ranged weapon uses DEX"), FWeaponRules::AttackAbility(Longbow, Strong) == EAbility::Dexterity);
    TestTrue(TEXT("Thrown javelin uses STR"), FWeaponRules::AttackAbility(FWeaponRules::Profile(EWeapon::Javelin), Nimble) == EAbility::Strength);
    // Reach
    TestEqual(TEXT("Reach: glaive 10 ft = 2 cells"), FWeaponRules::ReachCells(Glaive), 2);
    TestTrue(TEXT("Reach: glaive hits at 2 cells, longsword does not"), FWeaponRules::Band(Glaive, 2, false) == ERangeBand::Melee && FWeaponRules::Band(Longsword, 2, false) == ERangeBand::OutOfRange);
    // Range (normal/long) and Ammunition
    TestTrue(TEXT("Longbow 150 ft normal = 30 cells"), FWeaponRules::Band(Longbow, 30, false) == ERangeBand::Normal);
    TestTrue(TEXT("Longbow 155 ft is long range (disadvantage)"), FWeaponRules::Band(Longbow, 31, false) == ERangeBand::Long);
    TestTrue(TEXT("Longbow beyond 600 ft is out of range"), FWeaponRules::Band(Longbow, 121, false) == ERangeBand::OutOfRange);
    int32 Arrows = 1;
    TestTrue(TEXT("Ammunition: one arrow spent"), FWeaponRules::SpendAmmunition(Longbow, Arrows) && Arrows == 0);
    TestFalse(TEXT("Ammunition: no arrow, no shot, count untouched"), FWeaponRules::SpendAmmunition(Longbow, Arrows));
    TestEqual(TEXT("Ammunition untouched when rejected"), Arrows, 0);
    int32 None = 0;
    TestTrue(TEXT("Melee weapons need no ammunition"), FWeaponRules::SpendAmmunition(Longsword, None));
    // Thrown
    TestTrue(TEXT("Thrown dagger: 4 cells normal, 5 long, 13 out"), FWeaponRules::Band(Dagger, 4, true) == ERangeBand::Normal &&
        FWeaponRules::Band(Dagger, 5, true) == ERangeBand::Long && FWeaponRules::Band(Dagger, 13, true) == ERangeBand::OutOfRange);
    TestTrue(TEXT("Dagger in hand is melee at 1 cell"), FWeaponRules::Band(Dagger, 1, false) == ERangeBand::Melee);
    TestTrue(TEXT("Longsword cannot be thrown"), FWeaponRules::Band(Longsword, 3, true) == ERangeBand::OutOfRange);
    // Versatile, Two-handed and Critical
    int32 Count = 0, Sides = 0;
    FWeaponRules::DamageDice(Longsword, false, false, Count, Sides); TestTrue(TEXT("Versatile one-handed 1d8"), Count == 1 && Sides == 8);
    FWeaponRules::DamageDice(Longsword, true, false, Count, Sides); TestTrue(TEXT("Versatile two-handed 1d10"), Count == 1 && Sides == 10);
    FWeaponRules::DamageDice(FWeaponRules::Profile(EWeapon::Greatsword), true, true, Count, Sides); TestTrue(TEXT("Critical doubles dice: 4d6"), Count == 4 && Sides == 6);
    TestFalse(TEXT("Two-handed: no shield with a longbow"), FWeaponRules::CanWield(Longbow, true, true));
    TestFalse(TEXT("Versatile two-handed: no shield"), FWeaponRules::CanWield(Longsword, true, true));
    TestTrue(TEXT("Versatile one-handed with shield"), FWeaponRules::CanWield(Longsword, false, true));
    // Light
    TestTrue(TEXT("Light: shortsword + dagger two-weapon fighting"), FWeaponRules::CanTwoWeaponFight(Shortsword, Dagger));
    TestFalse(TEXT("Light: longsword is not light"), FWeaponRules::CanTwoWeaponFight(Longsword, Dagger));
    TestEqual(TEXT("Off-hand adds no positive modifier"), FWeaponRules::DamageModifier(3, true), 0);
    TestEqual(TEXT("Off-hand keeps a negative modifier"), FWeaponRules::DamageModifier(-1, true), -1);
    // Heavy
    {
        FAbilityScores HeavyWeak; HeavyWeak.Strength = 12; HeavyWeak.Dexterity = 16;
        FAbilityScores HeavyStr; HeavyStr.Strength = 13; HeavyStr.Dexterity = 12;
        const FWeaponProfile& HeavyBow = FWeaponRules::Profile(EWeapon::Longbow);
        TestTrue(TEXT("Heavy: STR 12 melee has disadvantage"), FWeaponRules::HeavyImposesDisadvantage(Glaive, HeavyWeak));
        TestFalse(TEXT("Heavy: STR 13 melee does not"), FWeaponRules::HeavyImposesDisadvantage(Glaive, HeavyStr));
        TestTrue(TEXT("Heavy: DEX 12 ranged has disadvantage"), FWeaponRules::HeavyImposesDisadvantage(HeavyBow, HeavyStr));
        TestFalse(TEXT("Heavy: DEX 16 ranged does not"), FWeaponRules::HeavyImposesDisadvantage(HeavyBow, HeavyWeak));
        TestFalse(TEXT("Heavy: non-heavy weapon never"), FWeaponRules::HeavyImposesDisadvantage(Dagger, HeavyWeak));
    }
    TestTrue(TEXT("Light: hand crossbow + dagger qualify (5.2.1)"), FWeaponRules::CanTwoWeaponFight(FWeaponRules::Profile(EWeapon::HandCrossbow), Dagger));
    // Loading
    TestEqual(TEXT("Loading: one shot per action even with two attacks"), FWeaponRules::AttacksPerAction(HandCrossbow, 2), 1);
    TestEqual(TEXT("No loading: both attacks"), FWeaponRules::AttacksPerAction(Longbow, 2), 2);
    return !HasAnyErrors();
}

#endif
