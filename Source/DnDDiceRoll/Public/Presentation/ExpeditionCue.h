#pragma once

#include "CoreMinimal.h"
#include "ExpeditionCue.generated.h"

/** Sounds and moments the playback asks the world to express. Reflected so the recorded-cue data asset can key its map by it (#242). */
UENUM(BlueprintType)
enum class EExpeditionCue : uint8
{
    DiceRoll, DiceLand, Hit, Critical, Miss, Heal, Step, Click, Downed, Death,
    PartyTurn, EnemyTurn, Victory, Defeat, Spell, Bow, Swing, Camp, Loot, Route
};

/** How many cues there are; a test checks it against the reflected enum, so a new cue must update it. */
constexpr int32 ExpeditionCueCount = static_cast<int32>(EExpeditionCue::Route) + 1;
