#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Rules/SpellCatalogRules.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSpellCoverageTest, "Project.Expedition.Rules.SpellCoverage",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSpellCoverageTest::RunTest(const FString& Parameters)
{
    // Inventory of cantrips and level 1-4 spells: how many the engine resolves in one cast vs partial vs data only.
    int32 Total[5] = {}, Resolvable[5] = {}, Partial[5] = {}, DataOnly[5] = {};
    TArray<FString> Gaps;
    for (int32 I = 0; I < FSpellCatalogRules::Num(); ++I)
    {
        const FSrdSpell& S = FSpellCatalogRules::At(I);
        if (S.Level > 4) { continue; }
        ++Total[S.Level];
        const bool bResolvable = FSpellCatalogRules::CombatResolvable(S);
        Resolvable[S.Level] += bResolvable;
        Partial[S.Level] += S.Implementation == ESpellImplementation::Partial;
        DataOnly[S.Level] += S.Implementation == ESpellImplementation::DataOnly;
        if (!bResolvable) { Gaps.Add(FString::Printf(TEXT("L%d %s (%s)"), S.Level, S.Id, S.Implementation == ESpellImplementation::DataOnly ? TEXT("data") : S.Implementation == ESpellImplementation::Partial ? TEXT("partial") : TEXT("mechanics, not one-cast"))); }
    }
    for (int32 L = 0; L <= 4; ++L) { AddInfo(FString::Printf(TEXT("COVERAGE L%d total=%d resolvable=%d partial=%d dataonly=%d"), L, Total[L], Resolvable[L], Partial[L], DataOnly[L])); }
    AddInfo(TEXT("COVERAGEGAPS ") + FString::Join(Gaps, TEXT("; ")));
    TestTrue(TEXT("Catalog has cantrips and level 1-4 spells"), Total[0] > 0 && Total[4] > 0);
    return true;
}
#endif