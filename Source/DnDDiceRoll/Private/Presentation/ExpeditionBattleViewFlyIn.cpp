// The battle view's half of the route fly-in (#249; RouteFlyIn.h): starting it from a committed route choice on the journey map, advancing it, handing over to the
// board at the cut, and converging it under skip, Finish, Instant and Resume. Kept apart from ExpeditionBattleView.cpp like the set piece. Presentation only: the route
// is already committed when it starts, and nothing here reads back into rules, RNG or the checkpoint.
#include "Presentation/ExpeditionBattleView.h"
#include "Engine/StaticMesh.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace
{
const FName CameraFadeParameter(TEXT("CameraFade"));
}

void AExpeditionBattleView::StopSceneryCameraClip()
{
    // Once per move, on the journey's own parts (loose components with a material instance each): no per-frame cost.
    for (FExpeditionPart& Part : Scenery)
    {
        if (!Part.Mesh) { continue; }
        for (int32 Slot = 0; Slot < Part.Mesh->GetNumMaterials(); ++Slot)
        {
            UMaterialInterface* Look = Part.Mesh->GetMaterial(Slot);
            float Fade = 0.0f;
            if (!Look || !Look->GetScalarParameterValue(CameraFadeParameter, Fade) || Fade == 0.0f) { continue; }
            UMaterialInstanceDynamic* Own = Cast<UMaterialInstanceDynamic>(Look);
            if (!Own) { Own = Part.Mesh->CreateAndSetMaterialInstanceDynamic(Slot); }
            if (Own) { Own->SetScalarParameterValue(CameraFadeParameter, 0.0f); }
        }
    }
}

float AExpeditionBattleView::SceneryCameraFade() const
{
    float Largest = 0.0f;
    for (const FExpeditionPart& Part : Scenery)
    {
        if (!Part.Mesh) { continue; }
        for (int32 Slot = 0; Slot < Part.Mesh->GetNumMaterials(); ++Slot)
        {
            float Fade = 0.0f;
            if (const UMaterialInterface* Look = Part.Mesh->GetMaterial(Slot); Look && Look->GetScalarParameterValue(CameraFadeParameter, Fade)) { Largest = FMath::Max(Largest, Fade); }
        }
    }
    return Largest;
}

bool AExpeditionBattleView::RaisedDioramaBounds(int32 Offer, FBox& OutWorld) const
{
    FTransform World;
    if (!RaisedDioramaTransform(Offer, World)) { return false; }
    for (const JourneyLayout::FRaisedLook& Raised : JourneyLook.Raised)
    {
        if (Raised.Offer != Offer) { continue; }
        const UStaticMesh* Diorama = Raised.Kind == OverviewLayout::EDiorama::Vault ? OverviewVault.Get() : Raised.Kind == OverviewLayout::EDiorama::Vigil ? OverviewVigil.Get() : OverviewRoot.Get();
        if (!Diorama) { return false; }
        OutWorld = Diorama->GetBoundingBox().TransformBy(World);
        return OutWorld.IsValid != 0;
    }
    return false;
}

bool AExpeditionBattleView::StartFlyIn(int32 Offer)
{
    // The phase is already the committed fight's; the journey (built for the route screen) is still what is drawn, so its diorama and camera are still there to fly from.
    FBox Diorama;
    if (!bRouteFlyIn || PlaybackSpeed == EPlaybackSpeed::Instant || bShowRecords || !ShowingJourney() || !RaisedDioramaBounds(Offer, Diorama)) { return false; }
    FlyInMove = RouteFlyIn::PathFor(JourneyCameraTransform(), Diorama, JourneyLayout::CameraFieldOfView);
    FlyInRoute = Offer;
    FlyInSeconds = 0.0f;
    bFlyInHeld = false;
    FlyIn = ERouteFlyInStage::Moving;
    StopSceneryCameraClip();
    UpdateCamera(0.0f);
    return true;
}

void AExpeditionBattleView::HandOverFlyIn(bool bArrive)
{
    // The board of the committed route, at its opening framing (RebuildBoard drops the fly-in and snaps the camera): the one load hitch, here under the dark.
    RebuildBoard();
    if (bArrive) { FlyIn = ERouteFlyInStage::Arriving; FlyInSeconds = RouteFlyIn::CutSeconds; }
}

void AExpeditionBattleView::UpdateFlyIn(float DeltaSeconds)
{
    if (FlyIn == ERouteFlyInStage::None || bFlyInHeld) { return; }
    // At playback speed (Fast plays it faster), and never more than a short frame at a time: the cut's frame is long, and the fade-up starts from dark after it.
    FlyInSeconds += FMath::Min(FMath::Max(DeltaSeconds, 0.0f), RouteFlyIn::MaxStepSeconds) * FMath::Max(Show.Speed, 1.0f);
    if (FlyIn == ERouteFlyInStage::Moving && FlyInSeconds >= RouteFlyIn::CutSeconds) { HandOverFlyIn(true); return; }
    if (FlyIn == ERouteFlyInStage::Arriving && FlyInSeconds >= RouteFlyIn::DurationSeconds) { FlyIn = ERouteFlyInStage::None; ShowRouteBanner(); }
}

void AExpeditionBattleView::FinishFlyIn()
{
    if (FlyIn == ERouteFlyInStage::None) { return; }
    // A capture's held moment has no route behind it to hand over to.
    if (bFlyInHeld) { ClearFlyInPreview(); return; }
    if (FlyIn == ERouteFlyInStage::Moving) { HandOverFlyIn(false); }
    FlyIn = ERouteFlyInStage::None;
    ShowRouteBanner();
}

bool AExpeditionBattleView::PreviewFlyIn(int32 Offer, float Seconds)
{
    ClearFlyInPreview();
    Seconds = FMath::Clamp(Seconds, 0.0f, RouteFlyIn::DurationSeconds);
    if (Seconds < RouteFlyIn::CutSeconds)
    {
        // The move: the route screen's journey, the camera on its way into the offer's diorama.
        FBox Diorama;
        if (!HasRun() || Progress().Phase != EExpeditionPhase::Route || !ShowingJourney() || !RaisedDioramaBounds(Offer, Diorama)) { return false; }
        FlyInMove = RouteFlyIn::PathFor(JourneyCameraTransform(), Diorama, JourneyLayout::CameraFieldOfView);
        FlyInRoute = Offer;
        FlyIn = ERouteFlyInStage::Moving;
        StopSceneryCameraClip();
    }
    else
    {
        // The arrival: the fight's opening framing (as RebuildBoard leaves it after the route screen) under the fade.
        if (!InCombat() || bOverview) { return false; }
        TargetYaw = -90.0f;
        TargetDistance = FramedDistance(TargetYaw);
        bFramed = true;
        PanOffset = FVector2D::ZeroVector;
        LastFocusId = INDEX_NONE;
        bCameraPlaced = false;
        FlyInRoute = Offer;
        FlyIn = ERouteFlyInStage::Arriving;
    }
    FlyInSeconds = Seconds;
    bFlyInHeld = true;
    UpdateCamera(0.0f);
    return true;
}

void AExpeditionBattleView::ClearFlyInPreview()
{
    if (!bFlyInHeld) { return; }
    bFlyInHeld = false;
    FlyIn = ERouteFlyInStage::None;
    FlyInSeconds = 0.0f;
    bCameraPlaced = false;
    UpdateCamera(0.0f);
}
