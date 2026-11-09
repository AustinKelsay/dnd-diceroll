#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Rules/GrappleRules.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGrappleRulesTest, "Project.Expedition.Rules.GrappleAndShove",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FGrappleRulesTest::RunTest(const FString& Parameters)
{
    TestTrue(TEXT("Medium grapples Large"), FGrappleRules::SizeAllows(ECreatureSize::Medium, ECreatureSize::Large));
    TestFalse(TEXT("Medium cannot grapple Huge"), FGrappleRules::SizeAllows(ECreatureSize::Medium, ECreatureSize::Huge));
    TestTrue(TEXT("Small grapples Tiny"), FGrappleRules::SizeAllows(ECreatureSize::Small, ECreatureSize::Tiny));
    TestEqual(TEXT("Even contest, ties hold"), FGrappleRules::ContestOutOf400(0, 0), 190);
    TestEqual(TEXT("+5 edge"), FGrappleRules::ContestOutOf400(5, 0), 280);
    TestEqual(TEXT("Hopeless contest"), FGrappleRules::ContestOutOf400(0, 20), 0);
    TestEqual(TEXT("Grappled speed is 0"), FGrappleRules::GrappledSpeed(6), 0);
    TestEqual(TEXT("Drag doubles"), FGrappleRules::DragCost(3), 6);
    TestEqual(TEXT("Escape mirrors the contest"), FGrappleRules::EscapeOutOf400(2, 4), FGrappleRules::ContestOutOf400(2, 4));
    TestEqual(TEXT("Won push moves 1 cell"), FGrappleRules::ShovePushCells(true, EShoveChoice::PushAway), 1);
    TestEqual(TEXT("Lost push moves nothing"), FGrappleRules::ShovePushCells(false, EShoveChoice::PushAway), 0);
    TestTrue(TEXT("Won shove knocks prone"), FGrappleRules::ShoveKnocksProne(true, EShoveChoice::KnockProne));
    TestFalse(TEXT("Push does not knock prone"), FGrappleRules::ShoveKnocksProne(true, EShoveChoice::PushAway));
    return !HasAnyErrors();
}
#endif
