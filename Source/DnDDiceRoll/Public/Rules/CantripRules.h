#pragma once

#include "CoreMinimal.h"
#include "Rules/SpellRules.h"

/** SRD 5.2.1 cantrips: at will, damage dice grow at character levels 5, 11 and 17. */
struct DNDDICEROLL_API FCantripRules
{
    static bool IsCantrip(ESpell Spell);
    /** Slots spent: 0 for cantrips, else the spell's level. */
    static int32 SlotCost(ESpell Spell);
    /** Multiplier on the cantrip's base dice: 1, 2 at 5th, 3 at 11th, 4 at 17th. */
    static int32 ScalingTier(int32 CharacterLevel);
    /** Dice count for a cantrip at a character level; 0 for leveled spells. */
    static int32 DiceCount(ESpell Spell, int32 CharacterLevel);
    /** "2d10" style text. */
    static FString DiceText(ESpell Spell, int32 CharacterLevel);
};
