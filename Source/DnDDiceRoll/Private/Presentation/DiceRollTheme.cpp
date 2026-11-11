#include "Presentation/DiceRollTheme.h"
#include "Animation/AnimSequence.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Misc/PackageName.h"

UStaticMesh* UDiceRollTheme::Load(const TSoftObjectPtr<UStaticMesh>& Mesh)
{
    return Mesh.IsNull() ? nullptr : Mesh.LoadSynchronous();
}

namespace
{
/** An entry that is set and whose package is loaded or exists on disk: a dangling reference to a deleted package is not set. */
bool Set(const TSoftObjectPtr<UStaticMesh>& Mesh)
{
    if (Mesh.IsNull()) { return false; }
    return Mesh.IsValid() || FPackageName::DoesPackageExist(Mesh.ToSoftObjectPath().GetLongPackageName());
}
bool AllSet(const TArray<TSoftObjectPtr<UStaticMesh>>& Meshes, int32 Count)
{
    if (Meshes.Num() != Count) { return false; }
    for (const TSoftObjectPtr<UStaticMesh>& Mesh : Meshes) { if (!Set(Mesh)) { return false; } }
    return true;
}
}

bool UDiceRollTheme::VaultKitComplete() const
{
    return bVaultMetricKit && Set(VaultWorld) && Set(VaultOccluders) && Set(VaultArchTwoCell)
        && AllSet(VaultPillarVariants, 3) && AllSet(VaultWallVariants, 3) && AllSet(VaultStonesVariants, 3)
        && AllSet(VaultScatter, 8) && AllSet(VaultMistCards, 3) && AllSet(VaultLightShafts, 2);
}

FDiceRollWorldKit UDiceRollTheme::KitFor(EDiceRollKit Kind) const
{
    FDiceRollWorldKit Kit;
    if (Kind == EDiceRollKit::Vault && !VaultKitComplete()) { Kind = EDiceRollKit::Vigil; }
    Kit.Kind = Kind;
    switch (Kind)
    {
    case EDiceRollKit::Root:
        Kit.bMetric = bRootMetricKit;
        Kit.World = &RootWorld;
        Kit.Occluders = &RootOccluders;
        Kit.Pillars = &RootPillarVariants; Kit.Walls = &RootWallVariants; Kit.Stones = &RootStonesVariants;
        Kit.Arch = &RootArchTwoCell;
        Kit.Scatter = &RootScatter; Kit.Mist = &RootMistCards; Kit.Shafts = &RootLightShafts;
        break;
    case EDiceRollKit::Vault:
        Kit.bMetric = bVaultMetricKit;
        Kit.World = &VaultWorld;
        Kit.Occluders = &VaultOccluders;
        Kit.Pillars = &VaultPillarVariants; Kit.Walls = &VaultWallVariants; Kit.Stones = &VaultStonesVariants;
        Kit.Arch = &VaultArchTwoCell;
        Kit.Scatter = &VaultScatter; Kit.Mist = &VaultMistCards; Kit.Shafts = &VaultLightShafts;
        break;
    default:
        Kit.bMetric = bMetricKit;
        Kit.World = &VigilWorld;
        Kit.Occluders = &VigilOccluders;
        Kit.Pillars = &PillarVariants; Kit.Walls = &WallVariants; Kit.Stones = &StonesVariants;
        Kit.Arch = &ArchTwoCell;
        Kit.Scatter = &Scatter; Kit.Mist = &MistCards; Kit.Shafts = &LightShafts;
        break;
    }
    return Kit;
}

const TSoftObjectPtr<UAnimSequence>& FDiceRollAnimSet::For(EUnitPose Pose) const
{
    switch (Pose)
    {
    case EUnitPose::Walk: return Walk;
    case EUnitPose::Melee: return Melee;
    case EUnitPose::Ranged: return Ranged;
    case EUnitPose::Cast: return Cast;
    case EUnitPose::Hit: return Hit;
    case EUnitPose::Downed: return Downed;
    case EUnitPose::Death: return Death;
    case EUnitPose::Recover: return Recover;
    default: return Idle;
    }
}

bool FDiceRollCharacter::Load(USkeletalMesh*& OutMesh, TArray<UAnimSequence*>& OutClips) const
{
    OutMesh = nullptr;
    OutClips.Reset();
    if (Mesh.IsNull()) { return false; }
    USkeletalMesh* Loaded = Mesh.LoadSynchronous();
    TArray<UAnimSequence*> Clips;
    for (int32 Index = 0; Index < ExpeditionPose::Count; ++Index)
    {
        const TSoftObjectPtr<UAnimSequence>& Entry = Animations.For(static_cast<EUnitPose>(Index));
        UAnimSequence* Clip = Entry.IsNull() ? nullptr : Entry.LoadSynchronous();
        if (!Clip) { return false; }
        Clips.Add(Clip);
    }
    if (!Loaded) { return false; }
    OutMesh = Loaded;
    OutClips = MoveTemp(Clips);
    return true;
}
