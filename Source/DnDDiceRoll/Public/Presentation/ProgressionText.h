#pragma once

#include "CoreMinimal.h"
#include "Run/ProgressionQuery.h"

/**
 * Player-facing words for the progression facts (#218, contract 13 sections 4 and 8). Formatting only: every number comes from
 * FProgressionQuery, so the reward cards, the camp and the hero details always say what the rules commit (ADR-0001).
 */
namespace ProgressionText
{
DNDDICEROLL_API FString Arrow();
/** "VICTORY 2 OF 5 · SURVIVORS REACH LEVEL 3". */
DNDDICEROLL_API FString RewardTitle(const FProgressionFacts& Facts);
/** "Every card also gives +2 healing potions". */
DNDDICEROLL_API FString PotionLine(const FProgressionFacts& Facts);
DNDDICEROLL_API FString CardName(ERewardCard Card);
/** What the card does, in one line. */
DNDDICEROLL_API FString CardRule(ERewardCard Card);
/** Why the card cannot be taken, or (Gear at rank 3) what it gives instead; empty for ERewardCardNote::None. */
DNDDICEROLL_API FString NoteText(ERewardCardNote Note);
/** The card's before -> after lines ("Basic damage 6 -> 7", "Kit 1/2 -> 2/3", "Volley 4 -> 5 · Hunter's Finish 4 -> 6"), then the potions. */
DNDDICEROLL_API TArray<FString> CardLines(const FRewardCardFacts& Card);
/** The Claim button: "Give Reserve to Aria", or the card's blocker. */
DNDDICEROLL_API FString ClaimLabel(const FHeroProgressFacts& Hero, const FRewardCardFacts& Card);
/** "Basic damage 6 = 3 build + 2 levels + 1 gear" (+ Rage while raging). */
DNDDICEROLL_API FString DamageBreakdown(const FHeroProgressFacts& Hero);
/** "Upgrades: Reserve, Honed"; empty when the hero has neither. */
DNDDICEROLL_API FString UpgradesLine(const FHeroProgressFacts& Hero);
/** The party card after a victory: "basic 5 -> 6" for a hero that levelled. */
DNDDICEROLL_API FString LevelGainLine(const FHeroProgressFacts& Hero);
/** A fallen hero: "fell at Lv2". */
DNDDICEROLL_API FString FallenLine(const FHeroProgressFacts& Hero);
/** The camp's line for one survivor: "Aria  HP 12/30 -> 22/30 · Kit 1/3 -> 2/3". */
DNDDICEROLL_API FString CampLine(const FHeroProgressFacts& Hero);
/** One pool before -> after: "Kit 1/2 -> 2/3". */
DNDDICEROLL_API FString PoolChangeText(const FPoolChange& Pool);
}
