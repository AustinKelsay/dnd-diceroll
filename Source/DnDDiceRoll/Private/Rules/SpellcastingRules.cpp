#include "Rules/SpellcastingRules.h"
#include "Combat/CombatBattle.h"

bool FSpellcastingRules::IsSpellcaster(EAdventurerRole Role)
{
    switch (Role)
    {
    case EAdventurerRole::Bard: case EAdventurerRole::Cleric: case EAdventurerRole::Druid: case EAdventurerRole::Paladin:
    case EAdventurerRole::Ranger: case EAdventurerRole::Sorcerer: case EAdventurerRole::Warlock: case EAdventurerRole::Wizard: return true;
    default: return false;
    }
}

EAbility FSpellcastingRules::SpellcastingAbility(EAdventurerRole Role)
{
    // SRD 5.2.1 spellcasting abilities per class.
    switch (Role)
    {
    case EAdventurerRole::Cleric: case EAdventurerRole::Druid: case EAdventurerRole::Ranger: return EAbility::Wisdom;
    case EAdventurerRole::Bard: case EAdventurerRole::Paladin: case EAdventurerRole::Sorcerer: case EAdventurerRole::Warlock: return EAbility::Charisma;
    default: return EAbility::Intelligence;
    }
}

int32 FSpellcastingRules::SaveDc(int32 ProficiencyBonus, int32 AbilityModifier) { return 8 + ProficiencyBonus + AbilityModifier; }

int32 FSpellcastingRules::AttackBonus(int32 ProficiencyBonus, int32 AbilityModifier) { return ProficiencyBonus + AbilityModifier; }

FSpellDelivery FSpellcastingRules::Delivery(ESpell Spell)
{
    FSpellDelivery Out;
    switch (Spell)
    {
    case ESpell::FireBolt: case ESpell::GuidingBolt: Out.Kind = ESpellDelivery::Attack; break;
    case ESpell::SacredFlame: Out.Kind = ESpellDelivery::Save; Out.SaveAbility = EAbility::Dexterity; break;
    case ESpell::BurningHands: Out.Kind = ESpellDelivery::Save; Out.SaveAbility = EAbility::Dexterity; Out.bHalfOnSave = true; break;
    case ESpell::Thunderwave: Out.Kind = ESpellDelivery::Save; Out.SaveAbility = EAbility::Constitution; Out.bHalfOnSave = true; break;
    case ESpell::CureWounds: case ESpell::HealingWord: Out.Kind = ESpellDelivery::Healing; break;
    default: Out.Kind = ESpellDelivery::Automatic; break;
    }
    return Out;
}

int32 FSpellcastingRules::AttackHitOutOf400(int32 AttackBonus, int32 ArmorClass, ERollMode Mode)
{
    int32 Hits = 0;
    for (int32 Natural = 1; Natural <= 20; ++Natural) { Hits += Natural == 20 || (Natural != 1 && Natural + AttackBonus >= ArmorClass) ? 1 : 0; }
    return Mode == ERollMode::Advantage ? 400 - (20 - Hits) * (20 - Hits) : Mode == ERollMode::Disadvantage ? Hits * Hits : Hits * 20;
}

int32 FSpellcastingRules::SaveFailOutOf400(int32 TargetSaveModifier, int32 Dc, ERollMode Mode)
{
    return 400 - FCheckRules::SuccessOutOf400(TargetSaveModifier, Dc, Mode);
}

int32 FSpellcastingRules::DamageAfterSave(int32 Rolled, bool bSaved, bool bHalfOnSave)
{
    return !bSaved ? Rolled : bHalfOnSave ? Rolled / 2 : 0;
}
