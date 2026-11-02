#pragma once

#include "CoreMinimal.h"
#include "Combat/CombatBattle.h"
#include "Rules/PointBuyRules.h"

struct FPartyBuildSelection;

/**
 * Character creator draft (LOCAL PROPOSAL: character creation is outside the approved scope in CONTEXT.md).
 * Everything lives in this file pair, PointBuyRules, the additive FHeroSheet field and the HUD overlay, so it can be dropped cleanly.
 * Rules: SRD 5.2.1 27-point buy (8..15), level-1 class from the 12 SRD classes, class skill picks and level-1 Expertise.
 * Deliberately no species/background (no background skills, no +2/+1). Colour is the only cosmetic choice.
 */
struct DNDDICEROLL_API FCharacterDraft
{
    FString Name;
    EAdventurerRole Role = EAdventurerRole::Fighter;
    FAbilityScores Scores;
    TArray<ESkill> Skills;
    TArray<ESkill> Expertise;
    int32 Colors[3] = {0, 1, 2};
};

struct DNDDICEROLL_API FCharacterCreator
{
    static constexpr int32 PaletteSize = 8;
    static constexpr int32 ColorSlots = 3;
    static constexpr int32 MaxNameLength = 24;

    /** The 12 SRD 5.2.1 classes in SRD order (Artificer excluded). */
    static const TArray<EAdventurerRole>& Classes();
    static const TCHAR* ClassName(EAdventurerRole Role);
    /** Short level-1 SRD feature summary for the class picker. */
    static FString FeatureSummary(EAdventurerRole Role);
    static int32 HitDie(EAdventurerRole Role);
    static FLinearColor PaletteColor(int32 Index);
    static const TCHAR* PaletteName(int32 Index);
    static const TCHAR* SlotName(int32 Slot);

    /** Recommended standard array for the class, no skills, default colours. */
    static FCharacterDraft NewDraft(EAdventurerRole Role);
    /** Changes class; clears skills and Expertise, loads that class's recommended array. */
    static void SetClass(FCharacterDraft& Draft, EAdventurerRole Role);
    static int32 SkillPicks(EAdventurerRole Role);
    static int32 ExpertisePicks(EAdventurerRole Role);
    static TArray<ESkill> SkillList(EAdventurerRole Role);
    /** Adds or removes a class skill; refuses when full or off-list. Removing a skill also drops its Expertise. */
    static bool ToggleSkill(FCharacterDraft& Draft, ESkill Skill);
    /** Adds or removes Expertise on a chosen skill; refuses when full. */
    static bool ToggleExpertise(FCharacterDraft& Draft, ESkill Skill);
    static void CycleColor(FCharacterDraft& Draft, int32 Slot, int32 Step);
    /** Catalog kit a creator hero uses: the first published build of the class (starting equipment and features). */
    static FString BuildIdFor(EAdventurerRole Role);

    /** Full draft validation: name, SRD class, point buy, skills, Expertise and colours. */
    static bool Validate(const FCharacterDraft& Draft, FString& Error);
    static FHeroSheet ToSheet(const FCharacterDraft& Draft);
    static bool ToSelection(const FCharacterDraft& Draft, int32 SlotId, FPartyBuildSelection& Out, FString& Error);
    /** Rebuilds a draft from a saved selection (for re-editing). */
    static bool FromSelection(const FPartyBuildSelection& Selection, FCharacterDraft& Out);
    static bool ValidateSheet(EAdventurerRole Role, const FHeroSheet& Sheet, FString& Error);

    /**
     * Applies a validated sheet to a freshly created catalog hero: sets Sheet, recomputes MaxHP from the new CON,
     * and shifts the flat catalog values that were tuned to the prebuilt scores (initiative by the DEX change,
     * unarmed attack by the key-ability change, unarmored AC by the DEX change). Weapon attacks, armor AC,
     * saves and skills already derive from the scores. Hero is untouched on failure.
     */
    static bool Apply(const FHeroSheet& Sheet, FCombatantState& Hero, FString& Error);
    /** Saves, skills and Expertise for a custom hero at its current level; later Expertise is filled deterministically. */
    static FCharacterRules SheetCharacter(const FCombatantState& Hero);

    /** Saved roster (separate file from the expedition save so the creator stays isolated). */
    static FString RosterPath();
    static FString ToJson(const TArray<FCharacterDraft>& Roster);
    /** Strict: every entry must validate. Out is untouched on failure. */
    static bool FromJson(const FString& Json, TArray<FCharacterDraft>& Out, FString& Error);
    static bool SaveRoster(const TArray<FCharacterDraft>& Roster, const FString& Path, FString& Error);
    static bool LoadRoster(const FString& Path, TArray<FCharacterDraft>& Out, FString& Error);
    static constexpr int32 MaxRoster = 32;
};
