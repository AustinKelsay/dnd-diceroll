#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Rules/MonsterRules.h"
#include "Rules/AbilityRules.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMonsterStatBlockTest, "Project.Expedition.Rules.MonsterStatBlocks",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FMonsterStatBlockTest::RunTest(const FString& Parameters)
{
    // SRD 5.1 (CC-BY-4.0) numbers: hit-dice average + CON, prof +2, attack = prof + ability.
    const FMonsterStatBlock* Bandit = FMonsterRules::Find(TEXT("Bandit"));
    const FMonsterStatBlock* Thug = FMonsterRules::Find(TEXT("Thug"));
    const FMonsterStatBlock* Scout = FMonsterRules::Find(TEXT("Scout"));
    const FMonsterStatBlock* Acolyte = FMonsterRules::Find(TEXT("Acolyte"));
    const FMonsterStatBlock* Archer = FMonsterRules::Find(TEXT("Bandit Archer"));
    if (!TestTrue(TEXT("All five blocks"), Bandit && Thug && Scout && Acolyte && Archer)) { return false; }
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
    TestEqual(TEXT("Enemy scores prefer Thug STR 15"), FAbilityRules::EnemyScores(TEXT("Thug")).Strength, 15);
    TestEqual(TEXT("Legacy roles keep one attack"), FMonsterRules::AttacksPerAction(TEXT("Raider")), 1);
    TestEqual(TEXT("Raider maps to Bandit"), FMonsterRules::ResolveExpeditionRole(TEXT("Raider"), false)->Name, TEXT("Bandit"));
    TestEqual(TEXT("Dangerous Raider maps to Thug"), FMonsterRules::ResolveExpeditionRole(TEXT("Raider"), true)->Name, TEXT("Thug"));
    TestEqual(TEXT("Archer maps to Bandit Archer"), FMonsterRules::ResolveExpeditionRole(TEXT("Archer"), false)->Name, TEXT("Bandit Archer"));
    TestEqual(TEXT("Safe Raider scores are Bandit STR 11"), FAbilityRules::EnemyScores(TEXT("Raider"), false).Strength, 11);
    TestEqual(TEXT("Danger Raider scores are Thug STR 15"), FAbilityRules::EnemyScores(TEXT("Raider"), true).Strength, 15);
    return true;
}
#endif
