#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Rules/LockTrapRules.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLockTrapRulesTest, "Project.Expedition.Rules.LocksAndTraps",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FLockTrapRulesTest::RunTest(const FString& Parameters)
{
    TestEqual(TEXT("Simple lock DC 10"), FLockTrapRules::LockDc(ELockTier::Simple), 10);
    TestEqual(TEXT("Good lock DC 15"), FLockTrapRules::LockDc(ELockTier::Good), 15);
    TestEqual(TEXT("Superior lock DC 20"), FLockTrapRules::LockDc(ELockTier::Superior), 20);
    TestTrue(TEXT("Tools pick a simple lock on 10"), FLockTrapRules::Picks(10, ELockTier::Simple, true));
    TestFalse(TEXT("No tools, no pick"), FLockTrapRules::Picks(30, ELockTier::Simple, false));
    TestFalse(TEXT("Short of the DC"), FLockTrapRules::Picks(14, ELockTier::Good, true));
    TestTrue(TEXT("Finds trap at DC"), FLockTrapRules::FindsTrap(13, 13));
    TestFalse(TEXT("Misses trap below DC"), FLockTrapRules::FindsTrap(12, 13));
    TestTrue(TEXT("Disarms at DC"), FLockTrapRules::Disarm(15, 15, true) == EDisarmResult::Disarmed);
    TestTrue(TEXT("Fail by 4 is safe"), FLockTrapRules::Disarm(11, 15, true) == EDisarmResult::Failed);
    TestTrue(TEXT("Fail by 5 triggers"), FLockTrapRules::Disarm(10, 15, true) == EDisarmResult::Triggered);
    TestTrue(TEXT("No tools cannot disarm"), FLockTrapRules::Disarm(30, 15, false) == EDisarmResult::Failed);
    return !HasAnyErrors();
}
#endif
