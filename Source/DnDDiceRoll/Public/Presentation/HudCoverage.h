#pragma once

#include <algorithm>
#include <vector>
#include "Presentation/TargetChipLayout.h"

// Screen-coverage arithmetic for the persistent HUD budget (#146). Pure C++ so it can be tested
// without a viewport; the HUD feeds it the rectangles it really registered while drawing.
namespace ExpeditionLayout
{
/** Area of the union of Boxes clipped to Clip, so overlapping panels and the buttons on them count once. */
inline double UnionArea(const std::vector<Rect>& Boxes, const Rect& Clip)
{
    std::vector<Rect> Clipped;
    std::vector<double> Xs;
    for (const Rect& Box : Boxes)
    {
        const Rect C = {std::max(Box.Left, Clip.Left), std::max(Box.Top, Clip.Top), std::min(Box.Right, Clip.Right), std::min(Box.Bottom, Clip.Bottom)};
        if (C.Right <= C.Left || C.Bottom <= C.Top) { continue; }
        Clipped.push_back(C);
        Xs.push_back(C.Left);
        Xs.push_back(C.Right);
    }
    std::sort(Xs.begin(), Xs.end());
    Xs.erase(std::unique(Xs.begin(), Xs.end()), Xs.end());
    double Area = 0;
    for (size_t Index = 0; Index + 1 < Xs.size(); ++Index)
    {
        std::vector<std::pair<double, double>> Spans;
        for (const Rect& C : Clipped) { if (C.Left <= Xs[Index] && C.Right >= Xs[Index + 1]) { Spans.push_back({C.Top, C.Bottom}); } }
        std::sort(Spans.begin(), Spans.end());
        double Covered = 0, End = -1e30;
        for (const auto& Span : Spans)
        {
            const double Begin = std::max(Span.first, End);
            if (Span.second > Begin) { Covered += Span.second - Begin; End = Span.second; }
        }
        Area += Covered * (Xs[Index + 1] - Xs[Index]);
    }
    return Area;
}
}
