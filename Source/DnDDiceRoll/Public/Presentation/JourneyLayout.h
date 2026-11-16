#pragma once

#include "CoreMinimal.h"
#include "Presentation/OverviewLayout.h"
#include "Presentation/TargetChipLayout.h"
#include "Run/JourneyQuery.h"

/**
 * The war-table journey map (#248, spec #239): what the route screen and the ending draw and where, as pure functions of FJourneyQuery, so the choice is testable
 * without a world and the map never reads or writes rules state (ADR-0001). Coordinates are table-top cm as in OverviewLayout: +X is screen-left (the camera looks
 * along +Y), the table top is Z = 0. The positions of the stage sockets, the trailhead and the goal are painted on SM_Journey_Map, so they are mirrored in
 * Art/dark-fantasy-sources/r001-journey/journey_layout.json (tools/tests/test_journey_layout.py keeps the two equal).
 *
 * The screen is laid out in three bands at every supported aspect (the vertical field of view is fixed, so a design unit lands on the same scene point at 16:9 and
 * wider): the five stages and the Vault across the far side of the map (design y about 80 to 150), the offered worlds' dioramas raised on plinths in the middle
 * (y about 150 to 435) with a label plaque under each, and the two route cards in the bottom band (y 484 to 646), so no card covers a diorama.
 */
namespace JourneyLayout
{
using OverviewLayout::EDiorama;

// ------------------------------------------------------------------------------------------------------------------------------------------
// The camera. The engine's default MaintainYFOV keeps the vertical field of view that FieldOfView (horizontal) implies at the camera's 16:9 aspect, at any viewport.

/** The eye 303 cm in front of the table centre and 260 cm above the top, aimed 52 degrees down at (0, -100, 0). */
inline FVector CameraEye() { return FVector(0.0, -303.17, 260.04); }
inline FVector CameraLook() { return FVector(0.0, 0.61566, -0.78801); }
constexpr double CameraFieldOfView = 50.0;
constexpr double CameraReferenceAspect = 16.0 / 9.0;
inline double HalfTanY() { return FMath::Tan(FMath::DegreesToRadians(CameraFieldOfView * 0.5)) / CameraReferenceAspect; }

/** A point in table-top cm to a Width x Height viewport (pixels, or design units: the result scales with both); false behind the camera. */
inline bool Project(const FVector& TableCm, double Width, double Height, FVector2D& OutPixel)
{
    const FVector Forward = CameraLook().GetSafeNormal();
    const FVector Right = FVector::CrossProduct(FVector::UpVector, Forward).GetSafeNormal();
    const FVector Up = FVector::CrossProduct(Forward, Right);
    const FVector V = TableCm - CameraEye();
    const double Depth = FVector::DotProduct(V, Forward);
    if (Depth <= 1.0) { return false; }
    const double TanY = HalfTanY(), TanX = TanY * Width / Height;
    OutPixel = FVector2D((FVector::DotProduct(V, Right) / (Depth * TanX) * 0.5 + 0.5) * Width, (0.5 - FVector::DotProduct(V, Up) / (Depth * TanY) * 0.5) * Height);
    return true;
}

// ------------------------------------------------------------------------------------------------------------------------------------------
// Where things stand on the map (table-top cm; mirrored in journey_layout.json, painted on the map).

/** The map sheet's top above the table, where the tokens, ink and fog stand. */
constexpr float MapTop = 0.6f;
/** The five stage sockets, left to right on screen (stage 1 is screen-left, so it has the largest X). */
inline FVector2D StageSpot(int32 Index)
{
    static const FVector2D Spots[] = {FVector2D(102.8, -16.8), FVector2D(56.0, -11.9), FVector2D(8.3, -17.6), FVector2D(-39.2, -11.9), FVector2D(-86.1, -16.8)};
    return Spots[FMath::Clamp(Index, 0, 4)];
}
/** Where the road starts (under the party cards on a 16:9 screen) and the Vault keep that ends it. */
inline FVector2D TrailheadSpot() { return FVector2D(152.0, -22.0); }
inline FVector2D GoalSpot() { return FVector2D(-130.0, -38.0); }

/** The plinths the raised dioramas stand on: two offers either side of the centre, a lone one (the ending's showcase) in the middle. */
constexpr float PlinthDepth = -100.0f;
inline FVector2D PlinthSpot(int32 Index, int32 Count) { return Count <= 1 ? FVector2D(0.0, PlinthDepth) : FVector2D(Index == 0 ? 40.0 : -40.0, PlinthDepth); }
/** The plinth mesh is 14 cm tall at scale 1; it is drawn at the diorama's scale, so a diorama stands on top of it at 14 x scale. */
constexpr float PlinthHeight = 14.0f;
/** Uniform scale of a raised diorama (and its plinth): the cathedral is the tallest, so it is the smallest. The ending's showcase stands alone in the middle at the same scale. */
inline float DioramaScale(EDiorama Kind)
{
    return Kind == EDiorama::Vigil ? 0.62f : Kind == EDiorama::Vault ? 0.72f : 0.70f;
}
/** Each raised diorama's yaw on top of the turn toward the camera: a slight turn in toward the centre. */
inline float DioramaYaw(int32 Index, int32 Count) { return Count <= 1 ? 0.0f : Index == 0 ? -6.0f : 6.0f; }

/** The stage token, emblem and fog scales (meshes authored at scale 1 for these spots). */
constexpr float FogLift = 0.4f;

// ------------------------------------------------------------------------------------------------------------------------------------------
// What each stage shows: pure facts from the query, never a rules decision.

enum class EStageMark : uint8 { Cleared, Lost, Current, Fogged, Unreached };

struct FStageLook
{
    int32 Index = 0;
    EStageMark Mark = EStageMark::Fogged;
    /** The world emblem on the token: the world fought in (Cleared, Lost); Unknown for no emblem. */
    EExpeditionWorld Emblem = EExpeditionWorld::Unknown;
    /** A victory pennant beside a cleared token, a broken sword in a lost one, a fog bank over a fogged one. */
    bool bPennant = false;
    bool bBroken = false;
    bool bFog = false;
    /** The ink from the previous stop (the trailhead for stage 1) is walked (solid) or not yet (dashed). */
    bool bWalkedIn = false;
};

/** One diorama raised on a plinth: an offer of the current fork (Offer 0 or 1) or the ending's showcase (Offer INDEX_NONE). */
struct FRaisedLook
{
    EDiorama Kind = EDiorama::Root;
    int32 Offer = INDEX_NONE;
    FVector2D Spot = FVector2D::ZeroVector;
    float Scale = 1.0f;
    float Yaw = 0.0f;
    /** The route's name, and its risk word (empty where the run never disclosed one). */
    FString Name;
    FString Risk;
    bool bDangerous = false;
};

struct FJourneyLook
{
    TArray<FStageLook> Stages;
    TArray<FRaisedLook> Raised;
    /** The run has ended: no fog, the whole road shown. */
    bool bEnded = false;
    bool bVictory = false;
    /** The goal is reached (a won run): the ink to the Vault is walked and the keep glows. */
    bool bGoalReached = false;
    /** The stage whose ink forks down to the raised offers; INDEX_NONE when none are offered. */
    int32 ForkStage = INDEX_NONE;
};

/** The diorama of a world: Root, Vigil and Vault; Unknown has none (false). */
inline bool DioramaOf(EExpeditionWorld World, EDiorama& Out)
{
    switch (World)
    {
    case EExpeditionWorld::Root: Out = EDiorama::Root; return true;
    case EExpeditionWorld::Vigil: Out = EDiorama::Vigil; return true;
    case EExpeditionWorld::Vault: Out = EDiorama::Vault; return true;
    default: return false;
    }
}

/** The risk word the route card shows, or empty for None. */
inline FString RiskText(ERouteRisk Risk) { return Risk == ERouteRisk::None ? FString() : FString(FExpeditionRun::RiskWord(Risk)); }

inline FJourneyLook Look(const FJourney& Journey)
{
    FJourneyLook Out;
    Out.bEnded = Journey.Phase == EExpeditionPhase::Victory || Journey.Phase == EExpeditionPhase::Defeat;
    Out.bVictory = Journey.Phase == EExpeditionPhase::Victory;
    int32 LostIndex = INDEX_NONE;
    for (const FJourneyStage& Stage : Journey.Stages)
    {
        FStageLook Mark;
        Mark.Index = Stage.Index;
        switch (Stage.State)
        {
        case EJourneyState::Cleared: Mark.Mark = EStageMark::Cleared; Mark.Emblem = Stage.World; Mark.bPennant = true; Mark.bWalkedIn = true; break;
        case EJourneyState::Lost: Mark.Mark = EStageMark::Lost; Mark.Emblem = Stage.World; Mark.bBroken = true; Mark.bWalkedIn = true; LostIndex = Stage.Index; break;
        case EJourneyState::Current: Mark.Mark = EStageMark::Current; Mark.Emblem = Stage.bTaken ? Stage.World : EExpeditionWorld::Unknown; Mark.bWalkedIn = true; break;
        default: Mark.Mark = Out.bEnded ? EStageMark::Unreached : EStageMark::Fogged; Mark.bFog = !Out.bEnded; break;
        }
        Out.Stages.Add(Mark);
        if (Stage.State == EJourneyState::Current && !Stage.Offers.IsEmpty())
        {
            Out.ForkStage = Stage.Index;
            const int32 Count = FMath::Min(Stage.Offers.Num(), 2);
            for (int32 Offer = 0; Offer < Count; ++Offer)
            {
                const FJourneyRoute& Route = Stage.Offers[Offer];
                FRaisedLook Raised;
                if (!DioramaOf(Route.World, Raised.Kind)) { continue; }
                Raised.Offer = Offer;
                Raised.Spot = PlinthSpot(Offer, Count);
                Raised.Scale = DioramaScale(Raised.Kind);
                Raised.Yaw = DioramaYaw(Offer, Count);
                Raised.Name = Route.Name;
                Raised.Risk = RiskText(Route.Risk);
                Raised.bDangerous = Route.bDangerous;
                Out.Raised.Add(Raised);
            }
        }
    }
    Out.bGoalReached = Out.bVictory;
    // The ending's showcase: the Vault the party reached, or the world it fell in.
    if (Out.bEnded)
    {
        const FJourneyStage* Shown = Out.bVictory ? (Journey.Stages.IsEmpty() ? nullptr : &Journey.Stages.Last()) : Journey.Stages.IsValidIndex(LostIndex) ? &Journey.Stages[LostIndex] : nullptr;
        FRaisedLook Raised;
        if (Shown && Shown->bTaken && DioramaOf(Shown->World, Raised.Kind))
        {
            Raised.Spot = PlinthSpot(0, 1);
            Raised.Scale = DioramaScale(Raised.Kind);
            Raised.Name = Shown->Taken.Name;
            Raised.bDangerous = Shown->Taken.bDangerous;
            Out.Raised.Add(Raised);
        }
    }
    return Out;
}

/** The short name of a world on the map's chips. */
inline const TCHAR* WorldWord(EExpeditionWorld World)
{
    switch (World)
    {
    case EExpeditionWorld::Root: return TEXT("Forest");
    case EExpeditionWorld::Vigil: return TEXT("Vigil");
    case EExpeditionWorld::Vault: return TEXT("Vault");
    default: return TEXT("Unknown");
    }
}

/** The chip under a stage token: its number, then what is known (text as well as the token's colour, game-ui-ux). */
inline FString StageChip(const FStageLook& Stage, EEncounterStage Kind)
{
    const int32 Number = Stage.Index + 1;
    switch (Stage.Mark)
    {
    case EStageMark::Cleared: return FString::Printf(TEXT("%d  %s  won"), Number, WorldWord(Stage.Emblem));
    case EStageMark::Lost: return FString::Printf(TEXT("%d  %s  lost"), Number, WorldWord(Stage.Emblem));
    case EStageMark::Current: return Stage.Emblem == EExpeditionWorld::Unknown ? FString::Printf(TEXT("%d  choose"), Number) : FString::Printf(TEXT("%d  %s"), Number, WorldWord(Stage.Emblem));
    case EStageMark::Unreached: return FString::Printf(TEXT("%d  unreached"), Number);
    default: return Kind == EEncounterStage::Finale ? FString::Printf(TEXT("%d  finale"), Number) : FString::Printf(TEXT("%d  unknown"), Number);
    }
}

inline FString GoalChip(const FJourneyLook& Look) { return Look.bGoalReached ? TEXT("The Vault  recovered") : TEXT("The Vault"); }

/** A raised diorama's plaque: the route name over the risk word (the card's GUARDED or DANGEROUS where no risk was disclosed); the ending's showcase says what happened there. */
inline FString PlaqueTitle(const FRaisedLook& Raised) { return Raised.Name; }
inline FString PlaqueDetail(const FRaisedLook& Raised, const FJourneyLook& Look)
{
    if (Raised.Offer == INDEX_NONE) { return Look.bVictory ? TEXT("The artifact is recovered") : TEXT("Where the party fell"); }
    if (!Raised.Risk.IsEmpty()) { return FString::Printf(TEXT("Risk: %s"), *Raised.Risk); }
    return Raised.bDangerous ? TEXT("Dangerous") : TEXT("Guarded");
}

// ------------------------------------------------------------------------------------------------------------------------------------------
// The map's points as the camera shows them and the HUD around them, in design units (W x H, H = 720 at every supported aspect).

/** The middle of a stage token's top, a raised diorama's plinth front edge, and the goal keep's foot, in table-top cm. */
inline FVector StagePoint(int32 Index) { const FVector2D S = StageSpot(Index); return FVector(S.X, S.Y, MapTop); }
inline FVector GoalPoint() { const FVector2D S = GoalSpot(); return FVector(S.X, S.Y, MapTop); }
inline FVector PlinthFront(const FRaisedLook& Raised) { return FVector(Raised.Spot.X, Raised.Spot.Y - 40.0 * Raised.Scale, MapTop + PlinthHeight * Raised.Scale * 0.5); }

/**
 * The design-unit box of a raised diorama standing on its plinth, from the mesh's local bounds (metres in the asset, cm here) turned toward the camera plus the raise's yaw,
 * scaled and stood on the plinth: what the HUD makes clickable and keeps its panels clear of. The plinth's own box (PlinthBounds, at scale 1) is included.
 */
inline bool RaisedScreenBox(const FBox& DioramaBounds, const FBox& PlinthBounds, const FRaisedLook& Raised, double W, double H, ExpeditionLayout::Rect& Out)
{
    const FRotator Turn(0.0f, -90.0f + Raised.Yaw, 0.0f);
    FVector2D Low(1e9, 1e9), High(-1e9, -1e9);
    auto Corners = [&](const FBox& Local, double Lift)
    {
        for (int32 Corner = 0; Corner < 8; ++Corner)
        {
            const FVector Point((Corner & 1) ? Local.Max.X : Local.Min.X, (Corner & 2) ? Local.Max.Y : Local.Min.Y, (Corner & 4) ? Local.Max.Z : Local.Min.Z);
            const FVector OnTable = Turn.RotateVector(Point * Raised.Scale) + FVector(Raised.Spot.X, Raised.Spot.Y, MapTop + Lift);
            FVector2D Pixel;
            if (!Project(OnTable, W, H, Pixel)) { return false; }
            Low = FVector2D(FMath::Min(Low.X, Pixel.X), FMath::Min(Low.Y, Pixel.Y));
            High = FVector2D(FMath::Max(High.X, Pixel.X), FMath::Max(High.Y, Pixel.Y));
        }
        return true;
    };
    if (!Corners(DioramaBounds, PlinthHeight * Raised.Scale) || !Corners(PlinthBounds, 0.0)) { return false; }
    Out = {Low.X, Low.Y, High.X, High.Y};
    return true;
}

/** The compact route banner at the top centre ("CHOOSE YOUR PATH" and the stage line), clear of the status strip and the Menu. */
constexpr double BannerWidth = 520, BannerTop = 6, BannerHeight = 42;
inline ExpeditionLayout::Rect BannerRect(double W) { return {W * 0.5 - BannerWidth * 0.5, BannerTop, W * 0.5 + BannerWidth * 0.5, BannerTop + BannerHeight}; }

/** The two route cards in the bottom band, either side of the centre (offer 0 left). */
constexpr double CardWidth = 380, CardGap = 20, CardTop = 484, CardBottom = 646;
inline ExpeditionLayout::Rect CardRect(double W, int32 Index)
{
    const double Left = Index == 0 ? W * 0.5 - CardGap * 0.5 - CardWidth : W * 0.5 + CardGap * 0.5;
    return {Left, CardTop, Left + CardWidth, CardBottom};
}

/** A stage's chip, centred under its token. */
constexpr double ChipWidth = 112, ChipHeight = 18, ChipBelow = 14;
inline ExpeditionLayout::Rect ChipRectAt(const FVector2D& Anchor) { return {Anchor.X - ChipWidth * 0.5, Anchor.Y + ChipBelow, Anchor.X + ChipWidth * 0.5, Anchor.Y + ChipBelow + ChipHeight}; }
inline ExpeditionLayout::Rect StageChipRect(int32 Index, double W, double H)
{
    FVector2D At;
    Project(StagePoint(Index), W, H, At);
    return ChipRectAt(At);
}
inline ExpeditionLayout::Rect GoalChipRect(double W, double H)
{
    FVector2D At;
    Project(GoalPoint(), W, H, At);
    return ChipRectAt(At);
}

/** A raised diorama's plaque: two lines centred on the plinth's front face (where the brass plate is), so it labels its diorama and stays above the cards. */
constexpr double PlaqueWidth = 260, PlaqueHeight = 34;
inline ExpeditionLayout::Rect PlaqueRect(const FRaisedLook& Raised, double W, double H)
{
    FVector2D At;
    Project(PlinthFront(Raised), W, H, At);
    return {At.X - PlaqueWidth * 0.5, At.Y - PlaqueHeight * 0.5, At.X + PlaqueWidth * 0.5, At.Y + PlaqueHeight * 0.5};
}

/** Where a selected or hovered hero's details stand on the journey's route screen: the left column under the party cards, clear of the dioramas and the cards. */
inline ExpeditionLayout::Rect DetailRect()
{
    // 5 units under the last party card and (at its tallest) 7 above the cards: the HUD's 4-unit clearance holds after pixel rounding at every scale.
    const double Top = ExpeditionLayout::PartyCardTop + 3 * ExpeditionLayout::PartyCardPitch + ExpeditionLayout::PartyCardHeight + 5.0;
    return {ExpeditionLayout::PartyCardX, Top, ExpeditionLayout::PartyCardX + ExpeditionLayout::PartyDetailWidth, Top + ExpeditionLayout::PartyDetailPad + ExpeditionLayout::PartyDetailMaxLines * ExpeditionLayout::PartyDetailLineHeight};
}

/** The ending's panel on the journey: compact, in the bottom band, so the revealed road and the showcase stay in view. */
constexpr double EndingWidth = 880, EndingTop = 484, EndingBottom = 646;
inline ExpeditionLayout::Rect EndingRect(double W) { return {W * 0.5 - EndingWidth * 0.5, EndingTop, W * 0.5 + EndingWidth * 0.5, EndingBottom}; }

/**
 * The HUD the journey's dioramas must stay clear of, in design units: the Menu, the status strip, the party cards and a hero's details (route screen only), the banner and both
 * cards or the ending's panel, every stage chip and the goal's, and the feedback line. Each diorama's own plaque sits on its plinth and is checked separately (PlaqueRect).
 */
inline TArray<ExpeditionLayout::Rect> HudRects(const FJourneyLook& Look, double W, double H)
{
    using ExpeditionLayout::Rect;
    TArray<Rect> Out;
    Out.Add({W - 130.0, 4.0, W - 4.0, 48.0});
    Out.Add({8.0, 4.0, 288.0, 50.0});
    if (!Look.bEnded)
    {
        Out.Add({ExpeditionLayout::PartyCardX, ExpeditionLayout::PartyCardTop, ExpeditionLayout::PartyCardX + ExpeditionLayout::PartyCardWidth,
            ExpeditionLayout::PartyCardTop + 3 * ExpeditionLayout::PartyCardPitch + ExpeditionLayout::PartyCardHeight});
        Out.Add(DetailRect());
        Out.Add(BannerRect(W));
        Out.Add(CardRect(W, 0));
        Out.Add(CardRect(W, 1));
    }
    else { Out.Add(EndingRect(W)); }
    for (int32 Index = 0; Index < Look.Stages.Num(); ++Index) { Out.Add(StageChipRect(Index, W, H)); }
    Out.Add(GoalChipRect(W, H));
    Out.Add({190.0, H - 70.0, W - 190.0, H - 8.0});
    return Out;
}

/** Where the three warm lights hang over the journey: candles at the back corners of the table, and a lamp over each plinth (added per raised diorama). */
inline TArray<FVector> Candles() { return {FVector(178.0, 22.0, 40.0), FVector(-182.0, 18.0, 24.0)}; }
inline FVector LampOver(const FRaisedLook& Raised) { return FVector(Raised.Spot.X, Raised.Spot.Y - 34.0, 92.0); }
/** A broad warm lamp hanging over the middle of the chart, so the road reads from end to end. */
inline FVector MapLamp() { return FVector(-10.0, -40.0, 170.0); }
}
