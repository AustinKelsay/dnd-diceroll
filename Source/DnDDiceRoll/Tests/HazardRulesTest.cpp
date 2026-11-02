#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Rules/HazardRules.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHazardRulesTest, "Project.Expedition.Rules.ExplorationHazards",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FHazardRulesTest::RunTest(const FString& Parameters)
{
    TestTrue(TEXT("Cliff is Athletics DC 12"), FHazardRules::Spec(EHazard::Cliff).Skill == ESkill::Athletics && FHazardRules::Spec(EHazard::Cliff).Dc == 12);
    TestTrue(TEXT("Ledge is Acrobatics"), FHazardRules::Spec(EHazard::Ledge).Skill == ESkill::Acrobatics);
    TestTrue(TEXT("Lost is Survival and reroutes"), FHazardRules::Spec(EHazard::Lost).Skill == ESkill::Survival && FHazardRules::Spec(EHazard::Lost).bRerouteOnFail);
    TestTrue(TEXT("Hidden passage is Investigation DC 15"), FHazardRules::Spec(EHazard::HiddenPassage).Skill == ESkill::Investigation && FHazardRules::Spec(EHazard::HiddenPassage).Dc == 15);
    TestEqual(TEXT("Forecast matches check math"), FHazardRules::ForecastOutOf400(EHazard::Cliff, 3, ERollMode::Advantage), FCheckRules::SuccessOutOf400(3, 12, ERollMode::Advantage));
    int32 State = 42;
    const FHazardResult Fail = FHazardRules::Resolve(EHazard::Cliff, -40, ERollMode::Normal, State);
    TestFalse(TEXT("Hopeless climb fails"), Fail.Check.bSuccess);
    TestEqual(TEXT("Failed climb costs 4 HP"), Fail.HitPointsLost, 4);
    TestNotEqual(TEXT("Resolve advances the stream"), State, 42);
    const FHazardResult Lost = FHazardRules::Resolve(EHazard::Lost, -40, ERollMode::Normal, State);
    TestTrue(TEXT("Getting lost costs supplies and reroutes"), Lost.SuppliesLost == 1 && Lost.bRerouted);
    const FHazardResult Found = FHazardRules::Resolve(EHazard::HiddenPassage, 40, ERollMode::Normal, State);
    TestTrue(TEXT("Search finds the shortcut"), Found.Check.bSuccess && Found.bFoundShortcut && Found.HitPointsLost == 0);
    const FHazardResult Safe = FHazardRules::Resolve(EHazard::Ledge, 40, ERollMode::Normal, State);
    TestEqual(TEXT("Success costs nothing"), Safe.HitPointsLost + Safe.SuppliesLost, 0);
    return !HasAnyErrors();
}
#endif
