#include "Presentation/ExpeditionSpellFX.h"
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
// Fire: ember orange with a yellow core (kept off party gold, enemy red-orange, Danger and the cyan path).
const FLinearColor FireCore(1.0f, 0.62f, 0.05f);   // saturated yellow core (was pale cream 1/0.88/0.42)
const FLinearColor FireHue(1.0f, 0.5f, 0.06f);
const FLinearColor EmberDeep(0.7f, 0.16f, 0.02f);
const FLinearColor WhiteHot(1.0f, 0.97f, 0.86f);
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
        // Cantrip (size class 0): one small mote, about 0.4 s end to end, burst under half a cell.
        Out.Hue = FLinearColor(1.0f, 0.3f, 0.02f); Out.Glow = 0.85f; Out.Travel = 0.2f;   // deeper orange: the tonemapper lifts bright orange toward yellow Out.BoltScale = 1.0f;
        Out.ImpactGrow = 0.12f; Out.ImpactHold = 0.05f; Out.ImpactLinger = 0.05f; Out.LingerOpacity = 0.3f; Out.ImpactScale = 0.95f;
        return true;
    }
    if (Event.SpellId == TEXT("fireball"))
    {
        // 3rd level (size class 2): exact cells first, a bead, then a sub-second blast that lingers faintly.
        // Telegraph + travel = 0.30 s before impact (direction budget 0.2-0.4 s).
        Out.Glow = 1.0f; Out.Telegraph = 0.12f; Out.bArea = true; Out.Travel = 0.18f; Out.BoltScale = 0.6f;
        Out.ImpactGrow = 0.22f; Out.ImpactHold = 0.12f; Out.ImpactLinger = 0.75f; Out.LingerOpacity = 0.3f; Out.AreaVisualScale = 0.6f;
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
    if (!Material || !LoadMesh(IcoPath) || !LoadMesh(ShardPath) || !LoadMesh(FramePath)) { return nullptr; }
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
        for (const FIntPoint& C : Event.Cells) { Radius = FMath::Max(Radius, FVector2D::Distance(Centre, FVector2D(C.X, C.Y)) + 0.5f); }
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
        for (const FIntPoint& C : Event.Cells)
        {
            FExpeditionSpellFXPiece& P = Add(Frame, Style, false, FVector::UpVector, false, true);
            P.Start = 0.0f; P.Peak = 0.06f; P.End = Full; P.Linger = Gone; P.PeakOpacity = 0.9f; P.LingerOpacity = 0.3f;
            P.From = P.To = CellToWorld(C) + FVector(0, 0, 3);
            P.ScaleFrom = P.ScaleTo = FVector(CellCm / 100.0f * 0.9f, CellCm / 100.0f * 0.9f, 1.0f);
            P.Material->SetScalarParameterValue(TEXT("Glow"), 0.8f);
            P.Material->SetScalarParameterValue(TEXT("Edge"), 0.0f);
        }
    }
    {
        FExpeditionSpellFXPiece& P = Add(Shard, Style, true, Dir, false);
        P.Start = Launch; P.Peak = Hit; P.End = Hit; P.Linger = Hit;
        P.From = Caster + Dir * 40.0f; P.To = Impact - Dir * (Style.bArea ? 0.0f : 20.0f);
        P.ScaleFrom = P.ScaleTo = FVector(Style.BoltScale, Style.BoltScale * 0.8f, Style.BoltScale * 0.8f);
        P.Mesh->SetWorldRotation(Dir.Rotation()); P.Ink->SetWorldRotation(Dir.Rotation());
        // Top-down camera sees the face-on band, so the bolt body is the element hue, not the pale core.
        P.Material->SetVectorParameterValue(TEXT("Core"), Style.Hue); P.Material->SetVectorParameterValue(TEXT("Hue"), EmberDeep);
    }
    // Area blasts draw well inside the true area (the telegraphed cells stay exact) so the board stays readable.
    const float Diameter = Style.bArea ? Radius * 2.0f * CellCm / 100.0f * Style.AreaVisualScale : Style.ImpactScale;
    {
        // The blast (Fireball) or the burst (Fire Bolt): flat bands, ink shell, ragged outer edge.
        // No back-face ink shell on the big translucent blast: its noisy edge showed through as specks.
        FExpeditionSpellFXPiece& P = Add(Ico, Style, !Style.bArea, FVector::UpVector, true);
        P.Start = Hit; P.Peak = Full; P.End = Fade; P.Linger = Gone; P.PeakOpacity = Style.bArea ? 0.5f : 1.0f;
        // Both blasts read as ember orange (the camera looks straight down, so the face-on band is what shows);
        // the yellow (Fire Bolt) or white-hot (Fireball) centre comes from the core piece below.
        P.Material->SetVectorParameterValue(TEXT("Core"), Style.Hue); P.Material->SetVectorParameterValue(TEXT("Hue"), EmberDeep);
        P.LingerOpacity = Style.LingerOpacity;
        P.From = P.To = Impact;
        const FVector Size(Diameter, Diameter, Style.bArea ? Diameter * 0.5f : Diameter);
        P.ScaleFrom = Size * 0.2f; P.ScaleTo = Size;
        P.InkScale = Style.bArea ? 1.025f : 1.12f;
    }
    if (!Style.bArea)
    {
        // Fire Bolt: small yellow core inside the orange burst.
        FExpeditionSpellFXPiece& P = Add(Ico, Style, false, FVector::UpVector, true);
        P.Start = Hit; P.Peak = Full; P.End = Fade; P.Linger = Fade + 0.03f; P.PeakOpacity = 1.0f;
        P.From = P.To = Impact + FVector(0, 0, 8);
        P.ScaleFrom = FVector(Diameter * 0.08f); P.ScaleTo = FVector(Diameter * 0.3f);
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
        P.ScaleFrom = FVector(0.3f); P.ScaleTo = FVector(Diameter * 0.26f, Diameter * 0.26f, Diameter * 0.18f);
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
    for (FExpeditionSpellFXPiece& P : Pieces)
    {
        float Opacity = 0.0f;
        const float Last = FMath::Max(P.End, P.Linger);
        if (Age >= P.Start && Age <= Last + KINDA_SMALL_NUMBER)
        {
            if (Age <= P.End) { Opacity = P.PeakOpacity * FMath::Clamp((Age - P.Start) / 0.03f, 0.0f, 1.0f); }
            else { Opacity = P.LingerOpacity * (1.0f - (Age - P.End) / FMath::Max(0.01f, P.Linger - P.End)); }
        }
        const float A = P.Peak > P.Start ? EaseOut((Age - P.Start) / (P.Peak - P.Start)) : 1.0f;
        const FVector Where = FMath::Lerp(P.From, P.To, A);
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
