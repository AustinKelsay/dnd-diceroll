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
    /** Level-up / new class: assigns a default style when newly entitled and opens pools that were just unlocked (no refresh of spent uses). */
    static void Grant(FCombatantState& Hero);
    static bool Valid(const FCombatantState& Hero);
};