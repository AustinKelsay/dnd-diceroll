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

bool UDiceRollTheme::OverviewR002Complete() const
{
    return bOverviewR002 && Set(OverviewTabletop) && Set(OverviewRootDiorama) && Set(OverviewVigilDiorama) && Set(OverviewVaultDiorama);
}

FDiceRollOverview UDiceRollTheme::LoadOverview() const
{
    FDiceRollOverview Found;
    if (OverviewR002Complete())
    {
        Found.Table = Load(OverviewTabletop); Found.Root = Load(OverviewRootDiorama); Found.Vigil = Load(OverviewVigilDiorama); Found.Vault = Load(OverviewVaultDiorama);
        Found.bR002 = true;
        // A package that exists but fails to load sends the whole overview back to r001: one set or the other.
        if (Found.Table && Found.Root && Found.Vigil && Found.Vault) { return Found; }
        Found = FDiceRollOverview();
    }
    Found.Table = Load(Tabletop); Found.Root = Load(RootDiorama); Found.Vigil = Load(VigilDiorama);
    return Found;
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

FName UDiceRollTheme::ClassKey(EAdventurerRole Role)
{
    if (Role == EAdventurerRole::None) { return NAME_None; }
    const UEnum* Enum = StaticEnum<EAdventurerRole>();
    return Enum ? FName(*Enum->GetNameStringByValue(static_cast<int64>(Role))) : NAME_None;
}

const FDiceRollCharacter* UDiceRollTheme::RigForClass(EAdventurerRole Role) const
{
    const FName Key = ClassKey(Role);
    if (Key.IsNone()) { return nullptr; }
    const FDiceRollCharacter* Entry = ClassRigs.Find(Key);
    if (Entry && !Entry->Mesh.IsNull()) { return Entry; }
    const FDiceRollCharacter* Legacy = Role == EAdventurerRole::Fighter ? &FighterRig : Role == EAdventurerRole::Wizard ? &WizardRig : nullptr;
    return Legacy && !Legacy->Mesh.IsNull() ? Legacy : nullptr;
}

const FDiceRollCharacter* UDiceRollTheme::RigForFoe(FName Archetype) const
{
    if (Archetype.IsNone()) { return nullptr; }
    const FDiceRollCharacter* Entry = FoeRigs.Find(Archetype);
    if (Entry && !Entry->Mesh.IsNull()) { return Entry; }
    return Archetype == FName(TEXT("Raider")) && !RaiderRig.Mesh.IsNull() ? &RaiderRig : nullptr;
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
    case EUnitPose::Brace: return Brace;
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
        const EUnitPose Pose = static_cast<EUnitPose>(Index);
        const TSoftObjectPtr<UAnimSequence>& Entry = Animations.For(Pose);
        UAnimSequence* Shared = Entry.IsNull() ? nullptr : Entry.LoadSynchronous();
        if (!Shared) { return false; }
        const TSoftObjectPtr<UAnimSequence>* Override = Overrides.Find(FName(ExpeditionPose::ClipName(Pose)));
        if (Override && !Override->IsNull())
        {
            UAnimSequence* Replaced = Override->LoadSynchronous();
            if (!Replaced) { return false; }
            Clips.Add(Replaced);
            continue;
        }
        Clips.Add(Shared);
    }
    if (!Loaded) { return false; }
    OutMesh = Loaded;
    OutClips = MoveTemp(Clips);
    return true;
}
