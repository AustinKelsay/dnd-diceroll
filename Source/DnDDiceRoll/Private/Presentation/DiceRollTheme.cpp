#include "Presentation/DiceRollTheme.h"
#include "Animation/AnimSequence.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"

UStaticMesh* UDiceRollTheme::Load(const TSoftObjectPtr<UStaticMesh>& Mesh)
{
    return Mesh.IsNull() ? nullptr : Mesh.LoadSynchronous();
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
