#include "Run/ExpeditionRun.h"
#include "Party/PartyContinuity.h"

bool FExpeditionRun::NewRun(int32 Seed, FString& Error)
{
    FExpeditionRun Candidate;
    Candidate.State.Seed = Seed;
    Candidate.State.RunId = FString::Printf(TEXT("EXP-%08X"), static_cast<uint32>(Seed));
    Candidate.State.GenerationRandomState = Seed;
    if (!Candidate.Combat.Start(FCombatBattle::TutorialEncounter(), Seed, Error)) { return false; }
    Candidate.bStarted = true;
    Candidate.OfferRoutes();
    *this = MoveTemp(Candidate);
    return true;
}

void FExpeditionRun::OfferRoutes()
{
    FRandomStream Random(State.GenerationRandomState);
    State.Offers.Reset();
    const bool bSecuredSupplies = !State.History.IsEmpty() && State.History.Last().Route.bDangerous;
    for (int32 Index = 0; Index < 2; ++Index)
    {
        FExpeditionRoute Route;
        Route.Id = State.EncountersCompleted * 2 + Index + 1;
        Route.EncounterSeed = Random.RandRange(0, MAX_int32 - 1);
        Route.Recipe = Random.RandRange(0, 2);
        Route.Width = Random.RandRange(8, 9);
        Route.Height = Random.RandRange(7, 8);
        Route.bDangerous = Index == 1;
        Route.EnemyCount = Index == 1 ? 3 : 2;
        Route.RewardPotions = Index == 1 || bSecuredSupplies ? 2 : 1;
        Route.Name = Index == 0 ? TEXT("Stream trail") : TEXT("Watchtower road");
        const TCHAR* Terrain[] = {TEXT("split stones"), TEXT("ruined pillars"), TEXT("broken wall")};
        Route.Preview = FString::Printf(TEXT("%s | %d %s | %s | +1 gear, +%d potions%s"),
            Index == 0 ? TEXT("Guarded") : TEXT("Dangerous"), Route.EnemyCount,
            Index == 0 ? TEXT("scouts") : TEXT("raiders incl. archer"), Terrain[Route.Recipe], Route.RewardPotions,
            Index == 0 && bSecuredSupplies ? TEXT(" (secured supplies)") : TEXT(""));
        if (State.EncountersCompleted == 2) { Route.Name = TEXT("Artifact vault: ") + Route.Name; Route.Preview = FString::Printf(TEXT("%s | %d guards incl. archer | %s | recover artifact; no loot"), Index == 0 ? TEXT("Guarded") : TEXT("Dangerous"), Route.EnemyCount, Terrain[Route.Recipe]); }
        State.Offers.Add(Route);
    }
    State.GenerationRandomState = Random.GetCurrentSeed();
    State.Phase = EExpeditionPhase::Route;
}

bool FExpeditionRun::ChooseRoute(int32 RouteId, FString& Error)
{
    Error.Reset();
    const FExpeditionRoute* Offered = State.Offers.FindByPredicate([RouteId](const FExpeditionRoute& Route) { return Route.Id == RouteId; });
    if (!bStarted || State.Phase != EExpeditionPhase::Route || !Offered)
    {
        Error = TEXT("Choose one of the current route opportunities."); return false;
    }
    FExpeditionRun Candidate = *this;
    const FCombatEncounter Layout = Candidate.Generate(*Offered);
    const bool bReady = State.EncountersCompleted == 0 ? Candidate.Combat.Start(Layout, Offered->EncounterSeed, Error) :
        FPartyContinuity::Carry(Candidate.Combat, Layout, Offered->EncounterSeed, Error);
    if (!bReady) { return false; }
    FExpeditionEncounterRecord Record;
    Record.Route = *Offered;
    Record.Survivors = 0;
    for (const auto& Unit : Candidate.Combat.Snapshot().Encounter.Combatants)
    { if (Unit.Side == ECombatSide::Party && Unit.Life == ECombatLife::Conscious) { ++Record.Survivors; } }
    Candidate.State.History.Add(Record);
    Candidate.State.Offers.Reset();
    Candidate.State.Phase = EExpeditionPhase::Combat;
    *this = MoveTemp(Candidate);
    return true;
}

FCombatEncounter FExpeditionRun::Generate(const FExpeditionRoute& Route) const
{
    FCombatEncounter Layout = FCombatBattle::TutorialEncounter();
    Layout.Width = Route.Width;
    Layout.Height = Route.Height;
    // Isolated obstacles leave a full open perimeter, spawn columns, and lanes between
    // every obstruction. No retry loop or seed can disconnect the open floor.
    const TArray<FIntPoint> Patterns[] = {{{3, 2}, {3, 4}}, {{4, 3}, {3, 4}}, {{3, 3}, {4, 3}}};
    Layout.BlockedCells = Patterns[Route.Recipe];
    Layout.Combatants.SetNum(4);
    for (int32 Index = 0; Index < 4; ++Index) { Layout.Combatants[Index].Cell = {1, Index + 1}; }
    for (int32 Index = 0; Index < Route.EnemyCount; ++Index)
    {
        FCombatantState Enemy;
        Enemy.Id = 20 + Index;
        Enemy.Side = ECombatSide::Enemy;
        const bool bArcher = Index == Route.EnemyCount - 1;
        Enemy.Name = bArcher ? TEXT("Vault bow") : FString::Printf(TEXT("%s %d"), Route.bDangerous ? TEXT("Raider") : TEXT("Scout"), Index + 1);
        Enemy.Role = bArcher ? TEXT("Archer") : TEXT("Raider");
        Enemy.Cell = {Route.Width - (Route.Recipe == 1 ? 3 : 2), (Route.Recipe == 2 ? 1 : 2) + Index * 2};
        // Three enemies use rows 2/4/6: all recipes are at least seven rows tall.
        Enemy.Range = bArcher ? 4 : 1;
        Enemy.HP = Enemy.MaxHP = (Route.bDangerous ? 12 : 10) + State.EncountersCompleted * 2;
        Enemy.ArmorClass = Route.bDangerous ? 12 : 11;
        Enemy.AttackBonus = 2;
        Enemy.DamageDie = Route.bDangerous ? 6 : 4;
        Enemy.DamageBonus = 1;
        Enemy.InitiativeBonus = 1;
        Layout.Combatants.Add(Enemy);
    }
    return Layout;
}

FCombatResult FExpeditionRun::Submit(const FCombatCommand& Command)
{
    const FCombatantState* Actor = Combat.Find(Command.ActorId);
    if (!bStarted || State.Phase != EExpeditionPhase::Combat || !Actor || Actor->Side != ECombatSide::Party)
    { return {false, TEXT("Only party commands in an active encounter are allowed.")}; }
    FCombatResult Result = Combat.Submit(Command);
    if (Result.bAccepted) { ResolveCombat(); }
    return Result;
}

TArray<FCombatResult> FExpeditionRun::PlayEnemyTurn()
{
    if (!bStarted || State.Phase != EExpeditionPhase::Combat) { return {}; }
    auto Results = Combat.PlayEnemyTurn();
    ResolveCombat();
    return Results;
}

void FExpeditionRun::ResolveCombat()
{
    const EBattlePhase Outcome = Combat.Snapshot().Phase;
    if (State.Phase != EExpeditionPhase::Combat || Outcome == EBattlePhase::Fighting) { return; }
    FExpeditionEncounterRecord& Record = State.History.Last();
    Record.Outcome = Outcome;
    Record.Survivors = 0;
    for (const auto& Unit : Combat.Snapshot().Encounter.Combatants)
    { if (Unit.Side == ECombatSide::Party && Unit.Life == ECombatLife::Conscious) { ++Record.Survivors; } }
    if (Outcome == EBattlePhase::Defeat) { State.Phase = EExpeditionPhase::Defeat; return; }
    ++State.EncountersCompleted;
    State.bArtifactRecovered = State.EncountersCompleted == 3;
    State.Phase = State.bArtifactRecovered ? EExpeditionPhase::Victory : EExpeditionPhase::Reward;
}

bool FExpeditionRun::ClaimReward(int32 RecipientId, FString& Error)
{
    Error.Reset();
    if (!bStarted || State.Phase != EExpeditionPhase::Reward)
    { Error = TEXT("Loot can be claimed once after each of the first two victories."); return false; }
    if (!FPartyContinuity::Reward(Combat, RecipientId, State.History.Last().Route.RewardPotions, Error)) { return false; }
    State.History.Last().RewardRecipient = RecipientId;
    if (State.EncountersCompleted == 1) { State.Phase = EExpeditionPhase::Recovery; }
    else { OfferRoutes(); }
    return true;
}

FString FExpeditionRun::RecoveryDescription() { return FPartyContinuity::RecoveryDescription(); }

bool FExpeditionRun::Recover(bool bTakeRecovery, FString& Error)
{
    Error.Reset();
    if (!bStarted || State.Phase != EExpeditionPhase::Recovery || State.bCampResolved)
    { Error = TEXT("Recovery is offered only once, after the first victory reward."); return false; }
    if (bTakeRecovery && !FPartyContinuity::Recover(Combat, Error)) { return false; }
    State.bCampResolved = true;
    State.bCampUsed = bTakeRecovery;
    OfferRoutes();
    return true;
}
