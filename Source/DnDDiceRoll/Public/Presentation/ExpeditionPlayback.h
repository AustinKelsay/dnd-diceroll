#pragma once

#include "CoreMinimal.h"
#include "Combat/CombatBattle.h"
#include "Presentation/UnitPose.h"

/** Sounds and moments the playback asks the world to express. */
enum class EExpeditionCue : uint8
{
    DiceRoll, DiceLand, Hit, Critical, Miss, Heal, Step, Click, Downed, Death,
    PartyTurn, EnemyTurn, Victory, Defeat, Spell, Bow, Swing, Camp, Loot, Route
};

/**
 * What a capture-mode impact preview shows on the struck unit (#189): a plain hit, or a spell bolt (Cleric radiant, Wizard fire, or three Magic Missiles)
 * arriving as well. Arrow (#198) is a non-spell ranged attack's arrow as AddAttack throws it (a Ranger's, Rogue's or foe archer's: no light). Bolt is what
 * a Ranger's attack threw before #198, a spell bolt in the arrow's colour with its point light at the shooter, kept so the blowout it caused can be measured again.
 */
enum class EImpactPreview : uint8 { Hit, Spell, Fire, Missile, Arrow, Bolt };

/**
 * When a capture-mode impact preview is taken (#189). Landing: the hit itself with its flash (for Magic Missile the final bolt landing, as the
 * production impact step begins while the last bolt is still arriving). Flight: the bolt half way along its path, no hit. FirstLanding and
 * SecondLanding: Magic Missile only, the first or second bolt landing while the others are still in flight, before the hit. Release (#198): Arrow and
 * Bolt only, the moment the shot leaves the shooter (the Ranged clip's release frame), no hit.
 */
enum class EImpactMoment : uint8 { Landing, Flight, FirstLanding, SecondLanding, Release };

/** What a combatant looks like right now. Lags behind the rules while results play. */
struct DNDDICEROLL_API FUnitDisplay
{
    int32 Id = INDEX_NONE;
    FVector2D Position = FVector2D::ZeroVector; // Cell units.
    FVector2D Facing = FVector2D(1, 0);
    FVector2D Lean = FVector2D::ZeroVector;     // Cell units.
    float Hop = 0.0f;                           // Centimetres.
    float Flash = 0.0f;
    float Glow = 0.0f;
    float Tilt = 0.0f;                          // 0 upright, 1 lying down.
    int32 HP = 0;
    int32 MaxHP = 1;
    float ShownHP = 0.0f;
    float TrailHP = 0.0f;
    ECombatLife Life = ECombatLife::Conscious;
    /** The clip the figure should show (#147). Chosen from committed results; never read back by rules. */
    EUnitPose Pose = EUnitPose::Idle;
    /** Bumps whenever Pose is set or converged, so a repeated clip restarts and the view notices the change. */
    int32 PoseSerial = 0;
    /** True when the pose was reached by convergence on the committed state (reset, resume, skip, Finish): show the clip's end. */
    bool bPoseSnap = true;
};

struct DNDDICEROLL_API FFloatingText
{
    FString Text;
    FLinearColor Color = FLinearColor::White;
    FVector2D Cell = FVector2D::ZeroVector;
    float Age = 0.0f;
    float Life = 1.3f;
    float Scale = 1.0f;
};

struct DNDDICEROLL_API FPlaybackProjectile
{
    FVector2D From = FVector2D::ZeroVector;
    FVector2D To = FVector2D::ZeroVector;
    float Age = 0.0f;
    float Duration = 0.3f;
    float Arc = 0.0f;   // Peak offset in centimetres: positive lobs upward, negative curves sideways.
    float Size = 0.18f;
    FLinearColor Color = FLinearColor::White;
    bool bArrow = false;
};

struct DNDDICEROLL_API FPlaybackSparkle
{
    FVector2D Cell = FVector2D::ZeroVector;
    FLinearColor Color = FLinearColor::White;
    float Age = 0.0f;
    float Life = 0.8f;
    float Angle = 0.0f;
};

struct DNDDICEROLL_API FShownDie
{
    int32 Sides = 20;
    int32 Value = 1;
    float Delay = 0.0f;     // Seconds after the tray opens before this die is thrown.
    FLinearColor Color = FLinearColor::White;
};

/** The dice on screen for the latest roll, with the arithmetic that explains them. */
struct DNDDICEROLL_API FDiceTray
{
    TArray<FShownDie> Dice;
    FString Title;
    FString Detail;
    FString Verdict;
    FLinearColor VerdictColor = FLinearColor::White;
    float Age = 0.0f;
    bool bVisible = false;
    /** Capture and test support: a held tray does not age or expire. */
    bool bHeld = false;
    static constexpr float TumbleSeconds = 0.6f;
    static constexpr float LingerSeconds = 3.5f;
};

struct DNDDICEROLL_API FTurnBanner
{
    FString Title;
    FString Subtitle;
    FLinearColor Color = FLinearColor::White;
    float Age = 0.0f;
    float Life = 0.0f;
};

/**
 * Replays accepted combat results as timed steps. The rules have already committed
 * every result; playback only changes what is shown, and Finish always converges on
 * the committed snapshot. No rule or save state is read back from here.
 */
class DNDDICEROLL_API FExpeditionPlayback
{
public:
    TFunction<void(EExpeditionCue)> OnCue;
    TFunction<void(float)> OnShake;

    /** Shows the snapshot immediately and clears queued steps and effects. */
    void Reset(const FCombatSnapshot& Snapshot);
    /** Moves units to the snapshot without clearing effects (rewards, camp). */
    void Sync(const FCombatSnapshot& Snapshot);
    void Enqueue(const TArray<FCombatResult>& Results, const FCombatSnapshot& After);
    void Tick(float DeltaSeconds);
    /** Completes queued steps silently and converges on the committed snapshot. */
    void Finish();
    bool IsBusy() const { return Current < Steps.Num(); }

    void ShowBanner(const FString& Title, const FString& Subtitle, FLinearColor Color, float Life = 1.6f);
    void ShowText(FVector2D Cell, const FString& Text, FLinearColor Color, float Scale = 1.0f, float Life = 1.3f);
    void ShowSparkles(FVector2D Cell, FLinearColor Color, int32 Count = 8);
    /**
     * Capture support (#189): shows a hit on a unit without any rules result, exactly as AddImpact does at its peak (full flash, the
     * flinch clip when the unit stays up, the damage text), and for a spell the bolts built and staggered exactly as AddAttack throws them, from From
     * (a cell), aged to Moment. Only Landing shows the hit; the other moments show bolts alone (no flash, flinch or text).
     * Presentation only: no hit points change. HoldImpact keeps the moment on screen; ReleaseImpact lets it go.
     */
    void PreviewImpact(int32 TargetId, EImpactPreview Effect, FVector2D From, EImpactMoment Moment = EImpactMoment::Landing);

    /** Seconds a Magic Missile bolt takes to arrive, and the delay between its three bolts (production and preview share these). */
    static constexpr float MissileFlight = 0.38f;
    static constexpr float MissileStagger = 0.09f;
    /** Seconds from the start of a Ranged clip to its release frame (frame 26 at 30 fps, shared and the ranger's own): the arrow leaves then. */
    static constexpr float RangedReleaseSeconds = 26.0f / 30.0f;
    /** The three Magic Missile bolts as AddAttack throws them: the second and third start later (negative Age), and arc to either side. */
    static TArray<FPlaybackProjectile> MagicMissileBolts(FVector2D From, FVector2D To);
    /** The bolt of a ranged attack by a role: a rogue's or an enemy archer's arrow, a Cleric's radiant bolt, a Wizard's fire bolt. */
    static FPlaybackProjectile RangedBolt(FVector2D From, FVector2D To, EAdventurerRole Role, bool bArrow);
    /** Capture support (#189): pins the previewed unit's flash at its peak (1) and the previewed bolts just before they land; Tick would otherwise age both away. */
    void HoldImpact(int32 UnitId);
    void ReleaseImpact(int32 TargetId);

    float Speed = 1.0f;
    const FUnitDisplay* Unit(int32 Id) const { return Displays.Find(Id); }
    const TArray<FFloatingText>& Texts() const { return Floaters; }
    const TArray<FPlaybackProjectile>& Projectiles() const { return Shots; }
    const TArray<FPlaybackSparkle>& Sparkles() const { return Glitter; }
    const FDiceTray& Tray() const { return Dice; }
    /** Capture and test support (#190): show this tray as a landed roll and keep it up until cleared. */
    void PreviewTray(FDiceTray Tray, float Age = FDiceTray::TumbleSeconds + 0.5f) { Dice = MoveTemp(Tray); Dice.bVisible = true; Dice.bHeld = true; Dice.Age = Age; }
    void ClearTrayPreview() { if (Dice.bHeld) { Dice = FDiceTray(); } }
    const FTurnBanner& Banner() const { return TurnBanner; }
    /** The combatant the camera should favour while steps play. */
    int32 FocusId() const { return Focus; }

private:
    struct FStep
    {
        float Duration = 0.0f;
        TFunction<void()> Begin;
        TFunction<void(float Alpha)> Update;
    };
    TArray<FStep> Steps;
    int32 Current = 0;
    float StepTime = 0.0f;
    bool bStepBegun = false;
    TMap<int32, FUnitDisplay> Displays;
    TArray<FFloatingText> Floaters;
    TArray<FPlaybackProjectile> Shots;
    TArray<FPlaybackSparkle> Glitter;
    /** Capture support (#189): the bolts HoldImpact keeps in flight. */
    TArray<FPlaybackProjectile> HeldShots;
    bool bHeldFlash = false;
    FDiceTray Dice;
    FTurnBanner TurnBanner;
    int32 Focus = INDEX_NONE;
    bool bSkipping = false;

    void Add(float Duration, TFunction<void()> Begin, TFunction<void(float)> Update = nullptr);
    void Cue(EExpeditionCue Value) const;
    void Shake(float Amount) const;
    void AddResult(const FCombatResult& Result, const FCombatSnapshot& After);
    void AddMove(const FCombatResult& Result);
    void AddAttack(const FCombatResult& Result, const FCombatSnapshot& After);
    void AddHealing(const FCombatResult& Result, const FCombatSnapshot& After);
    void AddImpact(const FCombatResult& Result, FVector2D Direction);
    void AddFalls(const FCombatResult& Result, const FCombatSnapshot& After);
    /** Plays a clip on a figure. Repeating the clip already playing changes nothing. */
    static void SetPose(FUnitDisplay& Unit, EUnitPose Pose);
    /** Moves the pose to the one the committed life implies. A snap also finishes a clip still playing. */
    static void Converge(FUnitDisplay& Unit, bool bSnap);
    void StepRunner(float Seconds);
    void Advance(float Seconds);
};

/** Short display names and colors shared by the view and HUD. */
namespace ExpeditionStyle
{
DNDDICEROLL_API FLinearColor PartyColor(int32 Id);
DNDDICEROLL_API FLinearColor UnitColor(const FCombatantState& Unit);
DNDDICEROLL_API FString ActionName(ECombatCommandType Action);
/** Actions aimed at allies (healing, rescue) rather than enemies. */
DNDDICEROLL_API bool IsFriendlyAction(ECombatCommandType Action, const FCombatantState* Actor = nullptr);
DNDDICEROLL_API FString TargetChip(const FCombatBattle& Rules, int32 ActorId, ECombatCommandType Action, int32 TargetId);
/**
 * "Name: detail" for a target chip, at most MaxWidth wide by Measure. A long name is cut with "..."
 * so the forecast qualifier ("can drop", "crit drop") always stays whole; when even the detail alone
 * does not leave room for a name, the detail stands alone.
 */
DNDDICEROLL_API FString ChipLabel(const FString& Name, const FString& Detail, float MaxWidth, TFunctionRef<float(const FString&)> Measure);
DNDDICEROLL_API FString ForecastAmount(const FActionForecast& Forecast);
DNDDICEROLL_API const FCombatantState* FindUnit(const FCombatSnapshot& Snapshot, int32 Id);
}
