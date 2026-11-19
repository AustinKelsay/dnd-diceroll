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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FClassFeatureRows521Test, "Project.Expedition.Rules.ClassFeatureRows521",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FClassFeatureRows521Test::RunTest(const FString& Parameters)
{
    // #177 review finding 4: action costs and unlock levels from the SRD 5.2.1 class and subclass sections.
    using R = EAdventurerRole;
    auto Find = [](const TArray<FClassFeature>& Rows, const TCHAR* Prefix) -> const FClassFeature*
    { return Rows.FindByPredicate([Prefix](const FClassFeature& F) { return FString(F.Name).StartsWith(Prefix); }); };
    const TArray<FClassFeature> Paladin1 = FClassFeatureRules::FeaturesThrough(R::Paladin, 1);
    const FClassFeature* Lay = Find(Paladin1, TEXT("Lay on Hands"));
    TestTrue(TEXT("Paladin 1: Lay on Hands is a Bonus Action"), Lay && Lay->Cost == EActionCost::BonusAction);
    const TArray<FClassFeature> Druid2 = FClassFeatureRules::FeaturesThrough(R::Druid, 2);
    const FClassFeature* Wild = Find(Druid2, TEXT("Wild Shape"));
    TestTrue(TEXT("Druid 2: Wild Shape is a Bonus Action"), Wild && Wild->Cost == EActionCost::BonusAction);
    TestTrue(TEXT("Druid 2 lists no Natural Recovery"), Find(Druid2, TEXT("Land: Natural Recovery")) == nullptr);
    const TArray<FClassFeature> Druid5 = FClassFeatureRules::FeaturesThrough(R::Druid, 5);
    TestTrue(TEXT("Druid 5 lists no Natural Recovery"), Find(Druid5, TEXT("Land: Natural Recovery")) == nullptr);
    const TArray<FClassFeature> Druid6 = FClassFeatureRules::DataFeaturesThrough(R::Druid, 6);
    const FClassFeature* Recovery = Find(Druid6, TEXT("Land: Natural Recovery"));
    TestTrue(TEXT("Natural Recovery is a Circle of the Land level 6 feature"), Recovery && Recovery->Level == 6);
    const TArray<FClassFeature> Sorcerer1 = FClassFeatureRules::FeaturesThrough(R::Sorcerer, 1);
    const TArray<FClassFeature> Sorcerer2 = FClassFeatureRules::FeaturesThrough(R::Sorcerer, 2);
    TestTrue(TEXT("Sorcerer 1 has no Metamagic"), Find(Sorcerer1, TEXT("Metamagic")) == nullptr);
    const FClassFeature* Metamagic = Find(Sorcerer2, TEXT("Metamagic"));
    TestTrue(TEXT("Sorcerer 2 has Metamagic"), Metamagic && Metamagic->Level == 2);
    const TArray<FClassFeature> Cleric2 = FClassFeatureRules::FeaturesThrough(R::Cleric, 2);
    const TArray<FClassFeature> Cleric3 = FClassFeatureRules::FeaturesThrough(R::Cleric, 3);
    TestTrue(TEXT("Cleric 2 has no Disciple of Life before the subclass"), Find(Cleric2, TEXT("Life: Disciple of Life")) == nullptr);
    const FClassFeature* Disciple = Find(Cleric3, TEXT("Life: Disciple of Life"));
    TestTrue(TEXT("Disciple of Life arrives with the Life Domain at Cleric 3"), Disciple && Disciple->Level == 3);
    const TArray<FClassFeature> Wizard2 = FClassFeatureRules::FeaturesThrough(R::Wizard, 2);
    const TArray<FClassFeature> Wizard5 = FClassFeatureRules::FeaturesThrough(R::Wizard, 5);
    TestTrue(TEXT("Wizard 2 lists no Sculpt Spells"), Find(Wizard2, TEXT("Evocation: Sculpt Spells")) == nullptr);
    TestTrue(TEXT("Wizard 5 lists no Sculpt Spells"), Find(Wizard5, TEXT("Evocation: Sculpt Spells")) == nullptr);
    const TArray<FClassFeature> Wizard6 = FClassFeatureRules::DataFeaturesThrough(R::Wizard, 6);
    const FClassFeature* Sculpt = Find(Wizard6, TEXT("Evocation: Sculpt Spells"));
    TestTrue(TEXT("Sculpt Spells is an Evoker level 6 feature"), Sculpt && Sculpt->Level == 6);
    return !HasAnyErrors();
}
#endif
