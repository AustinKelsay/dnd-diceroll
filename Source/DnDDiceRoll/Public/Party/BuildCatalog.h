#pragma once
#include "Rules/AbilityRules.h"
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
}
class DNDDICEROLL_API FBuildCatalog
{
public:
    static constexpr int32 Revision = 7;
    static const TArray<FBuildDefinition>& All();
    static const FBuildDefinition* Find(const FString& BuildId);
    /** Invalid IDs leave output untouched. Instance identity is independent of build. */
    static bool Create(const FString& BuildId, int32 Id, const FString& Name, FCombatantState& Out);
    static TArray<FAbilityFacts> Abilities(const FCombatantState& Hero);
    static bool HasSpecial(const FCombatantState& Hero, ECombatCommandType Action);
    static FString DisplayName(const FCombatantState& Hero);
    static FString Status(const FCombatantState& Hero);
    static int32 AttackModifier(const FCombatantState& Hero);
    /** #79: SRD maximum hit points for the hero's class level(s) and CON. */
    static int32 MaxHitPoints(const FCombatantState& Hero);
    /** #79: prebuilt scores with the hero's ability score improvements replayed; false if a choice is illegal. */
    static bool HeroScores(const FCombatantState& Hero, FAbilityScores& Out);
    /** #79: improvements earned by class levels, minus those already chosen. */
    static int32 PendingAsi(const FCombatantState& Hero);
    static int32 BasicDamage(const FBuildDefinition& Build, int32 Level);
    static int32 SurgeCap(const FCombatantState& Hero);
    static bool ValidResources(const FCombatantState& Hero);
};
