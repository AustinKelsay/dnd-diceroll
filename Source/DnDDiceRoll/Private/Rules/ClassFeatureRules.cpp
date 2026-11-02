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

TArray<FClassFeature> FClassFeatureRules::DataFeaturesThrough(EAdventurerRole Role, int32 Level)
{
    using C = EActionCost; using R = EAdventurerRole;
    struct FRow { R Role; FClassFeature F; };
    static const FRow FeatureRows[] = {
        {R::Fighter, {TEXT("Fighting Style"), 1, C::Free, true}}, {R::Fighter, {TEXT("Second Wind"), 1, C::BonusAction, true}},
        {R::Fighter, {TEXT("Action Surge"), 2, C::Free, true}}, {R::Fighter, {TEXT("Superiority Dice"), 3, C::Free, true}},
        {R::Fighter, {TEXT("Extra Attack"), 5, C::Action, true}},
        {R::Rogue, {TEXT("Sneak Attack"), 1, C::Free, true}}, {R::Rogue, {TEXT("Cunning Action"), 2, C::BonusAction, true}},
        {R::Rogue, {TEXT("Uncanny Dodge"), 5, C::Reaction, true}},
        {R::Cleric, {TEXT("Spellcasting"), 1, C::Action, true}}, {R::Cleric, {TEXT("Channel Divinity"), 2, C::Action, true}},
        {R::Wizard, {TEXT("Spellcasting"), 1, C::Action, true}}, {R::Wizard, {TEXT("Arcane Recovery"), 1, C::Free, true}},
        {R::Wizard, {TEXT("Shield"), 1, C::Reaction, true}},
        {R::Barbarian, {TEXT("Rage"), 1, C::BonusAction, true}}, {R::Barbarian, {TEXT("Unarmored Defense"), 1, C::Free, true}}, {R::Barbarian, {TEXT("Reckless Attack"), 2, C::Free, true}},
        {R::Barbarian, {TEXT("Extra Attack"), 5, C::Action, true}},
        {R::Monk, {TEXT("Martial Arts"), 1, C::BonusAction, true}}, {R::Monk, {TEXT("Unarmored Defense"), 1, C::Free, true}}, {R::Monk, {TEXT("Ki"), 2, C::BonusAction, true}},
        {R::Monk, {TEXT("Deflect Missiles"), 3, C::Reaction, true}}, {R::Monk, {TEXT("Extra Attack"), 5, C::Action, true}},
        {R::Paladin, {TEXT("Lay on Hands"), 1, C::Action, true}}, {R::Paladin, {TEXT("Fighting Style"), 2, C::Free, true}},
        {R::Paladin, {TEXT("Divine Smite"), 2, C::Free, true}}, {R::Paladin, {TEXT("Channel Divinity"), 3, C::Action, true}},
        {R::Paladin, {TEXT("Extra Attack"), 5, C::Action, true}},
        {R::Ranger, {TEXT("Fighting Style"), 2, C::Free, true}}, {R::Ranger, {TEXT("Extra Attack"), 5, C::Action, true}},
        {R::Sorcerer, {TEXT("Spellcasting"), 1, C::Action, true}}, {R::Sorcerer, {TEXT("Sorcery Points"), 2, C::BonusAction, true}},
        {R::Sorcerer, {TEXT("Metamagic: Quickened Spell"), 3, C::Free, true}}, {R::Sorcerer, {TEXT("Counterspell"), 5, C::Reaction, true}},
        {R::Wizard, {TEXT("Counterspell"), 5, C::Reaction, true}},
        {R::Warlock, {TEXT("Pact Magic"), 1, C::Action, true}},
        {R::Druid, {TEXT("Spellcasting"), 1, C::Action, true}}, {R::Druid, {TEXT("Wild Shape"), 2, C::Action, true}},
        {R::Bard, {TEXT("Spellcasting"), 1, C::Action, true}}, {R::Bard, {TEXT("Bardic Inspiration"), 1, C::BonusAction, true}},
        // SRD 5.1 levels 1-5 gap fill (#rules audit): SRD subclass named first; * = listed, not yet modelled.
        {R::Fighter, {TEXT("Champion: Improved Critical"), 3, C::Free, true}},
        {R::Rogue, {TEXT("Expertise"), 1, C::Free, false}}, {R::Rogue, {TEXT("Thief: Fast Hands"), 3, C::BonusAction, false}},
        {R::Cleric, {TEXT("Life: Disciple of Life"), 1, C::Free, false}}, {R::Cleric, {TEXT("Destroy Undead"), 5, C::Action, false}},
        {R::Wizard, {TEXT("Evocation: Sculpt Spells"), 2, C::Free, false}},
        {R::Barbarian, {TEXT("Danger Sense"), 2, C::Free, false}}, {R::Barbarian, {TEXT("Berserker: Frenzy"), 3, C::BonusAction, false}},
        {R::Barbarian, {TEXT("Fast Movement"), 5, C::Free, false}},
        {R::Monk, {TEXT("Unarmored Movement"), 2, C::Free, false}}, {R::Monk, {TEXT("Open Hand Technique"), 3, C::Free, false}},
        {R::Monk, {TEXT("Slow Fall"), 4, C::Reaction, false}}, {R::Monk, {TEXT("Stunning Strike"), 5, C::Free, false}},
        {R::Paladin, {TEXT("Divine Sense"), 1, C::Action, false}}, {R::Paladin, {TEXT("Spellcasting"), 2, C::Action, true}},
        {R::Paladin, {TEXT("Divine Health"), 3, C::Free, false}},
        {R::Ranger, {TEXT("Favored Enemy"), 1, C::Free, false}}, {R::Ranger, {TEXT("Natural Explorer"), 1, C::Free, false}},
        {R::Ranger, {TEXT("Spellcasting"), 2, C::Action, true}}, {R::Ranger, {TEXT("Hunter: Colossus Slayer"), 3, C::Free, false}},
        {R::Ranger, {TEXT("Primeval Awareness"), 3, C::Action, false}},
        {R::Sorcerer, {TEXT("Draconic: Dragon Resilience"), 1, C::Free, false}},
        // Warlock and Bard follow SRD 5.2.1 (docs/srd-source-notes.md): the Pact Boon is now the Pact of the Blade/Chain/Tome invocations,
        // the subclass (Fiend Patron, College of Lore) arrives at 3, Expertise at Bard 2, and Song of Rest is gone (SRD 5.1 only).
        {R::Warlock, {TEXT("Eldritch Invocations"), 1, C::Free, false}}, {R::Warlock, {TEXT("Magical Cunning"), 2, C::Free, true}},
        {R::Warlock, {TEXT("Fiend: Dark One's Blessing"), 3, C::Free, false}}, {R::Warlock, {TEXT("Fiend: Fiend Spells"), 3, C::Free, false}},
        {R::Druid, {TEXT("Land: Natural Recovery"), 2, C::Free, false}},
        {R::Bard, {TEXT("Expertise"), 2, C::Free, false}}, {R::Bard, {TEXT("Jack of All Trades"), 2, C::Free, false}},
        {R::Bard, {TEXT("Lore: Bonus Proficiencies"), 3, C::Free, false}}, {R::Bard, {TEXT("Lore: Cutting Words"), 3, C::Reaction, false}},
        {R::Bard, {TEXT("Font of Inspiration"), 5, C::Free, false}},
        // Data past the engine cap (EngineLevelCap = 5): listed by DataFeaturesThrough, never by FeaturesThrough.
        {R::Warlock, {TEXT("Fiend: Dark One's Own Luck"), 6, C::Free, false}}, {R::Warlock, {TEXT("Ability Score Improvement"), 8, C::Free, false}},
        {R::Warlock, {TEXT("Contact Patron"), 9, C::Action, false}}, {R::Warlock, {TEXT("Fiend: Fiendish Resilience"), 10, C::Free, false}},
        {R::Warlock, {TEXT("Mystic Arcanum (level 6 spell)"), 11, C::Action, false}}, {R::Warlock, {TEXT("Ability Score Improvement"), 12, C::Free, false}},
        {R::Warlock, {TEXT("Mystic Arcanum (level 7 spell)"), 13, C::Action, false}}, {R::Warlock, {TEXT("Fiend: Hurl Through Hell"), 14, C::Free, false}},
        {R::Warlock, {TEXT("Mystic Arcanum (level 8 spell)"), 15, C::Action, false}}, {R::Warlock, {TEXT("Ability Score Improvement"), 16, C::Free, false}},
        {R::Warlock, {TEXT("Mystic Arcanum (level 9 spell)"), 17, C::Action, false}}, {R::Warlock, {TEXT("Epic Boon"), 19, C::Free, false}},
        {R::Warlock, {TEXT("Eldritch Master"), 20, C::Free, false}},
        {R::Bard, {TEXT("Lore: Magical Discoveries"), 6, C::Free, false}}, {R::Bard, {TEXT("Countercharm"), 7, C::Reaction, false}},
        {R::Bard, {TEXT("Ability Score Improvement"), 8, C::Free, false}}, {R::Bard, {TEXT("Expertise (two more skills)"), 9, C::Free, false}},
        {R::Bard, {TEXT("Magical Secrets"), 10, C::Free, false}}, {R::Bard, {TEXT("Ability Score Improvement"), 12, C::Free, false}},
        {R::Bard, {TEXT("Lore: Peerless Skill"), 14, C::Free, false}}, {R::Bard, {TEXT("Ability Score Improvement"), 16, C::Free, false}},
        {R::Bard, {TEXT("Superior Inspiration"), 18, C::Free, false}}, {R::Bard, {TEXT("Epic Boon"), 19, C::Free, false}},
        {R::Bard, {TEXT("Words of Creation"), 20, C::Free, false}},
        {R::Fighter, {TEXT("Ability Score Improvement"), 4, C::Free, true}}, {R::Rogue, {TEXT("Ability Score Improvement"), 4, C::Free, true}},
        {R::Cleric, {TEXT("Ability Score Improvement"), 4, C::Free, true}}, {R::Wizard, {TEXT("Ability Score Improvement"), 4, C::Free, true}},
        {R::Barbarian, {TEXT("Ability Score Improvement"), 4, C::Free, true}}, {R::Monk, {TEXT("Ability Score Improvement"), 4, C::Free, true}},
        {R::Paladin, {TEXT("Ability Score Improvement"), 4, C::Free, true}}, {R::Ranger, {TEXT("Ability Score Improvement"), 4, C::Free, true}},
        {R::Sorcerer, {TEXT("Ability Score Improvement"), 4, C::Free, true}}, {R::Warlock, {TEXT("Ability Score Improvement"), 4, C::Free, true}},
        {R::Druid, {TEXT("Ability Score Improvement"), 4, C::Free, true}}, {R::Bard, {TEXT("Ability Score Improvement"), 4, C::Free, true}},
    };
    TArray<FClassFeature> Out;
    for (const auto& Row : FeatureRows) { if (Row.Role == Role && Row.F.Level <= Level) { Out.Add(Row.F); } }
    return Out;
}

TArray<FClassFeature> FClassFeatureRules::FeaturesThrough(EAdventurerRole Role, int32 Level)
{
    return DataFeaturesThrough(Role, FMath::Min(Level, EngineLevelCap));
}

FString FClassFeatureRules::FeatureSummary(EAdventurerRole Role, int32 Level)
{
    TArray<FString> Parts;
    for (const auto& F : FeaturesThrough(Role, Level)) { Parts.Add(FString::Printf(TEXT("%s [%s]%s"), F.Name, FStandardActionRules::CostLabel(F.Cost), F.bPlayable ? TEXT("") : TEXT("*"))); }
    return FString::Join(Parts, TEXT(", "));
}