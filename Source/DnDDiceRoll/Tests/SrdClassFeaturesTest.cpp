#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Combat/CombatBattle.h"
#include "Rules/ClassFeatureRules.h"
#include "Rules/MonsterRules.h"
#include "Party/BuildCatalog.h"

namespace
{
bool WaitForActor(FCombatBattle& Battle, int32 Id, int32 MaxTurns = 16)
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

FCombatEncounter ClassDuo(ERuleset Ruleset, int32 FighterLevel)
{
    FCombatEncounter Layout;
    Layout.Width = 8; Layout.Height = 8; Layout.Ruleset = Ruleset;
    FCombatantState Hero;
    FBuildCatalog::Create(TEXT("fighter.champion"), 1, TEXT("Arden"), Hero);
    Hero.Cell = {1, 1}; Hero.Level = FighterLevel;
    Hero.ActionSurgeUses = FClassFeatureRules::ActionSurgeUses(FighterLevel);
    Hero.HP = Hero.MaxHP = 40;
    FCombatantState Foe;
    Foe.Id = 2; Foe.Name = TEXT("Thug"); Foe.Role = TEXT("Thug"); Foe.Side = ECombatSide::Enemy;
    Foe.Cell = {2, 1}; Foe.Speed = 4;
    Layout.Combatants = {Hero, Foe};
    return Layout;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSrdSecondWindScalesWithLevelTest, "Project.Expedition.Battle.SrdSecondWindScalesWithLevel",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSrdSecondWindScalesWithLevelTest::RunTest(const FString& Parameters)
{
    FString Error;
    FCombatEncounter Layout = ClassDuo(ERuleset::SRD, 5);
    Layout.Combatants[0].HP = 10;
    Layout.Combatants[0].SecondWindUses = 1;
    FCombatBattle Battle;
    if (!TestTrue(TEXT("start"), Battle.Start(Layout, 3, Error))) { return false; }
    if (!TestTrue(TEXT("fighter turn"), WaitForActor(Battle, 1))) { return false; }
    const FCombatResult Result = Battle.Submit({ECombatCommandType::SecondWind, 1, {}, 1});
    if (!TestTrue(TEXT("accepted"), Result.bAccepted)) { return false; }
    TestEqual(TEXT("modifier is fighter level"), Result.EffectModifier, 5);
    TestTrue(TEXT("healed"), Result.Healing >= 6 && Result.Healing <= 15);
    TestEqual(TEXT("HP matches heal"), Battle.Find(1)->HP, 10 + Result.Healing);
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSrdSneakAttackDiceByLevelTest, "Project.Expedition.Battle.SrdSneakAttackDiceByLevel",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSrdSneakAttackDiceByLevelTest::RunTest(const FString& Parameters)
{
    FString Error;
    FCombatEncounter Layout;
    Layout.Width = 8; Layout.Height = 8; Layout.Ruleset = ERuleset::SRD;
    FCombatantState Rogue;
    FBuildCatalog::Create(TEXT("rogue.thief"), 1, TEXT("Nix"), Rogue);
    Rogue.Cell = {1, 1}; Rogue.Level = 5; Rogue.bSneakWeapon = true;
    FCombatantState Ally;
    FBuildCatalog::Create(TEXT("fighter.champion"), 3, TEXT("Arden"), Ally);
    Ally.Cell = {3, 1};
    FCombatantState Foe;
    Foe.Id = 2; Foe.Name = TEXT("Raider"); Foe.Role = TEXT("Raider"); Foe.Side = ECombatSide::Enemy;
    Foe.Cell = {2, 1}; Foe.MaxHP = Foe.HP = 80; Foe.ArmorClass = 5; Foe.AttackBonus = 0; Foe.DamageDie = 4; Foe.DamageBonus = 0;
    Foe.Range = 1; Foe.Speed = 4;
    Layout.Combatants = {Rogue, Foe, Ally};
    FCombatBattle Battle;
    if (!TestTrue(TEXT("start"), Battle.Start(Layout, 1, Error))) { return false; }
    if (!TestTrue(TEXT("rogue turn"), WaitForActor(Battle, 1))) { return false; }
    const FCombatResult Result = Battle.Submit({ECombatCommandType::Attack, 1, {}, 2});
    if (!TestTrue(TEXT("hit with sneak"), Result.bAccepted && Result.bHit && Result.bSneakAttack)) { return false; }
    const FString Expect = Result.bCritical ? TEXT("SNEAK +6d6") : TEXT("SNEAK +3d6");
    TestTrue(TEXT("feedback names sneak dice"), Result.Feedback.Contains(Expect));
    TestTrue(TEXT("once per turn spent"), Battle.Snapshot().bSneakUsedThisTurn);
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSrdExtraAttackAndActionSurgeTest, "Project.Expedition.Battle.SrdExtraAttackAndActionSurge",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSrdExtraAttackAndActionSurgeTest::RunTest(const FString& Parameters)
{
    FString Error;
    FCombatEncounter Layout = ClassDuo(ERuleset::SRD, 5);
    FCombatBattle Battle;
    if (!TestTrue(TEXT("start"), Battle.Start(Layout, 9, Error))) { return false; }
    TestEqual(TEXT("Thug average HP helper"), FMonsterRules::HitPoints(*FMonsterRules::Find(TEXT("Thug"))), 32); TestTrue(TEXT("Thug rolled HP in hit-dice range"), Battle.Find(2)->MaxHP >= 5 && Battle.Find(2)->MaxHP <= 50);
    TestEqual(TEXT("Thug Multiattack table"), FMonsterRules::AttacksPerAction(TEXT("Thug")), 2);
    TestEqual(TEXT("Fighter Extra Attack"), FClassFeatureRules::AttacksPerAction(EAdventurerRole::Fighter, 5), 2);
    if (!TestTrue(TEXT("fighter turn"), WaitForActor(Battle, 1))) { return false; }
    const FCombatResult First = Battle.Submit({ECombatCommandType::Attack, 1, {}, 2});
    TestTrue(TEXT("first swing"), First.bAccepted);
    TestFalse(TEXT("action spent"), Battle.Snapshot().bActionAvailable);
    TestEqual(TEXT("one Extra Attack left"), Battle.Snapshot().AttacksLeft, 1);
    const FCombatResult Second = Battle.Submit({ECombatCommandType::Attack, 1, {}, 2});
    TestTrue(TEXT("second swing"), Second.bAccepted);
    TestEqual(TEXT("no attacks left"), Battle.Snapshot().AttacksLeft, 0);
    TestTrue(TEXT("surge available"), Battle.Find(1)->ActionSurgeUses == 1);
    const FCombatResult Surge = Battle.Submit({ECombatCommandType::ActionSurge, 1, {}, 1});
    TestTrue(TEXT("surge accepted"), Surge.bAccepted);
    TestTrue(TEXT("action restored"), Battle.Snapshot().bActionAvailable);
    TestEqual(TEXT("surge spent"), Battle.Find(1)->ActionSurgeUses, 0);
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSrdThugMultiattackAiTest, "Project.Expedition.Battle.SrdThugMultiattackAi",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSrdThugMultiattackAiTest::RunTest(const FString& Parameters)
{
    FString Error;
    FCombatEncounter Layout;
    Layout.Width = 8; Layout.Height = 8; Layout.Ruleset = ERuleset::SRD;
    FCombatantState Hero;
    FBuildCatalog::Create(TEXT("wizard.evocation"), 1, TEXT("Ivo"), Hero);
    Hero.Cell = {0, 0}; Hero.MaxHP = Hero.HP = 200;
    FCombatantState Foe;
    Foe.Id = 2; Foe.Name = TEXT("Thug"); Foe.Role = TEXT("Thug"); Foe.Side = ECombatSide::Enemy;
    Foe.Cell = {1, 0}; Foe.Speed = 4;
    Layout.Combatants = {Hero, Foe};
    FCombatBattle Battle;
    if (!TestTrue(TEXT("start"), Battle.Start(Layout, 7, Error))) { return false; }
    if (!TestTrue(TEXT("thug turn"), WaitForActor(Battle, 2))) { return false; }
    const TArray<FCombatResult> Results = Battle.PlayEnemyTurn();
    int32 Attacks = 0;
    for (const FCombatResult& R : Results)
    {
        if (R.bAccepted && R.Type == ECombatCommandType::Attack) { ++Attacks; }
    }
    TestEqual(TEXT("Thug swings twice"), Attacks, 2);
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSrdCunningActionDisengageTest, "Project.Expedition.Battle.SrdCunningActionDisengage",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSrdCunningActionDisengageTest::RunTest(const FString& Parameters)
{
    FString Error;
    FCombatEncounter Layout;
    Layout.Width = 8; Layout.Height = 8; Layout.Ruleset = ERuleset::SRD;
    FCombatantState Rogue;
    FBuildCatalog::Create(TEXT("rogue.thief"), 1, TEXT("Nix"), Rogue);
    Rogue.Cell = {1, 1}; Rogue.Level = 2;
    FCombatantState Foe;
    Foe.Id = 2; Foe.Name = TEXT("Raider"); Foe.Role = TEXT("Raider"); Foe.Side = ECombatSide::Enemy;
    Foe.Cell = {2, 1}; Foe.MaxHP = Foe.HP = 40; Foe.ArmorClass = 10; Foe.AttackBonus = 2; Foe.DamageDie = 6;
    Foe.Range = 1; Foe.Speed = 4;
    Layout.Combatants = {Rogue, Foe};
    FCombatBattle Battle;
    if (!TestTrue(TEXT("start"), Battle.Start(Layout, 5, Error))) { return false; }
    if (!TestTrue(TEXT("rogue turn"), WaitForActor(Battle, 1))) { return false; }
    TestTrue(TEXT("attack"), Battle.Submit({ECombatCommandType::Attack, 1, {}, 2}).bAccepted);
    TestFalse(TEXT("action gone"), Battle.Snapshot().bActionAvailable);
    const FCombatResult Dis = Battle.Submit({ECombatCommandType::Disengage, 1, {}, 1});
    TestTrue(TEXT("bonus Disengage"), Dis.bAccepted);
    TestTrue(TEXT("flag set"), Battle.Find(1)->bDisengaged);
    TestFalse(TEXT("bonus spent"), Battle.Snapshot().bBonusActionAvailable);
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSrdAcolyteSaveCantripTest, "Project.Expedition.Battle.SrdAcolyteSaveCantrip",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSrdAcolyteSaveCantripTest::RunTest(const FString& Parameters)
{
    FString Error;
    FCombatEncounter Layout;
    Layout.Width = 8; Layout.Height = 8; Layout.Ruleset = ERuleset::SRD;
    FCombatantState Hero;
    FBuildCatalog::Create(TEXT("wizard.evocation"), 1, TEXT("Ivo"), Hero);
    Hero.Cell = {0, 0}; Hero.MaxHP = Hero.HP = 200;
    FCombatantState Foe;
    Foe.Id = 2; Foe.Name = TEXT("Acolyte"); Foe.Role = TEXT("Acolyte"); Foe.Side = ECombatSide::Enemy;
    Foe.Cell = {3, 0}; Foe.Speed = 4;
    Layout.Combatants = {Hero, Foe};
    FCombatBattle Battle;
    if (!TestTrue(TEXT("start"), Battle.Start(Layout, 11, Error))) { return false; }
    if (!TestTrue(TEXT("acolyte turn"), WaitForActor(Battle, 2))) { return false; }
    TestEqual(TEXT("Acolyte DC 12 from block"), FMonsterRules::SpellSaveDc(*FMonsterRules::Find(TEXT("Acolyte"))), 12);
    const TArray<FCombatResult> Results = Battle.PlayEnemyTurn();
    const FCombatResult* Flame = Results.FindByPredicate([](const FCombatResult& R) { return R.bAccepted && R.Type == ECombatCommandType::Attack; });
    if (!TestNotNull(TEXT("Acolyte attacks"), Flame)) { return false; }
    TestEqual(TEXT("save DC in AC slot"), Flame->TargetArmorClass, 12);
    TestFalse(TEXT("no weapon crit on save cantrip"), Flame->bCritical);
    TestTrue(TEXT("feedback is a DEX save"), Flame->Feedback.Contains(TEXT("DEX")) && Flame->Feedback.Contains(TEXT("vs DC 12")));
    TestTrue(TEXT("names Sacred Flame"), Flame->Feedback.Contains(TEXT("Sacred Flame")));
    return !HasAnyErrors();
}

#endif
