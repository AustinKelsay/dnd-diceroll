#pragma once

#include "CoreMinimal.h"
#include "Rules/CheckRules.h"

enum class EAdventurerRole : uint8;

enum class EStandardAction : uint8 { Attack, Dash, Disengage, Dodge, Help, Hide };
enum class EActionCost : uint8 { Action, BonusAction, Reaction, Free };

/** SRD standard combat actions, without battle state. */
struct DNDDICEROLL_API FStandardActionRules
{
	/** Rogues from level 2 (Cunning Action) may Dash, Disengage or Hide as a bonus action. */
	static bool CanTakeAsBonusAction(EStandardAction Action, EAdventurerRole Role, int32 Level);
	static EActionCost Cost(EStandardAction Action, EAdventurerRole Role, int32 Level, bool bActionAlreadyUsed);
	/** Short label for feature menus: Action / Bonus / Reaction / Free. */
	static const TCHAR* CostLabel(EActionCost Cost);
	/** Each Dash adds the creature's speed again for this turn. */
	static int32 MovementBudget(int32 SpeedCells, int32 Dashes);
	/** Dodge: attackers it can see have disadvantage, and it has advantage on Dexterity saves. */
	static ERollMode AttackAgainstDodger(bool bDodging, bool bSeesAttacker);
	static ERollMode DexSaveWhileDodging(bool bDodging);
	/** Disengage: leaving reach provokes no opportunity attacks this turn. */
	static bool LeavingReachProvokes(bool bDisengaged);
};