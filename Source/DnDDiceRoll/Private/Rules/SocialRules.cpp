#include "Rules/SocialRules.h"
#include "Rules/CheckRules.h"

int32 FSocialRules::InfluenceDc(ENpcAttitude Attitude)
{
    return Attitude == ENpcAttitude::Friendly ? 10 : Attitude == ENpcAttitude::Indifferent ? 15 : 20;
}

ENpcAttitude FSocialRules::AfterInfluence(ENpcAttitude Attitude, int32 Total)
{
    return Total >= InfluenceDc(Attitude) ? FPerformanceRules::ShiftAttitude(Attitude, 1) : Attitude;
}

bool FSocialRules::LieSucceeds(int32 DeceptionTotal, int32 InsightTotal)
{
    return FCheckRules::ActorWinsContest(DeceptionTotal, InsightTotal);
}

ESocialOutcome FSocialRules::OutcomeFor(ENpcAttitude Before, ENpcAttitude After)
{
    if (Before == ENpcAttitude::Hostile && After != ENpcAttitude::Hostile) { return ESocialOutcome::AvoidFight; }
    if (Before == ENpcAttitude::Indifferent && After == ENpcAttitude::Friendly) { return ESocialOutcome::Information; }
    if (Before == ENpcAttitude::Friendly && After == ENpcAttitude::Friendly) { return ESocialOutcome::Reward; }
    return ESocialOutcome::None;
}
