#pragma once

#include "CoreMinimal.h"
#include "Rules/CheckRules.h"
#include "Rules/SpellRules.h"

/** SRD 5.2.1 concentration as math only. */
struct DNDDICEROLL_API FConcentrationRules
{
    /** Constitution save after damage: DC 10 or half the damage, whichever is higher, capped at 30 (SRD 5.2.1). */
    static int32 SaveDc(int32 Damage);
    static int32 KeepOutOf400(int32 ConstitutionSave, int32 Damage, ERollMode Mode);
    /** Casting another concentration spell ends the first. */
    static bool NewSpellEndsCurrent(bool bConcentrating, bool bNewSpellNeedsConcentration);
    static bool EndsOnIncapacitated(bool bIncapacitated);
    /** Reads FSpellFacts::bConcentration. */
    static bool RequiresConcentration(ESpell Spell);
};
