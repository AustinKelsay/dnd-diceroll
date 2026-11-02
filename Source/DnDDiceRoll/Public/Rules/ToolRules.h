#pragma once

#include "CoreMinimal.h"
#include "Rules/CheckRules.h"

/** A tool check: proficiency adds the bonus; also proficient in the matching skill grants advantage. */
struct DNDDICEROLL_API FToolCheck
{
    bool bAllowed = false;
    int32 Modifier = 0;
    ERollMode Mode = ERollMode::Normal;
};

/** SRD 5.1 tools and kits. Kit charges are passed in and returned; storing them needs saved inventory (#39). */
struct DNDDICEROLL_API FToolRules
{
    static constexpr int32 HealersKitUses = 10;
    /** bRequiresTool: picking a lock without thieves' tools is not allowed at all. */
    static FToolCheck Check(int32 AbilityModifier, int32 ProficiencyBonus, bool bHasTool, bool bToolProficient, bool bSkillProficient, bool bRequiresTool);
    /** A healer's kit stabilizes a dying creature with no check, spending one use. False (and no use) when empty. */
    static bool HealersKitStabilize(int32& UsesLeft);
    /** Herbalism kit proficiency lets you brew antitoxin and potions of healing at camp. */
    static bool CanBrewHealing(bool bHerbalismProficient, bool bHasKit);
};
