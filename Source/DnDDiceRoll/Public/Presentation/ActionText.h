#pragma once

#include "CoreMinimal.h"
#include "Combat/ActionFacts.h"

/**
 * Words for action facts (#58). The facts carry no strings; every cost, usage and reason phrase the
 * player reads comes from here, and these helpers read facts only: they never decide legality.
 * Each enum is mapped by an exhaustive switch, and a test walks every enumerator to prove it has text.
 */
namespace ExpeditionStyle
{
/** The shared name table for what a command spends: "Move", "Action", "Bonus action", and "No cost" for End Turn. */
DNDDICEROLL_API FString CostName(EActionCostKind Cost);
/** What a resource is called on a button and in a tooltip: "Kit", "Slot", "Wind", "Maneuver", "Rage", "Potion". */
DNDDICEROLL_API FString ResourceName(EActionResource Resource);
/** The short cost pill: "Action", "Bonus" or "Move". Empty for a command with no cost (End Turn). */
DNDDICEROLL_API FString CostPillText(const FActionFacts& Facts);
/**
 * The usage pill: the resources spent with what is held ("1 Kit · 2/3", "1 Kit · 1/1 + 1 Slot · 2/2"),
 * "At will" when nothing is spent, plus the Rage stance ("Needs Rage", "Not while raging"). Empty for End Turn.
 */
DNDDICEROLL_API FString UsagePillText(const FActionFacts& Facts);
/** The movement budget as the chips and the Move pill show it: "Move 6". */
DNDDICEROLL_API FString MoveBudgetText(int32 MovementLeft);
/** The cost pill of the combined Attack / Move button: both costs with the movement left ("Action · Move 6"). */
DNDDICEROLL_API FString AttackMovePillText(const FActionFacts& Attack, const FActionFacts& Move, int32 MovementLeft);
/**
 * The usage line on a dock button, short enough to fit it without shrinking: "Kit 2/2", "Kit 2/2 + Slot 2/2",
 * "Rage 2/2", "Needs Rage", "At will". With a resource the stance stays in the tooltip (UsageLines). Empty for End Turn.
 */
DNDDICEROLL_API FString UsageTagText(const FActionFacts& Facts);
/** What camp gives back for one resource: "Camp restores 1", "Camp restores all", or "Not refilled by camp". Empty when unknown. */
DNDDICEROLL_API FString CampRecoveryText(const FActionResourceCost& Cost);
/**
 * The tooltip's usage lines: one per resource with what camp gives back ("1 Kit · 2/3 · Camp restores 1"), then the
 * Rage stance, or "At will" when nothing is spent. Empty for a command with no cost.
 */
DNDDICEROLL_API TArray<FString> UsageLines(const FActionFacts& Facts);
/** What the build picker says about one ability: its cost and usage, from facts ("Bonus action | Needs Rage"). */
DNDDICEROLL_API FString AbilitySummaryText(const FActionFacts& Facts);
/** One reason an action is unavailable, as a short sentence fragment ("Action already used", "Need 1 Slot, have 0"). */
DNDDICEROLL_API FString BlockerText(const FActionBlocker& Blocker);
/** The words of an action tooltip, apart from the catalog's effect text. */
struct FActionTooltipText
{
    FString Headline;
    TArray<FString> Usage;
    /** The blockers in order, empty when the action is available. */
    TArray<FString> Reasons;
};
/**
 * The tooltip text for facts. For the combined Attack / Move button pass the Move facts as well: the reasons then
 * explain whichever of the two is unavailable (an Action already used, no movement left), each once.
 */
DNDDICEROLL_API FActionTooltipText TooltipText(const FActionFacts& Facts, const FActionFacts* AlsoMove = nullptr);
/** The tooltip headline: the command's name and its cost ("Magic Missile · Action"). */
DNDDICEROLL_API FString TooltipHeadline(const FActionFacts& Facts);
}
