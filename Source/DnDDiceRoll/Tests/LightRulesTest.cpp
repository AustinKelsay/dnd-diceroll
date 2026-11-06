#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Rules/LightRules.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLightVisionTest, "Project.Expedition.Rules.LightAndVision",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FLightVisionTest::RunTest(const FString& Parameters)
{
    TestTrue(TEXT("Darkness without darkvision is heavily obscured and unseen"),
        FLightRules::Obscurement(FLightRules::PerceivedLight(ELight::Darkness, 0, 10)) == EObscurement::Heavy && !FLightRules::CanSee(FLightRules::PerceivedLight(ELight::Darkness, 0, 10)));
    TestTrue(TEXT("Darkvision treats darkness as dim within range"), FLightRules::PerceivedLight(ELight::Darkness, 60, 60) == ELight::Dim);
    TestTrue(TEXT("Darkvision treats dim as bright"), FLightRules::PerceivedLight(ELight::Dim, 60, 30) == ELight::Bright);
    TestTrue(TEXT("Beyond darkvision range darkness stays dark"), FLightRules::PerceivedLight(ELight::Darkness, 60, 65) == ELight::Darkness);
    TestTrue(TEXT("Dim light: disadvantage on sight Perception"), FLightRules::SightPerception(FLightRules::Obscurement(ELight::Dim)) == ESightCheck::Disadvantage);
    TestTrue(TEXT("Darkness: sight checks fail"), FLightRules::SightPerception(EObscurement::Heavy) == ESightCheck::AutoFail);
    TestTrue(TEXT("Bright: normal"), FLightRules::SightPerception(EObscurement::None) == ESightCheck::Normal);
    TestTrue(TEXT("Unseen attacker has advantage"), FLightRules::AttackMode(true, false) == ERollMode::Advantage);
    TestTrue(TEXT("Attacking an unseen target has disadvantage"), FLightRules::AttackMode(false, true) == ERollMode::Disadvantage);
    TestTrue(TEXT("Both blind cancel"), FLightRules::AttackMode(false, false) == ERollMode::Normal);
    TestTrue(TEXT("Hide needs obscurement"), FLightRules::CanTryToHide(EObscurement::Light) && !FLightRules::CanTryToHide(EObscurement::None));
    TestEqual(TEXT("Goblin darkvision 60"), FLightRules::DarkvisionFeet(TEXT("Goblin")), 60);
    TestEqual(TEXT("Bandit has none"), FLightRules::DarkvisionFeet(TEXT("Bandit")), 0);
    return !HasAnyErrors();
}
#endif
