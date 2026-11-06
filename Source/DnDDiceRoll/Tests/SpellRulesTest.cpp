#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Combat/CombatBattle.h"
#include "Rules/SpellRules.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSpellFactsSlotsTest, "Project.Expedition.Rules.SpellFactsAndSlots",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSpellFactsSlotsTest::RunTest(const FString& Parameters)
{
    const FSpellFacts& FireBolt = FSpellRules::Facts(ESpell::FireBolt);
    TestTrue(TEXT("Fire Bolt: evocation cantrip, 120 ft, V S, 1d10 fire, Wizard"), FireBolt.Level == 0 && FireBolt.School == ESpellSchool::Evocation && FireBolt.RangeFeet == 120 &&
        FireBolt.bVerbal && FireBolt.bSomatic && !FireBolt.bMaterial && FireBolt.DieCount == 1 && FireBolt.DieSides == 10 && FireBolt.DamageType == EDamageType::Fire && FireBolt.bWizard);
    const FSpellFacts& Missile = FSpellRules::Facts(ESpell::MagicMissile);
    TestTrue(TEXT("Magic Missile: 1st-level, 3 darts of 1d4 force"), Missile.Level == 1 && Missile.DieCount == 3 && Missile.DieSides == 4 && Missile.DamageType == EDamageType::Force);
    const FSpellFacts& Cure = FSpellRules::Facts(ESpell::CureWounds);
    TestTrue(TEXT("Cure Wounds: touch healing 1d8, Cleric"), Cure.Level == 1 && Cure.bHealing && Cure.DieSides == 8 && Cure.RangeFeet == 0 && Cure.bCleric && !Cure.bWizard);
    TestTrue(TEXT("Healing Word is a bonus action, verbal only"), FCString::Strcmp(FSpellRules::Facts(ESpell::HealingWord).CastingTime, TEXT("1 bonus action")) == 0 && !FSpellRules::Facts(ESpell::HealingWord).bSomatic);
    TestEqual(TEXT("Tooltip summary"), FSpellRules::Summary(ESpell::MagicMissile), FString(TEXT("1st-level evocation | 1 action | 120 ft | V, S | Instantaneous")));
    TestEqual(TEXT("Cantrip summary"), FSpellRules::Summary(ESpell::FireBolt), FString(TEXT("evocation cantrip | 1 action | 120 ft | V, S | Instantaneous")));
    // Slots: full caster table.
    TestEqual(TEXT("Wizard 1: two 1st"), FSpellRules::SlotsFor(EAdventurerRole::Wizard, 1, 1), 2);
    TestEqual(TEXT("Cleric 3: four 1st"), FSpellRules::SlotsFor(EAdventurerRole::Cleric, 3, 1), 4);
    TestEqual(TEXT("Cleric 3: two 2nd"), FSpellRules::SlotsFor(EAdventurerRole::Cleric, 3, 2), 2);
    TestEqual(TEXT("Wizard 4: three 2nd"), FSpellRules::SlotsFor(EAdventurerRole::Wizard, 4, 2), 3);
    TestEqual(TEXT("Wizard 5: two 3rd"), FSpellRules::SlotsFor(EAdventurerRole::Wizard, 5, 3), 2);
    TestEqual(TEXT("Wizard 4: no 3rd"), FSpellRules::SlotsFor(EAdventurerRole::Wizard, 4, 3), 0);
    TestEqual(TEXT("Fighter: no slots"), FSpellRules::SlotsFor(EAdventurerRole::Fighter, 5, 1), 0);
    TestEqual(TEXT("Max slot level, Cleric 3"), FSpellRules::MaxSlotLevel(EAdventurerRole::Cleric, 3), 2);
    TestTrue(TEXT("Class lists"), FSpellRules::OnClassList(EAdventurerRole::Cleric, ESpell::CureWounds) && !FSpellRules::OnClassList(EAdventurerRole::Wizard, ESpell::CureWounds) && FSpellRules::OnClassList(EAdventurerRole::Wizard, ESpell::MagicMissile));
    // Upcasting.
    TestEqual(TEXT("Magic Missile at 2nd: 4 darts"), FSpellRules::UpcastDiceCount(ESpell::MagicMissile, 2), 4);
    TestEqual(TEXT("Cure Wounds at 3rd: 3d8"), FSpellRules::UpcastDiceCount(ESpell::CureWounds, 3), 3);
    TestEqual(TEXT("Guiding Bolt at 1st: 4d6"), FSpellRules::UpcastDiceCount(ESpell::GuidingBolt, 1), 4);
    TestEqual(TEXT("Slot too low"), FSpellRules::UpcastDiceCount(ESpell::MagicMissile, 0), 0);
    TestTrue(TEXT("Shield is Wizard abjuration reaction"), FSpellRules::OnClassList(EAdventurerRole::Wizard, ESpell::Shield) && !FSpellRules::OnClassList(EAdventurerRole::Cleric, ESpell::Shield));
    TestEqual(TEXT("Shield school"), (int32)FSpellRules::Facts(ESpell::Shield).School, (int32)ESpellSchool::Abjuration);
    TestTrue(TEXT("Shield concentrates"), FSpellRules::Facts(ESpell::Shield).bConcentration);
    // Every catalog spell must have school + at least one class tag (#116 lean).
    for (int32 SpellIndex = 0; SpellIndex < (int32)ESpell::Count; ++SpellIndex)
    {
        const FSpellFacts& F = FSpellRules::Facts((ESpell)SpellIndex);
        TestTrue(TEXT("Spell has a class tag"), F.bCleric || F.bWizard);
        TestTrue(TEXT("Spell school in range"), (int32)F.School >= 0 && (int32)F.School <= 7);
    }
    return !HasAnyErrors();
}
#endif
