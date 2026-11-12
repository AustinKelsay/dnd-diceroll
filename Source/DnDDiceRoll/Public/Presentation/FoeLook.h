#pragma once

#include "CoreMinimal.h"
#include "Combat/CombatBattle.h"

/**
 * Which rigged foe figure a combatant draws (#150 foe batch). Presentation only: it reads a combatant and never writes it, so nothing here
 * touches saved state, rules or the RNG.
 *
 * Today's enemies are scouts ("Scout N"), raiders ("Raider N") and archers ("Vault bow"; ranged, Range > 1); the encounter-identity contract
 * (#188) adds the skirmisher ("Skirmisher"), the brute ("Raider brute") and the warden ("Vault warden"). New names keep the words presentation
 * reads today, so the order of the name table below matters: "Raider brute" also contains "raider".
 */
enum class EFoeLook : uint8 { Scout, Raider, Archer, Skirmisher, Brute, Warden };

namespace ExpeditionFoe
{
constexpr int32 Count = 6;

/** The UDiceRollTheme::FoeRigs key (the archetype name) of a look: "Scout", "Raider", "Archer", "Skirmisher", "Brute", "Warden". */
DNDDICEROLL_API FName RigKey(EFoeLook Look);

/**
 * THE SEAM FOR #188: the saved FCombatantState::Role. When #188 lands, a combatant's Role names its role explicitly, and this is the one place that
 * reads it. It returns true only for a Role that names a look outright and unambiguously (today's saves carry "Raider" for scouts and raiders alike
 * and "Archer" for archers, so those two are not decided here). Name matching in LookFromName then stops being the source of truth: delete the name
 * table and make this return every role once the saved Role is reliable.
 */
DNDDICEROLL_API bool LookFromSavedRole(const FString& Role, EFoeLook& OutLook);

/**
 * The name-to-role table, the rule today's enemies are drawn by. A ranged foe (Range > 1) is an archer. Otherwise the first matching word wins, in
 * this order: "warden", "brute", "skirmisher", "raider" (case-insensitive, anywhere in the name; "Raider brute" is a brute, "Vault warden" a warden);
 * anything else, including every scout and any unknown name, is a scout.
 */
DNDDICEROLL_API EFoeLook LookFromName(const FString& Name, int32 Range);

/** The look of an enemy combatant: the saved Role when it names one (LookFromSavedRole), otherwise the name table. */
DNDDICEROLL_API EFoeLook LookOf(const FCombatantState& Unit);
}
