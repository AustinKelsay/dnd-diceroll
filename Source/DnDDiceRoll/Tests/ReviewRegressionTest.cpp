#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Templates/Function.h"
#include "Combat/CombatBattle.h"
#include "Party/BuildCatalog.h"
#include "Rules/AbilityRules.h"
#include "Rules/ProficiencyRules.h"
#include "Rules/OpportunityRules.h"
#include "Persistence/ExpeditionSave.h"
#include "ExpeditionTestSupport.h"
namespace
{
FCombatEncounter ReviewDuo()
{
    FCombatEncounter E; E.Ruleset = ERuleset::SRD;
    FCombatantState Hero; FBuildCatalog::Create(TEXT("fighter.champion"), 1, TEXT("Arden"), Hero); Hero.Cell = {1, 1};
    FCombatantState Foe; Foe.Id = 2; Foe.Name = TEXT("Raider"); Foe.Role = TEXT("Raider"); Foe.Side = ECombatSide::Enemy;
    Foe.Cell = {2, 1}; Foe.HP = Foe.MaxHP = 40; Foe.AttackBonus = 30; Foe.DamageDie = 1; Foe.DamageBonus = 0;
    E.Combatants = {Hero, Foe}; return E;
}
bool Activate(FCombatBattle& B, int32 Id)
{
    for (int32 I = 0; I < 4 && B.ActiveId() != Id; ++I) { if (!B.Submit({ECombatCommandType::EndTurn, B.ActiveId()}).bAccepted) { return false; } }
    return B.ActiveId() == Id;
}
/** A random state whose next d20 satisfies Accept, so a test can pick the roll it needs. */
int32 StateForD20(TFunctionRef<bool(int32)> Accept)
{
    for (int32 Seed = 1; Seed < 100000; ++Seed) { FRandomStream R(Seed); if (Accept(R.RandRange(1, 20))) { return Seed; } }
    return 0;
}
/** One-row corridor: walking {0,0} to {4,0} leaves the foe's reach when stepping off {2,0}. */
FCombatEncounter Corridor(bool bAlly)
{
    FCombatEncounter E; E.Ruleset = ERuleset::SRD; E.Width = 6; E.Height = 2;
    FCombatantState Hero; FBuildCatalog::Create(TEXT("fighter.champion"), 1, TEXT("Arden"), Hero);
    Hero.Cell = {0, 0}; Hero.MaxHP = 20; Hero.HP = 1; Hero.ArmorClass = 12; Hero.Speed = 6;
    FCombatantState Foe; Foe.Id = 2; Foe.Name = TEXT("Raider"); Foe.Role = TEXT("Raider"); Foe.Side = ECombatSide::Enemy;
    Foe.Cell = {2, 1}; Foe.HP = Foe.MaxHP = 40; Foe.AttackBonus = 30; Foe.DamageDie = 4; Foe.DamageBonus = 0;
    E.Combatants = {Hero, Foe};
    if (bAlly) { FCombatantState Ally; FBuildCatalog::Create(TEXT("rogue.thief"), 3, TEXT("Nix"), Ally); Ally.Cell = {5, 1}; E.Combatants.Add(Ally); }
    return E;
}
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReviewSavedFieldsTest, "Project.Expedition.Persistence.ReviewSavedFieldValidation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FReviewSavedFieldsTest::RunTest(const FString& Parameters)
{
    FString Error; FExpeditionRun Run;
    if (!TestTrue(TEXT("Start"), Run.NewRun(42, Error, TEXT("validation")))) { return false; }
    const auto Good = Run.Snapshot();
    for (int32 Case = 0; Case < 9; ++Case)
    {
        auto Bad = Good; auto& U = Bad.Battle.Encounter.Combatants[0];
        switch (Case)
        {
        case 0: Bad.Progress.Ruleset = static_cast<ERuleset>(2); break;
        case 1: Bad.Battle.Encounter.Ruleset = static_cast<ERuleset>(2); break;
        case 2: U.Size = -1; break;
        case 3: U.Size = 6; break;
        case 4: U.TemporaryHP = -1; break;
        case 5: U.GrapplerId = 9999; break;
        case 6: U.GrapplerId = U.Id; break;
        case 7: Bad.Battle.Encounter.Ruleset = ERuleset::SRD; break;
        case 8: U.DeathSaveFailures = -1; break;
        }
        TestFalse(FString::Printf(TEXT("Reject invalid field %d"), Case), Run.Restore(Bad, Error));
        TestTrue(TEXT("Atomic restore"), ExpeditionTestSupport::SameRun(Good, Run.Snapshot()));
    }
    FExpeditionSaveData Output; Output.Current = Good;
    TestFalse(TEXT("Missing Current rejected without Json warning"), FExpeditionSave::Decode(TEXT("{\"Version\":4,\"Records\":[]}"), Output, Error));
    TestFalse(TEXT("Missing Progress rejected without Json warning"), FExpeditionSave::Decode(TEXT("{\"Version\":4,\"Current\":{},\"Records\":[]}"), Output, Error));
    TestFalse(TEXT("Fractional version cannot migrate"), FExpeditionSave::Decode(TEXT("{\"Version\":4.5,\"Records\":[]}"), Output, Error));
    TestTrue(TEXT("Decode leaves output untouched"), ExpeditionTestSupport::SameRun(Good, Output.Current));
    // An SRD checkpoint taken mid-battle must round-trip through strict save validation.
    FExpeditionRun Srd;
    if (!TestTrue(TEXT("SRD run"), Srd.NewRun(42, Error, TEXT("srd-mid"), EFighterSubclass::Champion, ERuleset::SRD))) { return false; }
    TestTrue(TEXT("SRD route"), Srd.ChooseRoute(Srd.Progress().Offers[0].Id, Error));
    FExpeditionSaveData SrdSave; SrdSave.Current = Srd.Snapshot(); FString SrdJson;
    TestTrue(FString::Printf(TEXT("Encode SRD mid-battle: %s"), *Error), FExpeditionSave::Encode(SrdSave, SrdJson, Error));
    FExpeditionSaveData SrdLoaded;
    TestTrue(FString::Printf(TEXT("Decode SRD mid-battle: %s"), *Error), FExpeditionSave::Decode(SrdJson, SrdLoaded, Error));
    TestTrue(TEXT("SRD mid-battle identical"), ExpeditionTestSupport::SameRun(SrdSave.Current, SrdLoaded.Current));
    return !HasAnyErrors();
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReviewSurpriseTest, "Project.Expedition.Combat.SurpriseActsInRoundOne", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FReviewSurpriseTest::RunTest(const FString& Parameters)
{
    FString Error; auto E = ReviewDuo(); E.bAmbush = true; E.AmbushStealthTotals = {100, 100, 100, 100};
    FCombatBattle B;
    if (!TestTrue(TEXT("Ambush starts"), B.Start(E, 42, Error))) { return false; }
    FRandomStream R(42); const int32 HeroDie = R.RandRange(1, 20), A = R.RandRange(1, 20), C = R.RandRange(1, 20);
    TestEqual(TEXT("Hero normal initiative"), B.Find(1)->Initiative, HeroDie + B.Find(1)->InitiativeBonus);
    TestEqual(TEXT("Surprised initiative uses lower die"), B.Find(2)->Initiative, FMath::Min(A, C) + B.Find(2)->InitiativeBonus);
    TSet<int32> Acted;
    for (int32 I = 0; I < 2; ++I)
    {
        TestEqual(TEXT("Both creatures act in round 1"), B.Snapshot().Round, 1);
        TestTrue(TEXT("Action reset"), B.Snapshot().bActionAvailable);
        TestTrue(TEXT("Bonus reset"), B.Snapshot().bBonusActionAvailable);
        TestEqual(TEXT("Movement reset"), B.Snapshot().MovementLeft, B.Find(B.ActiveId())->Speed);
        Acted.Add(B.ActiveId()); TestTrue(TEXT("Turn accepted"), B.Submit({ECombatCommandType::EndTurn, B.ActiveId()}).bAccepted);
    }
    TestTrue(TEXT("Surprised foe gets its turn"), Acted.Contains(2));
    TestEqual(TEXT("Both acted once"), Acted.Num(), 2);
    return !HasAnyErrors();
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReviewGrappleSaveTest, "Project.Expedition.Combat.GrappleShoveEscape521DC", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FReviewGrappleSaveTest::RunTest(const FString& Parameters)
{
    FString Error; bool Won = false, Saved = false, Escaped = false, Held = false;
    for (int32 Seed = 1; Seed <= 40; ++Seed)
    {
        for (auto Type : {ECombatCommandType::Grapple, ECombatCommandType::Shove, ECombatCommandType::Escape})
        {
            FCombatBattle B; const auto E = ReviewDuo();
            if (!B.Start(E, Seed, Error) || !Activate(B, 1)) { return false; }
            auto S = B.Snapshot();
            if (Type == ECombatCommandType::Escape) { S.Encounter.Combatants[0].GrapplerId = 2; S.MovementLeft = 0; if (!B.Restore(S, Error)) { return false; } }
            FRandomStream R(B.Snapshot().RandomState); const int32 Die = R.RandRange(1, 20);
            const auto Hero = FProficiencyRules::PrebuiltCharacter(EAdventurerRole::Fighter, 1);
            const auto Foe = FAbilityRules::EnemyScores(TEXT("Raider"));
            const bool IsEscape = Type == ECombatCommandType::Escape;
            const int32 Dc = 8 + (IsEscape ? FAbilityRules::Modifier(Foe.Strength) : Hero.AbilityModifier(EAbility::Strength)) + 2;
            const int32 Mod = IsEscape ? FMath::Max(Hero.SkillBonus(ESkill::Athletics), Hero.SkillBonus(ESkill::Acrobatics)) : FMath::Max(FAbilityRules::Modifier(Foe.Strength), FAbilityRules::Modifier(Foe.Dexterity));
            const bool Success = IsEscape ? Die + Mod >= Dc : Die + Mod < Dc;
            const auto Result = B.Submit({Type, 1, {}, 2});
            TestTrue(TEXT("Command accepted"), Result.bAccepted);
            TestEqual(TEXT("One d20, no opposed roll"), B.Snapshot().RandomState, R.GetCurrentSeed());
            TestEqual(TEXT("DC 8 + STR + PB"), Result.TargetArmorClass, Dc);
            TestEqual(TEXT("Correct save/check modifier"), Result.AttackBonus, Mod);
            TestEqual(TEXT("Outcome follows DC including ties"), Result.bHit, Success);
            if (IsEscape) { Escaped |= Success; Held |= !Success; TestEqual(TEXT("Escape clears only on success"), B.Find(1)->GrapplerId, Success ? INDEX_NONE : 2); }
            else
            {
                Won |= Success; Saved |= !Success;
                if (Type == ECombatCommandType::Grapple) { TestEqual(TEXT("Grapple only on failed save"), B.Find(2)->GrapplerId, Success ? 1 : INDEX_NONE); }
                else { TestEqual(TEXT("Prone only on failed save"), B.Find(2)->bProne, Success); }
            }
        }
    }
    TestTrue(TEXT("Both save outcomes exercised"), Won && Saved && Escaped && Held);
    return !HasAnyErrors();
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReviewZeroHpDamageTest, "Project.Expedition.Combat.DamageAtZeroKeepsTally", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FReviewZeroHpDamageTest::RunTest(const FString& Parameters)
{
    FString Error; bool Hit = false, Crit = false;
    for (int32 Seed = 1; Seed <= 200 && !(Hit && Crit); ++Seed)
    {
        auto E = ReviewDuo(); FCombatantState Ally;
        FBuildCatalog::Create(TEXT("rogue.thief"), 3, TEXT("Nix"), Ally); Ally.Cell = {1, 2}; E.Combatants.Add(Ally);
        FCombatBattle B; if (!B.Start(E, Seed, Error) || !Activate(B, 2)) { return false; }
        auto S = B.Snapshot(); auto& Downed = S.Encounter.Combatants[0];
        Downed.Life = ECombatLife::Downed; Downed.HP = 0; Downed.MaxHP = 40; Downed.DeathSaveSuccesses = 1; Downed.DeathSaveFailures = 1;
        if (!TestTrue(TEXT("Restore downed fixture"), B.Restore(S, Error))) { return false; }
        const auto R = B.Submit({ECombatCommandType::Attack, 2, {}, 1});
        if (!TestTrue(TEXT("SRD can damage a downed target"), R.bAccepted)) { return false; }
        if (!R.bHit) { continue; }
        Hit |= !R.bCritical; Crit |= R.bCritical;
        TestEqual(TEXT("Keep successes on damage while dying"), B.Find(1)->DeathSaveSuccesses, 1);
        TestEqual(TEXT("One failure or two on critical"), B.Find(1)->DeathSaveFailures, R.bCritical ? 3 : 2);
        TestTrue(TEXT("Third failure kills"), B.Find(1)->Life == (R.bCritical ? ECombatLife::Dead : ECombatLife::Downed));
        // Replaying the same hit against temporary HP cannot add a failure.
        S.Encounter.Combatants[0].TemporaryHP = 10;
        if (!TestTrue(TEXT("Restore temp HP fixture"), B.Restore(S, Error))) { return false; }
        B.Submit({ECombatCommandType::Attack, 2, {}, 1});
        TestEqual(TEXT("Absorbed damage adds no failure"), B.Find(1)->DeathSaveFailures, 1);
    }
    TestTrue(TEXT("Normal and critical exercised"), Hit && Crit);
    return !HasAnyErrors();
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReviewDistanceTest, "Project.Expedition.Combat.OpportunityGrappleShareDistance", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FReviewDistanceTest::RunTest(const FString& Parameters)
{
    FString Error; auto E = ReviewDuo(); E.Combatants[1].Cell = {2, 2}; FCombatBattle B;
    if (!B.Start(E, 42, Error) || !Activate(B, 1)) { return false; }
    TestFalse(TEXT("Diagonal not grapple-adjacent"), B.LegalTargets(1, ECombatCommandType::Grapple).Contains(2));
    TestFalse(TEXT("Diagonal not opportunity-adjacent"), FOpportunityRules::InReach({2, 2}, 1, {1, 1}));
    const auto R = B.Submit({ECombatCommandType::Move, 1, {0, 1}});
    TestTrue(TEXT("Move accepted"), R.bAccepted);
    TestTrue(TEXT("No phantom diagonal opportunity attack"), B.Find(2)->bReactionAvailable);
    return !HasAnyErrors();
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReviewOpportunityStopTest, "Project.Expedition.Combat.OpportunityAttackStopsDroppedMover", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FReviewOpportunityStopTest::RunTest(const FString& Parameters)
{
    FString Error;
    for (const bool bAlly : {true, false})
    {
        for (const int32 Natural : {1, 12, 20})
        {
            FCombatBattle B;
            if (!TestTrue(TEXT("Corridor starts"), B.Start(Corridor(bAlly), 42, Error)) || !TestTrue(TEXT("Hero active"), Activate(B, 1))) { return false; }
            auto S = B.Snapshot();
            S.RandomState = StateForD20([Natural](int32 Die) { return Die == Natural; });
            if (!TestTrue(TEXT("Restore chosen dice"), B.Restore(S, Error))) { return false; }
            FRandomStream Dice(S.RandomState);
            TestEqual(TEXT("Chosen attack roll"), Dice.RandRange(1, 20), Natural);
            const int32 DamageDie = Dice.RandRange(1, 4);
            const int32 CriticalDie = Dice.RandRange(1, 4);
            const FCombatResult R = B.Submit({ECombatCommandType::Move, 1, {4, 0}});
            if (!TestTrue(TEXT("Move accepted"), R.bAccepted)) { return false; }
            const bool bDropped = Natural != 1;
            const int32 Moved = bDropped ? 2 : 4;
            TestEqual(TEXT("Mover stops in the cell it was leaving"), B.Find(1)->Cell, bDropped ? FIntPoint(2, 0) : FIntPoint(4, 0));
            TestEqual(TEXT("Path holds only the cells moved"), R.Path.Num(), Moved);
            if (!R.Path.IsEmpty()) { TestEqual(TEXT("Path ends at the mover"), R.Path.Last(), B.Find(1)->Cell); }
            TestTrue(TEXT("Pays only for the cells moved"), R.Feedback.StartsWith(FString::Printf(TEXT("Arden moves %d cells (%d left)."), Moved, S.MovementLeft - Moved)));
            TestEqual(TEXT("Damage dice logged"), R.EffectDice.Num(), Natural == 1 ? 0 : Natural == 20 ? 2 : 1);
            if (R.EffectDice.Num() >= 1) { TestEqual(TEXT("Damage die first"), R.EffectDice[0].Value, DamageDie); TestEqual(TEXT("Weapon die sides"), R.EffectDice[0].Sides, 4); }
            if (R.EffectDice.Num() >= 2) { TestEqual(TEXT("Critical die second"), R.EffectDice[1].Value, CriticalDie); }
            const FCombatantState* Active = B.Find(B.ActiveId());
            if (!TestNotNull(TEXT("Active combatant"), Active)) { return false; }
            TestTrue(TEXT("Active combatant is conscious"), Active->Life == ECombatLife::Conscious);
            FCombatBattle Copy;
            TestTrue(FString::Printf(TEXT("Snapshot restores: %s"), *Error), Copy.Restore(B.Snapshot(), Error));
            if (!bDropped)
            {
                TestEqual(TEXT("Missed mover keeps its turn"), B.ActiveId(), 1);
                TestEqual(TEXT("Full move paid"), B.Snapshot().MovementLeft, S.MovementLeft - 4);
                TestFalse(TEXT("Reaction spent"), B.Find(2)->bReactionAvailable);
            }
            else if (bAlly)
            {
                TestTrue(TEXT("Mover downed"), B.Find(1)->Life == ECombatLife::Downed && R.Downed.Contains(1));
                TestTrue(TEXT("Battle continues"), B.Snapshot().Phase == EBattlePhase::Fighting);
                TestTrue(TEXT("Turn passed on"), B.ActiveId() != 1);
                TestTrue(TEXT("Next combatant can act"), B.Submit({ECombatCommandType::EndTurn, B.ActiveId()}).bAccepted);
            }
            else
            {
                TestTrue(TEXT("Last hero down is a defeat"), B.Snapshot().Phase == EBattlePhase::Defeat);
                TestTrue(TEXT("Unrescued hero dies at defeat"), B.Find(1)->Life == ECombatLife::Dead && R.Died.Contains(1));
            }
        }
    }
    return !HasAnyErrors();
}
#endif
