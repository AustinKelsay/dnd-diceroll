#pragma once
#include "CoreMinimal.h"
#include "Combat/CombatBattle.h"

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
    static bool Valid(const FCombatantState& Hero);
    /** Level-up / new class with the hero as it was before: newly unlocked uses arrive ready. */
    static void Grant(FCombatantState& Hero, const FCombatantState& Before);

    // Feature commands (ECombatCommandType::ActionSurge and later).
    static bool IsFeature(ECombatCommandType Action) { return Action >= ECombatCommandType::ActionSurge && Action <= LastCommand; }
    static constexpr ECombatCommandType LastCommand = ECombatCommandType::UncannyDodge;
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
    /** Font of Magic (basic): sorcery points spent to regain one spell (kit) use, the SRD cost of a 1st-level slot. */
    static constexpr int32 FontCost = 2;
    static bool IsKi(ECombatCommandType Action) { return Action >= ECombatCommandType::FlurryOfBlows && Action <= ECombatCommandType::StepOfTheWind; }
    /** Short rest: refills the short-rest pools (Action Surge, Channel Divinity, Wild Shape, ki). */
    static void ShortRest(FCombatantState& Hero);
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