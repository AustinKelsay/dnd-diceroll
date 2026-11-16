#pragma once

#include "CoreMinimal.h"
#include "ExpeditionCue.generated.h"

/**
 * Sounds and moments the playback asks the world to express. Reflected so the recorded-cue data asset can key its map by it (#242).
 * Append only: the data asset keys its map by name, and the order below is the recipe's cue_order. The last eight (#243) are the outcomes the first
 * twenty shared a cue for: a resource spent (Kit or Second Wind or maneuver use, spell slot, potion, Rage), damage a protection reduced, a new round,
 * the finale won (the artifact recovered) and the run lost.
 */
UENUM(BlueprintType)
enum class EExpeditionCue : uint8
{
    DiceRoll, DiceLand, Hit, Critical, Miss, Heal, Step, Click, Downed, Death,
    PartyTurn, EnemyTurn, Victory, Defeat, Spell, Bow, Swing, Camp, Loot, Route,
    SpendKit, SpendSlot, SpendPotion, SpendRage, Protection, RoundStart, Finale, RunEnd,
    /** The finale set piece (#250, #246): the Waystone's flare building to the shatter, and the seal shattering. */
    WaystoneFlare, WaystoneShatter
};

/** How many cues there are; a test checks it against the reflected enum, so a new cue must update it. */
constexpr int32 ExpeditionCueCount = static_cast<int32>(EExpeditionCue::WaystoneShatter) + 1;
