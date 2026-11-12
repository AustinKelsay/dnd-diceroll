#pragma once

#include "CoreMinimal.h"
#include "Combat/CombatBattle.h"

/**
 * The ten clips of the shared humanoid skeleton (#147, Brace added by #150), by role. A pose is presentation only: playback
 * chooses it from committed results and never reads it back, and every figure converges on Rest(Life) once playback settles.
 * Brace is the short weapon-up rally a martial self or ally effect plays (Rage, Second Wind, a non-spell ward or touch); Cast
 * is the arms-raised gesture of a spell.
 */
enum class EUnitPose : uint8 { Idle, Walk, Melee, Ranged, Cast, Hit, Downed, Death, Recover, Brace };

namespace ExpeditionPose
{
constexpr int32 Count = 10;

/** The clip name in the animation set and in the Blender export ("Idle", "Walk", ...). */
DNDDICEROLL_API const TCHAR* ClipName(EUnitPose Pose);
/** Idle and Walk repeat; every other clip plays once and holds its last frame. */
DNDDICEROLL_API bool IsLooping(EUnitPose Pose);
/** The pose the committed state implies once nothing is playing: Death when dead, Downed when downed, otherwise Idle. */
DNDDICEROLL_API EUnitPose Rest(ECombatLife Life);
/** The actor's clip for a committed command. A ranged attack is one aimed more than one cell away. A spell attack (see
 * IsSpellAttack) casts at any distance instead of swinging or shooting. */
DNDDICEROLL_API EUnitPose ForCommand(ECombatCommandType Type, bool bRanged, bool bSpellAttack = false);
/** A fixed command's clip is chosen by ForCommand; Second Wind is a martial self-heal and plays Brace, Cure Wounds and Magic Missile
 * are spells and cast, a potion, a rescue and End Turn have no clip, and Special1 and Special2 are chosen by ForAbility from the ability. */
/** Presentation: whether a basic Attack is a spell, from the rules reference's fixed profiles. The wizard's basic attack is
 * Fire Bolt at any distance; the cleric's ranged attack is Sacred Flame (a save in SRD, an attack roll in Compact; a spell in
 * both). #150 batch 3 names the sorcerer, warlock and druid the same way as the cleric: their ranged basic attack is a spell bolt (an ember bolt, an
 * eldritch bolt, a thorn flame), and #150 batch 4 the bard (a dissonant teal bolt); the artificer keeps a crossbow bolt (a lightless arrow) for its basic attack and they swing a weapon up close. A presentation mapping keyed by class: nothing in the catalog, rules, save or RNG changes.
 * Other roles' basic attacks are weapon attacks, and only Attack is affected (Precision Strike is a weapon maneuver). */
DNDDICEROLL_API bool IsSpellAttack(ECombatCommandType Type, EAdventurerRole Role, bool bRanged);
/** The actor's clip for one effect of a Special1 or Special2 command (#150 batch 2), in this order:
 *  1. a spell (the ability's bSpell fact) casts, whatever its normalized effect, a swap or mitigation included;
 *  2. a non-spell ability that attacks (Strike, WeaponAttack, Area, Push, Surge, Missile) swings up close and shoots from more than one cell;
 *  3. a non-spell self or ally effect (Rage, Protect, Healing, a swap, a used mitigation) plays Brace, the weapon-up rally;
 *  4. nothing else has a clip: a potion, a rescue, End Turn (ForCommand), and a plain move walks (Walk). */
DNDDICEROLL_API EUnitPose ForEffect(EAbilityEffect Effect, bool bRanged, bool bSpell = false);
/** The same choice from the catalog's ability facts (Effect and bSpell), the one rule playback and the tests share. */
DNDDICEROLL_API EUnitPose ForAbility(const FAbilityFacts& Ability, bool bRanged);
/** The target's clip when damage lands and it stays on its feet. */
DNDDICEROLL_API EUnitPose ForDamage();
/** The clip for a unit newly downed (false) or killed (true). */
DNDDICEROLL_API EUnitPose ForFall(bool bDead);
/** The clip for a downed unit that is healed or rescued. */
DNDDICEROLL_API EUnitPose ForRecovery();
}
