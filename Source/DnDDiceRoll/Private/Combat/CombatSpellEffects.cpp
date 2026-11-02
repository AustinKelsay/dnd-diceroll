// Spells phases 3-4: condition effects, creature types and immunities, buffs and debuffs (SRD 5.2.1).
#include "Combat/CombatBattle.h"
#include "Rules/SpellEffectRules.h"
#include "Rules/SpellTacticsRules.h"
#include "Rules/SpellCatalogRules.h"
#include "Rules/SpellcastingRules.h"
#include "Rules/CheckRules.h"
#include "Rules/AreaRules.h"
#include "Rules/DamageRules.h"
#include "Rules/AbilityRules.h"

namespace CombatSpellSlots { bool ToCombatCondition(ESpellCondition In, ECombatCondition& Out); }

namespace CombatSpellEffects
{
int32 Cells(FIntPoint A, FIntPoint B) { return FMath::Abs(A.X - B.X) + FMath::Abs(A.Y - B.Y); }
uint16 TypeBit(EDamageType T) { return static_cast<uint16>(1u << static_cast<uint8>(T)); }
constexpr uint16 AllDamage = 0x1FFF;
uint32 SpellBit(ECombatCondition C)
{
    ESpellCondition S;
    switch (C)
    {
    case ECombatCondition::Blinded: S = ESpellCondition::Blinded; break;
    case ECombatCondition::Charmed: S = ESpellCondition::Charmed; break;
    case ECombatCondition::Deafened: S = ESpellCondition::Deafened; break;
    case ECombatCondition::Frightened: S = ESpellCondition::Frightened; break;
    case ECombatCondition::Grappled: S = ESpellCondition::Grappled; break;
    case ECombatCondition::Incapacitated: S = ESpellCondition::Incapacitated; break;
    case ECombatCondition::Invisible: S = ESpellCondition::Invisible; break;
    case ECombatCondition::Paralyzed: S = ESpellCondition::Paralyzed; break;
    case ECombatCondition::Petrified: S = ESpellCondition::Petrified; break;
    case ECombatCondition::Poisoned: S = ESpellCondition::Poisoned; break;
    case ECombatCondition::Prone: S = ESpellCondition::Prone; break;
    case ECombatCondition::Restrained: S = ESpellCondition::Restrained; break;
    case ECombatCondition::Stunned: S = ESpellCondition::Stunned; break;
    default: S = ESpellCondition::Unconscious; break;
    }
    return 1u << static_cast<uint8>(S);
}
bool Incapacitating(ECombatCondition C)
{
    return C == ECombatCondition::Incapacitated || C == ECombatCondition::Stunned || C == ECombatCondition::Paralyzed || C == ECombatCondition::Unconscious || C == ECombatCondition::Petrified;
}
}
using namespace CombatSpellEffects;

// ---- Queries ----

ECreatureType FCombatBattle::CreatureTypeOf(int32 Id) const
{
    const FCombatantState* Unit = Find(Id);
    return Unit ? FCreatureRules::TypeOf(Unit->Role) : ECreatureType::Humanoid;
}

const FActiveSpell* FCombatBattle::SpellEffectOn(int32 Id, const TCHAR* SpellId) const
{
    return State.ActiveSpells.FindByPredicate([&](const FActiveSpell& A) { return A.TargetId == Id && A.SpellId.Equals(SpellId, ESearchCase::IgnoreCase); });
}

int32 FCombatBattle::MirrorImagesLeft(int32 Id) const
{
    const FActiveSpell* A = SpellEffectOn(Id, TEXT("mirror-image"));
    return A ? A->Value : 0;
}

int32 FCombatBattle::SpellSpeed(int32 Id, int32 SpeedCells) const
{
    int32 Feet = 0, Percent = 100;
    bool bZero = false;
    for (const FActiveSpell& A : State.ActiveSpells)
    {
        const FSpellEffectSpec* Spec = A.TargetId == Id ? FSpellEffectRules::Find(A.SpellId) : nullptr;
        if (!Spec) { continue; }
        Feet += Spec->SpeedFeet;
        if (Spec->SpeedPercent > 0) { Percent = Percent * Spec->SpeedPercent / 100; }
        bZero |= (Spec->Flags & SpellEffectFlag::SpeedZero) != 0;
    }
    if (bZero) { return 0; }
    if (Feet == 0 && Percent == 100) { return SpeedCells; }
    return FMath::Max(0, (SpeedCells + Feet / FAreaRules::FeetPerCell) * Percent / 100);
}

uint16 FCombatBattle::SpellResistances(int32 Id) const
{
    const FCombatantState* Unit = Find(Id);
    uint16 Out = Unit && HasCondition(*Unit, ECombatCondition::Petrified) ? AllDamage : 0;
    for (const FActiveSpell& A : State.ActiveSpells)
    {
        const FSpellEffectSpec* Spec = A.TargetId == Id ? FSpellEffectRules::Find(A.SpellId) : nullptr;
        if (!Spec) { continue; }
        Out |= Spec->Resist;
        if (Spec->Flags & SpellEffectFlag::ResistChoice) { Out |= TypeBit(FSpellEffectRules::EnergyChoice(A.Value)); }
        if (Spec->Flags & SpellEffectFlag::FireShield) { Out |= TypeBit(A.Value == 1 ? EDamageType::Fire : EDamageType::Cold); }
    }
    return Out;
}

int32 FCombatBattle::SpellUnarmoredBonus(int32 Id) const
{
    int32 Best = 0;
    for (const FActiveSpell& A : State.ActiveSpells)
    {
        const FSpellEffectSpec* Spec = A.TargetId == Id ? FSpellEffectRules::Find(A.SpellId) : nullptr;
        if (Spec) { Best = FMath::Max(Best, Spec->AcBase); }
    }
    return Best;
}

int32 FCombatBattle::EffectiveArmorClass(int32 Id) const
{
    int32 AC = ArmorClassBeforeSpells(Id), Floor = 0;
    for (const FActiveSpell& A : State.ActiveSpells)
    {
        const FSpellEffectSpec* Spec = A.TargetId == Id ? FSpellEffectRules::Find(A.SpellId) : nullptr;
        if (!Spec) { continue; }
        AC += Spec->AcBonus;
        Floor = FMath::Max(Floor, Spec->AcFloor);
    }
    return FMath::Max(AC, Floor);
}

bool FCombatBattle::CharmForbids(int32 ActorId, int32 TargetId) const
{
    const FCombatantState* Actor = Find(ActorId);
    return Actor && Actor->Conditions.ContainsByPredicate([TargetId](const FCombatCondition& C) { return C.Condition == ECombatCondition::Charmed && C.SourceId == TargetId; });
}

bool FCombatBattle::SpellKeepsProne(int32 Id) const
{
    return State.ActiveSpells.ContainsByPredicate([Id](const FActiveSpell& A)
    {
        const FSpellEffectSpec* Spec = A.TargetId == Id ? FSpellEffectRules::Find(A.SpellId) : nullptr;
        return Spec && (Spec->Flags & SpellEffectFlag::CantStand);
    });
}

bool FCombatBattle::SpellBlocksReactions(int32 Id) const
{
    return State.ActiveSpells.ContainsByPredicate([Id](const FActiveSpell& A)
    {
        const FSpellEffectSpec* Spec = A.TargetId == Id ? FSpellEffectRules::Find(A.SpellId) : nullptr;
        return Spec && (Spec->Flags & SpellEffectFlag::NoReactions);
    });
}

bool FCombatBattle::FearBlocksStep(int32 Id, FIntPoint From, FIntPoint To) const
{
    const FCombatantState* Unit = Find(Id);
    if (!Unit) { return false; }
    for (const FCombatCondition& C : Unit->Conditions)
    {
        const FCombatantState* Source = C.Condition == ECombatCondition::Frightened ? Find(C.SourceId) : nullptr;
        if (Source && Source->Life == ECombatLife::Conscious && HasLineOfSight(From, Source->Cell) && Cells(To, Source->Cell) < Cells(From, Source->Cell)) { return true; }
    }
    return false;
}

void FCombatBattle::SpellAttackModes(const FCombatantState& Actor, const FCombatantState& Target, FAttackRollMode& Mode) const
{
    if (HasCondition(Actor, ECombatCondition::Invisible)) { Mode.Advantage.Add(TEXT("attacker invisible")); }
    if (HasCondition(Target, ECombatCondition::Invisible)) { Mode.Disadvantage.Add(TEXT("target invisible")); }
    if (HasCondition(Target, ECombatCondition::Petrified)) { Mode.Advantage.Add(TEXT("target petrified")); }
    for (const FActiveSpell& A : State.ActiveSpells)
    {
        const FSpellEffectSpec* Spec = A.TargetId == Target.Id ? FSpellEffectRules::Find(A.SpellId) : nullptr;
        if (!Spec) { continue; }
        const FSrdSpell* S = FSpellCatalogRules::Find(A.SpellId);
        const FString Name = S ? FString(S->Name) : A.SpellId;
        if (Spec->Flags & SpellEffectFlag::AttackersDisadvantage) { Mode.Disadvantage.Add(Name); }
        if (Spec->Flags & SpellEffectFlag::AttackersAdvantage) { Mode.Advantage.Add(Name); }
        if (Spec->Flags & SpellEffectFlag::WardEvilGood)
        {
            const ECreatureType T = FCreatureRules::TypeOf(Actor.Role);
            if (T == ECreatureType::Aberration || T == ECreatureType::Celestial || T == ECreatureType::Elemental || T == ECreatureType::Fey || T == ECreatureType::Fiend || T == ECreatureType::Undead)
            { Mode.Disadvantage.Add(Name); }
        }
    }
}

void FCombatBattle::SpellSaveModes(const FCombatantState& Unit, EAbility Ability, int32& Advantage, int32& Disadvantage) const
{
    for (const FActiveSpell& A : State.ActiveSpells)
    {
        const FSpellEffectSpec* Spec = A.TargetId == Unit.Id ? FSpellEffectRules::Find(A.SpellId) : nullptr;
        if (!Spec) { continue; }
        if ((Spec->Flags & SpellEffectFlag::DexSaveAdvantage) && Ability == EAbility::Dexterity) { ++Advantage; }
        if ((Spec->Flags & SpellEffectFlag::StrSaveAdvantage) && Ability == EAbility::Strength) { ++Advantage; }
    }
}

// ---- Targeting wrappers: charm, fear, spell target sides ----

TArray<int32> FCombatBattle::LegalTargets(int32 Id, ECombatCommandType Action) const
{
    TArray<int32> Out = LegalTargetsRaw(Id, Action);
    Out.RemoveAll([&](int32 T) { return CharmForbids(Id, T); });
    return Out;
}

TArray<FIntPoint> FCombatBattle::LegalMoves(int32 Id) const
{
    TArray<FIntPoint> Out = LegalMovesRaw(Id);
    const FCombatantState* Unit = Find(Id);
    if (Unit) { Out.RemoveAll([&](FIntPoint C) { return FearBlocksStep(Id, Unit->Cell, C); }); }
    return Out;
}

TArray<FIntPoint> FCombatBattle::PathTo(int32 Id, FIntPoint Destination) const
{
    const FCombatantState* Unit = Find(Id);
    if (Unit && FearBlocksStep(Id, Unit->Cell, Destination)) { return {}; }
    return PathToRaw(Id, Destination);
}

TArray<int32> FCombatBattle::SpellTargets(int32 Id, const FString& SpellId, int32 SlotLevel) const
{
    const FSrdSpell* Catalog = FSpellCatalogRules::Find(SpellId);
    const FSpellEffectSpec* Spec = Catalog ? FSpellEffectRules::Find(Catalog->Id) : nullptr;
    if (!Spec)
    {
        TArray<int32> Out = SpellTargetsRaw(Id, SpellId, SlotLevel);
        Out.RemoveAll([&](int32 T) { return CharmForbids(Id, T); });
        return Out;
    }
    TArray<int32> Out;
    const FCombatantState* Unit = Find(Id);
    int32 Slot = 0; EAdventurerRole Class = EAdventurerRole::None;
    const FSrdSpell* Spell = Unit ? UsableSpell(*Unit, SpellId, SlotLevel, Slot, Class) : nullptr;
    if (!Spell) { return Out; }
    if (Spec->Flags & SpellEffectFlag::Smite)
    {
        // Smites: a bonus action right after hitting the creature with a melee weapon this turn.
        if (ActiveId() != Id) { return Out; }
        for (const int32 T : State.MeleeHitThisTurn)
        {
            const FCombatantState* Target = Find(T);
            if (Target && Target->Life == ECombatLife::Conscious && Target->Side != Unit->Side) { Out.Add(T); }
        }
        return Out;
    }
    if (Spec->Target == ESpellEffectTarget::Foe)
    {
        Out = SpellTargetsRaw(Id, SpellId, SlotLevel);
        Out.RemoveAll([&](int32 T) { const FCombatantState* U = Find(T); return !U || CharmForbids(Id, T) || (Spell->Area == ESpellArea::None && !FSpellEffectRules::CanAffect(*Spec, U->Role)); });
        return Out;
    }
    const int32 Range = Spell->RangeFeet == 0 ? 0 : FMath::Max(1, Spell->RangeFeet / FAreaRules::FeetPerCell);
    for (const FCombatantState& Target : State.Encounter.Combatants)
    {
        if (Target.Side != Unit->Side || Target.Life == ECombatLife::Dead) { continue; }
        if ((Spec->Target == ESpellEffectTarget::Self || Range == 0) && Target.Id != Id) { continue; }
        if (Target.Id != Id && (Cells(Unit->Cell, Target.Cell) > Range || !HasLineOfSight(Unit->Cell, Target.Cell))) { continue; }
        // Mage Armor: a creature that isn't wearing armor.
        if (Spec->AcBase > 0 && Target.ArmorWorn > 0) { continue; }
        Out.Add(Target.Id);
    }
    return Out;
}

TArray<int32> FCombatBattle::SpellTargetList(const FCombatCommand& Command, const FSrdSpell& Spell, const FSpellEffectSpec& Spec, int32 Slot, const TArray<int32>& Legal) const
{
    TArray<int32> Out;
    const int32 Cap = Spec.Targets > 0 ? FSpellEffectRules::MaxTargets(Spec, Spell.Level, Slot) : 1;
    TArray<int32> Wanted = {Command.TargetId};
    Wanted.Append(Command.ExtraTargetIds);
    for (const int32 Id : Wanted)
    {
        if (Out.Num() >= Cap) { break; }
        if (Legal.Contains(Id)) { Out.AddUnique(Id); }
    }
    return Out;
}

// ---- Rolls and conditions ----

bool FCombatBattle::RollSpellSave(int32 VictimId, EAbility Ability, int32 Dc, const FSpellEffectSpec* Spec, bool bAdvantage)
{
    FCombatantState* Victim = MutableFind(VictimId);
    if (!Victim) { return true; }
    if (Spec && (Spec->Flags & SpellEffectFlag::SleepEscalates) && FCreatureRules::ImmuneToExhaustion(Victim->Role)) { return true; }
    int32 Bonus = Character(VictimId).SaveBonus(Ability);
    for (const FActiveSpell& A : State.ActiveSpells)
    {
        const FSpellEffectSpec* On = A.TargetId == VictimId ? FSpellEffectRules::Find(A.SpellId) : nullptr;
        if (!On) { continue; }
        Bonus += On->SaveBonus + (Ability == EAbility::Dexterity ? On->DexSaveBonus : 0);
    }
    // Bless/Bane: roll the d4 only when the effect is present (keeps seeded streams for battles without it).
    TArray<int32> Dice;
    for (const FActiveSpell& A : State.ActiveSpells)
    {
        const FSpellEffectSpec* On = A.TargetId == VictimId ? FSpellEffectRules::Find(A.SpellId) : nullptr;
        if (On && On->RollDie != 0) { Dice.Add(On->RollDie); }
    }
    for (const int32 Die : Dice) { Bonus += Die > 0 ? Roll(Die) : -Roll(-Die); }
    Victim = MutableFind(VictimId);
    const bool bRolled = FCheckRules::Resolve(Bonus, Dc, SaveMode(*Victim, Ability, bAdvantage ? ERollMode::Advantage : ERollMode::Normal), State.RandomState).bSuccess;
    return bRolled && !(IsHelpless(*Victim) && (Ability == EAbility::Strength || Ability == EAbility::Dexterity));
}

bool FCombatBattle::AddSpellCondition(FCombatantState& Victim, ECombatCondition Kind, int32 SourceId, int32 RoundsLeft)
{
    const uint32 Bit = SpellBit(Kind);
    if (Kind != ECombatCondition::Prone && (FCreatureRules::ConditionImmunities(Victim.Role) & Bit)) { return false; }
    for (const FActiveSpell& A : State.ActiveSpells)
    {
        const FSpellEffectSpec* On = A.TargetId == Victim.Id ? FSpellEffectRules::Find(A.SpellId) : nullptr;
        if (On && (On->ConditionImmunity & Bit)) { return false; }
    }
    if (Victim.Conditions.ContainsByPredicate([&](const FCombatCondition& C) { return C.Condition == Kind && C.SourceId == SourceId; })) { return true; }
    FCombatCondition Applied; Applied.Condition = Kind; Applied.SourceId = SourceId; Applied.RoundsLeft = RoundsLeft;
    Victim.Conditions.Add(Applied);
    // SRD 5.2.1: concentration ends when the concentrating creature has the Incapacitated condition.
    if (Incapacitating(Kind) && Victim.bConcentrating) { EndConcentration(Victim); }
    return true;
}

// ---- Casting a buff or debuff ----

bool FCombatBattle::ResolveEffectSpell(const FCombatCommand& Command, const FSrdSpell& S, const FSpellEffectSpec& Spec, int32 Slot, int32 Dc, int32 AttackBonus, int32 Mod, const TArray<int32>& Legal, FCombatResult& Result)
{
    FCombatantState* User = MutableFind(Command.ActorId);
    const int32 CasterId = User->Id;
    const int32 Above = FMath::Max(0, Slot - S.Level);
    TArray<FIntPoint> CastCells;
    TArray<int32> Targets;
    if (Spec.Target == ESpellEffectTarget::Foe && S.Area != ESpellArea::None)
    {
        Targets = SpellVictims(*User, S, Command.TargetId, &CastCells);
        Targets.RemoveAll([&](int32 V) { const FCombatantState* U = Find(V); return !U || !FSpellEffectRules::CanAffect(Spec, U->Role) || ((Spec.Flags & SpellEffectFlag::SparesAllies) && U->Side == User->Side); });
        if (Spec.Targets > 0) { Targets.SetNum(FMath::Min(Targets.Num(), FSpellEffectRules::MaxTargets(Spec, S.Level, Slot))); }
    }
    else { Targets = SpellTargetList(Command, S, Spec, Slot, Legal); }
    const bool bSave = Spec.Target == ESpellEffectTarget::Foe && S.Resolution == ESpellResolution::Save && !(Spec.Flags & SpellEffectFlag::Smite);
    Result.bSaveSpell = bSave;
    if (bSave) { Result.TargetArmorClass = Dc; }
    Result.DamageType = Spec.HitType;
    FString Lines;
    TArray<int32> Affected;
    int32 FirstSerial = INDEX_NONE;
    for (const int32 TargetId : Targets)
    {
        FCombatantState* Target = MutableFind(TargetId);
        if (!Target || Target->Life == ECombatLife::Dead) { continue; }
        if (bSave && RollSpellSave(TargetId, S.SaveAbility, Dc, &Spec))
        {
            Lines += FString::Printf(TEXT(" | %s saves"), *Target->Name);
            Result.Effects.Add({TargetId, EAbilityEffect::Strike, 0, Target->Cell});
            continue;
        }
        FString Text;
        int32 Dealt = 0;
        if (Spec.Flags & SpellEffectFlag::Smite)
        {
            // The smite's extra damage lands with the triggering hit (critical doubling not modelled).
            int32 Dice = Spec.HitDice + Spec.HitDicePerSlot * Above;
            if (Spec.BonusVsTypes & FCreatureRules::Bit(FCreatureRules::TypeOf(Target->Role))) { ++Dice; }
            DiceLog = &Result.EffectDice;
            const int32 Rolled = Dice > 0 && Spec.HitSides > 0 ? RollDice(Dice, Spec.HitSides) : 0;
            DiceLog = nullptr;
            Dealt = FDamageRules::Adjust(Rolled, Spec.HitType, DamageDefenses(TargetId));
            const int32 Prevented = FMath::Min(Dealt, ProtectionFor(*Target));
            ApplyDamage(*Target, Dealt);
            Dealt -= Prevented; Result.Damage += Dealt; Result.DamagePrevented += Prevented;
            Text += FString::Printf(TEXT(" %d %s"), Dealt, FDamageRules::TypeName(Spec.HitType));
            Target = MutableFind(TargetId);
            if (Target->Life != ECombatLife::Conscious || !(Spec.Flags & (SpellEffectFlag::StartTurnDamage | SpellEffectFlag::AttackersAdvantage)))
            {
                Result.Effects.Add({TargetId, EAbilityEffect::Strike, Dealt, Target->Cell});
                Lines += FString::Printf(TEXT(" | %s%s"), *Target->Name, *Text);
                Affected.Add(TargetId);
                continue;
            }
        }
        // One effect of a spell per creature: recasting replaces it.
        for (int32 I = State.ActiveSpells.Num() - 1; I >= 0; --I)
        {
            if (State.ActiveSpells[I].TargetId == TargetId && State.ActiveSpells[I].SpellId.Equals(S.Id, ESearchCase::IgnoreCase)) { RemoveActiveSpell(I, false); }
        }
        User = MutableFind(CasterId);
        if (S.bConcentration && !User->bConcentrating) { break; } // the caster lost concentration (e.g. a replaced effect ended it)
        FActiveSpell& Entry = AddActiveSpell(S, CasterId, TargetId, Slot, Dc, AttackBonus);
        if (FirstSerial == INDEX_NONE) { FirstSerial = Entry.Serial; }
        Entry.Value = Command.Choice;
        if (Spec.Flags & SpellEffectFlag::TempHpPerTurn) { Entry.Value = FMath::Max(1, Mod); }
        if (Spec.Flags & SpellEffectFlag::MirrorImages) { Entry.Value = 3; Text += TEXT(" 3 duplicates"); }
        if (Spec.MaxHpPerSlot > 0)
        {
            Entry.Value = Spec.MaxHpPerSlot * (1 + Above);
            Target->MaxHP += Entry.Value; Target->HP += Entry.Value;
            if (Target->HP > 0) { Target->Life = ECombatLife::Conscious; }
            Text += FString::Printf(TEXT(" +%d max HP"), Entry.Value);
        }
        if (Spec.TempSides > 0 || Spec.TempFlat > 0)
        {
            DiceLog = &Result.EffectDice;
            const int32 Temp = (Spec.TempDice > 0 ? RollDice(Spec.TempDice, Spec.TempSides) : 0) + Spec.TempFlat + Spec.TempPerSlot * Above;
            DiceLog = nullptr;
            Target->TemporaryHP = FMath::Max(Target->TemporaryHP, Temp);
            Text += FString::Printf(TEXT(" %d temp HP"), Temp);
        }
        if (Spec.Flags & SpellEffectFlag::Invisible) { AddSpellCondition(*Target, ECombatCondition::Invisible, CasterId, -1); Text += TEXT(" Invisible"); }
        if (Spec.ConditionImmunity != 0)
        {
            Target->Conditions.RemoveAll([&](const FCombatCondition& C) { return (Spec.ConditionImmunity & SpellBit(C.Condition)) != 0; });
        }
        if (Spec.Flags & SpellEffectFlag::TempHpPerTurn) { Target->Conditions.RemoveAll([](const FCombatCondition& C) { return C.Condition == ECombatCondition::Frightened; }); }
        Result.Effects.Add({TargetId, Spec.Target == ESpellEffectTarget::Foe ? EAbilityEffect::Strike : EAbilityEffect::Protect, Dealt, Target->Cell});
        Lines += FString::Printf(TEXT(" | %s%s%s"), *Target->Name, bSave ? TEXT(" fails,") : TEXT(""), *Text);
        Affected.Add(TargetId);
    }
    User = MutableFind(CasterId);
    if (S.bConcentration && Affected.IsEmpty()) { User->bConcentrating = false; }
    {
        FActiveSpell Probe; Probe.SpellId = S.Id; Probe.CasterId = CasterId; Probe.SlotLevel = Slot; Probe.RoundsLeft = FSpellTacticsRules::DurationRounds(S);
        if (const int32 I = IndexOfSpell(FirstSerial); I != INDEX_NONE) { Probe = State.ActiveSpells[I]; }
        EmitSpellEvent(ESpellEventKind::Cast, Probe, Targets.IsEmpty() ? TArray<int32>{Command.TargetId} : Targets);
        PendingSpellEvents.Last().Cells = CastCells;
    }
    Result.bHit = !Affected.IsEmpty();
    const FString SlotText = Slot == 0 ? FString(TEXT("cantrip")) : FString::Printf(TEXT("level %d slot"), Slot);
    const FString Check = bSave ? FString::Printf(TEXT(", %s save DC %d"), *FString(FAbilityRules::AbilityName(S.SaveAbility)).Left(3).ToUpper(), Dc) : FString();
    Result.Feedback = FString::Printf(TEXT("%s: %s (%s%s)%s"), *User->Name, S.Name, *SlotText, *Check, *Lines);
    CheckOutcome();
    return true;
}

void FCombatBattle::EndSpellEffects(const FActiveSpell& Ended)
{
    const FSpellEffectSpec* Spec = FSpellEffectRules::Find(Ended.SpellId);
    FCombatantState* Target = Ended.TargetId == INDEX_NONE ? nullptr : MutableFind(Ended.TargetId);
    if (!Spec || !Target) { return; }
    // Conditions this caster's spell held on the creature end with the last of its effects there.
    const int32 CasterId = Ended.CasterId, TargetId = Target->Id;
    if (!State.ActiveSpells.ContainsByPredicate([&](const FActiveSpell& A) { return A.CasterId == CasterId && A.TargetId == TargetId; }))
    {
        Target->Conditions.RemoveAll([CasterId](const FCombatCondition& C) { return C.SourceId == CasterId && C.RoundsLeft == -1 && C.Condition != ECombatCondition::Grappled; });
    }
    if (Spec->MaxHpPerSlot > 0 && Ended.Value > 0)
    {
        Target->MaxHP = FMath::Max(1, Target->MaxHP - Ended.Value);
        Target->HP = FMath::Min(Target->HP, Target->MaxHP);
    }
    // Haste: when it ends the creature can't move or take actions until after its next turn.
    if ((Spec->Flags & SpellEffectFlag::Lethargy) && State.Phase == EBattlePhase::Fighting && Target->Life == ECombatLife::Conscious)
    {
        FCombatCondition Lethargy; Lethargy.Condition = ECombatCondition::Incapacitated; Lethargy.SourceId = INDEX_NONE; Lethargy.RoundsLeft = 1;
        Target->Conditions.Add(Lethargy);
    }
}

// ---- Damage, attacks and turns ----

void FCombatBattle::SpellEffectsOnDamaged(int32 TargetId, int32 SourceId)
{
    const FCombatantState* Source = Find(SourceId);
    TArray<int32> Serials;
    for (const FActiveSpell& A : State.ActiveSpells) { if (A.TargetId == TargetId) { Serials.Add(A.Serial); } }
    for (const int32 Serial : Serials)
    {
        const int32 Index = IndexOfSpell(Serial);
        if (Index == INDEX_NONE) { continue; }
        const FActiveSpell A = State.ActiveSpells[Index];
        const FSpellEffectSpec* Spec = FSpellEffectRules::Find(A.SpellId);
        const FCombatantState* Caster = Find(A.CasterId);
        if (!Spec) { continue; }
        bool bEnds = (Spec->Flags & SpellEffectFlag::EndsOnAnyDamage) != 0;
        bEnds |= (Spec->Flags & SpellEffectFlag::EndsOnCasterSideDamage) && Source && Caster && Source->Side == Caster->Side && SourceId != TargetId;
        if (!bEnds && (Spec->Flags & SpellEffectFlag::SaveOnDamage))
        {
            const FSrdSpell* S = FSpellCatalogRules::Find(A.SpellId);
            bEnds = S && RollSpellSave(TargetId, S->SaveAbility, A.Dc, Spec, true);
        }
        if (bEnds) { if (const int32 I = IndexOfSpell(Serial); I != INDEX_NONE) { RemoveActiveSpell(I); } }
    }
    if (SourceId != TargetId && SourceId != INDEX_NONE) { EndOnHostileAct(SourceId); }
}

bool FCombatBattle::DeathWardSaves(FCombatantState& Target)
{
    const int32 Id = Target.Id;
    const int32 Index = State.ActiveSpells.IndexOfByPredicate([Id](const FActiveSpell& A)
    {
        const FSpellEffectSpec* Spec = A.TargetId == Id ? FSpellEffectRules::Find(A.SpellId) : nullptr;
        return Spec && (Spec->Flags & SpellEffectFlag::DeathWard);
    });
    if (Index == INDEX_NONE) { return false; }
    Target.HP = 1;
    RemoveActiveSpell(Index);
    return true;
}

int32 FCombatBattle::SpellAttackBonus(int32 ActorId, bool bWeapon, FCombatResult& Result)
{
    TArray<int32> Dice;
    int32 Flat = 0;
    for (const FActiveSpell& A : State.ActiveSpells)
    {
        const FSpellEffectSpec* Spec = A.TargetId == ActorId ? FSpellEffectRules::Find(A.SpellId) : nullptr;
        if (!Spec) { continue; }
        if (Spec->RollDie != 0) { Dice.Add(Spec->RollDie); }
        if (bWeapon && Spec->WeaponBonus > 0) { Flat += A.SlotLevel >= 6 ? 3 : A.SlotLevel >= 4 ? 2 : Spec->WeaponBonus; }
    }
    int32 Total = Flat;
    for (const int32 Die : Dice)
    {
        const int32 R = Roll(FMath::Abs(Die));
        Total += Die > 0 ? R : -R;
        Result.RollModeSources.Add(FString::Printf(TEXT("%s %+d"), Die > 0 ? TEXT("Bless") : TEXT("Bane"), Die > 0 ? R : -R));
    }
    if (Flat > 0) { Result.RollModeSources.Add(FString::Printf(TEXT("Magic Weapon +%d"), Flat)); }
    return Total;
}

int32 FCombatBattle::SpellHitDamage(int32 ActorId, int32 TargetId, bool bCritical, bool bWeapon, EDamageType WeaponType, FCombatResult& Result)
{
    struct FExtra { int32 Dice; int32 Sides; int32 Flat; EDamageType Type; };
    TArray<FExtra> Extras;
    for (const FActiveSpell& A : State.ActiveSpells)
    {
        const FSpellEffectSpec* Spec = FSpellEffectRules::Find(A.SpellId);
        if (!Spec) { continue; }
        const int32 Above = FMath::Max(0, A.SlotLevel - (FSpellCatalogRules::Find(A.SpellId) ? FSpellCatalogRules::Find(A.SpellId)->Level : 0));
        if ((Spec->Flags & SpellEffectFlag::MarkOnHit) && A.CasterId == ActorId && A.TargetId == TargetId)
        { Extras.Add({Spec->HitDice + Spec->HitDicePerSlot * Above, Spec->HitSides, 0, Spec->HitType}); }
        if (A.TargetId != ActorId || !bWeapon) { continue; }
        if (Spec->Flags & SpellEffectFlag::OwnWeaponHit) { Extras.Add({Spec->HitDice, Spec->HitSides, 0, Spec->HitType}); }
        else if (Spec->bWeaponTyped && Spec->HitDice > 0) { Extras.Add({Spec->HitDice, Spec->HitSides, 0, WeaponType}); }
        if (Spec->WeaponBonus > 0) { Extras.Add({0, 0, A.SlotLevel >= 6 ? 3 : A.SlotLevel >= 4 ? 2 : Spec->WeaponBonus, WeaponType}); }
    }
    if (Extras.IsEmpty()) { return 0; }
    const FDamageDefenses Defenses = DamageDefenses(TargetId);
    int32 Total = 0;
    TArray<FDieRoll>* Saved = DiceLog;
    DiceLog = &Result.EffectDice;
    for (const FExtra& E : Extras)
    {
        const int32 Rolled = (E.Dice > 0 && E.Sides > 0 ? RollDice(E.Dice * (bCritical ? 2 : 1), E.Sides) : 0) + E.Flat;
        Total += FDamageRules::Adjust(Rolled, E.Type, Defenses);
    }
    DiceLog = Saved;
    return Total;
}

bool FCombatBattle::SanctuaryStops(int32 ActorId, int32 TargetId, FCombatResult& Result)
{
    const FCombatantState* Actor = Find(ActorId);
    const FCombatantState* Target = Find(TargetId);
    if (!Actor || !Target || Actor->Side == Target->Side) { return false; }
    const FActiveSpell* Ward = State.ActiveSpells.FindByPredicate([TargetId](const FActiveSpell& A)
    {
        const FSpellEffectSpec* Spec = A.TargetId == TargetId ? FSpellEffectRules::Find(A.SpellId) : nullptr;
        return Spec && (Spec->Flags & SpellEffectFlag::Sanctuary);
    });
    if (!Ward) { return false; }
    const int32 Dc = Ward->Dc;
    if (RollSpellSave(ActorId, EAbility::Wisdom, Dc, nullptr)) { return false; }
    Result.bHit = false;
    Result.RollModeSources.Add(FString::Printf(TEXT("Sanctuary: failed WIS save DC %d, attack lost"), Dc));
    return true;
}

bool FCombatBattle::MirrorImageTakes(int32 TargetId, FCombatResult& Result)
{
    const int32 Index = State.ActiveSpells.IndexOfByPredicate([TargetId](const FActiveSpell& A)
    {
        const FSpellEffectSpec* Spec = A.TargetId == TargetId ? FSpellEffectRules::Find(A.SpellId) : nullptr;
        return Spec && (Spec->Flags & SpellEffectFlag::MirrorImages) && A.Value > 0;
    });
    if (Index == INDEX_NONE) { return false; }
    bool bDuplicate = false;
    for (int32 I = 0; I < State.ActiveSpells[Index].Value && !bDuplicate; ++I) { bDuplicate = Roll(6) >= 3; }
    if (!bDuplicate) { return false; }
    Result.RollModeSources.Add(TEXT("Mirror Image: a duplicate is destroyed"));
    if (--State.ActiveSpells[Index].Value <= 0) { RemoveActiveSpell(Index); }
    return true;
}

void FCombatBattle::SpellRetaliation(int32 AttackerId, int32 TargetId, FCombatResult& Result)
{
    const FActiveSpell* Shield = State.ActiveSpells.FindByPredicate([TargetId](const FActiveSpell& A)
    {
        const FSpellEffectSpec* Spec = A.TargetId == TargetId ? FSpellEffectRules::Find(A.SpellId) : nullptr;
        return Spec && (Spec->Flags & SpellEffectFlag::FireShield);
    });
    FCombatantState* Attacker = MutableFind(AttackerId);
    if (!Shield || !Attacker || Attacker->Life == ECombatLife::Dead) { return; }
    const FSpellEffectSpec* Spec = FSpellEffectRules::Find(Shield->SpellId);
    const EDamageType Type = Shield->Value == 1 ? EDamageType::Cold : EDamageType::Fire;
    const int32 Dealt = FDamageRules::Adjust(RollDice(Spec->HitDice, Spec->HitSides), Type, DamageDefenses(AttackerId));
    ApplyDamage(*Attacker, Dealt);
    Result.RollModeSources.Add(FString::Printf(TEXT("Fire Shield: %d %s to the attacker"), Dealt, FDamageRules::TypeName(Type)));
}

void FCombatBattle::EndOnHostileAct(int32 ActorId)
{
    for (int32 I = State.ActiveSpells.Num() - 1; I >= 0; --I)
    {
        if (!State.ActiveSpells.IsValidIndex(I)) { continue; }
        const FSpellEffectSpec* Spec = State.ActiveSpells[I].TargetId == ActorId ? FSpellEffectRules::Find(State.ActiveSpells[I].SpellId) : nullptr;
        if (Spec && (Spec->Flags & SpellEffectFlag::EndsOnHostileAct)) { RemoveActiveSpell(I); }
    }
}

void FCombatBattle::SpellEffectsOnTurnStart(int32 UnitId)
{
    TArray<int32> Serials;
    for (const FActiveSpell& A : State.ActiveSpells) { if (A.TargetId == UnitId) { Serials.Add(A.Serial); } }
    for (const int32 Serial : Serials)
    {
        const int32 Index = IndexOfSpell(Serial);
        FCombatantState* Unit = MutableFind(UnitId);
        if (Index == INDEX_NONE || !Unit || Unit->Life == ECombatLife::Dead) { continue; }
        const FActiveSpell A = State.ActiveSpells[Index];
        const FSpellEffectSpec* Spec = FSpellEffectRules::Find(A.SpellId);
        const FSrdSpell* S = FSpellCatalogRules::Find(A.SpellId);
        if (!Spec || !S) { continue; }
        // Heroism: temporary hit points equal to the spellcasting modifier at the start of each of its turns.
        if (Spec->Flags & SpellEffectFlag::TempHpPerTurn) { Unit->TemporaryHP = FMath::Max(Unit->TemporaryHP, A.Value); }
        // Searing Smite: the fire again at the start of each of its turns, then a CON save ends it.
        if ((Spec->Flags & SpellEffectFlag::StartTurnDamage) && Unit->Life == ECombatLife::Conscious)
        {
            const int32 Dice = Spec->HitDice + Spec->HitDicePerSlot * FMath::Max(0, A.SlotLevel - S->Level);
            ApplyDamage(*Unit, FDamageRules::Adjust(RollDice(Dice, Spec->HitSides), Spec->HitType, DamageDefenses(UnitId)));
            EmitSpellEvent(ESpellEventKind::Tick, A, {UnitId});
            Unit = MutableFind(UnitId);
            if (Unit->Life == ECombatLife::Conscious && RollSpellSave(UnitId, S->SaveAbility, A.Dc, Spec))
            {
                if (const int32 I = IndexOfSpell(Serial); I != INDEX_NONE) { RemoveActiveSpell(I); }
            }
        }
    }
}