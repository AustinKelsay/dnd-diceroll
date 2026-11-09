#pragma once

#include "CoreMinimal.h"
#include "Rules/GrappleRules.h"

/** SRD cover grades on the 5-foot grid. Terrain raycasting stays in battle; this library is pure bonuses. */
enum class ECoverLevel : uint8 { None, Half, ThreeQuarters, Total };

/** Pure cover queries. No board state. */
struct DNDDICEROLL_API FCoverRules
{
    /** SRD: half +2, three-quarters +5, none/total 0 (total blocks the attack instead). */
    static int32 AcBonus(ECoverLevel Cover);
    /** Same bonuses apply to Dexterity saving throws against effects the cover can interrupt. */
    static int32 DexSaveBonus(ECoverLevel Cover);
    /** Total cover: the target cannot be targeted by an attack or spell that requires a target. */
    static bool BlocksTargeting(ECoverLevel Cover);
    /**
     * House rule (kept from Compact): a creature at least one size larger than the attacker
     * grants half cover to whoever it interposes. Raw SRD has no automatic size-based cover.
     */
    static ECoverLevel CoverFromLargerCreature(ECreatureSize Attacker, ECreatureSize Interposer);
    /**
     * House rule (kept from Compact): AI with Intelligence 6 or less ignores cover when choosing
     * attacks. Raw SRD has no INT threshold for ignoring cover.
     */
    static bool IgnoresCover(int32 Intelligence);
};
