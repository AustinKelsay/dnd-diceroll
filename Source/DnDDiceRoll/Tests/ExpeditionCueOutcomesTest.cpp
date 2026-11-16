#if WITH_DEV_AUTOMATION_TESTS
#include "Algo/Count.h"
#include "Misc/AutomationTest.h"
#include "Party/BuildCatalog.h"
#include "Persistence/ExpeditionSave.h"
#include "Presentation/ExpeditionAudio.h"
#include "Presentation/ExpeditionPlayback.h"
#include "Run/ExpeditionRun.h"
#include "ExpeditionInputHarness.h"
#include "ExpeditionTestSupport.h"

// The cues of #243: a resource spent, damage a protection reduced, a new round, the finale won and the run lost each sound their own cue, once,
// driven only by committed commands and outcomes. Played at every pace a player can pick (normal, 2x, Instant, a skip part way), and again after
// a Resume, each outcome is heard once at most: skipping is silent by design (Cue is muted while Finish runs), and a cue is never repeated.

namespace CueOutcomes
{
enum class EEnd : uint8 { Normal, Fast, Instant, SkipMid };
const EEnd AllEnds[] = {EEnd::Normal, EEnd::Fast, EEnd::Instant, EEnd::SkipMid};

const TCHAR* EndName(EEnd End)
{
    switch (End)
    {
    case EEnd::Normal: return TEXT("1x");
    case EEnd::Fast: return TEXT("2x");
    case EEnd::Instant: return TEXT("Instant");
    default: return TEXT("skipped mid-way");
    }
}

/** Every cue a playback asked for, in order. */
struct FCueLog
{
    TArray<EExpeditionCue> Order;
    int32 Count(EExpeditionCue Cue) const { return Algo::CountIf(Order, [Cue](EExpeditionCue Heard) { return Heard == Cue; }); }
    void Attach(FExpeditionPlayback& Show) { Show.OnCue = [this](EExpeditionCue Cue) { Order.Add(Cue); }; }
};

/** Plays queued steps the way the player's pace or a skip would. */
void Settle(FExpeditionPlayback& Show, EEnd End)
{
    Show.Speed = End == EEnd::Fast ? 2.2f : 1.0f;
    switch (End)
    {
    case EEnd::Instant: Show.Finish(); break;
    case EEnd::SkipMid:
        for (int32 Frame = 0; Frame < 15; ++Frame) { Show.Tick(0.02f); }
        Show.Finish();
        break;
    default:
        for (int32 Frame = 0; Frame < 4000 && Show.IsBusy(); ++Frame) { Show.Tick(0.02f); }
        break;
    }
}

/**
 * What one cue's count must be after a batch of results settles at an end. Normal and 2x play every step; Instant and a skip mute the steps they
 * finish (the same for every cue since #242), so a cue is heard once at 1x and 2x, at most once after a skip (once when its step began before it),
 * and not at all at Instant. Never twice.
 */
void ExpectHeard(FAutomationTestBase& Test, const FString& What, EEnd End, int32 Count, bool bBeginsFirst)
{
    switch (End)
    {
    case EEnd::Normal: case EEnd::Fast: Test.TestEqual(*(What + TEXT(": heard once")), Count, 1); break;
    case EEnd::Instant: Test.TestEqual(*(What + TEXT(": muted by Instant")), Count, 0); break;
    case EEnd::SkipMid: Test.TestEqual(*(What + (bBeginsFirst ? TEXT(": heard once, its step began before the skip") : TEXT(": at most once after a skip"))),
        bBeginsFirst ? Count : FMath::Min(Count, 1), bBeginsFirst ? 1 : Count); break;
    }
}

FCombatantState Party(int32 Id, const TCHAR* BuildId, FIntPoint Cell)
{
    FCombatantState Unit;
    FBuildCatalog::Create(BuildId, Id, FString::Printf(TEXT("Hero%d"), Id), Unit);
    Unit.Id = Id;
    Unit.Cell = Cell;
    Unit.Side = ECombatSide::Party;
    return Unit;
}

FCombatantState Foe(int32 Id, FIntPoint Cell)
{
    FCombatantState Unit;
    Unit.Id = Id;
    Unit.Name = TEXT("Foe");
    Unit.Side = ECombatSide::Enemy;
    Unit.Cell = Cell;
    Unit.MaxHP = Unit.HP = 10;
    return Unit;
}

/** A hero (id 1) beside an ally (id 2) and a foe (id 20), turn order 1, 2, 20. Playback only reads the units a result names and the round. */
FCombatSnapshot Scene(const TCHAR* BuildId, int32 Round = 1, int32 TurnIndex = 0, EBattlePhase Phase = EBattlePhase::Fighting)
{
    FCombatSnapshot Snapshot;
    Snapshot.Encounter.Combatants = {Party(1, BuildId, FIntPoint(2, 3)), Party(2, TEXT("fighter.champion"), FIntPoint(2, 5)), Foe(20, FIntPoint(3, 3))};
    Snapshot.TurnOrder = {1, 2, 20};
    Snapshot.TurnIndex = TurnIndex;
    Snapshot.Round = Round;
    Snapshot.Phase = Phase;
    return Snapshot;
}

FCombatResult Result(ECombatCommandType Type, int32 Actor, int32 Target = INDEX_NONE)
{
    FCombatResult Out;
    Out.bAccepted = true;
    Out.Type = Type;
    Out.ActorId = Actor;
    Out.TargetId = Target;
    return Out;
}

FCombatResult Strike(ECombatCommandType Type, int32 Actor, int32 Target, int32 Prevented = 0)
{
    FCombatResult Out = Result(Type, Actor, Target);
    Out.bHit = true;
    Out.NaturalRoll = 15;
    Out.AttackTotal = 20;
    Out.Damage = 3;
    Out.DamagePrevented = Prevented;
    return Out;
}

struct FPlayed
{
    FCueLog Log;
    /** Cues the settled playback asked for after finishing again and ticking on: always the same as Log. */
    int32 Late = 0;
};

/** Plays Results from Before to After at an end, then finishes again and ticks on, then resumes: nothing sounds twice, and a Resume sounds nothing. */
FPlayed Play(FAutomationTestBase& Test, const FString& What, const FCombatSnapshot& Before, const TArray<FCombatResult>& Results, const FCombatSnapshot& After,
    EEnd End, bool bEndsRun = false)
{
    FPlayed Played;
    FExpeditionPlayback Show;
    Played.Log.Attach(Show);
    Show.Reset(Before);
    Show.Enqueue(Results, After, bEndsRun);
    Settle(Show, End);
    Test.TestFalse(*(What + TEXT(": playback is done")), Show.IsBusy());
    const int32 Heard = Played.Log.Order.Num();
    Show.Finish();
    for (int32 Frame = 0; Frame < 200; ++Frame) { Show.Tick(0.02f); }
    Played.Late = Played.Log.Order.Num() - Heard;
    Test.TestEqual(*(What + TEXT(": finishing again and playing on sounds nothing more")), Played.Late, 0);
    // A Resume or a new battle shows the committed snapshot silently.
    Show.Reset(After);
    Show.Tick(1.0f);
    Test.TestEqual(*(What + TEXT(": a Resume sounds nothing")), Played.Log.Order.Num(), Heard);
    return Played;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCueEnumGrowthTest, "Project.Expedition.Audio.CueSynthesis",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FCueEnumGrowthTest::RunTest(const FString& Parameters)
{
    // Every cue has a synthesized fallback and a volume, for a variation of each of the four.
    for (int32 Index = 0; Index < ExpeditionCueCount; ++Index)
    {
        const EExpeditionCue Cue = static_cast<EExpeditionCue>(Index);
        const FString Name = ExpeditionAudio::CueName(Cue);
        TestTrue(*FString::Printf(TEXT("%s: a sensible synthesis volume"), *Name), ExpeditionAudio::Volume(Cue) > 0.0f && ExpeditionAudio::Volume(Cue) <= 1.0f);
        for (int32 Variation = 0; Variation < ExpeditionAudio::SynthVariations; ++Variation)
        {
            const TArray<int16> Pcm = ExpeditionAudio::Synthesize(Cue, Variation);
            TestTrue(*FString::Printf(TEXT("%s (variation %d): synthesis makes a sound"), *Name, Variation), Pcm.Num() > ExpeditionAudio::SampleRate / 20);
            int32 Peak = 0;
            for (const int16 Sample : Pcm) { Peak = FMath::Max(Peak, FMath::Abs(static_cast<int32>(Sample))); }
            TestTrue(*FString::Printf(TEXT("%s (variation %d): audible and not clipped"), *Name, Variation), Peak > 3000 && Peak < 32767);
        }
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSpendCuesTest, "Project.Expedition.Audio.SpendCues",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSpendCuesTest::RunTest(const FString& Parameters)
{
    using namespace CueOutcomes;
    using EC = ECombatCommandType;
    struct FCase { const TCHAR* Build; const TCHAR* What; FCombatResult Outcome; TOptional<EExpeditionCue> Cue; };
    // The most specific resource sounds: Rage, then a potion, then a slot, then a use (Magic Missile spends a Kit use and a slot).
    const TArray<FCase> Cases = {
        {TEXT("fighter.champion"), TEXT("a potion"), Result(EC::Potion, 1, 1), EExpeditionCue::SpendPotion},
        {TEXT("fighter.champion"), TEXT("Second Wind"), Result(EC::SecondWind, 1, 1), EExpeditionCue::SpendKit},
        {TEXT("fighter.battle-master"), TEXT("a maneuver"), Strike(EC::PrecisionStrike, 1, 20), EExpeditionCue::SpendKit},
        {TEXT("wizard.evocation"), TEXT("Magic Missile (a use and a slot)"), Strike(EC::MagicMissile, 1, 20), EExpeditionCue::SpendSlot},
        {TEXT("wizard.evocation"), TEXT("Sculpted Burst (a use)"), Result(EC::Special2, 1, 20), EExpeditionCue::SpendKit},
        {TEXT("cleric.life"), TEXT("Cure Wounds (a slot)"), Result(EC::CureWounds, 1, 2), EExpeditionCue::SpendSlot},
        {TEXT("barbarian.berserker"), TEXT("Rage"), Result(EC::Special1, 1, 1), EExpeditionCue::SpendRage},
        // Nothing is spent: the attack, a step, ending the turn, a rescue.
        {TEXT("fighter.champion"), TEXT("a basic attack"), Strike(EC::Attack, 1, 20), {}},
        {TEXT("fighter.champion"), TEXT("a step"), Result(EC::Move, 1), {}},
        {TEXT("fighter.champion"), TEXT("ending the turn"), Result(EC::EndTurn, 1), {}},
    };
    for (const FCase& Case : Cases)
    {
        FCombatResult Outcome = Case.Outcome;
        if (Outcome.Type == EC::Move) { Outcome.Path = {FIntPoint(2, 4)}; }
        if (Outcome.Type == EC::Potion || Outcome.Type == EC::SecondWind) { Outcome.Healing = 4; }
        if (Outcome.Type == EC::Special1 && FString(Case.Build) == TEXT("barbarian.berserker"))
        { Outcome.Effects = {{1, EAbilityEffect::Rage, 2, FIntPoint(2, 3)}}; }
        const FCombatSnapshot Before = Scene(Case.Build);
        FCombatSnapshot After = Before;
        for (const EEnd End : AllEnds)
        {
            const FString What = FString::Printf(TEXT("%s, %s"), Case.What, EndName(End));
            const FPlayed Played = Play(*this, What, Before, {Outcome}, After, End);
            for (int32 Index = static_cast<int32>(EExpeditionCue::SpendKit); Index <= static_cast<int32>(EExpeditionCue::SpendRage); ++Index)
            {
                const EExpeditionCue Cue = static_cast<EExpeditionCue>(Index);
                const bool bExpected = Case.Cue.IsSet() && Case.Cue.GetValue() == Cue;
                if (bExpected) { ExpectHeard(*this, FString::Printf(TEXT("%s: %s"), *What, *ExpeditionAudio::CueName(Cue)), End, Played.Log.Count(Cue), true); }
                else { TestEqual(*FString::Printf(TEXT("%s: %s does not sound"), *What, *ExpeditionAudio::CueName(Cue)), Played.Log.Count(Cue), 0); }
            }
            if (Case.Cue.IsSet() && (End == EEnd::Normal || End == EEnd::Fast) && !Played.Log.Order.IsEmpty())
            { TestEqual(*FString::Printf(TEXT("%s: the spend is the first sound"), *What), Played.Log.Order[0], Case.Cue.GetValue()); }
            // Rage no longer borrows the party-turn chime.
            if (Case.Outcome.Type == EC::Special1 && FString(Case.Build) == TEXT("barbarian.berserker"))
            { TestEqual(*FString::Printf(TEXT("%s: no party-turn chime"), *What), Played.Log.Count(EExpeditionCue::PartyTurn), 0); }
        }
    }
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProtectionCueTest, "Project.Expedition.Audio.ProtectionCue",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FProtectionCueTest::RunTest(const FString& Parameters)
{
    using namespace CueOutcomes;
    using EC = ECombatCommandType;
    const FCombatSnapshot Before = Scene(TEXT("fighter.eldritch-knight"));
    const FCombatSnapshot After = Before;
    // A hit on a guarded unit: the hero's attack on the foe and the foe's attack on the hero both carry the committed reduction.
    for (const EEnd End : AllEnds)
    {
        const FString Label = EndName(End);
        for (const bool bFoeStrikes : {false, true})
        {
            const FString Who = bFoeStrikes ? TEXT("foe hits a guarded hero") : TEXT("hero hits a guarded foe");
            const FCombatResult Reduced = bFoeStrikes ? Strike(EC::Attack, 20, 1, 2) : Strike(EC::Attack, 1, 20, 2);
            const FPlayed Played = Play(*this, FString::Printf(TEXT("%s, %s"), *Who, *Label), Before, {Reduced}, After, End);
            // The hit lands after the dice settle, so a skip part way has not reached it yet.
            ExpectHeard(*this, FString::Printf(TEXT("%s, %s: protection"), *Who, *Label), End, Played.Log.Count(EExpeditionCue::Protection), false);
            TestEqual(*FString::Printf(TEXT("%s, %s: protection sounds exactly as often as the hit"), *Who, *Label), Played.Log.Count(EExpeditionCue::Protection), Played.Log.Count(EExpeditionCue::Hit));
            const FPlayed Plain = Play(*this, FString::Printf(TEXT("%s unreduced, %s"), *Who, *Label), Before, {bFoeStrikes ? Strike(EC::Attack, 20, 1) : Strike(EC::Attack, 1, 20)}, After, End);
            TestEqual(*FString::Printf(TEXT("%s unreduced, %s: no protection"), *Who, *Label), Plain.Log.Count(EExpeditionCue::Protection), 0);
        }
        // A damaging special lists "Prevented N" for each victim whose protection took damage off.
        FCombatResult Burst = Result(EC::Special2, 1, 20);
        Burst.Effects = {{20, EAbilityEffect::Area, 4, FIntPoint(3, 3)}, {20, EAbilityEffect::Mitigation, 2, FIntPoint(3, 3)}};
        const FPlayed Special = Play(*this, FString::Printf(TEXT("a burst with prevention, %s"), *Label), Before, {Burst}, After, End);
        ExpectHeard(*this, FString::Printf(TEXT("a burst with prevention, %s: protection"), *Label), End, Special.Log.Count(EExpeditionCue::Protection), false);
        FCombatResult Whole = Burst;
        Whole.Effects[1].Amount = 0;
        const FPlayed None = Play(*this, FString::Printf(TEXT("a burst with nothing prevented, %s"), *Label), Before, {Whole}, After, End);
        TestEqual(*FString::Printf(TEXT("a burst with nothing prevented, %s: no protection"), *Label), None.Log.Count(EExpeditionCue::Protection), 0);
    }
    // Applying a ward is not reducing damage: Sheltering a hero sounds no protection.
    FCombatResult Ward = Result(EC::Special1, 1, 1);
    Ward.Effects = {{1, EAbilityEffect::Protect, 3, FIntPoint(2, 3)}};
    const FPlayed Applied = Play(*this, TEXT("a ward applied"), Before, {Ward}, After, EEnd::Normal);
    TestEqual(TEXT("a ward applied: no protection cue"), Applied.Log.Count(EExpeditionCue::Protection), 0);
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRoundStartCueTest, "Project.Expedition.Audio.RoundStartCue",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRoundStartCueTest::RunTest(const FString& Parameters)
{
    using namespace CueOutcomes;
    const TCHAR* Build = TEXT("fighter.champion");
    // The turns of one round, then the wrap into the next.
    const FCombatSnapshot HeroTurn = Scene(Build, 1, 0), AllyTurn = Scene(Build, 1, 1), FoeTurn = Scene(Build, 1, 2), NextRound = Scene(Build, 2, 0), NextAlly = Scene(Build, 2, 1);
    const FCombatResult End1 = Result(ECombatCommandType::EndTurn, 1), End2 = Result(ECombatCommandType::EndTurn, 2), End20 = Result(ECombatCommandType::EndTurn, 20);
    for (const EEnd End : AllEnds)
    {
        const FString Label = EndName(End);
        const FPlayed Party = Play(*this, FString::Printf(TEXT("a party turn follows, %s"), *Label), HeroTurn, {End1}, AllyTurn, End);
        ExpectHeard(*this, FString::Printf(TEXT("a party turn follows, %s: the party-turn cue"), *Label), End, Party.Log.Count(EExpeditionCue::PartyTurn), true);
        TestEqual(*FString::Printf(TEXT("a party turn follows, %s: no round start"), *Label), Party.Log.Count(EExpeditionCue::RoundStart), 0);
        const FPlayed Enemy = Play(*this, FString::Printf(TEXT("the foe's turn follows, %s"), *Label), AllyTurn, {End2}, FoeTurn, End);
        ExpectHeard(*this, FString::Printf(TEXT("the foe's turn follows, %s: the enemy-turn cue"), *Label), End, Enemy.Log.Count(EExpeditionCue::EnemyTurn), true);
        TestEqual(*FString::Printf(TEXT("the foe's turn follows, %s: no round start"), *Label), Enemy.Log.Count(EExpeditionCue::RoundStart), 0);
        // The wrap into round 2 sounds the round, in place of the turn cue it used to reuse.
        const FPlayed Round = Play(*this, FString::Printf(TEXT("round 2 begins, %s"), *Label), FoeTurn, {End20}, NextRound, End);
        ExpectHeard(*this, FString::Printf(TEXT("round 2 begins, %s: the round-start cue"), *Label), End, Round.Log.Count(EExpeditionCue::RoundStart), true);
        TestEqual(*FString::Printf(TEXT("round 2 begins, %s: not also a turn cue"), *Label), Round.Log.Count(EExpeditionCue::PartyTurn) + Round.Log.Count(EExpeditionCue::EnemyTurn), 0);
    }

    // The round is learned even when the sound is not made: after an Instant wrap, the next turn of the same round is not a round start.
    {
        FExpeditionPlayback Show;
        FCueLog Log;
        Log.Attach(Show);
        Show.Reset(FoeTurn);
        Show.Enqueue({End20}, NextRound);
        Show.Finish();
        TestEqual(TEXT("Instant mutes the round start"), Log.Count(EExpeditionCue::RoundStart), 0);
        Show.Enqueue({End1}, NextAlly);
        Settle(Show, EEnd::Normal);
        TestEqual(TEXT("the next turn of that round is a party turn"), Log.Count(EExpeditionCue::PartyTurn), 1);
        TestEqual(TEXT("and not a second round start"), Log.Count(EExpeditionCue::RoundStart), 0);
    }
    // Two batches queued in one round announce it once; a Resume into round 2 then a turn of round 2 is no round start.
    {
        FExpeditionPlayback Show;
        FCueLog Log;
        Log.Attach(Show);
        Show.Reset(FoeTurn);
        Show.Enqueue({End20}, NextRound);
        Show.Enqueue({End1}, NextAlly);
        Settle(Show, EEnd::Normal);
        TestEqual(TEXT("queued batches announce the round once"), Log.Count(EExpeditionCue::RoundStart), 1);
        TestEqual(TEXT("and the second batch is a turn"), Log.Count(EExpeditionCue::PartyTurn), 1);
        Show.Reset(NextRound);
        Show.Enqueue({End1}, NextAlly);
        Settle(Show, EEnd::Normal);
        TestEqual(TEXT("after a Resume into round 2, a turn of round 2 is no round start"), Log.Count(EExpeditionCue::RoundStart), 1);
        TestEqual(TEXT("it is a party turn"), Log.Count(EExpeditionCue::PartyTurn), 2);
    }
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRunEndingCueTest, "Project.Expedition.Audio.EndingCues",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRunEndingCueTest::RunTest(const FString& Parameters)
{
    using namespace CueOutcomes;
    const FCombatSnapshot Before = Scene(TEXT("fighter.champion"), 3, 2);
    struct FCase { const TCHAR* What; EBattlePhase Phase; bool bEndsRun; EExpeditionCue Heard; EExpeditionCue Silent; };
    const FCase Cases[] = {
        {TEXT("a battle won with the run going on"), EBattlePhase::Victory, false, EExpeditionCue::Victory, EExpeditionCue::Finale},
        {TEXT("the finale won"), EBattlePhase::Victory, true, EExpeditionCue::Finale, EExpeditionCue::Victory},
        {TEXT("a battle lost without ending the run"), EBattlePhase::Defeat, false, EExpeditionCue::Defeat, EExpeditionCue::RunEnd},
        {TEXT("the run lost"), EBattlePhase::Defeat, true, EExpeditionCue::RunEnd, EExpeditionCue::Defeat},
    };
    for (const FCase& Case : Cases)
    {
        const FCombatSnapshot After = Scene(TEXT("fighter.champion"), 3, 0, Case.Phase);
        for (const EEnd End : AllEnds)
        {
            const FString What = FString::Printf(TEXT("%s, %s"), Case.What, EndName(End));
            const FPlayed Played = Play(*this, What, Before, {Result(ECombatCommandType::EndTurn, 20)}, After, End, Case.bEndsRun);
            ExpectHeard(*this, FString::Printf(TEXT("%s: %s"), *What, *ExpeditionAudio::CueName(Case.Heard)), End, Played.Log.Count(Case.Heard), true);
            TestEqual(*FString::Printf(TEXT("%s: not %s"), *What, *ExpeditionAudio::CueName(Case.Silent)), Played.Log.Count(Case.Silent), 0);
            // Nothing else of the pair sounds, and no turn cue is borrowed for the end.
            TestEqual(*FString::Printf(TEXT("%s: no turn or round cue"), *What),
                Played.Log.Count(EExpeditionCue::PartyTurn) + Played.Log.Count(EExpeditionCue::EnemyTurn) + Played.Log.Count(EExpeditionCue::RoundStart), 0);
        }
    }
    return !HasAnyErrors();
}

namespace CueOutcomes
{
/** The party's turn in a live view: attack when a foe is in range, else close the distance, then end the turn. */
void PartyActs(AExpeditionBattleView& View, bool bFight)
{
    const int32 Id = View.Rules().ActiveId();
    if (bFight)
    {
        TArray<int32> Targets = View.Rules().LegalTargets(Id);
        if (Targets.IsEmpty())
        {
            TArray<FIntPoint> Moves = View.Rules().LegalMoves(Id);
            auto Nearest = [&View](FIntPoint Cell)
            {
                int32 Best = 1000;
                for (const FCombatantState& Enemy : View.Rules().Snapshot().Encounter.Combatants)
                { if (Enemy.Side == ECombatSide::Enemy && Enemy.HP > 0) { Best = FMath::Min(Best, ExpeditionTestSupport::Distance(Cell, Enemy.Cell)); } }
                return Best;
            };
            Moves.Sort([&Nearest](FIntPoint A, FIntPoint B) { const int32 DA = Nearest(A), DB = Nearest(B); return DA == DB ? (A.Y == B.Y ? A.X < B.X : A.Y < B.Y) : DA < DB; });
            if (!Moves.IsEmpty()) { View.Execute({ECombatCommandType::Move, Id, Moves[0]}); }
            Targets = View.Rules().LegalTargets(Id);
        }
        if (!Targets.IsEmpty()) { View.Execute({ECombatCommandType::Attack, Id, {}, Targets[0]}); }
    }
    if (View.InCombat() && View.Rules().ActiveId() == Id) { View.Execute({ECombatCommandType::EndTurn, Id}); }
}

/** Plays a fight to its end in the view at 1x pace, letting every order and enemy turn present fully before the next. True when it ended. */
bool FightToTheEnd(AExpeditionBattleView& View, bool bFight)
{
    for (int32 Guard = 0; Guard < 6000 && View.InCombat(); ++Guard)
    {
        if (View.IsPresenting()) { View.Tick(0.05f); continue; }
        const FCombatantState* Active = View.Rules().Find(View.Rules().ActiveId());
        if (!Active) { return false; }
        if (Active->Side == ECombatSide::Party) { PartyActs(View, bFight); }
        else { View.Tick(0.1f); }
    }
    // The last results present before the ending cue sounds.
    for (int32 Guard = 0; Guard < 2000 && View.IsPresenting(); ++Guard) { View.Tick(0.05f); }
    return !View.InCombat();
}

/** A checkpoint of the fight a run is in, with every foe at one hit point (a fight the scripted attacker wins) or every hero at one (a fight it loses). */
FExpeditionSaveData Fixture(FExpeditionRun& Run, bool bFoesNearlyDead)
{
    FExpeditionSaveData Data;
    Data.Current = Run.Snapshot();
    for (FCombatantState& Unit : Data.Current.Battle.Encounter.Combatants)
    {
        if (Unit.Life == ECombatLife::Conscious && (Unit.Side == ECombatSide::Enemy) == bFoesNearlyDead) { Unit.HP = 1; }
    }
    return Data;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLiveRunEndingCueTest, "Project.Expedition.Audio.LiveRunEndings",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FLiveRunEndingCueTest::RunTest(const FString& Parameters)
{
    using namespace CueOutcomes;
    // Real runs through the real view: the finale won and a run lost end the run, an ordinary battle won does not.
    FExpeditionRun Finale;
    const int32 Seed = ExpeditionTestSupport::FirstSeedWhere([&Finale](int32 Try) { Finale = FExpeditionRun(); return ExpeditionTestSupport::ReachFinale(Finale, Try); });
    if (!TestNotEqual(TEXT("A seed reaches the finale with the party to act"), Seed, static_cast<int32>(INDEX_NONE))) { return false; }
    FExpeditionRun FirstFight;
    FString Error;
    if (!TestTrue(TEXT("The first fight starts"), FirstFight.NewRun(Seed, Error, TEXT("cue-first")) && FirstFight.ChooseRoute(FirstFight.Progress().Offers[0].Id, Error))) { return false; }

    struct FScenario { const TCHAR* What; FExpeditionRun* Run; bool bFoesNearlyDead; bool bFight; EExpeditionPhase Ends; EExpeditionCue Heard; EExpeditionCue Silent[3]; };
    FScenario Scenarios[] = {
        {TEXT("the finale won"), &Finale, true, true, EExpeditionPhase::Victory, EExpeditionCue::Finale, {EExpeditionCue::Victory, EExpeditionCue::Defeat, EExpeditionCue::RunEnd}},
        {TEXT("the run lost in the finale"), &Finale, false, false, EExpeditionPhase::Defeat, EExpeditionCue::RunEnd, {EExpeditionCue::Defeat, EExpeditionCue::Victory, EExpeditionCue::Finale}},
        {TEXT("the first fight won"), &FirstFight, true, true, EExpeditionPhase::Reward, EExpeditionCue::Victory, {EExpeditionCue::Finale, EExpeditionCue::Defeat, EExpeditionCue::RunEnd}},
        {TEXT("the run lost in the first fight"), &FirstFight, false, false, EExpeditionPhase::Defeat, EExpeditionCue::RunEnd, {EExpeditionCue::Defeat, EExpeditionCue::Victory, EExpeditionCue::Finale}},
    };
    for (const FScenario& Scenario : Scenarios)
    {
        const FExpeditionSaveData Saved = Fixture(*Scenario.Run, Scenario.bFoesNearlyDead);
        ExpeditionInputHarness::FHarness Game(*this, TEXT("CueEndings"), &Saved);
        if (!TestTrue(FString::Printf(TEXT("%s: the view is ready"), Scenario.What), Game.IsReady())) { return false; }
        Game.View->Resume();
        TestTrue(FString::Printf(TEXT("%s: resumed into the fight"), Scenario.What), Game.View->HasRun() && Game.View->InCombat());
        // Resuming into a fight sounds the route cue only: nothing of an ending, a round or a turn.
        TestEqual(FString::Printf(TEXT("%s: a Resume sounds the route cue once"), Scenario.What), Game.View->CueRequests(EExpeditionCue::Route), 1u);
        for (const EExpeditionCue Cue : {EExpeditionCue::Victory, EExpeditionCue::Defeat, EExpeditionCue::Finale, EExpeditionCue::RunEnd, EExpeditionCue::RoundStart})
        { TestEqual(FString::Printf(TEXT("%s: a Resume sounds no %s"), Scenario.What, *ExpeditionAudio::CueName(Cue)), Game.View->CueRequests(Cue), 0u); }
        const bool bEnded = FightToTheEnd(*Game.View, Scenario.bFight);
        if (!TestTrue(FString::Printf(TEXT("%s: the fight ends"), Scenario.What), bEnded)) { continue; }
        TestEqual(FString::Printf(TEXT("%s: the run is where the ending says"), Scenario.What), Game.View->Progress().Phase, Scenario.Ends);
        TestEqual(FString::Printf(TEXT("%s: %s sounds once"), Scenario.What, *ExpeditionAudio::CueName(Scenario.Heard)), Game.View->CueRequests(Scenario.Heard), 1u);
        for (const EExpeditionCue Cue : Scenario.Silent)
        { TestEqual(FString::Printf(TEXT("%s: %s does not"), Scenario.What, *ExpeditionAudio::CueName(Cue)), Game.View->CueRequests(Cue), 0u); }
        // Playing on changes nothing: the ending is not announced again.
        for (int32 Frame = 0; Frame < 100; ++Frame) { Game.View->Tick(0.05f); }
        TestEqual(FString::Printf(TEXT("%s: still once after playing on"), Scenario.What), Game.View->CueRequests(Scenario.Heard), 1u);
    }
    return !HasAnyErrors();
}
#endif
