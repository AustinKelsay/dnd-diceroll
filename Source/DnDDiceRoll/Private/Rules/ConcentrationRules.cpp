#include "Rules/ConcentrationRules.h"

int32 FConcentrationRules::SaveDc(int32 Damage)
{
    return FMath::Min(30, FMath::Max(10, Damage / 2));
}

int32 FConcentrationRules::KeepOutOf400(int32 ConstitutionSave, int32 Damage, ERollMode Mode)
{
    return FCheckRules::SuccessOutOf400(ConstitutionSave, SaveDc(Damage), Mode);
}

bool FConcentrationRules::NewSpellEndsCurrent(bool bConcentrating, bool bNewSpellNeedsConcentration)
{
    return bConcentrating && bNewSpellNeedsConcentration;
}

bool FConcentrationRules::EndsOnIncapacitated(bool bIncapacitated) { return bIncapacitated; }

bool FConcentrationRules::RequiresConcentration(ESpell Spell)
{
    return FSpellRules::Facts(Spell).bConcentration;
}
