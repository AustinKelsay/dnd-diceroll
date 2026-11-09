#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Combat/CombatBattle.h"
#include "Rules/SpellcastingRules.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSpellcastingDcAttackSaveTest, "Project.Expedition.Rules.SpellDcAttackAndSaves",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSpellcastingDcAttackSaveTest::RunTest(const FString& Parameters)
{
    TestTrue(TEXT("Cleric casts with Wisdom"), FSpellcastingRules::SpellcastingAbility(EAdventurerRole::Cleric) == EAbility::Wisdom);
    TestTrue(TEXT("Wizard casts with Intelligence"), FSpellcastingRules::SpellcastingAbility(EAdventurerRole::Wizard) == EAbility::Intelligence);
    TestTrue(TEXT("Casters"), FSpellcastingRules::IsSpellcaster(EAdventurerRole::Wizard) && !FSpellcastingRules::IsSpellcaster(EAdventurerRole::Rogue));
    TestTrue(TEXT("All casting roles"), FSpellcastingRules::IsSpellcaster(EAdventurerRole::Bard) && FSpellcastingRules::IsSpellcaster(EAdventurerRole::Druid) && FSpellcastingRules::IsSpellcaster(EAdventurerRole::Paladin) && FSpellcastingRules::IsSpellcaster(EAdventurerRole::Ranger) && FSpellcastingRules::IsSpellcaster(EAdventurerRole::Sorcerer) && FSpellcastingRules::IsSpellcaster(EAdventurerRole::Warlock) && FSpellcastingRules::IsSpellcaster(EAdventurerRole::Artificer));
    TestTrue(TEXT("Cha casters"), FSpellcastingRules::SpellcastingAbility(EAdventurerRole::Sorcerer) == EAbility::Charisma && FSpellcastingRules::SpellcastingAbility(EAdventurerRole::Warlock) == EAbility::Charisma && FSpellcastingRules::SpellcastingAbility(EAdventurerRole::Bard) == EAbility::Charisma && FSpellcastingRules::SpellcastingAbility(EAdventurerRole::Paladin) == EAbility::Charisma);
    TestTrue(TEXT("Wis casters"), FSpellcastingRules::SpellcastingAbility(EAdventurerRole::Druid) == EAbility::Wisdom && FSpellcastingRules::SpellcastingAbility(EAdventurerRole::Ranger) == EAbility::Wisdom);
    TestTrue(TEXT("Artificer Int"), FSpellcastingRules::SpellcastingAbility(EAdventurerRole::Artificer) == EAbility::Intelligence);
    TestEqual(TEXT("Save DC: 8 + 2 + 3"), FSpellcastingRules::SaveDc(2, 3), 13);
    TestEqual(TEXT("Spell attack: 2 + 3"), FSpellcastingRules::AttackBonus(2, 3), 5);
    TestTrue(TEXT("Fire Bolt is a spell attack"), FSpellcastingRules::Delivery(ESpell::FireBolt).Kind == ESpellDelivery::Attack);
    const FSpellDelivery Flame = FSpellcastingRules::Delivery(ESpell::SacredFlame);
    TestTrue(TEXT("Sacred Flame: DEX save, no damage on success"), Flame.Kind == ESpellDelivery::Save && Flame.SaveAbility == EAbility::Dexterity && !Flame.bHalfOnSave);
    const FSpellDelivery Wave = FSpellcastingRules::Delivery(ESpell::Thunderwave);
    TestTrue(TEXT("Thunderwave: CON save, half on success"), Wave.Kind == ESpellDelivery::Save && Wave.SaveAbility == EAbility::Constitution && Wave.bHalfOnSave);
    TestTrue(TEXT("Magic Missile never misses"), FSpellcastingRules::Delivery(ESpell::MagicMissile).Kind == ESpellDelivery::Automatic);
    TestEqual(TEXT("Attack +5 vs AC 15: 11 faces hit"), FSpellcastingRules::AttackHitOutOf400(5, 15, ERollMode::Normal), 11 * 20);
    TestEqual(TEXT("Natural 20 always hits"), FSpellcastingRules::AttackHitOutOf400(0, 40, ERollMode::Normal), 20);
    TestEqual(TEXT("Natural 1 always misses"), FSpellcastingRules::AttackHitOutOf400(30, 5, ERollMode::Normal), 19 * 20);
    TestEqual(TEXT("Save +1 vs DC 13: fails on 1-11"), FSpellcastingRules::SaveFailOutOf400(1, 13, ERollMode::Normal), 11 * 20);
    TestEqual(TEXT("Save with advantage fails less: 11^2"), FSpellcastingRules::SaveFailOutOf400(1, 13, ERollMode::Advantage), 121);
    TestEqual(TEXT("Failed save: full"), FSpellcastingRules::DamageAfterSave(13, false, true), 13);
    TestEqual(TEXT("Saved, half: rounds down"), FSpellcastingRules::DamageAfterSave(13, true, true), 6);
    TestEqual(TEXT("Saved, no half: none"), FSpellcastingRules::DamageAfterSave(13, true, false), 0);
    TestTrue(TEXT("Ray of Frost is a spell attack"), FSpellcastingRules::Delivery(ESpell::RayOfFrost).Kind == ESpellDelivery::Attack);
    const FSpellDelivery Inflict = FSpellcastingRules::Delivery(ESpell::InflictWounds);
    TestTrue(TEXT("Inflict Wounds CON save half"), Inflict.Kind == ESpellDelivery::Save && Inflict.SaveAbility == EAbility::Constitution && Inflict.bHalfOnSave);
    const FSpellDelivery SleepSave = FSpellcastingRules::Delivery(ESpell::Sleep);
    TestTrue(TEXT("Sleep Wis save no half"), SleepSave.Kind == ESpellDelivery::Save && SleepSave.SaveAbility == EAbility::Wisdom && !SleepSave.bHalfOnSave);
    TestTrue(TEXT("Spiritual Weapon is a spell attack"), FSpellcastingRules::Delivery(ESpell::SpiritualWeapon).Kind == ESpellDelivery::Attack);
    TestTrue(TEXT("Misty Step automatic"), FSpellcastingRules::Delivery(ESpell::MistyStep).Kind == ESpellDelivery::Automatic);
    const FSpellDelivery Bane = FSpellcastingRules::Delivery(ESpell::Bane);
    TestTrue(TEXT("Bane Cha save"), Bane.Kind == ESpellDelivery::Save && Bane.SaveAbility == EAbility::Charisma);
    return !HasAnyErrors();
}
#endif
