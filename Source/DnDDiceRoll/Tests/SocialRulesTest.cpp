#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Rules/SocialRules.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSocialRulesTest, "Project.Expedition.Rules.SocialInfluence",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSocialRulesTest::RunTest(const FString& Parameters)
{
    TestEqual(TEXT("Friendly DC 10"), FSocialRules::InfluenceDc(ENpcAttitude::Friendly), 10);
    TestEqual(TEXT("Indifferent DC 15"), FSocialRules::InfluenceDc(ENpcAttitude::Indifferent), 15);
    TestEqual(TEXT("Hostile DC 20"), FSocialRules::InfluenceDc(ENpcAttitude::Hostile), 20);
    TestTrue(TEXT("20 sways a hostile"), FSocialRules::AfterInfluence(ENpcAttitude::Hostile, 20) == ENpcAttitude::Indifferent);
    TestTrue(TEXT("19 does not"), FSocialRules::AfterInfluence(ENpcAttitude::Hostile, 19) == ENpcAttitude::Hostile);
    TestTrue(TEXT("Friendly stays friendly"), FSocialRules::AfterInfluence(ENpcAttitude::Friendly, 25) == ENpcAttitude::Friendly);
    TestTrue(TEXT("Lie beats Insight"), FSocialRules::LieSucceeds(14, 13));
    TestFalse(TEXT("Tie goes to the target"), FSocialRules::LieSucceeds(13, 13));
    TestTrue(TEXT("Swayed hostile avoids the fight"), FSocialRules::OutcomeFor(ENpcAttitude::Hostile, ENpcAttitude::Indifferent) == ESocialOutcome::AvoidFight);
    TestTrue(TEXT("Won-over NPC shares information"), FSocialRules::OutcomeFor(ENpcAttitude::Indifferent, ENpcAttitude::Friendly) == ESocialOutcome::Information);
    TestTrue(TEXT("Friendly NPC rewards"), FSocialRules::OutcomeFor(ENpcAttitude::Friendly, ENpcAttitude::Friendly) == ESocialOutcome::Reward);
    TestTrue(TEXT("Unmoved NPC gives nothing"), FSocialRules::OutcomeFor(ENpcAttitude::Indifferent, ENpcAttitude::Indifferent) == ESocialOutcome::None);
    return !HasAnyErrors();
}
#endif
