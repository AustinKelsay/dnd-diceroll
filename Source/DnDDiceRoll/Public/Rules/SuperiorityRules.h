#pragma once

#include "CoreMinimal.h"
#include "Rules/CheckRules.h"

/** Battle Master superiority dice as a pure table, in our own wording. */
struct DNDDICEROLL_API FSuperiorityRules
{
    /** d8, d10 from level 10, d12 from 18. */
    static int32 DieSides(int32 BattleMasterLevel);
    /** 4 dice from level 3, 5 from 7, 6 from 15. */
    static int32 DicePerRest(int32 BattleMasterLevel);
    /** Maneuver riders ride along with an attack; they never cost the Action. */
    static bool RiderCostsAction();
    /** Exact chance in 1/(400 x sides) that an attack hits when a superiority die is added to the roll. */
    static int32 PrecisionHitOutOf(int32 AttackBonus, int32 ArmorClass, ERollMode Mode, int32 Sides);
    static int32 ManeuverSaveDc(int32 ProficiencyBonus, int32 AbilityModifier);
};
