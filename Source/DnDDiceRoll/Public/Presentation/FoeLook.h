#pragma once

#include "CoreMinimal.h"
#include "Combat/CombatBattle.h"

/**
 * Which rigged foe figure a combatant draws (#150 foe batch). Presentation only: it reads a combatant and never writes it, so nothing here
 * touches saved state, rules or the RNG.
 *
 * The look follows the saved FCombatantState::Role (#188): "Raider", "Archer", "Skirmisher", "Brute" and "Warden". Every revision saves Guarded
 * raiders as "Raider" too, named "Scout N", so the name tells a scout from a raider; nothing else reads the name.
 */
enum class EFoeLook : uint8 { Scout, Raider, Archer, Skirmisher, Brute, Warden };

namespace ExpeditionFoe
{
constexpr int32 Count = 6;

/** The UDiceRollTheme::FoeRigs key (the archetype name) of a look: "Scout", "Raider", "Archer", "Skirmisher", "Brute", "Warden". */
DNDDICEROLL_API FName RigKey(EFoeLook Look);

/** The look a saved Role names, case-insensitively: Raider, Archer, Skirmisher, Brute or Warden. False for anything else (including "Scout",
 * which is a look, not a saved role). */
DNDDICEROLL_API bool LookFromSavedRole(const FString& Role, EFoeLook& OutLook);

/**
 * The look of an enemy combatant. A ranged foe (Range > 1) is an archer. Otherwise its saved Role decides; a Raider whose name holds "scout" (any
 * case) is a scout. A Role that names no look draws as a scout, the plainest figure.
 */
DNDDICEROLL_API EFoeLook LookOf(const FCombatantState& Unit);
}
