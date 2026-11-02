// Tests the production geometry without Unreal. Does not establish rendered UI acceptance.
#include "Presentation/TargetChipLayout.h"
#include <cassert>
#include <iostream>
#include <random>

using namespace ExpeditionLayout;

static void CheckPlacement(Point Anchor, double Width, const Rect& Bounds, std::vector<Rect>& Occupied)
{
    Chip Result;
    assert(PlaceChip(Anchor, Width, 24, Bounds, Occupied, Result));
    assert(Result.Anchor.X == Anchor.X && Result.Anchor.Y == Anchor.Y);
    assert(Result.Panel.Left >= Bounds.Left && Result.Panel.Right <= Bounds.Right);
    assert(Result.Panel.Top >= Bounds.Top && Result.Panel.Bottom <= Bounds.Bottom);
    for (const auto& Other : Occupied) { assert(!Overlaps(Result.Panel, Other, 12)); }
    Chip Again;
    assert(PlaceChip(Anchor, Width, 24, Bounds, Occupied, Again));
    assert(Result.Panel.Left == Again.Panel.Left && Result.Panel.Top == Again.Panel.Top);
    Occupied.push_back(Result.Panel);
}

int main()
{
    const Rect Bounds = {8, 150, 1272, 560};
    std::vector<Rect> Occupied;
    for (double Y : {200, 245, 245}) { CheckPlacement({640, Y}, 290, Bounds, Occupied); }
    Occupied.clear();
    for (int I = 0; I < 4; ++I) { CheckPlacement({640, 150}, 290, Bounds, Occupied); }
    Occupied.clear();
    for (Point Anchor : {Point{-100, -200}, Point{1400, 800}, Point{20, 400}})
    { CheckPlacement(Anchor, 290, Bounds, Occupied); }

    // Match the HUD's scaling, including large, narrow and short/wide windows.
    for (Point Viewport : {Point{1280, 720}, Point{1920, 1080}, Point{1024, 576}, Point{1600, 600}})
    {
        const double Scale = std::max(0.5, std::min(Viewport.X / 1280, Viewport.Y / 720));
        const double W = Viewport.X / Scale, H = Viewport.Y / Scale;
        const Rect Area = {8, 150, W - 8, H - 160};
        // Party, log, lingering dice, turn banner and a large hover forecast.
        const std::vector<Rect> Panels = {{12, 96, 262, 556}, {274, H-230, 710, H-128},
            {W-452, 54, W-12, 182}, {0, H*0.24, W, H*0.24+82}, {W-408, 350, W-8, 490}};
        Occupied = Panels;
        for (int I = 0; I < 8; ++I) { CheckPlacement({W/2, 150}, 290, Area, Occupied); }
        std::mt19937 Random(50);
        for (int Trial = 0; Trial < 100; ++Trial)
        {
            Occupied = Panels;
            for (int I = 0; I < 8; ++I)
            {
                const Point Anchor{static_cast<double>(Random()%2400)-200, static_cast<double>(Random()%1000)-100};
                CheckPlacement(Anchor, 120 + Random()%171, Area, Occupied);
            }
        }
    }
    Chip Result;
    assert(!PlaceChip({0, 0}, 290, 24, {0, 0, 200, 20}, {}, Result));
    assert(!PlaceChip({640, 300}, 290, 24, Bounds, {Bounds}, Result));
    std::cout << "PASS: collision regressions, edge anchors, HUD exclusions, four viewport sizes, 3200 randomized placements, no-space refusal\n";
}
