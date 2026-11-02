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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTurnUndeadTest, "Project.Expedition.Features.TurnUndead",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FTurnUndeadTest::RunTest(const FString& Parameters)
{
    using namespace FeatureTest;
    FCombatantState Cleric = Hero(TEXT("cleric.life"), 2);
    FFeatureKit::Refill(Cleric);
    TestEqual(TEXT("One Channel Divinity at Cleric 2"), Cleric.ChannelUses, 1);
    TestTrue(TEXT("Listed"), FFeatureKit::Commands(Cleric).Contains(ECombatCommandType::TurnUndead));
    TestFalse(TEXT("Not at Cleric 1"), FFeatureKit::Commands(Hero(TEXT("cleric.life"), 1)).Contains(ECombatCommandType::TurnUndead));
    TestTrue(TEXT("Action tag"), FCombatBattle::ActionCost(ECombatCommandType::TurnUndead, Cleric) == EActionCost::Action && FCombatBattle::CostTag(ECombatCommandType::TurnUndead).StartsWith(TEXT("Action")));
    int32 TurnedRuns = 0;
    for (int32 Seed = 1; Seed <= 12; ++Seed)
    {
        FCombatEncounter Encounter = Duel(Cleric, {3, 0});
        Encounter.Combatants[1].Role = TEXT("Skeleton"); Encounter.Combatants[1].Name = TEXT("Skeleton");
        FCombatantState Far = Encounter.Combatants[1]; Far.Id = 21; Far.Cell = {9, 9};
        FCombatantState Living = Encounter.Combatants[1]; Living.Id = 22; Living.Role = TEXT("Raider"); Living.Cell = {0, 2};
        Encounter.Combatants.Add(Far); Encounter.Combatants.Add(Living);
        FCombatBattle Battle; FString Error;
        if (!TestTrue(TEXT("Start"), Battle.Start(Encounter, Seed, Error))) { AddError(Error); return true; }
        const FCombatResult Turn = Battle.Submit({ECombatCommandType::TurnUndead, 10, {}, 10});
        TestTrue(TEXT("Accepted"), Turn.bAccepted);
        TestFalse(TEXT("Action spent"), Battle.Snapshot().bActionAvailable);
        TestEqual(TEXT("Use spent"), Battle.Find(10)->ChannelUses, 0);
        TestTrue(TEXT("No second channel"), Battle.LegalTargets(10, ECombatCommandType::TurnUndead).IsEmpty());
        auto Feared = [&](int32 Id) { return Battle.Find(Id)->Conditions.ContainsByPredicate([](const FCombatCondition& C) { return C.Condition == ECombatCondition::Frightened && C.SourceId == 10; }); };
        TestFalse(TEXT("Out of range undead unaffected"), Feared(21));
        TestFalse(TEXT("Living foes unaffected"), Feared(22));
        TestEqual(TEXT("Hit flag matches the condition"), Turn.bHit, Feared(20));
        if (Feared(20)) { ++TurnedRuns; }
        FCombatBattle Copy; TestTrue(TEXT("Snapshot round-trips"), Copy.Restore(Battle.Snapshot(), Error));
    }
    TestTrue(TEXT("Some seeds turn the skeleton"), TurnedRuns > 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBardicInspirationTest, "Project.Expedition.Features.BardicInspiration",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FBardicInspirationTest::RunTest(const FString& Parameters)
{
    using namespace FeatureTest;
    FCombatantState Bard = Hero(TEXT("bard.lore"), 1);
    FFeatureKit::Refill(Bard);
    TestTrue(TEXT("At least one use"), Bard.BardicUses >= 1 && Bard.BardicUses == FFeatureKit::BardicCap(Bard));
    TestEqual(TEXT("d6 at Bard 1"), FFeatureKit::BardicSides(Bard), 6);
    TestEqual(TEXT("d8 at Bard 5"), FFeatureKit::BardicSides(Hero(TEXT("bard.lore"), 5)), 8);
    TestTrue(TEXT("Bonus tag"), FCombatBattle::ActionCost(ECombatCommandType::BardicInspiration, Bard) == EActionCost::BonusAction && FCombatBattle::CostTag(ECombatCommandType::BardicInspiration).StartsWith(TEXT("Bonus")));
    TestTrue(TEXT("Targets an ally"), FFeatureKit::PicksTarget(ECombatCommandType::BardicInspiration));
    int32 Used = 0, Kept = 0;
    for (int32 Seed = 1; Seed <= 30; ++Seed)
    {
        FCombatEncounter Line; Line.Width = Line.Height = 10;
        FCombatantState B = Bard; B.Id = 10; B.Cell = {0, 0}; B.InitiativeBonus = 30;
        FCombatantState F = Hero(TEXT("fighter.champion"), 1); F.Id = 11; F.Cell = {1, 0}; F.InitiativeBonus = 5;
        FCombatantState Foe; Foe.Id = 20; Foe.Name = TEXT("Foe"); Foe.Role = TEXT("Raider"); Foe.Side = ECombatSide::Enemy; Foe.Cell = {2, 0}; Foe.InitiativeBonus = -10; Foe.HP = Foe.MaxHP = 200;
        Line.Combatants = {B, F, Foe};
        FCombatBattle Battle; FString Error;
        if (!TestTrue(TEXT("Start"), Battle.Start(Line, Seed, Error))) { AddError(Error); return true; }
        const TArray<int32> Targets = Battle.LegalTargets(10, ECombatCommandType::BardicInspiration);
        TestTrue(TEXT("Ally targetable, not self or foe"), Targets.Contains(11) && !Targets.Contains(10) && !Targets.Contains(20));
        TestTrue(TEXT("Inspire"), Battle.Submit({ECombatCommandType::BardicInspiration, 10, {}, 11}).bAccepted);
        TestFalse(TEXT("Bonus action spent"), Battle.Snapshot().bBonusActionAvailable);
        TestTrue(TEXT("Action still free"), Battle.Snapshot().bActionAvailable);
        TestEqual(TEXT("Die held"), Battle.Find(11)->BardicDie, 6);
        TestEqual(TEXT("Use spent"), Battle.Find(10)->BardicUses, Bard.BardicUses - 1);
        { FCombatBattle Copy; TestTrue(TEXT("Snapshot round-trips"), Copy.Restore(Battle.Snapshot(), Error)); }
        TestTrue(TEXT("End bard turn"), Battle.Submit({ECombatCommandType::EndTurn, 10, {}, INDEX_NONE}).bAccepted);
        const FCombatResult Swing = Battle.Submit({ECombatCommandType::Attack, 11, {}, 20});
        TestTrue(TEXT("Ally attacks"), Swing.bAccepted);
        if (Swing.BardicRoll > 0)
        {
            ++Used;
            TestTrue(TEXT("Roll in range"), Swing.BardicRoll <= 6);
            TestEqual(TEXT("Die spent"), Battle.Find(11)->BardicDie, 0);
            TestEqual(TEXT("Total includes the die"), Swing.AttackTotal, Swing.NaturalRoll + Swing.AttackBonus);
        }
        else { ++Kept; TestEqual(TEXT("Unneeded die kept"), Battle.Find(11)->BardicDie, 6); }
    }
    TestTrue(TEXT("Both branches seen"), Used > 0 && Kept > 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWildShapeTest, "Project.Expedition.Features.WildShape",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FWildShapeTest::RunTest(const FString& Parameters)
{
    using namespace FeatureTest;
    FCombatantState Druid = Hero(TEXT("druid.moon"), 2);
    FFeatureKit::Refill(Druid);
    TestEqual(TEXT("Two uses at Druid 2"), Druid.WildShapeUses, 2);
    TestFalse(TEXT("None at Druid 1"), FFeatureKit::Commands(Hero(TEXT("druid.moon"), 1)).Contains(ECombatCommandType::WildShape));
    TestTrue(TEXT("Action tag"), FCombatBattle::ActionCost(ECombatCommandType::WildShape, Druid) == EActionCost::Action && FCombatBattle::CostTag(ECombatCommandType::WildShape).StartsWith(TEXT("Action")));
    FCombatantState Bad = Druid; Bad.WildShapeHP = 12;
    TestFalse(TEXT("Form HP above the wolf is invalid"), FFeatureKit::Valid(Bad));
    FCombatantState Shaped = Druid; Shaped.WildShapeHP = FFeatureKit::WolfHP;
    int32 Count = 0, Sides = 0; FCombatBattle::WeaponDice(Shaped, Count, Sides);
    TestEqual(TEXT("Bite 2d4"), FIntPoint(Count, Sides), FIntPoint(2, 4));
    TestEqual(TEXT("Melee reach in form"), FCombatBattle::ActionRange(Shaped, ECombatCommandType::Attack), 1);

    FCombatBattle Battle; FString Error;
    if (!TestTrue(TEXT("Start"), Battle.Start(Duel(Druid, {1, 0}), 3, Error))) { AddError(Error); return true; }
    const int32 AcBefore = Battle.EffectiveArmorClass(10);
    TestTrue(TEXT("Shift"), Battle.Submit({ECombatCommandType::WildShape, 10, {}, 10}).bAccepted);
    TestFalse(TEXT("Action spent"), Battle.Snapshot().bActionAvailable);
    TestEqual(TEXT("Form HP"), Battle.Find(10)->WildShapeHP, 11);
    TestEqual(TEXT("Use spent"), Battle.Find(10)->WildShapeUses, 1);
    TestEqual(TEXT("Wolf AC"), Battle.EffectiveArmorClass(10), 13);
    AddInfo(FString::Printf(TEXT("AC before %d"), AcBefore));
    TestTrue(TEXT("No spells in form"), Battle.LegalTargets(10, ECombatCommandType::CureWounds).IsEmpty());
    TestTrue(TEXT("Cannot shift again while shaped"), Battle.LegalTargets(10, ECombatCommandType::WildShape).IsEmpty());
    { FCombatBattle Copy; TestTrue(TEXT("Snapshot round-trips"), Copy.Restore(Battle.Snapshot(), Error)); }
    // The foe's hits drain the form first.
    TestTrue(TEXT("End turn"), Battle.Submit({ECombatCommandType::EndTurn, 10, {}, INDEX_NONE}).bAccepted);
    const int32 HpBefore = Battle.Find(10)->HP;
    int32 Dealt = 0;
    for (int32 I = 0; I < 1; ++I)
    {
        const FCombatResult Hit = Battle.Submit({ECombatCommandType::Attack, 20, {}, 10});
        TestTrue(TEXT("Foe attacks"), Hit.bAccepted);
        if (Hit.bHit) { Dealt += Hit.Damage; }
    }
    const FCombatantState* After = Battle.Find(10);
    TestEqual(TEXT("Form absorbs before HP"), HpBefore - After->HP, FMath::Max(0, Dealt - 11));
    TestEqual(TEXT("Form HP drained"), After->WildShapeHP, FMath::Max(0, 11 - Dealt));
    return true;
}
#endif