#pragma once

#include "CoreMinimal.h"

enum class EAdventurerRole : uint8;

/** The six SRD abilities. Order is stable and used as an index. */
enum class EAbility : uint8 { Strength, Dexterity, Constitution, Intelligence, Wisdom, Charisma };

/** Value-only ability scores (SRD range 1..30). Plain data so it can move into the build catalog (#39) later. */
struct DNDDICEROLL_API FAbilityScores
{
    int32 Strength = 10;
    int32 Dexterity = 10;
    int32 Constitution = 10;
    int32 Intelligence = 10;
    int32 Wisdom = 10;
    int32 Charisma = 10;

    int32 Get(EAbility Ability) const;
    bool IsValid() const;
};

/** Pure ability rules. No state and no random numbers. */
struct DNDDICEROLL_API FAbilityRules
{
    static constexpr int32 AbilityCount = 6;
    static constexpr int32 MinScore = 1;
    static constexpr int32 MaxScore = 30;
    /** SRD modifier: floor((score - 10) / 2). */
    static int32 Modifier(int32 Score);
    static const TCHAR* AbilityName(EAbility Ability);
    static const TCHAR* AbilityAbbreviation(EAbility Ability);
    /**
     * Prebuilt adventurer scores. They are chosen so the existing fixed profiles stay unchanged at
     * level 1: key attack ability modifier + proficiency equals the prebuilt attack bonus.
     */
    static FAbilityScores PrebuiltScores(EAdventurerRole Role);
    /** Generic enemy scores keyed by the display role ("Archer", "Scout", ...). */
    static FAbilityScores EnemyScores(const FString& Role);
    /** Ability that drives the role's basic attack (Fighter STR, Rogue DEX, Cleric WIS, Wizard INT). */
    static EAbility AttackAbility(EAdventurerRole Role);
};
