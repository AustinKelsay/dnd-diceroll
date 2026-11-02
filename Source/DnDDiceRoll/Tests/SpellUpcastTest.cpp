#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Combat/CombatBattle.h"
#include "Rules/SpellCatalogRules.h"
#include "Rules/SpellEffectRules.h"
#include "Rules/SpellUtilityRules.h"
#include "Party/BuildCatalog.h"

namespace SpellUpcastTestLocal
{
/** Level-5 caster (the engine cap) with up to two slots of each level it has. */
FCombatEncounter UpcastHero(EAdventurerRole Role, int32 Level, FIntPoint FoeCell)
{
    FCombatEncounter Encounter;
    Encounter.Width = Encounter.Height = 12;
    FCombatantState Me;
    Me.Id = 10; Me.Name = TEXT("Archmage"); Me.Role = TEXT("Raider"); Me.AdventurerRole = Role; Me.Level = Level; Me.Cell = {0, 0}; Me.InitiativeBonus = 30;
    Me.MaxHP = Me.HP = 300; Me.SlotsByLevel.SetNum(FSpellRules::MaxSpellLevel);
    for (int32 L = 1; L <= FSpellRules::MaxSpellLevel; ++L) { Me.SlotsByLevel[L - 1] = FMath::Min(2, FBuildCatalog::SlotMax(Me, L)); }
    Me.SpellSlots = Me.SlotsByLevel[0];
    Encounter.Combatants.Add(Me);
    FCombatantState Foe;
    Foe.Id = 20; Foe.Name = TEXT("Foe"); Foe.Role = TEXT("Raider"); Foe.Side = ECombatSide::Enemy; Foe.Cell = FoeCell; Foe.InitiativeBonus = -10; Foe.MaxHP = Foe.HP = 500;
    Encounter.Combatants.Add(Foe);
    return Encounter;
}
FCombatCommand UpcastCast(const TCHAR* Id, int32 Target, int32 Slot)
{
    FCombatCommand C{ECombatCommandType::CastSpell, 10, {}, Target}; C.SpellId = Id; C.SlotLevel = Slot; return C;
}
int32 CountSides(const TArray<FDieRoll>& Dice, int32 Sides) { int32 N = 0; for (const FDieRoll& D : Dice) { N += D.Sides == Sides ? 1 : 0; } return N; }
}
using namespace SpellUpcastTestLocal;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSpellUpcastRulesTest, "Project.Expedition.Spells.UpcastRules", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSpellUpcastRulesTest::RunTest(const FString& Parameters)
{
    int32 Scaling = 0;
    for (int32 I = 0; I < FSpellCatalogRules::Num(); ++I)
    {
        const FSrdSpell& S = FSpellCatalogRules::At(I);
        if (S.Level < 1 || S.Level > 4) { continue; }
        const int32 Base = FSpellCatalogRules::DiceCount(S, 7, S.Level);
        TestEqual(FString::Printf(TEXT("%s: no dice below its level"), S.Id), S.Level > 1 ? FSpellCatalogRules::DiceCount(S, 7, S.Level - 1) : 0, 0);
        for (int32 Slot = S.Level + 1; Slot <= 9; ++Slot)
        {
            TestTrue(FString::Printf(TEXT("%s: dice never drop at slot %d"), S.Id, Slot), FSpellCatalogRules::DiceCount(S, 7, Slot) >= FSpellCatalogRules::DiceCount(S, 7, Slot - 1));
            TestTrue(FString::Printf(TEXT("%s: second dice never drop at slot %d"), S.Id, Slot), S.Dice2[Slot] >= S.Dice2[Slot - 1]);
        }
        if (S.Level < 4 && Base > 0 && S.Dice[4] > S.Dice[S.Level])
        {
            ++Scaling;
            TestTrue(FString::Printf(TEXT("%s: a 4th-level slot does more on average"), S.Id), FSpellCatalogRules::AverageEffect(S, 7, 4, 3) > FSpellCatalogRules::AverageEffect(S, 7, S.Level, 3));
        }
        if (const FSpellEffectSpec* Spec = FSpellEffectRules::Find(S.Id))
        {
            if (Spec->TargetsPerSlot > 0 && S.Level < 4)
            {
                TestEqual(FString::Printf(TEXT("%s: targets at slot 4"), S.Id), FSpellEffectRules::MaxTargets(*Spec, S.Level, 4), FSpellEffectRules::MaxTargets(*Spec, S.Level, S.Level) + Spec->TargetsPerSlot * (4 - S.Level));
                TestTrue(FString::Printf(TEXT("%s: more targets upcast"), S.Id), FSpellEffectRules::MaxTargets(*Spec, S.Level, 4) > FSpellEffectRules::MaxTargets(*Spec, S.Level, S.Level));
            }
        }
    }
    TestTrue(TEXT("Many damage/heal spells scale"), Scaling >= 20);
    // SRD 5.2.1 spot checks with a 4th-level slot.
    struct FCheck { const TCHAR* Id; int32 Dice; };
    const FCheck Checks[] = {{TEXT("fireball"), 9}, {TEXT("lightning-bolt"), 9}, {TEXT("burning-hands"), 6}, {TEXT("thunderwave"), 5}, {TEXT("guiding-bolt"), 7}, {TEXT("shatter"), 5}, {TEXT("inflict-wounds"), 5}, {TEXT("cure-wounds"), 8}, {TEXT("healing-word"), 8}};
    for (const FCheck& C : Checks)
    {
        const FSrdSpell* S = FSpellCatalogRules::Find(C.Id);
        if (TestNotNull(C.Id, S)) { TestEqual(FString::Printf(TEXT("%s dice at slot 4"), C.Id), FSpellCatalogRules::DiceCount(*S, 7, 4), C.Dice); }
    }
    const FSrdSpell* Mm = FSpellCatalogRules::Find(TEXT("magic-missile"));
    const FSrdSpell* Sr = FSpellCatalogRules::Find(TEXT("scorching-ray"));
    if (TestNotNull(TEXT("magic missile"), Mm)) { TestEqual(TEXT("Magic Missile 6 darts at slot 4"), FSpellUtilityRules::Beams(*Mm, 7, 4), 6); TestEqual(TEXT("3 darts at slot 1"), FSpellUtilityRules::Beams(*Mm, 7, 1), 3); }
    if (TestNotNull(TEXT("scorching ray"), Sr)) { TestEqual(TEXT("Scorching Ray 5 rays at slot 4"), FSpellUtilityRules::Beams(*Sr, 7, 4), 5); }
    // Per-slot riders.
    struct FSpecCheck { const TCHAR* Id; int32 FSpellEffectSpec::* Field; };
    const FSpecCheck Riders[] = {{TEXT("aid"), &FSpellEffectSpec::MaxHpPerSlot}, {TEXT("false-life"), &FSpellEffectSpec::TempPerSlot}, {TEXT("searing-smite"), &FSpellEffectSpec::HitDicePerSlot}};
    for (const FSpecCheck& R : Riders)
    {
        if (const FSpellEffectSpec* Spec = FSpellEffectRules::Find(R.Id)) { TestTrue(FString::Printf(TEXT("%s scales per slot"), R.Id), Spec->*R.Field > 0); }
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSpellUpcastBattleTest, "Project.Expedition.Spells.UpcastBattle", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSpellUpcastBattleTest::RunTest(const FString& Parameters)
{
    // The engine caps heroes at level 5 (3rd-level slots) and rejects a 4th-level slot at Start, so the battle checks use
    // 3rd-level slots; 4th-level scaling is covered by UpcastRules through the same DiceCount/Beams/MaxTargets functions.
    {
        FString Error; FCombatBattle Battle;
        if (!TestTrue(TEXT("Start wizard: ") + Error, Battle.Start(UpcastHero(EAdventurerRole::Wizard, 5, {4, 0}), 3, Error))) { return false; }
        const FCombatResult R = Battle.Submit(UpcastCast(TEXT("magic-missile"), 20, 3));
        TestTrue(TEXT("Magic Missile at slot 3: ") + R.Feedback, R.bAccepted);
        TestEqual(TEXT("5 darts of 1d4"), CountSides(R.EffectDice, 4), 5);
        TestEqual(TEXT("3rd-level slot spent"), Battle.Find(10)->SlotsByLevel[2], FMath::Min(2, FBuildCatalog::SlotMax(*Battle.Find(10), 3)) - 1);
    }
    {
        FString Error; FCombatBattle Battle;
        Battle.Start(UpcastHero(EAdventurerRole::Wizard, 5, {1, 0}), 3, Error);
        const FCombatResult R = Battle.Submit(UpcastCast(TEXT("burning-hands"), 20, 3));
        TestTrue(TEXT("Burning Hands at slot 3: ") + R.Feedback, R.bAccepted);
        TestEqual(TEXT("Burning Hands at slot 3 rolls 5d6"), CountSides(R.EffectDice, 6), 5);
    }
    {
        FString Error; FCombatBattle Battle;
        Battle.Start(UpcastHero(EAdventurerRole::Cleric, 5, {1, 0}), 3, Error);
        const FCombatResult R = Battle.Submit(UpcastCast(TEXT("inflict-wounds"), 20, 3));
        TestTrue(TEXT("Inflict Wounds at slot 3: ") + R.Feedback, R.bAccepted);
        const int32 D10 = CountSides(R.EffectDice, 10);
        TestTrue(TEXT("Inflict Wounds at slot 3 rolls 4d10 on a hit (8 on a crit)"), D10 == 0 || D10 == 4 || D10 == 8);
    }
    return true;
}
#endif
