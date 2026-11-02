#pragma once

#include "CoreMinimal.h"
#include "Rules/ProficiencyRules.h"
#include "Rules/CheckRules.h"
#include "Rules/DamageRules.h"
#include "Rules/StandardActionRules.h"
#include "CombatBattle.generated.h"

UENUM()
enum class ECombatSide : uint8 { Party, Enemy };
UENUM()
enum class EBattlePhase : uint8 { Fighting, Victory, Defeat };
UENUM()
enum class ECombatCommandType : uint8 { Move, Attack, EndTurn, SecondWind, CureWounds, MagicMissile, Rescue, Potion, PrecisionStrike, Special1, Special2, BurningHands, Thunderwave, Shove, Grapple, Dash, Disengage, Dodge, Help, Hide, Ready, OffHandAttack,
    /** Level 1-5 SRD class features (Party/FeatureKit). Append only. */
    ActionSurge, TurnUndead, BardicInspiration, WildShape, FlurryOfBlows, PatientDefense, StepOfTheWind, FontOfMagic, UncannyDodge, RecklessAttack, MartialArts, DeflectMissiles, SacredWeapon, QuickenedSpell, FlexibleCasting, Counterspell, CastSpell };
UENUM()
enum class EAdventurerRole : uint8 { None = 0, Fighter = 1, Rogue = 2, Cleric = 3, Wizard = 4, Barbarian = 5, Monk = 6, Paladin = 7, Ranger = 8, Sorcerer = 9, Warlock = 10, Druid = 11, Bard = 12, Artificer = 13 };
UENUM()
enum class EFighterSubclass : uint8 { None, Champion, BattleMaster };
UENUM()
enum class ECombatLife : uint8 { Conscious, Downed, Dead };
/** SRD conditions tracked in combat. Values are persisted (schema 5); append only. */
UENUM()
enum class ECombatCondition : uint8 { Prone, Poisoned, Blinded, Frightened, Restrained, Grappled, Incapacitated, Stunned, Paralyzed, Unconscious };

/** One active condition: its source (INDEX_NONE when none) and remaining owner turns (-1 until removed). */
USTRUCT()
struct DNDDICEROLL_API FCombatCondition
{
    GENERATED_BODY()
    UPROPERTY() ECombatCondition Condition = ECombatCondition::Prone;
    UPROPERTY() int32 SourceId = INDEX_NONE;
    UPROPERTY() int32 RoundsLeft = -1;
};

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
    /** Per-level slot pool (#71): empty, or one count per spell level 1..FSpellRules::MaxSpellLevel (legacy saves: 3) within FSpellRules::SlotsFor; level 1 mirrors SpellSlots. */
    UPROPERTY() TArray<int32> SlotsByLevel;
    UPROPERTY() int32 Potions = 0;
    UPROPERTY() int32 WeaponRank = 0;
    UPROPERTY() int32 Level = 1;
    /** #79: experience points; a party hero's level is FLevelRules::LevelForXp(Xp). */
    UPROPERTY() int32 Xp = 0;
    /** #79 multiclassing: levels taken in a second class (Level counts both classes). */
    UPROPERTY() EAdventurerRole SecondaryRole = EAdventurerRole::None;
    UPROPERTY() int32 SecondaryLevel = 0;
    /** #79 ability score improvements, as (first, second) EAbility pairs replayed onto the prebuilt scores. */
    UPROPERTY() TArray<uint8> AsiChoices;
    /** #79: the character level whose multiclass choice was spent (0 = none yet). */
    UPROPERTY() int32 ChoiceLevel = 0;
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
    /** Equipment (#69/#70): EWeapon index, or -1 to roll the fixed DamageDie. */
    UPROPERTY() int32 MainHand = -1;
    /** EArmor index below Shield, or -1 to use the fixed ArmorClass. */
    UPROPERTY() int32 ArmorWorn = -1;
    /** A shield adds its bonus and blocks two-handed weapons. */
    UPROPERTY() bool bShieldEquipped = false;
    /** Pieces of ammunition for an Ammunition weapon (0-99); one is spent per shot. */
    UPROPERTY() int32 Ammunition = 0;
    UPROPERTY() int32 Speed = 4;
    UPROPERTY() int32 InitiativeBonus = 0;
    UPROPERTY() int32 Initiative = 0;
    UPROPERTY() TArray<FCombatCondition> Conditions;
    /** Reaction budget; spent by an opportunity attack and restored at the owner's turn start. */
    UPROPERTY() bool bReactionAvailable = true;
    /** SRD standard actions (#60). Dodge, Disengage and Ready last until the owner's next turn start; Hide until it attacks. */
    UPROPERTY() bool bDodging = false;
    /** Barbarian 2 Reckless Attack: advantage on own melee attacks this turn, attacks against it have advantage until its next turn. */
    UPROPERTY() bool bReckless = false;
    /** Paladin 3 Channel Divinity (Sacred Weapon): own turns left with +CHA (min +1) on weapon attack rolls. */
    UPROPERTY() int32 SacredWeaponTurns = 0;
    /** Sorcerer 3 Quickened Spell: 0 none, 1 the next kit spell this turn costs the bonus action, 2 spent this turn. Cleared at turn start. */
    UPROPERTY() int32 QuickenState = 0;
    /** #79 Shield spell (reaction, 1st-level slot): +5 AC until the caster's next turn start. */
    UPROPERTY() bool bShielded = false;
    UPROPERTY() bool bDisengaged = false;
    UPROPERTY() bool bHidden = false;
    UPROPERTY() bool bReadied = false;
    /** SRD death saves (#67, used when the encounter opts in): three successes leave a downed hero stable. */
    UPROPERTY() int32 DeathSaveSuccesses = 0;
    UPROPERTY() int32 DeathSaveFailures = 0;
    /** Temporary hit points soak damage before real hit points (#67). */
    UPROPERTY() int32 TemporaryHP = 0;
    /** Hit dice spent on short rests (#76); a hero has one die per level. */
    UPROPERTY() int32 HitDiceSpent = 0;
    /** #74: holding a concentration spell; damage forces a CON save (DC max(10, half)), 0 HP ends it. */
    UPROPERTY() bool bConcentrating = false;
    /** #62: off-hand weapon (EWeapon index, -1 none) for two-weapon fighting. */
    UPROPERTY() int32 OffHand = -1;
    /** #66: creature size (ECreatureSize index, 2 = Medium); grapple and shove reach at most one size larger. */
    UPROPERTY() int32 Size = 2;
    /** #80: surprised; skips its first-round turn. */
    UPROPERTY() bool bSurprised = false;
    /** Heroic inspiration held (#87): spent automatically for advantage on the next attack roll. */
    UPROPERTY() bool bInspired = false;
    /** Pack weight in pounds beyond potions and shield (#89 encumbrance variant). */
    UPROPERTY() int32 CarriedPounds = 0;
    /** Set on an enemy by the Help action: the helper's Id; the next attack by one of the helper's allies has advantage. */
    UPROPERTY() int32 HelpedById = INDEX_NONE;
    /** SRD Fighting Style (EFightingStyle in Party/FeatureKit.h; 0 none). Fighter 1, Paladin 2, Ranger 2. */
    UPROPERTY() int32 FightingStyle = 0;
    /** Fighter 2: Action Surge uses left (1 per short or long rest). */
    UPROPERTY() int32 ActionSurgeUses = 0;
    /** Cleric 2: Channel Divinity uses left (1 per short or long rest). */
    UPROPERTY() int32 ChannelUses = 0;
    /** Bard 1: Bardic Inspiration uses left (CHA modifier, min 1, per long rest). */
    UPROPERTY() int32 BardicUses = 0;
    /** A held Bardic Inspiration die (sides; 0 = none), spent on the next attack roll it can turn into a hit. */
    UPROPERTY() int32 BardicDie = 0;
    /** Druid 2: Wild Shape uses left (2 per short or long rest). */
    UPROPERTY() int32 WildShapeUses = 0;
    /** Hit points of the current beast form (wolf); 0 = own form. Damage drains it first, the excess carries over. */
    UPROPERTY() int32 WildShapeHP = 0;
    /** Monk 2: ki points left (Monk level per short or long rest). */
    UPROPERTY() int32 KiPoints = 0;
    /** Sorcerer 2: sorcery points left (Sorcerer level per long rest). */
    UPROPERTY() int32 SorceryPoints = 0;
    /** Wizard 1 Arcane Recovery taken since the last long rest. */
    UPROPERTY() bool bArcaneRecovered = false;
    /** Warlock 2 Magical Cunning taken since the last long rest. */
    UPROPERTY() bool bMagicalCunningUsed = false;
    /** Warlock 1 Eldritch Invocations held (EInvocation values; SRD 5.2.1 count and prerequisites). */
    UPROPERTY() TArray<int32> Invocations;
};

USTRUCT()
struct DNDDICEROLL_API FCombatEncounter
{
    GENERATED_BODY()
    UPROPERTY() int32 Width = 8;
    UPROPERTY() int32 Height = 8;
    UPROPERTY() TArray<FIntPoint> BlockedCells;
    /** Difficult terrain (#64): entering one of these open cells costs two movement. */
    UPROPERTY() TArray<FIntPoint> DifficultCells;
    UPROPERTY() TArray<FCombatantState> Combatants;
    /** #67: downed heroes roll SRD death saves instead of the three-turn rescue clock. Off by default to keep seeded battles. */
    UPROPERTY() bool bDeathSaves = false;
    /** #63: other creatures in the line of attack give half cover (+2). Off by default to keep seeded results. */
    UPROPERTY() bool bCreatureCover = false;
    /** #78/#79: the Battle Master maneuver spends a superiority die (d8) added to the attack roll (SRD Precision Attack). On by default; off restores the old flat +2 hit/damage. */
    UPROPERTY() bool bSuperiorityDice = true;
    /** #80: the party sneaks up: party Stealth vs each foe's passive Perception; surprised foes lose round 1. Off by default. */
    UPROPERTY() bool bPartyAmbush = false;
    /** Off by default: a natural 20 grants inspiration, spent for advantage on the holder's next attack. */
    UPROPERTY() bool bInspiration = false;
    /** #77/#79: Sneak Attack scales with level (SRD ceil(level/2) d6, once per turn). On by default; off keeps a flat single die. */
    UPROPERTY() bool bScaledSneakAttack = true;
    /** Off by default: the whole field is dark; only darkvision (by role) sees, which sets attack advantage/disadvantage. */
    UPROPERTY() bool bDarkness = false;
    /** Off by default: variant encumbrance slows units by carried weight (pack + potions + shield) against Strength. */
    UPROPERTY() bool bEncumbrance = false;
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
    /** #62: the active unit took the Attack action this turn (enables the off-hand bonus attack). */
    UPROPERTY() bool bAttackActionTaken = false;
    /** Extra Attack: weapon attacks left after the Attack action was taken this turn (#61). */
    UPROPERTY() int32 AttacksRemaining = 0;
    /** Combatants that already dealt Sneak Attack this turn (once per turn, reactions included); cleared at each turn start. */
    UPROPERTY() TArray<int32> SneakAttackUsedBy;
    UPROPERTY() EBattlePhase Phase = EBattlePhase::Fighting;
};

struct DNDDICEROLL_API FCombatCommand
{
    ECombatCommandType Type = ECombatCommandType::EndTurn;
    int32 ActorId = INDEX_NONE;
    FIntPoint Destination = FIntPoint::ZeroValue;
    int32 TargetId = INDEX_NONE;
    /** Spells only: slot level to cast with; 0 picks the lowest open slot. Not persisted. */
    int32 SlotLevel = 0;
    /** CastSpell only: SRD catalog slug or name (FSpellCatalogRules::Find). Not persisted. */
    FString SpellId;
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
    /** #79: the economy tag; always agrees with bBonus. */
    /** Rides on an attack it modifies (Precision Attack); costs no action of its own. */
    bool bRider = false;
    EActionCost Cost() const { return bRider ? EActionCost::Free : bBonus ? EActionCost::BonusAction : EActionCost::Action; }
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

/** An opportunity attack made during a move, reported for presentation. */
struct DNDDICEROLL_API FReactionStrike
{
    int32 ReactorId = INDEX_NONE;
    /** The cell the mover was leaving when struck. */
    FIntPoint Cell = FIntPoint::ZeroValue;
    int32 NaturalRoll = 0;
    int32 AttackTotal = 0;
    int32 TargetArmorClass = 0;
    bool bHit = false;
    bool bCritical = false;
    int32 Damage = 0;
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
    /** #79: the target cast Shield as a reaction, turning this hit into a miss. */
    bool bShieldReaction = false;
    /** A stat block save cantrip (Sacred Flame): AttackTotal is the save total, TargetArmorClass the DC, bHit a failed save. */
    bool bSaveSpell = false;
    /** The spell was stopped by a hero's Counterspell reaction. */
    bool bCounterspelled = false;
    /** Bardic Inspiration die rolled and added to this attack (0 = none). */
    int32 BardicRoll = 0;
    /** College of Lore 3 Cutting Words die subtracted from this attack by a bard's reaction (0 = none). */
    int32 CuttingRoll = 0;
    /** An adjacent ally used the Protection fighting style (reaction) to impose disadvantage. */
    bool bProtectionReaction = false;
    /** Uncanny Dodge (Rogue 5) halved this hit's damage with the target's reaction. */
    bool bUncannyDodge = false;
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
    /** Move only: opportunity attacks in the order they happened. */
    TArray<FReactionStrike> Reactions;
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
    /** SlotLevel (spells only): 0 = lowest open slot, as in FCombatCommand. */
    FActionForecast Forecast(int32 ActorId, ECombatCommandType Action, int32 TargetId, int32 SlotLevel = 0) const;
    /** CastSpell: catalog spells the active hero can cast now (class list at that class's level, combat-resolvable, action budget, an open slot, armor). */
    TArray<FString> CastableSpells(int32 Id) const;
    /** CastSpell targets: foes for damage/condition spells, allies (downed too) for healing; range in 5-ft cells and sight; Self areas need the foe inside the template. */
    TArray<int32> SpellTargets(int32 Id, const FString& SpellId, int32 SlotLevel = 0) const;
    /** Upcast picker: the next slot level above Current with a slot left, wrapping to 0 (lowest open). */
    int32 NextSlotLevel(int32 Id, int32 Current) const;
    /** Ability scores: prebuilt by role for adventurers, generic for enemies. Read-only, no RNG. */
    FAbilityScores AbilityScores(int32 Id) const;
    /** SRD ability modifier for a combatant; 0 for unknown IDs. */
    int32 AbilityModifier(int32 Id, EAbility Ability) const;
    /** SRD Cure Wounds: 1d8 + the caster's spellcasting (WIS) modifier; multiclass heroes use their real WIS, unbuilt fixtures keep the prebuilt +3. */
    int32 CureModifier(int32 Id) const;
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
    /** #79 action economy: what the command costs this unit now (Cunning Action makes Dash/Disengage/Hide a bonus action once the action is spent). */
    static EActionCost ActionCost(ECombatCommandType Action, const FCombatantState& Unit, bool bActionAvailable = true);
    /** #79 SRD Shield: a conscious wizard (either class) with its reaction and a slot casts it when +5 AC turns a non-critical hit into a miss. */
    static bool ShieldWouldBlock(const FCombatantState& Target, int32 AttackTotal, int32 ArmorClass, bool bCritical);
    /** Tooltip prefix such as "[Bonus Action] ". */
    static FString CostPrefix(ECombatCommandType Action, const FCombatantState& Unit, bool bActionAvailable = true);
    /** SRD spell facts line (FSpellRules::Summary) for spell actions; empty otherwise. */
    static FString SpellTag(ECombatCommandType Action);
    static FCombatEncounter TutorialEncounter();
    /** Spends one 1st-level slot from SpellSlots and, when present, the per-level pool. No-op when empty. */
    static void SpendSpellSlot(FCombatantState& Unit);
    /** Damage dice for one weapon hit before a critical: the MainHand dice (versatile two-handed without a shield), else 1dDamageDie. */
    static void WeaponDice(const FCombatantState& Unit, int32& OutCount, int32& OutSides);
    /** AC from worn armor, shield and Dexterity; legacy units use ArmorClass (+2 with a shield). 0 for unknown IDs. */
    int32 EffectiveArmorClass(int32 Id) const;
    /** Action range in cells; weapon attacks use the MainHand reach (melee) or long range (ranged), else Range. */
    static int32 ActionRange(const FCombatantState& Unit, ECombatCommandType Action);
    // HUD facts (#2). Read-only, no RNG.
    /** Enemies whose reaction a mover would provoke walking Path from its current cell, in the order the move would meet them. */
    TArray<int32> OpportunityThreats(int32 MoverId, const TArray<FIntPoint>& Path) const;
    bool IsDifficultTerrain(FIntPoint Cell) const { return StepCost(Cell) > 1; }
    static const TCHAR* ConditionName(ECombatCondition Condition);
    /** "Poisoned (2 rounds), Prone"; empty without conditions. */
    FString StatusText(int32 Id) const;
    /** "AC 18 (Chain Mail, shield)" from worn armor, else "AC 13". */
    FString ArmorClassText(int32 Id) const;
private:
    const struct FSrdSpell* UsableSpell(const FCombatantState& Unit, const FString& SpellId, int32 RequestedSlot, int32& OutSlot, EAdventurerRole& OutClass) const;
    bool ResolveCatalogSpell(const FCombatCommand& Command, FCombatResult& Result);
    /** Ends the caster's concentration and the conditions its spell holds (RoundsLeft -1, caster as source). */
    void EndConcentration(FCombatantState& Caster);
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
    /** Movement spent to enter Cell. */
    int32 StepCost(FIntPoint Cell) const;
    /** The unit when it may use a targeted action now (turn, life, action budget, role, uses); otherwise null. */
    const FCombatantState* ActionUser(int32 Id, ECombatCommandType Action) const;
    /** Class feature commands (FFeatureKit): whether the unit may use it now, its legal targets, and its effect. */
    bool FeatureReady(const FCombatantState& Unit, ECombatCommandType Action) const;
    TArray<int32> FeatureTargets(const FCombatantState& Unit, ECombatCommandType Action) const;
    FString ResolveFeature(const FCombatCommand& Command, FCombatResult& Result);
    bool CanTargetFrom(const FCombatantState& Attacker, FIntPoint Origin, const FCombatantState& Target) const;
    bool SneakAttackApplies(const FCombatantState& Actor, const FCombatantState& Target) const;
    int32 Roll(int32 Sides);
    int32 RollDice(int32 Count, int32 Sides);
    void AdvanceTurn();
    void DownedTurn(FCombatantState& Unit);
    /** Starts the active unit's turn: movement from speed, grapple/restraint, and standing up from prone. */
    void BeginTurn();
    /** One weapon attack roll and its damage, without spending any budget. Shared by Attack and reactions. */
    void Strike(int32 ActorId, int32 TargetId, int32 ManeuverBonus, FCombatResult& Result, bool bOffHand = false);
    /** Drops conditions whose source or situation no longer supports them. */
    void PruneConditions();
    static bool HasCondition(const FCombatantState& Unit, ECombatCondition Condition);
    /** SRD: incapacitated, stunned, paralyzed and unconscious creatures take no actions or reactions. */
    static bool IsIncapacitated(const FCombatantState& Unit);
    /** Stunned, paralyzed and unconscious creatures cannot move and auto-fail STR and DEX saves. */
    static bool IsHelpless(const FCombatantState& Unit);
    ERollMode SaveMode(const FCombatantState& Unit, EAbility Ability, ERollMode Requested) const;
    ERollMode CheckMode(const FCombatantState& Unit, ERollMode Requested) const;
    void CheckOutcome();
    TArray<int32> OrderedAreaTargets(const FCombatantState& Actor, const FCombatantState& Target) const;
    int32 ProtectionFor(const FCombatantState& Target) const;
    void ApplyDamage(FCombatantState& Target, int32 Damage);
    FCombatantState* MutableFind(int32 Id);
    void ApplyDefenses(FCombatResult& Result, int32 ActorId, ECombatCommandType Action, int32 TargetId) const;
    static FString DefenseText(const FCombatResult& Result);
    FCombatResult Resolve(const FCombatCommand& Command);
};
