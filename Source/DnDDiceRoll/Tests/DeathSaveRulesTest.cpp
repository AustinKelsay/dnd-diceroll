#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Rules/DeathSaveRules.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeathSaveRulesTest, "Project.Expedition.Rules.DeathSaves",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FDeathSaveRulesTest::RunTest(const FString& Parameters)
{
    FDeathSaveTally SavingTally;
    TestTrue(TEXT("10 succeeds"), FDeathSaveRules::ApplySave(SavingTally, 10) == EDeathState::Dying && SavingTally.Successes == 1);
    FDeathSaveRules::ApplySave(SavingTally, 15);
    TestTrue(TEXT("Third success stabilizes"), FDeathSaveRules::ApplySave(SavingTally, 12) == EDeathState::Stable);
    FDeathSaveTally FailingTally;
    TestTrue(TEXT("Nat 1 is two failures"), FDeathSaveRules::ApplySave(FailingTally, 1) == EDeathState::Dying && FailingTally.Failures == 2);
    TestTrue(TEXT("Third failure kills"), FDeathSaveRules::ApplySave(FailingTally, 9) == EDeathState::Dead);
    FDeathSaveTally RevivingTally;
    RevivingTally.Failures = 2;
    TestTrue(TEXT("Nat 20 revives and resets"), FDeathSaveRules::ApplySave(RevivingTally, 20) == EDeathState::Revived && RevivingTally.Failures == 0);
    FDeathSaveTally HitTally;
    TestTrue(TEXT("Hit at 0 is one failure"), FDeathSaveRules::ApplyDamageAtZero(HitTally, 3, false, 20) == EDeathState::Dying && HitTally.Failures == 1);
    TestTrue(TEXT("Crit at 0 is two more, dead"), FDeathSaveRules::ApplyDamageAtZero(HitTally, 3, true, 20) == EDeathState::Dead);
    FDeathSaveTally HugeHitTally;
    TestTrue(TEXT("Damage of max HP at 0 kills"), FDeathSaveRules::ApplyDamageAtZero(HugeHitTally, 20, false, 20) == EDeathState::Dead);
    TestTrue(TEXT("Massive damage kills outright"), FDeathSaveRules::InstantDeath(30, 10, 20));
    TestFalse(TEXT("Just short survives"), FDeathSaveRules::InstantDeath(29, 10, 20));
    int32 TemporaryPool = 5;
    TestEqual(TEXT("Temp soaks first"), FDeathSaveRules::AbsorbWithTemporary(TemporaryPool, 8), 3);
    TestEqual(TEXT("Temp spent"), TemporaryPool, 0);
    TestEqual(TEXT("Temp does not stack"), FDeathSaveRules::GainTemporary(6, 4), 6);
    TestTrue(TEXT("Medicine 10 stabilizes"), FDeathSaveRules::MedicineStabilizes(10));
    TestFalse(TEXT("Medicine 9 does not"), FDeathSaveRules::MedicineStabilizes(9));
    return !HasAnyErrors();
}
#endif
