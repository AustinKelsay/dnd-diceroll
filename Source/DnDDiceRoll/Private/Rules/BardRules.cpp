#include "Rules/BardRules.h"

int32 FBardRules::InspirationDie(int32 BardLevel) { return BardLevel >= 15 ? 12 : BardLevel >= 10 ? 10 : BardLevel >= 5 ? 8 : BardLevel >= 1 ? 6 : 0; }
int32 FBardRules::InspirationUses(int32 BardLevel, int32 CharismaModifier) { return BardLevel >= 1 ? FMath::Max(1, CharismaModifier) : 0; }
bool FBardRules::HasFontOfInspiration(int32 BardLevel) { return BardLevel >= 5; }
int32 FBardRules::ExpertiseSkills(int32 BardLevel) { return BardLevel >= 9 ? 4 : BardLevel >= 2 ? 2 : 0; }
int32 FBardRules::JackOfAllTrades(int32 BardLevel, int32 ProficiencyBonus) { return BardLevel >= 2 ? ProficiencyBonus / 2 : 0; }
int32 FBardRules::CheckProficiency(int32 BardLevel, int32 ProficiencyBonus, bool bProficient, bool bExpertise)
{
    if (bExpertise && bProficient && ExpertiseSkills(BardLevel) > 0) { return ProficiencyBonus * 2; }
    return bProficient ? ProficiencyBonus : JackOfAllTrades(BardLevel, ProficiencyBonus);
}
int32 FBardRules::PreparedSpells(int32 BardLevel)
{
    static const int32 Prepared[] = {0, 4, 5, 6, 7, 9, 10, 11, 12, 14, 15, 16, 16, 17, 17, 18, 18, 19, 20, 21, 22};
    return Prepared[FMath::Clamp(BardLevel, 0, 20)];
}
int32 FBardRules::LoreBonusSkills(int32 BardLevel) { return BardLevel >= 3 ? 3 : 0; }
bool FBardRules::MagicalSecretsAllows(int32 BardLevel, const FString& ClassList)
{
    if (ClassList.Equals(TEXT("Bard"), ESearchCase::IgnoreCase)) { return BardLevel >= 1; }
    return BardLevel >= 10 && (ClassList.Equals(TEXT("Cleric"), ESearchCase::IgnoreCase) || ClassList.Equals(TEXT("Druid"), ESearchCase::IgnoreCase) || ClassList.Equals(TEXT("Wizard"), ESearchCase::IgnoreCase));
}
int32 FBardRules::SuperiorInspirationFloor(int32 BardLevel) { return BardLevel >= 18 ? 2 : 0; }