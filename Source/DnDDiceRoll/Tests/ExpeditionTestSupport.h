#pragma once

#include "Run/ExpeditionRun.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"

namespace ExpeditionTestSupport
{
inline bool SameRun(const FExpeditionSnapshot& A, const FExpeditionSnapshot& B)
{
    return FExpeditionSnapshot::StaticStruct()->CompareScriptStruct(&A, &B, 0);
}
// Checkpoint-failure fixtures put a directory at "<save>.pending" so the write fails.
// On Linux the engine's Unix file layer logs each failed open as a warning, and the
// generic file writer opens twice (before and after creating the folder). Declare
// exactly those messages for this one save path. Other platforms do not log them.
inline void ExpectFailedCheckpointWrites(FAutomationTestBase& Test, const FString& SavePath, int32 FailedWrites)
{
#if PLATFORM_UNIX
    const FString Fixture = FPaths::GetCleanFilename(FPaths::GetPath(SavePath)) / FPaths::GetCleanFilename(SavePath);
    Test.AddExpectedMessage(
        FString::Printf(TEXT("^open\\('.*/\\Q%s\\E\\.pending', Flags=0x[0-9A-F]{8}\\) failed: errno=21 \\(Is a directory\\)$"), *Fixture),
        ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Exact, 2 * FailedWrites);
#endif
}
// Make the next checkpoint write to SavePath fail once, declaring its expected warnings.
// Use once per save path; AllowCheckpointWrites undoes it.
inline void FailNextCheckpointWrite(FAutomationTestBase& Test, const FString& SavePath)
{
    ExpectFailedCheckpointWrites(Test, SavePath, 1);
    IFileManager::Get().MakeDirectory(*(SavePath + TEXT(".pending")), true);
}
inline void AllowCheckpointWrites(const FString& SavePath)
{
    IFileManager::Get().DeleteDirectory(*(SavePath + TEXT(".pending")), false, true);
}
inline int32 Distance(FIntPoint A, FIntPoint B) { return FMath::Abs(A.X-B.X) + FMath::Abs(A.Y-B.Y); }

// A bounded player policy using only commands, previews and snapshots. No stat overrides.
// Commands, when given, receives every party command in order so it can be replayed elsewhere.
inline bool FinishCombat(FExpeditionRun& Run, bool bFight = true, TArray<FCombatCommand>* Commands = nullptr)
{
    auto Submit = [&Run, Commands](const FCombatCommand& Command) { if (Commands) { Commands->Add(Command); } return Run.Submit(Command); };
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
                if (Active->HP <= Active->MaxHP / 2 && Battle.LegalTargets(Id, Action).Contains(Id)) { Submit({Action, Id, {}, Id}); }
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
                if (!Moves.IsEmpty()) { Submit({ECombatCommandType::Move, Id, Moves[0]}); }
            }
            Targets = Battle.LegalTargets(Id, ECombatCommandType::MagicMissile);
            if (!Targets.IsEmpty()) { Submit({ECombatCommandType::MagicMissile, Id, {}, Targets[0]}); }
            else
            {
                Targets = Battle.LegalTargets(Id);
                if (!Targets.IsEmpty()) { Submit({ECombatCommandType::Attack, Id, {}, Targets[0]}); }
            }
        }
        if (Run.Progress().Phase == EExpeditionPhase::Combat) { Submit({ECombatCommandType::EndTurn, Id}); }
    }
    return Run.Progress().Phase != EExpeditionPhase::Combat;
}

// Continues a run with the scripted choices the full-run tests use: the first offer, FinishCombat, the reward to the
// first conscious survivor, and the camp. True when the run ends in Victory.
inline bool FinishRunScripted(FExpeditionRun& Run)
{
    FString Error;
    for (int32 Guard = 0; Guard < 64; ++Guard)
    {
        switch (Run.Progress().Phase)
        {
        case EExpeditionPhase::Victory: return true;
        case EExpeditionPhase::Defeat: return false;
        case EExpeditionPhase::Route: if (!Run.ChooseRoute(Run.Progress().Offers[0].Id, Error)) { return false; } break;
        case EExpeditionPhase::Combat: if (!FinishCombat(Run)) { return false; } break;
        case EExpeditionPhase::Reward:
        {
            int32 Survivor = 1;
            while (Survivor <= 4 && Run.Battle().Find(Survivor)->Life != ECombatLife::Conscious) { ++Survivor; }
            if (Survivor > 4 || !Run.ClaimReward(Survivor, Error)) { return false; }
            break;
        }
        case EExpeditionPhase::Recovery: if (!Run.Recover(true, Error)) { return false; } break;
        }
    }
    return false;
}

// Since the Compact tuning of catalog revision 8 (#184), the scripted FinishCombat player can lose a run. A test whose
// subject is a completed five-fight run (persistence, progression) uses the first seed from 42 on that Accept takes.
// The search is deterministic; callers assert that a seed was found, so a later retune that makes it impossible fails.
inline int32 FirstSeedWhere(TFunctionRef<bool(int32)> Accept, int32 From = 42, int32 Tries = 64)
{
    for (int32 Seed = From; Seed < From + Tries; ++Seed) { if (Accept(Seed)) { return Seed; } }
    return INDEX_NONE;
}
}
