#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Persistence/ExpeditionSave.h"
#include "Party/BuildCatalog.h"
#include "Rules/SpellCatalogRules.h"
#include "ExpeditionTestSupport.h"
#include "Presentation/ExpeditionSpellFX.h"

namespace SpellFXTrialFixtureLocal
{
/** Hands the turn to a conscious party wizard (only the final encounter has a legal level-5 wizard) and refills its slots. */
bool WizardTurn(FExpeditionSnapshot& Snap)
{
    FCombatSnapshot& B = Snap.Battle;
    for (FCombatantState& Me : B.Encounter.Combatants)
    {
        if (Me.Side != ECombatSide::Party || Me.AdventurerRole != EAdventurerRole::Wizard || Me.Life != ECombatLife::Conscious || Me.Level < 5) { continue; }
        const int32 Turn = B.TurnOrder.IndexOfByKey(Me.Id);
        if (Turn == INDEX_NONE) { return false; }
        B.TurnIndex = Turn;
        B.bActionAvailable = true; B.bBonusActionAvailable = true; B.bAttackActionTaken = false; B.AttacksRemaining = 0;
        B.bSlotSpentThisTurn = false; B.MovementLeft = Me.Speed;
        // Expedition casters spend their legacy slots by the final battle; the trial grants the level-5 table (capped at 2,
        // as SpellVisualsTest's VisualCaster does) so a 3rd-level Fireball is castable. Dev fixture only.
        Me.SlotsByLevel.SetNum(FSpellRules::MaxSpellLevel);
        for (int32 L = 1; L <= FSpellRules::MaxSpellLevel; ++L) { Me.SlotsByLevel[L - 1] = FMath::Min(2, FBuildCatalog::SlotMax(Me, L)); }
        Me.SpellSlots = Me.SlotsByLevel[0];
        return true;
    }
    return false;
}
}

/**
 * Developer tool, not a behavior test: writes Saved/CaptureFixtures/spellfx-trial/current-v1.json,
 * the opening of a real final (Vault) battle where the party's level-5 wizard holds the turn, so the
 * Fire Bolt / Fireball VFX trial (-SpellFXTrial=<spell>) casts through the normal command path.
 * Run explicitly:  tools/dev.py test --filter Project.Tools.SpellFXTrialFixture
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSpellFXTrialFixture, "Project.Tools.SpellFXTrialFixture",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSpellFXTrialFixture::RunTest(const FString& Parameters)
{
    auto Survivor = [](const FExpeditionRun& Run) -> int32
    {
        for (const FCombatantState& Unit : Run.Battle().Snapshot().Encounter.Combatants)
        { if (Unit.Side == ECombatSide::Party && Unit.Life == ECombatLife::Conscious) { return Unit.Id; } }
        return INDEX_NONE;
    };
    for (int32 Seed = 1; Seed < 120; ++Seed)
    {
        FString Error;
        FExpeditionRun Run;
        Run.NewRun(Seed, Error, FString::Printf(TEXT("spellfx-trial-%d"), Seed));
        bool bOk = true;
        while (bOk && Run.Progress().Phase == EExpeditionPhase::Route && Run.Progress().EncountersCompleted < 4)
        {
            bOk = Run.ChooseRoute(Run.Progress().Offers[0].Id, Error) && ExpeditionTestSupport::FinishCombat(Run) &&
                Run.Progress().Phase == EExpeditionPhase::Reward && Run.ClaimReward(Survivor(Run), Error);
            if (bOk && Run.Progress().Phase != EExpeditionPhase::Route) { Run.Recover(true, Error); }
        }
        if (!bOk || Run.Progress().Phase != EExpeditionPhase::Route || !Run.ChooseRoute(Run.Progress().Offers[0].Id, Error)) { continue; }
        FExpeditionSnapshot Snap = Run.Snapshot();
        if (!SpellFXTrialFixtureLocal::WizardTurn(Snap)) { continue; }
        FExpeditionRun Check;
        if (!Check.Restore(Snap, Error)) { AddInfo(FString::Printf(TEXT("Seed %d rejected: %s"), Seed, *Error)); continue; }
        // Review frames need Fire Bolt to hit and Fireball to leave a foe standing (no Victory banner over the blast).
        // Rolls are deterministic from the snapshot, so simulate both casts exactly as the harness will make them.
        auto Trial = [&Snap](const FString& Spell, int32 Slot, bool& bHit, bool& bFoeLeft)
        {
            FExpeditionRun Sim; FString E;
            if (!Sim.Restore(Snap, E)) { return false; }
            const int32 Active = Sim.Battle().ActiveId();
            FCombatCommand Cast{ECombatCommandType::CastSpell, Active, {}, ExpeditionSpellFXTrial::PickTarget(Sim.Battle().Snapshot(), Active, Spell)};
            Cast.SpellId = Spell; Cast.SlotLevel = Slot;
            const FCombatResult R = Sim.Submit(Cast);
            bHit = R.bHit || R.Damage > 0; bFoeLeft = false;
            for (const FCombatantState& U : Sim.Battle().Snapshot().Encounter.Combatants) { bFoeLeft |= U.Side == ECombatSide::Enemy && U.Life == ECombatLife::Conscious; }
            return R.bAccepted;
        };
        bool bBoltHit = false, bBoltLeft = false, bBallHit = false, bBallLeft = false;
        if (!Trial(TEXT("fire-bolt"), 0, bBoltHit, bBoltLeft) || !bBoltHit || !bBoltLeft || !Trial(TEXT("fireball"), 3, bBallHit, bBallLeft) || !bBallHit || !bBallLeft)
        { AddInfo(FString::Printf(TEXT("Seed %d skipped: bolt hit %d left %d, fireball hit %d left %d"), Seed, bBoltHit, bBoltLeft, bBallHit, bBallLeft)); continue; }
        FExpeditionSaveData Data;
        Data.Current = Snap;
        TestTrue(TEXT("Fixture writes: ") + Error, FExpeditionSave::Write(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("CaptureFixtures"), TEXT("spellfx-trial"), TEXT("current-v1.json")), Data, Error));
        AddInfo(FString::Printf(TEXT("SpellFX trial fixture from seed %d"), Seed));
        return !HasAnyErrors();
    }
    AddError(TEXT("No seed reached the final battle with a living level-5 wizard whose Fire Bolt hits and whose Fireball leaves a foe standing."));
    return false;
}
#endif
