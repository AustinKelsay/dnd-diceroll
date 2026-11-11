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
 * What one world draws its kit with (#145 Drowned Vigil, #149 Root of the World): read-only views of the theme's own
 * properties, so they stay valid while the theme does. A world with no kit switched on has bMetric false and the view
 * ignores everything else; every entry is optional and falls back individually.
 */
struct FDiceRollWorldKit
{
    /** The world and its kit are authored at the true 150 cm cell and are placed unscaled. */
    bool bMetric = false;
    const TSoftObjectPtr<UStaticMesh>* Occluders = nullptr;
    const TArray<TSoftObjectPtr<UStaticMesh>>* Pillars = nullptr;
    const TArray<TSoftObjectPtr<UStaticMesh>>* Walls = nullptr;
    const TArray<TSoftObjectPtr<UStaticMesh>>* Stones = nullptr;
    const TSoftObjectPtr<UStaticMesh>* Arch = nullptr;
    const TArray<TSoftObjectPtr<UStaticMesh>>* Scatter = nullptr;
    const TArray<TSoftObjectPtr<UStaticMesh>>* Mist = nullptr;
    const TArray<TSoftObjectPtr<UStaticMesh>>* Shafts = nullptr;

    /** The obstacle variants of a recipe: 1 pillar, 2 wall, anything else stones. */
    const TArray<TSoftObjectPtr<UStaticMesh>>& Variants(int32 Recipe) const { return Recipe == 1 ? *Pillars : Recipe == 2 ? *Walls : *Stones; }
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

    /**
     * Rigged party characters (#150), by class: the key is the role's enum name ("Fighter", "Rogue", "Cleric", "Wizard", ...; see
     * ClassKey). A later cast batch gives another class a rig by adding an entry here, by data only. A class with no usable entry
     * (missing, or its mesh unset) falls back to its static r001 mesh, then to primitives; an entry whose mesh or any clip fails
     * to load does too (FDiceRollCharacter::Load).
     */
    UPROPERTY(EditDefaultsOnly, Category = "Rigged Characters") TMap<FName, FDiceRollCharacter> ClassRigs;
    /** Rigged foes by archetype name ("Raider"; the scout and archer follow in a later batch). */
    UPROPERTY(EditDefaultsOnly, Category = "Rigged Characters") TMap<FName, FDiceRollCharacter> FoeRigs;

    /**
     * The three pilot fields (#147), kept only so a theme saved before #150 still resolves its rigs: RigForClass and RigForFoe read
     * them when the maps have no entry. The import script moves them into the maps; new work uses the maps.
     */
    UPROPERTY(EditDefaultsOnly, Category = "Rigged Characters|Pilot (superseded by ClassRigs and FoeRigs)") FDiceRollCharacter FighterRig;
    UPROPERTY(EditDefaultsOnly, Category = "Rigged Characters|Pilot (superseded by ClassRigs and FoeRigs)") FDiceRollCharacter WizardRig;
    UPROPERTY(EditDefaultsOnly, Category = "Rigged Characters|Pilot (superseded by ClassRigs and FoeRigs)") FDiceRollCharacter RaiderRig;

    /** The ClassRigs key of a class: the role's enum name ("Rogue"). None has no key. */
    static FName ClassKey(EAdventurerRole Role);
    /** The rig definition of a class, or null when it has none (the caller falls back to the static mesh, then primitives). */
    const FDiceRollCharacter* RigForClass(EAdventurerRole Role) const;
    /** The rig definition of a foe archetype, or null. */
    const FDiceRollCharacter* RigForFoe(FName Archetype) const;

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

    /**
     * The Root of the World world kit r002 (#149), the forest equivalent of the Vigil kit above and built the same way:
     * authored at the true 150 cm cell, placed unscaled. The Root world keeps the RootWorld entry (its r002 export replaces
     * the r001 mesh at the same package), so the import script sets bRootMetricKit only when that mesh carries the r002 tag;
     * until then Root boards draw the r001 world scaled, and the rest of this group is ignored. Root obstacles are new
     * packages because the r001 pillar, wall and stones are shared with the Vigil. Every entry falls back individually.
     */
    UPROPERTY(EditDefaultsOnly, Category = "Root Kit") bool bRootMetricKit = false;
    /** Near architecture (arching roots) split from the world; same origin as RootWorld. Dither-fades when it blocks the camera. */
    UPROPERTY(EditDefaultsOnly, Category = "Root Kit") TSoftObjectPtr<UStaticMesh> RootOccluders;
    /** Three variants (A, B, C) per obstacle recipe: spires and stumps (pillar), root curtains and logs (wall), boulders (stones). */
    UPROPERTY(EditDefaultsOnly, Category = "Root Kit") TArray<TSoftObjectPtr<UStaticMesh>> RootPillarVariants;
    UPROPERTY(EditDefaultsOnly, Category = "Root Kit") TArray<TSoftObjectPtr<UStaticMesh>> RootWallVariants;
    UPROPERTY(EditDefaultsOnly, Category = "Root Kit") TArray<TSoftObjectPtr<UStaticMesh>> RootStonesVariants;
    /** Spans two adjacent blocked cells; pivot at the midpoint of their shared edge, long axis Y. */
    UPROPERTY(EditDefaultsOnly, Category = "Root Kit") TSoftObjectPtr<UStaticMesh> RootArchTwoCell;
    /** Non-blocking floor dressing (mushrooms, leaves, twigs, tendrils, ferns, pebbles, glow moss, husks), outside the board. */
    UPROPERTY(EditDefaultsOnly, Category = "Root Kit") TArray<TSoftObjectPtr<UStaticMesh>> RootScatter;
    /** Translucent mist cards (upright, leaning, flat) and light shafts (prism, crossed planes). */
    UPROPERTY(EditDefaultsOnly, Category = "Root Kit") TArray<TSoftObjectPtr<UStaticMesh>> RootMistCards;
    UPROPERTY(EditDefaultsOnly, Category = "Root Kit") TArray<TSoftObjectPtr<UStaticMesh>> RootLightShafts;

    /** The kit a board draws with: the Root kit on a forest (Meadow) board, the Vigil kit on every other. */
    FDiceRollWorldKit KitFor(bool bForest) const;

    /** Loads an entry; null when it is unset or missing. */
    static UStaticMesh* Load(const TSoftObjectPtr<UStaticMesh>& Mesh);
};
