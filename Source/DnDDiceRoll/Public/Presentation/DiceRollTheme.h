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
    /** The board metric: one cell in cm (150 = 5 ft). Art authored for r001 is scaled from 110 cm to this. */
    UPROPERTY(EditDefaultsOnly, Category = "Scale", meta = (ClampMin = "50", Units = "cm")) float CellSize = 150.0f;
    /** How tall a character stands above the board in cm (175 to 185). Themed meshes are fitted to it. */
    UPROPERTY(EditDefaultsOnly, Category = "Scale", meta = (ClampMin = "100", Units = "cm")) float FigureHeight = 180.0f;

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

    /**
     * The Drowned Vigil world kit r002 (#145). Everything in it is authored at the true 150 cm cell, unlike the r001 art
     * above (110 cm, scaled by the board metric), so it is placed unscaled. The import script sets bMetricKit only when the
     * world and the three variant-A props are all r002; until then the r001 entries above are what the board draws, and
     * the rest of this group is ignored. Every entry is optional and falls back individually.
     */
    UPROPERTY(EditDefaultsOnly, Category = "Vigil Kit") bool bMetricKit = false;
    /** Near architecture split from the world; same origin as VigilWorld. Dither-fades when it blocks the camera. */
    UPROPERTY(EditDefaultsOnly, Category = "Vigil Kit") TSoftObjectPtr<UStaticMesh> VigilOccluders;
    /** Three variants (A, B, C) per obstacle recipe; the variant of a cell is chosen from the seed and the cell. */
    UPROPERTY(EditDefaultsOnly, Category = "Vigil Kit") TArray<TSoftObjectPtr<UStaticMesh>> PillarVariants;
    UPROPERTY(EditDefaultsOnly, Category = "Vigil Kit") TArray<TSoftObjectPtr<UStaticMesh>> WallVariants;
    UPROPERTY(EditDefaultsOnly, Category = "Vigil Kit") TArray<TSoftObjectPtr<UStaticMesh>> StonesVariants;
    /** Spans two adjacent blocked cells; pivot at the midpoint of their shared edge, long axis Y. */
    UPROPERTY(EditDefaultsOnly, Category = "Vigil Kit") TSoftObjectPtr<UStaticMesh> ArchTwoCell;
    /** Non-blocking floor dressing (rubble, candles, bones, roots, weapons, puddle, urn, moss), outside the board. */
    UPROPERTY(EditDefaultsOnly, Category = "Vigil Kit") TArray<TSoftObjectPtr<UStaticMesh>> Scatter;
    /** Translucent mist cards (upright, leaning, flat) and light shafts (prism, crossed planes). */
    UPROPERTY(EditDefaultsOnly, Category = "Vigil Kit") TArray<TSoftObjectPtr<UStaticMesh>> MistCards;
    UPROPERTY(EditDefaultsOnly, Category = "Vigil Kit") TArray<TSoftObjectPtr<UStaticMesh>> LightShafts;

    /** Loads an entry; null when it is unset or missing. */
    static UStaticMesh* Load(const TSoftObjectPtr<UStaticMesh>& Mesh);
};
