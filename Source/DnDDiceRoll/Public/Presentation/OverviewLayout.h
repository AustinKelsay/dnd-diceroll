#pragma once

#include "CoreMinimal.h"

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
};

/** Depth of the diorama row: the r002 table's back edge is 60 cm behind the centre, so its dioramas stand 8 cm further back than the r001's. */
inline float DioramaDepth(bool bR002) { return bR002 ? 30.0f : 22.0f; }

/** Offer 0 stands screen-left under its card, offer 1 screen-right; a single road stands in the middle. */
inline TArray<FPlacement> Layout(TConstArrayView<EDiorama> Kinds, bool bR002)
{
    TArray<FPlacement> Out;
    for (int32 Index = 0; Index < Kinds.Num(); ++Index)
    {
        FPlacement Spot;
        Spot.Kind = Kinds[Index];
        Spot.Offset = FVector2D(Kinds.Num() == 1 ? 0.0f : Index == 0 ? 52.0f : -52.0f, DioramaDepth(bR002));
        Spot.Yaw = Index == 0 ? -10.0f : 10.0f;
        Out.Add(Spot);
    }
    return Out;
}

/** Where the three runtime candle lights hang: on the flames of the table's candlestick, pillar candle and candle cluster (r002), or the r001 spots. */
inline TArray<FVector> Candles(bool bR002)
{
    if (bR002) { return {FVector(-100.0f, 30.0f, 43.0f), FVector(100.0f, 34.0f, 28.0f), FVector(-95.0f, -155.0f, 15.0f)}; }
    return {FVector(-100.0f, 30.0f, 40.0f), FVector(100.0f, 34.0f, 40.0f), FVector(0.0f, -70.0f, 70.0f)};
}

/** The runtime lights' strength (candela): the r001 spots 16 each; the r002 cluster sits 15 cm over the table, close to the dice and the tankard, so it is a little under half of the other two. */
inline TArray<float> CandleStrengths(bool bR002)
{
    return bR002 ? TArray<float>({16.0f, 16.0f, 7.0f}) : TArray<float>({16.0f, 16.0f, 16.0f});
}
}
