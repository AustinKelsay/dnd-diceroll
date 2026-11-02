#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Combat/CombatBattle.h"
#include "Rules/SpellCatalogRules.h"
#include "Rules/SpellEffectRules.h"
#include "Party/BuildCatalog.h"

// Spells phases 3-4 (SRD 5.2.1): conditions, creature types and immunities, buffs and debuffs.
namespace SpellEffectsTest
{
FCombatEncounter EffectsCaster(EAdventurerRole Role, int32 Level, const TCHAR* FoeRole)
{
    FCombatEncounter Encounter;
    Encounter.Width = Encounter.Height = 12;
    FCombatantState Me;
    if (static_cast<uint8>(Role) >= static_cast<uint8>(EAdventurerRole::Monk))
    {
        for (const FBuildDefinition& Build : FBuildCatalog::All()) { if (Build.Profile.AdventurerRole == Role && FBuildCatalog::Create(Build.Id, 10, TEXT("Caster"), Me)) { break; } }
    }
    Me.Id = 10; Me.Name = TEXT("Caster"); Me.Role = TEXT("Raider"); Me.AdventurerRole = Role; Me.Level = Level; Me.Cell = {0, 0}; Me.InitiativeBonus = 30;
    Me.MaxHP = Me.HP = 300; Me.SlotsByLevel.SetNum(FSpellRules::MaxSpellLevel);
    for (int32 L = 1; L <= FSpellRules::MaxSpellLevel; ++L) { Me.SlotsByLevel[L - 1] = FMath::Min(2, FBuildCatalog::SlotMax(Me, L)); }
    if (FBuildCatalog::Find(Me.BuildId)) { Me.SlotsByLevel[0] = FMath::Min(Me.SlotsByLevel[0], FBuildCatalog::SlotCap(Me)); }
    Me.SpellSlots = Me.SlotsByLevel[0];
    Me.KitUses = FMath::Min(Me.KitUses, Me.SpellSlots);
    Encounter.Combatants.Add(Me);
    FCombatantState Foe;
    Foe.Id = 20; Foe.Name = TEXT("Foe"); Foe.Role = FoeRole; Foe.Side = ECombatSide::Enemy; Foe.Cell = {2, 0}; Foe.InitiativeBonus = -10; Foe.MaxHP = Foe.HP = 500;
    Encounter.Combatants.Add(Foe);
    return Encounter;
}
FCombatCommand EffectsCast(const TCHAR* Id, int32 Target, int32 Slot = 0, int32 Choice = 0)
{
    FCombatCommand C{ECombatCommandType::CastSpell, 10, {}, Target}; C.SpellId = Id; C.SlotLevel = Slot; C.Choice = Choice; return C;
}
bool Has(const FCombatantState* U, ECombatCondition K) { return U && U->Conditions.ContainsByPredicate([K](const FCombatCondition& C) { return C.Condition == K; }); }
}
using namespace SpellEffectsTest;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSpellConditionTypesTest, "Project.Expedition.Spells.Effects.ConditionsAndCreatureTypes",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSpellConditionTypesTest::RunTest(const FString& Parameters)
{
    struct FTypeRow { const TCHAR* Role; ECreatureType Type; bool bNoExhaustion; };
    const FTypeRow Types[] = { {TEXT("Wolf"), ECreatureType::Beast, false}, {TEXT("Skeleton"), ECreatureType::Undead, true}, {TEXT("Imp"), ECreatureType::Fiend, false}, {TEXT("Raider"), ECreatureType::Humanoid, false} };
    for (const FTypeRow& Row : Types)
    {
        TestEqual(FString::Printf(TEXT("%s type"), Row.Role), FCreatureRules::TypeOf(Row.Role), Row.Type);
        TestEqual(FString::Printf(TEXT("%s exhaustion immunity"), Row.Role), FCreatureRules::ImmuneToExhaustion(Row.Role), Row.bNoExhaustion);
    }
    // Creature-type gates on targeting.
    struct FGateRow { EAdventurerRole Role; const TCHAR* Spell; const TCHAR* Foe; bool bTargetable; };
    const FGateRow Gates[] = {
        {EAdventurerRole::Wizard, TEXT("hold-person"), TEXT("Raider"), true}, {EAdventurerRole::Wizard, TEXT("hold-person"), TEXT("Wolf"), false},
        {EAdventurerRole::Druid, TEXT("animal-friendship"), TEXT("Wolf"), true}, {EAdventurerRole::Druid, TEXT("animal-friendship"), TEXT("Raider"), false},
        {EAdventurerRole::Wizard, TEXT("charm-person"), TEXT("Imp"), false} };
    for (const FGateRow& Row : Gates)
    {
        FString Error; FCombatBattle Battle;
        if (!TestTrue(TEXT("Start: ") + Error, Battle.Start(EffectsCaster(Row.Role, 5, Row.Foe), 3, Error))) { return false; }
        TestEqual(FString::Printf(TEXT("%s on %s"), Row.Spell, Row.Foe), Battle.SpellTargets(10, Row.Spell).Contains(20), Row.bTargetable);
    }
    // A failed save applies the condition; the right conditions land per spell.
    struct FCondRow { EAdventurerRole Role; const TCHAR* Spell; ECombatCondition Kind; };
    const FCondRow Conds[] = { {EAdventurerRole::Wizard, TEXT("hold-person"), ECombatCondition::Paralyzed}, {EAdventurerRole::Wizard, TEXT("charm-person"), ECombatCondition::Charmed},
        {EAdventurerRole::Cleric, TEXT("command"), ECombatCondition::Prone}, {EAdventurerRole::Bard, TEXT("hideous-laughter"), ECombatCondition::Prone} };
    for (const FCondRow& Row : Conds)
    {
        bool bLanded = false, bSaved = false;
        for (int32 Seed = 1; Seed < 120 && !(bLanded && bSaved); ++Seed)
        {
            FString Error; FCombatBattle Battle;
            Battle.Start(EffectsCaster(Row.Role, 5, TEXT("Raider")), Seed, Error);
            const FCombatResult R = Battle.Submit(EffectsCast(Row.Spell, 20));
            if (!TestTrue(FString::Printf(TEXT("%s resolves: %s"), Row.Spell, *R.Feedback), R.bAccepted)) { return false; }
            (SpellEffectsTest::Has(Battle.Find(20), Row.Kind) ? bLanded : bSaved) = true;
        }
        TestTrue(FString::Printf(TEXT("%s lands on a failed save"), Row.Spell), bLanded);
        TestTrue(FString::Printf(TEXT("%s can be saved against"), Row.Spell), bSaved);
    }
    // Paralyzed ends the target's concentration-free turn options; Sleep never affects exhaustion-immune undead.
    for (int32 Seed = 1; Seed < 30; ++Seed)
    {
        FString Error; FCombatBattle Battle;
        Battle.Start(EffectsCaster(EAdventurerRole::Wizard, 5, TEXT("Skeleton")), Seed, Error);
        Battle.Submit(EffectsCast(TEXT("sleep"), 20));
        TestFalse(TEXT("Sleep never incapacitates a skeleton"), SpellEffectsTest::Has(Battle.Find(20), ECombatCondition::Incapacitated));
    }
    // A charmed creature can't attack its charmer.
    for (int32 Seed = 1; Seed < 120; ++Seed)
    {
        FString Error; FCombatBattle Battle;
        Battle.Start(EffectsCaster(EAdventurerRole::Wizard, 5, TEXT("Raider")), Seed, Error);
        Battle.Submit(EffectsCast(TEXT("charm-person"), 20));
        if (!SpellEffectsTest::Has(Battle.Find(20), ECombatCondition::Charmed)) { continue; }
        TestFalse(TEXT("Charmed foe can't target the charmer"), Battle.LegalTargets(20, ECombatCommandType::Attack).Contains(10));
        break;
    }
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSpellBuffsTest, "Project.Expedition.Spells.Effects.BuffsAndDebuffs",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSpellBuffsTest::RunTest(const FString& Parameters)
{
    enum class EProbe : uint8 { AcDelta, AcAtLeast, Speed, MaxHpDelta, TempHpAtLeast, Images, Invisible, Resists };
    struct FRow { EAdventurerRole Role; const TCHAR* Spell; int32 Target; EProbe Probe; int32 Expected; int32 Choice; };
    const FRow Rows[] = {
        {EAdventurerRole::Cleric, TEXT("shield-of-faith"), 10, EProbe::AcDelta, 2, 0},
        {EAdventurerRole::Wizard, TEXT("haste"), 10, EProbe::AcDelta, 2, 0},
        {EAdventurerRole::Wizard, TEXT("mage-armor"), 10, EProbe::AcAtLeast, 13, 0},
        {EAdventurerRole::Druid, TEXT("barkskin"), 10, EProbe::AcAtLeast, 17, 0},
        {EAdventurerRole::Druid, TEXT("longstrider"), 10, EProbe::Speed, 8, 0},
        {EAdventurerRole::Wizard, TEXT("expeditious-retreat"), 10, EProbe::Speed, 12, 0},
        {EAdventurerRole::Cleric, TEXT("aid"), 10, EProbe::MaxHpDelta, 5, 0},
        {EAdventurerRole::Wizard, TEXT("false-life"), 10, EProbe::TempHpAtLeast, 6, 0},
        {EAdventurerRole::Wizard, TEXT("mirror-image"), 10, EProbe::Images, 3, 0},
        {EAdventurerRole::Wizard, TEXT("invisibility"), 10, EProbe::Invisible, 1, 0},
        {EAdventurerRole::Wizard, TEXT("protection-from-energy"), 10, EProbe::Resists, 1, 2},
    };
    for (const FRow& Row : Rows)
    {
        FString Error; FCombatBattle Battle;
        if (!TestTrue(TEXT("Start: ") + Error, Battle.Start(EffectsCaster(Row.Role, 5, TEXT("Raider")), 3, Error))) { return false; }
        const int32 Ac = Battle.EffectiveArmorClass(Row.Target), Max = Battle.Find(Row.Target)->MaxHP;
        const FCombatResult R = Battle.Submit(EffectsCast(Row.Spell, Row.Target, 0, Row.Choice));
        if (!TestTrue(FString::Printf(TEXT("%s resolves: %s"), Row.Spell, *R.Feedback), R.bAccepted)) { continue; }
        const FCombatantState* U = Battle.Find(Row.Target);
        int32 Got = 0;
        switch (Row.Probe)
        {
        case EProbe::AcDelta: Got = Battle.EffectiveArmorClass(Row.Target) - Ac; break;
        case EProbe::AcAtLeast: Got = Battle.EffectiveArmorClass(Row.Target) >= Row.Expected ? Row.Expected : Battle.EffectiveArmorClass(Row.Target); break;
        case EProbe::Speed: Got = Battle.SpellSpeed(Row.Target, 6); break;
        case EProbe::MaxHpDelta: Got = U->MaxHP - Max; break;
        case EProbe::TempHpAtLeast: Got = U->TemporaryHP >= Row.Expected ? Row.Expected : U->TemporaryHP; break;
        case EProbe::Images: Got = Battle.MirrorImagesLeft(Row.Target); break;
        case EProbe::Invisible: Got = SpellEffectsTest::Has(U, ECombatCondition::Invisible) ? 1 : 0; break;
        case EProbe::Resists: Got = Battle.SpellResistances(Row.Target) != 0 ? 1 : 0; break;
        }
        TestEqual(FString::Printf(TEXT("%s effect"), Row.Spell), Got, Row.Expected);
        TestNotNull(FString::Printf(TEXT("%s tracked on target"), Row.Spell), Battle.SpellEffectOn(Row.Target, Row.Spell));
    }
    // Debuffs on a foe: Slow halves speed and lowers AC on a failed save.
    bool bSlowed = false;
    for (int32 Seed = 1; Seed < 120 && !bSlowed; ++Seed)
    {
        FString Error; FCombatBattle Battle;
        Battle.Start(EffectsCaster(EAdventurerRole::Wizard, 5, TEXT("Raider")), Seed, Error);
        const int32 Ac = Battle.EffectiveArmorClass(20);
        Battle.Submit(EffectsCast(TEXT("slow"), 20));
        if (!Battle.SpellEffectOn(20, TEXT("slow"))) { continue; }
        bSlowed = true;
        TestEqual(TEXT("Slow AC -2"), Battle.EffectiveArmorClass(20), Ac - 2);
        TestEqual(TEXT("Slow halves speed"), Battle.SpellSpeed(20, 6), 3);
    }
    TestTrue(TEXT("Saw Slow land"), bSlowed);
    return !HasAnyErrors();
}
#endif