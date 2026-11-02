#pragma once

#include "CoreMinimal.h"
#include "Rules/CheckRules.h"

enum class EAdventurerRole : uint8;

/** Result of lifting or planting an item. */
enum class ESleightResult : uint8 { Unnoticed, Fumbled, Caught };

struct DNDDICEROLL_API FSleightOutcome
{
    FD20Outcome Check;
    ESleightResult Result = ESleightResult::Fumbled;
};

/** SRD 5.1 Sleight of Hand (DEX) against a target's Perception. */
struct DNDDICEROLL_API FSleightRules
{
    /** An unaware target uses passive Perception as the DC; an alert one rolls Perception (contest, ties to the target). */
    static int32 SuccessOutOf400(int32 SleightModifier, ERollMode Mode, int32 TargetPassivePerception);
    /** Meet the DC: item taken unnoticed. Miss by 1-4: nothing happens. Miss by 5+: caught (the target turns hostile). */
    static ESleightResult ResultFor(int32 Total, int32 TargetPassivePerception);
    static FSleightOutcome Resolve(int32 SleightModifier, ERollMode Mode, int32 TargetPassivePerception, int32& RandomState);
    /** Rogues from level 2 use an object (including Sleight of Hand) as a bonus action via Cunning Action. */
    static bool UseObjectAsBonusAction(EAdventurerRole Role, int32 Level);
};
