// Spells phase 5: reactions, beams, summons, terrain and utility spells as tactical equivalents (SRD 5.2.1).
#include "Combat/CombatBattle.h"
#include "Rules/SpellUtilityRules.h"
#include "Rules/SpellEffectRules.h"
#include "Rules/SpellTacticsRules.h"
#include "Rules/SpellCatalogRules.h"
#include "Rules/SpellcastingRules.h"
#include "Rules/ProficiencyRules.h"
#include "Rules/AreaRules.h"
#include "Rules/DamageRules.h"
#include "Rules/AbilityRules.h"
#include "Party/BuildCatalog.h"
#include "Rules/SpellRules.h"
#include "Rules/CheckRules.h"
#include "Rules/WarlockRules.h"
#include "Party/FeatureKit.h"

namespace CombatSpellUtility
{
int32 Grid(FIntPoint A, FIntPoint B) { return FMath::Max(FMath::Abs(A.X - B.X), FMath::Abs(A.Y - B.Y)); }
int32 Manhattan(FIntPoint A, FIntPoint B) { return FMath::Abs(A.X - B.X) + FMath::Abs(A.Y - B.Y); }
}
using namespace CombatSpellUtility;

int32 FCombatBattle::ReactionSlot(const FCombatantState& Unit, const FSrdSpell& Spell, EAdventurerRole& OutClass) const
{
    const int32 PrimaryLevels = Unit.Level - Unit.SecondaryLevel;
    if (FSpellCatalogRules::CanLearn(Unit.AdventurerRole, PrimaryLevels, Spell)) { OutClass = Unit.AdventurerRole; }
    else if (Unit.SecondaryLevel > 0 && FSpellCatalogRules::CanLearn(Unit.SecondaryRole, Unit.SecondaryLevel, Spell)) { OutClass = Unit.SecondaryRole; }
    else { return 0; }
    if (!Unit.PreparedSpells.IsEmpty() && !Unit.PreparedSpells.Contains(FString(Spell.Id))) { return 0; }
    for (int32 Level = FMath::Max(1, Spell.Level); Level <= FSpellRules::MaxSpellLevel; ++Level)
    {
        const int32 Left = Level == 1 ? Unit.SpellSlots : Unit.SlotsByLevel.IsValidIndex(Level - 1) ? Unit.SlotsByLevel[Level - 1] : 0;
        if (Left <= 0) { continue; }
        if (Level == 1 && FBuildCatalog::Find(Unit.BuildId))
        {
            FCombatantState After = Unit; SpendSpellSlot(After);
            if (!FBuildCatalog::ValidResources(After)) { continue; } // never spend a slot a kit has reserved
        }
        return Level;
    }
    return 0;
}

void FCombatBattle::SpendSlotLevel(FCombatantState& Unit, int32 Slot)
{
    if (Slot == 1) { SpendSpellSlot(Unit); }
    else if (Unit.SlotsByLevel.IsValidIndex(Slot - 1) && Unit.SlotsByLevel[Slot - 1] > 0) { --Unit.SlotsByLevel[Slot - 1]; }
}

void FCombatBattle::SpellReactionsOnDamaged(int32 AttackerId, int32 TargetId, FCombatResult& Result)
{
    // SRD 5.2.1 Hellish Rebuke: reaction to taking damage from a creature within 60 feet that you can see.
    const FSrdSpell* S = FSpellCatalogRules::Find(TEXT("hellish-rebuke"));
    FCombatantState* Hero = MutableFind(TargetId);
    const FCombatantState* Attacker = Find(AttackerId);
    if (!S || !Hero || !Attacker || Hero->Side != ECombatSide::Party || Hero->Life != ECombatLife::Conscious || !Hero->bReactionAvailable ||
        IsIncapacitated(*Hero) || SpellBlocksReactions(TargetId) || Attacker->Life == ECombatLife::Dead || Attacker->Side == Hero->Side ||
        Manhattan(Hero->Cell, Attacker->Cell) > S->RangeFeet / FAreaRules::FeetPerCell || !HasLineOfSight(Hero->Cell, Attacker->Cell)) { return; }
    EAdventurerRole Class = EAdventurerRole::None;
    const int32 Slot = ReactionSlot(*Hero, *S, Class);
    if (Slot == 0) { return; }
    SpendSlotLevel(*Hero, Slot);
    Hero->bReactionAvailable = false;
    const int32 Mod = AbilityModifier(TargetId, FSpellcastingRules::SpellcastingAbility(Class));
    const int32 Dc = FSpellcastingRules::SaveDc(FProficiencyRules::ProficiencyBonus(Hero->Level), Mod);
    const int32 Count = FSpellCatalogRules::DiceCount(*S, Hero->Level, Slot);
    const int32 Rolled = RollDice(Count, S->DieSides);
    const bool bSaved = RollSpellSave(AttackerId, S->SaveAbility, Dc, nullptr);
    const int32 Dealt = FDamageRules::Adjust(FSpellcastingRules::DamageAfterSave(Rolled, bSaved, S->bHalfOnSave), S->DamageType, DamageDefenses(AttackerId));
    if (FCombatantState* Victim = MutableFind(AttackerId)) { ApplyDamage(*Victim, Dealt); }
    Result.RollModeSources.Add(FString::Printf(TEXT("Hellish Rebuke (reaction, level %d slot): %s %s, %d fire"), Slot, *Attacker->Name, bSaved ? TEXT("saves") : TEXT("fails"), Dealt));
    FActiveSpell Probe; Probe.SpellId = S->Id; Probe.CasterId = TargetId; Probe.SlotLevel = Slot; Probe.Dc = Dc; Probe.RoundsLeft = 0;
    EmitSpellEvent(ESpellEventKind::Cast, Probe, {AttackerId});
    CheckOutcome();
}

// ======================================================================================
// Spells phase 6: the remaining cantrips and level 1-4 spells as tactical equivalents.
// ======================================================================================
namespace CombatSpellUtilityLocal
{
bool Is(const FSrdSpell& S, const TCHAR* Id) { return FCString::Stricmp(S.Id, Id) == 0; }
bool IsId(const FString& A, const TCHAR* Id) { return A.Equals(Id, ESearchCase::IgnoreCase); }
/** Zone kinds for SpellZoneAt: 1 bright light, 2 silence, 3 heavily obscured. */
uint8 ZoneKind(const FString& Id)
{
    if (const FSpellUtility* U = FSpellUtilityRules::Find(Id))
    {
        if (U->Kind == ESpellUtility::Light) { return 1; }
        if (U->Kind == ESpellUtility::Silence) { return 2; }
        if (U->Kind == ESpellUtility::Obscure) { return 3; }
    }
    return 0;
}
bool FollowsCaster(const FString& Id) { const FSpellZone* Z = FSpellTacticsRules::Zone(Id); return Z && Z->bFollowsCaster; }
/** Spells SRD 5.2.1 calls curses, ended by Remove Curse. */
bool IsCurse(const FString& Id) { return IsId(Id, TEXT("hex")) || IsId(Id, TEXT("bestow-curse")); }
FIntPoint GustDir(int32 Value) { return FIntPoint(Value / 3 - 1, Value % 3 - 1); }
}
using namespace CombatSpellUtilityLocal;

const FActiveSpell* FCombatBattle::SpellExtraOn(int32 Id, uint32 Bit) const
{
    for (const FActiveSpell& A : State.ActiveSpells)
    {
        const FSpellEffectSpec* Spec = A.TargetId == Id ? FSpellEffectRules::Find(A.SpellId) : nullptr;
        if (Spec && (Spec->Extra & Bit)) { return &A; }
        // Blink: while Ethereal the caster is out of reach like a sealed sphere.
        if (Spec && (Bit & SpellEffectExtra::Sphere) && (Spec->Extra & SpellEffectExtra::Blink) && A.Value == 1) { return &A; }
    }
    return nullptr;
}

bool FCombatBattle::SpellZoneAt(FIntPoint Cell, uint8 Kind) const
{
    bool bFound = false;
    for (const FActiveSpell& A : State.ActiveSpells)
    {
        const uint8 K = ZoneKind(A.SpellId);
        if (K == 0 || !ZoneCovers(A, Cell)) { continue; }
        // Magical darkness: light spells can't brighten it.
        if (Kind == 1 && IsId(A.SpellId, TEXT("darkness"))) { return false; }
        bFound |= K == Kind;
    }
    return bFound;
}

bool FCombatBattle::SpellCanSee(int32 ViewerId, int32 TargetId, bool bInvisible) const
{
    const FCombatantState* Viewer = Find(ViewerId);
    const FCombatantState* Target = Find(TargetId);
    if (!Viewer || !Target || ViewerId == TargetId) { return true; }
    // Heavily obscured (Fog Cloud, Darkness): sight into or out of the cloud is blocked. Darkvision doesn't pierce magical darkness.
    if (SpellZoneAt(Viewer->Cell, 3) || SpellZoneAt(Target->Cell, 3)) { return false; }
    if (bInvisible && HasCondition(*Target, ECombatCondition::Invisible) && !SpellExtraOn(ViewerId, SpellEffectExtra::SeeInvisible)) { return false; }
    return true;
}

int32 FCombatBattle::SpellCheckBonus(const FCombatantState& Unit, EAbility Ability, ERollMode& Mode, bool bStealth)
{
    int32 Adv = Mode == ERollMode::Advantage ? 1 : 0, Dis = Mode == ERollMode::Disadvantage ? 1 : 0, Bonus = 0;
    for (const FActiveSpell& A : State.ActiveSpells)
    {
        const FSpellEffectSpec* Spec = A.TargetId == Unit.Id ? FSpellEffectRules::Find(A.SpellId) : nullptr;
        if (!Spec) { continue; }
        // Guidance: +1d4 to ability checks with the chosen skill (the contested skills here). Enhance Ability: advantage on the chosen ability.
        if (Spec->Extra & SpellEffectExtra::CheckDie) { Bonus += Roll(4); }
        if ((Spec->Extra & SpellEffectExtra::CheckAdvantage) && static_cast<EAbility>(A.Value) == Ability) { ++Adv; }
        // Hex: disadvantage on checks with the chosen ability.
        if ((Spec->Extra & SpellEffectExtra::CheckDisadvantage) && static_cast<EAbility>(A.Value) == Ability) { ++Dis; }
    }
    if (bStealth)
    {
        // Pass without Trace: +10 to Dexterity (Stealth) checks for creatures of the caster's choice in the aura.
        for (const FActiveSpell& A : State.ActiveSpells)
        {
            const FCombatantState* Caster = IsId(A.SpellId, TEXT("pass-without-trace")) ? Find(A.CasterId) : nullptr;
            if (Caster && Caster->Side == Unit.Side && ZoneCovers(A, Unit.Cell)) { Bonus += 10; break; }
        }
    }
    Mode = FCheckRules::CombineMode(Adv, Dis);
    return Bonus;
}

int32 FCombatBattle::ResistanceCut(int32 TargetId, EDamageType Type, int32 Damage)
{
    // SRD 5.2.1 Resistance: once per turn, damage of the chosen type is reduced by 1d4.
    if (Damage <= 0) { return Damage; }
    for (FActiveSpell& A : State.ActiveSpells)
    {
        const FSpellEffectSpec* Spec = A.TargetId == TargetId ? FSpellEffectRules::Find(A.SpellId) : nullptr;
        if (!Spec || !(Spec->Extra & SpellEffectExtra::ReduceChosen) || A.Value != static_cast<int32>(Type) || A.TriggeredThisTurn.Contains(TargetId)) { continue; }
        A.TriggeredThisTurn.Add(TargetId);
        return FMath::Max(0, Damage - Roll(4));
    }
    return Damage;
}

int32 FCombatBattle::SpellDamageIntake(FCombatantState& Target, int32 Damage)
{
    // Resilient Sphere: nothing inside can be damaged by anything outside.
    if (Damage > 0 && SpellExtraOn(Target.Id, SpellEffectExtra::Sphere)) { return 0; }
    return Damage;
}

void FCombatBattle::SpellDamageShared(int32 TargetId, int32 Real)
{
    // SRD 5.2.1 Warding Bond: each time the target takes damage, the caster takes the same amount.
    if (bSharingDamage) { return; }
    TArray<int32> Casters;
    for (const FActiveSpell& A : State.ActiveSpells)
    {
        const FSpellEffectSpec* Spec = A.TargetId == TargetId ? FSpellEffectRules::Find(A.SpellId) : nullptr;
        if (Spec && (Spec->Extra & SpellEffectExtra::ShareDamage) && A.CasterId != TargetId) { Casters.Add(A.CasterId); }
    }
    bSharingDamage = true;
    for (const int32 Id : Casters)
    {
        FCombatantState* Caster = MutableFind(Id);
        if (!Caster || Caster->Life != ECombatLife::Conscious) { continue; }
        ApplyDamage(*Caster, Real);
        if (Caster->HP == 0)
        {
            // The bond ends when the caster drops to 0.
            const int32 Index = State.ActiveSpells.IndexOfByPredicate([&](const FActiveSpell& A) { return A.CasterId == Id && IsId(A.SpellId, TEXT("warding-bond")); });
            if (Index != INDEX_NONE) { RemoveActiveSpell(Index); }
        }
    }
    bSharingDamage = false;
}

bool FCombatBattle::AuraOfLifeRevives(FCombatantState& Unit)
{
    // SRD 5.2.1 Aura of Life: an ally in the aura who starts its turn at 0 HP regains 1 HP.
    if (Unit.Life != ECombatLife::Downed) { return false; }
    for (const FActiveSpell& A : State.ActiveSpells)
    {
        const FCombatantState* Caster = IsId(A.SpellId, TEXT("aura-of-life")) ? Find(A.CasterId) : nullptr;
        if (!Caster || Caster->Life != ECombatLife::Conscious || Caster->Side != Unit.Side || !ZoneCovers(A, Unit.Cell)) { continue; }
        Unit.HP = 1; Unit.Life = ECombatLife::Conscious; Unit.RescueTurnsLeft = 0; Unit.DeathSaveSuccesses = 0; Unit.DeathSaveFailures = 0;
        EmitSpellEvent(ESpellEventKind::Trigger, A, {Unit.Id});
        return true;
    }
    return false;
}

void FCombatBattle::SpellTurnControl(FCombatantState& Unit, const FActiveSpell& A, const FSpellEffectSpec& Spec)
{
    if (Unit.Life != ECombatLife::Conscious || ActiveId() != Unit.Id) { return; }
    const int32 UnitId = Unit.Id;
    FCombatResult Scratch;
    auto Adjacent = [&](bool bAllies) -> int32
    {
        TArray<int32> Out;
        for (const FCombatantState& O : State.Encounter.Combatants)
        {
            if (O.Id == UnitId || O.Life != ECombatLife::Conscious || Manhattan(O.Cell, Unit.Cell) > 1) { continue; }
            if (bAllies && O.Side != Unit.Side) { continue; }
            Out.Add(O.Id);
        }
        return Out.IsEmpty() ? INDEX_NONE : Out[bAllies ? 0 : (Roll(Out.Num()) - 1)];
    };
    if (Spec.Extra & SpellEffectExtra::Confused)
    {
        // SRD 5.2.1 Confusion, d10 at turn start: 1 wanders (no action), 2-6 no action or move, 7-8 melee a random adjacent creature, 9-10 acts normally. No reactions.
        Unit.bReactionAvailable = false;
        const int32 D = Roll(10);
        if (D <= 6) { State.bActionAvailable = false; State.bBonusActionAvailable = false; if (D >= 2) { State.MovementLeft = 0; } }
        else if (D <= 8)
        {
            State.bActionAvailable = false; State.bBonusActionAvailable = false; State.MovementLeft = 0;
            if (const int32 Victim = Adjacent(false); Victim != INDEX_NONE) { Strike(UnitId, Victim, 0, Scratch); }
        }
        EmitSpellEvent(ESpellEventKind::Tick, A, {UnitId});
    }
    if (Spec.Extra & SpellEffectExtra::Pacified)
    {
        // Suggestion: the creature pursues a harmless course of action; it spends its turn on it.
        State.bActionAvailable = false; State.bBonusActionAvailable = false;
    }
    if (Spec.Extra & SpellEffectExtra::Dominated)
    {
        // Dominate: the caster commands it; it attacks one of its own allies next to it, then holds.
        if (const int32 Victim = Adjacent(true); Victim != INDEX_NONE) { Strike(UnitId, Victim, 0, Scratch); EmitSpellEvent(ESpellEventKind::Tick, A, {Victim}); }
        State.bActionAvailable = false; State.bBonusActionAvailable = false;
    }
}

bool FCombatBattle::UtilityTargets(int32 Id, const FSrdSpell& S, int32 SlotLevel, TArray<int32>& Out) const
{
    const FSpellUtility* U = FSpellUtilityRules::Find(S.Id);
    if (!U || U->Kind == ESpellUtility::Exception || U->Kind == ESpellUtility::Engine || U->Kind == ESpellUtility::Reaction ||
        U->Kind == ESpellUtility::Beams || U->Kind == ESpellUtility::ChoiceAttack || U->Kind == ESpellUtility::Buff || U->Kind == ESpellUtility::Debuff) { return false; }
    Out.Reset();
    const FCombatantState* Unit = Find(Id);
    int32 Slot = 0; EAdventurerRole Class = EAdventurerRole::None;
    if (!Unit || !UsableSpell(*Unit, S.Id, SlotLevel, Slot, Class)) { return true; }
    const int32 Range = S.RangeFeet == 0 ? 0 : FMath::Max(1, S.RangeFeet / FAreaRules::FeetPerCell);
    auto Reach = [&](const FCombatantState& T, int32 R) { return T.Id == Id || (Manhattan(Unit->Cell, T.Cell) <= R && HasLineOfSight(Unit->Cell, T.Cell)); };
    if (Is(S, TEXT("true-strike"))) { Out = LegalTargets(Id, ECombatCommandType::Attack); return true; }
    if (Is(S, TEXT("spare-the-dying")))
    {
        const int32 R = 3 << (FSpellUtilityRules::CantripDice(Unit->Level) - 1); // 15/30/60/120 ft
        for (const FCombatantState& T : State.Encounter.Combatants) { if (T.Side == Unit->Side && T.Life == ECombatLife::Downed && T.DeathSaveSuccesses < 3 && Reach(T, R)) { Out.Add(T.Id); } }
        return true;
    }
    if (Is(S, TEXT("revivify")))
    {
        for (const FCombatantState& T : State.Encounter.Combatants) { if (T.Side == Unit->Side && T.Life == ECombatLife::Dead && Reach(T, 1)) { Out.Add(T.Id); } }
        return true;
    }
    if (Is(S, TEXT("lesser-restoration")))
    {
        for (const FCombatantState& T : State.Encounter.Combatants)
        {
            const bool bAiling = T.Conditions.ContainsByPredicate([](const FCombatCondition& C) { return C.Condition == ECombatCondition::Blinded || C.Condition == ECombatCondition::Deafened || C.Condition == ECombatCondition::Paralyzed || C.Condition == ECombatCondition::Poisoned; });
            if (T.Side == Unit->Side && T.Life == ECombatLife::Conscious && bAiling && Reach(T, 1)) { Out.Add(T.Id); }
        }
        return true;
    }
    if (Is(S, TEXT("dispel-magic")))
    {
        for (const FCombatantState& T : State.Encounter.Combatants)
        {
            if (T.Life == ECombatLife::Dead || !Reach(T, Range)) { continue; }
            if (State.ActiveSpells.ContainsByPredicate([&](const FActiveSpell& A) { return A.TargetId == T.Id || (A.CasterId == T.Id && A.TargetId == INDEX_NONE && ZoneCovers(A, T.Cell)); })) { Out.Add(T.Id); }
        }
        return true;
    }
    if (Is(S, TEXT("remove-curse")))
    {
        for (const FCombatantState& T : State.Encounter.Combatants)
        {
            if (T.Life != ECombatLife::Dead && Reach(T, 1) && State.ActiveSpells.ContainsByPredicate([&](const FActiveSpell& A) { return A.TargetId == T.Id && IsCurse(A.SpellId); })) { Out.Add(T.Id); }
        }
        return true;
    }
    if (Is(S, TEXT("gust-of-wind")))
    {
        for (const FCombatantState& T : State.Encounter.Combatants) { if (T.Side != Unit->Side && T.Life == ECombatLife::Conscious && T.Id != Id && Reach(T, 12)) { Out.Add(T.Id); } }
        return true;
    }
    // Self-centred: teleports, summons, emanations.
    if (U->Kind == ESpellUtility::Teleport || U->Kind == ESpellUtility::Summon || Range == 0 || FollowsCaster(S.Id) || Is(S, TEXT("guardian-of-faith"))) { Out.Add(Id); return true; }
    // Point zones: aim at any creature in range and sight (the cell it stands on).
    for (const FCombatantState& T : State.Encounter.Combatants) { if (T.Life != ECombatLife::Dead && Reach(T, Range)) { Out.Add(T.Id); } }
    return true;
}

bool FCombatBattle::UtilityCastBlocked(const FCombatCommand& Command, const FSrdSpell& S, FString& Why) const
{
    const FCombatantState* User = Find(Command.ActorId);
    if (!User) { return false; }
    if (Is(S, TEXT("misty-step")) || Is(S, TEXT("dimension-door")))
    {
        const int32 Reach = Is(S, TEXT("misty-step")) ? 30 / FAreaRules::FeetPerCell : 500 / FAreaRules::FeetPerCell;
        if (!IsOpen(Command.Destination, Command.ActorId) || Manhattan(User->Cell, Command.Destination) > Reach || Command.Destination == User->Cell)
        { Why = TEXT("Teleport destination must be an open cell in range."); return true; }
        if (Is(S, TEXT("misty-step")) && !HasLineOfSight(User->Cell, Command.Destination)) { Why = TEXT("Misty Step needs a space you can see."); return true; }
    }
    return false;
}

void FCombatBattle::PushUnit(int32 Id, FIntPoint Dir, int32 Count)
{
    for (int32 Step = 0; Step < Count; ++Step)
    {
        FCombatantState* Unit = MutableFind(Id);
        if (!Unit || Unit->Life != ECombatLife::Conscious) { return; }
        const FIntPoint From = Unit->Cell, To = From + Dir;
        if (!IsOpen(To, Id)) { return; }
        Unit->Cell = To;
        SpellsOnStep(Id, From, To);
    }
}

bool FCombatBattle::UtilityAttack(int32 ActorId, int32 TargetId, int32 AttackBonus, FCombatResult& Result, bool& bCritical, bool bRanged)
{
    const FCombatantState* User = Find(ActorId);
    const FCombatantState* Target = Find(TargetId);
    bCritical = false;
    if (!User || !Target) { return false; }
    const FAttackRollMode Mode = AttackRollMode(ActorId, TargetId);
    bool bCrowded = false;
    if (bRanged)
    {
        for (const FCombatantState& Other : State.Encounter.Combatants) { bCrowded |= Other.Side != User->Side && Other.Life == ECombatLife::Conscious && !IsIncapacitated(Other) && Manhattan(Other.Cell, User->Cell) <= 1; }
    }
    const ERollMode RollMode = FCheckRules::CombineMode(Mode.Advantage.Num(), Mode.Disadvantage.Num() + (bCrowded ? 1 : 0));
    const int32 First = Roll(20);
    const int32 Second = RollMode != ERollMode::Normal ? Roll(20) : First;
    const int32 Natural = RollMode == ERollMode::Advantage ? FMath::Max(First, Second) : RollMode == ERollMode::Disadvantage ? FMath::Min(First, Second) : First;
    const int32 Bonus = AttackBonus + SpellAttackBonus(ActorId, false, Result);
    const int32 Ac = EffectiveArmorClass(TargetId);
    bCritical = Natural == 20;
    const bool bHit = bCritical || (Natural != 1 && Natural + Bonus >= Ac);
    if (Result.AttackRolls.IsEmpty())
    {
        Result.RollMode = RollMode; Result.AttackRolls.Add(First); if (RollMode != ERollMode::Normal) { Result.AttackRolls.Add(Second); }
        Result.NaturalRoll = Natural; Result.AttackBonus = Bonus; Result.AttackTotal = Natural + Bonus; Result.TargetArmorClass = Ac; Result.bCritical = bCritical;
    }
    return bHit;
}

int32 FCombatBattle::UtilityDamage(int32 TargetId, int32 Amount, EDamageType Type)
{
    FCombatantState* Victim = MutableFind(TargetId);
    if (!Victim || Victim->Life == ECombatLife::Dead || Amount <= 0) { return 0; }
    const int32 Dealt = ResistanceCut(TargetId, Type, FDamageRules::Adjust(Amount, Type, DamageDefenses(TargetId)));
    ApplyDamage(*Victim, Dealt);
    return Dealt;
}

void FCombatBattle::ZoneCastExtras(int32 Serial, const FCombatCommand& Command)
{
    const int32 Index = IndexOfSpell(Serial);
    if (Index == INDEX_NONE) { return; }
    FActiveSpell& Entry = State.ActiveSpells[Index];
    const FSrdSpell* S = FSpellCatalogRules::Find(Entry.SpellId);
    if (!S) { return; }
    Entry.Value = Command.Choice;
    if (Is(*S, TEXT("fog-cloud"))) { Entry.RadiusCells = (20 + 20 * FMath::Max(0, Entry.SlotLevel - 1)) / FAreaRules::FeetPerCell; }
    if (Is(*S, TEXT("plant-growth"))) { Entry.RoundsLeft = -1; } // overgrowth lasts until cleared
    if (Is(*S, TEXT("guardian-of-faith"))) { Entry.Value = 0; if (IsInside(Command.Destination) && Command.Destination != FIntPoint::ZeroValue) { Entry.Origin = Command.Destination; } }
    if (Is(*S, TEXT("continual-flame"))) { Entry.RoundsLeft = -1; }
    // Daylight dispels magical Darkness of level 3 or lower it overlaps; Darkness dispels light of level 2 or lower.
    const FActiveSpell Copy = Entry;
    const bool bDaylight = Is(*S, TEXT("daylight")), bDarkness = Is(*S, TEXT("darkness"));
    if (!bDaylight && !bDarkness) { return; }
    for (int32 I = State.ActiveSpells.Num() - 1; I >= 0; --I)
    {
        const FActiveSpell& Other = State.ActiveSpells[I];
        const FSrdSpell* O = FSpellCatalogRules::Find(Other.SpellId);
        if (!O || Other.Serial == Copy.Serial) { continue; }
        const bool bOverlap = ZoneCells(Other).ContainsByPredicate([&](FIntPoint C) { return ZoneCovers(Copy, C); });
        if (!bOverlap) { continue; }
        if ((bDaylight && Is(*O, TEXT("darkness"))) || (bDarkness && ZoneKind(Other.SpellId) == 1 && O->Level <= 2)) { RemoveActiveSpell(I); }
    }
}

void FCombatBattle::SummonsOnTurnStart(int32 UnitId)
{
    TArray<int32> Serials;
    for (const FActiveSpell& A : State.ActiveSpells)
    {
        const FSpellUtility* U = FSpellUtilityRules::Find(A.SpellId);
        if (U && (U->Kind == ESpellUtility::Summon || U->Kind == ESpellUtility::Push)) { Serials.Add(A.Serial); }
    }
    for (const int32 Serial : Serials)
    {
        const int32 Index = IndexOfSpell(Serial);
        const FCombatantState* Caster = Index != INDEX_NONE ? Find(State.ActiveSpells[Index].CasterId) : nullptr;
        if (!Caster || Caster->Life != ECombatLife::Conscious) { continue; }
        FActiveSpell& A = State.ActiveSpells[Index];
        if (IsId(A.SpellId, TEXT("gust-of-wind")))
        {
            // A creature starting its turn in the line: STR save or pushed 15 ft away from the caster.
            const FCombatantState* Unit = Find(UnitId);
            if (!Unit || UnitId == A.CasterId || Unit->Life != ECombatLife::Conscious) { continue; }
            const FIntPoint Dir = GustDir(A.Value);
            const FIntPoint Perp(-Dir.Y, Dir.X);
            bool bIn = false;
            for (int32 K = 1; K <= 12 && !bIn; ++K) { const FIntPoint P = Caster->Cell + Dir * K; bIn = Unit->Cell == P || Unit->Cell == P + Perp; }
            if (!bIn) { continue; }
            const FActiveSpell Copy = A;
            if (!RollSpellSave(UnitId, EAbility::Strength, Copy.Dc, nullptr)) { PushUnit(UnitId, Dir, 3); }
            EmitSpellEvent(ESpellEventKind::Trigger, Copy, {UnitId});
            continue;
        }
        if (A.CasterId != UnitId) { continue; }
        const FActiveSpell Copy = A;
        // The summon acts on its caster's turn: the nearest conscious foe of the caster within reach of the token.
        const bool bSteed = IsId(Copy.SpellId, TEXT("find-steed")), bInsect = IsId(Copy.SpellId, TEXT("giant-insect")), bHound = IsId(Copy.SpellId, TEXT("faithful-hound"));
        FIntPoint Token = bSteed ? Caster->Cell : Copy.Origin;
        int32 Best = INDEX_NONE, BestDistance = MAX_int32;
        for (const FCombatantState& O : State.Encounter.Combatants)
        {
            if (O.Side == Caster->Side || O.Life != ECombatLife::Conscious) { continue; }
            const int32 D = Manhattan(Token, O.Cell);
            if (D < BestDistance) { Best = O.Id; BestDistance = D; }
        }
        if (Best == INDEX_NONE) { continue; }
        if (bInsect && BestDistance > 1)
        {
            // Giant Insect: flies/climbs 40 ft toward its prey (the token is not a grid occupant).
            const FIntPoint Goal = Find(Best)->Cell;
            for (int32 Step = 0; Step < 8 && Manhattan(Token, Goal) > 1; ++Step)
            {
                const FIntPoint Delta = Goal - Token;
                Token += FMath::Abs(Delta.X) >= FMath::Abs(Delta.Y) ? FIntPoint(FMath::Sign(Delta.X), 0) : FIntPoint(0, FMath::Sign(Delta.Y));
            }
            A.Origin = Token;
            BestDistance = Manhattan(Token, Goal);
        }
        if (BestDistance > 1) { continue; }
        FCombatResult Scratch;
        int32 Dealt = 0;
        if (bHound)
        {
            // Faithful Hound: a foe within 5 ft: DEX save or 4d8 force.
            if (!RollSpellSave(Best, EAbility::Dexterity, Copy.Dc, nullptr)) { Dealt = UtilityDamage(Best, RollDice(4, 8), EDamageType::Force); }
        }
        else
        {
            // Otherworldly Steed slam 1d8 + slot radiant; Giant Insect (spider) jab 1d6 + 3 + slot piercing plus 1d4 + slot poison. Spell attack bonus.
            const FCombatantState* Prey = Find(Best);
            const int32 Ac = EffectiveArmorClass(Best);
            const int32 Natural = Roll(20);
            const bool bCrit = Natural == 20;
            if (Prey && (bCrit || (Natural != 1 && Natural + Copy.AttackBonus >= Ac)))
            {
                const int32 M = bCrit ? 2 : 1;
                if (bSteed) { Dealt = UtilityDamage(Best, RollDice(M, 8) + Copy.SlotLevel, EDamageType::Radiant); }
                else
                {
                    Dealt = UtilityDamage(Best, RollDice(M, 6) + 3 + Copy.SlotLevel, EDamageType::Piercing);
                    Dealt += UtilityDamage(Best, RollDice(M, 4) + Copy.SlotLevel, EDamageType::Poison);
                }
            }
        }
        if (const int32 I = IndexOfSpell(Serial); I != INDEX_NONE) { EmitSpellEvent(ESpellEventKind::Trigger, State.ActiveSpells[I], {Best}); }
        (void)Dealt;
    }
}

bool FCombatBattle::ResolveUtilitySpell(const FCombatCommand& Command, const FSrdSpell& S, int32 Slot, int32 Dc, int32 AttackBonus, int32 Mod, FCombatResult& Result)
{
    const FSpellUtility* U = FSpellUtilityRules::Find(S.Id);
    const bool bIceKnife = Is(S, TEXT("ice-knife"));
    if (!bIceKnife && (!U || U->Kind == ESpellUtility::Exception || U->Kind == ESpellUtility::Engine || U->Kind == ESpellUtility::Reaction ||
        U->Kind == ESpellUtility::Buff || U->Kind == ESpellUtility::Debuff || ZoneKind(S.Id) != 0 || U->Kind == ESpellUtility::Zone)) { return false; }
    FCombatantState* User = MutableFind(Command.ActorId);
    if (!User) { return false; }
    const int32 CasterId = User->Id;
    const FString SlotText = Slot == 0 ? FString(TEXT("cantrip")) : FString::Printf(TEXT("level %d slot"), Slot);
    FString Lines;
    TArray<int32> Touched;
    FActiveSpell Probe; Probe.SpellId = S.Id; Probe.CasterId = CasterId; Probe.SlotLevel = Slot; Probe.Dc = Dc; Probe.AttackBonus = AttackBonus; Probe.RoundsLeft = 0;
    auto Name = [&](int32 Id) { const FCombatantState* T = Find(Id); return T ? T->Name : FString(TEXT("?")); };
    auto Logged = [&](int32 Count, int32 Sides) { TArray<FDieRoll>* Saved = DiceLog; DiceLog = &Result.EffectDice; const int32 R = RollDice(Count, Sides); DiceLog = Saved; return R; };
    Result.bHit = false;

    if (U && U->Kind == ESpellUtility::Beams && Command.ExtraTargetIds.IsEmpty()) { return false; }
    if (U && U->Kind == ESpellUtility::Beams)
    {
        // Each beam/ray/dart picks its own target (Command.TargetId, then ExtraTargetIds; spare beams repeat the first target).
        const int32 Beams = FSpellUtilityRules::Beams(S, User->Level, Slot);
        // The slot/action is already spent here, so validate each extra aim directly (foe, conscious, in range, seen).
        const int32 BeamRange = FMath::Max(1, S.RangeFeet / FAreaRules::FeetPerCell);
        auto Aimable = [&](int32 Id)
        {
            const FCombatantState* T = Find(Id);
            return T && T->Side != User->Side && T->Life == ECombatLife::Conscious && Manhattan(User->Cell, T->Cell) <= BeamRange &&
                HasLineOfSight(User->Cell, T->Cell) && SpellCanSee(CasterId, Id) && !SpellExtraOn(Id, SpellEffectExtra::Sphere);
        };
        TArray<int32> Aims = {Command.TargetId};
        for (const int32 Id : Command.ExtraTargetIds) { if (Aimable(Id)) { Aims.Add(Id); } }
        const bool bMissile = Is(S, TEXT("magic-missile")), bBlast = Is(S, TEXT("eldritch-blast"));
        const int32 Agonizing = bBlast && FFeatureKit::HasInvocation(*User, EInvocation::AgonizingBlast) ? Mod : 0;
        Result.DamageType = S.DamageType;
        for (int32 Beam = 0; Beam < Beams; ++Beam)
        {
            const int32 Aim = Aims.IsValidIndex(Beam) ? Aims[Beam] : Aims[0];
            const FCombatantState* T = Find(Aim);
            if (!T || T->Life == ECombatLife::Dead) { continue; }
            bool bCrit = false;
            const bool bHit = bMissile || UtilityAttack(CasterId, Aim, AttackBonus, Result, bCrit, true);
            int32 Dealt = 0;
            if (bHit)
            {
                const int32 Dice = (bMissile ? 1 : bBlast ? 1 : 2) * (bCrit ? 2 : 1);
                const int32 Raw = Logged(Dice, bMissile ? 4 : bBlast ? 10 : 6) + (bMissile ? 1 : Agonizing);
                Dealt = UtilityDamage(Aim, Raw, S.DamageType);
                Result.Damage += Dealt; Result.bHit = true; Result.bCritical |= bCrit;
            }
            Touched.AddUnique(Aim);
            Lines += FString::Printf(TEXT(" | %s %d: %s %s"), bMissile ? TEXT("dart") : TEXT("beam"), Beam + 1, *Name(Aim), bHit ? *FString::Printf(TEXT("%d"), Dealt) : TEXT("miss"));
        }
    }
    else if (Is(S, TEXT("chromatic-orb")) || Is(S, TEXT("sorcerous-burst")))
    {
        // Chosen type (Command.Choice as EDamageType; default the first listed).
        static const EDamageType Orb[] = {EDamageType::Acid, EDamageType::Cold, EDamageType::Fire, EDamageType::Lightning, EDamageType::Poison, EDamageType::Thunder};
        static const EDamageType Burst[] = {EDamageType::Acid, EDamageType::Cold, EDamageType::Fire, EDamageType::Lightning, EDamageType::Poison, EDamageType::Psychic, EDamageType::Thunder};
        const bool bOrb = Is(S, TEXT("chromatic-orb"));
        EDamageType Type = bOrb ? Orb[0] : Burst[0];
        for (const EDamageType T : bOrb ? TArrayView<const EDamageType>(Orb) : TArrayView<const EDamageType>(Burst)) { if (static_cast<int32>(T) == Command.Choice) { Type = T; } }
        Result.DamageType = Type;
        int32 Aim = Command.TargetId, Leaps = 0;
        TArray<int32> Hit;
        while (Aim != INDEX_NONE)
        {
            bool bCrit = false;
            const bool bHitNow = UtilityAttack(CasterId, Aim, AttackBonus, Result, bCrit, true);
            Hit.Add(Aim); Touched.AddUnique(Aim);
            int32 Dealt = 0;
            bool bPair = false;
            if (bHitNow)
            {
                TArray<int32> Dice;
                TArray<FDieRoll>* Saved = DiceLog; DiceLog = &Result.EffectDice;
                const int32 Base = (bOrb ? 3 + FMath::Max(0, Slot - 1) : FSpellUtilityRules::CantripDice(User->Level)) * (bCrit ? 2 : 1);
                for (int32 I = 0; I < Base; ++I) { Dice.Add(Roll(8)); }
                int32 Total = 0;
                for (const int32 D : Dice) { Total += D; }
                if (!bOrb)
                {
                    // Sorcerous Burst: each 8 rolls another d8, up to the spellcasting modifier in extra dice.
                    int32 Pending = 0, Extra = 0;
                    for (const int32 D : Dice) { Pending += D == 8 ? 1 : 0; }
                    while (Pending > 0 && Extra < FMath::Max(0, Mod)) { const int32 D = Roll(8); Total += D; ++Extra; --Pending; Pending += D == 8 ? 1 : 0; }
                }
                DiceLog = Saved;
                TSet<int32> Seen;
                for (const int32 D : Dice) { bool bAlready = false; Seen.Add(D, &bAlready); bPair |= bAlready; }
                Dealt = UtilityDamage(Aim, Total, Type);
                Result.Damage += Dealt; Result.bHit = true; Result.bCritical |= bCrit;
            }
            Lines += FString::Printf(TEXT(" | %s %s"), *Name(Aim), bHitNow ? *FString::Printf(TEXT("%d"), Dealt) : TEXT("miss"));
            Aim = INDEX_NONE;
            // Chromatic Orb: two matching d8s leap the orb to another creature within 30 ft of the last, up to the slot level in leaps.
            if (bOrb && bPair && Leaps < Slot)
            {
                const FCombatantState* From = Find(Hit.Last());
                int32 Best = MAX_int32;
                for (const FCombatantState& O : State.Encounter.Combatants)
                {
                    if (!From || O.Id == CasterId || O.Side == User->Side || O.Life != ECombatLife::Conscious || Hit.Contains(O.Id) || Manhattan(From->Cell, O.Cell) > 6) { continue; }
                    if (Manhattan(From->Cell, O.Cell) < Best) { Best = Manhattan(From->Cell, O.Cell); Aim = O.Id; }
                }
                if (Aim != INDEX_NONE) { ++Leaps; Lines += TEXT(" | leaps"); }
            }
        }
    }
    else if (bIceKnife)
    {
        // Ice Knife: attack for 1d10 piercing, then hit or miss the shard explodes: the target and each creature within 5 ft make a DEX save or take 2d6 cold (+1d6/slot).
        bool bCrit = false;
        const bool bHitNow = UtilityAttack(CasterId, Command.TargetId, AttackBonus, Result, bCrit, true);
        Result.DamageType = EDamageType::Piercing;
        if (bHitNow)
        {
            const int32 Dealt = UtilityDamage(Command.TargetId, Logged(bCrit ? 2 : 1, 10), EDamageType::Piercing);
            Result.Damage += Dealt; Result.bHit = true;
            Lines += FString::Printf(TEXT(" | %s %d piercing"), *Name(Command.TargetId), Dealt);
        }
        else { Lines += FString::Printf(TEXT(" | %s miss"), *Name(Command.TargetId)); }
        const FIntPoint Center = Find(Command.TargetId)->Cell;
        const int32 Cold = Logged(S.Dice2[FMath::Clamp(Slot, 1, 9)] > 0 ? S.Dice2[FMath::Clamp(Slot, 1, 9)] : 2 + FMath::Max(0, Slot - 1), 6);
        TArray<int32> Burst;
        for (const FCombatantState& O : State.Encounter.Combatants) { if (O.Life != ECombatLife::Dead && Grid(O.Cell, Center) <= 1) { Burst.Add(O.Id); } }
        for (const int32 V : Burst)
        {
            const bool bSaved = RollSpellSave(V, EAbility::Dexterity, Dc, nullptr);
            const int32 Dealt = bSaved ? 0 : UtilityDamage(V, Cold, EDamageType::Cold);
            if (V == Command.TargetId) { Result.Damage += Dealt; Result.bHit |= Dealt > 0; }
            Touched.AddUnique(V);
            Lines += FString::Printf(TEXT(" | %s %s %d cold"), *Name(V), bSaved ? TEXT("saves,") : TEXT("fails,"), Dealt);
        }
        Result.bSaveSpell = false;
    }
    else if (Is(S, TEXT("true-strike")))
    {
        // True Strike: one weapon attack using the spellcasting modifier; +1d6 radiant on a hit at level 5 (2d6 at 11, 3d6 at 17).
        bTrueStrike = true;
        Strike(CasterId, Command.TargetId, 0, Result);
        bTrueStrike = false;
        const int32 Extra = FSpellUtilityRules::CantripDice(User->Level) - 1;
        if (Result.bHit && Extra > 0)
        {
            const int32 Dealt = UtilityDamage(Command.TargetId, Logged(Extra * (Result.bCritical ? 2 : 1), 6), EDamageType::Radiant);
            Result.Damage += Dealt;
        }
        Touched.Add(Command.TargetId);
        Lines += FString::Printf(TEXT(" | %s %s %d"), *Name(Command.TargetId), Result.bHit ? TEXT("hit") : TEXT("miss"), Result.Damage);
    }
    else if (Is(S, TEXT("shillelagh")))
    {
        if (const int32 I = State.ActiveSpells.IndexOfByPredicate([&](const FActiveSpell& A) { return A.TargetId == CasterId && IsId(A.SpellId, TEXT("shillelagh")); }); I != INDEX_NONE) { RemoveActiveSpell(I, false); }
        FActiveSpell& Entry = AddActiveSpell(S, CasterId, CasterId, Slot, Dc, AttackBonus);
        Probe = Entry;
        Result.bHit = true; Touched.Add(CasterId);
        Lines = TEXT(" | club or quarterstaff empowered for 1 minute");
    }
    else if (Is(S, TEXT("spare-the-dying")))
    {
        FCombatantState* T = MutableFind(Command.TargetId);
        T->DeathSaveSuccesses = 3; T->DeathSaveFailures = 0;
        Result.bHit = true; Touched.Add(T->Id);
        Lines = FString::Printf(TEXT(" | %s is stable"), *T->Name);
    }
    else if (Is(S, TEXT("revivify")))
    {
        FCombatantState* T = MutableFind(Command.TargetId);
        T->Life = ECombatLife::Conscious; T->HP = 1; T->RescueTurnsLeft = 0; T->DeathSaveSuccesses = 0; T->DeathSaveFailures = 0;
        Result.Healing = 1; Result.bHit = true; Touched.Add(T->Id);
        Result.Effects.Add({T->Id, EAbilityEffect::Healing, 1, T->Cell});
        Lines = FString::Printf(TEXT(" | %s returns with 1 HP"), *T->Name);
    }
    else if (Is(S, TEXT("lesser-restoration")))
    {
        FCombatantState* T = MutableFind(Command.TargetId);
        constexpr ECombatCondition LrNone = ECombatCondition::Petrified;
        ECombatCondition Ended = LrNone;
        for (const ECombatCondition C : {static_cast<ECombatCondition>(Command.Choice), ECombatCondition::Paralyzed, ECombatCondition::Blinded, ECombatCondition::Poisoned, ECombatCondition::Deafened})
        {
            if ((C == ECombatCondition::Paralyzed || C == ECombatCondition::Blinded || C == ECombatCondition::Poisoned || C == ECombatCondition::Deafened) && HasCondition(*T, C)) { Ended = C; break; }
        }
        if (Ended != LrNone) { T->Conditions.RemoveAll([Ended](const FCombatCondition& C) { return C.Condition == Ended; }); }
        Result.bHit = Ended != LrNone; Touched.Add(T->Id);
        Lines = FString::Printf(TEXT(" | %s: %s ends"), *T->Name, Ended != LrNone ? *FString(ConditionName(Ended)) : TEXT("nothing"));
    }
    else if (Is(S, TEXT("dispel-magic")))
    {
        // Each spell on the creature ends if its level is at most the slot level, otherwise on a spellcasting check DC 10 + its level.
        const int32 TargetId = Command.TargetId;
        const FCombatantState* T = Find(TargetId);
        TArray<int32> Serials;
        for (const FActiveSpell& A : State.ActiveSpells) { if (A.TargetId == TargetId || (A.CasterId == TargetId && A.TargetId == INDEX_NONE && T && ZoneCovers(A, T->Cell))) { Serials.Add(A.Serial); } }
        int32 Ended = 0;
        for (const int32 Serial : Serials)
        {
            const int32 I = IndexOfSpell(Serial);
            if (I == INDEX_NONE) { continue; }
            const FSrdSpell* O = FSpellCatalogRules::Find(State.ActiveSpells[I].SpellId);
            const int32 Level = FMath::Max(O ? O->Level : 0, State.ActiveSpells[I].SlotLevel);
            if (Level <= Slot || Roll(20) + Mod >= 10 + Level) { RemoveActiveSpell(I); ++Ended; }
        }
        Result.bHit = Ended > 0; Touched.Add(TargetId);
        Lines = FString::Printf(TEXT(" | %s: %d spell(s) ended"), *Name(TargetId), Ended);
    }
    else if (Is(S, TEXT("remove-curse")))
    {
        const int32 TargetId = Command.TargetId;
        int32 Ended = 0;
        for (int32 I = State.ActiveSpells.Num() - 1; I >= 0; --I)
        {
            if (State.ActiveSpells.IsValidIndex(I) && State.ActiveSpells[I].TargetId == TargetId && IsCurse(State.ActiveSpells[I].SpellId)) { RemoveActiveSpell(I); ++Ended; }
        }
        Result.bHit = Ended > 0; Touched.Add(TargetId);
        Lines = FString::Printf(TEXT(" | %s: %d curse(s) ended"), *Name(TargetId), Ended);
    }
    else if (U && U->Kind == ESpellUtility::Teleport)
    {
        const FIntPoint From = User->Cell;
        User->Cell = Command.Destination;
        Result.Path = {Command.Destination};
        Touched.Add(CasterId);
        // Dimension Door: one willing adjacent ally comes along to a free cell next to the destination.
        if (Is(S, TEXT("dimension-door")) && !Command.ExtraTargetIds.IsEmpty())
        {
            FCombatantState* Buddy = MutableFind(Command.ExtraTargetIds[0]);
            if (Buddy && Buddy->Side == User->Side && Buddy->Life == ECombatLife::Conscious && Manhattan(Buddy->Cell, From) <= 1)
            {
                for (const FIntPoint D : {FIntPoint(1, 0), FIntPoint(-1, 0), FIntPoint(0, 1), FIntPoint(0, -1), FIntPoint(1, 1), FIntPoint(-1, -1), FIntPoint(1, -1), FIntPoint(-1, 1)})
                {
                    if (IsOpen(Command.Destination + D, Buddy->Id)) { const FIntPoint Old = Buddy->Cell; Buddy->Cell = Command.Destination + D; SpellsOnStep(Buddy->Id, Old, Buddy->Cell); Touched.Add(Buddy->Id); break; }
                }
            }
        }
        if (FCombatantState* Mover = MutableFind(CasterId)) { SpellsOnStep(CasterId, From, Mover->Cell); }
        Result.bHit = true;
        Lines = FString::Printf(TEXT(" | teleports to (%d,%d)"), Command.Destination.X, Command.Destination.Y);
    }
    else if (Is(S, TEXT("gust-of-wind")))
    {
        // A 60-ft line, 10 ft wide, from the caster toward the target: each creature in it makes a STR save or is pushed 15 ft. Lasts (concentration).
        const FIntPoint Delta = Find(Command.TargetId)->Cell - User->Cell;
        const FIntPoint Dir(FMath::Sign(Delta.X), FMath::Sign(Delta.Y));
        FActiveSpell& Entry = AddActiveSpell(S, CasterId, INDEX_NONE, Slot, Dc, AttackBonus);
        Entry.Value = (Dir.X + 1) * 3 + (Dir.Y + 1);
        Entry.Origin = User->Cell;
        Probe = Entry;
        const FIntPoint Perp(-Dir.Y, Dir.X);
        TArray<int32> InLine;
        for (const FCombatantState& O : State.Encounter.Combatants)
        {
            if (O.Id == CasterId || O.Life != ECombatLife::Conscious) { continue; }
            for (int32 K = 1; K <= 12; ++K) { const FIntPoint P = User->Cell + Dir * K; if (O.Cell == P || O.Cell == P + Perp) { InLine.Add(O.Id); break; } }
        }
        // Farthest first so pushed creatures don't block each other.
        InLine.Sort([&](int32 A, int32 B) { return Manhattan(Find(A)->Cell, User->Cell) > Manhattan(Find(B)->Cell, User->Cell); });
        for (const int32 V : InLine)
        {
            const bool bSaved = RollSpellSave(V, EAbility::Strength, Dc, nullptr);
            if (!bSaved) { PushUnit(V, Dir, 3); }
            Touched.Add(V); Result.bHit |= !bSaved;
            Lines += FString::Printf(TEXT(" | %s %s"), *Name(V), bSaved ? TEXT("holds") : TEXT("pushed 15 ft"));
        }
    }
    else if (U && U->Kind == ESpellUtility::Summon)
    {
        FActiveSpell& Entry = AddActiveSpell(S, CasterId, INDEX_NONE, Slot, Dc, AttackBonus);
        Entry.Origin = IsInside(Command.Destination) && IsOpen(Command.Destination, CasterId) && Command.Destination != FIntPoint::ZeroValue ? Command.Destination : User->Cell;
        if (Is(S, TEXT("find-steed"))) { Entry.RoundsLeft = -1; } // the steed stays until it drops or is dismissed
        Probe = Entry;
        Result.bHit = true; Touched.Add(CasterId);
        Lines = FString::Printf(TEXT(" | summoned at (%d,%d)"), Entry.Origin.X, Entry.Origin.Y);
    }
    else { return false; }

    User = MutableFind(CasterId);
    Result.Feedback = FString::Printf(TEXT("%s: %s (%s)%s"), User ? *User->Name : TEXT("?"), S.Name, *SlotText, *Lines);
    if (const int32 I = IndexOfSpell(Probe.Serial); Probe.Serial != 0 && I != INDEX_NONE) { Probe = State.ActiveSpells[I]; }
    EmitSpellEvent(ESpellEventKind::Cast, Probe, Touched);
    return true;
}
