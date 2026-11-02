#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Combat/CombatBattle.h"
#include "Run/ExpeditionRun.h"
#include "Persistence/ExpeditionSave.h"

// HOUSE RULE (not SRD 5.2.1; needs Austin's sign-off): no death saves; Downed for 2 full rounds,
// any healing revives at the healed HP, any attack or damage while Downed kills.
namespace
{
FCombatantState Unit(int32 Id, ECombatSide Side, FIntPoint Cell, int32 Init, int32 HP)
{
    FCombatantState U; U.Id = Id; U.Name = U.Role = FString::Printf(TEXT("U%d"), Id); U.Side = Side; U.Cell = Cell;
    U.InitiativeBonus = Init; U.MaxHP = FMath::Max(30, HP); U.HP = HP;
    if (Side == ECombatSide::Party) { U.AdventurerRole = EAdventurerRole::Cleric; U.SpellSlots = 2; U.Potions = 1; }
    else { U.Speed = 0; }
    if (HP == 0) { U.Life = ECombatLife::Downed; U.RescueTurnsLeft = FCombatBattle::DownedRounds; }
    return U;
}
// Hero 1 acts first, the enemy far away, a downed ally beside the hero.
FCombatEncounter Base()
{
    FCombatEncounter E;
    E.Combatants = {Unit(1, ECombatSide::Party, {0,0}, 30, 20), Unit(2, ECombatSide::Party, {0,1}, -10, 0), Unit(20, ECombatSide::Enemy, {7,7}, 0, 100)};
    return E;
}
bool Same(const FCombatSnapshot& A, const FCombatSnapshot& B) { return FCombatSnapshot::StaticStruct()->CompareScriptStruct(&A, &B, 0); }
bool StartOr(FAutomationTestBase& T, FCombatBattle& B, const FCombatEncounter& E, int32 Seed, FString& Error)
{
    const bool bOk = B.Start(E, Seed, Error); if (!bOk) { T.AddError(Error); } return bOk;
}
void EndTurn(FCombatBattle& B) { B.Submit({ECombatCommandType::EndTurn, B.ActiveId()}); }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDownedExpiryTest, "Project.Expedition.DownedHouseRule.CountdownExpiresAfterTwoFullRounds",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FDownedExpiryTest::RunTest(const FString& Parameters)
{
    FString Error; FCombatBattle B;
    TestTrue(TEXT("Defaults on"), FCombatEncounter().bTwoRoundDowned);
    if (!TestTrue(TEXT("Starts"), StartOr(*this, B, Base(), 42, Error))) { return false; }
    TestTrue(TEXT("House rule in force"), B.TwoRoundDowned());
    const int32 Slot = B.Snapshot().TurnOrder.IndexOfByKey(2);
    auto Ref = B.Snapshot();
    Ref.Encounter.Combatants[1].RescueTurnsLeft = 3;
    FCombatBattle Bad; TestFalse(TEXT("A 3-turn clock is invalid under the house rule"), Bad.Restore(Ref, Error));
    const int32 Round0 = B.Snapshot().Round;
    for (int32 Lap = 1; Lap <= 2; ++Lap)
    {
        // Step until the ally's slot has come round Lap times.
        for (int32 G = 0; G < 20 && B.Snapshot().Round < Round0 + Lap; ++G) { EndTurn(B); }
        const auto* Ally = B.Find(2);
        if (Lap == 1) { TestTrue(TEXT("One round down, one left"), Ally->Life == ECombatLife::Downed && Ally->RescueTurnsLeft == 1); TestTrue(TEXT("Status shows rounds"), B.StatusText(2).Contains(TEXT("Downed (1 round, dies if hit)"))); }
        else { TestTrue(TEXT("Dead after two full rounds"), Ally->Life == ECombatLife::Dead && Ally->RescueTurnsLeft == 0 && Ally->DownedAtSlot == INDEX_NONE); }
    }
    TestTrue(TEXT("No death saves rolled"), B.Find(2)->DeathSaveSuccesses == 0 && B.Find(2)->DeathSaveFailures == 0);
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDownedHealedTest, "Project.Expedition.DownedHouseRule.AnyHealingRevivesAtHealedHP",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FDownedHealedTest::RunTest(const FString& Parameters)
{
    FString Error;
    for (auto Action : {ECombatCommandType::CureWounds, ECombatCommandType::Potion, ECombatCommandType::Rescue})
    {
        FCombatBattle B; StartOr(*this, B, Base(), 42, Error);
        for (int32 G = 0; G < 10 && B.Snapshot().Round < 2; ++G) { EndTurn(B); } // one full round passes: 1 round left
        if (!TestEqual(TEXT("Hero active again"), B.ActiveId(), 1)) { return false; }
        TestEqual(TEXT("One round left"), B.Find(2)->RescueTurnsLeft, 1);
        TestTrue(TEXT("Downed ally is a legal heal target"), B.LegalTargets(1, Action).Contains(2));
        const auto R = B.Submit({Action, 1, {}, 2});
        const auto* Ally = B.Find(2);
        TestTrue(TEXT("Healed in time"), R.bAccepted && Ally->Life == ECombatLife::Conscious);
        TestTrue(TEXT("Revived at the healed HP"), R.Healing > 0 && Ally->HP == R.Healing);
        TestTrue(TEXT("Clock cleared"), Ally->RescueTurnsLeft == 0 && Ally->DownedAtSlot == INDEX_NONE);
        FCombatBattle Copy; TestTrue(TEXT("Revived state restores"), Copy.Restore(B.Snapshot(), Error));
        for (int32 I = 0; I < 9; ++I) { EndTurn(B); }
        TestTrue(TEXT("Revived ally no longer counts down"), B.Find(2)->Life == ECombatLife::Conscious);
    }
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDownedAttackedTest, "Project.Expedition.DownedHouseRule.AttackedWhileDownedDies",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FDownedAttackedTest::RunTest(const FString& Parameters)
{
    FString Error;
    auto E = Base(); E.Combatants[2].Cell = {1, 1}; E.Combatants[2].InitiativeBonus = 10; // adjacent to the ally only
    FCombatBattle B; StartOr(*this, B, E, 42, Error);
    EndTurn(B);
    if (!TestEqual(TEXT("Enemy acts"), B.ActiveId(), 20)) { return false; }
    TestTrue(TEXT("Downed hero is attackable"), B.LegalTargets(20, ECombatCommandType::Attack).Contains(2));
    const FActionForecast F = B.Forecast(20, ECombatCommandType::Attack, 2, 0);
    TestTrue(TEXT("Forecast says dies if hit"), F.bValid && F.bKillsDowned && F.HitChancePercent == 100);
    const auto R = B.Submit({ECombatCommandType::Attack, 20, {}, 2});
    TestTrue(TEXT("Attack on a downed hero kills with no roll"), R.bAccepted && R.Died.Contains(2) && B.Find(2)->Life == ECombatLife::Dead && R.AttackRolls.IsEmpty());
    TestTrue(TEXT("Feedback says dead"), R.Feedback.Contains(TEXT("DEAD")));
    // The AI prefers a conscious hero in reach over finishing the downed one.
    E.Combatants[0].Cell = {2, 1};
    FCombatBattle AI; StartOr(*this, AI, E, 42, Error); EndTurn(AI);
    AI.PlayEnemyTurn();
    TestTrue(TEXT("AI attacks the conscious hero first"), AI.Find(2)->Life == ECombatLife::Downed);
    // With only the downed hero in reach, the AI kills it.
    E.Combatants[0].Cell = {0, 0};
    FCombatBattle AI2; StartOr(*this, AI2, E, 42, Error); EndTurn(AI2);
    AI2.PlayEnemyTurn();
    TestTrue(TEXT("AI finishes a lone downed hero"), AI2.Find(2)->Life == ECombatLife::Dead);
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDownedMultiTest, "Project.Expedition.DownedHouseRule.MultipleUnitsKeepOwnClocks",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FDownedMultiTest::RunTest(const FString& Parameters)
{
    FString Error;
    auto E = Base();
    FCombatantState Third = Unit(3, ECombatSide::Party, {6,6}, 0, 1); E.Combatants.Add(Third);
    E.Combatants[2].InitiativeBonus = 20; E.Combatants[2].Cell = {7,6}; E.Combatants[2].AttackBonus = 30; E.Combatants[2].DamageBonus = 50;
    FCombatBattle B; if (!TestTrue(TEXT("Starts"), StartOr(*this, B, E, 7, Error))) { return false; }
    for (int32 G = 0; G < 10 && B.ActiveId() != 20; ++G) { EndTurn(B); }
    const int32 DropRound = B.Snapshot().Round, DropSlot = B.Snapshot().TurnIndex;
    const auto R = B.Submit({ECombatCommandType::Attack, 20, {}, 3});
    TestTrue(TEXT("Third hero drops"), R.Downed.Contains(3));
    TestTrue(TEXT("Dropped at the enemy's slot with 2 rounds"), B.Find(3)->DownedAtSlot == DropSlot && B.Find(3)->RescueTurnsLeft == 2);
    TestEqual(TEXT("Ally 2 keeps its own clock"), B.Find(2)->DownedAtSlot, INDEX_NONE);
    EndTurn(B);
    for (int32 G = 0; G < 20 && !(B.Snapshot().Round == DropRound + 1 && B.ActiveId() == 20) && B.Snapshot().Phase == EBattlePhase::Fighting; ++G) { EndTurn(B); }
    TestTrue(TEXT("One full round since the drop"), B.Find(3)->Life == ECombatLife::Downed && B.Find(3)->RescueTurnsLeft == 1);
    TestTrue(TEXT("Ally 2 ticked separately"), B.Find(2)->RescueTurnsLeft == 1);
    EndTurn(B);
    for (int32 G = 0; G < 20 && !(B.Snapshot().Round == DropRound + 2 && B.ActiveId() == 20) && B.Snapshot().Phase == EBattlePhase::Fighting; ++G) { EndTurn(B); }
    TestTrue(TEXT("Third hero dead after two full rounds"), B.Find(3)->Life == ECombatLife::Dead);
    TestTrue(TEXT("Ally 2 dead on its own slot"), B.Find(2)->Life == ECombatLife::Dead);
    TestTrue(TEXT("Hero 1 fights on"), B.Find(1)->Life == ECombatLife::Conscious && B.Snapshot().Phase == EBattlePhase::Fighting);
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDownedSaveTest, "Project.Expedition.DownedHouseRule.CountdownSaveLoadRoundTrip",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FDownedSaveTest::RunTest(const FString& Parameters)
{
    FString Error;
    auto E = Base(); FCombatantState Third = Unit(3, ECombatSide::Party, {6,6}, 0, 1); E.Combatants.Add(Third);
    E.Combatants[2].InitiativeBonus = 20; E.Combatants[2].Cell = {7,6}; E.Combatants[2].AttackBonus = 30; E.Combatants[2].DamageBonus = 50;
    FCombatBattle B; StartOr(*this, B, E, 7, Error);
    for (int32 G = 0; G < 10 && B.ActiveId() != 20; ++G) { EndTurn(B); }
    B.Submit({ECombatCommandType::Attack, 20, {}, 3});
    const auto Snap = B.Snapshot();
    FCombatBattle Loaded;
    if (!TestTrue(TEXT("Downed countdown restores"), Loaded.Restore(Snap, Error))) { AddError(Error); return false; }
    TestTrue(TEXT("Exact state"), Same(Snap, Loaded.Snapshot()));
    for (int32 I = 0; I < 8; ++I) { EndTurn(B); EndTurn(Loaded); }
    TestTrue(TEXT("Restored clock continues identically"), Same(B.Snapshot(), Loaded.Snapshot()));
    auto Bad = Snap; Bad.Encounter.Combatants[3].DownedAtSlot = Bad.TurnOrder.Num();
    TestFalse(TEXT("Out-of-range slot rejected"), Loaded.Restore(Bad, Error));
    Bad = Snap; Bad.Encounter.Combatants[0].DownedAtSlot = 0;
    TestFalse(TEXT("Slot on a conscious unit rejected"), Loaded.Restore(Bad, Error));
    Bad = Snap; Bad.Encounter.bTwoRoundDowned = false;
    TestFalse(TEXT("Slot with the rule off rejected"), Loaded.Restore(Bad, Error));
    // The field is part of the strict version-5 save JSON.
    FExpeditionRun Run; Run.NewRun(42, Error, TEXT("downed-fixture")); Run.ChooseRoute(Run.Progress().Offers[0].Id, Error);
    FExpeditionSaveData Data; Data.Current = Run.Snapshot();
    FString Json; TestTrue(TEXT("Encodes"), FExpeditionSave::Encode(Data, Json, Error));
    TestTrue(TEXT("Save carries the countdown slot and the toggle"), Json.Contains(TEXT("\"DownedAtSlot\"")) && Json.Contains(TEXT("\"bTwoRoundDowned\"")));
    FExpeditionSaveData Back; TestTrue(TEXT("Decodes"), FExpeditionSave::Decode(Json, Back, Error));
    TestEqual(TEXT("Version stays 5"), Back.Version, 5);
    FString Missing = Json.Replace(TEXT("\"DownedAtSlot\""), TEXT("\"DownedAtSlotX\""));
    TestFalse(TEXT("Missing field rejected"), FExpeditionSave::Decode(Missing, Back, Error));
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDownedToggleOffTest, "Project.Expedition.DownedHouseRule.ToggleOffRestoresRescueClock",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FDownedToggleOffTest::RunTest(const FString& Parameters)
{
    FString Error;
    auto E = Base(); E.bTwoRoundDowned = false; E.Combatants[1].RescueTurnsLeft = 3;
    E.Combatants[2].Cell = {1, 1}; E.Combatants[2].InitiativeBonus = 10;
    FCombatBattle B; if (!TestTrue(TEXT("Starts"), StartOr(*this, B, E, 42, Error))) { return false; }
    TestFalse(TEXT("Rule off"), B.TwoRoundDowned());
    TestFalse(TEXT("Cure Wounds cannot reach a downed ally"), B.LegalTargets(1, ECombatCommandType::CureWounds).Contains(2));
    EndTurn(B);
    TestFalse(TEXT("Downed hero is not attackable"), B.LegalTargets(20, ECombatCommandType::Attack).Contains(2));
    TestFalse(TEXT("No kill forecast"), B.Forecast(20, ECombatCommandType::Attack, 2, 0).bKillsDowned);
    EndTurn(B); EndTurn(B);
    TestTrue(TEXT("Old clock ticks per own turn"), B.Find(2)->RescueTurnsLeft == 2 && B.Find(2)->DownedAtSlot == INDEX_NONE);
    TestTrue(TEXT("Status has no house-rule text"), !B.StatusText(2).Contains(TEXT("dies if hit")));
    // Death saves on also disables the house rule.
    auto D = Base(); D.bDeathSaves = true; D.Combatants[1].RescueTurnsLeft = 3;
    FCombatBattle S; TestTrue(TEXT("Starts with death saves"), StartOr(*this, S, D, 42, Error)); TestFalse(TEXT("Death saves override"), S.TwoRoundDowned());
    return !HasAnyErrors();
}
#endif
