#if WITH_DEV_AUTOMATION_TESTS
#include "Rules/GrappleRules.h"
#include "Misc/AutomationTest.h"
#include "Rules/MonsterRules.h"
#include "Combat/CombatBattle.h"
#include "Run/ExpeditionRun.h"
#include "Persistence/ExpeditionSave.h"
#include "Rules/WeaponRules.h"
#include "Rules/ArmorRules.h"
#include "Rules/ClassFeatureRules.h"
#include "Party/BuildCatalog.h"
#include "Party/FeatureKit.h"
#include "ExpeditionTestSupport.h"
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
    TestEqual(TEXT("Save schema is version 6 (hex grid)"), FExpeditionSave::Version, 6);
    TestEqual(TEXT("New save data defaults to version 6"), FExpeditionSaveData().Version, 6);
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
    TestFalse(TEXT("Square-grid version 5 saves are rejected"), FExpeditionSave::Decode(Text(Root), Old, Error));
    TestTrue(TEXT("Rejection explains the hex grid change"), Error.Contains(TEXT("unsupported")) && Error.Contains(TEXT("hex grid")));
    Root->SetNumberField(TEXT("Version"), 6);
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSchema5SlotPoolTest, "Project.Expedition.Schema5.PerLevelSlotPoolFollowsTable",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSchema5SlotPoolTest::RunTest(const FString& Parameters)
{
    FString Error;
    auto Caster = [](int32 Level, int32 Slots, TArray<int32> Pool)
    {
        FCombatEncounter Encounter = Pair({7, 7});
        FCombatantState& Unit = Encounter.Combatants[0];
        Unit.AdventurerRole = EAdventurerRole::Wizard; Unit.Level = Level; Unit.SpellSlots = Slots; Unit.SlotsByLevel = Pool;
        return Encounter;
    };
    FCombatBattle Battle;
    TestTrue(TEXT("An empty pool keeps the single counter"), Battle.Start(Caster(1, 2, {}), 7, Error));
    TestTrue(TEXT("Level 1 table: 2/0/0"), Battle.Start(Caster(1, 2, {2, 0, 0}), 7, Error));
    TestTrue(TEXT("Level 5 table: up to 4/3/2"), Battle.Start(Caster(5, 2, {2, 3, 2}), 7, Error));
    TestTrue(TEXT("Spent slots are fine"), Battle.Start(Caster(3, 0, {0, 1, 0}), 7, Error));
    TestFalse(TEXT("No 2nd-level slots at level 1"), Battle.Start(Caster(1, 2, {2, 1, 0}), 7, Error));
    TestFalse(TEXT("No 3rd-level slots at level 4"), Battle.Start(Caster(4, 2, {2, 3, 1}), 7, Error));
    TestFalse(TEXT("One entry per spell level"), Battle.Start(Caster(1, 2, {2, 0}), 7, Error));
    TestFalse(TEXT("Level 1 mirrors SpellSlots"), Battle.Start(Caster(1, 2, {1, 0, 0}), 7, Error));
    TestFalse(TEXT("No negative counts"), Battle.Start(Caster(5, 2, {2, -1, 0}), 7, Error));
    {
        FCombatEncounter Fighter = Caster(5, 0, {0, 1, 0});
        Fighter.Combatants[0].AdventurerRole = EAdventurerRole::Fighter;
        TestFalse(TEXT("Non-casters have no slots"), Battle.Start(Fighter, 7, Error));
    }
    FCombatantState Unit; Unit.SpellSlots = 2; Unit.SlotsByLevel = {2, 3, 0};
    FCombatBattle::SpendSpellSlot(Unit);
    TestEqual(TEXT("Spending lowers the counter"), Unit.SpellSlots, 1);
    TestEqual(TEXT("and the 1st-level pool"), Unit.SlotsByLevel[0], 1);
    TestEqual(TEXT("Higher levels untouched"), Unit.SlotsByLevel[1], 3);
    Unit.SpellSlots = Unit.SlotsByLevel[0] = 0;
    FCombatBattle::SpendSpellSlot(Unit);
    TestEqual(TEXT("Never below zero"), Unit.SlotsByLevel[0], 0);

    FExpeditionRun Run, Restored; FString Json;
    FExpeditionSnapshot Snapshot = ActiveRun(Run);
    FCombatantState* Wizard = Snapshot.Battle.Encounter.Combatants.FindByPredicate([](const FCombatantState& U) { return U.Side == ECombatSide::Party && U.SpellSlots > 0; });
    if (!TestNotNull(TEXT("Fixture has a caster"), Wizard)) { return false; }
    Wizard->SlotsByLevel = {Wizard->SpellSlots, 0, 0};
    if (!TestTrue(TEXT("Pool restores"), Restored.Restore(Snapshot, Error))) { AddError(Error); return false; }
    FExpeditionSaveData Data; Data.Current = Restored.Snapshot();
    TestTrue(TEXT("Encodes"), FExpeditionSave::Encode(Data, Json, Error));
    FExpeditionSaveData Loaded;
    TestTrue(TEXT("Decodes"), FExpeditionSave::Decode(Json, Loaded, Error));
    TestTrue(TEXT("Pool survives the JSON round trip"), Same(Loaded.Current, Data.Current));
    TSharedPtr<FJsonObject> Root;
    FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Root);
    FirstCombatantJson(Root)->RemoveField(TEXT("SlotsByLevel"));
    TestFalse(TEXT("The pool is a required schema 5 field"), FExpeditionSave::Decode(Text(Root), Loaded, Error));
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSchema5EquipmentTest, "Project.Expedition.Schema5.EquipmentDrivesDamageAndArmorClass",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSchema5EquipmentTest::RunTest(const FString& Parameters)
{
    FString Error;
    auto Dice = [](int32 Weapon, bool bShield)
    {
        FCombatantState Unit; Unit.MainHand = Weapon; Unit.bShieldEquipped = bShield;
        int32 Count = 0, Sides = 0; FCombatBattle::WeaponDice(Unit, Count, Sides); return FIntPoint(Count, Sides);
    };
    TestEqual(TEXT("No weapon keeps the fixed die"), Dice(-1, false), FIntPoint(1, 6));
    TestEqual(TEXT("Greatsword 2d6"), Dice(static_cast<int32>(EWeapon::Greatsword), false), FIntPoint(2, 6));
    TestEqual(TEXT("Longsword two-handed 1d10"), Dice(static_cast<int32>(EWeapon::Longsword), false), FIntPoint(1, 10));
    TestEqual(TEXT("Longsword with a shield 1d8"), Dice(static_cast<int32>(EWeapon::Longsword), true), FIntPoint(1, 8));

    auto Armored = [](int32 Armor, bool bShield)
    {
        FCombatEncounter Encounter = Pair({1, 0});
        Encounter.Combatants[1].ArmorWorn = Armor; Encounter.Combatants[1].bShieldEquipped = bShield;
        Encounter.Combatants[1].MaxHP = Encounter.Combatants[1].HP = 500;
        return Encounter;
    };
    {
        FCombatBattle Battle;
        TestTrue(TEXT("Plate and shield start"), Battle.Start(Armored(static_cast<int32>(EArmor::Plate), true), 7, Error));
        TestEqual(TEXT("Plate + shield is AC 20"), Battle.EffectiveArmorClass(20), 20);
        TestEqual(TEXT("Forecast uses worn armor"), Battle.Forecast(10, ECombatCommandType::Attack, 20).TargetArmorClass, 20);
        const FCombatResult Swing = Battle.Submit({ECombatCommandType::Attack, 10, {}, 20});
        TestTrue(TEXT("Attack resolves"), Swing.bAccepted);
        TestEqual(TEXT("Attack rolls against worn armor"), Swing.TargetArmorClass, 20);
    }
    {
        FCombatBattle Battle;
        TestTrue(TEXT("Leather starts"), Battle.Start(Armored(static_cast<int32>(EArmor::Leather), false), 7, Error));
        TestEqual(TEXT("Leather adds Dexterity"), Battle.EffectiveArmorClass(20), FArmorRules::ArmorClass(EArmor::Leather, false, Battle.AbilityScores(20).Get(EAbility::Dexterity)));
        TestTrue(TEXT("Legacy shield"), Battle.Start(Armored(-1, true), 7, Error));
        TestEqual(TEXT("Legacy AC plus shield"), Battle.EffectiveArmorClass(20), 14);
    }
    {
        FCombatEncounter Encounter = Pair({1, 0});
        Encounter.Combatants[0].MainHand = static_cast<int32>(EWeapon::Greataxe);
        Encounter.Combatants[1].MaxHP = Encounter.Combatants[1].HP = 500;
        bool bChecked = false;
        for (int32 Seed = 1; Seed < 60 && !bChecked; ++Seed)
        {
            FCombatBattle Battle;
            if (!Battle.Start(Encounter, Seed, Error)) { AddError(Error); break; }
            const FCombatResult Swing = Battle.Submit({ECombatCommandType::Attack, 10, {}, 20});
            if (!Swing.bHit) { continue; }
            bChecked = true;
            TestEqual(TEXT("Greataxe rolls d12s"), Swing.EffectDice[0].Sides, 12);
            TestEqual(TEXT("One die, two on a critical"), Swing.EffectDice.Num(), Swing.bCritical ? 2 : 1);
        }
        TestTrue(TEXT("Some seed hits"), bChecked);
    }
    TArray<TTuple<int32, int32, bool>> Bad = {
        {static_cast<int32>(EWeapon::Greatsword), -1, true},
        {static_cast<int32>(EWeapon::Count), -1, false},
        {-2, -1, false},
        {-1, static_cast<int32>(EArmor::Shield), false},
        {-1, static_cast<int32>(EArmor::Count), false}};
    for (const auto& Gear : Bad)
    {
        FCombatEncounter Encounter = Pair({7, 7});
        Encounter.Combatants[0].MainHand = Gear.Get<0>(); Encounter.Combatants[0].ArmorWorn = Gear.Get<1>(); Encounter.Combatants[0].bShieldEquipped = Gear.Get<2>();
        FCombatBattle Battle;
        TestFalse(TEXT("Unknown or unwieldable equipment rejected"), Battle.Start(Encounter, 7, Error));
    }
    {
        FExpeditionRun Run, Restored; FString Json;
        FExpeditionSnapshot Snapshot = ActiveRun(Run);
        FirstParty(Snapshot).MainHand = static_cast<int32>(EWeapon::Longsword);
        FirstParty(Snapshot).ArmorWorn = static_cast<int32>(EArmor::ChainMail);
        FirstParty(Snapshot).bShieldEquipped = true;
        if (!TestTrue(TEXT("Equipment restores"), Restored.Restore(Snapshot, Error))) { AddError(Error); return false; }
        FExpeditionSaveData Data; Data.Current = Restored.Snapshot();
        TestTrue(TEXT("Encodes"), FExpeditionSave::Encode(Data, Json, Error));
        FExpeditionSaveData Loaded;
        TestTrue(TEXT("Decodes"), FExpeditionSave::Decode(Json, Loaded, Error));
        TestTrue(TEXT("Equipment survives the JSON round trip"), Same(Loaded.Current, Data.Current));
        for (const TCHAR* Field : {TEXT("MainHand"), TEXT("ArmorWorn"), TEXT("bShieldEquipped")})
        {
            TSharedPtr<FJsonObject> Root;
            FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Root);
            FirstCombatantJson(Root)->RemoveField(Field);
            TestFalse(TEXT("Equipment fields are required"), FExpeditionSave::Decode(Text(Root), Loaded, Error));
        }
    }
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSchema5WeaponArmorTest, "Project.Expedition.Schema5.WeaponPropertiesAndArmorPenalties",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSchema5WeaponArmorTest::RunTest(const FString& Parameters)
{
    FString Error;
    auto Armed = [](FIntPoint EnemyCell, EWeapon Weapon, int32 Ammo)
    {
        FCombatEncounter Encounter = Pair(EnemyCell);
        Encounter.Combatants[0].MainHand = static_cast<int32>(Weapon); Encounter.Combatants[0].Ammunition = Ammo;
        Encounter.Combatants[1].MaxHP = Encounter.Combatants[1].HP = 500;
        return Encounter;
    };
    {
        FCombatantState Unit; Unit.Range = 3;
        TestEqual(TEXT("Legacy range is the unit value"), FCombatBattle::ActionRange(Unit, ECombatCommandType::Attack), 3);
        Unit.MainHand = static_cast<int32>(EWeapon::Glaive);
        TestEqual(TEXT("Reach weapon reaches two cells"), FCombatBattle::ActionRange(Unit, ECombatCommandType::Attack), 2);
        Unit.MainHand = static_cast<int32>(EWeapon::Longsword);
        TestEqual(TEXT("Melee weapon reaches one cell"), FCombatBattle::ActionRange(Unit, ECombatCommandType::Attack), 1);
        Unit.MainHand = static_cast<int32>(EWeapon::HandCrossbow);
        TestEqual(TEXT("Ranged weapon reaches its long range"), FCombatBattle::ActionRange(Unit, ECombatCommandType::Attack), 24);
    }
    {
        FCombatBattle Battle;
        TestTrue(TEXT("Glaive starts"), Battle.Start(Armed({2, 0}, EWeapon::Glaive, 0), 7, Error));
        TestTrue(TEXT("Glaive reaches two cells"), Battle.LegalTargets(10).Contains(20));
    }
    {
        FCombatBattle Battle;
        TestTrue(TEXT("Hand crossbow starts"), Battle.Start(Armed({0, 7}, EWeapon::HandCrossbow, 1), 7, Error));
        TestTrue(TEXT("Long range is legal"), Battle.LegalTargets(10).Contains(20));
        TestTrue(TEXT("Long range imposes disadvantage"), Battle.AttackRollMode(10, 20).Disadvantage.Contains(TEXT("long range")));
        TestTrue(TEXT("Shot resolves"), Battle.Submit({ECombatCommandType::Attack, 10, {}, 20}).bAccepted);
        TestEqual(TEXT("Shot spends ammunition"), Battle.Find(10)->Ammunition, 0);
    }
    {
        FCombatBattle Battle;
        TestTrue(TEXT("Empty quiver starts"), Battle.Start(Armed({0, 3}, EWeapon::Shortbow, 0), 7, Error));
        TestEqual(TEXT("No ammunition, no shot"), Battle.LegalTargets(10).Num(), 0);
        TestFalse(TEXT("Normal range has no long-range disadvantage"), Battle.AttackRollMode(10, 20).Disadvantage.Contains(TEXT("long range")));
    }
    {
        FCombatEncounter Encounter = Armed({1, 0}, EWeapon::Rapier, 0);
        Encounter.Combatants[0].AdventurerRole = EAdventurerRole::Rogue;
        FCombatBattle Battle;
        TestTrue(TEXT("Rogue starts"), Battle.Start(Encounter, 7, Error));
        const FCombatResult Swing = Battle.Submit({ECombatCommandType::Attack, 10, {}, 20});
        TestEqual(TEXT("Finesse attacks with Dexterity"), Swing.AttackBonus, 2 + 3);
    }
    {
        FCombatEncounter Encounter = Armed({1, 0}, EWeapon::Longsword, 0);
        Encounter.Combatants[0].AdventurerRole = EAdventurerRole::Wizard;
        Encounter.Combatants[0].ArmorWorn = static_cast<int32>(EArmor::Plate);
        Encounter.Combatants[0].Speed = 6;
        FCombatBattle Battle;
        TestTrue(TEXT("Wizard in plate starts"), Battle.Start(Encounter, 7, Error));
        TestEqual(TEXT("Strength under the requirement costs 10 ft"), Battle.Snapshot().MovementLeft, 4);
        TestTrue(TEXT("Unproficient armor imposes disadvantage"), Battle.AttackRollMode(10, 20).Disadvantage.Contains(TEXT("unproficient armor")));
        Encounter.Combatants[0].AdventurerRole = EAdventurerRole::Fighter;
        TestTrue(TEXT("Fighter in plate starts"), Battle.Start(Encounter, 7, Error));
        TestEqual(TEXT("Proficient and strong enough"), Battle.Snapshot().MovementLeft, 6);
        TestEqual(TEXT("No armor disadvantage"), Battle.AttackRollMode(10, 20).Disadvantage.Num(), 0);
    }
    {
        FCombatEncounter Encounter = Pair({2, 0});
        Encounter.Combatants[1].MainHand = static_cast<int32>(EWeapon::Glaive);
        FCombatBattle Battle;
        TestTrue(TEXT("Reach reactor starts"), Battle.Start(Encounter, 7, Error));
        const FCombatResult Step = Battle.Submit({ECombatCommandType::Move, 10, {0, 3}, INDEX_NONE});
        TestTrue(TEXT("Move resolves"), Step.bAccepted);
        TestEqual(TEXT("Leaving a reach weapon's reach provokes"), Step.Reactions.Num(), 1);
    }
    {
        FCombatEncounter Encounter = Pair({7, 7});
        Encounter.Combatants[0].Ammunition = -1;
        FCombatBattle Battle;
        TestFalse(TEXT("Negative ammunition rejected"), Battle.Start(Encounter, 7, Error));
        FExpeditionRun Run; FString Json;
        FExpeditionSnapshot Snapshot = ActiveRun(Run);
        FExpeditionSaveData Data; Data.Current = Snapshot;
        TestTrue(TEXT("Encodes"), FExpeditionSave::Encode(Data, Json, Error));
        TSharedPtr<FJsonObject> Root;
        FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Root);
        FirstCombatantJson(Root)->RemoveField(TEXT("Ammunition"));
        FExpeditionSaveData Loaded;
        TestFalse(TEXT("Ammunition is a required field"), FExpeditionSave::Decode(Text(Root), Loaded, Error));
    }
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSchema5ExtraAttackCatalogSneakTest, "Project.Expedition.Schema5.CatalogExtraAttackAndSneakOncePerTurn",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSchema5ExtraAttackCatalogSneakTest::RunTest(const FString& Parameters)
{
    FString Error;
    TestEqual(TEXT("Barbarian 5 Extra Attack"), FClassFeatureRules::AttacksPerAction(EAdventurerRole::Barbarian, 5), 2);
    TestEqual(TEXT("Paladin 4 one attack"), FClassFeatureRules::AttacksPerAction(EAdventurerRole::Paladin, 4), 1);
    TestEqual(TEXT("Ranger 11 still two"), FClassFeatureRules::AttacksPerAction(EAdventurerRole::Ranger, 11), 2);
    {
        FCombatEncounter Encounter = Pair({2, 0});
        FCombatantState Beast;
        TestTrue(TEXT("Beast build"), FBuildCatalog::Create(TEXT("barbarian.beast"), 10, TEXT("Claw"), Beast));
        Beast.Cell = {0, 0}; Beast.InitiativeBonus = 30; Beast.Level = 5;
        Encounter.Combatants[0] = Beast;
        Encounter.Combatants[1].MaxHP = Encounter.Combatants[1].HP = 500;
        FCombatBattle Battle;
        if (!TestTrue(TEXT("Beast starts"), Battle.Start(Encounter, 7, Error))) { AddError(Error); return false; }
        TestTrue(TEXT("Rages first"), Battle.Submit({ECombatCommandType::Special1, 10, {}, 10}).bAccepted);
        TestTrue(TEXT("Long Claw resolves"), Battle.Submit({ECombatCommandType::Special2, 10, {}, 20}).bAccepted);
        TestEqual(TEXT("An action strike banks Extra Attack"), Battle.Snapshot().AttacksRemaining, 1);
        TestTrue(TEXT("Moves into reach"), Battle.Submit({ECombatCommandType::Move, 10, {1, 0}}).bAccepted);
        TestTrue(TEXT("Extra weapon attack"), Battle.Submit({ECombatCommandType::Attack, 10, {}, 20}).bAccepted);
        TestEqual(TEXT("Extra attack spent"), Battle.Snapshot().AttacksRemaining, 0);
    }
    {
        FCombatEncounter Encounter = Pair({1, 0});
        Encounter.Combatants[0].AdventurerRole = EAdventurerRole::Rogue; Encounter.Combatants[0].bSneakWeapon = true;
        Encounter.Combatants[1].MaxHP = Encounter.Combatants[1].HP = 500;
        FCombatantState Friend = Encounter.Combatants[0];
        Friend.Id = 11; Friend.Name = TEXT("Friend"); Friend.Cell = {1, 1}; Friend.AdventurerRole = EAdventurerRole::None; Friend.bSneakWeapon = false; Friend.InitiativeBonus = 0;
        Encounter.Combatants.Add(Friend);
        FCombatBattle Battle;
        TestTrue(TEXT("Rogue starts"), Battle.Start(Encounter, 7, Error));
        TestTrue(TEXT("Sneak attack forecast"), Battle.Forecast(10, ECombatCommandType::Attack, 20).bSneakAttack);
        bool bSneaked = false;
        for (int32 Seed = 1; Seed < 80 && !bSneaked; ++Seed)
        {
            TestTrue(TEXT("Rogue restarts"), Battle.Start(Encounter, Seed, Error));
            bSneaked = Battle.Submit({ECombatCommandType::Attack, 10, {}, 20}).bSneakAttack;
        }
        if (TestTrue(TEXT("Some seed sneak attacks"), bSneaked))
        {
            TestTrue(TEXT("Sneak Attack is recorded for the turn"), Battle.Snapshot().SneakAttackUsedBy.Contains(10));
            TestFalse(TEXT("No second Sneak Attack this turn"), Battle.Forecast(10, ECombatCommandType::Attack, 20).bSneakAttack);
            FCombatSnapshot Bad = Battle.Snapshot(); Bad.SneakAttackUsedBy.Add(99);
            FCombatBattle Copy;
            TestTrue(TEXT("Used sneak restores"), Copy.Restore(Battle.Snapshot(), Error));
            TestFalse(TEXT("Unknown sneak user rejected"), Copy.Restore(Bad, Error));
            TestTrue(TEXT("Ends turn"), Battle.Submit({ECombatCommandType::EndTurn, 10}).bAccepted);
            TestEqual(TEXT("Cleared at the next turn"), Battle.Snapshot().SneakAttackUsedBy.Num(), 0);
        }
    }
    {
        FExpeditionRun Run; FString Json;
        FExpeditionSaveData Data; Data.Current = ActiveRun(Run);
        TestTrue(TEXT("Encodes"), FExpeditionSave::Encode(Data, Json, Error));
        TSharedPtr<FJsonObject> Root;
        FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Root);
        Root->GetObjectField(TEXT("Current"))->GetObjectField(TEXT("Battle"))->RemoveField(TEXT("SneakAttackUsedBy"));
        FExpeditionSaveData Loaded;
        TestFalse(TEXT("SneakAttackUsedBy is required"), FExpeditionSave::Decode(Text(Root), Loaded, Error));
    }
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSchema5UpcastAreaSpellsTest, "Project.Expedition.Schema5.UpcastAndAreaSpells",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSchema5UpcastAreaSpellsTest::RunTest(const FString& Parameters)
{
    FString Error;
    auto Wizard = [](FIntPoint EnemyCell)
    {
        FCombatEncounter Encounter = Pair(EnemyCell);
        FCombatantState& W = Encounter.Combatants[0];
        W.Name = TEXT("Wizard"); W.AdventurerRole = EAdventurerRole::Wizard; W.Level = 3; W.SpellSlots = 2; W.SlotsByLevel = {2, 2, 0};
        Encounter.Combatants[1].MaxHP = Encounter.Combatants[1].HP = 500;
        FCombatantState Far = Encounter.Combatants[1];
        Far.Id = 21; Far.Name = TEXT("Far"); Far.Cell = {6, 6};
        Encounter.Combatants.Add(Far);
        return Encounter;
    };
    {
        FCombatBattle Battle;
        if (!TestTrue(TEXT("Wizard starts"), Battle.Start(Wizard({2, 0}), 7, Error))) { AddError(Error); return false; }
        FCombatCommand Missile{ECombatCommandType::MagicMissile, 10, {}, 20}; Missile.SlotLevel = 3;
        TestFalse(TEXT("No 3rd-level slot"), Battle.Submit(Missile).bAccepted);
        Missile.SlotLevel = 2;
        const FCombatResult R = Battle.Submit(Missile);
        TestTrue(TEXT("Upcast Magic Missile"), R.bAccepted);
        TestEqual(TEXT("Four darts at 2nd level"), R.EffectDice.Num(), 4);
        TestEqual(TEXT("2nd-level slot spent"), Battle.Find(10)->SlotsByLevel[1], 1);
        TestEqual(TEXT("1st-level slots kept"), Battle.Find(10)->SpellSlots, 2);
    }
    {
        FCombatEncounter Encounter = Wizard({2, 0});
        Encounter.Combatants[0].SpellSlots = 0; Encounter.Combatants[0].SlotsByLevel = {0, 2, 0};
        FCombatBattle Battle;
        TestTrue(TEXT("Starts without 1st-level slots"), Battle.Start(Encounter, 7, Error));
        const FCombatResult R = Battle.Submit({ECombatCommandType::MagicMissile, 10, {}, 20});
        TestTrue(TEXT("Lowest open slot is used"), R.bAccepted);
        TestEqual(TEXT("Auto upcast to four darts"), R.EffectDice.Num(), 4);
        TestEqual(TEXT("Spent from level 2"), Battle.Find(10)->SlotsByLevel[1], 1);
    }
    {
        FCombatBattle Battle;
        TestTrue(TEXT("Burning Hands battle"), Battle.Start(Wizard({1, 0}), 7, Error));
        TestTrue(TEXT("Cone covers the adjacent foe"), Battle.LegalTargets(10, ECombatCommandType::BurningHands).Contains(20));
        TestFalse(TEXT("Far foe is outside the cone"), Battle.LegalTargets(10, ECombatCommandType::BurningHands).Contains(21));
        FCombatCommand Cast{ECombatCommandType::BurningHands, 10, {}, 20}; Cast.SlotLevel = 2;
        const FCombatResult R = Battle.Submit(Cast);
        TestTrue(TEXT("Burning Hands resolves"), R.bAccepted);
        TestEqual(TEXT("4d6 at 2nd level"), R.EffectDice.Num(), 4);
        TestEqual(TEXT("Fire"), R.DamageType, EDamageType::Fire);
        TestTrue(TEXT("Foe burned"), Battle.Find(20)->HP < 500);
        TestEqual(TEXT("Far foe untouched"), Battle.Find(21)->HP, 500);
        TestFalse(TEXT("Action spent"), Battle.Snapshot().bActionAvailable);
    }
    {
        bool bPushed = false;
        for (int32 Seed = 1; Seed < 60 && !bPushed; ++Seed)
        {
            FCombatBattle Battle;
            TestTrue(TEXT("Thunderwave battle"), Battle.Start(Wizard({1, 0}), Seed, Error));
            const FCombatResult R = Battle.Submit({ECombatCommandType::Thunderwave, 10, {}, 20});
            TestTrue(TEXT("Thunderwave resolves"), R.bAccepted);
            TestEqual(TEXT("2d8"), R.EffectDice.Num(), 2);
            TestTrue(TEXT("Foe hurt"), Battle.Find(20)->HP < 500);
            bPushed = Battle.Find(20)->Cell == FIntPoint(3, 0);
        }
        TestTrue(TEXT("A failed save pushes 10 feet"), bPushed);
    }
    {
        FCombatEncounter Encounter = Wizard({1, 0});
        Encounter.Combatants[0].AdventurerRole = EAdventurerRole::Cleric;
        FCombatBattle Battle;
        TestTrue(TEXT("Cleric battle"), Battle.Start(Encounter, 7, Error));
        TestEqual(TEXT("Cleric lacks Burning Hands"), Battle.LegalTargets(10, ECombatCommandType::BurningHands).Num(), 0);
        Encounter = Wizard({1, 0});
        Encounter.Combatants[0].ArmorWorn = static_cast<int32>(EArmor::Plate);
        TestTrue(TEXT("Armored wizard"), Battle.Start(Encounter, 7, Error));
        TestEqual(TEXT("No casting in unproficient armor"), Battle.LegalTargets(10, ECombatCommandType::Thunderwave).Num(), 0);
    }
    return !HasAnyErrors();
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSchema5HudFactsTest, "Project.Expedition.Schema5.HudTerrainConditionsThreatsArmor",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSchema5HudFactsTest::RunTest(const FString& Parameters)
{
    FString Error;
    FCombatEncounter Encounter = Pair({1, 0});
    Encounter.DifficultCells = {{3, 3}};
    Encounter.Combatants[0].Conditions = {Cond(ECombatCondition::Poisoned, INDEX_NONE, 2)};
    Encounter.Combatants[0].ArmorWorn = static_cast<int32>(EArmor::ChainMail);
    Encounter.Combatants[0].bShieldEquipped = true;
    FCombatBattle Battle;
    if (!TestTrue(TEXT("Starts"), Battle.Start(Encounter, 7, Error))) { AddError(Error); return false; }
    TestTrue(TEXT("Difficult cell flagged"), Battle.IsDifficultTerrain({3, 3}));
    TestFalse(TEXT("Open cell plain"), Battle.IsDifficultTerrain({2, 2}));
    TestEqual(TEXT("Condition name"), FString(FCombatBattle::ConditionName(ECombatCondition::Restrained)), FString(TEXT("Restrained")));
    TestTrue(TEXT("Status lists poison and rounds"), Battle.StatusText(10).Contains(TEXT("Poisoned (2 rounds)")));
    TestTrue(TEXT("No conditions, empty status"), Battle.StatusText(20).IsEmpty());
    const FString Armor = Battle.ArmorClassText(10);
    TestTrue(TEXT("AC text uses effective AC"), Armor.StartsWith(FString::Printf(TEXT("AC %d"), Battle.EffectiveArmorClass(10))));
    TestTrue(TEXT("AC names the armor"), Armor.Contains(TEXT("Chain Mail")) && Armor.Contains(TEXT("shield")));
    TestEqual(TEXT("Legacy AC text"), Battle.ArmorClassText(20), FString::Printf(TEXT("AC %d"), Battle.Find(20)->ArmorClass));
    TestEqual(TEXT("Staying in reach is safe"), Battle.OpportunityThreats(10, {{0, 1}}).Num(), 0);
    const TArray<int32> Threats = Battle.OpportunityThreats(10, {{0, 1}, {0, 2}});
    TestTrue(TEXT("Leaving reach warns"), Threats.Num() == 1 && Threats[0] == 20);
    TestTrue(TEXT("Preview spends no reaction"), Battle.Find(20)->bReactionAvailable);
    return !HasAnyErrors();
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSchema5GeneratedFactsTest, "Project.Expedition.Schema5.GenerationPlacesTerrainConditionsEquipment",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSchema5GeneratedFactsTest::RunTest(const FString& Parameters)
{
    bool bSawTerrain = false, bSawProne = false;
    for (int32 Seed = 0; Seed < 16; ++Seed)
    {
        for (int32 Choice = 0; Choice < 2; ++Choice)
        {
            FExpeditionRun Run; FString Error;
            Run.NewRun(Seed, Error, TEXT("generation-fixture"));
            const FExpeditionRoute Route = Run.Progress().Offers[Choice];
            if (!TestTrue(TEXT("Route starts"), Run.ChooseRoute(Route.Id, Error))) { return false; }
            const FCombatBattle& Battle = Run.Battle();
            const FCombatEncounter& Layout = Battle.Snapshot().Encounter;
            bSawTerrain |= !Layout.DifficultCells.IsEmpty();
            for (const FIntPoint& Cell : Layout.DifficultCells)
            {
                TestFalse(TEXT("Rough ground is never a wall"), Layout.BlockedCells.Contains(Cell));
                TestFalse(TEXT("Nobody spawns in rough ground"), Layout.Combatants.ContainsByPredicate([Cell](const FCombatantState& U) { return U.Cell == Cell; }));
            }
            for (const FCombatantState& Unit : Layout.Combatants)
            {
                if (Unit.Side != ECombatSide::Enemy) { continue; }
                // Deliberate change (SRD stat blocks): AC is the block's AC; every block but the Acolyte wears leather (11 + DEX).
                const FMonsterStatBlock* Block = FMonsterRules::Find(Unit.Role);
                if (!TestNotNull(TEXT("Every enemy uses an SRD stat block"), Block)) { continue; }
                TestEqual(TEXT("Leather only on leather blocks"), Unit.ArmorWorn > static_cast<int32>(EArmor::None), Block->bWearsLeather);
                TestEqual(TEXT("Effective AC equals the stat block AC"), Battle.EffectiveArmorClass(Unit.Id), Block->ArmorClass);
                if (Block->bWearsLeather) { TestTrue(TEXT("HUD names the armor"), Battle.ArmorClassText(Unit.Id).Contains(TEXT("Leather"))); }
                if (Has(&Unit, ECombatCondition::Prone)) { bSawProne = true; TestFalse(TEXT("Only quiet routes catch a scout resting"), Route.bDangerous); }
            }
        }
    }
    TestTrue(TEXT("Some layouts have difficult terrain"), bSawTerrain);
    TestTrue(TEXT("Some layouts start a scout prone"), bSawProne);
    return !HasAnyErrors();
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSchema5ShoveGrappleTest, "Project.Expedition.Schema5.ShoveAndGrappleApplyConditions",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSchema5ShoveGrappleTest::RunTest(const FString& Parameters)
{
    const ECombatCommandType Kinds[] = {ECombatCommandType::Shove, ECombatCommandType::Grapple};
    const ECombatCondition Applied[] = {ECombatCondition::Prone, ECombatCondition::Grappled};
    for (int32 K = 0; K < 2; ++K)
    {
        int32 Wins = 0, Losses = 0;
        for (int32 Seed = 1; Seed <= 40; ++Seed)
        {
            FCombatBattle Battle; FString Error;
            if (!Battle.Start(Pair({1, 0}), Seed, Error)) { AddError(Error); return false; }
            TestTrue(TEXT("Adjacent foe is a legal target"), Battle.LegalTargets(10, Kinds[K]).Contains(20));
            FCombatCommand Command; Command.ActorId = 10; Command.TargetId = 20; Command.Type = Kinds[K];
            const FCombatResult Result = Battle.Submit(Command);
            if (!TestTrue(TEXT("Contest accepted"), Result.bAccepted)) { return false; }
            TestFalse(TEXT("Uses the action"), Battle.Snapshot().bActionAvailable);
            const bool bWon = Has(Battle.Find(20), Applied[K]);
            (bWon ? Wins : Losses)++;
            TestTrue(TEXT("Feedback names the contest"), Result.Feedback.Contains(K == 0 ? TEXT("Shove") : TEXT("Grapple")));
            if (bWon && K == 1) { TestEqual(TEXT("Grapple remembers its source"), Battle.Find(20)->Conditions.Last().SourceId, 10); }
        }
        TestTrue(TEXT("Contest can be won"), Wins > 0);
        TestTrue(TEXT("Contest can be lost"), Losses > 0);
    }
    FCombatBattle Far; FString Error;
    Far.Start(Pair({3, 0}), 1, Error);
    TestFalse(TEXT("Shove needs reach"), Far.LegalTargets(10, ECombatCommandType::Shove).Contains(20));
    FCombatEncounter Down = Pair({1, 0});
    Down.Combatants[1].Conditions = {Cond(ECombatCondition::Prone)};
    FCombatBattle Already; Already.Start(Down, 1, Error);
    TestFalse(TEXT("No shoving a prone foe prone again"), Already.LegalTargets(10, ECombatCommandType::Shove).Contains(20));
    TestTrue(TEXT("Grapple still allowed"), Already.LegalTargets(10, ECombatCommandType::Grapple).Contains(20));
    return !HasAnyErrors();
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSchema5SrdConditionEffectsTest, "Project.Expedition.Schema5.SrdConditionEffects",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSchema5SrdConditionEffectsTest::RunTest(const FString& Parameters)
{
    FString Error;
    auto Begin = [&Error](const FCombatEncounter& E, int32 Seed, FCombatBattle& B) { const bool bOk = B.Start(E, Seed, Error); return bOk; };
    TestEqual(TEXT("New condition names"), FString(FCombatBattle::ConditionName(ECombatCondition::Paralyzed)), FString(TEXT("Paralyzed")));
    {
        FCombatEncounter E = Pair({1, 0});
        E.Combatants[1].Conditions = {Cond(ECombatCondition::Stunned)};
        FCombatBattle B; if (!TestTrue(TEXT("Stunned foe starts"), Begin(E, 3, B))) { AddError(Error); return false; }
        TestTrue(TEXT("Attacks on a stunned target have advantage"), B.AttackRollMode(10, 20).Mode == ERollMode::Advantage);
        TestEqual(TEXT("Stunned foe cannot make opportunity attacks"), B.OpportunityThreats(10, {{0, 1}, {0, 2}}).Num(), 0);
        FD20Outcome Out;
        TestTrue(TEXT("Save rolls"), B.SavingThrow(20, EAbility::Dexterity, 1, ERollMode::Normal, Out, Error));
        TestFalse(TEXT("Stunned auto-fails DEX saves"), Out.bSuccess);
        TestTrue(TEXT("CON save still rolls at DC 1"), B.SavingThrow(20, EAbility::Constitution, 1, ERollMode::Normal, Out, Error) && Out.bSuccess);
    }
    int32 Hits = 0;
    for (int32 Seed = 1; Seed <= 30; ++Seed)
    {
        FCombatEncounter E = Pair({1, 0});
        E.Combatants[1].Conditions = {Cond(ECombatCondition::Paralyzed)};
        FCombatBattle B; Begin(E, Seed, B);
        FCombatCommand Attack; Attack.ActorId = 10; Attack.TargetId = 20; Attack.Type = ECombatCommandType::Attack;
        const FCombatResult R = B.Submit(Attack);
        if (R.bHit) { ++Hits; TestTrue(TEXT("Hits within 5 ft of a paralyzed target are critical"), R.bCritical); }
    }
    TestTrue(TEXT("Some attacks hit"), Hits > 0);
    {
        FCombatEncounter E = Pair({3, 0});
        E.Combatants[0].Conditions = {Cond(ECombatCondition::Stunned)};
        FCombatBattle B; Begin(E, 1, B);
        TestTrue(TEXT("Stunned (5.2.1): Speed is unchanged"), B.Snapshot().MovementLeft > 0);
        TestFalse(TEXT("Stunned: no action"), B.Snapshot().bActionAvailable);
        TestFalse(TEXT("Stunned: no reaction"), B.Find(10)->bReactionAvailable);
    }
    {
        FCombatEncounter E = Pair({3, 0});
        E.Combatants[0].Conditions = {Cond(ECombatCondition::Incapacitated)};
        FCombatBattle B; Begin(E, 1, B);
        TestFalse(TEXT("Incapacitated: no action"), B.Snapshot().bActionAvailable);
        TestTrue(TEXT("Incapacitated alone can still move"), B.Snapshot().MovementLeft > 0);
    }
    for (ECombatCondition Frozen : {ECombatCondition::Paralyzed, ECombatCondition::Unconscious, ECombatCondition::Petrified})
    {
        FCombatEncounter E = Pair({3, 0});
        E.Combatants[0].Conditions = {Cond(Frozen)};
        FCombatBattle B; Begin(E, 1, B);
        TestEqual(FString::Printf(TEXT("%s: Speed 0"), FCombatBattle::ConditionName(Frozen)), B.Snapshot().MovementLeft, 0);
    }
    {
        FCombatEncounter Plain = Pair({3, 0}), Bound = Pair({3, 0});
        Bound.Combatants[0].Conditions = {Cond(ECombatCondition::Restrained), Cond(ECombatCondition::Poisoned)};
        FCombatBattle A, B; Begin(Plain, 1, A); Begin(Bound, 1, B);
        TestTrue(TEXT("Restrained: DEX save disadvantage"), B.SavingThrowChance(10, EAbility::Dexterity, 12, ERollMode::Normal) < A.SavingThrowChance(10, EAbility::Dexterity, 12, ERollMode::Normal));
        TestEqual(TEXT("Restrained: other saves normal"), B.SavingThrowChance(10, EAbility::Wisdom, 12, ERollMode::Normal), A.SavingThrowChance(10, EAbility::Wisdom, 12, ERollMode::Normal));
        TestTrue(TEXT("Poisoned: ability check disadvantage"), B.SkillCheckChance(10, ESkill::Athletics, 12, ERollMode::Normal) < A.SkillCheckChance(10, ESkill::Athletics, 12, ERollMode::Normal));
    }
    {
        FCombatEncounter E = Pair({3, 0});
        E.Combatants[0].Conditions = {Cond(ECombatCondition::Frightened, 20)};
        FCombatBattle B; Begin(E, 1, B);
        FCombatCommand Move; Move.ActorId = 10; Move.Type = ECombatCommandType::Move; Move.Destination = {1, 0};
        TestFalse(TEXT("Frightened: cannot move closer to the source"), B.Submit(Move).bAccepted);
        TestFalse(TEXT("Closer cell is not offered"), B.LegalMoves(10).Contains(FIntPoint(1, 0)));
        Move.Destination = {0, 1};
        TestTrue(TEXT("Frightened: may move without closing in"), B.Submit(Move).bAccepted);
    }
    {
        FCombatEncounter E = Pair({1, 0});
        E.Combatants[0].Conditions = {Cond(ECombatCondition::Stunned)};
        E.Combatants[1].Conditions = {Cond(ECombatCondition::Grappled, 10)};
        FCombatBattle B; Begin(E, 1, B);
        TestFalse(TEXT("Grapple ends when the grappler is incapacitated"), Has(B.Find(20), ECombatCondition::Grappled));
    }
    return !HasAnyErrors();
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSchema5NewActionForecastTest, "Project.Expedition.Schema5.ForecastsForAreaSpellsContestsAndUpcast",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSchema5NewActionForecastTest::RunTest(const FString& Parameters)
{
    FString Error;
    FCombatEncounter E = Pair({1, 0});
    FCombatantState& W = E.Combatants[0];
    W.Name = TEXT("Wizard"); W.AdventurerRole = EAdventurerRole::Wizard; W.Level = 3; W.SpellSlots = 2; W.SlotsByLevel = {2, 2, 0};
    E.Combatants[1].MaxHP = E.Combatants[1].HP = 500;
    FCombatBattle B;
    if (!TestTrue(TEXT("Wizard starts"), B.Start(E, 7, Error))) { AddError(Error); return false; }
    const FActionForecast Burn = B.Forecast(10, ECombatCommandType::BurningHands, 20);
    TestTrue(TEXT("Burning Hands forecast"), Burn.bValid && Burn.Formula.Contains(TEXT("3d6")) && Burn.Formula.Contains(TEXT("DEX")));
    TestTrue(TEXT("Chance the foe fails its save"), Burn.HitChanceOutOf400 > 0 && Burn.HitChanceOutOf400 < 400);
    TestTrue(TEXT("Lists creatures in the area"), Burn.Effects.ContainsByPredicate([](const FTargetEffect& X) { return X.TargetId == 20; }));
    TestTrue(TEXT("Upcast forecast adds a die"), B.Forecast(10, ECombatCommandType::BurningHands, 20, 2).Formula.Contains(TEXT("4d6")));
    TestTrue(TEXT("Thunderwave forecast"), B.Forecast(10, ECombatCommandType::Thunderwave, 20).Formula.Contains(TEXT("2d8")));
    TestTrue(TEXT("Upcast Magic Missile forecast"), B.Forecast(10, ECombatCommandType::MagicMissile, 20, 2).MaxAmount > B.Forecast(10, ECombatCommandType::MagicMissile, 20).MaxAmount);
    TestEqual(TEXT("Slot cycle: auto -> 1"), B.NextSlotLevel(10, 0), 1);
    TestEqual(TEXT("Slot cycle: 1 -> 2"), B.NextSlotLevel(10, 1), 2);
    TestEqual(TEXT("Slot cycle: 2 -> auto (no level 3 slots)"), B.NextSlotLevel(10, 2), 0);
    const FActionForecast Shove = B.Forecast(10, ECombatCommandType::Shove, 20);
    TestTrue(TEXT("Shove forecast is a contest"), Shove.bValid && !Shove.bAttackRoll && Shove.HitChanceOutOf400 > 0 && Shove.HitChanceOutOf400 < 400 && Shove.Formula.Contains(TEXT("Prone")));
    TestTrue(TEXT("Grapple forecast"), B.Forecast(10, ECombatCommandType::Grapple, 20).Formula.Contains(TEXT("Grappled")));
    for (const ECombatCommandType Kind : {ECombatCommandType::BurningHands, ECombatCommandType::Thunderwave, ECombatCommandType::Shove, ECombatCommandType::Grapple})
    {
        TestFalse(TEXT("Described action"), FCombatBattle::DescribeAction(Kind).StartsWith(TEXT("ATTACK / MOVE")));
    }
    TestTrue(TEXT("Area spell usage tag counts slots"), FCombatBattle::UsageTag(ECombatCommandType::BurningHands, *B.Find(10)).Contains(TEXT("slot")));
    return !HasAnyErrors();
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSchema5CatalogEquipmentTest, "Project.Expedition.Schema5.CatalogBuildsWearArmor",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSchema5CatalogEquipmentTest::RunTest(const FString& Parameters)
{
    TestTrue(TEXT("Barbarian: medium and shield"), FArmorRules::IsProficient(EAdventurerRole::Barbarian, EArmorCategory::Medium) && FArmorRules::IsProficient(EAdventurerRole::Barbarian, EArmorCategory::Shield) && !FArmorRules::IsProficient(EAdventurerRole::Barbarian, EArmorCategory::Heavy));
    TestTrue(TEXT("Paladin: everything"), FArmorRules::IsProficient(EAdventurerRole::Paladin, EArmorCategory::Heavy));
    TestTrue(TEXT("Monk and Sorcerer: none"), !FArmorRules::IsProficient(EAdventurerRole::Monk, EArmorCategory::Light) && !FArmorRules::IsProficient(EAdventurerRole::Sorcerer, EArmorCategory::Light));
    TestTrue(TEXT("Bard and Warlock: light, no shield"), FArmorRules::IsProficient(EAdventurerRole::Bard, EArmorCategory::Light) && !FArmorRules::IsProficient(EAdventurerRole::Warlock, EArmorCategory::Shield));
    int32 Equipped = 0;
    for (const FBuildDefinition& Build : FBuildCatalog::All())
    {
        FCombatEncounter E = Pair({1, 0});
        FCombatantState Hero;
        if (!TestTrue(TEXT("Create"), FBuildCatalog::Create(Build.Id, 10, TEXT("Hero"), Hero))) { continue; }
        Hero.Cell = {0, 0}; Hero.InitiativeBonus = 30; E.Combatants[0] = Hero;
        FCombatBattle B; FString Error;
        if (!TestTrue(Build.Id + TEXT(" starts"), B.Start(E, 7, Error))) { AddError(Error); continue; }
        // Equipment replaces the flat AC number without changing it; the SRD Defense fighting style (default for armored fighters) adds 1.
        TestEqual(Build.Id + TEXT(" keeps its AC"), B.EffectiveArmorClass(10), Build.Profile.ArmorClass + (FFeatureKit::Style(Hero) == EFightingStyle::Defense && Hero.ArmorWorn >= 0 ? 1 : 0));
        if (Hero.ArmorWorn < 0 && !Hero.bShieldEquipped) { continue; }
        ++Equipped;
        const FArmorPenalties P = FArmorRules::Penalties(Hero.AdventurerRole, Hero.ArmorWorn < 0 ? EArmor::None : static_cast<EArmor>(Hero.ArmorWorn), Hero.bShieldEquipped, B.AbilityScores(10).Get(EAbility::Strength));
        TestTrue(Build.Id + TEXT(" is trained for its armor"), !P.bStrDexDisadvantage && !P.bCannotCastSpells && P.SpeedPenaltyFeet == 0);
        TestTrue(Build.Id + TEXT(" names its armor"), B.ArmorClassText(10).Contains(Hero.ArmorWorn > 0 ? FArmorRules::Profile(static_cast<EArmor>(Hero.ArmorWorn)).Name : TEXT("shield")));
    }
    TestTrue(TEXT("Fighters wear armor"), FBuildCatalog::Find(TEXT("fighter.champion"))->Profile.ArmorWorn > 0);
    TestTrue(TEXT("Clerics wear armor"), FBuildCatalog::Find(TEXT("cleric.life"))->Profile.ArmorWorn > 0);
    TestEqual(TEXT("Wizards stay unarmored"), FBuildCatalog::Find(TEXT("wizard.evocation"))->Profile.ArmorWorn, -1);
    TestTrue(TEXT("Most armored classes equip something"), Equipped >= 20);
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSchema5StandardActionsTest, "Project.Expedition.Schema5.StandardActionsInBattle",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSchema5StandardActionsTest::RunTest(const FString& Parameters)
{
    FString Error;
    {
        FCombatBattle B;
        TestTrue(TEXT("Dash starts"), B.Start(Pair({7, 7}), 7, Error));
        const int32 Before = B.Snapshot().MovementLeft;
        TestTrue(TEXT("Dash accepted"), B.Submit({ECombatCommandType::Dash, 10, {}, 10}).bAccepted);
        TestEqual(TEXT("Dash adds speed"), B.Snapshot().MovementLeft, Before * 2);
        TestFalse(TEXT("Dash spends the action"), B.Snapshot().bActionAvailable);
        TestFalse(TEXT("No second Dash without Cunning Action"), B.Submit({ECombatCommandType::Dash, 10, {}, 10}).bAccepted);
        FCombatBattle Copy;
        TestTrue(TEXT("Dashed movement restores"), Copy.Restore(B.Snapshot(), Error));
    }
    {
        FCombatBattle B;
        TestTrue(TEXT("Disengage starts"), B.Start(Pair({1, 0}), 7, Error));
        TestTrue(TEXT("Disengage accepted"), B.Submit({ECombatCommandType::Disengage, 10, {}, 10}).bAccepted);
        const FCombatResult Away = B.Submit({ECombatCommandType::Move, 10, {0, 3}});
        TestTrue(TEXT("Disengaged move provokes nothing"), Away.bAccepted && Away.Reactions.IsEmpty());
        TestTrue(TEXT("Foe keeps its reaction"), B.Find(20)->bReactionAvailable);
    }
    {
        FCombatBattle B;
        TestTrue(TEXT("Dodge starts"), B.Start(Pair({1, 0}), 7, Error));
        const int32 SaveBefore = B.SavingThrowChance(10, EAbility::Dexterity, 15, ERollMode::Normal);
        TestTrue(TEXT("Dodge accepted"), B.Submit({ECombatCommandType::Dodge, 10, {}, 10}).bAccepted);
        TestEqual(TEXT("Attacks against a dodger have disadvantage"), B.AttackRollMode(20, 10).Mode, ERollMode::Disadvantage);
        TestTrue(TEXT("Dodger has advantage on DEX saves"), B.SavingThrowChance(10, EAbility::Dexterity, 15, ERollMode::Normal) > SaveBefore);
        B.Submit({ECombatCommandType::EndTurn, 10});
        TestTrue(TEXT("Dodge lasts through the foe's turn"), B.Find(10)->bDodging);
        B.Submit({ECombatCommandType::EndTurn, 20});
        TestFalse(TEXT("Dodge ends at the dodger's next turn"), B.Find(10)->bDodging);
    }
    {
        FCombatEncounter E = Pair({1, 0});
        FCombatantState Second = E.Combatants[0];
        Second.Id = 11; Second.Name = TEXT("Second"); Second.Cell = {1, 1}; Second.InitiativeBonus = 20;
        E.Combatants.Add(Second);
        FCombatBattle B;
        TestTrue(TEXT("Help starts"), B.Start(E, 7, Error));
        TestTrue(TEXT("Help accepted on an adjacent foe"), B.Submit({ECombatCommandType::Help, 10, {}, 20}).bAccepted);
        TestEqual(TEXT("Foe is marked"), B.Find(20)->HelpedById, 10);
        TestEqual(TEXT("Ally attacks the marked foe with advantage"), B.AttackRollMode(11, 20).Mode, ERollMode::Advantage);
        TestEqual(TEXT("Helper gains no advantage itself"), B.AttackRollMode(10, 20).Mode, ERollMode::Normal);
        B.Submit({ECombatCommandType::EndTurn, 10});
        TestEqual(TEXT("Second acts next"), B.ActiveId(), 11);
        TestTrue(TEXT("Helped attack"), B.Submit({ECombatCommandType::Attack, 11, {}, 20}).bAccepted);
        TestEqual(TEXT("Help is used up"), B.Find(20)->HelpedById, INDEX_NONE);
    }
    {
        bool bSawHidden = false;
        for (int32 Seed = 1; Seed < 40 && !bSawHidden; ++Seed)
        {
            FCombatBattle B;
            B.Start(Pair({7, 7}), Seed, Error);
            const FCombatResult Hide = B.Submit({ECombatCommandType::Hide, 10, {}, 10});
            TestTrue(TEXT("Hide accepted"), Hide.bAccepted);
            TestEqual(TEXT("Hide result matches state"), B.Find(10)->bHidden, Hide.bHit);
            if (!B.Find(10)->bHidden) { continue; }
            bSawHidden = true;
            TestEqual(TEXT("Hidden attacker has advantage"), B.AttackRollMode(10, 20).Mode, ERollMode::Advantage);
            TestEqual(TEXT("Attacks against the hidden have disadvantage"), B.AttackRollMode(20, 10).Mode, ERollMode::Disadvantage);
        }
        TestTrue(TEXT("Some seed hides"), bSawHidden);
    }
    {
        FCombatBattle B;
        TestTrue(TEXT("Ready starts"), B.Start(Pair({5, 0}), 7, Error));
        TestTrue(TEXT("Ready accepted"), B.Submit({ECombatCommandType::Ready, 10, {}, 10}).bAccepted);
        B.Submit({ECombatCommandType::EndTurn, 10});
        const FCombatResult Approach = B.Submit({ECombatCommandType::Move, 20, {1, 0}});
        TestTrue(TEXT("Foe approach accepted"), Approach.bAccepted);
        if (TestEqual(TEXT("Readied attack fires once"), Approach.Reactions.Num(), 1)) { TestEqual(TEXT("By the readier"), Approach.Reactions[0].ReactorId, 10); }
        TestFalse(TEXT("Readied reaction spent"), B.Find(10)->bReactionAvailable);
        TestFalse(TEXT("Ready consumed"), B.Find(10)->bReadied);
    }
    TestTrue(TEXT("Potion is the Use an Object action"), FCombatBattle::DescribeAction(ECombatCommandType::Potion).Contains(TEXT("Use an Object")));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSchema5DeathSavesInBattleTest, "Project.Expedition.Schema5.DeathSavesInBattle",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSchema5DeathSavesInBattleTest::RunTest(const FString& Parameters)
{
    FString Error;
    auto Downed = [](bool bDeathSaves)
    {
        FCombatEncounter Encounter = Pair({2, 1});
        Encounter.bDeathSaves = bDeathSaves; Encounter.bTwoRoundDowned = false;
        FCombatantState Friend = Encounter.Combatants[0];
        Friend.Id = 11; Friend.Name = TEXT("Friend"); Friend.Cell = {1, 1}; Friend.InitiativeBonus = 0; Friend.TemporaryHP = 50;
        Encounter.Combatants.Add(Friend);
        FCombatantState& Hero = Encounter.Combatants[0];
        Hero.HP = 0; Hero.Life = ECombatLife::Downed; Hero.RescueTurnsLeft = 3;
        return Encounter;
    };
    {
        FCombatBattle B;
        TestTrue(TEXT("Legacy clock starts"), B.Start(Downed(false), 7, Error));
        TestEqual(TEXT("Legacy rescue clock ticks"), B.Find(10)->RescueTurnsLeft, 2);
    }
    bool bSawStable = false, bSawDead = false, bSawSoak = false;
    for (int32 Seed = 1; Seed <= 40; ++Seed)
    {
        FCombatBattle B;
        if (!TestTrue(TEXT("Death-save battle starts"), B.Start(Downed(true), Seed, Error))) { return false; }
        const FCombatantState* Hero = B.Find(10);
        TestTrue(TEXT("First save recorded"), Hero->Life == ECombatLife::Conscious ? Hero->HP == 1 :
            Hero->DeathSaveSuccesses + Hero->DeathSaveFailures >= 1 && Hero->RescueTurnsLeft == 3);
        FCombatBattle Copy;
        TestTrue(TEXT("Death-save state restores"), Copy.Restore(B.Snapshot(), Error));
        for (int32 Turn = 0; Turn < 12 && B.Snapshot().Phase == EBattlePhase::Fighting && B.Find(10)->Life == ECombatLife::Downed; ++Turn)
        {
            const int32 Active = B.Snapshot().TurnOrder[B.Snapshot().TurnIndex];
            if (Active == 20 && !bSawSoak)
            {
                const FCombatResult Hit = B.Submit({ECombatCommandType::Attack, 20, {}, 11});
                if (Hit.bHit) { bSawSoak = true; TestEqual(TEXT("Temporary HP soak the hit"), B.Find(11)->HP, B.Find(11)->MaxHP); TestTrue(TEXT("Temporary HP spent"), B.Find(11)->TemporaryHP < 50); }
            }
            B.Submit({ECombatCommandType::EndTurn, B.Snapshot().TurnOrder[B.Snapshot().TurnIndex]});
            if (B.Find(10)->DeathSaveSuccesses == 3) { bSawStable = true; break; }
        }
        bSawDead |= B.Find(10)->Life == ECombatLife::Dead;
    }
    TestTrue(TEXT("Some hero stabilizes"), bSawStable);
    TestTrue(TEXT("Some hero dies of failed saves"), bSawDead);
    TestTrue(TEXT("Temporary HP absorbed a hit"), bSawSoak);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSchema5ShortRestTest, "Project.Expedition.Schema5.ShortRestHitDice",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSchema5ShortRestTest::RunTest(const FString& Parameters)
{
    bool bChecked = false;
    for (int32 Seed = 1; Seed < 60 && !bChecked; ++Seed)
    {
        FString Error;
        FExpeditionRun Run;
        Run.NewRun(Seed, Error, FString::Printf(TEXT("rest-%d"), Seed));
        int32 Healed = 0;
        TestFalse(TEXT("No short rest on the road"), Run.ShortRest(1, Healed, Error));
        Run.ChooseRoute(Run.Progress().Offers[0].Id, Error);
        if (!ExpeditionTestSupport::FinishCombat(Run) || Run.Progress().Phase != EExpeditionPhase::Reward) { continue; }
        int32 HeroId = INDEX_NONE;
        for (const FCombatantState& Unit : Run.Battle().Snapshot().Encounter.Combatants)
        { if (Unit.Side == ECombatSide::Party && Unit.Life == ECombatLife::Conscious && Unit.HP < Unit.MaxHP) { HeroId = Unit.Id; break; } }
        if (HeroId == INDEX_NONE) { continue; }
        bChecked = true;
        const int32 Level = Run.Battle().Find(HeroId)->Level;
        int32 Rests = 0;
        while (Run.ShortRest(HeroId, Healed, Error)) { ++Rests; TestTrue(TEXT("Hit die never hurts"), Healed >= 0); }
        const FCombatantState* Hero = Run.Battle().Find(HeroId);
        TestTrue(TEXT("Rests until dice or wounds run out"), Rests >= 1 && (Hero->HitDiceSpent == Level || Hero->HP == Hero->MaxHP));
        TestEqual(TEXT("One die per rest"), Hero->HitDiceSpent, Rests);
        FExpeditionRun Copy;
        TestTrue(TEXT("Spent hit dice restore"), Copy.Restore(Run.Snapshot(), Error));
        const int32 Spent = Hero->HitDiceSpent;
        Run.ClaimReward(HeroId, Error);
        if (Run.Progress().Phase == EExpeditionPhase::Recovery && TestTrue(TEXT("Camp"), Run.Recover(true, Error)))
        { TestTrue(TEXT("Camp regains hit dice"), Run.Battle().Find(HeroId)->HitDiceSpent < Spent || Spent == 0); }
    }
    TestTrue(TEXT("A wounded survivor rested"), bChecked);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSchema5ConcentrationInBattleTest, "Project.Expedition.Schema5.ConcentrationInBattle",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSchema5ConcentrationInBattleTest::RunTest(const FString& Parameters)
{
    bool bKept = false, bLost = false;
    for (int32 Seed = 1; Seed <= 80 && !(bKept && bLost); ++Seed)
    {
        FString Error;
        FCombatEncounter Encounter = Pair({1, 0});
        Encounter.Combatants[0].bConcentrating = true;
        Encounter.Combatants[0].MaxHP = Encounter.Combatants[0].HP = 200;
        FCombatBattle B;
        if (!TestTrue(TEXT("Concentration battle starts"), B.Start(Encounter, Seed, Error))) { return false; }
        FCombatBattle Copy;
        TestTrue(TEXT("Concentration restores"), Copy.Restore(B.Snapshot(), Error));
        B.Submit({ECombatCommandType::EndTurn, 10});
        const FCombatResult Hit = B.Submit({ECombatCommandType::Attack, 20, {}, 10});
        if (!Hit.bHit || B.Find(10)->HP == 200) { TestTrue(TEXT("A miss keeps concentration"), B.Find(10)->bConcentrating); continue; }
        (B.Find(10)->bConcentrating ? bKept : bLost) = true;
    }
    TestTrue(TEXT("Some CON saves keep concentration"), bKept);
    TestTrue(TEXT("Some CON saves break it"), bLost);
    FString Error;
    FCombatEncounter Bad = Pair({1, 0});
    Bad.Combatants[0].bConcentrating = true; Bad.Combatants[0].HP = 0; Bad.Combatants[0].Life = ECombatLife::Downed; Bad.Combatants[0].RescueTurnsLeft = 3;
    FCombatBattle B;
    TestFalse(TEXT("Downed units cannot concentrate"), B.Start(Bad, 1, Error));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSchema5FireBoltCantripTest, "Project.Expedition.Schema5.FireBoltCantripScaling",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSchema5FireBoltCantripTest::RunTest(const FString& Parameters)
{
    FCombatantState Wizard = FBuildCatalog::Find(TEXT("wizard.evocation"))->Profile;
    int32 Count = 0, Sides = 0;
    Wizard.Level = 4;
    FCombatBattle::WeaponDice(Wizard, Count, Sides);
    TestTrue(TEXT("Fire Bolt is 1d10 before 5th level"), Count == 1 && Sides == 10);
    Wizard.Level = 5;
    FCombatBattle::WeaponDice(Wizard, Count, Sides);
    TestTrue(TEXT("Fire Bolt is 2d10 at 5th level"), Count == 2 && Sides == 10);
    FCombatantState Fighter = FBuildCatalog::Find(TEXT("fighter.champion"))->Profile;
    Fighter.Level = 5;
    FCombatBattle::WeaponDice(Fighter, Count, Sides);
    TestEqual(TEXT("Weapons do not scale like cantrips"), Count, 1);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSchema5TwoWeaponTest, "Project.Expedition.Schema5.TwoWeaponOffHand",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSchema5TwoWeaponTest::RunTest(const FString& Parameters)
{
    FCombatEncounter Encounter = Schema5Test::Pair({1, 0});
    Encounter.Combatants[0].MainHand = static_cast<int32>(EWeapon::Shortsword);
    Encounter.Combatants[0].OffHand = static_cast<int32>(EWeapon::Dagger);
    Encounter.Combatants[1].HP = Encounter.Combatants[1].MaxHP = 200;
    FCombatBattle Battle;
    FString Error;
    TestTrue(TEXT("Starts"), Battle.Start(Encounter, 7, Error));
    TestFalse(TEXT("Off-hand needs the Attack action first"), Battle.Submit({ECombatCommandType::OffHandAttack, 10, {0, 0}, 20}).bAccepted);
    TestTrue(TEXT("Attack"), Battle.Submit({ECombatCommandType::Attack, 10, {0, 0}, 20}).bAccepted);
    TestTrue(TEXT("Off-hand target is legal"), Battle.LegalTargets(10, ECombatCommandType::OffHandAttack).Contains(20));
    TestTrue(TEXT("Off-hand bonus attack"), Battle.Submit({ECombatCommandType::OffHandAttack, 10, {0, 0}, 20}).bAccepted);
    TestFalse(TEXT("Bonus action spent"), Battle.Snapshot().bBonusActionAvailable);
    TestFalse(TEXT("Only one off-hand attack"), Battle.Submit({ECombatCommandType::OffHandAttack, 10, {0, 0}, 20}).bAccepted);
    Encounter.Combatants[0].OffHand = static_cast<int32>(EWeapon::Longsword);
    FCombatBattle Heavy;
    TestFalse(TEXT("Non-light off-hand is rejected"), Heavy.Start(Encounter, 7, Error));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSchema5GrappleSizeTest, "Project.Expedition.Schema5.GrappleSizeLimit",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSchema5GrappleSizeTest::RunTest(const FString& Parameters)
{
    FCombatEncounter Encounter = Schema5Test::Pair({1, 0});
    Encounter.Combatants[1].Size = static_cast<int32>(ECreatureSize::Large);
    FCombatBattle Battle;
    FString Error;
    TestTrue(TEXT("Starts"), Battle.Start(Encounter, 7, Error));
    TestTrue(TEXT("Medium can grapple Large"), Battle.LegalTargets(10, ECombatCommandType::Grapple).Contains(20));
    Encounter.Combatants[1].Size = static_cast<int32>(ECreatureSize::Huge);
    FCombatBattle Huge;
    TestTrue(TEXT("Starts huge"), Huge.Start(Encounter, 7, Error));
    TestFalse(TEXT("Medium cannot grapple Huge"), Huge.LegalTargets(10, ECombatCommandType::Grapple).Contains(20));
    TestFalse(TEXT("Medium cannot shove Huge"), Huge.LegalTargets(10, ECombatCommandType::Shove).Contains(20));
    Encounter.Combatants[1].Size = 9;
    FCombatBattle Bad;
    TestFalse(TEXT("Invalid size rejected"), Bad.Start(Encounter, 7, Error));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSchema5CreatureCoverTest, "Project.Expedition.Schema5.CreatureCover",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSchema5CreatureCoverTest::RunTest(const FString& Parameters)
{
    FCombatEncounter Encounter = Schema5Test::Pair({4, 0});
    Encounter.Combatants[0].MainHand = static_cast<int32>(EWeapon::Shortbow);
    Encounter.Combatants[0].Ammunition = 20;
    FCombatantState Blocker = Encounter.Combatants[1];
    Blocker.Id = 21; Blocker.Name = TEXT("Blocker"); Blocker.Cell = {2, 0};
    Encounter.Combatants.Add(Blocker);
    FString Error;
    FCombatBattle Off;
    TestTrue(TEXT("Starts"), Off.Start(Encounter, 7, Error));
    TestEqual(TEXT("Default: creatures give no cover"), Off.CoverBonus(10, 20), 0);
    Encounter.bCreatureCover = true;
    FCombatBattle On;
    TestTrue(TEXT("Starts with creature cover"), On.Start(Encounter, 7, Error));
    TestEqual(TEXT("A creature in the line gives half cover"), On.CoverBonus(10, 20), 2);
    TestEqual(TEXT("No creature between, no cover"), On.CoverBonus(10, 21), 0);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSchema5AreaBlockedTest, "Project.Expedition.Schema5.AreaBlockedByWalls",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSchema5AreaBlockedTest::RunTest(const FString& Parameters)
{
    FCombatEncounter Encounter = Schema5Test::Pair({2, 0});
    FCombatantState& W = Encounter.Combatants[0];
    W.Name = TEXT("Wizard"); W.AdventurerRole = EAdventurerRole::Wizard; W.Level = 3; W.SpellSlots = 2; W.SlotsByLevel = {2, 2, 0};
    FString Error;
    FCombatBattle Open;
    TestTrue(TEXT("Starts open"), Open.Start(Encounter, 7, Error));
    TestTrue(TEXT("Cone reaches the foe in the open"), Open.LegalTargets(10, ECombatCommandType::BurningHands).Contains(20));
    Encounter.BlockedCells.Add({1, 0});
    FCombatBattle Walled;
    TestTrue(TEXT("Starts walled"), Walled.Start(Encounter, 7, Error));
    TestFalse(TEXT("Total cover blocks the cone"), Walled.LegalTargets(10, ECombatCommandType::BurningHands).Contains(20));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSchema5SuperiorityDieTest, "Project.Expedition.Schema5.SuperiorityDieInBattle",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSchema5SuperiorityDieTest::RunTest(const FString& Parameters)
{
    FCombatEncounter Encounter = Schema5Test::Pair({1, 0});
    FCombatantState Master = FBuildCatalog::Find(TEXT("fighter.battle-master"))->Profile;
    Master.Id = 10; Master.Name = TEXT("Master"); Master.Cell = {0, 0}; Master.InitiativeBonus = 30; Master.Side = ECombatSide::Party;
    Encounter.Combatants[0] = Master;
    Encounter.Combatants[1].HP = Encounter.Combatants[1].MaxHP = 200;
    FString Error;
    Encounter.bSuperiorityDice = false; // legacy flat path; the die is the default since #79
    FCombatBattle Plain;
    TestTrue(TEXT("Starts"), Plain.Start(Encounter, 7, Error));
    const int32 Base = Plain.Submit({ECombatCommandType::Attack, 10, {}, 20}).AttackBonus;
    FCombatBattle Flat;
    TestTrue(TEXT("Starts flat"), Flat.Start(Encounter, 7, Error));
    TestEqual(TEXT("Legacy Precision Strike is +2"), Flat.Submit({ECombatCommandType::PrecisionStrike, 10, {}, 20}).AttackBonus, Base + 2);
    Encounter.bSuperiorityDice = true;
    TSet<int32> Seen;
    for (int32 Seed = 1; Seed <= 20; ++Seed)
    {
        FCombatBattle Dice;
        TestTrue(TEXT("Starts dice"), Dice.Start(Encounter, Seed, Error));
        const FCombatResult R = Dice.Submit({ECombatCommandType::PrecisionStrike, 10, {}, 20});
        TestTrue(TEXT("Accepted"), R.bAccepted);
        TestTrue(TEXT("Superiority die is a d8"), R.AttackBonus - Base >= 1 && R.AttackBonus - Base <= 8);
        Seen.Add(R.AttackBonus - Base);
    }
    TestTrue(TEXT("The die varies"), Seen.Num() > 1);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSchema5AmbushSurpriseTest, "Project.Expedition.Schema5.AmbushSurprise",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSchema5AmbushSurpriseTest::RunTest(const FString& Parameters)
{
    // Foe acts first (higher initiative); a surprised foe loses that turn and the ally acts in round 1.
    FCombatEncounter Encounter = Schema5Test::Pair({5, 5});
    Encounter.Combatants[0].InitiativeBonus = -10;
    Encounter.Combatants[1].InitiativeBonus = 30;
    FString Error;
    FCombatBattle Normal;
    TestTrue(TEXT("Starts"), Normal.Start(Encounter, 7, Error));
    TestEqual(TEXT("Without ambush the foe acts first"), Normal.ActiveId(), 20);
    Encounter.bPartyAmbush = true;
    bool bSurprisedOnce = false;
    for (int32 Seed = 1; Seed <= 20; ++Seed)
    {
        FCombatBattle Ambush;
        TestTrue(TEXT("Starts ambush"), Ambush.Start(Encounter, Seed, Error));
        if (Ambush.ActiveId() == 10)
        {
            bSurprisedOnce = true;
            TestEqual(TEXT("Still round 1"), Ambush.Snapshot().Round, 1);
            TestTrue(TEXT("Ends turn"), Ambush.Submit({ECombatCommandType::EndTurn, 10}).bAccepted);
            TestEqual(TEXT("Foe acts in round 2"), Ambush.ActiveId(), 20);
            TestEqual(TEXT("Round 2"), Ambush.Snapshot().Round, 2);
        }
    }
    TestTrue(TEXT("Some seed surprises the foe"), bSurprisedOnce);
    return true;
}
namespace CampTest
{
bool RewardRun(FExpeditionRun& Run, int32 Seed)
{
    FString Error;
    Run.NewRun(Seed, Error, FString::Printf(TEXT("camp-%d"), Seed));
    Run.ChooseRoute(Run.Progress().Offers[0].Id, Error);
    return ExpeditionTestSupport::FinishCombat(Run) && Run.Progress().Phase == EExpeditionPhase::Reward;
}
int32 Living(const FExpeditionRun& Run, TFunctionRef<bool(const FCombatantState&)> Pick)
{
    for (const FCombatantState& Unit : Run.Battle().Snapshot().Encounter.Combatants)
    { if (Unit.Side == ECombatSide::Party && Unit.Life == ECombatLife::Conscious && Pick(Unit)) { return Unit.Id; } }
    return INDEX_NONE;
}
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSchema5CampPickLockTest, "Project.Expedition.Schema5.CampPickLock",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSchema5CampPickLockTest::RunTest(const FString& Parameters)
{
    bool bChecked = false, bOpened = false;
    for (int32 Seed = 1; Seed < 80; ++Seed)
    {
        FExpeditionRun Run;
        FString Error, Outcome;
        TestFalse(TEXT("No camp on the road"), Run.CampActivity(1, ECampActivity::PickLock, Outcome, Error));
        if (!CampTest::RewardRun(Run, Seed)) { continue; }
        const int32 Rogue = CampTest::Living(Run, [](const FCombatantState& U) { return U.AdventurerRole == EAdventurerRole::Rogue; });
        const int32 Other = CampTest::Living(Run, [](const FCombatantState& U) { return U.AdventurerRole != EAdventurerRole::Rogue; });
        if (Rogue == INDEX_NONE || Other == INDEX_NONE) { continue; }
        bChecked = true;
        TestFalse(TEXT("Needs thieves' tools"), Run.CampActivity(Other, ECampActivity::PickLock, Outcome, Error));
        TestTrue(TEXT("Rogue tries the lock"), Run.CampActivity(Rogue, ECampActivity::PickLock, Outcome, Error));
        TestFalse(TEXT("Once per rest point"), Run.CampActivity(Rogue, ECampActivity::PickLock, Outcome, Error));
        const int32 Bonus = Run.Progress().History.Last().BonusPotions;
        bOpened |= Bonus == 1;
        FExpeditionRun Copy;
        TestTrue(TEXT("Restores"), Copy.Restore(Run.Snapshot(), Error));
        const int32 Before = Run.Battle().Find(Rogue)->Potions;
        TestTrue(TEXT("Claims"), Run.ClaimReward(Rogue, Error));
        TestEqual(TEXT("Loot includes the strongbox potion"), Run.Battle().Find(Rogue)->Potions - Before, Run.Progress().History.Last().Route.RewardPotions + Bonus);
        TestTrue(TEXT("Restores after claim"), Copy.Restore(Run.Snapshot(), Error));
        if (bOpened) { break; }
    }
    TestTrue(TEXT("A rogue reached a rest point"), bChecked);
    TestTrue(TEXT("Some seed opens the lock"), bOpened);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSchema5CampParleyTest, "Project.Expedition.Schema5.CampParley",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSchema5CampParleyTest::RunTest(const FString& Parameters)
{
    bool bChecked = false, bGift = false, bPlain = false;
    for (int32 Seed = 1; Seed < 120 && !(bGift && bPlain); ++Seed)
    {
        FExpeditionRun Run;
        FString Error, Outcome;
        if (!CampTest::RewardRun(Run, Seed)) { continue; }
        const int32 Hero = CampTest::Living(Run, [](const FCombatantState&) { return true; });
        bChecked = true;
        TestTrue(TEXT("Anyone may parley"), Run.CampActivity(Hero, ECampActivity::Parley, Outcome, Error));
        TestFalse(TEXT("Outcome described"), Outcome.IsEmpty());
        TestFalse(TEXT("Once per rest point"), Run.CampActivity(Hero, ECampActivity::Parley, Outcome, Error));
        const int32 Bonus = Run.Progress().History.Last().BonusPotions;
        (Bonus == 1 ? bGift : bPlain) = true;
        FExpeditionRun Copy;
        TestTrue(TEXT("Restores"), Copy.Restore(Run.Snapshot(), Error));
        TestTrue(TEXT("Claims"), Run.ClaimReward(Hero, Error));
        TestTrue(TEXT("Restores after claim"), Copy.Restore(Run.Snapshot(), Error));
    }
    TestTrue(TEXT("Reached a rest point"), bChecked);
    TestTrue(TEXT("Some seed earns a gift"), bGift);
    TestTrue(TEXT("Some seed earns nothing"), bPlain);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSchema5CampBuyPotionTest, "Project.Expedition.Schema5.CampBuyPotion",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSchema5CampBuyPotionTest::RunTest(const FString& Parameters)
{
    bool bChecked = false;
    for (int32 Seed = 1; Seed < 60 && !bChecked; ++Seed)
    {
        FExpeditionRun Run;
        FString Error, Outcome;
        if (!CampTest::RewardRun(Run, Seed)) { continue; }
        bChecked = true;
        TestEqual(TEXT("Victory purse"), Run.Progress().Coins, FExpeditionRun::VictoryPurseCopper);
        const int32 Hero = CampTest::Living(Run, [](const FCombatantState&) { return true; });
        TestFalse(TEXT("Cannot afford yet"), Run.CampActivity(Hero, ECampActivity::BuyPotion, Outcome, Error));
        FExpeditionSnapshot Rich = Run.Snapshot();
        Rich.Progress.Coins = 10000;
        TestTrue(TEXT("Richer purse restores"), Run.Restore(Rich, Error));
        TestTrue(TEXT("Buys"), Run.CampActivity(Hero, ECampActivity::BuyPotion, Outcome, Error));
        const int32 Spent = 10000 - Run.Progress().Coins;
        TestTrue(TEXT("Haggled price"), Spent >= 4000 && Spent <= 5000);
        TestEqual(TEXT("Potion in the loot"), Run.Progress().History.Last().BonusPotions, 1);
        TestFalse(TEXT("Once per rest"), Run.CampActivity(Hero, ECampActivity::BuyPotion, Outcome, Error));
        FExpeditionSnapshot Broke = Run.Snapshot();
        Broke.Progress.Coins = -1;
        FExpeditionRun Copy;
        TestFalse(TEXT("Negative purse rejected"), Copy.Restore(Broke, Error));
        TestTrue(TEXT("Restores"), Copy.Restore(Run.Snapshot(), Error));
    }
    TestTrue(TEXT("Reached a rest point"), bChecked);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSchema5RouteHazardTest, "Project.Expedition.Schema5.RouteHazard",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSchema5RouteHazardTest::RunTest(const FString& Parameters)
{
    bool bHurt = false, bOff = false;
    for (int32 Seed = 1; Seed < 200 && !(bHurt && bOff); ++Seed)
    {
        FString Error;
        FExpeditionRun Plain, Hazards;
        Plain.NewRun(Seed, Error, TEXT("hazard"));
        Hazards.NewRun(Seed, Error, TEXT("hazard"));
        TestTrue(TEXT("Toggle on the road"), Hazards.SetRouteHazards(true, Error));
        const FExpeditionRoute* Danger = Plain.Progress().Offers.FindByPredicate([](const FExpeditionRoute& R) { return R.bDangerous; });
        if (!Danger) { continue; }
        TestTrue(TEXT("Plain route"), Plain.ChooseRoute(Danger->Id, Error));
        TestTrue(TEXT("Hazard route"), Hazards.ChooseRoute(Danger->Id, Error));
        TestEqual(TEXT("Off by default: no hazard"), Plain.Progress().History.Last().Hazard, -1);
        bOff = true;
        const FExpeditionEncounterRecord& Met = Hazards.Progress().History.Last();
        TestTrue(TEXT("Hazard rolled"), Met.Hazard >= 0);
        TestFalse(TEXT("Hazard described"), FExpeditionRun::HazardDescription(Met).IsEmpty());
        FExpeditionRun Copy;
        TestTrue(TEXT("Hazard run restores"), Copy.Restore(Hazards.Snapshot(), Error));
        if (Met.Hazard <= 1 && !Met.bHazardPassed)
        {
            bHurt = true;
            for (const FCombatantState& Unit : Hazards.Battle().Snapshot().Encounter.Combatants)
            { if (Unit.Side == ECombatSide::Party) { TestTrue(TEXT("Lost HP but alive"), Unit.HP < Plain.Battle().Find(Unit.Id)->HP || Unit.HP == 1); } }
        }
    }
    TestTrue(TEXT("Default compared"), bOff);
    TestTrue(TEXT("Some hazard hurt the party"), bHurt);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSchema5CampForageTest, "Project.Expedition.Schema5.CampForage",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSchema5CampForageTest::RunTest(const FString& Parameters)
{
    bool bFound = false, bEmpty = false;
    for (int32 Seed = 1; Seed < 150 && !(bFound && bEmpty); ++Seed)
    {
        FExpeditionRun Run;
        FString Error, Outcome;
        if (!CampTest::RewardRun(Run, Seed)) { continue; }
        const int32 Hero = CampTest::Living(Run, [](const FCombatantState&) { return true; });
        TestTrue(TEXT("Forage"), Run.CampActivity(Hero, ECampActivity::Forage, Outcome, Error));
        TestTrue(TEXT("Group result described"), Outcome.Contains(TEXT("pass DC 12")));
        TestFalse(TEXT("Once per rest"), Run.CampActivity(Hero, ECampActivity::Forage, Outcome, Error));
        (Run.Progress().History.Last().BonusPotions == 1 ? bFound : bEmpty) = true;
        FExpeditionRun Copy;
        TestTrue(TEXT("Restores"), Copy.Restore(Run.Snapshot(), Error));
    }
    TestTrue(TEXT("Some group finds herbs"), bFound);
    TestTrue(TEXT("Some group finds nothing"), bEmpty);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSchema5InspirationTest, "Project.Expedition.Schema5.InspirationInBattle",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSchema5InspirationTest::RunTest(const FString& Parameters)
{
    FCombatEncounter Encounter = Schema5Test::Pair({1, 0});
    Encounter.Combatants[1].HP = Encounter.Combatants[1].MaxHP = 200;
    Encounter.Combatants[0].bInspired = true;
    FString Error;
    FCombatBattle Off;
    TestTrue(TEXT("Starts"), Off.Start(Encounter, 7, Error));
    TestEqual(TEXT("Off by default: plain roll"), Off.Submit({ECombatCommandType::Attack, 10, {}, 20}).RollMode, ERollMode::Normal);
    Encounter.bInspiration = true;
    FCombatBattle On;
    TestTrue(TEXT("Starts on"), On.Start(Encounter, 7, Error));
    const FCombatResult R = On.Submit({ECombatCommandType::Attack, 10, {}, 20});
    TestEqual(TEXT("Spent for advantage"), R.RollMode, ERollMode::Advantage);
    TestTrue(TEXT("Source named"), R.RollModeSources.Contains(TEXT("inspiration")));
    TestEqual(TEXT("Spent unless a 20 regained it"), On.Find(10)->bInspired, R.NaturalRoll == 20);
    Encounter.Combatants[0].bInspired = false;
    bool bGranted = false;
    for (int32 Seed = 1; Seed < 400 && !bGranted; ++Seed)
    {
        FCombatBattle Roll;
        Roll.Start(Encounter, Seed, Error);
        const FCombatResult S = Roll.Submit({ECombatCommandType::Attack, 10, {}, 20});
        TestEqual(TEXT("Only a natural 20 grants"), Roll.Find(10)->bInspired, S.NaturalRoll == 20);
        bGranted = S.NaturalRoll == 20;
    }
    TestTrue(TEXT("Some natural 20 grants inspiration"), bGranted);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSchema5DarknessTest, "Project.Expedition.Schema5.DarknessInBattle",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSchema5DarknessTest::RunTest(const FString& Parameters)
{
    FCombatEncounter Encounter = Schema5Test::Pair({1, 0});
    Encounter.Combatants[1].Role = TEXT("Goblin");
    Encounter.Combatants[1].HP = Encounter.Combatants[1].MaxHP = 200;
    FString Error;
    FCombatBattle Lit;
    TestTrue(TEXT("Starts"), Lit.Start(Encounter, 7, Error));
    TestEqual(TEXT("Off by default: lit field"), Lit.Submit({ECombatCommandType::Attack, 10, {}, 20}).RollMode, ERollMode::Normal);
    Encounter.bDarkness = true;
    FCombatBattle Dark;
    TestTrue(TEXT("Starts dark"), Dark.Start(Encounter, 7, Error));
    const FCombatResult R = Dark.Submit({ECombatCommandType::Attack, 10, {}, 20});
    TestEqual(TEXT("Blind human vs darkvision goblin: disadvantage"), R.RollMode, ERollMode::Disadvantage);
    Encounter.Combatants[1].Role = TEXT("Raider");
    FCombatBattle BothBlind;
    TestTrue(TEXT("Starts blind"), BothBlind.Start(Encounter, 7, Error));
    const FCombatResult B = BothBlind.Submit({ECombatCommandType::Attack, 10, {}, 20});
    TestEqual(TEXT("Both blind: cancels"), B.RollMode, ERollMode::Normal);
    TestEqual(TEXT("Both sources listed"), B.RollModeSources.Num(), 2);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSchema5EncumbranceTest, "Project.Expedition.Schema5.EncumbranceInBattle",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSchema5EncumbranceTest::RunTest(const FString& Parameters)
{
    FCombatEncounter Encounter = Schema5Test::Pair({7, 7});
    Encounter.Combatants[0].Speed = 6;
    FString Error;
    FCombatBattle Probe;
    TestTrue(TEXT("Starts"), Probe.Start(Encounter, 7, Error));
    const int32 Strength = Probe.AbilityScores(10).Strength;
    Encounter.Combatants[0].CarriedPounds = 10 * Strength + 1;
    FCombatBattle Off;
    TestTrue(TEXT("Starts off"), Off.Start(Encounter, 7, Error));
    TestEqual(TEXT("Off by default: full speed"), Off.Snapshot().MovementLeft, 6);
    Encounter.bEncumbrance = true;
    FCombatBattle Heavy;
    TestTrue(TEXT("Starts heavy"), Heavy.Start(Encounter, 7, Error));
    TestEqual(TEXT("Heavily encumbered: -4 cells"), Heavy.Snapshot().MovementLeft, 2);
    Encounter.Combatants[0].CarriedPounds = 5 * Strength + 1;
    FCombatBattle Light;
    TestTrue(TEXT("Starts light"), Light.Start(Encounter, 7, Error));
    TestEqual(TEXT("Encumbered: -2 cells"), Light.Snapshot().MovementLeft, 4);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSchema5CampPickpocketTest, "Project.Expedition.Schema5.CampPickpocket",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSchema5CampPickpocketTest::RunTest(const FString& Parameters)
{
    bool bGain = false, bNoGain = false;
    for (int32 Seed = 1; Seed < 150 && !(bGain && bNoGain); ++Seed)
    {
        FExpeditionRun Run;
        FString Error, Outcome;
        if (!CampTest::RewardRun(Run, Seed)) { continue; }
        const int32 Hero = CampTest::Living(Run, [](const FCombatantState&) { return true; });
        const int32 Before = Run.Progress().Coins;
        TestTrue(TEXT("Lift"), Run.CampActivity(Hero, ECampActivity::Pickpocket, Outcome, Error));
        TestFalse(TEXT("Once per rest"), Run.CampActivity(Hero, ECampActivity::Pickpocket, Outcome, Error));
        const int32 Delta = Run.Progress().Coins - Before;
        TestTrue(TEXT("Gain, nothing, or a fine"), Delta == FExpeditionRun::PickpocketCopper || Delta == 0 || Delta == -FExpeditionRun::PickpocketCopper / 2);
        (Delta > 0 ? bGain : bNoGain) = true;
        FExpeditionRun Copy;
        TestTrue(TEXT("Restores"), Copy.Restore(Run.Snapshot(), Error));
    }
    TestTrue(TEXT("Some lift succeeds"), bGain);
    TestTrue(TEXT("Some lift fails"), bNoGain);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSchema5CampPerformTest, "Project.Expedition.Schema5.CampPerform",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSchema5CampPerformTest::RunTest(const FString& Parameters)
{
    bool bPaid = false, bUnpaid = false;
    for (int32 Seed = 1; Seed < 150 && !(bPaid && bUnpaid); ++Seed)
    {
        FExpeditionRun Run;
        FString Error, Outcome;
        if (!CampTest::RewardRun(Run, Seed)) { continue; }
        const int32 Hero = CampTest::Living(Run, [](const FCombatantState&) { return true; });
        const int32 Before = Run.Progress().Coins;
        TestTrue(TEXT("Perform"), Run.CampActivity(Hero, ECampActivity::Perform, Outcome, Error));
        TestFalse(TEXT("Once per rest"), Run.CampActivity(Hero, ECampActivity::Perform, Outcome, Error));
        const int32 Delta = Run.Progress().Coins - Before;
        TestTrue(TEXT("Base, double, or nothing"), Delta == 0 || Delta == FExpeditionRun::PerformanceCopper || Delta == 2 * FExpeditionRun::PerformanceCopper);
        TestTrue(TEXT("Inspiration only on a standout"), !Run.Battle().Find(Hero)->bInspired || Delta == 2 * FExpeditionRun::PerformanceCopper);
        (Delta > 0 ? bPaid : bUnpaid) = true;
        FExpeditionRun Copy;
        TestTrue(TEXT("Restores"), Copy.Restore(Run.Snapshot(), Error));
    }
    TestTrue(TEXT("Some show pays"), bPaid);
    TestTrue(TEXT("Some show flops"), bUnpaid);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSchema5CampBeastTest, "Project.Expedition.Schema5.CampBeast",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSchema5CampBeastTest::RunTest(const FString& Parameters)
{
    bool bBitten = false, bCalmed = false;
    for (int32 Seed = 1; Seed < 150 && !(bBitten && bCalmed); ++Seed)
    {
        FExpeditionRun Run;
        FString Error, Outcome;
        if (!CampTest::RewardRun(Run, Seed)) { continue; }
        const int32 Hero = CampTest::Living(Run, [](const FCombatantState&) { return true; });
        const int32 Before = Run.Battle().Find(Hero)->HP;
        TestTrue(TEXT("Calm"), Run.CampActivity(Hero, ECampActivity::CalmBeast, Outcome, Error));
        const bool bBit = Outcome.Contains(TEXT("bitten"));
        FString Ignored;
        TestFalse(TEXT("Once per rest"), Run.CampActivity(Hero, ECampActivity::CalmBeast, Ignored, Error));
        const int32 After = Run.Battle().Find(Hero)->HP;
        TestTrue(TEXT("Never below 1"), After >= 1 && After <= Before);
        TestTrue(TEXT("Bite is at most 1d6"), Before - After <= 6);
        (bBit ? bBitten : bCalmed) = true;
        if (!bBit) { TestEqual(TEXT("Calm costs nothing"), After, Before); }
        FExpeditionRun Copy;
        TestTrue(TEXT("Restores"), Copy.Restore(Run.Snapshot(), Error));
    }
    TestTrue(TEXT("Some wolf bites"), bBitten);
    TestTrue(TEXT("Some wolf calms"), bCalmed);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSchema5CampTreatTest, "Project.Expedition.Schema5.CampTreat",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSchema5CampTreatTest::RunTest(const FString& Parameters)
{
    bool bTreated = false;
    for (int32 Seed = 1; Seed < 150 && !bTreated; ++Seed)
    {
        FExpeditionRun Run;
        FString Error, Outcome;
        if (!CampTest::RewardRun(Run, Seed)) { continue; }
        const int32 Hero = CampTest::Living(Run, [](const FCombatantState&) { return true; });
        int32 MissingBefore = 0, MissingAfter = 0;
        for (const FCombatantState& U : Run.Battle().Snapshot().Encounter.Combatants)
        {
            if (U.Side == ECombatSide::Party && U.Life == ECombatLife::Conscious) { MissingBefore += U.MaxHP - U.HP; }
        }
        if (MissingBefore == 0)
        {
            TestFalse(TEXT("Nobody hurt"), Run.CampActivity(Hero, ECampActivity::TreatWounds, Outcome, Error));
            continue;
        }
        TestTrue(TEXT("Treat"), Run.CampActivity(Hero, ECampActivity::TreatWounds, Outcome, Error));
        for (const FCombatantState& U : Run.Battle().Snapshot().Encounter.Combatants)
        {
            if (U.Side == ECombatSide::Party && U.Life == ECombatLife::Conscious) { MissingAfter += U.MaxHP - U.HP; TestTrue(TEXT("Capped"), U.HP <= U.MaxHP); }
        }
        TestTrue(TEXT("Heals"), MissingAfter < MissingBefore);
        FString Ignored;
        TestFalse(TEXT("Once per rest"), Run.CampActivity(Hero, ECampActivity::TreatWounds, Ignored, Error));
        FExpeditionRun Copy;
        TestTrue(TEXT("Restores"), Copy.Restore(Run.Snapshot(), Error));
        bTreated = true;
    }
    TestTrue(TEXT("Some party is treated"), bTreated);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSchema5CampBrewTest, "Project.Expedition.Schema5.CampBrew",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSchema5CampBrewTest::RunTest(const FString& Parameters)
{
    FExpeditionRun Run;
    FString Error, Outcome;
    int32 Seed = 1;
    while (Seed < 150 && !CampTest::RewardRun(Run, Seed)) { ++Seed; Run = FExpeditionRun(); }
    TestTrue(TEXT("Reached a rest"), Seed < 150);
    for (const FCombatantState& U : Run.Battle().Snapshot().Encounter.Combatants)
    {
        if (U.Side != ECombatSide::Party || U.Life != ECombatLife::Conscious) { continue; }
        const bool bHerbalist = Run.Battle().Character(U.Id).IsToolProficient(ETool::HerbalismKit);
        TestEqual(TEXT("Only herbalists brew"), Run.CampActivity(U.Id, ECampActivity::BrewPotion, Outcome, Error), bHerbalist);
        if (!bHerbalist) { TestTrue(TEXT("Explains"), Error.Contains(TEXT("herbalism"))); }
    }
    return true;
}
namespace SneakScaleTest
{
FCombatEncounter Layout(bool bScaled, int32 Level)
{
    const FCombatEncounter Tutorial = FCombatBattle::TutorialEncounter();
    FCombatEncounter L = Tutorial;
    L.BlockedCells.Reset();
    L.bScaledSneakAttack = bScaled;
    L.Combatants = {Tutorial.Combatants[1], Tutorial.Combatants[0], Tutorial.Combatants[4], Tutorial.Combatants[5]};
    FCombatantState& Rogue = L.Combatants[0];
    Rogue.Id = 1; Rogue.Cell = {0, 0}; Rogue.Range = 6; Rogue.AttackBonus = 5; Rogue.InitiativeBonus = 30; Rogue.bSneakWeapon = true; Rogue.Level = Level;
    L.Combatants[1].Id = 2; L.Combatants[1].Cell = {5, 0};
    L.Combatants[2].Id = 3; L.Combatants[2].Cell = {0, 5};
    FCombatantState& Target = L.Combatants[3];
    Target.Id = 4; Target.Cell = {4, 0}; Target.ArmorClass = 12; Target.MaxHP = Target.HP = 200;
    return L;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSchema5ScaledSneakTest, "Project.Expedition.Schema5.ScaledSneakAttack",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSchema5ScaledSneakTest::RunTest(const FString& Parameters)
{
    FString Error;
    FCombatBattle Flat, Scaled;
    TestTrue(TEXT("Flat starts"), Flat.Start(SneakScaleTest::Layout(false, 5), 5, Error));
    TestTrue(TEXT("Scaled starts"), Scaled.Start(SneakScaleTest::Layout(true, 5), 5, Error));
    const FActionForecast A = Flat.Forecast(1, ECombatCommandType::Attack, 4);
    const FActionForecast B = Scaled.Forecast(1, ECombatCommandType::Attack, 4);
    TestTrue(TEXT("Both sneak"), A.bSneakAttack && B.bSneakAttack);
    TestTrue(TEXT("Default stays one die"), A.Formula.Contains(TEXT("+1d6 sneak")));
    TestTrue(TEXT("Level 5 rogue gets 3d6"), B.Formula.Contains(TEXT("+3d6 sneak")));
    TestEqual(TEXT("Two more dice of range"), B.MaxAmount - A.MaxAmount, 12);
    TestEqual(TEXT("Two more minimum"), B.MinAmount - A.MinAmount, 2);
    FCombatBattle Low;
    TestTrue(TEXT("Level 1 starts"), Low.Start(SneakScaleTest::Layout(true, 1), 5, Error));
    TestTrue(TEXT("Level 1 is 1d6"), Low.Forecast(1, ECombatCommandType::Attack, 4).Formula.Contains(TEXT("+1d6 sneak")));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSchema5RangerInitiativeTest, "Project.Expedition.Schema5.RangerInitiative",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSchema5RangerInitiativeTest::RunTest(const FString& Parameters)
{
    // DEX 16 => +3 initiative (SRD 5.2.1); the profile used to say +2.
    for (const TCHAR* Id : {TEXT("ranger.hunter"), TEXT("ranger.beast-master"), TEXT("ranger.fey-wanderer"), TEXT("ranger.swarmkeeper")})
    {
        FCombatantState Ranger;
        TestTrue(FString::Printf(TEXT("%s builds"), Id), FBuildCatalog::Create(Id, 1, TEXT("R"), Ranger));
        TestEqual(FString::Printf(TEXT("%s initiative"), Id), Ranger.InitiativeBonus, 3);
    }
    return true;
}
#endif
