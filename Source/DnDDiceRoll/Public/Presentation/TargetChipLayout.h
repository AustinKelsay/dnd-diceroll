#pragma once

#include <algorithm>
#include <limits>
#include <vector>

// Canvas design-unit geometry only. Kept independent of Unreal so the exact
// production placement arithmetic can also be tested without an editor.
namespace ExpeditionLayout
{
struct Point { double X, Y; };
struct Rect { double Left, Top, Right, Bottom; };
struct Chip { Point Anchor; Rect Panel; };

inline bool Overlaps(const Rect& A, const Rect& B, double Gap = 0)
{
    return A.Left < B.Right + Gap && A.Right + Gap > B.Left &&
        A.Top < B.Bottom + Gap && A.Bottom + Gap > B.Top;
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
}
