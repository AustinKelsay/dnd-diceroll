#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Combat/ActionFacts.h"
#include "Combat/CombatBattle.h"
#include "Party/BuildCatalog.h"
#include "Presentation/ActionText.h"

/**
 * The words for action facts (#58): every enumerator has text, exact phrases for the fixed cases, and
 * every action of every kit reads sensibly from facts alone. The HUD does not use these yet.
 */
namespace ActionTextTest
{
using EC = ECombatCommandType;

FCombatBattle Start(const FBuildDefinition& Build)
{
    FCombatEncounter E;
    E.Width = E.Height = 8;
    FCombatantState Hero = Build.Profile;
    Hero.Id = 10; Hero.Name = TEXT("Actor"); Hero.Cell = {1, 1}; Hero.InitiativeBonus = 30;
    Hero.HP = FMath::Max(1, Hero.MaxHP - 1); Hero.Potions = 1;
    FCombatantState Foe;
    Foe.Id = 20; Foe.Name = TEXT("Target"); Foe.Role = TEXT("Raider"); Foe.Side = ECombatSide::Enemy; Foe.Cell = {2, 1}; Foe.InitiativeBonus = -10;
    E.Combatants = {Hero, Foe};
    FCombatBattle Battle;
    FString Error;
    Battle.Start(E, 42, Error);
    return Battle;
}

FActionResourceCost Cost(EActionResource Resource, int32 Amount, int32 Held, int32 Cap, int32 Recovery)
{
    FActionResourceCost C;
    C.Resource = Resource; C.Amount = Amount; C.Held = Held; C.Cap = Cap; C.CampRecovery = Recovery;
    return C;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FActionTextEveryEnumeratorTest, "Project.Expedition.Combat.ActionTextEveryEnumerator",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FActionTextEveryEnumeratorTest::RunTest(const FString& Parameters)
{
    using namespace ActionTextTest;
    for (uint8 I = 0; I <= static_cast<uint8>(EActionCostKind::None); ++I)
    { TestFalse(FString::Printf(TEXT("cost kind %d has a name"), I), ExpeditionStyle::CostName(static_cast<EActionCostKind>(I)).IsEmpty()); }
    for (uint8 I = 0; I <= static_cast<uint8>(EActionResource::Potion); ++I)
    { TestFalse(FString::Printf(TEXT("resource %d has a name"), I), ExpeditionStyle::ResourceName(static_cast<EActionResource>(I)).IsEmpty()); }
    TSet<FString> Seen;
    for (uint8 I = 0; I < static_cast<uint8>(EActionBlocker::Count); ++I)
    {
        FActionBlocker B;
        B.Reason = static_cast<EActionBlocker>(I);
        B.Resource = EActionResource::Slot; B.Needed = 1; B.Range = 3;
        const FString Text = ExpeditionStyle::BlockerText(B);
        TestFalse(FString::Printf(TEXT("blocker %d has text"), I), Text.IsEmpty());
        TestFalse(FString::Printf(TEXT("blocker %d text is its own"), I), Seen.Contains(Text));
        Seen.Add(Text);
    }
    // Every command a bar can hold has a headline naming it.
    for (const EC Command : {EC::Move, EC::Attack, EC::EndTurn, EC::SecondWind, EC::CureWounds, EC::MagicMissile, EC::Rescue, EC::Potion, EC::PrecisionStrike})
    {
        FActionFacts F;
        F.Command = Command;
        TestFalse(FString::Printf(TEXT("command %d has a headline"), static_cast<int32>(Command)), ExpeditionStyle::TooltipHeadline(F).IsEmpty());
    }
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FActionTextPhrasesTest, "Project.Expedition.Combat.ActionTextPhrases",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FActionTextPhrasesTest::RunTest(const FString& Parameters)
{
    using namespace ActionTextTest;
    using namespace ExpeditionStyle;
    TestEqual(TEXT("Cost names"), CostName(EActionCostKind::Movement) + TEXT("/") + CostName(EActionCostKind::Action) + TEXT("/") + CostName(EActionCostKind::BonusAction),
        FString(TEXT("Move/Action/Bonus action")));

    FActionFacts Attack;
    Attack.Command = EC::Attack; Attack.Cost = EActionCostKind::Action;
    TestEqual(TEXT("Attack cost pill"), CostPillText(Attack), FString(TEXT("Action")));
    TestEqual(TEXT("Attack is at will"), UsagePillText(Attack), FString(TEXT("At will")));
    TestEqual(TEXT("Attack headline"), TooltipHeadline(Attack), FString(TEXT("Attack · Action")));

    FActionFacts Move;
    Move.Command = EC::Move; Move.Cost = EActionCostKind::Movement;
    TestEqual(TEXT("Move cost pill"), CostPillText(Move), FString(TEXT("Move")));
    TestEqual(TEXT("Move is at will"), UsagePillText(Move), FString(TEXT("At will")));

    FActionFacts End;
    End.Command = EC::EndTurn; End.Cost = EActionCostKind::None;
    TestTrue(TEXT("End Turn has no cost pill"), CostPillText(End).IsEmpty());
    TestTrue(TEXT("End Turn has no usage pill"), UsagePillText(End).IsEmpty());
    TestEqual(TEXT("End Turn headline carries no cost"), TooltipHeadline(End), FString(TEXT("End Turn")));

    FActionFacts Special;
    Special.Command = EC::Special1; Special.Name = TEXT("Flurry"); Special.Cost = EActionCostKind::BonusAction;
    Special.Resources = {Cost(EActionResource::Kit, 1, 2, 3, 1)};
    TestEqual(TEXT("Bonus cost pill"), CostPillText(Special), FString(TEXT("Bonus")));
    TestEqual(TEXT("A named resource shows cost, held and cap"), UsagePillText(Special), FString(TEXT("1 Kit · 2/3")));
    TestEqual(TEXT("A catalog ability is headlined by its own name"), TooltipHeadline(Special), FString(TEXT("Flurry · Bonus action")));

    FActionFacts Missile;
    Missile.Command = EC::MagicMissile; Missile.Cost = EActionCostKind::Action;
    Missile.Resources = {Cost(EActionResource::Kit, 1, 1, 1, 1), Cost(EActionResource::Slot, 1, 0, 2, 2)};
    TestEqual(TEXT("Two resources are both shown"), UsagePillText(Missile), FString(TEXT("1 Kit · 1/1 + 1 Slot · 0/2")));
    TestEqual(TEXT("Named spell headline"), TooltipHeadline(Missile), FString(TEXT("Magic Missile · Action")));

    FActionFacts Unknown;
    Unknown.Cost = EActionCostKind::Action;
    Unknown.Resources = {Cost(EActionResource::Kit, 1, 2, INDEX_NONE, INDEX_NONE)};
    TestEqual(TEXT("Without a known cap only the count is shown"), UsagePillText(Unknown), FString(TEXT("1 Kit · 2")));

    FActionFacts Stance;
    Stance.Cost = EActionCostKind::Action; Stance.bRequiresRage = true;
    TestEqual(TEXT("A stance requirement"), UsagePillText(Stance), FString(TEXT("Needs Rage")));
    Stance.bRequiresRage = false; Stance.bForbidsRage = true;
    Stance.Resources = {Cost(EActionResource::Rage, 1, 2, 3, 1)};
    TestEqual(TEXT("A stance block beside its resource"), UsagePillText(Stance), FString(TEXT("1 Rage · 2/3 | Not while raging")));

    TestEqual(TEXT("Camp restores some"), CampRecoveryText(Cost(EActionResource::Slot, 1, 0, 3, 1)), FString(TEXT("Camp restores 1")));
    TestEqual(TEXT("Camp restores all"), CampRecoveryText(Cost(EActionResource::Slot, 1, 0, 3, 3)), FString(TEXT("Camp restores all")));
    TestEqual(TEXT("Potions are not refilled"), CampRecoveryText(Cost(EActionResource::Potion, 1, 1, INDEX_NONE, 0)), FString(TEXT("Not refilled by camp")));
    TestTrue(TEXT("An unknown recovery says nothing"), CampRecoveryText(Cost(EActionResource::Kit, 1, 1, INDEX_NONE, INDEX_NONE)).IsEmpty());

    FActionBlocker Short; Short.Reason = EActionBlocker::ResourceShort; Short.Resource = EActionResource::Slot; Short.Held = 0; Short.Needed = 1;
    TestEqual(TEXT("A short resource names held and needed"), BlockerText(Short), FString(TEXT("Need 1 Slot, have 0")));
    FActionBlocker Far; Far.Reason = EActionBlocker::OutOfRange; Far.Range = 6;
    TestEqual(TEXT("Out of range names the range"), BlockerText(Far), FString(TEXT("Out of range (6)")));
    FActionBlocker Spent; Spent.Reason = EActionBlocker::ActionSpent;
    TestEqual(TEXT("Action spent"), BlockerText(Spent), FString(TEXT("Action already used")));
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FActionTextEveryKitTest, "Project.Expedition.Combat.ActionTextEveryKit",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FActionTextEveryKitTest::RunTest(const FString& Parameters)
{
    using namespace ActionTextTest;
    using namespace ExpeditionStyle;
    int32 Pills = 0;
    for (const FBuildDefinition& Build : FBuildCatalog::All())
    {
        const FCombatBattle Battle = Start(Build);
        for (const EC Command : FActionFactsQuery::BarFor(Battle, 10))
        {
            const FActionFacts F = FActionFactsQuery::For(Battle, 10, Command);
            const FString Where = FString::Printf(TEXT("%s / command %d"), *Build.Id, static_cast<int32>(Command));
            if (F.Cost == EActionCostKind::None)
            {
                TestTrue(Where + TEXT(": a free command has no pills"), CostPillText(F).IsEmpty() && UsagePillText(F).IsEmpty());
                continue;
            }
            ++Pills;
            TestFalse(Where + TEXT(": has a cost pill"), CostPillText(F).IsEmpty());
            TestFalse(Where + TEXT(": has a usage pill"), UsagePillText(F).IsEmpty());
            TestFalse(Where + TEXT(": has a headline"), TooltipHeadline(F).IsEmpty());
            for (const FActionResourceCost& R : F.Resources)
            { TestTrue(Where + TEXT(": the usage pill names ") + ResourceName(R.Resource), UsagePillText(F).Contains(ResourceName(R.Resource))); }
            for (const FActionBlocker& B : F.Blockers) { TestFalse(Where + TEXT(": a blocker has text"), BlockerText(B).IsEmpty()); }
        }
    }
    TestTrue(TEXT("Pills were produced for every kit"), Pills > 70 * 4);
    return !HasAnyErrors();
}
#endif
