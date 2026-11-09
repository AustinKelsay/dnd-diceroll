#pragma once

#include "CoreMinimal.h"

/** Locks and traps for chests and doors. Tiers follow the SRD ladder: simple 10, good 15, superior 20. */
enum class ELockTier : uint8 { Simple, Good, Superior };

enum class EDisarmResult : uint8 { Disarmed, Failed, Triggered };

struct DNDDICEROLL_API FLockTrapRules
{
    static int32 LockDc(ELockTier Tier);
    /** Picking a lock needs thieves' tools in hand. */
    static bool CanPickLock(bool bHasThievesTools);
    static bool Picks(int32 Total, ELockTier Tier, bool bHasThievesTools);
    /** Finding a trap: Investigation total or passive Perception against the trap's DC. */
    static bool FindsTrap(int32 BestTotalOrPassive, int32 TrapDc);
    /** Disarming needs thieves' tools; failing by 5 or more sets the trap off. */
    static EDisarmResult Disarm(int32 Total, int32 TrapDc, bool bHasThievesTools);
};
