#pragma once

#include "CoreMinimal.h"
#include "Rules/CheckRules.h"

/** SRD 5.2.1 light levels for a cell. */
enum class ELight : uint8 { Bright, Dim, Darkness };
/** SRD 5.2.1 obscurement: lightly (dim light, fog) or heavily (darkness, dense fog). */
enum class EObscurement : uint8 { None, Light, Heavy };
/** How a sight-based Perception check is rolled. */
enum class ESightCheck : uint8 { Normal, Disadvantage, AutoFail };

/** SRD 5.2.1 Vision and Light. Pure queries; battle lighting per cell waits for a terrain/light field. */
struct DNDDICEROLL_API FLightRules
{
    /** Light as one observer perceives it: within darkvision range darkness counts as dim and dim as bright. */
    static ELight PerceivedLight(ELight CellLight, int32 DarkvisionFeet, int32 DistanceFeet);
    static EObscurement Obscurement(ELight PerceivedLight);
    /** A heavily obscured target is effectively unseen (blinded-equivalent for the observer). */
    static bool CanSee(ELight PerceivedLight);
    /** Lightly obscured: disadvantage on sight Perception; heavily obscured: sight checks fail. */
    static ESightCheck SightPerception(EObscurement Obscurement);
    /** Attack mode from sight alone: an unseen attacker has advantage, an unseen target imposes disadvantage. */
    static ERollMode AttackMode(bool bAttackerSeesTarget, bool bTargetSeesAttacker);
    /** You can try to hide only where the observer's view of you is obscured. */
    static bool CanTryToHide(EObscurement Obscurement);
    /** Darkvision range in feet by profile name; adventurers have none until ancestries exist. */
    static int32 DarkvisionFeet(const FString& Profile);
};
