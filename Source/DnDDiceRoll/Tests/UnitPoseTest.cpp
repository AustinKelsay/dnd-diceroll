#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "RenderingThread.h"
#include "UObject/GarbageCollection.h"
#include "Engine/World.h"
#include "Components/PrimitiveComponent.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Party/BuildCatalog.h"
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
struct FPoseScene
{
    FCombatSnapshot Before, After;
    int32 Hero = INDEX_NONE, Ally = INDEX_NONE, Foe = INDEX_NONE;
    FCombatantState* Unit(FCombatSnapshot& Snapshot, int32 Id)
    { return Snapshot.Encounter.Combatants.FindByPredicate([Id](const FCombatantState& U) { return U.Id == Id; }); }
};

bool MakeScene(FPoseScene& Scene, int32 FoeX)
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
    // The hero's basic Attack is a weapon attack unless a scene makes it a caster (a wizard's Attack is Fire Bolt).
    Scene.Unit(Scene.Before, Scene.Hero)->AdventurerRole = EAdventurerRole::Fighter;
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
void RunScene(FAutomationTestBase& Test, const TCHAR* Name, const FPoseScene& Scene, const FCombatResult& Outcome, TCheck Check)
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
    // Basic spell attacks cast at any distance; weapon attacks keep Melee and Ranged.
    using ExpeditionPose::IsSpellAttack;
    const auto Basic = [](EAdventurerRole Role, bool bFar)
    { return ExpeditionPose::ForCommand(ECombatCommandType::Attack, bFar, IsSpellAttack(ECombatCommandType::Attack, Role, bFar)); };
    ExpectPose(*this, TEXT("wizard Fire Bolt, adjacent"), Basic(EAdventurerRole::Wizard, false), EUnitPose::Cast);
    ExpectPose(*this, TEXT("wizard Fire Bolt, distant"), Basic(EAdventurerRole::Wizard, true), EUnitPose::Cast);
    ExpectPose(*this, TEXT("cleric Sacred Flame, distant"), Basic(EAdventurerRole::Cleric, true), EUnitPose::Cast);
    ExpectPose(*this, TEXT("cleric up close (a weapon)"), Basic(EAdventurerRole::Cleric, false), EUnitPose::Melee);
    ExpectPose(*this, TEXT("fighter, adjacent"), Basic(EAdventurerRole::Fighter, false), EUnitPose::Melee);
    ExpectPose(*this, TEXT("rogue, distant"), Basic(EAdventurerRole::Rogue, true), EUnitPose::Ranged);
    ExpectPose(*this, TEXT("an enemy, distant"), Basic(EAdventurerRole::None, true), EUnitPose::Ranged);
    TestFalse(TEXT("Precision Strike is never a spell"), IsSpellAttack(ECombatCommandType::PrecisionStrike, EAdventurerRole::Wizard, true));
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
        FPoseScene Scene;
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
        FPoseScene Scene;
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
        FPoseScene Scene;
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
        FPoseScene Scene;
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
        FPoseScene Scene;
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
        FPoseScene Scene;
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
        FPoseScene Scene;
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
    {
        // A spell whose normalized effect is Area (Sculpted Burst, the evocation wizard's Special2) casts; a weapon ability
        // with the same command (the berserker's Frenzy, a Strike) still swings or shoots.
        const FAbilityFacts* Burst = nullptr;
        const FBuildDefinition* Evocation = FBuildCatalog::Find(TEXT("wizard.evocation"));
        if (Evocation) { Burst = Evocation->Abilities.FindByPredicate([](const FAbilityFacts& A) { return A.Command == ECombatCommandType::Special2; }); }
        if (!TestTrue(TEXT("The catalog has Sculpted Burst"), Burst && Burst->Name == TEXT("Sculpted Burst") && Burst->Effect == EAbilityEffect::Area)) { return false; }
        TestTrue(TEXT("Sculpted Burst is a spell"), Burst->bSpell);
        const FBuildDefinition* Berserker = FBuildCatalog::Find(TEXT("barbarian.berserker"));
        const FAbilityFacts* Frenzy = Berserker ? Berserker->Abilities.FindByPredicate([](const FAbilityFacts& A) { return A.Command == ECombatCommandType::Special2; }) : nullptr;
        if (TestNotNull(TEXT("The catalog has Frenzy"), Frenzy)) { TestFalse(TEXT("Frenzy is a weapon ability"), Frenzy->bSpell); }
        ExpectPose(*this, TEXT("a spell Area effect up close"), ExpeditionPose::ForEffect(EAbilityEffect::Area, false, true), EUnitPose::Cast);
        ExpectPose(*this, TEXT("a weapon Area effect at range"), ExpeditionPose::ForEffect(EAbilityEffect::Area, true, false), EUnitPose::Ranged);
        ExpectPose(*this, TEXT("a spell swap still has no clip"), ExpeditionPose::ForEffect(EAbilityEffect::Swap, false, true), EUnitPose::Idle);

        FPoseScene Scene;
        if (!TestTrue(TEXT("Scene builds"), MakeScene(Scene, 7))) { return false; }
        FCombatantState* Hero = Scene.Unit(Scene.Before, Scene.Hero);
        FCombatantState Wizard;
        if (!TestTrue(TEXT("The hero becomes the evocation wizard"), FBuildCatalog::Create(TEXT("wizard.evocation"), Scene.Hero, Hero->Name, Wizard))) { return false; }
        Wizard.Cell = Hero->Cell; Wizard.Side = Hero->Side; Wizard.MaxHP = Wizard.HP = Hero->MaxHP;
        *Hero = Wizard;
        Scene.After = Scene.Before;
        Scene.Unit(Scene.After, Scene.Foe)->HP = Scene.Unit(Scene.Before, Scene.Foe)->HP - 3;
        FCombatResult Special = MakeResult(ECombatCommandType::Special2, Scene.Hero, Scene.Foe);
        Special.bHit = true;
        Special.Damage = 3;
        FTargetEffect Effect;
        Effect.TargetId = Scene.Foe;
        Effect.Effect = EAbilityEffect::Area;
        Effect.Amount = 3;
        Effect.Cell = Scene.Unit(Scene.After, Scene.Foe)->Cell;
        Special.Effects.Add(Effect);
        RunScene(*this, TEXT("Sculpted Burst"), Scene, Special, [&](const FString& Label, const FSeen& Seen)
        {
            TestTrue(Label + TEXT(": the wizard casts"), Seen.Saw(Scene.Hero, EUnitPose::Cast));
            TestFalse(Label + TEXT(": not a weapon clip"), Seen.Saw(Scene.Hero, EUnitPose::Ranged) || Seen.Saw(Scene.Hero, EUnitPose::Melee));
            TestTrue(Label + TEXT(": the target flinches"), Seen.Saw(Scene.Foe, EUnitPose::Hit));
        });
    }
    // The wizard's basic Attack is Fire Bolt: it casts adjacent and at range, never swings or shoots.
    for (const int32 FoeX : {3, 7})
    {
        FPoseScene Scene;
        if (!TestTrue(TEXT("Scene builds"), MakeScene(Scene, FoeX))) { return false; }
        Scene.Unit(Scene.Before, Scene.Hero)->AdventurerRole = EAdventurerRole::Wizard;
        Scene.After = Scene.Before;
        Scene.Unit(Scene.After, Scene.Foe)->HP = Scene.Unit(Scene.Before, Scene.Foe)->HP - 4;
        FCombatResult Bolt = MakeResult(ECombatCommandType::Attack, Scene.Hero, Scene.Foe);
        Bolt.bHit = true;
        Bolt.Damage = 4;
        RunScene(*this, FoeX == 3 ? TEXT("Fire Bolt, adjacent") : TEXT("Fire Bolt, distant"), Scene, Bolt, [&](const FString& Label, const FSeen& Seen)
        {
            TestTrue(Label + TEXT(": the wizard casts"), Seen.Saw(Scene.Hero, EUnitPose::Cast));
            TestFalse(Label + TEXT(": not a weapon clip"), Seen.Saw(Scene.Hero, EUnitPose::Ranged) || Seen.Saw(Scene.Hero, EUnitPose::Melee));
            TestTrue(Label + TEXT(": the target flinches"), Seen.Saw(Scene.Foe, EUnitPose::Hit));
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
    int32 Falls = 0, Flinches = 0, Walks = 0, SpellAttacks = 0;
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
                const EUnitPose Expected = ExpeditionPose::ForCommand(Outcome.Type, bFar, ExpeditionPose::IsSpellAttack(Outcome.Type, Actor->AdventurerRole, bFar));
                TestTrue(Label + TEXT(": the attacker shows the clip for its command"), Seen.Saw(Actor->Id, Expected));
                if (Expected == EUnitPose::Cast && Outcome.Type == ECombatCommandType::Attack) { ++SpellAttacks; }
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
    // One seeded encounter is one sample of dice: every hit in it may down or kill. The Hit clip is covered
    // deterministically by PlaybackChoosesPoses (a crafted non-fatal hit), so this count is information only.
    AddInfo(FString::Printf(TEXT("Real encounters: %d walks, %d falls, %d non-fatal hits, %d spell attacks"), Walks, Falls, Flinches, SpellAttacks));
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
    // The four original classes are rigged (#147 fighter and wizard, #150 batch 1 rogue and cleric); the rest keep static figures until their batch.
    if (Unit.Side == ECombatSide::Party)
    {
        return Unit.AdventurerRole == EAdventurerRole::Fighter || Unit.AdventurerRole == EAdventurerRole::Rogue ||
            Unit.AdventurerRole == EAdventurerRole::Cleric || Unit.AdventurerRole == EAdventurerRole::Wizard;
    }
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

// A rigged figure that is Downed or Dead in the committed state: its held end pose is the interesting thing to resume.
bool HasFallenRig(const AExpeditionBattleView& View)
{
    for (const FCombatantState& Unit : View.Rules().Snapshot().Encounter.Combatants)
    { if (Unit.Life != ECombatLife::Conscious && View.UnitAnimation(Unit.Id).bSkeletal) { return true; } }
    return false;
}

enum class EView : uint8 { Rigged, Static, Primitive };

// Each view lives in its own transient world and is released with it: destroying an actor in a context-less test
// world logs "World has no context", which the zero-warning gate rejects.
AExpeditionBattleView* MakeView(UWorld* /*Shared*/, const FString& Path, EView Mode)
{
    UWorld* World = NewRigWorld();
    if (!World) { return nullptr; }
    AExpeditionBattleView* View = World->SpawnActor<AExpeditionBattleView>();
    if (!View) { ReleaseRigWorld(World); return nullptr; }
    View->ConfigureStorage(Path);
    View->DispatchBeginPlay();
    if (Mode == EView::Static) { View->UseStaticFigures(); }
    else if (Mode == EView::Primitive) { View->UsePrimitives(); }
    return View;
}

void ReleaseView(AExpeditionBattleView* View)
{
    if (View) { ReleaseRigWorld(View->GetWorld()); }
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
                // Per-figure render settings reach the skin (it draws through Rig.Skin, not Rig.Parts): the silhouette's custom depth
                // and stencil by side, and a tintable material per skin slot for the hit flash and rim.
                TestEqual(*FString::Printf(TEXT("%s's skin carries its side's silhouette stencil"), *Unit.Name),
                    View->FigureStencil(Unit.Id), Unit.Side == ECombatSide::Enemy ? AExpeditionBattleView::EnemyStencil : AExpeditionBattleView::PartyStencil);
                TArray<FLinearColor> SkinColors;
                FLinearColor SkinRing;
                TestTrue(*FString::Printf(TEXT("%s's skin has a tintable slot"), *Unit.Name), View->UnitShownColors(Unit.Id, SkinColors, SkinRing) && SkinColors.Num() == 1);
                const double Height = FigureHeight(*View, RigRoot);
                TestTrue(FString::Printf(TEXT("%s stands 175 to 185 cm (measured %.1f)"), *Unit.Name, Height), Height >= 175.0 && Height <= 185.0);
            }
            TestTrue(TEXT("The default party has rigged characters"), Expected > 0);
            // Batch 1 (#150): the rogue and the cleric are rigged, never a static or primitive fallback, once the theme is imported.
            TSet<EAdventurerRole> RiggedRoles;
            for (const FCombatantState& Unit : View->Rules().Snapshot().Encounter.Combatants)
            { if (Unit.Side == ECombatSide::Party && View->UnitAnimation(Unit.Id).bSkeletal) { RiggedRoles.Add(Unit.AdventurerRole); } }
            for (const EAdventurerRole Role : {EAdventurerRole::Fighter, EAdventurerRole::Rogue, EAdventurerRole::Cleric, EAdventurerRole::Wizard})
            { TestTrue(FString::Printf(TEXT("The default party's %s is rigged"), *UDiceRollTheme::ClassKey(Role).ToString()), RiggedRoles.Contains(Role)); }
            TestEqual(TEXT("Exactly the expected units are rigged"), View->SkeletalFigureCount(), Expected);
            ExpectFiguresConverged(*this, *View, TEXT("a fresh board"));
            ReleaseView(View);
        }
    }

    // Playback drives the rig; at every speed, and when skipped mid-animation, the figures converge on the committed state.
    int32 FallenResumed = 0;
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
            // Stop mid-encounter once a rigged figure has fallen, so Resume below has a held Downed or Death pose to restore.
            if (!View->IsPresenting() && Moving > 0 && (End != EEnd::SkipMid || Skips > 0) && HasFallenRig(*View)) { break; }
            const FCombatantState* Active = View->Rules().Find(View->Rules().ActiveId());
            if (View->IsPresenting())
            {
                for (const FCombatantState& Unit : View->Rules().Snapshot().Encounter.Combatants)
                { Moving += View->UnitAnimation(Unit.Id).Pose != EUnitPose::Idle && View->UnitAnimation(Unit.Id).bSkeletal ? 1 : 0; }
                if (End == EEnd::SkipMid && Frame % 7 == 0)
                {
                    View->SkipPresentation();
                    View->Tick(0.001f);
                    ExpectFiguresConverged(*this, *View, Label + TEXT(" after a skip"));
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
        // Resume of this state: a fresh world and a fresh view read the checkpoint file from disk through the normal Resume
        // path, the same code a new process runs (it is not a separate process). The figures must stand or lie as the
        // checkpoint says, at the end of their clips.
        View->Save();
        TestTrue(*(Label + TEXT(": the checkpoint file was written")), IFileManager::Get().FileExists(*Path));
        const int32 OriginalRigs = View->SkeletalFigureCount();
        FExpeditionSnapshot Original;
        Original.Progress = View->Progress();
        Original.Battle = View->Rules().Snapshot();
        const bool bFallen = HasFallenRig(*View);
        AExpeditionBattleView* Resumed = MakeView(World, Path, EView::Rigged);
        if (TestNotNull(TEXT("Resumed view"), Resumed))
        {
            Resumed->Resume();
            Resumed->Tick(0.016f);
            TestTrue(*(Label + TEXT(": the resumed view restored a run")), Resumed->HasRun() && (Resumed->InCombat() || Resumed->Progress().Phase != EExpeditionPhase::Combat));
            if (Resumed->HasRun())
            {
                FExpeditionSnapshot Restored;
                Restored.Progress = Resumed->Progress();
                Restored.Battle = Resumed->Rules().Snapshot();
                TestTrue(*(Label + TEXT(": the resumed snapshot equals the saved one")), ExpeditionTestSupport::SameRun(Original, Restored));
            }
            int32 ExpectedRigs = 0;
            for (const FCombatantState& Unit : Resumed->Rules().Snapshot().Encounter.Combatants) { ExpectedRigs += ExpectsRig(Unit) ? 1 : 0; }
            const int32 Inspected = ExpectFiguresConverged(*this, *Resumed, Label + TEXT(" resumed"));
            TestTrue(*(Label + TEXT(": the resumed view has rigged figures")), Inspected > 0);
            TestEqual(*(Label + TEXT(": the resumed rig count matches the original view")), Inspected, OriginalRigs);
            if (Resumed->InCombat()) { TestEqual(*(Label + TEXT(": the resumed rig count matches the rigged units")), Inspected, ExpectedRigs); }
            if (bFallen)
            {
                ++FallenResumed;
                TestTrue(*(Label + TEXT(": a fallen rigged figure was resumed")), HasFallenRig(*Resumed));
            }
            ReleaseView(Resumed);
        }
        ReleaseView(View);
    }
    // The encounter may not drop a rigged unit at every ending, so this count is information only; RiggedResumeHoldsFallenPoses
    // resumes a downed rigged hero and a dead rigged raider deterministically.
    AddInfo(FString::Printf(TEXT("%d of 4 endings resumed with a fallen rigged figure"), FallenResumed));
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
    ReleaseView(View);
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
        ReleaseView(View);
    }
    ReleaseRigWorld(World);
    IFileManager::Get().DeleteDirectory(*Directory, false, true);
    return !HasAnyErrors();
}
namespace RigPoseTest
{
// Plays real rules commands until a unit Find picks has fallen while the encounter goes on. bFight false: the party only
// ends its turns, so the enemies down someone. bFight true: the party attacks, choosing targets Find would accept first, so
// that unit falls before the last enemy ends the encounter. Returns that unit's id, or INDEX_NONE if the encounter ended first.
int32 PlayUntilFallen(FExpeditionRun& Run, bool bFight, TFunctionRef<bool(const FCombatantState&)> Find)
{
    auto Fallen = [&Run, &Find]() -> int32
    {
        if (Run.Progress().Phase != EExpeditionPhase::Combat) { return INDEX_NONE; }
        for (const FCombatantState& Unit : Run.Battle().Snapshot().Encounter.Combatants) { if (Find(Unit)) { return Unit.Id; } }
        return INDEX_NONE;
    };
    for (int32 Turn = 0; Turn < 400 && Run.Progress().Phase == EExpeditionPhase::Combat; ++Turn)
    {
        const FCombatBattle& Battle = Run.Battle();
        const int32 Id = Battle.ActiveId();
        const FCombatantState* Active = Battle.Find(Id);
        if (!Active) { break; }
        if (Active->Side == ECombatSide::Enemy) { Run.PlayEnemyTurn(); }
        else
        {
            if (bFight)
            {
                TArray<int32> Targets = Battle.LegalTargets(Id);
                const int32* Preferred = Targets.FindByPredicate([&Battle, &Find](int32 Target)
                {
                    const FCombatantState* Unit = Battle.Find(Target);
                    if (!Unit) { return false; }
                    FCombatantState Fallen = *Unit; // Would Find accept this target once it falls?
                    Fallen.Life = ECombatLife::Dead;
                    return Find(Fallen);
                });
                if (!Targets.IsEmpty()) { Run.Submit({ECombatCommandType::Attack, Id, {}, Preferred ? *Preferred : Targets[0]}); }
                else if (const TArray<FIntPoint> Moves = Battle.LegalMoves(Id); !Moves.IsEmpty()) { Run.Submit({ECombatCommandType::Move, Id, Moves.Last()}); }
            }
            if (Run.Progress().Phase == EExpeditionPhase::Combat && Run.Battle().ActiveId() == Id) { Run.Submit({ECombatCommandType::EndTurn, Id}); }
        }
        if (const int32 Found = Fallen(); Found != INDEX_NONE) { return Found; }
    }
    return INDEX_NONE;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRiggedResumeFallenTest, "Project.Expedition.Presentation.RiggedResumeHoldsFallenPoses",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRiggedResumeFallenTest::RunTest(const FString& Parameters)
{
    using namespace RigPoseTest;
    // A checkpoint written mid-encounter with a downed rigged hero, and one with a dead rigged raider, resume through the normal
    // Resume path in a fresh world and view: the fallen figure lies at the end of its Downed or Death clip at once.
    // The states come from real commands on fixed seeds, so the precondition is checked, never assumed.
    const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation/RiggedResumeFallen"), FGuid::NewGuid().ToString());
    // Raiders (the rigged enemy) guard only the dangerous route; the guarded route's enemies are scouts.
    struct FCase { const TCHAR* Name; bool bFight; bool bDangerous; ECombatSide Side; ECombatLife Life; EUnitPose Pose; };
    for (const FCase& Case : {FCase{TEXT("downed hero"), false, false, ECombatSide::Party, ECombatLife::Downed, EUnitPose::Downed},
                              FCase{TEXT("dead raider"), true, true, ECombatSide::Enemy, ECombatLife::Dead, EUnitPose::Death}})
    {
        FExpeditionSnapshot Checkpoint;
        int32 FallenId = INDEX_NONE, Seed = 0;
        for (const int32 Candidate : {7, 11, 13, 21, 29, 37, 43, 53})
        {
            FExpeditionRun Attempt;
            FString Error;
            if (!Attempt.NewRun(Candidate, Error, TEXT("rig-resume-fallen"))) { continue; }
            const auto* Offer = Attempt.Progress().Offers.FindByPredicate([&Case](const auto& Route) { return Route.bDangerous == Case.bDangerous; });
            if (!Offer || !Attempt.ChooseRoute(Offer->Id, Error)) { continue; }
            FallenId = PlayUntilFallen(Attempt, Case.bFight, [&Case](const FCombatantState& Unit)
                { return Unit.Side == Case.Side && Unit.Life == Case.Life && ExpectsRig(Unit); });
            if (FallenId != INDEX_NONE) { Checkpoint = Attempt.Snapshot(); Seed = Candidate; break; }
        }
        const FString Label = Case.Name;
        if (!TestTrue(*(Label + TEXT(": a fixed seed reaches a mid-encounter checkpoint with this rigged figure fallen")), FallenId != INDEX_NONE)) { continue; }
        AddInfo(FString::Printf(TEXT("%s: seed %d, unit %d"), Case.Name, Seed, FallenId));
        const FString Path = FPaths::Combine(Directory, FString::FromInt(static_cast<int32>(Case.Life)), TEXT("current.json"));
        FExpeditionSaveData Data;
        Data.Current = Checkpoint;
        FString Error;
        if (!TestTrue(*(Label + TEXT(": the checkpoint writes")), FExpeditionSave::Write(Path, Data, Error))) { AddError(Error); continue; }
        AExpeditionBattleView* View = MakeView(nullptr, Path, EView::Rigged);
        if (!TestNotNull(*(Label + TEXT(": resumed view")), View)) { continue; }
        View->Resume();
        View->Tick(0.016f);
        TestTrue(*(Label + TEXT(": the resumed view is mid-encounter")), View->HasRun() && View->InCombat());
        FExpeditionSnapshot Restored;
        Restored.Progress = View->Progress();
        Restored.Battle = View->Rules().Snapshot();
        TestTrue(*(Label + TEXT(": the resumed snapshot equals the checkpoint")), ExpeditionTestSupport::SameRun(Data.Current, Restored));
        const FCombatantState* Unit = View->Rules().Find(FallenId);
        if (TestNotNull(*(Label + TEXT(": the fallen unit is in the resumed encounter")), Unit))
        {
            TestTrue(*(Label + TEXT(": it is still fallen")), Unit->Life == Case.Life);
            const FUnitAnimationState State = View->UnitAnimation(FallenId);
            TestTrue(*(Label + TEXT(": it is a rigged figure")), State.bSkeletal);
            ExpectPose(*this, Label + TEXT(": its held clip"), State.Pose, Case.Pose);
            TestTrue(*(Label + TEXT(": the clip is bound")), State.Clip.Contains(ExpeditionPose::ClipName(Case.Pose)));
            TestTrue(*FString::Printf(TEXT("%s: it holds the clip's last frame (%.3f of %.3f s)"), *Label, State.Time, State.Length),
                State.Length > 0.0f && FMath::IsNearlyEqual(State.Time, State.Length, 1.0e-3f));
        }
        ExpectFiguresConverged(*this, *View, Label + TEXT(" resumed"));
        ReleaseView(View);
    }
    IFileManager::Get().DeleteDirectory(*Directory, false, true);
    return !HasAnyErrors();
}

namespace RigPoseTest
{
/** The ordered clips each unit showed while a scene played (a pose is recorded when it changes). */
using FClipOrder = TMap<int32, TArray<EUnitPose>>;

// Plays a scene at a speed, recording each unit's clip order; Seconds is the simulated time until playback finished, and
// bDone is false if it never finished within the frame cap.
FClipOrder PlayRecordingOrder(const FPoseScene& Scene, const FCombatResult& Outcome, float Speed, float& Seconds, bool& bDone)
{
    FClipOrder Order;
    FExpeditionPlayback Show;
    Show.Reset(Scene.Before);
    Show.Speed = Speed;
    Show.Enqueue({Outcome}, Scene.After);
    auto Note = [&Show, &Scene, &Order]()
    {
        for (const FCombatantState& Unit : Scene.After.Encounter.Combatants)
        {
            const FUnitDisplay* Display = Show.Unit(Unit.Id);
            if (!Display) { continue; }
            TArray<EUnitPose>& Seq = Order.FindOrAdd(Unit.Id);
            if (Seq.IsEmpty() || Seq.Last() != Display->Pose) { Seq.Add(Display->Pose); }
        }
    };
    Note();
    Seconds = 0.0f;
    for (int32 Frame = 0; Frame < 8000 && Show.IsBusy(); ++Frame) { Show.Tick(0.01f); Seconds += 0.01f; Note(); }
    bDone = !Show.IsBusy();
    return Order;
}

FString OrderText(const TArray<EUnitPose>& Seq)
{
    TArray<FString> Names;
    for (const EUnitPose Pose : Seq) { Names.Add(ExpeditionPose::ClipName(Pose)); }
    return FString::Join(Names, TEXT(" > "));
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPlaybackSpeedOrderTest, "Project.Expedition.Presentation.PlaybackSpeedKeepsClipOrder",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FPlaybackSpeedOrderTest::RunTest(const FString& Parameters)
{
    using namespace RigPoseTest;
    // Fast speed shows the same clips in the same order as normal speed, only sooner: speed scales time, never the choice.
    struct FCaseScene { FString Name; FPoseScene Scene; FCombatResult Outcome; };
    TArray<FCaseScene> Cases;
    {
        FCaseScene Case{TEXT("move")};
        if (!TestTrue(TEXT("Scene builds"), MakeScene(Case.Scene, 7))) { return false; }
        Case.Outcome = MakeResult(ECombatCommandType::Move, Case.Scene.Hero);
        Case.Outcome.Path = {FIntPoint(2, 3), FIntPoint(3, 3), FIntPoint(4, 3)};
        Case.Scene.Unit(Case.Scene.After, Case.Scene.Hero)->Cell = FIntPoint(4, 3);
        Cases.Add(Case);
    }
    {
        FCaseScene Case{TEXT("melee hit")};
        if (!TestTrue(TEXT("Scene builds"), MakeScene(Case.Scene, 3))) { return false; }
        Case.Outcome = MakeResult(ECombatCommandType::Attack, Case.Scene.Hero, Case.Scene.Foe);
        Case.Outcome.bHit = true;
        Case.Outcome.Damage = 3;
        Case.Scene.Unit(Case.Scene.After, Case.Scene.Foe)->HP -= 3;
        Cases.Add(Case);
    }
    {
        FCaseScene Case{TEXT("Fire Bolt kills")};
        if (!TestTrue(TEXT("Scene builds"), MakeScene(Case.Scene, 7))) { return false; }
        Case.Scene.Unit(Case.Scene.Before, Case.Scene.Hero)->AdventurerRole = EAdventurerRole::Wizard;
        Case.Scene.After = Case.Scene.Before;
        Case.Outcome = MakeResult(ECombatCommandType::Attack, Case.Scene.Hero, Case.Scene.Foe);
        Case.Outcome.bHit = true;
        Case.Outcome.Damage = 99;
        Case.Outcome.Died = {Case.Scene.Foe};
        Case.Scene.Unit(Case.Scene.After, Case.Scene.Foe)->HP = 0;
        Case.Scene.Unit(Case.Scene.After, Case.Scene.Foe)->Life = ECombatLife::Dead;
        Cases.Add(Case);
    }
    {
        FCaseScene Case{TEXT("a hero goes down")};
        if (!TestTrue(TEXT("Scene builds"), MakeScene(Case.Scene, 3))) { return false; }
        Case.Outcome = MakeResult(ECombatCommandType::Attack, Case.Scene.Foe, Case.Scene.Hero);
        Case.Outcome.bHit = true;
        Case.Outcome.Damage = 99;
        Case.Outcome.Downed = {Case.Scene.Hero};
        Case.Scene.Unit(Case.Scene.After, Case.Scene.Hero)->HP = 0;
        Case.Scene.Unit(Case.Scene.After, Case.Scene.Hero)->Life = ECombatLife::Downed;
        Cases.Add(Case);
    }
    {
        FCaseScene Case{TEXT("recovery")};
        if (!TestTrue(TEXT("Scene builds"), MakeScene(Case.Scene, 7))) { return false; }
        Case.Scene.Unit(Case.Scene.Before, Case.Scene.Ally)->Life = ECombatLife::Downed;
        Case.Scene.Unit(Case.Scene.Before, Case.Scene.Ally)->HP = 0;
        Case.Scene.After = Case.Scene.Before;
        Case.Scene.Unit(Case.Scene.After, Case.Scene.Ally)->Life = ECombatLife::Conscious;
        Case.Scene.Unit(Case.Scene.After, Case.Scene.Ally)->HP = 5;
        Case.Outcome = MakeResult(ECombatCommandType::CureWounds, Case.Scene.Hero, Case.Scene.Ally);
        Case.Outcome.Healing = 5;
        Cases.Add(Case);
    }
    for (const FCaseScene& Case : Cases)
    {
        float NormalSeconds = 0.0f, FastSeconds = 0.0f;
        bool bNormalDone = false, bFastDone = false;
        const FClipOrder Normal = PlayRecordingOrder(Case.Scene, Case.Outcome, 1.0f, NormalSeconds, bNormalDone);
        const FClipOrder Fast = PlayRecordingOrder(Case.Scene, Case.Outcome, 2.2f, FastSeconds, bFastDone);
        TestTrue(FString::Printf(TEXT("%s: playback finishes at both speeds"), *Case.Name), bNormalDone && bFastDone);
        // Fast speed scales playback time: it finishes in well under the normal time (the speed is 2.2; allow for fixed steps).
        TestTrue(FString::Printf(TEXT("%s: fast speed finishes sooner (%.2f s against %.2f s)"), *Case.Name, FastSeconds, NormalSeconds),
            NormalSeconds > 0.0f && FastSeconds <= NormalSeconds / 1.8f);
        bool bAnyClip = false;
        for (const TPair<int32, TArray<EUnitPose>>& Entry : Normal)
        {
            const TArray<EUnitPose>* Other = Fast.Find(Entry.Key);
            bAnyClip |= Entry.Value.Num() > 1;
            TestTrue(FString::Printf(TEXT("%s: unit %d shows the same clips in the same order at both speeds (normal %s; fast %s)"), *Case.Name, Entry.Key,
                *OrderText(Entry.Value), Other ? *OrderText(*Other) : TEXT("none")), Other && *Other == Entry.Value);
        }
        TestTrue(FString::Printf(TEXT("%s: some unit changed clip"), *Case.Name), bAnyClip);
    }
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRiggedRecoverTest, "Project.Expedition.Presentation.RiggedHeroRecoverPlaysClip",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRiggedRecoverTest::RunTest(const FString& Parameters)
{
    using namespace RigPoseTest;
    // A downed rigged hero rescued by an ally plays Recover on its skinned mesh, then stands at Idle. Real commands on fixed seeds
    // reach the moment just before an ally's Rescue of a downed rigged hero; that checkpoint resumes in a fresh view, which
    // executes the Rescue at normal speed.
    FExpeditionSnapshot Checkpoint;
    FCombatCommand Rescue;
    int32 HeroId = INDEX_NONE, Seed = 0;
    for (const int32 Candidate : {7, 11, 13, 21, 29, 37, 43, 53, 61, 71})
    {
        FExpeditionRun Run;
        FString Error;
        if (!Run.NewRun(Candidate, Error, TEXT("rig-recover"))) { continue; }
        const auto* Offer = Run.Progress().Offers.FindByPredicate([](const auto& Route) { return !Route.bDangerous; });
        if (!Offer || !Run.ChooseRoute(Offer->Id, Error)) { continue; }
        const int32 Hero = PlayUntilFallen(Run, false, [](const FCombatantState& Unit)
            { return Unit.Side == ECombatSide::Party && Unit.Life == ECombatLife::Downed && ExpectsRig(Unit); });
        if (Hero == INDEX_NONE) { continue; }
        // Allies walk to the downed hero and rescue it; enemies take their turns.
        for (int32 Turn = 0; Turn < 200 && Run.Progress().Phase == EExpeditionPhase::Combat && HeroId == INDEX_NONE; ++Turn)
        {
            const FCombatBattle& Battle = Run.Battle();
            const FCombatantState* Down = Battle.Find(Hero);
            if (!Down || Down->Life != ECombatLife::Downed) { break; }
            const int32 Id = Battle.ActiveId();
            const FCombatantState* Active = Battle.Find(Id);
            if (!Active) { break; }
            if (Active->Side == ECombatSide::Enemy) { Run.PlayEnemyTurn(); continue; }
            if (Active->Life == ECombatLife::Conscious && Id != Hero)
            {
                if (Battle.LegalTargets(Id, ECombatCommandType::Rescue).Contains(Hero))
                {
                    Checkpoint = Run.Snapshot();
                    Rescue = {ECombatCommandType::Rescue, Id, {}, Hero};
                    HeroId = Hero;
                    Seed = Candidate;
                    break;
                }
                TArray<FIntPoint> Moves = Battle.LegalMoves(Id);
                const FIntPoint Goal = Down->Cell;
                Moves.Sort([Goal](FIntPoint A, FIntPoint B) { return ExpeditionTestSupport::Distance(A, Goal) < ExpeditionTestSupport::Distance(B, Goal); });
                if (!Moves.IsEmpty() && ExpeditionTestSupport::Distance(Moves[0], Goal) < ExpeditionTestSupport::Distance(Active->Cell, Goal))
                {
                    Run.Submit({ECombatCommandType::Move, Id, Moves[0]});
                    if (Run.Battle().LegalTargets(Id, ECombatCommandType::Rescue).Contains(Hero))
                    {
                        Checkpoint = Run.Snapshot();
                        Rescue = {ECombatCommandType::Rescue, Id, {}, Hero};
                        HeroId = Hero;
                        Seed = Candidate;
                        break;
                    }
                }
            }
            if (Run.Progress().Phase == EExpeditionPhase::Combat && Run.Battle().ActiveId() == Id) { Run.Submit({ECombatCommandType::EndTurn, Id}); }
        }
        if (HeroId != INDEX_NONE) { break; }
    }
    if (!TestTrue(TEXT("A fixed seed reaches an ally's Rescue of a downed rigged hero"), HeroId != INDEX_NONE)) { return false; }
    AddInfo(FString::Printf(TEXT("Rescue: seed %d, hero %d, rescuer %d"), Seed, HeroId, Rescue.ActorId));
    const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation/RiggedRecover"), FGuid::NewGuid().ToString());
    const FString Path = FPaths::Combine(Directory, TEXT("current.json"));
    FExpeditionSaveData Data;
    Data.Current = Checkpoint;
    FString Error;
    if (!TestTrue(TEXT("The checkpoint writes"), FExpeditionSave::Write(Path, Data, Error))) { AddError(Error); return false; }
    AExpeditionBattleView* View = MakeView(nullptr, Path, EView::Rigged);
    if (!TestNotNull(TEXT("Resumed view"), View)) { return false; }
    View->Resume();
    View->Tick(0.016f);
    TestTrue(TEXT("The resumed view is mid-encounter"), View->HasRun() && View->InCombat());
    TestTrue(TEXT("The downed hero is a rigged figure"), View->UnitAnimation(HeroId).bSkeletal);
    ExpectPose(*this, TEXT("Before the rescue the hero lies Downed"), View->UnitAnimation(HeroId).Pose, EUnitPose::Downed);
    View->Execute(Rescue);
    bool bRecovered = false, bClip = false;
    for (int32 Frame = 0; Frame < 600 && !bRecovered; ++Frame)
    {
        View->Tick(0.02f);
        const FUnitAnimationState State = View->UnitAnimation(HeroId);
        if (State.Pose == EUnitPose::Recover)
        {
            bRecovered = true;
            bClip = State.bSkeletal && State.Clip.Contains(ExpeditionPose::ClipName(EUnitPose::Recover));
        }
    }
    TestTrue(TEXT("The rescued rigged hero plays Recover"), bRecovered);
    TestTrue(TEXT("Its skinned mesh has the Recover clip bound"), bClip);
    const FCombatantState* Hero = View->Rules().Find(HeroId);
    TestTrue(TEXT("The rescue committed: the hero is conscious"), Hero && Hero->Life == ECombatLife::Conscious);
    // Once Recover has played out, the hero stands at Idle.
    for (int32 Frame = 0; Frame < 4000 && View->IsPresenting(); ++Frame) { View->Tick(0.05f); }
    for (int32 Frame = 0; Frame < 100; ++Frame) { View->Tick(0.05f); }
    ExpectPose(*this, TEXT("After Recover the hero stands"), View->UnitAnimation(HeroId).Pose, EUnitPose::Idle);
    ReleaseView(View);
    IFileManager::Get().DeleteDirectory(*Directory, false, true);
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCastBatchFallenTest, "Project.Expedition.Presentation.CastBatchFallenAndRecover",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FCastBatchFallenTest::RunTest(const FString& Parameters)
{
    using namespace RigPoseTest;
    // The rogue and the cleric (#150 batch 1), downed and dead in a committed checkpoint, resume in a fresh world and view at the end of
    // their Downed and Death clips; their skinned meshes also play Recover, and playback takes the figure back to the committed pose.
    const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation/CastBatchFallen"), FGuid::NewGuid().ToString());
    for (const EAdventurerRole Role : {EAdventurerRole::Rogue, EAdventurerRole::Cleric})
    {
        for (const ECombatLife Life : {ECombatLife::Downed, ECombatLife::Dead})
        {
            const FString Label = FString::Printf(TEXT("%s %s"), *UDiceRollTheme::ClassKey(Role).ToString(), Life == ECombatLife::Downed ? TEXT("downed") : TEXT("dead"));
            FExpeditionRun Run;
            FString Error;
            if (!TestTrue(Label + TEXT(": a run starts"), Run.NewRun(7, Error, TEXT("cast-fallen")))) { continue; }
            const auto* Offer = Run.Progress().Offers.FindByPredicate([](const auto& Route) { return !Route.bDangerous; });
            if (!TestTrue(Label + TEXT(": a route is chosen"), Offer && Run.ChooseRoute(Offer->Id, Error))) { continue; }
            FExpeditionSnapshot Snapshot = Run.Snapshot();
            int32 HeroId = INDEX_NONE;
            for (FCombatantState& Unit : Snapshot.Battle.Encounter.Combatants)
            {
                if (Unit.Side != ECombatSide::Party || Unit.AdventurerRole != Role) { continue; }
                Unit.HP = 0;
                Unit.Life = Life;
                Unit.RescueTurnsLeft = Life == ECombatLife::Downed ? 2 : 0;
                HeroId = Unit.Id;
            }
            if (!TestTrue(Label + TEXT(": the default party has one"), HeroId != INDEX_NONE)) { continue; }
            const FString Path = FPaths::Combine(Directory, FString::Printf(TEXT("%d-%d"), static_cast<int32>(Role), static_cast<int32>(Life)), TEXT("current.json"));
            FExpeditionSaveData Data;
            Data.Current = Snapshot;
            if (!TestTrue(Label + TEXT(": the checkpoint writes"), FExpeditionSave::Write(Path, Data, Error))) { AddError(Error); continue; }
            AExpeditionBattleView* View = MakeView(nullptr, Path, EView::Rigged);
            if (!TestNotNull(Label + TEXT(": resumed view"), View)) { continue; }
            View->Resume();
            View->Tick(0.016f);
            if (TestTrue(Label + TEXT(": the resumed view is mid-encounter"), View->HasRun() && View->InCombat()))
            {
                const FUnitAnimationState State = View->UnitAnimation(HeroId);
                TestTrue(Label + TEXT(" is a rigged figure"), State.bSkeletal);
                const EUnitPose Rest = ExpeditionPose::Rest(Life);
                ExpectPose(*this, Label + TEXT(" holds its fallen pose"), State.Pose, Rest);
                TestTrue(Label + TEXT(" has its clip bound"), State.Clip.Contains(ExpeditionPose::ClipName(Rest)));
                TestTrue(*(Label + FString::Printf(TEXT(" holds the end of its clip (%.3f of %.3f s)"), State.Time, State.Length)), State.Length > 0.1f && State.Time >= State.Length - 0.001f);
                ExpectFiguresConverged(*this, *View, Label + TEXT(" resumed"));
                // Its skinned mesh plays Recover (the clip a rescue shows), and handing it back to playback converges on the committed pose.
                if (TestTrue(Label + TEXT(": Recover can be held"), View->PreviewPose(HeroId, EUnitPose::Recover, 1.0f)))
                {
                    View->Tick(0.016f);
                    const FUnitAnimationState Held = View->UnitAnimation(HeroId);
                    ExpectPose(*this, Label + TEXT(" plays Recover"), Held.Pose, EUnitPose::Recover);
                    TestTrue(Label + TEXT(" has its Recover clip bound"), Held.Clip.Contains(ExpeditionPose::ClipName(EUnitPose::Recover)) && Held.Length > 0.1f);
                    View->ClearPosePreview();
                    View->Tick(0.016f);
                    ExpectFiguresConverged(*this, *View, Label + TEXT(" after Recover"));
                }
            }
            ReleaseView(View);
        }
    }
    IFileManager::Get().DeleteDirectory(*Directory, false, true);
    return !HasAnyErrors();
}

#endif
