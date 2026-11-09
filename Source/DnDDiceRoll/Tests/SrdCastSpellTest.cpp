#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Combat/CombatBattle.h"
#include "Rules/SpellRules.h"
#include "Rules/CantripRules.h"
#include "Party/BuildCatalog.h"
#include "Run/ExpeditionRun.h"

namespace
{
bool WaitFor(FCombatBattle& Battle, int32 Id, int32 MaxTurns = 16)
{
    for (int32 I = 0; I < MaxTurns; ++I)
    {
        if (Battle.Snapshot().Phase != EBattlePhase::Fighting) { return false; }
        if (Battle.ActiveId() == Id) { return true; }
        const FCombatantState* Active = Battle.Find(Battle.ActiveId());
        if (!Active) { return false; }
        if (Active->Side == ECombatSide::Enemy) { Battle.PlayEnemyTurn(); }
        else { Battle.Submit({ECombatCommandType::EndTurn, Active->Id}); }
    }
    return Battle.ActiveId() == Id;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSrdPerLevelSlotsOnNewRunTest, "Project.Expedition.Run.SrdPerLevelSpellSlots",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSrdPerLevelSlotsOnNewRunTest::RunTest(const FString& Parameters)
{
    FExpeditionRun Run; FString Error;
    TestTrue(TEXT("SRD NewRun"), Run.NewRun(11, Error, TEXT("slots-srd"), EFighterSubclass::Champion, ERuleset::SRD));
    const FCombatantState* Wizard = nullptr; const FCombatantState* Cleric = nullptr; const FCombatantState* Fighter = nullptr;
    for (const auto& U : Run.Battle().Snapshot().Encounter.Combatants)
    {
        if (U.AdventurerRole == EAdventurerRole::Wizard) { Wizard = &U; }
        if (U.AdventurerRole == EAdventurerRole::Cleric) { Cleric = &U; }
        if (U.AdventurerRole == EAdventurerRole::Fighter) { Fighter = &U; }
    }
    if (!TestTrue(TEXT("party roles"), Wizard && Cleric && Fighter)) { return false; }
    TestEqual(TEXT("Wizard L1: 2 first-level"), Wizard->SpellSlot1, 2);
    TestEqual(TEXT("Wizard L1: no 2nd"), Wizard->SpellSlot2, 0);
    TestEqual(TEXT("Cleric L1: 2 first-level"), Cleric->SpellSlot1, 2);
    TestEqual(TEXT("Fighter: no slots"), Fighter->SpellSlot1, 0);
    // Compact still uses SpellSlots pool only.
    FExpeditionRun Compact; TestTrue(TEXT("Compact"), Compact.NewRun(11, Error, TEXT("slots-c"), EFighterSubclass::Champion, ERuleset::Compact));
    for (const auto& U : Compact.Battle().Snapshot().Encounter.Combatants)
    {
        if (U.AdventurerRole == EAdventurerRole::Wizard)
        {
            TestEqual(TEXT("Compact SpellSlots 2"), U.SpellSlots, 2);
            TestEqual(TEXT("Compact SpellSlot1 stays 0"), U.SpellSlot1, 0);
        }
    }
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSrdCastSpellClassListAndFireBoltTest, "Project.Expedition.Battle.SrdCastSpellClassListFireBolt",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSrdCastSpellClassListAndFireBoltTest::RunTest(const FString& Parameters)
{
    FString Error;
    FCombatEncounter Layout; Layout.Width = 8; Layout.Height = 8; Layout.Ruleset = ERuleset::SRD;
    FCombatantState Wizard; FBuildCatalog::Create(TEXT("wizard.evocation"), 1, TEXT("Ivo"), Wizard);
    Wizard.Cell = {1, 1}; Wizard.Level = 1; Wizard.SpellSlot1 = 2;
    FCombatantState Foe; Foe.Id = 2; Foe.Name = TEXT("Raider"); Foe.Role = TEXT("Raider"); Foe.Side = ECombatSide::Enemy;
    Foe.Cell = {4, 1}; Foe.MaxHP = Foe.HP = 40; Foe.ArmorClass = 10; Foe.Speed = 4;
    Layout.Combatants = {Wizard, Foe};
    FCombatBattle Battle;
    if (!TestTrue(TEXT("start"), Battle.Start(Layout, 3, Error))) { return false; }
    if (!TestTrue(TEXT("wizard turn"), WaitFor(Battle, 1))) { return false; }
    // Cleric-only spell rejected for Wizard.
    FCombatCommand Bad{ECombatCommandType::CastSpell, 1, {}, 2};
    Bad.SpellId = static_cast<int32>(ESpell::CureWounds); Bad.SlotLevel = 1;
    TestFalse(TEXT("Wizard cannot Cure Wounds"), Battle.Submit(Bad).bAccepted);
    FCombatCommand Bolt{ECombatCommandType::CastSpell, 1, {}, 2};
    Bolt.SpellId = static_cast<int32>(ESpell::FireBolt); Bolt.SlotLevel = 0;
    const FCombatResult Result = Battle.Submit(Bolt);
    TestTrue(TEXT("Fire Bolt accepted"), Result.bAccepted);
    TestTrue(TEXT("names Fire Bolt"), Result.Feedback.Contains(TEXT("Fire Bolt")));
    TestEqual(TEXT("cantrip spends no slot"), Battle.Find(1)->SpellSlot1, 2);
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSrdCastSpellShieldNegatesMissileTest, "Project.Expedition.Battle.SrdCastSpellShieldNegatesMissile",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSrdCastSpellShieldNegatesMissileTest::RunTest(const FString& Parameters)
{
    FString Error;
    FCombatEncounter Layout; Layout.Width = 8; Layout.Height = 8; Layout.Ruleset = ERuleset::SRD;
    FCombatantState Defender; FBuildCatalog::Create(TEXT("wizard.evocation"), 1, TEXT("Ivo"), Defender);
    Defender.Cell = {1, 1}; Defender.Level = 1; Defender.SpellSlot1 = 2; Defender.InitiativeBonus = 30;
    FCombatantState Attacker;
    Attacker.Id = 2; Attacker.Name = TEXT("Nyx"); Attacker.Role = TEXT("Wizard");
    Attacker.AdventurerRole = EAdventurerRole::Wizard; Attacker.Side = ECombatSide::Enemy;
    Attacker.Cell = {3, 1}; Attacker.Level = 1; Attacker.SpellSlot1 = 2;
    Attacker.MaxHP = Attacker.HP = 40; Attacker.ArmorClass = 10; Attacker.AttackBonus = 2;
    Attacker.DamageDie = 4; Attacker.DamageBonus = 0; Attacker.Range = 4; Attacker.Speed = 4;
    Layout.Combatants = {Defender, Attacker};
    FCombatBattle Battle;
    if (!TestTrue(TEXT("start"), Battle.Start(Layout, 5, Error))) { return false; }
    // Start overwrites InitiativeBonus in SRD; wait for defender.
    if (!TestTrue(TEXT("defender turn"), WaitFor(Battle, 1))) { return false; }
    FCombatCommand Shield{ECombatCommandType::CastSpell, 1, {}, 1};
    Shield.SpellId = static_cast<int32>(ESpell::Shield); Shield.SlotLevel = 1;
    TestTrue(TEXT("Shield"), Battle.Submit(Shield).bAccepted);
    TestEqual(TEXT("+5 AC"), Battle.Find(1)->SpellArmorBonus, 5);
    TestTrue(TEXT("negates MM"), Battle.Find(1)->bNegatesMagicMissile);
    Battle.Submit({ECombatCommandType::EndTurn, 1});
    if (!TestTrue(TEXT("attacker turn"), WaitFor(Battle, 2))) { return false; }
    const int32 HpBefore = Battle.Find(1)->HP;
    FCombatCommand MM{ECombatCommandType::CastSpell, 2, {}, 1};
    MM.SpellId = static_cast<int32>(ESpell::MagicMissile); MM.SlotLevel = 1;
    const FCombatResult Result = Battle.Submit(MM);
    TestTrue(TEXT("MM accepted"), Result.bAccepted);
    TestEqual(TEXT("Shield negates all damage"), Result.Damage, 0);
    TestEqual(TEXT("HP unchanged"), Battle.Find(1)->HP, HpBefore);
    TestTrue(TEXT("feedback says negated"), Result.Feedback.Contains(TEXT("negated")));
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSrdUpcastMagicMissileSlotsTest, "Project.Expedition.Battle.SrdUpcastMagicMissile",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSrdUpcastMagicMissileSlotsTest::RunTest(const FString& Parameters)
{
    FString Error;
    FCombatEncounter Layout; Layout.Width = 8; Layout.Height = 8; Layout.Ruleset = ERuleset::SRD;
    FCombatantState Wizard; FBuildCatalog::Create(TEXT("wizard.evocation"), 1, TEXT("Ivo"), Wizard);
    Wizard.Cell = {1, 1}; Wizard.Level = 5; Wizard.SpellSlot1 = 4; Wizard.SpellSlot2 = 3; Wizard.SpellSlot3 = 2;
    FCombatantState Foe; Foe.Id = 2; Foe.Name = TEXT("Raider"); Foe.Role = TEXT("Raider"); Foe.Side = ECombatSide::Enemy;
    Foe.Cell = {3, 1}; Foe.MaxHP = Foe.HP = 100; Foe.ArmorClass = 10; Foe.Speed = 4;
    Layout.Combatants = {Wizard, Foe};
    FCombatBattle Battle;
    if (!TestTrue(TEXT("start"), Battle.Start(Layout, 7, Error))) { return false; }
    if (!TestTrue(TEXT("wizard"), WaitFor(Battle, 1))) { return false; }
    TestEqual(TEXT("upcast dice at slot 2"), FSpellRules::UpcastDiceCount(ESpell::MagicMissile, 2), 4);
    FCombatCommand Cmd{ECombatCommandType::CastSpell, 1, {}, 2};
    Cmd.SpellId = static_cast<int32>(ESpell::MagicMissile); Cmd.SlotLevel = 2;
    const FCombatResult Result = Battle.Submit(Cmd);
    TestTrue(TEXT("accepted"), Result.bAccepted && Result.bHit);
    TestEqual(TEXT("spent 2nd-level slot"), Battle.Find(1)->SpellSlot2, 2);
    TestEqual(TEXT("1st untouched"), Battle.Find(1)->SpellSlot1, 4);
    TestEqual(TEXT("four dart dice"), Result.EffectDice.Num(), 4);
    TestTrue(TEXT("feedback slot 2"), Result.Feedback.Contains(TEXT("slot 2")));
    return !HasAnyErrors();
}
#endif