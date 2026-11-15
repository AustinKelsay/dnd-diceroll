#pragma once

#include "CoreMinimal.h"
#include "Combat/ActionFacts.h"
#include "Run/ExpeditionRun.h"

/**
 * What progression presentation shows (#218, docs/runs/gameplay-depth/13-progression-contract.md section 4): level gains, the basic-damage
 * breakdown, the three reward cards with their before -> after values and limits, and the camp's before -> after values. Facts only (ADR-0001):
 * no display strings and no decisions. Derived from saved state alone; every query is const and draws no random numbers. The after-values
 * are the reward and camp rules themselves applied to a copy of the battle, so a fact always equals what the claim or the camp commits.
 */

/** Why a card cannot be taken, or (GearAtCap) what the legal Gear card gives instead. Presentation words them. */
enum class ERewardCardNote : uint8 { None, Fallen, GearOnlyRun, ReserveTaken, AlreadyHoned, GearAtCap };

/** Which number a special's before -> after value is: its amount, its range (Swap), or a fixed modifier (Cure Wounds, Magic Missile,
 * Second Wind, Precision Strike, Rage). */
enum class ESpecialValue : uint8 { Amount, Range, Modifier };

struct DNDDICEROLL_API FBasicDamageParts
{
    /** FBuildCatalog::AttackModifier: the flat basic-attack damage on top of the weapon die. Always Build + Levels + Gear + Rage. */
    int32 Total = 0;
    int32 Build = 0, Levels = 0, Gear = 0, Rage = 0;
};

struct DNDDICEROLL_API FPoolFacts
{
    EActionResource Resource = EActionResource::None;
    int32 Held = 0, Cap = 0;
};

struct DNDDICEROLL_API FPoolChange
{
    EActionResource Resource = EActionResource::None;
    int32 HeldBefore = 0, CapBefore = 0, HeldAfter = 0, CapAfter = 0;
};

struct DNDDICEROLL_API FSpecialChange
{
    ECombatCommandType Command = ECombatCommandType::Special1;
    FString Name;
    ESpecialValue Value = ESpecialValue::Amount;
    int32 Before = 0, After = 0;
};

struct DNDDICEROLL_API FRewardCardFacts
{
    ERewardCard Card = ERewardCard::Gear;
    /** The claim would be accepted. Gear at rank 3 stays available (Note GearAtCap: potions only). */
    bool bAvailable = false;
    ERewardCardNote Note = ERewardCardNote::None;
    FBasicDamageParts DamageBefore, DamageAfter;
    int32 GearBefore = 0, GearAfter = 0;
    /** Every pool the hero has, before -> after (only Reserve changes them). */
    TArray<FPoolChange> Pools;
    /** Both specials, before -> after (only Honed changes them). */
    TArray<FSpecialChange> Specials;
    int32 PotionsBefore = 0, PotionsAfter = 0;
};

struct DNDDICEROLL_API FHeroProgressFacts
{
    int32 Id = INDEX_NONE;
    FString Name;
    ECombatLife Life = ECombatLife::Conscious;
    /** The saved level; a dead hero keeps its death level. */
    int32 Level = 1;
    /** The level before the last victory: Level - 1 for a hero that levelled, otherwise Level. */
    int32 LevelBefore = 1;
    /** This hero gained a level at the last victory (a survivor of victories 1-4, between that victory and the next fight). */
    bool bLevelled = false;
    FBasicDamageParts Damage;
    /** Basic damage before the last victory's level (equal to Damage.Total unless bLevelled). */
    int32 DamageBeforeLevel = 0;
    int32 GearRank = 0;
    bool bReserve = false, bHoned = false;
    FResourceCaps Caps;
    /** The pools the hero's build has, at its own caps. */
    TArray<FPoolFacts> Pools;
    /** Reward phase: Gear, Reserve and Honed, in that order. Empty otherwise. */
    TArray<FRewardCardFacts> Cards;
    /** Recovery phase, survivors only: the camp's HP and pools before -> after. */
    bool bCamp = false;
    int32 HpBefore = 0, HpAfter = 0, MaxHP = 0;
    TArray<FPoolChange> Camp;
};

struct DNDDICEROLL_API FProgressionFacts
{
    EExpeditionPhase Phase = EExpeditionPhase::Route;
    int32 Victories = 0;
    /** The run offers Reserve and Honed (Compact, catalog revision 10 or later). */
    bool bUpgradeCards = false;
    /** Reward phase: the potions this reward gives, whatever the card. */
    int32 RewardPotions = 0;
    /** Reward phase: the level the last victory's survivors reached. */
    int32 SurvivorLevel = 0;
    /** The party, in slot order. */
    TArray<FHeroProgressFacts> Heroes;
};

struct DNDDICEROLL_API FProgressionQuery
{
    static FProgressionFacts For(const FExpeditionRun& Run);
    /** The parts of FBuildCatalog::AttackModifier for this hero: build base, levels, gear and an active Rage. */
    static FBasicDamageParts BasicDamage(const FCombatantState& Hero);
};
