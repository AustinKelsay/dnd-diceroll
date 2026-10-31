#pragma once
#include "CoreMinimal.h"
#include "Combat/CombatBattle.h"

/** Between-encounter seam for the next run module. A successful victory transition gives
 * one conscious recipient +1 weapon damage rank (cap 3) and one potion (cap 9).
 * Layout owns cells/enemies; the previous battle owns every party value and identity.
 * Atomic rejection, including invalid layouts, preserves the completed battle and RNG.
 * Recovery/save/routing belong to later slices; no implicit healing or resource reset.
 */
class DNDDICEROLL_API FPartyContinuity
{
public:
    static bool Advance(FCombatBattle& Battle, const FCombatEncounter& Layout, int32 Seed, int32 RecipientId, FString& Error);
};
