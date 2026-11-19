#pragma once

#include "CoreMinimal.h"

enum class EAdventurerRole : uint8;

/** Levels in each adventuring class (multiclass). Character level = sum of entries. */
struct DNDDICEROLL_API FClassLevels
{
    int32 Fighter = 0;
    int32 Rogue = 0;
    int32 Cleric = 0;
    int32 Wizard = 0;
    int32 Barbarian = 0;
    int32 Monk = 0;
    int32 Paladin = 0;
    int32 Ranger = 0;
    int32 Sorcerer = 0;
    int32 Warlock = 0;
    int32 Druid = 0;
    int32 Bard = 0;
    int32 Artificer = 0;

    int32 Get(EAdventurerRole Role) const;
    void Set(EAdventurerRole Role, int32 Levels);
    /** Sum of all class levels (character level). */
    int32 CharacterLevel() const;
    /** Roles with at least one level, stable enum order. */
    TArray<EAdventurerRole> ClassesTaken() const;
};

/**
 * Pure SRD advancement math (ADR-0001): XP, rolled HP, multiclass gates/slots, class aura colors.
 * Feats: data-driven hook reserved; do not ship feat rows until licensing is settled — ASI-only for now.
 */
struct DNDDICEROLL_API FAdvancementRules
{
    /** Playable band for this stack (features / slots still capped here). */
    static constexpr int32 EngineLevelCap = 5;
    /** XP table supports through 15 for the product goal. */
    static constexpr int32 XpTableCap = 15;
    static constexpr int32 AbsoluteLevelCap = 20;
    /** SRD multiclass ability-score prerequisite (default ON; product question if we relax it). */
    static constexpr int32 MulticlassAbilityPrerequisite = 13;

    /** Cumulative XP required to be this character level (SRD 5.2.1). Level 1 = 0. */
    static int32 XpForLevel(int32 Level);
    static int32 LevelFromTotalXp(int32 TotalXp);
    static int32 XpToNextLevel(int32 CurrentLevel, int32 TotalXp);
    /** Pending character level-ups TotalXp unlocks above CurrentCharacterLevel (apply between battles only). */
    static int32 PendingLevelUps(int32 CurrentCharacterLevel, int32 TotalXp);

    static int32 HitDieSides(EAdventurerRole Role);
    /** Level 1 max HP: hit die max + Con mod (never below 1). */
    static int32 MaxHpAtLevel1(EAdventurerRole Role, int32 ConstitutionModifier);
    /**
     * HP gained for one new class level after character level 1.
     * HitDieRoll is the seeded d(HitDieSides) result (1..sides); never below 1 after Con mod.
     */
    static int32 HpGainFromRoll(int32 HitDieRoll, int32 ConstitutionModifier);
    /** Convenience: HpGainFromRoll after clamping Roll into 1..HitDieSides(Role). */
    static int32 HpGainForClassLevel(EAdventurerRole Role, int32 HitDieRoll, int32 ConstitutionModifier);

    static int32 ProficiencyBonus(int32 CharacterLevel);

    /** Subclass choice class-level (3 for current roster; 0 = none). */
    static int32 SubclassChoiceLevel(EAdventurerRole Role);
    /** True when the class level being taken equals the subclass gate (existing subclasses first). */
    static bool OffersSubclassAt(EAdventurerRole Role, int32 NewClassLevel);
    /**
     * True when gaining NewClassLevel in Role grants Ability Score Improvement (SRD 5.2.1 class tables): class levels 4, 8, 12 and 16
     * for every class, plus Fighter 6 and 14 and Rogue 10. Keyed by class level, not character level: a Fighter 2 / Rogue 2 has none,
     * and a Fighter 4 / Rogue 1 has one. Level 19 is Epic Boon, not an ASI. Feat picks deferred (licensing).
     */
    static bool OffersAsiAt(EAdventurerRole Role, int32 NewClassLevel);

    /** Cantrips column of the SRD 5.2.1 class table at ClassLevel (clamped to 1..EngineLevelCap); 0 for classes without cantrips. */
    static int32 CantripsKnown(EAdventurerRole Role, int32 ClassLevel);
    /**
     * Prepared Spells column of the SRD 5.2.1 class table at ClassLevel (clamped to 1..EngineLevelCap). 5.2.1 fixes the count per
     * class level, so SpellcastingAbilityModifier is ignored for SRD classes; only the non-SRD roster Artificer still reads it.
     */
    static int32 SpellsKnownOrPrepared(EAdventurerRole Role, int32 ClassLevel, int32 SpellcastingAbilityModifier);

    /** SRD multiclass: ability score >= 13 for the class (and both for some). Scores are full 1-30 values. */
    static bool MeetsMulticlassPrerequisite(EAdventurerRole Role, int32 Str, int32 Dex, int32 Con, int32 Intelligence, int32 Wis, int32 Cha);
    /** Proficiencies granted when taking the first level in a new class while already having another (SRD multiclass table). */
    static TArray<FString> MulticlassProficienciesGained(EAdventurerRole NewClass);
    /**
     * Multiclass spellcaster level for the spell slot table (Warlock pact slots stay separate via SlotsFor(Warlock,...)).
     * Full casters +1/level; half (Paladin/Ranger) +1 per 2 levels; Artificer +1 per 2 levels rounded up.
     */
    static int32 MulticlassSpellcasterLevel(const FClassLevels& Levels);
    /** Slots from the full-caster table at MulticlassSpellcasterLevel (levels 1-3 only in engine band). */
    static int32 MulticlassSlotsFor(const FClassLevels& Levels, int32 SpellLevel);
};