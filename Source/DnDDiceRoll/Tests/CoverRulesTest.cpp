#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Rules/CoverRules.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCoverRulesTest, "Project.Expedition.Rules.CoverBonuses",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FCoverRulesTest::RunTest(const FString& Parameters)
{
    TestEqual(TEXT("No cover"), FCoverRules::AcBonus(ECoverLevel::None), 0);
    TestEqual(TEXT("Half cover +2 AC"), FCoverRules::AcBonus(ECoverLevel::Half), 2);
    TestEqual(TEXT("Three-quarters +5 AC"), FCoverRules::AcBonus(ECoverLevel::ThreeQuarters), 5);
    TestEqual(TEXT("Total cover no AC bonus"), FCoverRules::AcBonus(ECoverLevel::Total), 0);
    TestEqual(TEXT("Half helps Dex saves"), FCoverRules::DexSaveBonus(ECoverLevel::Half), 2);
    TestEqual(TEXT("Three-quarters helps Dex saves"), FCoverRules::DexSaveBonus(ECoverLevel::ThreeQuarters), 5);
    TestFalse(TEXT("Half does not block targeting"), FCoverRules::BlocksTargeting(ECoverLevel::Half));
    TestTrue(TEXT("Total blocks targeting"), FCoverRules::BlocksTargeting(ECoverLevel::Total));
    TestTrue(TEXT("Large interposer vs Medium is half"), FCoverRules::CoverFromLargerCreature(ECreatureSize::Medium, ECreatureSize::Large) == ECoverLevel::Half);
    TestTrue(TEXT("Same size is none"), FCoverRules::CoverFromLargerCreature(ECreatureSize::Medium, ECreatureSize::Medium) == ECoverLevel::None);
    TestTrue(TEXT("Smaller interposer is none"), FCoverRules::CoverFromLargerCreature(ECreatureSize::Large, ECreatureSize::Medium) == ECoverLevel::None);
    TestTrue(TEXT("INT 6 ignores cover"), FCoverRules::IgnoresCover(6));
    TestTrue(TEXT("INT 3 ignores cover"), FCoverRules::IgnoresCover(3));
    TestFalse(TEXT("INT 7 does not"), FCoverRules::IgnoresCover(7));
    return !HasAnyErrors();
}
#endif
