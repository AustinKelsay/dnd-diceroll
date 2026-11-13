#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Combat/CombatBattle.h"
#include "Party/BuildCatalog.h"

// FCombatBattle::Protection (#179) is the public form of the reduction resolution applies. It must report
// exactly what the next hit loses, and who supplied it, without changing or rolling anything.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProtectionQueryTest, "Project.Expedition.Combat.ProtectionQueryMatchesResolution",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FProtectionQueryTest::RunTest(const FString& Parameters)
{
    FString Error;
    auto Enemy = []()
    {
        FCombatantState E; E.Id = 20; E.Name = E.Role = TEXT("Enemy"); E.Side = ECombatSide::Enemy; E.InitiativeBonus = -10;
        E.AttackBonus = 30; E.DamageDie = 1; E.DamageBonus = 4; // a non-critical hit deals exactly 1 + 4 = 5 before protection
        return E;
    };
    constexpr int32 RawHit = 5;
    // The HP the target actually lost, from the snapshots on either side of the hit, against raw damage minus the query.
    auto CheckHit = [this](FCombatBattle& Battle, int32 AttackerId, int32 TargetId, int32 Expected, const TCHAR* What)
    {
        const FProtectionFacts Queried = Battle.Protection(TargetId);
        TestEqual(FString(What) + TEXT(": queried amount"), Queried.Amount, Expected);
        const int32 Before = Battle.Find(TargetId)->HP;
        const FCombatResult Hit = Battle.Submit({ECombatCommandType::Attack, AttackerId, {}, TargetId});
        if (!TestTrue(FString(What) + TEXT(": fixture lands a non-critical hit"), Hit.bAccepted && Hit.bHit && !Hit.bCritical)) { return; }
        TestEqual(FString(What) + TEXT(": HP lost is raw damage minus the queried protection"), Before - Battle.Find(TargetId)->HP, FMath::Max(0, RawHit - Queried.Amount));
        TestEqual(FString(What) + TEXT(": the result reports the same prevention"), Hit.DamagePrevented, FMath::Min(RawHit, Queried.Amount));
    };
    {
        // Two wards of 3 on one hero: the strongest applies (not the sum), and the first in roster order supplies it.
        FCombatantState Knight, Psi;
        FBuildCatalog::Create(TEXT("fighter.eldritch-knight"), 1, TEXT("Knight"), Knight);
        FBuildCatalog::Create(TEXT("fighter.psi-warrior"), 2, TEXT("Psi"), Psi);
        Knight.Cell = {1, 1}; Knight.InitiativeBonus = 30; Psi.Cell = {1, 2}; Psi.InitiativeBonus = 10;
        FCombatantState Foe = Enemy(); Foe.Cell = {2, 1};
        FCombatEncounter Encounter; Encounter.Combatants = {Knight, Psi, Foe};
        FCombatBattle Battle;
        if (!TestTrue(TEXT("Start ward scenario"), Battle.Start(Encounter, 42, Error))) { return false; }
        TestEqual(TEXT("No protection before any ward"), Battle.Protection(1).Amount, 0);
        TestEqual(TEXT("No source before any ward"), Battle.Protection(1).SourceId, INDEX_NONE);
        Battle.Submit({ECombatCommandType::Special1, 1, {}, 1}); Battle.Submit({ECombatCommandType::EndTurn, 1});
        Battle.Submit({ECombatCommandType::Special1, 2, {}, 1}); Battle.Submit({ECombatCommandType::EndTurn, 2});
        const FCombatSnapshot Before = Battle.Snapshot();
        const FProtectionFacts Ward = Battle.Protection(1);
        TestTrue(TEXT("The query changes nothing and rolls nothing"), FCombatSnapshot::StaticStruct()->CompareScriptStruct(&Before, &Battle.Snapshot(), 0));
        TestEqual(TEXT("Strongest ward, not the sum"), Ward.Amount, 3);
        TestEqual(TEXT("First ward owner in roster order supplies it"), Ward.SourceId, 1);
        CheckHit(Battle, 20, 1, 3, TEXT("Two wards"));
    }
    {
        // Rage alone gives 1 from the hero itself; a Rage-gated Shelter (no Kit amount) falls back to 3 from its owner.
        FCombatantState Totem, Ally;
        FBuildCatalog::Create(TEXT("barbarian.totem-warrior"), 1, TEXT("Totem"), Totem);
        FBuildCatalog::Create(TEXT("fighter.champion"), 2, TEXT("Ally"), Ally);
        Totem.Cell = {1, 1}; Totem.InitiativeBonus = 30; Ally.Cell = {1, 3}; Ally.InitiativeBonus = 10;
        FCombatantState Foe = Enemy(); Foe.Cell = {2, 3};
        FCombatEncounter Encounter; Encounter.Combatants = {Totem, Ally, Foe};
        FCombatBattle Battle;
        if (!TestTrue(TEXT("Start rage scenario"), Battle.Start(Encounter, 42, Error))) { return false; }
        TestTrue(TEXT("Rage"), Battle.Submit({ECombatCommandType::Special1, 1, {}, 1}).bAccepted);
        TestEqual(TEXT("Rage protects its owner by 1"), Battle.Protection(1).Amount, 1);
        TestEqual(TEXT("The raging hero supplies its own protection"), Battle.Protection(1).SourceId, 1);
        TestTrue(TEXT("Shelter the ally"), Battle.Submit({ECombatCommandType::Special2, 1, {}, 2}).bAccepted);
        const FProtectionFacts Shelter = Battle.Protection(2);
        TestEqual(TEXT("Shelter without a Kit amount gives 3"), Shelter.Amount, 3);
        TestEqual(TEXT("The barbarian supplies the shelter"), Shelter.SourceId, 1);
        Battle.Submit({ECombatCommandType::EndTurn, 1}); Battle.Submit({ECombatCommandType::EndTurn, 2});
        CheckHit(Battle, 20, 2, 3, TEXT("Shelter"));
        TestEqual(TEXT("Unknown combatant has no protection"), Battle.Protection(99).Amount, 0);
    }
    {
        // Rage only: the raging hero is hit and loses 1 less; an unprotected hero loses the full hit.
        FCombatantState Berserker, Ally;
        FBuildCatalog::Create(TEXT("barbarian.berserker"), 1, TEXT("Berserker"), Berserker);
        FBuildCatalog::Create(TEXT("fighter.champion"), 2, TEXT("Ally"), Ally);
        Berserker.Cell = {1, 1}; Berserker.InitiativeBonus = 30; Ally.Cell = {3, 2}; Ally.InitiativeBonus = 10;
        FCombatantState Foe = Enemy(); Foe.Cell = {2, 1}; // beside the Berserker
        FCombatantState Second = Enemy(); Second.Id = 21; Second.Cell = {3, 1}; // beside the Ally; both enemies act after the heroes
        FCombatEncounter Encounter; Encounter.Combatants = {Berserker, Ally, Foe, Second};
        // Hits and criticals follow the seed: take the first seed (deterministically) whose two enemy attacks are plain hits.
        auto Opening = [&](int32 Seed, FCombatBattle& Out)
        {
            FString Ignored;
            if (!Out.Start(Encounter, Seed, Ignored)) { return false; }
            Out.Submit({ECombatCommandType::Special1, 1, {}, 1}); Out.Submit({ECombatCommandType::EndTurn, 1}); Out.Submit({ECombatCommandType::EndTurn, 2});
            return true;
        };
        int32 Seed = INDEX_NONE;
        for (int32 Candidate = 1; Candidate <= 200 && Seed == INDEX_NONE; ++Candidate)
        {
            FCombatBattle Probe;
            bool bPlain = Opening(Candidate, Probe);
            for (int32 Turn = 0; Turn < 2 && bPlain; ++Turn)
            {
                const int32 Active = Probe.ActiveId();
                const FCombatResult Hit = Probe.Submit({ECombatCommandType::Attack, Active, {}, Active == 20 ? 1 : 2});
                bPlain = Hit.bAccepted && Hit.bHit && !Hit.bCritical && Probe.Submit({ECombatCommandType::EndTurn, Active}).bAccepted;
            }
            if (bPlain) { Seed = Candidate; }
        }
        if (!TestTrue(TEXT("Some seed gives two plain enemy hits"), Seed != INDEX_NONE)) { return false; }
        FCombatBattle Battle;
        if (!TestTrue(TEXT("Start rage-only scenario"), Battle.Start(Encounter, Seed, Error))) { return false; }
        TestEqual(TEXT("No protection before Rage"), Battle.Protection(1).Amount, 0);
        TestTrue(TEXT("Rage"), Battle.Submit({ECombatCommandType::Special1, 1, {}, 1}).bAccepted);
        TestEqual(TEXT("Rage supplies its own protection"), Battle.Protection(1).SourceId, 1);
        Battle.Submit({ECombatCommandType::EndTurn, 1}); Battle.Submit({ECombatCommandType::EndTurn, 2});
        // The two enemies act in seeded order; the Rage lasts until the Berserker's next turn, so it covers both.
        for (int32 Turn = 0; Turn < 2; ++Turn)
        {
            const int32 Active = Battle.ActiveId();
            if (!TestTrue(TEXT("An enemy has the turn"), Active == 20 || Active == 21)) { break; }
            if (Active == 20) { CheckHit(Battle, 20, 1, 1, TEXT("Rage only")); }
            else { CheckHit(Battle, 21, 2, 0, TEXT("Unprotected")); }
            Battle.Submit({ECombatCommandType::EndTurn, Active});
        }
    }
    return !HasAnyErrors();
}
#endif
