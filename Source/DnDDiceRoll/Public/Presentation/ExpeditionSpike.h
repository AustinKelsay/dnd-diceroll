#pragma once

#include "CoreMinimal.h"

/** #140 look-dev spike switches. Throwaway: every field is read from the command line, defaults are today's game. */
struct DNDDICEROLL_API FExpeditionSpike
{
    enum class ECam : uint8 { Today, B, Zoom };
    enum class EFilter : uint8 { Nearest, Trilinear, Sharp };
    enum class ELight : uint8 { Today, Key };

    bool bWorld = false;
    bool bGrid = false;
    bool bLumen = false;
    float Cell = 110.0f;
    ECam Cam = ECam::Today;
    EFilter Filter = EFilter::Nearest;
    ELight Light = ELight::Today;
    float Fill = 0.12f, Rim = 0.35f, GlowK = 2.5f;

    static const FExpeditionSpike& Get();
    /** Distances in poses and clamps are authored for 110 cm cells and FOV 50; this maps them. */
    float DistanceScale() const;
    float Fov() const { return Cam == ECam::Today ? 50.0f : 40.0f; }
    FString Describe() const;
};
