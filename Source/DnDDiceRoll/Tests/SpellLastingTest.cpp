#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Combat/CombatBattle.h"
#include "Rules/SpellCatalogRules.h"
#include "Rules/SpellTacticsRules.h"
#include "Party/BuildCatalog.h"

// Spells phase 2 (SRD 5.2.1): durations in rounds, lasting zones, repeat saves, one slot per turn, CastSpell forecast, prepared lists.
namespace SpellLastingTest
{
FCombatEncounter LastingCaster(EAdventurerRole Role, int32 Level, FIntPoint FoeCell)
{
    FCombatEncounter Encounter;
    Encounter.Width = Encounter.Height = 12;
    FCombatantState Me;
    if (static_cast<uint8>(Role) >= static_cast<uint8>(EAdventurerRole::Monk))
    {
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
FCombatCommand LastingCast(const TCHAR* Id, int32 Target, int32 Slot = 0)
{
    FCombatCommand C{ECombatCommandType::CastSpell, 10, {}, Target}; C.SpellId = Id; C.SlotLevel = Slot; return C;
}
FCombatCommand LastingEnd(int32 Id) { return FCombatCommand{ECombatCommandType::EndTurn, Id}; }
bool LastingCastable(const TCHAR* Id) { const FSrdSpell* S = FSpellCatalogRules::Find(Id); return S && FSpellCatalogRules::CombatResolvable(*S); }
}
using namespace SpellLastingTest;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSpellDurationRoundsTest, "Project.Expedition.Spells.Lasting.DurationRounds",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSpellDurationRoundsTest::RunTest(const FString& Parameters)
{
    TestEqual(TEXT("Instantaneous"), FSpellTacticsRules::DurationRounds(TEXT("instantaneous")), 0);
    TestEqual(TEXT("1 round"), FSpellTacticsRules::DurationRounds(TEXT("1 round")), 1);
    TestEqual(TEXT("1 minute = 10 rounds"), FSpellTacticsRules::DurationRounds(TEXT("up to 1 minute")), 10);
    TestEqual(TEXT("10 minutes"), FSpellTacticsRules::DurationRounds(TEXT("up to 10 minutes")), 100);
    TestEqual(TEXT("1 hour"), FSpellTacticsRules::DurationRounds(TEXT("1 hour")), 600);
    TestEqual(TEXT("8 hours"), FSpellTacticsRules::DurationRounds(TEXT("8 hours")), 4800);
    TestEqual(TEXT("Until dispelled"), FSpellTacticsRules::DurationRounds(TEXT("until dispelled")), -1);
    // Every catalog cantrip and level 1-4 spell parses to a known duration (data-driven for the level-20 expansion).
    for (int32 Index = 0; Index < FSpellCatalogRules::Num(); ++Index)
    {
        const FSrdSpell& S = FSpellCatalogRules::At(Index);
        if (S.Level > 4) { continue; }
        const int32 R = FSpellTacticsRules::DurationRounds(S);
        TestTrue(FString::Printf(TEXT("%s duration parses"), S.Id), R >= -1);
        if (S.bConcentration) { TestTrue(FString::Printf(TEXT("%s concentration lasts rounds"), S.Id), R != 0); }
    }
    for (int32 Index = 0; Index < FSpellTacticsRules::ZoneCount(); ++Index)
    {
        TestNotNull(FString::Printf(TEXT("Zone %s is a catalog spell"), FSpellTacticsRules::ZoneAt(Index).Id), FSpellCatalogRules::Find(FSpellTacticsRules::ZoneAt(Index).Id));
    }
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSpellOneSlotPerTurnTest, "Project.Expedition.Spells.Lasting.OneSlotPerTurn",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSpellOneSlotPerTurnTest::RunTest(const FString& Parameters)
{
    FString Error; FCombatBattle Battle;
    TestTrue(TEXT("Start cleric"), Battle.Start(LastingCaster(EAdventurerRole::Cleric, 5, {1, 0}), 7, Error));
    TestTrue(TEXT("Guiding Bolt"), Battle.Submit(LastingCast(TEXT("guiding-bolt"), 20)).bAccepted);
    TestTrue(TEXT("Slot spent flag"), Battle.Snapshot().bSlotSpentThisTurn);
    TestFalse(TEXT("Healing Word (bonus action) needs a second slot this turn"), Battle.Submit(LastingCast(TEXT("healing-word"), 10)).bAccepted);
    TestFalse(TEXT("Not listed castable"), Battle.CastableSpells(10).Contains(TEXT("healing-word")));
    TestTrue(TEXT("End caster turn"), Battle.Submit(LastingEnd(10)).bAccepted);
    TestTrue(TEXT("End foe turn"), Battle.Submit(LastingEnd(20)).bAccepted);
    TestFalse(TEXT("Flag resets at the next turn"), Battle.Snapshot().bSlotSpentThisTurn);
    TestTrue(TEXT("Healing Word next turn"), Battle.Submit(LastingCast(TEXT("healing-word"), 10)).bAccepted);
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSpellZonesTest, "Project.Expedition.Spells.Lasting.Zones",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSpellZonesTest::RunTest(const FString& Parameters)
{
    if (LastingCastable(TEXT("spirit-guardians")))
    {
        FString Error; FCombatBattle Battle;
        TestTrue(TEXT("Start cleric"), Battle.Start(LastingCaster(EAdventurerRole::Cleric, 5, {1, 0}), 11, Error));
        const FCombatResult R = Battle.Submit(LastingCast(TEXT("spirit-guardians"), 20));
        TestTrue(TEXT("Spirit Guardians: ") + R.Feedback, R.bAccepted);
        TestEqual(TEXT("No damage when the emanation appears (SRD 5.2.1)"), Battle.Find(20)->HP, 500);
        TestEqual(TEXT("One zone in effect"), Battle.ActiveSpells().Num(), 1);
        if (Battle.ActiveSpells().Num() == 1)
        {
            TestEqual(TEXT("10 minutes = 100 rounds"), Battle.ActiveSpells()[0].RoundsLeft, 100);
            TestEqual(TEXT("15-ft emanation = 3 cells"), Battle.ActiveSpells()[0].RadiusCells, 3);
            TestTrue(TEXT("Zone covers the foe"), Battle.ZoneCells(Battle.ActiveSpells()[0]).Contains(FIntPoint(1, 0)));
        }
        TestTrue(TEXT("Concentrating"), Battle.Find(10)->bConcentrating);
        TestTrue(TEXT("Cast VFX event"), R.SpellEvents.Num() == 1 && R.SpellEvents[0].Kind == ESpellEventKind::Cast && R.SpellEvents[0].Cells.Num() > 0 && R.SpellEvents[0].DurationRounds == 100);
        Battle.Submit(LastingEnd(10));
        const FCombatResult F = Battle.Submit(LastingEnd(20));
        TestTrue(TEXT("Foe ends turn in the zone and takes radiant damage"), Battle.Find(20)->HP < 500);
        TestTrue(TEXT("Trigger event"), F.SpellEvents.ContainsByPredicate([](const FSpellCastEvent& E) { return E.Kind == ESpellEventKind::Trigger && E.TargetIds.Contains(20); }));
        // Mid-zone save and restore keeps the zone; a bad serial is rejected.
        FCombatBattle Copy; TestTrue(TEXT("Restore with a live zone: ") + Error, Copy.Restore(Battle.Snapshot(), Error));
        FCombatSnapshot Bad = Battle.Snapshot();
        if (Bad.ActiveSpells.Num() > 0) { Bad.ActiveSpells[0].Serial = Bad.NextSpellSerial; TestFalse(TEXT("Serial at or past NextSpellSerial rejected"), Copy.Restore(Bad, Error)); }
        Bad = Battle.Snapshot(); Bad.Encounter.Combatants[0].bConcentrating = false;
        TestFalse(TEXT("Concentration zone without concentration rejected"), Copy.Restore(Bad, Error));
    }
    if (LastingCastable(TEXT("moonbeam")))
    {
        FString Error; FCombatBattle Battle;
        TestTrue(TEXT("Start druid"), Battle.Start(LastingCaster(EAdventurerRole::Druid, 5, {4, 0}), 5, Error));
        const FCombatResult R = Battle.Submit(LastingCast(TEXT("moonbeam"), 20));
        TestTrue(TEXT("Moonbeam: ") + R.Feedback, R.bAccepted);
        TestTrue(TEXT("Moonbeam hurts when it appears"), Battle.Find(20)->HP < 500);
        // 1 minute: the caster's tenth turn start ends it and its concentration.
        for (int32 Turn = 0; Turn < 10 && Battle.Snapshot().Phase == EBattlePhase::Fighting; ++Turn)
        {
            TestEqual(FString::Printf(TEXT("Still up before turn %d"), Turn), Battle.ActiveSpells().Num(), 1);
            Battle.Submit(LastingEnd(10)); Battle.Submit(LastingEnd(20));
        }
        TestEqual(TEXT("Moonbeam ends after 10 rounds"), Battle.ActiveSpells().Num(), 0);
        TestFalse(TEXT("Concentration ends with it"), Battle.Find(10)->bConcentrating);
    }
    if (LastingCastable(TEXT("spike-growth")))
    {
        FString Error; FCombatBattle Battle;
        TestTrue(TEXT("Start druid"), Battle.Start(LastingCaster(EAdventurerRole::Druid, 5, {6, 6}), 5, Error));
        TestTrue(TEXT("Spike Growth"), Battle.Submit(LastingCast(TEXT("spike-growth"), 20)).bAccepted);
        TestEqual(TEXT("No damage on casting"), Battle.Find(20)->HP, 500);
        TestTrue(TEXT("Zone is difficult terrain"), Battle.IsDifficultTerrain({6, 7}));
        Battle.Submit(LastingEnd(10));
        FCombatCommand Move{ECombatCommandType::Move, 20, {6, 7}};
        TestTrue(TEXT("Foe walks one cell in the spikes"), Battle.Submit(Move).bAccepted);
        const int32 After = Battle.Find(20)->HP;
        TestTrue(TEXT("2d4 piercing per 5 ft"), After <= 498 && After >= 492);
    }
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSpellRepeatSaveTest, "Project.Expedition.Spells.Lasting.RepeatSaves",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSpellRepeatSaveTest::RunTest(const FString& Parameters)
{
    TestTrue(TEXT("Hold Person repeats its save"), FSpellTacticsRules::RepeatsSaveAtEndOfTurn(*FSpellCatalogRules::Find(TEXT("hold-person"))));
    TestFalse(TEXT("Fireball does not"), FSpellTacticsRules::RepeatsSaveAtEndOfTurn(*FSpellCatalogRules::Find(TEXT("fireball"))));
    bool bHeldOnce = false, bBroke = false;
    for (int32 Seed = 1; Seed < 60 && !(bHeldOnce && bBroke); ++Seed)
    {
        FString Error; FCombatBattle Battle;
        if (!Battle.Start(LastingCaster(EAdventurerRole::Wizard, 5, {2, 0}), Seed, Error)) { continue; }
        if (!Battle.Submit(LastingCast(TEXT("hold-person"), 20)).bAccepted || Battle.ActiveSpells().IsEmpty()) { continue; }
        bHeldOnce = true;
        TestEqual(TEXT("Held entry tracks the target"), Battle.ActiveSpells()[0].TargetId, 20);
        for (int32 Round = 0; Round < 9 && !Battle.ActiveSpells().IsEmpty(); ++Round) { Battle.Submit(LastingEnd(10)); Battle.Submit(LastingEnd(20)); }
        const bool bParalyzed = Battle.Find(20)->Conditions.ContainsByPredicate([](const FCombatCondition& C) { return C.Condition == ECombatCondition::Paralyzed; });
        TestEqual(TEXT("Paralysis lasts exactly while the spell does"), bParalyzed, !Battle.ActiveSpells().IsEmpty());
        if (Battle.ActiveSpells().IsEmpty()) { bBroke = true; TestFalse(TEXT("Concentration drops when the only target saves"), Battle.Find(10)->bConcentrating); }
    }
    TestTrue(TEXT("Some seed paralyzes"), bHeldOnce);
    TestTrue(TEXT("Some target breaks free by saving"), bBroke);
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSpellForecastTest, "Project.Expedition.Spells.Lasting.Forecast",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSpellForecastTest::RunTest(const FString& Parameters)
{
    FString Error; FCombatBattle Battle;
    TestTrue(TEXT("Start wizard"), Battle.Start(LastingCaster(EAdventurerRole::Wizard, 5, {4, 0}), 9, Error));
    const int32 Rng = Battle.Snapshot().RandomState;
    const FSpellForecast Bolt = Battle.ForecastSpell(10, TEXT("fire-bolt"), 20);
    TestTrue(TEXT("Fire Bolt forecast valid"), Bolt.bValid && Bolt.bAttackRoll);
    TestTrue(TEXT("Hit chance in range"), Bolt.ChanceOutOf400 > 0 && Bolt.ChanceOutOf400 < 400);
    TestEqual(TEXT("Fire Bolt 2d10 at level 5 min"), Bolt.MinAmount, 2);
    TestEqual(TEXT("Fire Bolt 2d10 at level 5 max"), Bolt.MaxAmount, 20);
    const FSpellForecast Ball = Battle.ForecastSpell(10, TEXT("fireball"), 20);
    TestTrue(TEXT("Fireball save forecast"), Ball.bValid && Ball.bSave && Ball.SaveDc > 0);
    TestEqual(TEXT("Fireball 8d6 min"), Ball.MinAmount, 8);
    TestEqual(TEXT("Fireball 8d6 max"), Ball.MaxAmount, 48);
    TestTrue(TEXT("Fireball reaches the foe and shows cells"), Ball.AffectedIds.Contains(20) && Ball.Cells.Num() > 1);
    const FSpellForecast Up = Battle.ForecastSpell(10, TEXT("fireball"), 20, 4);
    TestFalse(TEXT("No 4th-level slot at level 5: upcast forecast invalid"), Up.bValid);
    const FSpellForecast Hold = Battle.ForecastSpell(10, TEXT("hold-person"), 20);
    TestTrue(TEXT("Hold Person concentration, 10 rounds"), Hold.bConcentration && Hold.DurationRounds == 10 && !Hold.bEndsConcentration);
    TestFalse(TEXT("Invalid target"), Battle.ForecastSpell(10, TEXT("fire-bolt"), 10).bValid);
    TestEqual(TEXT("Forecast consumes no RNG"), Battle.Snapshot().RandomState, Rng);
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSpellPreparedTest, "Project.Expedition.Spells.Lasting.PreparedSpells",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSpellPreparedTest::RunTest(const FString& Parameters)
{
    FString Error; FCombatBattle Battle;
    FCombatEncounter E = LastingCaster(EAdventurerRole::Wizard, 1, {2, 0});
    E.Combatants[0].PreparedSpells = {TEXT("fire-bolt"), TEXT("magic-missile")};
    TestTrue(TEXT("Prepared list accepted: ") + Error, Battle.Start(E, 3, Error));
    const TArray<FString> List = Battle.CastableSpells(10);
    TestTrue(TEXT("Only prepared spells castable"), List.Num() == 2 && List.Contains(TEXT("fire-bolt")) && List.Contains(TEXT("magic-missile")));
    TestFalse(TEXT("Unprepared Ray of Frost refused"), Battle.Submit(LastingCast(TEXT("ray-of-frost"), 20)).bAccepted);
    FCombatEncounter Wrong = E; Wrong.Combatants[0].PreparedSpells = {TEXT("cure-wounds")};
    TestFalse(TEXT("Off-list spell rejected"), FCombatBattle().Start(Wrong, 3, Error));
    FCombatEncounter High = E; High.Combatants[0].PreparedSpells = {TEXT("fireball")};
    TestFalse(TEXT("Spell above the class level rejected"), FCombatBattle().Start(High, 3, Error));
    FCombatEncounter Dup = E; Dup.Combatants[0].PreparedSpells = {TEXT("fire-bolt"), TEXT("fire-bolt")};
    TestFalse(TEXT("Duplicate rejected"), FCombatBattle().Start(Dup, 3, Error));
    FCombatEncounter Many = E; Many.Combatants[0].PreparedSpells.Reset();
    const int32 Cap = FSpellCatalogRules::CantripsKnown(EAdventurerRole::Wizard, 1);
    for (int32 Index = 0; Index < FSpellCatalogRules::Num() && Many.Combatants[0].PreparedSpells.Num() <= Cap; ++Index)
    {
        const FSrdSpell& S = FSpellCatalogRules::At(Index);
        if (S.Level == 0 && FSpellCatalogRules::CanLearn(EAdventurerRole::Wizard, 1, S)) { Many.Combatants[0].PreparedSpells.Add(S.Id); }
    }
    TestFalse(TEXT("More cantrips than the SRD allows rejected"), FCombatBattle().Start(Many, 3, Error));
    return !HasAnyErrors();
}
#endif