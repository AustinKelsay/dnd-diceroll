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
