#if WITH_DEV_AUTOMATION_TESTS
#include "BalanceHarness.h"

#include "Combat/ActionFacts.h"
#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Party/BuildCatalog.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace BalanceHarness
{
namespace Policy
{
using ECmd = ECombatCommandType;

int32 Manhattan(FIntPoint A, FIntPoint B) { return FMath::Abs(A.X - B.X) + FMath::Abs(A.Y - B.Y); }
bool Conscious(const FCombatantState& U) { return U.Life == ECombatLife::Conscious; }
bool Low(const FCombatantState& U) { return Conscious(U) && U.HP * 2 <= U.MaxHP; }
double Fraction(const FCombatantState& U) { return U.MaxHP > 0 ? static_cast<double>(U.HP) / U.MaxHP : 0.0; }
bool IsDamage(EAbilityEffect E)
{
    return E == EAbilityEffect::Strike || E == EAbilityEffect::Surge || E == EAbilityEffect::Push || E == EAbilityEffect::Area ||
        E == EAbilityEffect::Missile || E == EAbilityEffect::WeaponAttack;
}

/** Walking distance from every cell to the nearest conscious enemy of Side (a heuristic; legality comes from LegalMoves).
 * With bUnits, cells holding any other unit (bodies included) are impassable except for Self's own cell. */
struct FField
{
    int32 Width = 0, Height = 0;
    TArray<int32> Steps;
    int32 At(FIntPoint C) const
    { return C.X >= 0 && C.Y >= 0 && C.X < Width && C.Y < Height ? Steps[C.Y * Width + C.X] : MAX_int32; }
};

/** Breadth-first walking distance from Sources over unblocked cells; with bUnits, cells holding a unit other than SelfId are impassable. */
FField Spread(const FCombatSnapshot& S, const TArray<FIntPoint>& Sources, int32 SelfId, bool bUnits)
{
    FField Field;
    Field.Width = S.Encounter.Width; Field.Height = S.Encounter.Height;
    Field.Steps.Init(MAX_int32, Field.Width * Field.Height);
    TArray<FIntPoint> Queue;
    for (const FIntPoint Cell : Sources) { if (Field.At(Cell) == MAX_int32) { Field.Steps[Cell.Y * Field.Width + Cell.X] = 0; Queue.Add(Cell); } }
    const FIntPoint Offsets[] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    for (int32 Index = 0; Index < Queue.Num(); ++Index)
    {
        const FIntPoint Cell = Queue[Index];
        const int32 Next = Field.At(Cell) + 1;
        for (const FIntPoint Offset : Offsets)
        {
            const FIntPoint To = Cell + Offset;
            if (To.X < 0 || To.Y < 0 || To.X >= Field.Width || To.Y >= Field.Height || S.Encounter.BlockedCells.Contains(To) || Field.At(To) != MAX_int32) { continue; }
            if (bUnits && S.Encounter.Combatants.ContainsByPredicate([&](const FCombatantState& U) { return U.Id != SelfId && U.Cell == To; })) { continue; }
            Field.Steps[To.Y * Field.Width + To.X] = Next;
            Queue.Add(To);
        }
    }
    return Field;
}

FField EnemyField(const FCombatSnapshot& S, const FCombatantState& Self, bool bUnits)
{
    TArray<FIntPoint> Sources;
    for (const auto& U : S.Encounter.Combatants) { if (U.Side != Self.Side && Conscious(U)) { Sources.Add(U.Cell); } }
    return Spread(S, Sources, Self.Id, bUnits);
}

/** Walking steps from the actor over open cells, matching how the rules count movement (other units block). */
FField StepsFrom(const FCombatSnapshot& S, const FCombatantState& Me)
{
    FField Field;
    Field.Width = S.Encounter.Width; Field.Height = S.Encounter.Height;
    Field.Steps.Init(MAX_int32, Field.Width * Field.Height);
    Field.Steps[Me.Cell.Y * Field.Width + Me.Cell.X] = 0;
    TArray<FIntPoint> Queue = {Me.Cell};
    const FIntPoint Offsets[] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    for (int32 Index = 0; Index < Queue.Num(); ++Index)
    {
        const FIntPoint Cell = Queue[Index];
        const int32 Next = Field.At(Cell) + 1;
        for (const FIntPoint Offset : Offsets)
        {
            const FIntPoint To = Cell + Offset;
            if (To.X < 0 || To.Y < 0 || To.X >= Field.Width || To.Y >= Field.Height || S.Encounter.BlockedCells.Contains(To) || Field.At(To) != MAX_int32) { continue; }
            if (S.Encounter.Combatants.ContainsByPredicate([&](const FCombatantState& U) { return U.Id != Me.Id && U.Cell == To; })) { continue; }
            Field.Steps[To.Y * Field.Width + To.X] = Next;
            Queue.Add(To);
        }
    }
    return Field;
}

bool CellBefore(FIntPoint A, FIntPoint B) { return A.Y == B.Y ? A.X < B.X : A.Y < B.Y; }

/** Protection that already applies to a target: the strongest guard on it, or 1 for its own Rage. */
int32 ExistingProtection(const FCombatSnapshot& S, const FCombatantState& T)
{
    int32 Amount = T.RageTurns > 0 ? 1 : 0;
    for (const auto& U : S.Encounter.Combatants)
    { if (U.GuardTargetId == T.Id) { Amount = FMath::Max(Amount, U.GuardAmount > 0 ? U.GuardAmount : 3); } }
    return Amount;
}

bool Threatened(const FCombatSnapshot& S, const FCombatantState& T)
{
    return S.Encounter.Combatants.ContainsByPredicate([&](const FCombatantState& E)
    { return E.Side != T.Side && Conscious(E) && Manhattan(E.Cell, T.Cell) <= E.Speed + E.Range; });
}

bool AdjacentEnemy(const FCombatSnapshot& S, const FCombatantState& T)
{
    return S.Encounter.Combatants.ContainsByPredicate([&](const FCombatantState& E)
    { return E.Side != T.Side && Conscious(E) && Manhattan(E.Cell, T.Cell) <= 1; });
}

int32 NearestEnemy(const FCombatSnapshot& S, const FCombatantState& Me, FIntPoint Cell)
{
    int32 Best = MAX_int32;
    for (const auto& E : S.Encounter.Combatants) { if (E.Side != Me.Side && Conscious(E)) { Best = FMath::Min(Best, Manhattan(E.Cell, Cell)); } }
    return Best;
}

double Expected(const FActionForecast& F, EAbilityEffect Effect)
{
    if (Effect == EAbilityEffect::Area)
    {
        int32 Sum = 0;
        for (const auto& Hit : F.Effects) { if (Hit.Effect == EAbilityEffect::Strike) { Sum += Hit.Amount; } }
        return Sum;
    }
    return 0.5 * (F.MinAmount + F.MaxAmount) * F.HitChancePercent / 100.0;
}

/** Move the actor on a discarded copy and test the condition there. A Compact move consumes no random numbers. */
template <typename FCondition>
bool AfterMove(const FExpeditionRun& Run, int32 Id, FIntPoint Cell, FCondition&& Condition)
{
    FExpeditionRun Copy = Run;
    return Copy.Submit({ECmd::Move, Id, Cell}).bAccepted && Condition(Copy.Battle());
}

/** The approach move: the legal cell closest to an enemy by the unit-aware distance, or by terrain alone when
 * the units block every route. Only a cell closer than the hero's own counts. Shared by the policy and PassiveEnd. */
const FIntPoint* ApproachMove(const FCombatSnapshot& S, const FCombatantState& Me, const TArray<FIntPoint>& Moves, const FField& Steps)
{
    for (const bool bUnits : {true, false})
    {
        const FField Field = EnemyField(S, Me, bUnits);
        const FIntPoint* Best = nullptr;
        for (const FIntPoint& C : Moves)
        {
            if (Field.At(C) >= Field.At(Me.Cell)) { continue; }
            if (!Best || Field.At(C) < Field.At(*Best) || (Field.At(C) == Field.At(*Best) && (Steps.At(C) < Steps.At(*Best) || (Steps.At(C) == Steps.At(*Best) && CellBefore(C, *Best))))) { Best = &C; }
        }
        if (Best) { return Best; }
    }
    return nullptr;
}

struct FChoice
{
    FCombatCommand Command;
    bool bValid = false;
};

FChoice Make(ECmd Type, int32 Id, int32 Target) { FChoice C; C.Command = {Type, Id, FIntPoint::ZeroValue, Target}; C.bValid = true; return C; }
FChoice MakeMove(int32 Id, FIntPoint Cell) { FChoice C; C.Command = {ECmd::Move, Id, Cell}; C.bValid = true; return C; }

/** One decision step of the documented policy (contract section 3). Returns no choice for End Turn. */
FChoice Decide(const FExpeditionRun& Run, int32 Id)
{
    const FCombatBattle& B = Run.Battle();
    const FCombatSnapshot& S = B.Snapshot();
    const FCombatantState* MePtr = B.Find(Id);
    if (!MePtr) { return {}; }
    const FCombatantState& Me = *MePtr;
    const TArray<FAbilityFacts> Abilities = FBuildCatalog::Abilities(Me);
    // Healer's reserve: a kit with a Kit-paid Healing special keeps its last Kit use for healing.
    const bool bHealerKit = Abilities.ContainsByPredicate([](const FAbilityFacts& A) { return A.Effect == EAbilityEffect::Healing && A.bKitResource; });
    auto Reserved = [&](const FAbilityFacts& A) { return bHealerKit && A.Effect != EAbilityEffect::Healing && A.bKitResource && Me.KitUses <= A.ResourceCost; };
    auto Unit = [&B](int32 TargetId) -> const FCombatantState& { return *B.Find(TargetId); };
    auto Downed = [&S, &Me]()
    {
        TArray<const FCombatantState*> Result;
        for (const auto& U : S.Encounter.Combatants) { if (U.Side == Me.Side && U.Id != Me.Id && U.Life == ECombatLife::Downed) { Result.Add(&U); } }
        Result.Sort([](const FCombatantState& A, const FCombatantState& C) { return A.RescueTurnsLeft == C.RescueTurnsLeft ? A.Id < C.Id : A.RescueTurnsLeft < C.RescueTurnsLeft; });
        return Result;
    }();

    // 1. Rage when a fight is about to start.
    for (const auto& A : Abilities)
    {
        if (A.Effect == EAbilityEffect::Rage && B.LegalTargets(Id, A.Command).Contains(Id) && NearestEnemy(S, Me, Me.Cell) <= Me.Speed + Me.Range + 1)
        { return Make(A.Command, Id, Id); }
    }
    // 2. Revive an adjacent Downed ally: a Potion if legal, else Rescue.
    for (const FCombatantState* Ally : Downed)
    {
        if (B.LegalTargets(Id, ECmd::Potion).Contains(Ally->Id)) { return Make(ECmd::Potion, Id, Ally->Id); }
        if (B.LegalTargets(Id, ECmd::Rescue).Contains(Ally->Id)) { return Make(ECmd::Rescue, Id, Ally->Id); }
    }
    // 3. Walk to a Downed ally when the Action is still free.
    const TArray<FIntPoint> Moves = B.LegalMoves(Id);
    if (S.bActionAvailable && !Downed.IsEmpty() && !Moves.IsEmpty())
    {
        const FField Steps = StepsFrom(S, Me);
        TArray<FIntPoint> Cells = Moves.FilterByPredicate([&](FIntPoint C)
        { return Downed.ContainsByPredicate([C](const FCombatantState* Ally) { return Manhattan(C, Ally->Cell) == 1; }); });
        Cells.Sort([&Steps](FIntPoint A, FIntPoint C) { return Steps.At(A) == Steps.At(C) ? CellBefore(A, C) : Steps.At(A) < Steps.At(C); });
        for (int32 I = 0; I < Cells.Num() && I < 16; ++I)
        {
            const bool bReaches = AfterMove(Run, Id, Cells[I], [&](const FCombatBattle& After)
            {
                for (const FCombatantState* Ally : Downed)
                { if (After.LegalTargets(Id, ECmd::Rescue).Contains(Ally->Id) || After.LegalTargets(Id, ECmd::Potion).Contains(Ally->Id)) { return true; } }
                return false;
            });
            if (bReaches) { return MakeMove(Id, Cells[I]); }
        }
        // Not reachable this turn: the ally nearest the most urgent Downed hero walks toward a free cell beside them.
        const FCombatantState& Urgent = *Downed[0];
        TArray<FIntPoint> Beside;
        for (const FIntPoint Offset : {FIntPoint(1, 0), FIntPoint(-1, 0), FIntPoint(0, 1), FIntPoint(0, -1)})
        {
            const FIntPoint C = Urgent.Cell + Offset;
            if (C.X >= 0 && C.Y >= 0 && C.X < S.Encounter.Width && C.Y < S.Encounter.Height && !S.Encounter.BlockedCells.Contains(C) &&
                !S.Encounter.Combatants.ContainsByPredicate([C](const FCombatantState& U) { return U.Cell == C; })) { Beside.Add(C); }
        }
        const FField ToAlly = Spread(S, Beside, INDEX_NONE, true);
        auto DistanceOf = [&](const FCombatantState& U)
        {
            int32 Best = MAX_int32;
            for (const FIntPoint Offset : {FIntPoint(1, 0), FIntPoint(-1, 0), FIntPoint(0, 1), FIntPoint(0, -1)})
            { const int32 D = ToAlly.At(U.Cell + Offset); if (D != MAX_int32) { Best = FMath::Min(Best, D + 1); } }
            return Best;
        };
        const int32 Mine = DistanceOf(Me);
        const bool bRescuer = Mine != MAX_int32 && !S.Encounter.Combatants.ContainsByPredicate([&](const FCombatantState& U)
        {
            if (U.Side != Me.Side || U.Id == Me.Id || !Conscious(U)) { return false; }
            const int32 Theirs = DistanceOf(U);
            return Theirs < Mine || (Theirs == Mine && U.Id < Me.Id);
        });
        const FIntPoint* Toward = nullptr;
        for (const FIntPoint& C : Moves)
        {
            if (!bRescuer || ToAlly.At(C) >= Mine) { continue; }
            if (!Toward || ToAlly.At(C) < ToAlly.At(*Toward) || (ToAlly.At(C) == ToAlly.At(*Toward) && (Steps.At(C) < Steps.At(*Toward) || (Steps.At(C) == Steps.At(*Toward) && CellBefore(C, *Toward))))) { Toward = &C; }
        }
        if (Toward) { return MakeMove(Id, *Toward); }
    }
    // 4. Heal the lowest low hero: Bonus Action heals first, then the larger heal, Potion last on ties.
    {
        struct FHeal { ECmd Command; int32 Target; double Fraction; bool bBonus; double Amount; bool bPotion; };
        TArray<FHeal> Heals;
        auto Consider = [&](ECmd Command, bool bBonus, bool bPotion)
        {
            for (int32 TargetId : B.LegalTargets(Id, Command))
            {
                const auto& T = Unit(TargetId);
                if (!Low(T)) { continue; }
                const FActionForecast F = B.Forecast(Id, Command, TargetId);
                Heals.Add({Command, TargetId, Fraction(T), bBonus, 0.5 * (F.MinAmount + F.MaxAmount), bPotion});
            }
        };
        for (const auto& A : Abilities) { if (A.Effect == EAbilityEffect::Healing) { Consider(A.Command, A.bBonus, false); } }
        Consider(ECmd::Potion, false, true);
        if (!Heals.IsEmpty())
        {
            Heals.Sort([](const FHeal& A, const FHeal& C)
            {
                if (A.Fraction != C.Fraction) { return A.Fraction < C.Fraction; }
                if (A.Target != C.Target) { return A.Target < C.Target; }
                if (A.bBonus != C.bBonus) { return A.bBonus; }
                if (A.Amount != C.Amount) { return A.Amount > C.Amount; }
                if (A.bPotion != C.bPotion) { return !A.bPotion; }
                return static_cast<uint8>(A.Command) < static_cast<uint8>(C.Command);
            });
            return Make(Heals[0].Command, Id, Heals[0].Target);
        }
    }
    // Protection on a threatened hero not already covered by at least as much (rules 5 and 10).
    auto Protect = [&](bool bBonus) -> FChoice
    {
        const FCombatantState* Best = nullptr;
        ECmd Command = ECmd::EndTurn;
        for (const auto& A : Abilities)
        {
            if (A.Effect != EAbilityEffect::Protect || A.bBonus != bBonus || Reserved(A)) { continue; }
            const int32 Amount = A.Amount > 0 ? A.Amount : 3;
            for (int32 TargetId : B.LegalTargets(Id, A.Command))
            {
                const auto& T = Unit(TargetId);
                if (!Threatened(S, T) || ExistingProtection(S, T) >= Amount) { continue; }
                if (!Best || Fraction(T) < Fraction(*Best) || (Fraction(T) == Fraction(*Best) && T.Id < Best->Id)) { Best = &T; Command = A.Command; }
            }
        }
        return Best ? Make(Command, Id, Best->Id) : FChoice();
    };
    // 5. Bonus Action protection.
    if (FChoice C = Protect(true); C.bValid) { return C; }
    // 6. Exchange places with a low ally caught in melee.
    for (const auto& A : Abilities)
    {
        if (A.Effect != EAbilityEffect::Swap || Low(Me) || Reserved(A)) { continue; }
        const FCombatantState* Best = nullptr;
        for (int32 TargetId : B.LegalTargets(Id, A.Command))
        {
            const auto& T = Unit(TargetId);
            if (T.Id == Id || !Low(T) || !AdjacentEnemy(S, T) || Fraction(Me) < Fraction(T) + 0.25) { continue; }
            if (!Best || Fraction(T) < Fraction(*Best) || (Fraction(T) == Fraction(*Best) && T.Id < Best->Id)) { Best = &T; }
        }
        if (Best) { return Make(A.Command, Id, Best->Id); }
    }
    // 7. Damage specials by guaranteed drop, then expected damage.
    const TArray<int32> AttackTargets = B.LegalTargets(Id, ECmd::Attack);
    {
        double BestAttack = 0.0;
        for (int32 TargetId : AttackTargets) { BestAttack = FMath::Max(BestAttack, Expected(B.Forecast(Id, ECmd::Attack, TargetId), EAbilityEffect::WeaponAttack)); }
        struct FOption { ECmd Command; int32 Target; bool bBonus; bool bDrop; double Value; int32 HP; };
        TArray<FOption> Options;
        for (const auto& A : Abilities)
        {
            if (!IsDamage(A.Effect)) { continue; }
            for (int32 TargetId : B.LegalTargets(Id, A.Command))
            {
                const auto& T = Unit(TargetId);
                const FActionForecast F = B.Forecast(Id, A.Command, TargetId);
                if (!F.bValid) { continue; }
                const bool bDrop = F.HitChancePercent >= 100 && F.MinAmount >= T.HP;
                if (Reserved(A) && !bDrop) { continue; }
                const double Value = Expected(F, A.Effect);
                if (!A.bBonus && !AttackTargets.IsEmpty() && !bDrop && Value <= BestAttack) { continue; }
                Options.Add({A.Command, TargetId, A.bBonus, bDrop, Value, T.HP});
            }
        }
        if (!Options.IsEmpty())
        {
            Options.Sort([](const FOption& A, const FOption& C)
            {
                if (A.bBonus != C.bBonus) { return A.bBonus; }
                if (A.bDrop != C.bDrop) { return A.bDrop; }
                if (A.Value != C.Value) { return A.Value > C.Value; }
                if (A.HP != C.HP) { return A.HP < C.HP; }
                if (A.Target != C.Target) { return A.Target < C.Target; }
                return static_cast<uint8>(A.Command) < static_cast<uint8>(C.Command);
            });
            return Make(Options[0].Command, Id, Options[0].Target);
        }
    }
    // 8. Basic attack on the lowest-HP legal target.
    if (!AttackTargets.IsEmpty())
    {
        int32 Best = AttackTargets[0];
        for (int32 TargetId : AttackTargets)
        { const auto& T = Unit(TargetId); const auto& Current = Unit(Best); if (T.HP < Current.HP || (T.HP == Current.HP && T.Id < Current.Id)) { Best = TargetId; } }
        return Make(ECmd::Attack, Id, Best);
    }
    // 9. Move: to a cell with a legal attack, otherwise closer to the enemy.
    if (S.bActionAvailable && !Moves.IsEmpty())
    {
        const FField Steps = StepsFrom(S, Me);
        const bool bRanged = Me.Range >= 2;
        TArray<FIntPoint> Cells = Moves.FilterByPredicate([&](FIntPoint C) { return NearestEnemy(S, Me, C) <= Me.Range; });
        Cells.Sort([&](FIntPoint A, FIntPoint C)
        {
            const int32 DA = NearestEnemy(S, Me, A), DC = NearestEnemy(S, Me, C);
            if (bRanged && DA != DC) { return DA > DC; }
            if (Steps.At(A) != Steps.At(C)) { return Steps.At(A) < Steps.At(C); }
            return CellBefore(A, C);
        });
        for (int32 I = 0; I < Cells.Num() && I < 16; ++I)
        {
            if (AfterMove(Run, Id, Cells[I], [Id](const FCombatBattle& After) { return !After.LegalTargets(Id, ECmd::Attack).IsEmpty(); }))
            { return MakeMove(Id, Cells[I]); }
        }
        if (const FIntPoint* Best = ApproachMove(S, Me, Moves, Steps)) { return MakeMove(Id, *Best); }
    }
    // 10. Action protection when nothing else used the Action.
    if (FChoice C = Protect(false); C.bValid) { return C; }
    return {};
}

/** True when ending now would skip a legal attack or a legal move closer to an enemy (contract section 7). */
bool PassiveEnd(const FCombatBattle& B, int32 Id)
{
    const FCombatSnapshot& S = B.Snapshot();
    const FCombatantState* Me = B.Find(Id);
    if (!Me || !S.bActionAvailable) { return false; }
    if (!B.LegalTargets(Id, ECmd::Attack).IsEmpty()) { return true; }
    return ApproachMove(S, *Me, B.LegalMoves(Id), StepsFrom(S, *Me)) != nullptr;
}

bool Legal(const FCombatBattle& B, const FCombatCommand& C)
{
    if (C.Type == ECmd::Move) { return B.LegalMoves(C.ActorId).Contains(C.Destination); }
    if (C.Type == ECmd::EndTurn) { return B.ActiveId() == C.ActorId; }
    return B.LegalTargets(C.ActorId, C.Type).Contains(C.TargetId);
}
}

namespace
{
using Policy::ECmd;

/** Credits a party command's effects to its actor and an enemy turn's to the heroes it hit. */
struct FTracker
{
    FRunStats& Run;
    TMap<int32, int32> HeroIndex;

    explicit FTracker(FRunStats& InRun) : Run(InRun) {}
    FHeroStats* Hero(int32 Id) { const int32* I = HeroIndex.Find(Id); return I ? &Run.Heroes[*I] : nullptr; }
    FEncounterStats& Encounter() { return Run.Encounters.Last(); }

    /** Life changes common to every step: downs and deaths (with cause). */
    void Lives(const FCombatSnapshot& Before, const FCombatSnapshot& After, EBattlePhase Phase)
    {
        for (int32 I = 0; I < After.Encounter.Combatants.Num() && I < Before.Encounter.Combatants.Num(); ++I)
        {
            const auto& Was = Before.Encounter.Combatants[I];
            const auto& Now = After.Encounter.Combatants[I];
            FHeroStats* H = Now.Side == ECombatSide::Party ? Hero(Now.Id) : nullptr;
            if (!H || Was.Life == Now.Life) { continue; }
            if (Now.Life == ECombatLife::Downed) { ++H->Downs; ++Encounter().Downs; }
            if (Now.Life == ECombatLife::Dead)
            {
                H->bSurvived = false; H->DeathStage = Encounter().Stage;
                H->DeathCause = Phase == EBattlePhase::Victory ? TEXT("victory") : Phase == EBattlePhase::Defeat ? TEXT("wipe") : TEXT("deadline");
                ++Encounter().Deaths; ++Run.Deaths;
            }
        }
    }

    void PartyCommand(const FCombatCommand& Command, const FCombatSnapshot& Before, const FCombatSnapshot& After, EBattlePhase Phase)
    {
        FHeroStats* Actor = Hero(Command.ActorId);
        ++Run.Commands;
        for (int32 I = 0; I < After.Encounter.Combatants.Num() && I < Before.Encounter.Combatants.Num(); ++I)
        {
            const auto& Was = Before.Encounter.Combatants[I];
            const auto& Now = After.Encounter.Combatants[I];
            if (Now.Side == ECombatSide::Enemy)
            {
                const int32 Lost = FMath::Max(0, Was.HP - Now.HP);
                Actor->Damage += Lost; Encounter().Damage += Lost; Run.Damage += Lost;
                if (Was.Life != ECombatLife::Dead && Now.Life == ECombatLife::Dead) { ++Actor->Kills; }
            }
            else
            {
                const int32 Gained = FMath::Max(0, Now.HP - Was.HP);
                Actor->Healing += Gained; Encounter().Healing += Gained; Run.Healing += Gained;
                if (Was.Life == ECombatLife::Downed && Now.Life == ECombatLife::Conscious) { ++Actor->Revives; ++Encounter().Revives; }
            }
        }
        if (Command.Type == ECmd::Potion) { ++Actor->PotionsUsed; ++Run.PotionsUsed; }
        for (auto& Special : Actor->Specials)
        { if (Special.Command == Command.Type) { ++Special.Uses; ++Encounter().SpecialsUsed; ++Run.SpecialsUsed; } }
        Lives(Before, After, Phase);
    }

    void EnemyTurn(const TArray<FCombatResult>& Results, const FCombatSnapshot& Before, const FCombatSnapshot& After, EBattlePhase Phase)
    {
        for (const auto& Result : Results)
        {
            if (!Result.bAccepted || Result.DamagePrevented <= 0) { continue; }
            const FCombatantState* Target = Before.Encounter.Combatants.FindByPredicate([&](const FCombatantState& U) { return U.Id == Result.TargetId; });
            if (!Target) { continue; }
            // Credit whoever supplied the protection that applied: the strongest guard, else the target's own Rage.
            int32 Source = Target->RageTurns > 0 ? Target->Id : INDEX_NONE;
            int32 Strongest = Target->RageTurns > 0 ? 1 : 0;
            for (const auto& U : Before.Encounter.Combatants)
            {
                const int32 Amount = U.GuardAmount > 0 ? U.GuardAmount : 3;
                if (U.GuardTargetId == Target->Id && Amount >= Strongest && (Source == INDEX_NONE || Source == Target->Id || Amount > Strongest)) { Source = U.Id; Strongest = Amount; }
            }
            if (FHeroStats* Giver = Hero(Source)) { Giver->Protection += Result.DamagePrevented; }
            Encounter().Protection += Result.DamagePrevented; Run.Protection += Result.DamagePrevented;
        }
        for (int32 I = 0; I < After.Encounter.Combatants.Num() && I < Before.Encounter.Combatants.Num(); ++I)
        {
            const auto& Was = Before.Encounter.Combatants[I];
            const auto& Now = After.Encounter.Combatants[I];
            if (FHeroStats* H = Now.Side == ECombatSide::Party ? Hero(Now.Id) : nullptr)
            {
                const int32 Lost = FMath::Max(0, Was.HP - Now.HP);
                H->DamageTaken += Lost; Encounter().DamageTaken += Lost; Run.DamageTaken += Lost;
            }
        }
        Lives(Before, After, Phase);
        int32 Standing = 0;
        for (const auto& U : After.Encounter.Combatants) { if (U.Side == ECombatSide::Party && U.Life != ECombatLife::Dead) { Standing += U.HP; } }
        Encounter().HpLow = FMath::Min(Encounter().HpLow, Standing);
    }
};

int32 PartyHP(const FCombatSnapshot& S, bool bMax)
{
    int32 Sum = 0;
    for (const auto& U : S.Encounter.Combatants) { if (U.Side == ECombatSide::Party && U.Life != ECombatLife::Dead) { Sum += bMax ? U.MaxHP : U.HP; } }
    return Sum;
}

const TCHAR* PhaseName(EBattlePhase Phase)
{
    switch (Phase)
    {
    case EBattlePhase::Fighting: return TEXT("Fighting");
    case EBattlePhase::Victory: return TEXT("Victory");
    case EBattlePhase::Defeat: return TEXT("Defeat");
    }
    return TEXT("Unknown");
}

/** Plays one party turn. Returns false on a stall. */
bool PartyTurn(FExpeditionRun& Run, FTracker& Tracker, int32 Id, FString& Stall, FString* Trace)
{
    FHeroStats* Hero = Tracker.Hero(Id);
    if (!Hero) { Stall = TEXT("active hero unknown"); return false; }
    ++Hero->Turns;
    const int32 Specials = Hero->Specials.Num();
    TArray<bool> Available; Available.Init(false, Specials);
    TArray<int32> FirstBlocker; FirstBlocker.Init(INDEX_NONE, Specials);
    bool bActed = false;
    for (int32 Step = 0; ; ++Step)
    {
        if (Run.Progress().Phase != EExpeditionPhase::Combat || Run.Battle().ActiveId() != Id) { break; }
        if (Step >= MaxSteps) { Stall = FString::Printf(TEXT("turn of hero %d reached %d steps"), Id, MaxSteps); return false; }
        for (int32 K = 0; K < Specials; ++K)
        {
            const FActionFacts Facts = FActionFactsQuery::For(Run.Battle(), Id, Hero->Specials[K].Command);
            Available[K] |= Facts.bAvailable;
            if (Step == 0 && !Facts.bAvailable && !Facts.Blockers.IsEmpty()) { FirstBlocker[K] = static_cast<int32>(Facts.Blockers[0].Reason); }
        }
        const Policy::FChoice Choice = Policy::Decide(Run, Id);
        FCombatCommand Command = Choice.Command;
        if (!Choice.bValid)
        {
            if (Policy::PassiveEnd(Run.Battle(), Id)) { ++Tracker.Run.PassiveEnds; }
            Command = {ECmd::EndTurn, Id};
        }
        else if (!Policy::Legal(Run.Battle(), Command))
        {
            ++Tracker.Run.IllegalProposals;
            Command = {ECmd::EndTurn, Id};
        }
        const FCombatSnapshot Before = Run.Battle().Snapshot();
        const FCombatResult Result = Run.Submit(Command);
        if (Trace) { *Trace += FString::Printf(TEXT("  hero %d %s -> (%d,%d) target %d: %s\n"), Id, CommandName(Command.Type), Command.Destination.X, Command.Destination.Y, Command.TargetId, *Result.Feedback); }
        if (!Result.bAccepted)
        {
            ++Tracker.Run.PolicyErrors;
            if (Command.Type == ECmd::EndTurn) { Stall = TEXT("End Turn rejected"); return false; }
            continue;
        }
        Tracker.PartyCommand(Command, Before, Run.Battle().Snapshot(), Run.Battle().Snapshot().Phase);
        if (Command.Type == ECmd::EndTurn) { break; }
        bActed = true;
    }
    if (!bActed) { ++Hero->IdleTurns; }
    for (int32 K = 0; K < Specials; ++K)
    {
        if (Available[K]) { ++Hero->Specials[K].AvailableTurns; }
        else if (FirstBlocker[K] != INDEX_NONE) { ++Hero->Specials[K].Blockers[FirstBlocker[K]]; }
    }
    return true;
}

FString Join(const TArray<FString>& Values, const TCHAR* Separator) { return FString::Join(Values, Separator); }

/** One text board: '#' blocked, digits party slots, letters enemies (lower case when Downed or dead). */
FString Board(const FCombatSnapshot& S)
{
    FString Out = FString::Printf(TEXT("round %d, active %d, move %d, action %d, bonus %d\n"), S.Round, S.TurnOrder.IsValidIndex(S.TurnIndex) ? S.TurnOrder[S.TurnIndex] : -1,
        S.MovementLeft, S.bActionAvailable ? 1 : 0, S.bBonusActionAvailable ? 1 : 0);
    for (int32 Y = 0; Y < S.Encounter.Height; ++Y)
    {
        for (int32 X = 0; X < S.Encounter.Width; ++X)
        {
            TCHAR Glyph = S.Encounter.BlockedCells.Contains(FIntPoint(X, Y)) ? TEXT('#') : TEXT('.');
            for (const auto& U : S.Encounter.Combatants)
            {
                if (U.Cell != FIntPoint(X, Y)) { continue; }
                Glyph = U.Side == ECombatSide::Party ? static_cast<TCHAR>(TEXT('0') + U.Id % 10) : static_cast<TCHAR>((U.Life == ECombatLife::Conscious ? TEXT('A') : TEXT('a')) + (U.Id - 20) % 26);
            }
            Out.AppendChar(Glyph);
        }
        Out += TEXT("\n");
    }
    for (const auto& U : S.Encounter.Combatants)
    {
        Out += FString::Printf(TEXT("  %d %s %s hp %d/%d at (%d,%d) range %d %s\n"), U.Id, *U.Name, *U.BuildId, U.HP, U.MaxHP, U.Cell.X, U.Cell.Y, U.Range,
            U.Life == ECombatLife::Conscious ? TEXT("") : U.Life == ECombatLife::Downed ? TEXT("DOWNED") : TEXT("DEAD"));
    }
    return Out;
}
}

FRunStats PlayRun(const FPartySpec& Party, int32 Seed, int32 RoutePattern, FExpeditionSnapshot* OutFinal, FString* Trace)
{
    FRunStats Stats;
    Stats.PartyId = Party.Id; Stats.Archetype = Party.Archetype; Stats.Builds = Party.Builds;
    Stats.Seed = Seed; Stats.RoutePattern = RoutePattern;
    FTracker Tracker(Stats);
    TArray<FPartyBuildSelection> Selection;
    for (int32 I = 0; I < Party.Builds.Num(); ++I)
    {
        FPartyBuildSelection Slot; Slot.Id = I + 1; Slot.Name = FString::Printf(TEXT("Hero %d"), I + 1); Slot.BuildId = Party.Builds[I];
        Selection.Add(Slot);
    }
    FExpeditionRun Run;
    FString Error;
    if (!Run.NewRun(Seed, Selection, Error, FString::Printf(TEXT("balance-%s-%d"), *Party.Id, Seed), ERuleset::Compact))
    {
        Stats.Outcome = TEXT("Stall"); Stats.StallReason = TEXT("NewRun rejected: ") + Error; return Stats;
    }
    for (int32 I = 0; I < Selection.Num(); ++I)
    {
        const FBuildDefinition* Build = FBuildCatalog::Find(Selection[I].BuildId);
        FHeroStats Hero;
        Hero.Slot = I + 1; Hero.Build = Build->Id; Hero.Class = Build->Profile.ClassId;
        Hero.KitCap = Build->KitCap; Hero.SlotCap = Build->SlotCap; Hero.WindCap = Build->WindCap;
        Hero.ManeuverCap = Build->ManeuverCap; Hero.RageCap = Build->RageCap;
        for (const ECombatCommandType Command : Build->Specials) { FSpecialStats Special; Special.Command = Command; Hero.Specials.Add(Special); }
        Tracker.HeroIndex.Add(I + 1, Stats.Heroes.Add(Hero));
    }
    FString Stall;
    // Each loop iteration advances the run; the bound only guards against a harness bug.
    for (int32 Guard = 0; Guard < 100000 && Stall.IsEmpty(); ++Guard)
    {
        const EExpeditionPhase Phase = Run.Progress().Phase;
        if (Phase == EExpeditionPhase::Victory || Phase == EExpeditionPhase::Defeat) { break; }
        if (Phase == EExpeditionPhase::Route)
        {
            const int32 Stage = Run.Progress().EncountersCompleted;
            const auto& Offers = Run.Progress().Offers;
            const int32 Choice = (RoutePattern >> Stage) & 1;
            const FExpeditionRoute Route = Offers[FMath::Min(Choice, Offers.Num() - 1)];
            if (!Run.ChooseRoute(Route.Id, Error)) { Stall = TEXT("route rejected: ") + Error; break; }
            FEncounterStats Encounter;
            Encounter.Stage = Stage + 1; Encounter.bDangerous = Route.bDangerous; Encounter.Recipe = Route.Recipe;
            Encounter.Width = Route.Width; Encounter.Height = Route.Height; Encounter.Enemies = Route.EnemyCount;
            const auto& S = Run.Battle().Snapshot();
            Encounter.HpStart = Encounter.HpLow = PartyHP(S, false); Encounter.HpMax = PartyHP(S, true);
            for (const auto& U : S.Encounter.Combatants) { if (U.Side == ECombatSide::Party && U.Life == ECombatLife::Conscious) { ++Encounter.ConsciousStart; } }
            Stats.Encounters.Add(Encounter);
            if (Trace) { *Trace += FString::Printf(TEXT("== fight %d: %s, recipe %d\n"), Stage + 1, *Route.Name, Route.Recipe) + Board(S); }
        }
        else if (Phase == EExpeditionPhase::Combat)
        {
            const FCombatBattle& Battle = Run.Battle();
            if (Battle.Snapshot().Round >= MaxRounds) { Stall = FString::Printf(TEXT("fight %d reached round %d"), Run.Progress().EncountersCompleted + 1, MaxRounds); break; }
            const int32 Id = Battle.ActiveId();
            const FCombatantState* Active = Battle.Find(Id);
            if (!Active) { Stall = TEXT("no active combatant"); break; }
            if (Trace && Battle.Snapshot().TurnIndex == 0) { *Trace += Board(Battle.Snapshot()); }
            if (Active->Side == ECombatSide::Enemy)
            {
                const FCombatSnapshot Before = Battle.Snapshot();
                const TArray<FCombatResult> Results = Run.PlayEnemyTurn();
                if (Results.IsEmpty()) { Stall = TEXT("enemy turn did nothing"); break; }
                Tracker.EnemyTurn(Results, Before, Run.Battle().Snapshot(), Run.Battle().Snapshot().Phase);
                if (Trace) { for (const auto& R : Results) { *Trace += FString::Printf(TEXT("  enemy %d %s: %s\n"), Id, CommandName(R.Type), *R.Feedback); } }
            }
            else if (!PartyTurn(Run, Tracker, Id, Stall, Trace)) { break; }
            if (Run.Progress().Phase != EExpeditionPhase::Combat)
            {
                FEncounterStats& Encounter = Stats.Encounters.Last();
                const auto& S = Run.Battle().Snapshot();
                Encounter.Outcome = PhaseName(S.Phase); Encounter.Rounds = S.Round; Encounter.HpEnd = PartyHP(S, false);
                Stats.Rounds += S.Round;
                if (S.Phase == EBattlePhase::Victory) { ++Stats.EncountersWon; }
            }
        }
        else if (Phase == EExpeditionPhase::Reward)
        {
            const FCombatantState* Best = nullptr;
            for (const auto& U : Run.Battle().Snapshot().Encounter.Combatants)
            {
                if (U.Side != ECombatSide::Party || U.Life != ECombatLife::Conscious) { continue; }
                if (!Best || U.WeaponRank < Best->WeaponRank || (U.WeaponRank == Best->WeaponRank && U.Id < Best->Id)) { Best = &U; }
            }
            if (!Best || !Run.ClaimReward(Best->Id, Error)) { Stall = TEXT("reward rejected: ") + Error; break; }
        }
        else if (Phase == EExpeditionPhase::Recovery)
        {
            if (!Run.Recover(true, Error)) { Stall = TEXT("recovery rejected: ") + Error; break; }
        }
    }
    const EExpeditionPhase End = Run.Progress().Phase;
    if (Stall.IsEmpty() && End != EExpeditionPhase::Victory && End != EExpeditionPhase::Defeat) { Stall = TEXT("run did not finish"); }
    Stats.Outcome = !Stall.IsEmpty() ? TEXT("Stall") : End == EExpeditionPhase::Victory ? TEXT("Victory") : TEXT("Defeat");
    Stats.StallReason = Stall;
    if (!Stall.IsEmpty() && !Stats.Encounters.IsEmpty() && Stats.Encounters.Last().Outcome.IsEmpty()) { Stats.Encounters.Last().Outcome = TEXT("Stall"); }
    for (auto& Hero : Stats.Heroes)
    {
        const FCombatantState* U = Run.Battle().Find(Hero.Slot);
        if (!U) { continue; }
        Hero.Level = U->Level;
        Hero.KitLeft = U->KitUses; Hero.SlotLeft = U->SpellSlots; Hero.WindLeft = U->SecondWindUses;
        Hero.ManeuverLeft = U->ManeuverUses; Hero.RageLeft = U->RageUses; Hero.PotionsLeft = U->Potions;
        if (U->Life != ECombatLife::Dead) { ++Stats.Survivors; }
        // A defeat leaves the last heroes Downed-then-dead in the final snapshot; record anyone the tracker missed.
        if (U->Life == ECombatLife::Dead && Hero.bSurvived) { Hero.bSurvived = false; Hero.DeathStage = Stats.Encounters.Num(); Hero.DeathCause = TEXT("wipe"); ++Stats.Deaths; }
    }
    if (OutFinal) { *OutFinal = Run.Snapshot(); }
    if (Trace) { *Trace += FString::Printf(TEXT("== %s %s\n"), *Stats.Outcome, *Stats.StallReason) + Board(Run.Battle().Snapshot()); }
    return Stats;
}

FString TraceRun(const FPartySpec& Party, int32 Seed, int32 RoutePattern)
{
    FString Trace;
    PlayRun(Party, Seed, RoutePattern, nullptr, &Trace);
    return Trace;
}

const FPartySpec* FindParty(const FString& Id)
{
    static const TArray<FPartySpec> All = []
    {
        TArray<FPartySpec> Parties = RepresentativeParties();
        for (int32 Pass = 0; Pass < RotationPasses; ++Pass) { Parties.Append(RotationParties(Pass)); }
        return Parties;
    }();
    return All.FindByPredicate([&Id](const FPartySpec& Party) { return Party.Id == Id; });
}

TArray<FRunStats> PlayGroup(const TArray<FPartySpec>& Parties, int32 Seeds)
{
    TArray<FRunStats> Runs;
    for (const auto& Party : Parties)
    {
        for (int32 I = 0; I < Seeds; ++I) { Runs.Add(PlayRun(Party, BaseSeed + I, I)); }
    }
    return Runs;
}

TArray<FPartySpec> RepresentativeParties()
{
    auto Four = [](const TCHAR* Build) { return TArray<FString>{Build, Build, Build, Build}; };
    return {
        {TEXT("familiar"), TEXT("mixed"), {TEXT("fighter.champion"), TEXT("rogue.thief"), TEXT("cleric.life"), TEXT("wizard.evocation")}},
        {TEXT("melee"), TEXT("melee"), {TEXT("barbarian.berserker"), TEXT("fighter.battle-master"), TEXT("paladin.devotion"), TEXT("monk.open-hand")}},
        {TEXT("ranged"), TEXT("ranged"), {TEXT("ranger.hunter"), TEXT("wizard.evocation"), TEXT("warlock.fiend"), TEXT("sorcerer.draconic-bloodline")}},
        {TEXT("support-poor"), TEXT("support-poor"), {TEXT("rogue.assassin"), TEXT("barbarian.beast"), TEXT("sorcerer.wild-magic"), TEXT("ranger.fey-wanderer")}},
        {TEXT("support-heavy"), TEXT("support-heavy"), {TEXT("paladin.devotion"), TEXT("cleric.life"), TEXT("druid.stars"), TEXT("bard.lore")}},
        {TEXT("mixed"), TEXT("mixed"), {TEXT("paladin.watchers"), TEXT("ranger.beast-master"), TEXT("cleric.peace"), TEXT("artificer.artillerist")}},
        {TEXT("dup-fighter"), TEXT("duplicate"), Four(TEXT("fighter.champion"))},
        {TEXT("dup-cleric"), TEXT("duplicate"), Four(TEXT("cleric.life"))},
        {TEXT("dup-wizard"), TEXT("duplicate"), Four(TEXT("wizard.evocation"))},
    };
}

TArray<FPartySpec> RotationParties(int32 Pass)
{
    static const int32 Multipliers[RotationPasses] = {11, 13, 17, 19, 23};
    const auto& Kits = FBuildCatalog::All();
    const int32 Count = Kits.Num();
    TArray<FPartySpec> Parties;
    if (Pass < 0 || Pass >= RotationPasses || Count == 0) { return Parties; }
    auto At = [&](int32 Position) -> const FString& { return Kits[((Position % Count) * Multipliers[Pass] + 3 * Pass) % Count].Id; };
    for (int32 R = 0; R * 4 < Count; ++R)
    {
        FPartySpec Party;
        Party.Id = FString::Printf(TEXT("rot%d-%02d"), Pass + 1, R + 1);
        Party.Archetype = TEXT("rotation");
        for (int32 K = 0; K < 4; ++K) { Party.Builds.Add(At(R * 4 + K)); }
        Parties.Add(Party);
    }
    return Parties;
}

const TCHAR* CommandName(ECombatCommandType Command)
{
    switch (Command)
    {
    case ECmd::Move: return TEXT("Move");
    case ECmd::Attack: return TEXT("Attack");
    case ECmd::EndTurn: return TEXT("EndTurn");
    case ECmd::SecondWind: return TEXT("SecondWind");
    case ECmd::CureWounds: return TEXT("CureWounds");
    case ECmd::MagicMissile: return TEXT("MagicMissile");
    case ECmd::Rescue: return TEXT("Rescue");
    case ECmd::Potion: return TEXT("Potion");
    case ECmd::PrecisionStrike: return TEXT("PrecisionStrike");
    case ECmd::Special1: return TEXT("Special1");
    case ECmd::Special2: return TEXT("Special2");
    case ECmd::Grapple: return TEXT("Grapple");
    case ECmd::Shove: return TEXT("Shove");
    case ECmd::Escape: return TEXT("Escape");
    }
    return TEXT("Unknown");
}

const TCHAR* BlockerName(EActionBlocker Blocker)
{
    switch (Blocker)
    {
    case EActionBlocker::BattleInactive: return TEXT("BattleInactive");
    case EActionBlocker::NotYourTurn: return TEXT("NotYourTurn");
    case EActionBlocker::ActorDown: return TEXT("ActorDown");
    case EActionBlocker::NotInKit: return TEXT("NotInKit");
    case EActionBlocker::ActionSpent: return TEXT("ActionSpent");
    case EActionBlocker::BonusSpent: return TEXT("BonusSpent");
    case EActionBlocker::NoMovementLeft: return TEXT("NoMovementLeft");
    case EActionBlocker::NoOpenCell: return TEXT("NoOpenCell");
    case EActionBlocker::ResourceShort: return TEXT("ResourceShort");
    case EActionBlocker::StanceRequired: return TEXT("StanceRequired");
    case EActionBlocker::StanceBlocks: return TEXT("StanceBlocks");
    case EActionBlocker::NoWoundedTarget: return TEXT("NoWoundedTarget");
    case EActionBlocker::NoDownedTarget: return TEXT("NoDownedTarget");
    case EActionBlocker::OutOfRange: return TEXT("OutOfRange");
    case EActionBlocker::SightBlocked: return TEXT("SightBlocked");
    case EActionBlocker::NoEligibleTarget: return TEXT("NoEligibleTarget");
    case EActionBlocker::Count: break;
    }
    return TEXT("Unknown");
}

FString RunsCsv(const TArray<FRunStats>& Runs)
{
    FString Out = TEXT("party,archetype,builds,seed,route_pattern,outcome,stall_reason,encounters_won,rounds,deaths,survivors,damage,damage_taken,healing,protection,specials_used,potions_used,commands,policy_errors,illegal_proposals,passive_ends\n");
    for (const auto& R : Runs)
    {
        Out += FString::Printf(TEXT("%s,%s,%s,%d,%d,%s,%s,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n"), *R.PartyId, *R.Archetype, *Join(R.Builds, TEXT(";")),
            R.Seed, R.RoutePattern, *R.Outcome, *R.StallReason.Replace(TEXT(","), TEXT(" ")), R.EncountersWon, R.Rounds, R.Deaths, R.Survivors, R.Damage, R.DamageTaken,
            R.Healing, R.Protection, R.SpecialsUsed, R.PotionsUsed, R.Commands, R.PolicyErrors, R.IllegalProposals, R.PassiveEnds);
    }
    return Out;
}

FString EncountersCsv(const TArray<FRunStats>& Runs)
{
    FString Out = TEXT("party,seed,stage,route,recipe,width,height,enemies,outcome,rounds,conscious_start,hp_start,hp_low,hp_end,hp_max,damage,damage_taken,healing,protection,downs,deaths,revives,specials_used\n");
    for (const auto& R : Runs)
    {
        for (const auto& E : R.Encounters)
        {
            Out += FString::Printf(TEXT("%s,%d,%d,%s,%d,%d,%d,%d,%s,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n"), *R.PartyId, R.Seed, E.Stage,
                E.bDangerous ? TEXT("dangerous") : TEXT("guarded"), E.Recipe, E.Width, E.Height, E.Enemies, *E.Outcome, E.Rounds, E.ConsciousStart,
                E.HpStart, E.HpLow, E.HpEnd, E.HpMax, E.Damage, E.DamageTaken, E.Healing, E.Protection, E.Downs, E.Deaths, E.Revives, E.SpecialsUsed);
        }
    }
    return Out;
}

FString HeroesCsv(const TArray<FRunStats>& Runs)
{
    FString Out = TEXT("party,archetype,seed,outcome,slot,build,class,survived,death_stage,death_cause,level,turns,idle_turns,damage,kills,healing,protection,damage_taken,downs,revives,potions_used");
    for (int32 K = 1; K <= 2; ++K) { Out += FString::Printf(TEXT(",s%d_command,s%d_available_turns,s%d_uses,s%d_top_blocker,s%d_top_blocker_turns"), K, K, K, K, K); }
    Out += TEXT(",kit_left,kit_cap,slot_left,slot_cap,wind_left,wind_cap,maneuver_left,maneuver_cap,rage_left,rage_cap,potions_left\n");
    for (const auto& R : Runs)
    {
        for (const auto& H : R.Heroes)
        {
            Out += FString::Printf(TEXT("%s,%s,%d,%s,%d,%s,%s,%d,%d,%s,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d"), *R.PartyId, *R.Archetype, R.Seed, *R.Outcome, H.Slot, *H.Build, *H.Class,
                H.bSurvived ? 1 : 0, H.DeathStage, *H.DeathCause, H.Level, H.Turns, H.IdleTurns, H.Damage, H.Kills, H.Healing, H.Protection, H.DamageTaken, H.Downs, H.Revives, H.PotionsUsed);
            for (int32 K = 0; K < 2; ++K)
            {
                if (!H.Specials.IsValidIndex(K)) { Out += TEXT(",,0,0,,0"); continue; }
                const auto& Special = H.Specials[K];
                int32 Top = INDEX_NONE;
                for (int32 B = 0; B < static_cast<int32>(EActionBlocker::Count); ++B) { if (Special.Blockers[B] > 0 && (Top == INDEX_NONE || Special.Blockers[B] > Special.Blockers[Top])) { Top = B; } }
                Out += FString::Printf(TEXT(",%s,%d,%d,%s,%d"), CommandName(Special.Command), Special.AvailableTurns, Special.Uses,
                    Top == INDEX_NONE ? TEXT("") : BlockerName(static_cast<EActionBlocker>(Top)), Top == INDEX_NONE ? 0 : Special.Blockers[Top]);
            }
            Out += FString::Printf(TEXT(",%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n"), H.KitLeft, H.KitCap, H.SlotLeft, H.SlotCap, H.WindLeft, H.WindCap,
                H.ManeuverLeft, H.ManeuverCap, H.RageLeft, H.RageCap, H.PotionsLeft);
        }
    }
    return Out;
}

FString GroupFolder(const FString& Group)
{
    static const FString Stamp = FDateTime::UtcNow().ToString(TEXT("%Y%m%dT%H%M%SZ"));
    FString Root = FPlatformMisc::GetEnvironmentVariable(TEXT("DND_BALANCE_OUT"));
    if (Root.IsEmpty()) { Root = FPaths::Combine(TEXT(".local"), TEXT("balance"), Stamp); }
    if (FPaths::IsRelative(Root)) { Root = FPaths::Combine(FPaths::ProjectDir(), Root); }
    return FPaths::ConvertRelativePathToFull(FPaths::Combine(Root, Group));
}

bool WriteGroup(const FString& Folder, const FString& Group, const TArray<FPartySpec>& Parties, const TArray<FRunStats>& Runs, double Seconds, FString& Error)
{
    if (!IFileManager::Get().MakeDirectory(*Folder, true)) { Error = TEXT("Cannot create ") + Folder; return false; }
    const TSharedRef<FJsonObject> Meta = MakeShared<FJsonObject>();
    Meta->SetStringField(TEXT("group"), Group);
    Meta->SetNumberField(TEXT("policy_version"), PolicyVersion);
    Meta->SetStringField(TEXT("ruleset"), TEXT("Compact"));
    Meta->SetNumberField(TEXT("catalog_revision"), FBuildCatalog::Revision);
    Meta->SetNumberField(TEXT("base_seed"), BaseSeed);
    Meta->SetNumberField(TEXT("seeds"), Parties.IsEmpty() ? 0 : Runs.Num() / Parties.Num());
    Meta->SetNumberField(TEXT("runs"), Runs.Num());
    Meta->SetNumberField(TEXT("max_rounds"), MaxRounds);
    Meta->SetNumberField(TEXT("max_steps"), MaxSteps);
    Meta->SetNumberField(TEXT("seconds"), Seconds);
    TArray<TSharedPtr<FJsonValue>> List;
    for (const auto& Party : Parties)
    {
        const TSharedRef<FJsonObject> Entry = MakeShared<FJsonObject>();
        Entry->SetStringField(TEXT("id"), Party.Id);
        Entry->SetStringField(TEXT("archetype"), Party.Archetype);
        TArray<TSharedPtr<FJsonValue>> Builds;
        for (const auto& Build : Party.Builds) { Builds.Add(MakeShared<FJsonValueString>(Build)); }
        Entry->SetArrayField(TEXT("builds"), Builds);
        List.Add(MakeShared<FJsonValueObject>(Entry));
    }
    Meta->SetArrayField(TEXT("parties"), List);
    FString Json;
    const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Json);
    FJsonSerializer::Serialize(Meta, Writer);
    const TPair<const TCHAR*, FString> Files[] = {
        {TEXT("runs.csv"), RunsCsv(Runs)}, {TEXT("encounters.csv"), EncountersCsv(Runs)}, {TEXT("heroes.csv"), HeroesCsv(Runs)}, {TEXT("group.json"), Json}};
    for (const auto& File : Files)
    {
        if (!FFileHelper::SaveStringToFile(File.Value, *FPaths::Combine(Folder, File.Key), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
        { Error = TEXT("Cannot write ") + FPaths::Combine(Folder, File.Key); return false; }
    }
    return true;
}
}
#endif
