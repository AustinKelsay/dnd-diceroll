#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Combat/CombatBattle.h"
#include "Rules/SpellCatalogRules.h"
#include "Rules/SpellUtilityRules.h"
#include "Party/BuildCatalog.h"

// Spells phase 5 (SRD 5.2.1): reaction spells fire automatically, never as an action.
namespace SpellReactionTest
{
FCombatEncounter ReactionDuel(EAdventurerRole Role, int32 Level)
{
    FCombatEncounter Encounter;
    Encounter.Width = Encounter.Height = 10;
    FCombatantState Me;
    for (const FBuildDefinition& Build : FBuildCatalog::All()) { if (Build.Profile.AdventurerRole == Role && FBuildCatalog::Create(Build.Id, 10, TEXT("Hero"), Me)) { break; } }
    Me.Id = 10; Me.Name = TEXT("Hero"); Me.AdventurerRole = Role; Me.Level = Level; Me.Cell = {0, 0}; Me.InitiativeBonus = 30;
    Me.MaxHP = Me.HP = 300; Me.PreparedSpells.Reset();
    Me.SlotsByLevel.SetNum(FSpellRules::MaxSpellLevel);
    for (int32 L = 1; L <= FSpellRules::MaxSpellLevel; ++L) { Me.SlotsByLevel[L - 1] = FMath::Min(2, FBuildCatalog::SlotMax(Me, L)); }
    if (FBuildCatalog::Find(Me.BuildId)) { Me.SlotsByLevel[0] = FMath::Min(Me.SlotsByLevel[0], FBuildCatalog::SlotCap(Me)); }
    Me.SpellSlots = Me.SlotsByLevel[0]; Me.KitUses = FMath::Min(Me.KitUses, Me.SpellSlots);
    Encounter.Combatants.Add(Me);
    FCombatantState Foe;
    Foe.Id = 20; Foe.Name = TEXT("Raider"); Foe.Role = TEXT("Raider"); Foe.Side = ECombatSide::Enemy; Foe.Cell = {1, 0}; Foe.InitiativeBonus = -10; Foe.MaxHP = Foe.HP = 500;
    Foe.AttackBonus = 30; // always hits
    Encounter.Combatants.Add(Foe);
    return Encounter;
}
}
using namespace SpellReactionTest;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSpellReactionTest, "Project.Expedition.Spells.Phase5.Reactions",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSpellReactionTest::RunTest(const FString& Parameters)
{
    // Reaction spells are never offered as actions.
    {
        FString Error; FCombatBattle Battle;
        if (!TestTrue(TEXT("Start: ") + Error, Battle.Start(ReactionDuel(EAdventurerRole::Warlock, 5), 7, Error))) { return false; }
        FCombatCommand C{ECombatCommandType::CastSpell, 10, {}, 20}; C.SpellId = TEXT("hellish-rebuke");
        TestFalse(TEXT("hellish-rebuke is not an action"), Battle.Submit(C).bAccepted);
        TestTrue(TEXT("hellish-rebuke counts as resolvable"), FSpellCatalogRules::CombatResolvable(*FSpellCatalogRules::Find(TEXT("hellish-rebuke"))));
        TestTrue(TEXT("shield counts as resolvable"), FSpellCatalogRules::CombatResolvable(*FSpellCatalogRules::Find(TEXT("shield"))));
    }
    // Hellish Rebuke answers a damaging hit: reaction and a slot spent, the attacker takes fire.
    bool bFired = false;
    for (int32 Seed = 1; Seed < 60 && !bFired; ++Seed)
    {
        FString Error; FCombatBattle Battle;
        if (!Battle.Start(ReactionDuel(EAdventurerRole::Warlock, 5), Seed, Error)) { AddError(Error); return false; }
        Battle.Submit(FCombatCommand{ECombatCommandType::EndTurn, 10, {}, INDEX_NONE});
        if (Battle.ActiveId() != 20) { continue; }
        const int32 SlotsBefore = Battle.Find(10)->SlotsByLevel[0] + Battle.Find(10)->SlotsByLevel[1] + Battle.Find(10)->SlotsByLevel[2];
        const FCombatResult R = Battle.Submit(FCombatCommand{ECombatCommandType::Attack, 20, {}, 10});
        if (!R.bHit || R.Damage <= 0) { continue; }
        bFired = true;
        const FCombatantState* Hero = Battle.Find(10);
        TestFalse(TEXT("reaction spent"), Hero->bReactionAvailable);
        TestEqual(TEXT("one slot spent"), Hero->SlotsByLevel[0] + Hero->SlotsByLevel[1] + Hero->SlotsByLevel[2], SlotsBefore - 1);
        TestTrue(TEXT("attacker burned"), Battle.Find(20)->HP < 500);
        TestTrue(TEXT("logged"), R.RollModeSources.ContainsByPredicate([](const FString& S) { return S.StartsWith(TEXT("Hellish Rebuke")); }));
    }
    TestTrue(TEXT("Hellish Rebuke fired on some seed"), bFired);
    // A fighter (no Hellish Rebuke) never answers.
    {
        FString Error; FCombatBattle Battle;
        if (!Battle.Start(ReactionDuel(EAdventurerRole::Fighter, 5), 3, Error)) { AddError(Error); return false; }
        Battle.Submit(FCombatCommand{ECombatCommandType::EndTurn, 10, {}, INDEX_NONE});
        const FCombatResult R = Battle.Submit(FCombatCommand{ECombatCommandType::Attack, 20, {}, 10});
        TestFalse(TEXT("no rebuke from a fighter"), R.RollModeSources.ContainsByPredicate([](const FString& S) { return S.StartsWith(TEXT("Hellish Rebuke")); }));
    }
    // Utility table: size tiers grow with spell level; beam counts follow SRD scaling.
    TestEqual(TEXT("cantrip tier"), FSpellUtilityRules::SizeClass(0), 0);
    TestEqual(TEXT("L4 tier"), FSpellUtilityRules::SizeClass(4), 2);
    TestEqual(TEXT("EB beams at 11"), FSpellUtilityRules::Beams(*FSpellCatalogRules::Find(TEXT("eldritch-blast")), 11, 0), 3);
    TestEqual(TEXT("Magic Missile at slot 3"), FSpellUtilityRules::Beams(*FSpellCatalogRules::Find(TEXT("magic-missile")), 5, 3), 5);
    TestEqual(TEXT("Scorching Ray at slot 4"), FSpellUtilityRules::Beams(*FSpellCatalogRules::Find(TEXT("scorching-ray")), 7, 4), 5);
    return !HasAnyErrors();
}
#endif
