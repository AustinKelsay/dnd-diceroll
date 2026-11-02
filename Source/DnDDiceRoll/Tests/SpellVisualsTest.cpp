#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Combat/CombatBattle.h"
#include "Rules/SpellCatalogRules.h"
#include "Rules/SpellUtilityRules.h"
#include "Party/BuildCatalog.h"

namespace SpellVisualsTestLocal
{
FCombatEncounter VisualCaster(EAdventurerRole Role, FIntPoint FoeCell)
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
    Me.Id = 10; Me.Name = TEXT("Caster"); Me.Role = TEXT("Raider"); Me.AdventurerRole = Role; Me.Level = 5; Me.Cell = {0, 0}; Me.InitiativeBonus = 30;
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
FCombatCommand VisualCast(const TCHAR* Id, int32 Target, int32 Slot = 0)
{
    FCombatCommand C{ECombatCommandType::CastSpell, 10, {}, Target}; C.SpellId = Id; C.SlotLevel = Slot; return C;
}
const FSpellCastEvent* FirstOf(const TArray<FSpellCastEvent>& Events, ESpellEventKind Kind, const TCHAR* Id)
{
    return Events.FindByPredicate([&](const FSpellCastEvent& E) { return E.Kind == Kind && E.SpellId == Id; });
}
}
using namespace SpellVisualsTestLocal;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSpellVisualsTest, "Project.Expedition.Spells.VisualHooks", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSpellVisualsTest::RunTest(const FString& Parameters)
{
    // Size classes: cantrip, 1-2, 3, 4-6, 7-9.
    const int32 Expected[10] = {0, 1, 1, 2, 3, 3, 3, 4, 4, 4};
    for (int32 L = 0; L <= 9; ++L) { TestEqual(FString::Printf(TEXT("size class of level %d"), L), FSpellUtilityRules::SizeClass(L), Expected[L]); }
    {
        FString Error; FCombatBattle Battle;
        if (!TestTrue(TEXT("Start wizard: ") + Error, Battle.Start(VisualCaster(EAdventurerRole::Wizard, {4, 0}), 3, Error))) { return false; }
        int32 Hooked = 0;
        Battle.OnSpellEvent = [&](const FSpellCastEvent&) { ++Hooked; };
        const FCombatResult R = Battle.Submit(VisualCast(TEXT("fire-bolt"), 20));
        const FSpellCastEvent* E = FirstOf(R.SpellEvents, ESpellEventKind::Cast, TEXT("fire-bolt"));
        if (TestNotNull(TEXT("Fire Bolt cast event"), E))
        {
            TestEqual(TEXT("cantrip size class"), E->SizeClass, 0);
            TestEqual(TEXT("level 0"), E->SpellLevel, 0);
            TestEqual(TEXT("evocation"), E->School, static_cast<uint8>(FSpellCatalogRules::Find(TEXT("fire-bolt"))->School));
            TestEqual(TEXT("level-5 cantrip tier"), E->Tier, 1);
            TestFalse(TEXT("instant spell does not persist"), E->bPersist);
            TestEqual(TEXT("caster cell"), E->CasterCell, FIntPoint(0, 0));
            TestTrue(TEXT("target cell"), E->TargetIds.Contains(20) && E->TargetCells.Contains(FIntPoint(4, 0)));
        }
        TestEqual(TEXT("hook saw every event"), Hooked, R.SpellEvents.Num());
        TestEqual(TEXT("nothing lingering"), Battle.SpellVisuals().Num(), 0);
    }
    {
        FString Error; FCombatBattle Battle;
        Battle.Start(VisualCaster(EAdventurerRole::Wizard, {6, 6}), 3, Error);
        const FCombatResult R = Battle.Submit(VisualCast(TEXT("fireball"), 20, 3));
        const FSpellCastEvent* E = FirstOf(R.SpellEvents, ESpellEventKind::Cast, TEXT("fireball"));
        if (TestNotNull(TEXT("Fireball cast event"), E))
        {
            TestEqual(TEXT("level 3 size class"), E->SizeClass, 2);
            TestEqual(TEXT("sphere"), E->AreaShape, static_cast<uint8>(ESpellArea::Sphere));
            TestTrue(TEXT("area cells"), E->Cells.Num() > 1);
            TestEqual(TEXT("slot"), E->SlotLevel, 3);
        }
    }
    {
        // A lasting zone: Persist snapshots while it is up, per-turn Tick events, an End event when it goes.
        FString Error; FCombatBattle Battle;
        Battle.Start(VisualCaster(EAdventurerRole::Druid, {4, 0}), 5, Error);
        const FCombatResult R = Battle.Submit(VisualCast(TEXT("moonbeam"), 20));
        TestTrue(TEXT("Moonbeam: ") + R.Feedback, R.bAccepted);
        const FSpellCastEvent* E = FirstOf(R.SpellEvents, ESpellEventKind::Cast, TEXT("moonbeam"));
        if (TestNotNull(TEXT("Moonbeam cast event"), E)) { TestTrue(TEXT("cast persists"), E->bPersist); TestEqual(TEXT("level 2 size class"), E->SizeClass, 1); }
        const TArray<FSpellCastEvent> Now = Battle.SpellVisuals();
        if (TestEqual(TEXT("one visual in effect"), Now.Num(), 1))
        {
            TestTrue(TEXT("persist kind"), Now[0].Kind == ESpellEventKind::Persist && Now[0].bPersist);
            TestTrue(TEXT("zone cells"), Now[0].Cells.Num() > 0);
            TestTrue(TEXT("rounds left"), Now[0].DurationRounds > 0);
            TestEqual(TEXT("same serial as cast"), Now[0].Serial, E ? E->Serial : -1);
        }
        bool bTick = false, bEnd = false;
        for (int32 Turn = 0; Turn < 12 && !bEnd && Battle.Snapshot().Phase == EBattlePhase::Fighting; ++Turn)
        {
            for (const int32 Id : {10, 20})
            {
                const FCombatResult T = Battle.Submit(FCombatCommand{ECombatCommandType::EndTurn, Id});
                bTick |= FirstOf(T.SpellEvents, ESpellEventKind::Tick, TEXT("moonbeam")) || FirstOf(T.SpellEvents, ESpellEventKind::Trigger, TEXT("moonbeam"));
                bEnd |= FirstOf(T.SpellEvents, ESpellEventKind::End, TEXT("moonbeam")) != nullptr;
            }
        }
        TestTrue(TEXT("per-turn tick/trigger events"), bTick);
        TestTrue(TEXT("end event"), bEnd);
        TestEqual(TEXT("no visuals after it ends"), Battle.SpellVisuals().Num(), 0);
    }
    return true;
}
#endif
