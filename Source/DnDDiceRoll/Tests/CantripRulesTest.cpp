#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Rules/CantripRules.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCantripScalingTest, "Project.Expedition.Rules.CantripScaling",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FCantripScalingTest::RunTest(const FString& Parameters)
{
    TestTrue(TEXT("Fire Bolt and Sacred Flame are cantrips"), FCantripRules::IsCantrip(ESpell::FireBolt) && FCantripRules::IsCantrip(ESpell::SacredFlame));
    TestFalse(TEXT("Magic Missile is not"), FCantripRules::IsCantrip(ESpell::MagicMissile));
    TestEqual(TEXT("Cantrips cost no slot"), FCantripRules::SlotCost(ESpell::FireBolt), 0);
    TestEqual(TEXT("Magic Missile costs a 1st-level slot"), FCantripRules::SlotCost(ESpell::MagicMissile), 1);
    const int32 Levels[] = {1, 4, 5, 10, 11, 16, 17, 20};
    const int32 Tiers[] = {1, 1, 2, 2, 3, 3, 4, 4};
    for (int32 Index = 0; Index < 8; ++Index)
    {
        TestEqual(FString::Printf(TEXT("Tier at level %d"), Levels[Index]), FCantripRules::ScalingTier(Levels[Index]), Tiers[Index]);
    }
    TestEqual(TEXT("Fire Bolt level 4: 1d10"), FCantripRules::DiceText(ESpell::FireBolt, 4), FString(TEXT("1d10")));
    TestEqual(TEXT("Fire Bolt level 5: 2d10"), FCantripRules::DiceText(ESpell::FireBolt, 5), FString(TEXT("2d10")));
    TestEqual(TEXT("Sacred Flame level 11: 3d8"), FCantripRules::DiceText(ESpell::SacredFlame, 11), FString(TEXT("3d8")));
    TestEqual(TEXT("Leveled spells do not scale here"), FCantripRules::DiceCount(ESpell::MagicMissile, 17), 0);
    return !HasAnyErrors();
}
#endif
