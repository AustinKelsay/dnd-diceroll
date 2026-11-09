#include "Rules/CantripRules.h"

bool FCantripRules::IsCantrip(ESpell Spell) { return FSpellRules::Facts(Spell).Level == 0; }

int32 FCantripRules::SlotCost(ESpell Spell) { return FSpellRules::Facts(Spell).Level; }

int32 FCantripRules::ScalingTier(int32 CharacterLevel)
{
    return CharacterLevel >= 17 ? 4 : CharacterLevel >= 11 ? 3 : CharacterLevel >= 5 ? 2 : 1;
}

int32 FCantripRules::DiceCount(ESpell Spell, int32 CharacterLevel)
{
    return IsCantrip(Spell) ? FSpellRules::Facts(Spell).DieCount * ScalingTier(CharacterLevel) : 0;
}

FString FCantripRules::DiceText(ESpell Spell, int32 CharacterLevel)
{
    return FString::Printf(TEXT("%dd%d"), DiceCount(Spell, CharacterLevel), FSpellRules::Facts(Spell).DieSides);
}
