#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Combat/CombatBattle.h"

namespace CoverTestFixtures
{
// Wizard (id 1, acts first) at (0,0) against one Raider; only the given cells block.
FCombatEncounter CoverDuel(FIntPoint FoeCell, const TArray<FIntPoint>& Walls, int32 ExtraArmor)
{
    const FCombatEncounter Tutorial = FCombatBattle::TutorialEncounter();
    FCombatEncounter Layout = Tutorial;
    Layout.BlockedCells = Walls;
    Layout.Combatants = {Tutorial.Combatants[3], Tutorial.Combatants[4]};
    FCombatantState& Hero = Layout.Combatants[0];
    Hero.Id = 1; Hero.Cell = {0, 0}; Hero.InitiativeBonus = 30; Hero.AttackBonus = 0; Hero.Range = 8;
    FCombatantState& Foe = Layout.Combatants[1];
    Foe.Id = 2; Foe.Cell = FoeCell; Foe.ArmorClass = 12 + ExtraArmor; Foe.MaxHP = Foe.HP = 200; Foe.Speed = 0;
    return Layout;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCoverFromTerrainTest, "Project.Expedition.Rules.CoverFromTerrain",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FCoverFromTerrainTest::RunTest(const FString& Parameters)
{
    using namespace CoverTestFixtures;
    struct FCase { const TCHAR* What; FIntPoint Foe; TArray<FIntPoint> Walls; int32 Bonus; };
    const FCase Cases[] = {
        {TEXT("open ground"), {4, 2}, {}, 0},
        {TEXT("wall beside the line, best corner clear"), {4, 0}, {{2, 1}}, 0},
        {TEXT("wall grazing the straight hex row: half"), {4, 0}, {{3, 1}}, 2},
        {TEXT("wall at the target's near side: three-quarters"), {5, 2}, {{4, 1}}, 5}};
    FString Error;
    for (const FCase& Case : Cases)
    {
        FCombatBattle Battle, Armored;
        if (!TestTrue(Case.What, Battle.Start(CoverDuel(Case.Foe, Case.Walls, 0), 7, Error) && Armored.Start(CoverDuel(Case.Foe, {}, Case.Bonus), 7, Error))) { return false; }
        TestEqual(FString(Case.What) + TEXT(": bonus"), Battle.CoverBonus(1, 2), Case.Bonus);
        const FCombatSnapshot Before = Battle.Snapshot();
        const FActionForecast Forecast = Battle.Forecast(1, ECombatCommandType::Attack, 2);
        TestTrue(FString(Case.What) + TEXT(": forecast valid with cover and effective AC"), Forecast.bValid && Forecast.CoverBonus == Case.Bonus && Forecast.TargetArmorClass == 12 + Case.Bonus);
        TestEqual(FString(Case.What) + TEXT(": hit chance equals AC raised by cover"), Forecast.HitChanceOutOf400, Armored.Forecast(1, ECombatCommandType::Attack, 2).HitChanceOutOf400);
        TestTrue(FString(Case.What) + TEXT(": forecast spends no RNG"), FCombatSnapshot::StaticStruct()->CompareScriptStruct(&Before, &Battle.Snapshot(), 0));
        const FCombatResult Result = Battle.Submit({ECombatCommandType::Attack, 1, {}, 2});
        const FCombatResult Same = Armored.Submit({ECombatCommandType::Attack, 1, {}, 2});
        TestTrue(FString(Case.What) + TEXT(": resolution uses effective AC"), Result.TargetArmorClass == 12 + Case.Bonus && Result.CoverBonus == Case.Bonus && Result.bHit == Same.bHit && Result.Damage == Same.Damage);
        TestEqual(FString(Case.What) + TEXT(": feedback names cover only when it applies"), Result.Feedback.Contains(TEXT("cover")), Case.Bonus > 0);
    }
    FCombatBattle Hidden;
    TestTrue(TEXT("Total cover start"), Hidden.Start(CoverDuel({4, 0}, {{2, 0}}, 0), 7, Error));
    TestFalse(TEXT("Total cover: no line of sight, not targetable"), Hidden.LegalTargets(1, ECombatCommandType::Attack).Contains(2));
    // Hit chance moves by exactly two faces at AC 12 -> 14 with +0 to hit.
    FCombatBattle Half;
    TestTrue(TEXT("Half start"), Half.Start(CoverDuel({4, 0}, {{3, 1}}, 0), 7, Error));
    const FActionForecast HalfForecast = Half.Forecast(1, ECombatCommandType::Attack, 2);
    TestTrue(TEXT("Half cover: 7 of 20 faces hit"), HalfForecast.RollMode != ERollMode::Normal || HalfForecast.HitChanceOutOf400 == 7 * 20);
    return !HasAnyErrors();
}
#endif
