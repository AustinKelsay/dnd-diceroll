#include "Rules/SuperiorityRules.h"

int32 FSuperiorityRules::DieSides(int32 BattleMasterLevel) { return BattleMasterLevel >= 18 ? 12 : BattleMasterLevel >= 10 ? 10 : 8; }
int32 FSuperiorityRules::DicePerRest(int32 BattleMasterLevel) { return BattleMasterLevel >= 15 ? 6 : BattleMasterLevel >= 7 ? 5 : BattleMasterLevel >= 3 ? 4 : 0; }
bool FSuperiorityRules::RiderCostsAction() { return false; }

int32 FSuperiorityRules::PrecisionHitOutOf(int32 AttackBonus, int32 ArmorClass, ERollMode Mode, int32 Sides)
{
    int32 Hits = 0;
    for (int32 First = 1; First <= 20; ++First)
    {
        for (int32 Second = 1; Second <= 20; ++Second)
        {
            const int32 Kept = Mode == ERollMode::Advantage ? FMath::Max(First, Second) : Mode == ERollMode::Disadvantage ? FMath::Min(First, Second) : First;
            for (int32 Die = 1; Die <= Sides; ++Die)
            {
                const bool bHit = Kept == 20 || (Kept != 1 && Kept + AttackBonus + Die >= ArmorClass);
                Hits += bHit ? 1 : 0;
            }
        }
    }
    return Hits;
}

int32 FSuperiorityRules::ManeuverSaveDc(int32 ProficiencyBonus, int32 AbilityModifier) { return 8 + ProficiencyBonus + AbilityModifier; }
