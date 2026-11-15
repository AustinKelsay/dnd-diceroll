#include "Presentation/WaystoneSetPiece.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

namespace WaystoneSetPiece
{
namespace
{
float Smooth(float A, float B, float X) { return FMath::SmoothStep(A, B, X); }
float Smoother(float A, float B, float X)
{
    const float T = FMath::Clamp((X - A) / (B - A), 0.0f, 1.0f);
    return T * T * T * (T * (T * 6.0f - 15.0f) + 10.0f);
}
/** 1 until the shatter, easing to 0 at the end of the timeline: what is left of a flash when the set piece settles is exactly nothing. */
float Fade(float Seconds) { return 1.0f - Smooth(ShatterSeconds, DurationSeconds, Seconds); }
constexpr float HopSeconds = 0.2f;

/**
 * Where each piece lies once the seal has broken, as offsets from the pedestal (cm) and a turn about the vertical. Authored against the
 * Vault apse (build_vault_r002.py): the three dais steps start 105, 65 and 25 cm in front of the pedestal, the monolith stands 150 cm behind it
 * and the kneeling statues 190 cm to each side and 40 cm behind, so nothing lands in them; and nothing lands nearer the board than 150 cm,
 * which keeps the pieces clear of the brazier ring at the back of the board. Shard k hangs at 30 + 60k degrees.
 */
struct FRestSpot { float X, Y, Yaw; };
constexpr FRestSpot UpperRests[Shards] = {{235, -45, 20}, {70, -15, -35}, {-225, -30, 60}, {-265, -125, 10}, {30, -140, -70}, {270, -120, 45}};
constexpr FRestSpot LowerRests[Shards] = {{125, -10, 80}, {-45, 10, -15}, {-130, 5, 35}, {-150, -85, -60}, {-25, -95, 25}, {155, -80, -40}};
constexpr FRestSpot ChipRests[Chips] = {{300, -70, 10}, {185, -135, 70}, {-60, -50, 130}, {105, 15, 200}, {-300, -60, 250}, {-190, -140, 40},
    {-95, -125, 300}, {70, -60, 160}, {150, -30, 95}, {340, -100, 220}, {-15, -20, 15}, {45, -145, 275}};
/** The legacy pedestal stands in a board corner with less room around it: the same pattern, drawn in. */
constexpr float PedestalSpread = 0.8f;
/** Resting heights above the floor of a half lying on its side and of a chip (their cross-sections' half widths). */
constexpr float HalfLift = 8.0f;
constexpr float ChipLift = 3.5f;
}

float FlareGain(float Seconds)
{
    if (Seconds <= 0.0f) { return 1.0f; }
    if (Seconds < ShatterSeconds) { const float A = Seconds / ShatterSeconds; return 1.0f + 2.5f * A * A; }
    const float U = Seconds - ShatterSeconds;
    return 1.0f + (2.5f * FMath::Exp(-1.4f * U) + 5.0f * FMath::Exp(-9.0f * U)) * Fade(Seconds);
}

float SealGlow(float Seconds)
{
    if (Seconds <= 0.0f) { return 1.0f; }
    if (Seconds < ShatterSeconds) { const float A = Seconds / ShatterSeconds; return 1.0f + 5.0f * A * A; }
    // Spent runes: the broken halves keep a third of their glow.
    constexpr float Spent = 0.35f;
    return Spent + (6.0f - Spent) * FMath::Exp(-3.0f * (Seconds - ShatterSeconds)) * Fade(Seconds);
}

float LiftAlpha(float Seconds) { return Smoother(LiftStartSeconds, LiftEndSeconds, Seconds); }

float ExtraSpin(float Seconds)
{
    if (Seconds <= 0.0f) { return 0.0f; }
    if (Seconds >= SpinSeconds) { return ExtraSpinDegrees; }
    return ExtraSpinDegrees * 0.5f * (1.0f - FMath::Cos(PI * Seconds / SpinSeconds));
}

float CameraWeight(float Seconds) { return Seconds <= 0.0f ? 0.0f : Smooth(0.0f, CameraInSeconds, Seconds); }
float ShotBlend(float Seconds) { return Smooth(LiftStartSeconds, LiftEndSeconds + 0.1f, Seconds); }

void CuesBetween(float From, float To, TArray<EWaystoneCue>& Out)
{
    const TPair<float, EWaystoneCue> Moments[] = {{ShatterSeconds, EWaystoneCue::Shatter}, {LiftStartSeconds, EWaystoneCue::Lift}, {DurationSeconds, EWaystoneCue::Settled}};
    for (const auto& Moment : Moments) { if (From < Moment.Key && Moment.Key <= To) { Out.Add(Moment.Value); } }
}

const TCHAR* CueName(EWaystoneCue Cue)
{
    switch (Cue)
    {
    case EWaystoneCue::Flare: return TEXT("flare");
    case EWaystoneCue::Shatter: return TEXT("shatter");
    case EWaystoneCue::Lift: return TEXT("lift");
    default: return TEXT("settled");
    }
}

float LandedAt(const FFlight& Flight) { return ShatterSeconds + Flight.Delay + Flight.Duration + HopSeconds; }

FTransform FlightAt(const FFlight& Flight, float Seconds)
{
    const float Local = Seconds - ShatterSeconds - Flight.Delay;
    if (Local <= 0.0f) { return Flight.Start; }
    if (Seconds >= LandedAt(Flight)) { return Flight.Rest; }
    const FVector From = Flight.Start.GetLocation(), To = Flight.Rest.GetLocation();
    FVector Across = To - From;
    Across.Z = 0.0;
    const FVector Direction = Across.IsNearlyZero() ? FVector::ForwardVector : Across.GetSafeNormal();
    // The arc ends short of the rest by the slide, and the hop carries the piece the rest of the way.
    const FVector Touch = To - Direction * Flight.Slide;
    const FQuat Rest = Flight.Rest.GetRotation();
    if (Local < Flight.Duration)
    {
        const float U = Local / Flight.Duration;
        const FVector Arc = FMath::Lerp(From, Touch, U) + FVector(0.0, 0.0, 4.0 * Flight.Apex * U * (1.0 - U));
        // Whole tumbling turns about the axis: at U = 1 the tumble is a full rotation and the piece already wears its resting turn.
        const FQuat Tumble(Flight.TumbleAxis.GetSafeNormal(), 2.0 * PI * Flight.Turns * U);
        return FTransform(FQuat::Slerp(Flight.Start.GetRotation(), Rest, U) * Tumble, Arc, Flight.Rest.GetScale3D());
    }
    const float V = FMath::Clamp((Local - Flight.Duration) / HopSeconds, 0.0f, 1.0f);
    return FTransform(Rest, FMath::Lerp(Touch, To, V) + FVector(0.0, 0.0, Flight.Hop * FMath::Sin(PI * V)), Flight.Rest.GetScale3D());
}

float FloorAt(const FSite& Site, FVector2D Point)
{
    if (Site.bApse)
    {
        // The dais (build_vault_r002.py waystone_local): steps 20, 40 and 60 cm high, half widths 320, 280 and 240 cm, starting 105, 65 and 25 cm in
        // front of the pedestal and running back to the wall.
        const double X = FMath::Abs(Point.X), Y = Point.Y;
        if (X <= 240.0 && Y >= -25.0) { return 60.0f; }
        if (X <= 280.0 && Y >= -65.0) { return 40.0f; }
        if (X <= 320.0 && Y >= -105.0) { return 20.0f; }
    }
    else if (Site.PedestalRadius > 0.0f && Point.Size() <= Site.PedestalRadius) { return Site.PedestalTop; }
    if (Point.X >= Site.BoardMin.X && Point.X <= Site.BoardMax.X && Point.Y >= Site.BoardMin.Y && Point.Y <= Site.BoardMax.Y) { return Site.BoardTop; }
    return 0.0f;
}

TArray<FFlight> Plan(const FSite& Site)
{
    TArray<FFlight> Out;
    Out.Reserve(Pieces);
    const float Spread = Site.bApse ? 1.0f : PedestalSpread;
    const FVector Centre(0.0, 0.0, Site.ArtifactHeight);
    auto RingPoint = [&](int32 Shard)
    {
        const float Degrees = 30.0f + 60.0f * Shard;
        const float Radians = FMath::DegreesToRadians(Degrees);
        return TPair<FVector, float>(Centre + FVector(FMath::Cos(Radians), FMath::Sin(Radians), 0.0) * RingRadiusCm, Degrees);
    };
    auto Resting = [&](const FRestSpot& Spot, float Lift, const FRotator& Turn)
    {
        const FVector2D At(Spot.X * Spread, Spot.Y * Spread);
        return FTransform(Turn, FVector(At.X, At.Y, FloorAt(Site, At) + Lift));
    };
    for (int32 Shard = 0; Shard < Shards; ++Shard)
    {
        const TPair<FVector, float> Ring = RingPoint(Shard);
        // Upright, its rune face (+X) turned outward, the top leaning away from the artifact: both halves share the pivot on the crack.
        const FTransform Hanging(FRotator(-ShardLeanDegrees, Ring.Value, 0.0f), Ring.Key);
        const float Radians = FMath::DegreesToRadians(Ring.Value);
        const FVector Tangent(-FMath::Sin(Radians), FMath::Cos(Radians), 0.3);
        for (int32 Half = 0; Half < 2; ++Half)
        {
            const bool bUpper = Half == 0;
            const FRestSpot& Spot = bUpper ? UpperRests[Shard] : LowerRests[Shard];
            FFlight Flight;
            Flight.Start = Hanging;
            // Lying on its side: the long axis horizontal, turned by the authored yaw, rolled to one of its faces.
            Flight.Rest = Resting(Spot, HalfLift, FRotator(bUpper ? 90.0f : -90.0f, Ring.Value + Spot.Yaw, 60.0f * Shard));
            Flight.Delay = bUpper ? 0.02f * Shard : 0.05f + 0.02f * Shard;
            Flight.Duration = bUpper ? 0.75f + 0.05f * (Shard % 3) : 0.55f + 0.05f * (Shard % 2);
            Flight.Apex = bUpper ? 70.0f + 15.0f * (Shard % 3) : 35.0f;
            Flight.Hop = 6.0f;
            Flight.Slide = 10.0f;
            Flight.TumbleAxis = bUpper ? Tangent : FVector(Tangent.Y, -Tangent.X, 0.4);
            Flight.Turns = bUpper ? 1 + Shard % 2 : 1;
            Out.Add(Flight);
        }
    }
    for (int32 Chip = 0; Chip < Chips; ++Chip)
    {
        const TPair<FVector, float> Ring = RingPoint(Chip / 2);
        FFlight Flight;
        Flight.Start = FTransform(FRotator(37.0f * Chip, 53.0f * Chip, 71.0f * Chip), Ring.Key + FVector(0.0, 0.0, Chip % 2 ? -6.0 : 6.0));
        Flight.Rest = Resting(ChipRests[Chip], ChipLift, FRotator(0.0f, ChipRests[Chip].Yaw, Chip % 2 ? 180.0f : 0.0f));
        Flight.Delay = 0.012f * Chip;
        Flight.Duration = 0.5f + 0.04f * (Chip % 4);
        Flight.Apex = 55.0f + 10.0f * (Chip % 3);
        Flight.Hop = 3.0f;
        Flight.Slide = 6.0f;
        Flight.TumbleAxis = FVector(Chip % 2 ? 1.0 : 0.3, Chip % 3 ? 0.6 : -0.8, 0.2);
        Flight.Turns = 2;
        Out.Add(Flight);
    }
    return Out;
}

FShots ShotsFor(const FSite& Site)
{
    FShots Shots;
    if (Site.bApse)
    {
        // From over the back of the board: first the whole Waystone with the seal at its foot, then (the hold) lower and to one side, turned so the
        // Waystone stands in the left third of the frame beside the ending panel.
        Shots.EyeA = FVector(0.0, -1450.0, 470.0); Shots.FocusA = FVector(0.0, 70.0, 250.0);
        Shots.EyeB = FVector(-180.0, -1600.0, 340.0); Shots.FocusB = FVector(0.0, 60.0, 300.0);
    }
    else
    {
        // The legacy pedestal in the board's far corner, seen from across the board.
        Shots.EyeA = FVector(-420.0, -950.0, 520.0); Shots.FocusA = FVector(0.0, 0.0, 170.0);
        Shots.EyeB = FVector(-480.0, -1050.0, 380.0); Shots.FocusB = FVector(0.0, 0.0, 200.0);
    }
    Shots.HeldYawDegrees = 15.0f;
    return Shots;
}

void ShotAt(const FShots& Shots, float Seconds, FVector& Eye, FQuat& Rotation)
{
    const float Blend = ShotBlend(Seconds);
    Eye = FMath::Lerp(Shots.EyeA, Shots.EyeB, Blend);
    const FVector Focus = FMath::Lerp(Shots.FocusA, Shots.FocusB, Blend);
    FRotator Look = (Focus - Eye).Rotation();
    Look.Yaw += Shots.HeldYawDegrees * Blend;
    if (Seconds > ShatterSeconds)
    {
        // The shatter's jolt: a few centimetres, gone well before the hold.
        const float Amount = 7.0f * FMath::Exp(-5.0f * (Seconds - ShatterSeconds)) * Fade(Seconds);
        Eye += FVector(FMath::Sin(Seconds * 61.0f), FMath::Sin(Seconds * 47.0f + 1.0f), FMath::Sin(Seconds * 53.0f + 2.0f)) * Amount;
    }
    Rotation = Look.Quaternion();
}

TArray<FSoftObjectPath> ThemedMeshPaths()
{
    return {
        FSoftObjectPath(TEXT("/Game/DnDDiceRoll/Theme/SetPiece/SM_SetPiece_Vault_SealUpper.SM_SetPiece_Vault_SealUpper")),
        FSoftObjectPath(TEXT("/Game/DnDDiceRoll/Theme/SetPiece/SM_SetPiece_Vault_SealLower.SM_SetPiece_Vault_SealLower")),
        FSoftObjectPath(TEXT("/Game/DnDDiceRoll/Theme/SetPiece/SM_SetPiece_Vault_Chip.SM_SetPiece_Vault_Chip")),
    };
}
}

namespace
{
/** Which group a piece draws in (upper halves, lower halves, chips) and its instance there. */
int32 GroupOf(int32 Piece) { return Piece < WaystoneSetPiece::Halves ? Piece % 2 : 2; }
int32 InstanceOf(int32 Piece) { return Piece < WaystoneSetPiece::Halves ? Piece / 2 : Piece - WaystoneSetPiece::Halves; }
/** The engine cube standing in for each piece on the primitive look: its size and where it sits from the piece's pivot (cm). */
FTransform PrimitiveLocal(int32 Group)
{
    switch (Group)
    {
    case 0: return FTransform(FQuat::Identity, FVector(0.0, 0.0, 14.0), FVector(0.15, 0.15, 0.28));
    case 1: return FTransform(FQuat::Identity, FVector(0.0, 0.0, -9.0), FVector(0.15, 0.15, 0.18));
    default: return FTransform(FQuat::Identity, FVector::ZeroVector, FVector(0.09, 0.07, 0.06));
    }
}
}

bool UWaystoneSetPiece::Build(const FWaystoneSetPieceSetup& Setup)
{
    Clear();
    const bool bThemed = !Setup.Meshes.IsEmpty();
    if (bThemed && (Setup.Meshes.Num() != 3 || Setup.Meshes.Contains(nullptr))) { return false; }
    if (!bThemed && !Setup.Cube) { return false; }
    AActor* Owner = GetOwner();
    if (!Owner) { return false; }
    Site = Setup.Site;
    Shots = WaystoneSetPiece::ShotsFor(Site);
    Flights = WaystoneSetPiece::Plan(Site);
    // The primitive stand-ins: dark basalt halves and chips in one plain colour.
    UMaterialInstanceDynamic* Plain = nullptr;
    if (!bThemed && Setup.PlainMaterial)
    {
        Plain = UMaterialInstanceDynamic::Create(Setup.PlainMaterial, this);
        Plain->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.11f, 0.13f, 0.19f));
    }
    for (int32 Group = 0; Group < 3; ++Group)
    {
        UStaticMesh* Mesh = bThemed ? Setup.Meshes[Group] : Setup.Cube;
        UInstancedStaticMeshComponent* Set = NewObject<UInstancedStaticMeshComponent>(Owner);
        Owner->AddInstanceComponent(Set);
        Set->SetupAttachment(this);
        Set->SetMobility(EComponentMobility::Movable);
        Set->SetStaticMesh(Mesh);
        Set->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Set->SetCanEverAffectNavigation(false);
        // The halves cast shadows; the chips are too small to.
        Set->SetCastShadow(Group < 2);
        Set->SetHasPerInstancePrevTransforms(true);
        if (bThemed)
        {
            if (UMaterialInterface* Own = Mesh->GetMaterial(0))
            {
                UMaterialInstanceDynamic* Look = UMaterialInstanceDynamic::Create(Own, this);
                float Glow = 0.0f;
                Look->GetScalarParameterValue(FName(TEXT("Glow")), Glow);
                Set->SetMaterial(0, Look);
                Looks.Add(Look);
                BaseGlow.Add(Glow);
            }
        }
        else if (Plain) { Set->SetMaterial(0, Plain); }
        const int32 Count = Group < 2 ? WaystoneSetPiece::Shards : WaystoneSetPiece::Chips;
        for (int32 Index = 0; Index < Count; ++Index) { Set->AddInstance(FTransform(FQuat::Identity, FVector::ZeroVector, FVector::ZeroVector), false); }
        Set->RegisterComponent();
        Groups.Add(Set);
    }
    Current = EWaystoneSetPieceState::Intact;
    Seconds = 0.0f;
    Drawn.Init(FTransform(FQuat::Identity, FVector::ZeroVector, FVector::ZeroVector), WaystoneSetPiece::Pieces);
    Pose(false);
    return true;
}

void UWaystoneSetPiece::Clear()
{
    for (UInstancedStaticMeshComponent* Set : Groups)
    {
        if (!IsValid(Set)) { continue; }
        if (AActor* Owner = GetOwner()) { Owner->RemoveInstanceComponent(Set); }
        Set->DestroyComponent();
    }
    Groups.Reset(); Looks.Reset(); BaseGlow.Reset(); Flights.Reset(); Drawn.Reset();
    Current = EWaystoneSetPieceState::Unbound;
    Seconds = 0.0f; bHeld = false; ShownGlow = -1.0f; Writes = 0;
}

void UWaystoneSetPiece::Play()
{
    if (Current != EWaystoneSetPieceState::Intact) { return; }
    Current = EWaystoneSetPieceState::Playing;
    Seconds = 0.0f;
    Announce(EWaystoneCue::Flare);
    Pose(false);
}

void UWaystoneSetPiece::Settle()
{
    if (!IsBound()) { return; }
    bHeld = false;
    if (Current == EWaystoneSetPieceState::Settled) { return; }
    Current = EWaystoneSetPieceState::Settled;
    Seconds = WaystoneSetPiece::DurationSeconds;
    Pose(false);
}

void UWaystoneSetPiece::Restore()
{
    if (!IsBound()) { return; }
    bHeld = false;
    Current = EWaystoneSetPieceState::Intact;
    Seconds = 0.0f;
    Pose(false);
}

void UWaystoneSetPiece::Advance(float DeltaSeconds)
{
    if (Current != EWaystoneSetPieceState::Playing || bHeld || DeltaSeconds <= 0.0f) { return; }
    const float From = Seconds;
    Seconds = FMath::Min(Seconds + DeltaSeconds, WaystoneSetPiece::DurationSeconds);
    TArray<EWaystoneCue, TInlineAllocator<3>> Passed;
    {
        TArray<EWaystoneCue> Cues;
        WaystoneSetPiece::CuesBetween(From, Seconds, Cues);
        Passed.Append(Cues);
    }
    if (Seconds >= WaystoneSetPiece::DurationSeconds) { Current = EWaystoneSetPieceState::Settled; }
    Pose(Current == EWaystoneSetPieceState::Playing);
    for (const EWaystoneCue Cue : Passed) { Announce(Cue); }
}

void UWaystoneSetPiece::Hold(float At)
{
    if (!IsBound()) { return; }
    Seconds = FMath::Clamp(At, 0.0f, WaystoneSetPiece::DurationSeconds);
    bHeld = Seconds < WaystoneSetPiece::DurationSeconds;
    Current = bHeld ? EWaystoneSetPieceState::Playing : EWaystoneSetPieceState::Settled;
    Pose(false);
}

float UWaystoneSetPiece::FlareGain() const
{
    return Current == EWaystoneSetPieceState::Playing ? WaystoneSetPiece::FlareGain(Seconds) : 1.0f;
}

float UWaystoneSetPiece::LiftAlpha() const
{
    return Current == EWaystoneSetPieceState::Settled ? 1.0f : Current == EWaystoneSetPieceState::Playing ? WaystoneSetPiece::LiftAlpha(Seconds) : 0.0f;
}

float UWaystoneSetPiece::ExtraSpin() const
{
    return Current == EWaystoneSetPieceState::Settled ? WaystoneSetPiece::ExtraSpinDegrees : Current == EWaystoneSetPieceState::Playing ? WaystoneSetPiece::ExtraSpin(Seconds) : 0.0f;
}

float UWaystoneSetPiece::CameraWeight() const
{
    return Current == EWaystoneSetPieceState::Settled ? 1.0f : Current == EWaystoneSetPieceState::Playing ? WaystoneSetPiece::CameraWeight(Seconds) : 0.0f;
}

void UWaystoneSetPiece::Shot(FVector& Eye, FQuat& Rotation) const
{
    FVector LocalEye;
    FQuat LocalRotation;
    WaystoneSetPiece::ShotAt(Shots, Current == EWaystoneSetPieceState::Settled ? WaystoneSetPiece::DurationSeconds : Seconds, LocalEye, LocalRotation);
    const FTransform Space = GetComponentTransform();
    Eye = Space.TransformPosition(LocalEye);
    Rotation = Space.GetRotation() * LocalRotation;
}

void UWaystoneSetPiece::FrameCamera(FVector& Eye, FRotator& Rotation) const
{
    const float Weight = CameraWeight();
    if (Weight <= 0.0f) { return; }
    FVector ShotEye;
    FQuat ShotRotation;
    Shot(ShotEye, ShotRotation);
    if (Weight >= 1.0f) { Eye = ShotEye; Rotation = ShotRotation.Rotator(); return; }
    Eye = FMath::Lerp(Eye, ShotEye, Weight);
    Rotation = FQuat::Slerp(Rotation.Quaternion(), ShotRotation, Weight).Rotator();
}

int32 UWaystoneSetPiece::ComponentCount() const { return Groups.Num(); }

void UWaystoneSetPiece::Announce(EWaystoneCue Cue) const
{
    UE_LOG(LogTemp, Verbose, TEXT("WaystoneSetPiece cue %s at %.2f s"), WaystoneSetPiece::CueName(Cue), Seconds);
    if (OnCue) { OnCue(Cue); }
}

void UWaystoneSetPiece::Pose(bool bMotion)
{
    if (Groups.Num() != 3 || Flights.Num() != WaystoneSetPiece::Pieces) { return; }
    const bool bSettled = Current == EWaystoneSetPieceState::Settled;
    const bool bIntact = Current == EWaystoneSetPieceState::Intact;
    const float Charge = bIntact || bSettled ? 0.0f : FMath::Clamp(Seconds / WaystoneSetPiece::ShatterSeconds, 0.0f, 1.0f);
    const bool bBroken = !bIntact && (bSettled || Seconds >= WaystoneSetPiece::ShatterSeconds);
    const bool bThemed = !Looks.IsEmpty();
    TArray<FTransform> New[3], Previous[3];
    for (int32 Piece = 0; Piece < WaystoneSetPiece::Pieces; ++Piece)
    {
        const WaystoneSetPiece::FFlight& Flight = Flights[Piece];
        const bool bChip = Piece >= WaystoneSetPiece::Halves;
        FTransform At;
        if (bChip && !bBroken) { At = FTransform(FQuat::Identity, Flight.Start.GetLocation(), FVector::ZeroVector); }
        else if (bSettled) { At = Flight.Rest; }
        else if (bIntact) { At = Flight.Start; }
        else
        {
            At = WaystoneSetPiece::FlightAt(Flight, Seconds);
            if (!bBroken)
            {
                // The seal strains as the Waystone charges: a tremble that grows to the break.
                const float Shake = 2.5f * Charge * Charge;
                const float Phase = Seconds * 53.0f + Piece * 1.7f;
                At.AddToTranslation(FVector(FMath::Sin(Phase), FMath::Sin(Phase * 1.3f + 1.0f), FMath::Sin(Phase * 0.7f + 2.0f)) * Shake);
            }
        }
        const int32 Group = GroupOf(Piece);
        const FTransform Instance = bThemed ? At : PrimitiveLocal(Group) * At;
        New[Group].Add(Instance);
        FTransform Before = Instance;
        if (bMotion) { Groups[Group]->GetInstanceTransform(InstanceOf(Piece), Before, false); }
        Previous[Group].Add(Before);
        Drawn[Piece] = At;
    }
    for (int32 Group = 0; Group < 3; ++Group)
    {
        Groups[Group]->BatchUpdateInstancesTransforms(0, New[Group], Previous[Group], false, true, false);
        ++Writes;
    }
    const float Glow = bIntact ? 1.0f : bSettled ? WaystoneSetPiece::SealGlow(WaystoneSetPiece::DurationSeconds) : WaystoneSetPiece::SealGlow(Seconds);
    if (!FMath::IsNearlyEqual(Glow, ShownGlow, 1e-4f))
    {
        for (int32 Index = 0; Index < Looks.Num(); ++Index) { if (Looks[Index]) { Looks[Index]->SetScalarParameterValue(TEXT("Glow"), BaseGlow[Index] * Glow); } }
        ShownGlow = Glow;
    }
}
