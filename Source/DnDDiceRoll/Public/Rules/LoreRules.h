#pragma once

#include "CoreMinimal.h"
#include "Rules/CheckRules.h"

enum class ESkill : uint8;

/** SRD 5.2.1 knowledge and Wisdom checks used outside combat. DCs follow the SRD difficulty ladder (easy 10, medium 15, hard 20). */
struct DNDDICEROLL_API FLoreRules
{
    /** The recall skill for a creature: Religion for undead, Arcana for elementals, Nature for beasts, History for humanoids. */
    static ESkill RecallSkill(const FString& Profile);
    static int32 RecallDc(const FString& Profile);
    /** Facts revealed (resistances, then abilities, then weaknesses): 0 on a miss, 1 on a success, +1 per 5 over, max 3. */
    static int32 FactsRevealed(int32 Total, int32 Dc);
    /** Identify: Arcana DC 15 for magic items and runes, History DC 10 for old mundane items. */
    static ESkill IdentifySkill(bool bMagical);
    static int32 IdentifyDc(bool bMagical);
    /** Medicine at camp (DC 10): success treats an injury for 1 extra hit point per character level; failure treats nothing. */
    static int32 TreatmentHitPoints(int32 Total, int32 PatientLevel);
    /** Insight reads an NPC when the total meets 10 + the NPC's Deception modifier (its passive Deception). */
    static bool ReadsIntent(int32 InsightTotal, int32 NpcDeceptionModifier);
};
