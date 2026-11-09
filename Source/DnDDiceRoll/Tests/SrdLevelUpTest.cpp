#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Combat/CombatBattle.h"
#include "Party/BuildCatalog.h"
#include "Party/PartyContinuity.h"
#include "Rules/MonsterRules.h"
#include "Rules/AdvancementRules.h"
#include "Rules/AbilityRules.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSrdRolledEnemyHpAndLevelUpTest, "Project.Expedition.Rules.SrdRolledEnemyHpAndLevelUp",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSrdRolledEnemyHpAndLevelUpTest::RunTest(const FString& Parameters)
{
    const FMonsterStatBlock* Bandit = FMonsterRules::Find(TEXT("Bandit"));
    TestTrue(TEXT("Bandit block"), Bandit != nullptr);
    FRandomStream StreamA(12345), StreamB(12345);
    TestEqual(TEXT("Same seed same HP"), FMonsterRules::RollHitPoints(*Bandit, StreamA), FMonsterRules::RollHitPoints(*Bandit, StreamB));

    FCombatEncounter Encounter;
    Encounter.Ruleset = ERuleset::SRD;
    Encounter.Width = 8; Encounter.Height = 8;
    const FBuildDefinition* Build = FBuildCatalog::Find(TEXT("fighter.champion"));
    TestTrue(TEXT("Champion build"), Build != nullptr);
    FCombatantState Hero = Build->Profile;
    Hero.Id = 1; Hero.Name = TEXT("Hero"); Hero.Side = ECombatSide::Party; Hero.Cell = FIntPoint(1, 1);
    Hero.Level = 1; Hero.ExperiencePoints = 0; Hero.ClassLevels.Fighter = 1;
    FCombatantState Enemy;
    Enemy.Id = 2; Enemy.Name = TEXT("Bandit"); Enemy.Role = TEXT("Bandit"); Enemy.Side = ECombatSide::Enemy;
    Enemy.Cell = FIntPoint(3, 1); Enemy.MaxHP = 11; Enemy.HP = 11; Enemy.ArmorClass = 12; Enemy.AttackBonus = 3;
    Enemy.DamageDie = 6; Enemy.DamageBonus = 1; Enemy.Speed = 4; Enemy.Level = 1;
    Encounter.Combatants = { Hero, Enemy };

    FCombatBattle Battle; FString Error;
    TestTrue(TEXT("Start SRD"), Battle.Start(Encounter, 42, Error));
    FCombatBattle Battle2;
    TestTrue(TEXT("Start again"), Battle2.Start(Encounter, 42, Error));
    TestEqual(TEXT("Same seed same enemy HP"), Battle.Find(2)->MaxHP, Battle2.Find(2)->MaxHP);

    Battle.State.Phase = EBattlePhase::Victory;
    FPartyContinuity::AwardEncounterExperience(Battle);
    TestEqual(TEXT("Bandit XP 25"), Battle.Find(1)->ExperiencePoints, 25);
    TestFalse(TEXT("25 XP not enough to level"), Battle.Find(1)->bLevelUpAvailable);

    {
        FCombatEncounter Duo = Encounter;
        FCombatantState Hero2 = Hero; Hero2.Id = 3; Hero2.Name = TEXT("Hero2"); Hero2.Cell = FIntPoint(1, 2);
        Duo.Combatants = { Hero, Hero2, Enemy };
        FCombatBattle Split; FString Err;
        if (!TestTrue(TEXT("Start duo"), Split.Start(Duo, 7, Err))) { return false; }
        Split.State.Phase = EBattlePhase::Victory;
        FPartyContinuity::AwardEncounterExperience(Split);
        TestEqual(TEXT("Split XP floor 12"), Split.Find(1)->ExperiencePoints, 12);
        TestEqual(TEXT("Split XP equal"), Split.Find(3)->ExperiencePoints, 12);
    }

    {
        // Friend: FillSrdSpellSlots without a full Start (Warlock has no catalog build).
        FCombatantState Lock;
        Lock.Id = 1; Lock.Name = TEXT("Lock"); Lock.Side = ECombatSide::Party; Lock.Role = TEXT("Warlock");
        Lock.AdventurerRole = EAdventurerRole::Warlock; Lock.Level = 5; Lock.ClassLevels.Warlock = 5;
        Lock.MaxHP = 20; Lock.HP = 20; Lock.ArmorClass = 12; Lock.Speed = 4; Lock.Cell = FIntPoint(1, 1);
        FCombatBattle PactBattle;
        PactBattle.State.Encounter.Ruleset = ERuleset::SRD;
        PactBattle.FillSrdSpellSlots(Lock);
        TestEqual(TEXT("Pact level 3 at WL5"), Lock.PactSlotLevel, 3);
        TestEqual(TEXT("Pact slots 2 at WL5"), Lock.PactSlots, 2);
        TestEqual(TEXT("No multiclass 1st"), Lock.SpellSlot1, 0);
        TestEqual(TEXT("No multiclass 3rd"), Lock.SpellSlot3, 0);
    }

    Battle.State.Encounter.Combatants[0].ExperiencePoints = 300;
    Battle.State.Encounter.Combatants[0].bLevelUpAvailable = true;
    const int32 MaxBefore = Battle.Find(1)->MaxHP;
    const int32 ConMod = FAbilityRules::Modifier(FAbilityRules::PrebuiltScores(EAdventurerRole::Fighter).Constitution);
    TestTrue(TEXT("Apply L2"), FPartyContinuity::ApplyLevelUp(Battle, 1, EAdventurerRole::Fighter, 7, Error));
    TestEqual(TEXT("Now level 2"), Battle.Find(1)->Level, 2);
    TestEqual(TEXT("Fighter class 2"), Battle.Find(1)->ClassLevels.Fighter, 2);
    TestEqual(TEXT("HP gained"), Battle.Find(1)->MaxHP, MaxBefore + FAdvancementRules::HpGainForClassLevel(EAdventurerRole::Fighter, 7, ConMod));
    TestFalse(TEXT("No pending after L2 at 300 XP"), Battle.Find(1)->bLevelUpAvailable);

    Battle.State.Phase = EBattlePhase::Fighting;
    Battle.State.Encounter.Combatants[0].ExperiencePoints = 6500;
    TestFalse(TEXT("Reject during fight"), FPartyContinuity::ApplyLevelUp(Battle, 1, EAdventurerRole::Fighter, 5, Error));
    return !HasAnyErrors();
}
#endif