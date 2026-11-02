#include "Misc/AutomationTest.h"
#include "Rules/MonsterRules.h"
#include "Rules/AbilityRules.h"
#include "Combat/CombatBattle.h"
#include "Party/FeatureKit.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMonsterStatBlockTest, "Project.Expedition.Rules.MonsterStatBlocks",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMonsterStatBlockTest::RunTest(const FString& Parameters)
{
    // SRD 5.1 (CC-BY-4.0) numbers: hit-dice average + CON, prof +2, attack = prof + ability.
    const FMonsterStatBlock* Bandit = FMonsterRules::Find(TEXT("Bandit"));
    const FMonsterStatBlock* Thug = FMonsterRules::Find(TEXT("Thug"));
    const FMonsterStatBlock* Scout = FMonsterRules::Find(TEXT("Scout"));
    const FMonsterStatBlock* Acolyte = FMonsterRules::Find(TEXT("Acolyte"));
    if (!TestTrue(TEXT("All four blocks"), Bandit && Thug && Scout && Acolyte)) { return false; }
    TestEqual(TEXT("Bandit HP 11 (2d8+2)"), FMonsterRules::HitPoints(*Bandit), 11);
    TestEqual(TEXT("Bandit AC 12"), Bandit->ArmorClass, 12);
    TestEqual(TEXT("Bandit +3 to hit"), FMonsterRules::AttackBonus(*Bandit), 3);
    TestEqual(TEXT("Thug HP 32 (5d8+10)"), FMonsterRules::HitPoints(*Thug), 32);
    TestEqual(TEXT("Thug +4 to hit"), FMonsterRules::AttackBonus(*Thug), 4);
    TestEqual(TEXT("Thug Multiattack"), FMonsterRules::AttacksPerAction(TEXT("Thug")), 2);
    TestEqual(TEXT("Scout HP 16 (3d8+3)"), FMonsterRules::HitPoints(*Scout), 16);
    TestEqual(TEXT("Scout +4 to hit"), FMonsterRules::AttackBonus(*Scout), 4);
    TestEqual(TEXT("Bandit single attack"), FMonsterRules::AttacksPerAction(TEXT("Bandit")), 1);
    TestEqual(TEXT("Acolyte HP 9 (2d8)"), FMonsterRules::HitPoints(*Acolyte), 9);
    TestEqual(TEXT("Acolyte spell save DC 12"), FMonsterRules::SpellSaveDc(*Acolyte), 12);
    TestTrue(TEXT("Acolyte casts a save cantrip"), Acolyte->Kind == EMonsterAttack::SaveCantrip);
    TestEqual(TEXT("Enemy scores come from the block"), FAbilityRules::EnemyScores(TEXT("Thug")).Strength, 15);
    TestEqual(TEXT("Legacy roles keep generic scores"), FMonsterRules::AttacksPerAction(TEXT("Raider")), 1);
    return true;
}

namespace MonsterTestFixtures
{
// One SRD foe (id 2, acts first) against the tutorial wizard Ivo (id 1); open ground.
FCombatEncounter Duel(const TCHAR* Role, FIntPoint FoeCell, int32 WizardLevel)
{
    const FCombatEncounter Tutorial = FCombatBattle::TutorialEncounter();
    FCombatEncounter Layout = Tutorial;
    Layout.BlockedCells.Reset();
    Layout.Combatants = {Tutorial.Combatants[3], Tutorial.Combatants[4]};
    FCombatantState& Hero = Layout.Combatants[0];
    Hero.Id = 1; Hero.Cell = {0, 0}; Hero.InitiativeBonus = -10; Hero.Level = WizardLevel; Hero.MaxHP = Hero.HP = 200; Hero.SpellSlots = 2;
    const FMonsterStatBlock& Block = *FMonsterRules::Find(Role);
    FCombatantState& Foe = Layout.Combatants[1];
    Foe.Id = 2; Foe.Cell = FoeCell; Foe.Role = Role; Foe.Name = Role; Foe.InitiativeBonus = 30; Foe.Range = Block.Range;
    Foe.MaxHP = Foe.HP = FMonsterRules::HitPoints(Block); Foe.AttackBonus = FMonsterRules::AttackBonus(Block);
    Foe.DamageDie = Block.DamageDieSides; Foe.DamageBonus = FMonsterRules::DamageBonus(Block);
    return Layout;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMonsterCombatTest, "Project.Expedition.Rules.MonsterActions",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMonsterCombatTest::RunTest(const FString& Parameters)
{
    using namespace MonsterTestFixtures;
    FString Error;
    auto Attacks = [](const TArray<FCombatResult>& Results, bool bSave)
    {
        int32 N = 0; for (const FCombatResult& R : Results) { N += R.bAccepted && R.bSaveSpell == bSave && !R.AttackRolls.IsEmpty() ? 1 : 0; } return N;
    };
    {
        FCombatBattle Battle;
        if (!TestTrue(TEXT("Thug duel starts: ") + Error, Battle.Start(Duel(TEXT("Thug"), {1, 0}, 1), 7, Error))) { return false; }
        TestEqual(TEXT("Thug Multiattack: two mace swings in one turn"), Attacks(Battle.PlayEnemyTurn(), false), 2);
    }
    {
        FCombatBattle Battle;
        if (!TestTrue(TEXT("Acolyte duel starts"), Battle.Start(Duel(TEXT("Acolyte"), {3, 0}, 1), 7, Error))) { return false; }
        const TArray<FCombatResult> Results = Battle.PlayEnemyTurn();
        const FCombatResult* Flame = Results.FindByPredicate([](const FCombatResult& R) { return R.bSaveSpell; });
        if (TestNotNull(TEXT("Acolyte casts Sacred Flame"), Flame))
        {
            TestEqual(TEXT("Against its spell save DC"), Flame->TargetArmorClass, 12);
            TestFalse(TEXT("A level 1 wizard cannot Counterspell"), Flame->bCounterspelled);
            TestTrue(TEXT("Feedback names the save"), Flame->Feedback.Contains(TEXT("DEX save")));
            TestEqual(TEXT("Failed save is the only way to take damage"), Flame->Damage > 0, Flame->bHit);
        }
    }
    {
        FCombatBattle Battle;
        if (!TestTrue(TEXT("Counterspell duel starts"), Battle.Start(Duel(TEXT("Acolyte"), {3, 0}, 5), 7, Error))) { return false; }
        if (!TestTrue(TEXT("Wizard 5 has Counterspell"), FFeatureKit::HasCounterspell(*Battle.Find(1)))) { return false; }
        const TArray<FCombatResult> Results = Battle.PlayEnemyTurn();
        const FCombatResult* Flame = Results.FindByPredicate([](const FCombatResult& R) { return R.bSaveSpell; });
        if (TestNotNull(TEXT("Acolyte casts"), Flame))
        {
            TestTrue(TEXT("Counterspell stops it"), Flame->bCounterspelled && Flame->Damage == 0);
            TestEqual(TEXT("Counterspell spends a slot"), Battle.Find(1)->SpellSlots, 1);
            // The reaction refreshes when Ivo's own turn starts right after, so check the log instead.
            TestTrue(TEXT("Counterspell spends the reaction"), Flame->RollModeSources.ContainsByPredicate([](const FString& S) { return S.Contains(TEXT("reaction")); }));
        }
        TestTrue(TEXT("Counterspell is a Reaction"), FFeatureKit::Cost(ECombatCommandType::Counterspell) == EActionCost::Reaction);
    }
    {
        // Walled off: no path to the wizard, so the melee Bandit takes the Dodge action.
        FCombatEncounter Layout = Duel(TEXT("Bandit"), {6, 0}, 1);
        for (int32 Y = 0; Y < Layout.Height; ++Y) { Layout.BlockedCells.Add({4, Y}); }
        FCombatBattle Battle;
        if (!TestTrue(TEXT("Walled duel starts"), Battle.Start(Layout, 7, Error))) { return false; }
        Battle.PlayEnemyTurn();
        TestTrue(TEXT("Unreachable melee foe Dodges"), Battle.Find(2)->bDodging);
    }
    {
        // Far but reachable: the Bandit Dashes and closes the gap.
        FCombatEncounter Layout = Duel(TEXT("Bandit"), {Duel(TEXT("Bandit"), {0, 0}, 1).Width - 1, 0}, 1);
        FCombatBattle Battle;
        if (!TestTrue(TEXT("Long duel starts"), Battle.Start(Layout, 7, Error))) { return false; }
        const TArray<FCombatResult> Results = Battle.PlayEnemyTurn();
        TestTrue(TEXT("Out-of-reach foe Dashes"), Results.ContainsByPredicate([](const FCombatResult& R) { return R.bAccepted && R.Feedback.Contains(TEXT("dashes")); }));
    }
    {
        // A bloodied Scout next to the wizard Disengages and backs off instead of trading blows.
        FCombatEncounter Layout = Duel(TEXT("Scout"), {1, 0}, 1);
        Layout.Combatants[1].HP = 5;
        FCombatBattle Battle;
        if (!TestTrue(TEXT("Skirmish duel starts"), Battle.Start(Layout, 7, Error))) { return false; }
        Battle.PlayEnemyTurn();
        TestTrue(TEXT("and ends out of melee"), FMath::Max(FMath::Abs(Battle.Find(2)->Cell.X), FMath::Abs(Battle.Find(2)->Cell.Y)) > 1);
    }
    return true;
}
#endif