#pragma once
#include "CoreMinimal.h"
#include "Combat/CombatBattle.h"
#include "Rules/WarlockRules.h"

/** SRD Fighting Style options (FCombatantState::FightingStyle; values are persisted, append only). */
enum class EFightingStyle : uint8 { None, Archery, Defense, Dueling, GreatWeaponFighting, Protection, TwoWeaponFighting, Count };

/** Level 1-5 SRD class features that are played in combat: entitlement, pools, defaults and validation. */
class DNDDICEROLL_API FFeatureKit
{
public:
    static const TCHAR* StyleName(EFightingStyle Style);
    static const TCHAR* StyleEffect(EFightingStyle Style);
    /** Fighter 1, Paladin 2 or Ranger 2 (either class). */
    static bool HasFightingStyle(const FCombatantState& Hero);
    /** The chosen style, or None when the hero is not entitled. */
    static EFightingStyle Style(const FCombatantState& Hero);
    /** Equipment-based default: ranged weapon Archery, two-handed weapon Great Weapon Fighting, two weapons Two-Weapon Fighting, armor Defense, else Dueling. */
    static EFightingStyle DefaultStyle(const FCombatantState& Hero);
    /** Picks a style for an entitled hero (replaces the default; SRD fixes it once chosen, so only before the hero's first fight at that level). */
    static bool ChooseStyle(FCombatantState& Hero, EFightingStyle Choice, FString& Error);
    /** Long rest / creation / level-up: fills feature pools to their caps and assigns a default style when newly entitled. */
    static void Refill(FCombatantState& Hero);
    /** Fill SlotsByLevel from the SRD slot tables (FBuildCatalog::SlotMax) when the hero has slots above 1st level. */
    static void RefillSlotTable(FCombatantState& Hero);
    static bool Valid(const FCombatantState& Hero);
    /** Level-up / new class with the hero as it was before: newly unlocked uses arrive ready. */
    static void Grant(FCombatantState& Hero, const FCombatantState& Before);

    // Feature commands (ECombatCommandType::ActionSurge and later).
    static bool IsFeature(ECombatCommandType Action) { return Action >= ECombatCommandType::ActionSurge && Action <= LastCommand; }
    static constexpr ECombatCommandType LastCommand = ECombatCommandType::Counterspell;
    static const TCHAR* CommandName(ECombatCommandType Action);
    static EActionCost Cost(ECombatCommandType Action);
    /** "1 left" style usage for the HUD button. */
    static FString Usage(ECombatCommandType Action, const FCombatantState& Hero);
    /** Feature commands the hero has (by class levels), in HUD order. */
    static TArray<ECombatCommandType> Commands(const FCombatantState& Hero);
    /** True when the command picks a target other than the user (HUD targeting mode). */
    static bool PicksTarget(ECombatCommandType Action);
    static int32 ActionSurgeCap(const FCombatantState& Hero);
    static int32 ChannelCap(const FCombatantState& Hero);
    static int32 BardicCap(const FCombatantState& Hero);
    /** d6 at Bard 1-4, d8 from Bard 5. */
    static int32 BardicSides(const FCombatantState& Bard);
    static constexpr int32 BardicCells = 12;
    static int32 WildShapeCap(const FCombatantState& Hero);
    /** Monk level from Monk 2. */
    static int32 KiCap(const FCombatantState& Hero);
    /** Sorcerer level from Sorcerer 2. */
    static int32 SorceryCap(const FCombatantState& Hero);
    /** Rogue 5: an automatic reaction that halves the damage of a hit from an attacker the rogue can see (HUD shows it as a passive button). */
    static bool HasUncannyDodge(const FCombatantState& Hero);
    static bool HasRecklessAttack(const FCombatantState& Hero);
    /** Monk 1 Martial Arts: after the Attack action, one unarmed strike as a bonus action (no ki). */
    static bool HasMartialArts(const FCombatantState& Hero);
    /** Monk 3 Deflect Missiles: automatic reaction, a ranged weapon hit loses 1d10 + DEX + monk level damage. */
    static bool HasDeflectMissiles(const FCombatantState& Hero);
    /** SRD Counterspell (3rd-level wizard/sorcerer spell, so Wizard or Sorcerer 5): automatic reaction against an enemy spell within 60 ft. */
    static bool HasCounterspell(const FCombatantState& Hero);
    static constexpr int32 CounterspellRange = 12;
    /** Paladin 3 Channel Divinity: Sacred Weapon (Oath of Devotion, the SRD oath), 1 minute = 10 rounds; shares ChannelUses. */
    static bool HasSacredWeapon(const FCombatantState& Hero);
    static constexpr int32 SacredWeaponRounds = 10;
    /** Wizard 1: once per long rest, at a short rest (the rest point), regain spell resources worth half the wizard level (rounded up) in slot levels. */
    static bool HasArcaneRecovery(const FCombatantState& Hero);
    /** Slot levels Arcane Recovery would restore now (0 when used or nothing is missing). */
    static int32 ArcaneRecoveryPreview(const FCombatantState& Hero);
    /** Applies Arcane Recovery; returns the slot levels restored (0 = refused). */
    static int32 ArcaneRecover(FCombatantState& Hero);
    /** Warlock 2 (SRD 5.2.1): a 1-minute rite, once per long rest, regains expended Pact Magic slots up to half the maximum (rounded up). */
    static bool HasMagicalCunning(const FCombatantState& Hero);
    /** Pact slots Magical Cunning would restore now (0 when used or nothing is missing). */
    static int32 MagicalCunningPreview(const FCombatantState& Hero);
    /** Applies Magical Cunning; returns the slots restored (0 = refused). */
    static int32 MagicalCunning(FCombatantState& Hero);
    /** Reward info sheet: one "Name [Tag]: usage" line per feature (HUD-independent so it survives HUD rework). */
    static TArray<FString> RewardSheet(const FCombatantState& Hero);
    /** Font of Magic (basic): sorcery points spent to regain one spell (kit) use, the SRD cost of a 1st-level slot. */
    static constexpr int32 FontCost = 2;
    /** Sorcerer 3 Metamagic: Quickened Spell (2 points). Flexible Casting reverse (Sorcerer 2): a spent spell use (1st-level slot) yields 2 points, bonus action. */
    static constexpr int32 QuickenCost = 2;
    static bool HasQuickenedSpell(const FCombatantState& Hero);
    static bool IsKi(ECombatCommandType Action) { return Action >= ECombatCommandType::FlurryOfBlows && Action <= ECombatCommandType::StepOfTheWind; }
    /** Short rest: refills the short-rest pools (Action Surge, Channel Divinity, Wild Shape, ki, Warlock Pact Magic slots). */
    static void ShortRest(FCombatantState& Hero);
    /** Warlock 1 Pact Magic: true when the hero's slot pool is the Pact Magic table (no other slot-casting class adds to it). */
    static bool HasPactMagic(const FCombatantState& Hero);
    /** Regains up to Budget expended Pact Magic slots (pact slot level; kit warlocks regain kit uses); returns how many came back. */
    static int32 PactRecover(FCombatantState& Hero, int32 Budget);
    /** Bard 2 Jack of All Trades: half proficiency (rounded down) added to ability checks without proficiency; the engine applies it to Initiative. */
    static int32 JackOfAllTrades(const FCombatantState& Hero);
    /** Warlock 1 Eldritch Invocations: keeps the held set legal and fills empty picks from the default order (on long rest and level-up). */
    static void EnsureInvocations(FCombatantState& Hero);
    /** Replaces the held invocations when the set is legal for the hero's Warlock level (count, level and invocation prerequisites). */
    static bool ChooseInvocations(FCombatantState& Hero, const TArray<int32>& Picks, FString& Why);
    /** True when the hero holds the invocation (always false below Warlock 1). */
    static bool HasInvocation(const FCombatantState& Hero, EInvocation Invocation);
    // SRD wolf (CR 1/4), the basic form: AC 13, 11 HP, bite +4 for 2d4+2.
    static constexpr int32 WolfHP = 11;
    static constexpr int32 WolfAC = 13;
    static constexpr int32 WolfAttack = 4;
    static constexpr int32 WolfDamageBonus = 2;
    /** Whether any feature of the hero can spend the bonus action (save validation). */
    static bool HasBonusFeature(const FCombatantState& Hero);
    /** SRD creature type from the monster profile (Skeleton, Zombie). */
    static bool IsUndead(const FCombatantState& Unit);
    static constexpr int32 TurnUndeadCells = 6;
    static constexpr int32 TurnUndeadRounds = 10;
};