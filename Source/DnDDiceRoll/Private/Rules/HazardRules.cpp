#include "Rules/HazardRules.h"

FHazardSpec FHazardRules::Spec(EHazard Hazard)
{
    FHazardSpec S;
    switch (Hazard)
    {
    case EHazard::Cliff: S.Skill = ESkill::Athletics; S.Dc = 12; S.HitPointsLost = 4; break;
    case EHazard::Ledge: S.Skill = ESkill::Acrobatics; S.Dc = 12; S.HitPointsLost = 3; break;
    case EHazard::Lost: S.Skill = ESkill::Survival; S.Dc = 13; S.SuppliesLost = 1; S.bRerouteOnFail = true; break;
    case EHazard::HiddenPassage: S.Skill = ESkill::Investigation; S.Dc = 15; break;
    }
    return S;
}

int32 FHazardRules::ForecastOutOf400(EHazard Hazard, int32 Modifier, ERollMode Mode)
{
    return FCheckRules::SuccessOutOf400(Modifier, Spec(Hazard).Dc, Mode);
}

FHazardResult FHazardRules::Resolve(EHazard Hazard, int32 Modifier, ERollMode Mode, int32& RandomState)
{
    const FHazardSpec S = Spec(Hazard);
    FHazardResult R;
    R.Check = FCheckRules::Resolve(Modifier, S.Dc, Mode, RandomState);
    if (R.Check.bSuccess)
    {
        R.bFoundShortcut = Hazard == EHazard::HiddenPassage;
        return R;
    }
    R.HitPointsLost = S.HitPointsLost;
    R.SuppliesLost = S.SuppliesLost;
    R.bRerouted = S.bRerouteOnFail;
    return R;
}
