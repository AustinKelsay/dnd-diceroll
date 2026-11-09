#include "Rules/LockTrapRules.h"

int32 FLockTrapRules::LockDc(ELockTier Tier)
{
    return Tier == ELockTier::Simple ? 10 : Tier == ELockTier::Good ? 15 : 20;
}

bool FLockTrapRules::CanPickLock(bool bHasThievesTools) { return bHasThievesTools; }

bool FLockTrapRules::Picks(int32 Total, ELockTier Tier, bool bHasThievesTools)
{
    return CanPickLock(bHasThievesTools) && Total >= LockDc(Tier);
}

bool FLockTrapRules::FindsTrap(int32 BestTotalOrPassive, int32 TrapDc) { return BestTotalOrPassive >= TrapDc; }

EDisarmResult FLockTrapRules::Disarm(int32 Total, int32 TrapDc, bool bHasThievesTools)
{
    if (!bHasThievesTools) { return EDisarmResult::Failed; }
    if (Total >= TrapDc) { return EDisarmResult::Disarmed; }
    return TrapDc - Total >= 5 ? EDisarmResult::Triggered : EDisarmResult::Failed;
}
