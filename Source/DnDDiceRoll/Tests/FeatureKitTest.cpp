#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Rules/AbilityRules.h"
#include "Rules/ArmorRules.h"
#include "Combat/CombatBattle.h"
#include "Party/BuildCatalog.h"
#include "Party/FeatureKit.h"
#include "Rules/WeaponRules.h"
#include "Rules/ClassFeatureRules.h"

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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKiTest, "Project.Expedition.Features.Ki",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FKiTest::RunTest(const FString& Parameters)
{
    using namespace FeatureTest;
    TestEqual(TEXT("No ki at Monk 1"), FFeatureKit::KiCap(Hero(TEXT("monk.shadow"), 1)), 0);
    FCombatantState Monk = Hero(TEXT("monk.shadow"), 3);
    FFeatureKit::Refill(Monk);
    TestEqual(TEXT("Ki = Monk level"), Monk.KiPoints, 3);
    TestTrue(TEXT("Bonus tags"), FCombatBattle::ActionCost(ECombatCommandType::FlurryOfBlows, Monk) == EActionCost::BonusAction && FCombatBattle::ActionCost(ECombatCommandType::PatientDefense, Monk) == EActionCost::BonusAction && FCombatBattle::ActionCost(ECombatCommandType::StepOfTheWind, Monk) == EActionCost::BonusAction);
    FCombatantState Bad = Monk; Bad.KiPoints = 4;
    TestFalse(TEXT("Ki above cap invalid"), FFeatureKit::Valid(Bad));

    FString Error;
    {
        FCombatBattle Battle;
        if (!TestTrue(TEXT("Start"), Battle.Start(Duel(Monk, {1, 0}), 3, Error))) { AddError(Error); return true; }
        TestTrue(TEXT("No flurry before the Attack action"), Battle.LegalTargets(10, ECombatCommandType::FlurryOfBlows).IsEmpty());
        TestTrue(TEXT("Attack"), Battle.Submit({ECombatCommandType::Attack, 10, {}, 20}).bAccepted);
        TestTrue(TEXT("Flurry targets the adjacent foe"), Battle.LegalTargets(10, ECombatCommandType::FlurryOfBlows).Contains(20));
        const int32 FoeHp = Battle.Find(20)->HP;
        const FCombatResult Flurry = Battle.Submit({ECombatCommandType::FlurryOfBlows, 10, {}, 20});
        TestTrue(TEXT("Flurry"), Flurry.bAccepted);
        TestEqual(TEXT("Damage matches"), FoeHp - Battle.Find(20)->HP, Flurry.Damage);
        TestEqual(TEXT("Ki spent"), Battle.Find(10)->KiPoints, 2);
        TestFalse(TEXT("Bonus spent"), Battle.Snapshot().bBonusActionAvailable);
        TestTrue(TEXT("Only one bonus"), Battle.LegalTargets(10, ECombatCommandType::PatientDefense).IsEmpty());
        FCombatBattle Copy; TestTrue(TEXT("Snapshot round-trips"), Copy.Restore(Battle.Snapshot(), Error));
    }
    {
        FCombatBattle Battle; Battle.Start(Duel(Monk, {1, 0}), 3, Error);
        TestTrue(TEXT("Patient Defense"), Battle.Submit({ECombatCommandType::PatientDefense, 10, {}, 10}).bAccepted);
        TestTrue(TEXT("Dodging"), Battle.Find(10)->bDodging);
        TestTrue(TEXT("Action still free"), Battle.Snapshot().bActionAvailable);
    }
    {
        FCombatBattle Battle; Battle.Start(Duel(Monk, {1, 0}), 3, Error);
        TestTrue(TEXT("Step of the Wind"), Battle.Submit({ECombatCommandType::StepOfTheWind, 10, {}, 10}).bAccepted);
        TestTrue(TEXT("Disengaged"), Battle.Find(10)->bDisengaged);
    }
    Monk.KiPoints = 0; FFeatureKit::ShortRest(Monk);
    TestEqual(TEXT("Short rest refills ki"), Monk.KiPoints, 3);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSorceryPointsTest, "Project.Expedition.Features.SorceryPoints",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSorceryPointsTest::RunTest(const FString& Parameters)
{
    using namespace FeatureTest;
    TestEqual(TEXT("None at Sorcerer 1"), FFeatureKit::SorceryCap(Hero(TEXT("sorcerer.wild-magic"), 1)), 0);
    FCombatantState Sorcerer = Hero(TEXT("sorcerer.wild-magic"), 4);
    FFeatureKit::Refill(Sorcerer);
    TestEqual(TEXT("Points = Sorcerer level"), Sorcerer.SorceryPoints, 4);
    TestTrue(TEXT("Bonus tag"), FCombatBattle::ActionCost(ECombatCommandType::FontOfMagic, Sorcerer) == EActionCost::BonusAction);
    const int32 Cap = FBuildCatalog::Find(Sorcerer.BuildId)->KitCap;
    FString Error;
    {
        FCombatBattle Battle; Battle.Start(Duel(Sorcerer, {3, 0}), 3, Error);
        TestTrue(TEXT("Nothing to regain at full uses"), Battle.LegalTargets(10, ECombatCommandType::FontOfMagic).IsEmpty());
    }
    Sorcerer.KitUses = FMath::Max(0, Cap - 2);
    FCombatBattle Battle;
    if (!TestTrue(TEXT("Start"), Battle.Start(Duel(Sorcerer, {3, 0}), 3, Error))) { AddError(Error); return true; }
    TestTrue(TEXT("Font"), Battle.Submit({ECombatCommandType::FontOfMagic, 10, {}, 10}).bAccepted);
    TestEqual(TEXT("One use regained"), Battle.Find(10)->KitUses, Sorcerer.KitUses + 1);
    TestEqual(TEXT("Two points spent"), Battle.Find(10)->SorceryPoints, 2);
    TestFalse(TEXT("Bonus spent"), Battle.Snapshot().bBonusActionAvailable);
    FCombatBattle Copy; TestTrue(TEXT("Snapshot round-trips"), Copy.Restore(Battle.Snapshot(), Error));
    FCombatantState Bad = Sorcerer; Bad.SorceryPoints = 5;
    TestFalse(TEXT("Points above cap invalid"), FFeatureKit::Valid(Bad));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUncannyDodgeTest, "Project.Expedition.Features.UncannyDodge",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FUncannyDodgeTest::RunTest(const FString& Parameters)
{
    using namespace FeatureTest;
    TestFalse(TEXT("Not at Rogue 4"), FFeatureKit::HasUncannyDodge(Hero(TEXT("rogue.thief"), 4)));
    FCombatantState Rogue = Hero(TEXT("rogue.thief"), 5);
    Rogue.HP = Rogue.MaxHP = 500;
    TestTrue(TEXT("Rogue 5"), FFeatureKit::HasUncannyDodge(Rogue));
    TestTrue(TEXT("Reaction tag"), FCombatBattle::ActionCost(ECombatCommandType::UncannyDodge, Rogue) == EActionCost::Reaction && FCombatBattle::CostTag(ECombatCommandType::UncannyDodge).StartsWith(TEXT("Reaction")));
    FString Error; FCombatBattle Battle;
    FCombatEncounter Encounter = Duel(Rogue, {1, 0});
    Encounter.Combatants[1].AttackBonus = 30; Encounter.Combatants[1].DamageBonus = 9;
    if (!TestTrue(TEXT("Start"), Battle.Start(Encounter, 3, Error))) { AddError(Error); return true; }
    TestTrue(TEXT("Not pressable"), Battle.LegalTargets(10, ECombatCommandType::UncannyDodge).IsEmpty());
    TestTrue(TEXT("End turn"), Battle.Submit({ECombatCommandType::EndTurn, 10, {}, INDEX_NONE}).bAccepted);
    const int32 Before = Battle.Find(10)->HP;
    const FCombatResult First = Battle.Submit({ECombatCommandType::Attack, 20, {}, 10});
    TestTrue(TEXT("Hit"), First.bAccepted && First.bHit);
    TestTrue(TEXT("Dodged"), First.bUncannyDodge);
    TestEqual(TEXT("Damage applied matches the halved hit"), Before - Battle.Find(10)->HP, First.Damage);
    TestFalse(TEXT("Reaction spent"), Battle.Find(10)->bReactionAvailable);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRecklessAttackTest, "Project.Expedition.Features.RecklessAttack",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRecklessAttackTest::RunTest(const FString& Parameters)
{
    using namespace FeatureTest;
    TestFalse(TEXT("Not at Barbarian 1"), FFeatureKit::HasRecklessAttack(Hero(TEXT("barbarian.berserker"), 1)));
    FCombatantState Barbarian = Hero(TEXT("barbarian.berserker"), 2);
    TestTrue(TEXT("Free tag"), FCombatBattle::ActionCost(ECombatCommandType::RecklessAttack, Barbarian) == EActionCost::Free);
    FString Error; FCombatBattle Battle;
    if (!TestTrue(TEXT("Start"), Battle.Start(Duel(Barbarian, {1, 0}), 3, Error))) { AddError(Error); return true; }
    TestTrue(TEXT("Go reckless"), Battle.Submit({ECombatCommandType::RecklessAttack, 10, {}, 10}).bAccepted);
    TestTrue(TEXT("Flag set"), Battle.Find(10)->bReckless);
    TestTrue(TEXT("Action untouched"), Battle.Snapshot().bActionAvailable);
    TestTrue(TEXT("Once per turn"), Battle.LegalTargets(10, ECombatCommandType::RecklessAttack).IsEmpty());
    TestTrue(TEXT("Own melee attack has advantage"), Battle.Forecast(10, ECombatCommandType::Attack, 20).AdvantageSources.Contains(TEXT("reckless attack")));
    { FCombatBattle Copy; TestTrue(TEXT("Snapshot round-trips"), Copy.Restore(Battle.Snapshot(), Error)); }
    TestTrue(TEXT("Attack"), Battle.Submit({ECombatCommandType::Attack, 10, {}, 20}).bAccepted);
    TestTrue(TEXT("End turn"), Battle.Submit({ECombatCommandType::EndTurn, 10, {}, INDEX_NONE}).bAccepted);
    TestTrue(TEXT("Foe gets advantage"), Battle.Forecast(20, ECombatCommandType::Attack, 10).AdvantageSources.Contains(TEXT("target reckless")));
    TestTrue(TEXT("Foe ends turn"), Battle.Submit({ECombatCommandType::EndTurn, 20, {}, INDEX_NONE}).bAccepted);
    TestFalse(TEXT("Cleared at the barbarian's next turn"), Battle.Find(10)->bReckless);
    FCombatantState Bad = Hero(TEXT("wizard.evocation"), 2); Bad.bReckless = true;
    TestFalse(TEXT("Non-barbarian reckless invalid"), FFeatureKit::Valid(Bad));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FArcaneRecoveryTest, "Project.Expedition.Features.ArcaneRecovery",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FArcaneRecoveryTest::RunTest(const FString& Parameters)
{
    using namespace FeatureTest;
    FCombatantState Evoker = Hero(TEXT("wizard.evocation"), 3);
    FFeatureKit::Refill(Evoker);
    TestTrue(TEXT("Wizard has it"), FFeatureKit::HasArcaneRecovery(Evoker));
    TestEqual(TEXT("Nothing to recover when full"), FFeatureKit::ArcaneRecoveryPreview(Evoker), 0);
    const int32 Full = Evoker.SpellSlots;
    Evoker.SpellSlots = 0; Evoker.KitUses = 0;
    if (!Evoker.SlotsByLevel.IsEmpty()) { Evoker.SlotsByLevel[0] = 0; }
    TestEqual(TEXT("Preview: 2 slot levels at Wizard 3"), FFeatureKit::ArcaneRecoveryPreview(Evoker), FMath::Min(2, Full));
    TestEqual(TEXT("Recover"), FFeatureKit::ArcaneRecover(Evoker), FMath::Min(2, Full));
    TestEqual(TEXT("Slots back"), Evoker.SpellSlots, FMath::Min(2, Full));
    TestTrue(TEXT("Marked used"), Evoker.bArcaneRecovered);
    TestTrue(TEXT("Still valid"), FFeatureKit::Valid(Evoker) && FBuildCatalog::ValidResources(Evoker));
    Evoker.SpellSlots = 0;
    TestEqual(TEXT("Once per long rest"), FFeatureKit::ArcaneRecover(Evoker), 0);
    FFeatureKit::Refill(Evoker);
    TestFalse(TEXT("Long rest resets"), Evoker.bArcaneRecovered);

    FCombatantState Illusionist = Hero(TEXT("wizard.illusion"), 1);
    const int32 Cap = FBuildCatalog::Find(Illusionist.BuildId)->KitCap;
    Illusionist.KitUses = 0;
    TestEqual(TEXT("Kit wizard regains one use at Wizard 1"), FFeatureKit::ArcaneRecover(Illusionist), FMath::Min(1, Cap));
    FCombatantState Fighter = Hero(TEXT("fighter.champion"), 3); Fighter.bArcaneRecovered = true;
    TestFalse(TEXT("Non-wizard flag invalid"), FFeatureKit::Valid(Fighter));
    TestTrue(TEXT("Free tag in the rules table"), FClassFeatureRules::FeaturesThrough(EAdventurerRole::Wizard, 1).ContainsByPredicate([](const FClassFeature& F) { return FCString::Strcmp(F.Name, TEXT("Arcane Recovery")) == 0 && F.bPlayable; }));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPactMagicTest, "Project.Expedition.Features.PactMagic",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FPactMagicTest::RunTest(const FString& Parameters)
{
    using namespace FeatureTest;
    for (int32 Level = 1; Level <= 5; ++Level)
    {
        FCombatantState Lock = Hero(TEXT("warlock.fiend"), Level);
        FFeatureKit::Refill(Lock);
        TestTrue(FString::Printf(TEXT("Warlock %d has Pact Magic"), Level), FFeatureKit::HasPactMagic(Lock));
        const FCombatantState Full = Lock;
        Lock.KitUses = 0;
        const int32 PactLevel = FMath::Min(5, (Level + 1) / 2);
        if (Lock.SlotsByLevel.IsValidIndex(PactLevel - 1) && PactLevel > 1) { Lock.SlotsByLevel[PactLevel - 1] = 0; }
        FFeatureKit::ShortRest(Lock);
        TestEqual(FString::Printf(TEXT("Warlock %d: short rest restores kit uses"), Level), Lock.KitUses, Full.KitUses);
        TestEqual(FString::Printf(TEXT("Warlock %d: short rest restores pact slots"), Level), Lock.SlotsByLevel, Full.SlotsByLevel);
        TestTrue(FString::Printf(TEXT("Warlock %d: still valid"), Level), FBuildCatalog::ValidResources(Lock) && FFeatureKit::Valid(Lock));
    }
    FCombatantState FighterLock = Hero(TEXT("fighter.champion"), 3); FighterLock.SecondaryRole = EAdventurerRole::Warlock; FighterLock.SecondaryLevel = 1;
    TestTrue(TEXT("Fighter/Warlock 1: Pact Magic (class level, multiclass-safe)"), FFeatureKit::HasPactMagic(FighterLock));
    FCombatantState WizardLock = Hero(TEXT("wizard.evocation"), 3); WizardLock.SecondaryRole = EAdventurerRole::Warlock; WizardLock.SecondaryLevel = 1;
    TestFalse(TEXT("Wizard/Warlock: the wizard pool is not refilled on a short rest"), FFeatureKit::HasPactMagic(WizardLock));
    TestFalse(TEXT("Bard has no Pact Magic"), FFeatureKit::HasPactMagic(Hero(TEXT("bard.lore"), 3)));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCureWisdomTest, "Project.Expedition.Features.CureWoundsWisdom",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FCureWisdomTest::RunTest(const FString& Parameters)
{
    using namespace FeatureTest;
    FString Error;
    auto Check = [&](FCombatantState Unit, const TCHAR* Label)
    {
        Unit.HP = 1;
        FCombatBattle Battle;
        if (!TestTrue(Label, Battle.Start(Duel(Unit, {5, 0}), 3, Error))) { AddError(Error); return; }
        const int32 Mod = Battle.AbilityModifier(10, EAbility::Wisdom);
        TestEqual(FString(Label) + TEXT(": CureModifier is WIS"), Battle.CureModifier(10), Mod);
        const FActionForecast F = Battle.Forecast(10, ECombatCommandType::CureWounds, 10);
        if (TestTrue(FString(Label) + TEXT(": forecast"), F.bValid))
        {
            TestEqual(FString(Label) + TEXT(": min 1 + WIS"), F.MinAmount, FMath::Clamp(1 + Mod, 0, Unit.MaxHP - 1));
            TestEqual(FString(Label) + TEXT(": max 8 + WIS"), F.MaxAmount, FMath::Clamp(8 + Mod, 0, Unit.MaxHP - 1));
        }
        const int32 Before = Battle.Find(10)->HP;
        const FCombatResult R = Battle.Submit({ECombatCommandType::CureWounds, 10, {}, 10});
        if (TestTrue(FString(Label) + TEXT(": cast"), R.bAccepted))
        {
            TestEqual(FString(Label) + TEXT(": result modifier"), R.EffectModifier, Mod);
            const int32 Healed = Battle.Find(10)->HP - Before;
            TestTrue(FString(Label) + TEXT(": healing within 1d8+WIS"), Healed >= FMath::Max(1, 1 + Mod) - 0 && Healed <= 8 + Mod);
        }
    };
    Check(Hero(TEXT("cleric.life"), 1), TEXT("Life cleric"));
    FCombatantState Multi = Hero(TEXT("fighter.champion"), 2);
    Multi.SecondaryRole = EAdventurerRole::Cleric; Multi.SecondaryLevel = 1; Multi.Level = 2;
    Multi.SpellSlots = 2; Multi.SlotsByLevel.Reset(); // multiclass slots ride on the flat pool
    Multi.MaxHP = FBuildCatalog::MaxHitPoints(Multi);
    TestTrue(TEXT("Multiclass grants Cure"), FBuildCatalog::HasSpecial(Multi, ECombatCommandType::CureWounds));
    Check(Multi, TEXT("Fighter/Cleric"));
    TestTrue(TEXT("Description names WIS"), FCombatBattle::DescribeAction(ECombatCommandType::CureWounds).Contains(TEXT("WIS")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSecondClassFeaturesTest, "Project.Expedition.Features.SecondClass",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSecondClassFeaturesTest::RunTest(const FString& Parameters)
{
    using namespace FeatureTest;
    auto Multi = [](const TCHAR* Build, int32 Level, EAdventurerRole Second, int32 SecondLevel)
    {
        FCombatantState H = Hero(Build, Level);
        H.SecondaryRole = Second; H.SecondaryLevel = SecondLevel;
        H.SlotsByLevel.Reset(); H.SpellSlots = FMath::Min(H.SpellSlots, FBuildCatalog::SlotCap(H));
        H.MaxHP = H.HP = FBuildCatalog::MaxHitPoints(H);
        FFeatureKit::Refill(H);
        return H;
    };
    FString Error;
    auto Starts = [&](const FCombatantState& H, const TCHAR* Label)
    {
        FCombatBattle Battle;
        TestTrue(FString(Label) + TEXT(" starts"), Battle.Start(Duel(H, {1, 0}), 3, Error));
        if (!Error.IsEmpty()) { AddError(Error); Error.Reset(); }
        TestTrue(FString(Label) + TEXT(" valid"), FFeatureKit::Valid(H));
    };
    const FCombatantState RogueFighter = Multi(TEXT("rogue.thief"), 4, EAdventurerRole::Fighter, 2);
    TestEqual(TEXT("Fighter 2 secondary: Action Surge"), RogueFighter.ActionSurgeUses, 1);
    TestTrue(TEXT("Fighter 1 secondary: Fighting Style"), FFeatureKit::HasFightingStyle(RogueFighter));
    TestFalse(TEXT("Rogue 2 primary: no Uncanny Dodge yet"), FFeatureKit::HasUncannyDodge(RogueFighter));
    Starts(RogueFighter, TEXT("Rogue/Fighter"));
    const FCombatantState FighterMonk = Multi(TEXT("fighter.champion"), 4, EAdventurerRole::Monk, 2);
    TestEqual(TEXT("Monk 2 secondary: 2 ki"), FighterMonk.KiPoints, 2);
    TestTrue(TEXT("Flurry listed"), FFeatureKit::Commands(FighterMonk).Contains(ECombatCommandType::FlurryOfBlows));
    Starts(FighterMonk, TEXT("Fighter/Monk"));
    const FCombatantState FighterBarb = Multi(TEXT("fighter.champion"), 4, EAdventurerRole::Barbarian, 2);
    TestTrue(TEXT("Barbarian 2 secondary: Reckless"), FFeatureKit::HasRecklessAttack(FighterBarb));
    Starts(FighterBarb, TEXT("Fighter/Barbarian"));
    const FCombatantState ClericDruid = Multi(TEXT("cleric.life"), 4, EAdventurerRole::Druid, 2);
    TestEqual(TEXT("Druid 2 secondary: Wild Shape"), ClericDruid.WildShapeUses, 2);
    Starts(ClericDruid, TEXT("Cleric/Druid"));
    const FCombatantState FighterSorc = Multi(TEXT("fighter.champion"), 4, EAdventurerRole::Sorcerer, 2);
    TestEqual(TEXT("Sorcerer 2 secondary: points"), FighterSorc.SorceryPoints, 2);
    Starts(FighterSorc, TEXT("Fighter/Sorcerer"));
    FCombatantState FighterWizard = Multi(TEXT("fighter.champion"), 3, EAdventurerRole::Wizard, 1);
    FighterWizard.SpellSlots = FBuildCatalog::SlotCap(FighterWizard);
    TestTrue(TEXT("Wizard 1 secondary: Arcane Recovery"), FFeatureKit::HasArcaneRecovery(FighterWizard));
    if (FighterWizard.SpellSlots > 0)
    {
        FighterWizard.SpellSlots -= 1;
        TestEqual(TEXT("Recovers one level-1 slot"), FFeatureKit::ArcaneRecover(FighterWizard), 1);
        TestTrue(TEXT("Resources still valid"), FBuildCatalog::ValidResources(FighterWizard));
    }
    const FCombatantState FighterBard = Multi(TEXT("fighter.champion"), 3, EAdventurerRole::Bard, 1);
    TestTrue(TEXT("Bard 1 secondary: inspiration"), FighterBard.BardicUses >= 1);
    Starts(FighterBard, TEXT("Fighter/Bard"));
    const FCombatantState FighterCleric = Multi(TEXT("fighter.champion"), 4, EAdventurerRole::Cleric, 2);
    TestEqual(TEXT("Cleric 2 secondary: Channel Divinity"), FighterCleric.ChannelUses, 1);
    TestTrue(TEXT("Turn Undead listed"), FFeatureKit::Commands(FighterCleric).Contains(ECombatCommandType::TurnUndead));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRewardSheetTest, "Project.Expedition.Features.RewardSheet",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRewardSheetTest::RunTest(const FString& Parameters)
{
    using namespace FeatureTest;
    FCombatantState Fighter = Hero(TEXT("fighter.champion"), 2);
    FFeatureKit::Refill(Fighter);
    const TArray<FString> Lines = FFeatureKit::RewardSheet(Fighter);
    TestEqual(TEXT("One line per feature command"), Lines.Num(), FFeatureKit::Commands(Fighter).Num());
    TestTrue(TEXT("Action Surge tagged Free with uses"), Lines.ContainsByPredicate([](const FString& L) { return L.StartsWith(TEXT("Action Surge [Free]")); }));
    FCombatantState Evoker = Hero(TEXT("wizard.evocation"), 3);
    FFeatureKit::Refill(Evoker);
    TestTrue(TEXT("Wizard lists Arcane Recovery"), FFeatureKit::RewardSheet(Evoker).ContainsByPredicate([](const FString& L) { return L.StartsWith(TEXT("Arcane Recovery [Free]")); }));
    Evoker.bArcaneRecovered = true;
    TestTrue(TEXT("Shows used"), FFeatureKit::RewardSheet(Evoker).ContainsByPredicate([](const FString& L) { return L.Contains(TEXT("used until camp")); }));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMartialArtsTest, "Project.Expedition.Features.MartialArts",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FMartialArtsTest::RunTest(const FString& Parameters)
{
    using namespace FeatureTest;
    const FCombatantState Monk = Hero(TEXT("monk.open-hand"), 1);
    TestTrue(TEXT("Monk 1 has Martial Arts"), FFeatureKit::HasMartialArts(Monk));
    TestFalse(TEXT("Fighter lacks it"), FFeatureKit::HasMartialArts(Hero(TEXT("fighter.champion"), 1)));
    TestTrue(TEXT("Bonus tag"), FFeatureKit::Cost(ECombatCommandType::MartialArts) == EActionCost::BonusAction);
    FCombatBattle Battle; FString Error;
    TestTrue(TEXT("Starts"), Battle.Start(Duel(Monk, {1, 0}), 3, Error));
    TestTrue(TEXT("Not before the Attack action"), Battle.LegalTargets(10, ECombatCommandType::MartialArts).IsEmpty());
    TestTrue(TEXT("Attack"), Battle.Submit({ECombatCommandType::Attack, 10, {}, 20}).bAccepted);
    TestTrue(TEXT("Offered after Attack"), Battle.LegalTargets(10, ECombatCommandType::MartialArts).Contains(20));
    const int32 Ki = Battle.Find(10)->KiPoints;
    TestTrue(TEXT("Strike"), Battle.Submit({ECombatCommandType::MartialArts, 10, {}, 20}).bAccepted);
    TestEqual(TEXT("No ki spent"), Battle.Find(10)->KiPoints, Ki);
    TestTrue(TEXT("Bonus action spent"), Battle.LegalTargets(10, ECombatCommandType::MartialArts).IsEmpty());
    TestFalse(TEXT("Table entry playable"), FClassFeatureRules::FeatureSummary(EAdventurerRole::Monk, 1).Contains(TEXT("*")));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeflectMissilesTest, "Project.Expedition.Features.DeflectMissiles",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FDeflectMissilesTest::RunTest(const FString& Parameters)
{
    using namespace FeatureTest;
    TestFalse(TEXT("Not at Monk 2"), FFeatureKit::HasDeflectMissiles(Hero(TEXT("monk.open-hand"), 2)));
    const FCombatantState Monk = Hero(TEXT("monk.open-hand"), 3);
    TestTrue(TEXT("Monk 3 has it"), FFeatureKit::HasDeflectMissiles(Monk));
    TestTrue(TEXT("Reaction tag"), FFeatureKit::Cost(ECombatCommandType::DeflectMissiles) == EActionCost::Reaction);
    int32 Deflected = 0, Hits = 0;
    for (int32 Seed = 1; Seed <= 40 && Hits < 6; ++Seed)
    {
        FCombatEncounter Encounter = Duel(Monk, {5, 0});
        Encounter.Combatants[1].Range = 6; Encounter.Combatants[1].AttackBonus = 20; Encounter.Combatants[1].DamageDie = 4; Encounter.Combatants[1].InitiativeBonus = 30; Encounter.Combatants[0].InitiativeBonus = -10;
        FCombatBattle Battle; FString Error;
        if (!Battle.Start(Encounter, Seed, Error)) { AddError(Error); return false; }
        const FCombatResult Shot = Battle.Submit({ECombatCommandType::Attack, 20, {}, 10});
        if (!Shot.bAccepted || !Shot.bHit) { continue; }
        ++Hits;
        if (Shot.RollModeSources.ContainsByPredicate([](const FString& S) { return S.StartsWith(TEXT("Deflect Missiles")); })) { ++Deflected; TestFalse(TEXT("Reaction spent"), Battle.Find(10)->bReactionAvailable); }
    }
    TestTrue(TEXT("Some ranged hits"), Hits > 0);
    TestEqual(TEXT("Every ranged hit deflected"), Deflected, Hits);
    FCombatEncounter Melee = Duel(Monk, {1, 0}); Melee.Combatants[1].AttackBonus = 20; Melee.Combatants[1].InitiativeBonus = 30; Melee.Combatants[0].InitiativeBonus = -10;
    FCombatBattle Battle; FString Error; Battle.Start(Melee, 2, Error);
    const FCombatResult Punch = Battle.Submit({ECombatCommandType::Attack, 20, {}, 10});
    TestFalse(TEXT("Melee hits are not deflected"), Punch.RollModeSources.ContainsByPredicate([](const FString& S) { return S.StartsWith(TEXT("Deflect")); }));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSacredWeaponTest, "Project.Expedition.Features.SacredWeapon",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSacredWeaponTest::RunTest(const FString& Parameters)
{
    using namespace FeatureTest;
    FCombatantState Low = Hero(TEXT("paladin.devotion"), 2); FFeatureKit::Refill(Low);
    TestFalse(TEXT("Not at Paladin 2"), FFeatureKit::Commands(Low).Contains(ECombatCommandType::SacredWeapon));
    FCombatantState Paladin = Hero(TEXT("paladin.devotion"), 3); FFeatureKit::Refill(Paladin);
    TestEqual(TEXT("One Channel Divinity use"), Paladin.ChannelUses, 1);
    TestFalse(TEXT("Paladins do not Turn Undead"), FFeatureKit::Commands(Paladin).Contains(ECombatCommandType::TurnUndead));
    TestTrue(TEXT("Action tag"), FFeatureKit::Cost(ECombatCommandType::SacredWeapon) == EActionCost::Action);
    FCombatBattle Battle; FString Error;
    TestTrue(TEXT("Starts"), Battle.Start(Duel(Paladin, {1, 0}), 3, Error));
    const int32 Before = Battle.Forecast(10, ECombatCommandType::Attack, 20).HitChanceOutOf400;
    TestTrue(TEXT("Channel"), Battle.Submit({ECombatCommandType::SacredWeapon, 10, {}, 10}).bAccepted);
    TestEqual(TEXT("Use spent"), Battle.Find(10)->ChannelUses, 0);
    TestEqual(TEXT("Ten rounds"), Battle.Find(10)->SacredWeaponTurns, FFeatureKit::SacredWeaponRounds);
    TestTrue(TEXT("Action spent"), Battle.LegalTargets(10, ECombatCommandType::Attack).IsEmpty());
    FCombatantState Blessed = Paladin; Blessed.SacredWeaponTurns = 5;
    const FActionForecast After = Swing(Blessed, {1, 0});
    TestTrue(TEXT("Blessed forecast valid"), After.bValid);
    TestTrue(TEXT("Attack chance improves"), After.HitChanceOutOf400 > Before || Before >= 380);
    TestTrue(TEXT("End turn"), Battle.Submit({ECombatCommandType::EndTurn, 10, {}, 10}).bAccepted);
    TestEqual(TEXT("Ticks down at end of own turn"), Battle.Find(10)->SacredWeaponTurns, FFeatureKit::SacredWeaponRounds - 1);
    FCombatantState Bad = Hero(TEXT("fighter.champion"), 3); Bad.SacredWeaponTurns = 2;
    TestFalse(TEXT("Non-paladin sacred weapon invalid"), FFeatureKit::Valid(Bad));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FQuickenFlexTest, "Project.Expedition.Features.Metamagic",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FQuickenFlexTest::RunTest(const FString& Parameters)
{
    using namespace FeatureTest;
    FCombatantState Two = Hero(TEXT("sorcerer.draconic-bloodline"), 2); FFeatureKit::Refill(Two);
    TestFalse(TEXT("No Quickened Spell at Sorcerer 2"), FFeatureKit::Commands(Two).Contains(ECombatCommandType::QuickenedSpell));
    TestTrue(TEXT("Flexible Casting at Sorcerer 2"), FFeatureKit::Commands(Two).Contains(ECombatCommandType::FlexibleCasting));
    TestTrue(TEXT("Quickened tagged Free"), FFeatureKit::Cost(ECombatCommandType::QuickenedSpell) == EActionCost::Free);
    TestTrue(TEXT("Flexible tagged Bonus"), FFeatureKit::Cost(ECombatCommandType::FlexibleCasting) == EActionCost::BonusAction);
    {
        FCombatBattle Battle; FString Error;
        TestTrue(TEXT("Starts full"), Battle.Start(Duel(Two, {2, 0}), 3, Error));
        TestTrue(TEXT("Full points: no conversion"), Battle.LegalTargets(10, ECombatCommandType::FlexibleCasting).IsEmpty());
        FCombatantState Empty = Two; Empty.SorceryPoints = 0;
        FCombatBattle Other; TestTrue(TEXT("Starts empty"), Other.Start(Duel(Empty, {2, 0}), 3, Error));
        const int32 Kit = Other.Find(10)->KitUses;
        TestTrue(TEXT("Convert"), Other.Submit({ECombatCommandType::FlexibleCasting, 10, {}, 10}).bAccepted);
        TestEqual(TEXT("Two points gained"), Other.Find(10)->SorceryPoints, 2);
        TestEqual(TEXT("One spell use spent"), Other.Find(10)->KitUses, Kit - 1);
    }
    FCombatantState Three = Hero(TEXT("sorcerer.draconic-bloodline"), 3); FFeatureKit::Refill(Three);
    FCombatBattle Battle; FString Error;
    TestTrue(TEXT("Starts"), Battle.Start(Duel(Three, {2, 0}), 3, Error));
    TestTrue(TEXT("Quicken"), Battle.Submit({ECombatCommandType::QuickenedSpell, 10, {}, 10}).bAccepted);
    TestEqual(TEXT("Points spent"), Battle.Find(10)->SorceryPoints, 1);
    TestTrue(TEXT("Action spell offered"), Battle.LegalTargets(10, ECombatCommandType::Special1).Contains(20));
    TestTrue(TEXT("Cast quickened"), Battle.Submit({ECombatCommandType::Special1, 10, {}, 20}).bAccepted);
    TestEqual(TEXT("Quicken spent"), Battle.Find(10)->QuickenState, 2);
    TestTrue(TEXT("Action still free"), Battle.LegalTargets(10, ECombatCommandType::Dodge).Num() > 0);
    TestTrue(TEXT("Bonus spent"), Battle.LegalTargets(10, ECombatCommandType::Special2).IsEmpty());
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUnarmoredDefenseTest, "Project.Expedition.Features.UnarmoredDefense",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FUnarmoredDefenseTest::RunTest(const FString& Parameters)
{
    using namespace FeatureTest;
    for (const auto* Id : {TEXT("barbarian.berserker"), TEXT("monk.open-hand"), TEXT("rogue.thief")})
    {
        FCombatantState H = Hero(Id, 1); H.ArmorWorn = static_cast<int32>(EArmor::None); H.bShieldEquipped = false;
        FCombatBattle Battle; FString Error;
        if (!TestTrue(TEXT("Starts"), Battle.Start(Duel(H, {4, 0}), 3, Error))) { continue; }
        FAbilityScores S; FBuildCatalog::HeroScores(*Battle.Find(10), S);
        const int32 Base = 10 + FAbilityRules::Modifier(S.Dexterity);
        const int32 Extra = FCString::Strstr(Id, TEXT("barbarian")) ? FAbilityRules::Modifier(S.Constitution) : FCString::Strstr(Id, TEXT("monk")) ? FAbilityRules::Modifier(S.Wisdom) : 0;
        TestEqual(Id, Battle.EffectiveArmorClass(10), Base + FMath::Max(0, Extra));
    }
    return true;
}
#endif