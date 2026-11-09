#include "Rules/CoverRules.h"

int32 FCoverRules::AcBonus(ECoverLevel Cover)
{
    switch (Cover)
    {
    case ECoverLevel::Half: return 2;
    case ECoverLevel::ThreeQuarters: return 5;
    default: return 0;
    }
}

int32 FCoverRules::DexSaveBonus(ECoverLevel Cover)
{
    return AcBonus(Cover);
}

bool FCoverRules::BlocksTargeting(ECoverLevel Cover)
{
    return Cover == ECoverLevel::Total;
}

ECoverLevel FCoverRules::CoverFromLargerCreature(ECreatureSize Attacker, ECreatureSize Interposer)
{
    return (int32)Interposer >= (int32)Attacker + 1 ? ECoverLevel::Half : ECoverLevel::None;
}

bool FCoverRules::IgnoresCover(int32 Intelligence)
{
    return Intelligence <= 6;
}
