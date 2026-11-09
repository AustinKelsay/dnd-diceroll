#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Rules/GroupCheckRules.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPassiveHelpGroupTest, "Project.Expedition.Rules.PassiveHelpGroupChecks",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FPassiveHelpGroupTest::RunTest(const FString& Parameters)
{
    TestEqual(TEXT("Passive: 10 + 3"), FGroupCheckRules::PassiveScore(3, ERollMode::Normal), 13);
    TestEqual(TEXT("Passive with advantage: +5"), FGroupCheckRules::PassiveScore(3, ERollMode::Advantage), 18);
    TestEqual(TEXT("Passive with disadvantage: -5"), FGroupCheckRules::PassiveScore(-1, ERollMode::Disadvantage), 4);
    TestTrue(TEXT("Help grants advantage"), FGroupCheckRules::HelpedMode(0, 0, true) == ERollMode::Advantage);
    TestTrue(TEXT("Help cancels against disadvantage"), FGroupCheckRules::HelpedMode(0, 2, true) == ERollMode::Normal);
    TestTrue(TEXT("No help, no change"), FGroupCheckRules::HelpedMode(0, 1, false) == ERollMode::Disadvantage);
    TestTrue(TEXT("Group of 4: 2 successes pass"), FGroupCheckRules::GroupSucceeds(2, 4));
    TestFalse(TEXT("Group of 4: 1 success fails"), FGroupCheckRules::GroupSucceeds(1, 4));
    TestTrue(TEXT("Group of 3: 2 successes pass"), FGroupCheckRules::GroupSucceeds(2, 3));
    TestFalse(TEXT("Group of 3: 1 success fails"), FGroupCheckRules::GroupSucceeds(1, 3));
    TestFalse(TEXT("Empty group fails"), FGroupCheckRules::GroupSucceeds(0, 0));
    // Two members at exactly 50% (+0 vs DC 11): at least one of two succeeds 75% of the time.
    const TArray<FGroupMember> Pair = {{0, ERollMode::Normal}, {0, ERollMode::Normal}};
    TestEqual(TEXT("Group chance: two coin flips"), FGroupCheckRules::GroupSuccessPerMillion(Pair, 11), 750000);
    const TArray<FGroupMember> Sure = {{20, ERollMode::Normal}, {-20, ERollMode::Normal}};
    TestEqual(TEXT("Group chance: one sure success carries a pair"), FGroupCheckRules::GroupSuccessPerMillion(Sure, 15), 1000000);
    const TArray<FGroupMember> Party = {{5, ERollMode::Normal}, {0, ERollMode::Advantage}, {-1, ERollMode::Disadvantage}, {2, ERollMode::Normal}};
    int32 State = 1234, Replay = 1234;
    const FGroupCheckOutcome Group = FGroupCheckRules::Resolve(Party, 12, State);
    int32 Successes = 0;
    for (int32 Index = 0; Index < Party.Num(); ++Index)
    {
        const FD20Outcome One = FCheckRules::Resolve(Party[Index].Modifier, 12, Party[Index].Mode, Replay);
        TestTrue(FString::Printf(TEXT("Member %d matches a solo roll in order"), Index), Group.Members.IsValidIndex(Index) && Group.Members[Index].Total == One.Total && Group.Members[Index].bSuccess == One.bSuccess);
        Successes += One.bSuccess ? 1 : 0;
    }
    TestEqual(TEXT("Group RNG advances exactly like solo rolls"), State, Replay);
    TestTrue(TEXT("Group result follows the half rule"), Group.Successes == Successes && Group.bSuccess == (Successes * 2 >= 4));
    return !HasAnyErrors();
}
#endif
