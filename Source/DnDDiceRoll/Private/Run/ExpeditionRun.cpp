#include "Run/ExpeditionRun.h"
#include "Rules/RestRules.h"
#include "Rules/MovementRules.h"
#include "Rules/AbilityRules.h"
#include "Party/PartyContinuity.h"
#include "Party/BuildCatalog.h"
#include "Rules/ArmorRules.h"
#include "Rules/LockTrapRules.h"
#include "Rules/SocialRules.h"
#include "Rules/CoinRules.h"
#include "Rules/HazardRules.h"
#include "Rules/GroupCheckRules.h"
#include "Rules/SleightRules.h"
#include "Rules/PerformanceRules.h"
#include "Rules/InspirationRules.h"
#include "Rules/ProficiencyRules.h"

namespace
{
struct FEncounterRecipe
{
    const TCHAR* Terrain;
    TArray<FIntPoint> BlockedCells;
    int32 EnemyColumn;
    int32 FirstEnemyRow;
};

// Generated grids are about three times the original 8-9 x 7-8 area.
constexpr int32 MinWidth = 16, MaxWidth = 18, MinHeight = 11, MaxHeight = 12;
// The party forms a column here, centred vertically. Each recipe puts the enemy line at a
// fixed column 5..7 cells away, so fights open within about a turn at speed 4 and the rest
// of the board stays free for flanking and retreat on both sides.
constexpr int32 PartyColumn = 4;

// The preview and generated geometry read the same bounded recipe definition.
// Obstacles fit the smallest 16 x 11 grid and never touch a spawn: the party uses column 4,
// rows 3..7, and each enemy line uses its own column on rows 3/5/7 or 4/6/8.
const FEncounterRecipe Recipes[] = {
    {TEXT("split stones"), {{6, 2}, {7, 2}, {6, 3}, {8, 6}, {8, 7}, {7, 7}, {12, 1}, {13, 1}, {2, 9}, {3, 9}, {13, 5}, {15, 8}}, 10, 3},
    {TEXT("ruined pillars"), {{6, 2}, {6, 8}, {7, 5}, {11, 3}, {11, 7}, {13, 1}, {14, 9}, {1, 1}, {1, 9}, {2, 5}}, 9, 4},
    {TEXT("broken wall"), {{7, 1}, {7, 2}, {7, 3}, {7, 5}, {7, 6}, {7, 8}, {7, 9}, {13, 1}, {14, 1}, {15, 1}, {13, 9}, {14, 9}}, 11, 3}
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
    Route.Preview = FString::Printf(TEXT("%s | %d %s | %s | +1 gear (rank 3 cap) | +%d potions%s"),
        Choice == 0 ? TEXT("Guarded") : TEXT("Dangerous"), Route.EnemyCount,
        Choice == 0 ? TEXT("scouts") : TEXT("raiders incl. archer"), Recipe.Terrain, Route.RewardPotions,
        Choice == 0 && bSecuredSupplies ? TEXT(" (secured supplies)") : TEXT(""));
    if (Stage == FExpeditionRun::EncounterCount - 1)
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

bool FExpeditionRun::NewRun(int32 Seed, FString& Error, const FString& Identity, EFighterSubclass Subclass)
{
    if (Subclass != EFighterSubclass::Champion && Subclass != EFighterSubclass::BattleMaster) { Error = TEXT("Choose a Fighter subclass."); return false; }
    TArray<FPartyBuildSelection> Selection;
    const auto Tutorial = FCombatBattle::TutorialEncounter();
    for (int32 I = 0; I < 4; ++I)
    {
        FPartyBuildSelection Slot;
        Slot.Id = I + 1; Slot.Name = Tutorial.Combatants[I].Name; Slot.BuildId = Tutorial.Combatants[I].BuildId;
        if (I == 0) { Slot.BuildId = Subclass == EFighterSubclass::BattleMaster ? TEXT("fighter.battle-master") : TEXT("fighter.champion"); }
        Selection.Add(Slot);
    }
    return NewRun(Seed, Selection, Error, Identity);
}

bool FExpeditionRun::NewRun(int32 Seed, const TArray<FPartyBuildSelection>& Selection, FString& Error, const FString& Identity)
{
    Error.Reset();
    if (!State.Party.IsEmpty() && State.Phase != EExpeditionPhase::Victory && State.Phase != EExpeditionPhase::Defeat) { Error = TEXT("Finish this expedition before changing its party."); return false; }
    if (Selection.Num() != 4) { Error = TEXT("Choose exactly four adventurers."); return false; }
    FExpeditionRun Candidate;
    Candidate.State.Seed = Seed;
    Candidate.State.RunId = Identity.IsEmpty() ? FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens) : Identity;
    Candidate.State.GenerationRandomState = Seed;
    auto Party = FCombatBattle::TutorialEncounter();
    for (int32 I = 0; I < 4; ++I)
    {
        const auto Cell = Party.Combatants[I].Cell;
        const auto& Slot = Selection[I];
        if (Slot.Id != I + 1 || !FBuildCatalog::Create(Slot.BuildId, Slot.Id, Slot.Name, Party.Combatants[I]))
        { Error = TEXT("Choose a valid build and name for each independent slot 1–4."); return false; }
        Party.Combatants[I].Cell = Cell;
    }
    Candidate.State.Party = Selection;
    if (!Candidate.Combat.Start(Party, Seed, Error)) { return false; }
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
    if (State.bRouteHazards && Offered->bDangerous)
    {
        // #85: a keyed hazard on dangerous roads; the best-suited living hero leads the check.
        int32 Dice = static_cast<int32>(HashCombine(HashCombine(GetTypeHash(State.Seed), GetTypeHash(State.EncountersCompleted)), GetTypeHash(Offered->EncounterSeed ^ 0x4a7a)) & 0x7fffffff) | 1;
        const EHazard Hazard = static_cast<EHazard>(FDiceRules::Roll(1, 4, Dice) - 1);
        const FHazardSpec Spec = FHazardRules::Spec(Hazard);
        int32 Leader = INDEX_NONE, Best = MIN_int32;
        for (const FCombatantState& Unit : Candidate.Combat.Snapshot().Encounter.Combatants)
        {
            if (Unit.Side != ECombatSide::Party || Unit.Life != ECombatLife::Conscious) { continue; }
            const int32 Bonus = Candidate.Combat.Character(Unit.Id).SkillBonus(Spec.Skill);
            if (Bonus > Best) { Best = Bonus; Leader = Unit.Id; }
        }
        if (Leader != INDEX_NONE)
        {
            const FHazardResult Result = FHazardRules::Resolve(Hazard, Best, ERollMode::Normal, Dice);
            Record.Hazard = static_cast<int32>(Hazard);
            Record.bHazardPassed = Result.Check.bSuccess;
            for (const FCombatantState& Unit : Candidate.Combat.Snapshot().Encounter.Combatants)
            {
                if (Unit.Side != ECombatSide::Party || Unit.Life != ECombatLife::Conscious) { continue; }
                FCombatantState* Hurt = Candidate.Combat.MutableFind(Unit.Id);
                Hurt->HP = FMath::Max(1, Hurt->HP - Result.HitPointsLost);
                if (Result.SuppliesLost > 0 && Unit.Id == Leader) { Hurt->Potions = FMath::Max(0, Hurt->Potions - Result.SuppliesLost); }
            }
        }
    }
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
    for (int32 I = 0; I < 4; ++I)
    {
        if (const auto* Hero = Combat.Find(I + 1)) { Layout.Combatants[I] = *Hero; }
    }
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
        Enemy.Cell = {Recipe.EnemyColumn, Recipe.FirstEnemyRow + Index * 2};
        // Three enemies use rows 3/5/7 or 4/6/8; every grid is at least eleven rows tall.
        Enemy.Range = bArcher ? 4 : 1;
        Enemy.HP = Enemy.MaxHP = (Route.bDangerous ? 12 : 10) + State.EncountersCompleted * 2;
        Enemy.ArmorClass = Route.bDangerous ? 12 : 11;
        Enemy.AttackBonus = 2;
        Enemy.DamageDie = Route.bDangerous ? 6 : 4;
        Enemy.DamageBonus = 1;
        Enemy.InitiativeBonus = 1;
        // Equipment (schema 5): light armor whose base equals the tuned AC; the role's Dexterity bonus (+0..+2) now adds on top.
        Enemy.ArmorWorn = static_cast<int32>(Route.bDangerous ? EArmor::StuddedLeather : EArmor::Leather);
        // A quiet route catches its first scout resting: prone until it spends half its movement to stand.
        if (!Route.bDangerous && Index == 0 && !bArcher)
        {
            FCombatCondition Resting;
            Resting.Condition = ECombatCondition::Prone;
            Enemy.Conditions.Add(Resting);
        }
        Layout.Combatants.Add(Enemy);
    }
    // Rubble: the open cell east of each obstacle is difficult terrain. It only slows, so the floor stays connected.
    for (const FIntPoint& Wall : Layout.BlockedCells)
    {
        const FIntPoint Rubble(Wall.X + 1, Wall.Y);
        if (Rubble.X >= Layout.Width || Layout.BlockedCells.Contains(Rubble) || Layout.DifficultCells.Contains(Rubble) ||
            Layout.Combatants.ContainsByPredicate([Rubble](const FCombatantState& U) { return U.Cell == Rubble; })) { continue; }
        Layout.DifficultCells.Add(Rubble);
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
    if (State.EncountersCompleted < EncounterCount)
    {
        for (auto& Unit : Combat.State.Encounter.Combatants)
        {
            if (Unit.Side == ECombatSide::Party && Unit.Life == ECombatLife::Conscious) { ++Unit.Level; Unit.DamageBonus = FBuildCatalog::BasicDamage(*FBuildCatalog::Find(Unit.BuildId), Unit.Level); }
        }
    }
    State.bArtifactRecovered = State.EncountersCompleted == EncounterCount;
    State.Coins += VictoryPurseCopper;
    State.Phase = State.bArtifactRecovered ? EExpeditionPhase::Victory : EExpeditionPhase::Reward;
}

bool FExpeditionRun::ClaimReward(int32 RecipientId, FString& Error)
{
    Error.Reset();
    if (!bStarted || State.Phase != EExpeditionPhase::Reward)
    { Error = TEXT("Loot can be claimed once after each of the first four victories."); return false; }
    if (!FPartyContinuity::Reward(Combat, RecipientId, State.History.Last().Route.RewardPotions + State.History.Last().BonusPotions, Error)) { return false; }
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

bool FExpeditionRun::ShortRest(int32 UnitId, int32& Healed, FString& Error)
{
    Error.Reset();
    Healed = 0;
    if (!bStarted || State.Phase != EExpeditionPhase::Reward) { Error = TEXT("Short rests happen at the rest point after a victory."); return false; }
    FCombatantState* Hero = Combat.MutableFind(UnitId);
    if (!Hero || Hero->Side != ECombatSide::Party || Hero->Life != ECombatLife::Conscious) { Error = TEXT("Only a living survivor can take a short rest."); return false; }
    if (Hero->HitDiceSpent >= Hero->Level) { Error = FString::Printf(TEXT("%s has no hit dice left until camp."), *Hero->Name); return false; }
    if (Hero->HP >= Hero->MaxHP) { Error = FString::Printf(TEXT("%s is already at full health."), *Hero->Name); return false; }
    // A keyed roll: resting never shifts the run's generation stream.
    int32 Dice = static_cast<int32>(HashCombine(HashCombine(GetTypeHash(State.Seed), GetTypeHash(State.EncountersCompleted)), HashCombine(GetTypeHash(UnitId), GetTypeHash(Hero->HitDiceSpent))) & 0x7fffffff) | 1;
    const int32 Roll = FDiceRules::Roll(1, FRestRules::HitDieSides(Hero->AdventurerRole), Dice);
    Healed = FMath::Min(Hero->MaxHP - Hero->HP, FRestRules::HitDieHealing(Roll, Combat.AbilityModifier(UnitId, EAbility::Constitution)));
    Hero->HP += Healed;
    ++Hero->HitDiceSpent;
    return true;
}

int32 FExpeditionRun::CampD20(int32 UnitId, ECampActivity Activity) const
{
    int32 Dice = static_cast<int32>(HashCombine(HashCombine(GetTypeHash(State.Seed), GetTypeHash(State.EncountersCompleted)), HashCombine(GetTypeHash(UnitId), GetTypeHash(1000 + static_cast<int32>(Activity)))) & 0x7fffffff) | 1;
    return FDiceRules::Roll(1, 20, Dice);
}

FString FExpeditionRun::CampActivityName(ECampActivity Activity)
{
    switch (Activity)
    {
    case ECampActivity::PickLock: return TEXT("Pick the strongbox");
    case ECampActivity::Parley: return TEXT("Parley with traders");
    case ECampActivity::BuyPotion: return TEXT("Buy a potion");
    case ECampActivity::Forage: return TEXT("Forage together");
    case ECampActivity::Pickpocket: return TEXT("Lift a purse");
    case ECampActivity::Perform: return TEXT("Perform for coin");
    default: return TEXT("Unknown");
    }
}

bool FExpeditionRun::CampActivity(int32 UnitId, ECampActivity Activity, FString& Outcome, FString& Error)
{
    Error.Reset();
    Outcome.Reset();
    if (!bStarted || State.Phase != EExpeditionPhase::Reward || State.History.IsEmpty()) { Error = TEXT("Camp activities happen at the rest point after a victory."); return false; }
    if (static_cast<uint8>(Activity) >= static_cast<uint8>(ECampActivity::Count)) { Error = TEXT("Unknown camp activity."); return false; }
    FExpeditionEncounterRecord& Record = State.History.Last();
    const int32 Bit = 1 << static_cast<int32>(Activity);
    if (Record.ActivitiesUsed & Bit) { Error = FString::Printf(TEXT("%s: already tried at this rest point."), *CampActivityName(Activity)); return false; }
    const FCombatantState* Hero = Combat.Find(UnitId);
    if (!Hero || Hero->Side != ECombatSide::Party || Hero->Life != ECombatLife::Conscious) { Error = TEXT("Only a living survivor can do that."); return false; }
    const FCharacterRules Sheet = Combat.Character(UnitId);
    const int32 D20 = CampD20(UnitId, Activity);
    switch (Activity)
    {
    case ECampActivity::PickLock:
    {
        // #81: a strongbox with a good lock; thieves' tools (Rogues) are required.
        const bool bTools = Hero->AdventurerRole == EAdventurerRole::Rogue;
        if (!FLockTrapRules::CanPickLock(bTools)) { Error = TEXT("Picking the strongbox needs thieves' tools (a Rogue)."); return false; }
        const int32 Total = D20 + Sheet.ToolBonus(ETool::ThievesTools, EAbility::Dexterity);
        const bool bOpen = FLockTrapRules::Picks(Total, ELockTier::Good, bTools);
        if (bOpen) { ++Record.BonusPotions; }
        Outcome = FString::Printf(TEXT("%s %s the strongbox (%d vs DC %d)%s"), *Hero->Name, bOpen ? TEXT("picks") : TEXT("fails to pick"), Total,
            FLockTrapRules::LockDc(ELockTier::Good), bOpen ? TEXT(": +1 healing potion in the loot.") : TEXT("."));
        break;
    }
    case ECampActivity::Parley:
    {
        // #82: persuade wary (indifferent) traders; winning them over (Indifferent -> Friendly, the rules' Information outcome) earns a gift potion.
        const int32 Total = D20 + Sheet.SkillBonus(ESkill::Persuasion);
        const ENpcAttitude After = FSocialRules::AfterInfluence(ENpcAttitude::Indifferent, Total);
        const ESocialOutcome Result = FSocialRules::OutcomeFor(ENpcAttitude::Indifferent, After);
        const bool bWon = After == ENpcAttitude::Friendly;
        if (bWon) { ++Record.BonusPotions; }
        Outcome = FString::Printf(TEXT("%s parleys with traders (%d vs DC %d): %s"), *Hero->Name, Total, FSocialRules::InfluenceDc(ENpcAttitude::Indifferent),
            bWon ? TEXT("won over, they share road news and gift a healing potion.") : Result == ESocialOutcome::None ? TEXT("they shrug and move on.") : TEXT("they part peacefully."));
        break;
    }
    case ECampActivity::BuyPotion:
    {
        // #84: haggle (Persuasion) for a healing potion paid from the party purse.
        const int32 Total = D20 + Sheet.SkillBonus(ESkill::Persuasion);
        const int32 Price = FCoinRules::HaggledPrice(FCoinRules::Price(EShopItem::PotionOfHealing), Total);
        if (State.Coins < Price) { Error = FString::Printf(TEXT("A potion costs %s after haggling; the purse holds %s."), *FCoinRules::Format(Price), *FCoinRules::Format(State.Coins)); return false; }
        State.Coins -= Price;
        ++Record.BonusPotions;
        Outcome = FString::Printf(TEXT("%s haggles (%d) and buys a healing potion for %s."), *Hero->Name, Total, *FCoinRules::Format(Price));
        break;
    }
    case ECampActivity::Forage:
    {
        // #86: every living survivor rolls Survival (DC 12); half or more succeeding finds healing herbs.
        TArray<FGroupMember> Group;
        for (const FCombatantState& Unit : Combat.Snapshot().Encounter.Combatants)
        {
            if (Unit.Side != ECombatSide::Party || Unit.Life != ECombatLife::Conscious) { continue; }
            FGroupMember Member;
            Member.Modifier = Combat.Character(Unit.Id).SkillBonus(ESkill::Survival);
            Group.Add(Member);
        }
        int32 Dice = static_cast<int32>(HashCombine(HashCombine(GetTypeHash(State.Seed), GetTypeHash(State.EncountersCompleted)), GetTypeHash(2000 + static_cast<int32>(Activity))) & 0x7fffffff) | 1;
        const FGroupCheckOutcome Result = FGroupCheckRules::Resolve(Group, 12, Dice);
        if (Result.bSuccess) { ++Record.BonusPotions; }
        Outcome = FString::Printf(TEXT("The party forages (%d of %d pass DC 12): %s"), Result.Successes, Group.Num(), Result.bSuccess ? TEXT("healing herbs, +1 potion in the loot.") : TEXT("nothing useful."));
        break;
    }
    case ECampActivity::Pickpocket:
    {
        // #90: Sleight of Hand against a passing trader's passive Perception 12; caught means paying a fine.
        int32 Dice = static_cast<int32>(HashCombine(HashCombine(GetTypeHash(State.Seed), GetTypeHash(State.EncountersCompleted)), HashCombine(GetTypeHash(UnitId), GetTypeHash(3000 + static_cast<int32>(Activity)))) & 0x7fffffff) | 1;
        const FSleightOutcome Lift = FSleightRules::Resolve(Sheet.SkillBonus(ESkill::SleightOfHand), ERollMode::Normal, 12, Dice);
        const int32 Gain = Lift.Result == ESleightResult::Unnoticed ? PickpocketCopper : Lift.Result == ESleightResult::Caught ? -FMath::Min(State.Coins, PickpocketCopper / 2) : 0;
        State.Coins += Gain;
        Outcome = FString::Printf(TEXT("%s tries to lift a purse (%d vs passive 12): %s"), *Hero->Name, Lift.Check.Total,
            Lift.Result == ESleightResult::Unnoticed ? *FString::Printf(TEXT("unnoticed, +%s."), *FCoinRules::Format(Gain)) :
            Lift.Result == ESleightResult::Caught ? *FString::Printf(TEXT("caught, fined %s."), *FCoinRules::Format(-Gain)) : TEXT("fumbles but slips away."));
        break;
    }
    case ECampActivity::Perform:
    {
        // #91: Performance DC 12 for coin (double on beating it by 10); a standout show also grants heroic inspiration.
        int32 Dice = static_cast<int32>(HashCombine(HashCombine(GetTypeHash(State.Seed), GetTypeHash(State.EncountersCompleted)), HashCombine(GetTypeHash(UnitId), GetTypeHash(3000 + static_cast<int32>(Activity)))) & 0x7fffffff) | 1;
        const FPerformanceOutcome Show = FPerformanceRules::Resolve(Sheet.SkillBonus(ESkill::Performance), ERollMode::Normal, 12, PerformanceCopper, Dice);
        State.Coins += Show.Coins;
        const bool bStandout = Show.Check.Total >= 22;
        if (bStandout) { FInspirationRules::Grant(Combat.MutableFind(UnitId)->bInspired); }
        Outcome = FString::Printf(TEXT("%s performs (%d vs DC 12): %s%s"), *Hero->Name, Show.Check.Total,
            Show.Coins > 0 ? *FString::Printf(TEXT("+%s"), *FCoinRules::Format(Show.Coins)) : TEXT("the crowd drifts away"), bStandout ? TEXT(" and heroic inspiration.") : TEXT("."));
        break;
    }
    default: break;
    }
    Record.ActivitiesUsed |= Bit;
    return true;
}

bool FExpeditionRun::SetRouteHazards(bool bEnabled, FString& Error)
{
    Error.Reset();
    if (!bStarted || State.Phase != EExpeditionPhase::Route) { Error = TEXT("Route hazards can be toggled while choosing a route."); return false; }
    State.bRouteHazards = bEnabled;
    return true;
}

FString FExpeditionRun::HazardDescription(const FExpeditionEncounterRecord& Record)
{
    if (Record.Hazard < 0) { return FString(); }
    static const TCHAR* Names[] = { TEXT("A crumbling cliff"), TEXT("A narrow ledge"), TEXT("The trail is lost"), TEXT("A hidden passage") };
    static const TCHAR* Passed[] = { TEXT("climbed safely"), TEXT("crossed safely"), TEXT("found the way"), TEXT("found a shortcut") };
    static const TCHAR* Failed[] = { TEXT("everyone takes 4 damage"), TEXT("everyone takes 3 damage"), TEXT("the guide drops a potion"), TEXT("missed it") };
    const int32 Index = FMath::Clamp(Record.Hazard, 0, 3);
    return FString::Printf(TEXT("%s: %s."), Names[Index], Record.bHazardPassed ? Passed[Index] : Failed[Index]);
}

bool FExpeditionRun::Restore(const FExpeditionSnapshot& Snapshot, FString& Error)
{
    Error.Reset();
    auto Reject = [&Error]() { Error = TEXT("Invalid saved expedition: phase, party, routes or history disagree."); return false; };
    const auto& P = Snapshot.Progress;
    if (P.Party.Num() != 4 || P.CatalogRevision != FBuildCatalog::Revision || P.RunId.IsEmpty() || P.RunId.Len() > 80 || static_cast<uint8>(P.Phase) > static_cast<uint8>(EExpeditionPhase::Defeat) ||
        P.EncountersCompleted < 0 || P.EncountersCompleted > EncounterCount || P.History.Num() > EncounterCount ||
        (P.bCampUsed && !P.bCampResolved) || P.bArtifactRecovered != (P.Phase == EExpeditionPhase::Victory)) { return Reject(); }
    for (const auto& Record : P.History)
    {
        if (P.Coins < 0 || Record.Hazard < -1 || Record.Hazard > static_cast<int32>(EHazard::HiddenPassage) || (Record.Hazard >= 0 && !Record.Route.bDangerous) ||
            (Record.Hazard < 0 && Record.bHazardPassed) || Record.BonusPotions < 0 || Record.BonusPotions > static_cast<int32>(ECampActivity::Count) || Record.ActivitiesUsed < 0 ||
            Record.ActivitiesUsed >= (1 << static_cast<int32>(ECampActivity::Count))) { return Reject(); }
    }
    for (int32 I = 0; I < 4; ++I)
    {
        const auto& Selection = P.Party[I];
        if (Selection.Id != I + 1 || Selection.Name.IsEmpty() || Selection.Name.Len() > 80 || !FBuildCatalog::Find(Selection.BuildId)) { return Reject(); }
    }
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
    if ((bVictory ? P.EncountersCompleted != EncounterCount : P.EncountersCompleted >= EncounterCount) ||
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
    TArray<int32> DeathsByLevel;
    DeathsByLevel.Init(0, EncounterCount + 1);
    for (int32 I = 0; I < P.History.Num(); ++I)
    {
        const auto& H = P.History[I];
        const bool bUnfinished = I == P.History.Num() - 1 && bCombat;
        const bool bLost = I == P.History.Num() - 1 && bDefeat;
        const bool bLootPending = I == P.History.Num() - 1 && bReward;
        const bool bHasReward = I < EncounterCount - 1 && !bUnfinished && !bLost && !bLootPending;
        if (!ValidRoute(H.Route, I, H.Route.bDangerous ? 1 : 0) ||
            H.Outcome != (bUnfinished ? EBattlePhase::Fighting : bLost ? EBattlePhase::Defeat : EBattlePhase::Victory) ||
            H.Survivors < (bLost ? 0 : 1) || H.Survivors > PreviousSurvivors || (bLost && H.Survivors != 0) ||
            (bUnfinished && H.Survivors != PreviousSurvivors) ||
            (bHasReward ? H.RewardRecipient < 1 || H.RewardRecipient > 4 : H.RewardRecipient != INDEX_NONE)) { return Reject(); }
        if (!bUnfinished) { DeathsByLevel[I + 1] = PreviousSurvivors - H.Survivors; }
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
        DeathsByLevel[P.History.Num()] = P.History.Last().Survivors - Remaining;
    }
    for (int32 I = 0; I < Layout.Combatants.Num(); ++I)
    {
        const auto& U = Saved.Combatants[I]; auto Base = Layout.Combatants[I];
        if (U.Side != (I < 4 ? ECombatSide::Party : ECombatSide::Enemy)) { return Reject(); }
        if (U.Side == ECombatSide::Party)
        {
            const auto* Build = FBuildCatalog::Find(U.BuildId);
            if (!Build || !P.Party.IsValidIndex(I) || U.Id != P.Party[I].Id || U.Name != P.Party[I].Name || U.BuildId != P.Party[I].BuildId || U.ClassId != Build->Profile.ClassId || U.SubclassId != Build->Profile.SubclassId ||
                U.FighterSubclass != Build->Profile.FighterSubclass || !FBuildCatalog::ValidResources(U)) { return Reject(); }
            const auto Spawn = Base.Cell;
            Base = Build->Profile; Base.Id = U.Id; Base.Name = U.Name; Base.Cell = Spawn;
            // Names are instance values; detached combat validation bounds them.
            const int32 ExpectedLevel = FMath::Min(5, P.EncountersCompleted + 1);
            int32 MinimumLevel = 1;
            for (int32 Stage = 0; Stage < P.History.Num(); ++Stage)
            {
                const auto& Record = P.History[Stage];
                // Four survivors prove everyone reached this stage. A named loot
                // recipient also proves that adventurer earned its victory level.
                if (Record.Survivors == 4 || Record.RewardRecipient == U.Id)
                {
                    const int32 EarnedLevel = Stage + 1 + (Record.Outcome == EBattlePhase::Victory ? 1 : 0);
                    MinimumLevel = FMath::Max(MinimumLevel, FMath::Min(5, EarnedLevel));
                }
            }
            if (U.Level < MinimumLevel || U.Level > ExpectedLevel || (U.Life == ECombatLife::Dead && U.Level > FMath::Max(1, P.EncountersCompleted + ((bCombat || bDefeat) ? 1 : 0))) || (U.Life != ECombatLife::Dead && U.Level != ExpectedLevel)) { return Reject(); }
            // Each permanent loss belongs to the level of the encounter where it
            // occurred; matching the distribution also bounds unidentified deaths.
            if (U.Life == ECombatLife::Dead && --DeathsByLevel[U.Level] < 0) { return Reject(); }
            Base.DamageBonus = FBuildCatalog::BasicDamage(*Build, U.Level);
        }
        if (U.Id != Base.Id || U.Side != Base.Side || U.AdventurerRole != Base.AdventurerRole || U.Name != Base.Name || U.Role != Base.Role ||
            U.MaxHP != Base.MaxHP || U.ArmorClass != Base.ArmorClass || U.AttackBonus != Base.AttackBonus || U.DamageDie != Base.DamageDie ||
            U.DamageBonus != Base.DamageBonus || U.Range != Base.Range || U.Speed != Base.Speed || U.InitiativeBonus != Base.InitiativeBonus || U.bSneakWeapon != Base.bSneakWeapon ||
            (U.AdventurerRole != EAdventurerRole::Fighter && U.SecondWindUses != 0) ||
            (U.AdventurerRole != EAdventurerRole::Cleric && U.AdventurerRole != EAdventurerRole::Wizard && U.SpellSlots != 0) ||
            (U.Side == ECombatSide::Enemy && (!U.BuildId.IsEmpty() || !U.ClassId.IsEmpty() || !U.SubclassId.IsEmpty() || U.Level != 1 || U.FighterSubclass != EFighterSubclass::None || U.Potions != 0 || U.WeaponRank != 0))) { return Reject(); }
        if (U.Side == ECombatSide::Party)
        {
            int32 Gear = 0, SuppliedPotions = 1;
            for (const auto& H : P.History) { if (H.RewardRecipient == U.Id) { ++Gear; SuppliedPotions += H.Route.RewardPotions + H.BonusPotions; } }
            if (U.WeaponRank != FMath::Min(3, Gear) || U.Potions > SuppliedPotions) { return Reject(); }
        }
        if (P.History.IsEmpty() && (U.Cell != Base.Cell || U.HP != Base.HP || U.Life != Base.Life || U.Potions != Base.Potions ||
            U.KitUses != Base.KitUses || U.GuardTurns != 0 || U.GuardAmount != 0 || U.RageUses != Base.RageUses || U.RageTurns != 0 || U.GuardTargetId != INDEX_NONE || U.LastSurge != 0 ||
            U.ManeuverUses != (U.FighterSubclass == EFighterSubclass::BattleMaster ? 2 : 0) || U.WeaponRank != 0 || U.SpellSlots != Base.SpellSlots || U.SecondWindUses != Base.SecondWindUses)) { return Reject(); }
    }
    for (int32 Deaths : DeathsByLevel) { if (Deaths != 0) { return Reject(); } }
    if (P.History.IsEmpty() && (Snapshot.Battle.Round != 1 || !Snapshot.Battle.bActionAvailable || !Snapshot.Battle.bBonusActionAvailable ||
        Snapshot.Battle.TurnIndex != 0 || Snapshot.Battle.MovementLeft != Candidate.Combat.Find(Candidate.Combat.ActiveId())->Speed)) { return Reject(); }
    *this = MoveTemp(Candidate);
    return true;
}
