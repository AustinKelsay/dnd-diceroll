#include "Presentation/DiceRollTheme.h"
#include "Animation/AnimSequence.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"

UStaticMesh* UDiceRollTheme::Load(const TSoftObjectPtr<UStaticMesh>& Mesh)
{
    return Mesh.IsNull() ? nullptr : Mesh.LoadSynchronous();
}

FDiceRollWorldKit UDiceRollTheme::KitFor(bool bForest) const
{
    FDiceRollWorldKit Kit;
    if (bForest)
    {
        Kit.bMetric = bRootMetricKit;
        Kit.Occluders = &RootOccluders;
        Kit.Pillars = &RootPillarVariants; Kit.Walls = &RootWallVariants; Kit.Stones = &RootStonesVariants;
        Kit.Arch = &RootArchTwoCell;
        Kit.Scatter = &RootScatter; Kit.Mist = &RootMistCards; Kit.Shafts = &RootLightShafts;
    }
    else
    {
        Kit.bMetric = bMetricKit;
        Kit.Occluders = &VigilOccluders;
        Kit.Pillars = &PillarVariants; Kit.Walls = &WallVariants; Kit.Stones = &StonesVariants;
        Kit.Arch = &ArchTwoCell;
        Kit.Scatter = &Scatter; Kit.Mist = &MistCards; Kit.Shafts = &LightShafts;
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
