#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "DiceRollTheme.generated.h"

class UMaterialInterface;
class UStaticMesh;

/**
 * The dark-fantasy art set the battle view draws with. Every entry is optional: a missing
 * mesh falls back to the engine-primitive presentation, so rules and automation never
 * depend on art. Meshes use one tintable material whose base is TintMaterial.
 */
UCLASS(BlueprintType)
class DNDDICEROLL_API UDiceRollTheme : public UPrimaryDataAsset
{
    GENERATED_BODY()
public:
    /** Parent of every themed mesh material: texture x vertex colour x "Color" tint. */
    UPROPERTY(EditDefaultsOnly, Category = "Theme") TSoftObjectPtr<UMaterialInterface> TintMaterial;

    UPROPERTY(EditDefaultsOnly, Category = "Characters") TSoftObjectPtr<UStaticMesh> Fighter;
    UPROPERTY(EditDefaultsOnly, Category = "Characters") TSoftObjectPtr<UStaticMesh> Rogue;
    UPROPERTY(EditDefaultsOnly, Category = "Characters") TSoftObjectPtr<UStaticMesh> Cleric;
    UPROPERTY(EditDefaultsOnly, Category = "Characters") TSoftObjectPtr<UStaticMesh> Wizard;
    UPROPERTY(EditDefaultsOnly, Category = "Characters") TSoftObjectPtr<UStaticMesh> Raider;
    UPROPERTY(EditDefaultsOnly, Category = "Characters") TSoftObjectPtr<UStaticMesh> Scout;
    UPROPERTY(EditDefaultsOnly, Category = "Characters") TSoftObjectPtr<UStaticMesh> Archer;

    /** Surrounding worlds, authored around an 18 x 12 cell footprint centred on the origin. */
    UPROPERTY(EditDefaultsOnly, Category = "Worlds") TSoftObjectPtr<UStaticMesh> RootWorld;
    UPROPERTY(EditDefaultsOnly, Category = "Worlds") TSoftObjectPtr<UStaticMesh> VigilWorld;
    /** One cell, origin at the bottom centre. */
    UPROPERTY(EditDefaultsOnly, Category = "Worlds") TSoftObjectPtr<UStaticMesh> RootTile;
    UPROPERTY(EditDefaultsOnly, Category = "Worlds") TSoftObjectPtr<UStaticMesh> VigilTile;
    /** Height of a tile mesh in cm; tiles sit on the world ground with their tops at board level. */
    UPROPERTY(EditDefaultsOnly, Category = "Worlds", meta = (ClampMin = "1", Units = "cm")) float TileHeight = 16.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Props") TSoftObjectPtr<UStaticMesh> Stones;
    UPROPERTY(EditDefaultsOnly, Category = "Props") TSoftObjectPtr<UStaticMesh> Pillar;
    UPROPERTY(EditDefaultsOnly, Category = "Props") TSoftObjectPtr<UStaticMesh> Wall;
    UPROPERTY(EditDefaultsOnly, Category = "Props") TSoftObjectPtr<UStaticMesh> Brazier;
    UPROPERTY(EditDefaultsOnly, Category = "Props") TSoftObjectPtr<UStaticMesh> Flame;
    UPROPERTY(EditDefaultsOnly, Category = "Props") TSoftObjectPtr<UStaticMesh> Banner;
    UPROPERTY(EditDefaultsOnly, Category = "Props") TSoftObjectPtr<UStaticMesh> Cage;

    /** Mission overview: the candlelit table and low-detail route dioramas. */
    UPROPERTY(EditDefaultsOnly, Category = "Overview") TSoftObjectPtr<UStaticMesh> Tabletop;
    UPROPERTY(EditDefaultsOnly, Category = "Overview") TSoftObjectPtr<UStaticMesh> RootDiorama;
    UPROPERTY(EditDefaultsOnly, Category = "Overview") TSoftObjectPtr<UStaticMesh> VigilDiorama;

    /** Loads an entry; null when it is unset or missing. */
    static UStaticMesh* Load(const TSoftObjectPtr<UStaticMesh>& Mesh);
};
