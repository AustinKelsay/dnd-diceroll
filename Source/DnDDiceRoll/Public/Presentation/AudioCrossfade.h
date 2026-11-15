#pragma once

#include "CoreMinimal.h"

/** One looping layer of a crossfade: which bed or track (Key), and how far it has faded in. */
struct FAudioLayer
{
    /** The bed or track (an enum value; 0 is silence and never has a layer). */
    uint8 Key = 0;
    /** Unique per layer for the director's lifetime, so a player can keep one voice per layer. */
    uint32 Id = 0;
    /** Fade progress, 0 silent to 1 full. */
    float Level = 0.0f;
    /** Where Level is heading: 1 while this is the current layer, 0 once another took over. */
    float Target = 1.0f;
    /** Seconds a whole fade (0 to 1 or back) takes at the current rate. */
    float FadeSeconds = 1.0f;
    /** Equal-power gain for the level: the outgoing and incoming layers keep a steady sum of power through the overlap. */
    float Amplitude() const { return FMath::Sin(FMath::Clamp(Level, 0.0f, 1.0f) * HALF_PI); }
    bool IsFadingOut() const { return Target <= 0.0f; }
};

/**
 * The crossfade director for a looping bus (ambience #244, music #245): which layer is current and how loud every layer is,
 * as plain state with no audio objects, so tests can read it with no audio device. The view keeps one voice per layer
 * and sets its volume to the layer's Amplitude every frame.
 *
 * Select starts the new layer from silence and fades every other layer out over the same time, so there is never a gap.
 * At most MaxLayers are alive: selecting while one layer is fading in and another out drops the quieter outgoing one at
 * once, so beds never stack. Selecting the current key changes nothing (a Resume into the same bed does not restart it);
 * selecting a key that is still fading out turns it back up from where it is. Key 0 fades everything out.
 * Presentation only (ADR-0001): no RNG, rules or save state.
 */
class DNDDICEROLL_API FAudioCrossfade
{
public:
    static constexpr int32 MaxLayers = 2;

    /** Makes Key the current layer, fading in (and every other layer out) over FadeSeconds. Returns whether anything changed. */
    bool Select(uint8 Key, float FadeSeconds);
    /** Advances every fade; layers that reach silence while fading out are removed. */
    void Tick(float DeltaSeconds);
    /** Removes every layer at once, with no fade. */
    void Reset();
    /** The key the director is heading to (0: silence). */
    uint8 Current() const { return CurrentKey; }
    const TArray<FAudioLayer>& Layers() const { return Active; }
    const FAudioLayer* Find(uint8 Key) const { return Active.FindByPredicate([Key](const FAudioLayer& Layer) { return Layer.Key == Key; }); }

private:
    TArray<FAudioLayer> Active;
    uint8 CurrentKey = 0;
    uint32 NextId = 1;
};
