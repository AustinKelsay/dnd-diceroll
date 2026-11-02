// Tests the production geometry without Unreal. Does not establish rendered UI acceptance.
#include "Presentation/TargetChipLayout.h"
#include <cassert>
#include <cmath>
#include <iostream>
#include <random>

using namespace ExpeditionLayout;

static bool Near(double A, double B) { return std::abs(A-B) < 0.00001; }

static void CheckLeader(Point From, Point To, const std::vector<Rect>& Exclusions)
{
    for (const auto& Segment : VisibleLeader(From, To, Exclusions))
    {
        for (int I=0; I<=100; ++I)
        {
            const double T = I/100.0;
            const Point P{Segment.From.X + (Segment.To.X-Segment.From.X)*T,
                Segment.From.Y + (Segment.To.Y-Segment.From.Y)*T};
            for (const auto& Box : Exclusions)
            {
                assert(!(P.X > Box.Left-6+0.00001 && P.X < Box.Right+6-0.00001 &&
                    P.Y > Box.Top-6+0.00001 && P.Y < Box.Bottom+6-0.00001));
            }
            // Clipping must not reroute a leader to a different target.
            assert(Near((P.X-From.X)*(To.Y-From.Y), (P.Y-From.Y)*(To.X-From.X)));
        }
    }
}

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
    CheckLeader(Anchor, {std::clamp(Anchor.X, Result.Panel.Left, Result.Panel.Right),
        std::clamp(Anchor.Y, Result.Panel.Top, Result.Panel.Bottom)}, Occupied);
}

int main()
{
    // Reviewer reproduction: target behind a party card, chip outside it.
    const std::vector<Rect> Foreground = {{12,212,262,324}, {350,238,450,262}};
    auto Segments = VisibleLeader({100,250}, {350,250}, Foreground);
    assert(Segments.size() == 1);
    assert(Near(Segments[0].From.X,268) && Near(Segments[0].To.X,344));
    CheckLeader({100,250}, {350,250}, Foreground);
    CheckLeader({350,250}, {100,250}, Foreground);
    CheckLeader({100,100}, {500,500}, Foreground);
    assert(VisibleLeader({100,250},{200,250},Foreground).empty());
    assert(VisibleLeader({100,250},{100,250},Foreground).empty());
    // Crossing an earlier/later chip and overlapping exclusions must leave gaps.
    const std::vector<Rect> Crossings = {{20,-10,40,10}, {30,-10,60,10}};
    Segments = VisibleLeader({0,0},{100,0},Crossings);
    assert(Segments.size() == 2);
    assert(Near(Segments[0].From.X,0) && Near(Segments[0].To.X,14));
    assert(Near(Segments[1].From.X,66) && Near(Segments[1].To.X,100));
    CheckLeader({0,0},{100,0},Crossings);
    Segments = VisibleLeader({0,-20},{0,20},Crossings);
    assert(Segments.size() == 1 && Near(Segments[0].From.Y,-20) && Near(Segments[0].To.Y,20));

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
        // Fixed legend at Y=133, budget/turn row at H-108 and bottom help text
        // have no panels. Test the production frame reservation at every size.
        std::vector<Rect> Frame;
        ExcludeHUDFrame({0,0,W,H}, Area, Frame);
        assert(VisibleLeader({W/2,100},{W/2,150},Frame).empty());
        assert(VisibleLeader({W/2,H-50},{W/2,H-160},Frame).empty());
        Segments = VisibleLeader({W/2,100},{W/2,300},Frame);
        assert(Segments.size() == 1 && Near(Segments[0].From.Y,156));
        Segments = VisibleLeader({W/2,H-50},{W/2,H-250},Frame);
        assert(Segments.size() == 1 && Near(Segments[0].From.Y,H-166));
        CheckLeader({W/2,100},{W/2,300},Frame);
        CheckLeader({W/2,H-50},{W/2,H-250},Frame);
        CheckLeader({W-1,100},{W/2,H-250},Frame);
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
    std::cout << "PASS: collision regressions, edge anchors, HUD/chip/fixed-text leader clipping, four viewport sizes, 3200 randomized placements, no-space refusal\n";
}
