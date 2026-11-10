#include "Combat/CombatBattle.h"
#include "Party/BuildCatalog.h"
#include "Rules/AbilityRules.h"
#include "Rules/CheckRules.h"
#include "Rules/ConcentrationRules.h"
#include "Rules/CoverRules.h"
#include "Rules/DamageRules.h"
#include "Rules/DeathSaveRules.h"
#include "Rules/GrappleRules.h"
#include "Rules/ClassFeatureRules.h"
#include "Rules/CantripRules.h"
#include "Rules/MonsterRules.h"
#include "Rules/MovementRules.h"
#include "Rules/StandardActionRules.h"
#include "Rules/OpportunityRules.h"
#include "Rules/ProficiencyRules.h"
#include "Rules/SpellRules.h"
#include "Rules/AdvancementRules.h"
#include "Rules/SpellcastingRules.h"
#include "Rules/StealthRules.h"

namespace
{
int32 GridDistance(FIntPoint A, FIntPoint B)
{
    return FOpportunityRules::GridDistance(A, B);
}
FAbilityScores AbilityScoresOf(const FCombatantState& Unit)
{
    if (Unit.AdventurerRole != EAdventurerRole::None) { return FAbilityRules::PrebuiltScores(Unit.AdventurerRole); }
    return FAbilityRules::EnemyScores(Unit.Role);
}
int32 DexterityModifier(const FCombatantState& Unit)
{
    return FAbilityRules::Modifier(AbilityScoresOf(Unit).Dexterity);
}
using namespace BuildRules;
constexpr int32 PotionDice = 2;
constexpr int32 PotionDie = 4;
constexpr int32 PotionBonus = 2;
constexpr int32 SacredFlameDie = 8; // SRD Sacred Flame, 1d8 radiant at character levels 1-4
/** Full saving throw bonus: ability modifier plus proficiency where proficient. */
int32 SaveBonusOf(const FCombatantState& Unit, EAbility Ability)
{
    FCharacterRules Character = Unit.AdventurerRole != EAdventurerRole::None
        ? FProficiencyRules::PrebuiltCharacter(Unit.AdventurerRole, Unit.Level)
        : FProficiencyRules::EnemyCharacter(Unit.Role);
    // Enemy rules are built at level 1; proficiency follows the combatant's own level.
    Character.Level = Unit.Level;
    return Character.SaveBonus(Ability);
}
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
    if (Encounter.Ruleset != ERuleset::Compact && Encounter.Ruleset != ERuleset::SRD) { return Reject(TEXT("Invalid encounter ruleset.")); }
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
            Unit.Size < static_cast<int32>(ECreatureSize::Tiny) || Unit.Size > static_cast<int32>(ECreatureSize::Gargantuan) ||
            Unit.TemporaryHP < 0 || Unit.DeathSaveSuccesses < 0 || Unit.DeathSaveSuccesses > 3 || Unit.DeathSaveFailures < 0 || Unit.DeathSaveFailures > 3 ||
            Unit.MaxHP < 1 || Unit.MaxHP > 1000 || Unit.HP < 0 || Unit.HP > Unit.MaxHP ||
            Unit.ArmorClass < 1 || Unit.ArmorClass > 40 || Unit.AttackBonus < -10 || Unit.AttackBonus > 30 ||
            Unit.DamageDie < 1 || Unit.DamageDie > 100 || Unit.DamageBonus < 0 || Unit.DamageBonus > 100 ||
            Unit.Range < 1 || Unit.Range > 32 || Unit.Speed < 0 || Unit.Speed > 32 ||
            Unit.InitiativeBonus < -10 || Unit.InitiativeBonus > 30 ||
            static_cast<uint8>(Unit.AdventurerRole) > static_cast<uint8>(EAdventurerRole::Artificer) ||
            static_cast<uint8>(Unit.Life) > static_cast<uint8>(ECombatLife::Dead) ||
            (Unit.Life == ECombatLife::Conscious ? Unit.HP <= 0 || Unit.RescueTurnsLeft != 0 : Unit.HP != 0) ||
            (Unit.Life == ECombatLife::Downed && (Unit.Side != ECombatSide::Party ||
                (Encounter.Ruleset == ERuleset::SRD
                    // SRD has no rescue countdown. Dying holds 0-2 of each count; Stable is 3 successes and no failures.
                    ? (Unit.RescueTurnsLeft != 0 || Unit.DeathSaveFailures > 2 || (Unit.DeathSaveSuccesses == 3 && Unit.DeathSaveFailures != 0))
                    : (Unit.RescueTurnsLeft < 1 || Unit.RescueTurnsLeft > 3)))) ||
            // SRD death-save counts exist only while Downed.
            (Encounter.Ruleset == ERuleset::SRD && Unit.Life != ECombatLife::Downed && (Unit.DeathSaveSuccesses != 0 || Unit.DeathSaveFailures != 0)) ||
            (Unit.Life == ECombatLife::Dead && Unit.RescueTurnsLeft != 0) ||
            Unit.SecondWindUses < 0 || Unit.SecondWindUses > 2 || Unit.ActionSurgeUses < 0 || Unit.ActionSurgeUses > 2 || Unit.SpellSlots < 0 || Unit.SpellSlots > 2 || Unit.SpellSlot1 < 0 || Unit.SpellSlot1 > 4 || Unit.SpellSlot2 < 0 || Unit.SpellSlot2 > 3 || Unit.SpellSlot3 < 0 || Unit.SpellSlot3 > 2 ||
            Unit.Potions < 0 || Unit.Potions > 9 || Unit.WeaponRank < 0 || Unit.WeaponRank > 3 ||
            (Unit.Side != ECombatSide::Party && Unit.Side != ECombatSide::Enemy))
        {
            return Reject(TEXT("Invalid combatant stats, ID, or occupied spawn."));
        }
        const auto* Build = FBuildCatalog::Find(Unit.BuildId);
        if (Build && !FBuildCatalog::ValidResources(Unit))
        { return Reject(TEXT("Resources do not belong to the selected build.")); }
        const bool bBarbarian = Unit.AdventurerRole == EAdventurerRole::Barbarian && Unit.Side == ECombatSide::Party && Build && Build->Profile.AdventurerRole == EAdventurerRole::Barbarian;
        if (Build && Build->KitCap > 0 && (Unit.AdventurerRole != Build->Profile.AdventurerRole || Unit.Side != ECombatSide::Party))
        { return Reject(TEXT("Kit resources require their compatible party build.")); }
        if (static_cast<uint8>(Unit.AdventurerRole) >= static_cast<uint8>(EAdventurerRole::Monk) &&
            (!Build || Build->Profile.AdventurerRole != Unit.AdventurerRole || Unit.Side != ECombatSide::Party))
        { return Reject(TEXT("Martial role requires a compatible build.")); }
        if (Unit.KitUses < 0 || Unit.KitUses > (Build ? Build->KitCap : 0) || Unit.GuardTurns < 0 || Unit.GuardTurns > 1 || Unit.GuardAmount < 0 || Unit.GuardAmount > 4 ||
            ((Unit.GuardTurns > 0) != (Unit.GuardAmount > 0)) ||
            (Unit.GuardTurns > 0 && (Unit.Life != ECombatLife::Conscious || Unit.GuardTargetId == INDEX_NONE || !Build || Build->KitCap == 0 || Unit.KitUses == Build->KitCap)))
        { return Reject(TEXT("Invalid kit resource or protection duration.")); }
        if (Unit.GuardTurns > 0)
        {
            bool Compatible = false;
            for (const auto& A : FBuildCatalog::Abilities(Unit))
            { Compatible |= A.Effect == EAbilityEffect::Protect && A.Amount == Unit.GuardAmount && (A.Target == EAbilityTarget::Self ? Unit.GuardTargetId == Unit.Id : Unit.GuardTargetId != Unit.Id); }
            if (!Compatible) { return Reject(TEXT("Protection does not belong to build.")); }
        }
        if (Unit.AdventurerRole == EAdventurerRole::Barbarian && !bBarbarian) { return Reject(TEXT("Barbarian requires a compatible build.")); }
        if (Unit.RageUses < 0 || Unit.RageUses > (bBarbarian ? Build->RageCap : 0) || Unit.RageTurns < 0 || Unit.RageTurns > (bBarbarian ? 2 : 0) ||
            Unit.LastSurge < 0 || Unit.LastSurge > FBuildCatalog::SurgeCap(Unit) ||
            (Unit.RageTurns > 0 && (Unit.RageUses == Build->RageCap || Unit.Life != ECombatLife::Conscious)) ||
            (Unit.LastSurge > 0 && Unit.Life != ECombatLife::Conscious) ||
            (Unit.GuardTargetId != INDEX_NONE && Unit.GuardTurns == 0 && (Unit.BuildId != TEXT("barbarian.totem-warrior") || Unit.RageTurns == 0)))
        { return Reject(TEXT("Invalid owned ability state.")); }
        if (Unit.GuardTargetId != INDEX_NONE)
        {
            const auto* Target = Encounter.Combatants.FindByPredicate([&](const auto& T) { return T.Id == Unit.GuardTargetId; });
            if (!Target || (Target->Id == Unit.Id && Unit.GuardTurns == 0) || Target->Side != Unit.Side || Target->Life != ECombatLife::Conscious) { return Reject(TEXT("Invalid protected ally.")); }
        }
        if (Unit.GrapplerId != INDEX_NONE && (Unit.GrapplerId == Unit.Id ||
            !Encounter.Combatants.ContainsByPredicate([&](const auto& U) { return U.Id == Unit.GrapplerId; })))
        { return Reject(TEXT("Invalid grappler reference.")); }
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
        // SRD: initiative is DEX only (Compact Rogue keeps its historical +1 via InitiativeBonus).
        if (IsSrd())
        {
            Unit.InitiativeBonus = DexterityModifier(Unit);
            Unit.bReactionAvailable = true;
            // Hydrate per-level slots when unset (fresh fixtures / schema-5 migrates).
            if (Unit.Side == ECombatSide::Party && Unit.SpellSlot1 == 0 && Unit.SpellSlot2 == 0 && Unit.SpellSlot3 == 0
                && FSpellcastingRules::IsSpellcaster(Unit.AdventurerRole))
            {
                FillSrdSpellSlots(Unit);
            }
            // Exact SRD role names (Bandit/Thug/...) get stat blocks; Compact expedition aliases stay test-tunable.
            if (Unit.Side == ECombatSide::Enemy)
            {
                if (const FMonsterStatBlock* Block = FMonsterRules::Find(Unit.Role))
                {
                    Unit.ArmorClass = Block->ArmorClass;
                    // SRD: roll HP per creature from a unit-local seed so battle RandomState (initiative) stays stable.
                    {
                        FRandomStream HpRng(Seed ^ (Unit.Id * 0x9E3779B9));
                        Unit.MaxHP = Unit.HP = FMonsterRules::RollHitPoints(*Block, HpRng);
                    }
                    Unit.AttackBonus = FMonsterRules::AttackBonus(*Block);
                    Unit.DamageDie = Block->DamageDieSides;
                    Unit.DamageBonus = FMonsterRules::DamageBonus(*Block);
                    Unit.Range = Block->Range;
                }
            }
        }
        if (IsSrd() && State.Encounter.bAmbush && Unit.Side == ECombatSide::Enemy && Unit.Life == ECombatLife::Conscious)
        {
            const int32 PassivePerception = FStealthRules::PassiveScore(FAbilityRules::Modifier(AbilityScoresOf(Unit).Wisdom), ERollMode::Normal);
            Unit.bSurprised = FStealthRules::IsSurprised(State.Encounter.AmbushStealthTotals, PassivePerception);
        }
        const ERollMode InitMode = (IsSrd() && Unit.bSurprised) ? FStealthRules::InitiativeMode(true) : ERollMode::Normal;
        if (InitMode == ERollMode::Disadvantage)
        {
            const int32 A = Roll(20), B = Roll(20);
            Unit.Initiative = FMath::Min(A, B) + Unit.InitiativeBonus;
        }
        else
        {
            Unit.Initiative = Roll(20) + Unit.InitiativeBonus;
        }
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
        DownedTurn(*First);
        // SRD: a natural 20 death save revives the creature, which then takes this turn.
        if (First->Life != ECombatLife::Conscious) { AdvanceTurn(); }
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
    // What the action does. What it costs and spends comes from FActionFactsQuery (#58), never from here.
    switch (Action)
    {
    case ECombatCommandType::PrecisionStrike:
        return TEXT("Enemy in reach; an attack with +2 to hit and +2 damage");
    case ECombatCommandType::SecondWind:
        return FString::Printf(TEXT("Self; heal d%d+%d"), WindDie, FighterLevel);
    case ECombatCommandType::ActionSurge:
        return TEXT("Restore your Action (Fighter 2+)");
    case ECombatCommandType::Dash:
        return TEXT("Gain Speed again this turn");
    case ECombatCommandType::Disengage:
        return TEXT("Leave reach without opportunity attacks");
    case ECombatCommandType::CureWounds:
        return FString::Printf(TEXT("Self or ally, %d cell; heal %dd%d+%d"), TouchRange, CureDice, CureDie, HealingModifier);
    case ECombatCommandType::MagicMissile:
        return FString::Printf(TEXT("Enemy, %d cells + sight; %dd%d+%d, never misses"), MissileRange, MissileDarts, MissileDie, MissileDarts * DartBonus);
    case ECombatCommandType::Rescue:
        return FString::Printf(TEXT("Downed ally, %d cell; restore 1 HP"), TouchRange);
    case ECombatCommandType::Potion:
        return FString::Printf(TEXT("Self or ally, %d cell; heal %dd%d+%d"), TouchRange, PotionDice, PotionDie, PotionBonus);
    default:
        return FString::Printf(TEXT("Attack an enemy in reach, or move to a marked tile. Rogue: +d%d with a conscious ally by the target; Wizard: Fire Bolt"), SneakDie);
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
    return Target.Side != Attacker.Side && (Target.HP > 0 || (IsSrd() && Target.Life == ECombatLife::Downed)) && GridDistance(Origin, Target.Cell) <= Attacker.Range &&
        HasLineOfSight(Origin, Target.Cell);
}

TArray<int32> FCombatBattle::LegalTargets(int32 Id, ECombatCommandType Action) const
{
    TArray<int32> Result;
    const FCombatantState* Unit = Find(Id);
    if (!bStarted || State.Phase != EBattlePhase::Fighting || ActiveId() != Id || !Unit || Unit->Life != ECombatLife::Conscious) { return Result; }
    if (Action == ECombatCommandType::Special1 || Action == ECombatCommandType::Special2 || (Action == ECombatCommandType::CureWounds && !Unit->BuildId.IsEmpty()))
    {
        for (const auto& A : FBuildCatalog::Abilities(*Unit))
        {
            if (A.Command != Action || !A.bResourceAvailable || !(A.bBonus ? State.bBonusActionAvailable : State.bActionAvailable)) { continue; }
            for (const auto& T : State.Encounter.Combatants)
            {
                const bool Kind = A.Target == EAbilityTarget::Self ? T.Id == Id : A.Target == EAbilityTarget::Enemy ? T.Side != Unit->Side : T.Side == Unit->Side && (A.Target == EAbilityTarget::Friendly || T.Id != Id);
                if (Kind && (A.Effect != EAbilityEffect::Healing || T.HP < T.MaxHP) && T.Life == ECombatLife::Conscious && GridDistance(Unit->Cell, T.Cell) <= A.Range && HasLineOfSight(Unit->Cell, T.Cell)) { Result.Add(T.Id); }
            }
        }
        return Result;
    }
    if (Action == ECombatCommandType::SecondWind)
    {
        if (FBuildCatalog::HasSpecial(*Unit, Action) && Unit->SecondWindUses > 0 && State.bBonusActionAvailable && Unit->HP < Unit->MaxHP) { Result.Add(Id); }
        return Result;
    }
    if (IsSrd() && Action == ECombatCommandType::ActionSurge)
    {
        if (Unit->AdventurerRole == EAdventurerRole::Fighter && Unit->ActionSurgeUses > 0 && FClassFeatureRules::ActionSurgeUses(Unit->Level) > 0)
        { Result.Add(Id); }
        return Result;
    }
    if (IsSrd() && Action == ECombatCommandType::CastSpell)
    {
        // Targets depend on SpellId carried on the command; LegalTargets without spell returns self for buff probes.
        Result.Add(Id);
        for (const FCombatantState& Target : State.Encounter.Combatants)
        {
            if (Target.Id == Id) { continue; }
            if (Target.Life == ECombatLife::Conscious && HasLineOfSight(Unit->Cell, Target.Cell))
            {
                Result.Add(Target.Id);
            }
        }
        return Result;
    }
    if (IsSrd() && (Action == ECombatCommandType::Dash || Action == ECombatCommandType::Disengage))
    {
        const EStandardAction Std = Action == ECombatCommandType::Dash ? EStandardAction::Dash : EStandardAction::Disengage;
        const bool bBonus = FStandardActionRules::CanTakeAsBonusAction(Std, Unit->AdventurerRole, Unit->Level) && State.bBonusActionAvailable;
        const bool bAction = State.bActionAvailable;
        if (bBonus || bAction) { Result.Add(Id); }
        return Result;
    }
    // SRD Extra Attack / Multiattack: further swings after the Attack action was spent.
    if (IsSrd() && (Action == ECombatCommandType::Attack || Action == ECombatCommandType::PrecisionStrike)
        && !State.bActionAvailable && State.AttacksLeft > 0)
    {
        for (const FCombatantState& Target : State.Encounter.Combatants)
        {
            if (Action == ECombatCommandType::PrecisionStrike && (!FBuildCatalog::HasSpecial(*Unit, Action) || Unit->ManeuverUses <= 0)) { break; }
            if (CanTargetFrom(*Unit, Unit->Cell, Target)) { Result.Add(Target.Id); }
        }
        return Result;
    }
    if (!State.bActionAvailable) { return Result; }
    if (Action == ECombatCommandType::PrecisionStrike && (!FBuildCatalog::HasSpecial(*Unit, Action) || Unit->ManeuverUses <= 0)) { return Result; }
    if (Action == ECombatCommandType::CureWounds && (!FBuildCatalog::HasSpecial(*Unit, Action) || (IsSrd() ? Unit->SpellSlot1 <= 0 : Unit->SpellSlots <= 0))) { return Result; }
    if (Action == ECombatCommandType::MagicMissile && (!FBuildCatalog::HasSpecial(*Unit, Action) || (IsSrd() ? Unit->SpellSlot1 <= 0 : Unit->SpellSlots <= 0))) { return Result; }
    if (Action == ECombatCommandType::MagicMissile) { if (const auto* B = FBuildCatalog::Find(Unit->BuildId); B && Unit->KitUses < B->MissileKitCost) { return Result; } }
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
                (Action != ECombatCommandType::Rescue || Target.Life == ECombatLife::Downed) &&
                (Action != ECombatCommandType::CureWounds || Target.Life == ECombatLife::Conscious);
        }
        else if (IsSrd() && (Action == ECombatCommandType::Grapple || Action == ECombatCommandType::Shove))
        {
            bLegal = Target.Side != Unit->Side && Target.Life == ECombatLife::Conscious && GridDistance(Unit->Cell, Target.Cell) <= 1
                && FGrappleRules::SizeAllows(static_cast<ECreatureSize>(Unit->Size), static_cast<ECreatureSize>(Target.Size));
        }
        else if (IsSrd() && Action == ECombatCommandType::Escape)
        {
            bLegal = Unit->GrapplerId != INDEX_NONE && Target.Id == Unit->GrapplerId && Target.Life == ECombatLife::Conscious;
        }
        if (bLegal) { Result.Add(Target.Id); }
    }
    return Result;
}

FActionReach FCombatBattle::ActionReach(int32 ActorId, ECombatCommandType Action) const
{
    FActionReach Result;
    const auto* Actor = Find(ActorId);
    if (!bStarted || State.Phase != EBattlePhase::Fighting || !Actor || ActiveId() != ActorId || Actor->Life != ECombatLife::Conscious) { return Result; }
    int32 Range = -1;
    if (Action == ECombatCommandType::Attack) { Range = Actor->Range; }
    else if (Action == ECombatCommandType::Potion || Action == ECombatCommandType::Rescue) { Range = TouchRange; }
    else
    {
        for (const auto& Ability : FBuildCatalog::Abilities(*Actor))
        { if (Ability.Command == Action) { Range = Ability.Range; break; } }
        // Legacy adapters use the same constants as their validation.
        if (Range < 0 && FBuildCatalog::HasSpecial(*Actor, Action))
        {
            if (Action == ECombatCommandType::SecondWind) { Range = 0; }
            else if (Action == ECombatCommandType::PrecisionStrike) { Range = Actor->Range; }
            else if (Action == ECombatCommandType::MagicMissile) { Range = MissileRange; }
            else if (Action == ECombatCommandType::CureWounds) { Range = TouchRange; }
        }
    }
    for (int32 Y = 0; Y < State.Encounter.Height; ++Y)
    for (int32 X = 0; X < State.Encounter.Width; ++X)
    {
        const FIntPoint Cell(X, Y);
        if (GridDistance(Actor->Cell, Cell) > Range) { continue; }
        (HasLineOfSight(Actor->Cell, Cell) ? Result.Visible : Result.Obstructed).Add(Cell);
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



int32 FCombatBattle::SpellSlotsRemaining(const FCombatantState& Unit, int32 SpellLevel) const
{
    int32 Count = 0;
    if (SpellLevel == 1) { Count = Unit.SpellSlot1; }
    else if (SpellLevel == 2) { Count = Unit.SpellSlot2; }
    else if (SpellLevel == 3) { Count = Unit.SpellSlot3; }
    else { return 0; }
    // Pact Magic is a separate pool; available at PactSlotLevel in addition to multiclass slots.
    if (Unit.PactSlots > 0 && Unit.PactSlotLevel == SpellLevel) { Count += Unit.PactSlots; }
    return Count;
}

bool FCombatBattle::SpendSpellSlot(FCombatantState& Unit, int32 SpellLevel)
{
    // Prefer multiclass/full-caster slots first; then a pact slot of the same level.
    if (SpellLevel == 1 && Unit.SpellSlot1 > 0) { --Unit.SpellSlot1; return true; }
    if (SpellLevel == 2 && Unit.SpellSlot2 > 0) { --Unit.SpellSlot2; return true; }
    if (SpellLevel == 3 && Unit.SpellSlot3 > 0) { --Unit.SpellSlot3; return true; }
    if (Unit.PactSlots > 0 && Unit.PactSlotLevel == SpellLevel) { --Unit.PactSlots; return true; }
    return false;
}

void FCombatBattle::FillSrdSpellSlots(FCombatantState& Unit)
{
    if (Unit.Side != ECombatSide::Party) { return; }
    FClassLevels Levels;
    Levels.Fighter = Unit.ClassLevels.Fighter; Levels.Rogue = Unit.ClassLevels.Rogue; Levels.Cleric = Unit.ClassLevels.Cleric;
    Levels.Wizard = Unit.ClassLevels.Wizard; Levels.Barbarian = Unit.ClassLevels.Barbarian; Levels.Monk = Unit.ClassLevels.Monk;
    Levels.Paladin = Unit.ClassLevels.Paladin; Levels.Ranger = Unit.ClassLevels.Ranger; Levels.Sorcerer = Unit.ClassLevels.Sorcerer;
    Levels.Warlock = Unit.ClassLevels.Warlock; Levels.Druid = Unit.ClassLevels.Druid; Levels.Bard = Unit.ClassLevels.Bard;
    Levels.Artificer = Unit.ClassLevels.Artificer;
    if (Levels.CharacterLevel() == 0 && Unit.Level > 0 && Unit.AdventurerRole != EAdventurerRole::None)
    {
        Levels.Set(Unit.AdventurerRole, Unit.Level);
    }
    // Non-pact multiclass slots only (Warlock excluded from MulticlassSpellcasterLevel).
    Unit.SpellSlot1 = FAdvancementRules::MulticlassSlotsFor(Levels, 1);
    Unit.SpellSlot2 = FAdvancementRules::MulticlassSlotsFor(Levels, 2);
    Unit.SpellSlot3 = FAdvancementRules::MulticlassSlotsFor(Levels, 3);
    if (Levels.Warlock > 0)
    {
        Unit.PactSlotLevel = FSpellRules::MaxSlotLevel(EAdventurerRole::Warlock, Levels.Warlock);
        Unit.PactSlots = FSpellRules::SlotsFor(EAdventurerRole::Warlock, Levels.Warlock, Unit.PactSlotLevel);
    }
    else
    {
        Unit.PactSlotLevel = 0;
        Unit.PactSlots = 0;
    }
}

int32 FCombatBattle::EffectiveArmorClass(const FCombatantState& Unit) const
{
    return Unit.ArmorClass + (IsSrd() ? Unit.SpellArmorBonus : 0);
}

void FCombatBattle::TickSpellArmor(FCombatantState& Unit)
{
    if (Unit.SpellArmorRounds > 0 && --Unit.SpellArmorRounds == 0)
    {
        // Timed Shield-style bonus expires; concentration SoF uses ConcentratedSpell path.
        if (!Unit.bConcentrating || Unit.ConcentratedSpell != static_cast<int32>(ESpell::ShieldOfFaith))
        {
            Unit.SpellArmorBonus = FMath::Max(0, Unit.SpellArmorBonus - 5);
        }
        Unit.bNegatesMagicMissile = false;
    }
}

int32 FCombatBattle::AttacksPerActionFor(const FCombatantState& Unit) const
{
    if (!IsSrd()) { return 1; }
    if (Unit.Side == ECombatSide::Enemy)
    {
        return FMath::Max(1, FMonsterRules::AttacksPerAction(Unit.Role));
    }
    return FMath::Max(1, FClassFeatureRules::AttacksPerAction(Unit.AdventurerRole, Unit.Level));
}

void FCombatBattle::SpendAttackAction(FCombatantState& Actor)
{
    if (!IsSrd())
    {
        State.bActionAvailable = false;
        return;
    }
    if (State.bActionAvailable)
    {
        State.bActionAvailable = false;
        State.AttacksLeft = AttacksPerActionFor(Actor) - 1;
    }
    else if (State.AttacksLeft > 0)
    {
        --State.AttacksLeft;
    }
    else
    {
        State.bActionAvailable = false;
    }
}

bool FCombatBattle::SneakAttackApplies(const FCombatantState& Actor, const FCombatantState& Target) const
{
    const bool bRogueKit = FBuildCatalog::Find(Actor.BuildId) ? FBuildCatalog::Find(Actor.BuildId)->SneakDie > 0 : Actor.AdventurerRole == EAdventurerRole::Rogue;
    if (!bRogueKit || !Actor.bSneakWeapon) { return false; }
    const bool bAlly = State.Encounter.Combatants.ContainsByPredicate([&Actor, &Target](const FCombatantState& Ally)
    {
        return Ally.Id != Actor.Id && Ally.Side == Actor.Side && Ally.Life == ECombatLife::Conscious && GridDistance(Ally.Cell, Target.Cell) <= 1;
    });
    if (!IsSrd()) { return bAlly; }
    // SRD: ally OR advantage (prone melee), once per turn; disadvantage blocks.
    const int32 Dist = GridDistance(Actor.Cell, Target.Cell);
    const ERollMode Mode = Target.bProne ? FMovementRules::AttackAgainstProne(Dist) : ERollMode::Normal;
    return FClassFeatureRules::SneakAttackAllowed(true, State.bSneakUsedThisTurn, Mode, bAlly);
}

FCombatBattle::FAttackSetup FCombatBattle::AttackSetup(const FCombatantState& Actor, ECombatCommandType Action, const FCombatantState& Target) const
{
    FAttackSetup Setup;
    // Share save setup across forecasts and resolution, including Acolyte Sacred Flame.
    const FMonsterStatBlock* Monster = IsSrd() && Actor.Side == ECombatSide::Enemy ? FMonsterRules::Find(Actor.Role) : nullptr;
    const bool MonsterSave = Monster && Monster->Kind == EMonsterAttack::SaveCantrip;
    Setup.bSave = IsSrd() && Action == ECombatCommandType::Attack && (MonsterSave || (Actor.AdventurerRole == EAdventurerRole::Cleric && Actor.Range > 1));
    if (Setup.bSave)
    {
        const FCharacterRules Caster = FProficiencyRules::PrebuiltCharacter(EAdventurerRole::Cleric, Actor.Level);
        Setup.TargetNumber = MonsterSave ? FMonsterRules::SpellSaveDc(*Monster) : FSpellcastingRules::SaveDc(Caster.ProficiencyBonus(), Caster.AbilityModifier(EAbility::Wisdom));
        // Sacred Flame gives no benefit from cover, so the save is the full Dexterity save bonus alone.
        Setup.Modifier = SaveBonusOf(Target, MonsterSave ? Monster->SaveAbility : EAbility::Dexterity);
        return Setup;
    }
    Setup.Modifier = Actor.AttackBonus + (Action == ECombatCommandType::PrecisionStrike ? PrecisionBonus : 0);
    Setup.TargetNumber = EffectiveArmorClass(Target) + (IsSrd() ? CoverAcBonus(Actor.Id, Target.Id) : 0);
    Setup.bAdvantage = IsSrd() && Target.Life == ECombatLife::Downed;
    Setup.bAutoCritical = Setup.bAdvantage && GridDistance(Actor.Cell, Target.Cell) <= 1;
    return Setup;
}

FActionForecast FCombatBattle::Forecast(int32 ActorId, ECombatCommandType Action, int32 TargetId) const
{
    FActionForecast Forecast;
    const FCombatantState* Actor = Find(ActorId);
    const FCombatantState* Target = Find(TargetId);
    if (!Actor || !Target || !LegalTargets(ActorId, Action).Contains(TargetId)) { return Forecast; }
    Forecast.bValid = true;
    if (Action == ECombatCommandType::Special1 || Action == ECombatCommandType::Special2 || (Action == ECombatCommandType::CureWounds && !Actor->BuildId.IsEmpty()))
    {
        for (const auto& A : FBuildCatalog::Abilities(*Actor))
        {
            if (A.Command != Action) { continue; }
            Forecast.Formula = A.Description;
            if (A.Effect == EAbilityEffect::Area)
            {
                for (int32 Id : OrderedAreaTargets(*Actor,*Target))
                {
                    const auto& Other = *Find(Id);
                    const int32 Prevented=FMath::Min(A.Amount,ProtectionFor(Other));
                    const int32 Lost=FMath::Min(Other.HP,FMath::Max(0,A.Amount-Prevented));
                    Forecast.Formula += TEXT(" | ") + Other.Name + FString::Printf(TEXT(" loses %d HP"),Lost);
                    if(Prevented) { Forecast.Effects.Add({Id,EAbilityEffect::Mitigation,Prevented,Other.Cell}); }
                    Forecast.Effects.Add({Id,EAbilityEffect::Strike,Lost,Other.Cell});
                }
            }
            if (A.Effect == EAbilityEffect::Healing)
            { Forecast.bHealing = true; Forecast.MinAmount = Forecast.MaxAmount = FMath::Min(A.Amount, Target->MaxHP - Target->HP); if (A.bCureDice) { Forecast.MinAmount = FMath::Min(CureDice + HealingModifier,Target->MaxHP-Target->HP); Forecast.MaxAmount = FMath::Min(CureDice * CureDie + HealingModifier,Target->MaxHP-Target->HP); } }
            Forecast.bDiscreteAmounts = A.Effect == EAbilityEffect::Surge;
            if (A.Effect == EAbilityEffect::Strike || A.Effect == EAbilityEffect::Surge || A.Effect == EAbilityEffect::Push || A.Effect == EAbilityEffect::Area)
            {
                Forecast.HitChancePercent = 100;
                Forecast.MinAmount = A.Amount;
                Forecast.MaxAmount = A.Effect == EAbilityEffect::Surge ? A.Amount * 2 : A.Amount;
                auto Reduce = [&](int32 Amount)
                {
                    const int32 Protection = ProtectionFor(*Target);
                    return FMath::Min(Target->HP, FMath::Max(0, Amount - Protection));
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
        const FAttackSetup Setup = AttackSetup(*Actor, Action, *Target);
        if (Setup.bSave)
        {
            // Saves have no natural 1 or 20 rule; the damage lands when the target fails.
            Forecast.bSavingThrow = true;
            Forecast.HitChancePercent = (400 - FCheckRules::SuccessOutOf400(Setup.Modifier, Setup.TargetNumber, ERollMode::Normal) + 2) / 4;
            Effect(1, SacredFlameDie, 0);
            break;
        }
        Forecast.bAttackRoll = true;
        int32 Hits = 0;
        for (int32 Natural = 1; Natural <= 20; ++Natural)
        { Hits += Natural >= CriticalThreshold(*Actor) || (Natural != 1 && Natural + Setup.Modifier >= Setup.TargetNumber) ? 1 : 0; }
        // Advantage keeps the higher of two d20s, so it misses only when both dice miss.
        Forecast.HitChancePercent = Setup.bAdvantage ? (400 - (20 - Hits) * (20 - Hits) + 2) / 4 : Hits * 5;
        // A hit that is always critical doubles the weapon and Sneak Attack dice.
        const int32 WeaponDice = Setup.bAutoCritical ? 2 : 1;
        Effect(WeaponDice, Actor->DamageDie, FBuildCatalog::AttackModifier(*Actor) + (Action == ECombatCommandType::PrecisionStrike ? PrecisionBonus : 0));
        Forecast.bSneakAttack = SneakAttackApplies(*Actor, *Target);
        if (Forecast.bSneakAttack)
        {
            const int32 Dice = WeaponDice * (IsSrd() ? FMath::Max(1, FClassFeatureRules::SneakAttackDice(Actor->Level)) : 1);
            Forecast.MinAmount += Dice;
            Forecast.MaxAmount += Dice * SneakDie;
            Forecast.Formula += FString::Printf(TEXT(" +%dd%d sneak"), Dice, SneakDie);
        }
        break;
    }
    case ECombatCommandType::MagicMissile: Forecast.HitChancePercent = 100; Effect(MissileDarts, MissileDie, MissileDarts * DartBonus); break;
    case ECombatCommandType::SecondWind: Effect(1, WindDie, IsSrd() && Actor ? Actor->Level : FighterLevel); break;
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
        const int32 CriticalExtra = Forecast.bAttackRoll ? Actor->DamageDie + (Forecast.bSneakAttack ? (IsSrd() ? FMath::Max(1, FClassFeatureRules::SneakAttackDice(Actor->Level)) * SneakDie : SneakDie) : 0) : 0;
        Forecast.bCanDrop = Forecast.MaxAmount + CriticalExtra >= Target->HP;
    }
    if (!Forecast.bHealing)
    {
        const int32 Protection = ProtectionFor(*Target);
        const int32 CriticalExtra = Forecast.bAttackRoll ? Actor->DamageDie + (Forecast.bSneakAttack ? (IsSrd() ? FMath::Max(1, FClassFeatureRules::SneakAttackDice(Actor->Level)) * SneakDie : SneakDie) : 0) : 0;
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
    if (Result.DamagePrevented > 0) { Result.Feedback += FString::Printf(TEXT(" | protection prevented %d damage"),Result.DamagePrevented); }
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
        FCombatantState* Moving = State.Encounter.Combatants.FindByPredicate([&Command](const FCombatantState& Unit) { return Unit.Id == Command.ActorId; });
        // SRD: stand up from prone (half speed) by Moving to your own cell.
        if (IsSrd() && Moving->bProne && Command.Destination == Moving->Cell)
        {
            const int32 StandCost = FMovementRules::StandUpCost(Moving->Speed);
            if (State.MovementLeft < StandCost) { return Reject(TEXT("Not enough movement to stand from prone.")); }
            State.MovementLeft -= StandCost;
            Moving->bProne = false;
            Result.Feedback = FString::Printf(TEXT("%s stands up (%d movement left)."), *Actor->Name, State.MovementLeft);
        }
        else
        {
            if (IsSrd() && Moving->GrapplerId != INDEX_NONE) { return Reject(TEXT("Grappled: speed is 0 until you Escape.")); }
            if (IsSrd() && Moving->bProne) { return Reject(TEXT("Prone: stand up (move to your cell) before crawling elsewhere.")); }
            Result.Path = PathTo(Command.ActorId, Command.Destination);
            if (Result.Path.IsEmpty()) { return Reject(TEXT("Cell is occupied, blocked, or beyond remaining movement.")); }
            int32 Cost = Result.Path.Num();
            TArray<FIntPoint> FullPath;
            FullPath.Add(Moving->Cell);
            FullPath.Append(Result.Path);
            Moving->Cell = Command.Destination;
            if (IsSrd())
            {
                // A mover an opportunity attack drops stops in the cell it was leaving and pays only for the cells it moved.
                Cost = ResolveOpportunityAttacks(*Moving, FullPath, Result);
                Result.Path.SetNum(Cost);
            }
            State.MovementLeft -= Cost;
            Result.Feedback = FString::Printf(TEXT("%s moves %d cells (%d left)."), *Actor->Name, Cost, State.MovementLeft) + Result.Feedback;
            // Leaving reach of your own grapple victim ends it.
            if (IsSrd())
            {
                for (FCombatantState& Other : State.Encounter.Combatants)
                {
                    if (Other.GrapplerId == Moving->Id && GridDistance(Other.Cell, Moving->Cell) > 1) { Other.GrapplerId = INDEX_NONE; }
                }
                // A downed or dead mover cannot finish its turn. Pass the turn on even when the battle
                // just ended, so the active combatant is conscious as Restore requires.
                if (Moving->Life != ECombatLife::Conscious)
                {
                    CheckOutcome();
                    AdvanceTurn();
                }
            }
        }
    }
    else if (Command.Type == ECombatCommandType::Special1 || Command.Type == ECombatCommandType::Special2 || (Command.Type == ECombatCommandType::CureWounds && !Actor->BuildId.IsEmpty()))
    {
        if (!LegalTargets(Command.ActorId, Command.Type).Contains(Command.TargetId)) { return Reject(TEXT("Special unavailable: budget, stance, resource, target, range or sight.")); }
        for (const auto& A : FBuildCatalog::Abilities(*Actor))
        {
            if (A.Command != Command.Type) { continue; }
            auto* User = MutableFind(Command.ActorId); auto* Target = MutableFind(Command.TargetId);
            if (A.bKitResource) { User->KitUses -= A.ResourceCost; }
            if (A.Effect == EAbilityEffect::Swap)
            {
                const auto Cell = User->Cell; User->Cell = Target->Cell; Target->Cell = Cell;
                Result.Effects.Add({User->Id, A.Effect, 0, User->Cell});
                Result.Effects.Add({Target->Id, A.Effect, 0, Target->Cell});
            }
            else if (A.Effect == EAbilityEffect::Healing)
            {
                int32 Amount = A.Amount;
                if (A.bCureDice) { --User->SpellSlots; DiceLog = &Result.EffectDice; Result.EffectModifier = HealingModifier; Amount = RollDice(CureDice,CureDie)+HealingModifier; DiceLog = nullptr; }
                Result.Healing = FMath::Min(Amount, Target->MaxHP - Target->HP); Target->HP += Result.Healing;
                Result.Effects.Add({Target->Id, A.Effect, Result.Healing, Target->Cell});
            }
            else if (A.Effect == EAbilityEffect::Rage) { --User->RageUses; User->RageTurns = 2; }
            else if (A.Effect == EAbilityEffect::Protect)
            {
                User->GuardTargetId = Target->Id;
                if (A.bKitResource) { User->GuardAmount = A.Amount; User->GuardTurns = 1; }
            }
            else
            {
                int32 Amount = A.Amount;
                if (A.Effect == EAbilityEffect::Surge) { User->LastSurge = Result.SurgeOutcome = Roll(2); Amount = Result.SurgeOutcome * A.Amount; }
                TArray<int32> Targets = {Target->Id};
                if (A.Effect == EAbilityEffect::Area) { Targets = OrderedAreaTargets(*User,*Target); }
                for (int32 Id : Targets)
                {
                    auto* Victim = MutableFind(Id); const int32 Before = Victim->HP; const int32 Prevented = FMath::Min(Amount, ProtectionFor(*Victim));
                    ApplyDamage(*Victim, Amount); const int32 Lost = Before - Victim->HP;
                    Result.Damage += Lost; Result.bHit = true;
                    if (Prevented) { Result.Effects.Add({Id,EAbilityEffect::Mitigation,Prevented,Victim->Cell}); }
                    Result.Effects.Add({Id, A.Effect == EAbilityEffect::Surge ? EAbilityEffect::Surge : EAbilityEffect::Strike, Lost, Victim->Cell});
                    Result.DamagePrevented += Prevented;
                    if (A.Effect == EAbilityEffect::Push && Victim->Life == ECombatLife::Conscious)
                    {
                        const auto Delta = Victim->Cell - User->Cell;
                        const FIntPoint Step = FMath::Abs(Delta.X) >= FMath::Abs(Delta.Y) ? FIntPoint(FMath::Sign(Delta.X),0) : FIntPoint(0,FMath::Sign(Delta.Y));
                        if (IsOpen(Victim->Cell + Step, Id)) { Victim->Cell += Step; Result.Effects.Add({Id,EAbilityEffect::Push,0,Victim->Cell}); }
                    }
                }
            }
            if (A.Effect == EAbilityEffect::Protect || A.Effect == EAbilityEffect::Rage)
            { Result.Effects.Add({Target->Id, A.Effect, A.Effect == EAbilityEffect::Protect ? (A.Amount > 0 ? A.Amount : 3) : 2, Target->Cell}); }
            if (A.bBonus) { State.bBonusActionAvailable = false; } else { State.bActionAvailable = false; }
            Result.Feedback = A.Name + TEXT(": ") + A.Description + FString::Printf(TEXT(" | committed HP loss %d, heal %d, prevented %d"), Result.Damage, Result.Healing, Result.DamagePrevented);
        }
        CheckOutcome();
    }
    else if (Command.Type == ECombatCommandType::Attack || Command.Type == ECombatCommandType::PrecisionStrike)
    {
        if (!LegalTargets(Command.ActorId, Command.Type).Contains(Command.TargetId)) { return Reject(TEXT("No attack available, or target is inactive, friendly, out of range, or obscured.")); }
        FCombatantState* Target = State.Encounter.Combatants.FindByPredicate([&Command](const FCombatantState& Unit) { return Unit.Id == Command.TargetId; });
        const int32 ManeuverBonus = Command.Type == ECombatCommandType::PrecisionStrike ? PrecisionBonus : 0;
        if (ManeuverBonus) { --MutableFind(Command.ActorId)->ManeuverUses; }
        const FAttackSetup Setup = AttackSetup(*Actor, Command.Type, *Target);
        if (Setup.bSave)
        {
            const int32 Dc = Setup.TargetNumber;
            const int32 SaveMod = Setup.Modifier;
            const FD20Outcome Save = FCheckRules::Resolve(SaveMod, Dc, ERollMode::Normal, State.RandomState);
            Result.Type = Command.Type; Result.ActorId = Command.ActorId; Result.TargetId = Command.TargetId;
            Result.NaturalRoll = Save.Kept;
            Result.AttackBonus = SaveMod;
            Result.AttackTotal = Save.Total;
            Result.TargetArmorClass = Dc; // save DC presented in the AC slot for HUD parity
            Result.bCritical = false;
            Result.EffectModifier = 0;
            Result.bHit = !Save.bSuccess;
            if (Result.bHit)
            {
                DiceLog = &Result.EffectDice;
                Result.Damage = Roll(SacredFlameDie);
                DiceLog = nullptr;
                const int32 Protection = ProtectionFor(*Target);
                Result.DamagePrevented = FMath::Min(Result.Damage, Protection);
                ApplyDamage(*Target, Result.Damage, Result.bCritical);
                Result.Damage -= Result.DamagePrevented;
            }
            SpendAttackAction(*MutableFind(Command.ActorId));
            CheckOutcome();
            Result.Feedback = FString::Printf(TEXT("%s Sacred Flame > %s: %s %d + %d = %d vs DC %d | %s | %d radiant. HP %d/%d"),
                *Actor->Name, *Target->Name, FAbilityRules::AbilityAbbreviation(EAbility::Dexterity), Save.Kept, SaveMod, Save.Total, Dc,
                Save.bSuccess ? TEXT("SAVED") : TEXT("FAIL"), Result.Damage, Target->HP, Target->MaxHP);
        }
        else
        {
            Result.NaturalRoll = Roll(20);
            // SRD Advantage: roll a second d20 and keep the higher.
            if (Setup.bAdvantage) { const int32 Second = Roll(20); Result.NaturalRoll = FMath::Max(Result.NaturalRoll, Second); }
            Result.AttackTotal = Result.NaturalRoll + Setup.Modifier;
            Result.AttackBonus = Setup.Modifier;
            Result.TargetArmorClass = Setup.TargetNumber;
            Result.bCritical = Result.NaturalRoll >= CriticalThreshold(*Actor);
            Result.bHit = Result.bCritical || (Result.NaturalRoll != 1 && Result.AttackTotal >= Result.TargetArmorClass);
            Result.bCritical = Result.bCritical || (Result.bHit && Setup.bAutoCritical);
            if (Result.bHit)
            {
                DiceLog = &Result.EffectDice;
                // Weapon die, then the critical die; both have the same sides, so totals match earlier builds.
                Result.Damage = Roll(Actor->DamageDie);
                if (Result.bCritical) { Result.Damage += Roll(Actor->DamageDie); }
                Result.EffectModifier = FBuildCatalog::AttackModifier(*Actor) + ManeuverBonus;
                Result.Damage += Result.EffectModifier;
                Result.bSneakAttack = SneakAttackApplies(*Actor, *Target);
                if (Result.bSneakAttack)
                {
                    const int32 Dice = IsSrd() ? FMath::Max(1, FClassFeatureRules::SneakAttackDice(Actor->Level)) : 1;
                    Result.Damage += RollDice(Result.bCritical ? Dice * 2 : Dice, SneakDie);
                    if (IsSrd()) { State.bSneakUsedThisTurn = true; }
                }
                DiceLog = nullptr;
                const int32 Protection = ProtectionFor(*Target);
                Result.DamagePrevented = FMath::Min(Result.Damage, Protection);
                ApplyDamage(*Target, Result.Damage, Result.bCritical);
                Result.Damage -= Result.DamagePrevented;
            }
            SpendAttackAction(*MutableFind(Command.ActorId));
            CheckOutcome();
            FString TargetStatus = Target->Life == ECombatLife::Downed ? FString::Printf(TEXT(" DOWNED (%d rescue turns)"), Target->RescueTurnsLeft) :
                Target->Life == ECombatLife::Dead ? TEXT(" DEAD") : TEXT("");
            if (Result.bSneakAttack)
            {
                const int32 Dice = IsSrd() ? FMath::Max(1, FClassFeatureRules::SneakAttackDice(Actor->Level)) : 1;
                TargetStatus += FString::Printf(TEXT(" SNEAK +%dd%d"), Dice, SneakDie);
            }
            Result.Feedback = FString::Printf(TEXT("%s > %s: d20 %d + %d = %d vs AC %d | %s | %d damage. HP %d/%d%s"),
                *Actor->Name, *Target->Name, Result.NaturalRoll, Actor->AttackBonus, Result.AttackTotal, Result.TargetArmorClass,
                Result.bCritical ? TEXT("CRITICAL") : Result.bHit ? TEXT("HIT") : TEXT("MISS"), Result.Damage,
                Target->HP, Target->MaxHP, *TargetStatus);
        }
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
            if (IsSrd())
            {
                if (!SpendSpellSlot(*User, 1)) { return Reject(TEXT("No 1st-level spell slots remaining.")); }
            }
            else { --User->SpellSlots; }
            if (const auto* B = FBuildCatalog::Find(User->BuildId)) { User->KitUses -= B->MissileKitCost; }
            Result.bHit = true;
            if (IsSrd())
            {
                // SRD: each dart is its own damage instance so resistance/immunity applies per dart.
                if (Target->bNegatesMagicMissile)
                {
                    Result.Damage = 0;
                    Result.Feedback = FString::Printf(TEXT("%s Magic Missile > %s | negated by Shield"), *User->Name, *Target->Name);
                }
                else
                {
                    const FDamageDefenses Defenses = FDamageRules::ProfileDefenses(Target->Role);
                    int32 Total = 0;
                    DiceLog = &Result.EffectDice;
                    for (int32 Dart = 0; Dart < MissileDarts; ++Dart)
                    {
                        const int32 Raw = Roll(MissileDie) + DartBonus;
                        Total += FDamageRules::Adjust(Raw, EDamageType::Force, Defenses);
                    }
                    DiceLog = nullptr;
                    Result.EffectModifier = MissileDarts * DartBonus;
                    Result.Damage = Total;
                    const int32 Protection = ProtectionFor(*Target);
                    Result.DamagePrevented = FMath::Min(Result.Damage, Protection);
                    ApplyDamage(*Target, Result.Damage, Result.bCritical);
                    Result.Damage -= Result.DamagePrevented;
                }
            }
            else
            {
                DiceLog = &Result.EffectDice;
                Result.EffectModifier = MissileDarts * DartBonus;
                Result.Damage = RollDice(MissileDarts, MissileDie) + Result.EffectModifier;
                DiceLog = nullptr;
                const int32 Protection = ProtectionFor(*Target);
                Result.DamagePrevented = FMath::Min(Result.Damage, Protection);
                ApplyDamage(*Target, Result.Damage, Result.bCritical);
                Result.Damage -= Result.DamagePrevented;
            }
            Ability = FString::Printf(TEXT("Magic Missile (%d darts, 1 slot)"), MissileDarts);
        }
        else
        {
            int32 Amount = 1;
            DiceLog = &Result.EffectDice;
            if (Command.Type == ECombatCommandType::SecondWind)
            {
                --User->SecondWindUses;
                const int32 LevelBonus = IsSrd() ? User->Level : FighterLevel;
                Result.EffectModifier = LevelBonus;
                Amount = IsSrd() ? FClassFeatureRules::SecondWindHealing(Roll(WindDie), LevelBonus) : (Roll(WindDie) + FighterLevel);
                Ability = FString::Printf(TEXT("Second Wind (bonus, 1 use, +%d)"), LevelBonus);
            }
            else if (Command.Type == ECombatCommandType::CureWounds)
            {
                if (IsSrd()) { if (!SpendSpellSlot(*User, 1)) { DiceLog = nullptr; return Reject(TEXT("No 1st-level spell slots remaining.")); } }
                else { --User->SpellSlots; }
                Result.EffectModifier = HealingModifier;
                Amount = RollDice(CureDice, CureDie) + HealingModifier;
                Ability = TEXT("Cure Wounds (1 slot)");
            }
            else if (Command.Type == ECombatCommandType::Potion) { --User->Potions; Result.EffectModifier = PotionBonus; Amount = RollDice(PotionDice, PotionDie) + PotionBonus; Ability = TEXT("Potion (1 bottle)"); }
            else { Result.EffectModifier = 1; }
            DiceLog = nullptr;
            Result.Healing = FMath::Min(Amount, Target->MaxHP - Target->HP);
            Target->HP += Result.Healing;
            Target->Life = ECombatLife::Conscious;
            Target->RescueTurnsLeft = 0; Target->DeathSaveSuccesses = 0; Target->DeathSaveFailures = 0;
        }
        if (Command.Type == ECombatCommandType::SecondWind) { State.bBonusActionAvailable = false; }
        else { State.bActionAvailable = false; }
        Result.Feedback = FString::Printf(TEXT("%s: %s > %s | heal %d, damage %d | HP %d/%d"), *User->Name, *Ability, *Target->Name, Result.Healing, Result.Damage, Target->HP, Target->MaxHP);
        CheckOutcome();
    }

    else if (IsSrd() && Command.Type == ECombatCommandType::CastSpell)
    {
        FCombatantState* User = MutableFind(Command.ActorId);
        if (!User) { return Reject(TEXT("Caster missing.")); }
        if (Command.SpellId < 0 || Command.SpellId >= static_cast<int32>(ESpell::Count)) { return Reject(TEXT("Unknown spell.")); }
        const ESpell Spell = static_cast<ESpell>(Command.SpellId);
        const FSpellFacts& Facts = FSpellRules::Facts(Spell);
        if (!FSpellRules::OnClassList(User->AdventurerRole, Spell))
        { return Reject(TEXT("Spell is not on this class list (SRD).")); }
        const int32 SpellLevel = Facts.Level;
        const int32 SlotLevel = SpellLevel == 0 ? 0 : Command.SlotLevel;
        if (SpellLevel > 0)
        {
            if (SlotLevel < SpellLevel || SlotLevel > FSpellRules::MaxSpellLevel) { return Reject(TEXT("Slot level too low or invalid.")); }
            if (SpellSlotsRemaining(*User, SlotLevel) <= 0) { return Reject(TEXT("No spell slots of that level remaining.")); }
        }
        // Action economy from casting time text.
        const bool bReaction = FCString::Stristr(Facts.CastingTime, TEXT("reaction")) != nullptr;
        const bool bBonus = FCString::Stristr(Facts.CastingTime, TEXT("bonus")) != nullptr;
        if (bReaction)
        {
            if (!User->bReactionAvailable) { return Reject(TEXT("Reaction already used.")); }
        }
        else if (bBonus)
        {
            if (!State.bBonusActionAvailable) { return Reject(TEXT("Bonus action already used.")); }
        }
        else if (!State.bActionAvailable) { return Reject(TEXT("Action already used.")); }

        FCombatantState* Target = MutableFind(Command.TargetId);
        if (!Target) { Target = User; }
        // Range: feet/5 = cells; 0 = touch/self -> 1 cell for touch, self ignores.
        const int32 RangeCells = Facts.RangeFeet <= 0 ? 1 : FMath::Max(1, Facts.RangeFeet / 5);
        const bool bSelf = (Facts.RangeText && FCString::Stristr(Facts.RangeText, TEXT("Self")) != nullptr) || Spell == ESpell::Shield || Spell == ESpell::MageArmor || Spell == ESpell::MistyStep;
        if (!bSelf && GridDistance(User->Cell, Target->Cell) > RangeCells) { return Reject(TEXT("Target out of spell range.")); }
        if (!bSelf && !HasLineOfSight(User->Cell, Target->Cell)) { return Reject(TEXT("No line of sight.")); }

        if (SpellLevel > 0 && !SpendSpellSlot(*User, SlotLevel)) { return Reject(TEXT("Failed to spend spell slot.")); }
        if (bReaction) { User->bReactionAvailable = false; }
        else if (bBonus) { State.bBonusActionAvailable = false; }
        else { State.bActionAvailable = false; State.AttacksLeft = 0; }

        if (FConcentrationRules::RequiresConcentration(Spell))
        {
            if (FConcentrationRules::NewSpellEndsCurrent(User->bConcentrating, true))
            {
                // Drop prior concentration AC from SoF on the beneficiary, not the caster.
                if (User->ConcentratedSpell == static_cast<int32>(ESpell::ShieldOfFaith))
                {
                    if (FCombatantState* Prior = MutableFind(User->ConcentratedTargetId))
                    { Prior->SpellArmorBonus = FMath::Max(0, Prior->SpellArmorBonus - 2); }
                }
                User->bConcentrating = false; User->ConcentratedSpell = INDEX_NONE; User->ConcentratedTargetId = INDEX_NONE;
            }
            User->bConcentrating = true;
            User->ConcentratedSpell = static_cast<int32>(Spell);
            User->ConcentratedTargetId = Target->Id;
        }

        Result.Type = Command.Type; Result.ActorId = Command.ActorId; Result.TargetId = Target->Id;
        const FSpellDelivery Delivery = FSpellcastingRules::Delivery(Spell);

        if (Spell == ESpell::Shield)
        {
            User->SpellArmorBonus += Facts.ArmorClassBonus;
            User->SpellArmorRounds = 1;
            User->bNegatesMagicMissile = Facts.bNegatesMagicMissile;
            Result.bHit = true;
            Result.Feedback = FString::Printf(TEXT("%s casts Shield: +%d AC until start of next turn (negates Magic Missile)"), *User->Name, Facts.ArmorClassBonus);
        }
        else if (Spell == ESpell::ShieldOfFaith)
        {
            Target->SpellArmorBonus += Facts.ArmorClassBonus;
            // SoF concentrates on caster; bonus on target.
            Result.bHit = true;
            Result.Feedback = FString::Printf(TEXT("%s casts Shield of Faith on %s: +%d AC (concentration)"), *User->Name, *Target->Name, Facts.ArmorClassBonus);
        }
        else if (Spell == ESpell::MageArmor)
        {
            const int32 Dex = FAbilityRules::Modifier(AbilityScoresOf(*Target).Dexterity);
            Target->ArmorClass = 13 + Dex;
            Target->bMageArmor = true;
            Result.bHit = true;
            Result.Feedback = FString::Printf(TEXT("%s casts Mage Armor on %s: AC %d (13+DEX)"), *User->Name, *Target->Name, Target->ArmorClass);
        }
        else if (Delivery.Kind == ESpellDelivery::Healing)
        {
            const int32 Dice = FSpellRules::UpcastDiceCount(Spell, SlotLevel > 0 ? SlotLevel : SpellLevel);
            const int32 Mod = FProficiencyRules::PrebuiltCharacter(User->AdventurerRole, User->Level).AbilityModifier(FSpellcastingRules::SpellcastingAbility(User->AdventurerRole));
            DiceLog = &Result.EffectDice;
            const int32 Rolled = RollDice(Dice, Facts.DieSides) + Mod;
            DiceLog = nullptr;
            Result.EffectModifier = Mod;
            Result.Healing = FMath::Min(Rolled, Target->MaxHP - Target->HP);
            Target->HP += Result.Healing;
            Target->Life = ECombatLife::Conscious;
            Result.bHit = true;
            Result.Feedback = FString::Printf(TEXT("%s casts %s on %s | heal %d (slot %d)"), *User->Name, Facts.Name, *Target->Name, Result.Healing, SlotLevel);
        }
        else if (Delivery.Kind == ESpellDelivery::Automatic && Spell == ESpell::MagicMissile)
        {
            const int32 Darts = FSpellRules::UpcastDiceCount(Spell, SlotLevel);
            if (Target->bNegatesMagicMissile)
            {
                Result.Damage = 0; Result.bHit = true;
                Result.Feedback = FString::Printf(TEXT("%s Magic Missile > %s | negated by Shield"), *User->Name, *Target->Name);
            }
            else
            {
                const FDamageDefenses Defenses = FDamageRules::ProfileDefenses(Target->Role);
                int32 Total = 0;
                DiceLog = &Result.EffectDice;
                for (int32 Dart = 0; Dart < Darts; ++Dart)
                {
                    Total += FDamageRules::Adjust(Roll(Facts.DieSides) + Facts.FlatBonus, Facts.DamageType, Defenses);
                }
                DiceLog = nullptr;
                Result.EffectModifier = Darts * Facts.FlatBonus;
                Result.Damage = Total;
                const int32 Protection = ProtectionFor(*Target);
                Result.DamagePrevented = FMath::Min(Result.Damage, Protection);
                ApplyDamage(*Target, Result.Damage);
                Result.Damage -= Result.DamagePrevented;
                Result.bHit = true;
                Result.Feedback = FString::Printf(TEXT("%s casts Magic Missile (%d darts, slot %d) > %s | %d force"), *User->Name, Darts, SlotLevel, *Target->Name, Result.Damage);
            }
        }
        else if (Delivery.Kind == ESpellDelivery::Attack)
        {
            const FCharacterRules Caster = FProficiencyRules::PrebuiltCharacter(User->AdventurerRole, User->Level);
            const int32 Bonus = FSpellcastingRules::AttackBonus(Caster.ProficiencyBonus(), Caster.AbilityModifier(FSpellcastingRules::SpellcastingAbility(User->AdventurerRole)));
            Result.NaturalRoll = Roll(20);
            Result.AttackBonus = Bonus;
            Result.AttackTotal = Result.NaturalRoll + Bonus;
            const int32 Cover = CoverAcBonus(Command.ActorId, Target->Id);
            Result.TargetArmorClass = EffectiveArmorClass(*Target) + Cover;
            Result.bCritical = Result.NaturalRoll == 20;
            Result.bHit = Result.bCritical || (Result.NaturalRoll != 1 && Result.AttackTotal >= Result.TargetArmorClass);
            if (Result.bHit)
            {
                const int32 Dice = FCantripRules::IsCantrip(Spell) ? FCantripRules::DiceCount(Spell, User->Level) : FSpellRules::UpcastDiceCount(Spell, SlotLevel > 0 ? SlotLevel : SpellLevel);
                DiceLog = &Result.EffectDice;
                Result.Damage = RollDice(Result.bCritical ? Dice * 2 : Dice, Facts.DieSides);
                DiceLog = nullptr;
                const int32 Protection = ProtectionFor(*Target);
                Result.DamagePrevented = FMath::Min(Result.Damage, Protection);
                ApplyDamage(*Target, Result.Damage);
                Result.Damage -= Result.DamagePrevented;
            }
            Result.Feedback = FString::Printf(TEXT("%s casts %s > %s: d20 %d + %d = %d vs AC %d | %s | %d damage"),
                *User->Name, Facts.Name, *Target->Name, Result.NaturalRoll, Bonus, Result.AttackTotal, Result.TargetArmorClass,
                Result.bCritical ? TEXT("CRITICAL") : Result.bHit ? TEXT("HIT") : TEXT("MISS"), Result.Damage);
        }
        else if (Delivery.Kind == ESpellDelivery::Save)
        {
            const FCharacterRules Caster = FProficiencyRules::PrebuiltCharacter(User->AdventurerRole, User->Level);
            const int32 Dc = FSpellcastingRules::SaveDc(Caster.ProficiencyBonus(), Caster.AbilityModifier(FSpellcastingRules::SpellcastingAbility(User->AdventurerRole)));
            const FCharacterRules Defender = Target->AdventurerRole != EAdventurerRole::None
                ? FProficiencyRules::PrebuiltCharacter(Target->AdventurerRole, Target->Level)
                : [&]() { FCharacterRules E; E.Scores = AbilityScoresOf(*Target); E.Level = Target->Level; return E; }();
            const int32 CoverDex = Delivery.SaveAbility == EAbility::Dexterity ? CoverAcBonus(Command.ActorId, Target->Id) : 0;
            const int32 SaveMod = Defender.AbilityModifier(Delivery.SaveAbility) + CoverDex;
            const FD20Outcome Save = FCheckRules::Resolve(SaveMod, Dc, ERollMode::Normal, State.RandomState);
            Result.NaturalRoll = Save.Kept; Result.AttackBonus = SaveMod; Result.AttackTotal = Save.Total; Result.TargetArmorClass = Dc;
            Result.bCritical = false; Result.bHit = !Save.bSuccess;
            const int32 Dice = FCantripRules::IsCantrip(Spell) ? FCantripRules::DiceCount(Spell, User->Level) : FSpellRules::UpcastDiceCount(Spell, SlotLevel > 0 ? SlotLevel : SpellLevel);
            if (Dice > 0)
            {
                DiceLog = &Result.EffectDice;
                const int32 Rolled = RollDice(Dice, Facts.DieSides);
                DiceLog = nullptr;
                Result.Damage = FSpellcastingRules::DamageAfterSave(Rolled, Save.bSuccess, Delivery.bHalfOnSave);
                if (Result.Damage > 0)
                {
                    const int32 Protection = ProtectionFor(*Target);
                    Result.DamagePrevented = FMath::Min(Result.Damage, Protection);
                    ApplyDamage(*Target, Result.Damage);
                    Result.Damage -= Result.DamagePrevented;
                }
            }
            Result.Feedback = FString::Printf(TEXT("%s casts %s > %s: %s %d + %d = %d vs DC %d | %s | %d damage"),
                *User->Name, Facts.Name, *Target->Name, FAbilityRules::AbilityAbbreviation(Delivery.SaveAbility),
                Save.Kept, SaveMod, Save.Total, Dc, Save.bSuccess ? TEXT("SAVED") : TEXT("FAIL"), Result.Damage);
        }
        else
        {
            // Bless, Guidance, Light, etc.: mark accepted with concentration already applied.
            Result.bHit = true;
            Result.Feedback = FString::Printf(TEXT("%s casts %s%s"), *User->Name, Facts.Name,
                User->bConcentrating && FConcentrationRules::RequiresConcentration(Spell) ? TEXT(" (concentrating)") : TEXT(""));
        }
        CheckOutcome();
    }

    else if (IsSrd() && Command.Type == ECombatCommandType::ActionSurge)
    {
        FCombatantState* User = MutableFind(Command.ActorId);
        if (!User || User->AdventurerRole != EAdventurerRole::Fighter || User->ActionSurgeUses <= 0 || FClassFeatureRules::ActionSurgeUses(User->Level) <= 0)
        { return Reject(TEXT("Action Surge unavailable.")); }
        --User->ActionSurgeUses;
        State.bActionAvailable = true;
        State.AttacksLeft = 0;
        Result.Feedback = FString::Printf(TEXT("%s Action Surge: action restored (%d left)"), *User->Name, User->ActionSurgeUses);
    }
    else if (IsSrd() && (Command.Type == ECombatCommandType::Dash || Command.Type == ECombatCommandType::Disengage))
    {
        FCombatantState* User = MutableFind(Command.ActorId);
        if (!User || !LegalTargets(Command.ActorId, Command.Type).Contains(Command.ActorId))
        { return Reject(TEXT("Dash/Disengage unavailable.")); }
        const EStandardAction Std = Command.Type == ECombatCommandType::Dash ? EStandardAction::Dash : EStandardAction::Disengage;
        const bool bAsBonus = FStandardActionRules::CanTakeAsBonusAction(Std, User->AdventurerRole, User->Level) && State.bBonusActionAvailable && !State.bActionAvailable;
        if (bAsBonus) { State.bBonusActionAvailable = false; }
        else if (State.bActionAvailable) { State.bActionAvailable = false; State.AttacksLeft = 0; }
        else { return Reject(TEXT("No action budget for Dash/Disengage.")); }
        if (Command.Type == ECombatCommandType::Dash)
        {
            State.MovementLeft += User->Speed;
            Result.Feedback = FString::Printf(TEXT("%s Dash: +%d movement (now %d)"), *User->Name, User->Speed, State.MovementLeft);
        }
        else
        {
            User->bDisengaged = true;
            Result.Feedback = FString::Printf(TEXT("%s Disengage: no opportunity attacks this turn"), *User->Name);
        }
    }
    else if (IsSrd() && (Command.Type == ECombatCommandType::Grapple || Command.Type == ECombatCommandType::Shove || Command.Type == ECombatCommandType::Escape))
    {
        if (!LegalTargets(Command.ActorId, Command.Type).Contains(Command.TargetId))
        {
            return Reject(Command.Type == ECombatCommandType::Escape
                ? TEXT("Escape needs your grappler adjacent and conscious.")
                : TEXT("Grapple/Shove need an adjacent conscious foe within size limits."));
        }
        FCombatantState* User = MutableFind(Command.ActorId);
        FCombatantState* Target = MutableFind(Command.TargetId);
        auto SkillOrAbility = [&](const FCombatantState& U, ESkill Skill, EAbility Ability) -> int32
        {
            if (U.AdventurerRole != EAdventurerRole::None) { return FProficiencyRules::PrebuiltCharacter(U.AdventurerRole, U.Level).SkillBonus(Skill); }
            return FAbilityRules::Modifier(Ability == EAbility::Strength ? AbilityScoresOf(U).Strength : AbilityScoresOf(U).Dexterity);
        };
        auto SaveBonus = [&](const FCombatantState& U, EAbility Ability)
        {
            if (U.AdventurerRole != EAdventurerRole::None) { return FProficiencyRules::PrebuiltCharacter(U.AdventurerRole, U.Level).SaveBonus(Ability); }
            return FAbilityRules::Modifier(Ability == EAbility::Strength ? AbilityScoresOf(U).Strength : AbilityScoresOf(U).Dexterity);
        };
        auto GrappleDc = [&](const FCombatantState& U)
        { return 8 + FAbilityRules::Modifier(AbilityScoresOf(U).Strength) + FProficiencyRules::ProficiencyBonus(U.Level); };
        Result.Type = Command.Type; Result.ActorId = Command.ActorId; Result.TargetId = Command.TargetId;
        Result.bCritical = false; Result.EffectModifier = 0;
        if (Command.Type == ECombatCommandType::Escape)
        {
            if (!State.bActionAvailable) { return Reject(TEXT("Escape costs your Action.")); }
            // SRD 5.2.1: Athletics or Acrobatics check against the grappler's escape DC.
            const int32 EscMod = FMath::Max(SkillOrAbility(*User, ESkill::Athletics, EAbility::Strength), SkillOrAbility(*User, ESkill::Acrobatics, EAbility::Dexterity));
            const int32 GrpTotal = GrappleDc(*Target);
            const int32 EscDie = Roll(20);
            const int32 EscTotal = EscDie + EscMod;
            const bool bFreed = EscTotal >= GrpTotal;
            Result.NaturalRoll = EscDie; Result.AttackBonus = EscMod; Result.AttackTotal = EscTotal; Result.TargetArmorClass = GrpTotal;
            Result.bHit = bFreed;
            if (bFreed)
            {
                User->GrapplerId = INDEX_NONE;
                // Turn start set MovementLeft to GrappledSpeed(0). Escaping restores the speed
                // that was withheld — equivalent to SRD "remaining movement" for this model
                // (a grappled unit cannot spend movement before Escape on their turn).
                State.MovementLeft = User->Speed;
            }
            State.bActionAvailable = false;
            Result.Feedback = FString::Printf(TEXT("%s escapes %s: %d vs %d | %s"), *User->Name, *Target->Name, EscTotal, GrpTotal, bFreed ? TEXT("FREE") : TEXT("HELD"));
        }
        else
        {
            const int32 AtkTotal = GrappleDc(*User);
            const int32 DefMod = FMath::Max(SaveBonus(*Target, EAbility::Strength), SaveBonus(*Target, EAbility::Dexterity));
            const int32 DefDie = Roll(20);
            const int32 DefTotal = DefDie + DefMod;
            const bool bWon = DefTotal < AtkTotal;
            Result.NaturalRoll = DefDie; Result.AttackBonus = DefMod; Result.AttackTotal = DefTotal; Result.TargetArmorClass = AtkTotal;
            Result.bHit = bWon;
            FString Detail = bWon ? TEXT("SUCCESS") : TEXT("FAIL");
            if (bWon && Command.Type == ECombatCommandType::Grapple)
            {
                // One grappler per target; replace any prior.
                Target->GrapplerId = User->Id;
                Detail = TEXT("GRAPPLED");
                Result.Effects.Add({Target->Id, EAbilityEffect::Strike, 0, Target->Cell});
            }
            else if (bWon && Command.Type == ECombatCommandType::Shove)
            {
                const EShoveChoice Choice = Command.ShoveChoice == 1 ? EShoveChoice::PushAway : EShoveChoice::KnockProne;
                if (FGrappleRules::ShoveKnocksProne(true, Choice))
                {
                    Target->bProne = true;
                    Detail = TEXT("PRONE");
                }
                else if (FGrappleRules::ShovePushCells(true, Choice) > 0)
                {
                    const FIntPoint Delta = Target->Cell - User->Cell;
                    const FIntPoint Step = FMath::Abs(Delta.X) >= FMath::Abs(Delta.Y)
                        ? FIntPoint(FMath::Sign(Delta.X) == 0 ? 0 : FMath::Sign(Delta.X), 0)
                        : FIntPoint(0, FMath::Sign(Delta.Y) == 0 ? 0 : FMath::Sign(Delta.Y));
                    const FIntPoint Dest = Target->Cell + Step;
                    if (Step != FIntPoint::ZeroValue && IsInside(Dest) && IsOpen(Dest, Target->Id))
                    {
                        Target->Cell = Dest;
                        Detail = TEXT("PUSHED 5 FT");
                        Result.Effects.Add({Target->Id, EAbilityEffect::Push, 1, Target->Cell});
                        const FCombatantState* Grappler = Find(Target->GrapplerId);
                        if (Target->GrapplerId != INDEX_NONE && (!Grappler || GridDistance(Target->Cell, Grappler->Cell) > 1))
                        { Target->GrapplerId = INDEX_NONE; }
                    }
                    else { Detail = TEXT("PUSH BLOCKED"); }
                }
            }
            State.bActionAvailable = false;
            Result.Feedback = FString::Printf(TEXT("%s %s %s: %d vs %d | %s"),
                *User->Name, Command.Type == ECombatCommandType::Grapple ? TEXT("grapples") : TEXT("shoves"), *Target->Name,
                AtkTotal, DefTotal, *Detail);
        }
    }
    else if (Command.Type == ECombatCommandType::EndTurn)
    {
        Result.Feedback = Actor->Name + TEXT(" ends turn.");
        auto* User = MutableFind(Command.ActorId);
        if (User->RageTurns > 0 && --User->RageTurns == 0) { User->GuardTargetId = INDEX_NONE; }
        if (IsSrd()) { User->bReactionAvailable = true; User->bDisengaged = false; }
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

int32 FCombatBattle::ProtectionFor(const FCombatantState& Target) const
{
    int32 Protection = Target.RageTurns > 0 ? 1 : 0;
    for (const auto& U : State.Encounter.Combatants)
    { if (U.GuardTargetId == Target.Id) { Protection = FMath::Max(Protection, U.GuardAmount > 0 ? U.GuardAmount : 3); } }
    return Protection;
}

void FCombatBattle::ApplyDamage(FCombatantState& Target, int32 Damage, bool bCritical)
{
    const int32 Protection = ProtectionFor(Target);
    const int32 HPBefore = Target.HP;
    int32 Real = FMath::Max(0, Damage - Protection);
    if (IsSrd()) { Real = FDeathSaveRules::AbsorbWithTemporary(Target.TemporaryHP, Real); }
    if (IsSrd() && HPBefore == 0 && Target.Life == ECombatLife::Downed)
    {
        // Damage interrupts stability; no HP damage means no failure (e.g. temp HP).
        if (Real > 0)
        {
            FDeathSaveTally Tally; Tally.Successes = Target.DeathSaveSuccesses; Tally.Failures = Target.DeathSaveFailures;
            // Damage to a stable creature starts a fresh tally.
            if (Tally.Successes >= 3) { Tally = FDeathSaveTally(); }
            const EDeathState Outcome = FDeathSaveRules::ApplyDamageAtZero(Tally, Real, bCritical, Target.MaxHP);
            Target.DeathSaveSuccesses = Tally.Successes; Target.DeathSaveFailures = FMath::Min(Tally.Failures, 3);
            if (Outcome == EDeathState::Dead) { Target.Life = ECombatLife::Dead; Target.DeathSaveSuccesses = 0; Target.DeathSaveFailures = 0; }
        }
        return;
    }
    Target.HP = FMath::Max(0, Target.HP - Real);
    if (IsSrd() && Target.bConcentrating && Target.HP > 0 && Real > 0)
    {
        const FCharacterRules Character = Target.AdventurerRole != EAdventurerRole::None
            ? FProficiencyRules::PrebuiltCharacter(Target.AdventurerRole, Target.Level)
            : FCharacterRules{};
        const int32 ConSave = Character.Scores.IsValid() ? Character.SaveBonus(EAbility::Constitution) : FAbilityRules::Modifier(AbilityScoresOf(Target).Constitution);
        Target.bConcentrating = FCheckRules::Resolve(ConSave, FConcentrationRules::SaveDc(Real), ERollMode::Normal, State.RandomState).bSuccess;
    }
    if (Target.HP == 0)
    {
        Target.GuardTurns = 0; Target.GuardAmount = 0; Target.RageTurns = 0; Target.GuardTargetId = INDEX_NONE; Target.LastSurge = 0;
        for (auto& U : State.Encounter.Combatants) { if (U.GuardTargetId == Target.Id) { U.GuardTargetId = INDEX_NONE; U.GuardTurns = 0; U.GuardAmount = 0; } }
        const bool bMassive = IsSrd() && FDeathSaveRules::InstantDeath(Real, HPBefore, Target.MaxHP);
        Target.Life = (Target.Side == ECombatSide::Party && !bMassive) ? ECombatLife::Downed : ECombatLife::Dead;
        Target.RescueTurnsLeft = (!IsSrd() && Target.Life == ECombatLife::Downed) ? 3 : 0;
        Target.DeathSaveSuccesses = 0; Target.DeathSaveFailures = 0; Target.TemporaryHP = 0; Target.bConcentrating = false;
        Target.GrapplerId = INDEX_NONE; Target.bProne = false;
        for (FCombatantState& Other : State.Encounter.Combatants)
        { if (Other.GrapplerId == Target.Id) { Other.GrapplerId = INDEX_NONE; } }
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
            Unit.GuardTurns = 0; Unit.GuardAmount = 0; Unit.RageTurns = 0; Unit.GuardTargetId = INDEX_NONE; Unit.LastSurge = 0;
            if (Unit.Life == ECombatLife::Downed)
            {
                const bool bSurvives = IsSrd() && State.Phase == EBattlePhase::Victory && Unit.DeathSaveSuccesses >= 3;
                Unit.Life = bSurvives ? ECombatLife::Conscious : ECombatLife::Dead;
                Unit.HP = bSurvives ? 1 : 0;
                Unit.RescueTurnsLeft = 0; Unit.DeathSaveSuccesses = 0; Unit.DeathSaveFailures = 0;
            }
            Unit.bConcentrating = false; Unit.bReactionAvailable = true;
        }
    }
}

void FCombatBattle::DownedTurn(FCombatantState& Unit)
{
    if (!IsSrd())
    {
        if (--Unit.RescueTurnsLeft == 0) { Unit.Life = ECombatLife::Dead; }
        return;
    }
    if (Unit.DeathSaveSuccesses >= 3) { return; }
    FDeathSaveTally Tally; Tally.Successes = Unit.DeathSaveSuccesses; Tally.Failures = Unit.DeathSaveFailures;
    const EDeathState Outcome = FDeathSaveRules::ApplySave(Tally, Roll(20));
    Unit.DeathSaveSuccesses = Tally.Successes; Unit.DeathSaveFailures = Tally.Failures;
    if (Outcome == EDeathState::Dead) { Unit.Life = ECombatLife::Dead; Unit.RescueTurnsLeft = 0; Unit.DeathSaveSuccesses = 0; Unit.DeathSaveFailures = 0; }
    else if (Outcome == EDeathState::Revived) { Unit.Life = ECombatLife::Conscious; Unit.HP = 1; Unit.RescueTurnsLeft = 0; Unit.DeathSaveSuccesses = 0; Unit.DeathSaveFailures = 0; }
    // Becoming Stable resets both counts; three successes remain the saved stable marker.
    else if (Outcome == EDeathState::Stable) { Unit.DeathSaveSuccesses = 3; Unit.DeathSaveFailures = 0; }
}

void FCombatBattle::AdvanceTurn()
{
    for (int32 Count = 0; Count < State.TurnOrder.Num(); ++Count)
    {
        State.TurnIndex = (State.TurnIndex + 1) % State.TurnOrder.Num();
        if (State.TurnIndex == 0)
        {
            ++State.Round;
            if (IsSrd()) { for (FCombatantState& Unit : State.Encounter.Combatants) { Unit.bSurprised = false; } }
        }
        FCombatantState* Next = MutableFind(ActiveId());
        if (Next->Life == ECombatLife::Downed) { DownedTurn(*Next); }
        // SRD: a natural 20 death save revives the creature, which then takes this turn as normal.
        if (Next->Life == ECombatLife::Conscious)
        {
            Next->GuardTargetId = INDEX_NONE; Next->GuardAmount = 0; Next->GuardTurns = 0;
            if (IsSrd())
            {
                TickSpellArmor(*Next);
                Next->bReactionAvailable = true; Next->bDisengaged = false;
                if (Next->GrapplerId != INDEX_NONE)
                {
                    const FCombatantState* Grappler = Find(Next->GrapplerId);
                    if (!Grappler || Grappler->Life != ECombatLife::Conscious || GridDistance(Next->Cell, Grappler->Cell) > 1)
                    { Next->GrapplerId = INDEX_NONE; }
                }
            }
            State.MovementLeft = (IsSrd() && Next->GrapplerId != INDEX_NONE) ? FGrappleRules::GrappledSpeed(Next->Speed) : Next->Speed;
            State.bActionAvailable = true;
            State.bBonusActionAvailable = true;
            State.AttacksLeft = 0;
            State.bSneakUsedThisTurn = false;
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
    // SRD Multiattack / Extra Attack: keep swinging while LegalTargets still offers Attack.
    for (int32 Swing = 0; Swing < 8; ++Swing)
    {
        const TArray<int32> Targets = LegalTargets(Id, ECombatCommandType::Attack);
        if (Targets.IsEmpty()) { break; }
        Results.Add(Submit({ECombatCommandType::Attack, Id, {}, Targets[0]}));
        if (!IsSrd()) { break; }
    }
    if (State.Phase == EBattlePhase::Fighting && ActiveId() == Id) { Results.Add(Submit({ECombatCommandType::EndTurn, Id})); }
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
            const TCHAR* Builds[] = {TEXT("fighter.champion"), TEXT("rogue.thief"), TEXT("cleric.life"), TEXT("wizard.evocation")};
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
            (Snapshot.Phase != EBattlePhase::Fighting && (Unit.Life == ECombatLife::Downed || Unit.RageTurns != 0 || Unit.GuardTargetId != INDEX_NONE || Unit.LastSurge != 0 || Unit.GuardTurns != 0 || Unit.GuardAmount != 0))) { return Reject(); }
        bParty |= Unit.Side == ECombatSide::Party && Unit.Life == ECombatLife::Conscious;
        bEnemy |= Unit.Side == ECombatSide::Enemy && Unit.Life == ECombatLife::Conscious;
    }
    const auto Expected = !bParty ? EBattlePhase::Defeat : !bEnemy ? EBattlePhase::Victory : EBattlePhase::Fighting;
    const auto* Active = Candidate.Find(Candidate.ActiveId());
    if (Snapshot.Phase != Expected || !Active || Active->Life != ECombatLife::Conscious ||
        Snapshot.MovementLeft < 0 || Snapshot.MovementLeft > Active->Speed ||
        (!Snapshot.bBonusActionAvailable && ([&]()
        {
            const auto* Build = FBuildCatalog::Find(Active->BuildId);
            if (Build && Build->KitCap > 0)
            { return (Snapshot.Phase == EBattlePhase::Fighting && Active->KitUses >= Build->KitCap) || !FBuildCatalog::Abilities(*Active).ContainsByPredicate([](const auto& A) { return A.bBonus; }); }
            if (Active->AdventurerRole == EAdventurerRole::Barbarian)
            { return Snapshot.Phase == EBattlePhase::Fighting && Active->RageTurns == 0; }
            return Active->AdventurerRole != EAdventurerRole::Fighter || (Snapshot.Phase == EBattlePhase::Fighting && Active->SecondWindUses > 1);
        })())) { return Reject(); }
    *this = MoveTemp(Candidate);
    Error.Reset(); return true;
}

TArray<int32> FCombatBattle::OrderedAreaTargets(const FCombatantState& Actor, const FCombatantState& Target) const
{
    TArray<int32> Targets={Target.Id};
    for(const auto& Other:State.Encounter.Combatants)
    {
        if(Other.Id!=Target.Id && Other.Side!=Actor.Side && Other.Life==ECombatLife::Conscious && GridDistance(Target.Cell,Other.Cell)<=1 && HasLineOfSight(Target.Cell,Other.Cell)) { Targets.Add(Other.Id); }
    }
    return Targets;
}

int32 FCombatBattle::CoverAcBonus(int32 ActorId, int32 TargetId) const
{
    const FCombatantState* Actor = Find(ActorId);
    const FCombatantState* Target = Find(TargetId);
    if (!Actor || !Target || !IsSrd()) { return 0; }
    const int32 IntScore = AbilityScoresOf(*Actor).Intelligence;
    if (Actor->Side == ECombatSide::Enemy && FCoverRules::IgnoresCover(IntScore)) { return 0; }
    // Terrain: reuse HasLineOfSight as total-block; half cover when a blocked cell neighbors the target and sits between.
    int32 Best = 0;
    if (!HasLineOfSight(Actor->Cell, Target->Cell)) { return FCoverRules::AcBonus(ECoverLevel::Total); }
    for (const FIntPoint& Blocked : State.Encounter.BlockedCells)
    {
        if (GridDistance(Blocked, Target->Cell) == 1 && GridDistance(Actor->Cell, Blocked) + 1 == GridDistance(Actor->Cell, Target->Cell))
        {
            Best = FMath::Max(Best, FCoverRules::AcBonus(ECoverLevel::Half));
        }
    }
    // House rule: a conscious creature at least one size larger than the attacker grants half cover when adjacent to the target and between.
    for (const FCombatantState& Other : State.Encounter.Combatants)
    {
        if (Other.Id == ActorId || Other.Id == TargetId || Other.Life != ECombatLife::Conscious) { continue; }
        const ECoverLevel FromSize = FCoverRules::CoverFromLargerCreature(static_cast<ECreatureSize>(Actor->Size), static_cast<ECreatureSize>(Other.Size));
        if (FromSize == ECoverLevel::None) { continue; }
        if (GridDistance(Other.Cell, Target->Cell) == 1 && GridDistance(Actor->Cell, Other.Cell) + GridDistance(Other.Cell, Target->Cell) == GridDistance(Actor->Cell, Target->Cell))
        {
            Best = FMath::Max(Best, FCoverRules::AcBonus(FromSize));
        }
    }
    return Best;
}

int32 FCombatBattle::ResolveOpportunityAttacks(FCombatantState& Mover, const TArray<FIntPoint>& Path, FCombatResult& Result)
{
    // Path[0] is the start cell. Reactions resolve in path order, right before the step that leaves
    // the reactor's reach; reactors provoked by the same step roll in combatant order. Each rolls
    // its d20, then its damage die and any critical die.
    const int32 Reach = 1; // melee reach for OA; ranged opportunity deferred
    for (int32 Step = 1; Step < Path.Num(); ++Step)
    {
        for (FCombatantState& Reactor : State.Encounter.Combatants)
        {
            if (Reactor.Id == Mover.Id || Reactor.Side == Mover.Side || Reactor.Life != ECombatLife::Conscious ||
                !FOpportunityRules::StepProvokes(Reactor.Cell, Reach, Path[Step - 1], Path[Step], Mover.bDisengaged, false, Reactor.bReactionAvailable)) { continue; }
            Mover.Cell = Path[Step - 1];
            Reactor.bReactionAvailable = false;
            const int32 Natural = Roll(20);
            const int32 Total = Natural + Reactor.AttackBonus;
            const bool bCrit = Natural >= CriticalThreshold(Reactor);
            const bool bHit = bCrit || (Natural != 1 && Total >= Mover.ArmorClass);
            int32 Damage = 0;
            if (bHit)
            {
                DiceLog = &Result.EffectDice;
                Damage = Roll(Reactor.DamageDie) + Reactor.DamageBonus;
                if (bCrit) { Damage += Roll(Reactor.DamageDie); }
                DiceLog = nullptr;
                ApplyDamage(Mover, Damage, bCrit);
            }
            Result.Feedback += FString::Printf(TEXT(" | OA %s: d20 %d+%d=%d vs AC %d %s %d"),
                *Reactor.Name, Natural, Reactor.AttackBonus, Total, Mover.ArmorClass, bHit ? TEXT("HIT") : TEXT("MISS"), Damage);
            if (Mover.Life != ECombatLife::Conscious) { return Step - 1; }
        }
    }
    Mover.Cell = Path.Last();
    return Path.Num() - 1;
}
