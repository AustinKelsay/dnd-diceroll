#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "RenderingThread.h"
#include "UObject/GarbageCollection.h"
#include "Engine/World.h"
#include "Components/PrimitiveComponent.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Persistence/ExpeditionSave.h"
#include "Presentation/DiceRollTheme.h"
#include "Presentation/ExpeditionBattleView.h"
#include "Presentation/ExpeditionPlayback.h"
#include "Presentation/UnitPose.h"
#include "Run/ExpeditionRun.h"
#include "ExpeditionTestSupport.h"

// Rigged characters (#147): playback chooses clips from committed results and every figure converges on the pose the
// committed state implies. The first three tests need no world; the last three build boards and read UnitRigRoot and
// UnitAnimation. The board tests need the imported rig assets (Content/Python/import_dark_fantasy_theme.py).

namespace RigPoseTest
{
// Same teardown as the other board tests: release render resources now, or lingering worlds push RHI reserved memory
// past its budget later in the suite.
void ReleaseRigWorld(UWorld* World)
{
    World->DestroyWorld(false);
    FlushRenderingCommands();
    CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
}

UWorld* NewRigWorld()
{
    const UWorld::InitializationValues Values = UWorld::InitializationValues()
        .AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false);
    return UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
}

uint32 Bit(EUnitPose Pose) { return 1u << static_cast<uint32>(Pose); }

/** Every pose each unit showed while a scene played. */
struct FSeen
{
    TMap<int32, uint32> Poses;
    void Note(const FExpeditionPlayback& Show, const FCombatSnapshot& Snapshot)
    {
        for (const FCombatantState& Unit : Snapshot.Encounter.Combatants)
        {
            if (const FUnitDisplay* Display = Show.Unit(Unit.Id)) { Poses.FindOrAdd(Unit.Id) |= Bit(Display->Pose); }
        }
    }
    bool Saw(int32 Id, EUnitPose Pose) const { const uint32* Mask = Poses.Find(Id); return Mask && (*Mask & Bit(Pose)) != 0; }
};

// Plays queued steps in small frames, recording the poses seen.
void PlayOut(FExpeditionPlayback& Show, const FCombatSnapshot& Snapshot, FSeen& Seen)
{
    Seen.Note(Show, Snapshot);
    for (int32 Frame = 0; Frame < 4000 && Show.IsBusy(); ++Frame) { Show.Tick(0.02f); Seen.Note(Show, Snapshot); }
}

enum class EEnd : uint8 { Normal, Fast, Instant, SkipMid };

// Settles queued steps the way a player's speed setting or a skip would.
void Settle(FExpeditionPlayback& Show, const FCombatSnapshot& After, EEnd End, FSeen& Seen)
{
    Show.Speed = End == EEnd::Fast ? 2.2f : 1.0f;
    switch (End)
    {
    case EEnd::Instant: Show.Finish(); Seen.Note(Show, After); break;
    case EEnd::SkipMid:
        for (int32 Frame = 0; Frame < 15; ++Frame) { Show.Tick(0.02f); Seen.Note(Show, After); }
        Show.Finish();
        Seen.Note(Show, After);
        break;
    default: PlayOut(Show, After, Seen); break;
    }
}

void ExpectPose(FAutomationTestBase& Test, const FString& What, EUnitPose Actual, EUnitPose Expected)
{
    Test.TestTrue(FString::Printf(TEXT("%s: %s (expected %s)"), *What, ExpeditionPose::ClipName(Actual), ExpeditionPose::ClipName(Expected)), Actual == Expected);
}

// Every unit shows the pose its committed life implies.
void ExpectConverged(FAutomationTestBase& Test, const FString& What, const FExpeditionPlayback& Show, const FCombatSnapshot& Committed)
{
    for (const FCombatantState& Unit : Committed.Encounter.Combatants)
    {
        const FUnitDisplay* Display = Show.Unit(Unit.Id);
        if (!Test.TestNotNull(*FString::Printf(TEXT("%s: unit %d has a display"), *What, Unit.Id), Display)) { continue; }
        ExpectPose(Test, FString::Printf(TEXT("%s: %s"), *What, *Unit.Name), Display->Pose, ExpeditionPose::Rest(Unit.Life));
        Test.TestTrue(FString::Printf(TEXT("%s: %s shows the committed life"), *What, *Unit.Name), Display->Life == Unit.Life);
    }
}

bool StartRigRun(FExpeditionRun& Run, int32 Seed, const TCHAR* Identity)
{
    FString Error;
    return Run.NewRun(Seed, Error, Identity) && Run.ChooseRoute(Run.Progress().Offers[0].Id, Error);
}

// A hand-made scene on a real encounter: a hero, an ally and a foe placed on known cells. Playback only reads the units
// a result names.
struct FScene
{
    FCombatSnapshot Before, After;
    int32 Hero = INDEX_NONE, Ally = INDEX_NONE, Foe = INDEX_NONE;
    FCombatantState* Unit(FCombatSnapshot& Snapshot, int32 Id)
    { return Snapshot.Encounter.Combatants.FindByPredicate([Id](const FCombatantState& U) { return U.Id == Id; }); }
};

bool MakeScene(FScene& Scene, int32 FoeX)
{
    FExpeditionRun Run;
    if (!StartRigRun(Run, 11, TEXT("pose-scene"))) { return false; }
    Scene.Before = Run.Battle().Snapshot();
    for (const FCombatantState& Unit : Scene.Before.Encounter.Combatants)
    {
        if (Unit.Side == ECombatSide::Party && Scene.Hero == INDEX_NONE) { Scene.Hero = Unit.Id; }
        else if (Unit.Side == ECombatSide::Party && Scene.Ally == INDEX_NONE) { Scene.Ally = Unit.Id; }
        else if (Unit.Side == ECombatSide::Enemy && Scene.Foe == INDEX_NONE) { Scene.Foe = Unit.Id; }
    }
    if (Scene.Hero == INDEX_NONE || Scene.Ally == INDEX_NONE || Scene.Foe == INDEX_NONE) { return false; }
    Scene.Unit(Scene.Before, Scene.Hero)->Cell = FIntPoint(2, 3);
    Scene.Unit(Scene.Before, Scene.Foe)->Cell = FIntPoint(FoeX, 3);
    Scene.Unit(Scene.Before, Scene.Ally)->Cell = FIntPoint(2, 5);
    for (const int32 Id : {Scene.Hero, Scene.Ally, Scene.Foe})
    {
        FCombatantState* Unit = Scene.Unit(Scene.Before, Id);
        Unit->MaxHP = Id == Scene.Foe ? 10 : 20;
        Unit->HP = Unit->MaxHP;
    }
    Scene.After = Scene.Before;
    return true;
}

FCombatResult MakeResult(ECombatCommandType Type, int32 Actor, int32 Target = INDEX_NONE)
{
    FCombatResult Out;
    Out.bAccepted = true;
    Out.Type = Type;
    Out.ActorId = Actor;
    Out.TargetId = Target;
    return Out;
}

// One scene played at every ending; the checks run on the poses seen at 1x and 2x and on the final pose everywhere.
template <typename TCheck>
void RunScene(FAutomationTestBase& Test, const TCHAR* Name, const FScene& Scene, const FCombatResult& Outcome, TCheck Check)
{
    for (const EEnd End : {EEnd::Normal, EEnd::Fast, EEnd::Instant, EEnd::SkipMid})
    {
        const FString Label = FString::Printf(TEXT("%s (ending %d)"), Name, static_cast<int32>(End));
        FExpeditionPlayback Show;
        Show.Reset(Scene.Before);
        Show.Enqueue({Outcome}, Scene.After);
        FSeen Seen;
        Settle(Show, Scene.After, End, Seen);
        Test.TestFalse(*(Label + TEXT(": playback is done")), Show.IsBusy());
        ExpectConverged(Test, Label, Show, Scene.After);
        if (End == EEnd::Normal || End == EEnd::Fast) { Check(Label, Seen); }
        if (End == EEnd::Instant || End == EEnd::SkipMid)
        {
            for (const FCombatantState& Unit : Scene.After.Encounter.Combatants)
            { Test.TestTrue(*FString::Printf(TEXT("%s: %s converged on its end pose"), *Label, *Unit.Name), Show.Unit(Unit.Id) && Show.Unit(Unit.Id)->bPoseSnap); }
        }
    }
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUnitPoseChoiceTest, "Project.Expedition.Presentation.UnitPoseChoice",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FUnitPoseChoiceTest::RunTest(const FString& Parameters)
{
    using namespace RigPoseTest;
    // The clip for each committed event type.
    ExpectPose(*this, TEXT("move"), ExpeditionPose::ForCommand(ECombatCommandType::Move, false), EUnitPose::Walk);
    ExpectPose(*this, TEXT("adjacent attack"), ExpeditionPose::ForCommand(ECombatCommandType::Attack, false), EUnitPose::Melee);
    ExpectPose(*this, TEXT("distant attack"), ExpeditionPose::ForCommand(ECombatCommandType::Attack, true), EUnitPose::Ranged);
    ExpectPose(*this, TEXT("precision strike up close"), ExpeditionPose::ForCommand(ECombatCommandType::PrecisionStrike, false), EUnitPose::Melee);
    ExpectPose(*this, TEXT("magic missile"), ExpeditionPose::ForCommand(ECombatCommandType::MagicMissile, true), EUnitPose::Cast);
    ExpectPose(*this, TEXT("cure wounds"), ExpeditionPose::ForCommand(ECombatCommandType::CureWounds, false), EUnitPose::Cast);
    for (const ECombatCommandType Plain : {ECombatCommandType::EndTurn, ECombatCommandType::SecondWind, ECombatCommandType::Potion, ECombatCommandType::Rescue})
    { ExpectPose(*this, TEXT("a command with no clip"), ExpeditionPose::ForCommand(Plain, false), EUnitPose::Idle); }
    ExpectPose(*this, TEXT("a weapon effect up close"), ExpeditionPose::ForEffect(EAbilityEffect::WeaponAttack, false), EUnitPose::Melee);
    ExpectPose(*this, TEXT("a strike at range"), ExpeditionPose::ForEffect(EAbilityEffect::Strike, true), EUnitPose::Ranged);
    ExpectPose(*this, TEXT("a spell effect"), ExpeditionPose::ForEffect(EAbilityEffect::Surge, true), EUnitPose::Cast);
    ExpectPose(*this, TEXT("a swap"), ExpeditionPose::ForEffect(EAbilityEffect::Swap, false), EUnitPose::Idle);
    ExpectPose(*this, TEXT("damage"), ExpeditionPose::ForDamage(), EUnitPose::Hit);
    ExpectPose(*this, TEXT("downing"), ExpeditionPose::ForFall(false), EUnitPose::Downed);
    ExpectPose(*this, TEXT("death"), ExpeditionPose::ForFall(true), EUnitPose::Death);
    ExpectPose(*this, TEXT("recovery"), ExpeditionPose::ForRecovery(), EUnitPose::Recover);
    // The state that holds when nothing plays.
    ExpectPose(*this, TEXT("a conscious unit"), ExpeditionPose::Rest(ECombatLife::Conscious), EUnitPose::Idle);
    ExpectPose(*this, TEXT("a downed unit"), ExpeditionPose::Rest(ECombatLife::Downed), EUnitPose::Downed);
    ExpectPose(*this, TEXT("a dead unit"), ExpeditionPose::Rest(ECombatLife::Dead), EUnitPose::Death);
    // Names, loops and the theme's animation set agree for all nine clips.
    const FDiceRollAnimSet Set;
    TSet<FString> Names;
    TSet<const void*> Slots;
    for (int32 Index = 0; Index < ExpeditionPose::Count; ++Index)
    {
        const EUnitPose Pose = static_cast<EUnitPose>(Index);
        Names.Add(ExpeditionPose::ClipName(Pose));
        TestTrue(TEXT("Only Idle and Walk loop"), ExpeditionPose::IsLooping(Pose) == (Pose == EUnitPose::Idle || Pose == EUnitPose::Walk));
        Slots.Add(&Set.For(Pose));
    }
    TestEqual(TEXT("Nine distinct clip names"), Names.Num(), ExpeditionPose::Count);
    TestEqual(TEXT("Nine distinct animation slots"), Slots.Num(), ExpeditionPose::Count);
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPlaybackPosesTest, "Project.Expedition.Presentation.PlaybackChoosesPoses",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FPlaybackPosesTest::RunTest(const FString& Parameters)
{
    using namespace RigPoseTest;
    // Fresh boards (reset, new run, fresh-process Resume) show the committed pose at once.
    {
        FScene Scene;
        if (!TestTrue(TEXT("Scene builds"), MakeScene(Scene, 3))) { return false; }
        Scene.Unit(Scene.Before, Scene.Foe)->Life = ECombatLife::Dead;
        Scene.Unit(Scene.Before, Scene.Foe)->HP = 0;
        Scene.Unit(Scene.Before, Scene.Ally)->Life = ECombatLife::Downed;
        Scene.Unit(Scene.Before, Scene.Ally)->HP = 0;
        FExpeditionPlayback Show;
        Show.Reset(Scene.Before);
        ExpectConverged(*this, TEXT("fresh board"), Show, Scene.Before);
        ExpectPose(*this, TEXT("fresh board: the dead foe"), Show.Unit(Scene.Foe)->Pose, EUnitPose::Death);
        ExpectPose(*this, TEXT("fresh board: the downed ally"), Show.Unit(Scene.Ally)->Pose, EUnitPose::Downed);
        TestTrue(TEXT("A fresh board snaps to the end of the held clip"), Show.Unit(Scene.Foe)->bPoseSnap);
    }
    {
        FScene Scene;
        if (!TestTrue(TEXT("Scene builds"), MakeScene(Scene, 3))) { return false; }
        FCombatResult Move = MakeResult(ECombatCommandType::Move, Scene.Hero);
        Move.Path = {FIntPoint(3, 3), FIntPoint(4, 3)};
        Scene.Unit(Scene.After, Scene.Hero)->Cell = FIntPoint(4, 3);
        // Keep the foe off the path.
        Scene.Unit(Scene.Before, Scene.Foe)->Cell = FIntPoint(7, 3);
        Scene.Unit(Scene.After, Scene.Foe)->Cell = FIntPoint(7, 3);
        RunScene(*this, TEXT("move"), Scene, Move, [&](const FString& Label, const FSeen& Seen)
        {
            TestTrue(Label + TEXT(": the mover walks"), Seen.Saw(Scene.Hero, EUnitPose::Walk));
            TestFalse(Label + TEXT(": the bystander stays idle"), Seen.Saw(Scene.Foe, EUnitPose::Walk));
        });
    }
    {
        FScene Scene;
        if (!TestTrue(TEXT("Scene builds"), MakeScene(Scene, 3))) { return false; }
        FCombatResult Strike = MakeResult(ECombatCommandType::Attack, Scene.Hero, Scene.Foe);
        Strike.bHit = true;
        Strike.Damage = 3;
        Scene.Unit(Scene.After, Scene.Foe)->HP = Scene.Unit(Scene.Before, Scene.Foe)->HP - 3;
        TestTrue(TEXT("The foe survives the blow"), Scene.Unit(Scene.After, Scene.Foe)->HP > 0);
        RunScene(*this, TEXT("melee hit"), Scene, Strike, [&](const FString& Label, const FSeen& Seen)
        {
            TestTrue(Label + TEXT(": the attacker swings"), Seen.Saw(Scene.Hero, EUnitPose::Melee));
            TestFalse(Label + TEXT(": not a ranged clip"), Seen.Saw(Scene.Hero, EUnitPose::Ranged));
            TestTrue(Label + TEXT(": the target flinches"), Seen.Saw(Scene.Foe, EUnitPose::Hit));
            TestFalse(Label + TEXT(": the target does not fall"), Seen.Saw(Scene.Foe, EUnitPose::Death) || Seen.Saw(Scene.Foe, EUnitPose::Downed));
        });
        Strike.bHit = false;
        Strike.Damage = 0;
        Scene.After = Scene.Before;
        RunScene(*this, TEXT("melee miss"), Scene, Strike, [&](const FString& Label, const FSeen& Seen)
        {
            TestTrue(Label + TEXT(": the attacker still swings"), Seen.Saw(Scene.Hero, EUnitPose::Melee));
            TestFalse(Label + TEXT(": a miss does not flinch"), Seen.Saw(Scene.Foe, EUnitPose::Hit));
        });
    }
    {
        FScene Scene;
        if (!TestTrue(TEXT("Scene builds"), MakeScene(Scene, 3))) { return false; }
        FCombatResult Kill = MakeResult(ECombatCommandType::Attack, Scene.Hero, Scene.Foe);
        Kill.bHit = true;
        Kill.Damage = 99;
        Kill.Died = {Scene.Foe};
        Scene.Unit(Scene.After, Scene.Foe)->HP = 0;
        Scene.Unit(Scene.After, Scene.Foe)->Life = ECombatLife::Dead;
        RunScene(*this, TEXT("killing blow"), Scene, Kill, [&](const FString& Label, const FSeen& Seen)
        {
            TestTrue(Label + TEXT(": the foe dies"), Seen.Saw(Scene.Foe, EUnitPose::Death));
            TestFalse(Label + TEXT(": it does not flinch first"), Seen.Saw(Scene.Foe, EUnitPose::Hit));
        });
    }
    {
        FScene Scene;
        if (!TestTrue(TEXT("Scene builds"), MakeScene(Scene, 7))) { return false; }
        FCombatResult Shot = MakeResult(ECombatCommandType::Attack, Scene.Hero, Scene.Foe);
        Shot.bHit = true;
        Shot.Damage = 2;
        Scene.Unit(Scene.After, Scene.Foe)->HP = Scene.Unit(Scene.Before, Scene.Foe)->HP - 2;
        RunScene(*this, TEXT("ranged attack"), Scene, Shot, [&](const FString& Label, const FSeen& Seen)
        {
            TestTrue(Label + TEXT(": the shooter draws"), Seen.Saw(Scene.Hero, EUnitPose::Ranged));
            TestFalse(Label + TEXT(": not a melee clip"), Seen.Saw(Scene.Hero, EUnitPose::Melee));
        });
        FCombatResult Missile = MakeResult(ECombatCommandType::MagicMissile, Scene.Hero, Scene.Foe);
        Missile.bHit = true;
        Missile.Damage = 2;
        RunScene(*this, TEXT("magic missile"), Scene, Missile, [&](const FString& Label, const FSeen& Seen)
        {
            TestTrue(Label + TEXT(": the caster casts"), Seen.Saw(Scene.Hero, EUnitPose::Cast));
            TestTrue(Label + TEXT(": the target flinches"), Seen.Saw(Scene.Foe, EUnitPose::Hit));
        });
    }
    {
        FScene Scene;
        if (!TestTrue(TEXT("Scene builds"), MakeScene(Scene, 3))) { return false; }
        // The foe downs the hero: the downed clip, not death.
        FCombatResult Blow = MakeResult(ECombatCommandType::Attack, Scene.Foe, Scene.Hero);
        Blow.bHit = true;
        Blow.Damage = 99;
        Blow.Downed = {Scene.Hero};
        Scene.Unit(Scene.After, Scene.Hero)->HP = 0;
        Scene.Unit(Scene.After, Scene.Hero)->Life = ECombatLife::Downed;
        RunScene(*this, TEXT("a party member goes down"), Scene, Blow, [&](const FString& Label, const FSeen& Seen)
        {
            TestTrue(Label + TEXT(": the foe swings"), Seen.Saw(Scene.Foe, EUnitPose::Melee));
            TestTrue(Label + TEXT(": the hero is downed"), Seen.Saw(Scene.Hero, EUnitPose::Downed));
            TestFalse(Label + TEXT(": not dead"), Seen.Saw(Scene.Hero, EUnitPose::Death));
        });
    }
    {
        FScene Scene;
        if (!TestTrue(TEXT("Scene builds"), MakeScene(Scene, 7))) { return false; }
        // The hero heals a downed ally: Cast for the healer, Recover for the ally, Idle after.
        Scene.Unit(Scene.Before, Scene.Ally)->Life = ECombatLife::Downed;
        Scene.Unit(Scene.Before, Scene.Ally)->HP = 0;
        Scene.After = Scene.Before;
        Scene.Unit(Scene.After, Scene.Ally)->Life = ECombatLife::Conscious;
        Scene.Unit(Scene.After, Scene.Ally)->HP = 5;
        FCombatResult Cure = MakeResult(ECombatCommandType::CureWounds, Scene.Hero, Scene.Ally);
        Cure.Healing = 5;
        RunScene(*this, TEXT("recovery"), Scene, Cure, [&](const FString& Label, const FSeen& Seen)
        {
            TestTrue(Label + TEXT(": the healer casts"), Seen.Saw(Scene.Hero, EUnitPose::Cast));
            TestTrue(Label + TEXT(": the ally recovers"), Seen.Saw(Scene.Ally, EUnitPose::Recover));
        });
    }
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPlaybackConvergesTest, "Project.Expedition.Presentation.PlaybackPosesConverge",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FPlaybackConvergesTest::RunTest(const FString& Parameters)
{
    using namespace RigPoseTest;
    // A whole encounter of real commands, played at 1x, 2x, instant and skipped mid-animation: after every batch each
    // unit shows the pose its committed life implies, and (at 1x and 2x) the clips follow the committed results.
    int32 Falls = 0, Flinches = 0, Walks = 0;
    for (const EEnd End : {EEnd::Normal, EEnd::Fast, EEnd::Instant, EEnd::SkipMid})
    {
        FExpeditionRun Run;
        if (!TestTrue(TEXT("Run starts"), StartRigRun(Run, 7, TEXT("pose-converge")))) { return false; }
        FExpeditionPlayback Show;
        Show.Reset(Run.Battle().Snapshot());
        ExpectConverged(*this, TEXT("start"), Show, Run.Battle().Snapshot());
        auto Play = [&](const TArray<FCombatResult>& Results)
        {
            const FCombatSnapshot After = Run.Battle().Snapshot();
            Show.Finish(); // An accepted command completes earlier playback first, as the view does.
            Show.Enqueue(Results, After);
            FSeen Seen;
            Settle(Show, After, End, Seen);
            const FString Label = FString::Printf(TEXT("ending %d, round %d"), static_cast<int32>(End), After.Round);
            ExpectConverged(*this, Label, Show, After);
            if (End != EEnd::Normal && End != EEnd::Fast) { return; }
            for (const FCombatResult& Outcome : Results)
            {
                if (!Outcome.bAccepted) { continue; }
                if (Outcome.Type == ECombatCommandType::Move && !Outcome.Path.IsEmpty())
                {
                    TestTrue(Label + TEXT(": a move walks"), Seen.Saw(Outcome.ActorId, EUnitPose::Walk));
                    ++Walks;
                }
                const bool bStrike = Outcome.Type == ECombatCommandType::Attack || Outcome.Type == ECombatCommandType::PrecisionStrike || Outcome.Type == ECombatCommandType::MagicMissile;
                const FCombatantState* Actor = ExpeditionStyle::FindUnit(After, Outcome.ActorId);
                const FCombatantState* Target = ExpeditionStyle::FindUnit(After, Outcome.TargetId);
                if (!bStrike || !Actor || !Target) { continue; }
                const bool bFar = ExpeditionTestSupport::Distance(Actor->Cell, Target->Cell) > 1;
                TestTrue(Label + TEXT(": the attacker shows the clip for its command"), Seen.Saw(Actor->Id, ExpeditionPose::ForCommand(Outcome.Type, bFar)));
                if (Outcome.Died.Contains(Target->Id)) { TestTrue(Label + TEXT(": the slain fall"), Seen.Saw(Target->Id, EUnitPose::Death)); ++Falls; }
                else if (Outcome.Downed.Contains(Target->Id)) { TestTrue(Label + TEXT(": the downed fall"), Seen.Saw(Target->Id, EUnitPose::Downed)); ++Falls; }
                else if (Outcome.bHit && Target->HP > 0) { TestTrue(Label + TEXT(": a survivor flinches"), Seen.Saw(Target->Id, EUnitPose::Hit)); ++Flinches; }
            }
        };
        for (int32 Turn = 0; Turn < 300 && Run.Progress().Phase == EExpeditionPhase::Combat; ++Turn)
        {
            const FCombatBattle& Battle = Run.Battle();
            const int32 Id = Battle.ActiveId();
            const FCombatantState* Active = Battle.Find(Id);
            if (!Active) { break; }
            if (Active->Side == ECombatSide::Enemy) { Play(Run.PlayEnemyTurn()); continue; }
            auto Do = [&](const FCombatCommand& Command) { const FCombatResult Outcome = Run.Submit(Command); if (Outcome.bAccepted) { Play({Outcome}); } };
            if (Active->HP <= Active->MaxHP / 2 && Battle.LegalTargets(Id, ECombatCommandType::SecondWind).Contains(Id)) { Do({ECombatCommandType::SecondWind, Id, {}, Id}); }
            TArray<int32> Targets = Battle.LegalTargets(Id, ECombatCommandType::MagicMissile);
            if (!Targets.IsEmpty()) { Do({ECombatCommandType::MagicMissile, Id, {}, Targets[0]}); }
            else if (Targets = Battle.LegalTargets(Id); !Targets.IsEmpty()) { Do({ECombatCommandType::Attack, Id, {}, Targets[0]}); }
            else
            {
                TArray<FIntPoint> Moves = Battle.LegalMoves(Id);
                if (!Moves.IsEmpty()) { Do({ECombatCommandType::Move, Id, Moves.Last()}); }
            }
            if (Run.Progress().Phase == EExpeditionPhase::Combat) { Do({ECombatCommandType::EndTurn, Id}); }
        }
        // A fresh process resuming this state shows the same committed poses at once.
        FExpeditionPlayback Resumed;
        Resumed.Reset(Run.Battle().Snapshot());
        ExpectConverged(*this, TEXT("resume"), Resumed, Run.Battle().Snapshot());
    }
    TestTrue(TEXT("The encounters walked"), Walks > 0);
    TestTrue(TEXT("The encounters downed or killed someone"), Falls > 0);
    TestTrue(TEXT("The encounters landed a non-fatal hit"), Flinches > 0);
    return !HasAnyErrors();
}

namespace RigPoseTest
{
// Measured as BoardMetricSingleSource does: the highest visible primitive under the rig root, above the board top.
double FigureHeight(const AExpeditionBattleView& View, USceneComponent* RigRoot)
{
    TArray<USceneComponent*> Children;
    RigRoot->GetChildrenComponents(true, Children);
    double Highest = View.Metric().BoardTop();
    for (USceneComponent* Child : Children)
    {
        const UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(Child);
        if (Primitive && Primitive->IsVisible()) { Highest = FMath::Max(Highest, Primitive->Bounds.GetBox().Max.Z); }
    }
    return Highest - View.Metric().BoardTop();
}

bool ExpectsRig(const FCombatantState& Unit)
{
    if (Unit.Side == ECombatSide::Party) { return Unit.AdventurerRole == EAdventurerRole::Fighter || Unit.AdventurerRole == EAdventurerRole::Wizard; }
    return Unit.Range <= 1 && Unit.Name.Contains(TEXT("raider"));
}

// Every rigged figure shows the committed pose: Death and Downed at the end of their clip, Idle as Idle.
int32 ExpectFiguresConverged(FAutomationTestBase& Test, const AExpeditionBattleView& View, const FString& What)
{
    int32 Rigged = 0;
    for (const FCombatantState& Unit : View.Rules().Snapshot().Encounter.Combatants)
    {
        const FUnitAnimationState State = View.UnitAnimation(Unit.Id);
        if (!State.bSkeletal) { continue; }
        ++Rigged;
        const EUnitPose Rest = ExpeditionPose::Rest(Unit.Life);
        const FString Label = FString::Printf(TEXT("%s: %s"), *What, *Unit.Name);
        ExpectPose(Test, Label, State.Pose, Rest);
        Test.TestTrue(Label + TEXT(" plays the clip named for its pose"), State.Clip.Contains(ExpeditionPose::ClipName(Rest)));
        if (Rest != EUnitPose::Idle)
        {
            Test.TestTrue(Label + TEXT(" has a real clip"), State.Length > 0.1f);
            Test.TestTrue(*(Label + FString::Printf(TEXT(" holds the end of its clip (%.3f of %.3f s)"), State.Time, State.Length)), State.Time >= State.Length - 0.001f);
        }
    }
    return Rigged;
}

enum class EView : uint8 { Rigged, Static, Primitive };

AExpeditionBattleView* MakeView(UWorld* World, const FString& Path, EView Mode)
{
    AExpeditionBattleView* View = World->SpawnActor<AExpeditionBattleView>();
    if (!View) { return nullptr; }
    View->ConfigureStorage(Path);
    View->DispatchBeginPlay();
    if (Mode == EView::Static) { View->UseStaticFigures(); }
    else if (Mode == EView::Primitive) { View->UsePrimitives(); }
    return View;
}

// One rules-only party policy step for the view: attack, else move, then end the turn. Returns whether it commanded.
void PartyStep(AExpeditionBattleView& View, int32& LastId, int32& Stage)
{
    const int32 Id = View.Rules().ActiveId();
    if (Id != LastId) { LastId = Id; Stage = 0; }
    const TArray<int32> Targets = View.Rules().LegalTargets(Id);
    const TArray<FIntPoint> Moves = View.Rules().LegalMoves(Id);
    if (Stage == 0 && !Targets.IsEmpty()) { View.Execute({ECombatCommandType::Attack, Id, {}, Targets[0]}); }
    else if (Stage == 0 && !Moves.IsEmpty()) { View.Execute({ECombatCommandType::Move, Id, Moves.Last()}); }
    else { View.Execute({ECombatCommandType::EndTurn, Id}); }
    ++Stage;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRiggedFiguresTest, "Project.Expedition.Presentation.RiggedFigures",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRiggedFiguresTest::RunTest(const FString& Parameters)
{
    using namespace RigPoseTest;
    const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation/RiggedFigures"), FGuid::NewGuid().ToString());
    UWorld* World = NewRigWorld();
    if (!TestNotNull(TEXT("Transient world"), World)) { return false; }

    // The pilot characters stand on their cells at the metric's figure height, through UnitRigRoot.
    {
        AExpeditionBattleView* View = MakeView(World, FPaths::Combine(Directory, TEXT("stand"), TEXT("current.json")), EView::Rigged);
        if (TestNotNull(TEXT("Battle view"), View))
        {
            View->NewRun();
            View->ChooseRoute(0);
            View->Tick(0.016f);
            TestTrue(TEXT("The shipped theme loads"), View->UsesTheme());
            int32 Expected = 0;
            for (const FCombatantState& Unit : View->Rules().Snapshot().Encounter.Combatants)
            {
                USceneComponent* RigRoot = View->UnitRigRoot(Unit.Id);
                if (!TestNotNull(*FString::Printf(TEXT("%s has a rig root"), *Unit.Name), RigRoot)) { continue; }
                TestTrue(FString::Printf(TEXT("%s's rig root is on its cell"), *Unit.Name),
                    FVector2D(RigRoot->GetRelativeLocation()).Equals(FVector2D(View->CellPosition(Unit.Cell)), 0.1f));
                const FUnitAnimationState State = View->UnitAnimation(Unit.Id);
                if (ExpectsRig(Unit))
                {
                    ++Expected;
                    TestTrue(FString::Printf(TEXT("%s is a rigged figure (run import_dark_fantasy_theme.py)"), *Unit.Name), State.bSkeletal);
                }
                if (!State.bSkeletal) { continue; }
                const double Height = FigureHeight(*View, RigRoot);
                TestTrue(FString::Printf(TEXT("%s stands 175 to 185 cm (measured %.1f)"), *Unit.Name, Height), Height >= 175.0 && Height <= 185.0);
            }
            TestTrue(TEXT("The default party has rigged characters"), Expected > 0);
            TestEqual(TEXT("Exactly the expected units are rigged"), View->SkeletalFigureCount(), Expected);
            ExpectFiguresConverged(*this, *View, TEXT("a fresh board"));
            View->Destroy();
        }
    }

    // Playback drives the rig; at every speed, and when skipped mid-animation, the figures converge on the committed state.
    for (const EEnd End : {EEnd::Normal, EEnd::Fast, EEnd::Instant, EEnd::SkipMid})
    {
        const FString Label = FString::Printf(TEXT("ending %d"), static_cast<int32>(End));
        const FString Path = FPaths::Combine(Directory, FString::Printf(TEXT("play-%d"), static_cast<int32>(End)), TEXT("current.json"));
        AExpeditionBattleView* View = MakeView(World, Path, EView::Rigged);
        if (!TestNotNull(TEXT("Battle view"), View)) { continue; }
        View->NewRun();
        View->ChooseRoute(0);
        if (End == EEnd::Fast) { View->CycleSpeed(); }
        else if (End == EEnd::Instant) { View->CycleSpeed(); View->CycleSpeed(); }
        TestTrue(*(Label + TEXT(": speed is set")), (End == EEnd::Fast) == (View->Speed() == EPlaybackSpeed::Fast) && (End == EEnd::Instant) == (View->Speed() == EPlaybackSpeed::Instant));
        int32 LastId = INDEX_NONE, Stage = 0, Skips = 0, Moving = 0;
        for (int32 Frame = 0; Frame < 3000 && View->InCombat(); ++Frame)
        {
            View->Tick(0.05f);
            const FCombatantState* Active = View->Rules().Find(View->Rules().ActiveId());
            if (View->IsPresenting())
            {
                for (const FCombatantState& Unit : View->Rules().Snapshot().Encounter.Combatants)
                { Moving += View->UnitAnimation(Unit.Id).Pose != EUnitPose::Idle && View->UnitAnimation(Unit.Id).bSkeletal ? 1 : 0; }
                if (End == EEnd::SkipMid && Frame % 7 == 0)
                {
                    View->SkipPresentation();
                    View->Tick(0.001f);
                    ExpectFiguresConverged(*this, Label + TEXT(" after a skip"), *View, TEXT("skip"));
                    ++Skips;
                }
            }
            else if (Active && Active->Side == ECombatSide::Party) { PartyStep(*View, LastId, Stage); }
        }
        // Let one-shot clips play out (the longest, Recover, is 2.2 s), or skip them, then compare with the committed state.
        if (End == EEnd::SkipMid) { View->SkipPresentation(); View->Tick(0.001f); }
        else { for (int32 Frame = 0; Frame < 4000 && View->IsPresenting(); ++Frame) { View->Tick(0.05f); } for (int32 Frame = 0; Frame < 100; ++Frame) { View->Tick(0.05f); } }
        TestTrue(*(Label + TEXT(": figures were rigged")), ExpectFiguresConverged(*this, *View, Label + TEXT(" at rest")) > 0);
        if (End == EEnd::Normal || End == EEnd::Fast) { TestTrue(*(Label + TEXT(": clips played while results showed")), Moving > 0); }
        if (End == EEnd::SkipMid) { TestTrue(*(Label + TEXT(": the skip was exercised")), Skips > 0); }
        // Fresh-process Resume of this state: the figures stand or lie as the checkpoint says, at the end of their clips.
        View->Save();
        AExpeditionBattleView* Resumed = MakeView(World, Path, EView::Rigged);
        if (TestNotNull(TEXT("Resumed view"), Resumed))
        {
            Resumed->Resume();
            Resumed->Tick(0.016f);
            ExpectFiguresConverged(*this, *Resumed, Label + TEXT(" resumed"));
            Resumed->Destroy();
        }
        View->Destroy();
    }
    ReleaseRigWorld(World);
    IFileManager::Get().DeleteDirectory(*Directory, false, true);
    return !HasAnyErrors();
}

namespace RigPoseTest
{
struct FFigureRun
{
    FCombatSnapshot Battle;
    FString Checkpoint;
    int32 Commands = 0;
    int32 Rigged = 0;
};

// Plays the same rules-only policy as BoardMetricLeavesStateUnchanged and reads back the checkpoint it wrote.
FFigureRun PlayFigures(FAutomationTestBase& Test, UWorld* World, const FString& Directory, EView Mode)
{
    FFigureRun Outcome;
    const FString Path = FPaths::Combine(Directory, FString::FromInt(static_cast<int32>(Mode)), TEXT("current.json"));
    AExpeditionBattleView* View = MakeView(World, Path, Mode);
    if (!Test.TestNotNull(TEXT("Battle view"), View)) { return Outcome; }
    View->NewRun();
    View->ChooseRoute(0);
    View->Tick(0.016f);
    Outcome.Rigged = View->SkeletalFigureCount();
    for (int32 Step = 0; Step < 80 && View->InCombat(); ++Step)
    {
        const int32 Id = View->Rules().ActiveId();
        const FCombatantState* Active = View->Rules().Find(Id);
        if (!Active) { break; }
        if (Active->Side == ECombatSide::Enemy) { View->Tick(1.0f); View->SkipPresentation(); continue; }
        const TArray<int32> Targets = View->Rules().LegalTargets(Id);
        const TArray<FIntPoint> Moves = View->Rules().LegalMoves(Id);
        if (!Targets.IsEmpty()) { View->Execute({ECombatCommandType::Attack, Id, {}, Targets[0]}); ++Outcome.Commands; }
        else if (!Moves.IsEmpty()) { View->Execute({ECombatCommandType::Move, Id, Moves[0]}); ++Outcome.Commands; }
        View->SkipPresentation();
        if (View->InCombat() && View->Rules().ActiveId() == Id) { View->Execute({ECombatCommandType::EndTurn, Id}); ++Outcome.Commands; View->SkipPresentation(); }
    }
    View->Save();
    Outcome.Battle = View->Rules().Snapshot();
    FFileHelper::LoadFileToString(Outcome.Checkpoint, *Path);
    // Every NewRun draws a fresh run identity; it is the one field that differs by design, so normalise it.
    const FString RunId = View->Progress().RunId;
    Test.TestFalse(TEXT("The run has an identity"), RunId.IsEmpty());
    if (!RunId.IsEmpty()) { Outcome.Checkpoint.ReplaceInline(*RunId, TEXT("<run-id>"), ESearchCase::CaseSensitive); }
    View->Destroy();
    return Outcome;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFigureKindKeepsStateTest, "Project.Expedition.Presentation.RiggedFiguresLeaveStateUnchanged",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FFigureKindKeepsStateTest::RunTest(const FString& Parameters)
{
    using namespace RigPoseTest;
    // The same seeded run under rigged, static and primitive figures: snapshots and checkpoint bytes are identical,
    // and the two fallbacks still build a figure for every unit.
    UWorld* World = NewRigWorld();
    if (!TestNotNull(TEXT("Transient world"), World)) { return false; }
    const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation/RiggedFigureState"), FGuid::NewGuid().ToString());
    const FFigureRun Rigged = PlayFigures(*this, World, Directory, EView::Rigged);
    TestTrue(TEXT("The run played commands"), Rigged.Commands > 0);
    TestFalse(TEXT("The run wrote a checkpoint"), Rigged.Checkpoint.IsEmpty());
    TestTrue(TEXT("The reference run used rigged figures (run import_dark_fantasy_theme.py)"), Rigged.Rigged > 0);
    for (const EView Mode : {EView::Static, EView::Primitive})
    {
        const FString Name = Mode == EView::Static ? TEXT("static") : TEXT("primitive");
        const FFigureRun Other = PlayFigures(*this, World, Directory, Mode);
        TestEqual(*(Name + TEXT(": the same number of commands")), Other.Commands, Rigged.Commands);
        TestEqual(*(Name + TEXT(": no rigged figure")), Other.Rigged, 0);
        TestTrue(*(Name + TEXT(": battle snapshot is identical")), FCombatSnapshot::StaticStruct()->CompareScriptStruct(&Rigged.Battle, &Other.Battle, 0));
        TestTrue(*(Name + TEXT(": checkpoint bytes are identical")), Other.Checkpoint == Rigged.Checkpoint);
    }
    // The fallbacks still build every figure, with a rig root on its cell.
    for (const EView Mode : {EView::Static, EView::Primitive})
    {
        const FString Name = Mode == EView::Static ? TEXT("static") : TEXT("primitive");
        AExpeditionBattleView* View = MakeView(World, FPaths::Combine(Directory, TEXT("fallback"), Name, TEXT("current.json")), Mode);
        if (!TestNotNull(TEXT("Battle view"), View)) { continue; }
        View->NewRun();
        View->ChooseRoute(0);
        View->Tick(0.016f);
        TestTrue(*(Name + TEXT(": only static figures keep the theme")), View->UsesTheme() == (Mode == EView::Static));
        TestEqual(*(Name + TEXT(": no rigged figure")), View->SkeletalFigureCount(), 0);
        for (const FCombatantState& Unit : View->Rules().Snapshot().Encounter.Combatants)
        {
            USceneComponent* RigRoot = View->UnitRigRoot(Unit.Id);
            if (!TestNotNull(*FString::Printf(TEXT("%s: %s has a figure"), *Name, *Unit.Name), RigRoot)) { continue; }
            const double Height = FigureHeight(*View, RigRoot);
            TestTrue(*FString::Printf(TEXT("%s: %s stands 175 to 185 cm (measured %.1f)"), *Name, *Unit.Name, Height), Height >= 175.0 && Height <= 185.0);
            TestFalse(*FString::Printf(TEXT("%s: %s has no animation state"), *Name, *Unit.Name), View->UnitAnimation(Unit.Id).bSkeletal);
        }
    }
    ReleaseRigWorld(World);
    IFileManager::Get().DeleteDirectory(*Directory, false, true);
    return !HasAnyErrors();
}
#endif
