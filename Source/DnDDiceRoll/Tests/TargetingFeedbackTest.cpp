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
        Hero.Id = 10; Hero.Cell = {1,1}; Hero.InitiativeBonus = 30;
        Hero.HP = FMath::Max(1, Hero.MaxHP-1); Hero.Potions = 1;
        E.Combatants.Add(Hero);
        auto Ally = Hero; Ally.Id = 11; Ally.Cell = {1,2}; Ally.InitiativeBonus = -10;
        E.Combatants.Add(Ally);
        FCombatantState Enemy;
        Enemy.Id = 20; Enemy.Side = ECombatSide::Enemy; Enemy.Cell = {2,1}; Enemy.InitiativeBonus = -10;
        E.Combatants.Add(Enemy);
        auto NearbyEnemy = Enemy; NearbyEnemy.Id = 21; NearbyEnemy.Cell = {3,1};
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
    TestFalse(TEXT("Beyond range excluded"),Reach.Visible.Contains({7,7}));
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
#endif
