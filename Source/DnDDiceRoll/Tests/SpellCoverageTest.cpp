#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Rules/SpellCatalogRules.h"
#include "Rules/SpellUtilityRules.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSpellCoverageTest, "Project.Expedition.Rules.SpellCoverage",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSpellCoverageTest::RunTest(const FString& Parameters)
{
    // Inventory of cantrips and level 1-4 spells: how many the engine resolves in one cast vs partial vs data only.
    int32 Total[5] = {}, Resolvable[5] = {}, Partial[5] = {}, DataOnly[5] = {}, Exceptions[5] = {};
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
        const bool bException = FSpellUtilityRules::IsException(S.Id);
        Exceptions[S.Level] += bException;
        if (bException) { TestFalse(FString::Printf(TEXT("%s exception is not resolvable"), S.Id), bResolvable); }
        if (!bResolvable && !bException) { Gaps.Add(FString::Printf(TEXT("L%d %s (%s)"), S.Level, S.Id, S.Implementation == ESpellImplementation::DataOnly ? TEXT("data") : S.Implementation == ESpellImplementation::Partial ? TEXT("partial") : TEXT("mechanics, not one-cast"))); }
    }
    for (int32 L = 0; L <= 4; ++L) { AddInfo(FString::Printf(TEXT("COVERAGE L%d total=%d resolvable=%d exceptions=%d partial=%d dataonly=%d"), L, Total[L], Resolvable[L], Exceptions[L], Partial[L], DataOnly[L])); }
    AddInfo(TEXT("COVERAGEGAPS ") + FString::Join(Gaps, TEXT("; ")));
    // Every utility row names a real catalog spell; every exception documents its reason.
    for (int32 I = 0; I < FSpellUtilityRules::Num(); ++I)
    {
        const FSpellUtility& U = FSpellUtilityRules::At(I);
        TestNotNull(FString::Printf(TEXT("utility row %s is a catalog spell"), U.Id), FSpellCatalogRules::Find(U.Id));
        TestTrue(FString::Printf(TEXT("utility row %s has a note"), U.Id), U.Note && FCString::Strlen(U.Note) > 0);
    }
    TestTrue(TEXT("Catalog has cantrips and level 1-4 spells"), Total[0] > 0 && Total[4] > 0);
    // Floors after spells phase 5 item 1 (reactions, engine zones): 104 of 217 resolvable, 75 documented exceptions.
    const int32 Floor[5] = {11, 36, 25, 16, 16};
    for (int32 L = 0; L <= 4; ++L) { TestTrue(FString::Printf(TEXT("L%d resolvable >= %d"), L, Floor[L]), Resolvable[L] >= Floor[L]); }
    return !HasAnyErrors();
}
#endif