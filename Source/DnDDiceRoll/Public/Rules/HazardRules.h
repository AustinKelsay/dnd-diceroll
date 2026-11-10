#pragma once

#include "CoreMinimal.h"
#include "Rules/CheckRules.h"
#include "Rules/ProficiencyRules.h"

/** Exploration hazards resolved by a single skill check. */
enum class EHazard : uint8 { Cliff, Ledge, Lost, HiddenPassage };

struct DNDDICEROLL_API FHazardSpec
{
    ESkill Skill = ESkill::Athletics;
    int32 Dc = 10;
    int32 HitPointsLost = 0;
    int32 SuppliesLost = 0;
    bool bRerouteOnFail = false;
};

struct DNDDICEROLL_API FHazardResult
{
    FD20Outcome Check;
    int32 HitPointsLost = 0;
    int32 SuppliesLost = 0;
    bool bRerouted = false;
    bool bFoundShortcut = false;
};

struct DNDDICEROLL_API FHazardRules
{
    static FHazardSpec Spec(EHazard Hazard);
    /** Exact chance to pass in 1/400ths, shown before the party commits. */
    static int32 ForecastOutOf400(EHazard Hazard, int32 Modifier, ERollMode Mode);
    /** Success costs nothing (a hidden passage becomes a shortcut); failure applies the spec's costs. */
    static FHazardResult Resolve(EHazard Hazard, int32 Modifier, ERollMode Mode, int32& RandomState);
};
