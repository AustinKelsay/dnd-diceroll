#include "Presentation/ExpeditionBattleView.h"
#include "Combat/ActionFacts.h"
#include "Presentation/ActionText.h"
#include "Presentation/ExpeditionAudio.h"
#include "Presentation/DiceRollTheme.h"
#include "Presentation/OccluderFade.h"
#include "Presentation/ExpeditionHUD.h"
#include "Presentation/ExpeditionPlayerController.h"
#include "Camera/CameraComponent.h"
#include "Components/AudioComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/LightComponent.h"
#include "Components/PointLightComponent.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Components/PostProcessComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/DirectionalLight.h"
#include "Engine/Light.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Sound/SoundWaveProcedural.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
// Scenery is authored in reference units (the 110 cm r001 cell) under a root the metric scales.
constexpr float RefTop = FBoardMetric::ReferenceBoardTop;
constexpr float RefCell = FBoardMetric::ReferenceCellCm;
constexpr float EnemyThinkSeconds = 0.55f;
constexpr int32 TargetMarkers = 8;
constexpr int32 ShotPool = 4;
constexpr int32 SparkPool = 16;
// Table-top height of the mission overview; its legs stand on a floor laid over the sandbox's.
constexpr float OverviewHeight = 80.0f;

struct FPalette
{
    FLinearColor TileA, TileB, Ground, Rim, Rock, RockDark, Fog, Sun;
    float SunIntensity, SunPitch, SunYaw, FogDensity;
};

FPalette PaletteFor(EBoardTheme Theme)
{
    switch (Theme)
    {
    case EBoardTheme::Ruins:
        return {{0.42f, 0.36f, 0.28f}, {0.36f, 0.31f, 0.24f}, {0.3f, 0.26f, 0.19f}, {0.18f, 0.15f, 0.12f},
            {0.55f, 0.5f, 0.42f}, {0.4f, 0.36f, 0.3f}, {0.85f, 0.55f, 0.35f}, {1.0f, 0.72f, 0.5f}, 6.5f, -32.0f, 35.0f, 0.035f};
    case EBoardTheme::Vault:
        return {{0.17f, 0.18f, 0.24f}, {0.13f, 0.14f, 0.19f}, {0.06f, 0.06f, 0.09f}, {0.05f, 0.05f, 0.07f},
            {0.3f, 0.31f, 0.38f}, {0.2f, 0.2f, 0.26f}, {0.12f, 0.14f, 0.3f}, {0.55f, 0.6f, 1.0f}, 2.2f, -55.0f, 120.0f, 0.05f};
    default:
        return {{0.3f, 0.45f, 0.22f}, {0.26f, 0.4f, 0.19f}, {0.2f, 0.33f, 0.14f}, {0.2f, 0.15f, 0.1f},
            {0.52f, 0.52f, 0.5f}, {0.38f, 0.39f, 0.38f}, {0.55f, 0.72f, 0.9f}, {1.0f, 0.95f, 0.85f}, 7.5f, -45.0f, -30.0f, 0.02f};
    }
}

// Per-biome grade for the themed battle look (#143). Exposure is held in a tight band around a
// biome-typical scene luminance instead of adapting; the older bias values still apply on top.
struct FGrade
{
    float Vignette, Bloom, FogDensity, Luminance;
};

FGrade GradeFor(EBoardTheme Theme)
{
    switch (Theme)
    {
    case EBoardTheme::Ruins: return {0.20f, 0.22f, 0.014f, 0.16f};
    case EBoardTheme::Vault: return {0.25f, 0.28f, 0.020f, 0.09f};
    default: return {0.15f, 0.15f, 0.010f, 0.26f};
    }
}
// Exposure band half-width as a luminance ratio (about half a stop either way).
constexpr float ExposureBand = 1.41f;
// Theme cameras and shadows. The sun's shadow distance covers the farthest zoom, in reference cm.
constexpr float OverviewFieldOfView = 50.0f;
constexpr float SunShadowReach = 7000.0f;
constexpr int32 MaxShadowedLocalLights = 2;
// Fog begins past the board as seen from the default zoom, in reference cm.
constexpr float FogStartReference = 3600.0f;

// The dark-fantasy worlds carry their own baked light; the sun only adds direction and the
// fog sells depth. Ruins and the vault share the Drowned Vigil world with different grades.
FPalette ThemedPaletteFor(EBoardTheme Theme)
{
    FPalette Palette = PaletteFor(Theme);
    switch (Theme)
    {
    case EBoardTheme::Ruins:
        Palette.Sun = {0.6f, 0.68f, 1.0f}; Palette.SunIntensity = 2.4f; Palette.SunPitch = -50.0f; Palette.SunYaw = 60.0f;
        Palette.Fog = {0.1f, 0.13f, 0.26f}; Palette.FogDensity = 0.035f; break;
    case EBoardTheme::Vault:
        Palette.Sun = {0.45f, 0.5f, 0.9f}; Palette.SunIntensity = 1.3f; Palette.SunPitch = -60.0f; Palette.SunYaw = 120.0f;
        Palette.Fog = {0.05f, 0.06f, 0.14f}; Palette.FogDensity = 0.045f; break;
    default:
        Palette.Sun = {1.0f, 0.86f, 0.62f}; Palette.SunIntensity = 3.2f; Palette.SunPitch = -48.0f; Palette.SunYaw = -30.0f;
        Palette.Fog = {0.22f, 0.3f, 0.2f}; Palette.FogDensity = 0.03f; break;
    }
    return Palette;
}

// A dark room lit by candles: the sun is a faint warm fill.
FPalette OverviewPalette()
{
    FPalette Palette = PaletteFor(EBoardTheme::Vault);
    Palette.Sun = {1.0f, 0.7f, 0.45f}; Palette.SunIntensity = 0.7f; Palette.SunPitch = -62.0f; Palette.SunYaw = -60.0f;
    Palette.Fog = {0.06f, 0.035f, 0.02f}; Palette.FogDensity = 0.015f;
    return Palette;
}

// The contextual grid: how each bar style is drawn. Colours are linear and may exceed 1 (the overlay material is unlit);
// lifts are reference cm above the board top, layered so a later style never sinks under an earlier one.
struct FOverlayLook { FLinearColor Color; float Opacity; float Lift; };
FOverlayLook OverlayLook(BoardOverlay::EStyle Style)
{
    using BoardOverlay::EStyle;
    const FLinearColor Trail(0.5f, 0.95f, 1.0f);
    switch (Style)
    {
    case EStyle::MoveContour: return {FLinearColor(0.15f, 0.7f, 0.85f) * 1.5f, 0.85f, 0.6f};
    case EStyle::ReachContour: return {FLinearColor(0.35f, 0.5f, 1.0f) * 1.5f, 0.8f, 0.7f};
    case EStyle::Tick: return {FLinearColor::White, 0.16f, 0.4f};
    case EStyle::Blocked: return {FLinearColor(0.9f, 0.2f, 0.14f) * 1.3f, 0.5f, 0.8f};
    case EStyle::Ribbon: return {Trail * 1.5f, 0.85f, 1.0f};
    case EStyle::PathBracket: return {Trail * 1.7f, 0.95f, 1.1f};
    case EStyle::HoverBracket: return {FLinearColor::White * 1.5f, 0.7f, 1.1f};
    case EStyle::Victim: return {FLinearColor(1.0f, 0.82f, 0.3f) * 1.6f, 0.9f, 1.2f};
    default: return {FLinearColor::White, 0.11f, 0.3f};
    }
}
constexpr TCHAR OverlayMaterialPath[] = TEXT("/Game/DnDDiceRoll/Theme/Materials/M_BoardOverlay.M_BoardOverlay");
constexpr TCHAR EffectMaterialPath[] = TEXT("/Game/DnDDiceRoll/Theme/Materials/M_DiceRollEffect.M_DiceRollEffect");
constexpr TCHAR SilhouetteMaterialPath[] = TEXT("/Game/DnDDiceRoll/Theme/Materials/M_FigureSilhouette.M_FigureSilhouette");
constexpr int32 RingSegments = 24;

float Hash(int32 A, int32 B, int32 Salt = 0)
{
    uint32 Value = static_cast<uint32>(A) * 73856093u ^ static_cast<uint32>(B) * 19349663u ^ static_cast<uint32>(Salt) * 83492791u;
    Value = (Value ^ (Value >> 13)) * 1274126177u;
    return (Value & 0xFFFF) / 65535.0f;
}

FLinearColor Shade(const FLinearColor& Color, float Amount)
{
    return FLinearColor(Color.R * Amount, Color.G * Amount, Color.B * Amount, Color.A);
}
}

void FExpeditionPart::SetColor(const FLinearColor& Color)
{
    if (Material && !Shown.Equals(Color, 0.002f))
    {
        Material->SetVectorParameterValue(TEXT("Color"), Color);
        Shown = Color;
    }
}

AExpeditionBattleView::AExpeditionBattleView()
{
    PrimaryActorTick.bCanEverTick = true;
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("BattleCamera"));
    Camera->SetupAttachment(RootComponent);
    Camera->FieldOfView = 50.0f;
    Fog = CreateDefaultSubobject<UExponentialHeightFogComponent>(TEXT("Atmosphere"));
    Fog->SetupAttachment(RootComponent);
    Grade = CreateDefaultSubobject<UPostProcessComponent>(TEXT("Grade"));
    Grade->SetupAttachment(RootComponent);
    Grade->bUnbound = true;
    SceneryRoot = CreateDefaultSubobject<USceneComponent>(TEXT("Scenery"));
    SceneryRoot->SetupAttachment(RootComponent);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeAsset(TEXT("/Engine/BasicShapes/Cube.Cube"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderAsset(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereAsset(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> ConeAsset(TEXT("/Engine/BasicShapes/Cone.Cone"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> MaterialAsset(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
    Cube = CubeAsset.Object; Cylinder = CylinderAsset.Object; Sphere = SphereAsset.Object; Cone = ConeAsset.Object;
    BaseMaterial = MaterialAsset.Object;
    ThemeAsset = TSoftObjectPtr<UDiceRollTheme>(FSoftObjectPath(TEXT("/Game/DnDDiceRoll/Theme/DA_DiceRollTheme.DA_DiceRollTheme")));
}

FExpeditionPart AExpeditionBattleView::MakePart(UStaticMesh* Mesh, USceneComponent* Parent, FVector Location, FVector Scale, FLinearColor Color, FRotator Rotation)
{
    FExpeditionPart Part;
    ++LogicalParts;
    UStaticMeshComponent* Component = NewObject<UStaticMeshComponent>(this);
    AddInstanceComponent(Component);
    Spawned.Add(Component);
    Component->SetupAttachment(Parent ? Parent : SceneryRoot.Get());
    Component->SetMobility(EComponentMobility::Movable);
    Component->SetStaticMesh(Mesh);
    Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Component->SetRelativeLocationAndRotation(Location, Rotation);
    Component->SetRelativeScale3D(Scale);
    Part.Mesh = Component;
    Part.Base = Color;
    // Themed meshes keep their own texture; both materials take the same "Color" tint.
    UMaterialInterface* Look = BaseMaterial;
    if (ThemeMaterial && Mesh)
    {
        UMaterialInterface* Own = Mesh->GetMaterial(0);
        if (Own && Own->GetBaseMaterial() == ThemeMaterial->GetBaseMaterial()) { Look = Own; }
    }
    if (Look)
    {
        Part.Material = UMaterialInstanceDynamic::Create(Look, this);
        Component->SetMaterial(0, Part.Material);
        Part.SetColor(Color);
    }
    Component->RegisterComponent();
    return Part;
}


// ---------------------------------------------------------------------------------------
// Instanced groups (#144). One component per (mesh, material); tint and colour ride in per-instance custom data.

int32 AExpeditionBattleView::GroupFor(UStaticMesh* Mesh, UMaterialInterface* Material, bool bCastShadow, int32 CustomFloats, USceneComponent* Parent)
{
    USceneComponent* Attach = Parent ? Parent : SceneryRoot.Get();
    for (int32 Index = 0; Index < Groups.Num(); ++Index)
    {
        const FExpeditionGroup& Group = Groups[Index];
        if (!Group.bPlain && Group.Mesh == Mesh && Group.Material == Material && Group.bCastShadow == bCastShadow
            && Group.CustomFloats == CustomFloats && Group.Component && Group.Component->GetAttachParent() == Attach) { return Index; }
    }
    UInstancedStaticMeshComponent* Set = NewObject<UInstancedStaticMeshComponent>(this);
    AddInstanceComponent(Set);
    Spawned.Add(Set);
    Set->SetupAttachment(Attach);
    Set->SetMobility(EComponentMobility::Movable);
    Set->SetStaticMesh(Mesh);
    Set->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Set->SetCanEverAffectNavigation(false);
    Set->SetCastShadow(bCastShadow);
    // The count must be set before the first instance: each instance then carries that many floats.
    Set->SetNumCustomDataFloats(CustomFloats);
    if (Material) { Set->SetMaterial(0, Material); }
    Set->RegisterComponent();
    FExpeditionGroup Group;
    Group.Mesh = Mesh;
    Group.Material = Material;
    Group.Component = Set;
    Group.bCastShadow = bCastShadow;
    Group.CustomFloats = CustomFloats;
    return Groups.Add(Group);
}

int32 AExpeditionBattleView::PlainGroupFor(UStaticMesh* Mesh, const FLinearColor& Colour, bool bCastShadow)
{
    for (int32 Index = 0; Index < Groups.Num(); ++Index)
    {
        const FExpeditionGroup& Group = Groups[Index];
        if (Group.bPlain && Group.Mesh == Mesh && Group.bCastShadow == bCastShadow && Group.Colour.Equals(Colour, 0.002f)) { return Index; }
    }
    // BasicShapeMaterial reads "Color" only, so the group's one material instance carries the colour.
    UMaterialInstanceDynamic* Look = BaseMaterial ? UMaterialInstanceDynamic::Create(BaseMaterial, this) : nullptr;
    if (Look) { Look->SetVectorParameterValue(TEXT("Color"), Colour); }
    const int32 Index = GroupFor(Mesh, Look, bCastShadow, 0);
    Groups[Index].bPlain = true;
    Groups[Index].Colour = Colour;
    return Index;
}

FExpeditionInstance AExpeditionBattleView::PlaceInstance(int32 GroupIndex, const FTransform& Local, TConstArrayView<float> Data)
{
    FExpeditionInstance Result;
    if (!Groups.IsValidIndex(GroupIndex) || !Groups[GroupIndex].Component) { return Result; }
    FExpeditionGroup& Group = Groups[GroupIndex];
    Result.Group = GroupIndex;
    Result.Index = Group.Component->AddInstance(Local, false);
    if (Group.CustomFloats > 0 && Data.Num() > 0) { Group.Component->SetCustomData(Result.Index, Data, false); }
    Group.bDirty = true;
    return Result;
}

void AExpeditionBattleView::UpdateInstance(const FExpeditionInstance& Instance, const FTransform& Local, TConstArrayView<float> Data)
{
    if (!Instance.IsValid() || !Groups.IsValidIndex(Instance.Group)) { return; }
    FExpeditionGroup& Group = Groups[Instance.Group];
    if (!Group.Component) { return; }
    Group.Component->UpdateInstanceTransform(Instance.Index, Local, false, false, true);
    if (Group.CustomFloats > 0 && Data.Num() > 0) { Group.Component->SetCustomData(Instance.Index, Data, false); }
    Group.bDirty = true;
}

void AExpeditionBattleView::FlushGroups()
{
    for (FExpeditionGroup& Group : Groups)
    {
        if (!Group.bDirty) { continue; }
        Group.bDirty = false;
        // Tells the render thread once, at the end of the frame, however many instances moved.
        if (Group.Component) { Group.Component->MarkRenderInstancesDirty(); }
    }
}

FExpeditionProp AExpeditionBattleView::PlaceProp(UStaticMesh* Mesh, FVector Location, FVector Scale, FLinearColor Color, FRotator Rotation, bool bCastShadow, bool bOwnMaterial)
{
    ++LogicalProps;
    FExpeditionProp Prop;
    Prop.Local = FTransform(Rotation, Location, Scale);
    Prop.Rotation = Rotation;
    if (!bInstancedParts || !Mesh)
    {
        FExpeditionPart Part = bOwnMaterial ? MakeOwnMaterialPart(Mesh, Location, Scale, Rotation) : MakePart(Mesh, nullptr, Location, Scale, Color, Rotation);
        if (!bCastShadow && Part.Mesh) { Part.Mesh->SetCastShadow(false); }
        Scenery.Add(Part);
        Prop.Loose = Part.Mesh;
        return Prop;
    }
    ++LogicalParts;
    UMaterialInterface* Own = Mesh->GetMaterial(0);
    const bool bThemed = ThemeMaterial && Own && Own->GetBaseMaterial() == ThemeMaterial->GetBaseMaterial();
    if (bOwnMaterial)
    {
        // Cards and other props with their own material draw it as it is.
        Prop.Instance = PlaceInstance(GroupFor(Mesh, Own, bCastShadow, 0), Prop.Local, {});
    }
    else if (bThemed)
    {
        // The theme master reads its tint from custom data 0 (a grey scale: the colours are White x a brightness).
        const float Data[1] = {static_cast<float>(Color.R)};
        Prop.Instance = PlaceInstance(GroupFor(Mesh, Own, bCastShadow, 1), Prop.Local, MakeArrayView(Data, 1));
    }
    else if (EffectMaterial)
    {
        const float Data[3] = {Color.R, Color.G, Color.B};
        Prop.Instance = PlaceInstance(GroupFor(Mesh, EffectMaterial, bCastShadow, 3), Prop.Local, MakeArrayView(Data, 3));
    }
    else
    {
        Prop.Instance = PlaceInstance(PlainGroupFor(Mesh, Color, bCastShadow), Prop.Local, {});
    }
    return Prop;
}

FExpeditionPart AExpeditionBattleView::PlaceOccluder(UStaticMesh* Mesh, FVector Location, FVector Scale)
{
    if (!bInstancedParts)
    {
        ++LogicalProps;
        FExpeditionPart Loose = MakeOwnMaterialPart(Mesh, Location, Scale, FRotator::ZeroRotator);
        Scenery.Add(Loose);
        return Loose;
    }
    ++LogicalProps;
    ++LogicalParts;
    FExpeditionPart Part;
    UMaterialInterface* Own = Mesh ? Mesh->GetMaterial(0) : nullptr;
    // The sight lines are pushed to this one instance's material every frame, so the occluder keeps a material instance of its own.
    UMaterialInstanceDynamic* Look = Own ? UMaterialInstanceDynamic::Create(Own, this) : nullptr;
    const int32 Group = GroupFor(Mesh, Look ? Look : Own, true, 0);
    PlaceInstance(Group, FTransform(FRotator::ZeroRotator, Location, Scale), {});
    Part.Mesh = Groups[Group].Component;
    Part.Material = Look;
    return Part;
}

FVector AExpeditionBattleView::PropActorPosition(const FExpeditionProp& Prop) const
{
    if (Prop.Loose) { return GetActorTransform().InverseTransformPosition(Prop.Loose->GetComponentLocation()); }
    return GetActorTransform().InverseTransformPosition(SceneryRoot->GetComponentTransform().TransformPosition(Prop.Local.GetLocation()));
}

void AExpeditionBattleView::AddSway(const FExpeditionProp& Prop, float Degrees, float Flicker)
{
    if (Prop.Loose) { AddSway(Prop.Loose, Degrees, Flicker); return; }
    if (!Prop.Instance.IsValid()) { return; }
    FExpeditionSway Sway;
    Sway.Instance = Prop.Instance;
    Sway.Location = Prop.Local.GetLocation();
    Sway.Base = Prop.Rotation;
    Sway.Scale = Prop.Local.GetScale3D();
    Sway.Phase = Hash(Swaying.Num(), 17) * 2.0f * PI;
    Sway.Degrees = Degrees;
    Sway.Flicker = Flicker;
    Swaying.Add(Sway);
}

FExpeditionSlot AExpeditionBattleView::MakeSlot(UStaticMesh* Mesh, FVector Location, FVector Scale, FLinearColor Color, FRotator Rotation, bool bCastShadow)
{
    FExpeditionSlot Slot;
    Slot.Location = Location; Slot.Rotation = Rotation; Slot.Scale = Scale; Slot.Colour = Color;
    Slot.bVisible = true; Slot.bPlaced = true;
    if (!bInstancedEffects || !Mesh)
    {
        Slot.Loose = MakePart(Mesh, nullptr, Location, Scale, Color, Rotation);
        if (Slot.Loose.Mesh) { Slot.Loose.Mesh->SetCastShadow(bCastShadow); }
        return Slot;
    }
    ++LogicalParts;
    const float Data[3] = {Color.R, Color.G, Color.B};
    Slot.Instance = PlaceInstance(GroupFor(Mesh, EffectMaterial, bCastShadow, 3), FTransform(Rotation, Location, Scale), MakeArrayView(Data, 3));
    return Slot;
}

void AExpeditionBattleView::SetSlot(FExpeditionSlot& Slot, bool bVisible, const FVector& Location, const FRotator& Rotation, const FVector& Scale, const FLinearColor& Color)
{
    if (!Slot.Instance.IsValid())
    {
        UStaticMeshComponent* Mesh = Slot.Loose.Mesh;
        if (!Mesh) { return; }
        Mesh->SetVisibility(bVisible);
        if (!bVisible) { return; }
        Mesh->SetRelativeLocationAndRotation(Location, Rotation);
        Mesh->SetRelativeScale3D(Scale);
        Slot.Loose.SetColor(Color);
        return;
    }
    const bool bMoved = !Slot.bPlaced || Slot.bVisible != bVisible
        || (bVisible && (!Location.Equals(Slot.Location, 0.001) || !Rotation.Equals(Slot.Rotation, 0.001) || !Scale.Equals(Slot.Scale, 0.00001)));
    const bool bTinted = !Slot.bPlaced || (bVisible && !Color.Equals(Slot.Colour, 0.002f));
    if (!bMoved && !bTinted) { return; }
    if (bVisible) { Slot.Location = Location; Slot.Rotation = Rotation; Slot.Scale = Scale; Slot.Colour = Color; }
    // A hidden piece collapses to a point where it last stood.
    const FTransform Local = bVisible ? FTransform(Rotation, Location, Scale) : FTransform(Slot.Rotation, Slot.Location, FVector::ZeroVector);
    const float Data[3] = {Slot.Colour.R, Slot.Colour.G, Slot.Colour.B};
    UpdateInstance(Slot.Instance, Local, bTinted ? MakeArrayView(Data, 3) : TConstArrayView<float>());
    Slot.bVisible = bVisible;
    Slot.bPlaced = true;
}

bool AExpeditionBattleView::CanBankRings() const
{
    return bInstancedParts && OverlayMaterial && Cube;
}

void AExpeditionBattleView::EnsureBank()
{
    if (RingGroup != INDEX_NONE) { return; }
    // The bank sits in actor space (unit rings do not tilt with the scenery root); markers convert on the way in. Its material
    // instance is neutral (Color white, Opacity 1): each bar's rgb and opacity are the four custom floats (board-overlay-3).
    UMaterialInstanceDynamic* Look = UMaterialInstanceDynamic::Create(OverlayMaterial, this);
    Look->SetVectorParameterValue(TEXT("Color"), FLinearColor::White);
    Look->SetScalarParameterValue(TEXT("Opacity"), 1.0f);
    RingGroup = GroupFor(Cube, Look, false, 4, RootComponent);
    if (Groups.IsValidIndex(RingGroup) && Groups[RingGroup].Component)
    {
        RingBank = Groups[RingGroup].Component;
        RingBank->bReceivesDecals = false;
    }
}

int32 AExpeditionBattleView::AddRingSlot(const FLinearColor& Color)
{
    EnsureBank();
    ++LogicalParts;
    FExpeditionRingState State;
    State.Colour = Color;
    const int32 Slot = RingStates.Add(State);
    // Every bar starts collapsed; SetRing places them. The colour is the bank's per-instance rgb.
    const FTransform Hidden(FRotator::ZeroRotator, FVector::ZeroVector, FVector::ZeroVector);
    const float Data[4] = {Color.R, Color.G, Color.B, 1.0f};
    for (int32 Bar = 0; Bar < RingSegments; ++Bar) { PlaceInstance(RingGroup, Hidden, MakeArrayView(Data, 4)); }
    return Slot;
}

void AExpeditionBattleView::SetRing(int32 Slot, bool bVisible, const FVector& Centre, float Scale, const FLinearColor& Color)
{
    if (!RingStates.IsValidIndex(Slot) || !RingBank) { return; }
    FExpeditionRingState& State = RingStates[Slot];
    const bool bMoved = !State.bPlaced || State.bVisible != bVisible
        || (bVisible && (!State.Centre.Equals(Centre, 0.01) || !FMath::IsNearlyEqual(State.Scale, Scale, 0.00001f)));
    const bool bTinted = !State.bPlaced || !State.Colour.Equals(Color, 0.002f);
    if (!bMoved && !bTinted) { return; }
    const int32 Start = Slot * RingSegments;
    if (bMoved)
    {
        // A ring 100 cm across at unit scale: a polygon of thin bars, slightly overlapped so no gap shows (the same as the loose ring).
        TArray<FTransform, TInlineAllocator<RingSegments>> Bars;
        const float Side = 100.0f * FMath::Sin(PI / RingSegments);
        for (int32 Index = 0; Index < RingSegments; ++Index)
        {
            const float Angle = (Index + 0.5f) * 2.0f * PI / RingSegments;
            Bars.Add(bVisible
                ? FTransform(FRotator(0.0f, FMath::RadiansToDegrees(Angle) + 90.0f, 0.0f),
                    Centre + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f) * 50.0f * Scale,
                    FVector(Side * 1.1f / 100.0f, 0.05f, 0.012f) * Scale)
                : FTransform(FRotator::ZeroRotator, State.bPlaced ? State.Centre : Centre, FVector::ZeroVector));
        }
        RingBank->BatchUpdateInstancesTransforms(Start, TArrayView<const FTransform>(Bars.GetData(), Bars.Num()), false, false, true);
    }
    if (bTinted)
    {
        float Data[RingSegments * 4];
        for (int32 Index = 0; Index < RingSegments; ++Index)
        { Data[Index * 4] = Color.R; Data[Index * 4 + 1] = Color.G; Data[Index * 4 + 2] = Color.B; Data[Index * 4 + 3] = 1.0f; }
        RingBank->SetCustomData(Start, Start + RingSegments - 1, TConstArrayView<float>(Data, RingSegments * 4), false);
    }
    if (bVisible) { State.Centre = Centre; State.Scale = Scale; }
    State.Colour = Color;
    State.bVisible = bVisible;
    State.bPlaced = true;
    if (Groups.IsValidIndex(RingGroup)) { Groups[RingGroup].bDirty = true; }
}

void AExpeditionBattleView::SetOverlayBars(const TArray<FTransform>& Bars, const TArray<float>& Data)
{
    if (!RingBank || OverlayBase == INDEX_NONE) { return; }
    const int32 Count = Bars.Num();
    const int32 Have = RingBank->GetInstanceCount() - OverlayBase;
    // The bank only grows: bars beyond the current count are added once, and a smaller grid collapses what is left over.
    if (Count > Have)
    {
        TArray<FTransform> More;
        More.Append(Bars.GetData() + Have, Count - Have);
        RingBank->AddInstances(More, false, false, false);
    }
    if (Count > 0)
    {
        RingBank->BatchUpdateInstancesTransforms(OverlayBase, TArrayView<const FTransform>(Bars.GetData(), Count), false, false, true);
        RingBank->SetCustomData(OverlayBase, OverlayBase + Count - 1, TConstArrayView<float>(Data.GetData(), Data.Num()), false);
    }
    const int32 Left = RingBank->GetInstanceCount() - OverlayBase - Count;
    if (Left > 0) { RingBank->BatchUpdateInstancesTransform(OverlayBase + Count, Left, FTransform(FRotator::ZeroRotator, FVector::ZeroVector, FVector::ZeroVector), false, false, true); }
    if (Groups.IsValidIndex(RingGroup)) { Groups[RingGroup].bDirty = true; }
}

int32 AExpeditionBattleView::PresentationComponentCount() const
{
    int32 Count = 0;
    for (const TObjectPtr<UActorComponent>& Component : Spawned) { if (Cast<UPrimitiveComponent>(Component.Get())) { ++Count; } }
    return Count;
}

void AExpeditionBattleView::SetLooseParts(bool bLoose)
{
    bLooseParts = bLoose;
    RebuildBoard();
}

UPointLightComponent* AExpeditionBattleView::MakeLight(USceneComponent* Parent, FVector Location, FLinearColor Color, float Intensity, float Radius)
{
    UPointLightComponent* Light = NewObject<UPointLightComponent>(this);
    AddInstanceComponent(Light);
    Spawned.Add(Light);
    Light->SetupAttachment(Parent ? Parent : SceneryRoot.Get());
    Light->SetMobility(EComponentMobility::Movable);
    Light->SetRelativeLocation(Location);
    Light->SetLightColor(Color);
    Light->SetIntensityUnits(ELightUnits::Candelas);
    Light->SetIntensity(Intensity);
    // Radii are authored in reference cm like every other length; stretch them with the board.
    Light->SetAttenuationRadius(Radius * SceneScale());
    Light->SetCastShadows(false);
    Light->RegisterComponent();
    return Light;
}

void AExpeditionBattleView::BeginPlay()
{
    Super::BeginPlay();
    FString SaveRoot;
    if (!FParse::Value(FCommandLine::Get(), TEXT("ExpeditionSaveRoot="), SaveRoot))
    { SaveRoot = FPlatformMisc::GetEnvironmentVariable(TEXT("EXPEDITION_SAVE_ROOT")); }
    if (!SaveRoot.IsEmpty()) { ConfigureStorage(FPaths::Combine(SaveRoot, TEXT("current-v1.json"))); }
    // The starter fixture has no baked lighting. Runtime presentation uses dynamic lights.
    for (TActorIterator<ALight> Light(GetWorld()); Light; ++Light)
    {
        if (ULightComponent* Component = Light->GetLightComponent()) { Component->SetMobility(EComponentMobility::Movable); }
    }
    // -ExpeditionStaticFigures draws the static r001 figures instead of the rigged characters.
    if (FParse::Param(FCommandLine::Get(), TEXT("ExpeditionStaticFigures"))) { bSkeletalFigures = false; }
    // -ExpeditionLooseParts draws every themed part as its own component, as before #144 (for A/B profiling on one build).
    if (FParse::Param(FCommandLine::Get(), TEXT("ExpeditionLooseParts"))) { bLooseParts = true; }
    // -ExpeditionPrimitives keeps the engine-primitive presentation for comparison.
    if (!FParse::Param(FCommandLine::Get(), TEXT("ExpeditionPrimitives")) && !ThemeAsset.IsNull())
    {
        Theme = ThemeAsset.LoadSynchronous();
        ThemeMaterial = Theme && !Theme->TintMaterial.IsNull() ? Theme->TintMaterial.LoadSynchronous() : nullptr;
        if (!ThemeMaterial) { Theme = nullptr; }
        OverviewTable = Theme ? UDiceRollTheme::Load(Theme->Tabletop) : nullptr;
    }
    // The translucent material for the contextual grid and (themed) footprint rings is optional; without it they draw as
    // dimmed opaque lines. Created by Content/Python/create_board_overlay_material.py.
    OverlayMaterial = LoadObject<UMaterialInterface>(nullptr, OverlayMaterialPath, nullptr, LOAD_Quiet | LOAD_NoWarn);
    // The lit material that reads its colour from per-instance custom data (sparks, shots, camp, waystone) is optional too (#144):
    // without it those pieces stay loose components.
    EffectMaterial = LoadObject<UMaterialInterface>(nullptr, EffectMaterialPath, nullptr, LOAD_Quiet | LOAD_NoWarn);
    AttachSilhouette();
    // The theme owns the cell size and figure height; no theme means the same defaults.
    BoardMetric = Theme ? FBoardMetric(Theme->CellSize, Theme->FigureHeight) : FBoardMetric();
    Show.OnCue = [this](EExpeditionCue Cue) { PlayCue(Cue); };
    Show.OnShake = [this](float Amount) { ShakeAmount = FMath::Max(ShakeAmount, Amount); };
    Feedback.Add(TEXT("Welcome. Start a new expedition or continue your saved one."));
    bUnfinishedCheckpoint = Session.HasUnfinishedCheckpoint();
    RebuildBoard();
}

bool AExpeditionBattleView::InCombat() const
{
    return HasRun() && Progress().Phase == EExpeditionPhase::Combat && !bShowRecords;
}

FString AExpeditionBattleView::LocationName() const
{
    if (!HasRun() || Progress().History.IsEmpty()) { return TEXT("The trailhead"); }
    return Progress().History.Last().Route.Name;
}

void AExpeditionBattleView::ClearPresentation()
{
    for (UActorComponent* Component : Spawned)
    {
        if (IsValid(Component)) { RemoveInstanceComponent(Component); Component->DestroyComponent(); }
    }
    Spawned.Reset();
    Tiles.Reset(); OverlaySets.Reset(); OverlayCounts.Reset(); OverlayState = BoardOverlay::FMask(); OverlayInput = FBoardHighlights(); OverlayVictimsShown.Reset();
    bOverlayBuilt = false; bFullGridShown = false;
    Rigs.Reset(); Scenery.Reset(); Flames.Reset(); FlameStrength.Reset(); Markers.Reset();
    ShotSlots.Reset(); ShotLights.Reset(); SparkSlots.Reset(); CampSlots.Reset(); WaystoneSlots.Reset();
    Groups.Reset(); RingBank = nullptr; RingGroup = INDEX_NONE; OverlayBase = INDEX_NONE; RingStates.Reset();
    LogicalParts = 0; LogicalProps = 0;
    CampLight = nullptr; WaystoneLight = nullptr;
    Swaying.Reset();
    Occluder = FExpeditionPart(); bOccluderBoardSent = false; Dressing.Reset(); VariantsByCell.Reset(); SpotsByCell.Reset();
    bVigilKit = false; ObstaclePieces = 0; ArchPieces = 0;
}

void AExpeditionBattleView::RebuildBoard()
{
    ++Revision;
    ClearPresentation();
    // Themed boards draw their static parts, rings and effects from shared instanced groups (#144); the primitive look never does.
    bInstancedParts = Theme && !bLooseParts;
    bInstancedEffects = bInstancedParts && EffectMaterial;
    const FCombatEncounter& Encounter = Rules().Snapshot().Encounter;
    int32 Recipe = 0, Seed = 7;
    BoardTheme = EBoardTheme::Meadow;
    if (HasRun() && !Progress().History.IsEmpty())
    {
        const FExpeditionRoute& Route = Progress().History.Last().Route;
        Recipe = Route.Recipe;
        Seed = Route.EncounterSeed;
        BoardTheme = Progress().History.Num() >= FExpeditionRun::EncounterCount ? EBoardTheme::Vault : Route.bDangerous ? EBoardTheme::Ruins : EBoardTheme::Meadow;
    }
    bOverview = WantsOverview();
    SceneryRoot->SetRelativeScale3D(FVector(SceneScale()));
    ApplyTheme();
    if (bOverview) { BuildOverview(); }
    else
    {
        BuildScenery(Encounter, Recipe, Seed);
        for (const FCombatantState& Unit : Encounter.Combatants) { BuildRig(Unit); }
    }
    BuildEffects(Encounter);
    SelectShadowLights();
    BuildOverlay();
    Show.Reset(Rules().Snapshot());
    // Frame the whole board at the start. The offsets count cells beyond the original 8 x 7 board;
    // the 70-per-cell pullback (formerly 90) keeps units readable on 16-18 wide boards.
    if (bOverview) { TargetYaw = -90.0f; }
    TargetDistance = FramedDistance(TargetYaw);
    bFramed = !bOverview;
    PanOffset = FVector2D::ZeroVector;
    LastFocusId = INDEX_NONE;
    bCameraPlaced = false;
    UpdateRigs(); UpdateTiles(); UpdateOverlay(); UpdateMarkers(); UpdateEffects(); UpdateCamera(0.0f); UpdateOccluder(); FlushGroups();
}

float AExpeditionBattleView::FramedDistance() const
{
    if (bOverview) { return 300.0f; }
    // The distance at which the board's corners fit the screen area the HUD leaves free (see CameraRig.h); it anchors the pitch ramp.
    const FCombatEncounter& Encounter = Rules().Snapshot().Encounter;
    if (FramedCache.Width != Encounter.Width || FramedCache.Height != Encounter.Height || FramedCache.CellCm != BoardMetric.CellCm || !(FramedCache.Rect == Safe))
    {
        const ExpeditionCameraRig::FFit Home = ExpeditionCameraRig::FitDistance(Encounter.Width, Encounter.Height, BoardMetric, ExpeditionCameraRig::HomeYaw, 0.0f, Safe);
        FramedCache.Width = Encounter.Width; FramedCache.Height = Encounter.Height; FramedCache.CellCm = BoardMetric.CellCm; FramedCache.Rect = Safe;
        FramedCache.Anchor = FMath::Clamp(Home.Distance, ExpeditionCameraRig::MinFramed, ExpeditionCameraRig::MaxFramed);
    }
    return FramedCache.Anchor;
}

float AExpeditionBattleView::FramedDistance(float ForYaw) const
{
    if (bOverview) { return 300.0f; }
    const FCombatEncounter& Encounter = Rules().Snapshot().Encounter;
    const float Anchor = FramedDistance();
    return FMath::Clamp(ExpeditionCameraRig::FitDistance(Encounter.Width, Encounter.Height, BoardMetric, ForYaw, Anchor, Safe).Distance,
        ExpeditionCameraRig::MinDistance, ExpeditionCameraRig::MaxDistance);
}

bool AExpeditionBattleView::FramedFits(float ForYaw) const
{
    if (bOverview) { return true; }
    const FCombatEncounter& Encounter = Rules().Snapshot().Encounter;
    return ExpeditionCameraRig::FitBoard(Encounter.Width, Encounter.Height, BoardMetric, ForYaw, Safe).bFits;
}

bool AExpeditionBattleView::AtFramedZoom() const
{
    return !bOverview && HasRun() && FMath::IsNearlyEqual(TargetDistance, FramedDistance(TargetYaw), 1.0f);
}

void AExpeditionBattleView::SnapCamera(float InYaw, float InDistance)
{
    if (bOverview) { return; }
    TargetYaw = Yaw = InYaw;
    TargetDistance = Distance = FMath::Clamp(InDistance, ExpeditionCameraRig::MinDistance, ExpeditionCameraRig::MaxDistance);
    PanOffset = FVector2D::ZeroVector;
    bCameraPlaced = false;
    bFramed = AtFramedZoom();
    UpdateCamera(0.0f);
}

FTransform AExpeditionBattleView::CameraTransform() const { return Camera->GetComponentTransform(); }
float AExpeditionBattleView::CameraFieldOfView() const { return Camera->FieldOfView; }

void AExpeditionBattleView::SetBoardMetric(const FBoardMetric& Value)
{
    BoardMetric = FBoardMetric(Value.CellCm, Value.FigureCm);
    RebuildBoard();
}

void AExpeditionBattleView::UseStaticFigures()
{
    bSkeletalFigures = false;
    RebuildBoard();
}

int32 AExpeditionBattleView::SkeletalFigureCount() const
{
    int32 Count = 0;
    for (const FExpeditionUnitRig& Rig : Rigs) { Count += Rig.Skin ? 1 : 0; }
    return Count;
}

FUnitAnimationState AExpeditionBattleView::UnitAnimation(int32 UnitId) const
{
    FUnitAnimationState State;
    for (const FExpeditionUnitRig& Rig : Rigs)
    {
        if (Rig.Id != UnitId || !Rig.Skin) { continue; }
        State.bSkeletal = true;
        State.Pose = Rig.Pose;
        State.Time = Rig.PoseTime;
        const UAnimSequence* Clip = Rig.Clips.IsValidIndex(static_cast<int32>(Rig.Pose)) ? Rig.Clips[static_cast<int32>(Rig.Pose)].Get() : nullptr;
        State.Length = Clip ? Clip->GetPlayLength() : 0.0f;
        const UAnimSingleNodeInstance* Node = Rig.Skin->GetSingleNodeInstance();
        const UAnimationAsset* Shown = Node ? Node->GetAnimationAsset() : nullptr;
        State.Clip = Shown ? Shown->GetName() : FString();
        break;
    }
    return State;
}

void AExpeditionBattleView::UsePrimitives()
{
    Theme = nullptr; ThemeMaterial = nullptr; OverviewTable = nullptr;
    BoardMetric = FBoardMetric();
    RebuildBoard();
}

void AExpeditionBattleView::PrepareExposureForCapture()
{
    FPostProcessSettings& Settings = Grade->Settings;
    Settings.bOverride_AutoExposureSpeedUp = true;
    Settings.bOverride_AutoExposureSpeedDown = true;
    Settings.AutoExposureSpeedUp = 20.0f;
    Settings.AutoExposureSpeedDown = 20.0f;
}

void AExpeditionBattleView::ApplyTheme()
{
    const FPalette Palette = bOverview ? OverviewPalette() : Theme ? ThemedPaletteFor(BoardTheme) : PaletteFor(BoardTheme);
    for (TActorIterator<ADirectionalLight> Sun(GetWorld()); Sun; ++Sun)
    {
        if (ULightComponent* Light = Sun->GetLightComponent())
        {
            Light->SetMobility(EComponentMobility::Movable);
            Light->SetLightColor(Palette.Sun);
            Light->SetIntensity(Palette.SunIntensity);
            Sun->SetActorRotation(FRotator(Palette.SunPitch, Palette.SunYaw, 0.0f));
            // The key light casts cascaded shadows on the battle board; the tabletop keeps what the map gave it.
            if (!StarterSunShadows.IsSet()) { StarterSunShadows = Light->CastShadows; }
            Light->SetCastShadows(bOverview ? StarterSunShadows.GetValue() : true);
            if (UDirectionalLightComponent* Key = Cast<UDirectionalLightComponent>(Light); Key && !bOverview)
            {
                Key->DynamicShadowDistanceMovableLight = SunShadowReach * BoardMetric.Scale();
                Key->DynamicShadowCascades = 4;
                Key->CascadeDistributionExponent = 3.0f;
            }
        }
    }
    // Fog is tuned in reference cm; stretch it with the board so the look matches at any cell size.
    const float FogScale = bOverview ? 1.0f : BoardMetric.Scale();
    const FGrade Biome = GradeFor(BoardTheme);
    const bool bBattleLook = Theme && !bOverview;
    Fog->SetFogDensity((bBattleLook ? Biome.FogDensity : Palette.FogDensity) / FogScale);
    Fog->SetFogHeightFalloff(0.08f);
    Fog->SetFogInscatteringColor(Palette.Fog);
    Fog->SetStartDistance((bBattleLook ? FogStartReference : 1400.0f) * FogScale);
    Fog->SetWorldLocation(FVector(0, 0, -200));
    FPostProcessSettings& Settings = Grade->Settings;
    Settings.bOverride_VignetteIntensity = true;
    Settings.VignetteIntensity = BoardTheme == EBoardTheme::Vault ? 0.75f : 0.5f;
    Settings.bOverride_ColorSaturation = true;
    Settings.ColorSaturation = FVector4(1.12f, 1.12f, 1.12f, 1.0f);
    Settings.bOverride_ColorContrast = true;
    Settings.ColorContrast = FVector4(1.06f, 1.06f, 1.06f, 1.0f);
    Settings.bOverride_BloomIntensity = true;
    Settings.BloomIntensity = 0.9f;
    // Auto exposure brightened the mid-tone palette into pastels; pull it back to the authored colors.
    Settings.bOverride_AutoExposureBias = true;
    Settings.AutoExposureBias = BoardTheme == EBoardTheme::Vault ? -1.7f : BoardTheme == EBoardTheme::Meadow ? -0.6f : -1.0f;
    if (Theme)
    {
        // Darker, cooler and less saturated than the primitive look; candles carry the warmth.
        Settings.ColorSaturation = FVector4(0.95f, 0.95f, 0.95f, 1.0f);
        Settings.ColorContrast = FVector4(1.12f, 1.12f, 1.12f, 1.0f);
        Settings.VignetteIntensity = bOverview ? 0.85f : 0.6f;
        Settings.AutoExposureBias = bOverview ? -0.8f : BoardTheme == EBoardTheme::Vault ? -1.4f : -1.0f;
        Settings.BloomIntensity = 0.45f;
    }
    if (bBattleLook)
    {
        Settings.VignetteIntensity = Biome.Vignette;
        Settings.BloomIntensity = Biome.Bloom;
        // Fixed exposure: a tight luminance band, so the grade does not breathe as the camera moves.
        Settings.bOverride_AutoExposureMinBrightness = true;
        Settings.bOverride_AutoExposureMaxBrightness = true;
        Settings.AutoExposureMinBrightness = Biome.Luminance / ExposureBand;
        Settings.AutoExposureMaxBrightness = Biome.Luminance * ExposureBand;
        // No grain, motion blur or depth of field.
        Settings.bOverride_FilmGrainIntensity = true;
        Settings.FilmGrainIntensity = 0.0f;
        Settings.bOverride_MotionBlurAmount = true;
        Settings.MotionBlurAmount = 0.0f;
        Settings.bOverride_DepthOfFieldFocalDistance = true;
        Settings.DepthOfFieldFocalDistance = 0.0f;
    }
}

void AExpeditionBattleView::BuildScenery(const FCombatEncounter& Encounter, int32 Recipe, int32 Seed)
{
    if (Theme && BuildThemedScenery(Encounter, Recipe, Seed)) { return; }
    const FPalette Palette = PaletteFor(BoardTheme);
    const FVector Center = SceneBoard(FVector2D((Encounter.Width - 1) * 0.5f, (Encounter.Height - 1) * 0.5f));
    // Ground hides the starter floor; a raised slab makes the board read as a tabletop.
    Scenery.Add(MakePart(Cube, nullptr, FVector(Center.X, Center.Y, 3.0f), FVector(60.0f, 60.0f, 0.06f), Palette.Ground));
    Scenery.Add(MakePart(Cube, nullptr, FVector(Center.X, Center.Y, 12.0f),
        FVector(Encounter.Width * RefCell / 100.0f + 0.35f, Encounter.Height * RefCell / 100.0f + 0.35f, 0.24f), Palette.Rim));
    for (int32 Y = 0; Y < Encounter.Height; ++Y)
    {
        for (int32 X = 0; X < Encounter.Width; ++X)
        {
            const FIntPoint Cell(X, Y);
            const float Jitter = 0.88f + 0.22f * Hash(X, Y, Seed);
            const FLinearColor Color = Shade((X + Y) % 2 == 0 ? Palette.TileA : Palette.TileB, Jitter);
            const FVector Position = SceneBoard(FVector2D(Cell.X, Cell.Y));
            FExpeditionTile Tile;
            Tile.Cell = Cell;
            Tile.Part = MakePart(Cube, nullptr, FVector(Position.X, Position.Y, RefTop - 9.0f + Hash(Y, X, 3) * 2.0f),
                FVector(1.04f, 1.04f, 0.18f), Color);
            Tiles.Add(Tile);
        }
    }
    FRandomStream Random(Seed);
    for (const FIntPoint& Cell : Encounter.BlockedCells) { BuildObstacle(Cell, HasRun() && !Progress().History.IsEmpty() ? Recipe : 0, Random); }
    BuildDecor(Encounter, Random);
}

void AExpeditionBattleView::BuildObstacle(FIntPoint Cell, int32 Recipe, FRandomStream& Random)
{
    const FPalette Palette = PaletteFor(BoardTheme);
    const FVector Base = SceneBoard(FVector2D(Cell.X, Cell.Y));
    const FVector Floor(Base.X, Base.Y, RefTop);
    auto Rubble = [&](int32 Count)
    {
        for (int32 Index = 0; Index < Count; ++Index)
        {
            const float Size = Random.FRandRange(0.1f, 0.2f);
            Scenery.Add(MakePart(Sphere, nullptr, Floor + FVector(Random.FRandRange(-42, 42), Random.FRandRange(-42, 42), 4),
                FVector(Size, Size * 1.2f, Size * 0.6f), Shade(Palette.RockDark, Random.FRandRange(0.8f, 1.1f))));
        }
    };
    if (Recipe == 1)
    {
        // Ruined pillar: plinth, shaft of uneven height and a fallen capital.
        const float Height = Random.FRandRange(1.0f, 1.6f);
        Scenery.Add(MakePart(Cube, nullptr, Floor + FVector(0, 0, 8), FVector(0.85f, 0.85f, 0.16f), Palette.RockDark));
        Scenery.Add(MakePart(Cylinder, nullptr, Floor + FVector(0, 0, 16 + Height * 50), FVector(0.5f, 0.5f, Height), Palette.Rock));
        Scenery.Add(MakePart(Cube, nullptr, Floor + FVector(0, 0, 18 + Height * 100), FVector(0.62f, 0.62f, 0.12f), Shade(Palette.Rock, 1.1f),
            FRotator(Random.FRandRange(-8, 8), Random.FRandRange(0, 90), Random.FRandRange(-8, 8))));
        Rubble(3);
    }
    else if (Recipe == 2)
    {
        // Broken wall segment with a ragged top.
        Scenery.Add(MakePart(Cube, nullptr, Floor + FVector(0, 0, 45), FVector(1.08f, 0.42f, 0.9f), Palette.Rock));
        for (int32 Block = 0; Block < 3; ++Block)
        {
            const float Height = Random.FRandRange(0.12f, 0.4f);
            Scenery.Add(MakePart(Cube, nullptr, Floor + FVector(-36 + Block * 36, 0, 90 + Height * 50), FVector(0.34f, 0.4f, Height),
                Shade(Palette.Rock, Random.FRandRange(0.85f, 1.1f))));
        }
        Rubble(4);
    }
    else
    {
        // Split stones: a cluster of weathered boulders.
        for (int32 Stone = 0; Stone < 3; ++Stone)
        {
            const float Size = Stone == 0 ? Random.FRandRange(0.8f, 0.95f) : Random.FRandRange(0.4f, 0.6f);
            const FVector Offset = Stone == 0 ? FVector(0, 0, 0) : FVector(Random.FRandRange(-30, 30), Random.FRandRange(-30, 30), 0);
            Scenery.Add(MakePart(Sphere, nullptr, Floor + Offset + FVector(0, 0, Size * 32), FVector(Size, Size * 0.9f, Size * 0.8f),
                Shade(Stone == 0 ? Palette.Rock : Palette.RockDark, Random.FRandRange(0.9f, 1.1f)),
                FRotator(Random.FRandRange(-10, 10), Random.FRandRange(0, 360), 0)));
        }
        Rubble(2);
    }
}

void AExpeditionBattleView::BuildDecor(const FCombatEncounter& Encounter, FRandomStream& Random)
{
    const FPalette Palette = PaletteFor(BoardTheme);
    const float W = Encounter.Width, H = Encounter.Height;
    auto Ground = [this](float X, float Y) { return SceneBoard(FVector2D(X, Y), -RefTop + 6.0f); };
    auto Tree = [&](float X, float Y, float Size)
    {
        const FVector At = Ground(X, Y);
        Scenery.Add(MakePart(Cylinder, nullptr, At + FVector(0, 0, 60 * Size), FVector(0.22f, 0.22f, 1.2f) * Size, FLinearColor(0.28f, 0.18f, 0.1f)));
        const FLinearColor Leaf = Shade(FLinearColor(0.16f, 0.36f, 0.14f), Random.FRandRange(0.8f, 1.15f));
        Scenery.Add(MakePart(Cone, nullptr, At + FVector(0, 0, 150 * Size), FVector(1.3f, 1.3f, 1.3f) * Size, Leaf));
        Scenery.Add(MakePart(Cone, nullptr, At + FVector(0, 0, 215 * Size), FVector(0.95f, 0.95f, 1.0f) * Size, Shade(Leaf, 1.1f)));
    };
    auto Boulder = [&](float X, float Y, float Size)
    {
        Scenery.Add(MakePart(Sphere, nullptr, Ground(X, Y) + FVector(0, 0, 20 * Size), FVector(Size, Size * 0.85f, Size * 0.6f),
            Shade(Palette.Rock, Random.FRandRange(0.75f, 1.0f)), FRotator(0, Random.FRandRange(0, 360), 0)));
    };
    auto Tuft = [&](float X, float Y, const FLinearColor& Color)
    {
        Scenery.Add(MakePart(Cone, nullptr, Ground(X, Y) + FVector(0, 0, 10), FVector(0.14f, 0.14f, 0.24f), Shade(Color, Random.FRandRange(0.8f, 1.2f))));
    };
    auto Torch = [&](FVector At, float Brightness)
    {
        Scenery.Add(MakePart(Cylinder, nullptr, At, FVector(0.07f, 0.07f, 0.5f), FLinearColor(0.2f, 0.13f, 0.08f)));
        Scenery.Add(MakePart(Sphere, nullptr, At + FVector(0, 0, 30), FVector(0.16f, 0.16f, 0.22f), FLinearColor(1.0f, 0.55f, 0.15f) * 4.0f));
        Flames.Add(MakeLight(nullptr, At + FVector(0, 0, 45), FLinearColor(1.0f, 0.55f, 0.25f), Brightness, 900.0f));
    };
    // Scatter outside the board; the near (camera) side only gets low details.
    auto Spot = [&](float& X, float& Y, bool& bNear)
    {
        for (int32 Try = 0; Try < 12; ++Try)
        {
            X = Random.FRandRange(-6.0f, W + 5.0f);
            Y = Random.FRandRange(-6.0f, H + 5.0f);
            if (X > -1.8f && X < W + 0.8f && Y > -1.8f && Y < H + 0.8f) { continue; }
            bNear = Y < -1.0f;
            return true;
        }
        return false;
    };
    if (BoardTheme == EBoardTheme::Meadow)
    {
        // A stream along the far edge with stepping stones.
        const FVector Stream = Ground((W - 1) * 0.5f, H + 2.2f);
        Scenery.Add(MakePart(Cube, nullptr, Stream + FVector(0, 0, -1), FVector((W + 18) * RefCell / 100.0f, 1.5f, 0.04f), FLinearColor(0.12f, 0.32f, 0.5f)));
        Scenery.Add(MakePart(Cube, nullptr, Stream + FVector(0, -55, 0), FVector((W + 18) * RefCell / 100.0f, 0.25f, 0.05f), Shade(Palette.Rim, 1.4f)));
        for (int32 Stone = 0; Stone < 4; ++Stone) { Boulder((W - 1) * 0.5f - 1.5f + Stone, H + 2.2f + Random.FRandRange(-0.2f, 0.2f), 0.35f); }
        for (int32 Index = 0; Index < 46; ++Index)
        {
            float X, Y; bool bNear;
            if (!Spot(X, Y, bNear)) { continue; }
            const float Roll = Random.FRand();
            if (!bNear && Roll < 0.3f && FMath::Abs(Y - (H + 2.2f)) > 1.2f) { Tree(X, Y, Random.FRandRange(0.8f, 1.25f)); }
            else if (Roll < 0.45f) { Boulder(X, Y, Random.FRandRange(0.3f, 0.7f)); }
            else if (Roll < 0.6f)
            {
                Scenery.Add(MakePart(Sphere, nullptr, Ground(X, Y) + FVector(0, 0, 18), FVector(0.6f, 0.6f, 0.4f), Shade(FLinearColor(0.18f, 0.4f, 0.15f), Random.FRandRange(0.8f, 1.2f))));
            }
            else if (Roll < 0.75f)
            {
                const FLinearColor Petal = Random.FRand() < 0.5f ? FLinearColor(1.0f, 0.85f, 0.3f) : FLinearColor(0.85f, 0.45f, 0.9f);
                Scenery.Add(MakePart(Sphere, nullptr, Ground(X, Y) + FVector(0, 0, 8), FVector(0.08f), Petal));
            }
            else { Tuft(X, Y, FLinearColor(0.3f, 0.55f, 0.2f)); }
        }
        for (int32 Index = 0; Index < 30; ++Index) { Tuft(Random.FRandRange(-0.4f, W - 0.6f), Random.FRandRange(-0.4f, H - 0.6f), FLinearColor(0.32f, 0.5f, 0.22f)); }
    }
    else if (BoardTheme == EBoardTheme::Ruins)
    {
        // A dusty road on the far side, the watchtower beyond it and scattered ruins.
        const FVector Road = Ground((W - 1) * 0.5f, H + 1.8f);
        Scenery.Add(MakePart(Cube, nullptr, Road, FVector((W + 18) * RefCell / 100.0f, 1.3f, 0.04f), FLinearColor(0.5f, 0.42f, 0.3f)));
        const FVector Tower = Ground(W + 2.8f, H + 3.6f);
        Scenery.Add(MakePart(Cylinder, nullptr, Tower + FVector(0, 0, 210), FVector(1.7f, 1.7f, 4.2f), Palette.Rock));
        Scenery.Add(MakePart(Cylinder, nullptr, Tower + FVector(0, 0, 430), FVector(2.0f, 2.0f, 0.2f), Palette.RockDark));
        Scenery.Add(MakePart(Cone, nullptr, Tower + FVector(0, 0, 510), FVector(2.1f, 2.1f, 1.4f), FLinearColor(0.42f, 0.16f, 0.1f)));
        Scenery.Add(MakePart(Cube, nullptr, Tower + FVector(-86, 0, 300), FVector(0.05f, 0.3f, 0.45f), FLinearColor(0.05f, 0.04f, 0.04f)));
        Torch(Tower + FVector(-95, -40, 150), 140.0f);
        for (int32 Index = 0; Index < 34; ++Index)
        {
            float X, Y; bool bNear;
            if (!Spot(X, Y, bNear)) { continue; }
            const float Roll = Random.FRand();
            if (!bNear && Roll < 0.25f)
            {
                const float Height = Random.FRandRange(0.6f, 2.0f);
                Scenery.Add(MakePart(Cylinder, nullptr, Ground(X, Y) + FVector(0, 0, Height * 50), FVector(0.45f, 0.45f, Height), Shade(Palette.Rock, Random.FRandRange(0.8f, 1.05f))));
            }
            else if (!bNear && Roll < 0.38f)
            {
                // Dead tree.
                const FVector At = Ground(X, Y);
                Scenery.Add(MakePart(Cylinder, nullptr, At + FVector(0, 0, 70), FVector(0.14f, 0.14f, 1.4f), FLinearColor(0.2f, 0.15f, 0.12f), FRotator(Random.FRandRange(-8, 8), 0, Random.FRandRange(-8, 8))));
                Scenery.Add(MakePart(Cylinder, nullptr, At + FVector(15, 0, 110), FVector(0.07f, 0.07f, 0.6f), FLinearColor(0.2f, 0.15f, 0.12f), FRotator(0, 0, 40)));
            }
            else if (Roll < 0.7f) { Boulder(X, Y, Random.FRandRange(0.25f, 0.6f)); }
            else { Tuft(X, Y, FLinearColor(0.55f, 0.48f, 0.25f)); }
        }
    }
    else
    {
        // The vault: walls on three sides, torches, banners and the Waystone on its pedestal.
        const FLinearColor Wall = Shade(Palette.Rock, 0.8f);
        const float Height = 2.6f;
        auto WallRun = [&](FVector2D From, FVector2D To)
        {
            const FVector A = Ground(From.X, From.Y), B = Ground(To.X, To.Y);
            const FVector Mid = (A + B) * 0.5f;
            const FVector Size((B - A).Size() / 100.0f + 0.6f, 0.6f, Height);
            Scenery.Add(MakePart(Cube, nullptr, Mid + FVector(0, 0, Height * 50), Size, Wall, (B - A).Rotation()));
        };
        WallRun({-1.8f, H + 0.9f}, {W + 0.8f, H + 0.9f});
        WallRun({-1.8f, -1.6f}, {-1.8f, H + 0.9f});
        WallRun({W + 0.8f, -1.6f}, {W + 0.8f, H + 0.9f});
        for (const FVector2D Corner : {FVector2D(-1.8f, H + 0.9f), FVector2D(W + 0.8f, H + 0.9f), FVector2D(-1.8f, -1.6f), FVector2D(W + 0.8f, -1.6f)})
        {
            Scenery.Add(MakePart(Cylinder, nullptr, Ground(Corner.X, Corner.Y) + FVector(0, 0, 150), FVector(0.95f, 0.95f, 3.0f), Shade(Palette.Rock, 1.1f)));
        }
        for (int32 Index = 0; Index < 4; ++Index)
        {
            const float X = (Index + 0.5f) * (W - 1) / 4.0f;
            Torch(Ground(X, H + 0.45f) + FVector(0, 0, 120), 160.0f);
            if (Index == 1 || Index == 2) { continue; }
            Scenery.Add(MakePart(Cube, nullptr, Ground(X + 0.8f, H + 0.55f) + FVector(0, 0, 160), FVector(0.5f, 0.04f, 1.1f), FLinearColor(0.45f, 0.06f, 0.08f)));
        }
        for (int32 Side = 0; Side < 2; ++Side)
        {
            const float X = Side == 0 ? -1.2f : W + 0.2f;
            Torch(Ground(X, (H - 1) * 0.5f) + FVector(0, 0, 120), 140.0f);
        }
    }
}

void AExpeditionBattleView::AttachSilhouette()
{
    if (!SilhouetteMaterial)
    {
        // Created by Content/Python/import_dark_fantasy_theme.py; primitives and tests without the asset simply go without.
        SilhouetteMaterial = LoadObject<UMaterialInterface>(nullptr, SilhouetteMaterialPath, nullptr, LOAD_Quiet | LOAD_NoWarn);
    }
    if (!SilhouetteMaterial || HasSilhouetteBlendable()) { return; }
    Grade->Settings.WeightedBlendables.Array.Add(FWeightedBlendable(1.0f, SilhouetteMaterial));
}

bool AExpeditionBattleView::HasSilhouetteBlendable() const
{
    if (!SilhouetteMaterial) { return false; }
    for (const FWeightedBlendable& Blendable : Grade->Settings.WeightedBlendables.Array)
    {
        if (Blendable.Object == SilhouetteMaterial) { return true; }
    }
    return false;
}

void AExpeditionBattleView::SetFigureSilhouette(FExpeditionUnitRig& Rig, bool bOn)
{
    Rig.bSilhouette = bOn;
    const int32 Stencil = Rig.Side == ECombatSide::Enemy ? EnemyStencil : PartyStencil;
    for (FExpeditionPart& Part : Rig.Parts)
    {
        if (!Part.Mesh) { continue; }
        Part.Mesh->SetRenderCustomDepth(bOn);
        Part.Mesh->SetCustomDepthStencilValue(Stencil);
    }
    // A rigged figure (#147) draws through its skinned mesh.
    if (Rig.Skin)
    {
        Rig.Skin->SetRenderCustomDepth(bOn);
        Rig.Skin->SetCustomDepthStencilValue(Stencil);
    }
}

int32 AExpeditionBattleView::FigureStencil(int32 UnitId) const
{
    for (const FExpeditionUnitRig& Rig : Rigs)
    {
        if (Rig.Id != UnitId) { continue; }
        int32 Value = 0;
        bool bAny = false;
        TArray<const UPrimitiveComponent*, TInlineAllocator<8>> Drawn;
        for (const FExpeditionPart& Part : Rig.Parts) { if (Part.Mesh) { Drawn.Add(Part.Mesh); } }
        if (Rig.Skin) { Drawn.Add(Rig.Skin); }
        for (const UPrimitiveComponent* Mesh : Drawn)
        {
            const int32 Own = Mesh->bRenderCustomDepth ? Mesh->CustomDepthStencilValue : 0;
            if (!bAny) { Value = Own; bAny = true; }
            else if (Own != Value) { return -1; }
        }
        return Value;
    }
    return 0;
}

void AExpeditionBattleView::BuildRig(const FCombatantState& Unit)
{
    FExpeditionUnitRig Rig;
    Rig.Id = Unit.Id;
    Rig.Side = Unit.Side;
    Rig.Phase = Hash(Unit.Id, 11) * 2.0f * PI;
    USceneComponent* Root = NewObject<USceneComponent>(this);
    AddInstanceComponent(Root);
    Spawned.Add(Root);
    Root->SetupAttachment(RootComponent);
    Root->SetMobility(EComponentMobility::Movable);
    Root->RegisterComponent();
    Rig.Root = Root;
    // The rig root is scaled with the cell (bases and rings grow with it). The body holds the figure and
    // is fitted so every figure stands at the metric's figure height above the board.
    USceneComponent* Body = NewObject<USceneComponent>(this);
    AddInstanceComponent(Body);
    Spawned.Add(Body);
    Body->SetupAttachment(Root);
    Body->SetMobility(EComponentMobility::Movable);
    Body->RegisterComponent();
    auto BasePart = [&](UStaticMesh* Mesh, FVector Location, FVector Scale, FLinearColor Color)
    { Rig.Parts.Add(MakePart(Mesh, Root, Location, Scale, Color)); };
    // Primitive parts are authored in reference cm with default 100 cm shapes; track the highest point.
    float Top = 1.0f;
    auto Part = [&](UStaticMesh* Mesh, FVector Location, FVector Scale, FLinearColor Color, FRotator Rotation = FRotator::ZeroRotator)
    {
        Rig.Parts.Add(MakePart(Mesh, Body, Location, Scale, Color, Rotation));
        Top = FMath::Max(Top, FBox(Scale * -50.0f, Scale * 50.0f).TransformBy(FTransform(Rotation, Location)).Max.Z);
    };
    const FLinearColor Team = ExpeditionStyle::UnitColor(Unit);
    const FLinearColor Skin(0.92f, 0.74f, 0.6f);
    if (Theme)
    {
        // Themed look: a hollow footprint ring (56 cm on a 150 cm cell) in the side's colour keeps sides readable from
        // overhead. It lies on the board plane, in actor space, so it stays flat when the figure falls; UpdateRigs places it.
        if (CanBankRings())
        {
            // One bar bank serves every ring (#144); the colour rides in per-instance custom data, set by UpdateRigs.
            Rig.Ring.Base = Team * 1.15f;
            Rig.Ring.RingSlot = AddRingSlot(Rig.Ring.Base);
        }
        else
        {
            Rig.Ring = MakeRing(RootComponent, FVector::ZeroVector, Team * 1.15f);
            if (Rig.Ring.Mesh) { Rig.Ring.Mesh->SetRelativeScale3D(FVector(BoardMetric.FootprintDiameter() / 100.0f)); }
        }
    }
    else
    {
        // Primitive look, unchanged: the opaque side-coloured base disc under the figure.
        BasePart(Cylinder, FVector(0, 0, 2), FVector(0.74f, 0.74f, 0.04f), Shade(Team, 0.45f));
    }
    // Rigged character first (#147), then the static r001 mesh, then primitives.
    if (BuildSkeletalFigure(Rig, Body, Unit))
    {
        Rigs.Add(Rig);
        return;
    }
    if (UStaticMesh* Figure = UnitMesh(Unit))
    {
        // The figure itself is untinted.
        // Fit the mesh (origin at its feet) to the metric's figure height; the rig root already carries the cell scale.
        const float MeshHeight = Figure->GetBounds().GetBox().Max.Z;
        const float Fit = MeshHeight > 1.0f ? BoardMetric.FigureCm / BoardMetric.Scale() / MeshHeight : 1.45f;
        Part(Figure, FVector::ZeroVector, FVector(Fit), FLinearColor::White);
        Rigs.Add(Rig);
        return;
    }
    if (Unit.Side == ECombatSide::Party)
    {
        switch (Unit.AdventurerRole)
        {
        case EAdventurerRole::Barbarian:
            Part(Cylinder, FVector(0, 0, 30), FVector(0.56f, 0.56f, 0.56f), Skin);
            Part(Cylinder, FVector(0, 0, 52), FVector(0.58f, 0.58f, 0.12f), Team);
            Part(Sphere, FVector(0, 0, 72), FVector(0.34f), Skin);
            Part(Cube, FVector(12, 28, 46), FVector(0.08f, 0.08f, 0.7f), FLinearColor(0.4f, 0.25f, 0.12f));
            Part(Cube, FVector(12, 28, 80), FVector(0.12f, 0.45f, 0.22f), FLinearColor(0.7f, 0.72f, 0.75f));
            break;
        case EAdventurerRole::Fighter:
            Part(Cylinder, FVector(0, 0, 28), FVector(0.46f, 0.46f, 0.5f), FLinearColor(0.45f, 0.5f, 0.6f));
            Part(Cylinder, FVector(0, 0, 50), FVector(0.5f, 0.5f, 0.08f), Team);
            Part(Sphere, FVector(0, 0, 68), FVector(0.3f), Skin);
            Part(Cylinder, FVector(0, 0, 78), FVector(0.33f, 0.33f, 0.16f), FLinearColor(0.6f, 0.62f, 0.68f));
            Part(Cube, FVector(4, -26, 34), FVector(0.1f, 0.06f, 0.48f), Team);
            Part(Cube, FVector(8, -27, 34), FVector(0.04f, 0.36f, 0.42f), Team);
            Part(Cube, FVector(18, 26, 40), FVector(0.05f, 0.05f, 0.55f), FLinearColor(0.85f, 0.87f, 0.92f), FRotator(-25, 0, 0));
            break;
        case EAdventurerRole::Rogue:
            Part(Cylinder, FVector(0, 0, 26), FVector(0.38f, 0.38f, 0.48f), FLinearColor(0.16f, 0.28f, 0.2f));
            Part(Cone, FVector(-6, 0, 34), FVector(0.5f, 0.46f, 0.62f), Shade(Team, 0.55f));
            Part(Sphere, FVector(0, 0, 64), FVector(0.27f), Skin);
            Part(Cone, FVector(-2, 0, 78), FVector(0.36f, 0.36f, 0.34f), Shade(Team, 0.6f));
            Part(Cube, FVector(16, 20, 30), FVector(0.03f, 0.03f, 0.26f), FLinearColor(0.85f, 0.87f, 0.92f), FRotator(-50, 0, 0));
            Part(Cube, FVector(16, -20, 30), FVector(0.03f, 0.03f, 0.26f), FLinearColor(0.85f, 0.87f, 0.92f), FRotator(-50, 0, 0));
            break;
        case EAdventurerRole::Cleric:
            Part(Cone, FVector(0, 0, 30), FVector(0.58f, 0.58f, 0.6f), FLinearColor(0.92f, 0.9f, 0.82f));
            Part(Cylinder, FVector(0, 0, 44), FVector(0.36f, 0.36f, 0.1f), Team);
            Part(Sphere, FVector(0, 0, 66), FVector(0.29f), Skin);
            Part(Cylinder, FVector(0, 0, 92), FVector(0.36f, 0.36f, 0.015f), FLinearColor(1.0f, 0.85f, 0.35f) * 2.5f);
            Part(Cylinder, FVector(14, 24, 32), FVector(0.04f, 0.04f, 0.4f), FLinearColor(0.4f, 0.28f, 0.15f));
            Part(Sphere, FVector(14, 24, 54), FVector(0.12f), FLinearColor(0.75f, 0.7f, 0.5f));
            break;
        default:
            Part(Cone, FVector(0, 0, 30), FVector(0.56f, 0.56f, 0.62f), Shade(Team, 0.7f));
            Part(Sphere, FVector(0, 0, 66), FVector(0.28f), Skin);
            Part(Cylinder, FVector(0, 0, 76), FVector(0.5f, 0.5f, 0.025f), Shade(Team, 0.45f));
            Part(Cone, FVector(-3, 0, 100), FVector(0.32f, 0.32f, 0.5f), Shade(Team, 0.45f), FRotator(-8, 0, 0));
            Part(Cylinder, FVector(6, 26, 46), FVector(0.03f, 0.03f, 0.95f), FLinearColor(0.35f, 0.24f, 0.14f));
            Part(Sphere, FVector(6, 26, 96), FVector(0.12f), FLinearColor(0.75f, 0.55f, 1.0f) * 3.0f);
            break;
        }
    }
    else
    {
        const bool bArcher = Unit.Range > 1;
        const bool bRaider = Unit.Name.Contains(TEXT("raider")) || Unit.Name.StartsWith(TEXT("Raider"));
        const FLinearColor Cloth = bArcher ? FLinearColor(0.32f, 0.2f, 0.12f) : bRaider ? FLinearColor(0.42f, 0.1f, 0.08f) : FLinearColor(0.35f, 0.24f, 0.15f);
        Part(Cube, FVector(0, 0, 28), FVector(0.44f, 0.38f, 0.52f), Cloth);
        Part(Cube, FVector(0, 0, 52), FVector(0.5f, 0.46f, 0.1f), FLinearColor(0.22f, 0.2f, 0.2f));
        Part(Sphere, FVector(0, 0, 70), FVector(0.3f), FLinearColor(0.5f, 0.55f, 0.42f));
        Part(Sphere, FVector(13, -5, 72), FVector(0.05f), FLinearColor(1.0f, 0.2f, 0.1f) * 4.0f);
        Part(Sphere, FVector(13, 5, 72), FVector(0.05f), FLinearColor(1.0f, 0.2f, 0.1f) * 4.0f);
        if (bRaider)
        {
            Part(Cone, FVector(0, -11, 86), FVector(0.08f, 0.08f, 0.2f), FLinearColor(0.85f, 0.8f, 0.7f), FRotator(0, 0, -25));
            Part(Cone, FVector(0, 11, 86), FVector(0.08f, 0.08f, 0.2f), FLinearColor(0.85f, 0.8f, 0.7f), FRotator(0, 0, 25));
            Part(Cube, FVector(16, 24, 36), FVector(0.06f, 0.06f, 0.5f), FLinearColor(0.5f, 0.5f, 0.52f), FRotator(-20, 0, 0));
            Part(Cube, FVector(24, 24, 58), FVector(0.24f, 0.04f, 0.16f), FLinearColor(0.6f, 0.6f, 0.62f), FRotator(-20, 0, 0));
        }
        else
        {
            Part(Cone, FVector(-2, 0, 84), FVector(0.36f, 0.36f, 0.3f), Shade(Cloth, 0.8f));
        }
        if (bArcher)
        {
            Part(Cylinder, FVector(12, 24, 40), FVector(0.025f, 0.025f, 0.75f), FLinearColor(0.45f, 0.3f, 0.15f), FRotator(0, 0, 8));
            Part(Cylinder, FVector(-16, 6, 44), FVector(0.1f, 0.1f, 0.4f), FLinearColor(0.3f, 0.2f, 0.1f), FRotator(20, 0, 0));
        }
        else if (!bRaider)
        {
            Part(Cube, FVector(16, 22, 30), FVector(0.04f, 0.04f, 0.3f), FLinearColor(0.8f, 0.8f, 0.82f), FRotator(-45, 0, 0));
        }
    }
    // Primitive figures differ in height by role; fit each so they all stand at the figure height.
    Body->SetRelativeScale3D(FVector(BoardMetric.FigureCm / BoardMetric.Scale() / Top));
    Rigs.Add(Rig);
}

void AExpeditionBattleView::BuildEffects(const FCombatEncounter& Encounter)
{
    for (int32 Index = 0; Index < 2 + TargetMarkers; ++Index)
    {
        const FLinearColor Color = Index == 0 ? FLinearColor(1.0f, 0.82f, 0.2f) * 1.6f : Index == 1 ? FLinearColor(0.9f, 0.95f, 1.0f) : FLinearColor(1.0f, 0.18f, 0.1f) * 1.5f;
        if (Theme && CanBankRings())
        {
            // Themed look, instanced: a slot in the shared ring bank, hidden until placed.
            FExpeditionPart Marker;
            Marker.Base = Color;
            Marker.RingSlot = AddRingSlot(Color);
            Markers.Add(Marker);
            continue;
        }
        // Themed look: hollow rings. Primitive look, unchanged: flat opaque discs.
        Markers.Add(Theme ? MakeRing(nullptr, FVector::ZeroVector, Color)
            : MakePart(Cylinder, nullptr, FVector::ZeroVector, FVector(0.92f, 0.92f, 0.02f), Color));
        Markers.Last().Mesh->SetVisibility(false);
    }
    for (int32 Index = 0; Index < ShotPool; ++Index)
    {
        // Markers, shots, sparks and the camp are overlays and effects: they never cast shadows.
        ShotSlots.Add(MakeSlot(Sphere, FVector::ZeroVector, FVector(0.2f), FLinearColor::White, FRotator::ZeroRotator, false));
        SetSlot(ShotSlots.Last(), false, FVector::ZeroVector, FRotator::ZeroRotator, FVector(0.2f), FLinearColor::White);
        ShotLights.Add(MakeLight(nullptr, FVector::ZeroVector, FLinearColor::White, 0.0f, 450.0f));
    }
    for (int32 Index = 0; Index < SparkPool; ++Index)
    {
        SparkSlots.Add(MakeSlot(Sphere, FVector::ZeroVector, FVector(0.07f), FLinearColor::White, FRotator::ZeroRotator, false));
        SetSlot(SparkSlots.Last(), false, FVector::ZeroVector, FRotator::ZeroRotator, FVector(0.07f), FLinearColor::White);
    }
    // Camp: stones, crossed logs, flames and a warm light. Shown only at the recovery choice.
    const FVector Camp = SceneBoard(FVector2D(1.6f, (Encounter.Height - 1) * 0.5f));
    for (int32 Stone = 0; Stone < 7; ++Stone)
    {
        const float Angle = Stone * 2.0f * PI / 7.0f;
        CampSlots.Add(MakeSlot(Sphere, Camp + FVector(FMath::Cos(Angle) * 30, FMath::Sin(Angle) * 30, 5), FVector(0.14f, 0.14f, 0.1f), FLinearColor(0.35f, 0.34f, 0.32f), FRotator::ZeroRotator, false));
    }
    CampSlots.Add(MakeSlot(Cylinder, Camp + FVector(0, 0, 8), FVector(0.08f, 0.08f, 0.55f), FLinearColor(0.3f, 0.18f, 0.1f), FRotator(0, 0, 80), false));
    CampSlots.Add(MakeSlot(Cylinder, Camp + FVector(0, 0, 8), FVector(0.08f, 0.08f, 0.55f), FLinearColor(0.3f, 0.18f, 0.1f), FRotator(80, 0, 0), false));
    CampSlots.Add(MakeSlot(Cone, Camp + FVector(0, 0, 24), FVector(0.3f, 0.3f, 0.45f), FLinearColor(1.0f, 0.45f, 0.08f) * 5.0f, FRotator::ZeroRotator, false));
    CampSlots.Add(MakeSlot(Cone, Camp + FVector(3, 2, 22), FVector(0.18f, 0.18f, 0.3f), FLinearColor(1.0f, 0.85f, 0.3f) * 6.0f, FRotator::ZeroRotator, false));
    CampLight = MakeLight(nullptr, Camp + FVector(0, 0, 60), FLinearColor(1.0f, 0.55f, 0.25f), 0.0f, 1400.0f);
    for (FExpeditionSlot& Slot : CampSlots) { SetSlot(Slot, false, Slot.Location, Slot.Rotation, Slot.Scale, Slot.Colour); }
    for (FExpeditionPart& Part : Markers) { if (Part.Mesh) { Part.Mesh->SetCastShadow(false); } }
    // The Waystone floats above its pedestal in the vault until the finale is won.
    if (BoardTheme == EBoardTheme::Vault)
    {
        // The far-right corner keeps the Waystone clear of the initiative strip.
        const FVector Pedestal = SceneBoard(FVector2D(Encounter.Width - 0.3f, Encounter.Height + 0.1f), -RefTop + 6.0f);
        WaystoneSlots.Add(MakeSlot(Cylinder, Pedestal + FVector(0, 0, 45), FVector(0.7f, 0.7f, 0.9f), FLinearColor(0.25f, 0.25f, 0.32f), FRotator::ZeroRotator, true));
        WaystoneSlots.Add(MakeSlot(Cube, Pedestal + FVector(0, 0, 140), FVector(0.32f, 0.32f, 0.32f), FLinearColor(0.3f, 0.9f, 1.0f) * 4.0f, FRotator(45, 45, 0), true));
        WaystoneLight = MakeLight(nullptr, Pedestal + FVector(0, 0, 160), FLinearColor(0.3f, 0.85f, 1.0f), 250.0f, 1300.0f);
    }
}

FVector AExpeditionBattleView::CellPosition(FIntPoint Cell) const
{
    return BoardMetric.CellPosition(Cell);
}

FVector AExpeditionBattleView::BoardPosition(FVector2D Cell, float Height) const
{
    return BoardMetric.BoardPosition(Cell, Height);
}

FVector AExpeditionBattleView::UnitPosition(int32 Id, float Height) const
{
    const FUnitDisplay* Unit = Show.Unit(Id);
    if (!Unit) { return BoardPosition(FVector2D::ZeroVector, Height); }
    return BoardPosition(Unit->Position + Unit->Lean, BoardMetric.Scaled(Unit->Hop - Unit->Tilt * 45.0f) + Height);
}

void AExpeditionBattleView::UpdateRigs()
{
    for (FExpeditionUnitRig& Rig : Rigs)
    {
        const FUnitDisplay* Unit = Show.Unit(Rig.Id);
        if (!Unit || !Rig.Root) { continue; }
        const float Breath = Unit->Life == ECombatLife::Conscious ? FMath::Sin(Clock * 2.2f + Rig.Phase) : 0.0f;
        const float Facing = FMath::RadiansToDegrees(FMath::Atan2(Unit->Facing.Y, Unit->Facing.X));
        const FQuat Turn(FVector::UpVector, FMath::DegreesToRadians(Facing));
        if (Rig.Skin)
        {
            // A rigged figure stands on its cell and the clips do the rest: no hop, breathing squash or tilt.
            Rig.Root->SetRelativeLocationAndRotation(BoardPosition(Unit->Position + Unit->Lean), Turn);
            Rig.Root->SetRelativeScale3D(FVector(BoardMetric.Scale()));
            UpdateAnimation(Rig, *Unit);
        }
        else
        {
            const FVector Location = BoardPosition(Unit->Position + Unit->Lean, BoardMetric.Scaled(Unit->Hop));
            const FQuat Fall(FVector::ForwardVector, FMath::DegreesToRadians(84.0f * Unit->Tilt));
            Rig.Root->SetRelativeLocationAndRotation(Location, Turn * Fall);
            // Bodies still occupy their cells, so keep fallen rigs visible on the board.
            Rig.Root->SetRelativeScale3D(FVector(1.0f, 1.0f, 1.0f + Breath * 0.02f) * BoardMetric.Scale());
        }
        const bool bDead = Unit->Life == ECombatLife::Dead;
        auto Shown = [&](FLinearColor Color)
        {
            if (bDead) { const float Grey = Color.GetLuminance(); Color = FMath::Lerp(Color, FLinearColor(Grey, Grey, Grey) * 0.6f, 0.75f); }
            Color = FMath::Lerp(Color, FLinearColor(1.0f, 0.95f, 0.75f) * 2.0f, Unit->Glow * 0.35f);
            Color = FMath::Lerp(Color, FLinearColor(1.0f, 0.9f, 0.9f) * 3.0f, Unit->Flash * 0.85f);
            return Color;
        };
        auto Tint = [&](FExpeditionPart& Part) { Part.SetColor(Shown(Part.Base)); };
        for (FExpeditionPart& Part : Rig.Parts) { Tint(Part); }
        // Figures behind scenery show a side-coloured silhouette (M_FigureSilhouette reads the custom depth and stencil). The dead
        // lie on the board as bodies and drop out; the downed still matter (they can be saved), so they keep it.
        if (Rig.bSilhouette != !bDead) { SetFigureSilhouette(Rig, !bDead); }
        if (Rig.Ring.RingSlot != INDEX_NONE)
        {
            // Flat on the board under the figure, in actor space, not hopping or tilting with it.
            SetRing(Rig.Ring.RingSlot, true, BoardPosition(Unit->Position + Unit->Lean, BoardMetric.Scaled(0.8f)), BoardMetric.FootprintDiameter() / 100.0f, Shown(Rig.Ring.Base));
        }
        else if (Rig.Ring.Mesh)
        {
            // Flat on the board under the figure, not hopping or tilting with it.
            Rig.Ring.Mesh->SetRelativeLocation(BoardPosition(Unit->Position + Unit->Lean, BoardMetric.Scaled(0.8f)));
            Rig.Ring.Mesh->SetRelativeScale3D(FVector(BoardMetric.FootprintDiameter() / 100.0f));
            Tint(Rig.Ring);
        }
    }
}

const FDiceRollCharacter* AExpeditionBattleView::UnitRigged(const FCombatantState& Unit) const
{
    if (!Theme || !bSkeletalFigures) { return nullptr; }
    if (Unit.Side == ECombatSide::Party)
    {
        switch (Unit.AdventurerRole)
        {
        case EAdventurerRole::Fighter: return &Theme->FighterRig;
        case EAdventurerRole::Wizard: return &Theme->WizardRig;
        default: return nullptr;
        }
    }
    // The same roster rule as UnitMesh: ranged foes are archers, the rest are raiders or scouts.
    return Unit.Range <= 1 && Unit.Name.Contains(TEXT("raider")) ? &Theme->RaiderRig : nullptr;
}

bool AExpeditionBattleView::BuildSkeletalFigure(FExpeditionUnitRig& Rig, USceneComponent* Body, const FCombatantState& Unit)
{
    const FDiceRollCharacter* Definition = UnitRigged(Unit);
    USkeletalMesh* Mesh = nullptr;
    TArray<UAnimSequence*> Clips;
    // An incomplete definition (no mesh, or any clip missing) is not used at all: the static figure takes over.
    if (!Definition || !Definition->Load(Mesh, Clips) || !Mesh || Mesh->GetMaterials().IsEmpty()) { return false; }
    USkeletalMeshComponent* Skin = NewObject<USkeletalMeshComponent>(this);
    AddInstanceComponent(Skin);
    Spawned.Add(Skin);
    Skin->SetupAttachment(Body);
    Skin->SetMobility(EComponentMobility::Movable);
    Skin->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Skin->SetAnimationMode(EAnimationMode::AnimationSingleNode);
    Skin->SetSkeletalMesh(Mesh);
    // The mesh origin is at the feet. Fit it to the metric's figure height (the rig root already carries the cell scale),
    // exactly as static figures are fitted.
    const float MeshHeight = static_cast<float>(Mesh->GetBounds().GetBox().Max.Z);
    const float Fit = MeshHeight > 1.0f ? BoardMetric.FigureCm / BoardMetric.Scale() / MeshHeight : 1.0f;
    Skin->SetRelativeScale3D(FVector(Fit));
    Skin->SetRelativeRotation(FRotator(0.0f, Definition->YawOffset, 0.0f));
    // One tintable instance per slot, like static figures: Flash and Glow reach the figure through "Color".
    const TArray<FSkeletalMaterial>& Slots = Mesh->GetMaterials();
    for (int32 Slot = 0; Slot < Slots.Num(); ++Slot)
    {
        if (!Slots[Slot].MaterialInterface) { continue; }
        FExpeditionPart Look;
        Look.Material = UMaterialInstanceDynamic::Create(Slots[Slot].MaterialInterface, this);
        Look.Base = FLinearColor::White;
        Skin->SetMaterial(Slot, Look.Material);
        Look.SetColor(Look.Base);
        Rig.Parts.Add(Look);
    }
    Skin->RegisterComponent();
    Rig.Skin = Skin;
    for (UAnimSequence* Clip : Clips) { Rig.Clips.Add(Clip); }
    Rig.Pose = EUnitPose::Idle;
    Rig.PoseSerial = -1;
    Rig.Stamp = Clock;
    // The first UpdateRigs shows the display's pose (Rest on a fresh board); show Idle now so the component never has none.
    ShowPose(Rig, EUnitPose::Idle, true);
    return true;
}

void AExpeditionBattleView::ShowPose(FExpeditionUnitRig& Rig, EUnitPose Pose, bool bSnap)
{
    UAnimSequence* Clip = Rig.Clips.IsValidIndex(static_cast<int32>(Pose)) ? Rig.Clips[static_cast<int32>(Pose)].Get() : nullptr;
    if (!Rig.Skin || !Clip) { return; }
    const float Length = Clip->GetPlayLength();
    Rig.Pose = Pose;
    Rig.bIdleAfter = false;
    // The component holds the clip but never advances it: the view sets the time every frame (UpdateAnimation).
    Rig.Skin->PlayAnimation(Clip, ExpeditionPose::IsLooping(Pose));
    Rig.Skin->Stop();
    if (!bSnap) { Rig.PoseTime = 0.0f; }
    else if (ExpeditionPose::IsLooping(Pose)) { Rig.PoseTime = Length * (Rig.Phase / (2.0f * PI)); }
    else { Rig.PoseTime = Length; } // Converged on a held pose: Downed and Death show their last frame at once.
    Rig.Skin->SetPosition(Rig.PoseTime, false);
}

void AExpeditionBattleView::UpdateAnimation(FExpeditionUnitRig& Rig, const FUnitDisplay& Unit)
{
    const float Delta = FMath::Max(0.0f, Clock - Rig.Stamp) * Show.Speed;
    Rig.Stamp = Clock;
    if (Unit.PoseSerial != Rig.PoseSerial)
    {
        Rig.PoseSerial = Unit.PoseSerial;
        Rig.bIdleAfter = false;
        const UAnimSequence* Playing = Rig.Clips.IsValidIndex(static_cast<int32>(Rig.Pose)) ? Rig.Clips[static_cast<int32>(Rig.Pose)].Get() : nullptr;
        const bool bOneShotPlaying = !ExpeditionPose::IsLooping(Rig.Pose) && Playing && Rig.PoseTime < Playing->GetPlayLength();
        // Playback settling back to Idle lets a one-shot clip (a swing, a flinch, a recovery) play out; converging on
        // the committed state (reset, resume, skip, Finish) shows the final pose at once.
        if (!Unit.bPoseSnap && Unit.Pose == EUnitPose::Idle && bOneShotPlaying) { Rig.bIdleAfter = true; }
        else { ShowPose(Rig, Unit.Pose, Unit.bPoseSnap); return; }
    }
    const UAnimSequence* Clip = Rig.Clips.IsValidIndex(static_cast<int32>(Rig.Pose)) ? Rig.Clips[static_cast<int32>(Rig.Pose)].Get() : nullptr;
    if (!Clip) { return; }
    const float Length = Clip->GetPlayLength();
    Rig.PoseTime += Delta;
    if (ExpeditionPose::IsLooping(Rig.Pose)) { Rig.PoseTime = Length > 0.0f ? FMath::Fmod(Rig.PoseTime, Length) : 0.0f; }
    else if (Rig.PoseTime >= Length)
    {
        Rig.PoseTime = Length;
        if (Rig.bIdleAfter) { ShowPose(Rig, EUnitPose::Idle, false); return; }
    }
    Rig.Skin->SetPosition(Rig.PoseTime, false);
}

void AExpeditionBattleView::UpdateTiles()
{
    const bool bShowChoices = InCombat() && !IsPresenting();
    const FLinearColor Reach(0.15f, 0.7f, 0.85f);
    const FLinearColor Trail(0.5f, 0.95f, 1.0f);
    for (FExpeditionTile& Tile : Tiles)
    {
        FLinearColor Color = Tile.Part.Base;
        if (bShowChoices)
        {
            const float Pulse = 0.5f + 0.5f * FMath::Sin(Clock * 3.0f);
            if (Highlights.Reach.Obstructed.Contains(Tile.Cell)) { Color = FMath::Lerp(Color, FLinearColor(0.8f,0.16f,0.12f),0.35f); }
            else if (Highlights.Reach.Visible.Contains(Tile.Cell)) { Color = FMath::Lerp(Color,FLinearColor(0.3f,0.4f,0.9f),0.35f); }
            if (Highlights.Path.Contains(Tile.Cell)) { Color = FMath::Lerp(Color, Trail, 0.7f); }
            else if (Highlights.Moves.Contains(Tile.Cell)) { Color = FMath::Lerp(Color, Reach, 0.32f + Pulse * 0.06f); }
            if (Highlights.bHasHover && Highlights.Hover == Tile.Cell) { Color = Color * 1.35f; }
        }
        Tile.Part.SetColor(Color);
    }
}

void AExpeditionBattleView::UpdateMarkers()
{
    // Each marker size is given for both looks: a multiple of the footprint ring (themed) and the original disc scale.
    // Instanced markers live in the ring bank, in actor space; loose ones under the scaled scenery root.
    auto Place = [this](FExpeditionPart& Marker, int32 Id, float RingScale, float DiscScale, float Lift, const FLinearColor& Colour)
    {
        const FUnitDisplay* Unit = Show.Unit(Id);
        if (Marker.RingSlot != INDEX_NONE)
        {
            if (!Unit) { SetRing(Marker.RingSlot, false, FVector::ZeroVector, 0.0f, RingStates[Marker.RingSlot].Colour); return; }
            SetRing(Marker.RingSlot, true, SceneBoard(Unit->Position, Lift) * SceneScale(), BoardMetric.FootprintDiameter() * RingScale / 100.0f, Colour);
            return;
        }
        if (!Unit) { Marker.Mesh->SetVisibility(false); return; }
        Marker.Mesh->SetVisibility(true);
        Marker.Mesh->SetRelativeLocation(SceneBoard(Unit->Position, Lift));
        // Themed rings are a multiple of the footprint ring; the markers live under the scaled scenery root.
        Marker.Mesh->SetRelativeScale3D(Theme ? FVector(BoardMetric.FootprintDiameter() * RingScale / SceneScale() / 100.0f)
            : FVector(DiscScale, DiscScale, 0.02f));
    };
    auto Hide = [this](FExpeditionPart& Marker)
    {
        if (Marker.RingSlot != INDEX_NONE) { SetRing(Marker.RingSlot, false, FVector::ZeroVector, 0.0f, RingStates[Marker.RingSlot].Colour); }
        else { Marker.Mesh->SetVisibility(false); }
    };
    if (Markers.Num() < 2) { return; }
    const bool bCombat = InCombat();
    const int32 Active = bCombat && Rules().Snapshot().Phase == EBattlePhase::Fighting ? Rules().ActiveId() : INDEX_NONE;
    const int32 Shown = IsPresenting() && Show.FocusId() != INDEX_NONE ? INDEX_NONE : Active;
    if (Shown != INDEX_NONE) { Place(Markers[0], Shown, 1.6f + 0.06f * FMath::Sin(Clock * 4.0f), 0.95f + 0.05f * FMath::Sin(Clock * 4.0f), 0.6f, Markers[0].Base); }
    else { Hide(Markers[0]); }
    if (bCombat && Highlights.SelectedId != INDEX_NONE && Highlights.SelectedId != Shown) { Place(Markers[1], Highlights.SelectedId, 1.42f, 0.9f, 0.4f, Markers[1].Base); }
    else { Hide(Markers[1]); }
    for (int32 Index = 0; Index < TargetMarkers; ++Index)
    {
        FExpeditionPart& Marker = Markers[2 + Index];
        if (!bCombat || IsPresenting() || !Highlights.Targets.IsValidIndex(Index)) { Hide(Marker); continue; }
        const int32 Id = Highlights.Targets[Index];
        const bool bHover = Id == Highlights.HoverTargetId;
        const FLinearColor Colour = Highlights.bFriendlyTargets ? FLinearColor(0.3f, 1.0f, 0.5f) * (bHover ? 2.2f : 1.3f) : FLinearColor(1.0f, 0.18f, 0.1f) * (bHover ? 2.5f : 1.4f);
        Place(Marker, Id, (bHover ? 1.34f : 1.22f) + (bHover ? 0.05f * FMath::Sin(Clock * 8.0f) : 0.0f),
            (bHover ? 1.0f : 0.9f) + (bHover ? 0.06f * FMath::Sin(Clock * 8.0f) : 0.0f), 0.8f, Colour);
        if (Marker.RingSlot == INDEX_NONE) { Marker.SetColor(Colour); }
    }
}

void AExpeditionBattleView::UpdateEffects()
{
    const TArray<FPlaybackProjectile>& Shots = Show.Projectiles();
    for (int32 Index = 0; Index < ShotSlots.Num(); ++Index)
    {
        FExpeditionSlot& Slot = ShotSlots[Index];
        const bool bLive = Shots.IsValidIndex(Index) && Shots[Index].Age >= 0.0f;
        ShotLights[Index]->SetIntensity(0.0f);
        if (!bLive) { SetSlot(Slot, false, Slot.Location, Slot.Rotation, Slot.Scale, Slot.Colour); continue; }
        const FPlaybackProjectile& Shot = Shots[Index];
        const float Alpha = FMath::Clamp(Shot.Age / Shot.Duration, 0.0f, 1.0f);
        // Scenery-local (reference) space: the root scales the shot with the cell.
        const FVector From = SceneBoard(Shot.From, 60.0f), To = SceneBoard(Shot.To, 50.0f);
        const FVector Side = FVector::CrossProduct((To - From).GetSafeNormal(), FVector::UpVector);
        const float Curve = FMath::Sin(Alpha * PI);
        // Positive arcs lob upward; negative arcs curve sideways.
        const FVector Position = FMath::Lerp(From, To, Alpha) + (Shot.Arc >= 0 ? FVector(0, 0, Shot.Arc * Curve) : Side * Shot.Arc * Curve);
        SetSlot(Slot, true, Position, (To - From).Rotation(), Shot.bArrow ? FVector(0.5f, 0.05f, 0.05f) : FVector(Shot.Size),
            Shot.bArrow ? Shot.Color : Shot.Color * 6.0f);
        // The light follows the shot (it was a child of the shot's own component when pieces were loose).
        ShotLights[Index]->SetRelativeLocation(Position);
        if (!Shot.bArrow) { ShotLights[Index]->SetLightColor(Shot.Color); ShotLights[Index]->SetIntensity(180.0f); }
    }
    const TArray<FPlaybackSparkle>& Sparks = Show.Sparkles();
    for (int32 Index = 0; Index < SparkSlots.Num(); ++Index)
    {
        FExpeditionSlot& Slot = SparkSlots[Index];
        const bool bLive = Sparks.IsValidIndex(Index) && Sparks[Index].Age >= 0.0f;
        if (!bLive) { SetSlot(Slot, false, Slot.Location, Slot.Rotation, Slot.Scale, Slot.Colour); continue; }
        const FPlaybackSparkle& Spark = Sparks[Index];
        const float Alpha = Spark.Age / Spark.Life;
        const float Angle = Spark.Angle + Alpha * 3.0f;
        SetSlot(Slot, true, SceneBoard(Spark.Cell, 20.0f + Alpha * 100.0f) + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0) * (28.0f + Alpha * 10.0f),
            FRotator::ZeroRotator, FVector(0.08f * (1.0f - Alpha * 0.7f)), Spark.Color * 5.0f);
    }
    // Torches and the campfire flicker; the camp appears only at the recovery choice.
    for (int32 Index = 0; Index < Flames.Num(); ++Index)
    {
        if (!Flames[Index]) { continue; }
        const float Flicker = 0.8f + 0.12f * FMath::Sin(Clock * 13.0f + Index * 2.1f) + 0.08f * FMath::Sin(Clock * 29.0f + Index);
        const float Strength = FlameStrength.IsValidIndex(Index) && FlameStrength[Index] > 0.0f ? FlameStrength[Index] : BoardTheme == EBoardTheme::Vault ? 160.0f : 140.0f;
        Flames[Index]->SetIntensity(Strength * Flicker);
    }
    const bool bCamp = HasRun() && Progress().Phase == EExpeditionPhase::Recovery && !bShowRecords;
    for (int32 Index = 0; Index < CampSlots.Num(); ++Index)
    {
        FExpeditionSlot& Slot = CampSlots[Index];
        FVector Scale = Slot.Scale;
        if (bCamp && Index >= CampSlots.Num() - 2)
        {
            const float Flicker = 1.0f + 0.15f * FMath::Sin(Clock * 17.0f + Index * 3.0f);
            const float Size = Index == CampSlots.Num() - 1 ? 0.18f : 0.3f;
            Scale = FVector(Size, Size, Size * 1.5f * Flicker);
        }
        SetSlot(Slot, bCamp, Slot.Location, Slot.Rotation, Scale, Slot.Colour);
    }
    if (CampLight) { CampLight->SetIntensity(bCamp ? 220.0f * (0.85f + 0.15f * FMath::Sin(Clock * 11.0f)) : 0.0f); }
    if (WaystoneSlots.Num() == 2 && WaystoneLight)
    {
        const bool bClaimed = HasRun() && Progress().bArtifactRecovered;
        const float Rise = bClaimed ? 60.0f + 20.0f * FMath::Sin(Clock * 1.5f) : 10.0f * FMath::Sin(Clock * 1.8f);
        const FVector Base = WaystoneSlots[0].Location;
        SetSlot(WaystoneSlots[1], true, Base + FVector(0, 0, 95 + Rise), FRotator(45, Clock * 40.0f, 45), WaystoneSlots[1].Scale, WaystoneSlots[1].Colour);
        WaystoneLight->SetIntensity((bClaimed ? 320.0f : 200.0f) * (0.85f + 0.15f * FMath::Sin(Clock * 2.5f)));
    }
}

FVector AExpeditionBattleView::BaseFocus() const
{
    const FCombatEncounter& Encounter = Rules().Snapshot().Encounter;
    FVector Base = BoardPosition(FVector2D((Encounter.Width - 1) * 0.5f, (Encounter.Height - 1) * 0.5f), -BoardMetric.BoardTop());
    // Lean the view toward whoever is acting so long boards keep the action framed.
    const int32 FocusId = IsPresenting() && Show.FocusId() != INDEX_NONE ? Show.FocusId() : InCombat() ? Rules().ActiveId() : INDEX_NONE;
    if (FocusId != INDEX_NONE && Show.Unit(FocusId))
    {
        const FVector Centre = Base;
        Base = FMath::Lerp(Base, UnitPosition(FocusId, -BoardMetric.BoardTop()), 0.4f);
        // At the default zoom the lean may only use the slack that keeps the board's corners inside the safe rect.
        if (!bOverview)
        {
            // The pose actually rendered this frame (mid-orbit the target yaw and distance are not on screen yet).
            const float Pitch = ExpeditionCameraRig::PitchFor(Distance, FramedDistance());
            const FVector2D Leaned = ExpeditionCameraRig::LimitLean(FVector2D(Centre), FVector2D(Base), Encounter.Width, Encounter.Height, BoardMetric, Yaw, Distance, Pitch, Safe);
            Base.X = Leaned.X; Base.Y = Leaned.Y;
        }
    }
    return Base;
}

float AExpeditionBattleView::CameraPitch() const
{
    return bOverview ? FMath::RadiansToDegrees(FMath::Atan2(0.62f, 1.5f)) : ExpeditionCameraRig::PitchFor(TargetDistance, FramedDistance());
}

void AExpeditionBattleView::SetSafeRect(const ExpeditionCameraRig::FSafeRect& Value)
{
    if (Value == Safe) { return; }
    // A camera resting at the default zoom follows the new fit; a zoom the player (or a pose) chose stays.
    const bool bDefaultZoom = AtFramedZoom();
    Safe = Value;
    if (bDefaultZoom) { TargetDistance = FramedDistance(TargetYaw); bFramed = true; }
}

void AExpeditionBattleView::UpdateCamera(float DeltaSeconds)
{
    const FCombatEncounter& Encounter = Rules().Snapshot().Encounter;
    Camera->FieldOfView = bOverview ? OverviewFieldOfView : ExpeditionCameraRig::FieldOfView;
    // Yaw and distance first: the lean and the fit below belong to the pose rendered this frame, not to the targets.
    const bool bSnap = !bCameraPlaced || DeltaSeconds <= 0.0f;
    if (bSnap)
    {
        Yaw = TargetYaw; Distance = TargetDistance;
    }
    else
    {
        Yaw = FMath::FInterpTo(Yaw, TargetYaw, DeltaSeconds, 8.0f);
        Distance = FMath::FInterpTo(Distance, TargetDistance, DeltaSeconds, 6.0f);
        if (bFramed && !bOverview && !FMath::IsNearlyEqual(Yaw, TargetYaw, 0.01f))
        {
            // Easing yaw and distance independently dips below the fit in between (a long board end-on needs far more than
            // side-on), so the rendered distance never falls under the fit for the rendered yaw. This is one FitDistance solve
            // (a 25-unit outward scan plus at most 24 bisections, each four projections: roughly 10^4 flops) per frame, only while a
            // framed camera is still turning; once the yaw settles the target distance (already the fit) applies without a solve.
            Distance = FMath::Max(Distance, FramedDistance(Yaw));
        }
    }
    FVector Desired = FVector(0.0f, 30.0f, OverviewHeight + 22.0f);
    if (!bOverview)
    {
        // A new acting unit hands the view back to the auto focus; manual pan stays within the board plus a margin.
        const int32 FocusId = IsPresenting() && Show.FocusId() != INDEX_NONE ? Show.FocusId() : InCombat() ? Rules().ActiveId() : INDEX_NONE;
        if (FocusId != LastFocusId) { PanOffset = FVector2D::ZeroVector; LastFocusId = FocusId; }
        const FVector Base = BaseFocus();
        const FVector2D Clamped = ExpeditionCameraRig::ClampFocus(FVector2D(Base) + PanOffset, Encounter.Width, Encounter.Height, BoardMetric.CellCm);
        PanOffset = Clamped - FVector2D(Base);
        Desired = FVector(Clamped.X, Clamped.Y, Base.Z);
    }
    FocusTarget = Desired;
    if (bSnap)
    {
        Focus = Desired; bCameraPlaced = DeltaSeconds > 0.0f || bCameraPlaced;
    }
    else
    {
        Focus = FMath::VInterpTo(Focus, Desired, DeltaSeconds, 2.5f);
        if (bFramed && !bOverview && PanOffset.IsNearlyZero())
        {
            // The eased focus lags the lean, which was limited for an earlier yaw; hold it to what this pose can show.
            const FVector Centre = BoardPosition(FVector2D((Encounter.Width - 1) * 0.5f, (Encounter.Height - 1) * 0.5f), -BoardMetric.BoardTop());
            const float Pitch = ExpeditionCameraRig::PitchFor(Distance, FramedDistance());
            const FVector2D Held = ExpeditionCameraRig::LimitLean(FVector2D(Centre), FVector2D(Focus), Encounter.Width, Encounter.Height, BoardMetric, Yaw, Distance, Pitch, Safe);
            Focus.X = Held.X; Focus.Y = Held.Y;
        }
    }
    ShakeAmount = FMath::Max(0.0f, ShakeAmount - DeltaSeconds * 1.8f);
    const FVector Shake = ShakeAmount > 0.0f ? FVector(FMath::Sin(Clock * 61.0f), FMath::Sin(Clock * 47.0f + 1.0f), FMath::Sin(Clock * 53.0f + 2.0f)) * ShakeAmount * 14.0f * SceneScale() : FVector::ZeroVector;
    const float Radians = FMath::DegreesToRadians(Yaw);
    // The tabletop gets a low, seated view. Battles tie pitch to zoom: low and close, steep and far.
    // Distances are reference units; the board camera scales them with the cell (the tabletop is unscaled).
    const float Span = Distance * SceneScale();
    FVector Offset(FMath::Cos(Radians) * Span * 1.5f, FMath::Sin(Radians) * Span * 1.5f, Span * 0.62f);
    // Battles slide the view so the focus lands in the middle of the screen area the HUD leaves free.
    FVector Framing = FVector::ZeroVector;
    if (!bOverview)
    {
        const ExpeditionCameraRig::FArm Arm = ExpeditionCameraRig::Arm(Yaw, Distance, ExpeditionCameraRig::PitchFor(Distance, FramedDistance()), SceneScale(), Safe);
        Offset = Arm.Offset;
        Framing = Arm.Framing;
    }
    Camera->SetWorldLocation(GetActorTransform().TransformPosition(Focus) + Offset + Framing + Shake);
    // On the tabletop, aim below the dioramas so they sit above the centred menus and route cards.
    Camera->SetWorldRotation((-Offset - FVector(0.0f, 0.0f, bOverview ? Span * 0.32f : 0.0f)).Rotation());
}

// The tabletop is a fixed, composed view; orbit, zoom and pan belong to battles.
// A camera resting at the default zoom keeps the board fitted to the HUD-safe rect as the yaw changes (the fit differs per
// yaw: a long board needs far more distance end-on than side-on); the distance and its pitch then ease to the new fit.
// A zoom the player chose (nearer or farther than the fit) is kept.
void AExpeditionBattleView::Orbit(float Degrees)
{
    if (bOverview) { return; }
    // Mid-orbit the target is still the fit for the previous target yaw, so a consecutive orbit stays framed.
    bFramed = AtFramedZoom();
    TargetYaw += Degrees;
    if (bFramed) { TargetDistance = FramedDistance(TargetYaw); }
}
void AExpeditionBattleView::Zoom(float Amount) { if (bOverview) { return; } bFramed = false; TargetDistance = FMath::Clamp(TargetDistance + Amount, ExpeditionCameraRig::MinDistance, ExpeditionCameraRig::MaxDistance); }

void AExpeditionBattleView::Pan(float Right, float Away)
{
    if (bOverview) { return; }
    const float Radians = FMath::DegreesToRadians(TargetYaw);
    // The camera sits at (cos, sin) from the focus, so away from it is (-cos, -sin) and screen right is (sin, -cos).
    const FVector2D Step = (FVector2D(FMath::Sin(Radians), -FMath::Cos(Radians)) * Right + FVector2D(-FMath::Cos(Radians), -FMath::Sin(Radians)) * Away) * SceneScale();
    const FCombatEncounter& Encounter = Rules().Snapshot().Encounter;
    const FVector Base = BaseFocus();
    const FVector2D Clamped = ExpeditionCameraRig::ClampFocus(FVector2D(Base) + PanOffset + Step, Encounter.Width, Encounter.Height, BoardMetric.CellCm);
    PanOffset = Clamped - FVector2D(Base);
}

int32 AExpeditionBattleView::ShadowCastingLights() const
{
    int32 Count = 0;
    for (TActorIterator<ADirectionalLight> Sun(GetWorld()); Sun; ++Sun)
    { if (ULightComponent* Light = Sun->GetLightComponent()) { Count += Light->CastShadows ? 1 : 0; } }
    for (const UActorComponent* Component : Spawned)
    { if (const ULightComponent* Light = Cast<ULightComponent>(Component)) { Count += Light->CastShadows ? 1 : 0; } }
    return Count;
}

TArray<FVector> AExpeditionBattleView::ShadowedLightPositions() const
{
    TArray<FVector> Positions;
    for (const UActorComponent* Component : Spawned)
    { if (const ULightComponent* Light = Cast<ULightComponent>(Component)) { if (Light->CastShadows) { Positions.Add(Light->GetComponentLocation()); } } }
    return Positions;
}

float AExpeditionBattleView::FlameLightRadius() const
{
    for (const UPointLightComponent* Light : Flames) { if (Light) { return Light->AttenuationRadius; } }
    return 0.0f;
}

void AExpeditionBattleView::SelectShadowLights()
{
    if (bOverview) { return; }
    // The flame lights nearest the board centre cast shadows; ties go to the earlier light. Everything else stays unshadowed.
    const FCombatEncounter& Encounter = Rules().Snapshot().Encounter;
    const FVector Centre = GetActorTransform().TransformPosition(BoardPosition(FVector2D((Encounter.Width - 1) * 0.5f, (Encounter.Height - 1) * 0.5f)));
    TArray<TPair<float, int32>> Order;
    for (int32 Index = 0; Index < Flames.Num(); ++Index)
    {
        if (!Flames[Index]) { continue; }
        Order.Add(TPair<float, int32>(FVector::DistSquared2D(Flames[Index]->GetComponentLocation(), Centre), Index));
    }
    Order.Sort([](const TPair<float, int32>& A, const TPair<float, int32>& B) { return A.Key < B.Key || (A.Key == B.Key && A.Value < B.Value); });
    for (int32 Rank = 0; Rank < Order.Num(); ++Rank) { Flames[Order[Rank].Value]->SetCastShadows(Rank < MaxShadowedLocalLights); }
}

bool AExpeditionBattleView::CursorCell(APlayerController* Controller, FIntPoint& Cell) const
{
    FVector Origin, Direction;
    if (!Controller || !Controller->DeprojectMousePositionToWorld(Origin, Direction) || Direction.Z >= -KINDA_SMALL_NUMBER) { return false; }
    return CellAtRay(Origin, Direction, Cell);
}

bool AExpeditionBattleView::CellAtRay(const FVector& Origin, const FVector& Direction, FIntPoint& Cell) const
{
    if (Direction.Z >= -KINDA_SMALL_NUMBER) { return false; }
    // Board positions are relative to this actor; intersect the board plane in world space.
    const float PlaneZ = GetActorTransform().TransformPosition(FVector(0, 0, BoardMetric.CursorPlane())).Z;
    const FVector Intersection = GetActorTransform().InverseTransformPosition(Origin + Direction * ((PlaneZ - Origin.Z) / Direction.Z));
    Cell = BoardMetric.CellAt(Intersection);
    return Cell.X >= 0 && Cell.Y >= 0 && Cell.X < Rules().Snapshot().Encounter.Width && Cell.Y < Rules().Snapshot().Encounter.Height;
}

void AExpeditionBattleView::Record(const FCombatResult& Result)
{
    if (Result.Feedback.IsEmpty()) { return; }
    Feedback.Add((Result.bAccepted ? TEXT("") : TEXT("Cannot act: ")) + Result.Feedback);
    if (Feedback.Num() > 6) { Feedback.RemoveAt(0); }
}

void AExpeditionBattleView::Present(const TArray<FCombatResult>& Results)
{
    Show.Enqueue(Results, Rules().Snapshot());
    if (PlaybackSpeed == EPlaybackSpeed::Instant) { Show.Finish(); }
}

void AExpeditionBattleView::SkipPresentation() { Show.Finish(); }

void AExpeditionBattleView::CycleSpeed()
{
    PlaybackSpeed = PlaybackSpeed == EPlaybackSpeed::Normal ? EPlaybackSpeed::Fast : PlaybackSpeed == EPlaybackSpeed::Fast ? EPlaybackSpeed::Instant : EPlaybackSpeed::Normal;
    Show.Speed = PlaybackSpeed == EPlaybackSpeed::Fast ? 2.2f : 1.0f;
    if (PlaybackSpeed == EPlaybackSpeed::Instant) { Show.Finish(); }
    PlayCue(EExpeditionCue::Click);
}

bool AExpeditionBattleView::Execute(const FCombatCommand& Command)
{
    if (bShowRecords) { return false; }
    const FCombatantState* Unit = Rules().Find(Command.ActorId);
    if (!Unit || Unit->Side != ECombatSide::Party) { return false; }
    FCombatResult Result = Session.Submit(Command);
    if (!Result.bAccepted)
    {
        // The facts name what stops the action as a whole; the rules' own text stays for a target-specific failure they cannot explain.
        // Nothing changed, so the facts read the state the rejection was decided on.
        // A command the facts do not describe (cost None: the SRD-only Grapple, Shove and Escape, or one the kit lacks) keeps the rules' text.
        const FActionFacts Facts = FActionFactsQuery::For(Rules(), Command.ActorId, Command.Type);
        if (Facts.Cost != EActionCostKind::None && !Facts.Blockers.IsEmpty()) { Result.Feedback = ExpeditionStyle::BlockerText(Facts.Blockers[0]); }
    }
    Record(Result);
    // An accepted order completes whatever is still being shown (already resolved), then plays.
    // Rejected input leaves playback untouched, so stray clicks never skip animation.
    if (Result.bAccepted) { Show.Finish(); Present({Result}); }
    else { PlayCue(EExpeditionCue::Click); }
    return Result.bAccepted;
}

void AExpeditionBattleView::ChooseRoute(int32 Index)
{
    if (bShowRecords) { return; }
    FString Error;
    const auto& Offers = Session.Run().Progress().Offers;
    const int32 Id = Offers.IsValidIndex(Index) ? Offers[Index].Id : INDEX_NONE;
    if (Session.ChooseRoute(Id, Error))
    {
        Record({true, TEXT("Route committed. Injuries, uses and permanent deaths carry; no automatic rest.")});
        EnemyDelay = 0; RebuildBoard();
        const FCombatantState* First = Rules().Find(Rules().ActiveId());
        Show.ShowBanner(LocationName().ToUpper(), FString::Printf(TEXT("Encounter %d of %d  ·  %s acts first"), Progress().History.Num(), FExpeditionRun::EncounterCount,
            First ? *First->Name : TEXT("")), BoardTheme == EBoardTheme::Vault ? FLinearColor(0.5f, 0.85f, 1.0f) : FLinearColor(1.0f, 0.82f, 0.45f), 2.4f);
        PlayCue(EExpeditionCue::Route);
    }
    else { Record({false, Error}); PlayCue(EExpeditionCue::Click); }
}

void AExpeditionBattleView::ClaimReward(int32 RecipientId)
{
    if (bShowRecords) { return; }
    FString Error;
    const FCombatantState* Before = Rules().Find(RecipientId);
    const FCombatantState Previous = Before ? *Before : FCombatantState();
    if (Session.ClaimReward(RecipientId, Error))
    {
        Show.Finish();
        Record({true, TEXT("Reward claimed once for selected survivor. Continue at camp or choose the next route.")});
        Show.Sync(Rules().Snapshot());
        if (const FCombatantState* After = Rules().Find(RecipientId))
        {
            const FVector2D Cell(After->Cell.X, After->Cell.Y);
            Show.ShowText(Cell, FString::Printf(TEXT("+%d GEAR"), After->WeaponRank - Previous.WeaponRank), FLinearColor(1.0f, 0.82f, 0.3f), 1.2f, 2.2f);
            Show.ShowText(Cell, FString::Printf(TEXT("+%d POTIONS"), After->Potions - Previous.Potions), FLinearColor(0.5f, 1.0f, 0.6f), 1.0f, 2.2f);
            Show.ShowSparkles(Cell, FLinearColor(1.0f, 0.82f, 0.3f), 12);
        }
        PlayCue(EExpeditionCue::Loot);
    }
    else { Record({false, Error}); PlayCue(EExpeditionCue::Click); }
}

void AExpeditionBattleView::Recover(bool bTake)
{
    if (bShowRecords) { return; }
    FString Error;
    const TArray<FCombatantState> Before = Rules().Snapshot().Encounter.Combatants;
    if (Session.Recover(bTake, Error))
    {
        Record({true, bTake ? TEXT("Camp used: ") + FExpeditionRun::RecoveryDescription() : TEXT("Recovery skipped for this run. Choose the next route.")});
        Show.Sync(Rules().Snapshot());
        if (bTake)
        {
            for (const FCombatantState& Unit : Rules().Snapshot().Encounter.Combatants)
            {
                const FCombatantState* Old = Before.FindByPredicate([&Unit](const FCombatantState& Candidate) { return Candidate.Id == Unit.Id; });
                if (!Old || Unit.Side != ECombatSide::Party || Unit.Life != ECombatLife::Conscious) { continue; }
                const FVector2D Cell(Unit.Cell.X, Unit.Cell.Y);
                if (Unit.HP > Old->HP) { Show.ShowText(Cell, FString::Printf(TEXT("+%d"), Unit.HP - Old->HP), FLinearColor(0.4f, 1.0f, 0.55f), 1.2f, 2.0f); }
                Show.ShowSparkles(Cell, FLinearColor(1.0f, 0.7f, 0.35f), 6);
            }
            PlayCue(EExpeditionCue::Camp);
        }
        else { PlayCue(EExpeditionCue::Click); }
    }
    else { Record({false, Error}); PlayCue(EExpeditionCue::Click); }
}

void AExpeditionBattleView::NewRun(EFighterSubclass Subclass)
{
    if (HasRun() && Progress().Phase != EExpeditionPhase::Victory && Progress().Phase != EExpeditionPhase::Defeat)
    { Record({false, TEXT("Finish this expedition before starting a new run.")}); return; }
    FString Error, PreservedCheckpoint;
    const int32 NextSeed = HasRun() ? static_cast<int32>(static_cast<uint32>(Progress().Seed) + 1u) : 20261030;
    if (Session.NewRun(NextSeed, Error, FString(), &PreservedCheckpoint, Subclass))
    {
        Feedback.Reset();
        if (!PreservedCheckpoint.IsEmpty()) { Record({true, TEXT("The unreadable previous save was preserved. Its run and records could not be loaded.")}); }
        Record({true, TEXT("Fresh expedition: party power/resources reset. Choose a route.")});
        EnemyDelay = 0; TargetYaw = -90; bCheckpointBlocked = false; bShowRecords = false; RebuildBoard();
        Show.ShowBanner(TEXT("A NEW EXPEDITION"), TEXT("Recover the Waystone"), FLinearColor(1.0f, 0.82f, 0.45f), 2.2f);
        PlayCue(EExpeditionCue::Route);
    }
    else { Record({false, Error}); }
}

void AExpeditionBattleView::NewRun(const TArray<FPartyBuildSelection>& Party)
{
    if (HasRun() && Progress().Phase != EExpeditionPhase::Victory && Progress().Phase != EExpeditionPhase::Defeat)
    { Record({false, TEXT("Finish this expedition before starting a new run.")}); return; }
    FString Error, PreservedCheckpoint;
    const int32 NextSeed = HasRun() ? static_cast<int32>(static_cast<uint32>(Progress().Seed) + 1u) : 20261030;
    if (Session.NewRun(NextSeed, Party, Error, FString(), &PreservedCheckpoint))
    {
        Feedback.Reset();
        if (!PreservedCheckpoint.IsEmpty()) { Record({true, TEXT("The unreadable previous save was preserved. Its run and records could not be loaded.")}); }
        Record({true, TEXT("Fresh expedition: party power/resources reset. Choose a route.")});
        EnemyDelay = 0; TargetYaw = -90; bCheckpointBlocked = false; bShowRecords = false; RebuildBoard();
        Show.ShowBanner(TEXT("A NEW EXPEDITION"), TEXT("Recover the Waystone"), FLinearColor(1.0f, 0.82f, 0.45f), 2.2f);
        PlayCue(EExpeditionCue::Route);
    }
    else { Record({false, Error}); }
}

void AExpeditionBattleView::ConfigureStorage(const FString& Path)
{
    if (!HasRun()) { Session = FExpeditionSession(Path); }
}

void AExpeditionBattleView::Save()
{
    FString Error;
    if (Session.Save(Error)) { bCheckpointBlocked = false; Record({true, TEXT("Expedition saved. Every accepted action is also checkpointed.")}); PlayCue(EExpeditionCue::Click); }
    else { Record({false, Error}); }
}

void AExpeditionBattleView::Resume()
{
    FString Error;
    if (Session.Resume(Error))
    {
        EnemyDelay = 0; bCheckpointBlocked = false; bShowRecords = false; TargetYaw = -90;
        Feedback.Reset(); Record({true, TEXT("Expedition resumed: latest turns, resources and permanent losses retained.")}); RebuildBoard();
        Show.ShowBanner(TEXT("EXPEDITION RESUMED"), LocationName(), FLinearColor(1.0f, 0.82f, 0.45f), 2.0f);
        PlayCue(EExpeditionCue::Route);
    }
    else { Record({false, Error}); }
}

bool AExpeditionBattleView::CanReturnToMenu() const
{
    if (!HasRun() || bShowRecords) { return false; }
    const EExpeditionPhase Phase = Progress().Phase;
    return Phase != EExpeditionPhase::Victory && Phase != EExpeditionPhase::Defeat;
}

void AExpeditionBattleView::ReturnToMenu()
{
    if (!CanReturnToMenu()) { return; }
    // Every accepted action is already committed; only presentation is dropped here.
    Session.Close();
    bUnfinishedCheckpoint = Session.HasUnfinishedCheckpoint();
    EnemyDelay = 0; bCheckpointBlocked = false; TargetYaw = -90;
    Feedback.Reset(); Record({true, TEXT("Expedition kept at its last checkpoint. Press O to resume it.")}); RebuildBoard();
    PlayCue(EExpeditionCue::Click);
}

void AExpeditionBattleView::ToggleRecords()
{
    if (bShowRecords) { bShowRecords = false; return; }
    FString Error;
    if (Session.ReadRecords(EndingRecords, Error)) { bShowRecords = true; RecordsOffset = 0; PlayCue(EExpeditionCue::Click); }
    else { Record({false, Error}); }
}

void AExpeditionBattleView::PageRecords(int32 Direction)
{
    if (!bShowRecords) { return; }
    const int32 LastPage = EndingRecords.IsEmpty() ? 0 : ((EndingRecords.Num() - 1) / RecordsPerPage) * RecordsPerPage;
    RecordsOffset = FMath::Clamp(RecordsOffset + FMath::Clamp(Direction, -1, 1) * RecordsPerPage, 0, LastPage);
}

void AExpeditionBattleView::PlayCue(EExpeditionCue Cue)
{
    UWorld* World = GetWorld();
    if (!World || !World->bAllowAudioPlayback || !GEngine || !GEngine->UseSound()) { return; }
    const int32 Variation = SoundVariation++ % 4;
    const int32 Key = static_cast<int32>(Cue) * 8 + Variation;
    const TArray<int16>* Pcm = SoundCache.Find(Key);
    if (!Pcm) { Pcm = &SoundCache.Add(Key, ExpeditionAudio::Synthesize(Cue, Variation)); }
    if (Pcm->IsEmpty()) { return; }
    USoundWaveProcedural* Wave = NewObject<USoundWaveProcedural>(this);
    Wave->SetSampleRate(ExpeditionAudio::SampleRate);
    Wave->NumChannels = 1;
    Wave->Duration = Pcm->Num() / static_cast<float>(ExpeditionAudio::SampleRate);
    Wave->SoundGroup = SOUNDGROUP_Default;
    Wave->bLooping = false;
    Wave->QueueAudio(reinterpret_cast<const uint8*>(Pcm->GetData()), Pcm->Num() * sizeof(int16));
    if (UAudioComponent* Voice = UGameplayStatics::SpawnSound2D(this, Wave, ExpeditionAudio::Volume(Cue)))
    {
        Voices.Add(Voice);
        VoiceEnds.Add(Clock + Wave->Duration + 0.15f);
    }
}

void AExpeditionBattleView::ExpireVoices()
{
    for (int32 Index = Voices.Num() - 1; Index >= 0; --Index)
    {
        if (VoiceEnds[Index] > Clock && IsValid(Voices[Index])) { continue; }
        if (IsValid(Voices[Index])) { Voices[Index]->Stop(); }
        Voices.RemoveAt(Index);
        VoiceEnds.RemoveAt(Index);
    }
}

void AExpeditionBattleView::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    Clock += DeltaSeconds;
    if (!bShowRecords) { Show.Tick(DeltaSeconds); }
    // Route choice (after loot, camp, a new run or a resume) moves to the tabletop once playback settles.
    if (Theme && WantsOverview() != bOverview && !IsPresenting() && Show.Texts().IsEmpty() && Show.Sparkles().IsEmpty()) { RebuildBoard(); }
    UpdateRigs(); UpdateTiles(); UpdateOverlay(); UpdateMarkers(); UpdateEffects(); UpdateSway(); UpdateCamera(DeltaSeconds); UpdateOccluder(); FlushGroups();
    ExpireVoices();
    if (!HasRun() || bShowRecords || bCheckpointBlocked || bHoldEnemyTurns) { return; }
    const FCombatantState* Active = Rules().Find(Rules().ActiveId());
    if (Progress().Phase != EExpeditionPhase::Combat || Rules().Snapshot().Phase != EBattlePhase::Fighting || !Active || Active->Side != ECombatSide::Enemy || IsPresenting())
    { EnemyDelay = 0; return; }
    // Enemies take a visible beat to "think", then their whole turn plays out step by step.
    EnemyDelay += DeltaSeconds;
    if (EnemyDelay >= EnemyThinkSeconds / FMath::Max(1.0f, Show.Speed))
    {
        const TArray<FCombatResult> Results = Session.PlayEnemyTurn();
        // A failed checkpoint commits nothing and returns only its error; playback skips it.
        for (const FCombatResult& Result : Results) { Record(Result); if (!Result.bAccepted) { bCheckpointBlocked = true; } }
        Present(Results);
        EnemyDelay = 0;
    }
}

AExpeditionGameMode::AExpeditionGameMode()
{
    PlayerControllerClass = AExpeditionPlayerController::StaticClass();
    HUDClass = AExpeditionHUD::StaticClass();
    DefaultPawnClass = nullptr;
}

// ---------------------------------------------------------------------------------------
// Dark-fantasy theme: full surrounding worlds, themed figures and the tabletop overview.

UStaticMesh* AExpeditionBattleView::ThemeMesh(const TSoftObjectPtr<UStaticMesh>& Entry) const
{
    return Theme ? UDiceRollTheme::Load(Entry) : nullptr;
}

UStaticMesh* AExpeditionBattleView::UnitMesh(const FCombatantState& Unit) const
{
    if (!Theme) { return nullptr; }
    if (Unit.Side == ECombatSide::Party)
    {
        switch (Unit.AdventurerRole)
        {
        case EAdventurerRole::Fighter: return ThemeMesh(Theme->Fighter);
        case EAdventurerRole::Rogue: return ThemeMesh(Theme->Rogue);
        case EAdventurerRole::Cleric: return ThemeMesh(Theme->Cleric);
        case EAdventurerRole::Wizard: return ThemeMesh(Theme->Wizard);
        default: return nullptr;
        }
    }
    if (Unit.Range > 1) { return ThemeMesh(Theme->Archer); }
    const bool bRaider = Unit.Name.Contains(TEXT("raider"));
    return ThemeMesh(bRaider ? Theme->Raider : Theme->Scout);
}

bool AExpeditionBattleView::WantsOverview() const
{
    return OverviewTable && (!HasRun() || Progress().Phase == EExpeditionPhase::Route);
}

bool AExpeditionBattleView::BuildThemedScenery(const FCombatEncounter& Encounter, int32 Recipe, int32 Seed)
{
    const bool bForest = BoardTheme == EBoardTheme::Meadow;
    UStaticMesh* World = ThemeMesh(bForest ? Theme->RootWorld : Theme->VigilWorld);
    if (!World) { return false; }
    // No per-cell tiles (#142): the world's own flat ground is the playing surface, so it sits at board level and the
    // contextual grid draws on it. The theme's tile meshes are no longer placed (primitive boards keep their cube tiles).
    const float Ground = RefTop;
    const FVector Center = SceneBoard(FVector2D((Encounter.Width - 1) * 0.5f, (Encounter.Height - 1) * 0.5f));
    // The world surrounds an 18 x 12 footprint; a dark underlay hides the sandbox past its edge.
    PlaceProp(Cube, FVector(Center.X, Center.Y, Ground - 60.0f), FVector(600.0f, 600.0f, 0.1f), FLinearColor(0.01f, 0.01f, 0.012f));
    // The r002 kit is authored at the true 150 cm cell, so under the root the metric scales it is placed at 1 / scale.
    // The r001 art (110 cm) keeps the root's scale. Only Vigil boards use the r002 world; every board uses the r002 obstacles.
    const bool bMetric = Theme->bMetricKit;
    const bool bVigil = bMetric && !bForest;
    const FVector MetricScale(1.0f / SceneScale());
    PlaceProp(World, FVector(Center.X, Center.Y, Ground), bVigil ? MetricScale : FVector::OneVector, FLinearColor::White);
    bVigilKit = bVigil;
    if (bVigil)
    {
        if (UStaticMesh* Near = ThemeMesh(Theme->VigilOccluders))
        {
            Occluder = PlaceOccluder(Near, FVector(Center.X, Center.Y, Ground), MetricScale);
        }
    }
    // The world's ground covers the 18 x 12 footprint, the largest generated board. A custom larger board gets one subtle
    // plain plane just under the surface (never a checkerboard).
    if (Encounter.Width > 18 || Encounter.Height > 12)
    {
        PlaceProp(Cube, FVector(Center.X, Center.Y, Ground - 1.2f),
            FVector(Encounter.Width * RefCell / 100.0f + 0.2f, Encounter.Height * RefCell / 100.0f + 0.2f, 0.02f), FLinearColor(0.07f, 0.07f, 0.08f));
    }
    FRandomStream Random(Seed);
    const int32 Kind = HasRun() && !Progress().History.IsEmpty() ? Recipe : 0;
    // r002 obstacles choose a variant from the seed and the cell with a private hash; the random stream below is only
    // consumed by the r001 path, so the kit never changes what the fallback draws.
    if (!(bMetric && BuildKitObstacles(Encounter, Kind, Seed, bVigil && VigilKit::ArchesFor(Kind))))
    {
        UStaticMesh* Obstacle = ThemeMesh(Kind == 1 ? Theme->Pillar : Kind == 2 ? Theme->Wall : Theme->Stones);
        for (const FIntPoint& Cell : Encounter.BlockedCells)
        {
            if (!Obstacle) { BuildObstacle(Cell, Kind, Random); continue; }
            const float Turn = 90.0f * Random.RandRange(0, 3) + (Kind == 0 ? Random.FRandRange(-20.0f, 20.0f) : 0.0f);
            PlaceProp(Obstacle, SceneBoard(FVector2D(Cell.X, Cell.Y)), bMetric ? MetricScale : FVector::OneVector,
                FLinearColor::White * Random.FRandRange(0.85f, 1.0f), FRotator(0.0f, Turn, 0.0f));
        }
    }
    if (bVigil) { BuildKitDressing(Encounter, Seed); }
    // Braziers ring the board and banners mark the far edge; flames flicker, banners sway.
    UStaticMesh* Brazier = ThemeMesh(Theme->Brazier);
    UStaticMesh* Flame = ThemeMesh(Theme->Flame);
    UStaticMesh* Banner = ThemeMesh(Theme->Banner);
    const float W = Encounter.Width, H = Encounter.Height;
    const float BowlTop = Brazier ? Brazier->GetBounds().GetBox().Max.Z : 90.0f;
    const FVector2D Braziers[] = {{-1.3f, -1.3f}, {W + 0.3f, -1.3f}, {-1.3f, H + 0.3f}, {W + 0.3f, H + 0.3f},
        {(W - 1) * 0.5f, H + 0.5f}, {-1.5f, (H - 1) * 0.5f}, {W + 0.5f, (H - 1) * 0.5f}};
    for (const FVector2D& Spot : Braziers)
    {
        const FVector At = SceneBoard(Spot);
        if (Brazier) { PlaceProp(Brazier, At, FVector::OneVector, FLinearColor::White); }
        if (Flame)
        {
            AddSway(PlaceProp(Flame, At + FVector(0, 0, BowlTop - 4.0f), FVector::OneVector, FLinearColor::White, FRotator::ZeroRotator, false), 0.0f, 0.16f);
        }
        Flames.Add(MakeLight(nullptr, At + FVector(0, 0, BowlTop + 70.0f), FLinearColor(1.0f, 0.55f, 0.25f), 40.0f, 900.0f));
        FlameStrength.SetNum(Flames.Num());
        FlameStrength.Last() = 40.0f;
    }
    // Iron cages hang on chains from the cathedral's unseen vault and swing slowly.
    if (UStaticMesh* Cage = ThemeMesh(Theme->Cage); Cage && !bForest)
    {
        for (const FVector2D Spot : {FVector2D(-1.6f, H * 0.3f), FVector2D(W + 0.6f, H * 0.65f)})
        {
            AddSway(PlaceProp(Cage, SceneBoard(Spot, 430.0f), FVector::OneVector, FLinearColor::White), 6.0f, 0.0f);
        }
    }
    if (Banner)
    {
        for (const float X : {W * 0.22f, W * 0.78f - 1.0f})
        {
            AddSway(PlaceProp(Banner, SceneBoard(FVector2D(X, H + 1.3f)), FVector::OneVector, FLinearColor::White, FRotator(0, -90, 0)), 3.5f, 0.0f);
        }
    }
    return true;
}

FExpeditionPart AExpeditionBattleView::MakeOwnMaterialPart(UStaticMesh* Mesh, FVector Location, FVector Scale, FRotator Rotation)
{
    FExpeditionPart Part = MakePart(Mesh, nullptr, Location, Scale, FLinearColor::White, Rotation);
    if (UMaterialInterface* Own = Mesh ? Mesh->GetMaterial(0) : nullptr)
    {
        // The mesh's own material (dithered mask or translucent card) with an instance for per-part parameters.
        Part.Material = UMaterialInstanceDynamic::Create(Own, this);
        Part.Mesh->SetMaterial(0, Part.Material);
    }
    return Part;
}

bool AExpeditionBattleView::BuildKitObstacles(const FCombatEncounter& Encounter, int32 Recipe, int32 Seed, bool bArches)
{
    const TArray<TSoftObjectPtr<UStaticMesh>>& Entries = Recipe == 1 ? Theme->PillarVariants : Recipe == 2 ? Theme->WallVariants : Theme->StonesVariants;
    TArray<UStaticMesh*> Variants;
    bool bAny = false;
    for (const TSoftObjectPtr<UStaticMesh>& Entry : Entries) { Variants.Add(ThemeMesh(Entry)); bAny = bAny || Variants.Last() != nullptr; }
    if (!bAny) { return false; }
    const FVector MetricScale(1.0f / SceneScale());
    TSet<FIntPoint> Covered;
    if (UStaticMesh* Arch = bArches ? ThemeMesh(Theme->ArchTwoCell) : nullptr)
    {
        for (const VigilKit::FArch& Pair : VigilKit::ArchPairs(Encounter.BlockedCells))
        {
            const FExpeditionProp Piece = PlaceProp(Arch, SceneBoard(Pair.Midpoint()), MetricScale, FLinearColor::White, FRotator(0.0f, Pair.Yaw(), 0.0f));
            Covered.Add(Pair.A); Covered.Add(Pair.B);
            VariantsByCell.Add(Pair.A, -1); VariantsByCell.Add(Pair.B, -1);
            const FVector Spot = PropActorPosition(Piece);
            SpotsByCell.Add(Pair.A, Spot); SpotsByCell.Add(Pair.B, Spot);
            ++ArchPieces; ++ObstaclePieces;
        }
    }
    for (const FIntPoint& Cell : Encounter.BlockedCells)
    {
        if (Covered.Contains(Cell)) { continue; }
        // The chosen variant, or the next one that loaded.
        const int32 Wanted = VigilKit::VariantIndex(Seed, Recipe, Cell, Variants.Num());
        int32 Index = INDEX_NONE;
        for (int32 Step = 0; Step < Variants.Num() && Index == INDEX_NONE; ++Step)
        { if (Variants[(Wanted + Step) % Variants.Num()]) { Index = (Wanted + Step) % Variants.Num(); } }
        if (Index == INDEX_NONE) { continue; }
        const float Turn = 90.0f * static_cast<float>(VigilKit::Hash(Seed, 2, Cell) % 4u) + (Recipe == 0 ? (VigilKit::Unit(Seed, 3, Cell) - 0.5f) * 40.0f : 0.0f);
        const float Tint = 0.85f + 0.15f * VigilKit::Unit(Seed, 4, Cell);
        const FExpeditionProp Piece = PlaceProp(Variants[Index], SceneBoard(FVector2D(Cell.X, Cell.Y)), MetricScale, FLinearColor::White * Tint, FRotator(0.0f, Turn, 0.0f));
        VariantsByCell.Add(Cell, Index);
        SpotsByCell.Add(Cell, PropActorPosition(Piece));
        ++ObstaclePieces;
    }
    return true;
}

void AExpeditionBattleView::BuildKitDressing(const FCombatEncounter& Encounter, int32 Seed)
{
    auto Load = [this](const TArray<TSoftObjectPtr<UStaticMesh>>& Entries)
    {
        TArray<UStaticMesh*> Meshes;
        for (const TSoftObjectPtr<UStaticMesh>& Entry : Entries) { Meshes.Add(ThemeMesh(Entry)); }
        return Meshes;
    };
    const TArray<UStaticMesh*> Scatter = Load(Theme->Scatter), Mist = Load(Theme->MistCards), Shafts = Load(Theme->LightShafts);
    VigilKit::FDressingKinds Kinds;
    Kinds.Scatter = Scatter.Num(); Kinds.Mist = Mist.Num(); Kinds.Shaft = Shafts.Num();
    const FVector MetricScale(1.0f / SceneScale());
    for (const VigilKit::FDressing& Plan : VigilKit::PlanDressing(Encounter.Width, Encounter.Height, Seed, Kinds))
    {
        const TArray<UStaticMesh*>& Set = Plan.Kind == VigilKit::EDressingKind::Scatter ? Scatter : Plan.Kind == VigilKit::EDressingKind::Mist ? Mist : Shafts;
        UStaticMesh* Mesh = Set.IsValidIndex(Plan.Variant) ? Set[Plan.Variant] : nullptr;
        if (!Mesh) { continue; }
        const FVector Local = SceneBoard(Plan.Position());
        const FRotator Turn(0.0f, Plan.Yaw, 0.0f);
        // Translucent cards (mist and shafts) keep their own material and cast no shadow.
        const bool bCard = Plan.Kind != VigilKit::EDressingKind::Scatter;
        const FExpeditionProp Piece = PlaceProp(Mesh, Local, MetricScale, FLinearColor::White, Turn, !bCard, bCard);
        FPlacedDressing Placed;
        Placed.Plan = Plan;
        Placed.Position = PropActorPosition(Piece);
        Dressing.Add(Placed);
    }
}

bool AExpeditionBattleView::HasOccluder() const { return IsValid(Occluder.Mesh); }

void AExpeditionBattleView::OccluderSight(FVector& Eye, FVector& FocusPoint, FVector& UnitPoint) const
{
    const FTransform ActorSpace = GetActorTransform();
    Eye = Camera->GetComponentLocation();
    FocusPoint = ActorSpace.TransformPosition(Focus);
    UnitPoint = FocusPoint;
    if (InCombat())
    {
        const int32 Id = Rules().ActiveId();
        if (Rules().Find(Id) && Show.Unit(Id)) { UnitPoint = ActorSpace.TransformPosition(UnitPosition(Id, BoardMetric.Figure(0.6f))); }
    }
}

float AExpeditionBattleView::OccluderOpacityAt(const FVector& WorldPoint) const
{
    if (!HasOccluder()) { return 1.0f; }
    FVector Eye, FocusPoint, UnitPoint;
    OccluderSight(Eye, FocusPoint, UnitPoint);
    return OccluderFade::Lowest(WorldPoint, Eye, FocusPoint, UnitPoint, BoardMetric.CellCm * OccluderFade::RadiusCells);
}

void AExpeditionBattleView::OccluderBoard(FVector2D& Min, FVector2D& Max, float& PlaneZ) const
{
    const FCombatEncounter& Board = Rules().Snapshot().Encounter;
    const float Margin = OccluderFade::BoardMarginCells;
    const FTransform ActorSpace = GetActorTransform();
    const FVector Low = ActorSpace.TransformPosition(BoardPosition(FVector2D(-0.5f - Margin, -0.5f - Margin)));
    const FVector High = ActorSpace.TransformPosition(BoardPosition(FVector2D(Board.Width - 0.5f + Margin, Board.Height - 0.5f + Margin)));
    // The axis-aligned bounds of the two corners, so a view actor turned a quarter circle still gives a min and a max.
    Min = FVector2D(FMath::Min(Low.X, High.X), FMath::Min(Low.Y, High.Y));
    Max = FVector2D(FMath::Max(Low.X, High.X), FMath::Max(Low.Y, High.Y));
    PlaneZ = static_cast<float>(ActorSpace.TransformPosition(BoardPosition(FVector2D::ZeroVector)).Z);
}

float AExpeditionBattleView::OccluderBoardOpacityAt(const FVector& WorldPoint) const
{
    if (!HasOccluder()) { return 1.0f; }
    FVector2D Min, Max;
    float PlaneZ = 0.0f;
    OccluderBoard(Min, Max, PlaneZ);
    return OccluderFade::Board(WorldPoint, Camera->GetComponentLocation(), Min, Max, PlaneZ, BoardMetric.CellCm * OccluderFade::BoardRampCells);
}

void AExpeditionBattleView::UpdateOccluder()
{
    if (!Occluder.Material) { return; }
    FVector Eye, FocusPoint, UnitPoint;
    OccluderSight(Eye, FocusPoint, UnitPoint);
    auto Point = [](const FVector& Value) { return FLinearColor(static_cast<float>(Value.X), static_cast<float>(Value.Y), static_cast<float>(Value.Z), 0.0f); };
    Occluder.Material->SetVectorParameterValue(TEXT("SightEye"), Point(Eye));
    Occluder.Material->SetVectorParameterValue(TEXT("SightFocus"), Point(FocusPoint));
    Occluder.Material->SetVectorParameterValue(TEXT("SightUnit"), Point(UnitPoint));
    Occluder.Material->SetScalarParameterValue(TEXT("FadeRadius"), BoardMetric.CellCm * OccluderFade::RadiusCells);
    Occluder.Material->SetScalarParameterValue(TEXT("FadeMin"), OccluderFade::MinOpacity);
    // The board rectangle only changes with the board, its metric or the view actor's transform: push it when it does.
    FVector2D Min, Max;
    float PlaneZ = 0.0f;
    OccluderBoard(Min, Max, PlaneZ);
    const FVector4 Rect(Min.X, Min.Y, Max.X, Max.Y);
    if (!bOccluderBoardSent || !Rect.Equals(OccluderBoardSent, 0.01) || !FMath::IsNearlyEqual(PlaneZ, OccluderPlaneSent, 0.01f))
    {
        Occluder.Material->SetVectorParameterValue(TEXT("BoardRect"), FLinearColor(static_cast<float>(Min.X), static_cast<float>(Min.Y), static_cast<float>(Max.X), static_cast<float>(Max.Y)));
        Occluder.Material->SetScalarParameterValue(TEXT("BoardZ"), PlaneZ);
        Occluder.Material->SetScalarParameterValue(TEXT("BoardRamp"), BoardMetric.CellCm * OccluderFade::BoardRampCells);
        OccluderBoardSent = Rect; OccluderPlaneSent = PlaneZ; bOccluderBoardSent = true;
    }
}

void AExpeditionBattleView::BuildOverview()
{
    // The table stands on a dark floor raised over the sandbox floor (top at Z=0); everything
    // past the candlelight falls away.
    const FVector Lift(0.0f, 0.0f, OverviewHeight);
    Scenery.Add(MakePart(Cube, nullptr, FVector(0, 0, 5.0f), FVector(300.0f, 300.0f, 0.1f), FLinearColor(0.008f, 0.006f, 0.005f)));
    // Assets face +X; the camera looks from -Y, so turn everything to face it.
    const FRotator Toward(0.0f, -90.0f, 0.0f);
    Scenery.Add(MakePart(OverviewTable, nullptr, Lift, FVector::OneVector, FLinearColor::White, Toward));
    // One low-detail diorama per offered road: the forest for guarded roads, the drowned
    // cathedral for dangerous roads and the finale. The title shows both.
    TArray<bool> Cathedral;
    if (HasRun())
    {
        const bool bFinale = Progress().EncountersCompleted + 1 >= FExpeditionRun::EncounterCount;
        for (const FExpeditionRoute& Route : Progress().Offers) { Cathedral.Add(Route.bDangerous || bFinale); }
    }
    else { Cathedral = {false, true}; }
    for (int32 Index = 0; Index < Cathedral.Num(); ++Index)
    {
        UStaticMesh* Diorama = ThemeMesh(Cathedral[Index] ? Theme->VigilDiorama : Theme->RootDiorama);
        if (!Diorama) { continue; }
        // The camera looks along +Y, so world +X is screen-left: offer 0 sits left, under its card.
        const float X = Cathedral.Num() == 1 ? 0.0f : Index == 0 ? 52.0f : -52.0f;
        Scenery.Add(MakePart(Diorama, nullptr, Lift + FVector(X, 22.0f, 0.0f), FVector::OneVector, FLinearColor::White,
            Toward + FRotator(0.0f, Index == 0 ? -10.0f : 10.0f, 0.0f)));
    }
    // The surviving party stands on the map as painted miniatures.
    int32 Slot = 0;
    for (const FCombatantState& Unit : Rules().Snapshot().Encounter.Combatants)
    {
        if (Unit.Side != ECombatSide::Party || Unit.Life == ECombatLife::Dead) { continue; }
        if (UStaticMesh* Figure = UnitMesh(Unit))
        {
            Scenery.Add(MakePart(Figure, nullptr, Lift + FVector(27.0f - Slot * 18.0f, -30.0f, 0.0f), FVector(0.14f), FLinearColor::White, Toward));
        }
        ++Slot;
    }
    for (const FVector& Candle : {FVector(-100, 30, 40), FVector(100, 34, 40), FVector(0, -70, 70)})
    {
        Flames.Add(MakeLight(nullptr, Lift + Candle, FLinearColor(1.0f, 0.62f, 0.32f), 16.0f, 380.0f));
        FlameStrength.SetNum(Flames.Num());
        FlameStrength.Last() = 16.0f;
    }
}

void AExpeditionBattleView::AddSway(USceneComponent* Component, float Degrees, float Flicker)
{
    if (!Component) { return; }
    FExpeditionSway Sway;
    Sway.Component = Component;
    Sway.Base = Component->GetRelativeRotation();
    Sway.Scale = Component->GetRelativeScale3D();
    Sway.Phase = Hash(Swaying.Num(), 17) * 2.0f * PI;
    Sway.Degrees = Degrees;
    Sway.Flicker = Flicker;
    Swaying.Add(Sway);
}

void AExpeditionBattleView::UpdateSway()
{
    for (const FExpeditionSway& Sway : Swaying)
    {
        if (Sway.Instance.IsValid())
        {
            // An instanced prop sways through its instance transform (#144); the group is flushed once at the end of the frame.
            FRotator Turn = Sway.Base;
            FVector Size = Sway.Scale;
            if (Sway.Degrees > 0.0f)
            {
                const float Roll = FMath::Sin(Clock * 1.3f + Sway.Phase) * Sway.Degrees;
                const float Pitch = FMath::Sin(Clock * 0.9f + Sway.Phase * 1.7f) * Sway.Degrees * 0.4f;
                Turn = Sway.Base + FRotator(Pitch, 0.0f, Roll);
            }
            if (Sway.Flicker > 0.0f)
            {
                const float Lick = 0.6f * FMath::Sin(Clock * 13.0f + Sway.Phase) + 0.4f * FMath::Sin(Clock * 23.0f + Sway.Phase * 2.0f);
                Size = Sway.Scale * FVector(1.0f - Sway.Flicker * 0.3f * Lick, 1.0f - Sway.Flicker * 0.3f * Lick, 1.0f + Sway.Flicker * Lick);
            }
            UpdateInstance(Sway.Instance, FTransform(Turn, Sway.Location, Size), {});
            continue;
        }
        if (!Sway.Component) { continue; }
        if (Sway.Degrees > 0.0f)
        {
            const float Roll = FMath::Sin(Clock * 1.3f + Sway.Phase) * Sway.Degrees;
            const float Pitch = FMath::Sin(Clock * 0.9f + Sway.Phase * 1.7f) * Sway.Degrees * 0.4f;
            Sway.Component->SetRelativeRotation(Sway.Base + FRotator(Pitch, 0.0f, Roll));
        }
        if (Sway.Flicker > 0.0f)
        {
            const float Lick = 0.6f * FMath::Sin(Clock * 13.0f + Sway.Phase) + 0.4f * FMath::Sin(Clock * 23.0f + Sway.Phase * 2.0f);
            Sway.Component->SetRelativeScale3D(Sway.Scale * FVector(1.0f - Sway.Flicker * 0.3f * Lick, 1.0f - Sway.Flicker * 0.3f * Lick, 1.0f + Sway.Flicker * Lick));
        }
    }
}

// ---------------------------------------------------------------------------------------
// Contextual grid and footprint rings (#142). Thin bars on the board plane, instanced per style; no per-cell meshes.

UInstancedStaticMeshComponent* AExpeditionBattleView::MakeBarSet(FExpeditionPart& Part, USceneComponent* Parent, FLinearColor Color, float Opacity)
{
    UInstancedStaticMeshComponent* Set = NewObject<UInstancedStaticMeshComponent>(this);
    AddInstanceComponent(Set);
    Spawned.Add(Set);
    Set->SetupAttachment(Parent ? Parent : SceneryRoot.Get());
    Set->SetMobility(EComponentMobility::Movable);
    Set->SetStaticMesh(Cube);
    Set->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Set->SetCastShadow(false);
    Set->bReceivesDecals = false;
    Set->SetCanEverAffectNavigation(false);
    Part.Mesh = Set;
    Part.Base = Color;
    UMaterialInterface* Look = OverlayMaterial ? OverlayMaterial.Get() : BaseMaterial.Get();
    if (Look)
    {
        Part.Material = UMaterialInstanceDynamic::Create(Look, this);
        Set->SetMaterial(0, Part.Material);
        // Without the translucent material a faint line is drawn dimmer instead, so it never reads as a solid one.
        Part.SetColor(OverlayMaterial ? Color : Color * Opacity);
        if (OverlayMaterial) { Part.Material->SetScalarParameterValue(TEXT("Opacity"), Opacity); }
    }
    Set->RegisterComponent();
    return Set;
}

FExpeditionPart AExpeditionBattleView::MakeRing(USceneComponent* Parent, FVector Location, FLinearColor Color)
{
    FExpeditionPart Part;
    if (!Cube) { return Part; }
    ++LogicalParts;
    UInstancedStaticMeshComponent* Set = MakeBarSet(Part, Parent, Color, 1.0f);
    Set->SetRelativeLocation(Location);
    // A ring 100 cm across at unit scale: a polygon of thin bars, slightly overlapped so no gap shows. The component
    // scale makes it any diameter (and thickness stays proportional).
    TArray<FTransform> Bars;
    const float Side = 100.0f * FMath::Sin(PI / RingSegments);
    for (int32 Index = 0; Index < RingSegments; ++Index)
    {
        const float Angle = (Index + 0.5f) * 2.0f * PI / RingSegments;
        Bars.Add(FTransform(FRotator(0.0f, FMath::RadiansToDegrees(Angle) + 90.0f, 0.0f),
            FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f) * 50.0f, FVector(Side * 1.1f / 100.0f, 0.05f, 0.012f)));
    }
    Set->AddInstances(Bars, false, false, false);
    return Part;
}

float AExpeditionBattleView::FootprintRingDiameterCm(int32 UnitId) const
{
    for (const FExpeditionUnitRig& Rig : Rigs)
    {
        if (Rig.Id != UnitId) { continue; }
        if (Rig.Ring.RingSlot != INDEX_NONE && RingBank)
        {
            // Read from the bars themselves: two opposite bars of a 24-bar ring stand a diameter apart.
            FTransform A, B;
            const int32 Start = Rig.Ring.RingSlot * RingSegments;
            if (RingBank->GetInstanceTransform(Start, A, false) && RingBank->GetInstanceTransform(Start + RingSegments / 2, B, false))
            { return static_cast<float>(FVector::Dist(A.GetLocation(), B.GetLocation())); }
            return 0.0f;
        }
        if (Rig.Ring.Mesh) { return Rig.Ring.Mesh->GetRelativeScale3D().X * 100.0f; }
    }
    return 0.0f;
}

USceneComponent* AExpeditionBattleView::UnitRigRoot(int32 UnitId) const
{
    for (const FExpeditionUnitRig& Rig : Rigs) { if (Rig.Id == UnitId) { return Rig.Root; } }
    return nullptr;
}

int32 AExpeditionBattleView::OverlayBars(BoardOverlay::EStyle Style) const
{
    return OverlayCounts.IsValidIndex(static_cast<int32>(Style)) ? OverlayCounts[static_cast<int32>(Style)] : 0;
}

void AExpeditionBattleView::BuildOverlay()
{
    // Themed battle boards only: primitive boards keep the tinted cube tiles and the HUD markers.
    if (!Theme || bOverview || !Cube) { return; }
    if (CanBankRings())
    {
        // One component draws every ring, marker and grid bar (#144): the grid's bars follow the rings in the bank, each style's
        // colour and opacity riding in its bars' custom data. Rings are all built by now, so the bars start after them.
        EnsureBank();
        OverlayBase = RingBank ? RingBank->GetInstanceCount() : INDEX_NONE;
        OverlayCounts.SetNumZeroed(static_cast<int32>(BoardOverlay::EStyle::Count));
        return;
    }
    for (int32 Index = 0; Index < static_cast<int32>(BoardOverlay::EStyle::Count); ++Index)
    {
        const FOverlayLook Look = OverlayLook(static_cast<BoardOverlay::EStyle>(Index));
        FExpeditionPart Part;
        MakeBarSet(Part, RootComponent, Look.Color, Look.Opacity);
        Part.Mesh->SetVisibility(false);
        OverlaySets.Add(Part);
    }
    OverlayCounts.SetNumZeroed(OverlaySets.Num());
}

void AExpeditionBattleView::UpdateOverlay()
{
    const FCombatEncounter& Encounter = Rules().Snapshot().Encounter;
    // Choices show only while the party can act and nothing is playing back, like the tile tints they replace.
    const bool bChoices = InCombat() && !IsPresenting() && !bOverview;
    const bool bGrid = Highlights.bFullGrid && InCombat() && !bOverview;
    static const FBoardHighlights NoChoices;
    const FBoardHighlights& Source = bChoices ? Highlights : NoChoices;
    // Scratch storage is reused, so a frame in which nothing changed allocates nothing and builds no mask.
    OverlayVictims.Reset();
    if (bChoices)
    {
        for (const int32 Id : Highlights.Victims)
        { if (const FCombatantState* Unit = Rules().Find(Id)) { OverlayVictims.Add(Unit->Cell); } }
    }
    // Exactly the inputs BuildMask and BuildBars read.
    const FBoardHighlights& Last = OverlayInput;
    const bool bSameInput = Source.Moves == Last.Moves && Source.Reach.Visible == Last.Reach.Visible
        && Source.Reach.Obstructed == Last.Reach.Obstructed && Source.Path == Last.Path
        && Source.bHasHover == Last.bHasHover && (!Source.bHasHover || Source.Hover == Last.Hover)
        && Source.bHasOrigin == Last.bHasOrigin && (!Source.bHasOrigin || Source.Origin == Last.Origin)
        && OverlayVictims == OverlayVictimsShown;
    const bool bChanged = !bOverlayBuilt || !bSameInput || bGrid != bFullGridShown
        || Encounter.Width != OverlayState.Width || Encounter.Height != OverlayState.Height;
    if (!bChanged) { return; }
    OverlayInput = Source;
    OverlayVictimsShown = OverlayVictims;
    bFullGridShown = bGrid;
    bOverlayBuilt = true;
    OverlayState = BoardOverlay::BuildMask(Encounter.Width, Encounter.Height, OverlayInput, OverlayVictimsShown);
    if (OverlaySets.IsEmpty() && OverlayBase == INDEX_NONE) { return; }

    TArray<BoardOverlay::FBar> Bars;
    BoardOverlay::BuildBars(OverlayState, OverlayInput, bGrid, Bars);
    // Per style: the bars' transforms, grouped so a style's look and count stay together.
    TArray<TArray<FTransform>> ByStyle;
    ByStyle.SetNum(static_cast<int32>(BoardOverlay::EStyle::Count));
    for (const BoardOverlay::FBar& Bar : Bars)
    {
        const int32 Index = static_cast<int32>(Bar.Style);
        if (!ByStyle.IsValidIndex(Index)) { continue; }
        const FVector2D Delta = (Bar.B - Bar.A) * BoardMetric.CellCm;
        const float Length = Delta.Size();
        // The bar is a 1 cm thick slab lying on the board; its underside is at the layer's lift.
        const float Height = BoardMetric.Scaled(OverlayLook(Bar.Style).Lift) + 0.5f;
        ByStyle[Index].Add(FTransform(FRotator(0.0f, FMath::RadiansToDegrees(FMath::Atan2(Delta.Y, Delta.X)), 0.0f),
            BoardPosition((Bar.A + Bar.B) * 0.5f, Height), FVector(Length / 100.0f, Bar.Width * BoardMetric.CellCm / 100.0f, 0.01f)));
    }
    if (OverlayBase != INDEX_NONE)
    {
        // One component for every style (#144): the bars in style order, each with its style's rgb and opacity as custom data.
        TArray<FTransform> Flat;
        TArray<float> Data;
        for (int32 Index = 0; Index < ByStyle.Num(); ++Index)
        {
            const FOverlayLook Look = OverlayLook(static_cast<BoardOverlay::EStyle>(Index));
            for (const FTransform& Bar : ByStyle[Index])
            {
                Flat.Add(Bar);
                Data.Add(Look.Color.R); Data.Add(Look.Color.G); Data.Add(Look.Color.B); Data.Add(Look.Opacity);
            }
            OverlayCounts[Index] = ByStyle[Index].Num();
        }
        SetOverlayBars(Flat, Data);
        return;
    }
    for (int32 Index = 0; Index < OverlaySets.Num(); ++Index)
    {
        UInstancedStaticMeshComponent* Set = Cast<UInstancedStaticMeshComponent>(OverlaySets[Index].Mesh);
        if (!Set) { continue; }
        Set->ClearInstances();
        if (!ByStyle[Index].IsEmpty()) { Set->AddInstances(ByStyle[Index], false, false, false); }
        Set->SetVisibility(!ByStyle[Index].IsEmpty());
        OverlayCounts[Index] = ByStyle[Index].Num();
    }
}
