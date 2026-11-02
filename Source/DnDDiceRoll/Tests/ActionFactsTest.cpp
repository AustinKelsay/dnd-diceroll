#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Combat/CombatBattle.h"
#include "Rules/SpellRules.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FActionFactsTest, "Project.Expedition.Rules.ActionFactsAndSpellTags",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FActionFactsTest::RunTest(const FString& Parameters)
{
    FCombatantState Unit;
    Unit.SecondWindUses = 1;
    Unit.SpellSlots = 1;
    Unit.Potions = 3;
    Unit.ManeuverUses = 2;
    // One facts query owns the usage text the HUD buttons used to format themselves.
    TestEqual(TEXT("Second Wind usage"), FCombatBattle::UsageTag(ECombatCommandType::SecondWind, Unit), FString(TEXT("bonus, 1 left")));
    TestEqual(TEXT("Single slot is singular"), FCombatBattle::UsageTag(ECombatCommandType::CureWounds, Unit), FString(TEXT("1 slot left")));
    Unit.SpellSlots = 2;
    TestEqual(TEXT("Plural slots"), FCombatBattle::UsageTag(ECombatCommandType::MagicMissile, Unit), FString(TEXT("2 slots left")));
    Unit.SpellSlots = 0;
    TestEqual(TEXT("Zero slots"), FCombatBattle::UsageTag(ECombatCommandType::MagicMissile, Unit), FString(TEXT("0 slots left")));
    TestEqual(TEXT("Potion usage"), FCombatBattle::UsageTag(ECombatCommandType::Potion, Unit), FString(TEXT("3 left")));
    TestEqual(TEXT("Maneuver usage"), FCombatBattle::UsageTag(ECombatCommandType::PrecisionStrike, Unit), FString(TEXT("+d8 to hit, 2 dice left")));
    TestTrue(TEXT("Attack has no resource"), FCombatBattle::UsageTag(ECombatCommandType::Attack, Unit).IsEmpty());
    // Cost tag is the second field of the action description, so tooltip and chip cannot drift.
    TestEqual(TEXT("Cure cost"), FCombatBattle::CostTag(ECombatCommandType::CureWounds), FString(TEXT("Action + 1 slot")));
    TestEqual(TEXT("Second Wind cost"), FCombatBattle::CostTag(ECombatCommandType::SecondWind), FString(TEXT("Bonus + 1 use")));
    TestTrue(TEXT("Description contains its cost tag"), FCombatBattle::DescribeAction(ECombatCommandType::Potion).Contains(FCombatBattle::CostTag(ECombatCommandType::Potion)));
    // Spell hover tags come from the #71 spell facts.
    TestEqual(TEXT("Magic Missile spell tag"), FCombatBattle::SpellTag(ECombatCommandType::MagicMissile), FSpellRules::Summary(ESpell::MagicMissile));
    TestEqual(TEXT("Cure Wounds spell tag"), FCombatBattle::SpellTag(ECombatCommandType::CureWounds), FSpellRules::Summary(ESpell::CureWounds));
    TestTrue(TEXT("Non-spell has no spell tag"), FCombatBattle::SpellTag(ECombatCommandType::Potion).IsEmpty() && FCombatBattle::SpellTag(ECombatCommandType::SecondWind).IsEmpty());
    return true;
}
#endif
