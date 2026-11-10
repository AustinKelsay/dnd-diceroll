#pragma once

#include "CoreMinimal.h"

/**
 * Where the braziers of a themed battle board stand (#148). They ring the board perimeter, one every four cells along each
 * edge, a fixed distance outside the playable edge, with a small deterministic jitter along the edge. Pure presentation data
 * derived from the board size and the encounter seed through the private `VigilKit::Unit` hash: it never reads or advances
 * rules state or any random stream, so the theme choice cannot change a checkpoint or a snapshot. The count follows the board,
 * not the world, so it is the same on the r001 worlds, the Root and the Vigil.
 *
 * Cells are board cells (the centre of cell (X, Y) is (X, Y), so a board edge is at -0.5 and Width - 0.5 / Height - 0.5); the
 * view converts them with the one board metric.
 */
namespace BrazierRing
{
/** One brazier per this many cells of an edge (rounded down, never fewer than MinPerEdge). */
constexpr int32 CellsPerBrazier = 4;
constexpr int32 MinPerEdge = 2;
/** A brazier stands this far outside the playable edge, in cells: past the overlay's gutter cell and beyond the occluder's board margin (0.5). */
constexpr float OutsideCells = 1.25f;
/** Seed jitter along the edge, plus or minus, in cells. Never across the edge, so the distance outside is exact. */
constexpr float JitterCells = 0.25f;
/** Gate 1: at most two local lights cast shadows (the sun is separate). The view shadows this many braziers, the nearest the board centre. */
constexpr int32 MaxShadowed = 2;
/**
 * The braziers share the light the seven of #143 gave a board (7 x 40 cd): each gets TotalIntensity / count, so the average
 * level matches that board whatever its size. The radius is unchanged (900 reference cm).
 */
constexpr float TotalIntensity = 280.0f;
constexpr float LightRadius = 900.0f;
/** The kit's scatter keeps this many cells (centre to centre) from a brazier: see VigilKit::PlanDressing. */
constexpr float ScatterClearCells = 1.0f;

enum class EEdge : uint8 { South, North, West, East };

struct FBrazier
{
    EEdge Edge = EEdge::South;
    /** Index along the edge, from the low coordinate. */
    int32 Slot = 0;
    /** Where it stands, in cells. */
    FVector2D Cell = FVector2D::ZeroVector;
};

/** Braziers along an edge of this many cells: Length / CellsPerBrazier, at least MinPerEdge. */
DNDDICEROLL_API int32 EdgeCount(int32 Length);
/** Every brazier of a Width x Height board: 2 x (EdgeCount(Width) + EdgeCount(Height)). */
DNDDICEROLL_API int32 Count(int32 Width, int32 Height);
/**
 * The braziers of a Width x Height board in a fixed order (south, north, west, east edges, each from its low coordinate).
 * Each edge is split into EdgeCount equal spans with a brazier at the middle of each, moved along the edge by the seed's
 * jitter. Deterministic in (Width, Height, Seed).
 */
DNDDICEROLL_API TArray<FBrazier> Plan(int32 Width, int32 Height, int32 Seed);
/** The brazier positions alone, in cells (what VigilKit::PlanDressing avoids). */
DNDDICEROLL_API TArray<FVector2D> Cells(const TArray<FBrazier>& Braziers);
/** Each brazier light's intensity (candela) on a board with this many braziers. */
DNDDICEROLL_API float Intensity(int32 BrazierCount);
/** True when the position (cells) is more than `Clearance` cells outside the playable cells' rectangle on at least one axis. */
DNDDICEROLL_API bool OutsideBoard(FVector2D Cell, int32 Width, int32 Height, float Clearance);
}
