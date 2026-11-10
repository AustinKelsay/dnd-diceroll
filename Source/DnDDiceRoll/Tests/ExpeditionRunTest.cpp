#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Run/ExpeditionRun.h"
#include "Persistence/ExpeditionSave.h"

#include "ExpeditionTestSupport.h"
using namespace ExpeditionTestSupport;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRunStartTest, "Project.Expedition.Run.FreshSeededRun",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRunStartTest::RunTest(const FString& Parameters)
{
    FExpeditionRun Run;
    FString Error;
    TestTrue(TEXT("A seed starts a fresh expedition"), Run.NewRun(42, Error, TEXT("seeded-fixture")));
    TestEqual(TEXT("Seed visible"), Run.Progress().Seed, 42);
    TestEqual(TEXT("Two retained opportunities"), Run.Progress().Offers.Num(), 2);
    TestTrue(TEXT("Fresh run waits for a route"), Run.Progress().Phase == EExpeditionPhase::Route);
    return !HasAnyErrors();
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRunRouteTest, "Project.Expedition.Run.RetainedRouteStartsGeneratedCombat",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRunRouteTest::RunTest(const FString& Parameters)
{
    FExpeditionRun Run;
    FString Error;
    Run.NewRun(42, Error, TEXT("seeded-fixture"));
    const auto Route = Run.Progress().Offers[1];
    TestTrue(TEXT("Offered route starts combat"), Run.ChooseRoute(Route.Id, Error));
    TestTrue(TEXT("Combat phase"), Run.Progress().Phase == EExpeditionPhase::Combat);
    if (!TestEqual(TEXT("Selected history retained"), Run.Progress().History.Num(), 1)) { return false; }
    TestEqual(TEXT("Selected seed retained"), Run.Progress().History[0].Route.EncounterSeed, Route.EncounterSeed);
    TestEqual(TEXT("Actual enemy count matches preview"), Run.Battle().Snapshot().Encounter.Combatants.Num(), 4 + Route.EnemyCount);
    return !HasAnyErrors();
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRunLargeGridTest, "Project.Expedition.Run.LargeGridsKeepPlayableOpeningGap",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRunLargeGridTest::RunTest(const FString& Parameters)
{
    TSet<int32> Widths, Heights;
    for (int32 Seed = 0; Seed < 64; ++Seed)
    {
        for (int32 Choice = 0; Choice < 2; ++Choice)
        {
            FExpeditionRun Run; FString Error;
            Run.NewRun(Seed, Error, TEXT("large-grid-fixture"));
            const auto Offer = Run.Progress().Offers[Choice];
            // About three times the former 8-9 x 7-8 area.
            TestTrue(TEXT("Route width 16..18"), Offer.Width >= 16 && Offer.Width <= 18);
            TestTrue(TEXT("Route height 11..12"), Offer.Height >= 11 && Offer.Height <= 12);
            Widths.Add(Offer.Width); Heights.Add(Offer.Height);
            if (!TestTrue(TEXT("Large route starts combat"), Run.ChooseRoute(Offer.Id, Error))) { return false; }
            const auto& Layout = Run.Battle().Snapshot().Encounter;
            TestEqual(TEXT("Layout width matches route"), Layout.Width, Offer.Width);
            TestEqual(TEXT("Layout height matches route"), Layout.Height, Offer.Height);
            int32 Closest = MAX_int32;
            for (const auto& Hero : Layout.Combatants)
            {
                if (Hero.Side != ECombatSide::Party) { continue; }
                for (const auto& Foe : Layout.Combatants)
                {
                    if (Foe.Side == ECombatSide::Enemy) { Closest = FMath::Min(Closest, Distance(Hero.Cell, Foe.Cell)); }
                }
            }
            // Speed 4 closes this within about a turn; the lines sit mid-board with room behind both.
            TestTrue(TEXT("Opening gap 5..7 cells"), Closest >= 5 && Closest <= 7);
            TestTrue(TEXT("Obstacles scale with the board"), Layout.BlockedCells.Num() >= 8);
        }
    }
    TestEqual(TEXT("Every width occurs"), Widths.Num(), 3);
    TestEqual(TEXT("Every height occurs"), Heights.Num(), 2);
    return !HasAnyErrors();
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFiveEncounterRunTest, "Project.Expedition.Run.FiveVictoriesRecoverArtifact",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FFiveEncounterRunTest::RunTest(const FString& Parameters)
{
    FExpeditionRun Run;
    FString Error;
    Run.NewRun(42, Error, TEXT("seeded-fixture"));
    for (int32 Encounter = 0; Encounter < 5; ++Encounter)
    {
        if (Encounter == 4)
        {
            TestEqual(TEXT("Finale metadata offers no consumable loot"), Run.Progress().Offers[0].RewardPotions, 0);
            TestTrue(TEXT("Finale preview discloses artifact instead of loot"), Run.Progress().Offers[0].Preview.Contains(TEXT("no loot")));
        }
        TestTrue(TEXT("Choose guarded opportunity"), Run.ChooseRoute(Run.Progress().Offers[0].Id, Error));
        if (!TestTrue(TEXT("Public play completes within finite turn budget"), FinishCombat(Run))) { return false; }
        TestEqual(TEXT("Exactly one completion per victory"), Run.Progress().EncountersCompleted, Encounter + 1);
        for (const auto& Unit : Run.Battle().Snapshot().Encounter.Combatants)
        {
            if (Unit.Side == ECombatSide::Party && Unit.Life == ECombatLife::Conscious) { TestEqual(TEXT("Survivor milestone level"), Unit.Level, FMath::Min(5, Encounter + 2)); }
        }
        if (Encounter < 4)
        {
            if (!TestTrue(TEXT("Victory requires explicit reward"), Run.Progress().Phase == EExpeditionPhase::Reward)) { return false; }
            TestTrue(TEXT("Reward conscious fighter"), Run.ClaimReward(1, Error));
            FExpeditionRun Restored;
            TestTrue(TEXT("All four awards round-trip including capped gear"), Restored.Restore(Run.Snapshot(), Error));
            TestEqual(TEXT("Gear stays capped"), Run.Battle().Find(1)->WeaponRank, FMath::Min(3, Encounter + 1));
            if (Encounter == 0) { TestTrue(TEXT("One camp opportunity"), Run.Progress().Phase == EExpeditionPhase::Recovery); TestTrue(TEXT("Take explicit recovery"), Run.Recover(true, Error)); }
        }
    }
    TestTrue(TEXT("Artifact and terminal victory after exactly five encounters"), Run.Progress().Phase == EExpeditionPhase::Victory && Run.Progress().bArtifactRecovered);
    TestEqual(TEXT("Five selected encounters retained"), Run.Progress().History.Num(), 5);
    const auto Won = Run.Snapshot();
    TestFalse(TEXT("No sixth encounter"), Run.ChooseRoute(11, Error));
    TestFalse(TEXT("No terminal loot"), Run.ClaimReward(1, Error));
    TestFalse(TEXT("No terminal recovery"), Run.Recover(true, Error));
    Run.Submit({ECombatCommandType::EndTurn,1}); Run.PlayEnemyTurn();
    TestTrue(TEXT("Victory terminal is atomic"), SameRun(Won, Run.Snapshot()));
    FExpeditionRun Fresh; Fresh.NewRun(42, Error, TEXT("seeded-fixture")); Run.NewRun(42, Error, TEXT("seeded-fixture"));
    TestTrue(TEXT("New run resets earned power, resources, history and RNG to fresh seed"), SameRun(Fresh.Snapshot(), Run.Snapshot()));
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeathLevelHistoryTest, "Project.Expedition.Run.DeathLevelRespectsEarnedHistory",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FDeathLevelHistoryTest::RunTest(const FString& Parameters)
{
    FExpeditionRun Run;
    FString Error;
    if (!Run.NewRun(1, Error, TEXT("death-level-history")) ||
        !Run.ChooseRoute(Run.Progress().Offers[0].Id, Error) || !FinishCombat(Run)) { return false; }
    if (!TestTrue(TEXT("First victory retains all four adventurers"),
        Run.Progress().Phase == EExpeditionPhase::Reward && Run.Progress().History[0].Survivors == 4)) { return false; }
    if (!Run.ClaimReward(1, Error) || !Run.Recover(false, Error) ||
        !Run.ChooseRoute(Run.Progress().Offers[1].Id, Error) || !FinishCombat(Run, false)) { return false; }
    if (!TestTrue(TEXT("Second encounter ends in a permanent wipe"), Run.Progress().Phase == EExpeditionPhase::Defeat)) { return false; }
    const auto Earned = Run.Snapshot();
    FExpeditionRun Restored;
    if (!TestTrue(TEXT("Actual death-level snapshot restores"), Restored.Restore(Earned, Error))) { return false; }
    TestEqual(TEXT("Fighter died after earning level two"), Restored.Battle().Find(1)->Level, 2);
    auto Contradictory = Earned;
    Contradictory.Battle.Encounter.Combatants[0].Level = 1;
    --Contradictory.Battle.Encounter.Combatants[0].DamageBonus;
    TestFalse(TEXT("Retained all-survivor victory forbids lowering a death-level"), Restored.Restore(Contradictory, Error));
    TestTrue(TEXT("Rejected restore leaves the installed expedition unchanged"), SameRun(Earned, Restored.Snapshot()));
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRunGenerationTest, "Project.Expedition.Run.ManySeedsConnectedActionableAndVaried",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRunGenerationTest::RunTest(const FString& Parameters)
{
    TSet<FString> Variations;
    for (int32 Seed = -32; Seed < 96; ++Seed)
    {
        for (int32 Choice = 0; Choice < 2; ++Choice)
        {
            FExpeditionRun Run, Replay;
            FString Error;
            Run.NewRun(Seed, Error, TEXT("seeded-fixture")); Replay.NewRun(Seed, Error, TEXT("seeded-fixture"));
            const auto BeforeQuery = Run.Snapshot();
            const auto Offer = Run.Progress().Offers[Choice];
            Run.Snapshot(); Run.Progress(); Run.Battle().LegalMoves(Run.Battle().ActiveId());
            TestTrue(TEXT("Queries retain offers and all RNG"), SameRun(BeforeQuery, Run.Snapshot()));
            TestTrue(TEXT("Valid generated encounter"), Run.ChooseRoute(Offer.Id, Error));
            Replay.ChooseRoute(Offer.Id, Error);
            TestTrue(TEXT("Seed reproduces selected opportunity, layout and initiative"), SameRun(Run.Snapshot(), Replay.Snapshot()));
            const auto Layout = Run.Battle().Snapshot().Encounter;
            TSet<FIntPoint> Spawns;
            for (const auto& Unit : Layout.Combatants)
            {
                TestFalse(TEXT("Unique spawn"), Spawns.Contains(Unit.Cell)); Spawns.Add(Unit.Cell);
                TestFalse(TEXT("Walkable spawn"), Layout.BlockedCells.Contains(Unit.Cell));
                TestTrue(TEXT("Inside grid"), Unit.Cell.X >= 0 && Unit.Cell.Y >= 0 && Unit.Cell.X < Layout.Width && Unit.Cell.Y < Layout.Height);
                // Flood actual navigable cells with other bodies occupied. Every spawn can
                // reach a firing position next to every initial opposing spawn.
                TSet<FIntPoint> Reached; TArray<FIntPoint> Queue = {Unit.Cell}; Reached.Add(Unit.Cell);
                for (int32 CellIndex = 0; CellIndex < Queue.Num(); ++CellIndex)
                {
                    for (FIntPoint Offset : {FIntPoint(1,0), FIntPoint(-1,0), FIntPoint(0,1), FIntPoint(0,-1)})
                    {
                        const auto Next = Queue[CellIndex] + Offset;
                        const bool bOccupied = Layout.Combatants.ContainsByPredicate([&](const FCombatantState& Other) { return Other.Id != Unit.Id && Other.Cell == Next; });
                        if (Next.X >= 0 && Next.Y >= 0 && Next.X < Layout.Width && Next.Y < Layout.Height && !bOccupied && !Layout.BlockedCells.Contains(Next) && !Reached.Contains(Next))
                        { Reached.Add(Next); Queue.Add(Next); }
                    }
                }
                for (const auto& Target : Layout.Combatants)
                {
                    if (Target.Side == Unit.Side) { continue; }
                    TestTrue(TEXT("Can reach an adjacent attack cell for every opponent"), Queue.ContainsByPredicate([&](FIntPoint Cell) { return Distance(Cell, Target.Cell) == 1; }));
                }
            }
            // Floor itself is one connected region, independently of spawn occupancy.
            TSet<FIntPoint> Floor; TArray<FIntPoint> Queue = {{0,0}}; Floor.Add({0,0});
            for (int32 Index = 0; Index < Queue.Num(); ++Index)
            {
                for (FIntPoint Offset : {FIntPoint(1,0), FIntPoint(-1,0), FIntPoint(0,1), FIntPoint(0,-1)})
                {
                    const auto Next = Queue[Index] + Offset;
                    if (Next.X >= 0 && Next.Y >= 0 && Next.X < Layout.Width && Next.Y < Layout.Height && !Layout.BlockedCells.Contains(Next) && !Floor.Contains(Next))
                    { Floor.Add(Next); Queue.Add(Next); }
                }
            }
            TestEqual(TEXT("All walkable terrain connected"), Floor.Num(), Layout.Width * Layout.Height - Layout.BlockedCells.Num());
            Variations.Add(FString::Printf(TEXT("%d-%d-%d-%d-%d"), Layout.Width, Layout.Height, Offer.Recipe, Offer.EnemyCount, Layout.Combatants.Last().Cell.Y));
            if (Choice == 0 && Seed >= 0 && Seed < 16)
            {
                TestTrue(TEXT("AI/player play finite on generated terrain"), FinishCombat(Run));
                TestTrue(TEXT("Replay finite"), FinishCombat(Replay));
                TestTrue(TEXT("Same seed and commands reproduce full combat RNG/results"), SameRun(Run.Snapshot(), Replay.Snapshot()));
                for (int32 Next = 1; Next < 5 && Run.Progress().Phase == EExpeditionPhase::Reward; ++Next)
                {
                    int32 Recipient = INDEX_NONE;
                    for (const auto& Unit : Run.Battle().Snapshot().Encounter.Combatants)
                    { if (Unit.Side == ECombatSide::Party && Unit.Life == ECombatLife::Conscious) { Recipient = Unit.Id; break; } }
                    Run.ClaimReward(Recipient, Error); Replay.ClaimReward(Recipient, Error);
                    if (Next == 1) { Run.Recover(Seed % 2 == 0, Error); Replay.Recover(Seed % 2 == 0, Error); }
                    const int32 RouteId = Run.Progress().Offers[Seed % 2].Id;
                    TestTrue(TEXT("Next retained route valid"), Run.ChooseRoute(RouteId, Error));
                    TestTrue(TEXT("Replay next retained route valid"), Replay.ChooseRoute(RouteId, Error));
                    TestTrue(TEXT("Whole-run generated play finite"), FinishCombat(Run));
                    TestTrue(TEXT("Whole-run replay finite"), FinishCombat(Replay));
                    TestTrue(TEXT("Full seeded command sequence preserves every value and both streams"), SameRun(Run.Snapshot(), Replay.Snapshot()));
                }
                TestTrue(TEXT("Whole expedition terminates within five combats"), Run.Progress().Phase == EExpeditionPhase::Victory || Run.Progress().Phase == EExpeditionPhase::Defeat);
            }
        }
    }
    TestTrue(TEXT("Bounded terrain, enemy groups and placements vary"), Variations.Num() >= 12);
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRunCommandAtomicityTest, "Project.Expedition.Run.PhasesAtomicityRewardsCampAndHistory",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRunCommandAtomicityTest::RunTest(const FString& Parameters)
{
    FExpeditionRun Run; FString Error;
    Run.NewRun(42, Error, TEXT("seeded-fixture"));
    auto Before = Run.Snapshot();
    TestFalse(TEXT("Unknown route rejected"), Run.ChooseRoute(999, Error));
    TestFalse(TEXT("No loot before victory"), Run.ClaimReward(1, Error));
    TestFalse(TEXT("No early camp"), Run.Recover(true, Error));
    TestFalse(TEXT("No combat before route choice"), Run.Submit({ECombatCommandType::EndTurn, Run.Battle().ActiveId()}).bAccepted);
    TestTrue(TEXT("No enemy play outside combat"), Run.PlayEnemyTurn().IsEmpty());
    TestTrue(TEXT("All early rejection preserves complete snapshot/RNG"), SameRun(Before, Run.Snapshot()));
    const auto Chosen = Run.Progress().Offers[1]; Run.ChooseRoute(Chosen.Id, Error);
    Before = Run.Snapshot();
    TestFalse(TEXT("Cannot reroute in combat"), Run.ChooseRoute(Chosen.Id, Error));
    TestFalse(TEXT("Cannot take combat camp"), Run.Recover(true, Error));
    TestFalse(TEXT("Cannot claim combat loot"), Run.ClaimReward(1, Error));
    TestFalse(TEXT("Invalid combat command"), Run.Submit({ECombatCommandType::Attack,999,{},20}).bAccepted);
    TestTrue(TEXT("Rejected combat phase commands atomic"), SameRun(Before, Run.Snapshot()));
    if (!TestTrue(TEXT("Dangerous encounter completed"), FinishCombat(Run))) { return false; }
    if (!TestTrue(TEXT("Won"), Run.Progress().Phase == EExpeditionPhase::Reward)) { return false; }
    Before = Run.Snapshot();
    TestFalse(TEXT("No route before reward"), Run.ChooseRoute(3, Error));
    TestFalse(TEXT("No camp before reward"), Run.Recover(true, Error));
    TestFalse(TEXT("Enemy cannot receive reward"), Run.ClaimReward(20, Error));
    TestFalse(TEXT("Unknown cannot receive reward"), Run.ClaimReward(999, Error));
    TestTrue(TEXT("Invalid recipients preserve all values/RNG"), SameRun(Before, Run.Snapshot()));
    const auto Fighter = *Run.Battle().Find(1);
    TestTrue(TEXT("Selected survivor rewarded"), Run.ClaimReward(1, Error));
    TestEqual(TEXT("One gear rank"), Run.Battle().Find(1)->WeaponRank, Fighter.WeaponRank+1);
    TestEqual(TEXT("Danger disclosed two potions"), Run.Battle().Find(1)->Potions, Fighter.Potions+2);
    TestEqual(TEXT("Recipient retained"), Run.Progress().History.Last().RewardRecipient, 1);
    Before = Run.Snapshot();
    TestFalse(TEXT("Repeated loot rejected"), Run.ClaimReward(2, Error));
    TestFalse(TEXT("Route cannot skip camp decision"), Run.ChooseRoute(3, Error));
    TestTrue(TEXT("Repeated loot/skip atomic"), SameRun(Before, Run.Snapshot()));
    TestTrue(TEXT("Recovery taken"), Run.Recover(true, Error));
    for (const auto& Unit : Before.Battle.Encounter.Combatants)
    {
        if (Unit.Side != ECombatSide::Party) { continue; }
        const auto& After = *Run.Battle().Find(Unit.Id);
        TestEqual(TEXT("Camp health exactly capped +10"), After.HP, FMath::Min(Unit.MaxHP, Unit.HP+10));
        TestEqual(TEXT("Camp never adds potions"), After.Potions, Unit.Potions);
        TestEqual(TEXT("Camp never adds gear"), After.WeaponRank, Unit.WeaponRank);
        if (Unit.AdventurerRole == EAdventurerRole::Wizard || Unit.AdventurerRole == EAdventurerRole::Cleric)
        { TestEqual(TEXT("One capped slot"), After.SpellSlots, FMath::Min(2, Unit.SpellSlots+1)); }
    }
    Before = Run.Snapshot();
    TestFalse(TEXT("Camp cannot repeat"), Run.Recover(true, Error));
    TestFalse(TEXT("Used camp cannot be skipped again"), Run.Recover(false, Error));
    TestTrue(TEXT("Repeated camp preserves RNG and entire state"), SameRun(Before, Run.Snapshot()));
    TestTrue(TEXT("History concretely secures next guarded route supplies"), Run.Progress().Offers[0].RewardPotions==2 && Run.Progress().Offers[0].Preview.Contains(TEXT("secured supplies")));
    Run.ChooseRoute(Run.Progress().Offers[0].Id, Error);
    for (const auto& Unit : Before.Battle.Encounter.Combatants)
    {
        if (Unit.Side != ECombatSide::Party) { continue; }
        const auto& After = *Run.Battle().Find(Unit.Id);
        TestTrue(TEXT("Route carries exact persistent values, no hidden rest"), After.Id==Unit.Id && After.HP==Unit.HP && After.Life==Unit.Life && After.Potions==Unit.Potions && After.WeaponRank==Unit.WeaponRank && After.SpellSlots==Unit.SpellSlots && After.SecondWindUses==Unit.SecondWindUses);
    }
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRunLossAndResetTest, "Project.Expedition.Run.WipeAndFreshReset",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRunLossAndResetTest::RunTest(const FString& Parameters)
{
    FExpeditionRun Run; FString Error; Run.NewRun(7, Error, TEXT("seeded-fixture"));
    Run.ChooseRoute(Run.Progress().Offers[1].Id,Error);
    TestTrue(TEXT("Skipping attacks reaches finite wipe"),FinishCombat(Run,false));
    TestTrue(TEXT("Wipe is terminal run defeat"),Run.Progress().Phase==EExpeditionPhase::Defeat && !Run.Progress().bArtifactRecovered);
    TestTrue(TEXT("Loss recorded"),Run.Progress().History.Last().Outcome==EBattlePhase::Defeat && Run.Progress().History.Last().Survivors==0);
    const auto Lost=Run.Snapshot();
    TestFalse(TEXT("No continuing loss"),Run.ChooseRoute(1,Error)); TestFalse(TEXT("No loss loot"),Run.ClaimReward(1,Error)); TestFalse(TEXT("No loss resurrection"),Run.Recover(true,Error));
    Run.Submit({ECombatCommandType::EndTurn,1}); Run.PlayEnemyTurn();
    TestTrue(TEXT("Terminal commands atomic"),SameRun(Lost,Run.Snapshot()));
    TestTrue(TEXT("New seed starts fresh"),Run.NewRun(8, Error, TEXT("reset-fixture")));
    TestTrue(TEXT("New identity, empty history, no camp or artifact"),Run.Progress().Seed==8 && Run.Progress().RunId!=Lost.Progress.RunId && Run.Progress().History.IsEmpty() && !Run.Progress().bCampUsed && !Run.Progress().bArtifactRecovered);
    for(const auto& Unit:Run.Battle().Snapshot().Encounter.Combatants)
    {
        if(Unit.Side!=ECombatSide::Party){continue;}
        TestTrue(TEXT("Fresh living full-health ungeared party with starting potion"),Unit.Life==ECombatLife::Conscious && Unit.HP==Unit.MaxHP && Unit.WeaponRank==0 && Unit.Potions==1);
        if(Unit.AdventurerRole==EAdventurerRole::Fighter){TestEqual(TEXT("Fresh Wind"),Unit.SecondWindUses,2);}
        if(Unit.AdventurerRole==EAdventurerRole::Wizard || Unit.AdventurerRole==EAdventurerRole::Cleric){TestEqual(TEXT("Fresh slots"),Unit.SpellSlots,2);}
    }
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRunSurvivorTest, "Project.Expedition.Run.DeadRosterSurvivesRewardCampAndContinuation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRunSurvivorTest::RunTest(const FString& Parameters)
{
    FExpeditionRun Run; FString Error; Run.NewRun(42, Error, TEXT("seeded-fixture")); Run.ChooseRoute(Run.Progress().Offers[0].Id,Error);
    for(int32 Turn=0;Turn<120 && Run.Progress().Phase==EExpeditionPhase::Combat && Run.Battle().Find(1)->Life!=ECombatLife::Dead;++Turn)
    {
        const auto* Active=Run.Battle().Find(Run.Battle().ActiveId());
        if(Active->Side==ECombatSide::Enemy){Run.PlayEnemyTurn();}
        else{Run.Submit({ECombatCommandType::EndTurn,Active->Id});}
    }
    if(!TestTrue(TEXT("Actual first death with surviving party"),Run.Battle().Find(1)->Life==ECombatLife::Dead && Run.Progress().Phase==EExpeditionPhase::Combat)){return false;}
    TestTrue(TEXT("Shrinking party can complete encounter"),FinishCombat(Run));
    if(!TestTrue(TEXT("Survivor victory"),Run.Progress().Phase==EExpeditionPhase::Reward)){return false;}
    auto Before=Run.Snapshot();
    TestFalse(TEXT("Dead cannot receive loot"),Run.ClaimReward(1,Error)); TestTrue(TEXT("Dead loot rejection atomic"),SameRun(Before,Run.Snapshot()));
    int32 Recipient=INDEX_NONE;
    for(const auto& Unit:Run.Battle().Snapshot().Encounter.Combatants){if(Unit.Side==ECombatSide::Party && Unit.Life==ECombatLife::Conscious){Recipient=Unit.Id;break;}}
    Run.ClaimReward(Recipient,Error); Run.Recover(true,Error);
    TestTrue(TEXT("Camp cannot revive permanent death"),Run.Battle().Find(1)->Life==ECombatLife::Dead && Run.Battle().Find(1)->HP==0 && Run.Battle().Find(1)->WeaponRank==0 && Run.Battle().Find(1)->Potions==1);
    TestTrue(TEXT("Continue with full stable roster"),Run.ChooseRoute(Run.Progress().Offers[0].Id,Error));
    TestTrue(TEXT("Dead remains present and excluded from turns"),Run.Battle().Find(1)->Life==ECombatLife::Dead && !Run.Battle().Snapshot().TurnOrder.Contains(1));
    const auto DeathLevel = Run.Snapshot();
    FExpeditionRun Restored;
    if (!TestTrue(TEXT("Actual first-encounter death restores during encounter two"), Restored.Restore(DeathLevel, Error))) { return false; }
    auto AdvancedDead = DeathLevel;
    AdvancedDead.Battle.Encounter.Combatants[0].Level = 2;
    ++AdvancedDead.Battle.Encounter.Combatants[0].DamageBonus;
    TestFalse(TEXT("Retained survivor losses forbid advancing an earlier dead adventurer"), Restored.Restore(AdvancedDead, Error));
    TestTrue(TEXT("Contradictory death distribution leaves the installed run unchanged"), SameRun(DeathLevel, Restored.Snapshot()));
    return !HasAnyErrors();
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSkipRecoveryTest, "Project.Expedition.Run.SkippedRecoveryHasNoRestAndCannotReturn",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSkipRecoveryTest::RunTest(const FString& Parameters)
{
    FExpeditionRun Run; FString Error;
    Run.NewRun(42, Error, TEXT("seeded-fixture")); Run.ChooseRoute(Run.Progress().Offers[0].Id, Error);
    FinishCombat(Run); Run.ClaimReward(1, Error);
    const auto Before = Run.Battle().Snapshot();
    TestTrue(TEXT("Recovery can be explicitly skipped"), Run.Recover(false, Error));
    TestTrue(TEXT("Skip has no battle/rest mutation"), FCombatSnapshot::StaticStruct()->CompareScriptStruct(&Before, &Run.Battle().Snapshot(), 0));
    TestTrue(TEXT("Skip recorded permanently"), Run.Progress().bCampResolved && !Run.Progress().bCampUsed);
    const auto Skipped = Run.Snapshot();
    TestFalse(TEXT("Skipped opportunity cannot be used later"), Run.Recover(true, Error));
    TestTrue(TEXT("Skip rejection atomic"), SameRun(Skipped, Run.Snapshot()));
    TestEqual(TEXT("Guarded history does not grant extra supplies"), Run.Progress().Offers[0].RewardPotions, 1);
    return !HasAnyErrors();
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCompactEnemyTuningTest, "Project.Expedition.Run.CompactEnemyTuning",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FCompactEnemyTuningTest::RunTest(const FString& Parameters)
{
    // Catalog revision 8 (#184): the Compact enemy stats by stage, for each route, as the run generates them. Counts,
    // roles and ranges are unchanged; SRD keeps revision 7 (Project.Expedition.Run.SrdSeedUnchangedByCompactTuning).
    struct FExpected { int32 HP, AttackBonus, DamageBonus; };
    const FExpected Guarded[] = {{20, 4, 3}, {24, 4, 3}, {28, 5, 3}, {32, 5, 4}, {36, 5, 4}};
    const FExpected Dangerous[] = {{24, 4, 3}, {28, 4, 3}, {32, 5, 3}, {36, 5, 4}, {40, 5, 4}};
    for (const int32 Choice : {0, 1})
    {
        // The enemies of every fight, taking route Choice each time; a run that falls before fight 5 records fewer.
        auto Play = [Choice](int32 Seed, TArray<TArray<FCombatantState>>& Fights)
        {
            FExpeditionRun Run; FString Error; Run.NewRun(Seed, Error, TEXT("tuning-fixture"));
            for (int32 Guard = 0; Guard < 64; ++Guard)
            {
                const auto Phase = Run.Progress().Phase;
                if (Phase == EExpeditionPhase::Victory || Phase == EExpeditionPhase::Defeat) { break; }
                if (Phase == EExpeditionPhase::Route)
                {
                    if (!Run.ChooseRoute(Run.Progress().Offers[Choice].Id, Error)) { break; }
                    Fights.Add(Run.Battle().Snapshot().Encounter.Combatants.FilterByPredicate([](const FCombatantState& U) { return U.Side == ECombatSide::Enemy; }));
                }
                else if (Phase == EExpeditionPhase::Combat) { if (!FinishCombat(Run)) { break; } }
                else if (Phase == EExpeditionPhase::Reward)
                {
                    int32 Survivor = 1; while (Survivor <= 4 && Run.Battle().Find(Survivor)->Life != ECombatLife::Conscious) { ++Survivor; }
                    if (Survivor > 4 || !Run.ClaimReward(Survivor, Error)) { break; }
                }
                else if (!Run.Recover(true, Error)) { break; }
            }
        };
        const int32 Seed = FirstSeedWhere([&Play](int32 Candidate) { TArray<TArray<FCombatantState>> Fights; Play(Candidate, Fights); return Fights.Num() == 5; }, 42, 200);
        if (!TestTrue(TEXT("Some seed reaches all five fights on this route"), Seed != INDEX_NONE)) { continue; }
        TArray<TArray<FCombatantState>> Fights;
        Play(Seed, Fights);
        for (int32 Stage = 0; Stage < Fights.Num(); ++Stage)
        {
            const FExpected& E = (Choice == 1 ? Dangerous : Guarded)[Stage];
            const FString Where = FString::Printf(TEXT("%s fight %d"), Choice == 1 ? TEXT("Dangerous") : TEXT("Guarded"), Stage + 1);
            TestEqual(Where + TEXT(": enemy count unchanged"), Fights[Stage].Num(), Choice == 1 ? 3 : 2);
            for (int32 I = 0; I < Fights[Stage].Num(); ++I)
            {
                const FCombatantState& U = Fights[Stage][I];
                TestEqual(Where + TEXT(": HP"), U.MaxHP, E.HP);
                TestEqual(Where + TEXT(": armour"), U.ArmorClass, Choice == 1 ? 13 : 12);
                TestEqual(Where + TEXT(": attack bonus"), U.AttackBonus, E.AttackBonus);
                TestEqual(Where + TEXT(": damage die"), U.DamageDie, Choice == 1 ? 8 : 6);
                TestEqual(Where + TEXT(": damage bonus"), U.DamageBonus, E.DamageBonus);
                TestEqual(Where + TEXT(": the last enemy is the archer, the rest raiders"), U.Range, I == Fights[Stage].Num() - 1 ? 4 : 1);
            }
        }
    }
    // A revision 7 Compact run keeps revision 7 enemies for its whole lifetime: the format 4 fixture saved between
    // battles (after fight 1) generates its next fight with the old stats and keeps its revision.
    FString Error;
    FExpeditionSaveData Fixture;
    if (TestTrue(TEXT("Read the revision 7 fixture"), FExpeditionSave::Read(FPaths::ProjectDir() / TEXT("Source/DnDDiceRoll/Tests/Fixtures/Schema4/between-battles-v4.json"), Fixture, Error)))
    {
        FExpeditionRun Old;
        TestTrue(TEXT("Restore it"), Old.Restore(Fixture.Current, Error));
        TestEqual(TEXT("It is a revision 7 run"), Old.Progress().CatalogRevision, 7);
        TestTrue(TEXT("Choose its Dangerous road"), Old.ChooseRoute(Old.Progress().Offers[1].Id, Error));
        for (const auto& U : Old.Battle().Snapshot().Encounter.Combatants)
        {
            if (U.Side != ECombatSide::Enemy) { continue; }
            TestTrue(TEXT("Revision 7 Dangerous fight 2 enemy stats"), U.MaxHP == 14 && U.ArmorClass == 12 && U.AttackBonus == 2 && U.DamageDie == 6 && U.DamageBonus == 1);
        }
        TestEqual(TEXT("Still revision 7"), Old.Progress().CatalogRevision, 7);
    }
    // New runs are revision 8; a revision outside 7..8 is refused by name and changes nothing.
    FExpeditionRun Run; Run.NewRun(42, Error, TEXT("tuning-fixture"));
    TestEqual(TEXT("New runs use revision 8"), Run.Progress().CatalogRevision, 8);
    const FExpeditionSnapshot Before = Run.Snapshot();
    for (const int32 Revision : {6, 9})
    {
        FExpeditionSnapshot Unknown = Run.Snapshot(); Unknown.Progress.CatalogRevision = Revision;
        TestFalse(FString::Printf(TEXT("Revision %d refused"), Revision), Run.Restore(Unknown, Error));
        TestTrue(TEXT("The refusal names the revision and the supported ones: ") + Error,
            Error.Contains(FString::Printf(TEXT("revision %d;"), Revision)) && Error.Contains(TEXT("revisions 7 to 8")));
        TestTrue(TEXT("Refusal leaves the run unchanged"), SameRun(Before, Run.Snapshot()));
    }
    return !HasAnyErrors();
}
#endif
