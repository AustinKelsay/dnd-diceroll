#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Combat/CombatBattle.h"
#include "Party/BuildCatalog.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FActionEconomyTagTest, "Project.Expedition.Rules.ActionEconomyTags",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FActionEconomyTagTest::RunTest(const FString& Parameters)
{
    FCombatantState Rogue; FBuildCatalog::Create(TEXT("rogue.thief"), 1, TEXT("R"), Rogue); Rogue.Level = 2;
    // Every described command's cost field agrees with its data tag.
    for (uint8 Raw = 0; Raw <= static_cast<uint8>(ECombatCommandType::OffHandAttack); ++Raw)
    {
        const auto Action = static_cast<ECombatCommandType>(Raw);
        if (Action == ECombatCommandType::Move || Action == ECombatCommandType::EndTurn || Action == ECombatCommandType::Attack ||
            Action == ECombatCommandType::Special1 || Action == ECombatCommandType::Special2) { continue; }
        const FString Field = FCombatBattle::CostTag(Action);
        const EActionCost Cost = FCombatBattle::ActionCost(Action, Rogue);
        AddInfo(Field);
        TestTrue(TEXT("Description cost matches data tag"),
            (Cost == EActionCost::Action && Field.StartsWith(TEXT("Action"))) || (Cost == EActionCost::BonusAction && Field.StartsWith(TEXT("Bonus"))) ||
            (Cost == EActionCost::Free && Field.StartsWith(TEXT("Free"))) || (Cost == EActionCost::Reaction && Field.StartsWith(TEXT("Reaction"))));
    }
    TestTrue(TEXT("Movement is free"), FCombatBattle::ActionCost(ECombatCommandType::Move, Rogue) == EActionCost::Free);
    TestTrue(TEXT("Second Wind is a bonus action"), FCombatBattle::ActionCost(ECombatCommandType::SecondWind, Rogue) == EActionCost::BonusAction);
    TestTrue(TEXT("Cunning Action: Dash after the action is a bonus action"), FCombatBattle::ActionCost(ECombatCommandType::Dash, Rogue, false) == EActionCost::BonusAction);
    TestEqual(TEXT("Tooltip prefix"), FCombatBattle::CostPrefix(ECombatCommandType::OffHandAttack, Rogue), FString(TEXT("[Bonus Action] ")));
    TestEqual(TEXT("Reaction label"), FString(FStandardActionRules::CostLabel(EActionCost::Reaction)), FString(TEXT("Reaction")));
    for (const auto* Id : {TEXT("barbarian.berserker"), TEXT("monk.open-hand"), TEXT("cleric.life")})
    {
        FCombatantState Hero; if (!FBuildCatalog::Create(Id, 1, TEXT("H"), Hero)) { continue; }
        for (const auto& A : FBuildCatalog::Abilities(Hero))
        { TestTrue(TEXT("Kit ability tag agrees with its budget"), FCombatBattle::ActionCost(A.Command, Hero) == A.Cost()); }
    }
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShieldReactionTest, "Project.Expedition.Rules.ShieldReaction",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FShieldReactionTest::RunTest(const FString& Parameters)
{
    FCombatantState Wiz; FBuildCatalog::Create(TEXT("wizard.evocation"), 1, TEXT("W"), Wiz);
    TestFalse(TEXT("Slots reserved by kit uses are not spent on Shield"), FCombatBattle::ShieldWouldBlock(Wiz, 15, 12, false) && Wiz.SpellSlots <= Wiz.KitUses);
    Wiz.KitUses = 0;
    TestTrue(TEXT("A hit within 5 of AC is shielded"), FCombatBattle::ShieldWouldBlock(Wiz, 15, 12, false));
    TestFalse(TEXT("A hit at AC + 5 still lands"), FCombatBattle::ShieldWouldBlock(Wiz, 17, 12, false));
    TestFalse(TEXT("Critical hits ignore Shield"), FCombatBattle::ShieldWouldBlock(Wiz, 13, 12, true));
    Wiz.bReactionAvailable = false; TestFalse(TEXT("Needs the reaction"), FCombatBattle::ShieldWouldBlock(Wiz, 13, 12, false));
    Wiz.bReactionAvailable = true; Wiz.SpellSlots = 0; TestFalse(TEXT("Needs a slot"), FCombatBattle::ShieldWouldBlock(Wiz, 13, 12, false));
    FCombatantState Fighter; FBuildCatalog::Create(TEXT("fighter.champion"), 2, TEXT("F"), Fighter);
    TestFalse(TEXT("Only wizards know Shield"), FCombatBattle::ShieldWouldBlock(Fighter, 13, 12, false));
    return true;
}
#endif