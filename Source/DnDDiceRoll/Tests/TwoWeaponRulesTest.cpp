#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Rules/TwoWeaponRules.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTwoWeaponRulesTest, "Project.Expedition.Rules.TwoWeaponFighting",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FTwoWeaponRulesTest::RunTest(const FString& Parameters)
{
    const FWeaponProfile& MainShortsword = FWeaponRules::Profile(EWeapon::Shortsword);
    const FWeaponProfile& OffDagger = FWeaponRules::Profile(EWeapon::Dagger);
    const FWeaponProfile& MainLongsword = FWeaponRules::Profile(EWeapon::Longsword);
    TestTrue(TEXT("Shortsword and dagger after attacking"), FTwoWeaponRules::CanOffHandAttack(MainShortsword, OffDagger, true, false));
    TestFalse(TEXT("Needs the Attack action"), FTwoWeaponRules::CanOffHandAttack(MainShortsword, OffDagger, false, false));
    TestFalse(TEXT("Needs the bonus action"), FTwoWeaponRules::CanOffHandAttack(MainShortsword, OffDagger, true, true));
    TestFalse(TEXT("Longsword is not light"), FTwoWeaponRules::CanOffHandAttack(MainLongsword, OffDagger, true, false));
    TestEqual(TEXT("Positive modifier dropped"), FTwoWeaponRules::OffHandDamageModifier(3, false), 0);
    TestEqual(TEXT("Negative modifier kept"), FTwoWeaponRules::OffHandDamageModifier(-1, false), -1);
    TestEqual(TEXT("Style adds the modifier"), FTwoWeaponRules::OffHandDamageModifier(3, true), 3);
    return !HasAnyErrors();
}
#endif
