#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Combat/CombatBattle.h"
#include "Rules/ArmorRules.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FArmorClassDexCapsTest, "Project.Expedition.Rules.ArmorClassDexCaps",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FArmorClassDexCapsTest::RunTest(const FString& Parameters)
{
    TestEqual(TEXT("Unarmored DEX 16: 10 + 3"), FArmorRules::ArmorClass(EArmor::None, false, 16), 13);
    TestEqual(TEXT("Studded leather DEX 18: 12 + 4 (light, no cap)"), FArmorRules::ArmorClass(EArmor::StuddedLeather, false, 18), 16);
    TestEqual(TEXT("Breastplate DEX 18: 14 + 2 (medium caps DEX at 2)"), FArmorRules::ArmorClass(EArmor::Breastplate, false, 18), 16);
    TestEqual(TEXT("Half plate DEX 12: 15 + 1"), FArmorRules::ArmorClass(EArmor::HalfPlate, false, 12), 16);
    TestEqual(TEXT("Chain mail DEX 18: 16 (heavy ignores DEX)"), FArmorRules::ArmorClass(EArmor::ChainMail, false, 18), 16);
    TestEqual(TEXT("Chain mail DEX 8: 16 (heavy ignores negative DEX too)"), FArmorRules::ArmorClass(EArmor::ChainMail, false, 8), 16);
    TestEqual(TEXT("Plate + shield: 20"), FArmorRules::ArmorClass(EArmor::Plate, true, 10), 20);
    TestEqual(TEXT("Leather DEX 8: 11 - 1"), FArmorRules::ArmorClass(EArmor::Leather, false, 8), 10);
    TestEqual(TEXT("Medium DEX cap"), FArmorRules::DexCap(EArmorCategory::Medium), 2);
    TestEqual(TEXT("Heavy DEX cap"), FArmorRules::DexCap(EArmorCategory::Heavy), 0);
    TestTrue(TEXT("Table: splint 17, STR 15, stealth disadvantage"), FArmorRules::Profile(EArmor::Splint).BaseAC == 17 && FArmorRules::Profile(EArmor::Splint).StrengthRequirement == 15 && FArmorRules::Profile(EArmor::Splint).bStealthDisadvantage);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FArmorProficiencyPenaltiesTest, "Project.Expedition.Rules.ArmorProficiencyPenalties",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FArmorProficiencyPenaltiesTest::RunTest(const FString& Parameters)
{
    TestTrue(TEXT("Fighter: all armor and shields"), FArmorRules::IsProficient(EAdventurerRole::Fighter, EArmorCategory::Heavy) && FArmorRules::IsProficient(EAdventurerRole::Fighter, EArmorCategory::Shield));
    TestTrue(TEXT("Cleric: medium and shield, not heavy"), FArmorRules::IsProficient(EAdventurerRole::Cleric, EArmorCategory::Medium) && FArmorRules::IsProficient(EAdventurerRole::Cleric, EArmorCategory::Shield) && !FArmorRules::IsProficient(EAdventurerRole::Cleric, EArmorCategory::Heavy));
    TestTrue(TEXT("Rogue: light only"), FArmorRules::IsProficient(EAdventurerRole::Rogue, EArmorCategory::Light) && !FArmorRules::IsProficient(EAdventurerRole::Rogue, EArmorCategory::Medium));
    TestTrue(TEXT("Wizard: no armor, unarmored fine"), !FArmorRules::IsProficient(EAdventurerRole::Wizard, EArmorCategory::Light) && FArmorRules::IsProficient(EAdventurerRole::Wizard, EArmorCategory::None));
    const FArmorPenalties WizardLeather = FArmorRules::Penalties(EAdventurerRole::Wizard, EArmor::Leather, false, 10);
    TestTrue(TEXT("Unproficient: STR/DEX disadvantage and no spellcasting"), WizardLeather.bStrDexDisadvantage && WizardLeather.bCannotCastSpells);
    const FArmorPenalties RogueShield = FArmorRules::Penalties(EAdventurerRole::Rogue, EArmor::Leather, true, 10);
    TestTrue(TEXT("Unproficient shield alone applies the penalty"), RogueShield.bStrDexDisadvantage);
    const FArmorPenalties WeakFighter = FArmorRules::Penalties(EAdventurerRole::Fighter, EArmor::Plate, true, 13);
    TestTrue(TEXT("Plate under STR 15: -10 ft, stealth disadvantage, still proficient"), WeakFighter.SpeedPenaltyFeet == 10 && WeakFighter.bStealthDisadvantage && !WeakFighter.bStrDexDisadvantage);
    TestEqual(TEXT("Plate at STR 15: no speed penalty"), FArmorRules::Penalties(EAdventurerRole::Fighter, EArmor::Plate, false, 15).SpeedPenaltyFeet, 0);
    TestFalse(TEXT("Breastplate: no stealth disadvantage"), FArmorRules::Penalties(EAdventurerRole::Cleric, EArmor::Breastplate, false, 10).bStealthDisadvantage);
    return true;
}
#endif
