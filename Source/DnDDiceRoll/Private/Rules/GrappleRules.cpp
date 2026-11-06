#include "Rules/GrappleRules.h"

bool FGrappleRules::SizeAllows(ECreatureSize Attacker, ECreatureSize Target)
{
    return (int32)Target <= (int32)Attacker + 1;
}

int32 FGrappleRules::ContestOutOf400(int32 ActorModifier, int32 DefenderModifier)
{
    int32 Wins = 0;
    for (int32 ActorFace = 1; ActorFace <= 20; ++ActorFace)
    {
        for (int32 DefenderFace = 1; DefenderFace <= 20; ++DefenderFace)
        {
            Wins += ActorFace + ActorModifier > DefenderFace + DefenderModifier ? 1 : 0;
        }
    }
    return Wins;
}

int32 FGrappleRules::GrappledSpeed(int32 SpeedCells) { return 0; }
int32 FGrappleRules::DragCost(int32 Cells) { return FMath::Max(0, Cells) * 2; }
int32 FGrappleRules::EscapeOutOf400(int32 EscaperModifier, int32 GrapplerModifier) { return ContestOutOf400(EscaperModifier, GrapplerModifier); }
int32 FGrappleRules::ShovePushCells(bool bWon, EShoveChoice Choice) { return bWon && Choice == EShoveChoice::PushAway ? 1 : 0; }
bool FGrappleRules::ShoveKnocksProne(bool bWon, EShoveChoice Choice) { return bWon && Choice == EShoveChoice::KnockProne; }
