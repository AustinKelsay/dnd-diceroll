#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Combat/CombatBattle.h"
#include "Run/ExpeditionRun.h"
#include "Persistence/ExpeditionSave.h"
#include "Rules/WeaponRules.h"
#include "Rules/ArmorRules.h"
#include "Rules/ClassFeatureRules.h"
#include "Party/BuildCatalog.h"
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
#endif
