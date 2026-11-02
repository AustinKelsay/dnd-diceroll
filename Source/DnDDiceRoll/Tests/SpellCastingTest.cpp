#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Combat/CombatBattle.h"
#include "Rules/SpellCatalogRules.h"
#include "Party/BuildCatalog.h"

namespace SpellCastingTest
{
FCombatEncounter Caster(EAdventurerRole Role, int32 Level, FIntPoint FoeCell)
{
    FCombatEncounter Encounter;
    Encounter.Width = Encounter.Height = 12;
    FCombatantState Me;
    if (static_cast<uint8>(Role) >= static_cast<uint8>(EAdventurerRole::Monk))
    {
        // Newer classes start from their catalog build.
        for (const FBuildDefinition& Build : FBuildCatalog::All())
        {
            if (Build.Profile.AdventurerRole == Role && FBuildCatalog::Create(Build.Id, 10, TEXT("Caster"), Me)) { break; }
        }
    }
    Me.Id = 10; Me.Name = TEXT("Caster"); Me.Role = TEXT("Raider"); Me.AdventurerRole = Role; Me.Level = Level; Me.Cell = {0, 0}; Me.InitiativeBonus = 30;
    Me.MaxHP = Me.HP = 300; Me.SlotsByLevel.SetNum(FSpellRules::MaxSpellLevel);
    for (int32 L = 1; L <= FSpellRules::MaxSpellLevel; ++L) { Me.SlotsByLevel[L - 1] = FMath::Min(2, FBuildCatalog::SlotMax(Me, L)); }
    if (FBuildCatalog::Find(Me.BuildId)) { Me.SlotsByLevel[0] = FMath::Min(Me.SlotsByLevel[0], FBuildCatalog::SlotCap(Me)); }
    Me.SpellSlots = Me.SlotsByLevel[0];
    Me.KitUses = FMath::Min(Me.KitUses, Me.SpellSlots);
    Encounter.Combatants.Add(Me);
    FCombatantState Foe;
    Foe.Id = 20; Foe.Name = TEXT("Foe"); Foe.Role = TEXT("Raider"); Foe.Side = ECombatSide::Enemy; Foe.Cell = FoeCell; Foe.InitiativeBonus = -10; Foe.MaxHP = Foe.HP = 500;
    Encounter.Combatants.Add(Foe);
    return Encounter;
}
FCombatCommand Cast(const TCHAR* Id, int32 Target, int32 Slot = 0)
{
    FCombatCommand C{ECombatCommandType::CastSpell, 10, {}, Target}; C.SpellId = Id; C.SlotLevel = Slot; return C;
}
int32 Sum(const TArray<FDieRoll>& Dice) { int32 S = 0; for (const FDieRoll& D : Dice) { S += D.Value; } return S; }
}
using namespace SpellCastingTest;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSpellCastingTableTest, "Project.Expedition.Spells.EveryCombatSpellCasts",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSpellCastingTableTest::RunTest(const FString& Parameters)
{
    const EAdventurerRole Roles[] = {EAdventurerRole::Bard, EAdventurerRole::Cleric, EAdventurerRole::Druid, EAdventurerRole::Paladin,
        EAdventurerRole::Ranger, EAdventurerRole::Sorcerer, EAdventurerRole::Warlock, EAdventurerRole::Wizard};
    int32 ByLevel[10] = {};
    FString Names[10];
    for (int32 Index = 0; Index < FSpellCatalogRules::Num(); ++Index)
    {
        const FSrdSpell& Spell = FSpellCatalogRules::At(Index);
        if (!FSpellCatalogRules::CombatResolvable(Spell)) { continue; }
        bool bCastBySomeone = false;
        for (const EAdventurerRole Role : Roles)
        {
            if (!FSpellCatalogRules::CanLearn(Role, 5, Spell)) { continue; }
            FString Error; FCombatBattle Battle;
            if (!Battle.Start(Caster(Role, 5, {1, 0}), 3, Error)) { continue; } // kit-only classes need a kit build to start
            TestTrue(FString::Printf(TEXT("%s listed castable"), Spell.Id), Battle.CastableSpells(10).Contains(Spell.Id));
            const int32 Target = Spell.Effect == ESpellEffect::Healing ? 10 : 20;
            TestTrue(FString::Printf(TEXT("%s has a target"), Spell.Id), Battle.SpellTargets(10, Spell.Id).Contains(Target));
            const FCombatResult R = Battle.Submit(Cast(Spell.Id, Target));
            TestTrue(FString::Printf(TEXT("%s resolves for a level 5 caster: %s"), Spell.Id, *R.Feedback), R.bAccepted);
            if (Spell.Level > 0 && R.bAccepted)
            {
                int32 Left = 0, Start = 0; for (int32 L = 0; L < FSpellRules::MaxSpellLevel; ++L) { Left += Battle.Find(10)->SlotsByLevel[L]; Start += Caster(Role, 5, {1, 0}).Combatants[0].SlotsByLevel[L]; }
                TestEqual(FString::Printf(TEXT("%s spends one slot"), Spell.Id), Left, Start - 1);
            }
            TestFalse(FString::Printf(TEXT("%s cannot be cast again with the same action"), Spell.Id), Battle.Submit(Cast(Spell.Id, Target)).bAccepted);
            bCastBySomeone = true;
            break;
        }
        if (Spell.Level <= 3) { TestTrue(FString::Printf(TEXT("%s is castable by some level 5 class"), Spell.Id), bCastBySomeone); }
        if (bCastBySomeone) { ++ByLevel[Spell.Level]; Names[Spell.Level] += FString(Spell.Name) + TEXT(", "); }
    }
    AddInfo(FString::Printf(TEXT("Combat-castable spells by level: 0=%d 1=%d 2=%d 3=%d 4=%d 5=%d 6=%d 7=%d 8=%d 9=%d"),
        ByLevel[0], ByLevel[1], ByLevel[2], ByLevel[3], ByLevel[4], ByLevel[5], ByLevel[6], ByLevel[7], ByLevel[8], ByLevel[9]));
    for (int32 L = 0; L <= 3; ++L) { AddInfo(FString::Printf(TEXT("Castable level %d: %s"), L, *Names[L])); }
    TestTrue(TEXT("Cantrips wired"), ByLevel[0] >= 8);
    // Class tags gate casting: a wizard cannot cast Sacred Flame, a fighter casts nothing, a level 1 wizard cannot cast Fireball.
    FString Error; FCombatBattle Battle;
    Battle.Start(Caster(EAdventurerRole::Wizard, 1, {1, 0}), 3, Error);
    TestFalse(TEXT("Sacred Flame is not on the wizard list"), Battle.Submit(Cast(TEXT("sacred-flame"), 20)).bAccepted);
    TestFalse(TEXT("Fireball needs wizard level 5"), Battle.Submit(Cast(TEXT("fireball"), 20)).bAccepted);
    TestFalse(TEXT("A fighter learns no Fire Bolt"), FSpellCatalogRules::CanLearn(EAdventurerRole::Fighter, 5, *FSpellCatalogRules::Find(TEXT("fire-bolt"))));
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSpellFireBoltTest, "Project.Expedition.Spells.FireBoltEndToEnd",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSpellFireBoltTest::RunTest(const FString& Parameters)
{
    bool bHit = false, bMiss = false, bCrit = false;
    for (int32 Seed = 1; Seed < 400 && !(bHit && bMiss && bCrit); ++Seed)
    {
        FString Error; FCombatBattle Battle;
        if (!Battle.Start(Caster(EAdventurerRole::Wizard, 5, {4, 0}), Seed, Error)) { AddError(Error); return false; }
        TestTrue(TEXT("Fire Bolt reaches 4 cells"), Battle.SpellTargets(10, TEXT("fire-bolt")).Contains(20));
        const FCombatResult R = Battle.Submit(Cast(TEXT("fire-bolt"), 20));
        if (!TestTrue(TEXT("Fire Bolt resolves"), R.bAccepted)) { return false; }
        TestEqual(TEXT("Fire"), R.DamageType, EDamageType::Fire);
        TestEqual(TEXT("Cantrip spends no slot"), Battle.Find(10)->SpellSlots, 2);
        TestFalse(TEXT("Costs the action"), Battle.Snapshot().bActionAvailable);
        TestEqual(TEXT("One attack roll"), R.AttackRolls.Num(), 1);
        if (R.bCritical) { bCrit = true; TestEqual(TEXT("Crit doubles 2d10 to 4d10"), R.EffectDice.Num(), 4); TestEqual(TEXT("Crit damage"), 500 - Battle.Find(20)->HP, Sum(R.EffectDice)); }
        else if (R.bHit) { bHit = true; TestEqual(TEXT("Level 5 tier: 2d10"), R.EffectDice.Num(), 2); TestEqual(TEXT("Hit damage"), 500 - Battle.Find(20)->HP, Sum(R.EffectDice)); }
        else { bMiss = true; TestEqual(TEXT("A miss deals nothing"), Battle.Find(20)->HP, 500); }
    }
    TestTrue(TEXT("Saw a hit"), bHit); TestTrue(TEXT("Saw a miss"), bMiss); TestTrue(TEXT("Saw a critical"), bCrit);
    FString Error; FCombatBattle Low;
    Low.Start(Caster(EAdventurerRole::Wizard, 1, {4, 0}), 5, Error);
    const FCombatResult R = Low.Submit(Cast(TEXT("fire-bolt"), 20));
    if (R.bHit && !R.bCritical) { TestEqual(TEXT("Level 1: 1d10"), R.EffectDice.Num(), 1); }
    FCombatBattle Far;
    Far.Start(Caster(EAdventurerRole::Wizard, 5, {11, 11}), 5, Error);
    TestTrue(TEXT("Within 120 ft"), Far.SpellTargets(10, TEXT("fire-bolt")).Contains(20));
    return !HasAnyErrors();
}

#endif
