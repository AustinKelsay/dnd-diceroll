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
/** #79 SRD: Cure Wounds 1d8 + spellcasting modifier (prebuilt WIS 16); Second Wind 1d10 + fighter level. */
inline constexpr int32 CureDice = 1, CureDie = 8, HealingModifier = 3, WindDie = 10, FighterLevel = 1;
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
    /** #79 multiclass: the signature feature a second class grants (Fighter Second Wind, Cleric Cure Wounds, Wizard Magic Missile), or Attack when none is modelled. */
    static ECombatCommandType SecondaryFeature(EAdventurerRole Role);
    /** Resource caps and long-rest recovery including a second class's grant. */
    static int32 WindCap(const FCombatantState& Hero);
    static int32 SlotCap(const FCombatantState& Hero);
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
    /** Superiority dice per rest: the interim 2 below Fighter 3, then the SRD 4. */
    static int32 ManeuverCap(const FCombatantState& Hero);
    /** #79 menu sheet: level/XP, classes, HP and hit dice, slots and resources, tagged features, pending ASI. Plain text lines so any HUD layout (incl. #110) can draw them. */
    static TArray<FString> ProgressSheet(const FCombatantState& Hero);
    /** Levels the hero has in Role across both classes. */
    static int32 ClassLevel(const FCombatantState& Hero, EAdventurerRole Role);
    static int32 SurgeCap(const FCombatantState& Hero);
    static bool ValidResources(const FCombatantState& Hero);
};
