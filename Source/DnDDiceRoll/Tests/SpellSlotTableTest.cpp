#include "Misc/AutomationTest.h"
#include "Rules/SpellRules.h"
#include "Rules/LevelRules.h"
#include "Rules/ClassFeatureRules.h"
#include "Party/FeatureKit.h"
#include "Party/BuildCatalog.h"
#include "Combat/CombatBattle.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSpellSlotTableTest, "Project.Expedition.Rules.SpellSlotTable", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSpellSlotTableTest::RunTest(const FString&)
{
    // SRD 5.2.1 full-caster table, levels 1-20 (data-driven; engine plays 1..EngineLevelCap).
    const int32 Full20[9] = {4, 3, 3, 3, 3, 2, 2, 1, 1};
    for (int32 L = 1; L <= 9; ++L) { TestEqual(FString::Printf(TEXT("Wizard 20 slot %d"), L), FSpellRules::SlotsFor(EAdventurerRole::Wizard, 20, L), Full20[L - 1]); }
    TestEqual(TEXT("Wizard 7 gains a 4th-level slot"), FSpellRules::SlotsFor(EAdventurerRole::Wizard, 7, 4), 1);
    TestEqual(TEXT("Wizard 6 has no 4th-level slot"), FSpellRules::SlotsFor(EAdventurerRole::Wizard, 6, 4), 0);
    TestEqual(TEXT("Paladin 13 gains a 4th-level slot"), FSpellRules::SlotsFor(EAdventurerRole::Paladin, 13, 4), 1);
    TestEqual(TEXT("Warlock 7 pact slots are 4th level"), FSpellRules::SlotsFor(EAdventurerRole::Warlock, 7, 4), 2);
    TestEqual(TEXT("Warlock 20 has four 5th-level slots"), FSpellRules::SlotsFor(EAdventurerRole::Warlock, 20, 5), 4);
    TestEqual(TEXT("Wizard 5 / Cleric 2 multiclass gets a 4th"), FSpellRules::HeroSlotsFor(EAdventurerRole::Wizard, 5, EAdventurerRole::Cleric, 2, 4), 1);
    TestEqual(TEXT("Beyond the tables"), FSpellRules::SlotsFor(EAdventurerRole::Wizard, 20, 10), 0);
    // Within the engine level cap no class reaches 4th-level slots (SRD: full casters at 7).
    for (int32 Role = 0; Role <= (int32)EAdventurerRole::Artificer; ++Role)
    for (int32 L = 1; L <= FClassFeatureRules::EngineLevelCap; ++L)
    { TestEqual(TEXT("No 4th-level slot within cap"), FSpellRules::SlotsFor((EAdventurerRole)Role, L, 4), 0); }
    // Long rest fills the per-level pool straight from the table.
    FCombatantState W; W.AdventurerRole = EAdventurerRole::Wizard; W.Level = 5; W.SpellSlots = 4;
    FFeatureKit::RefillSlotTable(W);
    TestEqual(TEXT("Pool sized to MaxSpellLevel"), W.SlotsByLevel.Num(), FSpellRules::MaxSpellLevel);
    if (W.SlotsByLevel.Num() == 4) { TestEqual(TEXT("L2"), W.SlotsByLevel[1], 3); TestEqual(TEXT("L3"), W.SlotsByLevel[2], 2); TestEqual(TEXT("L4"), W.SlotsByLevel[3], 0); }
    FCombatantState Low; Low.AdventurerRole = EAdventurerRole::Wizard; Low.Level = 2; Low.SpellSlots = 2;
    FFeatureKit::RefillSlotTable(Low);
    TestTrue(TEXT("1st-level-only casters keep the flat pool"), Low.SlotsByLevel.IsEmpty());
    return true;
}