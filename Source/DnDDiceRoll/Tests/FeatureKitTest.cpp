#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Combat/CombatBattle.h"
#include "Party/BuildCatalog.h"
#include "Party/FeatureKit.h"
#include "Rules/WeaponRules.h"

namespace FeatureTest
{
/** One catalog hero (Id 10, cell 0,0, acts first) against a sturdy raider (Id 20). */
FCombatEncounter Duel(const FCombatantState& Hero, FIntPoint FoeCell)
{
    FCombatEncounter Encounter;
    Encounter.Width = Encounter.Height = 10;
    FCombatantState Unit = Hero; Unit.Id = 10; Unit.Cell = {0, 0}; Unit.InitiativeBonus = 30;
    Encounter.Combatants.Add(Unit);
    FCombatantState Foe;
    Foe.Id = 20; Foe.Name = TEXT("Foe"); Foe.Role = TEXT("Raider"); Foe.Side = ECombatSide::Enemy; Foe.Cell = FoeCell; Foe.InitiativeBonus = -10;
    Foe.HP = Foe.MaxHP = 200;
    Encounter.Combatants.Add(Foe);
    return Encounter;
}
FCombatantState Hero(const TCHAR* Build, int32 Level)
{
    FCombatantState Out; FBuildCatalog::Create(Build, 1, TEXT("Hero"), Out);
    Out.Level = Level; Out.MaxHP = Out.HP = FBuildCatalog::MaxHitPoints(Out);
    return Out;
}
FActionForecast Swing(const FCombatantState& Unit, FIntPoint FoeCell, ECombatCommandType Action = ECombatCommandType::Attack)
{
    FCombatBattle Battle; FString Error;
    if (!Battle.Start(Duel(Unit, FoeCell), 3, Error)) { return FActionForecast(); }
    return Battle.Forecast(10, Action, 20);
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFightingStyleTest, "Project.Expedition.Features.FightingStyle",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FFightingStyleTest::RunTest(const FString& Parameters)
{
    using namespace FeatureTest;
    FCombatantState Fighter = Hero(TEXT("fighter.champion"), 1);
    TestTrue(TEXT("Fighter 1 has a style"), FFeatureKit::HasFightingStyle(Fighter));
    TestTrue(TEXT("Creation assigns a default style"), FFeatureKit::Style(Fighter) != EFightingStyle::None);
    FCombatantState Paladin = Hero(TEXT("paladin.devotion"), 1);
    TestFalse(TEXT("Paladin 1 has no style yet"), FFeatureKit::HasFightingStyle(Paladin));
    Paladin.Level = 2;
    TestTrue(TEXT("Paladin 2 has a style"), FFeatureKit::HasFightingStyle(Paladin));
    FFeatureKit::Grant(Paladin, Hero(TEXT("paladin.devotion"), 1));
    TestTrue(TEXT("Level-up grants a default"), FFeatureKit::Style(Paladin) != EFightingStyle::None);
    FString Error;
    FCombatantState Wizard = Hero(TEXT("wizard.evocation"), 3);
    TestFalse(TEXT("Wizards cannot pick a style"), FFeatureKit::ChooseStyle(Wizard, EFightingStyle::Archery, Error));
    Wizard.FightingStyle = static_cast<int32>(EFightingStyle::Archery);
    TestFalse(TEXT("Invalid style state"), FFeatureKit::Valid(Wizard));
    { FCombatBattle Battle; AddInfo(TEXT("x")); TestFalse(TEXT("Start rejects a style the hero lacks"), Battle.Start(Duel(Wizard, {1, 0}), 3, Error)); }

    // Dueling: +2 damage with a one-handed melee weapon and nothing in the off hand.
    Fighter.MainHand = static_cast<int32>(EWeapon::Longsword); Fighter.OffHand = -1; Fighter.bShieldEquipped = false;
    FCombatantState Plain = Fighter; Plain.FightingStyle = 0;
    FCombatantState Duelist = Fighter; TestTrue(TEXT("Choose Dueling"), FFeatureKit::ChooseStyle(Duelist, EFightingStyle::Dueling, Error));
    const FActionForecast A = Swing(Plain, {1, 0}), B = Swing(Duelist, {1, 0});
    TestTrue(TEXT("Forecasts valid"), A.bValid && B.bValid);
    TestEqual(TEXT("Dueling adds 2 to minimum damage"), B.MinAmount, A.MinAmount + 2);
    TestEqual(TEXT("Dueling does not change the hit chance"), B.HitChanceOutOf400, A.HitChanceOutOf400);

    // Archery: +2 to hit with ranged weapons (+10 percentage points on a straight roll).
    FCombatantState Bow = Plain; Bow.MainHand = static_cast<int32>(EWeapon::Longbow); Bow.Ammunition = 20;
    FCombatantState Archer = Bow; FFeatureKit::ChooseStyle(Archer, EFightingStyle::Archery, Error);
    const FActionForecast C = Swing(Bow, {5, 0}), D = Swing(Archer, {5, 0});
    TestTrue(TEXT("Bow forecasts valid"), C.bValid && D.bValid);
    TestTrue(TEXT("Archery improves the hit chance"), D.HitChanceOutOf400 > C.HitChanceOutOf400);
    TestEqual(TEXT("Archery does not change damage"), D.MinAmount, C.MinAmount);
    FCombatantState SwordArcher = Fighter; FFeatureKit::ChooseStyle(SwordArcher, EFightingStyle::Archery, Error);
    TestEqual(TEXT("Archery ignores melee"), Swing(SwordArcher, {1, 0}).HitChanceOutOf400, A.HitChanceOutOf400);

    // Defense: +1 AC while wearing armor.
    FCombatantState Guard = Plain; FFeatureKit::ChooseStyle(Guard, EFightingStyle::Defense, Error);
    FCombatBattle P, Q;
    TestTrue(TEXT("Start plain"), P.Start(FeatureTest::Duel(Plain, {1, 0}), 3, Error));
    TestTrue(TEXT("Start defense"), Q.Start(FeatureTest::Duel(Guard, {1, 0}), 3, Error));
    TestEqual(TEXT("Defense adds 1 AC in armor"), Q.EffectiveArmorClass(10), P.EffectiveArmorClass(10) + (Plain.ArmorWorn >= 0 ? 1 : 0));

    // Defaults follow equipment.
    FCombatantState Great = Plain; Great.MainHand = static_cast<int32>(EWeapon::Greatsword);
    TestEqual(TEXT("Two-handed defaults to Great Weapon Fighting"), FFeatureKit::DefaultStyle(Great), EFightingStyle::GreatWeaponFighting);
    TestEqual(TEXT("Bow defaults to Archery"), FFeatureKit::DefaultStyle(Bow), EFightingStyle::Archery);

    // Protection: a shielded ally beside the target spends its reaction to impose disadvantage.
    FCombatEncounter Line;
    Line.Width = Line.Height = 10;
    FCombatantState Knight = Plain; Knight.Id = 10; Knight.Cell = {0, 0}; Knight.bShieldEquipped = true; Knight.InitiativeBonus = -10;
    FFeatureKit::ChooseStyle(Knight, EFightingStyle::Protection, Error);
    FCombatantState Ward = Hero(TEXT("wizard.evocation"), 1); Ward.Id = 11; Ward.Cell = {1, 0}; Ward.InitiativeBonus = -10;
    FCombatantState Foe; Foe.Id = 20; Foe.Name = TEXT("Foe"); Foe.Role = TEXT("Raider"); Foe.Side = ECombatSide::Enemy; Foe.Cell = {2, 0}; Foe.InitiativeBonus = 30;
    Line.Combatants = {Knight, Ward, Foe};
    FCombatBattle Battle;
    if (!TestTrue(TEXT("Start protection"), Battle.Start(Line, 3, Error))) { AddError(Error); }
    const FActionForecast Before = Battle.Forecast(20, ECombatCommandType::Attack, 11);
    const FCombatResult Hit = Battle.Submit({ECombatCommandType::Attack, 20, {0, 0}, 11});
    TestTrue(TEXT("Foe attacks the ward"), Hit.bAccepted);
    TestTrue(TEXT("Protection reacted"), Hit.bProtectionReaction);
    TestTrue(TEXT("Disadvantage imposed"), Hit.RollMode == ERollMode::Disadvantage || Before.RollMode == ERollMode::Advantage);
    TestTrue(TEXT("Knight's reaction is spent"), Battle.Find(10) && !Battle.Find(10)->bReactionAvailable);
    TestTrue(TEXT("Progress sheet names the style"), FBuildCatalog::ProgressSheet(Duelist).ContainsByPredicate([](const FString& L) { return L.StartsWith(TEXT("Fighting Style: Dueling")); }));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FActionSurgeTest, "Project.Expedition.Features.ActionSurge",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FActionSurgeTest::RunTest(const FString& Parameters)
{
    using namespace FeatureTest;
    FCombatantState One = Hero(TEXT("fighter.champion"), 1);
    TestFalse(TEXT("No Action Surge at Fighter 1"), FFeatureKit::Commands(One).Contains(ECombatCommandType::ActionSurge));
    FCombatantState Two = Hero(TEXT("fighter.champion"), 2);
    FFeatureKit::Refill(Two);
    TestEqual(TEXT("One use at Fighter 2"), Two.ActionSurgeUses, 1);
    FFeatureKit::Grant(Two, One);
    TestEqual(TEXT("Grant does not stack past the cap"), Two.ActionSurgeUses, 1);
    FCombatantState Spent = Two; Spent.ActionSurgeUses = 0;
    FFeatureKit::Grant(Spent, Two);
    TestEqual(TEXT("Grant at the same cap does not refresh a spent use"), Spent.ActionSurgeUses, 0);
    TestTrue(TEXT("Free tag"), FCombatBattle::ActionCost(ECombatCommandType::ActionSurge, Two) == EActionCost::Free);
    TestTrue(TEXT("Description is free"), FCombatBattle::CostTag(ECombatCommandType::ActionSurge).StartsWith(TEXT("Free")));
    FCombatantState Over = Two; Over.ActionSurgeUses = 2;
    TestFalse(TEXT("Uses above the cap are invalid"), FFeatureKit::Valid(Over));

    Two.MainHand = static_cast<int32>(EWeapon::Longsword); Two.OffHand = -1;
    FCombatBattle Battle; FString Error;
    if (!TestTrue(TEXT("Start"), Battle.Start(Duel(Two, {1, 0}), 3, Error))) { AddError(Error); return true; }
    TestTrue(TEXT("Not ready while the action is unspent"), Battle.LegalTargets(10, ECombatCommandType::ActionSurge).IsEmpty());
    TestTrue(TEXT("Rejected before the action"), !Battle.Submit({ECombatCommandType::ActionSurge, 10, {}, 10}).bAccepted);
    TestTrue(TEXT("Attack"), Battle.Submit({ECombatCommandType::Attack, 10, {}, 20}).bAccepted);
    TestFalse(TEXT("Action spent"), Battle.Snapshot().bActionAvailable);
    TestTrue(TEXT("Ready after the action"), Battle.LegalTargets(10, ECombatCommandType::ActionSurge).Contains(10));
    const FCombatResult Surge = Battle.Submit({ECombatCommandType::ActionSurge, 10, {}, 10});
    TestTrue(TEXT("Surge accepted"), Surge.bAccepted);
    TestTrue(TEXT("Action restored"), Battle.Snapshot().bActionAvailable);
    TestEqual(TEXT("Use spent"), Battle.Find(10)->ActionSurgeUses, 0);
    TestTrue(TEXT("Second attack"), Battle.Submit({ECombatCommandType::Attack, 10, {}, 20}).bAccepted);
    TestTrue(TEXT("No second surge"), Battle.LegalTargets(10, ECombatCommandType::ActionSurge).IsEmpty());
    { FCombatBattle Copy; TestTrue(TEXT("Snapshot round-trips"), Copy.Restore(Battle.Snapshot(), Error)); }
    return true;
}
#endif