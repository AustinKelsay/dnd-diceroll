#include "Rules/EncumbranceRules.h"

bool FEncumbranceRules::VariantEnabledByDefault() { return false; }
int32 FEncumbranceRules::CarryingCapacity(int32 Strength) { return 15 * Strength; }
int32 FEncumbranceRules::PushDragLift(int32 Strength) { return 30 * Strength; }

int32 FEncumbranceRules::SpeedCells(int32 BaseSpeedCells, int32 WeightPounds, int32 Strength, bool bVariant)
{
    if (WeightPounds > PushDragLift(Strength)) { return 0; }
    if (WeightPounds > CarryingCapacity(Strength)) { return FMath::Min(1, BaseSpeedCells); }
    if (bVariant && WeightPounds > 10 * Strength) { return FMath::Max(0, BaseSpeedCells - 4); }
    if (bVariant && WeightPounds > 5 * Strength) { return FMath::Max(0, BaseSpeedCells - 2); }
    return BaseSpeedCells;
}

ERollMode FEncumbranceRules::PhysicalRollMode(int32 WeightPounds, int32 Strength, bool bVariant)
{
    return bVariant && WeightPounds > 10 * Strength ? ERollMode::Disadvantage : ERollMode::Normal;
}

int32 FEncumbranceRules::Weight(EShopItem Item)
{
    switch (Item)
    {
    case EShopItem::Dagger: return 1;
    case EShopItem::Shortsword: return 2;
    case EShopItem::Longsword: return 3;
    case EShopItem::Shortbow: return 2;
    case EShopItem::Arrows20: return 1;
    case EShopItem::LeatherArmor: return 10;
    case EShopItem::ChainShirt: return 20;
    case EShopItem::Shield: return 6;
    case EShopItem::HealersKit: return 3;
    case EShopItem::ThievesTools: return 1;
    case EShopItem::PotionOfHealing: return 1;
    case EShopItem::Rations: return 2;
    default: return 0;
    }
}
