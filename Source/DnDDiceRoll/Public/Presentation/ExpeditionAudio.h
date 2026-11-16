#pragma once

#include "CoreMinimal.h"
#include "Presentation/ExpeditionPlayback.h"

DNDDICEROLL_API DECLARE_LOG_CATEGORY_EXTERN(LogExpeditionAudio, Log, All);

/**
 * Expedition sound effects. Recorded cues (#242) come from the cue data asset through FExpeditionCueBank; this namespace keeps the
 * synthesized fallback (mono 16-bit PCM, no imported audio) and the presentation-only variation choice both paths share.
 */
namespace ExpeditionAudio
{
constexpr int32 SampleRate = 48000;
/** The variations the synthesis fallback rotates through. */
constexpr int32 SynthVariations = 4;
/** The recorded cue set the battle view loads. */
constexpr const TCHAR* CueSoundsPath = TEXT("/Game/DnDDiceRoll/Audio/DA_ExpeditionCueSounds.DA_ExpeditionCueSounds");
/** Variation reshuffles small details (click spacing, pitch) so repeats don't sound identical. */
DNDDICEROLL_API TArray<int16> Synthesize(EExpeditionCue Cue, int32 Variation);
DNDDICEROLL_API float Volume(EExpeditionCue Cue);
/**
 * Which of Count variations the Serial-th play of a cue uses: a hash of the cue and the serial's block, so each run of Count consecutive plays
 * uses every variation once, in an order that changes from block to block. Pure and deterministic; it reads no RNG (ADR-0001).
 */
DNDDICEROLL_API int32 PickVariation(EExpeditionCue Cue, uint32 Serial, int32 Count);
/** False when the command line carries -ExpeditionSynthAudio, which forces the synthesized cues for comparison. */
DNDDICEROLL_API bool UseRecordedCues(const TCHAR* CommandLine);
/** The cue's enum name ("DiceRoll"), for logs and asset names. */
DNDDICEROLL_API FString CueName(EExpeditionCue Cue);
}
