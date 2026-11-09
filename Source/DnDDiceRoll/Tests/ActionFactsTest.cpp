#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Combat/ActionFacts.h"
#include "Combat/CombatBattle.h"
#include "Party/BuildCatalog.h"

/**
 * Action facts (#58): what each command costs and exactly why it is unavailable. The rules decide
 * legality; these tests hold the facts to the rules for every catalog kit and a matrix of turn states.
 */
namespace ActionFactsTest
{
using EC = ECombatCommandType;

const EC AllCommands[] = {EC::Move, EC::Attack, EC::EndTurn, EC::SecondWind, EC::CureWounds, EC::MagicMissile,
    EC::Rescue, EC::Potion, EC::PrecisionStrike, EC::Special1, EC::Special2};

struct FScenario
{
    const TCHAR* Name;
    TFunction<void(FCombatEncounter&)> Edit;
    TFunction<void(FCombatBattle&)> After;
};

FCombatantState* Unit(FCombatEncounter& Encounter, int32 Id)
{
    return Encounter.Combatants.FindByPredicate([Id](const FCombatantState& U) { return U.Id == Id; });
}

/** The hero (10) acts first beside a wounded ally (11) and two raiders (20, 21). */
FCombatEncounter MakeEncounter(const FBuildDefinition& Build, const FScenario& Scenario)
{
    FCombatEncounter E;
    E.Width = E.Height = 8;
    FCombatantState Hero = Build.Profile;
    Hero.Id = 10; Hero.Name = TEXT("Actor"); Hero.Cell = {1, 1}; Hero.InitiativeBonus = 30;
    Hero.HP = FMath::Max(1, Hero.MaxHP - 1); Hero.Potions = 1;
    FCombatantState Ally = Hero;
    Ally.Id = 11; Ally.Name = TEXT("Ally"); Ally.Cell = {1, 2}; Ally.InitiativeBonus = -10;
    FCombatantState Enemy;
    Enemy.Id = 20; Enemy.Name = TEXT("Target"); Enemy.Role = TEXT("Raider"); Enemy.Side = ECombatSide::Enemy;
    Enemy.Cell = {2, 1}; Enemy.InitiativeBonus = -10;
    FCombatantState Other = Enemy;
    Other.Id = 21; Other.Name = TEXT("Other"); Other.Cell = {3, 1};
    E.Combatants = {Hero, Ally, Enemy, Other};
    if (Scenario.Edit) { Scenario.Edit(E); }
    return E;
}

bool IsBonus(const FCombatantState& Hero, EC Command)
{
    if (Command == EC::SecondWind) { return true; }
    for (const FAbilityFacts& A : FBuildCatalog::Abilities(Hero)) { if (A.Command == Command) { return A.bBonus; } }
    return false;
}

/** Spends the hero's Bonus Action through the rules (first bonus ability with a legal target). */
void SpendBonus(FCombatBattle& Battle)
{
    const FCombatantState* Hero = Battle.Find(10);
    if (!Hero) { return; }
    TArray<EC> Commands = {EC::SecondWind};
    for (const FAbilityFacts& A : FBuildCatalog::Abilities(*Hero)) { Commands.Add(A.Command); }
    for (const EC Command : Commands)
    {
        if (!IsBonus(*Hero, Command) || !FBuildCatalog::HasSpecial(*Hero, Command)) { continue; }
        const TArray<int32> Targets = Battle.LegalTargets(10, Command);
        if (!Targets.IsEmpty()) { Battle.Submit({Command, 10, {}, Targets[0]}); return; }
    }
}

void SpendMovement(FCombatBattle& Battle)
{
    for (int32 Step = 0; Step < 40 && Battle.Snapshot().MovementLeft > 0; ++Step)
    {
        const TArray<FIntPoint> Moves = Battle.LegalMoves(10);
        if (Moves.IsEmpty()) { return; }
        Battle.Submit({EC::Move, 10, Moves[0]});
    }
}

void ZeroResources(FCombatEncounter& E)
{
    FCombatantState* H = Unit(E, 10);
    H->KitUses = H->SecondWindUses = H->SpellSlots = H->ManeuverUses = H->RageUses = H->Potions = 0;
}

const TArray<FScenario>& Scenarios()
{
    static const TArray<FScenario> All = {
        {TEXT("fresh"), nullptr, nullptr},
        {TEXT("no resources"), ZeroResources, nullptr},
        {TEXT("everyone healthy"), [](FCombatEncounter& E) { for (const int32 Id : {10, 11}) { FCombatantState* U = Unit(E, Id); U->HP = U->MaxHP; } }, nullptr},
        {TEXT("downed ally"), [](FCombatEncounter& E) { FCombatantState* A = Unit(E, 11); A->Life = ECombatLife::Downed; A->HP = 0; A->RescueTurnsLeft = 2; }, nullptr},
        {TEXT("foes and ally far away"), [](FCombatEncounter& E) { Unit(E, 20)->Cell = {7, 7}; Unit(E, 21)->Cell = {7, 6}; Unit(E, 11)->Cell = {5, 5}; }, nullptr},
        {TEXT("downed ally far away"), [](FCombatEncounter& E) { FCombatantState* A = Unit(E, 11); A->Life = ECombatLife::Downed; A->HP = 0; A->RescueTurnsLeft = 2; A->Cell = {5, 5}; }, nullptr},
        {TEXT("sight blocked"), [](FCombatEncounter& E) { E.BlockedCells = {{2, 1}}; Unit(E, 20)->Cell = {3, 1}; Unit(E, 21)->Cell = {4, 1}; }, nullptr},
        {TEXT("boxed in"), [](FCombatEncounter& E) { E.BlockedCells = {{0, 1}, {1, 0}}; }, nullptr},
        {TEXT("action spent"), nullptr, [](FCombatBattle& B) { B.Submit({EC::Attack, 10, {}, 20}); }},
        {TEXT("bonus spent"), nullptr, SpendBonus},
        {TEXT("all movement spent"), nullptr, SpendMovement},
        {TEXT("no resources, action spent"), ZeroResources, [](FCombatBattle& B) { B.Submit({EC::Attack, 10, {}, 20}); }},
    };
    return All;
}

const FScenario& Scenario(const TCHAR* Name)
{
    for (const FScenario& S : Scenarios()) { if (FCString::Strcmp(S.Name, Name) == 0) { return S; } }
    return Scenarios()[0];
}

bool Make(FAutomationTestBase& Test, const FBuildDefinition& Build, const FScenario& Scenario, FCombatBattle& Out)
{
    FString Error;
    if (!Test.TestTrue(Build.Id + TEXT(" / ") + Scenario.Name + TEXT(" starts"), Out.Start(MakeEncounter(Build, Scenario), 42, Error))) { return false; }
    if (Scenario.After) { Scenario.After(Out); }
    return true;
}

bool ExpectedAvailable(const FCombatBattle& Battle, int32 Id, EC Command)
{
    if (Command == EC::Move) { return !Battle.LegalMoves(Id).IsEmpty(); }
    if (Command == EC::EndTurn)
    {
        const FCombatantState* U = Battle.Find(Id);
        return U && U->Life == ECombatLife::Conscious && Battle.ActiveId() == Id && Battle.Snapshot().Phase == EBattlePhase::Fighting;
    }
    return !Battle.LegalTargets(Id, Command).IsEmpty();
}

int32 Held(const FCombatantState& U, EActionResource R)
{
    switch (R)
    {
    case EActionResource::Kit: return U.KitUses;
    case EActionResource::Slot: return U.SpellSlots;
    case EActionResource::Wind: return U.SecondWindUses;
    case EActionResource::Maneuver: return U.ManeuverUses;
    case EActionResource::Rage: return U.RageUses;
    case EActionResource::Potion: return U.Potions;
    default: return 0;
    }
}

const TCHAR* ResourceText(EActionResource R)
{
    switch (R)
    {
    case EActionResource::Kit: return TEXT("Kit");
    case EActionResource::Slot: return TEXT("Slot");
    case EActionResource::Wind: return TEXT("Wind");
    case EActionResource::Maneuver: return TEXT("Maneuver");
    case EActionResource::Rage: return TEXT("Rage");
    case EActionResource::Potion: return TEXT("Potion");
    default: return TEXT("None");
    }
}

FString BlockerText(const FActionBlocker& B)
{
    switch (B.Reason)
    {
    case EActionBlocker::BattleInactive: return TEXT("BattleInactive");
    case EActionBlocker::NotYourTurn: return TEXT("NotYourTurn");
    case EActionBlocker::ActorDown: return TEXT("ActorDown");
    case EActionBlocker::NotInKit: return TEXT("NotInKit");
    case EActionBlocker::ActionSpent: return TEXT("ActionSpent");
    case EActionBlocker::BonusSpent: return TEXT("BonusSpent");
    case EActionBlocker::NoMovementLeft: return TEXT("NoMovementLeft");
    case EActionBlocker::NoOpenCell: return TEXT("NoOpenCell");
    case EActionBlocker::ResourceShort: return FString::Printf(TEXT("ResourceShort(%s %d/%d)"), ResourceText(B.Resource), B.Held, B.Needed);
    case EActionBlocker::StanceRequired: return TEXT("StanceRequired");
    case EActionBlocker::StanceBlocks: return TEXT("StanceBlocks");
    case EActionBlocker::NoWoundedTarget: return TEXT("NoWoundedTarget");
    case EActionBlocker::NoDownedTarget: return TEXT("NoDownedTarget");
    case EActionBlocker::OutOfRange: return FString::Printf(TEXT("OutOfRange(%d)"), B.Range);
    case EActionBlocker::SightBlocked: return TEXT("SightBlocked");
    case EActionBlocker::NoEligibleTarget: return TEXT("NoEligibleTarget");
    default: return TEXT("?");
    }
}

/** Blockers as "A,B(…)" in reported order; empty when none. */
FString Reasons(const FActionFacts& Facts)
{
    TArray<FString> Parts;
    for (const FActionBlocker& B : Facts.Blockers) { Parts.Add(BlockerText(B)); }
    return FString::Join(Parts, TEXT(","));
}

FString Signature(const FActionFacts& F)
{
    FString S = FString::Printf(TEXT("%d|%s|%d|%d%d|%d|%d|%d|%d%d|%s|"), static_cast<int32>(F.Command), *F.Name, static_cast<int32>(F.Cost), F.bRequiresRage, F.bForbidsRage,
        static_cast<int32>(F.Target), F.bNeedsWounded, F.Range, F.bNeedsSight, F.bAvailable, *Reasons(F));
    for (const FActionResourceCost& R : F.Resources) { S += FString::Printf(TEXT("[%s %d %d %d %d]"), ResourceText(R.Resource), R.Amount, R.Held, R.Cap, R.CampRecovery); }
    return S;
}

/** Reports the first few mismatches and a total, so a broken query reads clearly instead of flooding the log. */
struct FChecks
{
    FAutomationTestBase& Test;
    int32 Total = 0, Failures = 0;
    void Count() { ++Total; }
    void Fail(const FString& Context, const TCHAR* What)
    {
        if (Failures++ < 15) { Test.AddError(FString::Printf(TEXT("%s: %s"), *Context, What)); }
    }
};

bool Same(const FCombatSnapshot& A, const FCombatSnapshot& B) { return FCombatSnapshot::StaticStruct()->CompareScriptStruct(&A, &B, 0); }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FActionFactsMatchLegalityTest, "Project.Expedition.Combat.ActionFactsMatchLegality",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FActionFactsMatchLegalityTest::RunTest(const FString& Parameters)
{
    using namespace ActionFactsTest;
    FChecks Checks{*this};
    int32 Available = 0, Blocked = 0;
    for (const FBuildDefinition& Build : FBuildCatalog::All())
    {
        for (const FScenario& Scene : Scenarios())
        {
            FCombatBattle Battle;
            if (!Make(*this, Build, Scene, Battle)) { continue; }
            for (const int32 Actor : {10, 11, 20})
            {
                for (const EC Command : AllCommands)
                {
                    const FActionFacts F = FActionFactsQuery::For(Battle, Actor, Command);
                    auto Context = [&]() { return FString::Printf(TEXT("%s / %s / actor %d / command %d"), *Build.Id, Scene.Name, Actor, static_cast<int32>(Command)); };
                    Checks.Count();
                    if (F.Command != Command) { Checks.Fail(Context(), TEXT("the fact names a different command")); }
                    if (F.bAvailable != ExpectedAvailable(Battle, Actor, Command)) { Checks.Fail(Context(), *FString::Printf(TEXT("availability disagrees with the rules (facts say %d; blockers: %s)"), F.bAvailable, *Reasons(F))); }
                    if (F.Blockers.IsEmpty() != F.bAvailable) { Checks.Fail(Context(), *FString::Printf(TEXT("blockers do not match availability (%s)"), *Reasons(F))); }
                    for (int32 I = 1; I < F.Blockers.Num(); ++I)
                    { if (F.Blockers[I - 1].Reason > F.Blockers[I].Reason) { Checks.Fail(Context(), TEXT("blockers are out of fixed order")); } }
                    for (const FActionBlocker& B : F.Blockers)
                    {
                        if (F.Blockers.Num() > 1 && B.Reason < EActionBlocker::ActionSpent) { Checks.Fail(Context(), TEXT("a turn-level blocker is not alone")); }
                        if (B.Reason >= EActionBlocker::Count) { Checks.Fail(Context(), TEXT("a blocker is out of range")); }
                    }
                    // The generic fallback reason only stands when nobody of the right kind exists, which no scene does for the hero.
                    if (Actor == 10 && F.Blockers.Num() == 1 && F.Blockers[0].Reason == EActionBlocker::NoEligibleTarget)
                    { Checks.Fail(Context(), TEXT("the only reason is the generic fallback, so the ladder missed the real cause")); }
                    (F.bAvailable ? Available : Blocked) += 1;
                }
            }
        }
    }
    TestEqual(TEXT("Facts agree with the rules everywhere"), Checks.Failures, 0);
    TestTrue(TEXT("The matrix covers every kit and state"), Checks.Total > 5000 && Available > 500 && Blocked > 500);
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FActionFactsMatchSubmitTest, "Project.Expedition.Combat.ActionFactsMatchSubmit",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FActionFactsMatchSubmitTest::RunTest(const FString& Parameters)
{
    using namespace ActionFactsTest;
    const EActionResource Resources[] = {EActionResource::Kit, EActionResource::Slot, EActionResource::Wind, EActionResource::Maneuver, EActionResource::Rage, EActionResource::Potion};
    FChecks Checks{*this};
    int32 Committed = 0, Refused = 0;
    for (const FBuildDefinition& Build : FBuildCatalog::All())
    {
        for (const FScenario& Scene : Scenarios())
        {
            FCombatBattle Battle;
            if (!Make(*this, Build, Scene, Battle)) { continue; }
            const FCombatSnapshot Before = Battle.Snapshot();
            for (const EC Command : AllCommands)
            {
                const FActionFacts F = FActionFactsQuery::For(Battle, 10, Command);
                auto Context = [&]() { return FString::Printf(TEXT("%s / %s / command %d"), *Build.Id, Scene.Name, static_cast<int32>(Command)); };
                Checks.Count();
                FCombatBattle Trial = Battle;
                FCombatCommand Submit{Command, 10};
                if (F.bAvailable)
                {
                    if (Command == EC::Move) { Submit.Destination = Trial.LegalMoves(10)[0]; }
                    else if (Command != EC::EndTurn) { Submit.TargetId = Trial.LegalTargets(10, Command)[0]; }
                    const FCombatResult Result = Trial.Submit(Submit);
                    if (!Result.bAccepted) { Checks.Fail(Context(), TEXT("an available action was rejected")); continue; }
                    ++Committed;
                    if (Command == EC::EndTurn) { continue; }
                    const FCombatSnapshot& After = Trial.Snapshot();
                    const bool bAction = Before.bActionAvailable && !After.bActionAvailable;
                    const bool bBonus = Before.bBonusActionAvailable && !After.bBonusActionAvailable;
                    const bool bMove = After.MovementLeft < Before.MovementLeft;
                    if ((F.Cost == EActionCostKind::Action) != bAction || (F.Cost == EActionCostKind::BonusAction) != bBonus || (F.Cost == EActionCostKind::Movement) != bMove)
                    { Checks.Fail(Context(), TEXT("the budget actually spent differs from the stated cost")); }
                    const FCombatantState* Was = Before.Encounter.Combatants.FindByPredicate([](const FCombatantState& U) { return U.Id == 10; });
                    const FCombatantState* Now = After.Encounter.Combatants.FindByPredicate([](const FCombatantState& U) { return U.Id == 10; });
                    if (!Was || !Now) { Checks.Fail(Context(), TEXT("the actor vanished")); continue; }
                    for (const EActionResource R : Resources)
                    {
                        int32 Stated = 0;
                        for (const FActionResourceCost& C : F.Resources) { if (C.Resource == R) { Stated += C.Amount; } }
                        if (Held(*Was, R) - Held(*Now, R) != Stated)
                        { Checks.Fail(Context(), *FString::Printf(TEXT("%s spent %d but the facts say %d"), ResourceText(R), Held(*Was, R) - Held(*Now, R), Stated)); }
                    }
                }
                else
                {
                    ++Refused;
                    for (const int32 Target : {20, 11})
                    {
                        FCombatBattle Reject = Battle;
                        FCombatCommand Try = Submit;
                        if (Command == EC::Move) { Try.Destination = {7, 7}; } else { Try.TargetId = Target; }
                        if (Reject.Submit(Try).bAccepted) { Checks.Fail(Context(), TEXT("an unavailable action was accepted")); }
                        if (!Same(Before, Reject.Snapshot())) { Checks.Fail(Context(), TEXT("a refused command changed state or the random stream")); }
                    }
                }
            }
        }
    }
    TestEqual(TEXT("Stated costs match what the rules spend, refusals change nothing"), Checks.Failures, 0);
    TestTrue(TEXT("Both committed and refused commands were exercised"), Committed > 300 && Refused > 300);
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FActionFactsReasonsTest, "Project.Expedition.Combat.ActionFactsReasons",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FActionFactsReasonsTest::RunTest(const FString& Parameters)
{
    using namespace ActionFactsTest;
    struct FCase { const TCHAR* Build; const TCHAR* Scene; int32 Actor; EC Command; const TCHAR* Expected; };
    const FCase Cases[] = {
        // Available means no blockers at all.
        {TEXT("fighter.champion"), TEXT("fresh"), 10, EC::Attack, TEXT("")},
        {TEXT("fighter.champion"), TEXT("fresh"), 10, EC::SecondWind, TEXT("")},
        {TEXT("fighter.champion"), TEXT("downed ally"), 10, EC::Rescue, TEXT("")},
        {TEXT("fighter.champion"), TEXT("fresh"), 10, EC::EndTurn, TEXT("")},
        // Budgets.
        {TEXT("fighter.champion"), TEXT("action spent"), 10, EC::Attack, TEXT("ActionSpent")},
        {TEXT("fighter.champion"), TEXT("action spent"), 10, EC::Potion, TEXT("ActionSpent")},
        // Spending the bonus action on Second Wind also healed the hero, so no wounded target is left: both are reported.
        {TEXT("fighter.champion"), TEXT("bonus spent"), 10, EC::SecondWind, TEXT("BonusSpent,NoWoundedTarget")},
        {TEXT("fighter.champion"), TEXT("all movement spent"), 10, EC::Move, TEXT("NoMovementLeft")},
        {TEXT("fighter.champion"), TEXT("boxed in"), 10, EC::Move, TEXT("NoOpenCell")},
        // Resources, with exact held and needed counts.
        {TEXT("fighter.champion"), TEXT("no resources"), 10, EC::SecondWind, TEXT("ResourceShort(Wind 0/1)")},
        {TEXT("fighter.champion"), TEXT("no resources"), 10, EC::Potion, TEXT("ResourceShort(Potion 0/1)")},
        {TEXT("fighter.battle-master"), TEXT("no resources"), 10, EC::PrecisionStrike, TEXT("ResourceShort(Maneuver 0/1)")},
        {TEXT("wizard.evocation"), TEXT("no resources"), 10, EC::MagicMissile, TEXT("ResourceShort(Kit 0/1),ResourceShort(Slot 0/1)")},
        {TEXT("wizard.evocation"), TEXT("no resources, action spent"), 10, EC::MagicMissile, TEXT("ActionSpent,ResourceShort(Kit 0/1),ResourceShort(Slot 0/1)")},
        // The Barbarian's Rage stance.
        {TEXT("barbarian.berserker"), TEXT("fresh"), 10, EC::Special1, TEXT("")},
        {TEXT("barbarian.berserker"), TEXT("fresh"), 10, EC::Special2, TEXT("StanceRequired")},
        {TEXT("barbarian.berserker"), TEXT("bonus spent"), 10, EC::Special1, TEXT("BonusSpent,StanceBlocks")},
        {TEXT("barbarian.berserker"), TEXT("bonus spent"), 10, EC::Special2, TEXT("BonusSpent")},
        {TEXT("barbarian.berserker"), TEXT("no resources"), 10, EC::Special1, TEXT("ResourceShort(Rage 0/1)")},
        // Targets.
        {TEXT("fighter.champion"), TEXT("everyone healthy"), 10, EC::Potion, TEXT("NoWoundedTarget")},
        {TEXT("fighter.champion"), TEXT("everyone healthy"), 10, EC::SecondWind, TEXT("NoWoundedTarget")},
        {TEXT("fighter.champion"), TEXT("fresh"), 10, EC::Rescue, TEXT("NoDownedTarget")},
        {TEXT("fighter.champion"), TEXT("foes and ally far away"), 10, EC::Attack, TEXT("OutOfRange(1)")},
        // A potion may be drunk by its user, so a far-away ally never makes it out of range.
        {TEXT("fighter.champion"), TEXT("foes and ally far away"), 10, EC::Potion, TEXT("")},
        {TEXT("fighter.champion"), TEXT("downed ally far away"), 10, EC::Rescue, TEXT("OutOfRange(1)")},
        {TEXT("wizard.evocation"), TEXT("sight blocked"), 10, EC::MagicMissile, TEXT("SightBlocked")},
        // Who may act and what the kit owns stand alone.
        {TEXT("fighter.champion"), TEXT("fresh"), 11, EC::Attack, TEXT("NotYourTurn")},
        {TEXT("fighter.champion"), TEXT("fresh"), 20, EC::Attack, TEXT("NotYourTurn")},
        {TEXT("fighter.champion"), TEXT("fresh"), 99, EC::Attack, TEXT("NotYourTurn")},
        {TEXT("fighter.champion"), TEXT("fresh"), 10, EC::Special1, TEXT("NotInKit")},
    };
    for (const FCase& Case : Cases)
    {
        const FBuildDefinition* Build = FBuildCatalog::Find(Case.Build);
        if (!TestNotNull(FString(Case.Build) + TEXT(" exists"), Build)) { continue; }
        FCombatBattle Battle;
        if (!Make(*this, *Build, Scenario(Case.Scene), Battle)) { continue; }
        const FActionFacts F = FActionFactsQuery::For(Battle, Case.Actor, Case.Command);
        const FString Context = FString::Printf(TEXT("%s / %s / actor %d / command %d"), Case.Build, Case.Scene, Case.Actor, static_cast<int32>(Case.Command));
        TestEqual(Context + TEXT(": reasons"), Reasons(F), FString(Case.Expected));
        TestEqual(Context + TEXT(": availability"), F.bAvailable, FCString::Strlen(Case.Expected) == 0);
    }

    // A finished battle: one Attack against a 1 HP target ends it, so nothing is available.
    FCombatEncounter Finish;
    Finish.Width = Finish.Height = 8;
    const FBuildDefinition* Champion = FBuildCatalog::Find(TEXT("fighter.champion"));
    if (!TestNotNull(TEXT("Champion exists"), Champion)) { return false; }
    FCombatantState Hero = Champion->Profile;
    Hero.Id = 10; Hero.Name = TEXT("Actor"); Hero.Cell = {1, 1}; Hero.InitiativeBonus = 30; Hero.AttackBonus = 30;
    FCombatantState Foe;
    Foe.Id = 20; Foe.Name = TEXT("Target"); Foe.Role = TEXT("Raider"); Foe.Side = ECombatSide::Enemy; Foe.Cell = {2, 1};
    Foe.HP = Foe.MaxHP = 1; Foe.ArmorClass = 1; Foe.InitiativeBonus = -10;
    Finish.Combatants = {Hero, Foe};
    bool bFinished = false;
    for (int32 Seed = 1; Seed <= 60 && !bFinished; ++Seed)
    {
        FCombatBattle Battle;
        FString Error;
        if (!Battle.Start(Finish, Seed, Error)) { continue; }
        Battle.Submit({EC::Attack, 10, {}, 20});
        if (Battle.Snapshot().Phase != EBattlePhase::Victory) { continue; }
        bFinished = true;
        const FActionFacts F = FActionFactsQuery::For(Battle, 10, EC::Attack);
        TestEqual(TEXT("A finished battle blocks everything with BattleInactive"), Reasons(F), FString(TEXT("BattleInactive")));
        TestFalse(TEXT("A finished battle offers nothing"), F.bAvailable);
    }
    TestTrue(TEXT("A seed that wins the battle exists"), bFinished);
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FActionFactsReadOnlyTest, "Project.Expedition.Combat.ActionFactsReadOnly",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FActionFactsReadOnlyTest::RunTest(const FString& Parameters)
{
    using namespace ActionFactsTest;
    int32 Queried = 0;
    for (const FBuildDefinition& Build : FBuildCatalog::All())
    {
        for (const FScenario& Scene : Scenarios())
        {
            FCombatBattle Battle;
            if (!Make(*this, Build, Scene, Battle)) { continue; }
            const FCombatSnapshot Before = Battle.Snapshot();
            for (const EC Command : AllCommands)
            {
                const FString First = Signature(FActionFactsQuery::For(Battle, 10, Command));
                const FString Second = Signature(FActionFactsQuery::For(Battle, 10, Command));
                ++Queried;
                if (First != Second) { AddError(Build.Id + TEXT(" / ") + Scene.Name + TEXT(": repeated queries differ")); }
            }
            FActionFactsQuery::BarFor(Battle, 10);
            if (!Same(Before, Battle.Snapshot())) { AddError(Build.Id + TEXT(" / ") + Scene.Name + TEXT(": a query changed the state or the random stream")); }
        }
    }
    TestTrue(TEXT("Every kit and state was queried"), Queried > 5000);
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FActionFactsCoverEveryAbilityTest, "Project.Expedition.Combat.ActionFactsCoverEveryAbility",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FActionFactsCoverEveryAbilityTest::RunTest(const FString& Parameters)
{
    using namespace ActionFactsTest;
    FChecks Checks{*this};
    int32 Abilities = 0;
    for (const FBuildDefinition& Build : FBuildCatalog::All())
    {
        FCombatBattle Battle;
        if (!Make(*this, Build, Scenario(TEXT("fresh")), Battle)) { continue; }
        const FCombatantState* Hero = Battle.Find(10);
        const TArray<EC> Bar = FActionFactsQuery::BarFor(Battle, 10);
        auto Context = [&](const TCHAR* What) { return FString::Printf(TEXT("%s: %s"), *Build.Id, What); };
        Checks.Count();
        if (!Bar.Contains(EC::Attack) || !Bar.Contains(EC::Move) || !Bar.Contains(EC::EndTurn) || !Bar.Contains(EC::Rescue) || !Bar.Contains(EC::Potion))
        { Checks.Fail(Context(TEXT("bar")), TEXT("the fixed actions are missing from the bar")); }
        for (int32 I = 0; I < Bar.Num(); ++I)
        { if (Bar.IndexOfByKey(Bar[I]) != I) { Checks.Fail(Context(TEXT("bar")), TEXT("the bar repeats a command")); } }
        for (const FAbilityFacts& A : FBuildCatalog::Abilities(*Hero))
        {
            ++Abilities;
            Checks.Count();
            const FString Where = Context(*A.Name);
            if (!Bar.Contains(A.Command)) { Checks.Fail(Where, TEXT("the ability is not on the bar")); }
            const FActionFacts F = FActionFactsQuery::For(Battle, 10, A.Command);
            if (F.Cost != (A.bBonus ? EActionCostKind::BonusAction : EActionCostKind::Action)) { Checks.Fail(Where, TEXT("cost kind differs from the catalog's bonus flag")); }
            if (F.Cost == EActionCostKind::None) { Checks.Fail(Where, TEXT("an ability has no cost")); }
            // The catalog names the resources ("Kit", "Kit + Slot", "Rage", ...); the facts must list the same ones.
            TArray<FString> Names;
            A.ResourceName.ParseIntoArray(Names, TEXT(" + "));
            for (const FString& Name : Names)
            {
                const EActionResource R = Name == TEXT("Kit") ? EActionResource::Kit : Name == TEXT("Slot") ? EActionResource::Slot : Name == TEXT("Wind") ? EActionResource::Wind :
                    Name == TEXT("Maneuver") ? EActionResource::Maneuver : Name == TEXT("Rage") ? EActionResource::Rage : EActionResource::None;
                const FActionResourceCost* Cost = F.Resources.FindByPredicate([R](const FActionResourceCost& C) { return C.Resource == R; });
                if (R == EActionResource::None || !Cost) { Checks.Fail(Where, *FString::Printf(TEXT("resource '%s' from the catalog is missing from the facts"), *Name)); continue; }
                if (R == EActionResource::Kit && Cost->Amount != FMath::Max(1, A.ResourceCost)) { Checks.Fail(Where, TEXT("Kit amount differs from the catalog")); }
            }
            if (Names.IsEmpty() != F.Resources.IsEmpty()) { Checks.Fail(Where, TEXT("the facts list resources the catalog does not, or the reverse")); }
        }
    }
    TestEqual(TEXT("Every ability maps to matching facts"), Checks.Failures, 0);
    TestTrue(TEXT("Abilities of every kit were covered"), Abilities > 100);
    return !HasAnyErrors();
}
#endif
