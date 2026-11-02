#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Combat/CombatBattle.h"
#include "Party/BuildCatalog.h"
#include "Party/FeatureKit.h"
#include "Rules/CantripRules.h"
#include "Rules/ClassFeatureRules.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FActionEconomyTagTest, "Project.Expedition.Rules.ActionEconomyTags",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FActionEconomyTagTest::RunTest(const FString& Parameters)
{
    FCombatantState Rogue; FBuildCatalog::Create(TEXT("rogue.thief"), 1, TEXT("R"), Rogue); Rogue.Level = 2;
    // Every described command's cost field agrees with its data tag.
    for (uint8 Raw = 0; Raw <= static_cast<uint8>(FFeatureKit::LastCommand); ++Raw)
    {
        const auto Action = static_cast<ECombatCommandType>(Raw);
        if (Action == ECombatCommandType::Move || Action == ECombatCommandType::EndTurn || Action == ECombatCommandType::Attack ||
            Action == ECombatCommandType::Special1 || Action == ECombatCommandType::Special2) { continue; }
        const FString Field = FCombatBattle::CostTag(Action);
        const EActionCost Cost = FCombatBattle::ActionCost(Action, Rogue);
        AddInfo(Field);
        TestTrue(TEXT("Description cost matches data tag"),
            (Cost == EActionCost::Action && Field.StartsWith(TEXT("Action"))) || (Cost == EActionCost::BonusAction && Field.StartsWith(TEXT("Bonus"))) ||
            (Cost == EActionCost::Free && Field.StartsWith(TEXT("Free"))) || (Cost == EActionCost::Reaction && Field.StartsWith(TEXT("Reaction"))));
    }
    TestTrue(TEXT("Movement is free"), FCombatBattle::ActionCost(ECombatCommandType::Move, Rogue) == EActionCost::Free);
    TestTrue(TEXT("Second Wind is a bonus action"), FCombatBattle::ActionCost(ECombatCommandType::SecondWind, Rogue) == EActionCost::BonusAction);
    TestTrue(TEXT("Cunning Action: Dash after the action is a bonus action"), FCombatBattle::ActionCost(ECombatCommandType::Dash, Rogue, false) == EActionCost::BonusAction);
    TestEqual(TEXT("Tooltip prefix"), FCombatBattle::CostPrefix(ECombatCommandType::OffHandAttack, Rogue), FString(TEXT("[Bonus Action] ")));
    TestEqual(TEXT("Reaction label"), FString(FStandardActionRules::CostLabel(EActionCost::Reaction)), FString(TEXT("Reaction")));
    for (const auto* Id : {TEXT("barbarian.berserker"), TEXT("monk.open-hand"), TEXT("cleric.life")})
    {
        FCombatantState Hero; if (!FBuildCatalog::Create(Id, 1, TEXT("H"), Hero)) { continue; }
        for (const auto& A : FBuildCatalog::Abilities(Hero))
        { TestTrue(TEXT("Kit ability tag agrees with its budget"), FCombatBattle::ActionCost(A.Command, Hero) == A.Cost()); }
    }
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShieldReactionTest, "Project.Expedition.Rules.ShieldReaction",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FShieldReactionTest::RunTest(const FString& Parameters)
{
    FCombatantState Wiz; FBuildCatalog::Create(TEXT("wizard.evocation"), 1, TEXT("W"), Wiz);
    TestFalse(TEXT("Slots reserved by kit uses are not spent on Shield"), FCombatBattle::ShieldWouldBlock(Wiz, 15, 12, false) && Wiz.SpellSlots <= Wiz.KitUses);
    Wiz.KitUses = 0;
    TestTrue(TEXT("A hit within 5 of AC is shielded"), FCombatBattle::ShieldWouldBlock(Wiz, 15, 12, false));
    TestFalse(TEXT("A hit at AC + 5 still lands"), FCombatBattle::ShieldWouldBlock(Wiz, 17, 12, false));
    TestFalse(TEXT("Critical hits ignore Shield"), FCombatBattle::ShieldWouldBlock(Wiz, 13, 12, true));
    Wiz.bReactionAvailable = false; TestFalse(TEXT("Needs the reaction"), FCombatBattle::ShieldWouldBlock(Wiz, 13, 12, false));
    Wiz.bReactionAvailable = true; Wiz.SpellSlots = 0; TestFalse(TEXT("Needs a slot"), FCombatBattle::ShieldWouldBlock(Wiz, 13, 12, false));
    FCombatantState Fighter; FBuildCatalog::Create(TEXT("fighter.champion"), 2, TEXT("F"), Fighter);
    TestFalse(TEXT("Only wizards know Shield"), FCombatBattle::ShieldWouldBlock(Fighter, 13, 12, false));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSrdDiceDefaultsTest, "Project.Expedition.Rules.SrdDiceDefaults",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSrdDiceDefaultsTest::RunTest(const FString& Parameters)
{
    const auto* Fighter = FBuildCatalog::Find(TEXT("fighter.champion"));
    TestEqual(TEXT("Level adds no flat damage"), FBuildCatalog::BasicDamage(*Fighter, 5), FBuildCatalog::BasicDamage(*Fighter, 1));
    TestEqual(TEXT("Cure Wounds is 1d8"), BuildRules::CureDice, 1);
    TestEqual(TEXT("Magic Missile: three darts"), BuildRules::MissileDarts * 10 + BuildRules::MissileDie, 34);
    FCombatantState Hero; FBuildCatalog::Create(TEXT("cleric.life"), 1, TEXT("C"), Hero); Hero.Level = 4; Hero.SecondaryRole = EAdventurerRole::Fighter; Hero.SecondaryLevel = 1;
    TestEqual(TEXT("Second Wind scales with fighter levels only"), FBuildCatalog::ClassLevel(Hero, EAdventurerRole::Fighter), 1);
    TestEqual(TEXT("Primary class levels"), FBuildCatalog::ClassLevel(Hero, EAdventurerRole::Cleric), 3);
    TestEqual(TEXT("Fire Bolt 2d10 at level 5"), FCantripRules::DiceCount(ESpell::FireBolt, 5), 2);
    TestEqual(TEXT("Fire Bolt 1d10 at level 4"), FCantripRules::DiceCount(ESpell::FireBolt, 4), 1);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSuperiorityDefaultsTest, "Project.Expedition.Rules.SuperiorityDefaults",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSuperiorityDefaultsTest::RunTest(const FString& Parameters)
{
    const FCombatEncounter Defaults;
    TestTrue(TEXT("Superiority dice on by default"), Defaults.bSuperiorityDice);
    TestTrue(TEXT("Scaled Sneak Attack on by default"), Defaults.bScaledSneakAttack);
    TestFalse(TEXT("Death saves off by default"), Defaults.bDeathSaves);
    TestEqual(TEXT("Extra Attack at Fighter 5"), FClassFeatureRules::AttacksPerAction(EAdventurerRole::Fighter, 5), 2);
    TestEqual(TEXT("Sneak 3d6 at level 5"), FClassFeatureRules::SneakAttackDice(5), 3);
    FCombatantState Master = FBuildCatalog::Find(TEXT("fighter.battle-master"))->Profile;
    TestEqual(TEXT("Interim 2 dice below Fighter 3"), FBuildCatalog::ManeuverCap(Master), 2);
    Master.Level = 3;
    TestEqual(TEXT("SRD 4 superiority dice at Fighter 3"), FBuildCatalog::ManeuverCap(Master), 4);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FClassFeatureTableTest, "Project.Expedition.Rules.ClassFeaturesTagged",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FClassFeatureTableTest::RunTest(const FString& Parameters)
{
    TestEqual(TEXT("Fighter 1"), FClassFeatureRules::FeatureSummary(EAdventurerRole::Fighter, 1), FString(TEXT("Fighting Style [Free], Second Wind [Bonus Action]")));
    // Deliberate: SRD gap fill added Champion Improved Critical (3) and Ability Score Improvement (4); was 5.
    TestEqual(TEXT("Fighter 5 has seven"), FClassFeatureRules::FeaturesThrough(EAdventurerRole::Fighter, 5).Num(), 7);
    TestEqual(TEXT("Level 20 capped at 5"), FClassFeatureRules::FeaturesThrough(EAdventurerRole::Fighter, 20).Num(), 7);
    TestTrue(TEXT("Cutting Words is a reaction, listed as not yet modelled"), FClassFeatureRules::FeatureSummary(EAdventurerRole::Bard, 3).Contains(TEXT("Lore: Cutting Words [Reaction]*")));
    TestTrue(TEXT("Stunning Strike at Monk 5"), FClassFeatureRules::FeatureSummary(EAdventurerRole::Monk, 5).Contains(TEXT("Stunning Strike [Free]*")));
    TestFalse(TEXT("Not before its level"), FClassFeatureRules::FeatureSummary(EAdventurerRole::Monk, 4).Contains(TEXT("Stunning Strike")));
    TestTrue(TEXT("Rage is a bonus action"), FClassFeatureRules::FeatureSummary(EAdventurerRole::Barbarian, 1).Contains(TEXT("Rage [Bonus Action]")));
    TestTrue(TEXT("Uncanny Dodge is a reaction"), FClassFeatureRules::FeatureSummary(EAdventurerRole::Rogue, 5).Contains(TEXT("Uncanny Dodge [Reaction]")));
    for (const EAdventurerRole Role : {EAdventurerRole::Fighter, EAdventurerRole::Barbarian, EAdventurerRole::Cleric, EAdventurerRole::Bard, EAdventurerRole::Druid, EAdventurerRole::Monk, EAdventurerRole::Paladin, EAdventurerRole::Sorcerer,
        EAdventurerRole::Rogue, EAdventurerRole::Wizard, EAdventurerRole::Ranger, EAdventurerRole::Warlock})
    {
        TestTrue(TEXT("Every SRD class has features"), FClassFeatureRules::FeaturesThrough(Role, 5).Num() > 0);
        TestTrue(TEXT("Every SRD class lists its level 4 ASI"), FClassFeatureRules::FeatureSummary(Role, 4).Contains(TEXT("Ability Score Improvement [Free]")));
    }
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProgressSheetTest, "Project.Expedition.Rules.ProgressSheet",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FProgressSheetTest::RunTest(const FString& Parameters)
{
    FCombatantState Hero; TestTrue(TEXT("Create"), FBuildCatalog::Create(TEXT("fighter.battle-master"), 1, TEXT("F"), Hero));
    const TArray<FString> Lines = FBuildCatalog::ProgressSheet(Hero);
    TestEqual(TEXT("Level line"), Lines[0], FString(TEXT("Level 1 | XP 0 / 300")));
    TestEqual(TEXT("Class line"), Lines[1], FString(TEXT("Class: Fighter 1")));
    TestTrue(TEXT("HP and hit dice"), Lines[2].Contains(TEXT("Hit dice 1 / 1")));
    TestTrue(TEXT("Features tagged"), Lines.Last().Contains(TEXT("[Bonus Action]")) || Lines.ContainsByPredicate([](const FString& L){ return L.Contains(TEXT("[Bonus Action]")); }));
    Hero.Xp = 300;
    TestTrue(TEXT("Level up flagged"), FBuildCatalog::ProgressSheet(Hero).Contains(TEXT("Level up available")));
    return true;
}
#endif