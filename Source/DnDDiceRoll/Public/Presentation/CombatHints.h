#pragma once

#include "CoreMinimal.h"
#include "Combat/ActionFacts.h"

/**
 * Contextual combat hints (#158, #112 slice 2). Four first-run callouts, each shown at most once per run.
 * Everything here is presentation: state lives in memory beside the player controller, never in a checkpoint,
 * and nothing in this file decides legality or touches the rules. The controller observes the battle through
 * the action facts and hands the result to FCombatHints; the HUD draws what it reports.
 */
enum class ECombatHint : uint8
{
    /** First party turn of a fresh run: click a glowing tile to move, a marked enemy to attack; arrows + Enter. */
    MoveSelectConfirm,
    /** First spent or short resource: what the count means and what camp gives back. */
    ScarceResources,
    /** First Downed party member: Rescue from an adjacent ally within the shown turns. */
    DownedRescue,
    /** The active hero has nothing available but End Turn. */
    EndingTurn,
    Count
};

/** What the battle shows right now, in the terms the hints need. Built by the controller from facts and the snapshot. */
struct DNDDICEROLL_API FHintObservation
{
    /** The party has the turn at rest: no playback, enemy turn, overlay or open Menu. Hints show only then. */
    bool bAtRest = false;
    /** An action in the acting hero's bar spends a resource that is below its cap, or is short for what it costs. */
    bool bResourceCue = false;
    FActionResourceCost Resource;
    /** A party member is Downed; the name and the fewest rescue turns left among them. */
    bool bDowned = false;
    FString DownedName;
    int32 RescueTurnsLeft = 0;
    /** Every action in the acting hero's bar except End Turn is unavailable. */
    bool bOnlyEndTurn = false;

    bool Holds(ECombatHint Hint) const;
    bool operator==(const FHintObservation& Other) const;
};

/** The words of one callout: a title and wrapped-by-the-HUD lines, all from the facts' formatters. */
struct DNDDICEROLL_API FHintContent
{
    FString Title;
    TArray<FString> Lines;
};

/**
 * Which hint is up, which have been shown, and the session's "tips off" switch.
 * New Run arms (shown flags cleared); Resume does not arm; Skip tips turns tips off for the session (a New Run keeps
 * them off); Replay tips arms and turns tips back on. Only one hint is up at a time, lowest first.
 */
class DNDDICEROLL_API FCombatHints
{
public:
    /** New Run: clears what was shown and arms. Tips stay off if the player skipped them this session. */
    void Arm();
    /** Resume: a returning player is not a first-time player, so nothing is shown until Replay tips. */
    void Disarm();
    /** Replay tips: arm and turn tips on. */
    void Replay();
    /** Skip tips: no further hints this session (until Replay tips). */
    void SkipTips();
    /** Got it / D: put the visible hint away. It counts as shown. */
    void Dismiss();
    /**
     * Once per frame. A hint appears only when the party is at rest; while it is not (an overlay, playback, an enemy
     * turn) nothing advances and nothing is dismissed. A hint whose cause has passed is put away.
     */
    void Observe(const FHintObservation& Now);
    /** Capture and test support: arm, then raise exactly this hint if it applies now. False when it does not. */
    bool Show(ECombatHint Hint, const FHintObservation& Now);

    bool Armed() const { return bArmed; }
    bool TipsOff() const { return bTipsOff; }
    bool WasShown(ECombatHint Hint) const { return Shown[int32(Hint)]; }
    /** The hint held for the player, whether or not it is drawn this frame; Count when none. */
    ECombatHint Held() const { return Active; }
    /** The hint that is drawn this frame; Count when none. */
    ECombatHint Visible() const { return bVisible ? Active : ECombatHint::Count; }
    /** Whether anything is left to observe: armed, tips on and a hint still unshown or up. */
    bool Wants() const;
    /** The current callout's words, from the latest observation. */
    FHintContent Content() const;
    static FHintContent ContentFor(ECombatHint Hint, const FHintObservation& Now);

private:
    bool bArmed = false;
    bool bTipsOff = false;
    bool bVisible = false;
    bool Shown[int32(ECombatHint::Count)] = {};
    ECombatHint Active = ECombatHint::Count;
    FHintObservation Last;
};

namespace ExpeditionStyle
{
DNDDICEROLL_API FString HintName(ECombatHint Hint);
/** The key that puts a visible hint away; shown on the callout and in Help. */
DNDDICEROLL_API FString HintDismissKey();
}
