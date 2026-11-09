#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Presentation/UnitPose.h"
#include "DiceRollTheme.generated.h"

class UAnimSequence;
class UMaterialInterface;
class USkeletalMesh;
class UStaticMesh;

/** One clip per pose of the shared humanoid skeleton (#147). Property names match EUnitPose's clip names. */
USTRUCT(BlueprintType)
struct DNDDICEROLL_API FDiceRollAnimSet
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, Category = "Animation") TSoftObjectPtr<UAnimSequence> Idle;
    UPROPERTY(EditAnywhere, Category = "Animation") TSoftObjectPtr<UAnimSequence> Walk;
    UPROPERTY(EditAnywhere, Category = "Animation") TSoftObjectPtr<UAnimSequence> Melee;
    UPROPERTY(EditAnywhere, Category = "Animation") TSoftObjectPtr<UAnimSequence> Ranged;
    UPROPERTY(EditAnywhere, Category = "Animation") TSoftObjectPtr<UAnimSequence> Cast;
    UPROPERTY(EditAnywhere, Category = "Animation") TSoftObjectPtr<UAnimSequence> Hit;
    UPROPERTY(EditAnywhere, Category = "Animation") TSoftObjectPtr<UAnimSequence> Downed;
    UPROPERTY(EditAnywhere, Category = "Animation") TSoftObjectPtr<UAnimSequence> Death;
    UPROPERTY(EditAnywhere, Category = "Animation") TSoftObjectPtr<UAnimSequence> Recover;

    const TSoftObjectPtr<UAnimSequence>& For(EUnitPose Pose) const;
};

/**
 * A rigged character: a skeletal mesh on the shared skeleton and its clips. Optional like every theme entry; an unset or
 * incomplete definition (any clip missing) leaves the unit on its static r001 mesh, then on primitives.
 */
USTRUCT(BlueprintType)
struct DNDDICEROLL_API FDiceRollCharacter
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, Category = "Character") TSoftObjectPtr<USkeletalMesh> Mesh;
    UPROPERTY(EditAnywhere, Category = "Character") FDiceRollAnimSet Animations;
    /** Extra yaw in degrees if the import faces the wrong way (the r001 exports face +X). */
    UPROPERTY(EditAnywhere, Category = "Character", meta = (Units = "deg")) float YawOffset = 0.0f;

    /** Loads the mesh and all nine clips (indexed by EUnitPose). False, with nothing returned, when any is missing. */
    bool Load(USkeletalMesh*& OutMesh, TArray<UAnimSequence*>& OutClips) const;
};

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

    /** Rigged characters (#147): the roster entries below use these when imported, otherwise the static meshes above. */
    UPROPERTY(EditDefaultsOnly, Category = "Rigged Characters") FDiceRollCharacter FighterRig;
    UPROPERTY(EditDefaultsOnly, Category = "Rigged Characters") FDiceRollCharacter WizardRig;
    UPROPERTY(EditDefaultsOnly, Category = "Rigged Characters") FDiceRollCharacter RaiderRig;

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
