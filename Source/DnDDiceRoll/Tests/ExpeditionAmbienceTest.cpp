#if WITH_DEV_AUTOMATION_TESTS
#include "ExpeditionInputHarness.h"
#include "Presentation/AudioCrossfade.h"
#include "Presentation/DiceRollTheme.h"
#include "Presentation/ExpeditionAmbience.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundWave.h"
#include "UObject/StrongObjectPtr.h"

// Ambience beds (#244): the bed follows the board (start screen silent, war table on the overview, the drawn world's bed in a fight),
// crossfades without stacking, stops on Return to menu, comes back on Resume, and a missing recording is silence, never synthesis.

namespace ExpeditionAmbienceTest
{
using ExpeditionInputHarness::FHarness;

uint8 Key(EExpeditionAmbience Bed) { return static_cast<uint8>(Bed); }

/** Every layer's keys are unique and there are never more than two. */
bool NoStacking(const FAudioCrossfade& Fade)
{
    TSet<uint8> Keys;
    for (const FAudioLayer& Layer : Fade.Layers()) { Keys.Add(Layer.Key); }
    return Fade.Layers().Num() <= FAudioCrossfade::MaxLayers && Keys.Num() == Fade.Layers().Num() && !Keys.Contains(0);
}

/** The bed the drawn world of the run's last chosen route should play. */
EExpeditionAmbience ExpectedBed(const AExpeditionBattleView& View)
{
    if (!View.HasRun() || View.Progress().History.IsEmpty()) { return EExpeditionAmbience::None; }
    if (View.UsesVaultKit()) { return EExpeditionAmbience::Vault; }
    if (View.UsesRootKit()) { return EExpeditionAmbience::Root; }
    if (View.UsesVigilKit()) { return EExpeditionAmbience::Vigil; }
    const EExpeditionWorld World = FExpeditionRun::Encounter(View.Progress(), View.Progress().History.Last().Route).World;
    return World == EExpeditionWorld::Root ? EExpeditionAmbience::Root : World == EExpeditionWorld::Vault ? EExpeditionAmbience::Vault : EExpeditionAmbience::Vigil;
}

/** A checkpoint on the route screen before the finale (four fights won), for the Vault bed. */
bool BeforeFinale(FExpeditionSaveData& Data)
{
    FString Error;
    for (int32 Seed = 1; Seed < 200; ++Seed)
    {
        FExpeditionRun Run;
        if (!Run.NewRun(Seed, Error, FString::Printf(TEXT("ambience-%d"), Seed))) { continue; }
        bool bOk = true;
        while (bOk && Run.Progress().Phase == EExpeditionPhase::Route && Run.Progress().EncountersCompleted < FExpeditionRun::EncounterCount - 1)
        {
            if (!Run.ChooseRoute(Run.Progress().Offers[0].Id, Error) || !ExpeditionTestSupport::FinishCombat(Run) || Run.Progress().Phase != EExpeditionPhase::Reward) { bOk = false; break; }
            int32 Survivor = INDEX_NONE;
            for (const FCombatantState& Unit : Run.Battle().Snapshot().Encounter.Combatants)
            { if (Unit.Side == ECombatSide::Party && Unit.Life == ECombatLife::Conscious) { Survivor = Unit.Id; break; } }
            if (Survivor == INDEX_NONE || !Run.ClaimReward(Survivor, Error)) { bOk = false; break; }
            if (Run.Progress().Phase == EExpeditionPhase::Recovery && !Run.Recover(true, Error)) { bOk = false; break; }
        }
        if (!bOk || Run.Progress().Phase != EExpeditionPhase::Route || Run.Progress().EncountersCompleted != FExpeditionRun::EncounterCount - 1) { continue; }
        Data = FExpeditionSaveData();
        Data.Current = Run.Snapshot();
        return true;
    }
    return false;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAudioCrossfadeTest, "Project.Expedition.Audio.Ambience.Crossfade",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FAudioCrossfadeTest::RunTest(const FString& Parameters)
{
    using namespace ExpeditionAmbienceTest;
    FAudioCrossfade Fade;
    TestEqual(TEXT("Silent at first"), Fade.Current(), uint8(0));
    TestFalse(TEXT("Selecting silence when silent changes nothing"), Fade.Select(0, 2.0f));
    TestTrue(TEXT("Selecting a bed starts it"), Fade.Select(Key(EExpeditionAmbience::WarTable), 2.0f));
    if (!TestEqual(TEXT("One layer"), Fade.Layers().Num(), 1)) { return false; }
    TestEqual(TEXT("It starts from silence"), Fade.Layers()[0].Amplitude(), 0.0f);
    Fade.Tick(1.0f);
    TestTrue(TEXT("Halfway in"), FMath::IsNearlyEqual(Fade.Layers()[0].Level, 0.5f, 1e-4f));
    TestTrue(TEXT("Equal power: halfway is -3 dB"), FMath::IsNearlyEqual(Fade.Layers()[0].Amplitude(), FMath::Sin(HALF_PI * 0.5f), 1e-4f));
    const uint32 FirstId = Fade.Layers()[0].Id;
    TestFalse(TEXT("Selecting the current bed again changes nothing"), Fade.Select(Key(EExpeditionAmbience::WarTable), 2.0f));
    TestEqual(TEXT("...and keeps the same layer (no restart)"), Fade.Layers()[0].Id, FirstId);
    Fade.Tick(5.0f);
    TestEqual(TEXT("A finished fade-in rests at full"), Fade.Layers()[0].Level, 1.0f);

    // A board change: the old bed fades out over the same time the new one fades in, so there is no gap.
    TestTrue(TEXT("A new bed is selected"), Fade.Select(Key(EExpeditionAmbience::Vigil), 2.0f));
    TestTrue(TEXT("Two layers while crossfading"), Fade.Layers().Num() == 2 && NoStacking(Fade));
    float Lowest = 1.0f;
    for (int32 Step = 0; Step < 10; ++Step)
    {
        Fade.Tick(0.2f);
        float Power = 0.0f;
        for (const FAudioLayer& Layer : Fade.Layers()) { Power += FMath::Square(Layer.Amplitude()); }
        Lowest = FMath::Min(Lowest, Power);
    }
    TestTrue(TEXT("The crossfade keeps the summed power (no dip or gap)"), Lowest > 0.99f);
    TestTrue(TEXT("After the fade only the new bed is left"), Fade.Layers().Num() == 1 && Fade.Layers()[0].Key == Key(EExpeditionAmbience::Vigil));

    // Quick changes: a third bed while two are crossfading drops the quieter outgoing one, so at most two ever play.
    Fade.Select(Key(EExpeditionAmbience::WarTable), 2.0f);
    Fade.Tick(0.5f);
    TestTrue(TEXT("Selecting a third bed mid-fade"), Fade.Select(Key(EExpeditionAmbience::Root), 2.0f));
    TestTrue(TEXT("Still at most two layers, one per bed"), NoStacking(Fade));
    TestNull(TEXT("The quieter outgoing bed was dropped"), Fade.Find(Key(EExpeditionAmbience::WarTable)));
    TestNotNull(TEXT("The louder outgoing bed fades out"), Fade.Find(Key(EExpeditionAmbience::Vigil)));
    // Returning to a bed that is still fading out turns it back up from where it is instead of adding another copy.
    const FAudioLayer* Fading = Fade.Find(Key(EExpeditionAmbience::Vigil));
    const uint32 FadingId = Fading ? Fading->Id : 0;
    const float FadingLevel = Fading ? Fading->Level : 0.0f;
    Fade.Select(Key(EExpeditionAmbience::Vigil), 2.0f);
    const FAudioLayer* Revived = Fade.Find(Key(EExpeditionAmbience::Vigil));
    TestTrue(TEXT("The same layer is revived"), Revived && Revived->Id == FadingId && Revived->Target == 1.0f && Revived->Level == FadingLevel);
    TestTrue(TEXT("No stacking after reviving"), NoStacking(Fade));

    // Silence fades everything out and empties the director.
    TestTrue(TEXT("Silence is selected"), Fade.Select(0, 1.5f));
    TestEqual(TEXT("Silence is current"), Fade.Current(), uint8(0));
    for (const FAudioLayer& Layer : Fade.Layers()) { TestTrue(TEXT("Every layer fades out"), Layer.IsFadingOut()); }
    Fade.Tick(1.6f);
    TestEqual(TEXT("Every layer has stopped"), Fade.Layers().Num(), 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAmbienceBedForTest, "Project.Expedition.Audio.Ambience.BedFor",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FAmbienceBedForTest::RunTest(const FString& Parameters)
{
    using ExpeditionAmbience::BedFor;
    for (EDiceRollKit Kit : {EDiceRollKit::Vigil, EDiceRollKit::Root, EDiceRollKit::Vault})
    {
        TestEqual(TEXT("No run in play is silent"), BedFor(false, true, Kit), EExpeditionAmbience::None);
        TestEqual(TEXT("No run in play is silent even off the table"), BedFor(false, false, Kit), EExpeditionAmbience::None);
        TestEqual(TEXT("The overview is the war table"), BedFor(true, true, Kit), EExpeditionAmbience::WarTable);
    }
    TestEqual(TEXT("The Vigil kit plays the Vigil"), BedFor(true, false, EDiceRollKit::Vigil), EExpeditionAmbience::Vigil);
    TestEqual(TEXT("The Root kit plays the Root"), BedFor(true, false, EDiceRollKit::Root), EExpeditionAmbience::Root);
    TestEqual(TEXT("The Vault kit plays the Vault"), BedFor(true, false, EDiceRollKit::Vault), EExpeditionAmbience::Vault);
    TestEqual(TEXT("Bed names come from the enum"), ExpeditionAmbience::Name(EExpeditionAmbience::WarTable), FString(TEXT("WarTable")));
    TestEqual(TEXT("ExpeditionAmbienceCount matches the reflected enum"), ExpeditionAmbienceCount, static_cast<int32>(StaticEnum<EExpeditionAmbience>()->NumEnums() - 1));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAmbienceMissingTest, "Project.Expedition.Audio.Ambience.MissingIsSilence",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FAmbienceMissingTest::RunTest(const FString& Parameters)
{
    FExpeditionAmbienceBank Bank;
    Bank.Load(nullptr);
    TestEqual(TEXT("No asset: every bed is silent"), Bank.RecordedBeds().Num(), 0);
    TStrongObjectPtr<USoundWave> Loop(NewObject<USoundWave>(GetTransientPackage(), NAME_None, RF_Transient));
    Loop->Duration = 30.0f;
    Loop->NumChannels = 2;
    TStrongObjectPtr<UExpeditionAmbienceSounds> Sounds(NewObject<UExpeditionAmbienceSounds>(GetTransientPackage(), NAME_None, RF_Transient));
    FExpeditionAmbienceBed& Table = Sounds->Beds.Add(EExpeditionAmbience::WarTable);
    Table.Loop = TSoftObjectPtr<USoundWave>(Loop.Get());
    Table.Volume = 7.0f;
    Sounds->Beds.Add(EExpeditionAmbience::Vigil).Loop = TSoftObjectPtr<USoundWave>(FSoftObjectPath(TEXT("/Game/DnDDiceRoll/Audio/Missing/SW_NoBed.SW_NoBed")));
    Sounds->Beds.Add(EExpeditionAmbience::Root);
    Bank.Load(Sounds.Get());
    TestEqual(TEXT("The loaded bed plays its loop"), Bank.Wave(EExpeditionAmbience::WarTable), Loop.Get());
    TestEqual(TEXT("Its volume is clamped"), Bank.Volume(EExpeditionAmbience::WarTable), FExpeditionAmbienceBed::MaxVolume);
    TestNull(TEXT("A missing recording is silence"), Bank.Wave(EExpeditionAmbience::Vigil));
    TestNull(TEXT("An unset recording is silence"), Bank.Wave(EExpeditionAmbience::Root));
    TestNull(TEXT("A bed with no entry is silence"), Bank.Wave(EExpeditionAmbience::Vault));
    TestTrue(TEXT("Only the loaded bed is recorded"), Bank.RecordedBeds() == TArray<EExpeditionAmbience>({EExpeditionAmbience::WarTable}));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAmbienceShippedTest, "Project.Expedition.Audio.Ambience.ShippedBeds",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FAmbienceShippedTest::RunTest(const FString& Parameters)
{
    const UExpeditionAmbienceSounds* Sounds = LoadObject<UExpeditionAmbienceSounds>(nullptr, ExpeditionAmbience::SoundsPath, nullptr, LOAD_Quiet | LOAD_NoWarn);
    if (!TestNotNull(TEXT("The shipped ambience asset (run import_expedition_ambience.py)"), Sounds)) { return false; }
    FExpeditionAmbienceBank Bank;
    Bank.Load(Sounds);
    const USoundClass* Class = Bank.Class();
    if (TestNotNull(TEXT("The Ambience sound class"), Class)) { TestEqual(TEXT("It is SC_Ambience"), Class->GetName(), FString(TEXT("SC_Ambience"))); }
    TestEqual(TEXT("Every bed but silence is recorded"), Bank.RecordedBeds().Num(), ExpeditionAmbienceCount - 1);
    for (EExpeditionAmbience Bed : Bank.RecordedBeds())
    {
        const FString Name = ExpeditionAmbience::Name(Bed);
        const USoundWave* Wave = Bank.Wave(Bed);
        TestTrue(Name + TEXT(" loops"), Wave->bLooping);
        TestTrue(Name + TEXT(" is a long bed (20 s or more)"), Wave->GetDuration() >= 20.0f);
        TestTrue(Name + TEXT(" lives under /Game/DnDDiceRoll/Audio/Ambience/"), Wave->GetPathName().StartsWith(TEXT("/Game/DnDDiceRoll/Audio/Ambience/")));
        TestTrue(Name + TEXT(" plays through the Ambience class"), Wave->SoundClassObject.Get() == Class);
        TestTrue(Name + TEXT(" plays at a sensible volume"), Bank.Volume(Bed) > 0.0f && Bank.Volume(Bed) <= FExpeditionAmbienceBed::MaxVolume);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAmbienceFollowsBoardTest, "Project.Expedition.Audio.Ambience.FollowsTheBoard",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FAmbienceFollowsBoardTest::RunTest(const FString& Parameters)
{
    using namespace ExpeditionAmbienceTest;
    {
        FHarness Game(*this, TEXT("AmbienceBoard"));
        if (!TestTrue(TEXT("Input, board and HUD are ready"), Game.IsReady())) { return false; }
        AExpeditionBattleView* View = Game.View;
        Game.Frame();
        TestEqual(TEXT("The start screen is silent"), View->AmbienceBed(), EExpeditionAmbience::None);
        TestEqual(TEXT("...with no layer at all"), View->AmbienceFade().Layers().Num(), 0);
        Game.Press(EKeys::Enter);
        if (!TestTrue(TEXT("A new run reaches the route screen"), View->HasRun() && View->Progress().Phase == EExpeditionPhase::Route)) { return false; }
        Game.Settle();
        TestTrue(TEXT("The route screen is the overview"), View->ShowingOverview());
        TestEqual(TEXT("The overview plays the war table"), View->AmbienceBed(), EExpeditionAmbience::WarTable);

        Game.Press(EKeys::Enter);
        if (!TestTrue(TEXT("The first route starts its fight"), View->InCombat())) { return false; }
        const EExpeditionAmbience Fight = ExpectedBed(*View);
        TestTrue(TEXT("A world bed is expected"), Fight == EExpeditionAmbience::Vigil || Fight == EExpeditionAmbience::Root);
        TestEqual(TEXT("The fight plays its world's bed"), View->AmbienceBed(), Fight);
        TestTrue(TEXT("The board change crossfades from the war table without stacking"),
            NoStacking(View->AmbienceFade()) && View->AmbienceFade().Find(Key(EExpeditionAmbience::WarTable)) && View->AmbienceFade().Find(Key(Fight)));
        for (int32 Step = 0; Step < 80; ++Step) { Game.Frame(); TestTrue(TEXT("Never more than two layers"), NoStacking(View->AmbienceFade())); }
        TestTrue(TEXT("After the crossfade only the world's bed is left"),
            View->AmbienceFade().Layers().Num() == 1 && View->AmbienceFade().Layers()[0].Key == Key(Fight) && View->AmbienceFade().Layers()[0].Level == 1.0f);
        TestEqual(TEXT("The test world has no audio device, so nothing is voiced"), View->AmbienceVoiceCount(), 0);

        // Return to menu stops the bed; Resume brings the same bed back, fading in.
        const TArray<uint8> Bytes = Game.CheckpointBytes();
        Game.Press(EKeys::BackSpace);
        Game.Press(EKeys::Enter);
        if (!TestFalse(TEXT("Return to menu puts the run away"), View->HasRun())) { return false; }
        TestEqual(TEXT("Return to menu selects silence"), View->AmbienceBed(), EExpeditionAmbience::None);
        for (int32 Step = 0; Step < 80; ++Step) { Game.Frame(); }
        TestEqual(TEXT("...and the bed has stopped"), View->AmbienceFade().Layers().Num(), 0);
        Game.Press(EKeys::O);
        if (!TestTrue(TEXT("Resume brings the fight back"), View->InCombat())) { return false; }
        TestEqual(TEXT("Resume restores the fight's bed"), View->AmbienceBed(), Fight);
        TestTrue(TEXT("It fades in from silence"), View->AmbienceFade().Layers().Num() == 1 && View->AmbienceFade().Layers()[0].Level < 0.5f);
        TestTrue(TEXT("Ambience changed no checkpoint byte"), Game.CheckpointBytes() == Bytes);

        // Winning keeps the world's bed through the reward; the next route screen goes back to the war table.
        if (!TestTrue(TEXT("The party wins"), Game.WinBattle())) { return false; }
        Game.Settle();
        TestEqual(TEXT("The reward keeps the world's bed"), View->AmbienceBed(), Fight);
    }
    {
        FExpeditionSaveData Saved;
        if (!TestTrue(TEXT("A checkpoint before the finale"), BeforeFinale(Saved))) { return false; }
        FHarness Game(*this, TEXT("AmbienceFinale"), &Saved);
        if (!TestTrue(TEXT("Finale harness ready"), Game.IsReady())) { return false; }
        AExpeditionBattleView* View = Game.View;
        Game.Frame();
        TestEqual(TEXT("The start screen is silent before Resume"), View->AmbienceBed(), EExpeditionAmbience::None);
        Game.Press(EKeys::O);
        if (!TestTrue(TEXT("Resume reaches the route screen"), View->HasRun() && View->Progress().Phase == EExpeditionPhase::Route)) { return false; }
        TestEqual(TEXT("Resume on the route screen restores the war table"), View->AmbienceBed(), EExpeditionAmbience::WarTable);
        View->ChooseRoute(0);
        if (!TestTrue(TEXT("The finale starts"), View->InCombat())) { return false; }
        TestEqual(TEXT("The finale plays the bed of the kit it drew"), View->AmbienceBed(), ExpectedBed(*View));
        if (View->UsesVaultKit()) { TestEqual(TEXT("The shipped Vault kit plays the Vault"), View->AmbienceBed(), EExpeditionAmbience::Vault); }
        else { AddError(TEXT("The shipped theme should draw the finale with the Vault kit")); }
    }
    return !HasAnyErrors();
}
#endif
