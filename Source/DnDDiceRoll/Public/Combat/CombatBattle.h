#pragma once

#include "CoreMinimal.h"
#include "CombatBattle.generated.h"

/** Expedition ruleset chosen at NewRun and persisted in schema 5. Compact is the default. */
UENUM()
enum class ERuleset : uint8 { Compact = 0, SRD = 1 };

UENUM()
enum class ECombatSide : uint8 { Party, Enemy };
UENUM()
enum class EBattlePhase : uint8 { Fighting, Victory, Defeat };
UENUM()
enum class ECombatCommandType : uint8 { Move, Attack, EndTurn, SecondWind, CureWounds, MagicMissile, Rescue, Potion, PrecisionStrike, Special1, Special2, Grapple, Shove };
UENUM()
enum class EAdventurerRole : uint8 { None = 0, Fighter = 1, Rogue = 2, Cleric = 3, Wizard = 4, Barbarian = 5, Monk = 6, Paladin = 7, Ranger = 8, Sorcerer = 9, Warlock = 10, Druid = 11, Bard = 12, Artificer = 13 };
UENUM()
enum class EFighterSubclass : uint8 { None, Champion, BattleMaster };
UENUM()
enum class ECombatLife : uint8 { Conscious, Downed, Dead };

/** Serializable combat and persistent adventurer values; IDs never change between encounters. */
USTRUCT()
struct DNDDICEROLL_API FCombatantState
{
    GENERATED_BODY()
    UPROPERTY() int32 Id = INDEX_NONE;
    UPROPERTY() FString Name;
    UPROPERTY() FString Role;
    UPROPERTY() FString BuildId;
    UPROPERTY() FString ClassId;
    UPROPERTY() FString SubclassId;
    UPROPERTY() EAdventurerRole AdventurerRole = EAdventurerRole::None;
    UPROPERTY() ECombatLife Life = ECombatLife::Conscious;
    UPROPERTY() int32 RescueTurnsLeft = 0;
    UPROPERTY() int32 SecondWindUses = 0;
    UPROPERTY() int32 SpellSlots = 0;
    UPROPERTY() int32 Potions = 0;
    UPROPERTY() int32 WeaponRank = 0;
    UPROPERTY() int32 Level = 1;
    UPROPERTY() EFighterSubclass FighterSubclass = EFighterSubclass::None;
    UPROPERTY() int32 ManeuverUses = 0;
    UPROPERTY() int32 KitUses = 0;
    UPROPERTY() int32 GuardTurns = 0;
    UPROPERTY() int32 GuardAmount = 0;
    UPROPERTY() int32 RageUses = 0;
    UPROPERTY() int32 RageTurns = 0;
    UPROPERTY() int32 GuardTargetId = INDEX_NONE;
    UPROPERTY() int32 LastSurge = 0;
    UPROPERTY() bool bSneakWeapon = false;
    UPROPERTY() ECombatSide Side = ECombatSide::Party;
    UPROPERTY() FIntPoint Cell = FIntPoint::ZeroValue;
    UPROPERTY() int32 MaxHP = 20;
    UPROPERTY() int32 HP = 20;
    UPROPERTY() int32 ArmorClass = 12;
    UPROPERTY() int32 AttackBonus = 5;
    UPROPERTY() int32 DamageDie = 6;
    UPROPERTY() int32 DamageBonus = 3;
    UPROPERTY() int32 Range = 1;
    UPROPERTY() int32 Speed = 4;
    UPROPERTY() int32 InitiativeBonus = 0;
    UPROPERTY() int32 Initiative = 0;
    /** SRD reaction economy; Compact ignores. Default true so Compact snapshots stay valid. */
    UPROPERTY() bool bReactionAvailable = true;
    UPROPERTY() int32 DeathSaveSuccesses = 0;
    UPROPERTY() int32 DeathSaveFailures = 0;
    UPROPERTY() int32 TemporaryHP = 0;
    UPROPERTY() bool bConcentrating = false;
    /** ECreatureSize index; 2 = Medium. Used by cover/grapple house rules in SRD mode. */
    UPROPERTY() int32 Size = 2;
    UPROPERTY() bool bSurprised = false;
    UPROPERTY() bool bDisengaged = false;
};

USTRUCT()
struct DNDDICEROLL_API FCombatEncounter
{
    GENERATED_BODY()
    UPROPERTY() int32 Width = 8;
    UPROPERTY() int32 Height = 8;
    UPROPERTY() TArray<FIntPoint> BlockedCells;
    UPROPERTY() TArray<FCombatantState> Combatants;
    /** Compact default keeps seed-identity with staging; SRD enables battle wiring. */
    UPROPERTY() ERuleset Ruleset = ERuleset::Compact;
    /** When true in SRD mode, enemies may be surprised (Initiative Disadvantage). Compact ignores. */
    UPROPERTY() bool bAmbush = false;
    /** Party Stealth totals used when bAmbush is set (SRD surprise). */
    UPROPERTY() TArray<int32> AmbushStealthTotals;
};

/** Value-only reflected state: no actors, pointers, or implicit global randomness. */
USTRUCT()
struct DNDDICEROLL_API FCombatSnapshot
{
    GENERATED_BODY()
    UPROPERTY() FCombatEncounter Encounter;
    UPROPERTY() int32 RandomState = 0;
    UPROPERTY() TArray<int32> TurnOrder;
    UPROPERTY() int32 TurnIndex = 0;
    UPROPERTY() int32 Round = 1;
    UPROPERTY() int32 MovementLeft = 0;
    UPROPERTY() bool bActionAvailable = true;
    UPROPERTY() bool bBonusActionAvailable = true;
    UPROPERTY() EBattlePhase Phase = EBattlePhase::Fighting;
};

struct DNDDICEROLL_API FCombatCommand
{
    ECombatCommandType Type = ECombatCommandType::EndTurn;
    int32 ActorId = INDEX_NONE;
    FIntPoint Destination = FIntPoint::ZeroValue;
    int32 TargetId = INDEX_NONE;
};

/** One die in an effect roll, in the order it was rolled. */
struct DNDDICEROLL_API FDieRoll
{
    int32 Sides = 0;
    int32 Value = 0;
};

enum class EAbilityEffect : uint8 { Rage, Strike, Protect, Surge, Healing, WeaponAttack, Missile, Swap, Push, Area, Mitigation };
enum class EAbilityTarget : uint8 { Self, Enemy, Ally, Friendly };
struct DNDDICEROLL_API FAbilityFacts
{
    FString Name, Description, ResourceName;
    int32 ResourceCost = 0;
    ECombatCommandType Command = ECombatCommandType::Special1;
    EAbilityEffect Effect = EAbilityEffect::Rage;
    EAbilityTarget Target = EAbilityTarget::Self;
    bool bKitResource = false;
    bool bCureDice = false;
    bool bBonus = true;
    bool bResourceAvailable = false;
    int32 Range = 0, Amount = 0;
};
struct DNDDICEROLL_API FTargetEffect
{
    int32 TargetId = INDEX_NONE;
    EAbilityEffect Effect = EAbilityEffect::Strike;
    int32 Amount = 0;
    FIntPoint Cell = FIntPoint::ZeroValue;
};

struct DNDDICEROLL_API FCombatResult
{
    bool bAccepted = false;
    FString Feedback;
    int32 NaturalRoll = 0;
    int32 AttackTotal = 0;
    int32 Damage = 0;
    bool bHit = false;
    bool bCritical = false;
    int32 Healing = 0;
    bool bSneakAttack = false;
    // Facts about the resolved command for presentation. Rules never read them back.
    ECombatCommandType Type = ECombatCommandType::EndTurn;
    int32 ActorId = INDEX_NONE;
    int32 TargetId = INDEX_NONE;
    /** Move: each cell entered, in order, ending at the destination. */
    TArray<FIntPoint> Path;
    int32 AttackBonus = 0;
    int32 TargetArmorClass = 0;
    /** Damage or healing dice in roll order; the effect is their sum plus EffectModifier. */
    TArray<FDieRoll> EffectDice;
    int32 EffectModifier = 0;
    /** Committed damage reduction, after dice/modifiers and before the zero floor. */
    int32 DamagePrevented = 0;
    /** Combatants newly downed or killed by this command, including turn countdowns and battle end. */
    TArray<int32> Downed;
    TArray<int32> Died;
    TArray<FTargetEffect> Effects;
    int32 SurgeOutcome = 0;
};

/** Player-facing expectation for one legal action. Uses the same tuning as resolution. */
struct DNDDICEROLL_API FActionForecast
{
    bool bValid = false;
    bool bAttackRoll = false;
    int32 HitChancePercent = 0;
    bool bHealing = false;
    /** Only the two endpoints can occur (for example Wild Bolt), rather than a dice range. */
    bool bDiscreteAmounts = false;
    int32 MinAmount = 0;
    int32 MaxAmount = 0;
    bool bSneakAttack = false;
    /** Damage only: the best possible roll, counting a critical hit, reaches remaining health. */
    bool bCanDrop = false;
    FString Formula;
    /** Ordered deterministic area damage and mitigation facts, matching committed effects. */
    TArray<FTargetEffect> Effects;
};

/** Geometry only: independent of budgets, resources and occupant eligibility. */
struct DNDDICEROLL_API FActionReach
{
    TArray<FIntPoint> Visible;
    TArray<FIntPoint> Obstructed;
};

/** Authoritative combat module. Start rejects invalid encounters atomically. All commands,
 * including enemy decisions, use the same validation. Queries consume no random numbers.
 * Grid: cardinal movement, occupied cells block travel, attacks use Manhattan range and
 * blocked-cell line of sight. Limits: 16x16 cells, 32 combatants, stats bounded at Start.
 */
class DNDDICEROLL_API FCombatBattle
{
public:
    bool Start(const FCombatEncounter& Encounter, int32 Seed, FString& Error);
    bool Restore(const FCombatSnapshot& Snapshot, FString& Error);
    const FCombatSnapshot& Snapshot() const { return State; }
    const FCombatantState* Find(int32 Id) const;
    int32 ActiveId() const;
    TArray<FIntPoint> LegalMoves(int32 Id) const;
    TArray<int32> LegalTargets(int32 Id, ECombatCommandType Action = ECombatCommandType::Attack) const;
    FActionReach ActionReach(int32 ActorId, ECombatCommandType Action) const;
    /** Cells a legal move would enter, in order. Empty when the move is not legal. */
    TArray<FIntPoint> PathTo(int32 Id, FIntPoint Destination) const;
    /** Hit chance and effect range for a legal target. Consumes no random numbers. */
    FActionForecast Forecast(int32 ActorId, ECombatCommandType Action, int32 TargetId) const;
    FCombatResult Submit(const FCombatCommand& Command);
    /** Completes at most one enemy turn (move, attack, end). Never drives party turns. */
    TArray<FCombatResult> PlayEnemyTurn();
    /** Player-facing action facts share the tuning used by validation and resolution. */
    static FString DescribeAction(ECombatCommandType Action);
    static FCombatEncounter TutorialEncounter();
private:
    friend class FPartyContinuity;
    friend class FExpeditionRun;
    FCombatSnapshot State;
    bool bStarted = false;
    /** Set only while resolving an effect so its dice can be reported. */
    TArray<FDieRoll>* DiceLog = nullptr;
    static bool ValidateEncounter(const FCombatEncounter& Encounter, FString& Error);
    bool IsInside(FIntPoint Cell) const;
    bool IsOpen(FIntPoint Cell, int32 MovingId) const;
    TMap<FIntPoint, int32> Distances(int32 Id, TMap<FIntPoint, FIntPoint>* Parents = nullptr) const;
    bool HasLineOfSight(FIntPoint From, FIntPoint To) const;
    bool CanTargetFrom(const FCombatantState& Attacker, FIntPoint Origin, const FCombatantState& Target) const;
    bool SneakAttackApplies(const FCombatantState& Actor, const FCombatantState& Target) const;
    int32 Roll(int32 Sides);
    int32 RollDice(int32 Count, int32 Sides);
    void AdvanceTurn();
    void CheckOutcome();
    TArray<int32> OrderedAreaTargets(const FCombatantState& Actor, const FCombatantState& Target) const;
    int32 ProtectionFor(const FCombatantState& Target) const;
    void ApplyDamage(FCombatantState& Target, int32 Damage);
    FCombatantState* MutableFind(int32 Id);
    FCombatResult Resolve(const FCombatCommand& Command);
    bool IsSrd() const { return State.Encounter.Ruleset == ERuleset::SRD; }
    int32 CoverAcBonus(int32 ActorId, int32 TargetId) const;
    void ResolveOpportunityAttacks(FCombatantState& Mover, const TArray<FIntPoint>& Path, FCombatResult& Result);
    void DownedTurn(FCombatantState& Unit);
};
