#pragma once

#include "CoreMinimal.h"
#include "Combat/CombatBattle.h"
#include "Run/ExpeditionRun.h"

class AExpeditionBattleView;
class AExpeditionPlayerController;

/**
 * Autoplay (#148): a presentation-only driver that plays one seeded expedition fight through the real player
 * commands (the controller's HUD buttons and click path), so a performance run measures real animation,
 * effects and overlay. It never touches rules, saves or RNG: the same seed and commands give the same
 * checkpoint as manual play. Active only with -ExpeditionAutoplay=<script.json>.
 */
namespace ExpeditionAutoplay
{
/** The seed a first New Run uses (AExpeditionBattleView::NewRun starts at this value; the player path offers no other). */
constexpr int32 FirstRunSeed = 20261030;

/** The beats a measured fight must contain. Resume passes log their own markers (resume, claim, camp). */
enum class EBeat : uint8 { Move, Attack, Special, Downing, Rescue, Victory, Quit };
constexpr int32 BeatCount = 7;
DNDDICEROLL_API const TCHAR* BeatName(EBeat Beat);

/**
 * One tactical variant the planner tries. A party member (the bait) holds the line and is meant to fall;
 * the others hold back, then rescue it; the party then wins. The planner keeps the best variant that
 * reaches every beat, so a seed never has to be tuned by hand.
 */
struct DNDDICEROLL_API FTactic
{
    /** Party slot 0..3 (unit id Bait + 1). */
    int32 Bait = 0;
    /** The bait walks up to the nearest foe; otherwise it stays where it starts. */
    bool bAdvance = true;
    /** Before the downing, the others step back from foes within three cells. */
    bool bFallBack = false;
    /** After a Rescue, a wounded hero drinks and retreats instead of standing in reach. */
    bool bRetreat = true;
    /** A variant that has not won or fallen by this round is abandoned. */
    int32 MaxRounds = 40;
};

struct DNDDICEROLL_API FAutoplayScript
{
    FString Name;
    int32 Seed = FirstRunSeed;
    /** Exactly four catalog build ids, party slot order. */
    TArray<FString> Builds;
    /** The route to take: the dangerous Watchtower road, or the guarded one. */
    bool bDangerous = true;
    /** Frames before the first action (world, HUD and viewport up). */
    int32 BootFrames = 120;
    /** Frames a command stays shown (mode, cursor, path ribbon) before it is issued. */
    int32 ThinkFrames = 24;
    /** Frames the screen must stay settled before a transition counts as ended. */
    int32 SettleFrames = 30;
    /** Wall-clock limits: any one stage, and the whole fight. */
    int32 StageSeconds = 240;
    int32 FightSeconds = 1800;
    TArray<FTactic> Variants;
};

/** Pure: validates an autoplay script (version 1) and fills Out, or fails with a message naming the problem. */
DNDDICEROLL_API bool ParseScript(const FString& Json, FAutoplayScript& Out, FString& Error);

/** The party the controller's presets name, with the script's builds in slot order. */
DNDDICEROLL_API TArray<FPartyBuildSelection> PartyFor(const FAutoplayScript& Script);

/** One party command and the exact state the live game must be in when it is issued. */
struct DNDDICEROLL_API FAutoplayStep
{
    FCombatCommand Command;
    FExpeditionSnapshot Before;
    /** Beats this command is the first to meet. */
    TArray<EBeat> Beats;
};

struct DNDDICEROLL_API FAutoplayPlan
{
    bool bComplete = false;
    int32 Variant = INDEX_NONE;
    /** After the run starts and the route is chosen, and after the victory. */
    FExpeditionSnapshot Opening;
    FExpeditionSnapshot Final;
    TArray<FAutoplayStep> Steps;
    bool bReached[BeatCount] = {};
    int32 Survivors = 0;
    int32 DownedId = INDEX_NONE;
    int32 Rounds = 0;
    /** One line per variant tried: what it reached or why it was dropped. */
    TArray<FString> Report;
    bool Reached(EBeat Beat) const { return bReached[static_cast<int32>(Beat)]; }
};

/**
 * Plays every variant headlessly against the rules (a detached FExpeditionRun: no rendering, no files) from the
 * script's seed and the given party, and keeps the best: every beat reached (Quit is the driver's, not the
 * plan's), then the most survivors, then the fewest commands. False, with Plan.Report naming each variant's
 * failure, when none reaches every beat.
 */
DNDDICEROLL_API bool BuildPlan(const FAutoplayScript& Script, const TArray<FPartyBuildSelection>& Party, const FString& Identity, FAutoplayPlan& Plan);

/** Two snapshots equal in everything but the run's random identity. */
DNDDICEROLL_API bool SameModuloIdentity(const FExpeditionSnapshot& A, const FExpeditionSnapshot& B);
}

/**
 * The live driver. Created by the player controller when the command line has -ExpeditionAutoplay=<script.json>
 * (and by tests). Fresh pass: choose the party, start a run, take the route, play the planned commands, win,
 * and quit to the checkpoint. With -ExpeditionAutoplayResume: Resume that checkpoint, claim the reward, take
 * the camp and reach the route table. Every beat and transition is logged as an AUTOPLAY_ line and recorded as
 * a CSV profiler event, so a capture can be sliced; the process exits non-zero on any failure.
 */
class DNDDICEROLL_API FExpeditionAutoplayDirector
{
public:
    enum class EStatus : uint8 { Running, Passed, Failed };

    static bool Requested(const TCHAR* CommandLine);
    /** Null unless the command line asks for autoplay. A bad script fails loudly (exit 1) once the world is up. */
    static TSharedPtr<FExpeditionAutoplayDirector> TryCreate(AExpeditionPlayerController* Controller, AExpeditionBattleView* View);
    /** For tests and tools; bExitOnFinish false leaves the process running and skips the profiler flush. */
    static TSharedPtr<FExpeditionAutoplayDirector> Create(AExpeditionPlayerController* Controller, AExpeditionBattleView* View, const ExpeditionAutoplay::FAutoplayScript& Script, bool bResume, bool bExitOnFinish);

    /** Once per rendered frame, from the player controller. */
    void Tick();

    EStatus Status() const { return State; }
    const FString& Failure() const { return Failed; }
    /** The plan the fresh pass is following (empty before the run starts and on a resume pass). */
    const ExpeditionAutoplay::FAutoplayPlan& Plan() const { return Planned; }
    bool Saw(ExpeditionAutoplay::EBeat Beat) const { return bSeen[static_cast<int32>(Beat)]; }
    /** Names of the transitions that began and ended, in order. */
    const TArray<FString>& Transitions() const { return Finished; }

private:
    enum class EStage : uint8 { Boot, Builder, Starting, Routing, Choosing, Fight, Victory, Leaving, Resuming, Claiming, Camping, Flushing, Done };

    TWeakObjectPtr<AExpeditionPlayerController> Controller;
    TWeakObjectPtr<AExpeditionBattleView> View;
    ExpeditionAutoplay::FAutoplayScript Script;
    ExpeditionAutoplay::FAutoplayPlan Planned;
    FString ScriptPath, ScriptError;
    EStage Stage = EStage::Boot;
    EStatus State = EStatus::Running;
    FString Failed;
    bool bResumePass = false;
    bool bExit = true;
    bool bPartyChosen = false;
    bool bPlanned = false;
    bool bPrepared = false;
    bool bSeen[ExpeditionAutoplay::BeatCount] = {};
    bool bDowningSeen = false;
    uint64 Frames = 0;
    int32 StageFrames = 0;
    int32 ActFrames = 0;
    int32 NextStep = 0;
    int32 ExitCode = 0;
    double StartSeconds = 0.0;
    double StageStarted = 0.0;
    double FightStarted = 0.0;
    double FlushStarted = 0.0;
    bool bFlushRequested = false;
    FString OpenTransition;
    uint64 OpenFrame = 0;
    double OpenSeconds = 0.0;
    TArray<FString> Finished;

    bool Calm() const;
    bool TableShown(int32 WaitedFrames) const;
    void Enter(EStage To);
    void Mark(const FString& Kind, const FString& Name, const FString& Edge, const FString& Detail = FString());
    void Beat(ExpeditionAutoplay::EBeat Beat, const FString& Detail = FString());
    void BeginTransition(const FString& Name);
    void EndTransition();
    void WatchDowning();
    bool Prepare(const ExpeditionAutoplay::FAutoplayStep& Step);
    bool Act(const ExpeditionAutoplay::FAutoplayStep& Step);
    void Finish(bool bOk, const FString& Message);
    void Fail(const FString& Message);
    void Flush();
    void TickFresh();
    void TickResume();
};
