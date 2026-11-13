#if WITH_DEV_AUTOMATION_TESTS
#include "ExpeditionInputHarness.h"
#include "Algo/Reverse.h"
#include "Engine/StaticMesh.h"
#include "Misc/CommandLine.h"
#include "Presentation/BoardMetric.h"
#include "Presentation/DiceRollTheme.h"
#include "Presentation/OccluderFade.h"
#include "Presentation/VigilKit.h"
#include "KitPlacementSupport.h"

/**
 * The Drowned Vigil world kit r002 (#145): deterministic variant choice, arches, dressing placement, the occluder's
 * dither fade, and the guarantee that none of it reaches rules, snapshots or checkpoints. The pure rules are tested
 * without assets; the board-level checks need the imported kit and say so when the theme has none.
 */
namespace
{
UWorld* MakeKitWorld()
{
    const UWorld::InitializationValues Values = UWorld::InitializationValues()
        .AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false);
    return UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
}

// A test that creates its own world must release it like the shared harness does, or the zero-warning gate trips on RHI reserved memory.
void DestroyKitWorld(UWorld* World)
{
    World->DestroyWorld(false);
    FlushRenderingCommands();
    CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
}

/** The first dangerous (Ruins or Vault) route offer: Vigil boards are the ones that use the kit's world. */
int32 DangerousOffer(const AExpeditionBattleView& View)
{
    const TArray<FExpeditionRoute>& Offers = View.Progress().Offers;
    for (int32 Index = 0; Index < Offers.Num(); ++Index) { if (Offers[Index].bDangerous) { return Index; } }
    return INDEX_NONE;
}

AExpeditionBattleView* StartVigilBoard(FAutomationTestBase& Test, UWorld* World, const FString& Directory)
{
    AExpeditionBattleView* View = World->SpawnActor<AExpeditionBattleView>();
    if (!View) { return nullptr; }
    View->ConfigureStorage(FPaths::Combine(Directory, TEXT("current.json")));
    View->DispatchBeginPlay();
    View->NewRun();
    const int32 Offer = DangerousOffer(*View);
    if (Offer == INDEX_NONE) { Test.AddInfo(TEXT("No dangerous route is on offer in this run; the Vigil board checks were skipped.")); return nullptr; }
    View->ChooseRoute(Offer);
    View->HoldEnemyTurns(true);
    View->Tick(0.016f);
    return View;
}

struct FVigilSeededOutcome
{
    FCombatSnapshot Battle;
    FString Checkpoint;
    int32 Commands = 0;
};

// The same rules-only policy as the board metric test, under a presentation variant: 0 the shipped theme, 1 primitives.
FVigilSeededOutcome PlayThemed(FAutomationTestBase& Test, UWorld* World, const FString& Directory, int32 Variant)
{
    FVigilSeededOutcome Outcome;
    AExpeditionBattleView* View = World->SpawnActor<AExpeditionBattleView>();
    if (!Test.TestNotNull(TEXT("Battle view"), View)) { return Outcome; }
    const FString Path = FPaths::Combine(Directory, FString::FromInt(Variant), TEXT("current.json"));
    View->ConfigureStorage(Path);
    View->DispatchBeginPlay();
    if (Variant == 1) { View->UsePrimitives(); }
    View->NewRun();
    View->ChooseRoute(0);
    for (int32 Step = 0; Step < 80 && View->InCombat(); ++Step)
    {
        const int32 Id = View->Rules().ActiveId();
        const FCombatantState* Active = View->Rules().Find(Id);
        if (!Active) { break; }
        if (Active->Side == ECombatSide::Enemy) { View->Tick(1.0f); View->SkipPresentation(); continue; }
        const TArray<int32> Targets = View->Rules().LegalTargets(Id);
        const TArray<FIntPoint> Moves = View->Rules().LegalMoves(Id);
        if (!Targets.IsEmpty()) { View->Execute({ECombatCommandType::Attack, Id, {}, Targets[0]}); ++Outcome.Commands; }
        else if (!Moves.IsEmpty()) { View->Execute({ECombatCommandType::Move, Id, Moves[0]}); ++Outcome.Commands; }
        View->SkipPresentation();
        if (View->InCombat() && View->Rules().ActiveId() == Id) { View->Execute({ECombatCommandType::EndTurn, Id}); ++Outcome.Commands; View->SkipPresentation(); }
    }
    View->Save();
    Outcome.Battle = View->Rules().Snapshot();
    FFileHelper::LoadFileToString(Outcome.Checkpoint, *Path);
    // Each NewRun draws a fresh run identity; it is the one field that differs by design.
    const FString RunId = View->Progress().RunId;
    if (!RunId.IsEmpty()) { Outcome.Checkpoint.ReplaceInline(*RunId, TEXT("<run-id>"), ESearchCase::CaseSensitive); }
    return Outcome;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVigilVariantChoiceTest, "Project.Expedition.Presentation.VigilVariantChoiceIsDeterministic",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FVigilVariantChoiceTest::RunTest(const FString& Parameters)
{
    // Pure rule: the variant depends only on (seed, recipe, cell, count), never on call order or other cells.
    for (const int32 Seed : {7, 11, 12345, -5})
    {
        for (int32 Recipe = 0; Recipe < 3; ++Recipe)
        {
            TSet<int32> Used;
            for (int32 Y = 0; Y < 6; ++Y)
            {
                for (int32 X = 0; X < 6; ++X)
                {
                    const int32 Index = VigilKit::VariantIndex(Seed, Recipe, FIntPoint(X, Y), 3);
                    TestTrue(TEXT("A variant index is inside the three variants"), Index >= 0 && Index < 3);
                    TestEqual(TEXT("The same inputs choose the same variant"), VigilKit::VariantIndex(Seed, Recipe, FIntPoint(X, Y), 3), Index);
                    Used.Add(Index);
                }
            }
            TestEqual(FString::Printf(TEXT("All three variants occur across 36 cells (seed %d, recipe %d)"), Seed, Recipe), Used.Num(), 3);
        }
    }
    TestEqual(TEXT("One variant always chooses it"), VigilKit::VariantIndex(7, 1, FIntPoint(2, 3), 1), 0);
    TestEqual(TEXT("No variants choose zero"), VigilKit::VariantIndex(7, 1, FIntPoint(2, 3), 0), 0);
    int32 Differing = 0;
    for (int32 X = 0; X < 12; ++X) { if (VigilKit::VariantIndex(7, 1, FIntPoint(X, 2), 3) != VigilKit::VariantIndex(8, 1, FIntPoint(X, 2), 3)) { ++Differing; } }
    TestTrue(TEXT("A different seed chooses differently somewhere"), Differing > 0);

    // The same encounter built twice, and at another metric, draws the same variants on the same cells.
    UWorld* World = MakeKitWorld();
    if (!TestNotNull(TEXT("Transient world"), World)) { return false; }
    const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation/VigilVariants"), FGuid::NewGuid().ToString());
    if (AExpeditionBattleView* View = StartVigilBoard(*this, World, Directory))
    {
        const TMap<FIntPoint, int32> First = View->ObstacleVariants();
        if (First.IsEmpty()) { AddInfo(TEXT("The theme has no r002 obstacle variants yet; the board-level check was skipped.")); }
        else
        {
            const FBoardMetric Original = View->Metric();
            View->SetBoardMetric(FBoardMetric(180.0f, 190.0f));
            const TMap<FIntPoint, int32> Rescaled = View->ObstacleVariants();
            View->SetBoardMetric(Original);
            TestTrue(TEXT("The variants are the same at another cell size"), First.OrderIndependentCompareEqual(Rescaled));
            TestTrue(TEXT("The variants are the same after another rebuild"), First.OrderIndependentCompareEqual(View->ObstacleVariants()));
            for (const TPair<FIntPoint, int32>& Pair : First) { TestTrue(TEXT("A kit obstacle uses a variant or an arch"), Pair.Value >= -1 && Pair.Value < 3); }
            // Every blocked cell is covered by exactly one piece, and every piece sits on its cell through the metric.
            const FCombatEncounter& Encounter = View->Rules().Snapshot().Encounter;
            const FBoardMetric& Metric = View->Metric();
            // The family comes from the rules' encounter identity; the check reads the meshes actually placed on every obstacle cell (#188 review).
            const int32 Kind = View->Progress().History.IsEmpty() ? 0 : FExpeditionRun::Encounter(View->Progress(), View->Progress().History.Last().Route).ObstacleKind;
            if (const UDiceRollTheme* Theme = LoadObject<UDiceRollTheme>(nullptr, TEXT("/Game/DnDDiceRoll/Theme/DA_DiceRollTheme.DA_DiceRollTheme"), nullptr, LOAD_Quiet | LOAD_NoWarn))
            {
                const int32 Arches = KitPlacementSupport::CheckObstacles(*this, TEXT("The dangerous first fight"), *View, Theme->KitFor(EDiceRollKit::Vigil), Kind);
                if (!VigilKit::ArchesFor(Kind)) { TestEqual(TEXT("A stones board places no arches"), Arches, 0); }
                else { TestEqual(TEXT("Every adjacent blocked pair carries an arch"), Arches, VigilKit::ArchPairs(Encounter.BlockedCells).Num()); }
            }
            else { AddError(TEXT("The shipped theme does not load")); }
            for (const FIntPoint& Cell : Encounter.BlockedCells)
            {
                const FVector* Spot = View->ObstacleSpots().Find(Cell);
                if (!TestNotNull(TEXT("A blocked cell has an obstacle piece"), Spot)) { continue; }
                const int32* Variant = First.Find(Cell);
                if (Variant && *Variant >= 0)
                {
                    TestEqual(TEXT("The obstacle stands on its cell: X"), static_cast<float>(Spot->X), Cell.X * Metric.CellCm, 0.01f);
                    TestEqual(TEXT("The obstacle stands on its cell: Y"), static_cast<float>(Spot->Y), Cell.Y * Metric.CellCm, 0.01f);
                }
                TestEqual(TEXT("The obstacle stands on the board top"), static_cast<float>(Spot->Z), Metric.BoardTop(), 0.01f);
            }
        }
    }
    DestroyKitWorld(World);
    IFileManager::Get().DeleteDirectory(*Directory, false, true);
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVigilArchPairsTest, "Project.Expedition.Presentation.VigilArchPairs",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FVigilArchPairsTest::RunTest(const FString& Parameters)
{
    TArray<FIntPoint> Blocked = {FIntPoint(3, 3), FIntPoint(4, 3), FIntPoint(3, 4), FIntPoint(8, 8), FIntPoint(8, 9), FIntPoint(12, 2)};
    const TArray<VigilKit::FArch> Arches = VigilKit::ArchPairs(Blocked);
    TestEqual(TEXT("Two pairs of adjacent cells get arches; the L's third cell and the loner do not"), Arches.Num(), 2);
    TSet<FIntPoint> Covered;
    for (const VigilKit::FArch& Arch : Arches)
    {
        TestTrue(TEXT("An arch covers two blocked cells"), Blocked.Contains(Arch.A) && Blocked.Contains(Arch.B));
        TestEqual(TEXT("The cells are orthogonal neighbours"), FMath::Abs(Arch.A.X - Arch.B.X) + FMath::Abs(Arch.A.Y - Arch.B.Y), 1);
        TestTrue(TEXT("X-adjacent pairs are flagged and turned 90 degrees"), Arch.bAlongX == (Arch.A.Y == Arch.B.Y));
        TestEqual(TEXT("Yaw follows the axis"), Arch.Yaw(), Arch.bAlongX ? 90.0f : 0.0f);
        TestFalse(TEXT("No cell is in two arches"), Covered.Contains(Arch.A) || Covered.Contains(Arch.B));
        Covered.Add(Arch.A); Covered.Add(Arch.B);
        TestEqual(TEXT("The midpoint is on the shared edge: X"), static_cast<float>(Arch.Midpoint().X), 0.5f * (Arch.A.X + Arch.B.X));
        TestEqual(TEXT("The midpoint is on the shared edge: Y"), static_cast<float>(Arch.Midpoint().Y), 0.5f * (Arch.A.Y + Arch.B.Y));
    }
    Algo::Reverse(Blocked);
    const TArray<VigilKit::FArch> Again = VigilKit::ArchPairs(Blocked);
    TestEqual(TEXT("The pairing does not depend on the order the cells are listed"), Again.Num(), Arches.Num());
    for (int32 Index = 0; Index < FMath::Min(Again.Num(), Arches.Num()); ++Index)
    { TestTrue(TEXT("Same arches in the same order"), Again[Index].A == Arches[Index].A && Again[Index].B == Arches[Index].B); }
    TestTrue(TEXT("No blocked cells make no arches"), VigilKit::ArchPairs({}).IsEmpty());
    TestTrue(TEXT("Pillar and wall recipes take arches"), VigilKit::ArchesFor(1) && VigilKit::ArchesFor(2));
    TestFalse(TEXT("The stones recipe takes none"), VigilKit::ArchesFor(0));
    TestFalse(TEXT("An unknown recipe takes none"), VigilKit::ArchesFor(3) || VigilKit::ArchesFor(-1));
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVigilDressingTest, "Project.Expedition.Presentation.VigilDressingStaysOffPlayableCells",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FVigilDressingTest::RunTest(const FString& Parameters)
{
    VigilKit::FDressingKinds Kinds;
    Kinds.Scatter = 8; Kinds.Mist = 3; Kinds.Shaft = 2;
    for (const FIntPoint Board : {FIntPoint(16, 11), FIntPoint(18, 12), FIntPoint(17, 12), FIntPoint(8, 8)})
    {
        for (const int32 Seed : {1, 7, 4242, -9})
        {
            const TArray<VigilKit::FDressing> Plan = VigilKit::PlanDressing(Board.X, Board.Y, Seed, Kinds);
            TestEqual(TEXT("Counts are fixed"), Plan.Num(), VigilKit::ScatterCount + VigilKit::MistCount + VigilKit::ShaftCount);
            TSet<FIntPoint> Cells;
            for (const VigilKit::FDressing& Item : Plan)
            {
                const int32 Ring = VigilKit::RingDistance(Item.Cell, Board.X, Board.Y);
                const bool bScatter = Item.Kind == VigilKit::EDressingKind::Scatter;
                TestTrue(TEXT("Nothing is on or beside a playable cell: scatter is two or more cells out, mist and shafts three or more"),
                    Ring >= (bScatter ? VigilKit::ScatterMinRing : VigilKit::MistMinRing));
                TestTrue(TEXT("Nothing is far from the board"), Ring <= VigilKit::MistMaxRing);
                TestTrue(TEXT("The offset stays inside the cell's quarter"), FMath::Abs(Item.Offset.X) <= 0.25 && FMath::Abs(Item.Offset.Y) <= 0.25);
                // Even with its offset, the piece's own position rounds to the same off-board cell.
                TestEqual(TEXT("The piece rounds to its cell: X"), FMath::RoundToInt(static_cast<float>(Item.Position().X)), Item.Cell.X);
                TestEqual(TEXT("The piece rounds to its cell: Y"), FMath::RoundToInt(static_cast<float>(Item.Position().Y)), Item.Cell.Y);
                if (Item.Kind == VigilKit::EDressingKind::Shaft) { TestTrue(TEXT("Shafts lean toward -X, so they stand on the -X side"), Item.Cell.X <= VigilKit::ShaftMaxX); }
                const int32 Limit = Item.Kind == VigilKit::EDressingKind::Scatter ? Kinds.Scatter : Item.Kind == VigilKit::EDressingKind::Mist ? Kinds.Mist : Kinds.Shaft;
                TestTrue(TEXT("The variant exists"), Item.Variant >= 0 && Item.Variant < Limit);
                TestFalse(TEXT("No two pieces share a cell"), Cells.Contains(Item.Cell));
                Cells.Add(Item.Cell);
            }
            const TArray<VigilKit::FDressing> Again = VigilKit::PlanDressing(Board.X, Board.Y, Seed, Kinds);
            bool bSame = Again.Num() == Plan.Num();
            for (int32 Index = 0; bSame && Index < Plan.Num(); ++Index)
            { bSame = Plan[Index].Cell == Again[Index].Cell && Plan[Index].Variant == Again[Index].Variant && Plan[Index].Kind == Again[Index].Kind && Plan[Index].Yaw == Again[Index].Yaw; }
            TestTrue(TEXT("The plan is deterministic"), bSame);
        }
    }
    VigilKit::FDressingKinds Nothing;
    TestTrue(TEXT("A theme without dressing places none"), VigilKit::PlanDressing(18, 12, 7, Nothing).IsEmpty());

    // On a real kit board, what was placed stands where the plan says, through the metric, and clear of the board.
    UWorld* World = MakeKitWorld();
    if (!TestNotNull(TEXT("Transient world"), World)) { return false; }
    const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation/VigilDressing"), FGuid::NewGuid().ToString());
    if (AExpeditionBattleView* View = StartVigilBoard(*this, World, Directory))
    {
        if (!View->UsesVigilKit() || View->PlacedDressing().IsEmpty()) { AddInfo(TEXT("The theme's Vigil kit is not switched on; the board-level check was skipped.")); }
        else
        {
            const FCombatEncounter& Encounter = View->Rules().Snapshot().Encounter;
            const FBoardMetric& Metric = View->Metric();
            for (const FPlacedDressing& Placed : View->PlacedDressing())
            {
                const FIntPoint Cell = Metric.CellAt(Placed.Position);
                TestTrue(TEXT("The piece stands on its planned cell through the metric"), Cell == Placed.Plan.Cell);
                TestTrue(TEXT("The piece is off the board by the planned margin"), VigilKit::RingDistance(Cell, Encounter.Width, Encounter.Height) >= VigilKit::ScatterMinRing);
                TestEqual(TEXT("The piece stands on the board top"), static_cast<float>(Placed.Position.Z), Metric.BoardTop(), 0.5f);
            }
        }
    }
    DestroyKitWorld(World);
    IFileManager::Get().DeleteDirectory(*Directory, false, true);
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVigilOccluderFadeTest, "Project.Expedition.Presentation.VigilOccluderFadesWhenBlocking",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FVigilOccluderFadeTest::RunTest(const FString& Parameters)
{
    const FVector Eye(-1000.0f, 0.0f, 800.0f), Target(0.0f, 0.0f, 0.0f);
    const float Radius = 300.0f;
    const FVector OnLine = Eye + (Target - Eye) * 0.5f;
    TestEqual(TEXT("A piece on the sight line is faded to the minimum"), OccluderFade::Opacity(OnLine, Eye, Target, Radius), OccluderFade::MinOpacity, 0.0001f);
    TestTrue(TEXT("It is never fully hidden"), OccluderFade::Opacity(OnLine, Eye, Target, Radius) > 0.0f);
    TestEqual(TEXT("A piece well off the line is opaque"), OccluderFade::Opacity(OnLine + FVector(0.0f, 2.0f * Radius, 0.0f), Eye, Target, Radius), 1.0f, 0.0001f);
    TestEqual(TEXT("A piece behind the target is opaque"), OccluderFade::Opacity(Target + (Target - Eye) * 0.2f, Eye, Target, Radius), 1.0f, 0.0001f);
    TestEqual(TEXT("A piece behind the camera is opaque"), OccluderFade::Opacity(Eye - (Target - Eye) * 0.2f, Eye, Target, Radius), 1.0f, 0.0001f);
    float Last = 0.0f;
    for (float Away = 0.0f; Away <= 2.0f * Radius; Away += 10.0f)
    {
        const float Opacity = OccluderFade::Opacity(OnLine + FVector(0.0f, Away, 0.0f), Eye, Target, Radius);
        TestTrue(TEXT("The fade eases back to opaque the farther a piece is from the line"), Opacity >= Last - 0.0001f);
        Last = Opacity;
    }
    // Either watched point fades the piece: the lower of the two sight lines wins.
    const FVector Unit(0.0f, 900.0f, 0.0f);
    const FVector OnUnitLine = Eye + (Unit - Eye) * 0.5f;
    TestEqual(TEXT("A piece blocking only the unit's line fades"), OccluderFade::Lowest(OnUnitLine, Eye, Target, Unit, Radius), OccluderFade::MinOpacity, 0.0001f);
    TestEqual(TEXT("A piece blocking neither line is opaque"), OccluderFade::Lowest(FVector(-500.0f, -3000.0f, 400.0f), Eye, Target, Unit, Radius), 1.0f, 0.0001f);

    // Board test: a point that covers a board cell fades, a point over ground beside the board does not.
    {
        const FVector2D BoardMin(-300.0f, -300.0f), BoardMax(300.0f, 300.0f);
        const float PlaneZ = 0.0f, Ramp = 75.0f;
        const FVector Cell(100.0f, 50.0f, PlaneZ);
        const FVector InFront = Eye + (Cell - Eye) * 0.5f;
        TestEqual(TEXT("A piece in front of a board cell fades to the minimum"), OccluderFade::Board(InFront, Eye, BoardMin, BoardMax, PlaneZ, Ramp), OccluderFade::MinOpacity, 0.0001f);
        TestTrue(TEXT("The board fade is dithered, not hidden"), OccluderFade::Board(InFront, Eye, BoardMin, BoardMax, PlaneZ, Ramp) > 0.0f);
        const FVector Beside(0.0f, 1500.0f, PlaneZ);
        TestEqual(TEXT("A piece in front of ground beside the board is opaque"), OccluderFade::Board(Eye + (Beside - Eye) * 0.5f, Eye, BoardMin, BoardMax, PlaneZ, Ramp), 1.0f, 0.0001f);
        const FVector Behind(-1500.0f, 0.0f, PlaneZ);
        TestEqual(TEXT("A piece in front of ground behind the camera's side of the board is opaque"), OccluderFade::Board(Eye + (Behind - Eye) * 0.5f, Eye, BoardMin, BoardMax, PlaneZ, Ramp), 1.0f, 0.0001f);
        TestEqual(TEXT("A point on the board plane is not in front of it"), OccluderFade::Board(Cell, Eye, BoardMin, BoardMax, PlaneZ, Ramp), 1.0f, 0.0001f);
        TestEqual(TEXT("A point below the board plane is opaque"), OccluderFade::Board(Cell - FVector(0.0f, 0.0f, 40.0f), Eye, BoardMin, BoardMax, PlaneZ, Ramp), 1.0f, 0.0001f);
        TestEqual(TEXT("A point level with or above the camera is opaque"), OccluderFade::Board(Eye + FVector(500.0f, 0.0f, 0.0f), Eye, BoardMin, BoardMax, PlaneZ, Ramp), 1.0f, 0.0001f);
        const FVector OnEdge(BoardMax.X, 0.0f, PlaneZ);
        TestEqual(TEXT("A piece in front of the board's edge still fades fully"), OccluderFade::Board(Eye + (OnEdge - Eye) * 0.5f, Eye, BoardMin, BoardMax, PlaneZ, Ramp), OccluderFade::MinOpacity, 0.0001f);
        float Previous = 0.0f;
        for (float Past = 0.0f; Past <= 2.0f * Ramp; Past += 5.0f)
        {
            const FVector Beyond(BoardMax.X + Past, 0.0f, PlaneZ);
            const float Opacity = OccluderFade::Board(Eye + (Beyond - Eye) * 0.5f, Eye, BoardMin, BoardMax, PlaneZ, Ramp);
            TestTrue(TEXT("The board fade eases back to opaque past the rectangle"), Opacity >= Previous - 0.0001f);
            Previous = Opacity;
        }
        TestEqual(TEXT("It is opaque a full ramp past the rectangle"), Previous, 1.0f, 0.0001f);
    }

    // On a real kit board the same rule is fed the live camera, focus and active unit.
    UWorld* World = MakeKitWorld();
    if (!TestNotNull(TEXT("Transient world"), World)) { return false; }
    const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation/VigilOccluder"), FGuid::NewGuid().ToString());
    if (AExpeditionBattleView* View = StartVigilBoard(*this, World, Directory))
    {
        if (!View->HasOccluder()) { AddInfo(TEXT("The theme has no occluder mesh yet; the board-level check was skipped.")); }
        else
        {
            View->SnapCamera(-90.0f, View->FramedDistance());
            View->Tick(0.016f);
            FVector LiveEye, LiveFocus, LiveUnit;
            View->OccluderSight(LiveEye, LiveFocus, LiveUnit);
            const float Live = View->Metric().CellCm * OccluderFade::RadiusCells;
            const FVector Between = LiveEye + (LiveFocus - LiveEye) * 0.5f;
            TestEqual(TEXT("Something between the camera and the focus is faded"), View->OccluderOpacityAt(Between), View->OccluderMinOpacity(), 0.001f);
            TestTrue(TEXT("It is dithered, not hidden"), View->OccluderOpacityAt(Between) > 0.0f);
            TestEqual(TEXT("Something far to the side of the line is opaque"), View->OccluderOpacityAt(Between + FVector(0.0f, 0.0f, 4.0f * Live)), 1.0f, 0.001f);
            TestEqual(TEXT("Something past the focus is opaque"), View->OccluderOpacityAt(LiveFocus + (LiveFocus - LiveEye) * 0.3f), 1.0f, 0.001f);
            // The board test uses the live board rectangle: pieces over a board cell fade, pieces over ground well outside do not.
            FVector2D BoardMin, BoardMax;
            float PlaneZ = 0.0f;
            View->OccluderBoard(BoardMin, BoardMax, PlaneZ);
            const FVector BoardCentre((BoardMin.X + BoardMax.X) * 0.5f, (BoardMin.Y + BoardMax.Y) * 0.5f, PlaneZ);
            TestTrue(TEXT("The camera is above the board plane"), LiveEye.Z > PlaneZ);
            TestEqual(TEXT("Something between the camera and a board cell is faded"), View->OccluderBoardOpacityAt(LiveEye + (BoardCentre - LiveEye) * 0.5f), View->OccluderMinOpacity(), 0.001f);
            const FVector Outside(BoardMax.X + 20.0f * View->Metric().CellCm, BoardCentre.Y, PlaneZ);
            TestEqual(TEXT("Something over ground well beside the board is opaque"), View->OccluderBoardOpacityAt(LiveEye + (Outside - LiveEye) * 0.5f), 1.0f, 0.001f);
            // Turning the camera around the board moves the line: the old spot no longer blocks it.
            View->SnapCamera(90.0f, View->FramedDistance(90.0f));
            View->Tick(0.016f);
            TestEqual(TEXT("After the camera moves, the old spot is opaque again"), View->OccluderOpacityAt(Between), 1.0f, 0.001f);
        }
    }
    DestroyKitWorld(World);
    IFileManager::Get().DeleteDirectory(*Directory, false, true);
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVigilThemeKeepsStateTest, "Project.Expedition.Presentation.VigilThemeChoiceLeavesStateUnchanged",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FVigilThemeKeepsStateTest::RunTest(const FString& Parameters)
{
    // The same seeded run under the shipped theme (the Vigil kit when switched on) and under the primitives: the
    // authoritative snapshot and the checkpoint bytes must match, because art never reaches rules or saves.
    UWorld* World = MakeKitWorld();
    if (!TestNotNull(TEXT("Transient world"), World)) { return false; }
    const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation/VigilThemeState"), FGuid::NewGuid().ToString());
    const FVigilSeededOutcome Themed = PlayThemed(*this, World, Directory, 0);
    const FVigilSeededOutcome Primitive = PlayThemed(*this, World, Directory, 1);
    TestTrue(TEXT("The run played commands"), Themed.Commands > 0);
    TestFalse(TEXT("The run wrote a checkpoint"), Themed.Checkpoint.IsEmpty());
    TestEqual(TEXT("Both runs play the same number of commands"), Primitive.Commands, Themed.Commands);
    TestTrue(TEXT("The battle snapshot is identical"), FCombatSnapshot::StaticStruct()->CompareScriptStruct(&Themed.Battle, &Primitive.Battle, 0));
    TestTrue(TEXT("The checkpoint bytes are identical"), Primitive.Checkpoint == Themed.Checkpoint);
    DestroyKitWorld(World);
    IFileManager::Get().DeleteDirectory(*Directory, false, true);
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVigilPrimitiveFallbackTest, "Project.Expedition.Presentation.VigilPrimitiveFallbackStillBuilds",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FVigilPrimitiveFallbackTest::RunTest(const FString& Parameters)
{
    // The real -ExpeditionPrimitives path: the flag is read in BeginPlay, so set it on the command line for one world.
    const FString Original = FCommandLine::Get();
    FCommandLine::Set(*(Original + TEXT(" -ExpeditionPrimitives")));
    UWorld* World = MakeKitWorld();
    const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation/VigilPrimitives"), FGuid::NewGuid().ToString());
    AExpeditionBattleView* View = nullptr;
    if (World)
    {
        View = World->SpawnActor<AExpeditionBattleView>();
        if (View)
        {
            View->ConfigureStorage(FPaths::Combine(Directory, TEXT("current.json")));
            View->DispatchBeginPlay();
        }
    }
    FCommandLine::Set(*Original);
    if (!TestNotNull(TEXT("Transient world"), World)) { return false; }
    if (TestNotNull(TEXT("Battle view"), View))
    {
        View->NewRun();
        const int32 Offer = DangerousOffer(*View);
        View->ChooseRoute(Offer == INDEX_NONE ? 0 : Offer);
        View->HoldEnemyTurns(true);
        View->Tick(0.016f);
        const FCombatEncounter& Encounter = View->Rules().Snapshot().Encounter;
        TestFalse(TEXT("-ExpeditionPrimitives keeps the theme off"), View->UsesTheme());
        TestFalse(TEXT("No Vigil kit on a primitive board"), View->UsesVigilKit());
        TestFalse(TEXT("No occluder on a primitive board"), View->HasOccluder());
        TestTrue(TEXT("No kit dressing on a primitive board"), View->PlacedDressing().IsEmpty() && View->ObstacleVariants().IsEmpty());
        TestEqual(TEXT("A primitive board keeps one cube tile per cell"), View->TileCount(), Encounter.Width * Encounter.Height);
        TestEqual(TEXT("An absent occluder never fades anything"), View->OccluderOpacityAt(FVector(1.0f, 2.0f, 3.0f)), 1.0f);
        TestEqual(TEXT("The primitive board uses the default metric"), View->Metric().CellCm, FBoardMetric::DefaultCellCm);
    }
    DestroyKitWorld(World);
    IFileManager::Get().DeleteDirectory(*Directory, false, true);
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVigilThemeDataTest, "Project.Expedition.Presentation.VigilThemeDataIsComplete",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FVigilThemeDataTest::RunTest(const FString& Parameters)
{
    const UDiceRollTheme* Theme = LoadObject<UDiceRollTheme>(nullptr, TEXT("/Game/DnDDiceRoll/Theme/DA_DiceRollTheme.DA_DiceRollTheme"), nullptr, LOAD_Quiet | LOAD_NoWarn);
    if (!Theme) { AddInfo(TEXT("The shipped theme asset is not available here.")); return true; }
    // The r001 fallbacks stay in place whatever the kit state.
    TestFalse(TEXT("The r001 world fallback is set"), Theme->VigilWorld.IsNull());
    TestFalse(TEXT("The r001 obstacle fallbacks are set"), Theme->Pillar.IsNull() || Theme->Wall.IsNull() || Theme->Stones.IsNull());
    if (!Theme->bMetricKit) { AddInfo(TEXT("The Vigil kit is not switched on in the theme yet; the r002 entries were not checked.")); return !HasAnyErrors(); }
    TestNotNull(TEXT("Occluder mesh"), UDiceRollTheme::Load(Theme->VigilOccluders));
    TestNotNull(TEXT("2-cell arch"), UDiceRollTheme::Load(Theme->ArchTwoCell));
    const TPair<const TArray<TSoftObjectPtr<UStaticMesh>>*, int32> Sets[] = {
        {&Theme->PillarVariants, 3}, {&Theme->WallVariants, 3}, {&Theme->StonesVariants, 3}, {&Theme->Scatter, 8}, {&Theme->MistCards, 3}, {&Theme->LightShafts, 2}};
    for (const TPair<const TArray<TSoftObjectPtr<UStaticMesh>>*, int32>& Set : Sets)
    {
        TestEqual(TEXT("A kit set has its full variant count"), Set.Key->Num(), Set.Value);
        for (const TSoftObjectPtr<UStaticMesh>& Entry : *Set.Key) { TestNotNull(TEXT("A kit mesh loads"), UDiceRollTheme::Load(Entry)); }
    }
    return !HasAnyErrors();
}

/**
 * #209: the Vigil's stained-glass rose hangs on the home-side wall, about 8 cells past the board, and from a camera on that side its pixels'
 * rays land about 5 cells past the board's near edge, where the board fade did not reach, so it filled the bottom of the frame behind the action
 * bar. The fade now reaches 7.5 cells on the side(s) the camera is on, at every yaw, and stays at half a cell on the others, so the geometry
 * between the camera and the board dithers and the rose stays opaque from the sides and when it is behind the board.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVigilNearSideFadeTest, "Project.Expedition.Presentation.VigilNearSideGeometryFades",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FVigilNearSideFadeTest::RunTest(const FString& Parameters)
{
    UWorld* World = MakeKitWorld();
    if (!TestNotNull(TEXT("Transient world"), World)) { return false; }
    const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation/VigilNearSide"), FGuid::NewGuid().ToString());
    AExpeditionBattleView* View = StartVigilBoard(*this, World, Directory);
    if (TestNotNull(TEXT("A Vigil board"), View) && TestTrue(TEXT("It draws the Vigil kit with its occluder"), View->UsesVigilKit() && View->HasOccluder()))
    {
        TestEqual(TEXT("The Vigil's ramp is a cell"), View->OccluderRampCells(), OccluderFade::VigilBoardRampCells);
        const double Cell = View->Metric().CellCm;
        // Yaw -90 puts the camera on the low-Y side, 90 the high-Y side, 0 the high-X side, 180 the low-X side; the sides it is not on keep the base margin.
        struct FCase { float Yaw; int32 Wide; };
        for (const FCase& Case : {FCase{-90.0f, 2}, FCase{90.0f, 3}, FCase{0.0f, 1}, FCase{180.0f, 0}})
        {
            View->SnapCamera(Case.Yaw, View->FramedDistance(Case.Yaw));
            View->Tick(0.016f);
            float Margins[4];
            View->OccluderSideMargins(Margins);
            const FString Label = FString::Printf(TEXT("Yaw %.0f"), Case.Yaw);
            for (int32 Side = 0; Side < 4; ++Side)
            {
                if (Side == Case.Wide) { TestEqual(*FString::Printf(TEXT("%s: the camera's side (%d) reaches the near margin"), *Label, Side), Margins[Side], OccluderFade::VigilNearMarginCells, 0.01f); }
                else { TestEqual(*FString::Printf(TEXT("%s: side %d keeps the base margin"), *Label, Side), Margins[Side], OccluderFade::BoardMarginCells, 0.01f); }
            }
            // The rect the shader and the CPU query share: a point whose view ray lands 5 cells past the camera-side edge is faded, 8 cells past is not.
            FVector2D Min, Max;
            float PlaneZ = 0.0f;
            View->OccluderBoard(Min, Max, PlaneZ);
            const FVector Eye = View->CameraTransform().GetLocation();
            const FCombatEncounter& Board = View->Rules().Snapshot().Encounter;
            const FVector Centre = View->GetActorTransform().TransformPosition(View->BoardPosition(FVector2D((Board.Width - 1) * 0.5f, (Board.Height - 1) * 0.5f)));
            const FVector2D Toward = Case.Wide == 0 ? FVector2D(-1, 0) : Case.Wide == 1 ? FVector2D(1, 0) : Case.Wide == 2 ? FVector2D(0, -1) : FVector2D(0, 1);
            const double Half = (Case.Wide < 2 ? Board.Width : Board.Height) * 0.5 * Cell;
            auto Fade = [&](double CellsPast, FVector2D Direction)
            {
                const FVector Ground(Centre.X + Direction.X * (Half + CellsPast * Cell), Centre.Y + Direction.Y * (Half + CellsPast * Cell), PlaneZ);
                return View->OccluderBoardOpacityAt(Eye + (Ground - Eye) * 0.5);
            };
            TestEqual(*(Label + TEXT(": the floor 5 cells past the camera-side edge is faded (where the rose's rays land)")), Fade(5.0, Toward), View->OccluderMinOpacity(), 0.001f);
            TestEqual(*(Label + TEXT(": the floor past the margin and its ramp is opaque")), Fade(OccluderFade::VigilNearMarginCells + OccluderFade::VigilBoardRampCells + 0.5, Toward), 1.0f, 0.001f);
            TestEqual(*(Label + TEXT(": the floor 3 cells past the far edge is opaque")), Fade(3.0, -Toward), 1.0f, 0.001f);
        }
        // Between the axes both sides the camera is on reach out.
        View->SnapCamera(-45.0f, View->FramedDistance(-45.0f));
        View->Tick(0.016f);
        float Diagonal[4];
        View->OccluderSideMargins(Diagonal);
        TestTrue(TEXT("At a diagonal yaw both camera-side margins are wider than the base"), Diagonal[1] > OccluderFade::BoardMarginCells + 0.5f && Diagonal[2] > OccluderFade::BoardMarginCells + 0.5f);
    }
    DestroyKitWorld(World);
    IFileManager::Get().DeleteDirectory(*Directory, false, true);
    return !HasAnyErrors();
}

/**
 * #209, on-screen note 3: the stained-glass rose (on the home-side wall, 21.75 m from the board centre, 5.8 to 13.7 m up, 7.9 m wide: the extents of
 * its triangles in SM_World_Vigil_Occluders) must not fill the bottom of the frame behind the action bar at any zoom the camera allows. Every
 * point of a grid over the rose that projects into the bottom third of the viewport over the action bar and its flanks (the middle 60% of the width) is faded to the Vigil's floor opacity, at zooms from the nearest
 * to the farthest, yaws within 45 degrees either side of the home yaw, unpanned and panned to the near stop.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVigilRoseFadesBehindTheDockTest, "Project.Expedition.Presentation.VigilRoseFadesBehindTheDockAtEveryZoom",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FVigilRoseFadesBehindTheDockTest::RunTest(const FString& Parameters)
{
    using namespace ExpeditionCameraRig;
    UWorld* World = MakeKitWorld();
    if (!TestNotNull(TEXT("Transient world"), World)) { return false; }
    const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation/VigilRose"), FGuid::NewGuid().ToString());
    AExpeditionBattleView* View = StartVigilBoard(*this, World, Directory);
    if (TestNotNull(TEXT("A Vigil board"), View) && TestTrue(TEXT("It draws the Vigil kit with its occluder"), View->UsesVigilKit() && View->HasOccluder()))
    {
        View->SetSafeRect(FSafeRect::ForViewport(1920, 1080));
        const FCombatEncounter& Board = View->Rules().Snapshot().Encounter;
        const FVector Centre = View->GetActorTransform().TransformPosition(View->BoardPosition(FVector2D((Board.Width - 1) * 0.5f, (Board.Height - 1) * 0.5f)));
        FVector2D Min, Max;
        float PlaneZ = 0.0f;
        View->OccluderBoard(Min, Max, PlaneZ);
        TArray<FVector> Rose;
        for (const double X : {-390.0, -195.0, 0.0, 195.0, 390.0}) for (const double Up : {580.0, 840.0, 1100.0, 1370.0})
        { Rose.Add(FVector(Centre.X + X, Centre.Y - 2175.0, PlaneZ + Up)); }
        int32 Checked = 0, Zooms = 0;
        const float Floor = View->OccluderMinOpacity();
        for (const float Zoom : {MinDistance, 1100.0f, NearDistance, 2000.0f, -1.0f, 3500.0f, 5000.0f, MaxDistance})
        {
            for (const float Yaw : {-90.0f, -60.0f, -120.0f, -45.0f, -135.0f})
            {
                for (const bool bPanned : {false, true})
                {
                    const float Distance = Zoom < 0.0f ? View->FramedDistance(Yaw) : Zoom;
                    View->SnapCamera(Yaw, Distance);
                    View->Tick(0.016f);
                    if (bPanned) { View->Pan(0.0f, -1.0e5f); View->Pan(0.0f, -1.0e5f); View->Tick(5.0f); }
                    const FTransform Camera = View->CameraTransform();
                    const FString Label = FString::Printf(TEXT("Zoom %.0f yaw %.0f%s"), Distance, Yaw, bPanned ? TEXT(" panned to the near stop") : TEXT(""));
                    for (const FVector& Point : Rose)
                    {
                        FVector2D Screen;
                        if (!Project(Point, Camera.GetLocation(), Camera.GetRotation().GetForwardVector(), View->SafeRect().Aspect, Screen)) { continue; }
                        if (Screen.X < 0.2 || Screen.X > 0.8 || Screen.Y < 0.66 || Screen.Y > 1.0) { continue; }
                        ++Checked;
                        const float Opacity = View->OccluderBoardOpacityAt(Point);
                        if (Opacity > Floor + 0.001f)
                        { AddError(FString::Printf(TEXT("%s: a point of the rose at screen (%.2f, %.2f) in the bottom third has opacity %.2f, not %.2f"), *Label, Screen.X, Screen.Y, Opacity, Floor)); }
                    }
                }
            }
            ++Zooms;
        }
        AddInfo(FString::Printf(TEXT("%d rose points sampled in the bottom third over %d zooms"), Checked, Zooms));
        TestTrue(TEXT("Some rose points reached the bottom third, so the check is exercised"), Checked > 0);
    }
    DestroyKitWorld(World);
    IFileManager::Get().DeleteDirectory(*Directory, false, true);
    return !HasAnyErrors();
}
#endif
