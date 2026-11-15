// The battle view's half of the finale set piece (#250): building it with the board, starting it once a won finale's playback is done, and
// converging it under skip, Instant and Resume. Kept apart from ExpeditionBattleView.cpp so the set piece stays one reviewable unit; the view's
// own Tick, effects and camera call in here at fixed points. Presentation only: nothing here reads back into rules, RNG or the checkpoint.
#include "Presentation/ExpeditionBattleView.h"
#include "Presentation/WaystoneSetPiece.h"
#include "Presentation/BoardMetric.h"
#include "Engine/StaticMesh.h"

namespace
{
// The legacy pedestal (BuildEffects): an engine cylinder 0.7 x 0.9 of its 100 cm size, centred 45 reference cm above its base, and the floating
// shard 140 reference cm above that base.
constexpr float PedestalTopRef = 90.0f;
constexpr float PedestalRadiusRef = 35.0f;
constexpr float ShardHeightRef = 140.0f;
}

const UWaystoneSetPiece* AExpeditionBattleView::SetPiece() const { return SetPieceComponent; }

bool AExpeditionBattleView::SetPieceBusy() const
{
    if (!SetPieceComponent || !SetPieceComponent->IsBound()) { return false; }
    if (SetPieceComponent->IsPlaying()) { return true; }
    // Won and not yet started (playback still owes the victory, or the records panel is up): the ending waits for it.
    return SetPieceComponent->State() == EWaystoneSetPieceState::Intact && HasRun() && Progress().Phase == EExpeditionPhase::Victory;
}

void AExpeditionBattleView::OnSetPieceCue(TFunction<void(EWaystoneCue)> Handler)
{
    if (SetPieceComponent) { SetPieceComponent->OnCue = MoveTemp(Handler); }
}

bool AExpeditionBattleView::HoldsSetPiece() const { return SetPieceComponent && SetPieceComponent->IsHeld(); }

void AExpeditionBattleView::BuildSetPiece(const FVector& Pedestal, bool bApse, const FCombatEncounter& Encounter)
{
    if (!SetPieceComponent) { return; }
    SetPieceComponent->Clear();
    if (bOverview) { return; }
    FWaystoneSetPieceSetup Setup;
    if (Theme)
    {
        // A themed finale without the Vault kit keeps today's ending: the seal belongs to the apse.
        if (!bApse) { return; }
        for (const FSoftObjectPath& Path : SetPieceMeshOverride.IsEmpty() ? WaystoneSetPiece::ThemedMeshPaths() : SetPieceMeshOverride)
        {
            UStaticMesh* Mesh = Cast<UStaticMesh>(Path.TryLoad());
            if (!Mesh) { UE_LOG(LogTemp, Log, TEXT("WaystoneSetPiece: %s does not load; the finale ends without the set piece"), *Path.ToString()); return; }
            Setup.Meshes.Add(Mesh);
        }
    }
    else
    {
        Setup.Cube = Cube;
        Setup.PlainMaterial = BaseMaterial;
    }
    // The set piece's space is true cm with its origin at the pedestal's base: the scenery root scales reference cm by the metric, so undo that.
    const float Scale = SceneScale();
    WaystoneSetPiece::FSite& Site = Setup.Site;
    Site.bApse = bApse;
    Site.ArtifactHeight = ShardHeightRef * Scale;
    Site.PedestalTop = bApse ? 0.0f : PedestalTopRef * Scale;
    Site.PedestalRadius = bApse ? 0.0f : PedestalRadiusRef * Scale;
    const FVector Low = SceneBoard(FVector2D(-0.5f, -0.5f)) - Pedestal, High = SceneBoard(FVector2D(Encounter.Width - 0.5f, Encounter.Height - 0.5f)) - Pedestal;
    Site.BoardMin = FVector2D(FMath::Min(Low.X, High.X), FMath::Min(Low.Y, High.Y)) * Scale;
    Site.BoardMax = FVector2D(FMath::Max(Low.X, High.X), FMath::Max(Low.Y, High.Y)) * Scale;
    Site.BoardTop = static_cast<float>(Low.Z) * Scale;
    SetPieceComponent->SetRelativeLocationAndRotation(Pedestal, FRotator::ZeroRotator);
    SetPieceComponent->SetRelativeScale3D(FVector(1.0f / Scale));
    if (!SetPieceComponent->Build(Setup)) { return; }
    // A resumed (or rebuilt) victory shows the settled state; it never replays.
    if (HasRun() && Progress().Phase == EExpeditionPhase::Victory) { SetPieceComponent->Settle(); }
}

void AExpeditionBattleView::UpdateSetPiece(float DeltaSeconds)
{
    if (!SetPieceComponent || !SetPieceComponent->IsBound() || SetPieceComponent->IsHeld() || bShowRecords) { return; }
    if (SetPieceComponent->State() == EWaystoneSetPieceState::Intact && HasRun() && Progress().Phase == EExpeditionPhase::Victory && !Show.IsBusy())
    {
        if (PlaybackSpeed == EPlaybackSpeed::Instant) { SetPieceComponent->Settle(); return; }
        SetPieceComponent->Play();
        return;
    }
    if (SetPieceComponent->IsPlaying()) { SetPieceComponent->Advance(DeltaSeconds * Show.Speed); }
}

void AExpeditionBattleView::SettleSetPiece()
{
    if (SetPieceComponent && SetPieceComponent->IsBound() && HasRun() && Progress().Phase == EExpeditionPhase::Victory) { SetPieceComponent->Settle(); }
}

bool AExpeditionBattleView::PreviewSetPiece(float Seconds)
{
    if (!SetPieceComponent || !SetPieceComponent->IsBound() || !HasRun() || Progress().Phase != EExpeditionPhase::Victory) { return false; }
    SetPieceComponent->Hold(Seconds);
    return true;
}

void AExpeditionBattleView::ClearSetPiecePreview()
{
    if (HoldsSetPiece()) { SetPieceComponent->Settle(); }
}

void AExpeditionBattleView::OverrideSetPieceMeshes(const TArray<FSoftObjectPath>& Paths)
{
    SetPieceMeshOverride = Paths;
    RebuildBoard();
}
