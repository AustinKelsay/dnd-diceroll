#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Rules/CoinRules.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCoinRulesTest, "Project.Expedition.Rules.CoinsAndPrices",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FCoinRulesTest::RunTest(const FString& Parameters)
{
    TestEqual(TEXT("Mixed coins"), FCoinRules::Format(1234), FString(TEXT("12 gp 3 sp 4 cp")));
    TestEqual(TEXT("Gold only"), FCoinRules::Format(500), FString(TEXT("5 gp")));
    TestEqual(TEXT("Zero"), FCoinRules::Format(0), FString(TEXT("0 cp")));
    TestEqual(TEXT("Longsword 15 gp"), FCoinRules::Price(EShopItem::Longsword), 1500);
    TestEqual(TEXT("Potion 50 gp"), FCoinRules::Price(EShopItem::PotionOfHealing), 5000);
    TestEqual(TEXT("Rations 5 sp"), FCoinRules::Price(EShopItem::Rations), 50);
    for (int32 I = 0; I < (int32)EShopItem::Count; ++I)
    {
        TestTrue(TEXT("Every item has a price"), FCoinRules::Price((EShopItem)I) > 0);
    }
    TestEqual(TEXT("Sell at half"), FCoinRules::SellPrice(EShopItem::Shortsword), 500);
    TestEqual(TEXT("No discount below 15"), FCoinRules::HaggledPrice(1000, 14), 1000);
    TestEqual(TEXT("10% at 15"), FCoinRules::HaggledPrice(1000, 15), 900);
    TestEqual(TEXT("20% at 20"), FCoinRules::HaggledPrice(1000, 22), 800);
    return !HasAnyErrors();
}
#endif
