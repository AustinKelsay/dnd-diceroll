#if WITH_DEV_AUTOMATION_TESTS
#include "ExpeditionInputHarness.h"
#include "Presentation/ExpeditionMix.h"
#include "Sound/SoundClass.h"

// The mix (#246): the four volume buses and their settings file, the duck combat cues put on the beds and music, and the Settings panel
// (keyboard and pointer) on the start screen and in the run's Menu. Changing a volume never touches the run checkpoint.

namespace ExpeditionMixTest
{
using ExpeditionInputHarness::FHarness;
const FName MasterBar(TEXT("Vol:Master")), EffectsBar(TEXT("Vol:Effects")), MusicBar(TEXT("Vol:Music")), Close(TEXT("Close")), Settings(TEXT("Settings"));

FString Folder(const TCHAR* Name) { return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation"), Name, FGuid::NewGuid().ToString()); }

/** Tabs to a control on the current screen (at most a screenful of presses); true when it has the focus. */
bool TabTo(FHarness& Game, FName Id)
{
    for (int32 Press = 0; Press < 40 && Game.Player->MenuFocus() != Id; ++Press) { Game.Press(EKeys::Tab); }
    return Game.Player->MenuFocus() == Id;
}

/** Clicks at a fraction along a drawn button, through the pointer path a mouse uses. */
bool ClickAlong(FHarness& Game, FName Id, float Fraction)
{
    FBox2D Box;
    if (!Game.Hud->ButtonBox(Id, Box)) { return false; }
    Game.Player->SimulatePointer(FVector2D(FMath::Lerp(Box.Min.X, Box.Max.X, Fraction), Box.GetCenter().Y), FIntPoint(0, 0));
    Game.Frame();
    Game.Player->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::LeftMouseButton, IE_Pressed, 1.0f));
    Game.Frame();
    Game.Player->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::LeftMouseButton, IE_Released, 0.0f));
    Game.Frame();
    return true;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FExpeditionMixSettingsTest, "Project.Expedition.Audio.Mix.Settings",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FExpeditionMixSettingsTest::RunTest(const FString& Parameters)
{
    FExpeditionAudioSettings Volumes;
    for (int32 Index = 0; Index < ExpeditionBusCount; ++Index) { TestEqual(TEXT("Every slider starts at the measured mix"), Volumes.Get(static_cast<EExpeditionBus>(Index)), 1.0f); }
    TestTrue(TEXT("A slider moves"), Volumes.Set(EExpeditionBus::Music, 0.62f));
    TestTrue(TEXT("...snapped to the 5% step"), FMath::IsNearlyEqual(Volumes.Get(EExpeditionBus::Music), 0.6f, 1e-4f));
    TestFalse(TEXT("The same value is no change"), Volumes.Set(EExpeditionBus::Music, 0.61f));
    Volumes.Set(EExpeditionBus::Effects, 3.0f);
    TestEqual(TEXT("Clamped at full"), Volumes.Get(EExpeditionBus::Effects), 1.0f);
    Volumes.Set(EExpeditionBus::Ambience, -1.0f);
    TestEqual(TEXT("Clamped at silent"), Volumes.Get(EExpeditionBus::Ambience), 0.0f);
    TestFalse(TEXT("Nudging past silent is no change"), Volumes.Nudge(EExpeditionBus::Ambience, -1));
    TestTrue(TEXT("A nudge is one step"), Volumes.Nudge(EExpeditionBus::Ambience, 2) && FMath::IsNearlyEqual(Volumes.Get(EExpeditionBus::Ambience), 0.1f, 1e-4f));
    TestEqual(TEXT("The panel prints a percent"), FExpeditionAudioSettings::Percent(0.85f), FString(TEXT("85%")));

    // The file round-trips; a broken or foreign document gives the defaults and says so.
    FExpeditionAudioSettings Read;
    TestTrue(TEXT("The document reads back"), FExpeditionAudioSettings::FromJson(Volumes.ToJson(), Read) && Read == Volumes);
    TestFalse(TEXT("Not JSON is refused"), FExpeditionAudioSettings::FromJson(TEXT("volume=loud"), Read));
    TestTrue(TEXT("...with the defaults"), Read == FExpeditionAudioSettings());
    TestFalse(TEXT("Another version is refused"), FExpeditionAudioSettings::FromJson(TEXT("{\"version\": 2, \"master\": 0.1}"), Read));
    TestTrue(TEXT("A missing value keeps its default"), FExpeditionAudioSettings::FromJson(TEXT("{\"version\": 1, \"music\": 0.25}"), Read)
        && Read.Get(EExpeditionBus::Master) == 1.0f && FMath::IsNearlyEqual(Read.Get(EExpeditionBus::Music), 0.25f, 1e-4f));
    // Out-of-range values are refused, not clamped: a hand-edited {"master": -1} must not mute the game for good (review of #263).
    TestTrue(TEXT("A document with out-of-range values still reads"),
        FExpeditionAudioSettings::FromJson(TEXT("{\"version\": 1, \"master\": -1, \"music\": 7, \"ambience\": 1.0001, \"effects\": 0.4}"), Read));
    TestEqual(TEXT("A negative master keeps its default"), Read.Get(EExpeditionBus::Master), 1.0f);
    TestEqual(TEXT("An excessive music keeps its default"), Read.Get(EExpeditionBus::Music), 1.0f);
    TestEqual(TEXT("Just over full keeps its default"), Read.Get(EExpeditionBus::Ambience), 1.0f);
    TestTrue(TEXT("A valid value beside them is taken"), FMath::IsNearlyEqual(Read.Get(EExpeditionBus::Effects), 0.4f, 1e-4f));
    TestTrue(TEXT("The edges are valid"), FExpeditionAudioSettings::FromJson(TEXT("{\"version\": 1, \"master\": 0, \"music\": 1}"), Read)
        && Read.Get(EExpeditionBus::Master) == 0.0f && Read.Get(EExpeditionBus::Music) == 1.0f);
    const FString Directory = ExpeditionMixTest::Folder(TEXT("MixSettings"));
    const FString Path = FPaths::Combine(Directory, TEXT("nested"), TEXT("audio.json"));
    TestTrue(TEXT("A missing file is the defaults"), FExpeditionAudioSettings::Load(Path, Read) && Read == FExpeditionAudioSettings());
    TestTrue(TEXT("Saving creates the folder"), FExpeditionAudioSettings::Save(Path, Volumes));
    TestTrue(TEXT("Loading reads what was saved"), FExpeditionAudioSettings::Load(Path, Read) && Read == Volumes);
    TestFalse(TEXT("No temporary file is left behind"), IFileManager::Get().FileExists(*(Path + TEXT(".pending"))));
    // The write is atomic: a failed write keeps the last good settings.
    FExpeditionAudioSettings Other = Volumes;
    Other.Set(EExpeditionBus::Master, 0.2f);
    ExpeditionTestSupport::FailNextCheckpointWrite(*this, Path);
    TestFalse(TEXT("A write that cannot finish is reported"), FExpeditionAudioSettings::Save(Path, Other));
    ExpeditionTestSupport::AllowCheckpointWrites(Path);
    TestTrue(TEXT("...and the last good settings are still on disk"), FExpeditionAudioSettings::Load(Path, Read) && Read == Volumes);
    TestTrue(TEXT("A later write succeeds and replaces them"), FExpeditionAudioSettings::Save(Path, Other) && FExpeditionAudioSettings::Load(Path, Read) && Read == Other);
    FFileHelper::SaveStringToFile(TEXT("{ broken"), *Path);
    TestFalse(TEXT("An unreadable file is reported"), FExpeditionAudioSettings::Load(Path, Read));
    TestTrue(TEXT("...and plays the defaults"), Read == FExpeditionAudioSettings());
    IFileManager::Get().DeleteDirectory(*Directory, false, true);
    TestTrue(TEXT("-ExpeditionSettingsRoot selects the folder"), FExpeditionAudioSettings::DefaultPath(TEXT("Game -ExpeditionSettingsRoot=/tmp/x")) == FPaths::Combine(TEXT("/tmp/x"), TEXT("expedition-audio.json")));
    TestTrue(TEXT("The default file is under Saved/Settings, apart from the checkpoint"), FExpeditionAudioSettings::DefaultPath(nullptr).Contains(TEXT("Settings")) && !FExpeditionAudioSettings::DefaultPath(nullptr).Contains(TEXT("SaveGames")));

    // Buses: Master scales every other one.
    TestEqual(TEXT("Silent is silent"), ExpeditionMix::Amplitude(0.0f), 0.0f);
    TestEqual(TEXT("Full is unity"), ExpeditionMix::Amplitude(1.0f), 1.0f);
    TestEqual(TEXT("Half is about -12 dB"), ExpeditionMix::Amplitude(0.5f), 0.25f);
    FExpeditionAudioSettings Mixed;
    Mixed.Set(EExpeditionBus::Master, 0.5f);
    Mixed.Set(EExpeditionBus::Music, 0.5f);
    TestEqual(TEXT("Master alone"), ExpeditionMix::BusGain(Mixed, EExpeditionBus::Master), 0.25f);
    TestEqual(TEXT("Master times the bus"), ExpeditionMix::BusGain(Mixed, EExpeditionBus::Music), 0.0625f);
    TestEqual(TEXT("A full bus under a half master"), ExpeditionMix::BusGain(Mixed, EExpeditionBus::Effects), 0.25f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FExpeditionMixDuckTest, "Project.Expedition.Audio.Mix.Ducking",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FExpeditionMixDuckTest::RunTest(const FString& Parameters)
{
    for (EExpeditionCue Cue : {EExpeditionCue::Hit, EExpeditionCue::Critical, EExpeditionCue::Miss, EExpeditionCue::Swing, EExpeditionCue::Bow,
        EExpeditionCue::Spell, EExpeditionCue::Heal, EExpeditionCue::Downed, EExpeditionCue::Death})
    { TestTrue(TEXT("Combat cues duck"), ExpeditionMix::Ducks(Cue)); }
    for (EExpeditionCue Cue : {EExpeditionCue::Step, EExpeditionCue::Click, EExpeditionCue::DiceRoll, EExpeditionCue::PartyTurn, EExpeditionCue::Route})
    { TestFalse(TEXT("Steps, clicks, dice and turns do not"), ExpeditionMix::Ducks(Cue)); }
    FAudioDucker Duck(0.5f);
    TestEqual(TEXT("At rest it passes everything"), Duck.Gain(), 1.0f);
    Duck.Trigger();
    Duck.Tick(FAudioDucker::AttackSeconds);
    TestEqual(TEXT("It ducks fast"), Duck.Gain(), 0.5f);
    Duck.Tick(FAudioDucker::HoldSeconds * 0.9f);
    TestEqual(TEXT("It holds while the cue sounds"), Duck.Gain(), 0.5f);
    Duck.Trigger();
    Duck.Tick(0.01f);
    Duck.Tick(FAudioDucker::HoldSeconds * 0.9f);
    TestEqual(TEXT("Another cue extends the hold"), Duck.Gain(), 0.5f);
    Duck.Tick(FAudioDucker::HoldSeconds);
    Duck.Tick(FAudioDucker::ReleaseSeconds * 0.5f);
    TestTrue(TEXT("Then it releases"), Duck.Gain() > 0.5f && Duck.Gain() < 1.0f);
    Duck.Tick(FAudioDucker::ReleaseSeconds);
    TestEqual(TEXT("Briefly: back to rest"), Duck.Gain(), 1.0f);

    // The board ducks on a combat cue and recovers within a second, with no audio device.
    ExpeditionInputHarness::FHarness Game(*this, TEXT("MixDuck"));
    if (!TestTrue(TEXT("Harness ready"), Game.IsReady())) { return false; }
    Game.View->PlayCue(EExpeditionCue::Hit);
    Game.Frame(0.05f);
    TestEqual(TEXT("A hit ducks the ambience"), Game.View->AmbienceDuckGain(), ExpeditionMix::AmbienceDuck);
    TestEqual(TEXT("...and the music"), Game.View->MusicDuckGain(), ExpeditionMix::MusicDuck);
    for (int32 Step = 0; Step < 20; ++Step) { Game.Frame(0.05f); }
    TestEqual(TEXT("The ambience recovers"), Game.View->AmbienceDuckGain(), 1.0f);
    TestEqual(TEXT("The music recovers"), Game.View->MusicDuckGain(), 1.0f);
    Game.View->PlayCue(EExpeditionCue::Step);
    Game.Frame(0.05f);
    TestEqual(TEXT("A step does not duck"), Game.View->MusicDuckGain(), 1.0f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FExpeditionMixClassesTest, "Project.Expedition.Audio.Mix.Buses",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FExpeditionMixClassesTest::RunTest(const FString& Parameters)
{
    const USoundClass* Master = LoadObject<USoundClass>(nullptr, ExpeditionMix::MasterClassPath, nullptr, LOAD_Quiet | LOAD_NoWarn);
    if (!TestNotNull(TEXT("SC_Master (run setup_expedition_mix.py)"), Master)) { return false; }
    for (const TCHAR* Path : {TEXT("/Game/DnDDiceRoll/Audio/SC_Effects.SC_Effects"), TEXT("/Game/DnDDiceRoll/Audio/SC_Ambience.SC_Ambience"), TEXT("/Game/DnDDiceRoll/Audio/SC_Music.SC_Music")})
    {
        const USoundClass* Bus = LoadObject<USoundClass>(nullptr, Path, nullptr, LOAD_Quiet | LOAD_NoWarn);
        if (!TestNotNull(FString(Path) + TEXT(" exists"), Bus)) { continue; }
        TestTrue(FString(Path) + TEXT(" is a child of SC_Master"), Bus->ParentClass == Master && Master->ChildClasses.Contains(Bus));
    }
    TestEqual(TEXT("SC_Master has the three buses"), Master->ChildClasses.Num(), 3);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FExpeditionMixPanelTest, "Project.Expedition.Audio.Mix.SettingsPanel",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FExpeditionMixPanelTest::RunTest(const FString& Parameters)
{
    using namespace ExpeditionMixTest;
    const FString Shared = FPaths::Combine(Folder(TEXT("MixPanel")), TEXT("expedition-audio.json"));
    {
        FHarness Game(*this, TEXT("MixPanel"), nullptr, nullptr, Shared);
        if (!TestTrue(TEXT("Input, board and HUD are ready"), Game.IsReady())) { return false; }
        AExpeditionBattleView* View = Game.View;
        AExpeditionPlayerController* Player = Game.Player;
        Game.Frame();
        // The start screen: Settings is a Tab stop; Enter opens the panel on its first slider.
        if (!TestTrue(TEXT("Tab reaches Settings on the start screen"), TabTo(Game, Settings))) { return false; }
        Game.Press(EKeys::Enter);
        if (!TestTrue(TEXT("Enter opens Settings"), Player->ShowingSettings())) { return false; }
        Game.Frame();
        TestEqual(TEXT("The panel focuses Master"), Player->MenuFocus(), MasterBar);
        const TArray<FName> Stops = Game.Hud->MenuButtons();
        TestTrue(TEXT("The Tab stops are the four bars and Close"), Stops == TArray<FName>({MasterBar, EffectsBar, FName(TEXT("Vol:Ambience")), MusicBar, Close}));
        Game.Press(EKeys::Left);
        TestTrue(TEXT("Left moves Master down a step"), FMath::IsNearlyEqual(View->AudioSettings().Get(EExpeditionBus::Master), 0.95f, 1e-4f));
        TestTrue(TEXT("...and changes the Master bus"), FMath::IsNearlyEqual(View->ClassVolume(EExpeditionBus::Master), 0.95f * 0.95f, 1e-4f));
        Game.Press(EKeys::Down);
        TestEqual(TEXT("Down moves to Effects"), Player->MenuFocus(), EffectsBar);
        Game.Press(EKeys::Left); Game.Press(EKeys::Left);
        TestTrue(TEXT("Effects is at 90%"), FMath::IsNearlyEqual(View->AudioSettings().Get(EExpeditionBus::Effects), 0.9f, 1e-4f));
        TestTrue(TEXT("The Effects bus plays at Master times Effects"), FMath::IsNearlyEqual(View->BusVolume(EExpeditionBus::Effects), 0.95f * 0.95f * 0.9f * 0.9f, 1e-4f));
        Game.Press(EKeys::Right);
        TestTrue(TEXT("Right moves it back up"), FMath::IsNearlyEqual(View->AudioSettings().Get(EExpeditionBus::Effects), 0.95f, 1e-4f));
        // The pointer: a click on the Music bar sets it where it lands and takes the focus; the steps nudge it.
        if (TestTrue(TEXT("The Music bar is drawn"), ClickAlong(Game, MusicBar, 0.4f)))
        {
            const float Music = View->AudioSettings().Get(EExpeditionBus::Music);
            TestTrue(FString::Printf(TEXT("A click 40%% along the bar sets Music near 40%% (%.2f)"), Music), Music >= 0.35f && Music <= 0.45f);
            TestEqual(TEXT("The click focuses that bar"), Player->MenuFocus(), MusicBar);
        }
        const float BeforeStep = View->AudioSettings().Get(EExpeditionBus::Music);
        ClickAlong(Game, FName(TEXT("VolUp:Music")), 0.5f);
        TestTrue(TEXT("+ nudges a step"), FMath::IsNearlyEqual(View->AudioSettings().Get(EExpeditionBus::Music), BeforeStep + 0.05f, 1e-4f));
        // The panel owns input: gameplay and other overlays wait.
        Game.Press(EKeys::Slash);
        TestFalse(TEXT("Help waits behind Settings"), Player->ShowingHelp());
        Game.Press(EKeys::N);
        TestFalse(TEXT("New Run waits behind Settings"), View->HasRun());
        Game.Press(EKeys::Escape);
        TestFalse(TEXT("Esc closes Settings"), Player->ShowingSettings());
        TestEqual(TEXT("...and gives the focus back to Settings"), Player->MenuFocus(), Settings);
        TestTrue(TEXT("The settings file was written"), IFileManager::Get().FileExists(*Shared));

        // In a run: the Menu lists Settings; changing a volume writes no checkpoint byte.
        Game.Press(EKeys::N);
        if (!TestTrue(TEXT("N starts a new run once Settings is closed"), View->HasRun())) { return false; }
        Game.Settle();
        const TArray<uint8> Bytes = Game.CheckpointBytes();
        TestFalse(TEXT("The run has a checkpoint"), Bytes.IsEmpty());
        Game.Press(EKeys::I);
        TestTrue(TEXT("The Menu lists Settings"), Game.Hud->MenuButtons().Contains(Settings));
        if (!TestTrue(TEXT("Tab reaches the Menu's Settings row"), TabTo(Game, Settings))) { return false; }
        Game.Press(EKeys::Enter);
        if (!TestTrue(TEXT("Enter on the row opens Settings"), Player->ShowingSettings())) { return false; }
        TestFalse(TEXT("The Menu is put away under the panel"), Player->ShowingMenu());
        Game.Press(EKeys::Down); Game.Press(EKeys::Down);
        Game.Press(EKeys::Left);
        TestTrue(TEXT("Ambience moved from the run's Menu"), FMath::IsNearlyEqual(View->AudioSettings().Get(EExpeditionBus::Ambience), 0.95f, 1e-4f));
        TestTrue(TEXT("Changing volumes leaves the run checkpoint's bytes unchanged"), Game.CheckpointBytes() == Bytes);
        Game.Press(EKeys::Escape);
        TestFalse(TEXT("Esc closes it"), Player->ShowingSettings());
        // Back where it was opened from: the Menu, on its Settings row (review of #263); a second Esc closes the Menu onto the route.
        TestTrue(TEXT("Closing Settings brings the Menu back"), Player->ShowingMenu());
        TestEqual(TEXT("...focused on its Settings row"), Player->MenuFocus(), Settings);
        Game.Press(EKeys::Escape);
        TestFalse(TEXT("A second Esc closes the Menu"), Player->ShowingMenu());
        TestEqual(TEXT("...and the route screen has its own focus again"), Player->MenuFocus(), FName(TEXT("Route0")));
        TestTrue(TEXT("The run is still in play"), View->HasRun());
    }
    {
        // A new launch reads the same file: the volumes persist.
        FHarness Again(*this, TEXT("MixPanelAgain"), nullptr, nullptr, Shared);
        if (!TestTrue(TEXT("Second harness ready"), Again.IsReady())) { return false; }
        const FExpeditionAudioSettings& Read = Again.View->AudioSettings();
        TestTrue(TEXT("Master persists"), FMath::IsNearlyEqual(Read.Get(EExpeditionBus::Master), 0.95f, 1e-4f));
        TestTrue(TEXT("Effects persists"), FMath::IsNearlyEqual(Read.Get(EExpeditionBus::Effects), 0.95f, 1e-4f));
        TestTrue(TEXT("Ambience persists"), FMath::IsNearlyEqual(Read.Get(EExpeditionBus::Ambience), 0.95f, 1e-4f));
        TestTrue(TEXT("Music persists"), Read.Get(EExpeditionBus::Music) < 0.6f);
        TestTrue(TEXT("...and is applied to its bus"), FMath::IsNearlyEqual(Again.View->ClassVolume(EExpeditionBus::Music), ExpeditionMix::Amplitude(Read.Get(EExpeditionBus::Music)), 1e-4f));
    }
    IFileManager::Get().DeleteDirectory(*FPaths::GetPath(Shared), false, true);
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FExpeditionMixHoldTest, "Project.Expedition.Audio.Mix.SettingsHoldsTheBoard",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FExpeditionMixHoldTest::RunTest(const FString& Parameters)
{
    // Settings is modal for the board too (review of #263): while it is open no playback step advances and no enemy turn resolves.
    using namespace ExpeditionMixTest;
    FHarness Game(*this, TEXT("MixHold"));
    if (!TestTrue(TEXT("Harness ready"), Game.IsReady())) { return false; }
    AExpeditionBattleView* View = Game.View;
    AExpeditionPlayerController* Player = Game.Player;
    Game.Frame();
    Game.Press(EKeys::N);
    Game.Settle();
    Game.Press(EKeys::Enter);
    if (!TestTrue(TEXT("The first fight starts"), View->InCombat())) { return false; }
    Game.PartyTurn();
    // During playback: a committed move is showing.
    const FCombatantState* Hero = View->Rules().Find(View->Rules().ActiveId());
    if (!TestTrue(TEXT("The party has the turn"), Hero && Hero->Side == ECombatSide::Party)) { return false; }
    const TArray<FIntPoint> Moves = View->Rules().LegalMoves(Hero->Id);
    if (!TestFalse(TEXT("The hero can move"), Moves.IsEmpty())) { return false; }
    TestTrue(TEXT("The move is accepted"), View->Execute({ECombatCommandType::Move, Hero->Id, Moves.Last()}));
    Game.Frame();
    if (!TestTrue(TEXT("Its playback is running"), View->IsPresenting())) { return false; }
    // The Menu's Settings row by its handler, so the panel opens within a frame (the keyboard path is SettingsPanel's).
    Game.Press(EKeys::I);
    Player->HandleButton(Settings);
    Game.Frame();
    if (!TestTrue(TEXT("Settings opens over the playback"), Player->ShowingSettings() && View->HeldForSettings())) { return false; }
    const int32 Shown = View->Playback().StepsShown();
    const FExpeditionSnapshot During = Game.Live();
    const TArray<uint8> Bytes = Game.CheckpointBytes();
    for (int32 Step = 0; Step < 120; ++Step) { Game.Frame(); }
    TestEqual(TEXT("No playback step advances behind Settings"), View->Playback().StepsShown(), Shown);
    TestTrue(TEXT("The playback is still pending"), View->IsPresenting());
    TestTrue(TEXT("Nothing is resolved behind Settings"), ExpeditionTestSupport::SameRun(During, Game.Live()) && Game.CheckpointBytes() == Bytes);
    Game.Press(EKeys::Escape);
    Game.Press(EKeys::Escape);
    TestFalse(TEXT("Settings and the Menu are closed"), Player->ShowingSettings() || Player->ShowingMenu() || View->HeldForSettings());
    Game.Settle();
    TestFalse(TEXT("The committed playback finishes once Settings closes"), View->IsPresenting());
    TestTrue(TEXT("...having advanced"), View->Playback().StepsShown() > Shown || !View->IsPresenting());

    // During an enemy turn: an enemy is about to act (still thinking) when Settings opens.
    for (int32 Turn = 0; Turn < 12 && View->InCombat(); ++Turn)
    {
        const FCombatantState* Active = View->Rules().Find(View->Rules().ActiveId());
        if (Active && Active->Side == ECombatSide::Enemy) { break; }
        Game.PartyTurn();
        Game.Press(EKeys::SpaceBar);
    }
    const FCombatantState* Foe = View->Rules().Find(View->Rules().ActiveId());
    if (!TestTrue(TEXT("An enemy has the turn"), View->InCombat() && Foe && Foe->Side == ECombatSide::Enemy)) { return false; }
    // The Menu's Settings row by its handler, so the panel opens within a frame (the keyboard path is SettingsPanel's).
    Game.Press(EKeys::I);
    Player->HandleButton(Settings);
    Game.Frame();
    if (!TestTrue(TEXT("Settings opens on the enemy's turn"), Player->ShowingSettings())) { return false; }
    const FExpeditionSnapshot Waiting = Game.Live();
    const TArray<uint8> Before = Game.CheckpointBytes();
    for (int32 Step = 0; Step < 120; ++Step) { Game.Frame(); }
    TestTrue(TEXT("The enemy does not act behind Settings (6 s, ten times its think time)"), ExpeditionTestSupport::SameRun(Waiting, Game.Live()));
    TestTrue(TEXT("...and nothing is checkpointed"), Game.CheckpointBytes() == Before);
    Game.Press(EKeys::Escape);
    Game.Press(EKeys::Escape);
    for (int32 Step = 0; Step < 200 && ExpeditionTestSupport::SameRun(Waiting, Game.Live()); ++Step) { Game.Frame(); }
    TestFalse(TEXT("Once Settings closes the enemy takes its turn"), ExpeditionTestSupport::SameRun(Waiting, Game.Live()));
    TestTrue(TEXT("...and it is checkpointed"), Game.CheckpointBytes() != Before);
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FExpeditionMixLayoutTest, "Project.Expedition.Audio.Mix.SettingsLayout",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FExpeditionMixLayoutTest::RunTest(const FString& Parameters)
{
    using namespace ExpeditionMixTest;
    FHarness Game(*this, TEXT("MixLayout"));
    if (!TestTrue(TEXT("Harness ready"), Game.IsReady())) { return false; }
    Game.Frame();
    Game.Player->HandleButton(Settings);
    for (const FIntPoint Size : {FIntPoint(1920, 1080), FIntPoint(1280, 720), FIntPoint(1600, 600), FIntPoint(2560, 1080), FIntPoint(1024, 576)})
    {
        Game.Target.Size = Size;
        Game.Frame();
        const FString At = FString::Printf(TEXT("%dx%d: "), Size.X, Size.Y);
        const FBox2D Panel = Game.Hud->SettingsPanel();
        if (!TestTrue(At + TEXT("the panel is drawn"), Panel.bIsValid)) { continue; }
        TestTrue(At + TEXT("the panel is on screen"), Panel.Min.X >= 0 && Panel.Min.Y >= 0 && Panel.Max.X <= Size.X + 0.5f && Panel.Max.Y <= Size.Y + 0.5f);
        const TArray<FBox2D> Rects = Game.Hud->ButtonRects();
        TestEqual(At + TEXT("four bars, eight steps and Close"), Rects.Num(), 13);
        for (int32 A = 0; A < Rects.Num(); ++A)
        {
            TestTrue(At + TEXT("every control is inside the panel"), Panel.IsInside(Rects[A].Min + FVector2D(0.5f)) && Panel.IsInside(Rects[A].Max - FVector2D(0.5f)));
            for (int32 B = A + 1; B < Rects.Num(); ++B) { TestFalse(At + TEXT("no two controls overlap"), Rects[A].Intersect(Rects[B])); }
        }
        TestEqual(At + TEXT("the focus stays on Master"), Game.Player->MenuFocus(), MasterBar);
    }
    return !HasAnyErrors();
}
#endif
