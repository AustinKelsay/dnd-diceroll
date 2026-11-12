#pragma once

#include <algorithm>
#include <limits>
#include <vector>

// Canvas design-unit geometry only. Kept independent of Unreal so the exact
// production placement arithmetic can also be tested without an editor.
namespace ExpeditionLayout
{
// The HUD draws a 1280x720 design space scaled to the viewport. Shared with the
// portable layout test so both use the same scale and chip content bounds.
constexpr double DesignWidth = 1280, DesignHeight = 720;
constexpr double ChipHeight = 24, ChipMaxWidth = 290, ChipTextPadding = 16;

struct Point { double X, Y; };
struct Rect { double Left, Top, Right, Bottom; };
struct Chip { Point Anchor; Rect Panel; };
struct Segment { Point From, To; };

inline double ViewScale(double ViewportWidth, double ViewportHeight)
{ return std::max(0.5, std::min(ViewportWidth / DesignWidth, ViewportHeight / DesignHeight)); }

// The part of the viewport the board may fill, in design units for a viewport already divided by the
// scale: right of the party cards (x 12..262 in DrawPartyCard), below the initiative strip (y 12..68 in
// DrawInitiative), above the action bar's budget chips (H - 108 in DrawActionBar) and clear of the right edge.
// The camera rig fits the board to this rectangle (CameraRig.h), so the HUD and the camera share one source.
constexpr double BoardLeft = 270, BoardTop = 76, BoardRight = 12, BoardBottom = 112;

// The combat log panel (DrawLog): a fixed-width panel in the bottom-left corner whose height follows its wrapped lines
// (at most LogMaxLines). DrawLog and the board bounds both use this, so the camera cannot frame the board under the log.
constexpr double LogLeft = 8, LogWidth = 280, LogBottomMargin = 8, LogPadding = 14, LogLineHeight = 16;
constexpr int LogMaxLines = 4;
inline double LogHeight(int Lines) { return LogPadding + Lines * LogLineHeight; }
inline Rect LogRect(double W, double H, int Lines = LogMaxLines)
{
    const double Height = LogHeight(Lines);
    return {LogLeft, H - LogBottomMargin - Height, LogLeft + LogWidth, H - LogBottomMargin};
}

// The party cards (DrawPartyCard): a column of 200 x 64 cards, one every 68 units, from the top of the left edge. The roll card
// and the board bounds both read these.
constexpr double PartyCardX = 8, PartyCardTop = 54, PartyCardWidth = 200, PartyCardHeight = 64, PartyCardPitch = 68;

// The route screen (DrawRoutes): the "Choose your path" banner and the two route cards, centred, the cards side by side. DrawRoutes and the tabletop overview's layout
// (OverviewLayout.h, #149) both read these, so the dioramas cannot be placed under the HUD they are meant to show beside.
constexpr double RouteCardWidth = 340, RouteCardHeight = 250, RouteCardGap = 30;
constexpr double RouteBannerWidth = 740, RouteBannerHeight = 76, RouteBannerAbove = 84;
inline double RouteTop(double H) { return H * 0.5 - RouteCardHeight * 0.5 - 10; }
inline Rect RouteBannerRect(double W, double H)
{
    const double Top = RouteTop(H) - RouteBannerAbove;
    return {W * 0.5 - RouteBannerWidth * 0.5, Top, W * 0.5 + RouteBannerWidth * 0.5, Top + RouteBannerHeight};
}
inline Rect RouteCardRect(double W, double H, int Index)
{
    const double X = W * 0.5 + (Index == 0 ? -RouteCardWidth - RouteCardGap * 0.5 : RouteCardGap * 0.5);
    return {X, RouteTop(H), X + RouteCardWidth, RouteTop(H) + RouteCardHeight};
}

// Where the d20 roll card sits (#190): the strip of the left margin below the party cards and above the combat log. The camera keeps
// the board inside BoardBounds, to the right of this strip (BoardLeft), and the top band belongs to the status strip and the Menu, so a
// card here never covers the active unit, its target or their cells, whichever way the board is turned. The strip is as wide as the margin
// leaves and as tall as it can be; the card's own height follows its text.
constexpr double RollCardLeft = 8, RollCardGap = 8;
// Inside the card (design units from its top): the title above RollCardRowTop, the dice row between RollCardRowTop and RollCardRowBottom
// (tumbling, landing and glowing dice are kept inside it), the verdict from RollCardVerdictTop and the detail lines from RollCardDetailTop.
constexpr double RollCardRowTop = 26, RollCardRowBottom = 104, RollCardVerdictTop = 106, RollCardDetailTop = 138;
inline Rect RollCardBounds(double W, double H, int PartyCount = 4)
{
    const Rect Log = LogRect(W, H);
    const double Top = PartyCardTop + std::max(0, PartyCount - 1) * PartyCardPitch + PartyCardHeight + RollCardGap;
    return {RollCardLeft, Top, BoardLeft - RollCardGap, Log.Top - RollCardGap};
}

// The board area stays clear of the party cards, strips and action bar, and also of the tallest the combat log can get:
// where the log's columns overlap the board's, the bottom edge rises to the log's top.
inline Rect BoardBounds(double W, double H)
{
    Rect Bounds = {BoardLeft, BoardTop, W - BoardRight, H - BoardBottom};
    const Rect Log = LogRect(W, H);
    if (Log.Right > Bounds.Left && Log.Left < Bounds.Right) { Bounds.Bottom = std::min(Bounds.Bottom, Log.Top); }
    return Bounds;
}

// Where chips may sit, in design units for a viewport already divided by the scale:
// below the top bar and legend, above the action bar and budget row.
inline Rect ChipBounds(double W, double H) { return {8, 150, W - 8, H - 160}; }

// Fixed text need not own a panel. Reserve the complete HUD frame around the
// board content for connectors, using the same content bounds as chip placement.
inline void ExcludeHUDFrame(const Rect& Viewport, const Rect& Content, std::vector<Rect>& Exclusions)
{
    Exclusions.push_back({Viewport.Left, Viewport.Top, Viewport.Right, Content.Top});
    Exclusions.push_back({Viewport.Left, Content.Bottom, Viewport.Right, Viewport.Bottom});
    Exclusions.push_back({Viewport.Left, Content.Top, Content.Left, Content.Bottom});
    Exclusions.push_back({Content.Right, Content.Top, Viewport.Right, Content.Bottom});
}

inline bool Overlaps(const Rect& A, const Rect& B, double Gap = 0)
{
    return A.Left < B.Right + Gap && A.Right + Gap > B.Left &&
        A.Top < B.Bottom + Gap && A.Bottom + Gap > B.Top;
}

// Keep the original target-to-panel line, but omit portions hidden by HUD or
// chip rectangles (including their decorative frame/shadow). Exclusions are
// supplied after ALL chips have been placed, regardless of their draw order.
inline std::vector<Segment> VisibleLeader(Point From, Point To, const std::vector<Rect>& Exclusions)
{
    constexpr double Padding = 6;
    const double DX = To.X - From.X, DY = To.Y - From.Y;
    if (DX == 0 && DY == 0) { return {}; }
    struct Interval { double Begin, End; };
    std::vector<Interval> Hidden;
    for (const auto& Box : Exclusions)
    {
        double Begin = 0, End = 1;
        auto IntersectAxis = [&](double Origin, double Delta, double Low, double High)
        {
            if (Delta == 0) { return Origin >= Low && Origin <= High; }
            double A = (Low - Origin) / Delta, B = (High - Origin) / Delta;
            if (A > B) { std::swap(A, B); }
            Begin = std::max(Begin, A); End = std::min(End, B);
            return Begin <= End;
        };
        if (IntersectAxis(From.X, DX, Box.Left - Padding, Box.Right + Padding) &&
            IntersectAxis(From.Y, DY, Box.Top - Padding, Box.Bottom + Padding))
        { Hidden.push_back({Begin, End}); }
    }
    std::sort(Hidden.begin(), Hidden.end(), [](const Interval& A, const Interval& B) { return A.Begin < B.Begin; });
    auto At = [&](double T) -> Point { return {From.X + DX * T, From.Y + DY * T}; };
    std::vector<Segment> Visible;
    double Cursor = 0;
    for (const auto& Span : Hidden)
    {
        if (Span.Begin > Cursor) { Visible.push_back({At(Cursor), At(Span.Begin)}); }
        Cursor = std::max(Cursor, Span.End);
    }
    if (Cursor < 1) { Visible.push_back({At(Cursor), To}); }
    return Visible;
}

// Try the desired location and each obstacle edge. Every candidate is checked
// against every obstacle; no later displacement can reintroduce an overlap.
// A false return means the available area cannot fit this chip at this size.
inline bool PlaceChip(Point Anchor, double Width, double Height, const Rect& Bounds,
    const std::vector<Rect>& Occupied, Chip& Result)
{
    constexpr double Gap = 12;
    if (Width <= 0 || Height <= 0 || Width > Bounds.Right - Bounds.Left || Height > Bounds.Bottom - Bounds.Top) { return false; }
    const double DesiredX = std::clamp(Anchor.X - Width / 2, Bounds.Left, Bounds.Right - Width);
    const double DesiredY = std::clamp(Anchor.Y - Height / 2, Bounds.Top, Bounds.Bottom - Height);
    std::vector<double> Xs = {DesiredX, Bounds.Left, Bounds.Right - Width};
    std::vector<double> Ys = {DesiredY, Bounds.Top, Bounds.Bottom - Height};
    for (const auto& Other : Occupied)
    {
        Xs.push_back(Other.Left - Gap - Width); Xs.push_back(Other.Right + Gap);
        Ys.push_back(Other.Top - Gap - Height); Ys.push_back(Other.Bottom + Gap);
    }
    auto Unique = [](std::vector<double>& Values)
    { std::sort(Values.begin(), Values.end()); Values.erase(std::unique(Values.begin(), Values.end()), Values.end()); };
    Unique(Xs); Unique(Ys);
    double BestDistance = std::numeric_limits<double>::max();
    bool Found = false;
    for (double X : Xs)
    for (double Y : Ys)
    {
        const Rect Candidate = {X, Y, X + Width, Y + Height};
        if (X < Bounds.Left || Y < Bounds.Top || Candidate.Right > Bounds.Right || Candidate.Bottom > Bounds.Bottom) { continue; }
        if (std::any_of(Occupied.begin(), Occupied.end(), [&](const Rect& Other) { return Overlaps(Candidate, Other, Gap); })) { continue; }
        const double DX = X - DesiredX, DY = Y - DesiredY;
        const double Distance = DX * DX + DY * DY;
        if (Distance < BestDistance)
        {
            BestDistance = Distance;
            Result = {Anchor, Candidate}; // The true projection never moves with the panel.
            Found = true;
        }
    }
    return Found;
}

// The path-cost number's plate (#171). It sits just above the end of the path; when a panel or chip is there it moves
// to the nearest free spot instead, so the number is never hidden. With no free spot it stays where it would be drawn.
// The plate is drawn last, over everything, so it also reads when a spot cannot be found.
inline Rect PathCostPlate(Point End, double Width, double Height, const Rect& Bounds, const std::vector<Rect>& Occupied)
{
    constexpr double Lift = 21;
    const Point Anchor = {End.X, End.Y - Lift};
    Chip Placed;
    if (PlaceChip(Anchor, Width, Height, Bounds, Occupied, Placed)) { return Placed.Panel; }
    return {Anchor.X - Width / 2, Anchor.Y - Height / 2, Anchor.X + Width / 2, Anchor.Y + Height / 2};
}
}
