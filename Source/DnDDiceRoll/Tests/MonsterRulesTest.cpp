#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Rules/MonsterRules.h"
#include "Rules/AbilityRules.h"
#include "Combat/CombatBattle.h"
#include "Persistence/ExpeditionSave.h"
#include "Run/ExpeditionRun.h"

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
    TestEqual(TEXT("Thug block STR 15"), Thug->Scores.Strength, 15);
    TestEqual(TEXT("Legacy roles keep one attack"), FMonsterRules::AttacksPerAction(TEXT("Raider")), 1);
    TestEqual(TEXT("Raider maps to Bandit"), FMonsterRules::ResolveExpeditionRole(TEXT("Raider"), false)->Name, TEXT("Bandit"));
    TestEqual(TEXT("Dangerous Raider maps to Thug"), FMonsterRules::ResolveExpeditionRole(TEXT("Raider"), true)->Name, TEXT("Thug"));
    TestEqual(TEXT("Archer maps to Bandit Archer"), FMonsterRules::ResolveExpeditionRole(TEXT("Archer"), false)->Name, TEXT("Bandit Archer"));
    TestEqual(TEXT("Safe Raider resolves to Bandit STR 11"), FMonsterRules::ResolveExpeditionRole(TEXT("Raider"), false)->Scores.Strength, 11);
    TestEqual(TEXT("Danger Raider resolves to Thug STR 15"), FMonsterRules::ResolveExpeditionRole(TEXT("Raider"), true)->Scores.Strength, 15);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnemyScoresLegacyArcherTest, "Project.Expedition.Rules.EnemyScoresKeepLegacyArcher",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FEnemyScoresLegacyArcherTest::RunTest(const FString& Parameters)
{
    // #177 review finding 1: this libraries-only delivery keeps staging's EnemyScores interpretation, so SRD checkpoints saved
    // before it still restore. The explicit monster resolver (Bandit Archer, DEX 12) waits for later versioned wiring.
    const FAbilityScores Archer = FAbilityRules::EnemyScores(TEXT("Archer"));
    TestEqual(TEXT("Archer keeps DEX 14"), Archer.Dexterity, 14);
    TestEqual(TEXT("Archer keeps the +2 SRD initiative bonus"), FAbilityRules::Modifier(Archer.Dexterity), 2);
    TestEqual(TEXT("Scout keeps DEX 14"), FAbilityRules::EnemyScores(TEXT("Scout")).Dexterity, 14);
    TestEqual(TEXT("Raider keeps the generic DEX 12"), FAbilityRules::EnemyScores(TEXT("Raider")).Dexterity, 12);
    TestEqual(TEXT("Raider keeps the generic STR 12"), FAbilityRules::EnemyScores(TEXT("Raider")).Strength, 12);
    const FMonsterStatBlock* BanditArcher = FMonsterRules::Find(TEXT("Bandit Archer"));
    TestTrue(TEXT("The resolver's Bandit Archer stays available for later wiring"), BanditArcher && BanditArcher->Scores.Dexterity == 12);

    // A genuine SRD checkpoint with an Archer: Start derives +2, and the real save round trip restores it.
    FExpeditionRun Run; FString Error;
    if (!TestTrue(TEXT("An SRD run starts"), Run.NewRun(42, Error, TEXT("legacy-archer-srd"), EFighterSubclass::Champion, ERuleset::SRD))) { return false; }
    if (!TestTrue(TEXT("It chooses a route"), Run.ChooseRoute(Run.Progress().Offers[0].Id, Error))) { return false; }
    int32 Archers = 0;
    for (const FCombatantState& Unit : Run.Battle().Snapshot().Encounter.Combatants)
    {
        if (Unit.Side == ECombatSide::Enemy && Unit.Role == TEXT("Archer"))
        {
            ++Archers;
            TestEqual(FString::Printf(TEXT("%s rolls initiative at +2"), *Unit.Name), Unit.InitiativeBonus, 2);
        }
    }
    TestTrue(TEXT("The opening SRD fight has an Archer"), Archers > 0);
    FExpeditionSaveData Data; Data.Current = Run.Snapshot();
    FString Json; FExpeditionSaveData Loaded;
    TestTrue(TEXT("The checkpoint encodes"), FExpeditionSave::Encode(Data, Json, Error));
    TestTrue(TEXT("The checkpoint decodes"), FExpeditionSave::Decode(Json, Loaded, Error));
    FExpeditionRun Resumed;
    TestTrue(TEXT("The Archer checkpoint restores"), Resumed.Restore(Loaded.Current, Error));
    return !HasAnyErrors();
}
#endif
