#if WITH_DEV_AUTOMATION_TESTS
#include "ExpeditionInputHarness.h"
#include "Components/LightComponent.h"
#include "Presentation/BoardMetric.h"
#include "Presentation/BrazierRing.h"
#include "Presentation/VigilKit.h"

/**
 * The brazier ring (#148): braziers stand around the board perimeter, one every four cells along each edge, outside the
 * playable edge, so their count follows the board and is the same on every world. The plan is pure and tested without
 * assets (deterministic, the formula for 16 x 11 and 18 x 12, outside the footprint, scatter kept clear); the board-level
 * checks need the shipped theme and say so when it is missing, and check the light budget (at most two shadowed local
 * lights) and that none of it reaches rules, snapshots or checkpoints.
 */
namespace
{
UWorld* MakeBrazierWorld()
{
    const UWorld::InitializationValues Values = UWorld::InitializationValues()
        .AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false);
    return UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
}

// Release the world like the shared harness does, or later tests trip the zero-warning gate on RHI reserved memory.
void DestroyBrazierWorld(UWorld* World)
{
    World->DestroyWorld(false);
    FlushRenderingCommands();
    CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
}

/** A battle board on the first route offer that is (or is not) dangerous; null when the run offers no such road. */
AExpeditionBattleView* StartBrazierBoard(UWorld* World, const FString& Directory, bool bDangerous, bool bPrimitives = false)
{
    AExpeditionBattleView* View = World->SpawnActor<AExpeditionBattleView>();
    if (!View) { return nullptr; }
    View->ConfigureStorage(FPaths::Combine(Directory, TEXT("current.json")));
    View->DispatchBeginPlay();
    if (bPrimitives) { View->UsePrimitives(); }
    View->NewRun();
    int32 Offer = INDEX_NONE;
    const TArray<FExpeditionRoute>& Offers = View->Progress().Offers;
    for (int32 Index = 0; Index < Offers.Num() && Offer == INDEX_NONE; ++Index) { if (Offers[Index].bDangerous == bDangerous) { Offer = Index; } }
    if (Offer == INDEX_NONE) { return nullptr; }
    View->ChooseRoute(Offer);
    View->HoldEnemyTurns(true);
    View->Tick(0.016f);
    return View;
}

/** The saved checkpoint with the run's fresh identity masked: the one field that differs between runs by design. */
FString Checkpoint(AExpeditionBattleView& View, const FString& Directory)
{
    View.Save();
    FString Text;
    FFileHelper::LoadFileToString(Text, *FPaths::Combine(Directory, TEXT("current.json")));
    const FString RunId = View.Progress().RunId;
    if (!RunId.IsEmpty()) { Text.ReplaceInline(*RunId, TEXT("<run-id>"), ESearchCase::CaseSensitive); }
    return Text;
}

bool SameCells(const TArray<FVector2D>& A, const TArray<BrazierRing::FBrazier>& B)
{
    if (A.Num() != B.Num()) { return false; }
    for (int32 Index = 0; Index < A.Num(); ++Index) { if (!A[Index].Equals(B[Index].Cell, 0.0001)) { return false; } }
    return true;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBrazierRingPlanTest, "Project.Expedition.Presentation.BrazierRingPlan",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FBrazierRingPlanTest::RunTest(const FString& Parameters)
{
    using namespace BrazierRing;
    TestEqual(TEXT("A 16-cell edge has 4 braziers"), EdgeCount(16), 4);
    TestEqual(TEXT("An 18-cell edge has 4 braziers"), EdgeCount(18), 4);
    TestEqual(TEXT("A 12-cell edge has 3 braziers"), EdgeCount(12), 3);
    TestEqual(TEXT("An 11-cell edge has 2 braziers"), EdgeCount(11), 2);
    TestEqual(TEXT("A very short edge still has the minimum"), EdgeCount(3), MinPerEdge);
    TestEqual(TEXT("16 x 11 has 12 braziers (2 x (4 + 2))"), Count(16, 11), 12);
    TestEqual(TEXT("18 x 12 has 14 braziers (2 x (4 + 3))"), Count(18, 12), 14);
    TestEqual(TEXT("Each brazier carries an equal share of the budget"), Intensity(7) * 7.0f, TotalIntensity, 0.001f);
    TestEqual(TEXT("Seven braziers are the 40 cd of #143"), Intensity(7), 40.0f, 0.001f);
    TestTrue(TEXT("More braziers are dimmer, so the overall level holds"), Intensity(14) < Intensity(12));
    TestEqual(TEXT("14 braziers total the budget"), Intensity(14) * 14.0f, TotalIntensity, 0.001f);

    int32 SeedsThatDiffer = 0;
    for (const FIntPoint Board : {FIntPoint(16, 11), FIntPoint(18, 12), FIntPoint(17, 12), FIntPoint(8, 8)})
    {
        const FString Name = FString::Printf(TEXT("%d x %d"), Board.X, Board.Y);
        for (const int32 Seed : {1, 7, 4242, -9})
        {
            const TArray<FBrazier> Ring = Plan(Board.X, Board.Y, Seed);
            TestEqual(FString::Printf(TEXT("%s seed %d: the count is the formula"), *Name, Seed), Ring.Num(), Count(Board.X, Board.Y));
            TArray<FBrazier> Again = Plan(Board.X, Board.Y, Seed);
            bool bSame = Again.Num() == Ring.Num();
            for (int32 Index = 0; bSame && Index < Ring.Num(); ++Index) { bSame = Ring[Index].Cell == Again[Index].Cell && Ring[Index].Edge == Again[Index].Edge; }
            TestTrue(FString::Printf(TEXT("%s seed %d: the plan is deterministic"), *Name, Seed), bSame);

            TMap<int32, TArray<float>> Along;
            for (const FBrazier& Brazier : Ring)
            {
                const bool bAlongX = Brazier.Edge == EEdge::South || Brazier.Edge == EEdge::North;
                const int32 Length = bAlongX ? Board.X : Board.Y;
                const float OnEdge = static_cast<float>(bAlongX ? Brazier.Cell.X : Brazier.Cell.Y);
                const float Across = static_cast<float>(bAlongX ? Brazier.Cell.Y : Brazier.Cell.X);
                const float Wanted = Brazier.Edge == EEdge::South || Brazier.Edge == EEdge::West ? -0.5f - OutsideCells
                    : static_cast<float>(bAlongX ? Board.Y : Board.X) - 0.5f + OutsideCells;
                TestEqual(TEXT("A brazier stands exactly 1.25 cells outside the playable edge"), Across, Wanted, 0.0001f);
                TestTrue(TEXT("A brazier is outside the playable footprint, beyond the occluder margin and the overlay gutter"), OutsideBoard(Brazier.Cell, Board.X, Board.Y, 1.0f));
                const FIntPoint Nearest(FMath::RoundToInt(static_cast<float>(Brazier.Cell.X)), FMath::RoundToInt(static_cast<float>(Brazier.Cell.Y)));
                TestTrue(TEXT("Its own cell is two or more rings out, like the kit's scatter"), VigilKit::RingDistance(Nearest, Board.X, Board.Y) >= VigilKit::ScatterMinRing);
                TestTrue(TEXT("It stays between the edge's two corners"), OnEdge >= -0.5f && OnEdge <= Length - 0.5f);
                const int32 Slots = EdgeCount(Length);
                const float Nominal = (Brazier.Slot + 0.5f) * static_cast<float>(Length) / Slots - 0.5f;
                TestTrue(TEXT("The seed moves it at most a quarter cell along the edge"), FMath::Abs(OnEdge - Nominal) <= JitterCells + 0.0001f);
                Along.FindOrAdd(static_cast<int32>(Brazier.Edge)).Add(OnEdge);
            }
            for (const TPair<int32, TArray<float>>& Edge : Along)
            {
                const int32 Length = Edge.Key < 2 ? Board.X : Board.Y;
                TestEqual(TEXT("Every edge has its count"), Edge.Value.Num(), EdgeCount(Length));
                const float Spacing = static_cast<float>(Length) / Edge.Value.Num();
                for (int32 Index = 1; Index < Edge.Value.Num(); ++Index)
                {
                    const float Gap = Edge.Value[Index] - Edge.Value[Index - 1];
                    TestTrue(TEXT("Neighbours keep the edge's spacing, give or take twice the jitter"), FMath::Abs(Gap - Spacing) <= 2.0f * JitterCells + 0.0001f);
                    TestTrue(TEXT("Neighbours are about four cells apart or more"), Gap >= 3.0f);
                }
            }
            if (Plan(Board.X, Board.Y, Seed + 1)[0].Cell != Ring[0].Cell || Plan(Board.X, Board.Y, Seed + 1)[1].Cell != Ring[1].Cell) { ++SeedsThatDiffer; }
        }
    }
    TestTrue(TEXT("Another seed jitters the ring somewhere"), SeedsThatDiffer > 0);
    TestTrue(TEXT("A taller board has more braziers"), Count(16, 15) > Count(16, 11));
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBrazierScatterClearTest, "Project.Expedition.Presentation.BrazierRingKeepsScatterClear",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FBrazierScatterClearTest::RunTest(const FString& Parameters)
{
    VigilKit::FDressingKinds Kinds;
    Kinds.Scatter = 8; Kinds.Mist = 3; Kinds.Shaft = 2;
    for (const FIntPoint Board : {FIntPoint(16, 11), FIntPoint(18, 12)})
    {
        for (const int32 Seed : {1, 7, 4242, -9})
        {
            const TArray<FVector2D> Avoid = BrazierRing::Cells(BrazierRing::Plan(Board.X, Board.Y, Seed));
            const TArray<VigilKit::FDressing> Free = VigilKit::PlanDressing(Board.X, Board.Y, Seed, Kinds);
            const TArray<VigilKit::FDressing> Clear = VigilKit::PlanDressing(Board.X, Board.Y, Seed, Kinds, Avoid, BrazierRing::ScatterClearCells);
            TestEqual(TEXT("Keeping clear of the braziers loses no piece"), Clear.Num(), Free.Num());
            const TArray<VigilKit::FDressing> NoAvoid = VigilKit::PlanDressing(Board.X, Board.Y, Seed, Kinds, TArray<FVector2D>(), 5.0f);
            bool bSame = NoAvoid.Num() == Free.Num();
            for (int32 Index = 0; bSame && Index < Free.Num(); ++Index) { bSame = NoAvoid[Index].Cell == Free[Index].Cell && NoAvoid[Index].Variant == Free[Index].Variant; }
            TestTrue(TEXT("With nothing to avoid the plan is the one #145 shipped"), bSame);
            TSet<FIntPoint> Cells;
            for (const VigilKit::FDressing& Item : Clear)
            {
                TestFalse(TEXT("No two pieces share a cell"), Cells.Contains(Item.Cell));
                Cells.Add(Item.Cell);
                if (Item.Kind != VigilKit::EDressingKind::Scatter) { continue; }
                TestTrue(TEXT("Scatter stays two or more rings out"), VigilKit::RingDistance(Item.Cell, Board.X, Board.Y) >= VigilKit::ScatterMinRing);
                for (const FVector2D& Brazier : Avoid)
                {
                    TestTrue(TEXT("A scatter piece keeps its distance from every brazier"),
                        FVector2D::Distance(Brazier, FVector2D(Item.Cell.X, Item.Cell.Y)) >= BrazierRing::ScatterClearCells);
                }
            }
        }
    }
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBrazierBoardTest, "Project.Expedition.Presentation.BrazierRingOnBoard",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FBrazierBoardTest::RunTest(const FString& Parameters)
{
    UWorld* World = MakeBrazierWorld();
    if (!TestNotNull(TEXT("Transient world"), World)) { return false; }
    const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation/BrazierRing"), FGuid::NewGuid().ToString());
    for (const bool bDangerous : {true, false})
    {
        const TCHAR* Name = bDangerous ? TEXT("Dangerous road") : TEXT("Guarded road");
        AExpeditionBattleView* View = StartBrazierBoard(World, FPaths::Combine(Directory, bDangerous ? TEXT("dangerous") : TEXT("guarded")), bDangerous);
        if (!TestNotNull(FString::Printf(TEXT("%s: the board starts"), Name), View)) { continue; }
        if (!View->UsesTheme()) { AddInfo(TEXT("The shipped theme is not available here; the board-level brazier checks were skipped.")); break; }
        const FCombatEncounter& Encounter = View->Rules().Snapshot().Encounter;
        const int32 Seed = View->Progress().History.IsEmpty() ? 7 : View->Progress().History.Last().Route.EncounterSeed;
        const TArray<BrazierRing::FBrazier> Ring = BrazierRing::Plan(Encounter.Width, Encounter.Height, Seed);
        TestEqual(FString::Printf(TEXT("%s: the board has the formula's braziers"), Name), View->PlacedBraziers().Num(), BrazierRing::Count(Encounter.Width, Encounter.Height));
        TestTrue(FString::Printf(TEXT("%s: they stand where the plan says"), Name), SameCells(View->PlacedBraziers(), Ring));
        for (const FVector2D& Cell : View->PlacedBraziers())
        {
            TestTrue(FString::Printf(TEXT("%s: a brazier is outside the playable footprint"), Name), BrazierRing::OutsideBoard(Cell, Encounter.Width, Encounter.Height, 1.0f));
        }

        // Light budget: gate 1 allows at most two shadowed local lights, and the braziers keep that cap.
        TestTrue(FString::Printf(TEXT("%s: at most %d local lights cast shadows"), Name, BrazierRing::MaxShadowed), View->ShadowedLightPositions().Num() <= BrazierRing::MaxShadowed);
        TestEqual(FString::Printf(TEXT("%s: the cap is used (the ring has more braziers than the cap)"), Name), View->ShadowedLightPositions().Num(), BrazierRing::MaxShadowed);
        TInlineComponentArray<ULightComponent*> Lights;
        View->GetComponents(Lights);
        AddInfo(FString::Printf(TEXT("%s %dx%d: %d braziers, %d light components owned, %d primitive components, %d instanced groups, %.2f cd per brazier"),
            Name, Encounter.Width, Encounter.Height, View->PlacedBraziers().Num(), Lights.Num(), View->PresentationComponentCount(), View->InstancedGroupCount(),
            BrazierRing::Intensity(View->PlacedBraziers().Num())));

        // The ring is the same at another cell size and after another rebuild, and the cap holds there too.
        const TArray<FVector2D> First = View->PlacedBraziers();
        const FBoardMetric Original = View->Metric();
        View->SetBoardMetric(FBoardMetric(180.0f, 190.0f));
        TestTrue(FString::Printf(TEXT("%s: the braziers are the same at another cell size"), Name), View->PlacedBraziers() == First);
        TestTrue(FString::Printf(TEXT("%s: the cap holds at another cell size"), Name), View->ShadowedLightPositions().Num() <= BrazierRing::MaxShadowed);
        View->SetBoardMetric(Original);
        TestTrue(FString::Printf(TEXT("%s: the braziers are the same after another rebuild"), Name), View->PlacedBraziers() == First);

        // Instanced: loose parts draw the same braziers, and the instanced board stays under the component budget (#144).
        if (View->UsesInstancedParts())
        {
            const int32 Parts = View->PresentationComponentCount();
            TestTrue(FString::Printf(TEXT("%s: the instanced board stays under 40 drawing components (%d)"), Name, Parts), Parts < 40);
            const int32 Props = View->LogicalPropCount();
            View->SetLooseParts(true);
            TestEqual(FString::Printf(TEXT("%s: loose parts place the same braziers"), Name), View->PlacedBraziers().Num(), First.Num());
            TestEqual(FString::Printf(TEXT("%s: and the same props"), Name), View->LogicalPropCount(), Props);
            View->SetLooseParts(false);
        }
    }
    DestroyBrazierWorld(World);
    IFileManager::Get().DeleteDirectory(*Directory, false, true);
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBrazierPresentationOnlyTest, "Project.Expedition.Presentation.BrazierRingLeavesStateUnchanged",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FBrazierPresentationOnlyTest::RunTest(const FString& Parameters)
{
    // The ring is presentation: the same seeded run under the themed look and under the primitives (no braziers), and the
    // themed board at another cell size, save the same checkpoint bytes and hold the same snapshot.
    UWorld* World = MakeBrazierWorld();
    if (!TestNotNull(TEXT("Transient world"), World)) { return false; }
    const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation/BrazierState"), FGuid::NewGuid().ToString());
    AExpeditionBattleView* Themed = StartBrazierBoard(World, FPaths::Combine(Directory, TEXT("themed")), true);
    AExpeditionBattleView* Primitive = StartBrazierBoard(World, FPaths::Combine(Directory, TEXT("primitive")), true, true);
    if (TestNotNull(TEXT("Themed board"), Themed) && TestNotNull(TEXT("Primitive board"), Primitive))
    {
        if (!Themed->UsesTheme()) { AddInfo(TEXT("The shipped theme is not available here; the primitive board is compared with itself.")); }
        TestTrue(TEXT("The primitive look places no braziers"), Primitive->PlacedBraziers().IsEmpty());
        const FString Before = Checkpoint(*Themed, FPaths::Combine(Directory, TEXT("themed")));
        TestFalse(TEXT("The themed run wrote a checkpoint"), Before.IsEmpty());
        TestTrue(TEXT("The checkpoint bytes match the primitive look's"), Before == Checkpoint(*Primitive, FPaths::Combine(Directory, TEXT("primitive"))));
        const FCombatSnapshot Battle = Themed->Rules().Snapshot();
        Themed->SetBoardMetric(FBoardMetric(180.0f, 190.0f));
        Themed->Tick(0.016f);
        TestTrue(TEXT("The checkpoint bytes match after the braziers move with the metric"), Before == Checkpoint(*Themed, FPaths::Combine(Directory, TEXT("themed"))));
        const FCombatSnapshot After = Themed->Rules().Snapshot();
        TestTrue(TEXT("The battle snapshot is identical"), FCombatSnapshot::StaticStruct()->CompareScriptStruct(&Battle, &After, 0));
    }
    DestroyBrazierWorld(World);
    IFileManager::Get().DeleteDirectory(*Directory, false, true);
    return !HasAnyErrors();
}
#endif
