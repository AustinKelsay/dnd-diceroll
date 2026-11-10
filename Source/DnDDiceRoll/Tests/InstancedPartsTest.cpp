#if WITH_DEV_AUTOMATION_TESTS
#include "ExpeditionInputHarness.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Misc/CommandLine.h"
#include "Presentation/BoardMetric.h"

/**
 * Instanced presentation parts (#144). A themed board draws its static props, footprint rings and pooled effects from shared
 * instanced groups, so a rebuild creates a few dozen components instead of a few hundred, with the same logical parts. The
 * same board is also built with every part loose (SetLooseParts), which is what the board drew before, so the logical counts are
 * compared against that. Each test builds its views in its own world, like the harness does, and says so when the shipped
 * theme is not available here.
 */
namespace
{
constexpr int32 ComponentBudget = 40;

UWorld* MakePartsWorld()
{
    const UWorld::InitializationValues Values = UWorld::InitializationValues()
        .AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false);
    return UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
}

// Release the world like the shared harness does, or later tests trip the zero-warning gate on RHI reserved memory.
void DestroyPartsWorld(UWorld* World)
{
    World->DestroyWorld(false);
    FlushRenderingCommands();
    CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
}

/** A themed battle board on the first route offer that is (or is not) dangerous; null when the run offers no such road. */
AExpeditionBattleView* StartPartsBoard(UWorld* World, const FString& Directory, bool bDangerous)
{
    AExpeditionBattleView* View = World->SpawnActor<AExpeditionBattleView>();
    if (!View) { return nullptr; }
    View->ConfigureStorage(FPaths::Combine(Directory, TEXT("current.json")));
    View->DispatchBeginPlay();
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

/** Every component the view owns, runtime-created ones included: a rebuild that leaked components would grow this. */
int32 OwnedComponents(const AExpeditionBattleView& View)
{
    TInlineComponentArray<UActorComponent*> All;
    View.GetComponents(All);
    return All.Num();
}

/** Instances summed over every instanced component the view owns. */
int32 OwnedInstances(const AExpeditionBattleView& View)
{
    TInlineComponentArray<UInstancedStaticMeshComponent*> Sets;
    View.GetComponents(Sets);
    int32 Total = 0;
    for (const UInstancedStaticMeshComponent* Set : Sets) { Total += Set->GetInstanceCount(); }
    return Total;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInstancedPartsBudgetTest, "Project.Expedition.Presentation.InstancedPartsStayUnderBudget",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FInstancedPartsBudgetTest::RunTest(const FString& Parameters)
{
    UWorld* World = MakePartsWorld();
    if (!TestNotNull(TEXT("Transient world"), World)) { return false; }
    const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation/InstancedParts"), FGuid::NewGuid().ToString());
    int32 Checked = 0;
    // A Vigil board (a dangerous road: the r002 kit when switched on) and the forest board of the first road.
    for (const bool bDangerous : {true, false})
    {
        const TCHAR* Name = bDangerous ? TEXT("Vigil") : TEXT("Forest");
        AExpeditionBattleView* View = StartPartsBoard(World, Directory, bDangerous);
        if (!View) { AddInfo(FString::Printf(TEXT("No %s road is on offer in this run; its board was skipped."), Name)); continue; }
        if (!View->UsesTheme()) { AddInfo(TEXT("The shipped theme is not available here; the instanced-parts checks were skipped.")); break; }
        // The themed look needs M_DiceRollEffect and the board overlay material with graph board-overlay-3: both are created by the
        // headless theme import (Content/Python/import_dark_fantasy_theme.py and create_board_overlay_material.py).
        if (!TestTrue(FString::Printf(TEXT("%s: the themed board is instanced"), Name), View->UsesInstancedParts())
            || !TestTrue(FString::Printf(TEXT("%s: the effect material M_DiceRollEffect exists (run the theme import)"), Name), View->UsesInstancedEffects())
            || !TestTrue(FString::Printf(TEXT("%s: the footprint rings share one bank (run create_board_overlay_material.py)"), Name), View->UsesRingBank()))
        { continue; }
        ++Checked;

        // The headline number: components that draw parts. The nine board-overlay bar sets (#142) are not parts and are reported apart.
        const int32 Parts = View->PresentationComponentCount(false);
        const int32 WithOverlay = View->PresentationComponentCount(true);
        const int32 Logical = View->LogicalPartCount();
        const int32 Props = View->LogicalPropCount();
        const int32 Groups = View->InstancedGroupCount();
        const int32 Owned = OwnedComponents(*View);
        const int32 Instances = OwnedInstances(*View);
        const int32 Obstacles = View->PlacedObstacles();
        const int32 Dressing = View->PlacedDressing().Num();
        AddInfo(FString::Printf(TEXT("%s board, instanced: %d part components (%d with the overlay sets), %d groups, %d owned components, %d instances; %d logical parts, %d scenery props"),
            Name, Parts, WithOverlay, Groups, Owned, Instances, Logical, Props));
        TestTrue(FString::Printf(TEXT("%s: fewer than %d presentation components per themed rebuild (got %d)"), Name, ComponentBudget, Parts), Parts < ComponentBudget);
        TestTrue(FString::Printf(TEXT("%s: the board has parts and props"), Name), Logical > 0 && Props > 0);
        TestTrue(FString::Printf(TEXT("%s: the dressing and obstacles are in the props"), Name),
            Props >= View->PlacedObstacles() + View->PlacedDressing().Num());

        // The same board with every part loose is what it drew before: the logical parts are identical, only the components differ.
        View->SetLooseParts(true);
        View->Tick(0.016f);
        TestFalse(FString::Printf(TEXT("%s: loose parts are not instanced"), Name), View->UsesInstancedParts());
        const int32 LooseParts = View->PresentationComponentCount(false);
        TestEqual(FString::Printf(TEXT("%s: the same logical parts, loose or instanced"), Name), View->LogicalPartCount(), Logical);
        TestEqual(FString::Printf(TEXT("%s: the same logical props, loose or instanced"), Name), View->LogicalPropCount(), Props);
        TestEqual(FString::Printf(TEXT("%s: the same obstacles"), Name), View->PlacedObstacles(), Obstacles);
        TestEqual(FString::Printf(TEXT("%s: the same dressing"), Name), View->PlacedDressing().Num(), Dressing);
        TestTrue(FString::Printf(TEXT("%s: loose parts need clearly more components (%d against %d)"), Name, LooseParts, Parts), LooseParts > Parts * 2);
        AddInfo(FString::Printf(TEXT("%s board, loose: %d part components"), Name, LooseParts));
        View->SetLooseParts(false);
        View->Tick(0.016f);
        TestEqual(FString::Printf(TEXT("%s: back to the instanced count"), Name), View->PresentationComponentCount(false), Parts);
        TestEqual(FString::Printf(TEXT("%s: back to the same instances"), Name), OwnedInstances(*View), Instances);

        // Rebuilding must not leak components or instances: the board metric rebuild runs the same path as every other rebuild.
        for (int32 Round = 0; Round < 3; ++Round)
        {
            View->SetBoardMetric(View->Metric());
            View->Tick(0.016f);
            TestEqual(FString::Printf(TEXT("%s: rebuild %d keeps the owned components"), Name, Round), OwnedComponents(*View), Owned);
            TestEqual(FString::Printf(TEXT("%s: rebuild %d keeps the instances"), Name, Round), OwnedInstances(*View), Instances);
            TestEqual(FString::Printf(TEXT("%s: rebuild %d keeps the logical parts"), Name, Round), View->LogicalPartCount(), Logical);
        }
    }
    if (Checked == 0) { AddInfo(TEXT("No themed board was checked.")); }
    DestroyPartsWorld(World);
    IFileManager::Get().DeleteDirectory(*Directory, false, true);
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInstancedPartsFallbackTest, "Project.Expedition.Presentation.InstancedPartsPrimitiveFallbackUnchanged",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FInstancedPartsFallbackTest::RunTest(const FString& Parameters)
{
    // The real -ExpeditionPrimitives path: the flag is read in BeginPlay. A primitive board keeps one component per part, as before.
    const FString Original = FCommandLine::Get();
    FCommandLine::Set(*(Original + TEXT(" -ExpeditionPrimitives")));
    UWorld* World = MakePartsWorld();
    const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation/InstancedPartsPrimitives"), FGuid::NewGuid().ToString());
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
        View->ChooseRoute(0);
        View->HoldEnemyTurns(true);
        View->Tick(0.016f);
        TestFalse(TEXT("-ExpeditionPrimitives keeps the theme off"), View->UsesTheme());
        TestFalse(TEXT("A primitive board is not instanced"), View->UsesInstancedParts());
        TestFalse(TEXT("A primitive board has no ring bank"), View->UsesRingBank());
        TestFalse(TEXT("A primitive board has no instanced effects"), View->UsesInstancedEffects());
        TestEqual(TEXT("A primitive board has no instanced groups"), View->InstancedGroupCount(), 0);
        const FCombatEncounter& Encounter = View->Rules().Snapshot().Encounter;
        TestEqual(TEXT("A primitive board keeps one cube tile per cell"), View->TileCount(), Encounter.Width * Encounter.Height);
        TestTrue(TEXT("A primitive board has one component per part"), View->PresentationComponentCount(false) >= View->TileCount());
        TestTrue(TEXT("A primitive board has parts"), View->LogicalPartCount() > 0);
        const int32 Parts = View->LogicalPartCount();
        View->SetBoardMetric(View->Metric());
        TestEqual(TEXT("A primitive rebuild keeps the same parts"), View->LogicalPartCount(), Parts);
        for (const FCombatantState& Unit : Encounter.Combatants)
        { TestEqual(FString::Printf(TEXT("%s has no footprint ring in the primitive look"), *Unit.Name), View->FootprintRingDiameterCm(Unit.Id), 0.0f); }
    }
    DestroyPartsWorld(World);
    IFileManager::Get().DeleteDirectory(*Directory, false, true);
    return !HasAnyErrors();
}
#endif
