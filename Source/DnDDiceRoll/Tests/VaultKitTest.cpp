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
#include "Presentation/OccluderFade.h"
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
UDiceRollTheme* VaultThemeCopy(const UDiceRollTheme* Shipped)
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

const UDiceRollTheme* VaultShippedTheme()
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

AExpeditionBattleView* VaultResumeBoard(FAutomationTestBase& Test, UWorld* World, const FString& Path)
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

struct FVaultPlacement
{
    TMap<FIntPoint, int32> Variants;
    TMap<FIntPoint, FVector> Spots;
    TArray<TTuple<FIntPoint, int32, int32>> Dressing;
    int32 Obstacles = 0, Arches = 0;
    static FVaultPlacement Of(const AExpeditionBattleView& View)
    {
        FVaultPlacement Placement;
        Placement.Variants = View.ObstacleVariants();
        Placement.Spots = View.ObstacleSpots();
        for (const FPlacedDressing& Placed : View.PlacedDressing()) { Placement.Dressing.Add(MakeTuple(Placed.Plan.Cell, Placed.Plan.Variant, static_cast<int32>(Placed.Plan.Kind))); }
        Placement.Obstacles = View.PlacedObstacles();
        Placement.Arches = View.PlacedArches();
        return Placement;
    }
    bool operator==(const FVaultPlacement& Other) const
    {
        return Variants.OrderIndependentCompareEqual(Other.Variants) && Spots.OrderIndependentCompareEqual(Other.Spots)
            && Dressing == Other.Dressing && Obstacles == Other.Obstacles && Arches == Other.Arches;
    }
};

struct FVaultFinaleOutcome
{
    FCombatSnapshot Battle;
    FString Checkpoint;
    int32 Commands = 0;
    bool bVaultKit = false;
};

/** The rules-only policy of RootKitTest on the finale checkpoint, under a presentation variant: 0 the shipped theme, 1 primitives, 2 a copy with the Vault kit off. */
FVaultFinaleOutcome VaultPlayFinale(FAutomationTestBase& Test, UWorld* World, const FString& Directory, const FString& Source, int32 Variant)
{
    FVaultFinaleOutcome Outcome;
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
        if (const UDiceRollTheme* Shipped = VaultShippedTheme())
        {
            Off.Reset(VaultThemeCopy(Shipped));
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

int32 VaultRouteRecipe(const AExpeditionBattleView& View) { return View.Progress().History.IsEmpty() ? 0 : View.Progress().History.Last().Route.Recipe; }
int32 VaultRouteSeed(const AExpeditionBattleView& View) { return View.Progress().History.IsEmpty() ? 7 : View.Progress().History.Last().Route.EncounterSeed; }
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
    const UDiceRollTheme* Shipped = VaultShippedTheme();
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
        FVaultPlacement First, Second;
        int32 Seed = 0, Width = 0, Height = 0;
        TArray<FIntPoint> Blocked;
        bool bKit = false;
        for (int32 Build = 0; Build < 2; ++Build)
        {
            UWorld* World = MakeVaultWorld();
            if (!TestNotNull(TEXT("Transient world"), World)) { return false; }
            if (AExpeditionBattleView* View = VaultResumeBoard(*this, World, Path))
            {
                TestEqual(TEXT("The checkpoint's board is the recipe asked for"), VaultRouteRecipe(*View), Recipe);
                TestTrue(TEXT("It is the finale board"), View->Progress().History.Num() >= FExpeditionRun::EncounterCount);
                bKit = View->UsesVaultKit();
                (Build == 0 ? First : Second) = FVaultPlacement::Of(*View);
                if (Build == 0)
                {
                    Seed = VaultRouteSeed(*View);
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
        if (AExpeditionBattleView* View = VaultResumeBoard(*this, World, Path))
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
            TestTrue(TEXT("The choices are the same at another cell size"), FVaultPlacement::Of(*View).Variants.OrderIndependentCompareEqual(First.Variants));
            View->SetBoardMetric(Original);
            TestTrue(TEXT("The choices are the same after another rebuild"), FVaultPlacement::Of(*View) == First);
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
    const UDiceRollTheme* Shipped = VaultShippedTheme();
    if (!TestNotNull(TEXT("The shipped theme asset"), Shipped)) { return false; }
    UWorld* World = MakeVaultWorld();
    if (!TestNotNull(TEXT("Transient world"), World)) { return false; }
    if (AExpeditionBattleView* View = VaultResumeBoard(*this, World, Path))
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
            TStrongObjectPtr<UDiceRollTheme> Broken(VaultThemeCopy(Shipped));
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
    FVaultFinaleOutcome Outcomes[3];
    for (int32 Variant = 0; Variant < 3; ++Variant)
    {
        UWorld* World = MakeVaultWorld();
        if (!TestNotNull(TEXT("Transient world"), World)) { return false; }
        Outcomes[Variant] = VaultPlayFinale(*this, World, Directory, Source, Variant);
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

/**
 * #209: the Vault's near wall (up to 6 m high, 2 to 3 cells outside the board) is a black, opaque outer face seen from a camera outside the hall. It covered a
 * quarter of the view at some yaws and never dithered, because its pixels land on the floor between the wall and the board, outside the board rectangle the
 * fade used; and a fade that eased off toward the far pose left it a solid black band at the bottom of the frame at 180 degrees from home (on-screen D1).
 * The board fade now reaches VaultBoardMarginCells past the board with a VaultBoardRampCells ramp on the side(s) the camera is on, at every zoom, and stays at
 * the ordinary half cell on the others (a wall seen from inside, and the walls behind the board, stay opaque).
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVaultNearWallFadesTest, "Project.Expedition.Presentation.VaultNearWallFadesLikeTheOtherOccluders",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FVaultNearWallFadesTest::RunTest(const FString& Parameters)
{
    const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation/VaultNearWall"), FGuid::NewGuid().ToString());
    const FString Path = FPaths::Combine(Directory, TEXT("current.json"));
    if (!TestTrue(TEXT("A seed reaches the finale"), WriteFinaleCheckpoint(Path, -1))) { return false; }
    UWorld* World = MakeVaultWorld();
    if (!TestNotNull(TEXT("Transient world"), World)) { return false; }
    if (AExpeditionBattleView* View = VaultResumeBoard(*this, World, Path))
    {
        if (TestTrue(TEXT("The finale draws the Vault kit with its occluder"), View->UsesVaultKit() && View->HasOccluder()))
        {
            TestEqual(TEXT("The Vault's ramp is 1.5 cells"), View->OccluderRampCells(), OccluderFade::VaultBoardRampCells);
            const double Cell = View->Metric().CellCm;
            // The camera is on the low-Y side at yaw -90 (the open front: no wall, but the fade is the same), the high-Y side at 90 (the back wall), the high X side at 0, the low X side at 180.
            struct FCase { float Yaw; int32 Wide; };
            for (const bool bFar : {false, true})
            {
                for (const FCase& Case : {FCase{-90.0f, 2}, FCase{90.0f, 3}, FCase{0.0f, 1}, FCase{180.0f, 0}})
                {
                    View->SnapCamera(Case.Yaw, bFar ? View->MaxZoomFor(Case.Yaw) : View->FramedDistance(Case.Yaw));
                    View->Tick(1.0f / 30.0f);
                    const FString Label = FString::Printf(TEXT("Yaw %.0f at the %s zoom"), Case.Yaw, bFar ? TEXT("far") : TEXT("default"));
                    float Margins[4];
                    View->OccluderSideMargins(Margins);
                    const FCombatEncounter& Shape = View->Rules().Snapshot().Encounter;
                    // The walls' outer faces: a wall the camera is outside of is reached by the fade whatever the board's size; the open front by the camera's share; others the base.
                    const float GapX = OccluderFade::VaultSideWallOuterCells - Shape.Width * 0.5f + OccluderFade::VaultWallEdgeCells;
                    const float GapBack = OccluderFade::VaultBackWallOuterCells - Shape.Height * 0.5f + OccluderFade::VaultWallEdgeCells;
                    const float Wall = FMath::Max(OccluderFade::VaultBoardMarginCells, GapX), Back = FMath::Max(OccluderFade::VaultBoardMarginCells, GapBack), Base = OccluderFade::BoardMarginCells;
                    // Outside a side wall: that wall's side, and the high-Y margin (the wall runs to the back wall). Outside the back wall: the back, and both X margins
                    // (it spans the hall). Toward the open front: the camera's share of the ordinary wide margin. Otherwise the base.
                    const float Expected[4] = {Case.Wide == 0 || Case.Wide == 3 ? Wall : Base, Case.Wide == 1 || Case.Wide == 3 ? Wall : Base,
                        Case.Wide == 2 ? OccluderFade::VaultBoardMarginCells : Base, Case.Wide == 3 || Case.Wide < 2 ? Back : Base};
                    for (int32 Side = 0; Side < 4; ++Side) { TestEqual(*FString::Printf(TEXT("%s: side %d"), *Label, Side), Margins[Side], Expected[Side], 0.01f); }
                    FVector2D Min, Max;
                    float PlaneZ = 0.0f;
                    View->OccluderBoard(Min, Max, PlaneZ);
                    const FCombatEncounter& Board = View->Rules().Snapshot().Encounter;
                    const FVector Centre = View->GetActorTransform().TransformPosition(View->BoardPosition(FVector2D((Board.Width - 1) * 0.5f, (Board.Height - 1) * 0.5f)));
                    const FVector Eye = View->CameraTransform().GetLocation();
                    const FVector2D Toward = Case.Wide == 0 ? FVector2D(-1, 0) : Case.Wide == 1 ? FVector2D(1, 0) : Case.Wide == 2 ? FVector2D(0, -1) : FVector2D(0, 1);
                    const double Half = (Case.Wide < 2 ? Board.Width : Board.Height) * 0.5 * Cell;
                    auto Fade = [&](double CellsPast, FVector2D Direction)
                    {
                        const FVector Ground(Centre.X + Direction.X * (Half + CellsPast * Cell), Centre.Y + Direction.Y * (Half + CellsPast * Cell), PlaneZ);
                        return View->OccluderBoardOpacityAt(Eye + (Ground - Eye) * 0.5);
                    };
                    TestEqual(*(Label + TEXT(": the floor 2 cells past the camera-side edge (where a near wall's rays land) is faded")), Fade(2.0, Toward), View->OccluderMinOpacity(), 0.001f);
                    float Last = 0.0f;
                    for (double Past = Expected[Case.Wide]; Past <= Expected[Case.Wide] + OccluderFade::VaultBoardRampCells + 0.5; Past += 0.125)
                    {
                        const float Opacity = Fade(Past, Toward);
                        TestTrue(*(Label + TEXT(": the fade eases back to opaque past the grown rectangle")), Opacity >= Last - 0.0001f);
                        Last = Opacity;
                    }
                    TestEqual(*(Label + TEXT(": opaque a full ramp past it")), Last, 1.0f, 0.0001f);
                    TestEqual(*(Label + TEXT(": the floor behind the board, on the far side (3 cells past), is opaque")), Fade(3.0, -Toward), 1.0f, 0.001f);
                    TestEqual(*(Label + TEXT(": ground well outside is opaque")), Fade(20.0, Toward), 1.0f, 0.001f);
                }
            }
        }
        DestroyVaultWorld(World);
    }
    IFileManager::Get().DeleteDirectory(*Directory, false, true);
    return !HasAnyErrors();
}

namespace
{
/**
 * Whether the rendered frame is inside the Vault's set: a grid of rays over the whole viewport (through the real eased camera), each landing within HalfCm of
 * the board centre, and none meeting the near wall first. The walls are boxes: the camera outside a wall (the two side walls, the back wall; the front is open)
 * means that wall's outer face is the first thing a ray meets, and it is black, so a ray that hits the box before the ground and is not dithered by the occluder
 * fade (OccluderBoardOpacityAt, the CPU mirror of the shader's parameters) fills the screen with it (on-screen D1).
 */
bool ViewportInsideSet(AExpeditionBattleView& View, float HalfCm, FString& Why)
{
    using namespace ExpeditionCameraRig;
    const FSafeRect Rect = View.SafeRect();
    const FTransform Camera = View.CameraTransform();
    const FTransform Actor = View.GetActorTransform();
    const double PlaneZ = Actor.GetLocation().Z + View.Metric().BoardTop();
    const FCombatEncounter& Board = View.Rules().Snapshot().Encounter;
    const FVector2D Centre = BoardCentre(Board.Width, Board.Height, View.Metric());
    const double Cell = View.Metric().CellCm;
    const FVector Eye = Actor.InverseTransformPosition(Camera.GetLocation());
    const FVector Forward = Camera.GetRotation().GetForwardVector();
    struct FWallBox { double X0, X1, Y0, Y1, Z1; };
    // Board-centre relative, cm: inner face out to the outer face; side walls run from 1.7 m in front of the centre line to the back, 4.7 m high; the back wall 6 m high.
    const double SideX = 10.667 * Cell, BackY = 7.867 * Cell;   // the walls' inner faces, 16 m and 11.8 m out (measured from the occluder mesh)
    const FWallBox Walls[3] = {
        {SideX, SideX + 180.0, -170.0, 1400.0, 470.0 * 0.98},
        {-SideX - 180.0, -SideX, -170.0, 1400.0, 470.0 * 0.98},
        {-1750.0, 1750.0, BackY, 1530.0, 600.0 * 0.98}};
    const bool bOutside[3] = {Eye.X - Centre.X > SideX, Eye.X - Centre.X < -SideX, Eye.Y - Centre.Y > BackY};
    auto Slab = [](double Origin, double Dir, double Lo, double Hi, double& T0, double& T1)
    {
        if (FMath::Abs(Dir) < 1.0e-9) { return Origin >= Lo && Origin <= Hi; }
        double A = (Lo - Origin) / Dir, B = (Hi - Origin) / Dir;
        if (A > B) { Swap(A, B); }
        T0 = FMath::Max(T0, A); T1 = FMath::Min(T1, B);
        return T0 <= T1;
    };
    constexpr int32 Columns = 24, Rows = 14;
    for (int32 Row = 0; Row <= Rows; ++Row)
    {
        int32 Opaque = 0;
        FString First;
        for (int32 Column = 0; Column <= Columns; ++Column)
        {
            const FVector2D Screen(static_cast<double>(Column) / Columns, static_cast<double>(Row) / Rows);
            FVector2D Ground;
            if (!Unproject(Screen, Camera.GetLocation(), Forward, Rect.Aspect, static_cast<float>(PlaneZ), Ground)) { Why = TEXT("a viewport ray has no ground"); return false; }
            const FVector Hit = Actor.InverseTransformPosition(FVector(Ground.X, Ground.Y, PlaneZ));
            if (FMath::Abs(Hit.X - Centre.X) > HalfCm + 5.0 || FMath::Abs(Hit.Y - Centre.Y) > HalfCm + 5.0)
            { Why = FString::Printf(TEXT("the ray at screen (%.2f, %.2f) lands %.0f, %.0f cm from the board centre (set %.0f)"), Screen.X, Screen.Y, Hit.X - Centre.X, Hit.Y - Centre.Y, HalfCm); return false; }
            const FVector Direction = Hit - Eye;
            for (int32 Index = 0; Index < 3; ++Index)
            {
                if (!bOutside[Index]) { continue; }
                const FWallBox& Box = Walls[Index];
                double T0 = 0.0, T1 = 1.0;
                if (Slab(Eye.X - Centre.X, Direction.X, Box.X0, Box.X1, T0, T1) && Slab(Eye.Y - Centre.Y, Direction.Y, Box.Y0, Box.Y1, T0, T1) && Slab(Eye.Z - PlaneZ, Direction.Z, 0.0, Box.Z1, T0, T1) && T0 < 1.0)
                {
                    // The ray meets the wall before the ground: the pixel shows the wall's black outer face unless the occluder fade dithers it there.
                    const float Opacity = View.OccluderBoardOpacityAt(Actor.TransformPosition(Eye + Direction * T0));
                    if (Opacity > 0.5f)
                    {
                        ++Opaque;
                        if (First.IsEmpty())
                        {
                            FVector2D RectMin, RectMax;
                            float RectZ = 0.0f;
                            View.OccluderBoard(RectMin, RectMax, RectZ);
                            const FVector Entry = Eye + Direction * T0;
                            First = FString::Printf(TEXT("first at screen (%.2f, %.2f) on the camera-side %s wall at (%.0f, %.0f, %.0f) cm from the board centre, opacity %.2f, landing (%.0f, %.0f); the fade rectangle is x %.0f to %.0f, y %.0f to %.0f; the camera is at (%.0f, %.0f)"),
                                Screen.X, Screen.Y, Index == 2 ? TEXT("back") : TEXT("side"), Entry.X - Centre.X, Entry.Y - Centre.Y, Entry.Z - PlaneZ, Opacity, Hit.X - Centre.X, Hit.Y - Centre.Y,
                                RectMin.X - Actor.GetLocation().X - Centre.X, RectMax.X - Actor.GetLocation().X - Centre.X, RectMin.Y - Actor.GetLocation().Y - Centre.Y, RectMax.Y - Actor.GetLocation().Y - Centre.Y, Eye.X - Centre.X, Eye.Y - Centre.Y);
                        }
                        break;
                    }
                }
            }
        }
        // A band is a row of the frame, not a few grazing pixels: at least half the row meets an opaque camera-side wall.
        if (Opaque * 2 > Columns)
        { Why = FString::Printf(TEXT("%d of %d rays in the row at screen y %.2f meet an opaque camera-side wall (a black band): %s"), Opaque, Columns + 1, static_cast<double>(Row) / Rows, *First); return false; }
    }
    return true;
}
}

/**
 * #209: the Vault's camera shows no more than the authored set (the hall, its dressed ground and the far-hall ring, 27 m each way from the
 * board centre) at any legal pose. At the zoom maximum (6400) the hall floated in the dark, and the far pose from a diagonal yaw shows
 * far past the ring. The wheel stops at MaxZoomFor(yaw); the rendered pose obeys the cap of the rendered yaw every frame (an orbit eases yaw and
 * distance separately and used to pass through poses beyond it); and the focus stays where the viewport's ground is inside the set. Sampled on
 * the rendered camera at 30 Hz, at several aspects, at every 15 degrees of yaw, panned to the stops, through orbits in both directions
 * from a far zoom and with wheel notches during the easing. The Vigil and the Root keep the full range.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVaultZoomCapTest, "Project.Expedition.Presentation.VaultNeverShowsPastTheSet",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FVaultZoomCapTest::RunTest(const FString& Parameters)
{
    using namespace ExpeditionCameraRig;
    const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation/VaultZoomCap"), FGuid::NewGuid().ToString());
    const FString Path = FPaths::Combine(Directory, TEXT("current.json"));
    if (!TestTrue(TEXT("A seed reaches the finale"), WriteFinaleCheckpoint(Path, -1))) { return false; }
    UWorld* World = MakeVaultWorld();
    if (!TestNotNull(TEXT("Transient world"), World)) { return false; }
    if (AExpeditionBattleView* View = VaultResumeBoard(*this, World, Path))
    {
        if (TestTrue(TEXT("The finale draws the Vault kit, which bounds the camera by its set"), View->UsesVaultKit() && View->BoundsSet()))
        {
            const float Half = SetHalfExtentCells * View->Metric().CellCm;
            int32 Capped = 0, Frames = 0;
            struct FSize { int32 X, Y; };
            for (const FSize Size : {FSize{1920, 1080}, FSize{1280, 720}, FSize{2560, 1080}})
            {
                View->SetSafeRect(FSafeRect::ForViewport(Size.X, Size.Y));
                View->SnapCameraToTargets();
                for (float Yaw = -180.0f; Yaw <= 180.0f; Yaw += 15.0f)
                {
                    const FString Label = FString::Printf(TEXT("%dx%d yaw %.0f"), Size.X, Size.Y, Yaw);
                    View->SnapCamera(Yaw, View->FramedDistance(Yaw));
                    View->Tick(1.0f / 30.0f);
                    FString Why;
                    const float Cap = View->MaxZoomFor(Yaw);
                    TestTrue(*(Label + TEXT(": the cap is inside the zoom range and not nearer than the default")), Cap <= MaxDistance + 0.01f && Cap >= View->FramedDistance(Yaw) - 0.01f);
                    // Wheel out to the stop, a notch a frame, then pan to each stop; whenever the default framing fits the set, every rendered frame does.
                    for (int32 Frame = 0; Frame < 140; ++Frame)
                    {
                        if (Frame < 100) { View->Zoom(120.0f); }
                        View->Tick(1.0f / 30.0f); ++Frames;
                        TestTrue(*(Label + TEXT(": the rendered distance never passes the rendered yaw's cap")), View->RenderedDistance() <= View->MaxZoomFor(View->RenderedYaw()) + 0.5f);
                        if (!View->ShowsPastSet() && !ViewportInsideSet(*View, Half, Why)) { AddError(Label + FString::Printf(TEXT(" wheeling out, frame %d: %s"), Frame, *Why)); break; }
                    }
                    TestEqual(*(Label + TEXT(": the wheel stops at the cap")), View->CameraDistance(), Cap, 0.5f);
                    TestTrue(*(Label + TEXT(": and the rendered pose is at or inside it (the focus can lower it)")), View->RenderedDistance() <= Cap + 0.5f);
                    Capped += Cap < MaxDistance - 1.0f ? 1 : 0;
                    for (const FVector2D Direction : {FVector2D(0, 1), FVector2D(0, -1), FVector2D(1, 0), FVector2D(-1, 0)})
                    {
                        View->Pan(static_cast<float>(Direction.X) * 1.0e5f, static_cast<float>(Direction.Y) * 1.0e5f);
                        for (int32 Frame = 0; Frame < 30; ++Frame)
                        {
                            View->Tick(1.0f / 30.0f); ++Frames;
                            if (!View->ShowsPastSet() && !ViewportInsideSet(*View, Half, Why)) { AddError(Label + FString::Printf(TEXT(" panned (%.0f, %.0f), frame %d: %s"), Direction.X, Direction.Y, Frame, *Why)); break; }
                        }
                    }
                    View->SnapCamera(Yaw, 1500.0f);
                    View->Tick(1.0f / 30.0f);
                    View->Zoom(120.0f);
                    TestEqual(*(Label + TEXT(": a closer zoom still steps by a notch")), View->CameraDistance(), 1620.0f, 0.01f);
                }
                // Orbits from a far zoom in both directions, a quarter turn at a time, 30 Hz, with a wheel notch during the easing.
                for (const float Start : {-45.0f, 45.0f, 135.0f, -135.0f, 0.0f, 90.0f})
                {
                    for (const float Turn : {-45.0f, 45.0f, 90.0f, -90.0f})
                    {
                        const FString Label = FString::Printf(TEXT("%dx%d orbit from yaw %.0f by %.0f"), Size.X, Size.Y, Start, Turn);
                        View->SnapCamera(Start, View->MaxZoomFor(Start));
                        View->Tick(1.0f / 30.0f);
                        FString Why;
                        View->Orbit(Turn);
                        for (int32 Frame = 0; Frame < 90; ++Frame)
                        {
                            if (Frame == 3 || Frame == 9) { View->Zoom(120.0f); }
                            View->Tick(1.0f / 30.0f); ++Frames;
                            const float Cap = View->MaxZoomFor(View->RenderedYaw());
                            if (View->RenderedDistance() > Cap + 0.5f) { AddError(Label + FString::Printf(TEXT(", frame %d: rendered distance %.1f is past the cap %.1f of rendered yaw %.1f"), Frame, View->RenderedDistance(), Cap, View->RenderedYaw())); break; }
                            if (!View->ShowsPastSet() && !ViewportInsideSet(*View, Half, Why)) { AddError(Label + FString::Printf(TEXT(", frame %d: %s"), Frame, *Why)); break; }
                        }
                    }
                }
            }
            AddInfo(FString::Printf(TEXT("%d rendered frames sampled; %d yaw and aspect combinations capped below the zoom maximum"), Frames, Capped));
            TestTrue(TEXT("The cap does something"), Capped > 0);
        }
        DestroyVaultWorld(World);
    }
    IFileManager::Get().DeleteDirectory(*Directory, false, true);
    return !HasAnyErrors();
}

namespace
{
/** One finale checkpoint per board size (up to Count sizes), from the first seeds that reach one with the party acting first. */
struct FFinale { FExpeditionSaveData Data; int32 Width = 0, Height = 0; };
TArray<FFinale> FinaleSizes(int32 Count)
{
    TArray<FFinale> Found;
    TSet<FIntPoint> Sizes;
    for (int32 Seed = 1; Seed < 120 && Found.Num() < Count; ++Seed)
    {
        FExpeditionRun Run;
        if (!ExpeditionTestSupport::ReachFinale(Run, Seed, -1, true)) { continue; }
        const FExpeditionSnapshot Snapshot = Run.Snapshot();
        const FIntPoint Size(Snapshot.Battle.Encounter.Width, Snapshot.Battle.Encounter.Height);
        if (Sizes.Contains(Size)) { continue; }
        Sizes.Add(Size);
        FFinale Finale;
        Finale.Data.Current = Snapshot;
        Finale.Width = Size.X; Finale.Height = Size.Y;
        Found.Add(Finale);
    }
    return Found;
}

/** Puts the acting hero on a cell (swapping with whoever stands there); false when the cell is blocked or the actor is not a hero. */
bool PutActiveAt(FExpeditionSaveData& Data, FIntPoint Cell)
{
    FCombatSnapshot& Battle = Data.Current.Battle;
    if (!Battle.TurnOrder.IsValidIndex(Battle.TurnIndex) || Battle.Encounter.BlockedCells.Contains(Cell)) { return false; }
    const int32 ActiveId = Battle.TurnOrder[Battle.TurnIndex];
    FCombatantState* Active = Battle.Encounter.Combatants.FindByPredicate([ActiveId](const FCombatantState& Unit) { return Unit.Id == ActiveId; });
    if (!Active || Active->Side != ECombatSide::Party) { return false; }
    for (FCombatantState& Other : Battle.Encounter.Combatants) { if (&Other != Active && Other.Cell == Cell) { Other.Cell = Active->Cell; } }
    Active->Cell = Cell;
    return true;
}

/** Whether the acting unit's tile and head are inside the safe rect of the rendered pose. */
bool ActiveUnitVisible(AExpeditionBattleView& View, float InFill = 1.0f)
{
    using namespace ExpeditionCameraRig;
    const int32 Id = View.Rules().ActiveId();
    const FVector2D Unit(View.UnitPosition(Id, -View.Metric().BoardTop()));
    return UnitInside(Unit, View.Metric(), View.RenderedFocus(), View.RenderedYaw(), View.RenderedDistance(), PitchFor(View.RenderedDistance(), View.FramedDistance()), View.SafeRect(), InFill);
}

/**
 * #148's locked-axis rule on the rendered pose: where the safe rect's ground is wider than the pan bound on an axis, the view is centred on the board
 * exactly, the one exception being the unit hold, which moves the focus from the centring focus toward the acting unit and keeps it in view.
 */
bool LockedAxesCentred(AExpeditionBattleView& View, FString& Why)
{
    using namespace ExpeditionCameraRig;
    if (View.WholeBoardInView()) { return true; }
    const FSafeRect Rect = View.SafeRect();
    const FTransform Camera = View.CameraTransform();
    const FTransform Actor = View.GetActorTransform();
    const double PlaneZ = Actor.GetLocation().Z + View.Metric().BoardTop();
    const FCombatEncounter& Board = View.Rules().Snapshot().Encounter;
    const double Cell = View.Metric().CellCm;
    const FVector2D BoardMid = BoardCentre(Board.Width, Board.Height, View.Metric());
    double MinX = TNumericLimits<double>::Max(), MaxX = TNumericLimits<double>::Lowest(), MinY = MinX, MaxY = MaxX;
    for (const FVector2D& Corner : {FVector2D(Rect.Left, Rect.Top), FVector2D(1.0 - Rect.Right, Rect.Top), FVector2D(Rect.Left, 1.0 - Rect.Bottom), FVector2D(1.0 - Rect.Right, 1.0 - Rect.Bottom)})
    {
        FVector2D Ground;
        if (!Unproject(Corner, Camera.GetLocation(), Camera.GetRotation().GetForwardVector(), Rect.Aspect, static_cast<float>(PlaneZ), Ground)) { return true; }
        const FVector InView = Actor.InverseTransformPosition(FVector(Ground.X, Ground.Y, PlaneZ));
        MinX = FMath::Min(MinX, InView.X); MaxX = FMath::Max(MaxX, InView.X); MinY = FMath::Min(MinY, InView.Y); MaxY = FMath::Max(MaxY, InView.Y);
    }
    const double Lower = -(0.5 + PanMarginCells) * Cell, UpperX = (Board.Width - 1 + 0.5 + PanMarginCells) * Cell, UpperY = (Board.Height - 1 + 0.5 + PanMarginCells) * Cell;
    const FVector2D Focus = View.RenderedFocus();
    const FVector Unit = View.UnitPosition(View.Rules().ActiveId(), 0.0f);
    const bool bUnitIn = ActiveUnitVisible(View, 1.0f);
    auto Axis = [&](const TCHAR* Name, double Min, double Max, double Upper, double Mid, double FocusAxis, double UnitAxis)
    {
        if (Max - Min <= Upper - Lower) { return true; }
        const double Off = 0.5 * (Min + Max) - Mid;
        if (FMath::Abs(Off) <= 1.0) { return true; }
        const double Centring = FocusAxis - Off;
        const bool bTowardUnit = FocusAxis >= FMath::Min(Centring, UnitAxis) - 1.0 && FocusAxis <= FMath::Max(Centring, UnitAxis) + 1.0;
        if (bTowardUnit && bUnitIn) { return true; }
        Why = FString::Printf(TEXT("a locked %s axis is %.0f cm off the board centre and that is not the unit hold (focus %.0f, centring %.0f, unit %.0f, unit in view %d)"), Name, Off, FocusAxis, Centring, UnitAxis, bUnitIn ? 1 : 0);
        return false;
    };
    return Axis(TEXT("X"), MinX, MaxX, UpperX, BoardMid.X, Focus.X, Unit.X) && Axis(TEXT("Y"), MinY, MaxY, UpperY, BoardMid.Y, Focus.Y, Unit.Y);
}
}

/**
 * #209 round 2 (and #148): the Vault's set bound is met by the zoom alone, never by moving the focus. The pan bound, the locked axes and the unit hold decide
 * the focus as they always did, and the acting unit is never lost. The acting hero is put on each corner of the board in finales of every board size the
 * generator makes (16 to 18 by 11 to 12 as the first seeds give them), at 1080p and 720p, at every 15 degrees of yaw, wheeled out to the cap through the
 * easing: every rendered frame keeps the unit's tile and head in the safe rect, a locked axis stays centred (apart from the unit hold), and the viewport's
 * ground stays inside the set, except where even the default zoom does not fit at that focus, which the view reports (ShowsPastSet) and which is counted.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVaultEdgeUnitTest, "Project.Expedition.Presentation.VaultEdgeUnitsStayInViewAtTheFarCap",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FVaultEdgeUnitTest::RunTest(const FString& Parameters)
{
    using namespace ExpeditionCameraRig;
    const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation/VaultEdgeUnit"), FGuid::NewGuid().ToString());
    const TArray<FFinale> Finales = FinaleSizes(4);
    if (!TestTrue(TEXT("Finales of at least two board sizes exist"), Finales.Num() >= 2)) { return false; }
    int32 Frames = 0, Poses = 0, Reported = 0, Capped = 0;
    TSet<FIntPoint> Sizes;
    for (const FFinale& Finale : Finales)
    {
        Sizes.Add(FIntPoint(Finale.Width, Finale.Height));
        const FIntPoint Corners[4] = {FIntPoint(0, 0), FIntPoint(Finale.Width - 1, 0), FIntPoint(0, Finale.Height - 1), FIntPoint(Finale.Width - 1, Finale.Height - 1)};
        for (const FIntPoint& Corner : Corners)
        {
            FExpeditionSaveData Data = Finale.Data;
            if (!PutActiveAt(Data, Corner)) { continue; }
            const FString Path = FPaths::Combine(Directory, FString::Printf(TEXT("%dx%d-%d-%d"), Finale.Width, Finale.Height, Corner.X, Corner.Y), TEXT("current.json"));
            FString Error;
            if (!TestTrue(TEXT("A corner checkpoint writes"), FExpeditionSave::Write(Path, Data, Error))) { continue; }
            UWorld* World = MakeVaultWorld();
            if (!TestNotNull(TEXT("Transient world"), World)) { return false; }
            if (AExpeditionBattleView* View = VaultResumeBoard(*this, World, Path))
            {
                const FCombatantState* Active = View->Rules().Find(View->Rules().ActiveId());
                if (TestTrue(TEXT("The acting hero is on the corner"), Active && Active->Cell == Corner && View->UsesVaultKit() && View->BoundsSet()))
                {
                    const float Half = SetHalfExtentCells * View->Metric().CellCm;
                    struct FSize { int32 X, Y; };
                    for (const FSize Size : {FSize{1920, 1080}, FSize{1280, 720}})
                    {
                        View->SetSafeRect(FSafeRect::ForViewport(Size.X, Size.Y));
                        for (float Yaw = -180.0f; Yaw <= 165.0f; Yaw += 15.0f)
                        {
                            const FString Label = FString::Printf(TEXT("%dx%d board, unit on (%d, %d), %dx%d, yaw %.0f"), Finale.Width, Finale.Height, Corner.X, Corner.Y, Size.X, Size.Y, Yaw);
                            View->SnapCamera(Yaw, View->FramedDistance(Yaw));
                            View->Tick(1.0f / 30.0f);
                            bool bFailed = false;
                            for (int32 Frame = 0; Frame < 140 && !bFailed; ++Frame)
                            {
                                if (Frame < 100) { View->Zoom(120.0f); }
                                View->Tick(1.0f / 30.0f); ++Frames;
                                FString Why;
                                if (!ActiveUnitVisible(*View)) { AddError(Label + FString::Printf(TEXT(", frame %d: the acting unit is outside the safe rect"), Frame)); bFailed = true; }
                                else if (!LockedAxesCentred(*View, Why)) { AddError(Label + FString::Printf(TEXT(", frame %d: %s"), Frame, *Why)); bFailed = true; }
                                else if (!View->ShowsPastSet() && !ViewportInsideSet(*View, Half, Why)) { AddError(Label + FString::Printf(TEXT(", frame %d: %s"), Frame, *Why)); bFailed = true; }
                            }
                            ++Poses;
                            Reported += View->ShowsPastSet() ? 1 : 0;
                            Capped += View->RenderedDistance() < View->MaxZoomFor(Yaw) - 1.0f ? 1 : 0;
                        }
                    }
                }
                DestroyVaultWorld(World);
            }
        }
    }
    AddInfo(FString::Printf(TEXT("%d board sizes, %d settled poses, %d rendered frames; %d poses sat below the yaw's cap because of the focus; %d report that even the default zoom shows past the set at that focus"), Sizes.Num(), Poses, Frames, Capped, Reported));
    TestTrue(TEXT("Corner units exercised the focus-dependent cap"), Capped > 0);
    IFileManager::Get().DeleteDirectory(*Directory, false, true);
    return !HasAnyErrors();
}

/**
 * #209 round 2: the final shaken pose keeps the set too. A real shake (View.Shake, renewed every frame so it never decays) runs through capped poses with the
 * acting unit on each corner, through an orbit and the focus easing and wheel notches: every rendered viewport corner stays inside the set (the shake
 * amplitude is scaled down, to nothing at worst) and the acting unit stays in the safe rect.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVaultShakeTest, "Project.Expedition.Presentation.VaultShakeStaysInsideTheSet",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FVaultShakeTest::RunTest(const FString& Parameters)
{
    using namespace ExpeditionCameraRig;
    const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation/VaultShake"), FGuid::NewGuid().ToString());
    const TArray<FFinale> Finales = FinaleSizes(2);
    if (!TestTrue(TEXT("A finale exists"), Finales.Num() >= 1)) { return false; }
    int32 Frames = 0, Shaken = 0;
    for (const FFinale& Finale : Finales)
    {
        const FIntPoint Corners[4] = {FIntPoint(0, 0), FIntPoint(Finale.Width - 1, 0), FIntPoint(0, Finale.Height - 1), FIntPoint(Finale.Width - 1, Finale.Height - 1)};
        for (const FIntPoint& Corner : Corners)
        {
            FExpeditionSaveData Data = Finale.Data;
            if (!PutActiveAt(Data, Corner)) { continue; }
            const FString Path = FPaths::Combine(Directory, FString::Printf(TEXT("%dx%d-%d-%d"), Finale.Width, Finale.Height, Corner.X, Corner.Y), TEXT("current.json"));
            FString Error;
            if (!TestTrue(TEXT("A corner checkpoint writes"), FExpeditionSave::Write(Path, Data, Error))) { continue; }
            UWorld* World = MakeVaultWorld();
            if (!TestNotNull(TEXT("Transient world"), World)) { return false; }
            if (AExpeditionBattleView* View = VaultResumeBoard(*this, World, Path))
            {
                const float Half = SetHalfExtentCells * View->Metric().CellCm;
                for (const float Yaw : {-135.0f, -45.0f, 45.0f, 135.0f, 0.0f, 90.0f})
                {
                    const FString Label = FString::Printf(TEXT("%dx%d board, unit on (%d, %d), yaw %.0f"), Finale.Width, Finale.Height, Corner.X, Corner.Y, Yaw);
                    View->SnapCamera(Yaw, View->MaxZoomFor(Yaw));
                    View->Tick(1.0f / 30.0f);
                    bool bFailed = false;
                    for (int32 Frame = 0; Frame < 150 && !bFailed; ++Frame)
                    {
                        View->Shake(1.0f);
                        if (Frame == 20) { View->Orbit(45.0f); }
                        if (Frame == 70) { View->Orbit(-90.0f); }
                        if (Frame % 17 == 0 && Frame < 120) { View->Zoom(120.0f); }
                        View->Tick(1.0f / 30.0f); ++Frames;
                        FString Why;
                        if (!View->ShowsPastSet() && !ViewportInsideSet(*View, Half, Why)) { AddError(Label + FString::Printf(TEXT(", frame %d, shaking: %s"), Frame, *Why)); bFailed = true; }
                        else if (!ActiveUnitVisible(*View, 1.0f)) { AddError(Label + FString::Printf(TEXT(", frame %d, shaking: the acting unit is outside the safe rect"), Frame)); bFailed = true; }
                        Shaken += 1;
                    }
                }
                DestroyVaultWorld(World);
            }
        }
    }
    AddInfo(FString::Printf(TEXT("%d rendered frames sampled under a constant shake"), Frames));
    TestTrue(TEXT("The shake ran"), Shaken > 0);
    IFileManager::Get().DeleteDirectory(*Directory, false, true);
    return !HasAnyErrors();
}
#endif
