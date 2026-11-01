#include "Combat/CombatBattle.h"
#include "Party/BuildCatalog.h"

namespace
{
int32 GridDistance(FIntPoint A, FIntPoint B)
{
    return FMath::Abs(A.X - B.X) + FMath::Abs(A.Y - B.Y);
}
using namespace BuildRules;
constexpr int32 PotionDice = 2;
constexpr int32 PotionDie = 4;
constexpr int32 PotionBonus = 2;
int32 CriticalThreshold(const FCombatantState& Hero)
{
    if (const auto* B = FBuildCatalog::Find(Hero.BuildId)) { return B->CriticalThreshold; }
    return Hero.FighterSubclass == EFighterSubclass::Champion ? 19 : 20;
}
const FIntPoint Neighbors[] = {{1, 0}, {0, 1}, {-1, 0}, {0, -1}};
}

bool FCombatBattle::ValidateEncounter(const FCombatEncounter& Encounter, FString& Error)
{
    Error.Reset();
    auto Reject = [&Error](const TCHAR* Reason) { Error = Reason; return false; };
    if (Encounter.Width < 2 || Encounter.Height < 2 || Encounter.Width > 24 || Encounter.Height > 24 ||
        Encounter.Combatants.Num() < 2 || Encounter.Combatants.Num() > 32)
    {
        return Reject(TEXT("Encounter must have a 2..24 grid and 2..32 combatants."));
    }
    auto Inside = [&Encounter](FIntPoint Cell)
    {
        return Cell.X >= 0 && Cell.Y >= 0 && Cell.X < Encounter.Width && Cell.Y < Encounter.Height;
    };
    TSet<FIntPoint> Occupied;
    for (FIntPoint Cell : Encounter.BlockedCells)
    {
        if (!Inside(Cell) || Occupied.Contains(Cell)) { return Reject(TEXT("Invalid blocked cell.")); }
        Occupied.Add(Cell);
    }
    TSet<int32> Ids;
    for (const FCombatantState& Unit : Encounter.Combatants)
    {
        if (Unit.Id < 0 || Ids.Contains(Unit.Id) || !Inside(Unit.Cell) || Occupied.Contains(Unit.Cell) ||
            Unit.Level < 1 || Unit.Level > 5 || Unit.ManeuverUses < 0 || Unit.ManeuverUses > 2 ||
            static_cast<uint8>(Unit.FighterSubclass) > static_cast<uint8>(EFighterSubclass::BattleMaster) ||
            (Unit.FighterSubclass != EFighterSubclass::BattleMaster && Unit.ManeuverUses != 0) ||
            (Unit.AdventurerRole != EAdventurerRole::Fighter && Unit.FighterSubclass != EFighterSubclass::None) ||
            Unit.MaxHP < 1 || Unit.MaxHP > 1000 || Unit.HP < 0 || Unit.HP > Unit.MaxHP ||
            Unit.ArmorClass < 1 || Unit.ArmorClass > 40 || Unit.AttackBonus < -10 || Unit.AttackBonus > 30 ||
            Unit.DamageDie < 1 || Unit.DamageDie > 100 || Unit.DamageBonus < 0 || Unit.DamageBonus > 100 ||
            Unit.Range < 1 || Unit.Range > 32 || Unit.Speed < 0 || Unit.Speed > 32 ||
            Unit.InitiativeBonus < -10 || Unit.InitiativeBonus > 30 ||
            static_cast<uint8>(Unit.AdventurerRole) > static_cast<uint8>(EAdventurerRole::Barbarian) ||
            static_cast<uint8>(Unit.Life) > static_cast<uint8>(ECombatLife::Dead) ||
            (Unit.Life == ECombatLife::Conscious ? Unit.HP <= 0 || Unit.RescueTurnsLeft != 0 : Unit.HP != 0) ||
            (Unit.Life == ECombatLife::Downed && (Unit.Side != ECombatSide::Party || Unit.RescueTurnsLeft < 1 || Unit.RescueTurnsLeft > 3)) ||
            (Unit.Life == ECombatLife::Dead && Unit.RescueTurnsLeft != 0) ||
            Unit.SecondWindUses < 0 || Unit.SecondWindUses > 2 || Unit.SpellSlots < 0 || Unit.SpellSlots > 2 ||
            Unit.Potions < 0 || Unit.Potions > 9 || Unit.WeaponRank < 0 || Unit.WeaponRank > 3 ||
            (Unit.Side != ECombatSide::Party && Unit.Side != ECombatSide::Enemy))
        {
            return Reject(TEXT("Invalid combatant stats, ID, or occupied spawn."));
        }
        const auto* Build = FBuildCatalog::Find(Unit.BuildId);
        const bool bBarbarian = Unit.AdventurerRole == EAdventurerRole::Barbarian && Unit.Side == ECombatSide::Party && Build && Build->Profile.AdventurerRole == EAdventurerRole::Barbarian;
        if (Unit.AdventurerRole == EAdventurerRole::Barbarian && !bBarbarian) { return Reject(TEXT("Barbarian requires a compatible build.")); }
        if (Unit.RageUses < 0 || Unit.RageUses > (bBarbarian ? Build->RageCap : 0) || Unit.RageTurns < 0 || Unit.RageTurns > (bBarbarian ? 2 : 0) ||
            Unit.LastSurge < 0 || Unit.LastSurge > (bBarbarian && Unit.RageUses < 2 && Unit.BuildId == TEXT("barbarian.wild-magic") ? 2 : 0) ||
            (Unit.RageTurns > 0 && (Unit.RageUses == Build->RageCap || Unit.Life != ECombatLife::Conscious)) ||
            (Unit.LastSurge > 0 && Unit.Life != ECombatLife::Conscious) ||
            (Unit.GuardTargetId != INDEX_NONE && (Unit.BuildId != TEXT("barbarian.totem-warrior") || Unit.RageTurns == 0)))
        { return Reject(TEXT("Invalid owned ability state.")); }
        if (Unit.GuardTargetId != INDEX_NONE)
        {
            const auto* Target = Encounter.Combatants.FindByPredicate([&](const auto& T) { return T.Id == Unit.GuardTargetId; });
            if (!Target || Target->Id == Unit.Id || Target->Side != Unit.Side || Target->Life != ECombatLife::Conscious) { return Reject(TEXT("Invalid protected ally.")); }
        }
        Ids.Add(Unit.Id);
        Occupied.Add(Unit.Cell);
    }
    return true;
}

bool FCombatBattle::Start(const FCombatEncounter& Encounter, int32 Seed, FString& Error)
{
    if (!ValidateEncounter(Encounter, Error)) { return false; }
    const bool bParty = Encounter.Combatants.ContainsByPredicate([](const auto& U) { return U.Side == ECombatSide::Party && U.Life == ECombatLife::Conscious; });
    const bool bEnemy = Encounter.Combatants.ContainsByPredicate([](const auto& U) { return U.Side == ECombatSide::Enemy && U.Life == ECombatLife::Conscious; });
    if (!bParty || !bEnemy) { Error = TEXT("Both sides need an active combatant."); return false; }
    State = FCombatSnapshot();
    State.Encounter = Encounter;
    State.RandomState = Seed;
    bStarted = true;
    for (FCombatantState& Unit : State.Encounter.Combatants)
    {
        Unit.Initiative = Roll(20) + Unit.InitiativeBonus;
        if (Unit.Life != ECombatLife::Dead) { State.TurnOrder.Add(Unit.Id); }
    }
    State.TurnOrder.Sort([this](int32 A, int32 B)
    {
        const int32 IA = Find(A)->Initiative;
        const int32 IB = Find(B)->Initiative;
        return IA == IB ? A < B : IA > IB;
    });
    if (FCombatantState* First = MutableFind(ActiveId()); First->Life == ECombatLife::Downed)
    {
        if (--First->RescueTurnsLeft == 0) { First->Life = ECombatLife::Dead; }
        AdvanceTurn();
    }
    State.MovementLeft = Find(ActiveId())->Speed;
    return true;
}

const FCombatantState* FCombatBattle::Find(int32 Id) const
{
    return State.Encounter.Combatants.FindByPredicate([Id](const FCombatantState& Unit) { return Unit.Id == Id; });
}

int32 FCombatBattle::ActiveId() const
{
    return bStarted && State.TurnOrder.IsValidIndex(State.TurnIndex) ? State.TurnOrder[State.TurnIndex] : INDEX_NONE;
}

int32 FCombatBattle::Roll(int32 Sides)
{
    FRandomStream Random(State.RandomState);
    const int32 Value = Random.RandRange(1, Sides);
    State.RandomState = Random.GetCurrentSeed();
    if (DiceLog) { DiceLog->Add({Sides, Value}); }
    return Value;
}

int32 FCombatBattle::RollDice(int32 Count, int32 Sides)
{
    int32 Total = 0;
    for (int32 Index = 0; Index < Count; ++Index) { Total += Roll(Sides); }
    return Total;
}

FString FCombatBattle::DescribeAction(ECombatCommandType Action)
{
    switch (Action)
    {
    case ECombatCommandType::PrecisionStrike:
        return TEXT("PRECISION STRIKE | Action + 1 use | attack with +2 hit and +2 damage; no camp refill");
    case ECombatCommandType::SecondWind:
        return FString::Printf(TEXT("SECOND WIND | Bonus + 1 use | self | d%d+%d healing | target self"), WindDie, FighterLevel);
    case ECombatCommandType::CureWounds:
        return FString::Printf(TEXT("CURE WOUNDS | Action + 1 slot | ally/self, %d cell | %dd%d+%d healing"), TouchRange, CureDice, CureDie, HealingModifier);
    case ECombatCommandType::MagicMissile:
        return FString::Printf(TEXT("MAGIC MISSILE | Action + 1 slot | enemy, %d cells + sight | %dd%d+%d auto-hit"), MissileRange, MissileDarts, MissileDie, MissileDarts * DartBonus);
    case ECombatCommandType::Rescue:
        return FString::Printf(TEXT("RESCUE | Action, no item | downed ally, %d cell | restore 1 HP"), TouchRange);
    case ECombatCommandType::Potion:
        return FString::Printf(TEXT("POTION | Action + 1 bottle | ally/self, %d cell | %dd%d+%d healing"), TouchRange, PotionDice, PotionDie, PotionBonus);
    default:
        return FString::Printf(TEXT("ATTACK / MOVE | Attack costs Action | Rogue: +d%d with conscious ally by target; Wizard: Fire Bolt"), SneakDie);
    }
}

bool FCombatBattle::IsInside(FIntPoint Cell) const
{
    return Cell.X >= 0 && Cell.Y >= 0 && Cell.X < State.Encounter.Width && Cell.Y < State.Encounter.Height;
}

bool FCombatBattle::IsOpen(FIntPoint Cell, int32 MovingId) const
{
    return IsInside(Cell) && !State.Encounter.BlockedCells.Contains(Cell) &&
        !State.Encounter.Combatants.ContainsByPredicate([Cell, MovingId](const FCombatantState& Unit)
        {
            // Downed and dead bodies occupy their cells until the encounter transition.
            return Unit.Id != MovingId && Unit.Cell == Cell;
        });
}

TMap<FIntPoint, int32> FCombatBattle::Distances(int32 Id, TMap<FIntPoint, FIntPoint>* Parents) const
{
    TMap<FIntPoint, int32> Result;
    const FCombatantState* Unit = Find(Id);
    if (!Unit) { return Result; }
    TArray<FIntPoint> Queue = {Unit->Cell};
    Result.Add(Unit->Cell, 0);
    for (int32 Index = 0; Index < Queue.Num(); ++Index)
    {
        const FIntPoint Cell = Queue[Index];
        const int32 Cost = Result[Cell] + 1;
        for (FIntPoint Offset : Neighbors)
        {
            const FIntPoint Next = Cell + Offset;
            if (IsOpen(Next, Id) && !Result.Contains(Next))
            {
                Result.Add(Next, Cost);
                if (Parents) { Parents->Add(Next, Cell); }
                Queue.Add(Next);
            }
        }
    }
    return Result;
}

TArray<FIntPoint> FCombatBattle::LegalMoves(int32 Id) const
{
    TArray<FIntPoint> Result;
    if (!bStarted || State.Phase != EBattlePhase::Fighting || ActiveId() != Id) { return Result; }
    for (const auto& Entry : Distances(Id))
    {
        if (Entry.Value > 0 && Entry.Value <= State.MovementLeft) { Result.Add(Entry.Key); }
    }
    return Result;
}

bool FCombatBattle::HasLineOfSight(FIntPoint From, FIntPoint To) const
{
    // Supercover grid traversal. At a corner both touching cells must be clear.
    const FIntPoint Delta = To - From;
    const int32 NX = FMath::Abs(Delta.X);
    const int32 NY = FMath::Abs(Delta.Y);
    const int32 SX = FMath::Sign(Delta.X);
    const int32 SY = FMath::Sign(Delta.Y);
    int32 IX = 0;
    int32 IY = 0;
    auto Blocked = [this](FIntPoint Cell) { return State.Encounter.BlockedCells.Contains(Cell); };
    while (IX < NX || IY < NY)
    {
        const int32 Decision = (1 + 2 * IX) * NY - (1 + 2 * IY) * NX;
        if (Decision == 0)
        {
            if (Blocked(From + FIntPoint(SX, 0)) || Blocked(From + FIntPoint(0, SY))) { return false; }
            From += FIntPoint(SX, SY);
            ++IX; ++IY;
        }
        else if (Decision < 0) { From.X += SX; ++IX; }
        else { From.Y += SY; ++IY; }
        if (Blocked(From)) { return false; }
    }
    return true;
}

bool FCombatBattle::CanTargetFrom(const FCombatantState& Attacker, FIntPoint Origin, const FCombatantState& Target) const
{
    return Target.Side != Attacker.Side && Target.HP > 0 && GridDistance(Origin, Target.Cell) <= Attacker.Range &&
        HasLineOfSight(Origin, Target.Cell);
}

TArray<int32> FCombatBattle::LegalTargets(int32 Id, ECombatCommandType Action) const
{
    TArray<int32> Result;
    const FCombatantState* Unit = Find(Id);
    if (!bStarted || State.Phase != EBattlePhase::Fighting || ActiveId() != Id || !Unit || Unit->Life != ECombatLife::Conscious) { return Result; }
    if (Action == ECombatCommandType::Special1 || Action == ECombatCommandType::Special2)
    {
        for (const auto& A : FBuildCatalog::Abilities(*Unit))
        {
            if (A.Command != Action || !A.bResourceAvailable || !(A.bBonus ? State.bBonusActionAvailable : State.bActionAvailable)) { continue; }
            for (const auto& T : State.Encounter.Combatants)
            {
                const bool Kind = A.Target == EAbilityTarget::Self ? T.Id == Id : A.Target == EAbilityTarget::Enemy ? T.Side != Unit->Side : T.Side == Unit->Side && T.Id != Id;
                if (Kind && T.Life == ECombatLife::Conscious && GridDistance(Unit->Cell, T.Cell) <= A.Range && HasLineOfSight(Unit->Cell, T.Cell)) { Result.Add(T.Id); }
            }
        }
        return Result;
    }
    if (Action == ECombatCommandType::SecondWind)
    {
        if (FBuildCatalog::HasSpecial(*Unit, Action) && Unit->SecondWindUses > 0 && State.bBonusActionAvailable && Unit->HP < Unit->MaxHP) { Result.Add(Id); }
        return Result;
    }
    if (!State.bActionAvailable) { return Result; }
    if (Action == ECombatCommandType::PrecisionStrike && (!FBuildCatalog::HasSpecial(*Unit, Action) || Unit->ManeuverUses <= 0)) { return Result; }
    if (Action == ECombatCommandType::CureWounds && (!FBuildCatalog::HasSpecial(*Unit, Action) || Unit->SpellSlots <= 0)) { return Result; }
    if (Action == ECombatCommandType::MagicMissile && (!FBuildCatalog::HasSpecial(*Unit, Action) || Unit->SpellSlots <= 0)) { return Result; }
    if (Action == ECombatCommandType::Potion && Unit->Potions <= 0) { return Result; }
    for (const FCombatantState& Target : State.Encounter.Combatants)
    {
        bool bLegal = false;
        if ((Action == ECombatCommandType::Attack || Action == ECombatCommandType::PrecisionStrike)) { bLegal = CanTargetFrom(*Unit, Unit->Cell, Target); }
        else if (Action == ECombatCommandType::MagicMissile)
        {
            bLegal = Target.Side != Unit->Side && Target.Life == ECombatLife::Conscious && GridDistance(Unit->Cell, Target.Cell) <= MissileRange && HasLineOfSight(Unit->Cell, Target.Cell);
        }
        else if (Action == ECombatCommandType::CureWounds || Action == ECombatCommandType::Rescue || Action == ECombatCommandType::Potion)
        {
            bLegal = Unit->Side == ECombatSide::Party && Target.Side == Unit->Side && Target.Life != ECombatLife::Dead && Target.HP < Target.MaxHP &&
                GridDistance(Unit->Cell, Target.Cell) <= TouchRange && HasLineOfSight(Unit->Cell, Target.Cell) &&
                (Action != ECombatCommandType::Rescue || Target.Life == ECombatLife::Downed);
        }
        if (bLegal) { Result.Add(Target.Id); }
    }
    return Result;
}

TArray<FIntPoint> FCombatBattle::PathTo(int32 Id, FIntPoint Destination) const
{
    // Same legality as LegalMoves, with one search that also yields the route.
    TArray<FIntPoint> Path;
    const FCombatantState* Unit = Find(Id);
    if (!bStarted || State.Phase != EBattlePhase::Fighting || ActiveId() != Id || !Unit) { return Path; }
    TMap<FIntPoint, FIntPoint> Parents;
    const TMap<FIntPoint, int32> Costs = Distances(Id, &Parents);
    const int32* Cost = Costs.Find(Destination);
    if (!Cost || *Cost <= 0 || *Cost > State.MovementLeft) { return Path; }
    for (FIntPoint Cell = Destination; Cell != Unit->Cell; Cell = Parents[Cell]) { Path.Insert(Cell, 0); }
    return Path;
}

bool FCombatBattle::SneakAttackApplies(const FCombatantState& Actor, const FCombatantState& Target) const
{
    return (FBuildCatalog::Find(Actor.BuildId) ? FBuildCatalog::Find(Actor.BuildId)->SneakDie > 0 : Actor.AdventurerRole == EAdventurerRole::Rogue) && Actor.bSneakWeapon &&
        State.Encounter.Combatants.ContainsByPredicate([&Actor, &Target](const FCombatantState& Ally)
        {
            return Ally.Id != Actor.Id && Ally.Side == Actor.Side && Ally.Life == ECombatLife::Conscious && GridDistance(Ally.Cell, Target.Cell) <= 1;
        });
}

FActionForecast FCombatBattle::Forecast(int32 ActorId, ECombatCommandType Action, int32 TargetId) const
{
    FActionForecast Forecast;
    const FCombatantState* Actor = Find(ActorId);
    const FCombatantState* Target = Find(TargetId);
    if (!Actor || !Target || !LegalTargets(ActorId, Action).Contains(TargetId)) { return Forecast; }
    Forecast.bValid = true;
    if (Action == ECombatCommandType::Special1 || Action == ECombatCommandType::Special2)
    {
        for (const auto& A : FBuildCatalog::Abilities(*Actor))
        {
            if (A.Command != Action) { continue; }
            Forecast.Formula = A.Description;
            Forecast.bDiscreteAmounts = A.Effect == EAbilityEffect::Surge;
            if (A.Effect == EAbilityEffect::Strike || A.Effect == EAbilityEffect::Surge)
            {
                Forecast.HitChancePercent = 100;
                Forecast.MinAmount = A.Amount;
                Forecast.MaxAmount = A.Effect == EAbilityEffect::Surge ? A.Amount * 2 : A.Amount;
                auto Reduce = [&](int32 Amount)
                {
                    int32 Protection = Target->RageTurns > 0 ? 1 : 0;
                    for (const auto& U : State.Encounter.Combatants) { if (U.GuardTargetId == TargetId) { Protection = FMath::Max(Protection, 3); } }
                    return FMath::Max(0, Amount - Protection);
                };
                Forecast.MinAmount = Reduce(Forecast.MinAmount); Forecast.MaxAmount = Reduce(Forecast.MaxAmount);
                Forecast.bCanDrop = Forecast.MaxAmount >= Target->HP;
            }
        }
        return Forecast;
    }
    auto Effect = [&Forecast](int32 Count, int32 Sides, int32 Modifier)
    {
        Forecast.MinAmount += Count + Modifier;
        Forecast.MaxAmount += Count * Sides + Modifier;
        Forecast.Formula = Modifier ? FString::Printf(TEXT("%dd%d%+d"), Count, Sides, Modifier) : FString::Printf(TEXT("%dd%d"), Count, Sides);
    };
    switch (Action)
    {
    case ECombatCommandType::PrecisionStrike:
    case ECombatCommandType::Attack:
    {
        Forecast.bAttackRoll = true;
        int32 Hits = 0;
        for (int32 Natural = 1; Natural <= 20; ++Natural)
        { Hits += Natural >= CriticalThreshold(*Actor) || (Natural != 1 && Natural + Actor->AttackBonus + (Action == ECombatCommandType::PrecisionStrike ? PrecisionBonus : 0) >= Target->ArmorClass) ? 1 : 0; }
        Forecast.HitChancePercent = Hits * 5;
        Effect(1, Actor->DamageDie, FBuildCatalog::AttackModifier(*Actor) + (Action == ECombatCommandType::PrecisionStrike ? PrecisionBonus : 0));
        Forecast.bSneakAttack = SneakAttackApplies(*Actor, *Target);
        if (Forecast.bSneakAttack) { Forecast.MinAmount += 1; Forecast.MaxAmount += SneakDie; Forecast.Formula += FString::Printf(TEXT(" +1d%d sneak"), SneakDie); }
        break;
    }
    case ECombatCommandType::MagicMissile: Forecast.HitChancePercent = 100; Effect(MissileDarts, MissileDie, MissileDarts * DartBonus); break;
    case ECombatCommandType::SecondWind: Effect(1, WindDie, FighterLevel); break;
    case ECombatCommandType::CureWounds: Effect(CureDice, CureDie, HealingModifier); break;
    case ECombatCommandType::Potion: Effect(PotionDice, PotionDie, PotionBonus); break;
    case ECombatCommandType::Rescue: Forecast.MinAmount = Forecast.MaxAmount = 1; Forecast.Formula = TEXT("1"); break;
    default: Forecast.bValid = false; return Forecast;
    }
    Forecast.bHealing = Action != ECombatCommandType::Attack && Action != ECombatCommandType::PrecisionStrike && Action != ECombatCommandType::MagicMissile;
    if (Forecast.bHealing)
    {
        const int32 Missing = Target->MaxHP - Target->HP;
        Forecast.MinAmount = FMath::Min(Forecast.MinAmount, Missing);
        Forecast.MaxAmount = FMath::Min(Forecast.MaxAmount, Missing);
    }
    else
    {
        // A critical doubles the weapon and Sneak Attack dice, so warn on that best case too.
        const int32 CriticalExtra = Forecast.bAttackRoll ? Actor->DamageDie + (Forecast.bSneakAttack ? SneakDie : 0) : 0;
        Forecast.bCanDrop = Forecast.MaxAmount + CriticalExtra >= Target->HP;
    }
    if (!Forecast.bHealing)
    {
        int32 Protection = Target->RageTurns > 0 ? 1 : 0;
        for (const auto& U : State.Encounter.Combatants) { if (U.GuardTargetId == TargetId) { Protection = FMath::Max(Protection, 3); } }
        const int32 CriticalExtra = Forecast.bAttackRoll ? Actor->DamageDie + (Forecast.bSneakAttack ? SneakDie : 0) : 0;
        Forecast.bCanDrop = FMath::Max(0, Forecast.MaxAmount + CriticalExtra - Protection) >= Target->HP;
        Forecast.MinAmount = FMath::Max(0, Forecast.MinAmount - Protection);
        Forecast.MaxAmount = FMath::Max(0, Forecast.MaxAmount - Protection);
        if (Protection > 0) { Forecast.Formula += FString::Printf(TEXT(" minus %d protection (minimum 0)"), Protection); }
    }
    return Forecast;
}

FCombatResult FCombatBattle::Submit(const FCombatCommand& Command)
{
    TArray<ECombatLife> LifeBefore;
    for (const FCombatantState& Unit : State.Encounter.Combatants) { LifeBefore.Add(Unit.Life); }
    FCombatResult Result = Resolve(Command);
    DiceLog = nullptr;
    if (!Result.bAccepted) { return Result; }
    Result.Type = Command.Type;
    Result.ActorId = Command.ActorId;
    Result.TargetId = Command.TargetId;
    for (int32 Index = 0; Index < State.Encounter.Combatants.Num() && Index < LifeBefore.Num(); ++Index)
    {
        const FCombatantState& Unit = State.Encounter.Combatants[Index];
        if (Unit.Life == LifeBefore[Index]) { continue; }
        if (Unit.Life == ECombatLife::Downed) { Result.Downed.Add(Unit.Id); }
        if (Unit.Life == ECombatLife::Dead) { Result.Died.Add(Unit.Id); }
    }
    return Result;
}

FCombatResult FCombatBattle::Resolve(const FCombatCommand& Command)
{
    FCombatResult Result;
    auto Reject = [&Result](const TCHAR* Reason) { Result.Feedback = Reason; return Result; };
    const FCombatantState* Actor = Find(Command.ActorId);
    if (!bStarted || State.Phase != EBattlePhase::Fighting) { return Reject(TEXT("Battle is not active.")); }
    if (!Actor || Actor->Life != ECombatLife::Conscious || ActiveId() != Command.ActorId) { return Reject(TEXT("Only the active combatant can act.")); }
    if (Command.Type == ECombatCommandType::Move)
    {
        Result.Path = PathTo(Command.ActorId, Command.Destination);
        if (Result.Path.IsEmpty()) { return Reject(TEXT("Cell is occupied, blocked, or beyond remaining movement.")); }
        // Every step costs one: the shortest path length is the movement spent.
        const int32 Cost = Result.Path.Num();
        FCombatantState* Moving = State.Encounter.Combatants.FindByPredicate([&Command](const FCombatantState& Unit) { return Unit.Id == Command.ActorId; });
        Moving->Cell = Command.Destination;
        State.MovementLeft -= Cost;
        Result.Feedback = FString::Printf(TEXT("%s moves %d cells (%d left)."), *Actor->Name, Cost, State.MovementLeft);
    }
    else if (Command.Type == ECombatCommandType::Special1 || Command.Type == ECombatCommandType::Special2)
    {
        if (!LegalTargets(Command.ActorId, Command.Type).Contains(Command.TargetId)) { return Reject(TEXT("Special unavailable: budget, stance, resource, target, range or sight.")); }
        for (const auto& A : FBuildCatalog::Abilities(*Actor))
        {
            if (A.Command != Command.Type) { continue; }
            auto* User = MutableFind(Command.ActorId); auto* Target = MutableFind(Command.TargetId);
            if (A.Effect == EAbilityEffect::Rage) { --User->RageUses; User->RageTurns = 2; }
            else if (A.Effect == EAbilityEffect::Protect) { User->GuardTargetId = Target->Id; }
            else
            {
                int32 Amount = A.Amount;
                if (A.Effect == EAbilityEffect::Surge) { User->LastSurge = Result.SurgeOutcome = Roll(2); Amount = Result.SurgeOutcome * A.Amount; }
                const int32 Before = Target->HP; ApplyDamage(*Target, Amount); Result.Damage = Before - Target->HP; Result.bHit = true;
            }
            Result.Effects.Add({Target->Id, A.Effect, A.Effect == EAbilityEffect::Protect ? 3 : A.Effect == EAbilityEffect::Rage ? 2 : Result.Damage, Target->Cell});
            if (A.bBonus) { State.bBonusActionAvailable = false; } else { State.bActionAvailable = false; }
            Result.Feedback = A.Name + TEXT(": ") + A.Description;
        }
        CheckOutcome();
    }
    else if (Command.Type == ECombatCommandType::Attack || Command.Type == ECombatCommandType::PrecisionStrike)
    {
        if (!LegalTargets(Command.ActorId, Command.Type).Contains(Command.TargetId)) { return Reject(TEXT("No attack available, or target is inactive, friendly, out of range, or obscured.")); }
        FCombatantState* Target = State.Encounter.Combatants.FindByPredicate([&Command](const FCombatantState& Unit) { return Unit.Id == Command.TargetId; });
        const int32 ManeuverBonus = Command.Type == ECombatCommandType::PrecisionStrike ? PrecisionBonus : 0;
        if (ManeuverBonus) { --MutableFind(Command.ActorId)->ManeuverUses; }
        Result.NaturalRoll = Roll(20);
        Result.AttackTotal = Result.NaturalRoll + Actor->AttackBonus + ManeuverBonus;
        Result.AttackBonus = Actor->AttackBonus + ManeuverBonus;
        Result.TargetArmorClass = Target->ArmorClass;
        Result.bCritical = Result.NaturalRoll >= CriticalThreshold(*Actor);
        Result.bHit = Result.bCritical || (Result.NaturalRoll != 1 && Result.AttackTotal >= Target->ArmorClass);
        if (Result.bHit)
        {
            DiceLog = &Result.EffectDice;
            // Weapon die, then the critical die; both have the same sides, so totals match earlier builds.
            Result.Damage = Roll(Actor->DamageDie);
            if (Result.bCritical) { Result.Damage += Roll(Actor->DamageDie); }
            Result.EffectModifier = FBuildCatalog::AttackModifier(*Actor) + ManeuverBonus;
            Result.Damage += Result.EffectModifier;
            Result.bSneakAttack = SneakAttackApplies(*Actor, *Target);
            if (Result.bSneakAttack) { Result.Damage += RollDice(Result.bCritical ? 2 : 1, SneakDie); }
            DiceLog = nullptr;
            int32 Protection = Target->RageTurns > 0 ? 1 : 0;
            for (const auto& U : State.Encounter.Combatants) { if (U.GuardTargetId == Target->Id) { Protection = FMath::Max(Protection, 3); } }
            Result.DamagePrevented = FMath::Min(Result.Damage, Protection);
            ApplyDamage(*Target, Result.Damage);
            Result.Damage -= Result.DamagePrevented;
        }
        State.bActionAvailable = false;
        CheckOutcome();
        FString TargetStatus = Target->Life == ECombatLife::Downed ? FString::Printf(TEXT(" DOWNED (%d rescue turns)"), Target->RescueTurnsLeft) :
            Target->Life == ECombatLife::Dead ? TEXT(" DEAD") : TEXT("");
        if (Result.bSneakAttack) { TargetStatus += FString::Printf(TEXT(" SNEAK +d%d"), SneakDie); }
        Result.Feedback = FString::Printf(TEXT("%s > %s: d20 %d + %d = %d vs AC %d | %s | %d damage. HP %d/%d%s"),
            *Actor->Name, *Target->Name, Result.NaturalRoll, Actor->AttackBonus, Result.AttackTotal, Target->ArmorClass,
            Result.bCritical ? TEXT("CRITICAL") : Result.bHit ? TEXT("HIT") : TEXT("MISS"), Result.Damage,
            Target->HP, Target->MaxHP, *TargetStatus);
    }
    else if (Command.Type == ECombatCommandType::SecondWind || Command.Type == ECombatCommandType::CureWounds ||
        Command.Type == ECombatCommandType::MagicMissile || Command.Type == ECombatCommandType::Rescue || Command.Type == ECombatCommandType::Potion)
    {
        if (!LegalTargets(Command.ActorId, Command.Type).Contains(Command.TargetId)) { return Reject(TEXT("Ability unavailable: check role, budget, uses, living target, range and sight.")); }
        FCombatantState* User = MutableFind(Command.ActorId);
        FCombatantState* Target = MutableFind(Command.TargetId);
        FString Ability = TEXT("Rescue");
        if (Command.Type == ECombatCommandType::MagicMissile)
        {
            --User->SpellSlots;
            DiceLog = &Result.EffectDice;
            Result.EffectModifier = MissileDarts * DartBonus;
            Result.Damage = RollDice(MissileDarts, MissileDie) + Result.EffectModifier;
            DiceLog = nullptr;
            Result.bHit = true;
            int32 Protection = Target->RageTurns > 0 ? 1 : 0;
            for (const auto& U : State.Encounter.Combatants) { if (U.GuardTargetId == Target->Id) { Protection = FMath::Max(Protection, 3); } }
            Result.DamagePrevented = FMath::Min(Result.Damage, Protection);
            ApplyDamage(*Target, Result.Damage);
            Result.Damage -= Result.DamagePrevented;
            Ability = FString::Printf(TEXT("Magic Missile (%d darts, 1 slot)"), MissileDarts);
        }
        else
        {
            int32 Amount = 1;
            DiceLog = &Result.EffectDice;
            if (Command.Type == ECombatCommandType::SecondWind) { --User->SecondWindUses; Result.EffectModifier = FighterLevel; Amount = Roll(WindDie) + FighterLevel; Ability = TEXT("Second Wind (bonus, 1 use)"); }
            else if (Command.Type == ECombatCommandType::CureWounds) { --User->SpellSlots; Result.EffectModifier = HealingModifier; Amount = RollDice(CureDice, CureDie) + HealingModifier; Ability = TEXT("Cure Wounds (1 slot)"); }
            else if (Command.Type == ECombatCommandType::Potion) { --User->Potions; Result.EffectModifier = PotionBonus; Amount = RollDice(PotionDice, PotionDie) + PotionBonus; Ability = TEXT("Potion (1 bottle)"); }
            else { Result.EffectModifier = 1; }
            DiceLog = nullptr;
            Result.Healing = FMath::Min(Amount, Target->MaxHP - Target->HP);
            Target->HP += Result.Healing;
            Target->Life = ECombatLife::Conscious;
            Target->RescueTurnsLeft = 0;
        }
        if (Command.Type == ECombatCommandType::SecondWind) { State.bBonusActionAvailable = false; }
        else { State.bActionAvailable = false; }
        Result.Feedback = FString::Printf(TEXT("%s: %s > %s | heal %d, damage %d | HP %d/%d"), *User->Name, *Ability, *Target->Name, Result.Healing, Result.Damage, Target->HP, Target->MaxHP);
        CheckOutcome();
    }
    else if (Command.Type == ECombatCommandType::EndTurn)
    {
        Result.Feedback = Actor->Name + TEXT(" ends turn.");
        auto* User = MutableFind(Command.ActorId);
        if (User->RageTurns > 0 && --User->RageTurns == 0) { User->GuardTargetId = INDEX_NONE; }
        AdvanceTurn();
    }
    else { return Reject(TEXT("Unknown command.")); }
    Result.bAccepted = true;
    return Result;
}

FCombatantState* FCombatBattle::MutableFind(int32 Id)
{
    return State.Encounter.Combatants.FindByPredicate([Id](const FCombatantState& Unit) { return Unit.Id == Id; });
}

void FCombatBattle::ApplyDamage(FCombatantState& Target, int32 Damage)
{
    int32 Protection = Target.RageTurns > 0 ? 1 : 0;
    for (const auto& U : State.Encounter.Combatants) { if (U.GuardTargetId == Target.Id) { Protection = FMath::Max(Protection, 3); } }
    Target.HP = FMath::Max(0, Target.HP - FMath::Max(0, Damage - Protection));
    if (Target.HP == 0)
    {
        Target.RageTurns = 0; Target.GuardTargetId = INDEX_NONE; Target.LastSurge = 0;
        for (auto& U : State.Encounter.Combatants) { if (U.GuardTargetId == Target.Id) { U.GuardTargetId = INDEX_NONE; } }
        Target.Life = Target.Side == ECombatSide::Party ? ECombatLife::Downed : ECombatLife::Dead;
        Target.RescueTurnsLeft = Target.Life == ECombatLife::Downed ? 3 : 0;
    }
}

void FCombatBattle::CheckOutcome()
{
    bool bParty = false;
    bool bEnemy = false;
    for (const FCombatantState& Unit : State.Encounter.Combatants)
    {
        bParty |= Unit.Side == ECombatSide::Party && Unit.Life == ECombatLife::Conscious;
        bEnemy |= Unit.Side == ECombatSide::Enemy && Unit.Life == ECombatLife::Conscious;
    }
    State.Phase = !bParty ? EBattlePhase::Defeat : !bEnemy ? EBattlePhase::Victory : EBattlePhase::Fighting;
    // Rescue must happen during combat. A wipe or an unresolved victory deadline is permanent.
    if (State.Phase != EBattlePhase::Fighting)
    {
        for (FCombatantState& Unit : State.Encounter.Combatants)
        {
            Unit.RageTurns = 0; Unit.GuardTargetId = INDEX_NONE; Unit.LastSurge = 0;
            if (Unit.Life == ECombatLife::Downed) { Unit.Life = ECombatLife::Dead; Unit.RescueTurnsLeft = 0; }
        }
    }
}

void FCombatBattle::AdvanceTurn()
{
    for (int32 Count = 0; Count < State.TurnOrder.Num(); ++Count)
    {
        State.TurnIndex = (State.TurnIndex + 1) % State.TurnOrder.Num();
        if (State.TurnIndex == 0) { ++State.Round; }
        FCombatantState* Next = MutableFind(ActiveId());
        if (Next->Life == ECombatLife::Downed)
        {
            if (--Next->RescueTurnsLeft == 0) { Next->Life = ECombatLife::Dead; }
        }
        else if (Next->Life == ECombatLife::Conscious)
        {
            Next->GuardTargetId = INDEX_NONE;
            State.MovementLeft = Next->Speed;
            State.bActionAvailable = true;
            State.bBonusActionAvailable = true;
            return;
        }
    }
}

TArray<FCombatResult> FCombatBattle::PlayEnemyTurn()
{
    TArray<FCombatResult> Results;
    const int32 Id = ActiveId();
    const FCombatantState* Enemy = Find(Id);
    if (!bStarted || State.Phase != EBattlePhase::Fighting || !Enemy || Enemy->Side != ECombatSide::Enemy) { return Results; }
    if (LegalTargets(Id).IsEmpty() && State.MovementLeft > 0 && State.bActionAvailable)
    {
        TMap<FIntPoint, FIntPoint> Parents;
        const TMap<FIntPoint, int32> Costs = Distances(Id, &Parents);
        TOptional<FIntPoint> Best;
        int32 BestCost = MAX_int32;
        // Search in fixed cell order for a reachable firing cell. TMap iteration never chooses AI ties.
        for (int32 Y = 0; Y < State.Encounter.Height; ++Y)
        {
            for (int32 X = 0; X < State.Encounter.Width; ++X)
            {
                const FIntPoint Cell(X, Y);
                const int32* Cost = Costs.Find(Cell);
                if (!Cost || *Cost >= BestCost) { continue; }
                for (const FCombatantState& Target : State.Encounter.Combatants)
                {
                    if (CanTargetFrom(*Enemy, Cell, Target))
                    {
                        Best = Cell; BestCost = *Cost; break;
                    }
                }
            }
        }
        if (Best.IsSet() && BestCost > 0)
        {
            FIntPoint Destination = Best.GetValue();
            while (Costs[Destination] > State.MovementLeft) { Destination = Parents[Destination]; }
            Results.Add(Submit({ECombatCommandType::Move, Id, Destination}));
        }
    }
    const TArray<int32> Targets = LegalTargets(Id);
    if (!Targets.IsEmpty()) { Results.Add(Submit({ECombatCommandType::Attack, Id, {}, Targets[0]})); }
    if (State.Phase == EBattlePhase::Fighting) { Results.Add(Submit({ECombatCommandType::EndTurn, Id})); }
    return Results;
}

FCombatEncounter FCombatBattle::TutorialEncounter()
{
    FCombatEncounter Encounter;
    Encounter.BlockedCells = {{3, 3}, {4, 3}, {3, 4}};
    const TCHAR* Names[] = {TEXT("Arden"), TEXT("Nix"), TEXT("Sera"), TEXT("Ivo"), TEXT("Briar raider"), TEXT("Ash raider"), TEXT("Moss archer")};
    const TCHAR* Roles[] = {TEXT("Fighter"), TEXT("Rogue"), TEXT("Cleric"), TEXT("Wizard"), TEXT("Raider"), TEXT("Raider"), TEXT("Archer")};
    const FIntPoint Cells[] = {{1, 1}, {2, 1}, {1, 2}, {2, 2}, {5, 5}, {6, 5}, {5, 6}};
    for (int32 Index = 0; Index < 7; ++Index)
    {
        FCombatantState Unit;
        Unit.Id = Index + 1;
        Unit.Name = Names[Index]; Unit.Role = Roles[Index]; Unit.Cell = Cells[Index];
        Unit.Side = Index < 4 ? ECombatSide::Party : ECombatSide::Enemy;
        Unit.MaxHP = Unit.HP = Index == 0 ? 30 : Index < 4 ? 22 : 16;
        if (Index < 4)
        {
            const TCHAR* Builds[] = {TEXT("fighter.champion"), TEXT("rogue.base"), TEXT("cleric.base"), TEXT("wizard.base")};
            FBuildCatalog::Create(Builds[Index], Index + 1, Names[Index], Unit);
            Unit.Cell = Cells[Index];
            // Historical standalone fixture is a base Fighter; expedition chooses a subclass.
            if (Index == 0) { Unit.BuildId.Reset(); Unit.ClassId.Reset(); Unit.SubclassId.Reset(); Unit.FighterSubclass = EFighterSubclass::None; }
        }
        else
        {
            Unit.ArmorClass = 12; Unit.Range = Index == 6 ? 4 : 1;
            Unit.InitiativeBonus = 1; Unit.AttackBonus = 3; Unit.DamageBonus = 1;
        }
        Encounter.Combatants.Add(Unit);
    }
    return Encounter;
}

bool FCombatBattle::Restore(const FCombatSnapshot& Snapshot, FString& Error)
{
    if (!ValidateEncounter(Snapshot.Encounter, Error)) { return false; }
    auto Reject = [&Error]() { Error = TEXT("Invalid saved combat phase, initiative, turn or budget."); return false; };
    if (static_cast<uint8>(Snapshot.Phase) > static_cast<uint8>(EBattlePhase::Defeat) ||
        Snapshot.Round < 1 || Snapshot.Round > 1000000 || Snapshot.TurnOrder.IsEmpty() ||
        Snapshot.TurnOrder.Num() > Snapshot.Encounter.Combatants.Num() || !Snapshot.TurnOrder.IsValidIndex(Snapshot.TurnIndex)) { return Reject(); }
    FCombatBattle Candidate;
    Candidate.State = Snapshot;
    Candidate.bStarted = true;
    TSet<int32> Ordered;
    const FCombatantState* Previous = nullptr;
    for (int32 Id : Snapshot.TurnOrder)
    {
        const auto* Unit = Candidate.Find(Id);
        if (!Unit || Ordered.Contains(Id) || (Previous && (Previous->Initiative < Unit->Initiative ||
            (Previous->Initiative == Unit->Initiative && Previous->Id > Unit->Id)))) { return Reject(); }
        Ordered.Add(Id); Previous = Unit;
    }
    bool bParty = false, bEnemy = false;
    for (const auto& Unit : Snapshot.Encounter.Combatants)
    {
        if (Unit.Name.IsEmpty() || Unit.Name.Len() > 80 || Unit.Role.IsEmpty() || Unit.Role.Len() > 80 ||
            Unit.Initiative < 1 + Unit.InitiativeBonus || Unit.Initiative > 20 + Unit.InitiativeBonus ||
            (Unit.Life != ECombatLife::Dead && !Ordered.Contains(Unit.Id)) ||
            (Unit.Side == ECombatSide::Enemy && (Unit.AdventurerRole != EAdventurerRole::None || Unit.Life == ECombatLife::Downed)) ||
            (Snapshot.Phase != EBattlePhase::Fighting && (Unit.Life == ECombatLife::Downed || Unit.RageTurns != 0 || Unit.GuardTargetId != INDEX_NONE || Unit.LastSurge != 0))) { return Reject(); }
        bParty |= Unit.Side == ECombatSide::Party && Unit.Life == ECombatLife::Conscious;
        bEnemy |= Unit.Side == ECombatSide::Enemy && Unit.Life == ECombatLife::Conscious;
    }
    const auto Expected = !bParty ? EBattlePhase::Defeat : !bEnemy ? EBattlePhase::Victory : EBattlePhase::Fighting;
    const auto* Active = Candidate.Find(Candidate.ActiveId());
    if (Snapshot.Phase != Expected || !Active || Active->Life != ECombatLife::Conscious ||
        Snapshot.MovementLeft < 0 || Snapshot.MovementLeft > Active->Speed ||
        (!Snapshot.bBonusActionAvailable && Active->AdventurerRole == EAdventurerRole::Barbarian && Snapshot.Phase == EBattlePhase::Fighting && Active->RageTurns == 0) ||
        (!Snapshot.bBonusActionAvailable && ((Active->AdventurerRole != EAdventurerRole::Fighter && Active->AdventurerRole != EAdventurerRole::Barbarian) || (Active->AdventurerRole == EAdventurerRole::Fighter && Snapshot.Phase == EBattlePhase::Fighting && Active->SecondWindUses > 1)))) { return Reject(); }
    *this = MoveTemp(Candidate);
    Error.Reset(); return true;
}
