#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Combat/CombatBattle.h"
#include "ExpeditionSpellFX.generated.h"

class UMaterialInstanceDynamic;
class UMaterialInterface;
class UStaticMesh;
class UStaticMeshComponent;

/**
 * Spell VFX trial (candlelit arcane direction, drafts/11): low-poly emissive meshes driven by
 * one parent material (M_SpellFX, instance MI_SpellFX_Evocation). Plays from the rules' spell
 * hook data (FSpellCastEvent) and never reads rules back. Only Fire Bolt and Fireball have a
 * style today; every other spell returns false from Play and keeps its existing presentation.
 * Timings, sizes and colours live in FExpeditionSpellFXStyle so a revision is one table edit.
 */
USTRUCT()
struct FExpeditionSpellFXPiece
{
    GENERATED_BODY()
    UPROPERTY() TObjectPtr<UStaticMeshComponent> Mesh;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> Ink;
    UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> Material;
    UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> InkMaterial;
    float Start = 0.0f, Peak = 0.0f, End = 0.0f, Linger = 0.0f;
    float PeakOpacity = 1.0f, LingerOpacity = 0.0f;
    FVector From = FVector::ZeroVector, To = FVector::ZeroVector;
    FVector ScaleFrom = FVector::OneVector, ScaleTo = FVector::OneVector;
    float InkScale = 1.08f;
    bool bFlatInk = false;
};

struct DNDDICEROLL_API FExpeditionSpellFXStyle
{
    FLinearColor Core, Hue, Ink;
    float Glow = 3.0f;
    float Edge = 0.6f;
    // Projectile (both spells): travel time and size (scale of the 100 cm shard).
    float Travel = 0.2f;
    float BoltScale = 0.35f;
    // Impact: start after travel, grow, then fade; Linger keeps LingerOpacity until it ends.
    float ImpactGrow = 0.12f, ImpactHold = 0.05f, ImpactLinger = 0.05f, LingerOpacity = 0.0f;
    float ImpactScale = 0.45f;   // cantrip burst diameter in metres (Ico mesh is 1 m)
    // Area spells: telegraph shown first on the exact affected cells.
    float Telegraph = 0.0f;
    bool bArea = false;
    /** Fire Bolt and Fireball only (the trial). */
    static bool For(const FSpellCastEvent& Event, FExpeditionSpellFXStyle& Out);
};

UCLASS(NotBlueprintable)
class DNDDICEROLL_API AExpeditionSpellFX : public AActor
{
    GENERATED_BODY()
public:
    AExpeditionSpellFX();
    /** Spawns the effect for a Cast event. CellToWorld maps a grid cell to its board-top centre. */
    static AExpeditionSpellFX* Play(UWorld* World, const FSpellCastEvent& Event, TFunctionRef<FVector(FIntPoint)> CellToWorld);
    virtual void Tick(float DeltaSeconds) override;
    float Duration() const { return Total; }
private:
    void Build(const FSpellCastEvent& Event, const FExpeditionSpellFXStyle& Style, TFunctionRef<FVector(FIntPoint)> CellToWorld);
    FExpeditionSpellFXPiece& Add(UStaticMesh* Mesh, const FExpeditionSpellFXStyle& Style, bool bInk, FVector Lead, bool bRadial, bool bInkFill = false);
    UPROPERTY() TArray<FExpeditionSpellFXPiece> Pieces;
    UPROPERTY() TObjectPtr<UMaterialInterface> Material;
    float Age = 0.0f;
    float Total = 0.0f;
};
