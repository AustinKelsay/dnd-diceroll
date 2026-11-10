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
        E.AttackBonus = 30; E.DamageDie = 1; E.DamageBonus = 4; // every hit is exactly 5 before protection
        return E;
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
        const FCombatResult Hit = Battle.Submit({ECombatCommandType::Attack, 20, {}, 1});
        TestTrue(TEXT("Fixture hit lands"), Hit.bHit);
        TestEqual(TEXT("Resolution prevents exactly the queried amount"), Hit.DamagePrevented, FMath::Min(Hit.Damage + Hit.DamagePrevented, Ward.Amount));
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
        const FCombatResult Hit = Battle.Submit({ECombatCommandType::Attack, 20, {}, 2});
        TestTrue(TEXT("Fixture hit lands"), Hit.bHit);
        TestEqual(TEXT("Resolution prevents exactly the queried shelter"), Hit.DamagePrevented, FMath::Min(Hit.Damage + Hit.DamagePrevented, Shelter.Amount));
        TestEqual(TEXT("Unknown combatant has no protection"), Battle.Protection(99).Amount, 0);
    }
    return !HasAnyErrors();
}
#endif
