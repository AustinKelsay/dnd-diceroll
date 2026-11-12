#pragma once

#include "CoreMinimal.h"
#include "Presentation/TargetChipLayout.h"

/**
 * What the tabletop overview draws and where (#149, the war table r002): pure functions of the route screen's facts, so the choice is testable
 * without a world and a presentation change never reads or writes rules state (ADR-0001). Coordinates are table-top cm: +X is screen-left (the
 * camera looks along +Y), the table top is Z = 0.
 */
namespace OverviewLayout
{
/** The three worlds a route can lead to; each has a diorama on the table. */
enum class EDiorama : uint8 { Root, Vigil, Vault };

/**
 * One diorama per offered road. Guarded roads lead to the forest (Root), dangerous roads to the drowned cathedral (Vigil) and the finale, whatever its
 * roads, to the Vault; with no Vault diorama the finale draws the Vigil one, as before the Vault existed. The title (no run) shows the two
 * first worlds side by side.
 */
inline TArray<EDiorama> Kinds(bool bHasRun, bool bFinale, TConstArrayView<bool> Dangerous, bool bVaultDiorama)
{
    TArray<EDiorama> Out;
    if (!bHasRun) { Out.Add(EDiorama::Root); Out.Add(EDiorama::Vigil); return Out; }
    for (const bool bDanger : Dangerous)
    {
        Out.Add(bFinale ? (bVaultDiorama ? EDiorama::Vault : EDiorama::Vigil) : bDanger ? EDiorama::Vigil : EDiorama::Root);
    }
    return Out;
}

struct FPlacement
{
    EDiorama Kind = EDiorama::Root;
    /** Offset from the table-top centre in cm (X, Y); Z is always on the table. */
    FVector2D Offset = FVector2D::ZeroVector;
    /** Yaw added to the turn toward the camera, in degrees. */
    float Yaw = 0.0f;
    /** Uniform scale of the mesh (1 on the title and for the r001 set). */
    float Scale = 1.0f;
};

/** Depth of the title's diorama row: 22 cm behind the centre for the r001 set; the r002 row stands 6 cm nearer, so the tall cathedral's bounds stay inside the frame (its top is the first thing the camera crops). */
inline float DioramaDepth(bool bR002) { return bR002 ? 16.0f : 22.0f; }

/**
 * The route screens' side spots (r002): the HUD covers the middle of the frame with the banner and the two cards, so on the route and finale screens each diorama
 * stands on the lower table beside its card, flanking the war map: offer 0 to the screen-left of the left card, offer 1 to the right of the right card, scaled to fit
 * the open margin (the Vigil, the tallest, a little smaller). A lone offer takes the left spot. The numbers come from projecting the meshes' bounds with the overview
 * camera (the OverviewDioramasStayClearOfTheHud test), clear of every HUD rect in HudRects at both resolutions.
 */
inline FVector2D SideSpot(int32 Index) { return FVector2D(Index == 0 ? 112.0 : -112.0, -166.0); }
inline float SideScale(EDiorama Kind) { return Kind == EDiorama::Vigil ? 0.62f : 0.68f; }

/**
 * Where each diorama stands. On the title (and always on the r001 set) the pair stands at the back of the table, offer 0 screen-left, 1 screen-right, a lone one in the middle;
 * on the r002 route and finale screens (bRoute) they take the side spots.
 */
inline TArray<FPlacement> Layout(TConstArrayView<EDiorama> Kinds, bool bR002, bool bRoute = false)
{
    TArray<FPlacement> Out;
    for (int32 Index = 0; Index < Kinds.Num(); ++Index)
    {
        FPlacement Spot;
        Spot.Kind = Kinds[Index];
        Spot.Yaw = Index == 0 ? -10.0f : 10.0f;
        if (bR002 && bRoute)
        {
            Spot.Offset = SideSpot(Index);
            Spot.Scale = SideScale(Kinds[Index]);
        }
        else
        {
            // The title's two drawers leave 576 design units between them: the r002 pair is shifted so the cathedral (the wider) clears the build details on the right.
            const float Left = bR002 ? 50.0f : 52.0f, Right = bR002 ? -34.0f : -52.0f;
            Spot.Offset = FVector2D(Kinds.Num() == 1 ? 0.0f : Index == 0 ? Left : Right, DioramaDepth(bR002));
        }
        Out.Add(Spot);
    }
    return Out;
}

/** Where the three runtime candle lights hang: on the flames of the table's candlestick, pillar candle and candle cluster (r002), or the r001 spots. */
inline TArray<FVector> Candles(bool bR002)
{
    if (bR002) { return {FVector(-100.0f, 30.0f, 43.0f), FVector(100.0f, 34.0f, 28.0f), FVector(-55.0f, -90.0f, 15.0f)}; }
    return {FVector(-100.0f, 30.0f, 40.0f), FVector(100.0f, 34.0f, 40.0f), FVector(0.0f, -70.0f, 70.0f)};
}

/** The runtime lights' strength (candela): the r001 spots 16 each; the r002 cluster is the low one beside the dice and the tankard, so it is a quarter of the other two. */
inline TArray<float> CandleStrengths(bool bR002)
{
    return bR002 ? TArray<float>({16.0f, 16.0f, 4.0f}) : TArray<float>({16.0f, 16.0f, 16.0f});
}

// ------------------------------------------------------------------------------------------------------------------------------------------
// The overview camera and the HUD it sits behind, for the layout tests and the capture probe (tools/overview_probe.py has the same constants).

/** The fixed overview camera in table-top cm (the view's own: focus (0, 30, 102 - 80), offset (0, -450, 186), aimed 96 cm lower), horizontal FOV 50. */
inline FVector CameraEye() { return FVector(0.0, -420.0, 208.0); }
inline FVector CameraLook() { return FVector(0.0, 450.0, -282.0); }
constexpr double CameraFieldOfView = 50.0;

/** A point in table-top cm to pixels of a width x height viewport; false behind the camera. */
inline bool Project(const FVector& TableCm, double Width, double Height, FVector2D& OutPixel)
{
    const FVector Forward = CameraLook().GetSafeNormal();
    const FVector Right = FVector::CrossProduct(FVector::UpVector, Forward).GetSafeNormal();
    const FVector Up = FVector::CrossProduct(Forward, Right);
    const FVector V = TableCm - CameraEye();
    const double Depth = FVector::DotProduct(V, Forward);
    if (Depth <= 1.0) { return false; }
    const double Tan = FMath::Tan(FMath::DegreesToRadians(CameraFieldOfView * 0.5));
    OutPixel = FVector2D((FVector::DotProduct(V, Right) / (Depth * Tan) * 0.5 + 0.5) * Width, (0.5 - FVector::DotProduct(V, Up) / (Depth * Tan / (Width / Height)) * 0.5) * Height);
    return true;
}

/** The HUD rectangles the overview sits behind, in design units (1280 x 720, the HUD's own space): conservative bounds of what the title and the route screens draw there. */
inline TArray<ExpeditionLayout::Rect> HudRects(bool bRoute, double W, double H)
{
    using ExpeditionLayout::Rect;
    TArray<Rect> Out;
    Out.Add({W - 120.0, 4.0, W - 4.0, 48.0});                                                                // the Menu button
    if (bRoute)
    {
        Out.Add({8.0, 4.0, 300.0, 50.0});                                                                    // the status strip
        Out.Add({ExpeditionLayout::PartyCardX, ExpeditionLayout::PartyCardTop, ExpeditionLayout::PartyCardX + ExpeditionLayout::PartyCardWidth,
            ExpeditionLayout::PartyCardTop + 3 * ExpeditionLayout::PartyCardPitch + ExpeditionLayout::PartyCardHeight});   // the party cards
        Out.Add({216.0, 50.0, 545.0, 126.0});                                                                // a hero's details beside the cards
        Out.Add(ExpeditionLayout::RouteBannerRect(W, H));
        Out.Add(ExpeditionLayout::RouteCardRect(W, H, 0));
        Out.Add(ExpeditionLayout::RouteCardRect(W, H, 1));
        Out.Add({190.0, H - 70.0, W - 190.0, H - 8.0});                                                      // the feedback line
    }
    else
    {
        Out.Add({12.0, 12.0, 312.0, H - 12.0});                                                              // the left drawer: party, class, subclass
        Out.Add({W - 392.0, 12.0, W - 12.0, 330.0});                                                         // the build's details
        Out.Add({W - 392.0, 540.0, W - 12.0, H - 12.0});                                                     // the actions
        Out.Add({328.0, H - 70.0, W - 396.0, H - 8.0});                                                      // the feedback line
    }
    return Out;
}
}
