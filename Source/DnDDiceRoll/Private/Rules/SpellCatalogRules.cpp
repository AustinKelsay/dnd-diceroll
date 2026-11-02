#include "Rules/SpellCatalogRules.h"
#include "Combat/CombatBattle.h"

namespace SpellCatalog
{
    static const FSrdSpell Spells[] = {
#include "SpellCatalogData.inl"
    };
    static const TCHAR* ClassNames[] = { TEXT("Bard"), TEXT("Cleric"), TEXT("Druid"), TEXT("Paladin"), TEXT("Ranger"), TEXT("Sorcerer"), TEXT("Warlock"), TEXT("Wizard") };
    static int32 ByLevel(EAdventurerRole Role, int32 Level, const int32 (&Table)[20]) { return Level < 1 ? 0 : Table[FMath::Min(Level, 20) - 1]; }
}

int32 FSpellCatalogRules::Num() { return UE_ARRAY_COUNT(SpellCatalog::Spells); }
const FSrdSpell& FSpellCatalogRules::At(int32 Index) { return SpellCatalog::Spells[FMath::Clamp(Index, 0, Num() - 1)]; }

const FSrdSpell* FSpellCatalogRules::Find(const FString& IdOrName)
{
    for (const FSrdSpell& S : SpellCatalog::Spells) { if (IdOrName.Equals(S.Id, ESearchCase::IgnoreCase) || IdOrName.Equals(S.Name, ESearchCase::IgnoreCase)) { return &S; } }
    return nullptr;
}

uint16 FSpellCatalogRules::ClassBit(EAdventurerRole Role)
{
    switch (Role)
    {
    case EAdventurerRole::Bard: return SpellClass::Bard;
    case EAdventurerRole::Cleric: return SpellClass::Cleric;
    case EAdventurerRole::Druid: return SpellClass::Druid;
    case EAdventurerRole::Paladin: return SpellClass::Paladin;
    case EAdventurerRole::Ranger: return SpellClass::Ranger;
    case EAdventurerRole::Sorcerer: return SpellClass::Sorcerer;
    case EAdventurerRole::Warlock: return SpellClass::Warlock;
    case EAdventurerRole::Wizard: return SpellClass::Wizard;
    default: return 0;
    }
}

bool FSpellCatalogRules::OnClassList(EAdventurerRole Role, const FSrdSpell& Spell) { return (Spell.Classes & ClassBit(Role)) != 0; }
bool FSpellCatalogRules::HasCondition(const FSrdSpell& Spell, ESpellCondition Condition) { return (Spell.Conditions & (1u << (uint32)Condition)) != 0; }

EActionCost FSpellCatalogRules::ActionCost(const FSrdSpell& Spell)
{
    switch (Spell.CastTime)
    {
    case ESpellCastTime::BonusAction: return EActionCost::BonusAction;
    case ESpellCastTime::Reaction: return EActionCost::Reaction;
    default: return EActionCost::Action;
    }
}

int32 FSpellCatalogRules::DiceCount(const FSrdSpell& Spell, int32 CharacterLevel, int32 SlotLevel)
{
    if (Spell.Level == 0)
    {
        const int32 Tier = CharacterLevel >= 17 ? 3 : CharacterLevel >= 11 ? 2 : CharacterLevel >= 5 ? 1 : 0;
        return Spell.Dice[Tier];
    }
    return SlotLevel < Spell.Level || SlotLevel > 9 ? 0 : Spell.Dice[SlotLevel];
}

int32 FSpellCatalogRules::AverageEffect(const FSrdSpell& Spell, int32 CharacterLevel, int32 SlotLevel, int32 SpellModifier)
{
    if (Spell.Effect != ESpellEffect::Damage && Spell.Effect != ESpellEffect::Healing) { return 0; }
    if (Spell.Level > 0 && (SlotLevel < Spell.Level || SlotLevel > 9)) { return 0; }
    const int32 Count = DiceCount(Spell, CharacterLevel, SlotLevel);
    const int32 Flat = Spell.bAddModifier ? SpellModifier : Spell.FlatBonus;
    // Fixed-value effects (Aid, Heal) store the amount per slot in Dice with no die size.
    return Spell.DieSides == 0 ? Count + Flat : (Count * (Spell.DieSides + 1)) / 2 + Flat;
}

int32 FSpellCatalogRules::CantripsKnown(EAdventurerRole Role, int32 ClassLevel)
{
    if (ClassLevel < 1) { return 0; }
    int32 Base = 0;
    switch (Role)
    {
    case EAdventurerRole::Bard: case EAdventurerRole::Druid: case EAdventurerRole::Warlock: Base = 2; break;
    case EAdventurerRole::Cleric: case EAdventurerRole::Wizard: Base = 3; break;
    case EAdventurerRole::Sorcerer: Base = 4; break;
    default: return 0;
    }
    return Base + (ClassLevel >= 4 ? 1 : 0) + (ClassLevel >= 10 ? 1 : 0);
}

bool FSpellCatalogRules::PreparesSpells(EAdventurerRole Role)
{
    return Role == EAdventurerRole::Cleric || Role == EAdventurerRole::Druid || Role == EAdventurerRole::Paladin || Role == EAdventurerRole::Wizard;
}

int32 FSpellCatalogRules::SpellsKnownOrPrepared(EAdventurerRole Role, int32 ClassLevel, int32 AbilityModifier)
{
    static const int32 Bard[20] = { 4, 5, 6, 7, 8, 9, 10, 11, 12, 14, 15, 15, 16, 18, 19, 19, 20, 22, 22, 22 };
    static const int32 Sorcerer[20] = { 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 12, 13, 13, 14, 14, 15, 15, 15, 15 };
    static const int32 Warlock[20] = { 2, 3, 4, 5, 6, 7, 8, 9, 10, 10, 11, 11, 12, 12, 13, 13, 14, 14, 15, 15 };
    static const int32 Ranger[20] = { 0, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11 };
    if (ClassLevel < 1) { return 0; }
    switch (Role)
    {
    case EAdventurerRole::Bard: return SpellCatalog::ByLevel(Role, ClassLevel, Bard);
    case EAdventurerRole::Sorcerer: return SpellCatalog::ByLevel(Role, ClassLevel, Sorcerer);
    case EAdventurerRole::Warlock: return SpellCatalog::ByLevel(Role, ClassLevel, Warlock);
    case EAdventurerRole::Ranger: return SpellCatalog::ByLevel(Role, ClassLevel, Ranger);
    case EAdventurerRole::Cleric: case EAdventurerRole::Druid: case EAdventurerRole::Wizard: return FMath::Max(1, AbilityModifier + ClassLevel);
    case EAdventurerRole::Paladin: return ClassLevel < 2 ? 0 : FMath::Max(1, AbilityModifier + ClassLevel / 2);
    default: return 0;
    }
}

int32 FSpellCatalogRules::MaxLearnableLevel(EAdventurerRole Role, int32 ClassLevel)
{
    if (ClassLevel < 1) { return 0; }
    const int32 L = FMath::Min(ClassLevel, 20);
    switch (Role)
    {
    case EAdventurerRole::Bard: case EAdventurerRole::Cleric: case EAdventurerRole::Druid: case EAdventurerRole::Sorcerer: case EAdventurerRole::Wizard:
        return FMath::Min(9, (L + 1) / 2);
    case EAdventurerRole::Paladin: case EAdventurerRole::Ranger:
        return L < 2 ? 0 : FMath::Min(5, (L + 3) / 4);
    case EAdventurerRole::Warlock:
        // Pact Magic slot level; Mystic Arcanum (6th-9th from Warlock 11) is not modelled.
        return FMath::Min(5, (L + 1) / 2);
    default:
        return 0;
    }
}

bool FSpellCatalogRules::CanLearn(EAdventurerRole Role, int32 ClassLevel, const FSrdSpell& Spell)
{
    if (!OnClassList(Role, Spell) || ClassLevel < 1) { return false; }
    return Spell.Level == 0 ? CantripsKnown(Role, ClassLevel) > 0 : Spell.Level <= MaxLearnableLevel(Role, ClassLevel);
}

bool FSpellCatalogRules::HeroCanLearn(EAdventurerRole Primary, int32 PrimaryLevels, EAdventurerRole Secondary, int32 SecondaryLevels, const FSrdSpell& Spell)
{
    return CanLearn(Primary, PrimaryLevels, Spell) || (Secondary != EAdventurerRole::None && CanLearn(Secondary, SecondaryLevels, Spell));
}

FString FSpellCatalogRules::ClassListText(uint16 Classes)
{
    TArray<FString> Names;
    for (int32 I = 0; I < 8; ++I) { if (Classes & (1 << I)) { Names.Add(SpellCatalog::ClassNames[I]); } }
    return FString::Join(Names, TEXT(", "));
}

const TCHAR* FSpellCatalogRules::CastTimeTag(ESpellCastTime CastTime)
{
    switch (CastTime)
    {
    case ESpellCastTime::Action: return TEXT("Action");
    case ESpellCastTime::BonusAction: return TEXT("Bonus Action");
    case ESpellCastTime::Reaction: return TEXT("Reaction");
    default: return TEXT("Extended");
    }
}

FString FSpellCatalogRules::Tags(const FSrdSpell& Spell)
{
    static const TCHAR* Ordinals[] = { TEXT(""), TEXT("1st"), TEXT("2nd"), TEXT("3rd"), TEXT("4th"), TEXT("5th"), TEXT("6th"), TEXT("7th"), TEXT("8th"), TEXT("9th") };
    const FString LevelText = Spell.Level == 0 ? FString::Printf(TEXT("%s cantrip"), FSpellRules::SchoolName(Spell.School)) :
        FString::Printf(TEXT("%s-level %s"), Ordinals[FMath::Clamp(Spell.Level, 1, 9)], FSpellRules::SchoolName(Spell.School));
    TArray<FString> Parts;
    if (Spell.bVerbal) { Parts.Add(TEXT("V")); }
    if (Spell.bSomatic) { Parts.Add(TEXT("S")); }
    if (Spell.bMaterial) { Parts.Add(TEXT("M")); }
    const FString Time = Spell.CastTime == ESpellCastTime::Extended ? FString(Spell.CastingTimeText) : FString(CastTimeTag(Spell.CastTime));
    FString Text = FString::Printf(TEXT("%s | %s | %s | %s | %s | %s"), *LevelText, *ClassListText(Spell.Classes), *Time, Spell.RangeText, *FString::Join(Parts, TEXT(", ")), Spell.Duration);
    if (Spell.bConcentration) { Text += TEXT(" | Concentration"); }
    if (Spell.bRitual) { Text += TEXT(" | Ritual"); }
    if (Spell.Implementation == ESpellImplementation::DataOnly) { Text += TEXT(" | Effect not yet modelled"); }
    return Text;
}
