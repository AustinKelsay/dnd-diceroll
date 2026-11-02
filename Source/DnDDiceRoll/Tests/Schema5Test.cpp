#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Combat/CombatBattle.h"
#include "Run/ExpeditionRun.h"
#include "Persistence/ExpeditionSave.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

namespace Schema5Test
{
FCombatEncounter Pair(FIntPoint EnemyCell)
{
    FCombatEncounter Encounter;
    Encounter.Width = Encounter.Height = 8;
    FCombatantState Ally;
    Ally.Id = 10; Ally.Name = TEXT("Ally"); Ally.Role = TEXT("Raider"); Ally.Cell = {0, 0}; Ally.InitiativeBonus = 30;
    Encounter.Combatants.Add(Ally);
    FCombatantState Enemy;
    Enemy.Id = 20; Enemy.Name = TEXT("Foe"); Enemy.Role = TEXT("Raider"); Enemy.Side = ECombatSide::Enemy; Enemy.Cell = EnemyCell; Enemy.InitiativeBonus = -10;
    Encounter.Combatants.Add(Enemy);
    return Encounter;
}
FCombatCondition Cond(ECombatCondition Kind, int32 Source = INDEX_NONE, int32 Rounds = -1)
{
    FCombatCondition Condition; Condition.Condition = Kind; Condition.SourceId = Source; Condition.RoundsLeft = Rounds; return Condition;
}
bool Has(const FCombatantState* Unit, ECombatCondition Kind)
{
    return Unit && Unit->Conditions.ContainsByPredicate([Kind](const FCombatCondition& C) { return C.Condition == Kind; });
}
FExpeditionSnapshot ActiveRun(FExpeditionRun& Run)
{
    FString Error;
    Run.NewRun(42, Error, TEXT("schema5-fixture"));
    Run.ChooseRoute(Run.Progress().Offers[0].Id, Error);
    return Run.Snapshot();
}
FCombatantState& FirstParty(FExpeditionSnapshot& Snapshot)
{
    return *Snapshot.Battle.Encounter.Combatants.FindByPredicate([](const FCombatantState& U) { return U.Side == ECombatSide::Party && U.Life == ECombatLife::Conscious; });
}
bool Same(const FExpeditionSnapshot& A, const FExpeditionSnapshot& B) { return FExpeditionSnapshot::StaticStruct()->CompareScriptStruct(&A, &B, 0); }
TSharedPtr<FJsonObject> FirstCombatantJson(const TSharedPtr<FJsonObject>& Root)
{
    return Root->GetObjectField(TEXT("Current"))->GetObjectField(TEXT("Battle"))->GetObjectField(TEXT("Encounter"))->GetArrayField(TEXT("Combatants"))[0]->AsObject();
}
FString Text(const TSharedPtr<FJsonObject>& Root)
{
    FString Out; FJsonSerializer::Serialize(Root.ToSharedRef(), TJsonWriterFactory<>::Create(&Out)); return Out;
}
}
using namespace Schema5Test;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSchema5ConditionRoundtripTest, "Project.Expedition.Schema5.VersionAndConditionsRoundtrip",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSchema5ConditionRoundtripTest::RunTest(const FString& Parameters)
{
    TestEqual(TEXT("Save schema is version 5"), FExpeditionSave::Version, 5);
    TestEqual(TEXT("New save data defaults to version 5"), FExpeditionSaveData().Version, 5);
    FExpeditionRun Run, Restored;
    FString Error, Json;
    FExpeditionSnapshot Snapshot = ActiveRun(Run);
    FirstParty(Snapshot).Conditions = {Cond(ECombatCondition::Poisoned, INDEX_NONE, 2), Cond(ECombatCondition::Prone)};
    if (!TestTrue(TEXT("Conditions restore"), Restored.Restore(Snapshot, Error))) { AddError(Error); return false; }
    FExpeditionSaveData Data; Data.Current = Restored.Snapshot();
    TestTrue(TEXT("Encodes"), FExpeditionSave::Encode(Data, Json, Error));
    FExpeditionSaveData Loaded;
    TestTrue(TEXT("Decodes"), FExpeditionSave::Decode(Json, Loaded, Error));
    TestTrue(TEXT("Conditions survive the JSON round trip"), Same(Loaded.Current, Data.Current));
    TestEqual(TEXT("Both conditions kept"), FirstParty(Loaded.Current).Conditions.Num(), 2);

    TSharedPtr<FJsonObject> Root;
    FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Root);
    Root->SetNumberField(TEXT("Version"), 4);
    FExpeditionSaveData Old;
    TestFalse(TEXT("Version 4 saves are rejected"), FExpeditionSave::Decode(Text(Root), Old, Error));
    TestTrue(TEXT("Rejected as unsupported"), Error.Contains(TEXT("unsupported")));
    Root->SetNumberField(TEXT("Version"), 5);
    FirstCombatantJson(Root)->RemoveField(TEXT("Conditions"));
    TestFalse(TEXT("A combatant without the schema 5 field is rejected"), FExpeditionSave::Decode(Text(Root), Old, Error));
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSchema5ConditionValidationTest, "Project.Expedition.Schema5.InvalidConditionsRejected",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSchema5ConditionValidationTest::RunTest(const FString& Parameters)
{
    FExpeditionRun Run;
    FString Error;
    const FExpeditionSnapshot Original = ActiveRun(Run);
    FExpeditionSnapshot Probe = Original;
    const FCombatantState& Hero = FirstParty(Probe);
    const FCombatantState* Distant = Original.Battle.Encounter.Combatants.FindByPredicate([&](const FCombatantState& U) { return U.Side == ECombatSide::Enemy && FMath::Abs(U.Cell.X - Hero.Cell.X) + FMath::Abs(U.Cell.Y - Hero.Cell.Y) > 1; });
    if (!TestNotNull(TEXT("Fixture has a distant enemy"), Distant)) { return false; }
    const int32 HeroId = Hero.Id, FarId = Distant->Id;
    TArray<TArray<FCombatCondition>> Bad = {
        {Cond(ECombatCondition::Poisoned), Cond(ECombatCondition::Poisoned)},
        {Cond(ECombatCondition::Poisoned, 999)},
        {Cond(ECombatCondition::Poisoned, HeroId)},
        {Cond(ECombatCondition::Poisoned, INDEX_NONE, 0)},
        {Cond(ECombatCondition::Poisoned, INDEX_NONE, 11)},
        {Cond(ECombatCondition::Grappled)},
        {Cond(ECombatCondition::Grappled, FarId)},
        {Cond(static_cast<ECombatCondition>(200))}};
    for (const auto& Conditions : Bad)
    {
        FExpeditionSnapshot Corrupt = Original;
        FirstParty(Corrupt).Conditions = Conditions;
        TestFalse(TEXT("Invalid condition rejected"), Run.Restore(Corrupt, Error));
        TestFalse(TEXT("Clear reason"), Error.IsEmpty());
        TestTrue(TEXT("No partial mutation"), Same(Original, Run.Snapshot()));
    }
    FExpeditionSnapshot Dead = Original;
    FCombatantState& Enemy = *Dead.Battle.Encounter.Combatants.FindByPredicate([FarId](const FCombatantState& U) { return U.Id == FarId; });
    Enemy.Conditions = {Cond(ECombatCondition::Poisoned)};
    TestTrue(TEXT("A living enemy may be poisoned"), Run.Restore(Dead, Error));
    TestTrue(TEXT("Restore back"), Run.Restore(Original, Error));
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSchema5ConditionEffectsTest, "Project.Expedition.Schema5.ConditionsShapeAttacksAndMovement",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSchema5ConditionEffectsTest::RunTest(const FString& Parameters)
{
    FString Error;
    auto ModeWith = [&](FIntPoint EnemyCell, int32 AllyRange, TArray<FCombatCondition> AllyConditions, TArray<FCombatCondition> EnemyConditions)
    {
        FCombatEncounter Encounter = Pair(EnemyCell);
        Encounter.Combatants[0].Range = AllyRange;
        Encounter.Combatants[0].Conditions = AllyConditions;
        Encounter.Combatants[1].Conditions = EnemyConditions;
        FCombatBattle Battle;
        if (!Battle.Start(Encounter, 7, Error)) { AddError(Error); return ERollMode::Normal; }
        return Battle.AttackRollMode(10, 20).Mode;
    };
    TestEqual(TEXT("Poisoned attacker has disadvantage"), ModeWith({1, 0}, 1, {Cond(ECombatCondition::Poisoned)}, {}), ERollMode::Disadvantage);
    TestEqual(TEXT("Blinded attacker has disadvantage"), ModeWith({1, 0}, 1, {Cond(ECombatCondition::Blinded)}, {}), ERollMode::Disadvantage);
    TestEqual(TEXT("Frightened attacker with its source in sight has disadvantage"), ModeWith({1, 0}, 1, {Cond(ECombatCondition::Frightened, 20)}, {}), ERollMode::Disadvantage);
    TestEqual(TEXT("Restrained target grants advantage"), ModeWith({1, 0}, 1, {}, {Cond(ECombatCondition::Restrained)}), ERollMode::Advantage);
    TestEqual(TEXT("Adjacent prone target grants advantage"), ModeWith({1, 0}, 1, {}, {Cond(ECombatCondition::Prone)}), ERollMode::Advantage);
    TestEqual(TEXT("Distant prone target imposes disadvantage"), ModeWith({3, 0}, 4, {}, {Cond(ECombatCondition::Prone)}), ERollMode::Disadvantage);
    TestEqual(TEXT("Advantage and disadvantage cancel"), ModeWith({1, 0}, 1, {Cond(ECombatCondition::Poisoned)}, {Cond(ECombatCondition::Restrained)}), ERollMode::Normal);

    {
        FCombatEncounter Encounter = Pair({1, 0});
        Encounter.Combatants[0].Conditions = {Cond(ECombatCondition::Grappled, 20)};
        FCombatBattle Battle;
        TestTrue(TEXT("Adjacent grapple is valid"), Battle.Start(Encounter, 7, Error));
        TestEqual(TEXT("Grappled creature has no movement"), Battle.Snapshot().MovementLeft, 0);
        TestTrue(TEXT("Grappled creature has no legal moves"), Battle.LegalMoves(10).IsEmpty());
        TestTrue(TEXT("Ends turn"), Battle.Submit({ECombatCommandType::EndTurn, 10}).bAccepted);
        TestTrue(TEXT("Grappler steps away"), Battle.Submit({ECombatCommandType::Move, 20, {4, 0}}).bAccepted);
        TestFalse(TEXT("Grapple ends when the grappler leaves reach"), Has(Battle.Find(10), ECombatCondition::Grappled));
    }
    {
        FCombatEncounter Encounter = Pair({6, 6});
        Encounter.Combatants[0].Conditions = {Cond(ECombatCondition::Prone)};
        FCombatBattle Battle;
        TestTrue(TEXT("Starts prone"), Battle.Start(Encounter, 7, Error));
        TestFalse(TEXT("Stands up at the start of its turn"), Has(Battle.Find(10), ECombatCondition::Prone));
        TestEqual(TEXT("Standing costs half speed"), Battle.Snapshot().MovementLeft, Battle.Find(10)->Speed - Battle.Find(10)->Speed / 2);
    }
    {
        FCombatEncounter Encounter = Pair({6, 6});
        Encounter.Combatants[0].Conditions = {Cond(ECombatCondition::Poisoned, INDEX_NONE, 1), Cond(ECombatCondition::Blinded, INDEX_NONE, 2)};
        FCombatBattle Battle;
        TestTrue(TEXT("Starts"), Battle.Start(Encounter, 7, Error));
        TestTrue(TEXT("Timed condition active during the turn"), Has(Battle.Find(10), ECombatCondition::Poisoned));
        TestTrue(TEXT("Ends turn"), Battle.Submit({ECombatCommandType::EndTurn, 10}).bAccepted);
        TestFalse(TEXT("Last round expires at the end of the owner's turn"), Has(Battle.Find(10), ECombatCondition::Poisoned));
        const FCombatCondition* Blind = Battle.Find(10)->Conditions.FindByPredicate([](const FCombatCondition& C) { return C.Condition == ECombatCondition::Blinded; });
        TestTrue(TEXT("Longer condition counts down"), Blind && Blind->RoundsLeft == 1);
    }
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSchema5OpportunityAttackTest, "Project.Expedition.Schema5.LeavingReachProvokesOpportunityAttack",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSchema5OpportunityAttackTest::RunTest(const FString& Parameters)
{
    FString Error;
    {
        FCombatBattle Battle;
        TestTrue(TEXT("Starts adjacent"), Battle.Start(Pair({1, 0}), 7, Error));
        TestTrue(TEXT("Everyone starts with a reaction"), Battle.Find(20)->bReactionAvailable);
        const FCombatResult Away = Battle.Submit({ECombatCommandType::Move, 10, {0, 3}});
        TestTrue(TEXT("Move accepted"), Away.bAccepted);
        if (TestEqual(TEXT("Leaving reach provokes once"), Away.Reactions.Num(), 1))
        {
            TestEqual(TEXT("The adjacent melee enemy reacts"), Away.Reactions[0].ReactorId, 20);
            TestEqual(TEXT("Struck while leaving its last reach cell"), Away.Reactions[0].Cell, FIntPoint(0, 1));
        }
        TestFalse(TEXT("The reaction is spent"), Battle.Find(20)->bReactionAvailable);
        if (Battle.Find(10)->Life == ECombatLife::Conscious)
        {
            TestTrue(TEXT("Ends turn"), Battle.Submit({ECombatCommandType::EndTurn, 10}).bAccepted);
            TestTrue(TEXT("Reaction returns at the reactor's turn start"), Battle.Find(20)->bReactionAvailable);
        }
    }
    {
        FCombatEncounter Encounter = Pair({1, 0});
        Encounter.Combatants[1].Range = 4;
        FCombatBattle Battle;
        TestTrue(TEXT("Starts"), Battle.Start(Encounter, 7, Error));
        const FCombatResult Away = Battle.Submit({ECombatCommandType::Move, 10, {0, 3}});
        TestTrue(TEXT("Ranged enemies make no opportunity attack"), Away.bAccepted && Away.Reactions.IsEmpty());
        TestTrue(TEXT("Its reaction is kept"), Battle.Find(20)->bReactionAvailable);
    }
    {
        FCombatEncounter Encounter = Pair({1, 0});
        Encounter.Combatants[0].HP = 1;
        Encounter.Combatants[0].ArmorClass = 1;
        Encounter.Combatants[1].AttackBonus = 30;
        FCombatBattle Battle;
        TestTrue(TEXT("Starts"), Battle.Start(Encounter, 7, Error));
        const FCombatResult Away = Battle.Submit({ECombatCommandType::Move, 10, {0, 3}});
        TestTrue(TEXT("Move accepted"), Away.bAccepted);
        if (Away.Reactions.Num() == 1 && Away.Reactions[0].bHit)
        {
            TestEqual(TEXT("A dropped mover stops where it was struck"), Battle.Find(10)->Cell, FIntPoint(0, 1));
            TestEqual(TEXT("Path ends there"), Away.Path.Last(), FIntPoint(0, 1));
            TestTrue(TEXT("Reported as dropped (a lone party member's defeat is final)"), Away.Downed.Contains(10) || Away.Died.Contains(10));
            TestEqual(TEXT("Dropping the last hero ends the battle"), Battle.Snapshot().Phase, EBattlePhase::Defeat);
        }
        else { AddError(TEXT("Fixture seed must produce a hit")); }
    }
    {
        FExpeditionRun Run, Restored;
        FExpeditionSnapshot Snapshot = ActiveRun(Run);
        FirstParty(Snapshot).bReactionAvailable = false;
        TestTrue(TEXT("A spent reaction restores mid-battle"), Restored.Restore(Snapshot, Error));
        TestTrue(TEXT("And is retained"), Same(Snapshot, Restored.Snapshot()));
    }
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSchema5ExtraAttackTest, "Project.Expedition.Schema5.ExtraAttackSpendsPersistedAttacks",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSchema5ExtraAttackTest::RunTest(const FString& Parameters)
{
    FString Error;
    auto FighterAt = [](int32 Level)
    {
        FCombatEncounter Encounter = Pair({1, 0});
        Encounter.Combatants[0].AdventurerRole = EAdventurerRole::Fighter;
        Encounter.Combatants[0].Level = Level;
        Encounter.Combatants[1].MaxHP = Encounter.Combatants[1].HP = 500;
        return Encounter;
    };
    {
        FCombatBattle Battle;
        TestTrue(TEXT("Level 5 fighter starts"), Battle.Start(FighterAt(5), 7, Error));
        TestEqual(TEXT("No attacks banked before the Attack action"), Battle.Snapshot().AttacksRemaining, 0);
        TestTrue(TEXT("First attack"), Battle.Submit({ECombatCommandType::Attack, 10, {}, 20}).bAccepted);
        TestFalse(TEXT("The Attack action is spent"), Battle.Snapshot().bActionAvailable);
        TestEqual(TEXT("One extra attack remains"), Battle.Snapshot().AttacksRemaining, 1);
        TestTrue(TEXT("Still has a legal target"), Battle.LegalTargets(10).Contains(20));
        const FCombatSnapshot Mid = Battle.Snapshot();
        FCombatBattle Copy;
        TestTrue(TEXT("Mid-action state restores"), Copy.Restore(Mid, Error));
        FCombatSnapshot Bad = Mid; Bad.AttacksRemaining = 2;
        TestFalse(TEXT("More attacks than Extra Attack grants rejected"), Copy.Restore(Bad, Error));
        Bad = Mid; Bad.bActionAvailable = true;
        TestFalse(TEXT("Banked attacks with an unspent action rejected"), Copy.Restore(Bad, Error));
        TestTrue(TEXT("Second attack"), Battle.Submit({ECombatCommandType::Attack, 10, {}, 20}).bAccepted);
        TestEqual(TEXT("All attacks used"), Battle.Snapshot().AttacksRemaining, 0);
        TestFalse(TEXT("No third attack"), Battle.Submit({ECombatCommandType::Attack, 10, {}, 20}).bAccepted);
        TestTrue(TEXT("Ends turn"), Battle.Submit({ECombatCommandType::EndTurn, 10}).bAccepted);
        TestEqual(TEXT("Reset at the turn boundary"), Battle.Snapshot().AttacksRemaining, 0);
    }
    {
        FCombatBattle Battle;
        TestTrue(TEXT("Level 4 fighter starts"), Battle.Start(FighterAt(4), 7, Error));
        TestTrue(TEXT("Attack"), Battle.Submit({ECombatCommandType::Attack, 10, {}, 20}).bAccepted);
        TestEqual(TEXT("No extra attack before level 5"), Battle.Snapshot().AttacksRemaining, 0);
        TestFalse(TEXT("Second attack refused"), Battle.Submit({ECombatCommandType::Attack, 10, {}, 20}).bAccepted);
    }
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSchema5DifficultTerrainTest, "Project.Expedition.Schema5.DifficultTerrainCostsDoubleMovement",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSchema5DifficultTerrainTest::RunTest(const FString& Parameters)
{
    FString Error;
    {
        FCombatEncounter Encounter = Pair({7, 7});
        Encounter.DifficultCells = {{1, 0}, {1, 1}, {0, 1}};
        FCombatBattle Battle;
        TestTrue(TEXT("Starts surrounded by rubble"), Battle.Start(Encounter, 7, Error));
        TestTrue(TEXT("Three cells cost 2+1"), Battle.LegalMoves(10).Contains(FIntPoint(2, 0)));
        TestFalse(TEXT("Four cells would cost 5"), Battle.LegalMoves(10).Contains(FIntPoint(4, 0)));
        TestTrue(TEXT("Move through rubble"), Battle.Submit({ECombatCommandType::Move, 10, {2, 0}}).bAccepted);
        TestEqual(TEXT("Entering difficult terrain spends two"), Battle.Snapshot().MovementLeft, 1);
    }
    {
        FCombatEncounter Encounter = Pair({7, 7});
        Encounter.DifficultCells = {{1, 0}};
        FCombatBattle Battle;
        TestTrue(TEXT("Starts"), Battle.Start(Encounter, 7, Error));
        const FCombatResult Moved = Battle.Submit({ECombatCommandType::Move, 10, {1, 1}});
        TestTrue(TEXT("Moves"), Moved.bAccepted);
        TestEqual(TEXT("Cheapest route steps around the rubble, not the first-listed neighbour"), Moved.Path[0], FIntPoint(0, 1));
        TestEqual(TEXT("Clear route costs two"), Battle.Snapshot().MovementLeft, 2);
    }
    {
        FCombatEncounter Plain = Pair({7, 7});
        FCombatBattle Battle;
        TestTrue(TEXT("Starts"), Battle.Start(Plain, 7, Error));
        TestTrue(TEXT("Without terrain four cells are reachable"), Battle.LegalMoves(10).Contains(FIntPoint(4, 0)));
    }
    TArray<TArray<FIntPoint>> Bad = {{{8, 0}}, {{2, 2}, {2, 2}}, {{3, 3}}};
    for (const auto& Cells : Bad)
    {
        FCombatEncounter Encounter = Pair({7, 7});
        Encounter.BlockedCells = {{3, 3}};
        Encounter.DifficultCells = Cells;
        FCombatBattle Battle;
        TestFalse(TEXT("Outside, duplicate or blocked difficult cells rejected"), Battle.Start(Encounter, 7, Error));
    }
    {
        FExpeditionRun Run; FString Json;
        ActiveRun(Run);
        FExpeditionSaveData Data; Data.Current = Run.Snapshot();
        FExpeditionSave::Encode(Data, Json, Error);
        TSharedPtr<FJsonObject> Root;
        FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Root);
        Root->GetObjectField(TEXT("Current"))->GetObjectField(TEXT("Battle"))->GetObjectField(TEXT("Encounter"))->RemoveField(TEXT("DifficultCells"));
        FExpeditionSaveData Loaded;
        TestFalse(TEXT("Terrain is a required schema 5 field"), FExpeditionSave::Decode(Text(Root), Loaded, Error));
    }
    return !HasAnyErrors();
}
#endif