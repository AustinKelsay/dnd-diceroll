#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Rules/AreaRules.h"
#include "Rules/HexGrid.h"

namespace AreaTestFixtures
{
/** Fixtures are written in axial hexes; FAreaRules takes board cells, so convert on the way in and out. */
FAreaTemplate Make(EAreaShape Shape, FIntPoint Origin, FIntPoint Direction, int32 Feet)
{
    FAreaTemplate Area; Area.Shape = Shape; Area.SizeFeet = Feet;
    Area.Origin = FHexGrid::Board(Origin);
    Area.Direction = FHexGrid::Board(Origin + Direction) - Area.Origin;
    return Area;
}
TArray<FIntPoint> Hexes(const FAreaTemplate& Area)
{
    TArray<FIntPoint> Out = FAreaRules::Cells(Area);
    for (FIntPoint& C : Out) { C = FHexGrid::Axial(C); }
    Out.Sort([](const FIntPoint& A, const FIntPoint& B) { return A.X != B.X ? A.X < B.X : A.Y < B.Y; });
    return Out;
}
bool Hit(const FAreaTemplate& Area, FIntPoint Axial) { return FAreaRules::Covers(Area, FHexGrid::Board(Axial)); }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAreaTemplatesTest, "Project.Expedition.Rules.AreaOfEffectTemplates",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FAreaTemplatesTest::RunTest(const FString& Parameters)
{
    using namespace AreaTestFixtures;
    // 15-ft cone east: 1 hex, then 3, then 3 (60-degree wedge, edges inclusive).
    const TArray<FIntPoint> Cone = Hexes(Make(EAreaShape::Cone, {0, 0}, {1, 0}, 15));
    TestEqual(TEXT("Cone 15 ft east: 7 hexes"), Cone, TArray<FIntPoint>({{1, 0}, {1, 1}, {2, -1}, {2, 0}, {2, 1}, {3, -1}, {3, 0}}));
    TestFalse(TEXT("Cone excludes the caster"), Hit(Make(EAreaShape::Cone, {0, 0}, {1, 0}, 15), FIntPoint(0, 0)));
    TestEqual(TEXT("Cone 15 ft NE"), Hexes(Make(EAreaShape::Cone, {0, 0}, {1, -1}, 15)), TArray<FIntPoint>({{1, -2}, {1, -1}, {2, -3}, {2, -2}, {2, -1}, {3, -3}, {3, -2}}));
    TestEqual(TEXT("Cone 30 ft: 24 hexes"), Hexes(Make(EAreaShape::Cone, {4, 4}, {-2, 0}, 30)).Num(), 24);
    bool bSix = true;
    for (int32 Dir = 0; Dir < 6; ++Dir) { bSix &= Hexes(Make(EAreaShape::Cone, {0, 0}, FHexGrid::Direction(Dir), 15)).Num() == 7; }
    TestTrue(TEXT("Cone is the same size in all six directions"), bSix);
    TestEqual(TEXT("Off-axis aim snaps to the nearest direction"), Hexes(Make(EAreaShape::Cone, {0, 0}, {3, -1}, 15)), Cone);
    // 15-ft cube (Thunderwave) east: a 7-hex blob touching the caster, 3 hexes deep.
    const TArray<FIntPoint> Cube = Hexes(Make(EAreaShape::Cube, {2, 2}, {1, 0}, 15));
    TestEqual(TEXT("Cube 15 ft: 7 hexes around (4,2)"), Cube, TArray<FIntPoint>({{3, 2}, {3, 3}, {4, 1}, {4, 2}, {4, 3}, {5, 1}, {5, 2}}));
    TestEqual(TEXT("Cube 10 ft rounds up to 7 hexes"), Hexes(Make(EAreaShape::Cube, {0, 0}, {0, -1}, 10)).Num(), 7);
    TestEqual(TEXT("Cube 5 ft is one hex"), Hexes(Make(EAreaShape::Cube, {0, 0}, {0, 1}, 5)), TArray<FIntPoint>({{0, 1}}));
    TestEqual(TEXT("Cube 25 ft: 19 hexes"), Hexes(Make(EAreaShape::Cube, {0, 0}, {1, 0}, 25)).Num(), 19);
    // Line 30 ft x 5 ft: one hex wide.
    const TArray<FIntPoint> Line = Hexes(Make(EAreaShape::Line, {0, 0}, {1, 0}, 30));
    TestEqual(TEXT("Line 30 ft east: 6 hexes in a row"), Line, TArray<FIntPoint>({{1, 0}, {2, 0}, {3, 0}, {4, 0}, {5, 0}, {6, 0}}));
    for (const FIntPoint Aim : {FIntPoint(1, 1), FIntPoint(2, -5), FIntPoint(-3, 1), FIntPoint(0, 7)})
    {
        const TArray<FIntPoint> Slanted = Hexes(Make(EAreaShape::Line, {0, 0}, Aim, 30));
        bool bOneWide = Slanted.Num() == 6;
        TSet<int32> Steps;
        for (const FIntPoint& C : Slanted) { Steps.Add(FHexGrid::Distance(C, FIntPoint(0, 0))); }
        bOneWide &= Steps.Num() == 6 && !Steps.Contains(0) && !Steps.Contains(7);
        TestTrue(FString::Printf(TEXT("Line toward (%d,%d): 6 hexes, one per step"), Aim.X, Aim.Y), bOneWide);
    }
    // Sphere / cylinder: every hex within the radius, 1 + 3N(N+1).
    TestEqual(TEXT("Sphere 5 ft: 7 hexes"), Hexes(Make(EAreaShape::Sphere, {5, 5}, {1, 0}, 5)).Num(), 7);
    TestEqual(TEXT("Sphere 10 ft: 19 hexes"), Hexes(Make(EAreaShape::Sphere, {5, 5}, {1, 0}, 10)).Num(), 19);
    TestEqual(TEXT("Sphere 15 ft: 37 hexes"), Hexes(Make(EAreaShape::Sphere, {0, 0}, {1, 0}, 15)).Num(), 37);
    TestEqual(TEXT("Sphere 20 ft (Fireball): 61 hexes"), Hexes(Make(EAreaShape::Sphere, {0, 0}, {1, 0}, 20)).Num(), 61);
    TestTrue(TEXT("Sphere includes its center"), Hit(Make(EAreaShape::Sphere, {5, 5}, {1, 0}, 10), FIntPoint(5, 5)));
    TestFalse(TEXT("Sphere 10 ft stops at 2 steps"), Hit(Make(EAreaShape::Sphere, {5, 5}, {1, 0}, 10), FIntPoint(6, 6) + FIntPoint(1, 0)));
    TestEqual(TEXT("Cylinder uses the sphere footprint"), Hexes(Make(EAreaShape::Cylinder, {0, 0}, {1, 0}, 10)), Hexes(Make(EAreaShape::Sphere, {0, 0}, {1, 0}, 10)));
    // Preview equals resolution: Covers agrees with Cells everywhere nearby, for every shape.
    for (const EAreaShape Shape : {EAreaShape::Cone, EAreaShape::Cube, EAreaShape::Cylinder, EAreaShape::Line, EAreaShape::Sphere})
    {
        const FAreaTemplate Mixed = Make(Shape, {1, 1}, {2, 1}, 30);
        const TArray<FIntPoint> MixedCells = Hexes(Mixed);
        bool bAgree = MixedCells.Num() > 0;
        for (int32 X = -12; X <= 14; ++X) { for (int32 Y = -12; Y <= 14; ++Y) { bAgree &= Hit(Mixed, FIntPoint(X, Y)) == MixedCells.Contains(FIntPoint(X, Y)); } }
        TestTrue(FString::Printf(TEXT("Covers matches Cells (shape %d)"), (int32)Shape), bAgree);
    }
    TestEqual(TEXT("Zero aim covers nothing"), Hexes(Make(EAreaShape::Cone, {0, 0}, {0, 0}, 15)).Num(), 0);
    TestEqual(TEXT("Zero aim line covers nothing"), Hexes(Make(EAreaShape::Line, {0, 0}, {0, 0}, 15)).Num(), 0);
    return !HasAnyErrors();
}
#endif
