#pragma once

#include "CoreMinimal.h"
#include "Rules/SpellCatalogRules.h"

/** When a lasting zone hits a creature (bit flags). */
namespace SpellZoneTrigger { enum : uint8 { OnCast = 1, Enter = 2, EndTurn = 4, StartTurn = 8, PerStep = 16 }; }

/** A damaging area that persists after the casting turn (SRD 5.2.1 wording, grid-approximated). */
struct DNDDICEROLL_API FSpellZone
{
    const TCHAR* Id;
    uint8 Triggers;
    /** Radius in feet (Chebyshev cells on the grid); square areas use half their side. */
    int32 RadiusFeet;
    /** Emanation centred on and moving with the caster. */
    bool bFollowsCaster;
    /** The caster picks who is unaffected: allies are spared. */
    bool bSparesAllies;
    /** The area is difficult terrain. */
    bool bDifficult;
    /** Wall zones (Wall of Fire): total hex-path length in feet; 0 = radius zone. */
    int32 PathFeet = 0;
};

/** SRD 5.2.1 spell timing as data and arithmetic. Combat applies it; nothing here is saved. */
struct DNDDICEROLL_API FSpellTacticsRules
{
    /** Duration text in rounds: Instantaneous 0, "1 round" 1, N minutes 10N, N hours 600N, N days 14400N, "Until dispelled"/"triggered" -1. Unknown text is 0. */
    static int32 DurationRounds(const TCHAR* Duration);
    static int32 DurationRounds(const FSrdSpell& Spell) { return DurationRounds(Spell.Duration); }
    /** Lasting damage zone, or null. */
    static const FSpellZone* Zone(const FSrdSpell& Spell);
    static const FSpellZone* Zone(const FString& Id);
    /** Wall path: hex lines from Aim through each node in order, shared hexes once, cut at MaxFeet (5 ft per hex). Deterministic. */
    static TArray<FIntPoint> WallPath(FIntPoint Aim, const TArray<FIntPoint>& Nodes, int32 MaxFeet);
    static int32 ZoneCount();
    static const FSpellZone& ZoneAt(int32 Index);
    /** Held-condition spells whose target repeats the save at the end of each of its turns, ending the spell on itself on a success. */
    static bool RepeatsSaveAtEndOfTurn(const FSrdSpell& Spell);
    /** SRD 5.2.1 one-slot-per-turn rule: true when casting would spend a slot (leveled spell). */
    static bool SpendsSlot(const FSrdSpell& Spell) { return Spell.Level > 0; }
};