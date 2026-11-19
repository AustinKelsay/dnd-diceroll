#pragma once

#include "CoreMinimal.h"
#include "Presentation/ExpeditionCue.h"

/** The four volume buses (#246): Master scales the other three, which are the sound classes the cues, beds and music play through. */
enum class EExpeditionBus : uint8 { Master, Effects, Ambience, Music };
constexpr int32 ExpeditionBusCount = 4;

/**
 * The player's volume settings (#246): one slider per bus, 0 to 1 in 5 % steps. Kept in a small user settings file of its own,
 * never in the run checkpoint, so changing a volume writes no run byte. Presentation only (ADR-0001).
 */
struct DNDDICEROLL_API FExpeditionAudioSettings
{
    static constexpr float Step = 0.05f;
    /** The measured mix (#242, #244, #245) is the design: every slider starts at full. */
    static constexpr float Default = 1.0f;
    float Volumes[ExpeditionBusCount] = {Default, Default, Default, Default};

    float Get(EExpeditionBus Bus) const { return Volumes[static_cast<int32>(Bus)]; }
    /** Sets a slider, clamped to [0, 1] and snapped to the 5 % step; returns whether it changed. */
    bool Set(EExpeditionBus Bus, float Value);
    /** Moves a slider by whole steps (the arrow keys). */
    bool Nudge(EExpeditionBus Bus, int32 Steps) { return Set(Bus, Get(Bus) + Steps * Step); }
    bool operator==(const FExpeditionAudioSettings& Other) const;
    /** The slider as the panel prints it ("85%"). */
    static FString Percent(float Value);
    static FString BusName(EExpeditionBus Bus);

    /** The settings document: {"version": 1, "master": 1.0, "effects": 1.0, "ambience": 1.0, "music": 1.0}. */
    FString ToJson() const;
    /** Reads a document; a missing or invalid value keeps its default, and an unreadable document gives false and the defaults. */
    static bool FromJson(const FString& Text, FExpeditionAudioSettings& Out);
    /** The file, by default <Saved>/Settings/expedition-audio.json; -ExpeditionSettingsRoot=<dir> selects another folder (tests, isolated runs). */
    static FString DefaultPath(const TCHAR* CommandLine);
    /** Loads the file; a missing file is the defaults and true, an unreadable one the defaults and false (it is left in place). */
    static bool Load(const FString& Path, FExpeditionAudioSettings& Out);
    /** Writes the file (creating its folder). Returns whether it was written. */
    static bool Save(const FString& Path, const FExpeditionAudioSettings& Settings);
};

namespace ExpeditionMix
{
/** The bus classes: SC_Master is the parent of the other three (Content/Python/setup_expedition_mix.py). */
constexpr const TCHAR* MasterClassPath = TEXT("/Game/DnDDiceRoll/Audio/SC_Master.SC_Master");
constexpr const TCHAR* EffectsClassPath = TEXT("/Game/DnDDiceRoll/Audio/SC_Effects.SC_Effects");
/**
 * A slider's amplitude: the square of its value, so the travel is roughly even in loudness (half way is about -12 dB, 0 is silent,
 * 1 is unity and plays the measured mix as normalised).
 */
DNDDICEROLL_API float Amplitude(float Slider);
/** The amplitude a bus's sounds play at: Master times the bus (Master alone for Master). */
DNDDICEROLL_API float BusGain(const FExpeditionAudioSettings& Settings, EExpeditionBus Bus);
/** The combat cues that duck the ambience and music: blows, misses, shots, spells, healing, a fall and a death. */
DNDDICEROLL_API bool Ducks(EExpeditionCue Cue);
/** How far a combat cue ducks each bed (amplitude multipliers at full duck): ambience about -9 dB, music about -6 dB. */
constexpr float AmbienceDuck = 0.35f;
constexpr float MusicDuck = 0.5f;
}

/**
 * A short duck (#246): Trigger pulls the gain down over Attack, holds it while cues keep coming, then releases it over Release.
 * Gain() is 1 at rest and Floor at full duck. Plain state, ticked by the view; tests drive it directly.
 */
struct DNDDICEROLL_API FAudioDucker
{
    static constexpr float AttackSeconds = 0.05f;
    static constexpr float HoldSeconds = 0.35f;
    static constexpr float ReleaseSeconds = 0.6f;
    explicit FAudioDucker(float InFloor = 0.5f) : Floor(InFloor) {}
    void Trigger() { Held = HoldSeconds; bAttacking = true; }
    void Tick(float DeltaSeconds);
    /** 0 at rest, 1 at full duck. */
    float Amount() const { return Depth; }
    float Gain() const { return FMath::Lerp(1.0f, Floor, Depth); }
    float Floor = 0.5f;

private:
    float Depth = 0.0f;
    float Held = 0.0f;
    bool bAttacking = false;
};
