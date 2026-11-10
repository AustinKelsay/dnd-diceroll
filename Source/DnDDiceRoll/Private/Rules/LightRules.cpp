#include "Rules/LightRules.h"

ELight FLightRules::PerceivedLight(ELight CellLight, int32 DarkvisionFeet, int32 DistanceFeet)
{
    if (DarkvisionFeet <= 0 || DistanceFeet > DarkvisionFeet) { return CellLight; }
    return CellLight == ELight::Darkness ? ELight::Dim : ELight::Bright;
}

EObscurement FLightRules::Obscurement(ELight PerceivedLight)
{
    return PerceivedLight == ELight::Darkness ? EObscurement::Heavy : PerceivedLight == ELight::Dim ? EObscurement::Light : EObscurement::None;
}

bool FLightRules::CanSee(ELight PerceivedLight) { return PerceivedLight != ELight::Darkness; }

ESightCheck FLightRules::SightPerception(EObscurement Obscurement)
{
    return Obscurement == EObscurement::Heavy ? ESightCheck::AutoFail : Obscurement == EObscurement::Light ? ESightCheck::Disadvantage : ESightCheck::Normal;
}

ERollMode FLightRules::AttackMode(bool bAttackerSeesTarget, bool bTargetSeesAttacker)
{
    return FCheckRules::CombineMode(bTargetSeesAttacker ? 0 : 1, bAttackerSeesTarget ? 0 : 1);
}

bool FLightRules::CanTryToHide(EObscurement Obscurement) { return Obscurement != EObscurement::None; }

int32 FLightRules::DarkvisionFeet(const FString& Profile)
{
    static const TCHAR* Seers[] = {TEXT("Goblin"), TEXT("Skeleton"), TEXT("Zombie"), TEXT("Ice Mephit"), TEXT("Fire Elemental")};
    for (const TCHAR* Name : Seers) { if (Profile == Name) { return 60; } }
    return 0;
}
