#include "Combat/ActionFacts.h"
#include "Party/BuildCatalog.h"

// Stub for the failing-test-first step: every fact is "unavailable, no blockers, costs nothing".
FActionFacts FActionFactsQuery::For(const FCombatBattle& /*Battle*/, int32 /*ActorId*/, ECombatCommandType Action)
{
    FActionFacts Facts;
    Facts.Command = Action;
    return Facts;
}

TArray<ECombatCommandType> FActionFactsQuery::BarFor(const FCombatBattle& /*Battle*/, int32 /*ActorId*/)
{
    return {};
}
