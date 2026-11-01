#pragma once

#include "CoreMinimal.h"
#include "Presentation/ExpeditionPlayback.h"

/** Synthesized sound effects: no imported audio. Mono 16-bit PCM. */
namespace ExpeditionAudio
{
constexpr int32 SampleRate = 48000;
/** Variation reshuffles small details (click spacing, pitch) so repeats don't sound identical. */
DNDDICEROLL_API TArray<int16> Synthesize(EExpeditionCue Cue, int32 Variation);
DNDDICEROLL_API float Volume(EExpeditionCue Cue);
}
