#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Rules/AreaRules.h"

namespace AreaTestFixtures
{
FAreaTemplate Make(EAreaShape Shape, FIntPoint Origin, FIntPoint Direction, int32 Feet)
{
    FAreaTemplate Area; Area.Shape = Shape; Area.Origin = Origin; Area.Direction = Direction; Area.SizeFeet = Feet;
    return Area;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAreaTemplatesTest, "Project.Expedition.Rules.AreaOfEffectTemplates",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FAreaTemplatesTest::RunTest(const FString& Parameters)
{
    using namespace AreaTestFixtures;
    // 15-ft cone east: 1 cell, then 3, then 3.
    const TArray<FIntPoint> Cone = FAreaRules::Cells(Make(EAreaShape::Cone, {0, 0}, {1, 0}, 15));
    TestEqual(TEXT("Cone 15 ft east: 7 cells"), Cone.Num(), 7);
    TestTrue(TEXT("Cone widens"), Cone.Contains(FIntPoint(1, 0)) && !Cone.Contains(FIntPoint(1, 1)) && Cone.Contains(FIntPoint(2, 1)) && Cone.Contains(FIntPoint(3, -1)) && !Cone.Contains(FIntPoint(4, 0)));
    TestFalse(TEXT("Cone excludes the caster"), Cone.Contains(FIntPoint(0, 0)));
    TestEqual(TEXT("Cone 15 ft diagonal: 6 cells"), FAreaRules::Cells(Make(EAreaShape::Cone, {0, 0}, {1, 1}, 15)).Num(), 6);
    // 15-ft cube (Thunderwave) east: 3x3 in front, centered on the caster's row.
    const TArray<FIntPoint> Cube = FAreaRules::Cells(Make(EAreaShape::Cube, {2, 2}, {1, 0}, 15));
    TestEqual(TEXT("Cube 15 ft: 9 cells"), Cube.Num(), 9);
    TestTrue(TEXT("Cube spans x 3-5, y 1-3"), Cube.Contains(FIntPoint(3, 1)) && Cube.Contains(FIntPoint(5, 3)) && !Cube.Contains(FIntPoint(2, 2)) && !Cube.Contains(FIntPoint(6, 2)));
    TestEqual(TEXT("Cube 10 ft north: 4 cells"), FAreaRules::Cells(Make(EAreaShape::Cube, {0, 0}, {0, -1}, 10)).Num(), 4);
    // Line 30 ft x 5 ft.
    const TArray<FIntPoint> Line = FAreaRules::Cells(Make(EAreaShape::Line, {0, 0}, {1, 0}, 30));
    TestEqual(TEXT("Line 30 ft east: 6 cells"), Line.Num(), 6);
    TestTrue(TEXT("Line runs 1-6"), Line.Contains(FIntPoint(1, 0)) && Line.Contains(FIntPoint(6, 0)) && !Line.Contains(FIntPoint(7, 0)) && !Line.Contains(FIntPoint(3, 1)));
    TestEqual(TEXT("Line 30 ft diagonal: 4 cells (true length)"), FAreaRules::Cells(Make(EAreaShape::Line, {0, 0}, {1, 1}, 30)).Num(), 4);
    // Sphere / cylinder radius 10 ft at an intersection.
    const TArray<FIntPoint> Sphere = FAreaRules::Cells(Make(EAreaShape::Sphere, {5, 5}, {1, 0}, 10));
    TestEqual(TEXT("Sphere 10 ft: 12 cells"), Sphere.Num(), 12);
    TestTrue(TEXT("Sphere symmetric around the corner"), Sphere.Contains(FIntPoint(5, 5)) && Sphere.Contains(FIntPoint(4, 4)) && Sphere.Contains(FIntPoint(3, 4)) && !Sphere.Contains(FIntPoint(3, 3)));
    TestEqual(TEXT("Sphere 20 ft: 52 cells"), FAreaRules::Cells(Make(EAreaShape::Sphere, {0, 0}, {1, 0}, 20)).Num(), 52);
    TestEqual(TEXT("Cylinder uses the sphere footprint"), FAreaRules::Cells(Make(EAreaShape::Cylinder, {0, 0}, {1, 0}, 10)), FAreaRules::Cells(Make(EAreaShape::Sphere, {0, 0}, {1, 0}, 10)));
    // Preview equals resolution: Covers agrees with Cells everywhere nearby.
    const FAreaTemplate Mixed = Make(EAreaShape::Line, {1, 1}, {2, 1}, 30);
    const TArray<FIntPoint> MixedCells = FAreaRules::Cells(Mixed);
    bool bAgree = MixedCells.Num() > 0;
    for (int32 X = -10; X <= 12; ++X) { for (int32 Y = -10; Y <= 12; ++Y) { bAgree &= FAreaRules::Covers(Mixed, FIntPoint(X, Y)) == MixedCells.Contains(FIntPoint(X, Y)); } }
    TestTrue(TEXT("Covers matches Cells"), bAgree);
    TestEqual(TEXT("Zero aim covers nothing"), FAreaRules::Cells(Make(EAreaShape::Cone, {0, 0}, {0, 0}, 15)).Num(), 0);
    return !HasAnyErrors();
}
#endif
