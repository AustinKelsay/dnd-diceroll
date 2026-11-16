#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Presentation/ExpeditionAudio.h"
#include "Presentation/ExpeditionCueSounds.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundWave.h"
#include "UObject/StrongObjectPtr.h"

// Recorded cues (#242): every cue resolves to a recording, a missing one falls back to synthesis on its own, and the variation a play gets
// is a deterministic presentation-only hash that reads no RNG.

namespace
{
/** The cues that repeat within a turn, which the ticket asks to carry two to four variations. */
const EExpeditionCue RepeatingCues[] = {EExpeditionCue::Hit, EExpeditionCue::Step, EExpeditionCue::Swing, EExpeditionCue::DiceRoll,
    EExpeditionCue::DiceLand, EExpeditionCue::Click};

USoundWave* TransientWave(float Seconds)
{
    USoundWave* Wave = NewObject<USoundWave>(GetTransientPackage(), NAME_None, RF_Transient);
    Wave->Duration = Seconds;
    Wave->NumChannels = 1;
    return Wave;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FExpeditionCueEnumTest, "Project.Expedition.Audio.CueEnum",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FExpeditionCueEnumTest::RunTest(const FString& Parameters)
{
    const UEnum* Enum = StaticEnum<EExpeditionCue>();
    // NumEnums counts the generated _MAX entry.
    TestEqual(TEXT("ExpeditionCueCount matches the reflected enum"), ExpeditionCueCount, static_cast<int32>(Enum->NumEnums() - 1));
    TestEqual(TEXT("Cue names come from the enum"), ExpeditionAudio::CueName(EExpeditionCue::DiceRoll), FString(TEXT("DiceRoll")));
    TestEqual(TEXT("The last cue is named"), ExpeditionAudio::CueName(EExpeditionCue::RunEnd), FString(TEXT("RunEnd")));
    TestEqual(TEXT("The first cue added in #243 follows Route, so saved cue sets keep their keys"), static_cast<int32>(EExpeditionCue::SpendKit), static_cast<int32>(EExpeditionCue::Route) + 1);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FExpeditionCueVariationTest, "Project.Expedition.Audio.VariationChoice",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FExpeditionCueVariationTest::RunTest(const FString& Parameters)
{
    using ExpeditionAudio::PickVariation;
    for (int32 Index = 0; Index < ExpeditionCueCount; ++Index)
    {
        const EExpeditionCue Cue = static_cast<EExpeditionCue>(Index);
        TestEqual(TEXT("One variation is always the first"), PickVariation(Cue, 17, 1), 0);
        TestEqual(TEXT("No variations still answers zero"), PickVariation(Cue, 17, 0), 0);
        for (int32 Count = 2; Count <= ExpeditionAudio::SynthVariations; ++Count)
        {
            for (uint32 Block = 0; Block < 64; ++Block)
            {
                TSet<int32> Seen;
                for (int32 Step = 0; Step < Count; ++Step)
                {
                    const uint32 Serial = Block * Count + Step;
                    const int32 Variation = PickVariation(Cue, Serial, Count);
                    TestTrue(TEXT("The variation is in range"), Variation >= 0 && Variation < Count);
                    TestEqual(TEXT("The same play always picks the same variation"), PickVariation(Cue, Serial, Count), Variation);
                    Seen.Add(Variation);
                }
                TestEqual(FString::Printf(TEXT("%s: every block of %d plays uses every variation once"), *ExpeditionAudio::CueName(Cue), Count), Seen.Num(), Count);
            }
        }
    }
    // The order changes between blocks (a hash, not a fixed rotation): over 64 blocks a four-take cue starts on more than one take.
    TSet<int32> Starts;
    for (uint32 Block = 0; Block < 64; ++Block) { Starts.Add(PickVariation(EExpeditionCue::Hit, Block * 4, 4)); }
    TestTrue(TEXT("Blocks start on different variations"), Starts.Num() > 1);

    // ADR-0001: picking reads no RNG. The global stream draws the same value with or without a thousand picks in between.
    FMath::RandInit(4242);
    const int32 Expected = FMath::Rand();
    FMath::RandInit(4242);
    for (uint32 Serial = 0; Serial < 1000; ++Serial) { PickVariation(static_cast<EExpeditionCue>(Serial % ExpeditionCueCount), Serial, 4); }
    TestEqual(TEXT("Variation choice leaves the global random stream alone"), FMath::Rand(), Expected);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FExpeditionSynthFlagTest, "Project.Expedition.Audio.SynthFlag",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FExpeditionSynthFlagTest::RunTest(const FString& Parameters)
{
    TestTrue(TEXT("Recorded cues by default"), ExpeditionAudio::UseRecordedCues(TEXT("DnDDiceRoll -windowed")));
    TestTrue(TEXT("No command line means recorded cues"), ExpeditionAudio::UseRecordedCues(nullptr));
    TestFalse(TEXT("-ExpeditionSynthAudio forces synthesis"), ExpeditionAudio::UseRecordedCues(TEXT("DnDDiceRoll -ExpeditionSynthAudio -windowed")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FExpeditionCueFallbackTest, "Project.Expedition.Audio.Fallback",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FExpeditionCueFallbackTest::RunTest(const FString& Parameters)
{
    TStrongObjectPtr<USoundWave> First(TransientWave(0.2f));
    TStrongObjectPtr<USoundWave> Second(TransientWave(0.3f));
    TStrongObjectPtr<UExpeditionCueSounds> Sounds(NewObject<UExpeditionCueSounds>(GetTransientPackage(), NAME_None, RF_Transient));
    FExpeditionCueSound& Click = Sounds->Cues.Add(EExpeditionCue::Click);
    Click.Variations = {TSoftObjectPtr<USoundWave>(First.Get()), TSoftObjectPtr<USoundWave>(), TSoftObjectPtr<USoundWave>(Second.Get())};
    Click.Volume = 0.8f;
    // A recording that is not there, and one cue whose only take is missing.
    FExpeditionCueSound& Hit = Sounds->Cues.Add(EExpeditionCue::Hit);
    Hit.Variations = {TSoftObjectPtr<USoundWave>(FSoftObjectPath(TEXT("/Game/DnDDiceRoll/Audio/Missing/SW_NotThere.SW_NotThere")))};
    FExpeditionCueSound& Step = Sounds->Cues.Add(EExpeditionCue::Step);
    Step.Variations = {TSoftObjectPtr<USoundWave>(First.Get()), TSoftObjectPtr<USoundWave>(FSoftObjectPath(TEXT("/Game/DnDDiceRoll/Audio/Missing/SW_Gone.SW_Gone")))};
    Step.Volume = 9.0f;

    FExpeditionCueBank Bank;
    Bank.Load(Sounds.Get());
    TestEqual(TEXT("Unset variations are dropped"), Bank.Count(EExpeditionCue::Click), 2);
    TestEqual(TEXT("A missing take is dropped and the loaded one kept"), Bank.Count(EExpeditionCue::Step), 1);
    TestEqual(TEXT("A cue whose recordings are all missing has none"), Bank.Count(EExpeditionCue::Hit), 0);
    TestEqual(TEXT("A cue with no entry has none"), Bank.Count(EExpeditionCue::Victory), 0);
    TestTrue(TEXT("Synthesis covers the missing cue"), Bank.SynthesizedCues().Contains(EExpeditionCue::Hit));
    TestFalse(TEXT("Synthesis does not cover a recorded cue"), Bank.SynthesizedCues().Contains(EExpeditionCue::Click));

    for (uint32 Serial = 0; Serial < 8; ++Serial)
    {
        const FExpeditionCuePlay Recorded = Bank.Choose(EExpeditionCue::Click, Serial);
        TestTrue(TEXT("A recorded cue plays one of its takes"), Recorded.Wave == First.Get() || Recorded.Wave == Second.Get());
        TestEqual(TEXT("It plays at the asset's volume"), Recorded.Volume, 0.8f);
        TestEqual(TEXT("The take is the hashed variation"), Recorded.Variation, ExpeditionAudio::PickVariation(EExpeditionCue::Click, Serial, 2));

        const FExpeditionCuePlay Fallback = Bank.Choose(EExpeditionCue::Hit, Serial);
        TestNull(TEXT("The missing cue falls back to synthesis"), Fallback.Wave);
        TestEqual(TEXT("At the synthesis volume"), Fallback.Volume, ExpeditionAudio::Volume(EExpeditionCue::Hit));
        TestEqual(TEXT("With the hashed synthesis variation"), Fallback.Variation,
            ExpeditionAudio::PickVariation(EExpeditionCue::Hit, Serial, ExpeditionAudio::SynthVariations));
        TestFalse(TEXT("Which still makes a sound"), ExpeditionAudio::Synthesize(EExpeditionCue::Hit, Fallback.Variation).IsEmpty());
    }
    TestEqual(TEXT("A volume out of range is clamped"), Bank.Choose(EExpeditionCue::Step, 0).Volume, FExpeditionCueSound::MaxVolume);

    // No asset at all (or -ExpeditionSynthAudio, which never loads it): every cue synthesizes.
    Bank.Load(nullptr);
    TestEqual(TEXT("An absent asset leaves every cue to synthesis"), Bank.SynthesizedCues().Num(), ExpeditionCueCount);
    TestNull(TEXT("A formerly recorded cue now synthesizes"), Bank.Choose(EExpeditionCue::Click, 0).Wave);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FExpeditionShippedCuesTest, "Project.Expedition.Audio.ShippedCues",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FExpeditionShippedCuesTest::RunTest(const FString& Parameters)
{
    const UExpeditionCueSounds* Sounds = LoadObject<UExpeditionCueSounds>(nullptr, ExpeditionAudio::CueSoundsPath, nullptr, LOAD_Quiet | LOAD_NoWarn);
    if (!TestNotNull(TEXT("The recorded cue set is imported"), Sounds)) { return false; }
    FExpeditionCueBank Bank;
    Bank.Load(Sounds);
    TestEqual(TEXT("Every cue resolves to a recording (none synthesizes)"), Bank.SynthesizedCues().Num(), 0);
    if (!TestNotNull(TEXT("The Effects sound class loads"), Bank.Class())) { return false; }
    TestEqual(TEXT("The class is the Effects stub"), Bank.Class()->GetName(), FString(TEXT("SC_Effects")));
    for (int32 Index = 0; Index < ExpeditionCueCount; ++Index)
    {
        const EExpeditionCue Cue = static_cast<EExpeditionCue>(Index);
        const FString Name = ExpeditionAudio::CueName(Cue);
        const FExpeditionCueSound* Entry = Sounds->Cues.Find(Cue);
        if (!TestNotNull(*FString::Printf(TEXT("%s has an entry"), *Name), Entry)) { continue; }
        TestEqual(*FString::Printf(TEXT("%s: every variation in the asset loads"), *Name), Bank.Count(Cue), Entry->Variations.Num());
        TestTrue(*FString::Printf(TEXT("%s: one to four variations"), *Name), Bank.Count(Cue) >= 1 && Bank.Count(Cue) <= 4);
        TestTrue(*FString::Printf(TEXT("%s: a sensible volume"), *Name), Entry->Volume > 0.0f && Entry->Volume <= FExpeditionCueSound::MaxVolume);
        for (const TSoftObjectPtr<USoundWave>& Variation : Entry->Variations)
        {
            const USoundWave* Wave = Variation.LoadSynchronous();
            if (!TestNotNull(*FString::Printf(TEXT("%s: variation loads"), *Name), Wave)) { continue; }
            TestTrue(*FString::Printf(TEXT("%s: %s lives under the audio folder"), *Name, *Wave->GetPathName()), Wave->GetPathName().StartsWith(TEXT("/Game/DnDDiceRoll/Audio/")));
            TestEqual(*FString::Printf(TEXT("%s: %s is mono"), *Name, *Wave->GetName()), Wave->NumChannels, 1);
            TestTrue(*FString::Printf(TEXT("%s: %s is a cue, not a loop"), *Name, *Wave->GetName()), Wave->GetDuration() > 0.1f && Wave->GetDuration() < 4.0f && !Wave->bLooping);
            TestTrue(*FString::Printf(TEXT("%s: %s plays through Effects"), *Name, *Wave->GetName()), Wave->SoundClassObject == Bank.Class());
        }
    }
    for (EExpeditionCue Cue : RepeatingCues)
    {
        TestTrue(*FString::Printf(TEXT("%s repeats, so it carries two to four variations"), *ExpeditionAudio::CueName(Cue)), Bank.Count(Cue) >= 2);
    }
    return true;
}
#endif
