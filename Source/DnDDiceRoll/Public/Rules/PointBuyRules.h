#pragma once

#include "CoreMinimal.h"
#include "Rules/AbilityRules.h"

enum class EAdventurerRole : uint8;

/**
 * SRD 5.2.1 "Point Cost" ability generation (Character Creation, Step 3, p. 20): 27 points, every score
 * starts at 8, scores 8..15 before any bonus. Costs: 8=0, 9=1, 10=2, 11=3, 12=4, 13=5, 14=7, 15=9.
 * Pure value rules: no state, no random numbers. Local proposal (character creator), not yet approved scope.
 */
struct DNDDICEROLL_API FPointBuyRules
{
    static constexpr int32 Budget = 27;
    static constexpr int32 MinScore = 8;
    static constexpr int32 MaxScore = 15;
    /** Point cost of one score, or INDEX_NONE outside 8..15. */
    static int32 Cost(int32 Score);
    /** Total cost of six scores, or INDEX_NONE when any score is outside 8..15. */
    static int32 TotalCost(const FAbilityScores& Scores);
    /** Points left from the 27 budget (negative when overspent, INDEX_NONE-safe: out of range returns INDEX_NONE). */
    static int32 Remaining(const FAbilityScores& Scores);
    /** Legal point-buy spread: every score 8..15 and total cost <= 27. Error is reset on success. */
    static bool Validate(const FAbilityScores& Scores, FString& Error);
    /** Cost of raising Ability by one, or INDEX_NONE at 15. */
    static int32 RaiseCost(const FAbilityScores& Scores, EAbility Ability);
    static bool CanRaise(const FAbilityScores& Scores, EAbility Ability);
    static bool CanLower(const FAbilityScores& Scores, EAbility Ability);
    /** Applies one step when legal; Scores is untouched otherwise. */
    static bool Raise(FAbilityScores& Scores, EAbility Ability);
    static bool Lower(FAbilityScores& Scores, EAbility Ability);
    /** Modifier the score will give (SRD floor((score - 10) / 2)); a forecast for the UI. */
    static int32 ModifierForecast(const FAbilityScores& Scores, EAbility Ability);
    /** All six scores at 8 (27 points to spend). */
    static FAbilityScores Baseline();
    /** SRD 5.2.1 "Standard Array by Class" (p. 20). The standard array 15/14/13/12/10/8 costs exactly 27. */
    static FAbilityScores Recommended(EAdventurerRole Role);
    /** The 12 SRD 5.2.1 classes (Artificer is not in the SRD). */
    static bool IsSrdClass(EAdventurerRole Role);
    static void Set(FAbilityScores& Scores, EAbility Ability, int32 Value);
};
