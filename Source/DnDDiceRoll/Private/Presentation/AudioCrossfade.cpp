#include "Presentation/AudioCrossfade.h"

bool FAudioCrossfade::Select(uint8 Key, float FadeSeconds)
{
    const float Seconds = FMath::Max(FadeSeconds, 0.01f);
    if (Key == CurrentKey)
    {
        // Already heading there. A layer that is current never needs restarting; at most it is still fading in.
        return false;
    }
    CurrentKey = Key;
    for (FAudioLayer& Layer : Active)
    {
        Layer.Target = Layer.Key == Key ? 1.0f : 0.0f;
        Layer.FadeSeconds = Seconds;
    }
    if (Key != 0 && !Find(Key))
    {
        // Room for the incoming layer: drop the quietest outgoing ones first (a bed only fades out once a newer one replaced it).
        while (Active.Num() >= MaxLayers)
        {
            int32 Quietest = 0;
            for (int32 Index = 1; Index < Active.Num(); ++Index) { if (Active[Index].Level < Active[Quietest].Level) { Quietest = Index; } }
            Remember(Active[Quietest]);
            Active.RemoveAt(Quietest);
        }
        FAudioLayer& Layer = Active.AddDefaulted_GetRef();
        Layer.Key = Key;
        Layer.Id = NextId++;
        Layer.Level = 0.0f;
        Layer.Target = 1.0f;
        Layer.FadeSeconds = Seconds;
        Layer.Offset = Position(Key);
    }
    return true;
}

void FAudioCrossfade::Tick(float DeltaSeconds)
{
    const float Delta = FMath::Max(DeltaSeconds, 0.0f);
    for (int32 Index = Active.Num() - 1; Index >= 0; --Index)
    {
        FAudioLayer& Layer = Active[Index];
        Layer.Age += Delta;
        const float Step = Delta / FMath::Max(Layer.FadeSeconds, 0.01f);
        Layer.Level = Layer.Target > Layer.Level ? FMath::Min(Layer.Level + Step, Layer.Target) : FMath::Max(Layer.Level - Step, Layer.Target);
        if (Layer.IsFadingOut() && Layer.Level <= 0.0f) { Remember(Layer); Active.RemoveAt(Index); }
    }
}

void FAudioCrossfade::Reset()
{
    for (const FAudioLayer& Layer : Active) { Remember(Layer); }
    Active.Reset();
    CurrentKey = 0;
}
