#pragma once

#include "CoreMinimal.h"

enum class EShopItem : uint8 { Dagger, Shortsword, Longsword, Shortbow, Arrows20, LeatherArmor, ChainShirt, Shield, HealersKit, ThievesTools, PotionOfHealing, Rations, Count };

/** SRD coins, all values held in copper pieces (1 gp = 10 sp = 100 cp). */
struct DNDDICEROLL_API FCoinRules
{
    static constexpr int32 CopperPerSilver = 10;
    static constexpr int32 CopperPerGold = 100;
    /** Largest coins first, zero parts omitted: 1234 -> "12 gp 3 sp 4 cp"; 0 -> "0 cp". */
    static FString Format(int32 Copper);
    /** SRD list price in copper. */
    static int32 Price(EShopItem Item);
    /** Merchants buy gear back at half price, rounded down. */
    static int32 SellPrice(EShopItem Item);
    /** House rule: Persuasion 15 takes 10% off, 20 takes 20% off (rounded down discount). */
    static int32 HaggledPrice(int32 ListCopper, int32 PersuasionTotal);
};
