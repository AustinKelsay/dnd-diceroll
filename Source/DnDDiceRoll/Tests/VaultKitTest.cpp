#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Components/LightComponent.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Persistence/ExpeditionSave.h"
#include "Presentation/BoardMetric.h"
#include "Presentation/BrazierRing.h"
#include "Presentation/DiceRollTheme.h"
#include "Presentation/ExpeditionBattleView.h"
#include "Presentation/VigilKit.h"
#include "RenderingThread.h"
#include "UObject/GarbageCollection.h"
#include "UObject/StrongObjectPtr.h"
#include "ExpeditionTestSupport.h"

/**
 * The Vault world kit r002 (#149), the finale's own world, mirroring RootKitTest: which kit a board theme asks for, the Vault kit's
 * fallback to the Vigil kit when any Vault entry is missing, the metric flag, deterministic placement with its own decoration seed, no
 * decoration on a playable cell on any generated board size, and that the choice of presentation never changes the snapshot or the RNG
 * (ADR-0001). The board-level checks need the imported kit and say so when the shipped theme has none: the plain import adds the Vault
 * pieces and sets bVaultMetricKit.
 */
namespace
{
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

UWorld* MakeVaultWorld()
{
    const UWorld::InitializationValues Values = UWorld::InitializationValues()
        .AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false);
    return UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
}

void DestroyVaultWorld(UWorld* World)
{
    World->DestroyWorld(false);
    FlushRenderingCommands();
    CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
}

const UDiceRollTheme* ShippedTheme()
{
    return LoadObject<UDiceRollTheme>(nullptr, TEXT("/Game/DnDDiceRoll/Theme/DA_DiceRollTheme.DA_DiceRollTheme"), nullptr, LOAD_Quiet | LOAD_NoWarn);
}

/** A theme whose Vault entries are all set to a placeholder that exists (the engine cube; completeness asks that every entry is set and its package exists). */
void FillVault(UDiceRollTheme& Theme)
{
    const TSoftObjectPtr<UStaticMesh> Placeholder{FSoftObjectPath(TEXT("/Engine/BasicShapes/Cube.Cube"))};
    auto Fill = [&](TArray<TSoftObjectPtr<UStaticMesh>>& Meshes, int32 Count) { Meshes.Init(Placeholder, Count); };
    Theme.bVaultMetricKit = true;
    Theme.VaultWorld = Placeholder; Theme.VaultOccluders = Placeholder; Theme.VaultArchTwoCell = Placeholder;
    Fill(Theme.VaultPillarVariants, 3); Fill(Theme.VaultWallVariants, 3); Fill(Theme.VaultStonesVariants, 3);
    Fill(Theme.VaultScatter, 8); Fill(Theme.VaultMistCards, 3); Fill(Theme.VaultLightShafts, 2);
}

/** A checkpoint at the first fight of the finale (encounter 5) of the first seed that reaches it with the party acting first. */
bool WriteFinaleCheckpoint(const FString& Path, int32 Recipe, int32* FoundSeed = nullptr)
{
    for (int32 Seed = 1; Seed < 200; ++Seed)
    {
        FExpeditionRun Run;
        if (!ExpeditionTestSupport::ReachFinale(Run, Seed, Recipe)) { continue; }
        FExpeditionSaveData Data;
        Data.Current = Run.Snapshot();
        FString Error;
        if (FoundSeed) { *FoundSeed = Seed; }
        return FExpeditionSave::Write(Path, Data, Error);
    }
    return false;
}

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

struct FFinaleOutcome
{
    FCombatSnapshot Battle;
    FString Checkpoint;
    int32 Commands = 0;
    bool bVaultKit = false;
};

/** The rules-only policy of RootKitTest on the finale checkpoint, under a presentation variant: 0 the shipped theme, 1 primitives, 2 a copy with the Vault kit off. */
FFinaleOutcome PlayFinale(FAutomationTestBase& Test, UWorld* World, const FString& Directory, const FString& Source, int32 Variant)
{
    FFinaleOutcome Outcome;
    AExpeditionBattleView* View = World->SpawnActor<AExpeditionBattleView>();
    if (!Test.TestNotNull(TEXT("Battle view"), View)) { return Outcome; }
    const FString Path = FPaths::Combine(Directory, FString::FromInt(Variant), TEXT("current.json"));
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
    IFileManager::Get().Copy(*Path, *Source);
    View->ConfigureStorage(Path);
    View->DispatchBeginPlay();
    TStrongObjectPtr<UDiceRollTheme> Off;
    if (Variant == 1) { View->UsePrimitives(); }
    else if (Variant == 2)
    {
        if (const UDiceRollTheme* Shipped = ShippedTheme())
        {
            Off.Reset(TransientThemeCopy(Shipped));
            Off->bVaultMetricKit = false;
            View->UseTheme(Off.Get());
        }
    }
    View->Resume();
    View->HoldEnemyTurns(true);
    View->Tick(0.016f);
    Outcome.bVaultKit = View->UsesVaultKit();
    for (int32 Step = 0; Step < 40 && View->InCombat(); ++Step)
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
    return Outcome;
}

int32 RouteRecipe(const AExpeditionBattleView& View) { return View.Progress().History.IsEmpty() ? 0 : View.Progress().History.Last().Route.Recipe; }
int32 RouteSeed(const AExpeditionBattleView& View) { return View.Progress().History.IsEmpty() ? 7 : View.Progress().History.Last().Route.EncounterSeed; }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVaultKitSelectionTest, "Project.Expedition.Presentation.VaultKitIsSelectedAndFallsBackToVigil",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FVaultKitSelectionTest::RunTest(const FString& Parameters)
{
    // Which kit a board theme asks for: Meadow the Root, Ruins the Vigil, the finale the Vault. Old behaviour for the first two.
    TestTrue(TEXT("Meadow asks for the Root kit"), AExpeditionBattleView::KitKindFor(EBoardTheme::Meadow) == EDiceRollKit::Root);
    TestTrue(TEXT("Ruins asks for the Vigil kit"), AExpeditionBattleView::KitKindFor(EBoardTheme::Ruins) == EDiceRollKit::Vigil);
    TestTrue(TEXT("The finale asks for the Vault kit"), AExpeditionBattleView::KitKindFor(EBoardTheme::Vault) == EDiceRollKit::Vault);

    // Pure: a theme with nothing set has no Vault kit, and the Vault request returns the Vigil kit, as before the Vault existed.
    UDiceRollTheme* Theme = NewObject<UDiceRollTheme>(GetTransientPackage());
    TestFalse(TEXT("A fresh theme's Vault kit is incomplete"), Theme->VaultKitComplete());
    FDiceRollWorldKit Kit = Theme->KitFor(EDiceRollKit::Vault);
    TestTrue(TEXT("The Vault request falls back to the Vigil kit"), Kit.Kind == EDiceRollKit::Vigil && Kit.World == &Theme->VigilWorld && Kit.Occluders == &Theme->VigilOccluders);
    TestFalse(TEXT("The fallback is the Vigil kit's own metric flag (off here)"), Kit.bMetric);
    Theme->bMetricKit = true;
    TestTrue(TEXT("It follows the Vigil flag"), Theme->KitFor(EDiceRollKit::Vault).bMetric);
    Theme->bMetricKit = false;
    TestTrue(TEXT("The old requests are unchanged: Vigil"), Theme->KitFor(EDiceRollKit::Vigil).Kind == EDiceRollKit::Vigil);
    TestTrue(TEXT("The old requests are unchanged: Root"), Theme->KitFor(EDiceRollKit::Root).Kind == EDiceRollKit::Root && Theme->KitFor(EDiceRollKit::Root).World == &Theme->RootWorld);

    // The metric flag switches the kit on, and only a complete set of entries is the Vault kit.
    FillVault(*Theme);
    TestTrue(TEXT("Every entry set and the flag on is a complete kit"), Theme->VaultKitComplete());
    Kit = Theme->KitFor(EDiceRollKit::Vault);
    TestTrue(TEXT("The Vault request returns the Vault kit"), Kit.Kind == EDiceRollKit::Vault && Kit.bMetric);
    TestTrue(TEXT("It points at the Vault properties"), Kit.World == &Theme->VaultWorld && Kit.Occluders == &Theme->VaultOccluders && Kit.Arch == &Theme->VaultArchTwoCell
        && Kit.Scatter == &Theme->VaultScatter && Kit.Mist == &Theme->VaultMistCards && Kit.Shafts == &Theme->VaultLightShafts);
    TestTrue(TEXT("Recipe 1 is the pillar variants"), &Kit.Variants(1) == &Theme->VaultPillarVariants);
    TestTrue(TEXT("Recipe 2 is the wall variants"), &Kit.Variants(2) == &Theme->VaultWallVariants);
    TestTrue(TEXT("Anything else is the stones variants"), &Kit.Variants(0) == &Theme->VaultStonesVariants && &Kit.Variants(9) == &Theme->VaultStonesVariants);
    TestFalse(TEXT("The Vault flag leaves the Root kit alone"), Theme->KitFor(EDiceRollKit::Root).bMetric);
    TestFalse(TEXT("The Vault flag leaves the Vigil kit alone"), Theme->KitFor(EDiceRollKit::Vigil).bMetric);

    // Any missing Vault asset sends the finale back to the Vigil kit.
    auto Falls = [&](const TCHAR* What, TFunctionRef<void(UDiceRollTheme&)> Remove)
    {
        UDiceRollTheme* Broken = NewObject<UDiceRollTheme>(GetTransientPackage());
        FillVault(*Broken);
        Remove(*Broken);
        TestFalse(FString::Printf(TEXT("Without %s the kit is incomplete"), What), Broken->VaultKitComplete());
        TestTrue(FString::Printf(TEXT("Without %s the finale draws the Vigil kit"), What), Broken->KitFor(EDiceRollKit::Vault).Kind == EDiceRollKit::Vigil);
    };
    Falls(TEXT("the metric flag"), [](UDiceRollTheme& T) { T.bVaultMetricKit = false; });
    Falls(TEXT("the world"), [](UDiceRollTheme& T) { T.VaultWorld = nullptr; });
    Falls(TEXT("the occluders"), [](UDiceRollTheme& T) { T.VaultOccluders = nullptr; });
    Falls(TEXT("the arch"), [](UDiceRollTheme& T) { T.VaultArchTwoCell = nullptr; });
    Falls(TEXT("a pillar variant"), [](UDiceRollTheme& T) { T.VaultPillarVariants[1] = nullptr; });
    Falls(TEXT("a wall variant"), [](UDiceRollTheme& T) { T.VaultWallVariants.Pop(); });
    Falls(TEXT("a stones variant"), [](UDiceRollTheme& T) { T.VaultStonesVariants[2] = nullptr; });
    Falls(TEXT("a scatter piece"), [](UDiceRollTheme& T) { T.VaultScatter[7] = nullptr; });
    Falls(TEXT("a mist card"), [](UDiceRollTheme& T) { T.VaultMistCards.Empty(); });
    Falls(TEXT("a light shaft"), [](UDiceRollTheme& T) { T.VaultLightShafts[0] = nullptr; });
    // A reference to a deleted or unknown package is as good as unset: the entry is non-empty but dangling.
    const TSoftObjectPtr<UStaticMesh> Dangling{FSoftObjectPath(TEXT("/Game/DnDDiceRoll/Theme/Worlds/SM_Deleted_Vault_Piece.SM_Deleted_Vault_Piece"))};
    Falls(TEXT("a dangling world"), [&](UDiceRollTheme& T) { T.VaultWorld = Dangling; });
    Falls(TEXT("a dangling occluder"), [&](UDiceRollTheme& T) { T.VaultOccluders = Dangling; });
    Falls(TEXT("a dangling arch"), [&](UDiceRollTheme& T) { T.VaultArchTwoCell = Dangling; });
    Falls(TEXT("a dangling pillar variant"), [&](UDiceRollTheme& T) { T.VaultPillarVariants[2] = Dangling; });
    Falls(TEXT("a dangling wall variant"), [&](UDiceRollTheme& T) { T.VaultWallVariants[0] = Dangling; });
    Falls(TEXT("a dangling stones variant"), [&](UDiceRollTheme& T) { T.VaultStonesVariants[1] = Dangling; });
    Falls(TEXT("a dangling scatter piece"), [&](UDiceRollTheme& T) { T.VaultScatter[4] = Dangling; });
    Falls(TEXT("a dangling mist card"), [&](UDiceRollTheme& T) { T.VaultMistCards[1] = Dangling; });
    Falls(TEXT("a dangling light shaft"), [&](UDiceRollTheme& T) { T.VaultLightShafts[1] = Dangling; });

    // The shipped theme: the flag is set only with a complete kit (the import script sets it only when the world carries the r002 tag).
    // This test is about the shipped Vault, so it fails when the import has not switched it on.
    const UDiceRollTheme* Shipped = ShippedTheme();
    if (TestNotNull(TEXT("The shipped theme asset"), Shipped))
    {
        TestTrue(TEXT("The shipped theme's Vault kit is switched on (run the theme import)"), Shipped->bVaultMetricKit);
        TestTrue(TEXT("The shipped theme's Vault kit is complete and every entry exists"), Shipped->VaultKitComplete());
        TestTrue(TEXT("The shipped theme's Vault request returns the Vault kit"), Shipped->KitFor(EDiceRollKit::Vault).Kind == EDiceRollKit::Vault);
    }
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVaultDecorationTest, "Project.Expedition.Presentation.VaultDecorationNeverStandsOnAPlayableCell",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FVaultDecorationTest::RunTest(const FString& Parameters)
{
    VigilKit::FDressingKinds Kinds;
    Kinds.Scatter = 8; Kinds.Mist = 3; Kinds.Shaft = 2;
    int32 Plans = 0, Moved = 0;
    for (int32 Width = 16; Width <= 18; ++Width)
    {
        for (int32 Height = 11; Height <= 12; ++Height)
        {
            for (int32 Seed = -3; Seed < 60; ++Seed)
            {
                const int32 VaultSeed = VigilKit::KitSeed(Seed, Width, Height, static_cast<int32>(EDiceRollKit::Vault));
                // The Vigil and the Root keep the encounter seed (their boards do not move); the Vault has its own decoration seed.
                TestEqual(TEXT("The Vigil keeps the encounter seed"), VigilKit::KitSeed(Seed, Width, Height, static_cast<int32>(EDiceRollKit::Vigil)), Seed);
                TestEqual(TEXT("The Root keeps the encounter seed"), VigilKit::KitSeed(Seed, Width, Height, static_cast<int32>(EDiceRollKit::Root)), Seed);
                TestEqual(TEXT("The Vault's seed is stable"), VaultSeed, VigilKit::KitSeed(Seed, Width, Height, static_cast<int32>(EDiceRollKit::Vault)));
                if (VaultSeed != Seed) { ++Moved; }
                const TArray<FVector2D> Braziers = BrazierRing::Cells(BrazierRing::Plan(Width, Height, Seed));
                const TArray<VigilKit::FDressing> Plan = VigilKit::PlanDressing(Width, Height, VaultSeed, Kinds, Braziers, BrazierRing::ScatterClearCells);
                ++Plans;
                TestEqual(TEXT("The whole plan is drawn"), Plan.Num(), VigilKit::ScatterCount + VigilKit::MistCount + VigilKit::ShaftCount);
                TSet<FIntPoint> Cells;
                for (const VigilKit::FDressing& Item : Plan)
                {
                    TestFalse(TEXT("No two pieces share a cell"), Cells.Contains(Item.Cell));
                    Cells.Add(Item.Cell);
                    // Never on a playable cell, and never in the gutter cell beside the board: at least two cells out, wander included.
                    TestTrue(TEXT("A piece is two or more cells outside the board"), VigilKit::RingDistance(Item.Cell, Width, Height) >= VigilKit::ScatterMinRing);
                    const FVector2D At = Item.Position();
                    const bool bOutside = At.X < -0.5 - 1.0 || At.X > Width - 0.5 + 1.0 || At.Y < -0.5 - 1.0 || At.Y > Height - 0.5 + 1.0;
                    TestTrue(TEXT("Its drawn position is clear of the board by a cell or more"), bOutside);
                    TestTrue(TEXT("It is clear of every brazier"), !Braziers.ContainsByPredicate([&](const FVector2D& Brazier) { return FVector2D::Distance(Brazier, At) < BrazierRing::ScatterClearCells * 0.99f && Item.Kind == VigilKit::EDressingKind::Scatter; }));
                }
                // Deterministic: the same inputs draw the same plan.
                const TArray<VigilKit::FDressing> Again = VigilKit::PlanDressing(Width, Height, VaultSeed, Kinds, Braziers, BrazierRing::ScatterClearCells);
                bool bSame = Again.Num() == Plan.Num();
                for (int32 Index = 0; bSame && Index < Plan.Num(); ++Index) { bSame = Again[Index].Cell == Plan[Index].Cell && Again[Index].Variant == Plan[Index].Variant && Again[Index].Kind == Plan[Index].Kind; }
                TestTrue(TEXT("The same board and seed draw the same plan"), bSame);
                // The braziers keep the encounter seed (they are the game's, not the kit's) and stay outside the board too.
                for (const FVector2D& Brazier : Braziers) { TestTrue(TEXT("A brazier stands outside the playable cells"), BrazierRing::OutsideBoard(Brazier, Width, Height, 1.0f)); }
            }
        }
    }
    TestTrue(TEXT("Every board size and seed was planned"), Plans == 3 * 2 * 63);
    TestTrue(TEXT("The Vault's seed differs from the encounter seed almost always"), Moved > Plans * 9 / 10);
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVaultKitPlacementTest, "Project.Expedition.Presentation.VaultKitPlacementIsDeterministic",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FVaultKitPlacementTest::RunTest(const FString& Parameters)
{
    const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation/VaultKitPlacement"), FGuid::NewGuid().ToString());
    for (int32 Recipe = 0; Recipe < 3; ++Recipe)
    {
        const FString Path = FPaths::Combine(Directory, FString::FromInt(Recipe), TEXT("current.json"));
        if (!TestTrue(FString::Printf(TEXT("A seed reaches the finale with recipe %d"), Recipe), WriteFinaleCheckpoint(Path, Recipe))) { continue; }
        FPlacement First, Second;
        int32 Seed = 0, Width = 0, Height = 0;
        TArray<FIntPoint> Blocked;
        bool bKit = false;
        for (int32 Build = 0; Build < 2; ++Build)
        {
            UWorld* World = MakeVaultWorld();
            if (!TestNotNull(TEXT("Transient world"), World)) { return false; }
            if (AExpeditionBattleView* View = ResumeBoard(*this, World, Path))
            {
                TestEqual(TEXT("The checkpoint's board is the recipe asked for"), RouteRecipe(*View), Recipe);
                TestTrue(TEXT("It is the finale board"), View->Progress().History.Num() >= FExpeditionRun::EncounterCount);
                bKit = View->UsesVaultKit();
                (Build == 0 ? First : Second) = FPlacement::Of(*View);
                if (Build == 0)
                {
                    Seed = RouteSeed(*View);
                    Width = View->Rules().Snapshot().Encounter.Width; Height = View->Rules().Snapshot().Encounter.Height;
                    Blocked = View->Rules().Snapshot().Encounter.BlockedCells;
                    TestFalse(TEXT("The Vault board never uses the Root kit"), View->UsesRootKit());
                    TestTrue(TEXT("It uses at most one kit"), (View->UsesVigilKit() ? 1 : 0) + (View->UsesVaultKit() ? 1 : 0) <= 1);
                }
            }
            DestroyVaultWorld(World);
        }
        TestTrue(TEXT("The same seeded board places the same pieces twice"), First == Second);
        if (!TestTrue(FString::Printf(TEXT("Recipe %d: the finale board is built from the Vault kit (run the theme import)"), Recipe), bKit)) { continue; }

        TestEqual(TEXT("Every blocked cell has an obstacle spot"), First.Spots.Num(), Blocked.Num());
        TestEqual(TEXT("Pieces and arches cover every blocked cell exactly once"), First.Obstacles + First.Arches, Blocked.Num());
        for (const FIntPoint& Cell : Blocked) { TestTrue(TEXT("A blocked cell has an obstacle piece"), First.Spots.Contains(Cell)); }
        // The variant of a cell is the pure rule of the Vault's decoration seed, the recipe and the cell; a cell under an arch records -1.
        const int32 VaultSeed = VigilKit::KitSeed(Seed, Width, Height, static_cast<int32>(EDiceRollKit::Vault));
        int32 Counted = 0;
        for (const TPair<FIntPoint, int32>& Pair : First.Variants)
        {
            if (Pair.Value < 0) { ++Counted; continue; }
            TestTrue(TEXT("A Vault obstacle uses one of the three variants"), Pair.Value < 3);
            TestEqual(TEXT("The variant follows the shared rule on the Vault's decoration seed"), Pair.Value, VigilKit::VariantIndex(VaultSeed, Recipe, Pair.Key, 3));
        }
        TestEqual(TEXT("Cells under arches are the arches' two cells each"), Counted, 2 * First.Arches);
        if (!VigilKit::ArchesFor(Recipe)) { TestEqual(TEXT("A stones board places no arches"), First.Arches, 0); }
        else { TestTrue(TEXT("Arches only stand over adjacent blocked pairs"), First.Arches <= VigilKit::ArchPairs(Blocked).Num()); }

        // Dressing is the shared plan on the Vault's decoration seed: same cells, same variants, none on a playable cell.
        VigilKit::FDressingKinds Kinds;
        Kinds.Scatter = 8; Kinds.Mist = 3; Kinds.Shaft = 2;
        UWorld* World = MakeVaultWorld();
        if (!TestNotNull(TEXT("Transient world"), World)) { return false; }
        if (AExpeditionBattleView* View = ResumeBoard(*this, World, Path))
        {
            const TArray<VigilKit::FDressing> Plan = VigilKit::PlanDressing(Width, Height, VaultSeed, Kinds,
                BrazierRing::Cells(BrazierRing::Plan(Width, Height, Seed)), BrazierRing::ScatterClearCells);
            TestEqual(TEXT("The whole plan is placed"), View->PlacedDressing().Num(), Plan.Num());
            for (int32 Index = 0; Index < FMath::Min(Plan.Num(), View->PlacedDressing().Num()); ++Index)
            {
                TestTrue(TEXT("A piece stands on its planned cell"), View->PlacedDressing()[Index].Plan.Cell == Plan[Index].Cell);
                TestEqual(TEXT("A piece is its planned variant"), View->PlacedDressing()[Index].Plan.Variant, Plan[Index].Variant);
                TestTrue(TEXT("No piece is on a playable cell"), VigilKit::RingDistance(View->PlacedDressing()[Index].Plan.Cell, Width, Height) >= VigilKit::ScatterMinRing);
            }
            // The instanced board stays under #144's budget of 40 drawing components per rebuild, overlays included; the counts are reported.
            if (TestTrue(TEXT("The Vault board is instanced"), View->UsesInstancedParts()))
            {
                const int32 Parts = View->PresentationComponentCount();
                AddInfo(FString::Printf(TEXT("Vault recipe %d, %dx%d: %d presentation components, %d instanced groups, %d scenery props (%d obstacle pieces, %d arches, %d dressing)"),
                    Recipe, Width, Height, Parts, View->InstancedGroupCount(), View->LogicalPropCount(), View->PlacedObstacles(), View->PlacedArches(), View->PlacedDressing().Num()));
                TestTrue(FString::Printf(TEXT("The instanced Vault board stays under 40 drawing components (%d)"), Parts), Parts < 40);
            }
            // One Waystone light, at the apse; the legacy pedestal is not drawn.
            TestEqual(TEXT("The Vault board has exactly one Waystone light"), View->WaystoneLightCount(), 1);
            TestFalse(TEXT("The legacy Waystone pedestal is not drawn with the Vault kit"), View->WaystoneDrawsPedestal());
            int32 CyanLights = 0;
            TInlineComponentArray<ULightComponent*> Lights;
            View->GetComponents(Lights);
            const FColor Cyan = FLinearColor(0.3f, 0.85f, 1.0f).ToFColor(true);
            for (const ULightComponent* Light : Lights)
            { if (FMath::Abs(int32(Light->LightColor.R) - Cyan.R) < 3 && FMath::Abs(int32(Light->LightColor.G) - Cyan.G) < 3 && FMath::Abs(int32(Light->LightColor.B) - Cyan.B) < 3) { ++CyanLights; } }
            TestEqual(TEXT("No other light is Waystone-coloured"), CyanLights, 1);
            const FVector Centre = View->GetActorTransform().TransformPosition(View->BoardPosition(FVector2D((Width - 1) * 0.5f, (Height - 1) * 0.5f)));
            const FVector At = View->WaystoneLightLocation() - Centre;
            TestTrue(FString::Printf(TEXT("The Waystone light sits at the apse: on the centre line and 12 m behind the board centre (got %.0f, %.0f)"), At.X, At.Y), FMath::Abs(At.X) < 25.0f && FMath::Abs(At.Y - 1205.0f) < 25.0f);
            TestEqual(TEXT("With the Vault kit the world stands at the true metric"), View->WorldMeshScale(), 1.0f, 0.001f);
            TestTrue(TEXT("The kit placed obstacles, dressing and an occluder"), View->PlacedObstacles() > 0 && !View->PlacedDressing().IsEmpty() && View->HasOccluder());
            // The kit is presentation: another cell size moves the pieces with the metric but changes none of the choices.
            const FBoardMetric Original = View->Metric();
            View->SetBoardMetric(FBoardMetric(180.0f, 190.0f));
            TestTrue(TEXT("The choices are the same at another cell size"), FPlacement::Of(*View).Variants.OrderIndependentCompareEqual(First.Variants));
            View->SetBoardMetric(Original);
            TestTrue(TEXT("The choices are the same after another rebuild"), FPlacement::Of(*View) == First);
        }
        DestroyVaultWorld(World);
    }
    IFileManager::Get().DeleteDirectory(*Directory, false, true);
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVaultKitFallbackTest, "Project.Expedition.Presentation.VaultKitFallsBackToTheVigilKit",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FVaultKitFallbackTest::RunTest(const FString& Parameters)
{
    const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation/VaultKitFallback"), FGuid::NewGuid().ToString());
    const FString Path = FPaths::Combine(Directory, TEXT("current.json"));
    if (!TestTrue(TEXT("A seed reaches the finale"), WriteFinaleCheckpoint(Path, -1))) { return false; }
    const UDiceRollTheme* Shipped = ShippedTheme();
    if (!TestNotNull(TEXT("The shipped theme asset"), Shipped)) { return false; }
    UWorld* World = MakeVaultWorld();
    if (!TestNotNull(TEXT("Transient world"), World)) { return false; }
    if (AExpeditionBattleView* View = ResumeBoard(*this, World, Path))
    {
        const bool bOn = View->UsesVaultKit();
        if (!TestTrue(TEXT("The shipped theme draws the Vault kit on the finale (run the theme import)"), bOn)) { DestroyVaultWorld(World); return false; }
        TestFalse(TEXT("The Vault board is not also a Vigil board"), View->UsesVigilKit());
        TestTrue(TEXT("The Vault board has no pedestal and one Waystone light"), !View->WaystoneDrawsPedestal() && View->WaystoneLightCount() == 1);
        TestTrue(TEXT("The shipped theme's Vigil kit is on (the fallback has something to draw)"), Shipped->bMetricKit);

        // The Vault flag off, and then any one entry missing: the finale draws the Vigil kit exactly as it did before the Vault.
        // Cases 3 and 4 are real load failures: the package exists, so the theme calls the kit complete, but the object is not in it. The view loads every
        // piece before it places any, so the whole finale falls back (the engine's load warning is declared).
        const TSoftObjectPtr<UStaticMesh> Dangling{FSoftObjectPath(TEXT("/Game/DnDDiceRoll/Theme/Worlds/SM_Deleted_Vault_Piece.SM_Deleted_Vault_Piece"))};
        const TSoftObjectPtr<UStaticMesh> Unloadable{FSoftObjectPath(TEXT("/Engine/BasicShapes/Cube.NoSuchVaultMesh"))};
        for (int32 Case = 0; Case < 5; ++Case)
        {
            TStrongObjectPtr<UDiceRollTheme> Broken(TransientThemeCopy(Shipped));
            if (Case == 0) { Broken->bVaultMetricKit = false; }
            else if (Case == 1) { Broken->VaultOccluders = nullptr; }
            else if (Case == 2) { Broken->VaultScatter.Pop(); }
            else if (Case == 3) { Broken->VaultOccluders = Dangling; }
            else
            {
                TestTrue(TEXT("An unloadable entry still counts as complete (its package exists)"), (Broken->VaultStonesVariants[1] = Unloadable, Broken->VaultKitComplete()));
                AddExpectedMessage(TEXT("NoSuchVaultMesh"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 0);
            }
            View->UseTheme(Broken.Get());
            View->Tick(0.016f);
            TestFalse(FString::Printf(TEXT("Case %d: no Vault kit when one of its pieces is missing or does not load"), Case), View->UsesVaultKit());
            TestTrue(FString::Printf(TEXT("Case %d: the legacy Waystone pedestal is back"), Case), View->WaystoneDrawsPedestal() && View->WaystoneLightCount() == 1);
            TestTrue(FString::Printf(TEXT("Case %d: nothing of the Vault kit was placed (the dressing is the Vigil's)"), Case), View->UsesVigilKit() == Shipped->bMetricKit);
            TestEqual(TEXT("A finale board without the Vault kit is still a themed board"), View->UsesVigilKit(), Shipped->bMetricKit);
            if (Shipped->bMetricKit)
            {
                TestEqual(TEXT("The Vigil kit's world stands at the true metric"), View->WorldMeshScale(), 1.0f, 0.001f);
                TestTrue(TEXT("The Vigil kit placed its obstacles and occluder"), View->PlacedObstacles() > 0 && View->HasOccluder());
            }
        }
        // The Vault again on the shipped theme, after the fallbacks.
        View->UseTheme(const_cast<UDiceRollTheme*>(Shipped));
        View->Tick(0.016f);
        TestTrue(TEXT("The shipped theme draws the Vault kit again"), View->UsesVaultKit());
    }
    DestroyVaultWorld(World);
    IFileManager::Get().DeleteDirectory(*Directory, false, true);
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVaultThemeChoiceTest, "Project.Expedition.Presentation.VaultThemeChoiceLeavesStateUnchanged",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FVaultThemeChoiceTest::RunTest(const FString& Parameters)
{
    // The finale played with the same scripted policy under three presentations ends in the same snapshot and the same checkpoint
    // bytes: decoration never reads or advances the rules' RNG or state (ADR-0001).
    const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation/VaultThemeChoice"), FGuid::NewGuid().ToString());
    const FString Source = FPaths::Combine(Directory, TEXT("finale.json"));
    if (!TestTrue(TEXT("A seed reaches the finale"), WriteFinaleCheckpoint(Source, -1))) { return false; }
    FFinaleOutcome Outcomes[3];
    for (int32 Variant = 0; Variant < 3; ++Variant)
    {
        UWorld* World = MakeVaultWorld();
        if (!TestNotNull(TEXT("Transient world"), World)) { return false; }
        Outcomes[Variant] = PlayFinale(*this, World, Directory, Source, Variant);
        DestroyVaultWorld(World);
    }
    TestTrue(TEXT("The scripted policy gave commands"), Outcomes[0].Commands > 0);
    TestTrue(TEXT("The shipped presentation drew the Vault kit"), Outcomes[0].bVaultKit);
    TestFalse(TEXT("The primitive presentation did not"), Outcomes[1].bVaultKit);
    TestFalse(TEXT("The kit-off copy did not"), Outcomes[2].bVaultKit);
    for (int32 Variant = 1; Variant < 3; ++Variant)
    {
        TestEqual(TEXT("The same commands were accepted"), Outcomes[Variant].Commands, Outcomes[0].Commands);
        TestTrue(TEXT("The authoritative snapshot is identical"), FCombatSnapshot::StaticStruct()->CompareScriptStruct(&Outcomes[Variant].Battle, &Outcomes[0].Battle, 0));
        TestEqual(TEXT("The checkpoint bytes are identical"), Outcomes[Variant].Checkpoint, Outcomes[0].Checkpoint);
    }
    IFileManager::Get().DeleteDirectory(*Directory, false, true);
    return !HasAnyErrors();
}
#endif
