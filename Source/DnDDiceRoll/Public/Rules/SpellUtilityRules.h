#pragma once

#include "CoreMinimal.h"

struct FSrdSpell;

/** How the combat engine handles a cantrip or level 1-4 spell beyond the catalog's one-cast damage/heal/condition path. */
enum class ESpellUtility : uint8
{
    Engine,        // the catalog path resolves it once allowed (lasting zones)
    Reaction,      // cast automatically as a reaction (Shield, Hellish Rebuke, Counterspell)
    Beams,         // separate attacks/darts aimed at their own targets (Eldritch Blast, Scorching Ray, Magic Missile)
    ChoiceAttack,  // spell attack with a chosen damage type (Chromatic Orb, Sorcerous Burst)
    WeaponCantrip, // weapon attack riders (True Strike, Shillelagh)
    Restore,       // Spare the Dying, Revivify, Lesser Restoration, Dispel Magic, Calm Emotions
    Teleport,      // Misty Step, Dimension Door: to Command.Destination
    Push,          // Gust of Wind: line, STR save or pushed 15 ft
    Obscure,       // Fog Cloud, Darkness: heavily obscured zone
    Light,         // Light, Dancing Lights, Continual Flame, Daylight: bright light in darkness
    Silence,       // no verbal spells inside, thunder immunity
    Summon,        // a token that acts at the caster's turn start (Find Steed, Giant Insect, Faithful Hound)
    Zone,          // custom lasting area (Guardian of Faith, Aura of Life, Plant Growth, Conjure Minor Elementals)
    Debuff,        // custom lasting effect on foes (Ensnaring Strike, Ray of Enfeeblement, Confusion)
    Buff,          // custom lasting effect on allies (Guidance, Resistance, Darkvision, See Invisibility, Enhance Ability, Beacon of Hope)
    Exception,     // not modelled in tactical combat; Note says why
};

struct FSpellUtility
{
    const TCHAR* Id = TEXT("");
    ESpellUtility Kind = ESpellUtility::Exception;
    /** Exception: why it is not modelled. Others: the grid approximation, if any. */
    const TCHAR* Note = TEXT("");
};

/** SRD 5.2.1 cantrips and level 1-4 spells outside the catalog damage path: how each resolves, or why it can't. Rules only, no state. */
struct DNDDICEROLL_API FSpellUtilityRules
{
    static int32 Num();
    static const FSpellUtility& At(int32 Index);
    static const FSpellUtility* Find(const FString& Id);
    /** Handled by combat (any kind but Exception). */
    static bool Handled(const FString& Id);
    static bool IsException(const FString& Id);
    /** Beams/rays/darts at this cast: Eldritch Blast 1/2/3/4 (levels 1/5/11/17), Scorching Ray 3 + 1 per slot above 2, Magic Missile 3 + 1 per slot above 1. 1 otherwise. */
    static int32 Beams(const FSrdSpell& Spell, int32 CharacterLevel, int32 Slot);
    /** VFX size class by spell level: 0 cantrip (smallest), 1 levels 1-2, 2 level 3, 3 levels 4-6, 4 levels 7-9. */
    static int32 SizeClass(int32 SpellLevel);
    /** Cantrip tier dice (1/2/3/4 at character levels 1/5/11/17). */
    static int32 CantripDice(int32 CharacterLevel);
};
