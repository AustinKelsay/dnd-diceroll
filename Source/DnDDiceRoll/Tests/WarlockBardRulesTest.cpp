#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Rules/WarlockRules.h"
#include "Rules/BardRules.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWarlockTableTest, "Project.Expedition.Rules.WarlockTable",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FWarlockTableTest::RunTest(const FString& Parameters)
{
    // SRD 5.2.1 Warlock Features table, levels 1-20: invocations, pact slots, slot level.
    static const int32 Invocations[] = {1, 3, 3, 3, 5, 5, 6, 6, 7, 7, 7, 8, 8, 8, 9, 9, 9, 10, 10, 10};
    static const int32 Slots[] = {1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4};
    static const int32 SlotLevel[] = {1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5};
    static const int32 Cunning[] = {0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 4};
    static const int32 Arcana[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 4, 4};
    for (int32 L = 1; L <= 20; ++L)
    {
        TestEqual(FString::Printf(TEXT("Warlock %d invocations"), L), FWarlockRules::InvocationsKnown(L), Invocations[L - 1]);
        TestEqual(FString::Printf(TEXT("Warlock %d pact slots"), L), FWarlockRules::PactSlots(L), Slots[L - 1]);
        TestEqual(FString::Printf(TEXT("Warlock %d slot level"), L), FWarlockRules::PactSlotLevel(L), SlotLevel[L - 1]);
        TestEqual(FString::Printf(TEXT("Warlock %d Magical Cunning"), L), FWarlockRules::MagicalCunningSlots(L), Cunning[L - 1]);
        TestEqual(FString::Printf(TEXT("Warlock %d Mystic Arcanum count"), L), FWarlockRules::MysticArcanumLevels(L).Num(), Arcana[L - 1]);
        FString Why;
        TestTrue(FString::Printf(TEXT("Warlock %d default invocations are valid"), L), FWarlockRules::ValidSet(FWarlockRules::DefaultSet(L), L, true, true, Why));
        TestEqual(FString::Printf(TEXT("Warlock %d default invocations fill the table"), L), FWarlockRules::DefaultSet(L).Num(), Invocations[L - 1]);
    }
    TestEqual(TEXT("Mystic Arcanum at 17 reaches 9th"), FWarlockRules::MysticArcanumLevels(17).Last(), 9);
    TestEqual(TEXT("Dark One's Blessing: CHA 3 + level 3"), FWarlockRules::DarkOnesBlessing(3, 3), 6);
    TestEqual(TEXT("Dark One's Blessing minimum 1"), FWarlockRules::DarkOnesBlessing(-5, 3), 1);
    TestEqual(TEXT("No Dark One's Blessing before 3"), FWarlockRules::DarkOnesBlessing(3, 2), 0);
    TestEqual(TEXT("Fiend spells at 3"), FWarlockRules::FiendSpells(3).Num(), 4);
    TestEqual(TEXT("Fiend spells at 9"), FWarlockRules::FiendSpells(9).Num(), 10);

    // Invocation prerequisites, table-driven: {invocation, level, held, damage cantrip, attack cantrip, expected}.
    using I = EInvocation;
    struct FCase { I Pick; int32 Level; TArray<int32> Held; bool bDamage; bool bAttack; bool bOk; };
    const TArray<FCase> Cases = {
        {I::ArmorOfShadows, 1, {}, false, false, true},
        {I::PactOfTheBlade, 1, {}, false, false, true},
        {I::AgonizingBlast, 1, {}, true, true, false},                   // level 2+
        {I::AgonizingBlast, 2, {}, true, true, true},
        {I::AgonizingBlast, 2, {}, false, true, false},                  // needs a damaging cantrip
        {I::RepellingBlast, 2, {}, true, false, false},                  // needs an attack-roll cantrip
        {I::ThirstingBlade, 5, {}, true, true, false},                   // needs Pact of the Blade
        {I::ThirstingBlade, 5, {(int32)I::PactOfTheBlade}, true, true, true},
        {I::ThirstingBlade, 4, {(int32)I::PactOfTheBlade}, true, true, false},
        {I::DevouringBlade, 12, {(int32)I::PactOfTheBlade}, true, true, false},
        {I::InvestmentOfTheChainMaster, 5, {(int32)I::PactOfTheChain}, true, true, true},
        {I::GiftOfTheProtectors, 9, {(int32)I::PactOfTheTome}, true, true, true},
        {I::GiftOfTheProtectors, 8, {(int32)I::PactOfTheTome}, true, true, false},
        {I::WhispersOfTheGrave, 7, {}, true, true, true},
        {I::WitchSight, 14, {}, true, true, false},
        {I::WitchSight, 15, {}, true, true, true},
        {I::ArmorOfShadows, 1, {(int32)I::EldritchMind}, true, true, false},  // Warlock 1 knows one
        {I::ArmorOfShadows, 2, {(int32)I::ArmorOfShadows}, true, true, false}, // no repeats
    };
    for (const FCase& Case : Cases)
    {
        FString Why;
        TestEqual(FString::Printf(TEXT("%s at Warlock %d"), FWarlockRules::Facts(Case.Pick).Name, Case.Level),
            FWarlockRules::CanTake(Case.Pick, Case.Level, Case.Held, Case.bDamage, Case.bAttack, Why), Case.bOk);
        if (!Case.bOk) { TestFalse(TEXT("refusal explains itself"), Why.IsEmpty()); }
    }
    FString Why;
    TestFalse(TEXT("Thirsting Blade without its pact is an invalid set"), FWarlockRules::ValidSet({(int32)I::ThirstingBlade}, 5, true, true, Why));
    TestTrue(TEXT("Blade + Thirsting Blade is valid"), FWarlockRules::ValidSet({(int32)I::ThirstingBlade, (int32)I::PactOfTheBlade}, 5, true, true, Why));
    TestFalse(TEXT("Too many invocations for the level"), FWarlockRules::ValidSet({(int32)I::ArmorOfShadows, (int32)I::EldritchMind}, 1, true, true, Why));
    TestFalse(TEXT("Unknown invocation value"), FWarlockRules::ValidSet({99}, 5, true, true, Why));
    for (int32 Index = 1; Index < static_cast<int32>(I::Count); ++Index)
    {
        const FInvocationFacts& F = FWarlockRules::Facts(static_cast<I>(Index));
        TestTrue(FString::Printf(TEXT("%s has SRD text"), F.Name), FCString::Strlen(F.Effect) > 0 && F.MinLevel >= 1 && F.MinLevel <= 15);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBardTableTest, "Project.Expedition.Rules.BardTable",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FBardTableTest::RunTest(const FString& Parameters)
{
    // SRD 5.2.1 Bard Features table, levels 1-20.
    static const int32 Die[] = {6, 6, 6, 6, 8, 8, 8, 8, 8, 10, 10, 10, 10, 10, 12, 12, 12, 12, 12, 12};
    static const int32 Prepared[] = {4, 5, 6, 7, 9, 10, 11, 12, 14, 15, 16, 16, 17, 17, 18, 18, 19, 20, 21, 22};
    static const int32 Expertise[] = {0, 2, 2, 2, 2, 2, 2, 2, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4};
    for (int32 L = 1; L <= 20; ++L)
    {
        const int32 Prof = 2 + (L - 1) / 4;
        TestEqual(FString::Printf(TEXT("Bard %d die"), L), FBardRules::InspirationDie(L), Die[L - 1]);
        TestEqual(FString::Printf(TEXT("Bard %d prepared"), L), FBardRules::PreparedSpells(L), Prepared[L - 1]);
        TestEqual(FString::Printf(TEXT("Bard %d expertise"), L), FBardRules::ExpertiseSkills(L), Expertise[L - 1]);
        TestEqual(FString::Printf(TEXT("Bard %d Jack of All Trades"), L), FBardRules::JackOfAllTrades(L, Prof), L >= 2 ? Prof / 2 : 0);
        TestEqual(FString::Printf(TEXT("Bard %d Font"), L), FBardRules::HasFontOfInspiration(L), L >= 5);
        TestEqual(FString::Printf(TEXT("Bard %d Magical Secrets"), L), FBardRules::MagicalSecretsAllows(L, TEXT("Wizard")), L >= 10);
    }
    TestEqual(TEXT("Inspiration uses = CHA"), FBardRules::InspirationUses(1, 3), 3);
    TestEqual(TEXT("Inspiration uses minimum 1"), FBardRules::InspirationUses(1, -1), 1);
    TestEqual(TEXT("Expertise doubles proficiency"), FBardRules::CheckProficiency(3, 2, true, true), 4);
    TestEqual(TEXT("Jack of All Trades on unproficient checks"), FBardRules::CheckProficiency(5, 3, false, false), 1);
    TestEqual(TEXT("No Jack of All Trades at Bard 1"), FBardRules::CheckProficiency(1, 2, false, false), 0);
    TestTrue(TEXT("Bard list always"), FBardRules::MagicalSecretsAllows(1, TEXT("Bard")));
    TestFalse(TEXT("Magical Secrets never adds the Sorcerer list"), FBardRules::MagicalSecretsAllows(20, TEXT("Sorcerer")));
    TestEqual(TEXT("Lore bonus skills at 3"), FBardRules::LoreBonusSkills(3), 3);
    TestEqual(TEXT("Superior Inspiration at 18"), FBardRules::SuperiorInspirationFloor(18), 2);
    return true;
}
#endif