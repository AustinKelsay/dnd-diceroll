#include "Rules/ProficiencyRules.h"
#include "Combat/CombatBattle.h"

namespace
{
uint32 SkillBit(ESkill Skill) { return 1u << static_cast<uint32>(Skill); }
uint8 AbilityBit(EAbility Ability) { return static_cast<uint8>(1u << static_cast<uint32>(Ability)); }
uint8 ToolBit(ETool Tool) { return static_cast<uint8>(1u << static_cast<uint32>(Tool)); }
}

int32 FCharacterRules::ProficiencyBonus() const { return FProficiencyRules::ProficiencyBonus(Level); }
int32 FCharacterRules::AbilityModifier(EAbility Ability) const { return FAbilityRules::Modifier(Scores.Get(Ability)); }
bool FCharacterRules::IsProficient(ESkill Skill) const { return (SkillMask & SkillBit(Skill)) != 0; }
bool FCharacterRules::HasExpertise(ESkill Skill) const { return (ExpertiseMask & SkillBit(Skill)) != 0; }
bool FCharacterRules::IsSaveProficient(EAbility Ability) const { return (SaveMask & AbilityBit(Ability)) != 0; }
bool FCharacterRules::IsToolProficient(ETool Tool) const { return (ToolMask & ToolBit(Tool)) != 0; }
int32 FCharacterRules::AbilityCheckBonus(EAbility Ability) const { return AbilityModifier(Ability); }

int32 FCharacterRules::SkillBonus(ESkill Skill) const
{
    const int32 Proficiency = IsProficient(Skill) ? ProficiencyBonus() * (HasExpertise(Skill) ? 2 : 1) : 0;
    return AbilityModifier(FProficiencyRules::SkillAbility(Skill)) + Proficiency;
}

int32 FCharacterRules::SaveBonus(EAbility Ability) const
{
    return AbilityModifier(Ability) + (IsSaveProficient(Ability) ? ProficiencyBonus() : 0);
}

int32 FCharacterRules::ToolBonus(ETool Tool, EAbility Ability) const
{
    const bool bExpert = (ToolExpertiseMask & ToolBit(Tool)) != 0;
    return AbilityModifier(Ability) + (IsToolProficient(Tool) ? ProficiencyBonus() * (bExpert ? 2 : 1) : 0);
}

int32 FProficiencyRules::ProficiencyBonus(int32 Level)
{
    return 2 + (FMath::Clamp(Level, 1, 20) - 1) / 4;
}

EAbility FProficiencyRules::SkillAbility(ESkill Skill)
{
    const int32 Index = static_cast<int32>(Skill);
    return Index == 0 ? EAbility::Strength : Index <= 3 ? EAbility::Dexterity : Index <= 8 ? EAbility::Intelligence :
        Index <= 13 ? EAbility::Wisdom : EAbility::Charisma;
}

const TCHAR* FProficiencyRules::SkillName(ESkill Skill)
{
    static const TCHAR* Names[SkillCount] = {
        TEXT("Athletics"), TEXT("Acrobatics"), TEXT("Sleight of Hand"), TEXT("Stealth"),
        TEXT("Arcana"), TEXT("History"), TEXT("Investigation"), TEXT("Nature"), TEXT("Religion"),
        TEXT("Animal Handling"), TEXT("Insight"), TEXT("Medicine"), TEXT("Perception"), TEXT("Survival"),
        TEXT("Deception"), TEXT("Intimidation"), TEXT("Performance"), TEXT("Persuasion")};
    return Names[FMath::Min<int32>(static_cast<int32>(Skill), SkillCount - 1)];
}

const TCHAR* FProficiencyRules::ToolName(ETool Tool)
{
    return Tool == ETool::ThievesTools ? TEXT("Thieves' tools") : TEXT("Herbalism kit");
}

FClassProficiencies FProficiencyRules::ClassProficiencies(EAdventurerRole Role, int32 Level)
{
    using S = ESkill;
    FClassProficiencies Class;
    auto Saves = [](EAbility A, EAbility B) { return static_cast<uint8>(AbilityBit(A) | AbilityBit(B)); };
    switch (Role)
    {
    case EAdventurerRole::Fighter:
        Class.SaveMask = Saves(EAbility::Strength, EAbility::Constitution);
        Class.SkillList = {S::Acrobatics, S::AnimalHandling, S::Athletics, S::History, S::Insight, S::Intimidation, S::Perception, S::Survival};
        Class.SkillPicks = 2;
        break;
    case EAdventurerRole::Rogue:
        Class.SaveMask = Saves(EAbility::Dexterity, EAbility::Intelligence);
        Class.SkillList = {S::Acrobatics, S::Athletics, S::Deception, S::Insight, S::Intimidation, S::Investigation, S::Perception,
            S::Performance, S::Persuasion, S::SleightOfHand, S::Stealth};
        Class.SkillPicks = 4;
        Class.ToolMask = ToolBit(ETool::ThievesTools);
        Class.ExpertisePicks = Level >= 6 ? 4 : 2;
        break;
    case EAdventurerRole::Cleric:
        Class.SaveMask = Saves(EAbility::Wisdom, EAbility::Charisma);
        Class.SkillList = {S::History, S::Insight, S::Medicine, S::Persuasion, S::Religion};
        Class.SkillPicks = 2;
        break;
    case EAdventurerRole::Wizard:
        Class.SaveMask = Saves(EAbility::Intelligence, EAbility::Wisdom);
        Class.SkillList = {S::Arcana, S::History, S::Insight, S::Investigation, S::Medicine, S::Religion};
        Class.SkillPicks = 2;
        break;
    default:
        break;
    }
    return Class;
}

bool FProficiencyRules::BuildCharacter(EAdventurerRole Role, int32 Level, const FAbilityScores& Scores,
    const FProficiencyChoice& Choice, FCharacterRules& Out, FString& Error)
{
    auto Reject = [&Error](const TCHAR* Reason) { Error = Reason; return false; };
    if (Role == EAdventurerRole::None || static_cast<uint8>(Role) > static_cast<uint8>(EAdventurerRole::Wizard)) { return Reject(TEXT("Unknown class.")); }
    if (Level < 1 || Level > 20) { return Reject(TEXT("Level must be 1..20.")); }
    if (!Scores.IsValid()) { return Reject(TEXT("Ability scores must be 1..30.")); }
    const FClassProficiencies Class = ClassProficiencies(Role, Level);
    FCharacterRules Candidate;
    Candidate.Scores = Scores;
    Candidate.Level = Level;
    Candidate.SaveMask = Class.SaveMask;
    Candidate.ToolMask = Class.ToolMask;
    auto Valid = [](ESkill Skill) { return static_cast<uint8>(Skill) < SkillCount; };
    if (Choice.ClassSkills.Num() != Class.SkillPicks) { return Reject(TEXT("Wrong number of class skill picks.")); }
    for (ESkill Skill : Choice.ClassSkills)
    {
        if (!Valid(Skill) || !Class.SkillList.Contains(Skill) || Candidate.IsProficient(Skill)) { return Reject(TEXT("Class skill is not on the class list or is repeated.")); }
        Candidate.SkillMask |= SkillBit(Skill);
    }
    if (Choice.BackgroundSkills.Num() != BackgroundPicks) { return Reject(TEXT("Choose exactly two background skills.")); }
    for (ESkill Skill : Choice.BackgroundSkills)
    {
        if (!Valid(Skill) || Candidate.IsProficient(Skill)) { return Reject(TEXT("Background skill is invalid or already proficient.")); }
        Candidate.SkillMask |= SkillBit(Skill);
    }
    const int32 ExpertiseCount = Choice.ExpertiseSkills.Num() + (Choice.bThievesToolsExpertise ? 1 : 0);
    if (ExpertiseCount != Class.ExpertisePicks) { return Reject(TEXT("Wrong number of expertise picks.")); }
    for (ESkill Skill : Choice.ExpertiseSkills)
    {
        if (!Valid(Skill) || !Candidate.IsProficient(Skill) || Candidate.HasExpertise(Skill)) { return Reject(TEXT("Expertise needs a proficient, unrepeated skill.")); }
        Candidate.ExpertiseMask |= SkillBit(Skill);
    }
    if (Choice.bThievesToolsExpertise)
    {
        if (!Candidate.IsToolProficient(ETool::ThievesTools)) { return Reject(TEXT("Expertise needs thieves' tools proficiency.")); }
        Candidate.ToolExpertiseMask |= ToolBit(ETool::ThievesTools);
    }
    Out = Candidate;
    Error.Reset();
    return true;
}

FProficiencyChoice FProficiencyRules::PrebuiltChoice(EAdventurerRole Role)
{
    using S = ESkill;
    FProficiencyChoice Choice;
    switch (Role)
    {
    case EAdventurerRole::Fighter: Choice.ClassSkills = {S::Athletics, S::Perception}; Choice.BackgroundSkills = {S::Intimidation, S::Survival}; break;
    case EAdventurerRole::Rogue:
        Choice.ClassSkills = {S::Stealth, S::SleightOfHand, S::Acrobatics, S::Deception};
        Choice.BackgroundSkills = {S::Insight, S::Investigation};
        Choice.ExpertiseSkills = {S::Stealth};
        Choice.bThievesToolsExpertise = true;
        break;
    case EAdventurerRole::Cleric: Choice.ClassSkills = {S::Medicine, S::Religion}; Choice.BackgroundSkills = {S::Insight, S::History}; break;
    case EAdventurerRole::Wizard: Choice.ClassSkills = {S::Arcana, S::Investigation}; Choice.BackgroundSkills = {S::History, S::Religion}; break;
    default: break;
    }
    return Choice;
}

FCharacterRules FProficiencyRules::PrebuiltCharacter(EAdventurerRole Role, int32 Level)
{
    FCharacterRules Character;
    FString Error;
    const bool bBuilt = BuildCharacter(Role, Level, FAbilityRules::PrebuiltScores(Role), PrebuiltChoice(Role), Character, Error);
    // Classes without a prebuilt skill choice (catalog-build classes) still get their ability scores for saves.
    if (!bBuilt) { Character = FCharacterRules(); Character.Scores = FAbilityRules::PrebuiltScores(Role); }
    return Character;
}

FCharacterRules FProficiencyRules::EnemyCharacter(const FString& Role)
{
    FCharacterRules Character;
    Character.Scores = FAbilityRules::EnemyScores(Role);
    const bool bSkirmisher = Role.Contains(TEXT("Archer")) || Role.Contains(TEXT("Scout"));
    Character.SkillMask = SkillBit(bSkirmisher ? ESkill::Perception : ESkill::Athletics) | (bSkirmisher ? SkillBit(ESkill::Stealth) : 0u);
    Character.SaveMask = AbilityBit(bSkirmisher ? EAbility::Dexterity : EAbility::Strength);
    return Character;
}
