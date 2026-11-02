#pragma once

#include "CoreMinimal.h"

/** Bard class tables (SRD 5.2.1 levels 1-20) as pure data; the engine plays levels 1-5 (FClassFeatureRules::EngineLevelCap). */
struct DNDDICEROLL_API FBardRules
{
    /** Bardic die: d6 at 1, d8 at 5, d10 at 10, d12 at 15. */
    static int32 InspirationDie(int32 BardLevel);
    /** Uses = CHA modifier, minimum once (Bard 1+). */
    static int32 InspirationUses(int32 BardLevel, int32 CharismaModifier);
    /** Font of Inspiration (5+): Bardic Inspiration also comes back on a Short Rest, and a spell slot can buy one use back. */
    static bool HasFontOfInspiration(int32 BardLevel);
    /** Expertise: two skills at 2, two more at 9. */
    static int32 ExpertiseSkills(int32 BardLevel);
    /** Jack of All Trades (2+): half the Proficiency Bonus, rounded down, on ability checks without proficiency. */
    static int32 JackOfAllTrades(int32 BardLevel, int32 ProficiencyBonus);
    /** Ability-check bonus from proficiency: Expertise doubles it, plain proficiency adds it, otherwise Jack of All Trades. */
    static int32 CheckProficiency(int32 BardLevel, int32 ProficiencyBonus, bool bProficient, bool bExpertise);
    /** Bard table "Prepared Spells" (SRD 5.2.1): 4,5,6,7,9,10,11,12,14,15,16,16,17,17,18,18,19,20,21,22. */
    static int32 PreparedSpells(int32 BardLevel);
    /** College of Lore 3: Bonus Proficiencies in three skills. */
    static int32 LoreBonusSkills(int32 BardLevel);
    /** Magical Secrets (10+): Bard, Cleric, Druid and Wizard lists all count as Bard spells. */
    static bool MagicalSecretsAllows(int32 BardLevel, const FString& ClassList);
    /** Superior Inspiration (18+): roll Initiative with fewer than 2 uses and regain up to 2. */
    static int32 SuperiorInspirationFloor(int32 BardLevel);
};