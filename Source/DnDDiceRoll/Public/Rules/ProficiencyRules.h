#pragma once

#include "CoreMinimal.h"
#include "Rules/AbilityRules.h"

/** All 18 SRD skills. Order is stable and used as a bit index. */
enum class ESkill : uint8
{
    Athletics,
    Acrobatics, SleightOfHand, Stealth,
    Arcana, History, Investigation, Nature, Religion,
    AnimalHandling, Insight, Medicine, Perception, Survival,
    Deception, Intimidation, Performance, Persuasion
};

/** Tool proficiencies used by the current classes. More tools join with their content issues. */
enum class ETool : uint8 { ThievesTools, HerbalismKit };

/** A player's proficiency picks. Validated atomically by FProficiencyRules::BuildCharacter. */
struct DNDDICEROLL_API FProficiencyChoice
{
    TArray<ESkill> ClassSkills;
    /** Background-style picks: any two skills not already chosen. */
    TArray<ESkill> BackgroundSkills;
    TArray<ESkill> ExpertiseSkills;
};

/** Class proficiency facts (SRD 5.1 class tables). Small value data that can move into the #39 catalog. */
struct DNDDICEROLL_API FClassProficiencies
{
    uint8 SaveMask = 0;
    TArray<ESkill> SkillList;
    int32 SkillPicks = 0;
    uint8 ToolMask = 0;
    /** Expertise picks available at this level (Rogue: two proficient skills at level 1, per SRD 5.2.1). */
    int32 ExpertisePicks = 0;
};

/** A validated character's derived check facts. Value-only; no random numbers. */
struct DNDDICEROLL_API FCharacterRules
{
    FAbilityScores Scores;
    int32 Level = 1;
    uint8 SaveMask = 0;
    uint32 SkillMask = 0;
    uint32 ExpertiseMask = 0;
    uint8 ToolMask = 0;

    int32 ProficiencyBonus() const;
    int32 AbilityModifier(EAbility Ability) const;
    bool IsProficient(ESkill Skill) const;
    bool HasExpertise(ESkill Skill) const;
    bool IsSaveProficient(EAbility Ability) const;
    bool IsToolProficient(ETool Tool) const;
    /** Plain ability check with no proficiency. */
    int32 AbilityCheckBonus(EAbility Ability) const;
    /** Skill's ability modifier + proficiency (doubled with expertise) when proficient. */
    int32 SkillBonus(ESkill Skill) const;
    int32 SaveBonus(EAbility Ability) const;
    /** Tool check using the ability the situation calls for. */
    int32 ToolBonus(ETool Tool, EAbility Ability) const;
};

struct DNDDICEROLL_API FProficiencyRules
{
    static constexpr int32 SkillCount = 18;
    static constexpr int32 ToolCount = 2;
    static constexpr int32 BackgroundPicks = 2;
    /** SRD proficiency bonus: +2 at levels 1-4, +3 at 5-8, and so on. */
    static int32 ProficiencyBonus(int32 Level);
    static EAbility SkillAbility(ESkill Skill);
    static const TCHAR* SkillName(ESkill Skill);
    static const TCHAR* ToolName(ETool Tool);
    static FClassProficiencies ClassProficiencies(EAdventurerRole Role, int32 Level);
    /** Validates picks against the class table; Out is untouched when rejected. */
    static bool BuildCharacter(EAdventurerRole Role, int32 Level, const FAbilityScores& Scores,
        const FProficiencyChoice& Choice, FCharacterRules& Out, FString& Error);
    static FProficiencyChoice PrebuiltChoice(EAdventurerRole Role);
    static FCharacterRules PrebuiltCharacter(EAdventurerRole Role, int32 Level);
    static FCharacterRules EnemyCharacter(const FString& Role);
};
