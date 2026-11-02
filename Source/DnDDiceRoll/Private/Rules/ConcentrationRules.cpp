#include "Rules/ConcentrationRules.h"
#include "Rules/SpellCatalogRules.h"

int32 FConcentrationRules::SaveDc(int32 Damage) { return FMath::Max(10, Damage / 2); }
int32 FConcentrationRules::KeepOutOf400(int32 ConstitutionSave, int32 Damage, ERollMode Mode) { return FCheckRules::SuccessOutOf400(ConstitutionSave, SaveDc(Damage), Mode); }
bool FConcentrationRules::NewSpellEndsCurrent(bool bConcentrating, bool bNewSpellNeedsConcentration) { return bConcentrating && bNewSpellNeedsConcentration; }
bool FConcentrationRules::EndsOnIncapacitated(bool bIncapacitated) { return bIncapacitated; }
bool FConcentrationRules::RequiresConcentration(ESpell Spell)
{
    const FSrdSpell* Srd = FSpellCatalogRules::Find(FSpellRules::Facts(Spell).Name);
    return Srd && Srd->bConcentration;
}
bool FConcentrationRules::RequiresConcentration(const FSrdSpell& Spell) { return Spell.bConcentration; }
