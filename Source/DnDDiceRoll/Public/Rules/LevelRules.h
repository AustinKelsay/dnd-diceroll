#pragma once

#include "CoreMinimal.h"
#include "Rules/AbilityRules.h"

enum class EAdventurerRole : uint8;

/** SRD 5.1 character advancement (#79): experience, hit points by hit die, ability score improvements and multiclassing. */
struct DNDDICEROLL_API FLevelRules
{
    static constexpr int32 MaxLevel = 20;
    /** Experience needed to reach a character level (SRD advancement table). */
    static int32 XpForLevel(int32 Level);
    static int32 LevelForXp(int32 Xp);
    /** Class hit die: Barbarian d12; Fighter, Paladin, Ranger d10; Sorcerer, Wizard d6; everyone else d8. */
    static int32 HitDieSides(EAdventurerRole Role);
    /** Fixed (average) hit points gained at a later level: die/2 + 1 + CON, at least 1. */
    static int32 AverageLevelHitPoints(EAdventurerRole Role, int32 ConstitutionModifier);
    /** Maximum hit points: the first class gets its full die at character level 1, every other level the average. */
    static int32 MaxHitPoints(EAdventurerRole Primary, int32 PrimaryLevels, EAdventurerRole Secondary, int32 SecondaryLevels, int32 ConstitutionModifier);
    /** Class levels that grant an Ability Score Improvement (Fighter also 6 and 14, Rogue also 10). */
    static bool IsAsiLevel(EAdventurerRole Role, int32 ClassLevel);
    static int32 AsiCount(EAdventurerRole Role, int32 ClassLevel);
    /** +2 to one ability, or +1 to two (First == Second means +2); no score above 20. */
    static bool ApplyAsi(FAbilityScores& Scores, EAbility First, EAbility Second, FString& Error);
    /** SRD multiclassing prerequisites (13 in the listed abilities). */
    static bool MeetsMulticlassPrerequisite(EAdventurerRole Role, const FAbilityScores& Scores);
    /** Both the current class and the new class must meet their prerequisites. */
    static bool CanMulticlass(EAdventurerRole From, EAdventurerRole To, const FAbilityScores& Scores, FString& Error);
    /** Proficiencies gained when multiclassing into a class (SRD table), as display text. */
    static FString MulticlassProficiencies(EAdventurerRole Role);
    /** Spellcaster level contributed by class levels: full casters all, Paladin/Ranger half (down), Artificer half (up), others 0. */
    static int32 CasterLevel(EAdventurerRole Role, int32 ClassLevels);
    /** SRD multiclass spellcaster table: slots of SpellLevel (1..9) for a combined caster level (0..20). */
    static int32 MulticlassSlots(int32 CasterLevel, int32 SpellLevel);
};