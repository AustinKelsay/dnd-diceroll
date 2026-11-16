#if WITH_DEV_AUTOMATION_TESTS
#include "ExpeditionInputHarness.h"
#include "Presentation/ExpeditionMusic.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundWave.h"
#include "UObject/StrongObjectPtr.h"

// Music (#245): the phase -> track mapping, crossfades with no stacking and no silence over a second, run-end stings, and a mid-fight
// Resume that only fades the fight's track back in (from where it stopped), never restarting it from silence-then-top.

namespace ExpeditionMusicTest
{
using ExpeditionInputHarness::FHarness;

FExpeditionMusicMoment Moment(bool bRun, EExpeditionPhase Phase, int32 Encounter = 1, bool bFinale = false, bool bPresenting = false)
{
    FExpeditionMusicMoment Now;
    Now.bRunInPlay = bRun; Now.Phase = Phase; Now.Encounter = Encounter; Now.bFinale = bFinale; Now.bPresenting = bPresenting;
    return Now;
}

/** Runs the director through a moment for some seconds at 20 fps, tracking stacking and the longest stretch with no music at all. */
struct FWatch
{
    FAutomationTestBase& Test;
    FExpeditionMusicDirector& Director;
    float Silent = 0.0f;
    float LongestSilence = 0.0f;
    int32 MostLayers = 0;
    TArray<EExpeditionSting> Stings;
    void Run(const FExpeditionMusicMoment& Now, float Seconds)
    {
        for (float Time = 0.0f; Time < Seconds; Time += 0.05f)
        {
            if (TOptional<EExpeditionSting> Started = Director.Update(Now, 0.05f)) { Stings.Add(*Started); }
            float Power = 0.0f;
            TSet<uint8> Keys;
            for (const FAudioLayer& Layer : Director.Fade().Layers()) { Power += FMath::Square(Layer.Amplitude()); Keys.Add(Layer.Key); }
            MostLayers = FMath::Max(MostLayers, Director.Fade().Layers().Num());
            Test.TestEqual(TEXT("One layer per track"), Keys.Num(), Director.Fade().Layers().Num());
            // Audible: a track at -10 dB or more, or a sting still sounding.
            const bool bAudible = Power >= 0.1f || Director.Sting().IsSet();
            Silent = bAudible ? 0.0f : Silent + 0.05f;
            LongestSilence = FMath::Max(LongestSilence, Silent);
        }
    }
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FExpeditionMusicMappingTest, "Project.Expedition.Audio.Music.PhaseToTrack",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FExpeditionMusicMappingTest::RunTest(const FString& Parameters)
{
    using ExpeditionMusic::TrackFor;
    using ExpeditionMusic::StingFor;
    TestEqual(TEXT("The start screen plays the table theme"), TrackFor(false, EExpeditionPhase::Combat, 3, false), EExpeditionMusic::Table);
    for (EExpeditionPhase Phase : {EExpeditionPhase::Route, EExpeditionPhase::Reward, EExpeditionPhase::Recovery, EExpeditionPhase::Victory, EExpeditionPhase::Defeat})
    { TestEqual(TEXT("Every phase off the board's fight plays the table theme"), TrackFor(true, Phase, 2, false), EExpeditionMusic::Table); }
    TestEqual(TEXT("Fight 1 plays combat A"), TrackFor(true, EExpeditionPhase::Combat, 1, false), EExpeditionMusic::CombatA);
    TestEqual(TEXT("Fight 2 plays combat B"), TrackFor(true, EExpeditionPhase::Combat, 2, false), EExpeditionMusic::CombatB);
    TestEqual(TEXT("Fight 3 plays combat A"), TrackFor(true, EExpeditionPhase::Combat, 3, false), EExpeditionMusic::CombatA);
    TestEqual(TEXT("Fight 4 plays combat B"), TrackFor(true, EExpeditionPhase::Combat, 4, false), EExpeditionMusic::CombatB);
    TestEqual(TEXT("The Vault finale plays the finale"), TrackFor(true, EExpeditionPhase::Combat, 5, true), EExpeditionMusic::Finale);
    TestFalse(TEXT("No sting mid-run"), StingFor(true, EExpeditionPhase::Reward).IsSet());
    TestFalse(TEXT("No sting on the start screen"), StingFor(false, EExpeditionPhase::Victory).IsSet());
    TestTrue(TEXT("A won run stings victory"), StingFor(true, EExpeditionPhase::Victory) == TOptional<EExpeditionSting>(EExpeditionSting::Victory));
    TestTrue(TEXT("A lost run stings defeat"), StingFor(true, EExpeditionPhase::Defeat) == TOptional<EExpeditionSting>(EExpeditionSting::Defeat));
    TestEqual(TEXT("Names come from the enum"), ExpeditionMusic::Name(EExpeditionMusic::CombatB), FString(TEXT("CombatB")));
    TestEqual(TEXT("ExpeditionMusicCount matches the reflected enum"), ExpeditionMusicCount, static_cast<int32>(StaticEnum<EExpeditionMusic>()->NumEnums() - 1));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FExpeditionMusicTransitionsTest, "Project.Expedition.Audio.Music.Transitions",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FExpeditionMusicTransitionsTest::RunTest(const FString& Parameters)
{
    using namespace ExpeditionMusicTest;
    FExpeditionMusicDirector Director;
    Director.SetStingLengths(6.9f, 9.0f);
    FWatch Watch{*this, Director};
    Watch.Run(Moment(false, EExpeditionPhase::Route), 3.0f);
    TestEqual(TEXT("The start screen plays the table theme"), Director.Current(), EExpeditionMusic::Table);
    Watch.Run(Moment(true, EExpeditionPhase::Route), 2.0f);
    TestEqual(TEXT("A new run's route keeps it"), Director.Current(), EExpeditionMusic::Table);
    TestEqual(TEXT("...without restarting it"), Director.Fade().Layers().Num(), 1);
    Watch.Run(Moment(true, EExpeditionPhase::Combat, 1), 4.0f);
    TestEqual(TEXT("Fight 1 plays combat A"), Director.Current(), EExpeditionMusic::CombatA);
    // A fight's last blows are still playing: the music holds until they finish.
    Watch.Run(Moment(true, EExpeditionPhase::Reward, 1, false, true), 2.0f);
    TestEqual(TEXT("The music waits for the fight's playback"), Director.Current(), EExpeditionMusic::CombatA);
    Watch.Run(Moment(true, EExpeditionPhase::Reward, 1), 3.0f);
    TestEqual(TEXT("The reward plays the table theme"), Director.Current(), EExpeditionMusic::Table);
    Watch.Run(Moment(true, EExpeditionPhase::Recovery, 1), 1.0f);
    Watch.Run(Moment(true, EExpeditionPhase::Route, 1), 1.0f);
    Watch.Run(Moment(true, EExpeditionPhase::Combat, 2), 0.5f);
    TestEqual(TEXT("Fight 2 plays combat B"), Director.Current(), EExpeditionMusic::CombatB);
    // Quick changes never stack more than two tracks.
    Watch.Run(Moment(true, EExpeditionPhase::Route, 2), 0.5f);
    Watch.Run(Moment(true, EExpeditionPhase::Combat, 3), 0.5f);
    TestEqual(TEXT("Fight 3 plays combat A again"), Director.Current(), EExpeditionMusic::CombatA);
    Watch.Run(Moment(true, EExpeditionPhase::Combat, 5, true), 4.0f);
    TestEqual(TEXT("The finale plays the finale"), Director.Current(), EExpeditionMusic::Finale);

    // The run is won: the finale fades out under the victory sting, and the table theme comes in for its last second.
    Watch.Run(Moment(true, EExpeditionPhase::Victory, 5, true, true), 1.0f);
    TestTrue(TEXT("No sting while the last blow still plays"), Watch.Stings.IsEmpty());
    Watch.Run(Moment(true, EExpeditionPhase::Victory, 5, true), 0.05f);
    TestTrue(TEXT("The victory sting starts once"), Watch.Stings == TArray<EExpeditionSting>({EExpeditionSting::Victory}));
    TestEqual(TEXT("The finale fades out under the sting"), Director.Current(), EExpeditionMusic::None);
    Watch.Run(Moment(true, EExpeditionPhase::Victory, 5, true), 5.0f);
    TestEqual(TEXT("Still the sting"), Director.Current(), EExpeditionMusic::None);
    Watch.Run(Moment(true, EExpeditionPhase::Victory, 5, true), 4.0f);
    TestEqual(TEXT("The table theme follows the sting"), Director.Current(), EExpeditionMusic::Table);
    TestEqual(TEXT("The sting played only once"), Watch.Stings.Num(), 1);

    // A new run, then a lost one.
    Watch.Run(Moment(true, EExpeditionPhase::Route, 0), 1.0f);
    Watch.Run(Moment(true, EExpeditionPhase::Combat, 1), 3.0f);
    Watch.Run(Moment(true, EExpeditionPhase::Defeat, 1), 12.0f);
    TestTrue(TEXT("The defeat sting starts"), Watch.Stings.Num() == 2 && Watch.Stings[1] == EExpeditionSting::Defeat);
    TestEqual(TEXT("The table theme follows it"), Director.Current(), EExpeditionMusic::Table);

    // A new run before the sting has finished: the sting stops at once and the new run's music follows its phase (review of #262).
    for (const EExpeditionPhase End : {EExpeditionPhase::Defeat, EExpeditionPhase::Victory})
    {
        Watch.Run(Moment(true, EExpeditionPhase::Combat, 1), 3.0f);
        Watch.Run(Moment(true, End, 1), 2.0f);
        TestTrue(TEXT("The sting is sounding"), Director.Sting().IsSet() && Director.StingRemaining() > 4.0f);
        Watch.Run(Moment(true, EExpeditionPhase::Route, 0), 0.05f);
        TestFalse(TEXT("A new run stops the sting at once"), Director.Sting().IsSet());
        TestEqual(TEXT("...and its route plays the table theme"), Director.Current(), EExpeditionMusic::Table);
        Watch.Run(Moment(true, EExpeditionPhase::Combat, 1), 0.05f);
        TestEqual(TEXT("A fight picked before the sting would have ended plays its track at once"), Director.Current(), EExpeditionMusic::CombatA);
        Watch.Run(Moment(true, EExpeditionPhase::Combat, 1), 3.0f);
        const FAudioLayer* Fight = Director.Fade().Find(static_cast<uint8>(EExpeditionMusic::CombatA));
        TestTrue(TEXT("...fully faded in after the normal crossfade"), Fight && Fight->Level == 1.0f && Director.Fade().Layers().Num() == 1);
    }
    TestEqual(TEXT("Each sting started once"), Watch.Stings.Num(), 4);

    TestTrue(TEXT("Never more than two tracks at once"), Watch.MostLayers <= FAudioCrossfade::MaxLayers);
    TestTrue(FString::Printf(TEXT("No gap over 1 s (longest %.2f s)"), Watch.LongestSilence), Watch.LongestSilence <= 1.0f);

    // A missing sting has no length: the table theme follows at once, still with no gap.
    FExpeditionMusicDirector Bare;
    FWatch BareWatch{*this, Bare};
    BareWatch.Run(Moment(true, EExpeditionPhase::Combat, 1), 3.0f);
    BareWatch.Run(Moment(true, EExpeditionPhase::Victory, 1), 3.0f);
    TestEqual(TEXT("With no sting recording the table theme follows the fight"), Bare.Current(), EExpeditionMusic::Table);
    TestTrue(TEXT("...with no gap"), BareWatch.LongestSilence <= 1.0f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FExpeditionMusicResumeTest, "Project.Expedition.Audio.Music.ResumeFadesBackIn",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FExpeditionMusicResumeTest::RunTest(const FString& Parameters)
{
    using namespace ExpeditionMusicTest;
    FExpeditionMusicDirector Director;
    FWatch Watch{*this, Director};
    Watch.Run(Moment(true, EExpeditionPhase::Combat, 1), 20.0f);
    const FAudioLayer* Fight = Director.Fade().Find(static_cast<uint8>(EExpeditionMusic::CombatA));
    if (!TestNotNull(TEXT("The fight's track plays"), Fight)) { return false; }
    const float Played = Fight->Position();
    // Return to menu mid-fight: the table theme takes over; the fight's track stops and remembers where it was.
    Watch.Run(Moment(false, EExpeditionPhase::Route), 6.0f);
    TestNull(TEXT("The fight's track has stopped"), Director.Fade().Find(static_cast<uint8>(EExpeditionMusic::CombatA)));
    TestTrue(TEXT("...and remembers where it was"), Director.Fade().Position(static_cast<uint8>(EExpeditionMusic::CombatA)) >= Played);
    const float Stopped = Director.Fade().Position(static_cast<uint8>(EExpeditionMusic::CombatA));
    // Resume: the fight's track fades back in from where it stopped.
    Watch.Run(Moment(true, EExpeditionPhase::Combat, 1), 0.05f);
    const FAudioLayer* Back = Director.Fade().Find(static_cast<uint8>(EExpeditionMusic::CombatA));
    if (!TestNotNull(TEXT("Resume brings the fight's track back"), Back)) { return false; }
    TestTrue(TEXT("It fades in"), Back->Level < 0.1f && Back->Target == 1.0f);
    TestTrue(TEXT("It continues from where it stopped, not from the top"), FMath::IsNearlyEqual(Back->Offset, Stopped, 1e-3f) && Back->Offset > 0.0f);
    // A quick Resume while the fight's track is still fading out turns it back up instead of starting another copy.
    Watch.Run(Moment(true, EExpeditionPhase::Combat, 1), 3.0f);
    Watch.Run(Moment(false, EExpeditionPhase::Route), 0.5f);
    const FAudioLayer* Fading = Director.Fade().Find(static_cast<uint8>(EExpeditionMusic::CombatA));
    const uint32 FadingId = Fading ? Fading->Id : 0;
    Watch.Run(Moment(true, EExpeditionPhase::Combat, 1), 0.05f);
    const FAudioLayer* Revived = Director.Fade().Find(static_cast<uint8>(EExpeditionMusic::CombatA));
    TestTrue(TEXT("A quick Resume revives the same voice"), Fading && Revived && Revived->Id == FadingId);
    TestTrue(TEXT("No gap over 1 s"), Watch.LongestSilence <= 1.0f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FExpeditionMusicShippedTest, "Project.Expedition.Audio.Music.ShippedScore",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FExpeditionMusicShippedTest::RunTest(const FString& Parameters)
{
    const UExpeditionMusicSounds* Sounds = LoadObject<UExpeditionMusicSounds>(nullptr, ExpeditionMusic::SoundsPath, nullptr, LOAD_Quiet | LOAD_NoWarn);
    if (!TestNotNull(TEXT("The shipped music asset (run import_expedition_music.py)"), Sounds)) { return false; }
    FExpeditionMusicBank Bank;
    Bank.Load(Sounds);
    const USoundClass* Class = Bank.Class();
    if (TestNotNull(TEXT("The Music sound class"), Class)) { TestEqual(TEXT("It is SC_Music"), Class->GetName(), FString(TEXT("SC_Music"))); }
    for (int32 Index = 1; Index < ExpeditionMusicCount; ++Index)
    {
        const EExpeditionMusic Track = static_cast<EExpeditionMusic>(Index);
        const USoundWave* Wave = Bank.Track(Track);
        if (!TestNotNull(ExpeditionMusic::Name(Track) + TEXT(" is recorded"), Wave)) { continue; }
        TestTrue(ExpeditionMusic::Name(Track) + TEXT(" loops"), Wave->bLooping);
        TestTrue(ExpeditionMusic::Name(Track) + TEXT(" is at least 45 s"), Wave->GetDuration() >= 45.0f);
        TestTrue(ExpeditionMusic::Name(Track) + TEXT(" plays through the Music class"), Wave->SoundClassObject.Get() == Class);
        TestTrue(ExpeditionMusic::Name(Track) + TEXT(" lives under /Game/DnDDiceRoll/Audio/Music/"), Wave->GetPathName().StartsWith(TEXT("/Game/DnDDiceRoll/Audio/Music/")));
    }
    for (EExpeditionSting Sting : {EExpeditionSting::Victory, EExpeditionSting::Defeat})
    {
        const USoundWave* Wave = Bank.Sting(Sting);
        if (!TestNotNull(ExpeditionMusic::StingName(Sting) + TEXT(" sting is recorded"), Wave)) { continue; }
        TestFalse(ExpeditionMusic::StingName(Sting) + TEXT(" sting does not loop"), Wave->bLooping);
        TestTrue(ExpeditionMusic::StingName(Sting) + TEXT(" sting is short (2-12 s)"), Wave->GetDuration() >= 2.0f && Wave->GetDuration() <= 12.0f);
        TestTrue(ExpeditionMusic::StingName(Sting) + TEXT(" sting plays through the Music class"), Wave->SoundClassObject.Get() == Class);
    }
    // Missing pieces are silence, never synthesis.
    TStrongObjectPtr<UExpeditionMusicSounds> Broken(NewObject<UExpeditionMusicSounds>(GetTransientPackage(), NAME_None, RF_Transient));
    Broken->Tracks.Add(EExpeditionMusic::CombatA).Wave = TSoftObjectPtr<USoundWave>(FSoftObjectPath(TEXT("/Game/DnDDiceRoll/Audio/Missing/SW_NoTrack.SW_NoTrack")));
    FExpeditionMusicBank Empty;
    Empty.Load(Broken.Get());
    TestNull(TEXT("A missing track is silence"), Empty.Track(EExpeditionMusic::CombatA));
    TestTrue(TEXT("Nothing else is recorded"), Empty.RecordedPieces().IsEmpty());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FExpeditionMusicBoardTest, "Project.Expedition.Audio.Music.FollowsTheRun",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FExpeditionMusicBoardTest::RunTest(const FString& Parameters)
{
    using namespace ExpeditionMusicTest;
    FHarness Game(*this, TEXT("MusicBoard"));
    if (!TestTrue(TEXT("Input, board and HUD are ready"), Game.IsReady())) { return false; }
    AExpeditionBattleView* View = Game.View;
    Game.Frame();
    TestEqual(TEXT("The start screen plays the table theme"), View->MusicTrack(), EExpeditionMusic::Table);
    Game.Press(EKeys::Enter);
    Game.Settle();
    TestEqual(TEXT("The route screen keeps the table theme"), View->MusicTrack(), EExpeditionMusic::Table);
    Game.Press(EKeys::Enter);
    if (!TestTrue(TEXT("The first route starts its fight"), View->InCombat())) { return false; }
    Game.Settle();
    TestEqual(TEXT("Fight 1 plays combat A"), View->MusicTrack(), EExpeditionMusic::CombatA);
    for (int32 Step = 0; Step < 100; ++Step) { Game.Frame(); }
    const TArray<uint8> Bytes = Game.CheckpointBytes();
    Game.Press(EKeys::BackSpace);
    Game.Press(EKeys::Enter);
    if (!TestFalse(TEXT("Return to menu puts the run away"), View->HasRun())) { return false; }
    TestEqual(TEXT("The start screen plays the table theme again"), View->MusicTrack(), EExpeditionMusic::Table);
    for (int32 Step = 0; Step < 80; ++Step) { Game.Frame(); }
    const float Stopped = View->MusicDirector().Fade().Position(static_cast<uint8>(EExpeditionMusic::CombatA));
    TestTrue(TEXT("The fight's track remembers where it stopped"), Stopped > 0.0f);
    Game.Press(EKeys::O);
    if (!TestTrue(TEXT("Resume brings the fight back"), View->InCombat())) { return false; }
    Game.Frame();
    TestEqual(TEXT("Resume selects the fight's track"), View->MusicTrack(), EExpeditionMusic::CombatA);
    const FAudioLayer* Back = View->MusicDirector().Fade().Find(static_cast<uint8>(EExpeditionMusic::CombatA));
    TestTrue(TEXT("...fading back in from where it stopped"), Back && Back->Level < 0.5f && FMath::IsNearlyEqual(Back->Offset, Stopped, 1e-3f));
    TestTrue(TEXT("Music changed no checkpoint byte"), Game.CheckpointBytes() == Bytes);
    if (!TestTrue(TEXT("The party wins"), Game.WinBattle())) { return false; }
    Game.Settle();
    for (int32 Step = 0; Step < 10; ++Step) { Game.Frame(); }
    TestEqual(TEXT("The reward plays the table theme"), View->MusicTrack(), EExpeditionMusic::Table);
    TestEqual(TEXT("No sound plays in the test world"), View->MusicVoiceCount(), 0);
    return !HasAnyErrors();
}
#endif
