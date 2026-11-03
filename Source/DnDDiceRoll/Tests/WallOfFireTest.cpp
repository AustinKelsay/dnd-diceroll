#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Combat/CombatBattle.h"
#include "Rules/HexGrid.h"
#include "Rules/SpellCatalogRules.h"
#include "Rules/SpellTacticsRules.h"
#include "Party/BuildCatalog.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWallOfFirePathTest, "Project.Expedition.Spells.WallOfFirePath",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FWallOfFirePathTest::RunTest(const FString& Parameters)
{
    const FSpellZone* Zone = FSpellTacticsRules::Zone(TEXT("wall-of-fire"));
    if (!TestNotNull(TEXT("Wall of Fire is a zone"), Zone)) { return false; }
    TestEqual(TEXT("60 ft wall path"), Zone->PathFeet, 60);
    const FSrdSpell* S = FSpellCatalogRules::Find(TEXT("wall-of-fire"));
    TestTrue(TEXT("Catalog: Line 60"), S && S->Area == ESpellArea::Line && S->AreaFeet == 60);
    const FSrdSpell* Sphere = FSpellCatalogRules::Find(TEXT("flaming-sphere"));
    TestTrue(TEXT("Catalog: Flaming Sphere is a 5 ft sphere"), Sphere && Sphere->Area == ESpellArea::Sphere && Sphere->AreaFeet == 5);

    const TArray<FIntPoint> Single = FSpellTacticsRules::WallPath({4, 4}, {}, 60);
    TestEqual(TEXT("No nodes: one hex at the aim"), Single.Num(), 1);
    const TArray<FIntPoint> Bent = FSpellTacticsRules::WallPath({1, 1}, {{4, 1}, {4, 4}}, 60);
    TestTrue(TEXT("Bent wall reaches both nodes"), Bent.Contains(FIntPoint(4, 1)) && Bent.Contains(FIntPoint(4, 4)));
    TestEqual(TEXT("Bent wall starts at the aim"), Bent[0], FIntPoint(1, 1));
    bool bConnected = true;
    for (int32 I = 1; I < Bent.Num(); ++I) { bConnected &= FHexGrid::BoardDistance(Bent[I - 1], Bent[I]) == 1; }
    TestTrue(TEXT("Wall is one connected hex line"), bConnected);
    TestEqual(TEXT("Deterministic"), FSpellTacticsRules::WallPath({1, 1}, {{4, 1}, {4, 4}}, 60), Bent);
    const TArray<FIntPoint> Long = FSpellTacticsRules::WallPath({0, 0}, {{20, 0}, {20, 20}}, 60);
    TestEqual(TEXT("Capped at 60 ft = 12 hexes"), Long.Num(), 12);
    TestEqual(TEXT("A 10 ft wall is 2 hexes"), FSpellTacticsRules::WallPath({0, 0}, {{9, 0}}, 10).Num(), 2);

    // In combat: the wall burns only creatures on its hexes.
    FCombatEncounter E; E.Width = E.Height = 12;
    FCombatantState Me;
    Me.Id = 10; Me.Name = TEXT("Caster"); Me.Role = TEXT("Raider"); Me.AdventurerRole = EAdventurerRole::Wizard; Me.Level = 5; Me.Cell = {0, 0}; Me.InitiativeBonus = 30;
    Me.MaxHP = Me.HP = 300; Me.SlotsByLevel.SetNum(FSpellRules::MaxSpellLevel);
    for (int32 L = 1; L <= FSpellRules::MaxSpellLevel; ++L) { Me.SlotsByLevel[L - 1] = FMath::Min(2, FBuildCatalog::SlotMax(Me, L)); }
    if (FBuildCatalog::Find(Me.BuildId)) { Me.SlotsByLevel[0] = FMath::Min(Me.SlotsByLevel[0], FBuildCatalog::SlotCap(Me)); }
    Me.SpellSlots = Me.SlotsByLevel[0]; Me.KitUses = FMath::Min(Me.KitUses, Me.SpellSlots);
    E.Combatants.Add(Me);
    FCombatantState Foe; Foe.Side = ECombatSide::Enemy; Foe.Role = TEXT("Raider"); Foe.InitiativeBonus = -10; Foe.MaxHP = Foe.HP = 500;
    Foe.Id = 20; Foe.Name = TEXT("OnWall"); Foe.Cell = {4, 1}; E.Combatants.Add(Foe);
    Foe.Id = 21; Foe.Name = TEXT("OnNode"); Foe.Cell = {4, 3}; E.Combatants.Add(Foe);
    Foe.Id = 22; Foe.Name = TEXT("Clear"); Foe.Cell = {8, 8}; E.Combatants.Add(Foe);
    FString Error; FCombatBattle Battle;
    if (!TestTrue(TEXT("Starts: ") + Error, Battle.Start(E, 5, Error))) { return false; }
    // Combat levels stop at 5, so no hero can cast a 4th-level wall yet: place the zone through a restored snapshot.
    const TArray<FIntPoint> Expected = FSpellTacticsRules::WallPath({4, 1}, {{4, 3}}, 60);
    FCombatSnapshot Snap = Battle.Snapshot();
    FActiveSpell Wall; Wall.SpellId = TEXT("wall-of-fire"); Wall.CasterId = 10; Wall.SlotLevel = 4; Wall.RoundsLeft = 10; Wall.bConcentration = true;
    Wall.Origin = {4, 1}; Wall.Cells = Expected; Wall.Dc = 15; Wall.Serial = Snap.NextSpellSerial++;
    Snap.ActiveSpells.Add(Wall);
    for (FCombatantState& U : Snap.Encounter.Combatants) { if (U.Id == 10) { U.bConcentrating = true; } }
    FCombatBattle Live;
    if (!TestTrue(TEXT("Restore with a wall: ") + Error, Live.Restore(Snap, Error))) { return false; }
    TestEqual(TEXT("ZoneCells is the node path"), Live.ZoneCells(Live.ActiveSpells()[0]), Expected);
    FCombatBattle Copy;
    TestTrue(TEXT("Save and restore keeps the wall"), Copy.Restore(Live.Snapshot(), Error) && Copy.ActiveSpells().Num() == 1 && Copy.ActiveSpells()[0].Cells == Expected);
    for (int32 Step = 0; Step < 8 && Live.Snapshot().Phase == EBattlePhase::Fighting; ++Step) { Live.Submit(FCombatCommand{ECombatCommandType::EndTurn, Live.ActiveId()}); }
    TestTrue(TEXT("Creature on the wall burns at its turn end"), Live.Find(20)->HP < 500 && Live.Find(21)->HP < 500);
    TestEqual(TEXT("Creature off the wall is untouched"), Live.Find(22)->HP, 500);
    return !HasAnyErrors();
}
#endif
