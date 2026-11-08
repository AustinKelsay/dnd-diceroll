#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Combat/CombatBattle.h"
#include "Party/BuildCatalog.h"
#include "Presentation/ExpeditionPlayback.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTargetingFeedbackTest, "Project.Expedition.Presentation.TargetingFacts",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FTargetingFeedbackTest::RunTest(const FString& Parameters)
{
    FString Error;
    int32 Covered = 0;
    for (const auto& Build : FBuildCatalog::All())
    {
        FCombatEncounter E;
        E.Width = E.Height = 8;
        auto Hero = Build.Profile;
        Hero.Id = 10; Hero.Name = TEXT("Actor"); Hero.Cell = {1,1}; Hero.InitiativeBonus = 30;
        Hero.HP = FMath::Max(1, Hero.MaxHP-1); Hero.Potions = 1;
        E.Combatants.Add(Hero);
        auto Ally = Hero; Ally.Id = 11; Ally.Name = TEXT("Ally"); Ally.Cell = {1,2}; Ally.InitiativeBonus = -10;
        E.Combatants.Add(Ally);
        FCombatantState Enemy;
        Enemy.Id = 20; Enemy.Name = TEXT("Target"); Enemy.Role = TEXT("Raider"); Enemy.Side = ECombatSide::Enemy; Enemy.Cell = {2,1}; Enemy.InitiativeBonus = -10;
        E.Combatants.Add(Enemy);
        auto NearbyEnemy = Enemy; NearbyEnemy.Id = 21; NearbyEnemy.Name = TEXT("Area target"); NearbyEnemy.Cell = {3,1};
        E.Combatants.Add(NearbyEnemy);
        FCombatBattle B;
        if (!TestTrue(Build.Id + TEXT(" starts"), B.Start(E, 42, Error))) { continue; }
        auto Actions = Build.Specials;
        Actions.Append({ECombatCommandType::Attack, ECombatCommandType::Potion, ECombatCommandType::Rescue});
        for (const auto Action : Actions)
        {
            if (!TestTrue(TEXT("Each action starts with an independent budget"), B.Start(E,42,Error))) { continue; }
            if (Hero.RageUses > 0 && Action == ECombatCommandType::Special2)
            {
                TestTrue(TEXT("Signature enters Rage"), B.Submit({ECombatCommandType::Special1,10,FIntPoint::ZeroValue,10}).bAccepted);
                // Frenzy needs the next own turn's Bonus Action; use real turn transitions.
                TestTrue(TEXT("Raging turn ends"), B.Submit({ECombatCommandType::EndTurn,10}).bAccepted);
                for (int32 Turn=0; Turn<8 && B.ActiveId()!=10; ++Turn)
                { TestTrue(TEXT("Other turn ends"), B.Submit({ECombatCommandType::EndTurn,B.ActiveId()}).bAccepted); }
                TestEqual(TEXT("Raging hero acts again"),B.ActiveId(),10);
            }
            const auto Before = B.Snapshot();
            const auto Reach = B.ActionReach(10, Action);
            TestFalse(TEXT("Supported action has geometry even without a legal target"), Reach.Visible.IsEmpty());
            TestTrue(TEXT("Wrong actor has no reach"), B.ActionReach(11, Action).Visible.IsEmpty());
            if (Build.Specials.Contains(Action))
            { TestFalse(Build.Id + TEXT(" authored action is actually exercised"),B.LegalTargets(10,Action).IsEmpty()); }
            for (int32 Id : B.LegalTargets(10, Action))
            {
                const auto F = B.Forecast(10, Action, Id);
                TestTrue(TEXT("Every eligible target lies in visible geometry"), Reach.Visible.Contains(B.Find(Id)->Cell));
                TestTrue(TEXT("Legal target forecast is valid"), F.bValid);
                TestFalse(TEXT("Legal target has a presentation chip"), ExpeditionStyle::TargetChip(B,10,Action,Id).IsEmpty());
                FCombatBattle Candidate = B;
                const auto R = Candidate.Submit({Action,10,FIntPoint::ZeroValue,Id});
                TestTrue(TEXT("Previewed target commits"), R.bAccepted);
                if (F.bHealing) { TestTrue(TEXT("Committed heal lies in forecast"), R.Healing >= F.MinAmount && R.Healing <= F.MaxAmount); }
                if (!F.Effects.IsEmpty())
                {
                    TestEqual(TEXT("Area preview effect count"), R.Effects.Num(), F.Effects.Num());
                    for (int32 I=0; I<FMath::Min(R.Effects.Num(),F.Effects.Num()); ++I)
                    {
                        TestEqual(TEXT("Ordered victim"),R.Effects[I].TargetId,F.Effects[I].TargetId);
                        TestEqual(TEXT("Ordered amount"),R.Effects[I].Amount,F.Effects[I].Amount);
                    }
                }
            }
            B.ActionReach(10,Action); B.Forecast(10,Action,20);
            TestTrue(TEXT("All queries preserve full reflected state and RNG"), FCombatSnapshot::StaticStruct()->CompareScriptStruct(&Before,&B.Snapshot(),0));
        }
        auto Spent = B.Snapshot(); Spent.bActionAvailable = false;
        TestTrue(TEXT("Spent state restores"),B.Restore(Spent,Error));
        TestFalse(TEXT("Geometry survives spent budgets"),B.ActionReach(10,ECombatCommandType::Attack).Visible.IsEmpty());
        TestTrue(TEXT("Spent attack has no selectable targets"),B.LegalTargets(10).IsEmpty());
        ++Covered;
    }
    TestEqual(TEXT("Entire compact catalog covered"),Covered,70);
    FCombatEncounter E;
    FCombatantState A; A.Id=10; A.Cell={0,0}; A.Range=4; A.InitiativeBonus=30;
    FCombatantState T; T.Id=20; T.Side=ECombatSide::Enemy; T.Cell={2,0}; T.InitiativeBonus=-10;
    E.Combatants={A,T}; E.BlockedCells={{1,0}};
    FCombatBattle B; TestTrue(TEXT("Blocked fixture starts"),B.Start(E,42,Error));
    const auto Reach=B.ActionReach(10,ECombatCommandType::Attack);
    TestTrue(TEXT("Occluded target is marked obstructed"),Reach.Obstructed.Contains(T.Cell));
    TestFalse(TEXT("Occluded target cannot be selected"),B.LegalTargets(10).Contains(20));
    TestFalse(TEXT("Beyond range excluded"),Reach.Visible.Contains(FIntPoint(7,7)));
    E.BlockedCells.Reset(); E.Combatants[0].Potions=0;
    TestTrue(TEXT("Resource fixture starts"),B.Start(E,42,Error));
    TestTrue(TEXT("No bottle means no potion target"),B.LegalTargets(10,ECombatCommandType::Potion).IsEmpty());
    E.Combatants[0].Potions=1;
    E.Combatants[1].Side=ECombatSide::Party; E.Combatants[1].Cell={1,0};
    FCombatantState Foe=T; Foe.Id=30; Foe.Cell={7,7}; E.Combatants.Add(Foe);
    TestTrue(TEXT("Friendly fixture starts"),B.Start(E,42,Error));
    TestFalse(TEXT("Attack excludes ally"),B.LegalTargets(10).Contains(20));
    TestTrue(TEXT("Full health potion is ineligible"),B.LegalTargets(10,ECombatCommandType::Potion).IsEmpty());
    E.Combatants[1].Life=ECombatLife::Downed; E.Combatants[1].HP=0; E.Combatants[1].RescueTurnsLeft=2;
    TestTrue(TEXT("Downed fixture starts"),B.Start(E,42,Error));
    TestTrue(TEXT("Rescue includes downed ally"),B.LegalTargets(10,ECombatCommandType::Rescue).Contains(20));
    TestTrue(TEXT("Potion includes downed ally"),B.LegalTargets(10,ECombatCommandType::Potion).Contains(20));
    E.Combatants[1].Life=ECombatLife::Dead; E.Combatants[1].RescueTurnsLeft=0;
    TestTrue(TEXT("Dead fixture starts"),B.Start(E,42,Error));
    TestFalse(TEXT("Rescue excludes dead ally"),B.LegalTargets(10,ECombatCommandType::Rescue).Contains(20));
    TestFalse(TEXT("Potion excludes dead ally"),B.LegalTargets(10,ECombatCommandType::Potion).Contains(20));
    return !HasAnyErrors();
}
namespace
{
FCombatEncounter ChipEncounter(const TCHAR* BuildId)
{
    FCombatEncounter E;
    FCombatantState Hero;
    FBuildCatalog::Create(BuildId, 10, TEXT("Actor"), Hero);
    Hero.Cell = {1,1}; Hero.InitiativeBonus = 30; Hero.HP = 10; Hero.Potions = 1;
    FCombatantState Ally;
    Ally.Id = 11; Ally.Name = Ally.Role = TEXT("Ally"); Ally.Cell = {1,2}; Ally.HP = 5; Ally.InitiativeBonus = -10;
    FCombatantState Enemy;
    Enemy.Id = 20; Enemy.Name = Enemy.Role = TEXT("Target"); Enemy.Side = ECombatSide::Enemy;
    Enemy.Cell = {2,1}; Enemy.HP = Enemy.MaxHP = 40; Enemy.InitiativeBonus = -10;
    auto Other = Enemy; Other.Id = 21; Other.Name = TEXT("Other"); Other.Cell = {6,6};
    E.Combatants = {Hero, Ally, Enemy, Other};
    return E;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTargetChipSemanticsTest, "Project.Expedition.Presentation.TargetChipSemantics",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FTargetChipSemanticsTest::RunTest(const FString& Parameters)
{
    using C = ECombatCommandType;
    struct FCase { const TCHAR* Build; C Action; int32 Target; const TCHAR* Label; };
    const FCase Cases[] = {
        {TEXT("barbarian.berserker"), C::Special1, 10, TEXT("Rage: +2 basic damage, -1 taken")},
        {TEXT("artificer.artillerist"), C::Special2, 11, TEXT("Protect -2 / next turn")},
        {TEXT("rogue.thief"), C::Special1, 11, TEXT("Swap positions")},
        {TEXT("artificer.artillerist"), C::Special1, 20, TEXT("Auto | 5 + push if open")},
        {TEXT("sorcerer.wild-magic"), C::Special1, 20, TEXT("Auto | 4 or 8")},
        {TEXT("cleric.life"), C::CureWounds, 11, TEXT("Heal +5-15")},
        {TEXT("wizard.evocation"), C::MagicMissile, 20, TEXT("Auto | 6-15")},
        {TEXT("fighter.champion"), C::Rescue, 11, TEXT("Heal +1")},
    };
    FString Error;
    for (int32 Index = 0; Index < static_cast<int32>(UE_ARRAY_COUNT(Cases)); ++Index)
    {
        const auto& Case = Cases[Index];
        auto E = ChipEncounter(Case.Build);
        if (Case.Action == C::Rescue)
        { E.Combatants[1].Life = ECombatLife::Downed; E.Combatants[1].HP = 0; E.Combatants[1].RescueTurnsLeft = 2; }
        FCombatBattle B;
        if (!TestTrue(FString(Case.Build) + TEXT(" semantic fixture starts"), B.Start(E,42,Error))) { continue; }
        const auto Before = B.Snapshot();
        TestTrue(TEXT("Worked target selectable"), B.LegalTargets(10,Case.Action).Contains(Case.Target));
        TestTrue(TEXT("Worked target has forecast"), B.Forecast(10,Case.Action,Case.Target).bValid);
        TestEqual(TEXT("Literal truthful chip"), ExpeditionStyle::TargetChip(B,10,Case.Action,Case.Target), FString(Case.Label));
        TestTrue(TEXT("Formatting preserves full state and RNG"), FCombatSnapshot::StaticStruct()->CompareScriptStruct(&Before,&B.Snapshot(),0));
        if (!TestTrue(TEXT("Worked target commits"), B.Submit({Case.Action,10,{},Case.Target}).bAccepted)) { continue; }
        switch (Index)
        {
        case 0:
            TestEqual(TEXT("Rage lasts two own turn ends"), B.Find(10)->RageTurns, 2);
            TestEqual(TEXT("Rage spends one use"), B.Find(10)->RageUses, 1);
            break;
        case 1:
            TestEqual(TEXT("Guard belongs to caster"), B.Find(10)->GuardTargetId, 11);
            TestEqual(TEXT("Guard reduces damage by two"), B.Find(10)->GuardAmount, 2);
            TestEqual(TEXT("Guard lasts to next own turn"), B.Find(10)->GuardTurns, 1);
            break;
        case 2:
            TestEqual(TEXT("Swap moves caster to ally"), B.Find(10)->Cell, FIntPoint(1,2));
            TestEqual(TEXT("Swap moves ally to caster"), B.Find(11)->Cell, FIntPoint(1,1));
            TestEqual(TEXT("Swap causes no HP loss"), B.Find(11)->HP, 5);
            break;
        case 3:
            TestEqual(TEXT("Cannon deals five damage"), B.Find(20)->HP, 35);
            TestEqual(TEXT("Surviving target moves one open cell"), B.Find(20)->Cell, FIntPoint(3,1));
            break;
        case 4:
            TestTrue(TEXT("Surge commits one discrete endpoint"), B.Find(20)->HP == 36 || B.Find(20)->HP == 32);
            TestEqual(TEXT("Surge outcome explains endpoint"), 40-B.Find(20)->HP, B.Find(10)->LastSurge*4);
            break;
        case 5:
            TestTrue(TEXT("Cure commits bounded healing"), B.Find(11)->HP >= 10 && B.Find(11)->HP <= 20);
            TestEqual(TEXT("Cure spends Slot"), B.Find(10)->SpellSlots, 1);
            TestEqual(TEXT("Cure spends Kit"), B.Find(10)->KitUses, 1);
            break;
        case 6:
            TestTrue(TEXT("Missile commits automatic damage"), B.Find(20)->HP >= 25 && B.Find(20)->HP <= 34);
            break;
        case 7:
            TestEqual(TEXT("Rescue restores one HP"), B.Find(11)->HP, 1);
            TestTrue(TEXT("Rescue restores consciousness"), B.Find(11)->Life == ECombatLife::Conscious);
            TestEqual(TEXT("Rescue clears deadline"), B.Find(11)->RescueTurnsLeft, 0);
            break;
        }
    }
    // Literal ordinary/critical-only/impossible drop thresholds for a 1d6+3 attack.
    const TCHAR* Drops[] = {TEXT("70% | 4-9 | can drop"), TEXT("70% | 4-9 | crit drop"), TEXT("70% | 4-9")};
    const int32 HitPoints[] = {8,10,16};
    for (int32 I=0; I<3; ++I)
    {
        auto E = ChipEncounter(TEXT("fighter.champion")); E.Combatants[2].HP = HitPoints[I];
        FCombatBattle B;
        if (TestTrue(TEXT("Drop fixture starts"),B.Start(E,42,Error)))
        { TestEqual(TEXT("Literal drop qualifier"),ExpeditionStyle::TargetChip(B,10,C::Attack,20),FString(Drops[I])); }
    }
    // An open cell is insufficient if the hit kills; a blocked survivor stays put.
    for (int32 Mode=0; Mode<3; ++Mode)
    {
        auto E = ChipEncounter(TEXT("artificer.artillerist"));
        E.Combatants[2].HP = Mode == 0 ? 3 : 40;
        if (Mode == 2) { E.BlockedCells.Add(FIntPoint(3,1)); }
        FCombatBattle B;
        if (!TestTrue(TEXT("Push fixture starts"),B.Start(E,42,Error))) { continue; }
        TestEqual(TEXT("Lethal push has no displacement promise"),ExpeditionStyle::TargetChip(B,10,C::Special1,20),
            FString(Mode == 0 ? TEXT("Auto | 3 | can drop") : TEXT("Auto | 5 + push if open")));
        const auto R = B.Submit({C::Special1,10,{},20});
        TestTrue(TEXT("Push commits"),R.bAccepted);
        TestEqual(TEXT("Only open surviving target is displaced"),B.Find(20)->Cell,Mode == 1 ? FIntPoint(3,1) : FIntPoint(2,1));
        TestEqual(TEXT("Displacement fact agrees"),R.Effects.ContainsByPredicate([](const auto& Effect) { return Effect.Effect == EAbilityEffect::Push; }),Mode == 1);
        TestEqual(TEXT("Lethal target is dead"),B.Find(20)->Life == ECombatLife::Dead,Mode == 0);
    }
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTargetChipAvailabilityTest, "Project.Expedition.Presentation.TargetChipAvailability",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FTargetChipAvailabilityTest::RunTest(const FString& Parameters)
{
    using C = ECombatCommandType;
    FString Error;
    // Catalog Slot users require Slots >= Kit. Use coherent exhausted states,
    // plus a standalone legacy wizard to isolate a missing Slot from missing Kit.
    const TCHAR* Builds[] = {TEXT("rogue.thief"), TEXT("wizard.evocation"), TEXT("cleric.life"),
        TEXT("barbarian.berserker"), TEXT("fighter.champion"), TEXT("fighter.battle-master"), TEXT("fighter.champion"), TEXT("wizard.evocation")};
    const C Actions[] = {C::Special1,C::MagicMissile,C::CureWounds,C::Special1,C::SecondWind,C::PrecisionStrike,C::Potion,C::MagicMissile};
    const int32 Targets[] = {11,20,11,10,10,20,11,20};
    for (int32 I=0; I<8; ++I)
    {
        auto E = ChipEncounter(Builds[I]); auto& Hero = E.Combatants[0];
        if (I<=2) { Hero.KitUses=0; }
        if (I==1 || I==2) { Hero.SpellSlots=0; }
        if (I==3) { Hero.RageUses=0; }
        if (I==4) { Hero.SecondWindUses=0; }
        if (I==5) { Hero.ManeuverUses=0; }
        if (I==6) { Hero.Potions=0; }
        if (I==7) { Hero.BuildId.Reset(); Hero.KitUses=0; Hero.SpellSlots=0; }
        FCombatBattle B;
        if (!TestTrue(TEXT("Exhausted fixture is valid"),B.Start(E,42,Error))) { continue; }
        const auto Before = B.Snapshot();
        TestFalse(TEXT("Resource exhaustion retains geometric reach"),B.ActionReach(10,Actions[I]).Visible.IsEmpty());
        TestTrue(TEXT("Resource exhaustion removes targets"),B.LegalTargets(10,Actions[I]).IsEmpty());
        TestFalse(TEXT("Exhausted action has no forecast"),B.Forecast(10,Actions[I],Targets[I]).bValid);
        TestTrue(TEXT("Exhausted action has no chip"),ExpeditionStyle::TargetChip(B,10,Actions[I],Targets[I]).IsEmpty());
        TestFalse(TEXT("Exhausted action rejects commit"),B.Submit({Actions[I],10,{},Targets[I]}).bAccepted);
        TestTrue(TEXT("Exhausted queries/rejection preserve all state"),FCombatSnapshot::StaticStruct()->CompareScriptStruct(&Before,&B.Snapshot(),0));
    }
    for (bool Bonus : {false,true})
    {
        auto E = ChipEncounter(Bonus ? TEXT("rogue.thief") : TEXT("artificer.artillerist"));
        FCombatBattle B;
        if (!TestTrue(TEXT("Budget fixture starts"),B.Start(E,42,Error))) { continue; }
        const int32 Target = Bonus ? 11 : 20;
        TestTrue(TEXT("Spend actual budget through command"),B.Submit({C::Special1,10,{},Target}).bAccepted);
        TestEqual(TEXT("One Kit use remains"),B.Find(10)->KitUses,1);
        TestEqual(TEXT("Action budget preserved only for Bonus ability"),B.Snapshot().bActionAvailable,Bonus);
        TestEqual(TEXT("Bonus budget preserved only for Action ability"),B.Snapshot().bBonusActionAvailable,!Bonus);
        const auto Before = B.Snapshot();
        TestFalse(TEXT("Spent special retains reach"),B.ActionReach(10,C::Special1).Visible.IsEmpty());
        TestTrue(TEXT("Spent special has no targets"),B.LegalTargets(10,C::Special1).IsEmpty());
        TestTrue(TEXT("Spent special has no chip"),ExpeditionStyle::TargetChip(B,10,C::Special1,Target).IsEmpty());
        TestFalse(TEXT("Spent special rejects command"),B.Submit({C::Special1,10,{},Target}).bAccepted);
        TestTrue(TEXT("Budget rejection preserves all state"),FCombatSnapshot::StaticStruct()->CompareScriptStruct(&Before,&B.Snapshot(),0));
    }
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTargetChipLabelTest, "Project.Expedition.Presentation.TargetChipLabel",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FTargetChipLabelTest::RunTest(const FString& Parameters)
{
    // Seven units per character: the label limit is a character budget, so cases are exact.
    auto Measure = [](const FString& Value) { return Value.Len() * 7.0f; };
    const FString Detail = TEXT("70% | 4-9 | can drop");
    TestEqual(TEXT("A short name leads the forecast"), ExpeditionStyle::ChipLabel(TEXT("Raider 2"), Detail, 274, Measure), FString(TEXT("Raider 2: 70% | 4-9 | can drop")));
    TestEqual(TEXT("No name leaves the forecast alone"), ExpeditionStyle::ChipLabel(FString(), Detail, 274, Measure), Detail);
    for (const FString& Name : {FString(TEXT("Sir Bartholomew-Alexander of the Marsh Lantern Guild")), FString(TEXT("W")), FString(TEXT("An exceptionally and needlessly long hero name"))})
    {
        for (const float Limit : {274.0f, 220.0f, 180.0f})
        {
            const FString Label = ExpeditionStyle::ChipLabel(Name, Detail, Limit, Measure);
            TestTrue(Name + TEXT(": the label fits its limit"), Measure(Label) <= Limit || Label == Detail);
            TestTrue(Name + TEXT(": the whole qualifier stays visible"), Label.EndsWith(Detail));
        }
    }
    const FString Cut = ExpeditionStyle::ChipLabel(TEXT("An exceptionally and needlessly long hero name"), Detail, 274, Measure);
    TestTrue(TEXT("A long name is cut with an ellipsis, not the forecast"), Cut.Contains(TEXT("...: ")) && Cut.StartsWith(TEXT("An exc")));
    TestEqual(TEXT("When the forecast alone fills the budget the name is dropped"), ExpeditionStyle::ChipLabel(TEXT("Raider"), Detail, 140, Measure), Detail);

    // The Rage chip says what the catalog says: the bonus is for basic attacks only.
    bool bRageDescribed = false;
    for (const auto& Build : FBuildCatalog::All())
    { for (const auto& Ability : Build.Abilities) { if (Ability.Name == TEXT("Rage")) { bRageDescribed = true; TestTrue(TEXT("The catalog still limits Rage's +2 to basic damage"), Ability.Description.Contains(TEXT("+2 basic damage"))); } } }
    TestTrue(TEXT("A Rage ability exists to compare against"), bRageDescribed);
    return !HasAnyErrors();
}
#endif
