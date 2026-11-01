#pragma once

#include "CoreMinimal.h"
#include "Rules/ProficiencyRules.h"
#include "Rules/CheckRules.h"
#include "Rules/DamageRules.h"
#include "CombatBattle.generated.h"

UENUM()
enum class ECombatSide : uint8 { Party, Enemy };
UENUM()
enum class EBattlePhase : uint8 { Fighting, Victory, Defeat };
UENUM()
enum class ECombatCommandType : uint8 { Move, Attack, EndTurn, SecondWind, CureWounds, MagicMissile, Rescue, Potion, PrecisionStrike, Special1, Special2 };
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
};

USTRUCT()
struct DNDDICEROLL_API FCombatEncounter
{
    GENERATED_BODY()
    UPROPERTY() int32 Width = 8;
    UPROPERTY() int32 Height = 8;
    UPROPERTY() TArray<FIntPoint> BlockedCells;
    UPROPERTY() TArray<FCombatantState> Combatants;
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

/** Named reasons an attack roll has advantage or disadvantage, and the combined SRD mode. */
struct DNDDICEROLL_API FAttackRollMode
{
    ERollMode Mode = ERollMode::Normal;
    TArray<FString> Advantage;
    TArray<FString> Disadvantage;
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
    /** Attack rolls only: every d20 rolled (two with advantage or disadvantage); NaturalRoll is the kept die. */
    TArray<int32> AttackRolls;
    ERollMode RollMode = ERollMode::Normal;
    TArray<FString> RollModeSources;
    /** Damage only: type, the target's response, and the amount before resistance/vulnerability/immunity. */
    EDamageType DamageType = EDamageType::Slashing;
    EDamageResponse DamageResponse = EDamageResponse::Normal;
    int32 DamageBeforeDefenses = 0;
    // Facts about the resolved command for presentation. Rules never read them back.
    ECombatCommandType Type = ECombatCommandType::EndTurn;
    int32 ActorId = INDEX_NONE;
    int32 TargetId = INDEX_NONE;
    /** Move: each cell entered, in order, ending at the destination. */
    TArray<FIntPoint> Path;
    int32 AttackBonus = 0;
    int32 TargetArmorClass = 0;
    /** Cover bonus included in TargetArmorClass. */
    int32 CoverBonus = 0;
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
    /** Exact hit chance over all 400 pairs of d20 faces; HitChancePercent is this rounded. */
    int32 HitChanceOutOf400 = 0;
    ERollMode RollMode = ERollMode::Normal;
    TArray<FString> AdvantageSources;
    TArray<FString> DisadvantageSources;
    bool bHealing = false;
    /** Only the two endpoints can occur (for example Wild Bolt), rather than a dice range. */
    bool bDiscreteAmounts = false;
    int32 MinAmount = 0;
    int32 MaxAmount = 0;
    bool bSneakAttack = false;
    /** Attack rolls only: cover bonus already added to TargetArmorClass. */
    int32 CoverBonus = 0;
    int32 TargetArmorClass = 0;
    /** Damage only: Min/MaxAmount already include the target's response to this type. */
    EDamageType DamageType = EDamageType::Slashing;
    EDamageResponse DamageResponse = EDamageResponse::Normal;
    /** Damage only: the best possible roll, counting a critical hit, reaches remaining health. */
    bool bCanDrop = false;
    FString Formula;
    /** Ordered deterministic area damage and mitigation facts, matching committed effects. */
    TArray<FTargetEffect> Effects;
};

/** Board cells an action reaches from its user, by the same range and sight rules as LegalTargets. */
struct DNDDICEROLL_API FActionReach
{
    /** In range and in sight: a combatant here can be a legal target. */
    TArray<FIntPoint> InSight;
    /** In range but blocked terrain or out of line of sight. */
    TArray<FIntPoint> Obscured;
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
    /** Cells a legal move would enter, in order. Empty when the move is not legal. */
    TArray<FIntPoint> PathTo(int32 Id, FIntPoint Destination) const;
    /** Hit chance and effect range for a legal target. Consumes no random numbers. */
    FActionForecast Forecast(int32 ActorId, ECombatCommandType Action, int32 TargetId) const;
    /** Ability scores: prebuilt by role for adventurers, generic for enemies. Read-only, no RNG. */
    FAbilityScores AbilityScores(int32 Id) const;
    /** SRD ability modifier for a combatant; 0 for unknown IDs. */
    int32 AbilityModifier(int32 Id, EAbility Ability) const;
    /** Proficiency, skill, save and tool facts for a combatant at its current level. Read-only, no RNG. */
    FCharacterRules Character(int32 Id) const;
    /** Damage type of an action: role weapon or cantrip for attacks, force for Magic Missile. Read-only. */
    EDamageType DamageTypeOf(int32 ActorId, ECombatCommandType Action) const;
    /** Resistances, vulnerabilities and immunities derived from the unit's profile (Role). Read-only. */
    FDamageDefenses DamageDefenses(int32 Id) const;
    /** Advantage/disadvantage sources for an attack. Read-only, no RNG. */
    FAttackRollMode AttackRollMode(int32 ActorId, int32 TargetId) const;
    /** SRD cover from blocked cells against an attack: 0, 2 (half) or 5 (three-quarters). Creatures give no cover. Read-only. */
    int32 CoverBonus(int32 ActorId, int32 TargetId) const;
    static const TCHAR* CoverName(int32 Bonus);
    /** Exact success chance (1/400ths) for a skill check or saving throw. Forecasts spend no random numbers. */
    int32 SkillCheckChance(int32 Id, ESkill Skill, int32 Dc, ERollMode Mode) const;
    int32 SavingThrowChance(int32 Id, EAbility Ability, int32 Dc, ERollMode Mode) const;
    /** Rolls a check or save from the battle's seeded stream. Rejected atomically (no RNG spent) for unknown or downed units or DC outside 1..30. */
    bool SkillCheck(int32 Id, ESkill Skill, int32 Dc, ERollMode Mode, FD20Outcome& Out, FString& Error);
    bool SavingThrow(int32 Id, EAbility Ability, int32 Dc, ERollMode Mode, FD20Outcome& Out, FString& Error);
    /** Cells the action could reach now. Empty when LegalTargets would refuse the action outright. Consumes no random numbers. */
    FActionReach Reach(int32 Id, ECombatCommandType Action) const;
    FCombatResult Submit(const FCombatCommand& Command);
    /** Completes at most one enemy turn (move, attack, end). Never drives party turns. */
    TArray<FCombatResult> PlayEnemyTurn();
    /** Player-facing action facts share the tuning used by validation and resolution. */
    static FString DescribeAction(ECombatCommandType Action);
    /** Cost field of DescribeAction, e.g. "Action + 1 slot". */
    static FString CostTag(ECombatCommandType Action);
    /** Remaining-resource text for a unit, e.g. "2 slots left"; empty when the action spends nothing countable. */
    static FString UsageTag(ECombatCommandType Action, const FCombatantState& Unit);
    /** SRD spell facts line (FSpellRules::Summary) for spell actions; empty otherwise. */
    static FString SpellTag(ECombatCommandType Action);
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
    /** The unit when it may use a targeted action now (turn, life, action budget, role, uses); otherwise null. */
    const FCombatantState* ActionUser(int32 Id, ECombatCommandType Action) const;
    static int32 ActionRange(const FCombatantState& Unit, ECombatCommandType Action);
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
    void ApplyDefenses(FCombatResult& Result, int32 ActorId, ECombatCommandType Action, int32 TargetId) const;
    static FString DefenseText(const FCombatResult& Result);
    FCombatResult Resolve(const FCombatCommand& Command);
};
