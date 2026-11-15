#pragma once
#include "Combat/CombatBattle.h"

/** Immutable native definitions. Only implemented kits are published. IDs are save contracts. */
struct DNDDICEROLL_API FBuildDefinition
{
    FString Id;
    FCombatantState Profile;
    TArray<ECombatCommandType> Specials;
    int32 WindCap = 0, SlotCap = 0, ManeuverCap = 0;
    int32 WindRecovery = 0, SlotRecovery = 0, ManeuverRecovery = 0;
    TArray<FAbilityFacts> Abilities;
    int32 KitCap = 0, KitRecovery = 0;
    int32 RageCap = 0, RageRecovery = 0;
    int32 MissileKitCost = 0;
    int32 CriticalThreshold = 20;
    int32 SneakDie = 0;
};
/** Concrete compact-kit tuning shared by descriptions, forecasts and resolution. */
namespace BuildRules
{
inline constexpr int32 TouchRange = 1, MissileRange = 6, MissileDarts = 3, MissileDie = 4, DartBonus = 1;
inline constexpr int32 CureDice = 2, CureDie = 8, HealingModifier = 3, WindDie = 10, FighterLevel = 1;
inline constexpr int32 SneakDie = 6, PrecisionBonus = 2;
/** Rage's basic-damage bonus while raging (its -1 incoming damage and two-turn duration are fixed, never honed). */
inline constexpr int32 RageBonus = 2;
}
/**
 * The two upgrade cards of the victory reward (#218, docs/runs/gameplay-depth/13-progression-contract.md section 3). Values are the as-built
 * levers, each within the contract's bound. Honed raises both of a hero's specials by their effect's row; Reserve adds a cap and a use to
 * each pool the hero's specials spend. Fixed, never honed: the Potion and Rescue, touch and Magic Missile range, the Sneak Attack die, and
 * Rage's -1 incoming damage and duration.
 */
namespace UpgradeRules
{
/** Reserve: +1 cap and +1 use on each pool its specials spend (never a refill). */
inline constexpr int32 ReserveUses = 1;
/** Honed rows (as built: report 14 lever step 1, every row at its contract bound). Strike and Push damage; Area damage to each victim;
 * Surge base amount (outcomes A+2 or 2(A+2)). */
inline constexpr int32 HonedStrike = 3, HonedArea = 2, HonedSurge = 2;
/** Magic Missile's damage modifier (3d4+3 -> 3d4+6); Kit Healing; Cure Wounds' (2d8+6) and Second Wind's (d10+4) healing modifiers. */
inline constexpr int32 HonedMissile = 3, HonedKitHealing = 3, HonedCure = 3, HonedWind = 3;
/** Precision Strike's accuracy and damage bonus (2 -> 4); Protect's reduction; Swap's range; Rage's basic damage while raging (2 -> 3). */
inline constexpr int32 HonedPrecision = 2, HonedProtect = 1, HonedSwapRange = 2, HonedRage = 1;
/** The strongest reduction one Protect can hold: the catalog's strongest Protect (4) honed. */
inline constexpr int32 MaxProtect = 4 + HonedProtect;
static_assert(ReserveUses == 1, "Reserve gives exactly one cap and one use (contract 13 section 3)");
static_assert(HonedStrike >= 0 && HonedStrike <= 3 && HonedArea >= 0 && HonedArea <= 2 && HonedSurge >= 0 && HonedSurge <= 2, "Honed damage bounds (contract 13 section 3)");
static_assert(HonedMissile >= 0 && HonedMissile <= 3 && HonedKitHealing >= 0 && HonedKitHealing <= 3 && HonedCure >= 0 && HonedCure <= 3 &&
    HonedWind >= 0 && HonedWind <= 3, "Honed missile and healing bounds (contract 13 section 3)");
static_assert(HonedPrecision >= 0 && HonedPrecision <= 2 && HonedProtect >= 0 && HonedProtect <= 1 && HonedSwapRange >= 0 && HonedSwapRange <= 2 &&
    HonedRage >= 0 && HonedRage <= 1, "Honed bonus bounds (contract 13 section 3)");
}
/** A hero's live pool caps: its build's caps, plus Reserve on each pool the build has. A hero without a catalog build (a combat fixture or
 * an enemy) has the fixture caps: 2 Wind, Slot and Maneuver, no Kit or Rage. */
struct DNDDICEROLL_API FResourceCaps
{
    int32 Kit = 0, Slot = 0, Wind = 0, Maneuver = 0, Rage = 0;
};
class DNDDICEROLL_API FBuildCatalog
{
public:
    /** The revision new runs are created under. 8 (#184): Compact enemy tuning. 9 (#188): Compact encounter identity (stages,
     * layouts, enemy roles and previews). 10 (#218): Compact reward cards (Reserve and Honed); generation is revision 9's. */
    static constexpr int32 Revision = 10;
    /** The oldest revision that still restores. A run keeps the revision it was created under for its whole lifetime
     * (revision 7 Compact runs keep revision 7 enemies, revision 8 runs revision 8 generation); any other revision is refused by name. */
    static constexpr int32 MinimumRevision = 7;
    static const TArray<FBuildDefinition>& All();
    static const FBuildDefinition* Find(const FString& BuildId);
    /** Invalid IDs leave output untouched. Instance identity is independent of build. */
    static bool Create(const FString& BuildId, int32 Id, const FString& Name, FCombatantState& Out);
    /** The hero's specials as they play now: honed amounts, ranges and truthful descriptions when the hero is Honed (#218), and whether
     * each one's resource is held. The one seam forecast, legality, resolution and presentation read. */
    static TArray<FAbilityFacts> Abilities(const FCombatantState& Hero);
    /** The live per-hero pool caps (#218): Reserve's extra cap included. Validation, facts, camp and presentation read this, never the build. */
    static FResourceCaps Caps(const FCombatantState& Hero);
    /** The honed increase of a number a command reads from BuildRules rather than from its FAbilityFacts: Second Wind's and Cure Wounds'
     * healing modifier, Magic Missile's damage modifier, Precision Strike's accuracy and damage bonus, and Rage's basic damage (Command is
     * the Rage ability's command). 0 when the hero is not Honed or has no such special. */
    static int32 HonedBonus(const FCombatantState& Hero, ECombatCommandType Command);
    /** The reduction Guard's protection gives while it lasts: the stored amount of a Kit-paid Protect, otherwise (Shelter, paid by Rage)
     * the guard's own Rage-paid Protect fact, honed when the guard is Honed. 0 when Guard protects no one. */
    static int32 GuardAmount(const FCombatantState& Guard);
    static bool HasSpecial(const FCombatantState& Hero, ECombatCommandType Action);
    static FString DisplayName(const FCombatantState& Hero);
    static FString Status(const FCombatantState& Hero);
    static int32 AttackModifier(const FCombatantState& Hero);
    static int32 BasicDamage(const FBuildDefinition& Build, int32 Level);
    static int32 SurgeCap(const FCombatantState& Hero);
    static bool ValidResources(const FCombatantState& Hero);
};
