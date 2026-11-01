#include "Presentation/ExpeditionAudio.h"

namespace
{
/** Accumulates layered voices into a float buffer, then converts once to 16-bit. */
struct FMix
{
    TArray<float> Samples;
    FRandomStream Noise;
    explicit FMix(float Seconds, int32 Seed) : Noise(Seed) { Samples.SetNumZeroed(FMath::CeilToInt(Seconds * ExpeditionAudio::SampleRate)); }

    static float Envelope(float T, float Attack, float Length)
    {
        if (T < 0.0f || T > Length) { return 0.0f; }
        if (T < Attack) { return T / FMath::Max(Attack, 1e-4f); }
        const float Decay = (T - Attack) / FMath::Max(Length - Attack, 1e-4f);
        return FMath::Pow(1.0f - Decay, 2.2f);
    }
    /** A sine (or softened square) whose pitch glides from StartHz to EndHz. */
    void Tone(float Start, float Length, float StartHz, float EndHz, float Gain, float Attack = 0.005f, float Square = 0.0f)
    {
        double Phase = 0.0;
        const int32 First = FMath::Max(0, FMath::FloorToInt(Start * ExpeditionAudio::SampleRate));
        const int32 Count = FMath::CeilToInt(Length * ExpeditionAudio::SampleRate);
        for (int32 Index = 0; Index < Count && First + Index < Samples.Num(); ++Index)
        {
            const float T = Index / static_cast<float>(ExpeditionAudio::SampleRate);
            const float Hz = FMath::Lerp(StartHz, EndHz, T / Length);
            Phase += 2.0 * PI * Hz / ExpeditionAudio::SampleRate;
            const float Sine = FMath::Sin(static_cast<float>(Phase));
            const float Shaped = FMath::Lerp(Sine, FMath::Clamp(Sine * 3.0f, -1.0f, 1.0f), Square);
            Samples[First + Index] += Shaped * Gain * Envelope(T, Attack, Length);
        }
    }
    /** Filtered noise: Brightness 0 is a dull thump, 1 is a hiss. */
    void Burst(float Start, float Length, float Gain, float Brightness, float Attack = 0.002f)
    {
        float Low = 0.0f;
        const float Coefficient = FMath::Lerp(0.04f, 0.9f, FMath::Clamp(Brightness, 0.0f, 1.0f));
        const int32 First = FMath::Max(0, FMath::FloorToInt(Start * ExpeditionAudio::SampleRate));
        const int32 Count = FMath::CeilToInt(Length * ExpeditionAudio::SampleRate);
        for (int32 Index = 0; Index < Count && First + Index < Samples.Num(); ++Index)
        {
            const float T = Index / static_cast<float>(ExpeditionAudio::SampleRate);
            Low += Coefficient * (Noise.FRandRange(-1.0f, 1.0f) - Low);
            Samples[First + Index] += Low * Gain * Envelope(T, Attack, Length);
        }
    }
    /** Noise whose brightness sweeps, for whooshes. */
    void Sweep(float Start, float Length, float Gain, float From, float To)
    {
        const int32 Slices = 8;
        for (int32 Slice = 0; Slice < Slices; ++Slice)
        {
            const float Alpha = Slice / static_cast<float>(Slices - 1);
            const float Shape = FMath::Sin(Alpha * PI);
            Burst(Start + Length * Slice / Slices, Length / Slices * 1.6f, Gain * Shape, FMath::Lerp(From, To, Alpha), 0.01f);
        }
    }
    TArray<int16> Finish() const
    {
        float Peak = 0.0f;
        for (float Sample : Samples) { Peak = FMath::Max(Peak, FMath::Abs(Sample)); }
        const float Scale = Peak > 0.95f ? 0.95f / Peak : 1.0f;
        TArray<int16> Pcm;
        Pcm.SetNumUninitialized(Samples.Num());
        for (int32 Index = 0; Index < Samples.Num(); ++Index)
        {
            // Short fade at the end prevents clicks.
            const float Tail = FMath::Clamp((Samples.Num() - Index) / 240.0f, 0.0f, 1.0f);
            Pcm[Index] = static_cast<int16>(FMath::Clamp(Samples[Index] * Scale * Tail, -1.0f, 1.0f) * 32767.0f);
        }
        return Pcm;
    }
};

float Semitone(float Base, int32 Steps) { return Base * FMath::Pow(2.0f, Steps / 12.0f); }
}

namespace ExpeditionAudio
{
float Volume(EExpeditionCue Cue)
{
    switch (Cue)
    {
    case EExpeditionCue::Step: return 0.25f;
    case EExpeditionCue::Click: return 0.4f;
    case EExpeditionCue::DiceRoll: case EExpeditionCue::DiceLand: return 0.7f;
    case EExpeditionCue::Victory: case EExpeditionCue::Defeat: return 0.6f;
    case EExpeditionCue::PartyTurn: case EExpeditionCue::EnemyTurn: return 0.35f;
    default: return 0.55f;
    }
}

TArray<int16> Synthesize(EExpeditionCue Cue, int32 Variation)
{
    FRandomStream Random(Variation * 7919 + static_cast<int32>(Cue) * 104729);
    const float Pitch = Random.FRandRange(0.94f, 1.06f);
    switch (Cue)
    {
    case EExpeditionCue::DiceRoll:
    {
        // Bone dice rattling across wood: irregular bright clicks that slow down.
        FMix Mix(0.62f, Variation);
        float Time = 0.0f;
        for (int32 Click = 0; Click < 11; ++Click)
        {
            Mix.Burst(Time, 0.025f, 0.55f * (1.0f - Click / 14.0f), 0.85f);
            Mix.Tone(Time, 0.03f, Random.FRandRange(1800, 3200) * Pitch, 1500, 0.12f);
            Time += 0.022f + Click * Random.FRandRange(0.004f, 0.009f);
        }
        return Mix.Finish();
    }
    case EExpeditionCue::DiceLand:
    {
        FMix Mix(0.2f, Variation);
        Mix.Burst(0.0f, 0.06f, 0.6f, 0.6f);
        Mix.Tone(0.0f, 0.12f, 900 * Pitch, 700, 0.25f);
        return Mix.Finish();
    }
    case EExpeditionCue::Hit:
    {
        FMix Mix(0.32f, Variation);
        Mix.Burst(0.0f, 0.12f, 0.9f, 0.25f);
        Mix.Tone(0.0f, 0.22f, 160 * Pitch, 60, 0.8f, 0.002f);
        Mix.Burst(0.0f, 0.03f, 0.4f, 0.95f);
        return Mix.Finish();
    }
    case EExpeditionCue::Critical:
    {
        FMix Mix(0.9f, Variation);
        Mix.Burst(0.0f, 0.18f, 1.0f, 0.3f);
        Mix.Tone(0.0f, 0.35f, 120, 45, 0.9f, 0.002f);
        for (int32 Note = 0; Note < 4; ++Note)
        {
            const int32 Steps[] = {0, 4, 7, 12};
            Mix.Tone(0.05f + Note * 0.07f, 0.5f, Semitone(660, Steps[Note]), Semitone(660, Steps[Note]), 0.22f, 0.004f, 0.25f);
        }
        return Mix.Finish();
    }
    case EExpeditionCue::Miss:
    {
        FMix Mix(0.35f, Variation);
        Mix.Sweep(0.0f, 0.3f, 0.45f, 0.2f, 0.75f);
        return Mix.Finish();
    }
    case EExpeditionCue::Swing:
    {
        FMix Mix(0.22f, Variation);
        Mix.Sweep(0.0f, 0.18f, 0.5f, 0.35f, 0.9f);
        return Mix.Finish();
    }
    case EExpeditionCue::Bow:
    {
        FMix Mix(0.3f, Variation);
        Mix.Tone(0.0f, 0.12f, 220 * Pitch, 180, 0.4f, 0.001f, 0.5f);
        Mix.Sweep(0.04f, 0.22f, 0.3f, 0.6f, 0.95f);
        return Mix.Finish();
    }
    case EExpeditionCue::Spell:
    {
        FMix Mix(0.6f, Variation);
        Mix.Tone(0.0f, 0.5f, 440 * Pitch, 880 * Pitch, 0.25f, 0.05f);
        Mix.Tone(0.0f, 0.5f, 445 * Pitch, 890 * Pitch, 0.2f, 0.05f);
        Mix.Sweep(0.0f, 0.45f, 0.18f, 0.5f, 0.9f);
        return Mix.Finish();
    }
    case EExpeditionCue::Heal:
    {
        FMix Mix(0.9f, Variation);
        const int32 Steps[] = {0, 4, 7, 11, 14};
        for (int32 Note = 0; Note < 5; ++Note) { Mix.Tone(Note * 0.06f, 0.6f, Semitone(523, Steps[Note]), Semitone(523, Steps[Note]), 0.2f, 0.01f); }
        return Mix.Finish();
    }
    case EExpeditionCue::Step:
    {
        FMix Mix(0.09f, Variation);
        Mix.Burst(0.0f, 0.07f, 0.5f, Random.FRandRange(0.15f, 0.3f));
        return Mix.Finish();
    }
    case EExpeditionCue::Click:
    {
        FMix Mix(0.08f, Variation);
        Mix.Tone(0.0f, 0.05f, 1400, 1200, 0.35f, 0.001f);
        Mix.Burst(0.0f, 0.012f, 0.25f, 0.9f);
        return Mix.Finish();
    }
    case EExpeditionCue::Downed:
    {
        FMix Mix(0.7f, Variation);
        Mix.Burst(0.0f, 0.2f, 0.8f, 0.15f);
        Mix.Tone(0.05f, 0.55f, 330, 165, 0.3f, 0.01f);
        return Mix.Finish();
    }
    case EExpeditionCue::Death:
    {
        FMix Mix(1.4f, Variation);
        Mix.Burst(0.0f, 0.25f, 0.7f, 0.12f);
        const int32 Steps[] = {0, -3, -7};
        for (int32 Note = 0; Note < 3; ++Note) { Mix.Tone(0.1f + Note * 0.28f, 0.8f, Semitone(294, Steps[Note]), Semitone(294, Steps[Note]) * 0.98f, 0.25f, 0.02f, 0.15f); }
        return Mix.Finish();
    }
    case EExpeditionCue::PartyTurn:
    {
        FMix Mix(0.4f, Variation);
        Mix.Tone(0.0f, 0.25f, 784, 784, 0.25f, 0.005f);
        Mix.Tone(0.08f, 0.3f, 1175, 1175, 0.2f, 0.005f);
        return Mix.Finish();
    }
    case EExpeditionCue::EnemyTurn:
    {
        FMix Mix(0.45f, Variation);
        Mix.Tone(0.0f, 0.35f, 196, 185, 0.35f, 0.01f, 0.35f);
        Mix.Burst(0.0f, 0.1f, 0.3f, 0.1f);
        return Mix.Finish();
    }
    case EExpeditionCue::Victory:
    {
        FMix Mix(2.0f, Variation);
        const int32 Steps[] = {0, 4, 7, 12, 7, 12, 16};
        const float Times[] = {0.0f, 0.14f, 0.28f, 0.42f, 0.7f, 0.84f, 0.98f};
        for (int32 Note = 0; Note < 7; ++Note)
        {
            Mix.Tone(Times[Note], Note == 6 ? 0.95f : 0.3f, Semitone(392, Steps[Note]), Semitone(392, Steps[Note]), 0.22f, 0.006f, 0.3f);
        }
        return Mix.Finish();
    }
    case EExpeditionCue::Defeat:
    {
        FMix Mix(2.2f, Variation);
        const int32 Steps[] = {0, -2, -5, -9};
        for (int32 Note = 0; Note < 4; ++Note) { Mix.Tone(Note * 0.38f, Note == 3 ? 1.2f : 0.5f, Semitone(262, Steps[Note]), Semitone(262, Steps[Note]), 0.25f, 0.02f, 0.2f); }
        Mix.Burst(1.1f, 0.6f, 0.3f, 0.08f, 0.1f);
        return Mix.Finish();
    }
    case EExpeditionCue::Camp:
    {
        FMix Mix(1.6f, Variation);
        for (int32 Crackle = 0; Crackle < 18; ++Crackle) { Mix.Burst(Random.FRandRange(0.0f, 1.4f), 0.02f, Random.FRandRange(0.15f, 0.4f), 0.8f); }
        Mix.Burst(0.0f, 1.5f, 0.12f, 0.1f, 0.3f);
        Mix.Tone(0.2f, 1.2f, 262, 262, 0.12f, 0.3f);
        Mix.Tone(0.2f, 1.2f, 330, 330, 0.1f, 0.3f);
        return Mix.Finish();
    }
    case EExpeditionCue::Loot:
    {
        FMix Mix(0.7f, Variation);
        for (int32 Coin = 0; Coin < 5; ++Coin) { Mix.Tone(Coin * 0.05f, 0.25f, Random.FRandRange(2200, 3400), 2000, 0.15f, 0.001f); }
        Mix.Tone(0.2f, 0.4f, 1047, 1047, 0.2f, 0.005f);
        return Mix.Finish();
    }
    case EExpeditionCue::Route:
    {
        FMix Mix(1.0f, Variation);
        Mix.Tone(0.0f, 0.9f, 196, 196, 0.25f, 0.08f, 0.2f);
        Mix.Tone(0.15f, 0.8f, 294, 294, 0.2f, 0.08f, 0.2f);
        return Mix.Finish();
    }
    default:
        return {};
    }
}
}
