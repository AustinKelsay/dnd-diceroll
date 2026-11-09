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
/** What camp gives back for one resource: "Camp restores 1", "Camp restores all", or "Not refilled by camp". Empty when unknown. */
DNDDICEROLL_API FString CampRecoveryText(const FActionResourceCost& Cost);
/** One reason an action is unavailable, as a short sentence fragment ("Action already used", "Need 1 Slot, have 0"). */
DNDDICEROLL_API FString BlockerText(const FActionBlocker& Blocker);
/** The tooltip headline: the command's name and its cost ("Magic Missile · Action"). */
DNDDICEROLL_API FString TooltipHeadline(const FActionFacts& Facts);
}
