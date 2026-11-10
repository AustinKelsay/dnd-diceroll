#pragma once

#include "CoreMinimal.h"
#include "Combat/CombatBattle.h"

/**
 * The nine clips of the shared humanoid skeleton (#147), by role. A pose is presentation only: playback chooses it
 * from committed results and never reads it back, and every figure converges on Rest(Life) once playback settles.
 */
enum class EUnitPose : uint8 { Idle, Walk, Melee, Ranged, Cast, Hit, Downed, Death, Recover };

namespace ExpeditionPose
{
constexpr int32 Count = 9;

/** The clip name in the animation set and in the Blender export ("Idle", "Walk", ...). */
DNDDICEROLL_API const TCHAR* ClipName(EUnitPose Pose);
/** Idle and Walk repeat; every other clip plays once and holds its last frame. */
DNDDICEROLL_API bool IsLooping(EUnitPose Pose);
/** The pose the committed state implies once nothing is playing: Death when dead, Downed when downed, otherwise Idle. */
DNDDICEROLL_API EUnitPose Rest(ECombatLife Life);
/** The actor's clip for a committed command. A ranged attack is one aimed more than one cell away. A spell attack (see
 * IsSpellAttack) casts at any distance instead of swinging or shooting. */
DNDDICEROLL_API EUnitPose ForCommand(ECombatCommandType Type, bool bRanged, bool bSpellAttack = false);
/** Presentation: whether a basic Attack is a spell, from the rules reference's fixed profiles. The wizard's basic attack is
 * Fire Bolt at any distance; the cleric's ranged attack is Sacred Flame (a save in SRD, an attack roll in Compact; a spell in
 * both). Other roles' basic attacks are weapon attacks, and only Attack is affected (Precision Strike is a weapon maneuver). */
DNDDICEROLL_API bool IsSpellAttack(ECombatCommandType Type, EAdventurerRole Role, bool bRanged);
/** The actor's clip for one effect of a Special1 or Special2 command. A spell (the ability's bSpell fact) casts, whatever its
 * normalized effect, so a damaging spell is not mistaken for a weapon attack; swaps and mitigation still have no clip. */
DNDDICEROLL_API EUnitPose ForEffect(EAbilityEffect Effect, bool bRanged, bool bSpell = false);
/** The target's clip when damage lands and it stays on its feet. */
DNDDICEROLL_API EUnitPose ForDamage();
/** The clip for a unit newly downed (false) or killed (true). */
DNDDICEROLL_API EUnitPose ForFall(bool bDead);
/** The clip for a downed unit that is healed or rescued. */
DNDDICEROLL_API EUnitPose ForRecovery();
}
