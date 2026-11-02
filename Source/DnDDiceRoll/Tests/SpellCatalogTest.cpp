#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Combat/CombatBattle.h"
#include "Rules/ConcentrationRules.h"
#include "Rules/SpellCatalogRules.h"
#include "Rules/SpellRules.h"

namespace SpellCatalogTest
{
    const FSrdSpell& Get(FAutomationTestBase& Test, const TCHAR* Name)
    {
        const FSrdSpell* S = FSpellCatalogRules::Find(Name);
        Test.TestNotNull(FString::Printf(TEXT("%s is in the SRD catalog"), Name), S);
        return S ? *S : FSpellCatalogRules::At(0);
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSpellCatalogShapeTest, "Project.Expedition.Rules.SpellCatalog.Shape",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSpellCatalogShapeTest::RunTest(const FString& Parameters)
{
    // SRD 5.2.1 player spell list: 339 spells; per-level counts from the SRD 5.2.1 spell descriptions.
    TestEqual(TEXT("339 SRD 5.2.1 spells"), FSpellCatalogRules::Num(), 339);
    const int32 Expected[10] = { 27, 57, 57, 42, 34, 38, 31, 20, 17, 16 };
    int32 ByLevel[10] = {};
    TSet<FString> Ids;
    for (int32 I = 0; I < FSpellCatalogRules::Num(); ++I)
    {
        const FSrdSpell& S = FSpellCatalogRules::At(I);
        ++ByLevel[FMath::Clamp(S.Level, 0, 9)];
        TestTrue(FString::Printf(TEXT("%s has at least one class"), S.Name), S.Classes != 0 && (S.Classes & ~0xFF) == 0);
        TestTrue(FString::Printf(TEXT("%s has an SRD school"), S.Name), (int32)S.School <= (int32)ESpellSchool::Transmutation);
        TestFalse(FString::Printf(TEXT("%s id unique"), S.Name), Ids.Contains(S.Id)); Ids.Add(S.Id);
        if (S.Effect == ESpellEffect::Damage || S.Effect == ESpellEffect::Healing)
        { TestTrue(FString::Printf(TEXT("%s has dice at its own level"), S.Name), FSpellCatalogRules::DiceCount(S, 1, FMath::Max(1, S.Level)) > 0 || S.DieSides == 0); }
        if (S.Resolution == ESpellResolution::Save) { TestTrue(FString::Printf(TEXT("%s save spell"), S.Name), S.Effect != ESpellEffect::Utility || S.Implementation == ESpellImplementation::DataOnly); }
        TestEqual(FString::Printf(TEXT("%s data-only iff utility"), S.Name), S.Implementation == ESpellImplementation::DataOnly, S.Effect == ESpellEffect::Utility);
    }
    for (int32 L = 0; L < 10; ++L) { TestEqual(FString::Printf(TEXT("Spell level %d count"), L), ByLevel[L], Expected[L]); }
    // Per-class list sizes in the SRD 5.2.1 class spell lists.
    const TPair<EAdventurerRole, int32> Sizes[] = { {EAdventurerRole::Bard, 130}, {EAdventurerRole::Cleric, 109}, {EAdventurerRole::Druid, 124}, {EAdventurerRole::Paladin, 38},
        {EAdventurerRole::Ranger, 48}, {EAdventurerRole::Sorcerer, 140}, {EAdventurerRole::Warlock, 72}, {EAdventurerRole::Wizard, 218} };
    for (const auto& [Role, Size] : Sizes)
    {
        int32 N = 0; for (int32 I = 0; I < FSpellCatalogRules::Num(); ++I) { N += FSpellCatalogRules::OnClassList(Role, FSpellCatalogRules::At(I)) ? 1 : 0; }
        TestEqual(TEXT("Class list size"), N, Size);
    }
    for (EAdventurerRole Role : { EAdventurerRole::Fighter, EAdventurerRole::Rogue, EAdventurerRole::Barbarian, EAdventurerRole::Monk })
    { TestEqual(TEXT("Non-casters have no class list"), FSpellCatalogRules::ClassBit(Role), (uint16)0); }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSpellCatalogClassListTest, "Project.Expedition.Rules.SpellCatalog.ClassLists",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSpellCatalogClassListTest::RunTest(const FString& Parameters)
{
    using namespace SpellCatalogTest;
    struct FRow { const TCHAR* Name; uint16 Classes; ESpellSchool School; int32 Level; };
    // Hand-checked against the SRD 5.2.1 spell descriptions (class tags per spell).
    const FRow Rows[] = {
        { TEXT("Fire Bolt"), SpellClass::Sorcerer | SpellClass::Wizard, ESpellSchool::Evocation, 0 },
        { TEXT("Eldritch Blast"), SpellClass::Warlock, ESpellSchool::Evocation, 0 },
        { TEXT("Sacred Flame"), SpellClass::Cleric, ESpellSchool::Evocation, 0 },
        { TEXT("Vicious Mockery"), SpellClass::Bard, ESpellSchool::Enchantment, 0 },
        { TEXT("Cure Wounds"), SpellClass::Bard | SpellClass::Cleric | SpellClass::Druid | SpellClass::Paladin | SpellClass::Ranger, ESpellSchool::Abjuration, 1 },
        { TEXT("Magic Missile"), SpellClass::Sorcerer | SpellClass::Wizard, ESpellSchool::Evocation, 1 },
        { TEXT("Bless"), SpellClass::Cleric | SpellClass::Paladin, ESpellSchool::Enchantment, 1 },
        { TEXT("Hunter's Mark"), SpellClass::Ranger, ESpellSchool::Divination, 1 },
        { TEXT("Hex"), SpellClass::Warlock, ESpellSchool::Enchantment, 1 },
        { TEXT("Shield"), SpellClass::Sorcerer | SpellClass::Wizard, ESpellSchool::Abjuration, 1 },
        { TEXT("Spiritual Weapon"), SpellClass::Cleric, ESpellSchool::Evocation, 2 },
        { TEXT("Misty Step"), SpellClass::Sorcerer | SpellClass::Warlock | SpellClass::Wizard, ESpellSchool::Conjuration, 2 },
        { TEXT("Fireball"), SpellClass::Sorcerer | SpellClass::Wizard, ESpellSchool::Evocation, 3 },
        { TEXT("Revivify"), SpellClass::Cleric | SpellClass::Druid | SpellClass::Paladin | SpellClass::Ranger, ESpellSchool::Necromancy, 3 },
        { TEXT("Counterspell"), SpellClass::Sorcerer | SpellClass::Warlock | SpellClass::Wizard, ESpellSchool::Abjuration, 3 },
        { TEXT("Wish"), SpellClass::Sorcerer | SpellClass::Wizard, ESpellSchool::Conjuration, 9 },
    };
    for (const FRow& R : Rows)
    {
        const FSrdSpell& S = Get(*this, R.Name);
        TestEqual(FString::Printf(TEXT("%s classes"), R.Name), S.Classes, R.Classes);
        TestEqual(FString::Printf(TEXT("%s school"), R.Name), (int32)S.School, (int32)R.School);
        TestEqual(FString::Printf(TEXT("%s level"), R.Name), S.Level, R.Level);
    }
    // Casting-time tags map onto the action economy.
    TestEqual(TEXT("Healing Word is a Bonus Action"), (int32)FSpellCatalogRules::ActionCost(Get(*this, TEXT("Healing Word"))), (int32)EActionCost::BonusAction);
    TestEqual(TEXT("Shield is a Reaction"), (int32)FSpellCatalogRules::ActionCost(Get(*this, TEXT("Shield"))), (int32)EActionCost::Reaction);
    TestFalse(TEXT("Identify (1 minute) is not castable in a turn"), FSpellCatalogRules::CastableInCombat(Get(*this, TEXT("Identify"))));
    TestTrue(TEXT("Detect Magic is a ritual"), Get(*this, TEXT("Detect Magic")).bRitual);
    TestTrue(TEXT("SRD 5.2.1 spell entries carry no subclass lists"), FString(Get(*this, TEXT("Spiritual Weapon")).Subclasses).IsEmpty());
    // The eight combat-wired spells agree with the catalog.
    for (int32 I = 0; I < (int32)ESpell::Count; ++I)
    {
        const FSpellFacts& F = FSpellRules::Facts((ESpell)I);
        const FSrdSpell& S = Get(*this, F.Name);
        TestTrue(FString::Printf(TEXT("%s level/school match"), F.Name), S.Level == F.Level && S.School == F.School);
        TestEqual(FString::Printf(TEXT("%s tags list its classes"), F.Name), FSpellRules::Summary((ESpell)I).Contains(FSpellCatalogRules::ClassListText(S.Classes)), true);
    }
    TestTrue(TEXT("Sorcerer can use Fire Bolt"), FSpellRules::OnClassList(EAdventurerRole::Sorcerer, ESpell::FireBolt));
    TestTrue(TEXT("Paladin has Cure Wounds"), FSpellRules::OnClassList(EAdventurerRole::Paladin, ESpell::CureWounds));
    TestFalse(TEXT("Cleric lacks Magic Missile"), FSpellRules::OnClassList(EAdventurerRole::Cleric, ESpell::MagicMissile));
    TestEqual(TEXT("Tag line"), FSpellCatalogRules::Tags(Get(*this, TEXT("Fireball"))), FString(TEXT("3rd-level evocation | Sorcerer, Wizard | Action | 150 feet | V, S, M | Instantaneous")));
    TestTrue(TEXT("Concentration tag"), FSpellCatalogRules::Tags(Get(*this, TEXT("Bless"))).EndsWith(TEXT(" | Concentration")));
    TestTrue(TEXT("Unimplemented marker"), FSpellCatalogRules::Tags(Get(*this, TEXT("Identify"))).Contains(TEXT("Effect not yet modelled")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSpellCatalogGatingTest, "Project.Expedition.Rules.SpellCatalog.LevelGating",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSpellCatalogGatingTest::RunTest(const FString& Parameters)
{
    using namespace SpellCatalogTest;
    const FSrdSpell& MagicMissile = Get(*this, TEXT("Magic Missile"));
    const FSrdSpell& Fireball = Get(*this, TEXT("Fireball"));
    const FSrdSpell& Bless = Get(*this, TEXT("Bless"));
    const FSrdSpell& Aid = Get(*this, TEXT("Aid"));
    const FSrdSpell& Revivify = Get(*this, TEXT("Revivify"));
    TestTrue(TEXT("Wizard 1 learns Magic Missile"), FSpellCatalogRules::CanLearn(EAdventurerRole::Wizard, 1, MagicMissile));
    TestFalse(TEXT("Wizard 4 cannot learn Fireball"), FSpellCatalogRules::CanLearn(EAdventurerRole::Wizard, 4, Fireball));
    TestTrue(TEXT("Wizard 5 learns Fireball"), FSpellCatalogRules::CanLearn(EAdventurerRole::Wizard, 5, Fireball));
    TestFalse(TEXT("Cleric never learns Fireball"), FSpellCatalogRules::CanLearn(EAdventurerRole::Cleric, 20, Fireball));
    TestFalse(TEXT("Paladin 1 has no Spellcasting"), FSpellCatalogRules::CanLearn(EAdventurerRole::Paladin, 1, Bless));
    TestTrue(TEXT("Paladin 2 prepares Bless"), FSpellCatalogRules::CanLearn(EAdventurerRole::Paladin, 2, Bless));
    TestTrue(TEXT("Paladin 5 prepares Aid"), FSpellCatalogRules::CanLearn(EAdventurerRole::Paladin, 5, Aid));
    TestFalse(TEXT("Paladin 5 cannot prepare Revivify"), FSpellCatalogRules::CanLearn(EAdventurerRole::Paladin, 5, Revivify));
    TestTrue(TEXT("Cleric 5 prepares Revivify"), FSpellCatalogRules::CanLearn(EAdventurerRole::Cleric, 5, Revivify));
    TestFalse(TEXT("Paladins learn no cantrips"), FSpellCatalogRules::CanLearn(EAdventurerRole::Paladin, 5, Get(*this, TEXT("Sacred Flame"))));
    TestFalse(TEXT("Fighters cast nothing"), FSpellCatalogRules::CanLearn(EAdventurerRole::Fighter, 5, MagicMissile));
    // Multiclass: each spell through one class at that class's own level.
    TestFalse(TEXT("Wizard 3 / Fighter 2 cannot learn Fireball"), FSpellCatalogRules::HeroCanLearn(EAdventurerRole::Wizard, 3, EAdventurerRole::Fighter, 2, Fireball));
    TestFalse(TEXT("Cleric 3 / Wizard 2 cannot learn 2nd-level wizard spells"), FSpellCatalogRules::HeroCanLearn(EAdventurerRole::Cleric, 3, EAdventurerRole::Wizard, 2, Get(*this, TEXT("Misty Step"))));
    TestTrue(TEXT("Cleric 3 / Wizard 2 learns Magic Missile"), FSpellCatalogRules::HeroCanLearn(EAdventurerRole::Cleric, 3, EAdventurerRole::Wizard, 2, MagicMissile));
    // Learnable level matches the slot tables at levels 1-5.
    for (EAdventurerRole Role : { EAdventurerRole::Bard, EAdventurerRole::Cleric, EAdventurerRole::Druid, EAdventurerRole::Paladin, EAdventurerRole::Ranger, EAdventurerRole::Sorcerer, EAdventurerRole::Warlock, EAdventurerRole::Wizard })
    {
        for (int32 L = 1; L <= 5; ++L) { TestEqual(TEXT("Learnable level = slot table"), FSpellCatalogRules::MaxLearnableLevel(Role, L), FSpellRules::MaxSlotLevel(Role, L)); }
    }
    TestEqual(TEXT("Full caster 17 reaches 9th"), FSpellCatalogRules::MaxLearnableLevel(EAdventurerRole::Wizard, 17), 9);
    TestEqual(TEXT("Half caster 17 reaches 5th"), FSpellCatalogRules::MaxLearnableLevel(EAdventurerRole::Ranger, 17), 5);
    // Known / prepared / cantrips (SRD class tables).
    TestEqual(TEXT("Bard 5 knows 8"), FSpellCatalogRules::SpellsKnownOrPrepared(EAdventurerRole::Bard, 5, 3), 8);
    TestEqual(TEXT("Sorcerer 3 knows 4"), FSpellCatalogRules::SpellsKnownOrPrepared(EAdventurerRole::Sorcerer, 3, 3), 4);
    TestEqual(TEXT("Warlock 5 knows 6"), FSpellCatalogRules::SpellsKnownOrPrepared(EAdventurerRole::Warlock, 5, 3), 6);
    TestEqual(TEXT("Ranger 1 knows 0"), FSpellCatalogRules::SpellsKnownOrPrepared(EAdventurerRole::Ranger, 1, 3), 0);
    TestEqual(TEXT("Wizard 5 INT +3 prepares 8"), FSpellCatalogRules::SpellsKnownOrPrepared(EAdventurerRole::Wizard, 5, 3), 8);
    TestEqual(TEXT("Paladin 5 CHA +2 prepares 4"), FSpellCatalogRules::SpellsKnownOrPrepared(EAdventurerRole::Paladin, 5, 2), 4);
    TestEqual(TEXT("Prepared minimum 1"), FSpellCatalogRules::SpellsKnownOrPrepared(EAdventurerRole::Cleric, 1, -3), 1);
    TestEqual(TEXT("Sorcerer 4 cantrips 5"), FSpellCatalogRules::CantripsKnown(EAdventurerRole::Sorcerer, 4), 5);
    TestEqual(TEXT("Cleric 1 cantrips 3"), FSpellCatalogRules::CantripsKnown(EAdventurerRole::Cleric, 1), 3);
    TestEqual(TEXT("Ranger cantrips 0"), FSpellCatalogRules::CantripsKnown(EAdventurerRole::Ranger, 5), 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSpellCatalogMathTest, "Project.Expedition.Rules.SpellCatalog.DamageAndScaling",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSpellCatalogMathTest::RunTest(const FString& Parameters)
{
    using namespace SpellCatalogTest;
    const FSrdSpell& FireBolt = Get(*this, TEXT("Fire Bolt"));
    const int32 Tiers[4][2] = { {1, 1}, {5, 2}, {11, 3}, {17, 4} };
    for (const auto& T : Tiers) { TestEqual(TEXT("Fire Bolt cantrip tiers"), FSpellCatalogRules::DiceCount(FireBolt, T[0], 0), T[1]); }
    TestEqual(TEXT("Fire Bolt level 4 still 1 die"), FSpellCatalogRules::DiceCount(FireBolt, 4, 0), 1);
    TestTrue(TEXT("Fire Bolt d10 fire ranged attack"), FireBolt.DieSides == 10 && FireBolt.DamageType == EDamageType::Fire && FireBolt.Resolution == ESpellResolution::RangedAttack);
    const FSrdSpell& Fireball = Get(*this, TEXT("Fireball"));
    TestEqual(TEXT("Fireball 8d6 at 3rd"), FSpellCatalogRules::DiceCount(Fireball, 5, 3), 8);
    TestEqual(TEXT("Fireball 9d6 at 4th"), FSpellCatalogRules::DiceCount(Fireball, 7, 4), 9);
    TestEqual(TEXT("Fireball needs a 3rd-level slot"), FSpellCatalogRules::DiceCount(Fireball, 5, 2), 0);
    TestEqual(TEXT("Fireball average 28"), FSpellCatalogRules::AverageEffect(Fireball, 5, 3, 0), 28);
    TestTrue(TEXT("Fireball DEX half, 20-ft sphere"), Fireball.Resolution == ESpellResolution::Save && Fireball.SaveAbility == EAbility::Dexterity && Fireball.bHalfOnSave && Fireball.Area == ESpellArea::Sphere && Fireball.AreaFeet == 20);
    const FSrdSpell& Cure = Get(*this, TEXT("Cure Wounds"));
    TestTrue(TEXT("Cure Wounds heals 2d8 + modifier (SRD 5.2.1)"), Cure.Effect == ESpellEffect::Healing && Cure.DieSides == 8 && Cure.bAddModifier && FSpellCatalogRules::DiceCount(Cure, 1, 1) == 2);
    TestEqual(TEXT("Cure Wounds 3rd-level slot: 6d8"), FSpellCatalogRules::DiceCount(Cure, 5, 3), 6);
    TestEqual(TEXT("Cure Wounds avg at 1st, +3"), FSpellCatalogRules::AverageEffect(Cure, 1, 1, 3), 12);
    const FSrdSpell& Weapon = Get(*this, TEXT("Spiritual Weapon"));
    TestEqual(TEXT("Spiritual Weapon 2d8 at 3rd"), FSpellCatalogRules::DiceCount(Weapon, 5, 3), 2);
    TestEqual(TEXT("Spiritual Weapon 3d8 at 4th"), FSpellCatalogRules::DiceCount(Weapon, 7, 4), 3);
    const FSrdSpell& Missile = Get(*this, TEXT("Magic Missile"));
    TestEqual(TEXT("Magic Missile darts by slot"), FSpellCatalogRules::DiceCount(Missile, 3, 2), FSpellRules::UpcastDiceCount(ESpell::MagicMissile, 2));
    TestTrue(TEXT("Hold Person paralyzes on a failed WIS save"), FSpellCatalogRules::HasCondition(Get(*this, TEXT("Hold Person")), ESpellCondition::Paralyzed) && Get(*this, TEXT("Hold Person")).SaveAbility == EAbility::Wisdom);
    TestEqual(TEXT("Shield +5 AC"), Get(*this, TEXT("Shield")).ModifierValue, 5);
    TestTrue(TEXT("Aid raises max HP by 5"), Get(*this, TEXT("Aid")).Modifier == ESpellModifier::MaxHp && Get(*this, TEXT("Aid")).ModifierValue == 5);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSpellCatalogConcentrationTest, "Project.Expedition.Rules.SpellCatalog.Concentration",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSpellCatalogConcentrationTest::RunTest(const FString& Parameters)
{
    using namespace SpellCatalogTest;
    for (const TCHAR* Name : { TEXT("Bless"), TEXT("Hold Person"), TEXT("Hunter's Mark"), TEXT("Hex"), TEXT("Spirit Guardians"), TEXT("Haste"), TEXT("Fly"), TEXT("Spiritual Weapon") })
    { TestTrue(FString::Printf(TEXT("%s needs concentration"), Name), FConcentrationRules::RequiresConcentration(Get(*this, Name))); }
    for (const TCHAR* Name : { TEXT("Fireball"), TEXT("Magic Missile"), TEXT("Shield"), TEXT("Cure Wounds") })
    { TestFalse(FString::Printf(TEXT("%s does not"), Name), FConcentrationRules::RequiresConcentration(Get(*this, Name))); }
    for (int32 I = 0; I < (int32)ESpell::Count; ++I) { TestFalse(TEXT("Combat-wired spells need no concentration"), FConcentrationRules::RequiresConcentration((ESpell)I)); }
    TestTrue(TEXT("Casting Hold Person ends Bless"), FConcentrationRules::NewSpellEndsCurrent(true, Get(*this, TEXT("Hold Person")).bConcentration));
    TestFalse(TEXT("Casting Fireball keeps Bless"), FConcentrationRules::NewSpellEndsCurrent(true, Get(*this, TEXT("Fireball")).bConcentration));
    return true;
}
#endif
