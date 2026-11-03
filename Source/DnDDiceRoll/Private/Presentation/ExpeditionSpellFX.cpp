#include "Presentation/ExpeditionSpellFX.h"
#include "Rules/HexGrid.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

namespace ExpeditionSpellFXLocal
{
constexpr float CellCm = 110.0f;
constexpr float BoltHeight = 70.0f;
const TCHAR* const EvocationPath = TEXT("/Game/DnDDiceRoll/Theme/SpellFX/MI_SpellFX_Evocation.MI_SpellFX_Evocation");
const TCHAR* const IcoPath = TEXT("/Game/DnDDiceRoll/Theme/SpellFX/SM_SpellFX_Ico.SM_SpellFX_Ico");
const TCHAR* const ShardPath = TEXT("/Game/DnDDiceRoll/Theme/SpellFX/SM_SpellFX_Shard.SM_SpellFX_Shard");
const TCHAR* const FramePath = TEXT("/Game/DnDDiceRoll/Theme/SpellFX/SM_SpellFX_CellFrame.SM_SpellFX_CellFrame");
const TCHAR* const BurstPath = TEXT("/Game/DnDDiceRoll/Theme/SpellFX/SM_SpellFX_Burst.SM_SpellFX_Burst");
// Late-PS1 feel: the effect animates on 12 fps steps and snaps to a 4 cm lattice (the board keeps its full frame rate).
constexpr float StepFps = 12.0f;
constexpr float SnapCm = 4.0f;
// Fire: ember orange with a yellow core (kept off party gold, enemy red-orange, Danger and the cyan path).
// Matte, muddy torch-lit fire (Gauntlet Dark Legacy / late PS1): no white-hot, no glow; values stay under 1.
const FLinearColor FireCore(0.86f, 0.5f, 0.1f);    // dull amber core
const FLinearColor FireHue(0.68f, 0.22f, 0.04f);   // muddy orange body
const FLinearColor EmberDeep(0.3f, 0.07f, 0.02f);  // char-red shadow band
const FLinearColor WhiteHot(0.92f, 0.66f, 0.24f);  // Fireball centre: brightest value on screen, still matte
// The Ico mesh is 85 cm across.
constexpr float IcoCm = 85.0f;
const FLinearColor InkColor(0.035f, 0.02f, 0.03f);

UStaticMesh* LoadMesh(const TCHAR* Path) { return LoadObject<UStaticMesh>(nullptr, Path, nullptr, LOAD_NoWarn | LOAD_Quiet); }
float EaseOut(float A) { A = FMath::Clamp(A, 0.0f, 1.0f); return 1.0f - (1.0f - A) * (1.0f - A); }
}
using namespace ExpeditionSpellFXLocal;

bool FExpeditionSpellFXStyle::For(const FSpellCastEvent& Event, FExpeditionSpellFXStyle& Out)
{
    if (Event.Kind != ESpellEventKind::Cast) { return false; }
    Out = FExpeditionSpellFXStyle();
    Out.Core = FireCore; Out.Hue = FireHue; Out.Ink = InkColor;
    if (Event.SpellId == TEXT("fire-bolt"))
    {
        // SRD 5.2.1 Fire Bolt: "You hurl a mote of fire at a creature or an object within range" (120 ft ranged spell attack, 1d10 fire).
        // One small, fast streak straight to the one target, then a small flare on that target only (no area).
        Out.Glow = 1.0f; Out.Travel = 0.16f; Out.BoltScale = 1.3f;
        Out.ImpactGrow = 0.08f; Out.ImpactHold = 0.08f; Out.ImpactLinger = 0.17f; Out.LingerOpacity = 0.45f; Out.ImpactScale = 0.55f; Out.Edge = 0.0f;
        return true;
    }
    if (Event.SpellId == TEXT("fireball"))
    {
        // SRD 5.2.1 Fireball: "A bright streak flashes from you to a point you choose within range (150 ft) and then blossoms
        // into a fiery explosion" filling a 20-foot-radius sphere (8d6 fire, Dex save half). Exact cells first, a small bead
        // streaks to the point, then a rounded faceted dome exactly 20 ft in radius (4 cells) that chars down.
        Out.Glow = 1.0f; Out.Telegraph = 0.12f; Out.bArea = true; Out.Travel = 0.22f; Out.BoltScale = 0.45f;
        Out.ImpactGrow = 0.17f; Out.ImpactHold = 0.17f; Out.ImpactLinger = 0.67f; Out.LingerOpacity = 0.4f; Out.AreaVisualScale = 1.0f; Out.Edge = 0.35f;
        return true;
    }
    return false;
}

int32 ExpeditionSpellFXTrial::PickTarget(const FCombatSnapshot& Battle, int32 ActiveId, const FString& SpellId)
{
    const auto& All = Battle.Encounter.Combatants;
    const FCombatantState* Active = All.FindByPredicate([ActiveId](const FCombatantState& U) { return U.Id == ActiveId; });
    if (!Active) { return INDEX_NONE; }
    int32 Target = INDEX_NONE; float Best = -1e9f;
    for (const FCombatantState& Foe : All)
    {
        if (Foe.Side != ECombatSide::Enemy || Foe.Life != ECombatLife::Conscious) { continue; }
        float Score = -FVector2D::Distance(FVector2D(Foe.Cell), FVector2D(Active->Cell)) * 0.01f;
        if (SpellId != TEXT("fire-bolt"))
        { for (const FCombatantState& Other : All) { Score += Other.Side == ECombatSide::Enemy && Other.Life == ECombatLife::Conscious && FVector2D::Distance(FVector2D(Other.Cell), FVector2D(Foe.Cell)) <= 4.0f ? 1.0f : 0.0f; } }
        if (Score > Best) { Best = Score; Target = Foe.Id; }
    }
    return Target;
}

AExpeditionSpellFX::AExpeditionSpellFX()
{
    PrimaryActorTick.bCanEverTick = true;
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

AExpeditionSpellFX* AExpeditionSpellFX::Play(UWorld* World, const FSpellCastEvent& Event, TFunctionRef<FVector(FIntPoint)> CellToWorld)
{
    FExpeditionSpellFXStyle Style;
    if (!World || !FExpeditionSpellFXStyle::For(Event, Style)) { return nullptr; }
    UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, EvocationPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
    if (!Material || !LoadMesh(IcoPath) || !LoadMesh(ShardPath) || !LoadMesh(FramePath) || !LoadMesh(BurstPath)) { return nullptr; }
    AExpeditionSpellFX* Fx = World->SpawnActor<AExpeditionSpellFX>(CellToWorld(Event.CasterCell), FRotator::ZeroRotator);
    if (!Fx) { return nullptr; }
    Fx->Material = Material;
    Fx->Build(Event, Style, CellToWorld);
    UE_LOG(LogTemp, Verbose, TEXT("SPELLFX play %s pieces %d total %.2fs ico extent %s at %s"), *Event.SpellId, Fx->Pieces.Num(), Fx->Total, *LoadMesh(IcoPath)->GetBounds().BoxExtent.ToString(), *Fx->GetActorLocation().ToString());
    return Fx;
}

FExpeditionSpellFXPiece& AExpeditionSpellFX::Add(UStaticMesh* Mesh, const FExpeditionSpellFXStyle& Style, bool bInk, FVector Lead, bool bRadial, bool bInkFill)
{
    FExpeditionSpellFXPiece& Piece = Pieces.AddDefaulted_GetRef();
    auto Make = [&](int32 Sort) -> TPair<UStaticMeshComponent*, UMaterialInstanceDynamic*>
    {
        UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this);
        C->SetStaticMesh(Mesh);
        C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        C->SetCastShadow(false);
        C->SetTranslucentSortPriority(Sort);
        C->SetupAttachment(RootComponent);
        C->SetUsingAbsoluteLocation(true); C->SetUsingAbsoluteRotation(true); C->SetUsingAbsoluteScale(true);
        C->RegisterComponent();
        UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(Material, this);
        M->SetVectorParameterValue(TEXT("Core"), Style.Core);
        M->SetVectorParameterValue(TEXT("Hue"), Style.Hue);
        M->SetVectorParameterValue(TEXT("Ink"), Style.Ink);
        M->SetVectorParameterValue(TEXT("Lead"), FLinearColor(Lead.X, Lead.Y, Lead.Z));
        M->SetScalarParameterValue(TEXT("Glow"), Style.Glow);
        M->SetScalarParameterValue(TEXT("Edge"), Style.Edge);
        M->SetScalarParameterValue(TEXT("Radial"), bRadial ? 1.0f : 0.0f);
        M->SetScalarParameterValue(TEXT("Opacity"), 0.0f);
        C->SetMaterial(0, M);
        C->SetVisibility(false);
        return {C, M};
    };
    if (bInk || bInkFill)
    {
        auto InkPair = Make(10);
        Piece.Ink = InkPair.Key; Piece.InkMaterial = InkPair.Value;
        if (bInkFill)
        {
            // Flat pieces (cell frames) get a dark under-print instead of a back-face shell.
            InkPair.Value->SetVectorParameterValue(TEXT("Core"), Style.Ink);
            InkPair.Value->SetVectorParameterValue(TEXT("Hue"), Style.Ink);
            InkPair.Value->SetScalarParameterValue(TEXT("Glow"), 1.0f);
            InkPair.Value->SetScalarParameterValue(TEXT("Edge"), 0.0f);
            Piece.InkScale = 1.12f; Piece.bFlatInk = true;
        }
        else { InkPair.Value->SetScalarParameterValue(TEXT("InkShell"), 1.0f); }
    }
    auto Main = Make(11);
    Piece.Mesh = Main.Key; Piece.Material = Main.Value;
    return Piece;
}

void AExpeditionSpellFX::Build(const FSpellCastEvent& Event, const FExpeditionSpellFXStyle& Style, TFunctionRef<FVector(FIntPoint)> CellToWorld)
{
    UStaticMesh* Ico = LoadMesh(IcoPath);
    UStaticMesh* Shard = LoadMesh(ShardPath);
    UStaticMesh* Frame = LoadMesh(FramePath);
    const FVector Caster = CellToWorld(Event.CasterCell) + FVector(0, 0, BoltHeight);
    FVector Impact = Caster + FVector(CellCm, 0, 0);
    float Radius = 0.5f;   // in cells, area spells only
    if (Style.bArea && Event.Cells.Num() > 0)
    {
        FVector2D Sum = FVector2D::ZeroVector;
        for (const FIntPoint& C : Event.Cells) { Sum += FVector2D(C.X, C.Y); }
        const FVector2D Centre = Sum / Event.Cells.Num();
        for (const FIntPoint& C : Event.Cells) { Radius = FMath::Max(Radius, FVector2D::Distance(Centre, FVector2D(C.X, C.Y))); }
        // A sphere's footprint is its stated radius (Fireball: 20 ft = 4 squares), not the cell hull.
        if (Event.AreaFeet > 0) { Radius = Event.AreaFeet / 5.0f; }
        Impact = CellToWorld(FIntPoint(FMath::RoundToInt(Centre.X), FMath::RoundToInt(Centre.Y))) + FVector(0, 0, 10);
    }
    else if (Event.TargetCells.Num() > 0) { Impact = CellToWorld(Event.TargetCells[0]) + FVector(0, 0, BoltHeight); }

    const float Launch = Style.Telegraph;
    const float Hit = Launch + Style.Travel;
    const float Full = Hit + Style.ImpactGrow;
    const float Fade = Full + Style.ImpactHold;
    const float Gone = Fade + Style.ImpactLinger;
    const FVector Dir = (Impact - Caster).GetSafeNormal();

    if (Style.bArea)
    {
        // Hex area: every cell is a matte hex tile (three overlapping flat bars whose corners are the hex
        // vertices), inset so the dark board shows between cells, with ink bars on every hex edge on top.
        // Inner cells burn brighter than the rim; nothing glows.
        UStaticMesh* Bar = LoadMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
        const TSet<FIntPoint> Inside(Event.Cells);
        FIntPoint Hub = Event.Cells[0];
        {
            const FVector2D ImpactXY(Impact.X, Impact.Y);
            float Near = TNumericLimits<float>::Max();
            for (const FIntPoint& C : Event.Cells) { const float D = FVector2D::Distance(FVector2D(CellToWorld(C)), ImpactXY); if (D < Near) { Near = D; Hub = C; } }
        }
        int32 Reach = 1;
        for (const FIntPoint& C : Event.Cells) { Reach = FMath::Max(Reach, FHexGrid::BoardDistance(Hub, C)); }
        for (const FIntPoint& C : Event.Cells)
        {
            if (!Bar) { break; }
            const FVector Center = CellToWorld(C);
            const FVector Across0 = CellToWorld(FHexGrid::BoardStep(C, 0)) - Center;
            const float Apothem2 = Across0.Size2D();                 // flat-to-flat width
            const float EdgeLen = Apothem2 / FMath::Sqrt(3.0f);      // hex edge = circumradius
            const float T = FMath::Clamp(float(FHexGrid::BoardDistance(Hub, C)) / Reach, 0.0f, 1.0f);
            const FLinearColor TileCore = FMath::Lerp(FireCore, FireHue, T);
            const FLinearColor TileHue = FMath::Lerp(FireHue, EmberDeep, T);
            for (int32 K = 0; K < 3; ++K)
            {
                FExpeditionSpellFXPiece& P = Add(Bar, Style, false, FVector::UpVector, false, false);
                P.Start = Hit; P.Peak = Full; P.End = Fade; P.Linger = Gone;
                P.PeakOpacity = 0.8f; P.LingerOpacity = 0.3f;
                P.From = P.To = Center + FVector(0, 0, 4);
                P.ScaleFrom = FVector(Apothem2 * 0.86f / 100.0f, EdgeLen * 0.86f / 100.0f, 0.01f) * 0.6f;
                P.ScaleTo = FVector(Apothem2 * 0.86f / 100.0f, EdgeLen * 0.86f / 100.0f, 0.01f);
                const FRotator Turn = Across0.Rotation() + FRotator(0, 60.0f * K, 0);
                P.Mesh->SetWorldRotation(Turn);
                P.Material->SetVectorParameterValue(TEXT("Core"), TileCore); P.Material->SetVectorParameterValue(TEXT("Hue"), TileHue);
                P.Material->SetScalarParameterValue(TEXT("Glow"), 0.45f);
                P.Material->SetScalarParameterValue(TEXT("Edge"), 0.0f);
            }
            for (int32 Side = 0; Side < 6; ++Side)
            {
                const FIntPoint N = FHexGrid::BoardStep(C, Side);
                const bool bRim = !Inside.Contains(N);
                // Inner edges are shared: draw each once, from the lower cell.
                if (!bRim && (N.Y < C.Y || (N.Y == C.Y && N.X < C.X))) { continue; }
                const FVector Across = CellToWorld(N) - Center;
                const FVector Mid = Center + Across * 0.5f + FVector(0, 0, bRim ? 9 : 8);
                FExpeditionSpellFXPiece& P = Add(Bar, Style, false, FVector::UpVector, false, true);
                P.Start = 0.0f; P.Peak = 0.06f; P.End = Fade; P.Linger = Gone;
                P.PeakOpacity = bRim ? 1.0f : 0.85f; P.LingerOpacity = bRim ? 0.45f : 0.3f;
                P.From = P.To = Mid;
                P.ScaleFrom = P.ScaleTo = FVector(bRim ? 0.11f : 0.06f, EdgeLen * 1.04f / 100.0f, 0.01f);
                P.Mesh->SetWorldRotation(Across.Rotation()); if (P.Ink) { P.Ink->SetWorldRotation(Across.Rotation()); }
                P.Material->SetScalarParameterValue(TEXT("Glow"), 0.3f);
                P.Material->SetScalarParameterValue(TEXT("Edge"), 0.0f);
            }
        }
    }
    {
        FExpeditionSpellFXStyle BoltStyle = Style; BoltStyle.Edge = 0.6f;   // loose watercolour break on the leading edge only
        FExpeditionSpellFXPiece& P = Add(Shard, BoltStyle, true, Dir, false);
        P.Start = Launch; P.Peak = Hit; P.End = Hit; P.Linger = Hit;
        P.From = Caster + Dir * 40.0f; P.To = Impact - Dir * (Style.bArea ? 0.0f : 20.0f);
        // A long thin streak (Fire Bolt ~90 cm, Fireball bead ~45 cm), not a fat shard.
        P.ScaleFrom = P.ScaleTo = FVector(Style.BoltScale, Style.BoltScale * 0.4f, Style.BoltScale * 0.4f);
        P.Mesh->SetWorldRotation(Dir.Rotation()); P.Ink->SetWorldRotation(Dir.Rotation());
        // Top-down camera sees the face-on band, so the bolt body is the element hue, not the pale core.
        P.Material->SetVectorParameterValue(TEXT("Core"), Style.Hue); P.Material->SetVectorParameterValue(TEXT("Hue"), EmberDeep);
    }
    // Area blasts draw well inside the true area (the telegraphed cells stay exact) so the board stays readable.
    // Area spells: the hex tiles are the blast; the faceted ball is only a small burst at the impact hex.
    const float Diameter = Style.bArea ? 1.4f * CellCm / 100.0f : Style.ImpactScale;
    {
        // The blast (Fireball) or the burst (Fire Bolt): flat bands, ink shell, ragged outer edge.
        // No back-face ink shell on the big translucent blast: its noisy edge showed through as specks.
        // Rounded faceted ball (20-face Ico), not the star-shaped Burst: Fire Bolt is a small flare, Fireball a dome.
        FExpeditionSpellFXPiece& P = Add(Ico, Style, true, FVector::UpVector, true);
        P.Mesh->SetWorldRotation(FRotator(0, Style.bArea ? 11.0f : 37.0f, 0)); P.Ink->SetWorldRotation(P.Mesh->GetComponentRotation());
        P.Start = Hit; P.Peak = Full; P.End = Fade; P.Linger = Gone; P.PeakOpacity = Style.bArea ? 0.85f : 1.0f;
        // Both blasts read as ember orange (the camera looks straight down, so the face-on band is what shows);
        // the yellow (Fire Bolt) or white-hot (Fireball) centre comes from the core piece below.
        // Deep ember body, orange only on the faceted rim bands: the glow stays in the fire, the board stays dark.
        P.Material->SetVectorParameterValue(TEXT("Core"), Style.Hue); P.Material->SetVectorParameterValue(TEXT("Hue"), EmberDeep);
        P.LingerOpacity = Style.LingerOpacity;
        P.From = P.To = Impact;
        const float ToScale = 100.0f / IcoCm;
        const FVector Size = FVector(Diameter, Diameter, Style.bArea ? Diameter * 0.35f : Diameter) * ToScale;
        P.ScaleFrom = Size * 0.25f; P.ScaleTo = Size;
        P.InkScale = Style.bArea ? 1.04f : 1.14f;
    }
    if (!Style.bArea)
    {
        // Fire Bolt: small yellow core inside the orange burst.
        FExpeditionSpellFXPiece& P = Add(Ico, Style, false, FVector::UpVector, true);
        P.Start = Hit; P.Peak = Full; P.End = Fade; P.Linger = Fade + 0.03f; P.PeakOpacity = 1.0f;
        P.From = P.To = Impact + FVector(0, 0, 8);
        P.ScaleFrom = FVector(Diameter * 0.1f * 100.0f / IcoCm); P.ScaleTo = FVector(Diameter * 0.4f * 100.0f / IcoCm);
        P.Material->SetVectorParameterValue(TEXT("Core"), FireCore);
        P.Material->SetVectorParameterValue(TEXT("Hue"), Style.Hue);
        P.Material->SetScalarParameterValue(TEXT("Edge"), 0.0f);
    }
    if (Style.bArea)
    {
        // White-hot evocation core; gone before the blast settles.
        FExpeditionSpellFXPiece& P = Add(Ico, Style, false, FVector::UpVector, true);
        P.Start = Hit; P.Peak = Hit + Style.ImpactGrow * 0.6f; P.End = Fade; P.Linger = Fade + 0.1f; P.PeakOpacity = 1.0f;
        P.From = P.To = Impact + FVector(0, 0, 20);
        P.PeakOpacity = 0.85f;
        P.ScaleFrom = FVector(0.3f); P.ScaleTo = FVector(Diameter * 0.3f, Diameter * 0.3f, Diameter * 0.12f) * (100.0f / IcoCm);
        P.Material->SetVectorParameterValue(TEXT("Core"), WhiteHot);
        P.Material->SetVectorParameterValue(TEXT("Hue"), FireCore);
        P.Material->SetScalarParameterValue(TEXT("Edge"), 0.3f);
    }
    Total = Gone + 0.05f;
    Tick(0.0f);
}

void AExpeditionSpellFX::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    Age += DeltaSeconds;
    const float Step = FMath::FloorToFloat(Age * StepFps) / StepFps;
    for (FExpeditionSpellFXPiece& P : Pieces)
    {
        float Opacity = 0.0f;
        const float Last = FMath::Max(P.End, P.Linger);
        if (Step >= P.Start && Step <= Last + KINDA_SMALL_NUMBER)
        {
            if (Step <= P.End) { Opacity = P.PeakOpacity; }
            else { Opacity = P.LingerOpacity * (1.0f - (Step - P.End) / FMath::Max(0.01f, P.Linger - P.End)); }
        }
        const float A = P.Peak > P.Start ? EaseOut((Step - P.Start) / (P.Peak - P.Start)) : 1.0f;
        const FVector Where = FMath::Lerp(P.From, P.To, A).GridSnap(SnapCm);
        const FVector Scale = FMath::Lerp(P.ScaleFrom, P.ScaleTo, A);
        const bool bShow = Opacity > 0.005f;
        if (bShow && !P.Mesh->IsVisible()) { UE_LOG(LogTemp, Verbose, TEXT("SPELLFX show piece at %s scale %s age %.3f op %.2f"), *Where.ToString(), *Scale.ToString(), Age, Opacity); }
        P.Mesh->SetVisibility(bShow);
        P.Mesh->SetWorldLocation(Where); P.Mesh->SetWorldScale3D(Scale);
        P.Material->SetScalarParameterValue(TEXT("Opacity"), Opacity);
        if (P.Ink)
        {
            P.Ink->SetVisibility(bShow);
            P.Ink->SetWorldLocation(Where - (P.bFlatInk ? FVector(0, 0, 1) : FVector::ZeroVector));
            P.Ink->SetWorldScale3D(P.bFlatInk ? FVector(Scale.X * P.InkScale, Scale.Y * P.InkScale, 1.0f) : Scale * P.InkScale);
            P.InkMaterial->SetScalarParameterValue(TEXT("Opacity"), Opacity);
        }
    }
    if (Age > Total) { Destroy(); }
}
