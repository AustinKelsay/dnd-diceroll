#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Rules/SleightRules.h"
#include "Combat/CombatBattle.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSleightOfHandTest, "Project.Expedition.Rules.SleightOfHand",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSleightOfHandTest::RunTest(const FString& Parameters)
{
    TestEqual(TEXT("+5 vs passive 13: 13/20"), FSleightRules::SuccessOutOf400(5, ERollMode::Normal, 13), 260);
    TestTrue(TEXT("Meeting passive Perception: unnoticed"), FSleightRules::ResultFor(13, 13) == ESleightResult::Unnoticed);
    TestTrue(TEXT("Missing by 4: fumbled, not caught"), FSleightRules::ResultFor(9, 13) == ESleightResult::Fumbled);
    TestTrue(TEXT("Missing by 5: caught"), FSleightRules::ResultFor(8, 13) == ESleightResult::Caught);
    int32 SeedA = 4242, SeedB = 4242;
    const FSleightOutcome A = FSleightRules::Resolve(5, ERollMode::Normal, 13, SeedA);
    const FSleightOutcome B = FSleightRules::Resolve(5, ERollMode::Normal, 13, SeedB);
    TestTrue(TEXT("Seeded and deterministic"), A.Check.Total == B.Check.Total && A.Result == B.Result && SeedA == SeedB && SeedA != 4242);
    TestTrue(TEXT("Resolve matches ResultFor"), A.Result == FSleightRules::ResultFor(A.Check.Total, 13));
    int32 Seed = 7;
    TestTrue(TEXT("Huge bonus never caught"), FSleightRules::Resolve(30, ERollMode::Normal, 13, Seed).Result == ESleightResult::Unnoticed);
    TestTrue(TEXT("Rogue 2 Cunning Action"), FSleightRules::UseObjectAsBonusAction(EAdventurerRole::Rogue, 2));
    TestFalse(TEXT("Rogue 1 no"), FSleightRules::UseObjectAsBonusAction(EAdventurerRole::Rogue, 1));
    TestFalse(TEXT("Fighter no"), FSleightRules::UseObjectAsBonusAction(EAdventurerRole::Fighter, 5));
    return !HasAnyErrors();
}
#endif
