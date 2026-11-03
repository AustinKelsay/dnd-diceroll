#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Rules/HexGrid.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHexGridGeometryTest, "Project.Expedition.Rules.HexGrid.Geometry",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FHexGridGeometryTest::RunTest(const FString& Parameters)
{
    const FIntPoint O(0, 0);
    TestEqual(TEXT("Neighbor distance"), FHexGrid::Distance(O, FIntPoint(1, -1)), 1);
    TestEqual(TEXT("Axial (1,1) is two steps"), FHexGrid::Distance(O, FIntPoint(1, 1)), 2);
    TestEqual(TEXT("Distance symmetric"), FHexGrid::Distance(FIntPoint(3, -7), FIntPoint(-2, 4)), FHexGrid::Distance(FIntPoint(-2, 4), FIntPoint(3, -7)));
    TestEqual(TEXT("(3,-7) to (-2,4)"), FHexGrid::Distance(FIntPoint(3, -7), FIntPoint(-2, 4)), 11);
    TestEqual(TEXT("Six neighbors"), FHexGrid::Neighbors(O).Num(), 6);
    bool bAllAdjacent = true;
    for (const FIntPoint& N : FHexGrid::Neighbors(FIntPoint(4, 2))) { bAllAdjacent &= FHexGrid::Adjacent(N, FIntPoint(4, 2)); }
    TestTrue(TEXT("Neighbors adjacent"), bAllAdjacent);
    for (int32 R = 0; R <= 6; ++R)
    {
        TestEqual(FString::Printf(TEXT("Range %d has 1+3R(R+1) cells"), R), FHexGrid::Range(FIntPoint(2, -1), R).Num(), 1 + 3 * R * (R + 1));
        const TArray<FIntPoint> Ring = FHexGrid::Ring(FIntPoint(2, -1), R);
        TestEqual(FString::Printf(TEXT("Ring %d size"), R), Ring.Num(), R == 0 ? 1 : 6 * R);
        bool bOnRing = true; TSet<FIntPoint> Unique;
        for (const FIntPoint& C : Ring) { bOnRing &= FHexGrid::Distance(C, FIntPoint(2, -1)) == R; Unique.Add(C); }
        TestTrue(FString::Printf(TEXT("Ring %d distinct and exact"), R), bOnRing && Unique.Num() == Ring.Num());
    }
    TestEqual(TEXT("Rotate E -> NE"), FHexGrid::Rotate(FIntPoint(1, 0), 1), FIntPoint(1, -1));
    TestEqual(TEXT("Rotate six times is identity"), FHexGrid::Rotate(FIntPoint(3, -1), 6), FIntPoint(3, -1));
    TestEqual(TEXT("Rotate keeps length"), FHexGrid::Length(FHexGrid::Rotate(FIntPoint(3, -1), 2)), 3);
    TestEqual(TEXT("Rotate -1 == 5"), FHexGrid::Rotate(FIntPoint(2, 1), -1), FHexGrid::Rotate(FIntPoint(2, 1), 5));
    for (int32 I = 0; I < 6; ++I)
    {
        TestEqual(TEXT("Direction is its own nearest"), FHexGrid::NearestDirection(FHexGrid::Directions[I] * 3), I);
        TestEqual(TEXT("Directions rotate in order"), FHexGrid::Rotate(FHexGrid::Directions[I], 1), FHexGrid::Direction(I + 1));
    }
    TestEqual(TEXT("Zero has no direction"), FHexGrid::NearestDirection(O), (int32)INDEX_NONE);
    TestEqual(TEXT("(3,-1) leans E"), FHexGrid::NearestDirection(FIntPoint(3, -1)), 0);
    // Lines: Distance + 1 cells, each step adjacent.
    const TArray<FIntPoint> Line = FHexGrid::Line(FIntPoint(0, 0), FIntPoint(4, -3));
    TestEqual(TEXT("Line length"), Line.Num(), 5);
    bool bStepwise = Line[0] == FIntPoint(0, 0) && Line.Last() == FIntPoint(4, -3);
    for (int32 I = 1; I < Line.Num(); ++I) { bStepwise &= FHexGrid::Adjacent(Line[I - 1], Line[I]); }
    TestTrue(TEXT("Line steps hex by hex"), bStepwise);
    TestEqual(TEXT("Straight line along E"), FHexGrid::Line(O, FIntPoint(3, 0)), TArray<FIntPoint>({FIntPoint(0, 0), FIntPoint(1, 0), FIntPoint(2, 0), FIntPoint(3, 0)}));
    // Edge-aligned line (0,0)->(1,1): passes between (1,0) and (0,1); the bias picks the side.
    const TArray<FIntPoint> Plus = FHexGrid::Line(O, FIntPoint(1, 1), 1), Minus = FHexGrid::Line(O, FIntPoint(1, 1), -1);
    TestTrue(TEXT("Edge line sides differ"), Plus.Num() == 3 && Minus.Num() == 3 && Plus[1] != Minus[1] && FHexGrid::Adjacent(Plus[1], O) && FHexGrid::Adjacent(Minus[1], O));
    // World mapping round trip and picking near the edge.
    bool bRound = true;
    for (const FIntPoint& C : FHexGrid::Range(FIntPoint(3, 4), 5))
    {
        bRound &= FHexGrid::FromWorld(FHexGrid::ToWorld(C, 100.0), 100.0) == C;
        bRound &= FHexGrid::FromWorld(FHexGrid::ToWorld(C, 100.0) + FVector2D(40.0, -30.0), 100.0) == C;
    }
    TestTrue(TEXT("World round trip"), bRound);
    TestEqual(TEXT("E neighbor sits sqrt3 * size east"), FHexGrid::ToWorld(FIntPoint(1, 0), 10.0).X, 10.0 * UE_DOUBLE_SQRT_3, 1e-6);
    // Offset boards.
    TestEqual(TEXT("Offset round trip"), FHexGrid::ToOffset(FHexGrid::FromOffset(5, 7)), FIntPoint(5, 7));
    TestEqual(TEXT("Odd row axial"), FHexGrid::FromOffset(2, 3), FIntPoint(1, 3));
    TestEqual(TEXT("Board 16x11 cells"), FHexGrid::BoardCells(16, 11).Num(), 176);
    bool bInside = true;
    for (const FIntPoint& C : FHexGrid::BoardCells(16, 11)) { bInside &= FHexGrid::OnBoard(C, 16, 11); }
    TestTrue(TEXT("Board cells are on board"), bInside && !FHexGrid::OnBoard(FHexGrid::FromOffset(16, 0), 16, 11) && !FHexGrid::OnBoard(FHexGrid::FromOffset(0, -1), 16, 11));
    TestTrue(TEXT("Row 4 starts at q = -2"), FHexGrid::OnBoard(FIntPoint(-2, 4), 16, 11) && !FHexGrid::OnBoard(FIntPoint(-3, 4), 16, 11));
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHexGridReachTest, "Project.Expedition.Rules.HexGrid.Reachable",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FHexGridReachTest::RunTest(const FString& Parameters)
{
    const TMap<FIntPoint, int32> Open = FHexGrid::Reachable(FIntPoint(0, 0), 3, [](FIntPoint, FIntPoint) { return 1; });
    TestEqual(TEXT("Open field: 3 steps reach 37 hexes"), Open.Num(), 37);
    bool bExact = true;
    for (const auto& E : Open) { bExact &= E.Value == FHexGrid::Distance(FIntPoint(0, 0), E.Key); }
    TestTrue(TEXT("Open cost equals hex distance"), bExact);
    // A wall of the three east-side neighbors forces a detour.
    const TSet<FIntPoint> Wall = {FIntPoint(1, 0), FIntPoint(1, -1), FIntPoint(0, 1)};
    const TMap<FIntPoint, int32> Around = FHexGrid::Reachable(FIntPoint(0, 0), 6, [&](FIntPoint, FIntPoint To) { return Wall.Contains(To) ? 0 : 1; });
    TestFalse(TEXT("Wall cells unreachable"), Around.Contains(FIntPoint(1, 0)));
    TestEqual(TEXT("Detour to (2,0) costs 5"), Around.FindRef(FIntPoint(2, 0)), 5);
    // Difficult terrain doubles.
    const TMap<FIntPoint, int32> Mud = FHexGrid::Reachable(FIntPoint(0, 0), 4, [](FIntPoint, FIntPoint To) { return To.X == 1 ? 2 : 1; });
    TestEqual(TEXT("Entering difficult costs 2"), Mud.FindRef(FIntPoint(1, 0)), 2);
    TestEqual(TEXT("Around mud to (2,-1)"), Mud.FindRef(FIntPoint(2, -1)), 3);
    TestFalse(TEXT("Budget respected"), Mud.Contains(FIntPoint(5, 0)));
    return !HasAnyErrors();
}
#endif
