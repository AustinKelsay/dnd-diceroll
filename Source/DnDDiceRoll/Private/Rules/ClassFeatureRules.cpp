#include "Rules/ClassFeatureRules.h"
#include "Combat/CombatBattle.h"

int32 FClassFeatureRules::SecondWindHealing(int32 D10Roll, int32 Level) { return D10Roll + Level; }
int32 FClassFeatureRules::ActionSurgeUses(int32 Level) { return Level >= 17 ? 2 : Level >= 2 ? 1 : 0; }

int32 FClassFeatureRules::AttacksPerAction(EAdventurerRole Role, int32 Level)
{
    // SRD 5.1: Barbarian, Monk, Paladin and Ranger get one Extra Attack at 5; only the Fighter gains more.
    if (Role == EAdventurerRole::Barbarian || Role == EAdventurerRole::Monk || Role == EAdventurerRole::Paladin || Role == EAdventurerRole::Ranger) { return Level >= 5 ? 2 : 1; }
    if (Role != EAdventurerRole::Fighter) { return 1; }
    return Level >= 20 ? 4 : Level >= 11 ? 3 : Level >= 5 ? 2 : 1;
}

int32 FClassFeatureRules::SneakAttackDice(int32 Level) { return Level >= 1 ? (Level + 1) / 2 : 0; }

bool FClassFeatureRules::SneakAttackAllowed(bool bFinesseOrRanged, bool bUsedThisTurn, ERollMode EffectiveMode, bool bAllyAdjacentToTarget)
{
    if (!bFinesseOrRanged || bUsedThisTurn || EffectiveMode == ERollMode::Disadvantage) { return false; }
    return EffectiveMode == ERollMode::Advantage || bAllyAdjacentToTarget;
}

bool FClassFeatureRules::HasCunningAction(int32 Level) { return Level >= 2; }
int32 FClassFeatureRules::ChannelDivinityUses(int32 Level) { return Level >= 18 ? 3 : Level >= 6 ? 2 : Level >= 2 ? 1 : 0; }
int32 FClassFeatureRules::ArcaneRecoveryLevels(int32 Level) { return Level >= 1 ? (Level + 1) / 2 : 0; }

TArray<FClassFeature> FClassFeatureRules::FeaturesThrough(EAdventurerRole Role, int32 Level)
{
    using C = EActionCost; using R = EAdventurerRole;
    struct FRow { R Role; FClassFeature F; };
    static const FRow Table[] = {
        {R::Fighter, {TEXT("Fighting Style"), 1, C::Free, true}}, {R::Fighter, {TEXT("Second Wind"), 1, C::BonusAction, true}},
        {R::Fighter, {TEXT("Action Surge"), 2, C::Free, true}}, {R::Fighter, {TEXT("Superiority Dice"), 3, C::Free, true}},
        {R::Fighter, {TEXT("Extra Attack"), 5, C::Action, true}},
        {R::Rogue, {TEXT("Sneak Attack"), 1, C::Free, true}}, {R::Rogue, {TEXT("Cunning Action"), 2, C::BonusAction, true}},
        {R::Rogue, {TEXT("Uncanny Dodge"), 5, C::Reaction, false}},
        {R::Cleric, {TEXT("Spellcasting"), 1, C::Action, true}}, {R::Cleric, {TEXT("Channel Divinity"), 2, C::Action, true}},
        {R::Wizard, {TEXT("Spellcasting"), 1, C::Action, true}}, {R::Wizard, {TEXT("Arcane Recovery"), 1, C::Free, false}},
        {R::Wizard, {TEXT("Shield"), 1, C::Reaction, true}},
        {R::Barbarian, {TEXT("Rage"), 1, C::BonusAction, true}}, {R::Barbarian, {TEXT("Reckless Attack"), 2, C::Free, false}},
        {R::Barbarian, {TEXT("Extra Attack"), 5, C::Action, true}},
        {R::Monk, {TEXT("Martial Arts"), 1, C::BonusAction, false}}, {R::Monk, {TEXT("Ki"), 2, C::BonusAction, true}},
        {R::Monk, {TEXT("Deflect Missiles"), 3, C::Reaction, false}}, {R::Monk, {TEXT("Extra Attack"), 5, C::Action, true}},
        {R::Paladin, {TEXT("Lay on Hands"), 1, C::Action, true}}, {R::Paladin, {TEXT("Fighting Style"), 2, C::Free, true}},
        {R::Paladin, {TEXT("Divine Smite"), 2, C::Free, true}}, {R::Paladin, {TEXT("Channel Divinity"), 3, C::Action, false}},
        {R::Paladin, {TEXT("Extra Attack"), 5, C::Action, true}},
        {R::Ranger, {TEXT("Fighting Style"), 2, C::Free, true}}, {R::Ranger, {TEXT("Extra Attack"), 5, C::Action, true}},
        {R::Sorcerer, {TEXT("Spellcasting"), 1, C::Action, true}}, {R::Sorcerer, {TEXT("Sorcery Points"), 2, C::BonusAction, true}},
        {R::Warlock, {TEXT("Pact Magic"), 1, C::Action, true}},
        {R::Druid, {TEXT("Spellcasting"), 1, C::Action, true}}, {R::Druid, {TEXT("Wild Shape"), 2, C::Action, true}},
        {R::Bard, {TEXT("Spellcasting"), 1, C::Action, true}}, {R::Bard, {TEXT("Bardic Inspiration"), 1, C::BonusAction, true}},
    };
    TArray<FClassFeature> Out;
    for (const auto& Row : Table) { if (Row.Role == Role && Row.F.Level <= FMath::Min(Level, 5)) { Out.Add(Row.F); } }
    return Out;
}

FString FClassFeatureRules::FeatureSummary(EAdventurerRole Role, int32 Level)
{
    TArray<FString> Parts;
    for (const auto& F : FeaturesThrough(Role, Level)) { Parts.Add(FString::Printf(TEXT("%s [%s]%s"), F.Name, FStandardActionRules::CostLabel(F.Cost), F.bPlayable ? TEXT("") : TEXT("*"))); }
    return FString::Join(Parts, TEXT(", "));
}