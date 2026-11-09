#include "Rules/CoinRules.h"

FString FCoinRules::Format(int32 Copper)
{
    if (Copper <= 0) { return TEXT("0 cp"); }
    TArray<FString> Parts;
    const int32 Gp = Copper / CopperPerGold;
    const int32 Sp = (Copper % CopperPerGold) / CopperPerSilver;
    const int32 Cp = Copper % CopperPerSilver;
    if (Gp > 0) { Parts.Add(FString::Printf(TEXT("%d gp"), Gp)); }
    if (Sp > 0) { Parts.Add(FString::Printf(TEXT("%d sp"), Sp)); }
    if (Cp > 0) { Parts.Add(FString::Printf(TEXT("%d cp"), Cp)); }
    return FString::Join(Parts, TEXT(" "));
}

int32 FCoinRules::Price(EShopItem Item)
{
    switch (Item)
    {
    case EShopItem::Dagger: return 200;
    case EShopItem::Shortsword: return 1000;
    case EShopItem::Longsword: return 1500;
    case EShopItem::Shortbow: return 2500;
    case EShopItem::Arrows20: return 100;
    case EShopItem::LeatherArmor: return 1000;
    case EShopItem::ChainShirt: return 5000;
    case EShopItem::Shield: return 1000;
    case EShopItem::HealersKit: return 500;
    case EShopItem::ThievesTools: return 2500;
    case EShopItem::PotionOfHealing: return 5000;
    case EShopItem::Rations: return 50;
    default: return 0;
    }
}

int32 FCoinRules::SellPrice(EShopItem Item) { return Price(Item) / 2; }

int32 FCoinRules::HaggledPrice(int32 ListCopper, int32 PersuasionTotal)
{
    const int32 Percent = PersuasionTotal >= 20 ? 20 : PersuasionTotal >= 15 ? 10 : 0;
    return ListCopper - ListCopper * Percent / 100;
}
