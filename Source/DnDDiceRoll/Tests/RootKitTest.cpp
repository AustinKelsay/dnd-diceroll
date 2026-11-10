#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Components/PrimitiveComponent.h"
#include "HAL/FileManager.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Persistence/ExpeditionSave.h"
#include "Presentation/BoardMetric.h"
#include "Presentation/DiceRollTheme.h"
#include "Presentation/ExpeditionBattleView.h"
#include "Presentation/OccluderFade.h"
#include "Presentation/VigilKit.h"
#include "RenderingThread.h"
#include "UObject/GarbageCollection.h"
#include "UObject/StrongObjectPtr.h"
#include "ExpeditionTestSupport.h"

/**
 * The Root of the World world kit r002 (#149), mirroring VigilKitTest for the forest boards (the Stream trail, route offer 0):
 * deterministic placement from a seeded checkpoint, fallbacks when pieces of the kit or the whole kit are missing, no
 * obstacle cell without its blocker, the true-metric scale, and the 175 to 185 cm figure height on a Root r002 board.
 * Placement rules are shared with the Vigil (VigilKit.h), so those are tested there; the board-level checks need the
 * imported kit and say so when the shipped theme has none (the plain import adds the Root pieces, the world replacement
 * switches bRootMetricKit on).
 */
namespace
{
/** A transient, GC-collectable copy of the shipped theme for a fallback case. DuplicateObject keeps the source's RF_Standalone
 * (and RF_Public), so a plain copy would survive CollectGarbage after the strong pointer releases it and pile up across runs. */
UDiceRollTheme* TransientThemeCopy(const UDiceRollTheme* Shipped)
{
    UDiceRollTheme* Copy = Shipped ? DuplicateObject<UDiceRollTheme>(Shipped, GetTransientPackage()) : nullptr;
    if (Copy)
    {
        Copy->ClearFlags(RF_Standalone | RF_Public);
        Copy->SetFlags(RF_Transient);
    }
    return Copy;
}
UWorld* MakeRootWorld()
{
    const UWorld::InitializationValues Values = UWorld::InitializationValues()
        .AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false);
    return UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
}

// A test that creates its own world must release it like the shared harness does, or the zero-warning gate trips on RHI reserved memory.
void DestroyRootWorld(UWorld* World)
{
    World->DestroyWorld(false);
    FlushRenderingCommands();
    CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
}

constexpr const TCHAR* ShippedThemePath = TEXT("/Game/DnDDiceRoll/Theme/DA_DiceRollTheme.DA_DiceRollTheme");

const UDiceRollTheme* ShippedTheme()
{
    return LoadObject<UDiceRollTheme>(nullptr, ShippedThemePath, nullptr, LOAD_Quiet | LOAD_NoWarn);
}

/**
 * Writes a checkpoint at the first fixed seed whose Stream trail (route offer 0, the forest board) uses `Recipe`:
 * 0 split stones, 1 ruined pillars, 2 broken wall. Seeds are deterministic, so the same recipe always finds the same board.
 */
bool WriteRootCheckpoint(const FString& Path, int32 Recipe)
{
    for (int32 Seed = 1; Seed < 2000; ++Seed)
    {
        FString Error;
        FExpeditionRun Run;
        if (!Run.NewRun(Seed, Error, FString::Printf(TEXT("root-%d"), Seed))) { continue; }
        const FExpeditionRoute& Route = Run.Progress().Offers[0];
        if (Route.bDangerous || Route.Recipe != Recipe || !Run.ChooseRoute(Route.Id, Error)) { continue; }
        FExpeditionSaveData Data;
        Data.Current = Run.Snapshot();
        return FExpeditionSave::Write(Path, Data, Error);
    }
    return false;
}

/** A view resumed onto a checkpoint, with enemy turns held so the board stays as written. */
AExpeditionBattleView* ResumeBoard(FAutomationTestBase& Test, UWorld* World, const FString& Path)
{
    AExpeditionBattleView* View = World->SpawnActor<AExpeditionBattleView>();
    if (!Test.TestNotNull(TEXT("Battle view"), View)) { return nullptr; }
    View->ConfigureStorage(Path);
    View->DispatchBeginPlay();
    View->Resume();
    View->HoldEnemyTurns(true);
    View->Tick(0.016f);
    return View;
}

/** What a build placed, comparable across builds. */
struct FPlacement
{
    TMap<FIntPoint, int32> Variants;
    TMap<FIntPoint, FVector> Spots;
    TArray<TTuple<FIntPoint, int32, int32>> Dressing;
    int32 Obstacles = 0, Arches = 0;

    static FPlacement Of(const AExpeditionBattleView& View)
    {
        FPlacement Placement;
        Placement.Variants = View.ObstacleVariants();
        Placement.Spots = View.ObstacleSpots();
        for (const FPlacedDressing& Placed : View.PlacedDressing()) { Placement.Dressing.Add(MakeTuple(Placed.Plan.Cell, Placed.Plan.Variant, static_cast<int32>(Placed.Plan.Kind))); }
        Placement.Obstacles = View.PlacedObstacles();
        Placement.Arches = View.PlacedArches();
        return Placement;
    }
    bool operator==(const FPlacement& Other) const
    {
        return Variants.OrderIndependentCompareEqual(Other.Variants) && Spots.OrderIndependentCompareEqual(Other.Spots)
            && Dressing == Other.Dressing && Obstacles == Other.Obstacles && Arches == Other.Arches;
    }
};

struct FRootSeededOutcome
{
    FCombatSnapshot Battle;
    FString Checkpoint;
    int32 Commands = 0;
};

// The rules-only policy of the board metric test on the Stream trail (offer 0), under a presentation variant:
// 0 the shipped theme, 1 primitives, 2 a copy of the theme with the Root kit switched off.
FRootSeededOutcome PlayRoot(FAutomationTestBase& Test, UWorld* World, const FString& Directory, int32 Variant)
{
    FRootSeededOutcome Outcome;
    AExpeditionBattleView* View = World->SpawnActor<AExpeditionBattleView>();
    if (!Test.TestNotNull(TEXT("Battle view"), View)) { return Outcome; }
    const FString Path = FPaths::Combine(Directory, FString::FromInt(Variant), TEXT("current.json"));
    View->ConfigureStorage(Path);
    View->DispatchBeginPlay();
    TStrongObjectPtr<UDiceRollTheme> Off;
    if (Variant == 1) { View->UsePrimitives(); }
    else if (Variant == 2)
    {
        if (const UDiceRollTheme* Shipped = ShippedTheme())
        {
            Off.Reset(TransientThemeCopy(Shipped));
            Off->bRootMetricKit = false;
            View->UseTheme(Off.Get());
        }
    }
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

int32 RouteRecipe(const AExpeditionBattleView& View) { return View.Progress().History.IsEmpty() ? 0 : View.Progress().History.Last().Route.Recipe; }
int32 RouteSeed(const AExpeditionBattleView& View) { return View.Progress().History.IsEmpty() ? 7 : View.Progress().History.Last().Route.EncounterSeed; }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRootKitPlacementTest, "Project.Expedition.Presentation.RootKitPlacementIsDeterministic",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRootKitPlacementTest::RunTest(const FString& Parameters)
{
    const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation/RootKitPlacement"), FGuid::NewGuid().ToString());
    for (int32 Recipe = 0; Recipe < 3; ++Recipe)
    {
        const FString Path = FPaths::Combine(Directory, FString::FromInt(Recipe), TEXT("current.json"));
        if (!TestTrue(FString::Printf(TEXT("A seed gives a Stream trail board with recipe %d"), Recipe), WriteRootCheckpoint(Path, Recipe))) { continue; }
        // The same checkpoint built in two worlds draws the same kit pieces on the same cells.
        FPlacement First, Second;
        int32 Seed = 0;
        TArray<FIntPoint> Blocked;
        bool bKit = false;
        for (int32 Build = 0; Build < 2; ++Build)
        {
            UWorld* World = MakeRootWorld();
            if (!TestNotNull(TEXT("Transient world"), World)) { return false; }
            if (AExpeditionBattleView* View = ResumeBoard(*this, World, Path))
            {
                TestEqual(TEXT("The checkpoint's board is the forest recipe asked for"), RouteRecipe(*View), Recipe);
                bKit = View->UsesRootKit();
                (Build == 0 ? First : Second) = FPlacement::Of(*View);
                if (Build == 0)
                {
                    Seed = RouteSeed(*View);
                    Blocked = View->Rules().Snapshot().Encounter.BlockedCells;
                    TestFalse(TEXT("A forest board never uses the Vigil kit"), View->UsesVigilKit());
                }
            }
            DestroyRootWorld(World);
        }
        TestTrue(TEXT("The same seeded board places the same pieces twice"), First == Second);
        if (!bKit) { AddInfo(TEXT("The theme's Root kit is not switched on (no bRootMetricKit); the Root placement checks were skipped.")); continue; }

        // No blocked cell is left without its blocker: every cell has a piece (an arch counts for both of its cells).
        TestEqual(TEXT("Every blocked cell has an obstacle spot"), First.Spots.Num(), Blocked.Num());
        TestEqual(TEXT("Pieces and arches cover every blocked cell exactly once"), First.Obstacles + First.Arches, Blocked.Num());
        for (const FIntPoint& Cell : Blocked) { TestTrue(TEXT("A blocked cell has an obstacle piece"), First.Spots.Contains(Cell)); }
        // The variant of a cell is the pure rule of (seed, recipe, cell); a cell under an arch records -1.
        int32 Counted = 0;
        for (const TPair<FIntPoint, int32>& Pair : First.Variants)
        {
            if (Pair.Value < 0) { ++Counted; continue; }
            TestTrue(TEXT("A Root obstacle uses one of the three variants"), Pair.Value < 3);
            TestEqual(TEXT("The variant follows the shared placement rule"), Pair.Value, VigilKit::VariantIndex(Seed, Recipe, Pair.Key, 3));
        }
        TestEqual(TEXT("Cells under arches are the arches' two cells each"), Counted, 2 * First.Arches);
        if (!VigilKit::ArchesFor(Recipe)) { TestEqual(TEXT("A stones board places no arches"), First.Arches, 0); }
        else
        {
            // The Root's pillar and wall recipes take the arch, like the Vigil's: it stands over every adjacent blocked pair.
            const int32 Pairs = VigilKit::ArchPairs(Blocked).Num();
            TestTrue(TEXT("Arches only stand over adjacent blocked pairs"), First.Arches <= Pairs);
            if (Recipe == 2) { TestTrue(TEXT("The broken wall has adjacent cells, so the arch is placed"), First.Arches > 0 && First.Arches == Pairs); }
        }
        // Dressing is the shared plan: same cells, same variants, for this seed and the theme's counts (8 scatter, 3 mist, 2 shafts).
        VigilKit::FDressingKinds Kinds;
        Kinds.Scatter = 8; Kinds.Mist = 3; Kinds.Shaft = 2;
        UWorld* World = MakeRootWorld();
        if (!TestNotNull(TEXT("Transient world"), World)) { return false; }
        if (AExpeditionBattleView* View = ResumeBoard(*this, World, Path))
        {
            const FCombatEncounter& Encounter = View->Rules().Snapshot().Encounter;
            const TArray<VigilKit::FDressing> Plan = VigilKit::PlanDressing(Encounter.Width, Encounter.Height, Seed, Kinds);
            TestEqual(TEXT("The whole plan is placed"), View->PlacedDressing().Num(), Plan.Num());
            for (int32 Index = 0; Index < FMath::Min(Plan.Num(), View->PlacedDressing().Num()); ++Index)
            {
                TestTrue(TEXT("A piece stands on its planned cell"), View->PlacedDressing()[Index].Plan.Cell == Plan[Index].Cell);
                TestEqual(TEXT("A piece is its planned variant"), View->PlacedDressing()[Index].Plan.Variant, Plan[Index].Variant);
            }
            // The kit is presentation: another cell size moves the pieces with the metric but changes none of the choices.
            const FBoardMetric Original = View->Metric();
            View->SetBoardMetric(FBoardMetric(180.0f, 190.0f));
            TestTrue(TEXT("The choices are the same at another cell size"), FPlacement::Of(*View).Variants.OrderIndependentCompareEqual(First.Variants));
            View->SetBoardMetric(Original);
            TestTrue(TEXT("The choices are the same after another rebuild"), FPlacement::Of(*View) == First);
        }
        DestroyRootWorld(World);
    }
    IFileManager::Get().DeleteDirectory(*Directory, false, true);
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRootKitSelectionTest, "Project.Expedition.Presentation.RootKitIsSelectedPerWorld",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRootKitSelectionTest::RunTest(const FString& Parameters)
{
    // Pure: a theme with nothing set has no kit in either world, and the two worlds read their own properties.
    UDiceRollTheme* Theme = NewObject<UDiceRollTheme>(GetTransientPackage());
    TestFalse(TEXT("A fresh theme has no Root kit"), Theme->KitFor(true).bMetric);
    TestFalse(TEXT("A fresh theme has no Vigil kit"), Theme->KitFor(false).bMetric);
    Theme->bRootMetricKit = true;
    TestTrue(TEXT("bRootMetricKit switches the forest kit on"), Theme->KitFor(true).bMetric);
    TestFalse(TEXT("It leaves the Vigil kit off"), Theme->KitFor(false).bMetric);
    Theme->bRootMetricKit = false; Theme->bMetricKit = true;
    TestFalse(TEXT("The Vigil flag leaves the Root kit off"), Theme->KitFor(true).bMetric);
    TestTrue(TEXT("It switches the Vigil kit on"), Theme->KitFor(false).bMetric);
    const FDiceRollWorldKit Root = Theme->KitFor(true), Vigil = Theme->KitFor(false);
    TestTrue(TEXT("The Root kit points at the Root properties"), Root.Occluders == &Theme->RootOccluders && Root.Arch == &Theme->RootArchTwoCell
        && Root.Scatter == &Theme->RootScatter && Root.Mist == &Theme->RootMistCards && Root.Shafts == &Theme->RootLightShafts);
    TestTrue(TEXT("The Vigil kit points at the Vigil properties"), Vigil.Occluders == &Theme->VigilOccluders && Vigil.Arch == &Theme->ArchTwoCell
        && Vigil.Scatter == &Theme->Scatter && Vigil.Mist == &Theme->MistCards && Vigil.Shafts == &Theme->LightShafts);
    TestTrue(TEXT("Recipe 1 is the pillar variants"), &Root.Variants(1) == &Theme->RootPillarVariants && &Vigil.Variants(1) == &Theme->PillarVariants);
    TestTrue(TEXT("Recipe 2 is the wall variants"), &Root.Variants(2) == &Theme->RootWallVariants && &Vigil.Variants(2) == &Theme->WallVariants);
    TestTrue(TEXT("Anything else is the stones variants"), &Root.Variants(0) == &Theme->RootStonesVariants && &Vigil.Variants(0) == &Theme->StonesVariants && &Root.Variants(9) == &Theme->RootStonesVariants);
    TestTrue(TEXT("An unset kit entry loads nothing"), Root.Occluders->IsNull() && UDiceRollTheme::Load(*Root.Arch) == nullptr);
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRootKitFallbackTest, "Project.Expedition.Presentation.RootKitFallsBackToR001",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRootKitFallbackTest::RunTest(const FString& Parameters)
{
    const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation/RootKitFallback"), FGuid::NewGuid().ToString());
    const FString Path = FPaths::Combine(Directory, TEXT("current.json"));
    if (!TestTrue(TEXT("A seed gives a broken-wall Stream trail board"), WriteRootCheckpoint(Path, 2))) { return false; }
    const UDiceRollTheme* Shipped = ShippedTheme();
    if (!Shipped) { AddInfo(TEXT("The shipped theme asset is not available here.")); return true; }
    UWorld* World = MakeRootWorld();
    if (!TestNotNull(TEXT("Transient world"), World)) { return false; }
    if (AExpeditionBattleView* View = ResumeBoard(*this, World, Path))
    {
        const FCombatEncounter& Encounter = View->Rules().Snapshot().Encounter;
        const float ThemeScale = View->Metric().Scale();
        const bool bOn = View->UsesRootKit();
        if (!bOn) { AddInfo(TEXT("The shipped theme's Root kit is not switched on; fallbacks are checked against the kit-less theme only.")); }
        else
        {
            TestEqual(TEXT("With the kit the world stands at the true metric"), View->WorldMeshScale(), 1.0f, 0.001f);
            TestTrue(TEXT("The kit placed obstacles, dressing and an occluder"), View->PlacedObstacles() > 0 && !View->PlacedDressing().IsEmpty() && View->HasOccluder());
        }

        // The kit switched off (the plain import before the world is replaced): the r001 world, scaled by the board metric.
        TStrongObjectPtr<UDiceRollTheme> Off(TransientThemeCopy(Shipped));
        Off->bRootMetricKit = false;
        View->UseTheme(Off.Get());
        View->Tick(0.016f);
        TestFalse(TEXT("No Root kit when it is switched off"), View->UsesRootKit());
        TestFalse(TEXT("No occluder without the kit"), View->HasOccluder());
        TestTrue(TEXT("No kit dressing without the kit"), View->PlacedDressing().IsEmpty());
        TestEqual(TEXT("The r001 world keeps the board metric's scale"), View->WorldMeshScale(), ThemeScale, 0.001f);
        TestEqual(TEXT("An absent occluder never fades anything"), View->OccluderOpacityAt(FVector(1.0f, 2.0f, 3.0f)), 1.0f);
        if (!Shipped->bMetricKit)
        {
            TestTrue(TEXT("The r001 obstacles are not kit pieces"), View->ObstacleVariants().IsEmpty());
        }
        else
        {
            // With the Vigil kit on, a kit-less forest board keeps placing the Vigil variants without arches, as before the Root kit.
            TestEqual(TEXT("A kit-less forest board places no arches"), View->PlacedArches(), 0);
            for (const FIntPoint& Cell : Encounter.BlockedCells) { TestTrue(TEXT("Every blocked cell still has its blocker"), View->ObstacleSpots().Contains(Cell)); }
        }
        TestEqual(TEXT("The board is the same one"), View->Rules().Snapshot().Encounter.BlockedCells.Num(), Encounter.BlockedCells.Num());

        if (bOn)
        {
            // Each missing piece falls back on its own. No occluder: the world and obstacles remain.
            TStrongObjectPtr<UDiceRollTheme> NoOccluder(TransientThemeCopy(Shipped));
            NoOccluder->RootOccluders.Reset();
            View->UseTheme(NoOccluder.Get());
            TestTrue(TEXT("Without an occluder the kit is still on"), View->UsesRootKit() && !View->HasOccluder() && View->PlacedObstacles() > 0);
            // No dressing at all.
            TStrongObjectPtr<UDiceRollTheme> NoDressing(TransientThemeCopy(Shipped));
            NoDressing->RootScatter.Reset(); NoDressing->RootMistCards.Reset(); NoDressing->RootLightShafts.Reset();
            View->UseTheme(NoDressing.Get());
            TestTrue(TEXT("Without dressing meshes none is placed, and the obstacles remain"), View->UsesRootKit() && View->PlacedDressing().IsEmpty() && View->PlacedObstacles() > 0);
            // No arch mesh: the adjacent pair is two ordinary pieces, each blocked cell still covered.
            TStrongObjectPtr<UDiceRollTheme> NoArch(TransientThemeCopy(Shipped));
            NoArch->RootArchTwoCell.Reset();
            View->UseTheme(NoArch.Get());
            TestEqual(TEXT("Without the arch mesh no arch is placed"), View->PlacedArches(), 0);
            for (const FIntPoint& Cell : Encounter.BlockedCells) { TestTrue(TEXT("Every blocked cell still has its blocker without arches"), View->ObstacleSpots().Contains(Cell)); }
            // No Root obstacle variants at all: every cell still has a blocker (r001 art or primitives), and the world stays at the true metric.
            TStrongObjectPtr<UDiceRollTheme> NoVariants(TransientThemeCopy(Shipped));
            NoVariants->RootPillarVariants.Reset(); NoVariants->RootWallVariants.Reset(); NoVariants->RootStonesVariants.Reset();
            View->UseTheme(NoVariants.Get());
            TestTrue(TEXT("Without obstacle variants the kit world is still placed"), View->UsesRootKit());
            TestEqual(TEXT("It stays at the true metric"), View->WorldMeshScale(), 1.0f, 0.001f);
            TestEqual(TEXT("No Root arch stands without Root variants"), View->PlacedArches(), 0);
            for (const FIntPoint& Cell : Encounter.BlockedCells) { TestTrue(TEXT("Every blocked cell still has its blocker without Root variants"), View->ObstacleSpots().Contains(Cell)); }
            // A single variant that loads covers every cell: the choice falls to the next one that loaded.
            TStrongObjectPtr<UDiceRollTheme> OneVariant(TransientThemeCopy(Shipped));
            if (OneVariant->RootWallVariants.Num() == 3)
            {
                OneVariant->RootWallVariants[0].Reset(); OneVariant->RootWallVariants[1].Reset();
                View->UseTheme(OneVariant.Get());
                for (const FIntPoint& Cell : Encounter.BlockedCells) { TestTrue(TEXT("Every blocked cell has a blocker with only the third variant"), View->ObstacleSpots().Contains(Cell)); }
                for (const TPair<FIntPoint, int32>& Pair : View->ObstacleVariants()) { TestTrue(TEXT("Only the loaded variant (or an arch) is used"), Pair.Value == 2 || Pair.Value == -1); }
            }
        }
        // The primitive presentation builds a plain board with none of it.
        View->UsePrimitives();
        View->Tick(0.016f);
        TestFalse(TEXT("No Root kit on a primitive board"), View->UsesRootKit());
        TestFalse(TEXT("No occluder on a primitive board"), View->HasOccluder());
        TestTrue(TEXT("No kit pieces on a primitive board"), View->PlacedDressing().IsEmpty() && View->ObstacleVariants().IsEmpty());
        TestEqual(TEXT("A primitive board has no themed world"), View->WorldMeshScale(), 0.0f);
    }
    DestroyRootWorld(World);
    IFileManager::Get().DeleteDirectory(*Directory, false, true);
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRootKitMetricTest, "Project.Expedition.Presentation.RootKitStandsAtTrueMetricAndFigures180",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRootKitMetricTest::RunTest(const FString& Parameters)
{
    const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation/RootKitMetric"), FGuid::NewGuid().ToString());
    const FString Path = FPaths::Combine(Directory, TEXT("current.json"));
    if (!TestTrue(TEXT("A seed gives a broken-wall Stream trail board"), WriteRootCheckpoint(Path, 2))) { return false; }
    UWorld* World = MakeRootWorld();
    if (!TestNotNull(TEXT("Transient world"), World)) { return false; }
    if (AExpeditionBattleView* View = ResumeBoard(*this, World, Path))
    {
        if (!View->UsesRootKit()) { AddInfo(TEXT("The theme's Root kit is not switched on; the Root metric checks were skipped (BoardMetricTest still measures the figures on this forest board).")); }
        else
        {
            const FCombatEncounter& Encounter = View->Rules().Snapshot().Encounter;
            for (const FBoardMetric Metric : {FBoardMetric(150.0f, 180.0f), FBoardMetric(200.0f, 180.0f)})
            {
                View->SetBoardMetric(Metric);
                View->Tick(0.016f);
                // Authored at the true cell, so the world stays unscaled whatever the root's scale is.
                TestEqual(TEXT("The Root world is placed unscaled"), View->WorldMeshScale(), 1.0f, 0.001f);
                const FBoardMetric& Live = View->Metric();
                for (const FIntPoint& Cell : Encounter.BlockedCells)
                {
                    const FVector* Spot = View->ObstacleSpots().Find(Cell);
                    if (!TestNotNull(TEXT("A blocked cell has an obstacle piece"), Spot)) { continue; }
                    const int32* Variant = View->ObstacleVariants().Find(Cell);
                    if (Variant && *Variant >= 0)
                    {
                        TestEqual(TEXT("The obstacle stands on its cell: X"), static_cast<float>(Spot->X), Cell.X * Live.CellCm, 0.01f);
                        TestEqual(TEXT("The obstacle stands on its cell: Y"), static_cast<float>(Spot->Y), Cell.Y * Live.CellCm, 0.01f);
                    }
                    TestEqual(TEXT("The obstacle stands on the board top"), static_cast<float>(Spot->Z), Live.BoardTop(), 0.01f);
                }
                for (const FPlacedDressing& Placed : View->PlacedDressing())
                {
                    TestTrue(TEXT("A dressing piece stands on its planned cell through the metric"), Live.CellAt(Placed.Position) == Placed.Plan.Cell);
                    TestTrue(TEXT("It is off the board by the planned margin"), VigilKit::RingDistance(Placed.Plan.Cell, Encounter.Width, Encounter.Height) >= VigilKit::ScatterMinRing);
                    TestEqual(TEXT("It stands on the board top"), static_cast<float>(Placed.Position.Z), Live.BoardTop(), 0.5f);
                }
            }
            View->SetBoardMetric(FBoardMetric(150.0f, 180.0f));
            View->Tick(0.016f);

            // The figures on a Root r002 board stand 175-185 cm, with the rig root on the cell (as BoardMetricTest).
            int32 Measured = 0, Standing = 0;
            for (const FCombatantState& Unit : View->Rules().Snapshot().Encounter.Combatants)
            {
                if (Unit.Life != ECombatLife::Conscious) { continue; }
                ++Standing;
                USceneComponent* RigRoot = View->UnitRigRoot(Unit.Id);
                if (!RigRoot) { continue; }
                TArray<USceneComponent*> Children;
                RigRoot->GetChildrenComponents(true, Children);
                double Highest = View->Metric().BoardTop();
                for (USceneComponent* Child : Children)
                {
                    const UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(Child);
                    if (Primitive && Primitive->IsVisible())
                    {
                        Highest = FMath::Max(Highest, Primitive->Bounds.GetBox().Max.Z);
                        // Silhouettes: every figure part (the skinned body included) writes custom depth, so a hidden figure shows through the arches.
                        TestTrue(FString::Printf(TEXT("%s part writes custom depth on a Root board"), *Unit.Name), Primitive->bRenderCustomDepth);
                    }
                }
                const double Height = Highest - View->Metric().BoardTop();
                TestTrue(FString::Printf(TEXT("%s stands 175-185 cm on a Root r002 board (measured %.1f)"), *Unit.Name, Height), Height >= 175.0f && Height <= 185.0f);
                TestEqual(FString::Printf(TEXT("%s renders with its side's stencil"), *Unit.Name),
                    View->FigureStencil(Unit.Id), Unit.Side == ECombatSide::Enemy ? AExpeditionBattleView::EnemyStencil : AExpeditionBattleView::PartyStencil);
                ++Measured;
            }
            TestTrue(TEXT("Figures are present"), Standing > 0);
            TestEqual(TEXT("Every standing figure was measured"), Measured, Standing);

            // The occluder fades along the live sight lines and over the board, like the Vigil's.
            if (!View->HasOccluder()) { AddInfo(TEXT("The theme has no Root occluder mesh yet; the fade checks were skipped.")); }
            else
            {
                View->SnapCamera(-90.0f, View->FramedDistance());
                View->Tick(0.016f);
                FVector Eye, Focus, UnitPoint;
                View->OccluderSight(Eye, Focus, UnitPoint);
                const FVector Between = Eye + (Focus - Eye) * 0.5f;
                TestEqual(TEXT("Something between the camera and the focus is faded"), View->OccluderOpacityAt(Between), OccluderFade::MinOpacity, 0.001f);
                TestTrue(TEXT("It is dithered, not hidden"), View->OccluderOpacityAt(Between) > 0.0f);
                TestEqual(TEXT("Something past the focus is opaque"), View->OccluderOpacityAt(Focus + (Focus - Eye) * 0.3f), 1.0f, 0.001f);
            }
        }
    }
    DestroyRootWorld(World);
    IFileManager::Get().DeleteDirectory(*Directory, false, true);
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRootThemeKeepsStateTest, "Project.Expedition.Presentation.RootThemeChoiceLeavesStateUnchanged",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRootThemeKeepsStateTest::RunTest(const FString& Parameters)
{
    // The same seeded run on the Stream trail under the shipped theme (the Root kit when it is on), the primitives and a
    // kit-less copy of the theme: the authoritative snapshot and the checkpoint bytes must match, because art never reaches rules or saves.
    UWorld* World = MakeRootWorld();
    if (!TestNotNull(TEXT("Transient world"), World)) { return false; }
    const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation/RootThemeState"), FGuid::NewGuid().ToString());
    const FRootSeededOutcome Themed = PlayRoot(*this, World, Directory, 0);
    TestTrue(TEXT("The run played commands"), Themed.Commands > 0);
    TestFalse(TEXT("The run wrote a checkpoint"), Themed.Checkpoint.IsEmpty());
    for (const int32 Variant : {1, 2})
    {
        const FRootSeededOutcome Other = PlayRoot(*this, World, Directory, Variant);
        TestEqual(FString::Printf(TEXT("Variant %d plays the same number of commands"), Variant), Other.Commands, Themed.Commands);
        TestTrue(FString::Printf(TEXT("Variant %d: the battle snapshot is identical"), Variant), FCombatSnapshot::StaticStruct()->CompareScriptStruct(&Themed.Battle, &Other.Battle, 0));
        TestTrue(FString::Printf(TEXT("Variant %d: the checkpoint bytes are identical"), Variant), Other.Checkpoint == Themed.Checkpoint);
    }
    DestroyRootWorld(World);
    IFileManager::Get().DeleteDirectory(*Directory, false, true);
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRootThemeDataTest, "Project.Expedition.Presentation.RootThemeDataIsComplete",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRootThemeDataTest::RunTest(const FString& Parameters)
{
    const UDiceRollTheme* Theme = ShippedTheme();
    if (!Theme) { AddInfo(TEXT("The shipped theme asset is not available here.")); return true; }
    // The r001 fallbacks stay in place whatever the kit state.
    TestFalse(TEXT("The Root world entry is set (r001 until the world is replaced)"), Theme->RootWorld.IsNull());
    TestFalse(TEXT("The r001 obstacle fallbacks are set"), Theme->Pillar.IsNull() || Theme->Wall.IsNull() || Theme->Stones.IsNull());
    if (!Theme->bRootMetricKit) { AddInfo(TEXT("The Root kit is not switched on in the theme yet; the r002 entries were not checked.")); return !HasAnyErrors(); }
    TestNotNull(TEXT("Root world"), UDiceRollTheme::Load(Theme->RootWorld));
    TestNotNull(TEXT("Root occluder mesh"), UDiceRollTheme::Load(Theme->RootOccluders));
    TestNotNull(TEXT("Root 2-cell arch"), UDiceRollTheme::Load(Theme->RootArchTwoCell));
    const TPair<const TArray<TSoftObjectPtr<UStaticMesh>>*, int32> Sets[] = {
        {&Theme->RootPillarVariants, 3}, {&Theme->RootWallVariants, 3}, {&Theme->RootStonesVariants, 3},
        {&Theme->RootScatter, 8}, {&Theme->RootMistCards, 3}, {&Theme->RootLightShafts, 2}};
    for (const TPair<const TArray<TSoftObjectPtr<UStaticMesh>>*, int32>& Set : Sets)
    {
        TestEqual(TEXT("A Root kit set has its full variant count"), Set.Key->Num(), Set.Value);
        for (const TSoftObjectPtr<UStaticMesh>& Entry : *Set.Key) { TestNotNull(TEXT("A Root kit mesh loads"), UDiceRollTheme::Load(Entry)); }
    }
    // The Vigil kit is independent of the Root's: neither flag implies the other.
    if (Theme->bMetricKit) { TestTrue(TEXT("The Vigil kit is still complete"), Theme->PillarVariants.Num() == 3 && Theme->WallVariants.Num() == 3 && Theme->StonesVariants.Num() == 3); }
    return !HasAnyErrors();
}
#endif
