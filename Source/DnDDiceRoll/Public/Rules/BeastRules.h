#pragma once

#include "CoreMinimal.h"
#include "Rules/CheckRules.h"

/** What a beast does after an Animal Handling attempt. */
enum class EBeastBehaviour : uint8 { Hostile, Calmed, Redirected };

struct DNDDICEROLL_API FBeastOutcome
{
    FD20Outcome Check;
    EBeastBehaviour Behaviour = EBeastBehaviour::Hostile;
};

/** SRD 5.1 Animal Handling (WIS) and mounted combat basics. */
struct DNDDICEROLL_API FBeastRules
{
    /** Only beasts can be handled. */
    static bool IsBeast(const FString& Profile);
    /** Calm DC by beast; 0 when the profile is not a beast. A beast that has been hurt this fight cannot be calmed. */
    static int32 CalmDc(const FString& Profile);
    /** Miss: stays hostile. Meet: calmed (will not attack unless attacked). Beat by 5+: redirected (leaves the fight). */
    static EBeastBehaviour BehaviourFor(int32 Total, int32 Dc);
    static int32 CalmOutOf400(int32 Modifier, ERollMode Mode, const FString& Profile, bool bHurtThisFight);
    static FBeastOutcome Resolve(int32 Modifier, ERollMode Mode, const FString& Profile, bool bHurtThisFight, int32& RandomState);
    /** Mounting or dismounting costs half your speed (rounded up, in cells). */
    static int32 MountCost(int32 SpeedCells);
    /** A controlled mount may only Dash, Disengage or Dodge. */
    static bool ControlledMountMayTake(const FString& ActionName);
};
