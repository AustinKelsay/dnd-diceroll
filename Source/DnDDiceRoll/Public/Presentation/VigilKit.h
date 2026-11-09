#pragma once

#include "CoreMinimal.h"

/**
 * Placement rules of the Drowned Vigil world kit r002 (#145): which obstacle variant a cell gets, where the 2-cell
 * arches stand, and where the scatter, mist cards and light shafts go. Pure presentation data derived from the board
 * size, the blocked cells and the encounter seed with a private hash: it never reads or advances rules state or any
 * random stream, so the theme choice cannot change a checkpoint or a snapshot.
 *
 * Cells are board cells (the centre of cell (X, Y) is (X, Y)); the view converts them with the one board metric.
 */
namespace VigilKit
{
/** Scatter sits at least two cells outside the board (one empty gutter cell keeps the overlay contour clear). */
constexpr int32 ScatterMinRing = 2;
constexpr int32 ScatterMaxRing = 3;
/** Mist cards are up to 3 m across, so they start one cell farther out. */
constexpr int32 MistMinRing = 3;
constexpr int32 MistMaxRing = 5;
/** Light shafts lean toward -X, so their floor patch is on the -X side of the board, where the lean points away from it. */
constexpr int32 ShaftMaxX = -3;
constexpr int32 ScatterCount = 18;
constexpr int32 MistCount = 4;
constexpr int32 ShaftCount = 2;

/** A stable 32-bit hash of (seed, salt, cell); identical on every platform and engine version. */
DNDDICEROLL_API uint32 Hash(int32 Seed, int32 Salt, FIntPoint Cell);
/** Hash scaled to [0, 1). */
DNDDICEROLL_API float Unit(int32 Seed, int32 Salt, FIntPoint Cell);
/** The variant (0 .. Count-1) of a recipe's obstacle on a cell; 0 when Count is 1 or less. */
DNDDICEROLL_API int32 VariantIndex(int32 Seed, int32 Recipe, FIntPoint Cell, int32 Count);
/** Chebyshev distance in cells from a cell to the Width x Height board; 0 for every cell on the board. */
DNDDICEROLL_API int32 RingDistance(FIntPoint Cell, int32 Width, int32 Height);

/** Two orthogonally adjacent blocked cells covered by one arch. */
struct FArch
{
    FIntPoint A = FIntPoint::ZeroValue;
    FIntPoint B = FIntPoint::ZeroValue;
    /** True when the cells are X-adjacent (the arch is turned 90 degrees); false when they are Y-adjacent (authored orientation). */
    bool bAlongX = false;
    /** The midpoint of the shared edge, in cells. */
    FVector2D Midpoint() const { return FVector2D((A.X + B.X) * 0.5, (A.Y + B.Y) * 0.5); }
    float Yaw() const { return bAlongX ? 90.0f : 0.0f; }
};

/**
 * Greedy, order-independent pairing of adjacent blocked cells: scan in row-major order and pair each free cell with its
 * free +X neighbour, else its free +Y neighbour. Every blocked cell is in at most one arch.
 */
DNDDICEROLL_API TArray<FArch> ArchPairs(const TArray<FIntPoint>& Blocked);

enum class EDressingKind : uint8 { Scatter, Mist, Shaft };

struct FDressing
{
    EDressingKind Kind = EDressingKind::Scatter;
    /** Index into the theme's array for this kind. */
    int32 Variant = 0;
    FIntPoint Cell = FIntPoint::ZeroValue;
    /** Offset from the cell centre in cells; never more than a quarter cell. */
    FVector2D Offset = FVector2D::ZeroVector;
    float Yaw = 0.0f;
    FVector2D Position() const { return FVector2D(Cell.X + Offset.X, Cell.Y + Offset.Y); }
};

/** How many variants the theme has for each kind (zero switches the kind off). */
struct FDressingKinds
{
    int32 Scatter = 0;
    int32 Mist = 0;
    int32 Shaft = 0;
};

/**
 * The dressing of a Width x Height board: ScatterCount scatter pieces on cells 2 to 3 outside the board, MistCount mist
 * cards 3 to 5 outside it, ShaftCount light shafts on the -X side. No two share a cell, and nothing is ever on or beside
 * a playable cell. Deterministic in (Width, Height, Seed, Kinds).
 */
DNDDICEROLL_API TArray<FDressing> PlanDressing(int32 Width, int32 Height, int32 Seed, const FDressingKinds& Kinds);
}
