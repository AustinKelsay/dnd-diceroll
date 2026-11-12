#pragma once

#include "CoreMinimal.h"

/**
 * How bright a hit, a spell glow and a spell bolt may get on screen (#189). Presentation only (ADR-0001): nothing here feeds rules or saves.
 *
 * The figure materials multiply their "Color" parameter into the base colour and into the emissive fill, and the post-process bloom
 * then spreads whatever is over 1. An HDR tint (the flash used to aim at 3.0 and the bolts' glow at 6.0) therefore washed the target
 * out, and a spell bolt's point light (180 cd, half a metre above the board) clipped the floor and the footprint ring around it.
 * The flash is now a bounded, warm tint of the figure only, and the bolt's light and glow are sized so the floor stays readable.
 * The measured limit is tools/impact_flash.py: a target footprint whose peak stays below clipping with under 2% pure white.
 */
namespace ExpeditionFlash
{
/** The brightest a linear colour channel may become by flashing or glowing (a part that is already brighter stays as it was). */
constexpr float Ceiling = 1.5f;
/** How far a full hit flash pulls a part toward the hit colour. */
constexpr float HitWeight = 0.6f;
/** How far a full glow (a caster, an ability) pulls a part toward the glow colour. */
constexpr float GlowWeight = 0.35f;
/**
 * The point light that rides a lone spell bolt, in candelas (each bolt used to carry 180, so Magic Missile's three carried 540). In flight it lights
 * the floor along the path; BoltLandingCandelas is what is left as the bolt reaches its target, so the figure it strikes (whose cowl and rim are the
 * brightest pixels in a hit) never clips: see BoltLight.
 */
constexpr float BoltLightCandelas = 50.0f;
constexpr float BoltLandingCandelas = 3.5f;
/** The bolt's path fraction over which the light fades from its flight value to its landing value. */
constexpr float BoltFadeStart = 0.55f;
constexpr float BoltFadeEnd = 0.9f;
/**
 * The bolt's path fraction over which its light ramps in from nothing (#150 batch 3). A bolt starts on the caster's own cell, and the full 50 cd there
 * (half a metre above the board) whitened the caster and the floor around it at release (peak 255, up to 1.76% pure white in the Vault, 33% in the worst
 * shots, for about six frames). The light now rises over the first half of the flight, as the square of a smoothstep (about 1% of its flight value a tenth of the
 * way along, a quarter at a quarter of the way), so it is at its full value only once the bolt is well away from the caster and before the landing fade
 * begins at BoltFadeStart. The glow of the bolt itself is unchanged.
 */
constexpr float BoltRampEnd = 0.5f;
/** The multiple of a bolt's colour (it was 6) and of a sparkle's (it was 5). The effect material clamps it into a base colour, so above 1 it only desaturates the bolt toward white; 1 keeps the full saturated colour. */
constexpr float BoltEmissive = 1.0f;
constexpr float SparkEmissive = 1.0f;
/** How much larger than authored a spell bolt is drawn (a lone bolt was 20 cm across, a speck at default zoom). */
constexpr float BoltScale = 1.6f;

/**
 * One bolt's light when Bolts of them are in flight, Alpha (0 to 1) of the way along their path. Bolts light one another's meshes from a few
 * centimetres, so each light is the budget over Bolts squared: a Magic Missile's three together carry a third of one spell bolt's light.
 */
inline float BoltLight(int32 Bolts, float Alpha)
{
    const float Count = static_cast<float>(FMath::Max(1, Bolts));
    const float Fade = FMath::SmoothStep(BoltFadeStart, BoltFadeEnd, FMath::Clamp(Alpha, 0.0f, 1.0f));
    const float Ramp = FMath::Square(FMath::SmoothStep(0.0f, BoltRampEnd, FMath::Clamp(Alpha, 0.0f, 1.0f)));
    return Ramp * FMath::Lerp(BoltLightCandelas, BoltLandingCandelas, Fade) / (Count * Count);
}

/** The hot rim a hit lights on a figure's edge: the master material's "RimColor" (a cool blue) turns toward this warm white, never past Ceiling once multiplied by "Rim". */
inline FLinearColor RimHitColor() { return FLinearColor(1.0f, 0.85f, 0.7f); }

inline FLinearColor HitColor() { return FLinearColor(1.0f, 0.85f, 0.75f) * Ceiling; }
inline FLinearColor GlowColor() { return FLinearColor(1.0f, 0.95f, 0.75f) * (Ceiling * 0.9f); }

/**
 * The colour a part shows with Glow and Flash (each 0 to 1): the base pulled toward the glow colour, then the hit colour. Both targets are at
 * most Ceiling in every channel, so no channel of the result exceeds the larger of the base and Ceiling; with neither, the base is returned.
 */
inline FLinearColor Tint(const FLinearColor& Base, float Glow, float Flash)
{
    FLinearColor Color = FMath::Lerp(Base, GlowColor(), FMath::Clamp(Glow, 0.0f, 1.0f) * GlowWeight);
    Color = FMath::Lerp(Color, HitColor(), FMath::Clamp(Flash, 0.0f, 1.0f) * HitWeight);
    Color.A = Base.A;
    return Color;
}

/**
 * The rim colour a figure shows with a Flash (0 to 1). The figure materials add fresnel * RimColor * Rim, an emissive edge that does not depend on
 * the (often dark) albedo, so it is what makes a hit read on a dark figure. The warm target is scaled so Rim * RimColor stays within Ceiling in
 * every channel unless the base look was already brighter.
 */
inline FLinearColor RimColor(const FLinearColor& BaseColor, float BaseRim, float Flash)
{
    const float Fit = BaseRim > Ceiling ? Ceiling / BaseRim : 1.0f;
    FLinearColor Color = FMath::Lerp(BaseColor, RimHitColor() * Fit, FMath::Clamp(Flash, 0.0f, 1.0f));
    Color.A = BaseColor.A;
    return Color;
}
}
