#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Rules/ClassFeatureRules.h"
#include "Combat/CombatBattle.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FClassFeatureRulesTest, "Project.Expedition.Rules.ClassFeatureScaling",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FClassFeatureRulesTest::RunTest(const FString& Parameters)
{
    TestEqual(TEXT("Second Wind 6 at level 3"), FClassFeatureRules::SecondWindHealing(6, 3), 9);
    TestEqual(TEXT("No Action Surge at 1"), FClassFeatureRules::ActionSurgeUses(1), 0);
    TestEqual(TEXT("Action Surge at 2"), FClassFeatureRules::ActionSurgeUses(2), 1);
    TestEqual(TEXT("Two surges at 17"), FClassFeatureRules::ActionSurgeUses(17), 2);
    TestEqual(TEXT("Fighter 4 one attack"), FClassFeatureRules::AttacksPerAction(EAdventurerRole::Fighter, 4), 1);
    TestEqual(TEXT("Fighter 5 Extra Attack"), FClassFeatureRules::AttacksPerAction(EAdventurerRole::Fighter, 5), 2);
    TestEqual(TEXT("Fighter 11 three"), FClassFeatureRules::AttacksPerAction(EAdventurerRole::Fighter, 11), 3);
    TestEqual(TEXT("Rogue 5 one"), FClassFeatureRules::AttacksPerAction(EAdventurerRole::Rogue, 5), 1);
    TestEqual(TEXT("Sneak 1d6 at 1"), FClassFeatureRules::SneakAttackDice(1), 1);
    TestEqual(TEXT("Sneak 2d6 at 3"), FClassFeatureRules::SneakAttackDice(3), 2);
    TestEqual(TEXT("Sneak 10d6 at 20"), FClassFeatureRules::SneakAttackDice(20), 10);
    TestTrue(TEXT("Advantage enables Sneak"), FClassFeatureRules::SneakAttackAllowed(true, false, ERollMode::Advantage, false));
    TestTrue(TEXT("Ally enables Sneak"), FClassFeatureRules::SneakAttackAllowed(true, false, ERollMode::Normal, true));
    TestFalse(TEXT("Disadvantage blocks Sneak"), FClassFeatureRules::SneakAttackAllowed(true, false, ERollMode::Disadvantage, true));
    TestFalse(TEXT("Once per turn"), FClassFeatureRules::SneakAttackAllowed(true, true, ERollMode::Advantage, true));
    TestFalse(TEXT("Needs finesse or ranged"), FClassFeatureRules::SneakAttackAllowed(false, false, ERollMode::Advantage, true));
    TestTrue(TEXT("Cunning Action at 2"), FClassFeatureRules::HasCunningAction(2) && !FClassFeatureRules::HasCunningAction(1));
    TestEqual(TEXT("Channel Divinity at 2"), FClassFeatureRules::ChannelDivinityUses(2), 1);
    TestEqual(TEXT("Channel Divinity x2 at 6"), FClassFeatureRules::ChannelDivinityUses(6), 2);
    TestEqual(TEXT("Arcane Recovery 3 at level 5"), FClassFeatureRules::ArcaneRecoveryLevels(5), 3);
    TestEqual(TEXT("Arcane Recovery 1 at level 1"), FClassFeatureRules::ArcaneRecoveryLevels(1), 1);
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FClassFeatureCatalogTest, "Project.Expedition.Rules.ClassFeatureCatalog",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FClassFeatureCatalogTest::RunTest(const FString& Parameters)
{
	const TArray<FClassFeature> Fighter1 = FClassFeatureRules::FeaturesThrough(EAdventurerRole::Fighter, 1);
	TestTrue(TEXT("Fighter 1 has Second Wind"), Fighter1.ContainsByPredicate([](const FClassFeature& F) { return FString(F.Name) == TEXT("Second Wind") && F.bPlayable; }));
	const TArray<FClassFeature> Fighter2 = FClassFeatureRules::FeaturesThrough(EAdventurerRole::Fighter, 2);
	TestTrue(TEXT("Fighter 2 has Action Surge"), Fighter2.ContainsByPredicate([](const FClassFeature& F) { return FString(F.Name) == TEXT("Action Surge"); }));
	const TArray<FClassFeature> Rogue2 = FClassFeatureRules::FeaturesThrough(EAdventurerRole::Rogue, 2);
	TestTrue(TEXT("Rogue 2 has Cunning Action"), Rogue2.ContainsByPredicate([](const FClassFeature& F) { return FString(F.Name) == TEXT("Cunning Action"); }));
	const TArray<FClassFeature> Cap = FClassFeatureRules::FeaturesThrough(EAdventurerRole::Warlock, 20);
	TestTrue(TEXT("FeaturesThrough clamps to engine cap"), Cap.ContainsByPredicate([](const FClassFeature& F) { return F.Level <= FClassFeatureRules::EngineLevelCap; }));
	TestFalse(TEXT("FeaturesThrough omits L6+"), Cap.ContainsByPredicate([](const FClassFeature& F) { return F.Level > FClassFeatureRules::EngineLevelCap; }));
	const TArray<FClassFeature> Data = FClassFeatureRules::DataFeaturesThrough(EAdventurerRole::Warlock, 20);
	TestTrue(TEXT("DataFeaturesThrough keeps L6+"), Data.ContainsByPredicate([](const FClassFeature& F) { return F.Level >= 6; }));
	TestTrue(TEXT("Summary names Second Wind"), FClassFeatureRules::FeatureSummary(EAdventurerRole::Fighter, 1).Contains(TEXT("Second Wind")));
	return true;
}
#endif
