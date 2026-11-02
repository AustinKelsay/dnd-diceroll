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
    static constexpr ECombatCommandType LastCommand = ECombatCommandType::TurnUndead;
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
    /** SRD creature type from the monster profile (Skeleton, Zombie). */
    static bool IsUndead(const FCombatantState& Unit);
    static constexpr int32 TurnUndeadCells = 6;
    static constexpr int32 TurnUndeadRounds = 10;
};