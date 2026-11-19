#pragma once

#include "CoreMinimal.h"
#include "Rules/AbilityRules.h"

/** How a stat block's main action reaches its target. */
enum class EMonsterAttack : uint8 { Weapon, SaveCantrip };

/**
 * SRD 5.1 stat blocks (CC-BY-4.0, Wizards of the Coast) for the expedition's foes.
 * Attack bonus is proficiency + the attack ability modifier. HitPoints() is the average; RollHitPoints is seeded per creature (SRD).
 */
struct DNDDICEROLL_API FMonsterStatBlock
{
    const TCHAR* Name = TEXT("");
    int32 ArmorClass = 10;
    int32 HitDiceCount = 1;
    int32 HitDieSides = 8;
    FAbilityScores Scores;
    int32 ProficiencyBonus = 2;
    EAbility AttackAbility = EAbility::Strength;
    int32 Attacks = 1;          // Multiattack: attacks per Attack action
    int32 Range = 1;            // cells
    int32 DamageDieSides = 6;
    EMonsterAttack Kind = EMonsterAttack::Weapon;
    EAbility SpellAbility = EAbility::Wisdom; // SaveCantrip: DC 8 + proficiency + this modifier
    EAbility SaveAbility = EAbility::Dexterity;
    bool bWearsLeather = false;
    /** Behaviour flags for the shared enemy AI. */
    bool bSkirmisher = false;   // ranged: Disengage away when hurt and engaged, Hide when no shot
};

struct DNDDICEROLL_API FMonsterRules
{
    /** Stat block by role key (Bandit, Thug, Scout, Acolyte, Bandit Archer); null for legacy roles. */
    static const FMonsterStatBlock* Find(const FString& Role);
    static int32 HitPoints(const FMonsterStatBlock& Block);
    /** Sum of HitDiceCount rolls of HitDieSides + Con mod each; uses Stream (does not touch battle RNG). */
    static int32 RollHitPoints(const FMonsterStatBlock& Block, FRandomStream& Stream);
    /** SRD XP for defeating one creature of this role; 0 if unknown. */
    static int32 ExperienceReward(const FString& Role);
    static int32 AttackBonus(const FMonsterStatBlock& Block);
    static int32 DamageBonus(const FMonsterStatBlock& Block);
    static int32 SpellSaveDc(const FMonsterStatBlock& Block);
    /** Attacks per Attack action for a role; 1 without a stat block. */
    static int32 AttacksPerAction(const FString& Role);
    /**
     * Map Compact expedition roles onto SRD blocks (Raider->Bandit/Thug, Archer->Bandit Archer).
     * Dangerous routes use Thug; otherwise Bandit. Returns null when Role has no mapping.
     */
    static const FMonsterStatBlock* ResolveExpeditionRole(const FString& Role, bool bDangerous);
};
