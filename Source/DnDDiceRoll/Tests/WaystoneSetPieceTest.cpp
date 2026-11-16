#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Persistence/ExpeditionSave.h"
#include "Presentation/ExpeditionBattleView.h"
#include "Presentation/WaystoneSetPiece.h"
#include "RenderingThread.h"
#include "UObject/GarbageCollection.h"
#include "ExpeditionTestSupport.h"

/**
 * The finale's Waystone set piece (#250): its pure timeline and flight plan, and on a real board that a won finale plays it once and settles, that
 * skip, Instant and Resume all converge on the same settled frame, that a missing mesh leaves today's ending, that an intact or settled set piece
 * writes nothing per frame, and that none of it changes the rules, the RNG or a checkpoint byte (ADR-0001).
 */
namespace
{
using namespace WaystoneSetPiece;

UWorld* MakeSetPieceWorld()
{
    const UWorld::InitializationValues Values = UWorld::InitializationValues()
        .AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false);
    return UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
}

void DestroySetPieceWorld(UWorld* World)
{
    World->DestroyWorld(false);
    FlushRenderingCommands();
    CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
}

/** A checkpoint one party command short of winning the finale: the scripted player's run, replayed to its last command. */
struct FLastBlow
{
    FString Checkpoint;
    FCombatCommand Command;
    FExpeditionSnapshot Won;
    int32 Seed = 0;
};

bool FindLastBlow(const FString& Directory, FLastBlow& Out)
{
    for (int32 Seed = 1; Seed < 200; ++Seed)
    {
        FExpeditionRun Scripted;
        if (!ExpeditionTestSupport::ReachFinale(Scripted, Seed)) { continue; }
        FExpeditionRun Start = Scripted;
        TArray<FCombatCommand> Commands;
        if (!ExpeditionTestSupport::FinishCombat(Scripted, true, &Commands) || Scripted.Progress().Phase != EExpeditionPhase::Victory || Commands.IsEmpty()) { continue; }
        // Replay every command but the last, with the enemy turns between them exactly as the scripted player took them.
        FExpeditionRun Run = Start;
        for (int32 Index = 0; Index + 1 < Commands.Num(); ++Index)
        {
            while (Run.Progress().Phase == EExpeditionPhase::Combat && Run.Battle().Find(Run.Battle().ActiveId()) && Run.Battle().Find(Run.Battle().ActiveId())->Side == ECombatSide::Enemy) { Run.PlayEnemyTurn(); }
            Run.Submit(Commands[Index]);
        }
        while (Run.Progress().Phase == EExpeditionPhase::Combat && Run.Battle().Find(Run.Battle().ActiveId()) && Run.Battle().Find(Run.Battle().ActiveId())->Side == ECombatSide::Enemy) { Run.PlayEnemyTurn(); }
        if (Run.Progress().Phase != EExpeditionPhase::Combat || Run.Battle().ActiveId() != Commands.Last().ActorId) { continue; }
        FExpeditionRun Won = Run;
        if (!Won.Submit(Commands.Last()).bAccepted || Won.Progress().Phase != EExpeditionPhase::Victory) { continue; }
        FExpeditionSaveData Data;
        Data.Current = Run.Snapshot();
        FString Error;
        Out.Checkpoint = FPaths::Combine(Directory, TEXT("last-blow.json"));
        if (!FExpeditionSave::Write(Out.Checkpoint, Data, Error)) { return false; }
        Out.Command = Commands.Last();
        Out.Won = Won.Snapshot();
        Out.Seed = Seed;
        return true;
    }
    return false;
}

enum class ELook : uint8 { Themed, Primitives, MissingMesh };

/** A board resumed on a copy of a checkpoint, in a look. */
AExpeditionBattleView* OpenBoard(FAutomationTestBase& Test, UWorld* World, const FString& Source, const FString& Copy, ELook Look)
{
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(Copy), true);
    IFileManager::Get().Copy(*Copy, *Source);
    AExpeditionBattleView* View = World->SpawnActor<AExpeditionBattleView>();
    if (!Test.TestNotNull(TEXT("Battle view"), View)) { return nullptr; }
    View->ConfigureStorage(Copy);
    View->DispatchBeginPlay();
    if (Look == ELook::Primitives) { View->UsePrimitives(); }
    if (Look == ELook::MissingMesh) { View->OverrideSetPieceMeshes({FSoftObjectPath(TEXT("/Game/DnDDiceRoll/Theme/SetPiece/SM_Missing_Seal.SM_Missing_Seal")), WaystoneSetPiece::ThemedMeshPaths()[1], WaystoneSetPiece::ThemedMeshPaths()[2]}); }
    View->Resume();
    View->Tick(1.0f / 30.0f);
    return View;
}

/** Ticks at 30 fps until playback and the set piece are done (or a bound passes); true when it went idle. */
bool TickUntilIdle(AExpeditionBattleView& View, TFunctionRef<void()> Each, float Limit = 40.0f)
{
    for (float Elapsed = 0.0f; Elapsed < Limit; Elapsed += 1.0f / 30.0f)
    {
        if (!View.IsPresenting()) { return true; }
        View.Tick(1.0f / 30.0f);
        Each();
    }
    return !View.IsPresenting();
}

FString ReadCheckpoint(const FString& Path)
{
    FString Text;
    FFileHelper::LoadFileToString(Text, *Path);
    return Text;
}

bool SamePieces(const TArray<FTransform>& A, const TArray<FTransform>& B)
{
    if (A.Num() != B.Num()) { return false; }
    for (int32 Index = 0; Index < A.Num(); ++Index) { if (!A[Index].Equals(B[Index], 0.01)) { return false; } }
    return true;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWaystoneTimelineTest, "Project.Expedition.Presentation.WaystoneSetPieceTimelineAndPlan",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FWaystoneTimelineTest::RunTest(const FString& Parameters)
{
    // The timeline starts where the board is and ends exactly where the settled state is, so a played and a converged ending match.
    TestEqual(TEXT("Flare at rest before"), FlareGain(0.0f), 1.0f);
    TestEqual(TEXT("Flare at rest after"), FlareGain(DurationSeconds), 1.0f);
    TestTrue(TEXT("The flare builds to the shatter"), FlareGain(ShatterSeconds - 0.01f) > 1.5f && FlareGain(ShatterSeconds) > FlareGain(ShatterSeconds - 0.01f));
    TestEqual(TEXT("No lift before"), LiftAlpha(0.0f), 0.0f);
    TestEqual(TEXT("No lift at the shatter"), LiftAlpha(ShatterSeconds), 0.0f);
    TestEqual(TEXT("Lifted at the end"), LiftAlpha(DurationSeconds), 1.0f);
    TestEqual(TEXT("No extra spin at the start"), ExtraSpin(0.0f), 0.0f);
    TestEqual(TEXT("Whole extra turns at the end"), ExtraSpin(DurationSeconds), ExtraSpinDegrees);
    TestEqual(TEXT("Whole turns"), FMath::Fmod(ExtraSpinDegrees, 360.0f), 0.0f);
    TestEqual(TEXT("The board camera at the start"), CameraWeight(0.0f), 0.0f);
    TestEqual(TEXT("The shot at the end"), CameraWeight(DurationSeconds), 1.0f);
    TestEqual(TEXT("The held shot at the end"), ShotBlend(DurationSeconds), 1.0f);
    TArray<EWaystoneCue> Cues;
    CuesBetween(0.0f, DurationSeconds, Cues);
    TestTrue(TEXT("Shatter, Lift, Settled in order (Flare is Play's)"), Cues == TArray<EWaystoneCue>({EWaystoneCue::Shatter, EWaystoneCue::Lift, EWaystoneCue::Settled}));
    Cues.Reset();
    CuesBetween(0.0f, 0.5f, Cues);
    CuesBetween(0.5f, ShatterSeconds, Cues);
    CuesBetween(ShatterSeconds, ShatterSeconds + 0.01f, Cues);
    TestEqual(TEXT("A cue on a step boundary is announced once"), Cues.Num(), 1);

    for (const bool bApse : {true, false})
    {
        FSite Site;
        Site.bApse = bApse;
        Site.ArtifactHeight = 190.0f;
        if (!bApse) { Site.PedestalTop = 123.0f; Site.PedestalRadius = 48.0f; Site.BoardMin = FVector2D(-2000, -1500); Site.BoardMax = FVector2D(-30, -60); Site.BoardTop = 20.0f; }
        const TCHAR* Name = bApse ? TEXT("Apse") : TEXT("Pedestal");
        const TArray<FFlight> Flights = Plan(Site);
        TestEqual(FString::Printf(TEXT("%s: every piece is planned"), Name), Flights.Num(), Pieces);
        const TArray<FFlight> Again = Plan(Site);
        bool bSame = Again.Num() == Flights.Num();
        for (int32 Index = 0; bSame && Index < Flights.Num(); ++Index) { bSame = Flights[Index].Rest.Equals(Again[Index].Rest, 0.0) && Flights[Index].Start.Equals(Again[Index].Start, 0.0); }
        TestTrue(FString::Printf(TEXT("%s: the plan is deterministic"), Name), bSame);
        for (int32 Index = 0; Index < Flights.Num(); ++Index)
        {
            const FFlight& Flight = Flights[Index];
            TestTrue(FString::Printf(TEXT("%s piece %d lands before the set piece ends"), Name, Index), LandedAt(Flight) <= DurationSeconds);
            TestTrue(FString::Printf(TEXT("%s piece %d hangs until the shatter"), Name, Index), FlightAt(Flight, ShatterSeconds).Equals(Flight.Start, 0.0));
            TestTrue(FString::Printf(TEXT("%s piece %d lies exactly at rest once landed"), Name, Index), FlightAt(Flight, LandedAt(Flight)).Equals(Flight.Rest, 0.0) && FlightAt(Flight, DurationSeconds).Equals(Flight.Rest, 0.0));
            const FVector Rest = Flight.Rest.GetLocation();
            const float Floor = FloorAt(Site, FVector2D(Rest));
            TestTrue(FString::Printf(TEXT("%s piece %d rests just above its floor"), Name, Index), Rest.Z > Floor && Rest.Z < Floor + 10.0f);
            // In flight it never passes under the floor it is flying over by more than its own size.
            for (float At = ShatterSeconds; At <= LandedAt(Flight); At += 0.02f)
            {
                const FVector Point = FlightAt(Flight, At).GetLocation();
                if (Point.Z < FloorAt(Site, FVector2D(Point)) - 2.0f) { AddError(FString::Printf(TEXT("%s piece %d dips under the floor at %.2f s"), Name, Index, At)); break; }
            }
            if (bApse)
            {
                // Clear of the monolith (150 cm behind, 95 cm radius), the kneeling statues (190 cm each side, 40 cm behind) and the brazier ring (nothing nearer the board than 150 cm).
                TestTrue(FString::Printf(TEXT("Apse piece %d clears the monolith"), Index), FVector2D::Distance(FVector2D(Rest), FVector2D(0, 150)) > 110.0);
                TestTrue(FString::Printf(TEXT("Apse piece %d clears the statues"), Index), FVector2D::Distance(FVector2D(Rest), FVector2D(190, 40)) > 55.0 && FVector2D::Distance(FVector2D(Rest), FVector2D(-190, 40)) > 55.0);
                TestTrue(FString::Printf(TEXT("Apse piece %d stays behind the brazier ring"), Index), Rest.Y >= -150.0);
            }
            else { TestTrue(FString::Printf(TEXT("Pedestal piece %d lands outside the pedestal or on its top"), Index), FVector2D(Rest).Size() > Site.PedestalRadius || Rest.Z > Site.PedestalTop); }
        }
        // Both halves of a shard hang at one transform: the intact seal.
        for (int32 Shard = 0; Shard < Shards; ++Shard)
        { TestTrue(FString::Printf(TEXT("%s shard %d halves meet"), Name, Shard), Flights[2 * Shard].Start.Equals(Flights[2 * Shard + 1].Start, 0.0)); }
    }
    // The dais steps as authored in build_vault_r002.py.
    FSite Apse;
    TestEqual(TEXT("Top step"), FloorAt(Apse, FVector2D(0, 0)), 60.0f);
    TestEqual(TEXT("Second step"), FloorAt(Apse, FVector2D(260, -40)), 40.0f);
    TestEqual(TEXT("First step"), FloorAt(Apse, FVector2D(0, -90)), 20.0f);
    TestEqual(TEXT("Floor"), FloorAt(Apse, FVector2D(0, -140)), 0.0f);
    TestEqual(TEXT("Three themed meshes"), ThemedMeshPaths().Num(), 3);
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWaystoneConvergenceTest, "Project.Expedition.Presentation.WaystoneSetPieceConvergesUnderSkipInstantAndResume",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FWaystoneConvergenceTest::RunTest(const FString& Parameters)
{
    const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation/WaystoneSetPiece"), FGuid::NewGuid().ToString());
    FLastBlow Blow;
    if (!TestTrue(TEXT("A seed reaches the finale and the scripted player wins it with a party command"), FindLastBlow(Directory, Blow))) { return false; }
    AddInfo(FString::Printf(TEXT("Seed %d; the last blow is command type %d by %d"), Blow.Seed, static_cast<int32>(Blow.Command.Type), Blow.Command.ActorId));
    UWorld* World = MakeSetPieceWorld();
    if (!TestNotNull(TEXT("Transient world"), World)) { return false; }

    for (const ELook Look : {ELook::Themed, ELook::Primitives})
    {
        const TCHAR* Name = Look == ELook::Themed ? TEXT("Themed") : TEXT("Primitives");
        const FString Base = FPaths::Combine(Directory, Name);

        // Played: the blow lands, playback shows it, then the set piece plays once through its cues and settles.
        AExpeditionBattleView* Played = OpenBoard(*this, World, Blow.Checkpoint, FPaths::Combine(Base, TEXT("played/current.json")), Look);
        if (!Played) { continue; }
        if (Look == ELook::Themed && !Played->UsesVaultKit()) { AddError(TEXT("The shipped theme has no Vault kit here; run the theme import")); continue; }
        const UWaystoneSetPiece* Piece = Played->SetPiece();
        if (!TestTrue(FString::Printf(TEXT("%s: the finale board has the set piece (themed: run the theme import for Theme/SetPiece)"), Name), Piece && Piece->IsBound())) { continue; }
        TestTrue(FString::Printf(TEXT("%s: the seal is intact during the fight"), Name), Piece->State() == EWaystoneSetPieceState::Intact);
        TestTrue(FString::Printf(TEXT("%s: the set piece stands at the apse on the Vault kit, at the pedestal on the primitive look"), Name), Piece->IsApse() == (Look == ELook::Themed));
        TestEqual(FString::Printf(TEXT("%s: three instanced groups"), Name), Piece->ComponentCount(), 3);
        // Per-frame cost while the finale is fought: nothing is rewritten.
        const int32 IntactWrites = Piece->PieceWrites();
        // The artifact's own colour while the finale is fought: the flare scales it and every ending must come back to exactly it (#260 review).
        const FLinearColor IntactColour = Played->ArtifactShownColour();
        TestTrue(FString::Printf(TEXT("%s: the artifact is drawn"), Name), IntactColour.A > 0.0f || IntactColour.GetMax() > 0.0f);
        // Within the 0.002 a loose part's tint skips (FExpeditionPart::SetColor); a compounding flare is orders of magnitude off.
        auto SameColour = [](const FLinearColor& A, const FLinearColor& B) { return A.Equals(B, 0.0025f); };
        for (int32 Frame = 0; Frame < 60; ++Frame) { Played->Tick(1.0f / 30.0f); }
        TestEqual(FString::Printf(TEXT("%s: an intact seal writes nothing per frame"), Name), Piece->PieceWrites(), IntactWrites);
        TArray<EWaystoneCue> Cues;
        Played->OnSetPieceCue([&Cues](EWaystoneCue Cue) { Cues.Add(Cue); });
        if (!TestTrue(FString::Printf(TEXT("%s: the last blow is accepted"), Name), Played->Execute(Blow.Command))) { continue; }
        TestTrue(FString::Printf(TEXT("%s: the run is won"), Name), Played->Progress().Phase == EExpeditionPhase::Victory);
        TestTrue(FString::Printf(TEXT("%s: the rules are the rules-only run's"), Name), ExpeditionTestSupport::SameRun(Played->CurrentRun().Snapshot(), Blow.Won));
        TestTrue(FString::Printf(TEXT("%s: the ending waits while the victory plays"), Name), Played->IsPresenting());
        bool bSawPlaying = false, bSawShot = false, bBounded = true;
        const float Brightest = IntactColour.GetMax() * FMath::Sqrt(FlareGain(ShatterSeconds)) + 1e-3f;
        float LastTime = -1.0f;
        bool bMonotonic = true;
        TestTrue(FString::Printf(TEXT("%s: playback and the set piece finish"), Name), TickUntilIdle(*Played, [&]()
        {
            if (Piece->IsPlaying())
            {
                bSawPlaying = true;
                bMonotonic &= Piece->Time() >= LastTime;
                LastTime = Piece->Time();
                bSawShot |= Piece->CameraWeight() > 0.5f && Piece->CameraWeight() < 1.0f;
                bBounded &= Played->ArtifactShownColour().GetMax() <= Brightest;
            }
        }));
        TestTrue(FString::Printf(TEXT("%s: it played"), Name), bSawPlaying);
        TestTrue(FString::Printf(TEXT("%s: the flare never brightens the artifact past its peak (no compounding)"), Name), bBounded);
        TestTrue(FString::Printf(TEXT("%s: its time only moves forward"), Name), bMonotonic);
        TestTrue(FString::Printf(TEXT("%s: the camera eased into the shot"), Name), bSawShot);
        TestTrue(FString::Printf(TEXT("%s: it settled"), Name), Piece->State() == EWaystoneSetPieceState::Settled);
        TestTrue(FString::Printf(TEXT("%s: Flare, Shatter, Lift, Settled once each, in order"), Name),
            Cues == TArray<EWaystoneCue>({EWaystoneCue::Flare, EWaystoneCue::Shatter, EWaystoneCue::Lift, EWaystoneCue::Settled}));
        const int32 SettledWrites = Piece->PieceWrites();
        for (int32 Frame = 0; Frame < 60; ++Frame) { Played->Tick(1.0f / 30.0f); }
        TestEqual(FString::Printf(TEXT("%s: a settled set piece writes nothing per frame"), Name), Piece->PieceWrites(), SettledWrites);
        TestEqual(FString::Printf(TEXT("%s: no cue after it settles"), Name), Cues.Num(), 4);
        // #246: the flare and the shatter sound once each when played.
        TestEqual(FString::Printf(TEXT("%s: the flare sounds once"), Name), Played->CueRequests(EExpeditionCue::WaystoneFlare), 1u);
        TestEqual(FString::Printf(TEXT("%s: the shatter sounds once"), Name), Played->CueRequests(EExpeditionCue::WaystoneShatter), 1u);
        const TArray<FTransform> Settled = Piece->PieceTransforms();
        const FTransform SettledCamera = Played->CameraTransform();
        const FLinearColor SettledColour = Played->ArtifactShownColour();
        TestTrue(FString::Printf(TEXT("%s: the settled artifact wears its own colour again"), Name), SameColour(SettledColour, IntactColour));
        const FString PlayedFile = ReadCheckpoint(FPaths::Combine(Base, TEXT("played/current.json")));
        TestFalse(FString::Printf(TEXT("%s: the won run was checkpointed"), Name), PlayedFile.IsEmpty());

        // Resume into the won checkpoint: the settled state at once, no cue, nothing to wait for.
        {
            AExpeditionBattleView* Resumed = OpenBoard(*this, World, FPaths::Combine(Base, TEXT("played/current.json")), FPaths::Combine(Base, TEXT("resumed/current.json")), Look);
            const UWaystoneSetPiece* Again = Resumed ? Resumed->SetPiece() : nullptr;
            if (TestTrue(FString::Printf(TEXT("%s: a resumed victory has the set piece"), Name), Again && Again->IsBound()))
            {
                TestTrue(FString::Printf(TEXT("%s: a resumed victory is settled"), Name), Again->State() == EWaystoneSetPieceState::Settled);
                TestFalse(FString::Printf(TEXT("%s: a resumed victory shows the ending at once"), Name), Resumed->IsPresenting());
                TestTrue(FString::Printf(TEXT("%s: the resumed pieces lie where the played ones did"), Name), SamePieces(Again->PieceTransforms(), Settled));
                TestTrue(FString::Printf(TEXT("%s: the resumed camera holds the same shot"), Name), Resumed->CameraTransform().Equals(SettledCamera, 0.5));
                TestTrue(FString::Printf(TEXT("%s: the resumed artifact has the played colour"), Name), SameColour(Resumed->ArtifactShownColour(), SettledColour));
                TestTrue(FString::Printf(TEXT("%s: Resume writes nothing"), Name), ReadCheckpoint(FPaths::Combine(Base, TEXT("resumed/current.json"))) == PlayedFile);
                Resumed->Tick(1.0f / 30.0f);
                TestEqual(FString::Printf(TEXT("%s: a resumed victory sounds no set-piece cue"), Name), Resumed->CueRequests(EExpeditionCue::WaystoneFlare) + Resumed->CueRequests(EExpeditionCue::WaystoneShatter), 0u);
            }
        }

        // Skip mid-way: it settles at once and says nothing more.
        {
            AExpeditionBattleView* Skipped = OpenBoard(*this, World, Blow.Checkpoint, FPaths::Combine(Base, TEXT("skipped/current.json")), Look);
            const UWaystoneSetPiece* Mid = Skipped ? Skipped->SetPiece() : nullptr;
            TArray<EWaystoneCue> Heard;
            if (Mid && Mid->IsBound())
            {
                Skipped->OnSetPieceCue([&Heard](EWaystoneCue Cue) { Heard.Add(Cue); });
                Skipped->Execute(Blow.Command);
                for (int32 Frame = 0; Frame < 3000 && !(Mid->IsPlaying() && Mid->Time() > ShatterSeconds + 0.2f); ++Frame) { Skipped->Tick(1.0f / 30.0f); }
                TestTrue(FString::Printf(TEXT("%s: the set piece was past the shatter"), Name), Mid->IsPlaying() && Mid->Time() > ShatterSeconds);
                Skipped->SkipPresentation();
                TestTrue(FString::Printf(TEXT("%s: skip settles it"), Name), Mid->State() == EWaystoneSetPieceState::Settled);
                TestFalse(FString::Printf(TEXT("%s: skip shows the ending"), Name), Skipped->IsPresenting());
                Skipped->Tick(1.0f / 30.0f);
                TestTrue(FString::Printf(TEXT("%s: skipped pieces lie where the played ones did"), Name), SamePieces(Mid->PieceTransforms(), Settled));
                TestTrue(FString::Printf(TEXT("%s: the skipped camera holds the same shot"), Name), Skipped->CameraTransform().Equals(SettledCamera, 0.5));
                TestTrue(FString::Printf(TEXT("%s: the skipped artifact has the played colour"), Name), SameColour(Skipped->ArtifactShownColour(), SettledColour));
                TestTrue(FString::Printf(TEXT("%s: skip stops the cues"), Name), Heard == TArray<EWaystoneCue>({EWaystoneCue::Flare, EWaystoneCue::Shatter}));
                TestTrue(FString::Printf(TEXT("%s: the skipped set piece sounded its flare and shatter once each"), Name),
                    Skipped->CueRequests(EExpeditionCue::WaystoneFlare) == 1u && Skipped->CueRequests(EExpeditionCue::WaystoneShatter) == 1u);
                TestTrue(FString::Printf(TEXT("%s: skipping changes no checkpoint byte"), Name), ReadCheckpoint(FPaths::Combine(Base, TEXT("skipped/current.json"))) == PlayedFile);
            }
            else { AddError(FString::Printf(TEXT("%s: the skip board has no set piece"), Name)); }
        }

        // Skip (Finish) while the blow itself still plays: the whole victory converges, the set piece included, silently.
        {
            AExpeditionBattleView* Early = OpenBoard(*this, World, Blow.Checkpoint, FPaths::Combine(Base, TEXT("early/current.json")), Look);
            const UWaystoneSetPiece* Set = Early ? Early->SetPiece() : nullptr;
            TArray<EWaystoneCue> Heard;
            if (Set && Set->IsBound())
            {
                Early->OnSetPieceCue([&Heard](EWaystoneCue Cue) { Heard.Add(Cue); });
                Early->Execute(Blow.Command);
                Early->SkipPresentation();
                TestTrue(FString::Printf(TEXT("%s: Finish settles the set piece too"), Name), Set->State() == EWaystoneSetPieceState::Settled);
                TestFalse(FString::Printf(TEXT("%s: nothing is left to present"), Name), Early->IsPresenting());
                Early->Tick(1.0f / 30.0f);
                TestTrue(FString::Printf(TEXT("%s: converged pieces lie where the played ones did"), Name), SamePieces(Set->PieceTransforms(), Settled));
                TestTrue(FString::Printf(TEXT("%s: the converged artifact has the played colour"), Name), SameColour(Early->ArtifactShownColour(), SettledColour));
                TestEqual(FString::Printf(TEXT("%s: converging is silent"), Name), Heard.Num(), 0);
                TestEqual(FString::Printf(TEXT("%s: converging sounds no set-piece cue"), Name), Early->CueRequests(EExpeditionCue::WaystoneFlare) + Early->CueRequests(EExpeditionCue::WaystoneShatter), 0u);
            }
        }

        // Instant: the victory and the set piece converge with no wait and no cue.
        {
            AExpeditionBattleView* Instant = OpenBoard(*this, World, Blow.Checkpoint, FPaths::Combine(Base, TEXT("instant/current.json")), Look);
            const UWaystoneSetPiece* Set = Instant ? Instant->SetPiece() : nullptr;
            TArray<EWaystoneCue> Heard;
            if (Set && Set->IsBound())
            {
                Instant->OnSetPieceCue([&Heard](EWaystoneCue Cue) { Heard.Add(Cue); });
                Instant->CycleSpeed(); Instant->CycleSpeed();
                TestTrue(FString::Printf(TEXT("%s: Instant speed"), Name), Instant->Speed() == EPlaybackSpeed::Instant);
                Instant->Execute(Blow.Command);
                Instant->Tick(1.0f / 30.0f);
                TestTrue(FString::Printf(TEXT("%s: Instant settles it"), Name), Set->State() == EWaystoneSetPieceState::Settled);
                TestFalse(FString::Printf(TEXT("%s: Instant shows the ending"), Name), Instant->IsPresenting());
                TestTrue(FString::Printf(TEXT("%s: Instant pieces lie where the played ones did"), Name), SamePieces(Set->PieceTransforms(), Settled));
                TestTrue(FString::Printf(TEXT("%s: the Instant artifact has the played colour"), Name), SameColour(Instant->ArtifactShownColour(), SettledColour));
                TestEqual(FString::Printf(TEXT("%s: Instant is silent"), Name), Heard.Num(), 0);
                TestEqual(FString::Printf(TEXT("%s: Instant sounds no set-piece cue"), Name), Instant->CueRequests(EExpeditionCue::WaystoneFlare) + Instant->CueRequests(EExpeditionCue::WaystoneShatter), 0u);
                TestTrue(FString::Printf(TEXT("%s: Instant changes no rule"), Name), ExpeditionTestSupport::SameRun(Instant->CurrentRun().Snapshot(), Blow.Won));
                TestTrue(FString::Printf(TEXT("%s: Instant changes no checkpoint byte"), Name), ReadCheckpoint(FPaths::Combine(Base, TEXT("instant/current.json"))) == PlayedFile);
            }
        }

        // Capture support: a held moment is not owed playback, and letting it go settles.
        if (Played->PreviewSetPiece(ShatterSeconds + 0.3f))
        {
            TestTrue(FString::Printf(TEXT("%s: a held moment is held"), Name), Played->HoldsSetPiece() && Piece->IsPlaying());
            Played->Tick(1.0f / 30.0f);
            TestTrue(FString::Printf(TEXT("%s: a held moment does not advance"), Name), FMath::IsNearlyEqual(Piece->Time(), ShatterSeconds + 0.3f));
            // A held moment's colour is a function of its time alone: the same after other moments were held first.
            const FLinearColor Held = Played->ArtifactShownColour();
            TestTrue(FString::Printf(TEXT("%s: a held moment's artifact is its colour times the flare"), Name), SameColour(Held, IntactColour * FMath::Sqrt(FlareGain(ShatterSeconds + 0.3f))));
            for (const float Other : {0.2f, ShatterSeconds, 3.0f})
            {
                Played->PreviewSetPiece(Other);
                for (int32 Frame = 0; Frame < 4; ++Frame) { Played->Tick(1.0f / 30.0f); }
            }
            Played->PreviewSetPiece(ShatterSeconds + 0.3f);
            Played->Tick(1.0f / 30.0f);
            TestTrue(FString::Printf(TEXT("%s: held again after other moments, the artifact has the same colour"), Name), SameColour(Played->ArtifactShownColour(), Held));
            Played->ClearSetPiecePreview();
            Played->Tick(1.0f / 30.0f);
            TestTrue(FString::Printf(TEXT("%s: released, it is settled again"), Name), Piece->State() == EWaystoneSetPieceState::Settled && SamePieces(Piece->PieceTransforms(), Settled));
            TestTrue(FString::Printf(TEXT("%s: released, the artifact has the played colour"), Name), SameColour(Played->ArtifactShownColour(), SettledColour));
        }
        else { AddError(FString::Printf(TEXT("%s: a won finale takes a set-piece preview"), Name)); }
    }

    // An ordinary fight (the first road) never builds or plays the set piece, on either look.
    for (const ELook Look : {ELook::Themed, ELook::Primitives})
    {
        AExpeditionBattleView* Road = World->SpawnActor<AExpeditionBattleView>();
        if (!TestNotNull(TEXT("Road view"), Road)) { continue; }
        Road->ConfigureStorage(FPaths::Combine(Directory, Look == ELook::Themed ? TEXT("road-themed") : TEXT("road-primitives"), TEXT("current.json")));
        Road->DispatchBeginPlay();
        if (Look == ELook::Primitives) { Road->UsePrimitives(); }
        Road->NewRun();
        Road->ChooseRoute(0);
        Road->HoldEnemyTurns(true);
        Road->Tick(1.0f / 30.0f);
        TestTrue(TEXT("An ordinary fight is in combat"), Road->InCombat());
        TestFalse(TEXT("An ordinary fight has no set piece"), Road->SetPiece() && Road->SetPiece()->IsBound());
        TestFalse(TEXT("An ordinary fight owes no set piece"), Road->SetPieceBusy());
    }

    // A missing set-piece mesh: today's ending (no set piece, no wait, no camera move), and still the same rules and bytes.
    {
        const FString Base = FPaths::Combine(Directory, TEXT("Missing"));
        AExpeditionBattleView* Plain = OpenBoard(*this, World, Blow.Checkpoint, FPaths::Combine(Base, TEXT("current.json")), ELook::MissingMesh);
        if (Plain && Plain->UsesVaultKit())
        {
            TestFalse(TEXT("Missing mesh: no set piece"), Plain->SetPiece() && Plain->SetPiece()->IsBound());
            Plain->Execute(Blow.Command);
            TestTrue(TEXT("Missing mesh: the victory still plays"), TickUntilIdle(*Plain, []() {}));
            TestFalse(TEXT("Missing mesh: the ending is not held"), Plain->SetPieceBusy());
            TestTrue(TEXT("Missing mesh: the same rules"), ExpeditionTestSupport::SameRun(Plain->CurrentRun().Snapshot(), Blow.Won));
            const FString Themed = ReadCheckpoint(FPaths::Combine(Directory, TEXT("Themed/played/current.json")));
            TestTrue(TEXT("Missing mesh: the same checkpoint bytes as the played set piece"), ReadCheckpoint(FPaths::Combine(Base, TEXT("current.json"))) == Themed);
        }
        else { AddError(TEXT("The missing-mesh board needs the Vault kit (run the theme import)")); }
    }
    DestroySetPieceWorld(World);
    IFileManager::Get().DeleteDirectory(*Directory, false, true);
    return !HasAnyErrors();
}
#endif
