#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Rules/BeastRules.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnimalHandlingTest, "Project.Expedition.Rules.AnimalHandling",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FAnimalHandlingTest::RunTest(const FString& Parameters)
{
    TestTrue(TEXT("Wolf is a beast"), FBeastRules::IsBeast(TEXT("Wolf")));
    TestFalse(TEXT("Goblin is not"), FBeastRules::IsBeast(TEXT("Goblin")));
    TestEqual(TEXT("Wolf DC 13"), FBeastRules::CalmDc(TEXT("Wolf")), 13);
    TestTrue(TEXT("Miss: hostile"), FBeastRules::BehaviourFor(12, 13) == EBeastBehaviour::Hostile);
    TestTrue(TEXT("Meet: calmed"), FBeastRules::BehaviourFor(13, 13) == EBeastBehaviour::Calmed);
    TestTrue(TEXT("Beat by 5: redirected"), FBeastRules::BehaviourFor(18, 13) == EBeastBehaviour::Redirected);
    TestEqual(TEXT("+3 vs DC 13: 11/20"), FBeastRules::CalmOutOf400(3, ERollMode::Normal, TEXT("Wolf"), false), 220);
    TestEqual(TEXT("Hurt beast cannot be calmed"), FBeastRules::CalmOutOf400(3, ERollMode::Normal, TEXT("Wolf"), true), 0);
    TestEqual(TEXT("Non-beast cannot be calmed"), FBeastRules::CalmOutOf400(3, ERollMode::Normal, TEXT("Goblin"), false), 0);
    int32 Seed = 555;
    const FBeastOutcome Rejected = FBeastRules::Resolve(3, ERollMode::Normal, TEXT("Goblin"), false, Seed);
    TestTrue(TEXT("Rejected attempt keeps RNG"), Seed == 555 && Rejected.Behaviour == EBeastBehaviour::Hostile);
    int32 SeedA = 555, SeedB = 555;
    const FBeastOutcome A = FBeastRules::Resolve(3, ERollMode::Normal, TEXT("Wolf"), false, SeedA);
    const FBeastOutcome B = FBeastRules::Resolve(3, ERollMode::Normal, TEXT("Wolf"), false, SeedB);
    TestTrue(TEXT("Calming is deterministic per seed"), A.Behaviour == B.Behaviour && A.Check.Total == B.Check.Total && SeedA != 555);
    TestTrue(TEXT("Resolve matches table"), A.Behaviour == FBeastRules::BehaviourFor(A.Check.Total, 13));
    int32 Sure = 1;
    TestTrue(TEXT("Huge bonus redirects"), FBeastRules::Resolve(30, ERollMode::Normal, TEXT("Brown Bear"), false, Sure).Behaviour == EBeastBehaviour::Redirected);
    TestEqual(TEXT("Mount costs half speed (6 -> 3)"), FBeastRules::MountCost(6), 3);
    TestEqual(TEXT("Mount rounds up (5 -> 3)"), FBeastRules::MountCost(5), 3);
    TestTrue(TEXT("Controlled mount may Dash"), FBeastRules::ControlledMountMayTake(TEXT("Dash")));
    TestFalse(TEXT("Controlled mount may not Attack"), FBeastRules::ControlledMountMayTake(TEXT("Attack")));
    return !HasAnyErrors();
}
#endif
