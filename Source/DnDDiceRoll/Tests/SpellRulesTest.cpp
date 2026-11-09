#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Rules/SpellRules.h"
#include "Combat/CombatBattle.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSpellFactsSlotsTest, "Project.Expedition.Rules.SpellFactsAndSlots",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSpellFactsSlotsTest::RunTest(const FString& Parameters)
{
    const FSpellFacts& FireBolt = FSpellRules::Facts(ESpell::FireBolt);
    TestTrue(TEXT("Fire Bolt: evocation cantrip, 120 ft, V S, 1d10 fire, Wizard"), FireBolt.Level == 0 && FireBolt.School == ESpellSchool::Evocation && FireBolt.RangeFeet == 120 &&
        FireBolt.bVerbal && FireBolt.bSomatic && !FireBolt.bMaterial && !FireBolt.bConcentration && FireBolt.DieCount == 1 && FireBolt.DieSides == 10 && FireBolt.bWizard && !FireBolt.bCleric);
    TestTrue(TEXT("Fire Bolt also Sorcerer/Artificer"), FireBolt.bSorcerer && FireBolt.bArtificer && !FireBolt.bWarlock);
    const FSpellFacts& Sacred = FSpellRules::Facts(ESpell::SacredFlame);
    TestTrue(TEXT("Sacred Flame: Cleric cantrip, 60 ft, 1d8 radiant, no concentration"), Sacred.bCleric && !Sacred.bWizard && Sacred.RangeFeet == 60 && Sacred.DieSides == 8 && !Sacred.bConcentration);
    const FSpellFacts& Missile = FSpellRules::Facts(ESpell::MagicMissile);
    TestTrue(TEXT("Magic Missile: 1st-level, 3 darts of 1d4+1 force"), Missile.Level == 1 && Missile.DieCount == 3 && Missile.DieSides == 4 && Missile.FlatBonus == 1 && Missile.DamageType == EDamageType::Force && !Missile.bConcentration);
    TestTrue(TEXT("Magic Missile Sorcerer+Wizard only"), Missile.bWizard && Missile.bSorcerer && !Missile.bCleric && !Missile.bBard);
    const FSpellFacts& Cure = FSpellRules::Facts(ESpell::CureWounds);
    TestTrue(TEXT("Cure Wounds: Abjuration touch healing 2d8, Cleric (SRD 5.2.1)"), Cure.Level == 1 && Cure.School == ESpellSchool::Abjuration && Cure.bHealing && Cure.DieCount == 2 && Cure.DieSides == 8 && Cure.RangeFeet == 0 && Cure.bCleric && !Cure.bWizard && !Cure.bConcentration);
    TestTrue(TEXT("Cure Wounds half-casters and Bard/Druid/Artificer"), Cure.bBard && Cure.bDruid && Cure.bPaladin && Cure.bRanger && Cure.bArtificer && !Cure.bSorcerer && !Cure.bWarlock);
    TestTrue(TEXT("Healing Word: Abjuration bonus action, V only, 2d4 (SRD 5.2.1)"), FCString::Strcmp(FSpellRules::Facts(ESpell::HealingWord).CastingTime, TEXT("1 bonus action")) == 0 && !FSpellRules::Facts(ESpell::HealingWord).bSomatic && FSpellRules::Facts(ESpell::HealingWord).School == ESpellSchool::Abjuration && FSpellRules::Facts(ESpell::HealingWord).DieCount == 2 && FSpellRules::Facts(ESpell::HealingWord).DieSides == 4 && !FSpellRules::Facts(ESpell::HealingWord).bConcentration);
    TestTrue(TEXT("Healing Word Bard/Cleric/Druid"), FSpellRules::Facts(ESpell::HealingWord).bBard && FSpellRules::Facts(ESpell::HealingWord).bCleric && FSpellRules::Facts(ESpell::HealingWord).bDruid && !FSpellRules::Facts(ESpell::HealingWord).bPaladin);
    const FSpellFacts& Guide = FSpellRules::Facts(ESpell::GuidingBolt);
    TestTrue(TEXT("Guiding Bolt: 1 round, no concentration"), Guide.School == ESpellSchool::Evocation && !Guide.bConcentration && FCString::Strcmp(Guide.Duration, TEXT("1 round")) == 0);
    TestEqual(TEXT("Tooltip summary"), FSpellRules::Summary(ESpell::MagicMissile), FString(TEXT("1st-level evocation | 1 action | 120 ft | V, S | Instantaneous")));
    TestEqual(TEXT("Cantrip summary"), FSpellRules::Summary(ESpell::FireBolt), FString(TEXT("evocation cantrip | 1 action | 120 ft | V, S | Instantaneous")));

    // Full casters L1-5
    TestEqual(TEXT("Wizard 1: two 1st"), FSpellRules::SlotsFor(EAdventurerRole::Wizard, 1, 1), 2);
    TestEqual(TEXT("Cleric 3: four 1st"), FSpellRules::SlotsFor(EAdventurerRole::Cleric, 3, 1), 4);
    TestEqual(TEXT("Cleric 3: two 2nd"), FSpellRules::SlotsFor(EAdventurerRole::Cleric, 3, 2), 2);
    TestEqual(TEXT("Wizard 4: three 2nd"), FSpellRules::SlotsFor(EAdventurerRole::Wizard, 4, 2), 3);
    TestEqual(TEXT("Wizard 5: two 3rd"), FSpellRules::SlotsFor(EAdventurerRole::Wizard, 5, 3), 2);
    TestEqual(TEXT("Wizard 4: no 3rd"), FSpellRules::SlotsFor(EAdventurerRole::Wizard, 4, 3), 0);
    TestEqual(TEXT("Bard 5 matches full caster"), FSpellRules::SlotsFor(EAdventurerRole::Bard, 5, 3), 2);
    TestEqual(TEXT("Sorcerer 3: two 2nd"), FSpellRules::SlotsFor(EAdventurerRole::Sorcerer, 3, 2), 2);
    TestEqual(TEXT("Druid 2: three 1st"), FSpellRules::SlotsFor(EAdventurerRole::Druid, 2, 1), 3);

    // Half casters L1-5
    TestEqual(TEXT("Paladin 1: no slots"), FSpellRules::SlotsFor(EAdventurerRole::Paladin, 1, 1), 0);
    TestEqual(TEXT("Paladin 2: two 1st"), FSpellRules::SlotsFor(EAdventurerRole::Paladin, 2, 1), 2);
    TestEqual(TEXT("Ranger 5: four 1st"), FSpellRules::SlotsFor(EAdventurerRole::Ranger, 5, 1), 4);
    TestEqual(TEXT("Ranger 5: two 2nd"), FSpellRules::SlotsFor(EAdventurerRole::Ranger, 5, 2), 2);
    TestEqual(TEXT("Ranger 5: no 3rd"), FSpellRules::SlotsFor(EAdventurerRole::Ranger, 5, 3), 0);
    TestEqual(TEXT("Artificer 1: two 1st"), FSpellRules::SlotsFor(EAdventurerRole::Artificer, 1, 1), 2);
    TestEqual(TEXT("Artificer 5: four 1st two 2nd"), FSpellRules::SlotsFor(EAdventurerRole::Artificer, 5, 1), 4);
    TestEqual(TEXT("Artificer 5: two 2nd"), FSpellRules::SlotsFor(EAdventurerRole::Artificer, 5, 2), 2);

    // Warlock pact L1-5 (slots only at pact level)
    TestEqual(TEXT("Warlock 1: one 1st"), FSpellRules::SlotsFor(EAdventurerRole::Warlock, 1, 1), 1);
    TestEqual(TEXT("Warlock 2: two 1st"), FSpellRules::SlotsFor(EAdventurerRole::Warlock, 2, 1), 2);
    TestEqual(TEXT("Warlock 3: two 2nd"), FSpellRules::SlotsFor(EAdventurerRole::Warlock, 3, 2), 2);
    TestEqual(TEXT("Warlock 3: no 1st"), FSpellRules::SlotsFor(EAdventurerRole::Warlock, 3, 1), 0);
    TestEqual(TEXT("Warlock 5: two 3rd"), FSpellRules::SlotsFor(EAdventurerRole::Warlock, 5, 3), 2);
    TestEqual(TEXT("Warlock 5: no 2nd"), FSpellRules::SlotsFor(EAdventurerRole::Warlock, 5, 2), 0);
    TestEqual(TEXT("Warlock max slot L5"), FSpellRules::MaxSlotLevel(EAdventurerRole::Warlock, 5), 3);
    TestEqual(TEXT("Warlock max slot L3"), FSpellRules::MaxSlotLevel(EAdventurerRole::Warlock, 3), 2);

    TestEqual(TEXT("Fighter: no slots"), FSpellRules::SlotsFor(EAdventurerRole::Fighter, 5, 1), 0);
    TestEqual(TEXT("Max slot level, Cleric 3"), FSpellRules::MaxSlotLevel(EAdventurerRole::Cleric, 3), 2);
    TestEqual(TEXT("Max slot level, Paladin 5"), FSpellRules::MaxSlotLevel(EAdventurerRole::Paladin, 5), 2);
    TestTrue(TEXT("Class lists"), FSpellRules::OnClassList(EAdventurerRole::Cleric, ESpell::CureWounds) && !FSpellRules::OnClassList(EAdventurerRole::Wizard, ESpell::CureWounds) && FSpellRules::OnClassList(EAdventurerRole::Wizard, ESpell::MagicMissile));
    TestTrue(TEXT("Warlock Misty Step / Hold Person"), FSpellRules::OnClassList(EAdventurerRole::Warlock, ESpell::MistyStep) && FSpellRules::OnClassList(EAdventurerRole::Warlock, ESpell::HoldPerson) && !FSpellRules::OnClassList(EAdventurerRole::Warlock, ESpell::FireBolt));
    TestTrue(TEXT("Paladin Bless / Shield of Faith"), FSpellRules::OnClassList(EAdventurerRole::Paladin, ESpell::Bless) && FSpellRules::OnClassList(EAdventurerRole::Paladin, ESpell::ShieldOfFaith) && !FSpellRules::OnClassList(EAdventurerRole::Paladin, ESpell::MagicMissile));
    TestEqual(TEXT("Magic Missile at 2nd: 4 darts"), FSpellRules::UpcastDiceCount(ESpell::MagicMissile, 2), 4);
    TestEqual(TEXT("Cure Wounds at 2nd: 4d8"), FSpellRules::UpcastDiceCount(ESpell::CureWounds, 2), 4);
    TestEqual(TEXT("Healing Word at 2nd: 4d4"), FSpellRules::UpcastDiceCount(ESpell::HealingWord, 2), 4);
    TestEqual(TEXT("Guiding Bolt at 1st: 4d6"), FSpellRules::UpcastDiceCount(ESpell::GuidingBolt, 1), 4);
    TestEqual(TEXT("Slot too low"), FSpellRules::UpcastDiceCount(ESpell::MagicMissile, 0), 0);

    const FSpellFacts& Shield = FSpellRules::Facts(ESpell::Shield);
    TestTrue(TEXT("Shield is Wizard abjuration reaction"), FSpellRules::OnClassList(EAdventurerRole::Wizard, ESpell::Shield) && !FSpellRules::OnClassList(EAdventurerRole::Cleric, ESpell::Shield));
    TestEqual(TEXT("Shield school"), (int32)Shield.School, (int32)ESpellSchool::Abjuration);
    TestFalse(TEXT("Shield is not concentration (SRD 5.2.1)"), Shield.bConcentration);
    TestEqual(TEXT("Shield duration 1 round"), FString(Shield.Duration), FString(TEXT("1 round")));
    TestEqual(TEXT("Shield casting time reaction"), FString(Shield.CastingTime), FString(TEXT("1 reaction")));
    TestEqual(TEXT("Shield +5 AC"), Shield.ArmorClassBonus, 5);
    TestTrue(TEXT("Shield negates Magic Missile"), Shield.bNegatesMagicMissile);

    const FSpellFacts& Faith = FSpellRules::Facts(ESpell::ShieldOfFaith);
    TestTrue(TEXT("Shield of Faith Cleric abjuration concentration"), Faith.bCleric && !Faith.bWizard && Faith.School == ESpellSchool::Abjuration && Faith.bConcentration && Faith.bMaterial);
    TestTrue(TEXT("Shield of Faith also Paladin"), Faith.bPaladin);
    TestEqual(TEXT("Shield of Faith +2 AC"), Faith.ArmorClassBonus, 2);
    TestEqual(TEXT("Shield of Faith duration"), FString(Faith.Duration), FString(TEXT("Concentration, up to 10 minutes")));
    TestFalse(TEXT("Shield of Faith does not negate MM"), Faith.bNegatesMagicMissile);

    const FSpellFacts& Guidance = FSpellRules::Facts(ESpell::Guidance);
    TestTrue(TEXT("Guidance: Cleric Divination cantrip concentration"), Guidance.Level == 0 && Guidance.School == ESpellSchool::Divination && Guidance.bCleric && !Guidance.bWizard && Guidance.bConcentration && Guidance.DieCount == 1 && Guidance.DieSides == 4 && Guidance.RangeFeet == 0);
    TestTrue(TEXT("Guidance also Druid/Artificer"), Guidance.bDruid && Guidance.bArtificer);
    const FSpellFacts& Bless = FSpellRules::Facts(ESpell::Bless);
    TestTrue(TEXT("Bless: Cleric Enchantment concentration 1 minute"), Bless.Level == 1 && Bless.School == ESpellSchool::Enchantment && Bless.bCleric && Bless.bConcentration && Bless.bMaterial && Bless.RangeFeet == 30 && FCString::Strcmp(Bless.Duration, TEXT("Concentration, up to 1 minute")) == 0);
    const FSpellFacts& MageArmor = FSpellRules::Facts(ESpell::MageArmor);
    TestTrue(TEXT("Mage Armor: Wizard Abjuration 8 hours, not concentration"), MageArmor.Level == 1 && MageArmor.School == ESpellSchool::Abjuration && MageArmor.bWizard && !MageArmor.bCleric && !MageArmor.bConcentration && MageArmor.bMaterial && FCString::Strcmp(MageArmor.Duration, TEXT("8 hours")) == 0);

    // Lean #116/#117 finish â€” SRD 5.2.1 verified rows.
    const FSpellFacts& Ray = FSpellRules::Facts(ESpell::RayOfFrost);
    TestTrue(TEXT("Ray of Frost: Wizard cold cantrip 1d8"), Ray.Level == 0 && Ray.bWizard && Ray.DamageType == EDamageType::Cold && Ray.DieCount == 1 && Ray.DieSides == 8 && !Ray.bConcentration);
    TestTrue(TEXT("Shocking Grasp: Wizard lightning touch cantrip"), FSpellRules::Facts(ESpell::ShockingGrasp).DamageType == EDamageType::Lightning && FSpellRules::Facts(ESpell::ShockingGrasp).RangeFeet == 0);
    const FSpellFacts& Light = FSpellRules::Facts(ESpell::Light);
    TestTrue(TEXT("Light: Cleric and Wizard, V M, 1 hour"), Light.bCleric && Light.bWizard && Light.bVerbal && !Light.bSomatic && Light.bMaterial && !Light.bConcentration);
    TestTrue(TEXT("Light also Bard/Sorcerer/Artificer"), Light.bBard && Light.bSorcerer && Light.bArtificer);
    const FSpellFacts& Inflict = FSpellRules::Facts(ESpell::InflictWounds);
    TestTrue(TEXT("Inflict Wounds 5.2.1: 2d10 Necrotic (not 3d10 attack)"), Inflict.School == ESpellSchool::Necromancy && Inflict.DieCount == 2 && Inflict.DieSides == 10 && Inflict.DamageType == EDamageType::Necrotic && Inflict.bCleric);
    TestTrue(TEXT("Bane concentrates"), FSpellRules::Facts(ESpell::Bane).bConcentration && FSpellRules::Facts(ESpell::Bane).bCleric);
    TestTrue(TEXT("Bane also Bard"), FSpellRules::Facts(ESpell::Bane).bBard);
    TestFalse(TEXT("Sanctuary does not concentrate"), FSpellRules::Facts(ESpell::Sanctuary).bConcentration);
    TestEqual(TEXT("Feather Fall reaction"), FString(FSpellRules::Facts(ESpell::FeatherFall).CastingTime), FString(TEXT("1 reaction")));
    const FSpellFacts& Detect = FSpellRules::Facts(ESpell::DetectMagic);
    TestTrue(TEXT("Detect Magic: ritual concentration both classes"), Detect.bRitual && Detect.bConcentration && Detect.bCleric && Detect.bWizard);
    TestTrue(TEXT("Detect Magic nearly universal"), Detect.bBard && Detect.bDruid && Detect.bPaladin && Detect.bRanger && Detect.bSorcerer && Detect.bArtificer && !Detect.bWarlock);
    const FSpellFacts& Sleep = FSpellRules::Facts(ESpell::Sleep);
    TestTrue(TEXT("Sleep 5.2.1: concentration Wis sphere 60 ft"), Sleep.bConcentration && Sleep.RangeFeet == 60 && Sleep.bWizard && !Sleep.bCleric && Sleep.bMaterial);
    TestTrue(TEXT("Sleep also Bard/Sorcerer"), Sleep.bBard && Sleep.bSorcerer);
    TestEqual(TEXT("Misty Step bonus action"), FString(FSpellRules::Facts(ESpell::MistyStep).CastingTime), FString(TEXT("1 bonus action")));
    TestTrue(TEXT("Misty Step Sorcerer/Warlock/Wizard"), FSpellRules::Facts(ESpell::MistyStep).bSorcerer && FSpellRules::Facts(ESpell::MistyStep).bWarlock && FSpellRules::Facts(ESpell::MistyStep).bWizard);
    const FSpellFacts& Spirit = FSpellRules::Facts(ESpell::SpiritualWeapon);
    TestTrue(TEXT("Spiritual Weapon 5.2.1: concentration (changed from 5.1)"), Spirit.bConcentration && Spirit.Level == 2 && Spirit.bCleric && Spirit.DieCount == 1 && Spirit.DieSides == 8 && Spirit.DamageType == EDamageType::Force);
    TestEqual(TEXT("Scorching Ray base 6d6 total"), FSpellRules::UpcastDiceCount(ESpell::ScorchingRay, 2), 6);
    TestEqual(TEXT("Scorching Ray at 3rd: 8d6"), FSpellRules::UpcastDiceCount(ESpell::ScorchingRay, 3), 8);
    TestTrue(TEXT("Hold Person both classes concentration"), FSpellRules::Facts(ESpell::HoldPerson).bCleric && FSpellRules::Facts(ESpell::HoldPerson).bWizard && FSpellRules::Facts(ESpell::HoldPerson).bConcentration);
    TestTrue(TEXT("Hold Person full list"), FSpellRules::Facts(ESpell::HoldPerson).bBard && FSpellRules::Facts(ESpell::HoldPerson).bDruid && FSpellRules::Facts(ESpell::HoldPerson).bSorcerer && FSpellRules::Facts(ESpell::HoldPerson).bWarlock);
    TestEqual(TEXT("Aid +5 HP flat"), FSpellRules::Facts(ESpell::Aid).FlatBonus, 5);
    TestFalse(TEXT("Aid is not concentration"), FSpellRules::Facts(ESpell::Aid).bConcentration);
    TestTrue(TEXT("Aid Cleric/Paladin/Artificer"), FSpellRules::Facts(ESpell::Aid).bCleric && FSpellRules::Facts(ESpell::Aid).bPaladin && FSpellRules::Facts(ESpell::Aid).bArtificer && !FSpellRules::Facts(ESpell::Aid).bBard);

    for (int32 SpellIndex = 0; SpellIndex < (int32)ESpell::Count; ++SpellIndex)
    {
        const FSpellFacts& F = FSpellRules::Facts((ESpell)SpellIndex);
        const bool bAnyClass = F.bBard || F.bCleric || F.bDruid || F.bPaladin || F.bRanger || F.bSorcerer || F.bWarlock || F.bWizard || F.bArtificer;
        TestTrue(TEXT("Spell has a class tag"), bAnyClass);
        TestTrue(TEXT("Spell school in range"), (int32)F.School >= 0 && (int32)F.School <= 7);
    }
    TestTrue(TEXT("SpellsOnList Wizard cantrips nonempty"), FSpellRules::SpellsOnList(EAdventurerRole::Wizard, 0).Num() > 0);
    TestTrue(TEXT("SpellsOnList Warlock L2 includes Misty Step"), FSpellRules::SpellsOnList(EAdventurerRole::Warlock, 2).Contains(ESpell::MistyStep));
    return !HasAnyErrors();
}
#endif
