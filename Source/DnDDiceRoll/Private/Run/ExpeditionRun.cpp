#include "Run/ExpeditionRun.h"
#include "Party/PartyContinuity.h"

namespace
{
struct FEncounterRecipe
{
    const TCHAR* Terrain;
    TArray<FIntPoint> BlockedCells;
    int32 EnemyEdgeOffset;
    int32 FirstEnemyRow;
};

// Generated grids are about three times the original 8-9 x 7-8 area.
constexpr int32 MinWidth = 16, MaxWidth = 18, MinHeight = 11, MaxHeight = 12;
// The party forms a column here, centred vertically. Enemies stand at column
// Width - EnemyEdgeOffset, which leaves an 8..11 cell opening gap and room behind both lines.
constexpr int32 PartyColumn = 3;

// The preview and generated geometry read the same bounded recipe definition.
// Obstacles fit the smallest 16 x 11 grid. Between the lines they stay in columns 5..10;
// outside rows 3..8 they may reach any column, and column 15 is always behind the enemy
// line, so no spawn cell is ever blocked.
const FEncounterRecipe Recipes[] = {
    {TEXT("split stones"), {{6, 2}, {7, 2}, {6, 3}, {9, 6}, {9, 7}, {8, 7}, {12, 1}, {13, 1}, {2, 9}, {3, 9}, {15, 5}}, 4, 3},
    {TEXT("ruined pillars"), {{5, 2}, {5, 8}, {8, 4}, {8, 6}, {10, 2}, {10, 8}, {1, 1}, {1, 9}, {13, 0}, {14, 10}}, 5, 4},
    {TEXT("broken wall"), {{7, 1}, {7, 2}, {7, 3}, {7, 5}, {7, 6}, {7, 8}, {7, 9}, {11, 1}, {12, 1}, {13, 1}, {15, 3}, {15, 8}}, 4, 3}
};

// One profile supplies generated facts, truthful text and restore validation.
void DescribeRoute(FExpeditionRoute& Route, int32 Stage, int32 Choice, bool bSecuredSupplies)
{
    Route.Id = Stage * 2 + Choice + 1;
    Route.bDangerous = Choice == 1;
    Route.EnemyCount = Choice == 1 ? 3 : 2;
    Route.RewardPotions = Choice == 1 || bSecuredSupplies ? 2 : 1;
    Route.Name = Choice == 0 ? TEXT("Stream trail") : TEXT("Watchtower road");
    const auto& Recipe = Recipes[Route.Recipe];
    Route.Preview = FString::Printf(TEXT("%s | %d %s | %s | +1 gear, +%d potions%s"),
        Choice == 0 ? TEXT("Guarded") : TEXT("Dangerous"), Route.EnemyCount,
        Choice == 0 ? TEXT("scouts") : TEXT("raiders incl. archer"), Recipe.Terrain, Route.RewardPotions,
        Choice == 0 && bSecuredSupplies ? TEXT(" (secured supplies)") : TEXT(""));
    if (Stage == 2)
    {
        Route.RewardPotions = 0;
        Route.Name = TEXT("Artifact vault: ") + Route.Name;
        Route.Preview = FString::Printf(TEXT("%s | %d guards incl. archer | %s | recover artifact; no loot"),
            Choice == 0 ? TEXT("Guarded") : TEXT("Dangerous"), Route.EnemyCount, Recipe.Terrain);
    }
}

int32 CountSurvivors(const FCombatBattle& Battle)
{
    int32 Count = 0;
    for (const auto& Unit : Battle.Snapshot().Encounter.Combatants)
    { if (Unit.Side == ECombatSide::Party && Unit.Life == ECombatLife::Conscious) { ++Count; } }
    return Count;
}
}

bool FExpeditionRun::NewRun(int32 Seed, FString& Error, const FString& Identity)
{
    FExpeditionRun Candidate;
    Candidate.State.Seed = Seed;
    Candidate.State.RunId = Identity.IsEmpty() ? FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens) : Identity;
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
        Route.EncounterSeed = Random.RandRange(0, MAX_int32 - 1);
        Route.Recipe = Random.RandRange(0, UE_ARRAY_COUNT(Recipes) - 1);
        Route.Width = Random.RandRange(MinWidth, MaxWidth);
        Route.Height = Random.RandRange(MinHeight, MaxHeight);
        DescribeRoute(Route, State.EncountersCompleted, Index, bSecuredSupplies);
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
    Record.Survivors = CountSurvivors(Candidate.Combat);
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
    // Central obstacle clusters leave an open perimeter, spawn columns and lanes
    // around each cluster. No retry loop or seed can disconnect the open floor.
    const FEncounterRecipe& Recipe = Recipes[Route.Recipe];
    Layout.BlockedCells = Recipe.BlockedCells;
    Layout.Combatants.SetNum(4);
    const int32 FirstPartyRow = (Route.Height - 4) / 2;
    for (int32 Index = 0; Index < 4; ++Index) { Layout.Combatants[Index].Cell = {PartyColumn, FirstPartyRow + Index}; }
    for (int32 Index = 0; Index < Route.EnemyCount; ++Index)
    {
        FCombatantState Enemy;
        Enemy.Id = 20 + Index;
        Enemy.Side = ECombatSide::Enemy;
        const bool bArcher = Index == Route.EnemyCount - 1;
        Enemy.Name = bArcher ? TEXT("Vault bow") : FString::Printf(TEXT("%s %d"), Route.bDangerous ? TEXT("Raider") : TEXT("Scout"), Index + 1);
        Enemy.Role = bArcher ? TEXT("Archer") : TEXT("Raider");
        Enemy.Cell = {Route.Width - Recipe.EnemyEdgeOffset, Recipe.FirstEnemyRow + Index * 2};
        // Three enemies use rows 3/5/7 or 4/6/8; every grid is at least eleven rows tall.
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
    Record.Survivors = CountSurvivors(Combat);
    if (Outcome == EBattlePhase::Defeat) { State.Phase = EExpeditionPhase::Defeat; return; }
    ++State.EncountersCompleted;
    State.bArtifactRecovered = State.EncountersCompleted == EncounterCount;
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

bool FExpeditionRun::Restore(const FExpeditionSnapshot& Snapshot, FString& Error)
{
    Error.Reset();
    auto Reject = [&Error]() { Error = TEXT("Invalid saved expedition: phase, party, routes or history disagree."); return false; };
    const auto& P = Snapshot.Progress;
    if (P.RunId.IsEmpty() || P.RunId.Len() > 80 || static_cast<uint8>(P.Phase) > static_cast<uint8>(EExpeditionPhase::Defeat) ||
        P.EncountersCompleted < 0 || P.EncountersCompleted > 3 || P.History.Num() > 3 ||
        (P.bCampUsed && !P.bCampResolved) || P.bArtifactRecovered != (P.Phase == EExpeditionPhase::Victory)) { return Reject(); }
    FExpeditionRun Candidate;
    if (!Candidate.Combat.Restore(Snapshot.Battle, Error)) { return false; }
    Candidate.State = P;
    Candidate.bStarted = true;
    const bool bRoute = P.Phase == EExpeditionPhase::Route;
    const bool bCombat = P.Phase == EExpeditionPhase::Combat;
    const bool bDefeat = P.Phase == EExpeditionPhase::Defeat;
    const bool bReward = P.Phase == EExpeditionPhase::Reward;
    const bool bRecovery = P.Phase == EExpeditionPhase::Recovery;
    const bool bVictory = P.Phase == EExpeditionPhase::Victory;
    if ((bVictory ? P.EncountersCompleted != 3 : P.EncountersCompleted > 2) ||
        (bRecovery && P.EncountersCompleted != 1) || (bReward && P.EncountersCompleted < 1) ||
        P.History.Num() != P.EncountersCompleted + ((bCombat || bDefeat) ? 1 : 0) ||
        P.Offers.Num() != (bRoute ? 2 : 0) ||
        P.bCampResolved != (P.EncountersCompleted >= 1 && !bRecovery && !(bReward && P.EncountersCompleted == 1))) { return Reject(); }
    // Every retained route uses the same bounded recipe/preview facts as generation.
    auto ValidRoute = [&P](const FExpeditionRoute& R, int32 Stage, int32 Choice)
    {
        const bool bSupplies = Stage > 0 && P.History[Stage - 1].Route.bDangerous;
        if (R.EncounterSeed < 0 || R.EncounterSeed >= MAX_int32 || R.Recipe < 0 || R.Recipe >= UE_ARRAY_COUNT(Recipes) ||
            R.Width < MinWidth || R.Width > MaxWidth || R.Height < MinHeight || R.Height > MaxHeight) { return false; }
        auto Expected = R;
        DescribeRoute(Expected, Stage, Choice, bSupplies);
        return FExpeditionRoute::StaticStruct()->CompareScriptStruct(&R, &Expected, 0);
    };
    for (int32 I = 0; I < P.Offers.Num(); ++I) { if (!ValidRoute(P.Offers[I], P.EncountersCompleted, I)) { return Reject(); } }
    int32 PreviousSurvivors = 4;
    for (int32 I = 0; I < P.History.Num(); ++I)
    {
        const auto& H = P.History[I];
        const bool bUnfinished = I == P.History.Num() - 1 && bCombat;
        const bool bLost = I == P.History.Num() - 1 && bDefeat;
        const bool bLootPending = I == P.History.Num() - 1 && bReward;
        const bool bHasReward = I < 2 && !bUnfinished && !bLost && !bLootPending;
        if (!ValidRoute(H.Route, I, H.Route.bDangerous ? 1 : 0) ||
            H.Outcome != (bUnfinished ? EBattlePhase::Fighting : bLost ? EBattlePhase::Defeat : EBattlePhase::Victory) ||
            H.Survivors < (bLost ? 0 : 1) || H.Survivors > PreviousSurvivors || (bLost && H.Survivors != 0) ||
            (bHasReward ? H.RewardRecipient < 1 || H.RewardRecipient > 4 : H.RewardRecipient != INDEX_NONE)) { return Reject(); }
        PreviousSurvivors = H.Survivors;
    }
    const auto ExpectedPhase = bCombat || P.History.IsEmpty() ? EBattlePhase::Fighting : bDefeat ? EBattlePhase::Defeat : EBattlePhase::Victory;
    if (Snapshot.Battle.Phase != ExpectedPhase || (!P.History.IsEmpty() && !bCombat && CountSurvivors(Candidate.Combat) != P.History.Last().Survivors)) { return Reject(); }
    // Match immutable roster/recipe facts, while retaining exact live placement, fate,
    // resources, initiative and random state. No Start/NewRun is used to hydrate.
    FCombatEncounter Layout;
    if (P.History.IsEmpty()) { Layout = FCombatBattle::TutorialEncounter(); }
    else
    {
        Candidate.State.EncountersCompleted = P.History.Num() - 1;
        Layout = Candidate.Generate(P.History.Last().Route);
        Candidate.State.EncountersCompleted = P.EncountersCompleted;
    }
    const auto& Saved = Snapshot.Battle.Encounter;
    if (Saved.Width != Layout.Width || Saved.Height != Layout.Height || Saved.BlockedCells != Layout.BlockedCells || Saved.Combatants.Num() != Layout.Combatants.Num()) { return Reject(); }
    if (bCombat)
    {
        int32 Remaining = 0;
        for (const auto& U : Saved.Combatants) { if (U.Side == ECombatSide::Party && U.Life != ECombatLife::Dead) { ++Remaining; } }
        if (Remaining > P.History.Last().Survivors) { return Reject(); }
    }
    for (int32 I = 0; I < Layout.Combatants.Num(); ++I)
    {
        const auto& U = Saved.Combatants[I]; const auto& Base = Layout.Combatants[I];
        if (U.Id != Base.Id || U.Side != Base.Side || U.AdventurerRole != Base.AdventurerRole || U.Name != Base.Name || U.Role != Base.Role ||
            U.MaxHP != Base.MaxHP || U.ArmorClass != Base.ArmorClass || U.AttackBonus != Base.AttackBonus || U.DamageDie != Base.DamageDie ||
            U.DamageBonus != Base.DamageBonus || U.Range != Base.Range || U.Speed != Base.Speed || U.InitiativeBonus != Base.InitiativeBonus || U.bSneakWeapon != Base.bSneakWeapon ||
            (U.AdventurerRole != EAdventurerRole::Fighter && U.SecondWindUses != 0) ||
            (U.AdventurerRole != EAdventurerRole::Cleric && U.AdventurerRole != EAdventurerRole::Wizard && U.SpellSlots != 0) ||
            (U.Side == ECombatSide::Enemy && (U.Potions != 0 || U.WeaponRank != 0))) { return Reject(); }
        if (U.Side == ECombatSide::Party)
        {
            int32 Gear = 0, SuppliedPotions = 1;
            for (const auto& H : P.History) { if (H.RewardRecipient == U.Id) { ++Gear; SuppliedPotions += H.Route.RewardPotions; } }
            if (U.WeaponRank != Gear || U.Potions > SuppliedPotions) { return Reject(); }
        }
        if (P.History.IsEmpty() && (U.Cell != Base.Cell || U.HP != Base.HP || U.Life != Base.Life || U.Potions != Base.Potions ||
            U.WeaponRank != 0 || U.SpellSlots != Base.SpellSlots || U.SecondWindUses != Base.SecondWindUses)) { return Reject(); }
    }
    if (P.History.IsEmpty() && (Snapshot.Battle.Round != 1 || !Snapshot.Battle.bActionAvailable || !Snapshot.Battle.bBonusActionAvailable ||
        Snapshot.Battle.TurnIndex != 0 || Snapshot.Battle.MovementLeft != Candidate.Combat.Find(Candidate.Combat.ActiveId())->Speed)) { return Reject(); }
    *this = MoveTemp(Candidate);
    return true;
}
