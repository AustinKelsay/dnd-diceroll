#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Combat/CombatBattle.h"
#include "Rules/MonsterRules.h"

namespace CoverTacticsFixtures
{
// Two heroes (ids 1, 3; slow initiative) against one foe of Role (id 2, acts first) on the tutorial board.
FCombatEncounter TacticsDuel(const FString& Role, FIntPoint FoeCell, FIntPoint HeroA, FIntPoint HeroB, const TArray<FIntPoint>& Walls)
{
    const FCombatEncounter Tutorial = FCombatBattle::TutorialEncounter();
    FCombatEncounter Layout = Tutorial;
    Layout.BlockedCells = Walls;
    Layout.Combatants = {Tutorial.Combatants[3], Tutorial.Combatants[6], Tutorial.Combatants[2]};
    FCombatantState& A = Layout.Combatants[0];
    A.Id = 1; A.Cell = HeroA; A.InitiativeBonus = -10; A.MaxHP = A.HP = 200;
    FCombatantState& B = Layout.Combatants[2];
    B.Id = 3; B.Cell = HeroB; B.InitiativeBonus = -10; B.MaxHP = B.HP = 200;
    FCombatantState& Foe = Layout.Combatants[1];
    Foe.Id = 2; Foe.Role = Role; Foe.Name = Role; Foe.Cell = FoeCell; Foe.InitiativeBonus = 30; Foe.MaxHP = Foe.HP = 50; Foe.Range = 4;
    return Layout;
}
FIntPoint CellOf(const FCombatBattle& Battle, int32 Id) { return Battle.Find(Id)->Cell; }
int32 WorstCover(const FCombatBattle& Battle, FIntPoint Cell)
{
    return FMath::Min(Battle.CoverBetween(CellOf(Battle, 1), Cell), Battle.CoverBetween(CellOf(Battle, 3), Cell));
}
int32 AttackedId(const TArray<FCombatResult>& Results)
{
    for (const FCombatResult& R : Results) { if (R.bAccepted && !R.AttackRolls.IsEmpty()) { return R.TargetId; } }
    return INDEX_NONE;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCoverTacticsGateTest, "Project.Expedition.Rules.CoverTacticsGate",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FCoverTacticsGateTest::RunTest(const FString& Parameters)
{
    TestTrue(TEXT("Scout (INT 11) uses cover"), FMonsterRules::UsesCoverTactics(TEXT("Scout")));
    TestTrue(TEXT("Bandit (INT 10) uses cover"), FMonsterRules::UsesCoverTactics(TEXT("Bandit")));
    TestFalse(TEXT("Legacy Raider (no stat block) keeps the plain rush"), FMonsterRules::UsesCoverTactics(TEXT("Raider")));
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCoverTacticsAiTest, "Project.Expedition.Rules.CoverTacticsAI",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FCoverTacticsAiTest::RunTest(const FString& Parameters)
{
    using namespace CoverTacticsFixtures;
    // Foe at (0,0); hero 1 at (4,0) behind the (3,1) wall (half cover), hero 3 at (0,4) in the open.
    const TArray<FIntPoint> Walls = {{3, 1}};
    FString Error;
    FCombatBattle Smart, SmartAgain, Plain;
    if (!TestTrue(TEXT("starts"), Smart.Start(TacticsDuel(TEXT("Scout"), {0, 0}, {4, 0}, {0, 4}, Walls), 7, Error) &&
        SmartAgain.Start(TacticsDuel(TEXT("Scout"), {0, 0}, {4, 0}, {0, 4}, Walls), 7, Error) &&
        Plain.Start(TacticsDuel(TEXT("Raider"), {0, 0}, {4, 0}, {0, 4}, Walls), 7, Error))) { return false; }
    if (!TestEqual(TEXT("foe acts first"), Smart.ActiveId(), 2)) { return false; }
    TestTrue(TEXT("Precondition: hero 1 has more cover than hero 3"), Smart.CoverBonus(2, 1) > Smart.CoverBonus(2, 3));
    const int32 StartCover = WorstCover(Smart, {0, 0});
    const TArray<FCombatResult> SmartTurn = Smart.PlayEnemyTurn();
    const TArray<FCombatResult> PlainTurn = Plain.PlayEnemyTurn();
    SmartAgain.PlayEnemyTurn();
    TestEqual(TEXT("Thinking foe shoots the exposed hero"), AttackedId(SmartTurn), 3);
    TestEqual(TEXT("Mindless foe keeps roster order"), AttackedId(PlainTurn), 1);
    TestTrue(TEXT("Thinking foe never ends with less cover than it started"), WorstCover(Smart, CellOf(Smart, 2)) >= StartCover);
    TestEqual(TEXT("Mindless foe does not reposition after shooting"), CellOf(Plain, 2), FIntPoint(0, 0));
    TestTrue(TEXT("Deterministic under the same seed"), FCombatSnapshot::StaticStruct()->CompareScriptStruct(&Smart.Snapshot(), &SmartAgain.Snapshot(), 0));
    AddInfo(FString::Printf(TEXT("Scout ended at %s with cover %d (started %d)"), *CellOf(Smart, 2).ToString(), WorstCover(Smart, CellOf(Smart, 2)), StartCover));

    // Approach: from out of range, the thinking foe picks a firing cell with at least the cover the plain one gets.
    const TArray<FIntPoint> Maze = {{3, 3}, {4, 3}, {3, 4}, {5, 5}};
    FCombatBattle Far, FarPlain;
    if (!TestTrue(TEXT("far starts"), Far.Start(TacticsDuel(TEXT("Scout"), {7, 7}, {0, 0}, {1, 0}, Maze), 7, Error) &&
        FarPlain.Start(TacticsDuel(TEXT("Raider"), {7, 7}, {0, 0}, {1, 0}, Maze), 7, Error))) { return false; }
    Far.PlayEnemyTurn(); FarPlain.PlayEnemyTurn();
    TestTrue(TEXT("Approach: thinking foe ends in at least as much cover"), WorstCover(Far, CellOf(Far, 2)) >= WorstCover(FarPlain, CellOf(FarPlain, 2)));

    // Dex saves: cover from the area's origin.
    TestEqual(TEXT("CoverBetween is zero on the same cell"), Smart.CoverBetween({2, 2}, {2, 2}), 0);
    TestEqual(TEXT("CoverBetween matches attack cover with no creatures"), Smart.CoverBetween({0, 0}, {4, 0}), 2);
    return !HasAnyErrors();
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCoverTacticsMoveTest, "Project.Expedition.Rules.CoverTacticsMove",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FCoverTacticsMoveTest::RunTest(const FString& Parameters)
{
    using namespace CoverTacticsFixtures;
    // Scout at (6,0) in the open, heroes at (4,7) and (5,7), a wall at (3..5,4): it must leave the exposed hex and shoot from cover.
    const TArray<FIntPoint> Walls = {{3, 4}, {4, 4}, {5, 4}};
    FString Error;
    FCombatBattle Smart, Plain;
    if (!TestTrue(TEXT("starts"), Smart.Start(TacticsDuel(TEXT("Scout"), {6, 0}, {4, 7}, {5, 7}, Walls), 7, Error) &&
        Plain.Start(TacticsDuel(TEXT("Raider"), {6, 0}, {4, 7}, {5, 7}, Walls), 7, Error))) { return false; }
    if (!TestEqual(TEXT("foe acts first"), Smart.ActiveId(), 2)) { return false; }
    TestEqual(TEXT("Precondition: start hex is exposed to both heroes"), WorstCover(Smart, {6, 0}), 0);
    const TArray<FCombatResult> Turn = Smart.PlayEnemyTurn();
    Plain.PlayEnemyTurn();
    const FIntPoint End = CellOf(Smart, 2);
    TestNotEqual(TEXT("Thinking foe leaves the exposed hex"), End, FIntPoint(6, 0));
    TestTrue(TEXT("Thinking foe ends in cover against both heroes"), WorstCover(Smart, End) > 0);
    TestTrue(TEXT("Thinking foe still attacks"), AttackedId(Turn) != INDEX_NONE);
    TestEqual(TEXT("Mindless foe gets no cover"), WorstCover(Plain, CellOf(Plain, 2)), 0);
    AddInfo(FString::Printf(TEXT("Scout moved %s -> %s, cover %d"), *FIntPoint(6, 0).ToString(), *End.ToString(), WorstCover(Smart, End)));
    return !HasAnyErrors();
}
#endif