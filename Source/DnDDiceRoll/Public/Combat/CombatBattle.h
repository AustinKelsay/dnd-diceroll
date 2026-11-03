#pragma once

#include "CoreMinimal.h"
#include "Rules/ProficiencyRules.h"
#include "Rules/CheckRules.h"
#include "Rules/DamageRules.h"
#include "Rules/StandardActionRules.h"
#include "Rules/SpellCatalogRules.h"
#include "Rules/SpellEffectRules.h"
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
enum class ECombatCondition : uint8 { Prone, Poisoned, Blinded, Frightened, Restrained, Grappled, Incapacitated, Stunned, Paralyzed, Unconscious, Charmed, Deafened, Invisible, Petrified };

/** One active condition: its source (INDEX_NONE when none) and remaining owner turns (-1 until removed). */
USTRUCT()
struct DNDDICEROLL_API FCombatCondition
{
    GENERATED_BODY()
    UPROPERTY() ECombatCondition Condition = ECombatCondition::Prone;
    UPROPERTY() int32 SourceId = INDEX_NONE;
    UPROPERTY() int32 RoundsLeft = -1;
};

/**
 * Character-creator choices for a custom hero (local proposal, not yet approved scope). Empty = a catalog prebuilt.
 * Additive within save schema 5: missing in older v5 files is read as empty (see ExpeditionSave ImportStruct).
 * Plain int32 arrays because the strict save codec only handles int/bool/string/enum/struct/array.
 */
USTRUCT()
struct DNDDICEROLL_API FHeroSheet
{
    GENERATED_BODY()
    /** Six point-buy base scores in EAbility order (8..15, 27 points). Replaces the prebuilt scores; ASIs still replay on top. */
    UPROPERTY() TArray<int32> BaseScores;
    /** Level-1 class skill picks (ESkill indices). */
    UPROPERTY() TArray<int32> ClassSkills;
    /** Level-1 Expertise picks (ESkill indices; Rogue only at level 1). Later Expertise is filled deterministically. */
    UPROPERTY() TArray<int32> ExpertiseSkills;
    /** Cosmetic palette indices: primary, secondary, accent. */
    UPROPERTY() TArray<int32> Colors;

    bool IsCustom() const { return BaseScores.Num() == 6; }
    bool IsEmpty() const { return BaseScores.IsEmpty() && ClassSkills.IsEmpty() && ExpertiseSkills.IsEmpty() && Colors.IsEmpty(); }
    bool SameAs(const FHeroSheet& Other) const { return BaseScores == Other.BaseScores && ClassSkills == Other.ClassSkills && ExpertiseSkills == Other.ExpertiseSkills && Colors == Other.Colors; }
    /** Base scores as FAbilityScores; false unless IsCustom. */
    bool Scores(FAbilityScores& Out) const
    {
        if (!IsCustom()) { return false; }
        Out.Strength = BaseScores[0]; Out.Dexterity = BaseScores[1]; Out.Constitution = BaseScores[2];
        Out.Intelligence = BaseScores[3]; Out.Wisdom = BaseScores[4]; Out.Charisma = BaseScores[5];
        return true;
    }
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
    /** House rule (bTwoRoundDowned): the turn-order slot active when this unit dropped; its countdown ticks each time that slot comes round. INDEX_NONE = its own slot, or not downed. */
    UPROPERTY() int32 DownedAtSlot = INDEX_NONE;
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
    /** Character creator (local proposal): custom base scores, skills and colours. Empty for catalog prebuilts. */
    UPROPERTY() FHeroSheet Sheet;
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
    /** Prepared/known spell ids (catalog slugs, cantrips included). Empty = every class-list spell the hero can cast; when set, only these, within the SRD cantrip and prepared counts. */
    UPROPERTY() TArray<FString> PreparedSpells;
};

/** A spell still in effect after its casting turn (SRD 5.2.1 durations). Zones, held conditions and buffs live here; the battle ticks them. */
USTRUCT()
struct DNDDICEROLL_API FActiveSpell
{
    GENERATED_BODY()
    /** Catalog slug. */
    UPROPERTY() FString SpellId;
    UPROPERTY() int32 CasterId = INDEX_NONE;
    /** Creature the spell holds (repeat saves); INDEX_NONE for zones. */
    UPROPERTY() int32 TargetId = INDEX_NONE;
    UPROPERTY() int32 SlotLevel = 0;
    /** Caster turn starts left before it ends; -1 = until dispelled or concentration ends. */
    UPROPERTY() int32 RoundsLeft = -1;
    UPROPERTY() bool bConcentration = false;
    /** Zones: centre cell (follows the caster for emanations) and radius in cells. */
    UPROPERTY() FIntPoint Origin = FIntPoint::ZeroValue;
    UPROPERTY() int32 RadiusCells = 0;
    /** Save DC and spell attack bonus frozen at casting. */
    UPROPERTY() int32 Dc = 0;
    UPROPERTY() int32 AttackBonus = 0;
    /** Spell-specific payload (pool left, chosen option, duplicates). */
    UPROPERTY() int32 Value = 0;
    /** Creatures already affected this turn (once-per-turn zone triggers); cleared at every turn start. */
    UPROPERTY() TArray<int32> TriggeredThisTurn;
    UPROPERTY() int32 Serial = 0;
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
    /** HOUSE RULE, not SRD (needs Austin's sign-off): no death saves; a hero at 0 HP is Downed for 2 full rounds, any healing revives them at the healed HP, and any attack or damage while Downed kills them. On by default; ignored when bDeathSaves is on. Off restores the 3-turn rescue clock. */
    UPROPERTY() bool bTwoRoundDowned = true;
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
    /** Spells in effect (durations, zones, held conditions). Empty outside a fight. */
    UPROPERTY() TArray<FActiveSpell> ActiveSpells;
    /** SRD 5.2.1: a creature spends at most one spell slot per turn. */
    UPROPERTY() bool bSlotSpentThisTurn = false;
    /** Creatures the active unit hit with a melee weapon attack this turn (smite spells); cleared at every turn start. */
    UPROPERTY() TArray<int32> MeleeHitThisTurn;
    /** Of those, creatures it scored a critical hit on this turn (smite dice double, SRD 5.2.1). */
    UPROPERTY() TArray<int32> MeleeCritThisTurn;
    UPROPERTY() int32 NextSpellSerial = 1;
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
    /** CastSpell only: further targets for multi-target spells and per-beam/ray/dart targets (repeat an id to stack). Not persisted. */
    TArray<int32> ExtraTargetIds;
    /** CastSpell only: a spell option (damage type, Enlarge vs Reduce, Command word); 0 = default. Not persisted. */
    int32 Choice = 0;
};

/** What a spell event is, for presentation and VFX. */
/** Cast: the spell goes off. Trigger: a lasting spell fires on someone (Spirit Guardians, Blink vanishing, reactions).
 *  Tick: a per-turn effect (Moonbeam, Heat Metal, start/end-of-turn damage). End: a lasting spell ends.
 *  Persist: a snapshot of a spell still in effect, from FCombatBattle::SpellVisuals() (never in Result.SpellEvents). */
enum class ESpellEventKind : uint8 { Cast, Trigger, Tick, End, Persist };
/** VFX hook: everything a renderer needs to show a spell, emitted on cast and on every later trigger. Rules never read it back.
 *  See docs/rules-reference.md "Spell effect hooks". No art lives in the rules module. */
struct DNDDICEROLL_API FSpellCastEvent
{
    ESpellEventKind Kind = ESpellEventKind::Cast;
    FString SpellId;
    uint8 School = 0;
    int32 SpellLevel = 0;
    int32 SlotLevel = 0;
    /** Cantrip damage tier 0..3 (character levels 1/5/11/17); 0 for leveled spells. */
    int32 Tier = 0;
    /** VFX size class from the spell's level (FSpellUtilityRules::SizeClass): 0 cantrip, 1 levels 1-2, 2 level 3, 3 levels 4-6, 4 levels 7-9. */
    int32 SizeClass = 0;
    /** The spell stays in effect after this event (it is in ActiveSpells): keep its looping visual until an End event with the same Serial. */
    bool bPersist = false;
    /** ESpellArea value and size; cells covered on the grid (empty for single targets). */
    uint8 AreaShape = 0;
    int32 AreaFeet = 0;
    TArray<FIntPoint> Cells;
    int32 CasterId = INDEX_NONE;
    FIntPoint CasterCell = FIntPoint::ZeroValue;
    TArray<int32> TargetIds;
    TArray<FIntPoint> TargetCells;
    /** Rounds the effect lasts (0 instant, -1 until dispelled). */
    int32 DurationRounds = 0;
    int32 Serial = 0;
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
    /** VFX hooks: spells cast by this command and lasting spells that triggered or ended during it, in order. */
    TArray<FSpellCastEvent> SpellEvents;
};

/** CastSpell expectation (SRD 5.2.1 resolution, no RNG): chance per target, amount range, area and duration. */
struct DNDDICEROLL_API FSpellForecast
{
    bool bValid = false;
    FString SpellId;
    int32 SlotLevel = 0;
    bool bAttackRoll = false;
    bool bSave = false;
    /** Attack: hit chance vs TargetId; Save: chance TargetId fails, out of 400. */
    int32 ChanceOutOf400 = 0;
    int32 SaveDc = 0;
    int32 MinAmount = 0;
    int32 MaxAmount = 0;
    bool bHealing = false;
    /** Creatures the spell would reach (area victims or the target). */
    TArray<int32> AffectedIds;
    TArray<FIntPoint> Cells;
    int32 DurationRounds = 0;
    bool bConcentration = false;
    /** Casting would end the caster's current concentration spell. */
    bool bEndsConcentration = false;
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
    /** Two-round Downed house rule: the target is Downed, so this attack kills it outright with no roll. */
    bool bKillsDowned = false;
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
 * Grid: pointy-top hexes stored as odd-r offset board cells (FHexGrid), six-way movement, occupied cells
 * block travel, ranges in hex steps (1 hex = 5 ft) and blocked-hex line of sight. Limits: 16x16 cells, 32 combatants, stats bounded at Start.
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
    /** CastSpell forecast for a legal target; invalid otherwise. Consumes no random numbers. */
    FSpellForecast ForecastSpell(int32 ActorId, const FString& SpellId, int32 TargetId, int32 SlotLevel = 0) const;
    /** Spells in effect now. */
    const TArray<FActiveSpell>& ActiveSpells() const { return State.ActiveSpells; }
    /** VFX query: one Persist event per spell in effect now (zone cells, caster/target cells, rounds left). Consumes no random numbers. */
    TArray<FSpellCastEvent> SpellVisuals() const;
    /** VFX event hook: called once per event, in order, when an accepted Submit returns (the same events as Result.SpellEvents; rejected commands emit none). Optional; rules never depend on it. */
    TFunction<void(const FSpellCastEvent&)> OnSpellEvent;
    /** Zone cells of an active spell now (emanations follow their caster). */
    TArray<FIntPoint> ZoneCells(const FActiveSpell& Spell) const;
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
    /** The two-round Downed house rule is in force (bTwoRoundDowned and not bDeathSaves). */
    bool TwoRoundDowned() const { return State.Encounter.bTwoRoundDowned && !State.Encounter.bDeathSaves; }
    /** Rounds a Downed hero has left before dying, by the clock in force. */
    static constexpr int32 DownedRounds = 2;
    /** "AC 18 (Chain Mail, shield)" from worn armor, else "AC 13". */
    FString ArmorClassText(int32 Id) const;
    // Spell effects (CombatSpellEffects.cpp, spells phases 3-4). Read-only, no RNG.
    /** SRD 5.2.1 creature type of a unit (FCreatureRules by role). */
    ECreatureType CreatureTypeOf(int32 Id) const;
    /** Speed in cells after spells (Haste, Slow, Longstrider, Hypnotic Pattern). */
    int32 SpellSpeed(int32 Id, int32 SpeedCells) const;
    /** Active spell effect on a creature (TargetId) by spell id; null when none. */
    const FActiveSpell* SpellEffectOn(int32 Id, const TCHAR* SpellId) const;
    /** Mirror Image duplicates left. */
    int32 MirrorImagesLeft(int32 Id) const;
    /** Damage types resisted from spells (Stoneskin, Protection from Energy, Fire Shield, Warding Bond) and Petrified. */
    uint16 SpellResistances(int32 Id) const;
    /** True when a frightened unit's step would end closer to a visible source of its fear (SRD 5.2.1 Frightened). */
    bool FearBlocksStep(int32 Id, FIntPoint From, FIntPoint To) const;
private:
    TArray<int32> LegalTargetsRaw(int32 Id, ECombatCommandType Action) const;
    TArray<int32> SpellTargetsRaw(int32 Id, const FString& SpellId, int32 SlotLevel) const;
    TArray<int32> SpellTargetsSpec(int32 Id, const FString& SpellId, int32 SlotLevel) const;
    // ---- Spells phase 6: utility spells (CombatSpellUtility.cpp) ----
    bool ResolveUtilitySpell(const FCombatCommand& Command, const struct FSrdSpell& S, int32 Slot, int32 Dc, int32 AttackBonus, int32 Mod, FCombatResult& Result);
    bool UtilityTargets(int32 Id, const struct FSrdSpell& S, int32 Slot, TArray<int32>& Out) const;
    bool UtilityCastBlocked(const FCombatCommand& Command, const struct FSrdSpell& S, FString& Why) const;
    void ZoneCastExtras(int32 Serial, const FCombatCommand& Command);
    bool SpellZoneAt(FIntPoint Cell, uint8 Kind) const;
    bool SpellCanSee(int32 ViewerId, int32 TargetId, bool bInvisible = true) const;
    const FActiveSpell* SpellExtraOn(int32 Id, uint32 Bit) const;
    int32 SpellCheckBonus(const FCombatantState& Unit, EAbility Ability, ERollMode& Mode, bool bStealth = false);
    void SummonsOnTurnStart(int32 UnitId);
    /** Push Cells hexes along hex direction Dir (0..5, FHexGrid::Directions). */
    void PushUnit(int32 Id, int32 Dir, int32 Cells);
    int32 SpellDamageIntake(FCombatantState& Target, int32 Damage);
    void SpellDamageShared(int32 TargetId, int32 Real);
    int32 ResistanceCut(int32 TargetId, EDamageType Type, int32 Damage);
    bool AuraOfLifeRevives(FCombatantState& Unit);
    bool UtilityAttack(int32 ActorId, int32 TargetId, int32 AttackBonus, FCombatResult& Result, bool& bCritical, bool bRanged);
    int32 UtilityDamage(int32 TargetId, int32 Amount, EDamageType Type);
    int32 StrikeAbilitySwap(const FCombatantState& Actor) const;
    bool ShillelaghDie(const FCombatantState& Actor, int32& Count, int32& Sides) const;
    void SpellTurnControl(FCombatantState& Unit, const FActiveSpell& A, const struct FSpellEffectSpec& Spec);
    bool bTrueStrike = false;
    bool bSharingDamage = false;
    TArray<FIntPoint> LegalMovesRaw(int32 Id) const;
    TArray<FIntPoint> PathToRaw(int32 Id, FIntPoint Destination) const;
    int32 ArmorClassBeforeSpells(int32 Id) const;
    void SpellAttackModes(const FCombatantState& Actor, const FCombatantState& Target, FAttackRollMode& Mode) const;
    void SpellSaveModes(const FCombatantState& Unit, EAbility Ability, int32& Advantage, int32& Disadvantage) const;
    /** Mage Armor: bonus over 10 for an unarmored creature (13 + DEX), 0 without. */
    int32 SpellUnarmoredBonus(int32 Id) const;
    /** A charmed unit can't attack or target its charmer with harmful spells. */
    bool CharmForbids(int32 ActorId, int32 TargetId) const;
    bool SpellKeepsProne(int32 Id) const;
    bool SpellBlocksReactions(int32 Id) const;
    /** Saving throw against a spell: d4s from Bless/Bane, flat spell bonuses, advantage sources, auto-fails (helpless/petrified STR and DEX). */
    bool RollSpellSave(int32 VictimId, EAbility Ability, int32 Dc, const struct FSpellEffectSpec* Spec, bool bAdvantage = false);
    /** Adds a spell condition unless the creature is immune (stat block or a spell); incapacitating ones end its concentration. */
    bool AddSpellCondition(FCombatantState& Victim, ECombatCondition Kind, int32 SourceId, int32 RoundsLeft);
    bool ResolveEffectSpell(const FCombatCommand& Command, const struct FSrdSpell& Spell, const struct FSpellEffectSpec& Spec, int32 Slot, int32 Dc, int32 AttackBonus, int32 Mod, const TArray<int32>& Legal, FCombatResult& Result);
    /** Targets of a multi-target spell: the aimed one plus ExtraTargetIds, legal and unique, up to the slot's cap. */
    TArray<int32> SpellTargetList(const FCombatCommand& Command, const struct FSrdSpell& Spell, const struct FSpellEffectSpec& Spec, int32 Slot, const TArray<int32>& Legal) const;
    /** Undo what an ended effect changed (Aid hit points, Haste lethargy, held conditions). */
    void EndSpellEffects(const FActiveSpell& Ended);
    /** The creature took damage: Sleep/Hypnotic Pattern/charms end, Hideous Laughter re-saves. */
    void SpellEffectsOnDamaged(int32 TargetId, int32 SourceId);
    /** Bless/Bane d4 and Magic Weapon on an attack roll (rolls only when present). */
    int32 SpellAttackBonus(int32 ActorId, bool bWeapon, FCombatResult& Result);
    /** Hex, Hunter's Mark, Divine Favor, Enlarge, Magic Weapon dice on a hit, after defenses. Rolls only when present. */
    int32 SpellHitDamage(int32 ActorId, int32 TargetId, bool bCritical, bool bWeapon, EDamageType WeaponType, FCombatResult& Result);
    /** Sanctuary: the attacker's WIS save or the attack is lost. */
    bool SanctuaryStops(int32 ActorId, int32 TargetId, FCombatResult& Result);
    /** Mirror Image: a duplicate takes the hit. */
    bool MirrorImageTakes(int32 TargetId, FCombatResult& Result);
    /** Fire Shield: a melee hit from within 5 ft burns (or chills) the attacker. */
    void SpellRetaliation(int32 AttackerId, int32 TargetId, FCombatResult& Result);
    // Phase 5 utility spells (CombatSpellUtility.cpp).
    /** Hellish Rebuke: a damaged hero with the spell, a reaction and a slot answers a creature within 60 ft it can see. */
    void SpellReactionsOnDamaged(int32 AttackerId, int32 TargetId, FCombatResult& Result);
    /** Lowest open slot (>= the spell's level) a unit can spend outside its own turn on a spell it knows, and the class it casts through; 0 = none. */
    int32 ReactionSlot(const FCombatantState& Unit, const struct FSrdSpell& Spell, EAdventurerRole& OutClass) const;
    void SpendSlotLevel(FCombatantState& Unit, int32 Slot);
    /** Invisibility and Sanctuary end when the creature attacks, casts or deals damage. */
    void EndOnHostileAct(int32 ActorId);
    /** Heroism temp HP and Searing Smite damage/save at the start of the creature's turn. */
    void SpellEffectsOnTurnStart(int32 UnitId);
    /** Death Ward: a drop to 0 HP leaves 1 HP and ends the spell. */
    bool DeathWardSaves(FCombatantState& Target);
    const struct FSrdSpell* UsableSpell(const FCombatantState& Unit, const FString& SpellId, int32 RequestedSlot, int32& OutSlot, EAdventurerRole& OutClass) const;
    bool ResolveCatalogSpell(const FCombatCommand& Command, FCombatResult& Result);
    /** Ends the caster's concentration and the conditions its spell holds (RoundsLeft -1, caster as source). */
    void EndConcentration(FCombatantState& Caster);
    // Lasting spells (CombatSpellEffects.cpp).
    FActiveSpell& AddActiveSpell(const struct FSrdSpell& Spell, int32 CasterId, int32 TargetId, int32 Slot, int32 Dc, int32 AttackBonus);
    bool ZoneCovers(const FActiveSpell& Spell, FIntPoint Cell) const;
    /** One zone trigger on a creature (save and damage, conditions); respects once per turn. */
    void TriggerZone(int32 SpellIndex, int32 VictimId);
    /** The mover entered Cell (or an emanation reached it): enter triggers and per-step zone damage. */
    void SpellsOnStep(int32 MoverId, FIntPoint From, FIntPoint To);
    /** Turn start: caster durations tick down, once-per-turn marks reset. */
    void SpellsOnTurnStart(int32 UnitId);
    /** Turn end: end-of-turn zone saves and repeat saves against held conditions. */
    void SpellsOnTurnEnd(int32 UnitId);
    void RemoveActiveSpell(int32 Index, bool bEvent = true);
    void EmitSpellEvent(ESpellEventKind Kind, const FActiveSpell& Spell, const TArray<int32>& Targets);
    FSpellCastEvent MakeSpellEvent(ESpellEventKind Kind, const FActiveSpell& Spell, const TArray<int32>& Targets) const;
    bool ValidPreparedSpells(const FCombatantState& Unit, FString& Error) const;
    /** Creatures a catalog spell reaches from the caster aimed at TargetId (zone radius for lasting zones), and the cells it covers. */
    TArray<int32> SpellVictims(const FCombatantState& User, const struct FSrdSpell& Spell, int32 TargetId, TArray<FIntPoint>* OutCells) const;
    int32 IndexOfSpell(int32 Serial) const;
    /** VFX events gathered while resolving; Submit hands them to the result. */
    TArray<FSpellCastEvent> PendingSpellEvents;
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
    /** House rule: tick Downed countdowns whose slot just came round; clear stale slots. */
    void TickDownedSlots();
    bool DownedKillable(const FCombatantState& Attacker, const FCombatantState& Target) const;
    /** Starts the active unit's turn: movement from speed, grapple/restraint, and standing up from prone. */
    void BeginTurn();
    /** One weapon attack roll and its damage, without spending any budget. Shared by Attack and reactions. */
    void Strike(int32 ActorId, int32 TargetId, int32 ManeuverBonus, FCombatResult& Result, bool bOffHand = false);
    /** Drops conditions whose source or situation no longer supports them. */
    void PruneConditions();
    static bool HasCondition(const FCombatantState& Unit, ECombatCondition Condition);
    /** SRD: incapacitated, stunned, paralyzed and unconscious creatures take no actions or reactions. */
    static bool IsIncapacitated(const FCombatantState& Unit);
    /** Whether a nearby foe can see Target (not Blinded, not obscured or invisible, light or darkvision in darkness). */
    bool NearbyFoeSees(const FCombatantState& Viewer, const FCombatantState& Target) const;
    /** Stunned, Paralyzed, Unconscious and Petrified creatures auto-fail STR and DEX saves (SRD 5.2.1). Speed 0 is separate: not Stunned. */
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
