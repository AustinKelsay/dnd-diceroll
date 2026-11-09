#pragma once

#include "CoreMinimal.h"

enum class EAdventurerRole : uint8;

enum class ERestKind : uint8 { ShortRest, LongRest };
enum class ERestPool : uint8 { HitPoints, HitDice, SpellSlots, SecondWind, ActionSurge, ChannelDivinity, ArcaneRecovery, Superiority, Count };

/** SRD short and long rests as math only. */
struct DNDDICEROLL_API FRestRules
{
    static int32 HitDieSides(EAdventurerRole Role);
    /** One spent hit die heals its roll plus the Constitution modifier, never below 0. */
    static int32 HitDieHealing(int32 Roll, int32 ConstitutionModifier);
    /** A long rest returns half the character's hit dice (minimum 1), up to its level. */
    static int32 HitDiceAfterLongRest(int32 Available, int32 Level);
    static ERestKind RefreshedBy(ERestPool Pool);
    /** Design decision: a camp counts as a long rest. */
    static ERestKind CampRest();
};
