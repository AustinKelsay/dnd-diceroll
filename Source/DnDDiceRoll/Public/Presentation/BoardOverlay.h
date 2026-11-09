#pragma once

#include "CoreMinimal.h"

struct FBoardHighlights;

/**
 * The contextual grid of a themed battle board (#142): a per-cell state mask and the thin bars that draw it.
 * Both are pure presentation data. The mask is derived from what the controller asks the board to emphasise
 * (which in turn comes from the authoritative rules queries); nothing here reads or changes rules state.
 * Bars are in cell units on the board plane: the centre of cell (X, Y) is (X, Y), a cell edge is at +-0.5.
 * Widths and lengths are fractions of a cell, so the metric converts them to world cm in one place.
 */
namespace BoardOverlay
{
/** One state bit per cell. A cell can carry several (a reachable cell under the hover, for example). */
enum EBit : uint8
{
    /** A cell the actor can legally move into (Attack mode). */
    Move = 1 << 0,
    /** A cell the selected action reaches with clear sight. */
    Reach = 1 << 1,
    /** A cell in geometric range whose sight is blocked. */
    Obstructed = 1 << 2,
    /** A cell on the previewed path. */
    Path = 1 << 3,
    /** The cell under the pointer or the keyboard cursor. */
    Hover = 1 << 4,
    /** A cell holding a combatant the hovered action would strike. */
    Victim = 1 << 5,
};

struct DNDDICEROLL_API FMask
{
    int32 Width = 0;
    int32 Height = 0;
    TArray<uint8> Bits;

    bool IsValidCell(FIntPoint Cell) const { return Cell.X >= 0 && Cell.Y >= 0 && Cell.X < Width && Cell.Y < Height; }
    uint8 At(FIntPoint Cell) const { return IsValidCell(Cell) ? Bits[Cell.Y * Width + Cell.X] : 0; }
    bool Has(FIntPoint Cell, uint8 Bit) const { return (At(Cell) & Bit) != 0; }
    /** Every cell carrying the bit, in row-major order. */
    TArray<FIntPoint> Cells(uint8 Bit) const;
    bool operator==(const FMask& Other) const { return Width == Other.Width && Height == Other.Height && Bits == Other.Bits; }
};

/** Mask for a Width x Height board. Victims are cells (the caller resolves combatants to cells); cells off the board are ignored. */
DNDDICEROLL_API FMask BuildMask(int32 Width, int32 Height, const FBoardHighlights& Highlights, const TArray<FIntPoint>& VictimCells);

/** How a bar is drawn; the view maps each style to a colour, opacity and height. */
enum class EStyle : uint8
{
    MoveContour,
    ReachContour,
    Tick,
    Blocked,
    Ribbon,
    PathBracket,
    HoverBracket,
    Victim,
    Grid,
    Count
};

struct FBar
{
    FVector2D A = FVector2D::ZeroVector;
    FVector2D B = FVector2D::ZeroVector;
    /** Across the bar, in cells. */
    float Width = 0.0f;
    EStyle Style = EStyle::Tick;
};

/** Dimensions in cells, shared so the geometry and its tests agree. */
constexpr float ContourWidth = 0.024f;
constexpr float TickWidth = 0.008f;
constexpr float TickHalfLength = 0.1f;
constexpr float BlockedWidth = 0.012f;
constexpr float BlockedHalfDiagonal = 0.14f;
constexpr float RibbonWidth = 0.045f;
constexpr float CostTickWidth = 0.014f;
constexpr float CostTickHalfLength = 0.1f;
constexpr float BracketWidth = 0.02f;
constexpr float BracketInset = 0.42f;
constexpr float BracketArm = 0.18f;
constexpr float GridWidth = 0.006f;

/**
 * The bars for a mask: reach contours (Manhattan-exact edges of each region), corner ticks inside reach,
 * blocked-sight crosses, the path ribbon with a cost tick per step and a destination bracket, a hover bracket,
 * victim frames and, when asked, faint lines over the whole board. Path steps are drawn from the origin
 * (Highlights.Origin, when present) through Highlights.Path.
 */
DNDDICEROLL_API void BuildBars(const FMask& Mask, const FBoardHighlights& Highlights, bool bFullGrid, TArray<FBar>& Out);
}
