#pragma once

#include "Run/ExpeditionRun.h"

namespace ExpeditionTestSupport
{
inline bool SameRun(const FExpeditionSnapshot& A, const FExpeditionSnapshot& B)
{
    return FExpeditionSnapshot::StaticStruct()->CompareScriptStruct(&A, &B, 0);
}
inline int32 Distance(FIntPoint A, FIntPoint B) { return FMath::Abs(A.X-B.X) + FMath::Abs(A.Y-B.Y); }

// A bounded player policy using only commands, previews and snapshots. No stat overrides.
inline bool FinishCombat(FExpeditionRun& Run, bool bFight = true)
{
    for (int32 Turn = 0; Turn < 400 && Run.Progress().Phase == EExpeditionPhase::Combat; ++Turn)
    {
        const auto& Battle = Run.Battle();
        const int32 Id = Battle.ActiveId();
        const auto* Active = Battle.Find(Id);
        if (!Active) { return false; }
        if (Active->Side == ECombatSide::Enemy) { Run.PlayEnemyTurn(); continue; }
        if (bFight)
        {
            for (auto Action : {ECombatCommandType::SecondWind, ECombatCommandType::Potion, ECombatCommandType::CureWounds})
            {
                if (Active->HP <= Active->MaxHP / 2 && Battle.LegalTargets(Id, Action).Contains(Id)) { Run.Submit({Action, Id, {}, Id}); }
            }
            auto Targets = Battle.LegalTargets(Id);
            if (Targets.IsEmpty())
            {
                auto Moves = Battle.LegalMoves(Id);
                Moves.Sort([&Battle](FIntPoint A, FIntPoint B)
                {
                    auto Nearest = [&Battle](FIntPoint Cell)
                    {
                        int32 Best = 1000;
                        for (const auto& Enemy : Battle.Snapshot().Encounter.Combatants)
                        { if (Enemy.Side == ECombatSide::Enemy && Enemy.HP > 0) { Best = FMath::Min(Best, Distance(Cell, Enemy.Cell)); } }
                        return Best;
                    };
                    const int32 DA = Nearest(A), DB = Nearest(B);
                    return DA == DB ? (A.Y == B.Y ? A.X < B.X : A.Y < B.Y) : DA < DB;
                });
                if (!Moves.IsEmpty()) { Run.Submit({ECombatCommandType::Move, Id, Moves[0]}); }
            }
            Targets = Battle.LegalTargets(Id, ECombatCommandType::MagicMissile);
            if (!Targets.IsEmpty()) { Run.Submit({ECombatCommandType::MagicMissile, Id, {}, Targets[0]}); }
            else
            {
                Targets = Battle.LegalTargets(Id);
                if (!Targets.IsEmpty()) { Run.Submit({ECombatCommandType::Attack, Id, {}, Targets[0]}); }
            }
        }
        if (Run.Progress().Phase == EExpeditionPhase::Combat) { Run.Submit({ECombatCommandType::EndTurn, Id}); }
    }
    return Run.Progress().Phase != EExpeditionPhase::Combat;
}
}
