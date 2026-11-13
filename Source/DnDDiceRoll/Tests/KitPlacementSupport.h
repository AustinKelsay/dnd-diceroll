#pragma once

#if WITH_DEV_AUTOMATION_TESTS
#include "CoreMinimal.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Misc/AutomationTest.h"
#include "Presentation/DiceRollTheme.h"
#include "Presentation/ExpeditionBattleView.h"
#include "Presentation/VigilKit.h"

/**
 * Test support (#188 review of #216): what a board actually placed on its obstacle cells, read from the view's own components (each instance of
 * each instanced set, or each loose mesh component), not from any value the view caches about what it meant to draw.
 */
namespace KitPlacementSupport
{
/** Every mesh the view placed, at its view-actor-space position. */
inline TArray<TPair<FVector, const UStaticMesh*>> PlacedMeshes(const AExpeditionBattleView& View)
{
    TArray<TPair<FVector, const UStaticMesh*>> Placed;
    const FTransform Actor = View.GetActorTransform();
    TInlineComponentArray<UStaticMeshComponent*> Parts;
    View.GetComponents(Parts);
    for (const UStaticMeshComponent* Part : Parts)
    {
        const UStaticMesh* Mesh = Part ? Part->GetStaticMesh() : nullptr;
        if (!Mesh) { continue; }
        if (const UInstancedStaticMeshComponent* Set = Cast<UInstancedStaticMeshComponent>(Part))
        {
            for (int32 I = 0; I < Set->GetInstanceCount(); ++I)
            {
                FTransform World;
                if (Set->GetInstanceTransform(I, World, true) && !World.GetScale3D().IsNearlyZero()) { Placed.Add({Actor.InverseTransformPosition(World.GetLocation()), Mesh}); }
            }
        }
        else if (Part->IsVisible()) { Placed.Add({Actor.InverseTransformPosition(Part->GetComponentLocation()), Mesh}); }
    }
    return Placed;
}

inline TSet<const UStaticMesh*> Load(const TArray<TSoftObjectPtr<UStaticMesh>>& Entries)
{
    TSet<const UStaticMesh*> Meshes;
    for (const TSoftObjectPtr<UStaticMesh>& Entry : Entries) { if (const UStaticMesh* Mesh = Entry.LoadSynchronous()) { Meshes.Add(Mesh); } }
    return Meshes;
}

/** The meshes of Candidates placed within a centimetre of Spot. */
inline TArray<const UStaticMesh*> At(const TArray<TPair<FVector, const UStaticMesh*>>& Placed, const FVector& Spot, const TSet<const UStaticMesh*>& Candidates)
{
    TArray<const UStaticMesh*> Found;
    for (const TPair<FVector, const UStaticMesh*>& Piece : Placed) { if (Candidates.Contains(Piece.Value) && FVector::Dist(Piece.Key, Spot) < 1.0f) { Found.Add(Piece.Value); } }
    return Found;
}

/**
 * Checks the obstacle pieces of the current board against Kit drawing obstacle family Kind (0 stones, 1 pillars, 2 walls):
 * - every blocked cell has a spot on its cell (or, under an arch, at its pair's shared-edge midpoint) and exactly one obstacle mesh there;
 * - that mesh is one of Kit's Kind variants (the arch mesh on arch spots), never another family's variant;
 * - the arches are exactly VigilKit::ArchPairs of the blocked cells when the family takes arches (ArchesFor) and the kit has an arch mesh, otherwise none.
 * Returns the number of arch pairs found placed.
 */
inline int32 CheckObstacles(FAutomationTestBase& Test, const FString& Where, const AExpeditionBattleView& View, const FDiceRollWorldKit& Kit, int32 Kind)
{
    const FCombatEncounter& Encounter = View.Rules().Snapshot().Encounter;
    const TSet<const UStaticMesh*> Expected = Load(Kit.Variants(Kind));
    TSet<const UStaticMesh*> Others;
    for (int32 Other = 0; Other < 3; ++Other) { if (Other != Kind) { Others.Append(Load(Kit.Variants(Other))); } }
    Others = Others.Difference(Expected);
    const UStaticMesh* Arch = Kit.Arch ? Kit.Arch->LoadSynchronous() : nullptr;
    TSet<const UStaticMesh*> Obstacles = Expected.Union(Others);
    if (Arch) { Obstacles.Add(Arch); }
    if (!Test.TestFalse(Where + TEXT(": the kit has variants for this family"), Expected.IsEmpty())) { return 0; }
    const TArray<TPair<FVector, const UStaticMesh*>> Placed = PlacedMeshes(View);
    const float Cell = View.Metric().CellCm;
    const TArray<VigilKit::FArch> Pairs = VigilKit::ArchesFor(Kind) && Arch ? VigilKit::ArchPairs(Encounter.BlockedCells) : TArray<VigilKit::FArch>();
    TSet<FIntPoint> Arched;
    int32 ArchesPlaced = 0;
    for (const VigilKit::FArch& Pair : Pairs)
    {
        Arched.Add(Pair.A); Arched.Add(Pair.B);
        const FVector* SpotA = View.ObstacleSpots().Find(Pair.A);
        const FVector* SpotB = View.ObstacleSpots().Find(Pair.B);
        if (!Test.TestTrue(Where + TEXT(": both cells of an arch have its spot"), SpotA && SpotB)) { continue; }
        Test.TestTrue(Where + TEXT(": the two cells share one arch spot"), SpotA->Equals(*SpotB, 0.01f));
        Test.TestEqual(Where + TEXT(": the arch stands over the shared edge (X)"), static_cast<float>(SpotA->X), static_cast<float>(Pair.Midpoint().X) * Cell, 0.01f);
        Test.TestEqual(Where + TEXT(": the arch stands over the shared edge (Y)"), static_cast<float>(SpotA->Y), static_cast<float>(Pair.Midpoint().Y) * Cell, 0.01f);
        const TArray<const UStaticMesh*> Found = At(Placed, *SpotA, Obstacles);
        if (Test.TestEqual(Where + TEXT(": one obstacle piece at the arch spot"), Found.Num(), 1) && Found[0] == Arch) { ++ArchesPlaced; }
        else { Test.AddError(Where + TEXT(": the piece over an adjacent pair is not the kit's arch")); }
    }
    for (const FIntPoint& Blocked : Encounter.BlockedCells)
    {
        const FString At_ = FString::Printf(TEXT("%s (%d, %d)"), *Where, Blocked.X, Blocked.Y);
        const FVector* Spot = View.ObstacleSpots().Find(Blocked);
        if (!Test.TestNotNull(At_ + TEXT(": the blocked cell has an obstacle piece"), Spot) || Arched.Contains(Blocked)) { continue; }
        Test.TestEqual(At_ + TEXT(": the piece stands on its cell (X)"), static_cast<float>(Spot->X), Blocked.X * Cell, 0.01f);
        Test.TestEqual(At_ + TEXT(": the piece stands on its cell (Y)"), static_cast<float>(Spot->Y), Blocked.Y * Cell, 0.01f);
        const TArray<const UStaticMesh*> Found = At(Placed, *Spot, Obstacles);
        if (!Test.TestEqual(At_ + TEXT(": exactly one obstacle mesh on the cell"), Found.Num(), 1)) { continue; }
        Test.TestTrue(At_ + TEXT(": the mesh is one of the family's variants"), Expected.Contains(Found[0]));
        Test.TestFalse(At_ + TEXT(": the mesh is not another family's"), Others.Contains(Found[0]) || Found[0] == Arch);
    }
    Test.TestEqual(Where + TEXT(": the view counts the arches it placed"), View.PlacedArches(), ArchesPlaced);
    Test.TestEqual(Where + TEXT(": one piece per arch pair and per other blocked cell"), View.PlacedObstacles(), ArchesPlaced + Encounter.BlockedCells.Num() - Arched.Num());
    return ArchesPlaced;
}
}
#endif
