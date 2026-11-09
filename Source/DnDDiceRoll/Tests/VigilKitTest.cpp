#if WITH_DEV_AUTOMATION_TESTS
#include "ExpeditionInputHarness.h"
#include "Algo/Reverse.h"
#include "Engine/StaticMesh.h"
#include "Misc/CommandLine.h"
#include "Presentation/BoardMetric.h"
#include "Presentation/DiceRollTheme.h"
#include "Presentation/OccluderFade.h"
#include "Presentation/VigilKit.h"

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

struct FSeededOutcome
{
    FCombatSnapshot Battle;
    FString Checkpoint;
    int32 Commands = 0;
};

// The same rules-only policy as the board metric test, under a presentation variant: 0 the shipped theme, 1 primitives.
FSeededOutcome PlayThemed(FAutomationTestBase& Test, UWorld* World, const FString& Directory, int32 Variant)
{
    FSeededOutcome Outcome;
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
            const int32 Recipe = View->Progress().History.IsEmpty() ? 0 : View->Progress().History.Last().Route.Recipe;
            if (!VigilKit::ArchesFor(Recipe)) { TestEqual(TEXT("A stones board places no arches"), View->PlacedArches(), 0); }
            else { TestTrue(TEXT("Arches only stand over adjacent blocked pairs"), View->PlacedArches() <= VigilKit::ArchPairs(Encounter.BlockedCells).Num()); }
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
            TestEqual(TEXT("Something between the camera and the focus is faded"), View->OccluderOpacityAt(Between), OccluderFade::MinOpacity, 0.001f);
            TestTrue(TEXT("It is dithered, not hidden"), View->OccluderOpacityAt(Between) > 0.0f);
            TestEqual(TEXT("Something far to the side of the line is opaque"), View->OccluderOpacityAt(Between + FVector(0.0f, 0.0f, 4.0f * Live)), 1.0f, 0.001f);
            TestEqual(TEXT("Something past the focus is opaque"), View->OccluderOpacityAt(LiveFocus + (LiveFocus - LiveEye) * 0.3f), 1.0f, 0.001f);
            // Turning the camera around the board moves the line: the old spot no longer blocks it.
            View->SnapCamera(90.0f, View->FramedDistance());
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
    const FSeededOutcome Themed = PlayThemed(*this, World, Directory, 0);
    const FSeededOutcome Primitive = PlayThemed(*this, World, Directory, 1);
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
#endif
