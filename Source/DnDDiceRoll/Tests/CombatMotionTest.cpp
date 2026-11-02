#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Presentation/ExpeditionPlayback.h"
#include "Presentation/ExpeditionMotion.h"

namespace
{
FCombatSnapshot MotionFixture(EAdventurerRole Role, FIntPoint ActorCell)
{
    FCombatSnapshot Snapshot;
    FCombatantState Hero;
    Hero.Id = 1; Hero.Name = TEXT("Hero"); Hero.Side = ECombatSide::Party; Hero.AdventurerRole = Role; Hero.Cell = ActorCell;
    FCombatantState Foe;
    Foe.Id = 10; Foe.Name = TEXT("Foe"); Foe.Side = ECombatSide::Enemy; Foe.Cell = FIntPoint(4, 2);
    Snapshot.Encounter.Combatants = {Hero, Foe};
    return Snapshot;
}

FCombatResult Attack(ECombatCommandType Type, bool bHit)
{
    FCombatResult Result;
    Result.bAccepted = true; Result.Type = Type; Result.ActorId = 1; Result.TargetId = 10;
    Result.bHit = bHit; Result.Damage = bHit ? 3 : 0; Result.NaturalRoll = 12;
    return Result;
}

float Sum(const TArray<float>& Values) { float Total = 0; for (float Value : Values) { Total += Value; } return Total; }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMotionProfileTableTest, "Project.Expedition.Presentation.Motion.ProfilesAreSane",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FMotionProfileTableTest::RunTest(const FString& Parameters)
{
    for (int32 Index = 0; Index < static_cast<int32>(EExpeditionMotion::Count); ++Index)
    {
        const FMotionProfile& Profile = ExpeditionMotion::Profile(static_cast<EExpeditionMotion>(Index));
        TestTrue(FString::Printf(TEXT("Motion %d has a visible strike"), Index), Profile.Strike > 0.0f);
        TestTrue(FString::Printf(TEXT("Motion %d stays under a second at speed 1"), Index), ExpeditionMotion::TotalSeconds(Profile) < 1.5f);
    }
    const FMotionProfile& Melee = ExpeditionMotion::Profile(EExpeditionMotion::Melee);
    const FMotionProfile& Shoot = ExpeditionMotion::Profile(EExpeditionMotion::Shoot);
    TestTrue(TEXT("Melee wind-up covers the d20 tumble"), Melee.WindUp >= FDiceTray::TumbleSeconds);
    TestTrue(TEXT("Ranged wind-up covers the d20 tumble"), Shoot.WindUp >= FDiceTray::TumbleSeconds);
    TestTrue(TEXT("A lunge travels further than its wind-up"), Melee.Reach > Melee.Pullback);
    TestTrue(TEXT("A lunge stays inside the attacker's cell half"), Melee.Reach < 0.5f);
    TestTrue(TEXT("Dodges step aside"), ExpeditionMotion::Profile(EExpeditionMotion::Dodge).Reach > 0.0f);
    TestTrue(TEXT("Hits knock the target back"), ExpeditionMotion::Profile(EExpeditionMotion::Recoil).Reach > 0.0f);
    TestTrue(TEXT("Steps bob"), ExpeditionMotion::Profile(EExpeditionMotion::Step).Hop > 0.0f);
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMotionCurveTest, "Project.Expedition.Presentation.Motion.PoseCurves",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FMotionCurveTest::RunTest(const FString& Parameters)
{
    const FMotionProfile& Melee = ExpeditionMotion::Profile(EExpeditionMotion::Melee);
    TestEqual(TEXT("Wind-up starts upright"), ExpeditionMotion::WindUpLean(Melee, 0.0f), 0.0f);
    TestEqual(TEXT("Wind-up ends pulled back"), ExpeditionMotion::WindUpLean(Melee, 1.0f), -Melee.Pullback);
    TestEqual(TEXT("Strike starts where the wind-up ended"), ExpeditionMotion::StrikeLean(Melee, 0.0f), -Melee.Pullback);
    TestEqual(TEXT("Strike ends at full reach"), ExpeditionMotion::StrikeLean(Melee, 1.0f), Melee.Reach);
    TestEqual(TEXT("Arc starts at rest"), ExpeditionMotion::Arc(16.0f, 0.0f), 0.0f);
    TestEqual(TEXT("Arc peaks mid-way"), ExpeditionMotion::Arc(16.0f, 0.5f), 16.0f, 0.001f);
    TestEqual(TEXT("Arc lands at rest"), ExpeditionMotion::Arc(16.0f, 1.0f), 0.0f, 0.001f);
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMotionPlaybackStepsTest, "Project.Expedition.Presentation.Motion.PlaybackStepsFollowProfiles",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FMotionPlaybackStepsTest::RunTest(const FString& Parameters)
{
    const FMotionProfile& Melee = ExpeditionMotion::Profile(EExpeditionMotion::Melee);
    const FMotionProfile& Recoil = ExpeditionMotion::Profile(EExpeditionMotion::Recoil);
    const FCombatSnapshot Snapshot = MotionFixture(EAdventurerRole::Fighter, FIntPoint(3, 2));
    FExpeditionPlayback Show;
    Show.Reset(Snapshot);
    Show.Enqueue({Attack(ECombatCommandType::Attack, true)}, Snapshot);
    const TArray<float> Steps = Show.QueuedDurations();
    if (TestTrue(TEXT("Melee plays wind-up, verdict, lunge, impact and recovery"), Steps.Num() >= 5))
    {
        TestEqual(TEXT("Wind-up from the table"), Steps[0], Melee.WindUp);
        TestEqual(TEXT("Lunge from the table"), Steps[2], Melee.Strike);
        TestEqual(TEXT("Recoil from the table"), Steps[3], Recoil.Strike);
        TestEqual(TEXT("Recovery from the table"), Steps[4], Melee.Recover);
    }
    // Halfway through the lunge the fighter is past upright and moving toward the foe (+X).
    Show.Tick(Steps[0] + Steps[1] + Steps[2] * 0.9f);
    TestTrue(TEXT("The lunge leans toward the target"), Show.Unit(1)->Lean.X > 0.1f);
    Show.Finish();
    TestFalse(TEXT("Skip completes every step"), Show.IsBusy());
    TestTrue(TEXT("Skip settles the attacker"), Show.Unit(1)->Lean.IsNearlyZero());
    TestTrue(TEXT("Skip settles the target"), Show.Unit(10)->Lean.IsNearlyZero() && FMath::IsNearlyZero(Show.Unit(10)->Hop));

    FCombatResult Step;
    Step.bAccepted = true; Step.Type = ECombatCommandType::Move; Step.ActorId = 1; Step.Path = {FIntPoint(2, 2), FIntPoint(3, 2)};
    FCombatSnapshot Moved = MotionFixture(EAdventurerRole::Fighter, FIntPoint(1, 2));
    Show.Reset(Moved);
    Moved.Encounter.Combatants[0].Cell = FIntPoint(3, 2);
    Show.Enqueue({Step}, Moved);
    const FMotionProfile& StepProfile = ExpeditionMotion::Profile(EExpeditionMotion::Step);
    TestEqual(TEXT("One step per cell, timed by the table"), Show.QueuedDurations()[1], StepProfile.Strike);
    Show.Tick(StepProfile.Strike * 0.5f);
    TestTrue(TEXT("Mid-step the figure is off the ground"), Show.Unit(1)->Hop > StepProfile.Hop * 0.9f);
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMotionSpeedTest, "Project.Expedition.Presentation.Motion.SpeedScalesTiming",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FMotionSpeedTest::RunTest(const FString& Parameters)
{
    const FCombatSnapshot Snapshot = MotionFixture(EAdventurerRole::Fighter, FIntPoint(3, 2));
    FExpeditionPlayback Show;
    Show.Reset(Snapshot);
    Show.Enqueue({Attack(ECombatCommandType::Attack, false)}, Snapshot);
    const float Total = Sum(Show.QueuedDurations());
    Show.Speed = 2.0f;
    Show.Tick(Total * 0.5f - 0.05f);
    TestTrue(TEXT("Just before half time at double speed it is still playing"), Show.IsBusy());
    Show.Tick(0.1f);
    TestFalse(TEXT("Double speed finishes in half the time"), Show.IsBusy());
    TestTrue(TEXT("The dodge returns to the cell"), Show.Unit(10)->Lean.IsNearlyZero(0.01f));
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSpellVisualIdTest, "Project.Expedition.Presentation.Motion.SpellIdsReachPlaybackEvents",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSpellVisualIdTest::RunTest(const FString& Parameters)
{
    using namespace ExpeditionMotion;
    TestTrue(TEXT("Wizard at range casts Fire Bolt"), SpellFor(static_cast<uint8>(ECombatCommandType::Attack), true, true) == EExpeditionSpell::FireBolt);
    TestTrue(TEXT("Wizard in melee does not"), SpellFor(static_cast<uint8>(ECombatCommandType::Attack), true, false) == EExpeditionSpell::None);
    TestTrue(TEXT("A rogue's arrow is no spell"), SpellFor(static_cast<uint8>(ECombatCommandType::Attack), false, true) == EExpeditionSpell::None);
    TestTrue(TEXT("Magic Missile"), SpellFor(static_cast<uint8>(ECombatCommandType::MagicMissile), true, true) == EExpeditionSpell::MagicMissile);
    TestTrue(TEXT("Cure Wounds"), SpellFor(static_cast<uint8>(ECombatCommandType::CureWounds), false, false) == EExpeditionSpell::CureWounds);
    TestTrue(TEXT("Second Wind is no spell"), SpellFor(static_cast<uint8>(ECombatCommandType::SecondWind), false, false) == EExpeditionSpell::None);

    auto FirstShotSpell = [this](ECombatCommandType Type, EAdventurerRole Role, int32& Count)
    {
        const FCombatSnapshot Snapshot = MotionFixture(Role, FIntPoint(1, 2));
        FExpeditionPlayback Show;
        Show.Reset(Snapshot);
        Show.Enqueue({Attack(Type, true)}, Snapshot);
        for (int32 Tick = 0; Tick < 200 && Show.Projectiles().IsEmpty(); ++Tick) { Show.Tick(0.02f); }
        Count = Show.Projectiles().Num();
        return Count ? Show.Projectiles()[0].Spell : EExpeditionSpell::None;
    };
    int32 Count = 0;
    TestTrue(TEXT("Fire Bolt projectile carries its id"), FirstShotSpell(ECombatCommandType::Attack, EAdventurerRole::Wizard, Count) == EExpeditionSpell::FireBolt);
    TestTrue(TEXT("Fire Bolt trails embers"), Count >= 3);
    TestTrue(TEXT("Magic Missile darts carry their id"), FirstShotSpell(ECombatCommandType::MagicMissile, EAdventurerRole::Wizard, Count) == EExpeditionSpell::MagicMissile);
    TestEqual(TEXT("Three darts"), Count, 3);
    TestTrue(TEXT("Arrows carry none"), FirstShotSpell(ECombatCommandType::Attack, EAdventurerRole::Rogue, Count) == EExpeditionSpell::None);

    FCombatSnapshot Snapshot = MotionFixture(EAdventurerRole::Cleric, FIntPoint(1, 2));
    FCombatResult Heal;
    Heal.bAccepted = true; Heal.Type = ECombatCommandType::CureWounds; Heal.ActorId = 1; Heal.TargetId = 1; Heal.Healing = 4;
    FExpeditionPlayback Show;
    Show.Reset(Snapshot);
    Show.Enqueue({Heal}, Snapshot);
    const TArray<float> Steps = Show.QueuedDurations();
    Show.Tick(Steps[0] + Steps[1] * 0.1f);
    TestTrue(TEXT("Cure Wounds petals appear"), !Show.Sparkles().IsEmpty());
    TestTrue(TEXT("Cure Wounds petals carry the id"), !Show.Sparkles().IsEmpty() && Show.Sparkles()[0].Spell == EExpeditionSpell::CureWounds);
    return !HasAnyErrors();
}
#endif