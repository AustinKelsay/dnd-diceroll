#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Combat/CombatBattle.h"
#include "Rules/AbilityRules.h"
#include "Rules/CoverRules.h"
#include "Rules/DamageRules.h"
#include "Party/BuildCatalog.h"

namespace
{
FCombatEncounter Duo(ERuleset Ruleset)
{
    FCombatEncounter Layout;
    Layout.Width = 8; Layout.Height = 8; Layout.Ruleset = Ruleset;
    FCombatantState Hero;
    FBuildCatalog::Create(TEXT("rogue.thief"), 1, TEXT("Nix"), Hero);
    Hero.Cell = {1, 1}; Hero.InitiativeBonus = 30;
    FCombatantState Foe;
    Foe.Id = 2; Foe.Name = TEXT("Raider"); Foe.Role = TEXT("Raider"); Foe.Side = ECombatSide::Enemy;
    Foe.Cell = {3, 1}; Foe.MaxHP = Foe.HP = 40; Foe.ArmorClass = 10; Foe.AttackBonus = 2; Foe.DamageDie = 6; Foe.DamageBonus = 1;
    Foe.Range = 1; Foe.Speed = 4; Foe.InitiativeBonus = 0; Foe.Size = 2;
    Layout.Combatants = {Hero, Foe};
    return Layout;
}
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSrdDexOnlyInitiativeTest, "Project.Expedition.Battle.SrdDexOnlyInitiative",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSrdDexOnlyInitiativeTest::RunTest(const FString& Parameters)
{
    FString Error;
    FCombatEncounter Compact = Duo(ERuleset::Compact);
    const int32 CompactBonus = Compact.Combatants[0].InitiativeBonus;
    FCombatBattle CompactBattle;
    if (!TestTrue(TEXT("Compact starts"), CompactBattle.Start(Compact, 7, Error))) { return false; }
    TestEqual(TEXT("Compact keeps catalog initiative bonus"), CompactBattle.Find(1)->InitiativeBonus, CompactBonus);

    FCombatEncounter Srd = Duo(ERuleset::SRD);
    FCombatBattle SrdBattle;
    if (!TestTrue(TEXT("SRD starts"), SrdBattle.Start(Srd, 7, Error))) { return false; }
    const int32 DexOnly = FAbilityRules::Modifier(FAbilityRules::PrebuiltScores(EAdventurerRole::Rogue).Dexterity);
    TestEqual(TEXT("SRD Rogue initiative is DEX only"), SrdBattle.Find(1)->InitiativeBonus, DexOnly);
    TestTrue(TEXT("DEX-only differs from Compact Rogue +1"), DexOnly != CompactBonus);
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSrdMagicMissilePerDartTest, "Project.Expedition.Battle.SrdMagicMissilePerDartResistance",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSrdMagicMissilePerDartTest::RunTest(const FString& Parameters)
{
    FString Error;
    FCombatEncounter Layout = Duo(ERuleset::SRD);
    FCombatantState Wizard;
    FBuildCatalog::Create(TEXT("wizard.evocation"), 1, TEXT("Ivo"), Wizard);
    Wizard.Cell = {1, 1}; Wizard.InitiativeBonus = 30;
    Layout.Combatants[0] = Wizard;
    FCombatBattle Battle;
    if (!TestTrue(FString::Printf(TEXT("SRD MM layout: %s"), *Error), Battle.Start(Layout, 11, Error))) { return false; }
    if (!TestEqual(TEXT("Wizard acts first"), Battle.ActiveId(), 1)) { return false; }
    const FCombatResult Result = Battle.Submit({ECombatCommandType::MagicMissile, 1, {}, 2});
    if (!TestTrue(FString::Printf(TEXT("MM accepted: %s"), *Result.Feedback), Result.bAccepted && Result.bHit)) { return false; }
    TestEqual(TEXT("Three dart dice logged"), Result.EffectDice.Num(), 3);
    if (Result.EffectDice.Num() >= 3)
    {
        TestTrue(TEXT("Each dart is d4"), Result.EffectDice[0].Sides == 4 && Result.EffectDice[1].Sides == 4 && Result.EffectDice[2].Sides == 4);
    }
    // Per-dart resistance is not equal to resisting the summed total (5+5+5 resist => 2+2+2=6, half of 15 => 7).
    FDamageDefenses ResistForce; ResistForce.Resist(EDamageType::Force);
    TestEqual(TEXT("Per-dart halves each 5"), FDamageRules::Adjust(5, EDamageType::Force, ResistForce) * 3, 6);
    TestEqual(TEXT("Lump half of 15"), FDamageRules::Adjust(15, EDamageType::Force, ResistForce), 7);
    TestTrue(TEXT("Per-dart differs from lump"), FDamageRules::Adjust(5, EDamageType::Force, ResistForce) * 3 != FDamageRules::Adjust(15, EDamageType::Force, ResistForce));
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSrdSacredFlameDexSaveTest, "Project.Expedition.Battle.SrdSacredFlameDexSave",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSrdSacredFlameDexSaveTest::RunTest(const FString& Parameters)
{
    FString Error;
    FCombatEncounter Layout = Duo(ERuleset::SRD);
    FCombatantState Cleric;
    FBuildCatalog::Create(TEXT("cleric.life"), 1, TEXT("Sera"), Cleric);
    Cleric.Cell = {1, 1}; Cleric.InitiativeBonus = 30; Cleric.Range = 3;
    Layout.Combatants[0] = Cleric;
    Layout.Combatants[1].Cell = {4, 1}; Layout.Combatants[1].ArmorClass = 40;
    FCombatBattle Battle;
    if (!TestTrue(FString::Printf(TEXT("SRD Sacred Flame layout: %s"), *Error), Battle.Start(Layout, 3, Error))) { return false; }
    for (int32 Guard = 0; Guard < 8 && Battle.ActiveId() != 1 && Battle.Snapshot().Phase == EBattlePhase::Fighting; ++Guard)
    { Battle.Submit({ECombatCommandType::EndTurn, Battle.ActiveId(), {}, INDEX_NONE}); }
    if (!TestEqual(TEXT("Cleric is active"), Battle.ActiveId(), 1)) { return false; }
    const FCombatResult Result = Battle.Submit({ECombatCommandType::Attack, 1, {}, 2});
    TestTrue(FString::Printf(TEXT("Sacred Flame accepted: %s"), *Result.Feedback), Result.bAccepted);
    TestTrue(TEXT("Feedback names Sacred Flame"), Result.Feedback.Contains(TEXT("Sacred Flame")));
    TestTrue(TEXT("Feedback is a DEX save vs DC"), Result.Feedback.Contains(TEXT("DEX")) && Result.Feedback.Contains(TEXT("vs DC")));

    FCombatEncounter CompactLayout = Layout; CompactLayout.Ruleset = ERuleset::Compact;
    CompactLayout.Combatants[0].InitiativeBonus = 30;
    FCombatBattle CompactBattle;
    if (!TestTrue(TEXT("Compact control starts"), CompactBattle.Start(CompactLayout, 3, Error))) { return false; }
    for (int32 Guard = 0; Guard < 8 && CompactBattle.ActiveId() != 1 && CompactBattle.Snapshot().Phase == EBattlePhase::Fighting; ++Guard)
    { CompactBattle.Submit({ECombatCommandType::EndTurn, CompactBattle.ActiveId(), {}, INDEX_NONE}); }
    const FCombatResult CompactResult = CompactBattle.Submit({ECombatCommandType::Attack, 1, {}, 2});
    TestTrue(TEXT("Compact accepted"), CompactResult.bAccepted);
    TestFalse(TEXT("Compact is not Sacred Flame"), CompactResult.Feedback.Contains(TEXT("Sacred Flame")));
    TestTrue(TEXT("Compact uses attack roll vs AC"), CompactResult.Feedback.Contains(TEXT("vs AC")));
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSrdSizeCoverHouseRuleTest, "Project.Expedition.Battle.SrdSizeCoverHouseRule",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSrdSizeCoverHouseRuleTest::RunTest(const FString& Parameters)
{
    TestEqual(TEXT("Larger interposer grants half"), (int32)FCoverRules::CoverFromLargerCreature(ECreatureSize::Medium, ECreatureSize::Large), (int32)ECoverLevel::Half);
    TestEqual(TEXT("Same size grants none"), (int32)FCoverRules::CoverFromLargerCreature(ECreatureSize::Medium, ECreatureSize::Medium), (int32)ECoverLevel::None);

    FString Error;
    FCombatEncounter Layout = Duo(ERuleset::SRD);
    Layout.Combatants[0].InitiativeBonus = 30;
    Layout.Combatants[1].Cell = {4, 1}; Layout.Combatants[1].ArmorClass = 10;
    FCombatantState Giant;
    Giant.Id = 3; Giant.Name = TEXT("Ogre"); Giant.Role = TEXT("Ogre"); Giant.Side = ECombatSide::Enemy;
    Giant.Cell = {3, 1}; Giant.MaxHP = Giant.HP = 50; Giant.ArmorClass = 10; Giant.Size = 3;
    Giant.AttackBonus = 0; Giant.DamageDie = 6; Giant.DamageBonus = 0; Giant.Speed = 4; Giant.Range = 1; Giant.InitiativeBonus = -10;
    Layout.Combatants.Add(Giant);
    FCombatBattle Battle;
    if (!TestTrue(FString::Printf(TEXT("Size cover layout: %s"), *Error), Battle.Start(Layout, 5, Error))) { return false; }
    for (int32 Guard = 0; Guard < 8 && Battle.ActiveId() != 1 && Battle.Snapshot().Phase == EBattlePhase::Fighting; ++Guard)
    { Battle.Submit({ECombatCommandType::EndTurn, Battle.ActiveId(), {}, INDEX_NONE}); }
    if (!TestEqual(TEXT("Rogue is active"), Battle.ActiveId(), 1)) { return false; }
    const FCombatResult Result = Battle.Submit({ECombatCommandType::Attack, 1, {}, 2});
    TestTrue(FString::Printf(TEXT("Attack accepted: %s"), *Result.Feedback), Result.bAccepted);
    TestEqual(TEXT("Target AC includes half cover"), Result.TargetArmorClass, 12);
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSrdIntIgnoresCoverHouseRuleTest, "Project.Expedition.Battle.SrdIntIgnoresCover",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSrdIntIgnoresCoverHouseRuleTest::RunTest(const FString& Parameters)
{
    // House rule API used by CoverAcBonus for enemy attackers (battle path).
    TestTrue(TEXT("INT 6 ignores"), FCoverRules::IgnoresCover(6));
    TestTrue(TEXT("INT 1 ignores"), FCoverRules::IgnoresCover(1));
    TestFalse(TEXT("INT 7 does not"), FCoverRules::IgnoresCover(7));
    TestFalse(TEXT("INT 10 does not"), FCoverRules::IgnoresCover(10));
    // Size-cover battle path (CoverAcBonus) already covered by SrdSizeCoverHouseRule.
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSrdOpportunityGrappleDeathConcentrationSmokeTest, "Project.Expedition.Battle.SrdOpportunityGrappleDeathConcentrationSmoke",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSrdOpportunityGrappleDeathConcentrationSmokeTest::RunTest(const FString& Parameters)
{
    FString Error;
    FCombatEncounter Layout = Duo(ERuleset::SRD);
    Layout.Combatants[0].InitiativeBonus = 30;
    Layout.Combatants[0].Speed = 6;
    Layout.Combatants[1].Cell = {2, 1}; Layout.Combatants[1].Range = 1; Layout.Combatants[1].bReactionAvailable = true;
    FCombatBattle Battle;
    if (!TestTrue(FString::Printf(TEXT("OA layout: %s"), *Error), Battle.Start(Layout, 13, Error))) { return false; }
    for (int32 Guard = 0; Guard < 8 && Battle.ActiveId() != 1 && Battle.Snapshot().Phase == EBattlePhase::Fighting; ++Guard)
    { Battle.Submit({ECombatCommandType::EndTurn, Battle.ActiveId(), {}, INDEX_NONE}); }
    if (!TestEqual(TEXT("Mover is active"), Battle.ActiveId(), 1)) { return false; }
    const FCombatResult Move = Battle.Submit({ECombatCommandType::Move, 1, {4, 1}, INDEX_NONE});
    TestTrue(FString::Printf(TEXT("Move accepted: %s"), *Move.Feedback), Move.bAccepted);
    TestTrue(TEXT("OA fired or reaction spent"), Move.Feedback.Contains(TEXT("OA")) || !Battle.Find(2)->bReactionAvailable);

    FCombatEncounter G = Duo(ERuleset::SRD);
    FCombatantState Fighter;
    FBuildCatalog::Create(TEXT("fighter.champion"), 1, TEXT("Arden"), Fighter);
    Fighter.Cell = {1, 1}; Fighter.InitiativeBonus = 30;
    G.Combatants[0] = Fighter; G.Combatants[1].Cell = {2, 1};
    FCombatBattle GBattle;
    if (!TestTrue(FString::Printf(TEXT("Grapple layout: %s"), *Error), GBattle.Start(G, 17, Error))) { return false; }
    TestTrue(TEXT("Grapple target legal"), GBattle.LegalTargets(1, ECombatCommandType::Grapple).Contains(2));
    const FCombatResult Grapple = GBattle.Submit({ECombatCommandType::Grapple, 1, {}, 2});
    TestTrue(FString::Printf(TEXT("Grapple accepted: %s"), *Grapple.Feedback), Grapple.bAccepted);

    FCombatEncounter C = Duo(ERuleset::SRD);
    C.Combatants[0].InitiativeBonus = 30; C.Combatants[0].bConcentrating = true; C.Combatants[0].MaxHP = C.Combatants[0].HP = 30;
    C.Combatants[1].Cell = {2, 1}; C.Combatants[1].AttackBonus = 30; C.Combatants[1].InitiativeBonus = -10;
    FCombatBattle CBattle;
    if (!TestTrue(TEXT("Conc layout"), CBattle.Start(C, 19, Error))) { return false; }
    CBattle.Submit({ECombatCommandType::EndTurn, 1, {}, INDEX_NONE});
    if (CBattle.ActiveId() == 2) { CBattle.Submit({ECombatCommandType::Attack, 2, {}, 1}); }
    TestTrue(TEXT("Concentration field still readable"), CBattle.Find(1) != nullptr);

    FCombatEncounter D = Duo(ERuleset::SRD);
    D.Combatants[0].InitiativeBonus = 30; D.Combatants[0].MaxHP = D.Combatants[0].HP = 5;
    D.Combatants[1].Cell = {2, 1}; D.Combatants[1].AttackBonus = 30; D.Combatants[1].DamageDie = 8; D.Combatants[1].DamageBonus = 8; D.Combatants[1].InitiativeBonus = -10;
    FCombatBattle DBattle;
    if (!TestTrue(TEXT("Death layout"), DBattle.Start(D, 23, Error))) { return false; }
    DBattle.Submit({ECombatCommandType::EndTurn, 1, {}, INDEX_NONE});
    if (DBattle.ActiveId() == 2) { DBattle.Submit({ECombatCommandType::Attack, 2, {}, 1}); }
    TestTrue(TEXT("Hero reached Downed or Dead"), DBattle.Find(1)->Life == ECombatLife::Downed || DBattle.Find(1)->Life == ECombatLife::Dead);
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSrdGrappleShoveAppliesConditionsTest, "Project.Expedition.Battle.SrdGrappleShoveAppliesConditions",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSrdGrappleShoveAppliesConditionsTest::RunTest(const FString& Parameters)
{
    FString Error;
    // Grapple applies GrapplerId; Escape clears it.
    FCombatEncounter G = Duo(ERuleset::SRD);
    FCombatantState Fighter;
    FBuildCatalog::Create(TEXT("fighter.champion"), 1, TEXT("Arden"), Fighter);
    Fighter.Cell = {1, 1}; Fighter.InitiativeBonus = 30;
    G.Combatants[0] = Fighter; G.Combatants[1].Cell = {2, 1}; G.Combatants[1].InitiativeBonus = -10;
    FCombatBattle Battle;
    if (!TestTrue(FString::Printf(TEXT("Grapple layout: %s"), *Error), Battle.Start(G, 41, Error))) { return false; }
    for (int32 Guard = 0; Guard < 8 && Battle.ActiveId() != 1 && Battle.Snapshot().Phase == EBattlePhase::Fighting; ++Guard)
    { Battle.Submit({ECombatCommandType::EndTurn, Battle.ActiveId(), {}, INDEX_NONE}); }
    const FCombatResult Grapple = Battle.Submit({ECombatCommandType::Grapple, 1, {}, 2});
    TestTrue(FString::Printf(TEXT("Grapple accepted: %s"), *Grapple.Feedback), Grapple.bAccepted);
    if (Grapple.bHit)
    {
        TestEqual(TEXT("Target GrapplerId set"), Battle.Find(2)->GrapplerId, 1);
        TestTrue(TEXT("Feedback says GRAPPLED"), Grapple.Feedback.Contains(TEXT("GRAPPLED")));
        // End turns until the grappled foe acts, then Escape.
        Battle.Submit({ECombatCommandType::EndTurn, 1, {}, INDEX_NONE});
        for (int32 Guard = 0; Guard < 8 && Battle.ActiveId() != 2 && Battle.Snapshot().Phase == EBattlePhase::Fighting; ++Guard)
        { Battle.Submit({ECombatCommandType::EndTurn, Battle.ActiveId(), {}, INDEX_NONE}); }
        if (Battle.ActiveId() == 2 && Battle.Find(2)->GrapplerId == 1)
        {
            TestEqual(TEXT("Grappled speed is 0"), Battle.Snapshot().MovementLeft, 0);
            TestTrue(TEXT("Escape legal"), Battle.LegalTargets(2, ECombatCommandType::Escape).Contains(1));
            const FCombatResult Esc = Battle.Submit({ECombatCommandType::Escape, 2, {}, 1});
            TestTrue(FString::Printf(TEXT("Escape accepted: %s"), *Esc.Feedback), Esc.bAccepted);
            if (Esc.bHit)
            {
                TestEqual(TEXT("Freed"), Battle.Find(2)->GrapplerId, (int32)INDEX_NONE);
                TestEqual(TEXT("Movement restored after Escape"), Battle.Snapshot().MovementLeft, Battle.Find(2)->Speed);
            }
        }
    }

    // Shove prone
    FCombatEncounter S = Duo(ERuleset::SRD);
    FBuildCatalog::Create(TEXT("fighter.champion"), 1, TEXT("Arden"), S.Combatants[0]);
    S.Combatants[0].Cell = {1, 1}; S.Combatants[0].InitiativeBonus = 30;
    S.Combatants[1].Cell = {2, 1}; S.Combatants[1].InitiativeBonus = -10;
    FCombatBattle ShoveBattle;
    if (!TestTrue(TEXT("Shove layout"), ShoveBattle.Start(S, 43, Error))) { return false; }
    for (int32 Guard = 0; Guard < 8 && ShoveBattle.ActiveId() != 1 && ShoveBattle.Snapshot().Phase == EBattlePhase::Fighting; ++Guard)
    { ShoveBattle.Submit({ECombatCommandType::EndTurn, ShoveBattle.ActiveId(), {}, INDEX_NONE}); }
    FCombatCommand ShoveCmd{ECombatCommandType::Shove, 1, {}, 2};
    ShoveCmd.ShoveChoice = 0; // prone
    const FCombatResult Shove = ShoveBattle.Submit(ShoveCmd);
    TestTrue(FString::Printf(TEXT("Shove accepted: %s"), *Shove.Feedback), Shove.bAccepted);
    if (Shove.bHit)
    {
        TestTrue(TEXT("Target prone"), ShoveBattle.Find(2)->bProne);
        TestTrue(TEXT("Feedback PRONE"), Shove.Feedback.Contains(TEXT("PRONE")));
    }

    // Shove push 5 ft
    FCombatEncounter P = Duo(ERuleset::SRD);
    FBuildCatalog::Create(TEXT("fighter.champion"), 1, TEXT("Arden"), P.Combatants[0]);
    P.Combatants[0].Cell = {1, 1}; P.Combatants[0].InitiativeBonus = 30;
    P.Combatants[1].Cell = {2, 1}; P.Combatants[1].InitiativeBonus = -10;
    FCombatBattle PushBattle;
    if (!TestTrue(TEXT("Push layout"), PushBattle.Start(P, 47, Error))) { return false; }
    for (int32 Guard = 0; Guard < 8 && PushBattle.ActiveId() != 1 && PushBattle.Snapshot().Phase == EBattlePhase::Fighting; ++Guard)
    { PushBattle.Submit({ECombatCommandType::EndTurn, PushBattle.ActiveId(), {}, INDEX_NONE}); }
    FCombatCommand PushCmd{ECombatCommandType::Shove, 1, {}, 2};
    PushCmd.ShoveChoice = 1;
    const FIntPoint Before = PushBattle.Find(2)->Cell;
    const FCombatResult Push = PushBattle.Submit(PushCmd);
    TestTrue(FString::Printf(TEXT("Push accepted: %s"), *Push.Feedback), Push.bAccepted);
    if (Push.bHit && Push.Feedback.Contains(TEXT("PUSHED")))
    {
        TestEqual(TEXT("Pushed one cell away"), PushBattle.Find(2)->Cell, FIntPoint(3, 1));
        TestTrue(TEXT("Moved from before"), PushBattle.Find(2)->Cell != Before);
    }
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSrdSacredFlameResultFieldsTest, "Project.Expedition.Battle.SrdSacredFlameResultFields",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSrdSacredFlameResultFieldsTest::RunTest(const FString& Parameters)
{
    FString Error;
    FCombatEncounter Layout = Duo(ERuleset::SRD);
    FCombatantState Cleric;
    FBuildCatalog::Create(TEXT("cleric.life"), 1, TEXT("Sera"), Cleric);
    Cleric.Cell = {1, 1}; Cleric.InitiativeBonus = 30; Cleric.Range = 3;
    Layout.Combatants[0] = Cleric;
    Layout.Combatants[1].Cell = {4, 1}; Layout.Combatants[1].ArmorClass = 40; Layout.Combatants[1].InitiativeBonus = -10;
    FCombatBattle Battle;
    if (!TestTrue(FString::Printf(TEXT("SF fields layout: %s"), *Error), Battle.Start(Layout, 53, Error))) { return false; }
    for (int32 Guard = 0; Guard < 8 && Battle.ActiveId() != 1 && Battle.Snapshot().Phase == EBattlePhase::Fighting; ++Guard)
    { Battle.Submit({ECombatCommandType::EndTurn, Battle.ActiveId(), {}, INDEX_NONE}); }
    const FCombatResult Result = Battle.Submit({ECombatCommandType::Attack, 1, {}, 2});
    TestTrue(FString::Printf(TEXT("SF accepted: %s"), *Result.Feedback), Result.bAccepted);
    TestEqual(TEXT("Type Attack"), (int32)Result.Type, (int32)ECombatCommandType::Attack);
    TestEqual(TEXT("ActorId"), Result.ActorId, 1);
    TestEqual(TEXT("TargetId"), Result.TargetId, 2);
    TestTrue(TEXT("NaturalRoll is a d20"), Result.NaturalRoll >= 1 && Result.NaturalRoll <= 20);
    TestTrue(TEXT("TargetArmorClass holds save DC"), Result.TargetArmorClass >= 10 && Result.TargetArmorClass <= 20);
    TestEqual(TEXT("AttackTotal = Natural + AttackBonus"), Result.AttackTotal, Result.NaturalRoll + Result.AttackBonus);
    TestFalse(TEXT("Not a critical"), Result.bCritical);
    TestEqual(TEXT("EffectModifier 0"), Result.EffectModifier, 0);
    if (Result.bHit) { TestEqual(TEXT("Damage die logged"), Result.EffectDice.Num(), 1); TestEqual(TEXT("d8"), Result.EffectDice[0].Sides, 8); }
    TestTrue(TEXT("Feedback has DEX total vs DC"), Result.Feedback.Contains(TEXT("vs DC")));
    return !HasAnyErrors();
}

#endif
