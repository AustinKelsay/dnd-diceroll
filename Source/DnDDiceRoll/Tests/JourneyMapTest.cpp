#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Camera/CameraTypes.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Persistence/ExpeditionSave.h"
#include "Presentation/DiceRollTheme.h"
#include "Presentation/ExpeditionBattleView.h"
#include "Presentation/JourneyLayout.h"
#include "RenderingThread.h"
#include "SceneView.h"
#include "UObject/GarbageCollection.h"
#include "UObject/StrongObjectPtr.h"
#include "ExpeditionInputHarness.h"
#include "ExpeditionTestSupport.h"

/**
 * The war-table journey map (#248, spec #239): what the map shows for the journey fixtures (a fresh run, mid-run, the finale's fork, a victory, a defeat), that the
 * scene is built from that and nothing else, that a missing piece falls back to today's overview as a whole, that the raised dioramas stay clear of the HUD at the
 * four supported sizes, that the layout projects exactly like the engine's camera, and that a click on an offered diorama is the same route command as its card.
 */
namespace JourneyMapTest
{
using namespace JourneyLayout;
using ExpeditionTestSupport::FinishCombat;

/** Plays the scripted player (offer Choice(stage), the fight, the first conscious survivor's reward, the camp) until Done; false if a step fails first. */
bool PlayUntil(FExpeditionRun& Run, TFunctionRef<int32(int32)> Choice, TFunctionRef<bool(const FExpeditionRun&)> Done)
{
    FString Error;
    for (int32 Guard = 0; Guard < 64; ++Guard)
    {
        if (Done(Run)) { return true; }
        bool bOk = false;
        switch (Run.Progress().Phase)
        {
        case EExpeditionPhase::Route: bOk = Run.ChooseRoute(Run.Progress().Offers[Choice(Run.Progress().EncountersCompleted)].Id, Error); break;
        case EExpeditionPhase::Combat: bOk = FinishCombat(Run); break;
        case EExpeditionPhase::Reward:
            for (const FCombatantState& Unit : Run.Battle().Snapshot().Encounter.Combatants)
            { if (Unit.Side == ECombatSide::Party && Unit.Life == ECombatLife::Conscious) { bOk = Run.ClaimReward(Unit.Id, Error); break; } }
            break;
        case EExpeditionPhase::Recovery: bOk = Run.Recover(true, Error); break;
        default: break;
        }
        if (!bOk) { return false; }
    }
    return Done(Run);
}

int32 Alternating(int32 Stage) { return Stage % 2; }
bool AtRoute(const FExpeditionRun& Run, int32 Stage) { return Run.Progress().Phase == EExpeditionPhase::Route && Run.Progress().EncountersCompleted == Stage; }

/** The five journey fixtures, each the first seed from 42 on whose scripted run (alternating roads) reaches it. */
struct FFixture { FString Name; FExpeditionRun Run; };
TArray<FFixture> Fixtures()
{
    TArray<FFixture> Out;
    FString Error;
    auto Reach = [&](const TCHAR* Name, TFunctionRef<bool(FExpeditionRun&)> Make)
    {
        for (int32 Seed = 42; Seed < 240; ++Seed)
        {
            FExpeditionRun Run;
            if (!Run.NewRun(Seed, Error, FString::Printf(TEXT("journey-map-%d"), Seed)) || !Make(Run)) { continue; }
            Out.Add({Name, Run});
            return;
        }
    };
    Reach(TEXT("fresh"), [](FExpeditionRun& Run) { return AtRoute(Run, 0); });
    Reach(TEXT("mid"), [](FExpeditionRun& Run) { return PlayUntil(Run, Alternating, [](const FExpeditionRun& R) { return AtRoute(R, 2); }); });
    Reach(TEXT("finale"), [](FExpeditionRun& Run) { return PlayUntil(Run, Alternating, [](const FExpeditionRun& R) { return AtRoute(R, 4); }); });
    Reach(TEXT("victory"), [](FExpeditionRun& Run) { return PlayUntil(Run, Alternating, [](const FExpeditionRun& R) { return R.Progress().Phase == EExpeditionPhase::Victory; }); });
    Reach(TEXT("defeat"), [&Error](FExpeditionRun& Run)
    {
        // Two fights won, then a passive party on the third stage's dangerous road.
        if (!PlayUntil(Run, Alternating, [](const FExpeditionRun& R) { return AtRoute(R, 2); })) { return false; }
        return Run.ChooseRoute(Run.Progress().Offers[1].Id, Error) && (FinishCombat(Run, false), Run.Progress().Phase == EExpeditionPhase::Defeat);
    });
    return Out;
}

EDiorama DioramaFor(EExpeditionWorld World) { EDiorama Kind = EDiorama::Root; DioramaOf(World, Kind); return Kind; }

FString Describe(const FJourneyLook& Look)
{
    FString Out;
    for (const FStageLook& Stage : Look.Stages)
    {
        const TCHAR* Mark = Stage.Mark == EStageMark::Cleared ? TEXT("C") : Stage.Mark == EStageMark::Lost ? TEXT("L") : Stage.Mark == EStageMark::Current ? TEXT("N") : Stage.Mark == EStageMark::Fogged ? TEXT("F") : TEXT("U");
        Out += FString::Printf(TEXT("%s%s%s%s%s%s "), Mark, Stage.Emblem == EExpeditionWorld::Unknown ? TEXT("") : WorldWord(Stage.Emblem), Stage.bPennant ? TEXT("+p") : TEXT(""),
            Stage.bBroken ? TEXT("+x") : TEXT(""), Stage.bFog ? TEXT("+f") : TEXT(""), Stage.bWalkedIn ? TEXT("") : TEXT("~"));
    }
    for (const FRaisedLook& Raised : Look.Raised) { Out += FString::Printf(TEXT("[%d:%d] "), Raised.Offer, static_cast<int32>(Raised.Kind)); }
    return Out + (Look.bGoalReached ? TEXT("goal") : TEXT("-"));
}

const UDiceRollTheme* ShippedTheme()
{
    return LoadObject<UDiceRollTheme>(nullptr, TEXT("/Game/DnDDiceRoll/Theme/DA_DiceRollTheme.DA_DiceRollTheme"), nullptr, LOAD_Quiet | LOAD_NoWarn);
}

UDiceRollTheme* ThemeCopy(const UDiceRollTheme* Shipped)
{
    UDiceRollTheme* Copy = Shipped ? DuplicateObject<UDiceRollTheme>(Shipped, GetTransientPackage()) : nullptr;
    if (Copy) { Copy->ClearFlags(RF_Standalone | RF_Public); Copy->SetFlags(RF_Transient); }
    return Copy;
}

UWorld* MakeWorld()
{
    const UWorld::InitializationValues Values = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false);
    return UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
}

void DestroyWorld(UWorld* World)
{
    World->DestroyWorld(false);
    FlushRenderingCommands();
    CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
}

/** A view resumed on a checkpoint of Run, its playback settled. */
AExpeditionBattleView* Board(FAutomationTestBase& Test, UWorld* World, const FString& Path, const FExpeditionRun& Run, UDiceRollTheme* Theme = nullptr, bool bPrimitives = false)
{
    FExpeditionSaveData Data;
    Data.Current = Run.Snapshot();
    // A finished run's checkpoint carries its ending record (FExpeditionSave::Validate).
    if (Run.Progress().Phase == EExpeditionPhase::Victory || Run.Progress().Phase == EExpeditionPhase::Defeat) { Data.Records.Add(FExpeditionSave::Ending(Run)); }
    FString Error;
    if (!Test.TestTrue(TEXT("The checkpoint writes ") + Error, FExpeditionSave::Write(Path, Data, Error))) { return nullptr; }
    AExpeditionBattleView* View = World->SpawnActor<AExpeditionBattleView>();
    if (!Test.TestNotNull(TEXT("Battle view"), View)) { return nullptr; }
    View->ConfigureStorage(Path);
    View->DispatchBeginPlay();
    if (Theme) { View->UseTheme(Theme); }
    if (bPrimitives) { View->UsePrimitives(); }
    View->Resume();
    for (int32 Frame = 0; Frame < 5; ++Frame) { View->SkipPresentation(); View->Tick(0.05f); }
    return View;
}

int32 Count(const AExpeditionBattleView& View, const TCHAR* Role)
{
    int32 Found = 0;
    for (const AExpeditionBattleView::FJourneyPart& Part : View.JourneyParts()) { Found += Part.Role == FName(Role) ? 1 : 0; }
    return Found;
}
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJourneyMapLookTest, "Project.Expedition.Journey.MapShowsTheQueryForEachFixture",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FJourneyMapLookTest::RunTest(const FString& Parameters)
{
    using namespace JourneyMapTest;
    const TArray<FFixture> All = Fixtures();
    if (!TestEqual(TEXT("All five journey fixtures are reached"), All.Num(), 5)) { return false; }
    for (const FFixture& Fixture : All)
    {
        const FJourney Journey = FJourneyQuery::For(Fixture.Run);
        const FJourneyLook Look = JourneyLayout::Look(Journey);
        const FString Where = Fixture.Name + TEXT(" (") + Describe(Look) + TEXT(")");
        if (!TestEqual(Where + TEXT(": five stages"), Look.Stages.Num(), FExpeditionRun::EncounterCount)) { continue; }
        // Each stage mark is the query's state, with the emblem of the world it was fought in; fog only over unreached stages of a run in progress.
        for (int32 Index = 0; Index < Look.Stages.Num(); ++Index)
        {
            const FJourneyStage& Stage = Journey.Stages[Index];
            const FStageLook& Mark = Look.Stages[Index];
            const bool bEnded = Journey.CurrentIndex == INDEX_NONE;
            const EStageMark Expected = Stage.State == EJourneyState::Cleared ? EStageMark::Cleared : Stage.State == EJourneyState::Lost ? EStageMark::Lost
                : Stage.State == EJourneyState::Current ? EStageMark::Current : bEnded ? EStageMark::Unreached : EStageMark::Fogged;
            TestEqual(FString::Printf(TEXT("%s: stage %d mark"), *Where, Index), static_cast<int32>(Mark.Mark), static_cast<int32>(Expected));
            TestEqual(FString::Printf(TEXT("%s: stage %d emblem"), *Where, Index), static_cast<int32>(Mark.Emblem), static_cast<int32>(Stage.State == EJourneyState::Fogged ? EExpeditionWorld::Unknown : Stage.World));
            TestEqual(FString::Printf(TEXT("%s: stage %d fog"), *Where, Index), Mark.bFog, Stage.State == EJourneyState::Fogged && !bEnded);
            TestEqual(FString::Printf(TEXT("%s: stage %d pennant (won)"), *Where, Index), Mark.bPennant, Stage.State == EJourneyState::Cleared);
            TestEqual(FString::Printf(TEXT("%s: stage %d broken sword (lost)"), *Where, Index), Mark.bBroken, Stage.State == EJourneyState::Lost);
            TestEqual(FString::Printf(TEXT("%s: stage %d walked"), *Where, Index), Mark.bWalkedIn, Stage.State != EJourneyState::Fogged);
        }
        // The fork: each offer's world raised in offer order, named and with its risk word; the ending shows the Vault reached or the world the party fell in.
        if (Journey.CurrentIndex != INDEX_NONE)
        {
            const TArray<FJourneyRoute>& Offers = Journey.Stages[Journey.CurrentIndex].Offers;
            TestEqual(Where + TEXT(": the fork is the current stage"), Look.ForkStage, Journey.CurrentIndex);
            if (TestEqual(Where + TEXT(": one raised diorama per offer"), Look.Raised.Num(), Offers.Num()))
            {
                for (int32 Offer = 0; Offer < Offers.Num(); ++Offer)
                {
                    TestEqual(Where + TEXT(": offer order"), Look.Raised[Offer].Offer, Offer);
                    TestEqual(Where + TEXT(": the offer's world"), static_cast<int32>(Look.Raised[Offer].Kind), static_cast<int32>(DioramaFor(Offers[Offer].World)));
                    TestEqual(Where + TEXT(": the route's name"), Look.Raised[Offer].Name, Offers[Offer].Name);
                    TestEqual(Where + TEXT(": the route's risk word"), Look.Raised[Offer].Risk, FString(FExpeditionRun::RiskWord(Offers[Offer].Risk)));
                    TestFalse(Where + TEXT(": the current revision discloses a risk"), Look.Raised[Offer].Risk.IsEmpty());
                }
            }
            TestFalse(Where + TEXT(": the goal is not reached yet"), Look.bGoalReached);
        }
        else
        {
            TestTrue(Where + TEXT(": the ending has no fork"), Look.ForkStage == INDEX_NONE);
            const bool bWon = Journey.Phase == EExpeditionPhase::Victory;
            TestEqual(Where + TEXT(": the goal is reached only by a win"), Look.bGoalReached, bWon);
            if (TestEqual(Where + TEXT(": one showcase"), Look.Raised.Num(), 1))
            {
                TestEqual(Where + TEXT(": the showcase is no offer"), Look.Raised[0].Offer, int32(INDEX_NONE));
                const FJourneyStage* Shown = bWon ? &Journey.Stages.Last() : Journey.Stages.FindByPredicate([](const FJourneyStage& S) { return S.State == EJourneyState::Lost; });
                if (TestNotNull(Where + TEXT(": the showcased stage"), Shown)) { TestEqual(Where + TEXT(": its world"), static_cast<int32>(Look.Raised[0].Kind), static_cast<int32>(DioramaFor(Shown->World))); }
            }
        }
    }
    // The specific shapes each fixture must have.
    auto LookOf = [&](const TCHAR* Name) { const FFixture* F = All.FindByPredicate([Name](const FFixture& X) { return X.Name == Name; }); return F ? Describe(JourneyLayout::Look(FJourneyQuery::For(F->Run))) : FString(); };
    TestEqual(TEXT("fresh"), LookOf(TEXT("fresh")), FString(TEXT("N F+f~ F+f~ F+f~ F+f~ [0:0] [1:1] -")));
    TestEqual(TEXT("mid-run"), LookOf(TEXT("mid")), FString(TEXT("CForest+p CVigil+p N F+f~ F+f~ [0:0] [1:1] -")));
    TestEqual(TEXT("finale fork"), LookOf(TEXT("finale")), FString(TEXT("CForest+p CVigil+p CForest+p CVigil+p N [0:2] [1:2] -")));
    TestEqual(TEXT("victory"), LookOf(TEXT("victory")), FString(TEXT("CForest+p CVigil+p CForest+p CVigil+p CVault+p [-1:2] goal")));
    TestEqual(TEXT("defeat"), LookOf(TEXT("defeat")), FString(TEXT("CForest+p CVigil+p LVigil+x U~ U~ [-1:1] -")));
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJourneySceneTest, "Project.Expedition.Presentation.JourneySceneIsBuiltFromTheQuery",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FJourneySceneTest::RunTest(const FString& Parameters)
{
    using namespace JourneyMapTest;
    const UDiceRollTheme* Shipped = ShippedTheme();
    if (!TestNotNull(TEXT("The shipped theme asset"), Shipped) || !TestTrue(TEXT("Its journey map is on (run the theme import)"), Shipped->JourneyComplete())) { return false; }
    const TArray<FFixture> All = Fixtures();
    const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation/JourneyScene"), FGuid::NewGuid().ToString());
    UWorld* World = MakeWorld();
    if (!TestNotNull(TEXT("Transient world"), World)) { return false; }
    const FDiceRollOverview Set = Shipped->LoadOverview();
    auto DioramaName = [&](EDiorama Kind) { return (Kind == EDiorama::Vault ? Set.Vault : Kind == EDiorama::Vigil ? Set.Vigil : Set.Root)->GetFName(); };
    for (const FFixture& Fixture : All)
    {
        AExpeditionBattleView* View = Board(*this, World, FPaths::Combine(Directory, Fixture.Name, TEXT("current.json")), Fixture.Run);
        if (!View) { continue; }
        const FString Where = Fixture.Name;
        if (!TestTrue(Where + TEXT(": the journey map is shown (the route screen, or the ending)"), View->ShowingJourney())) { continue; }
        const FJourneyLook Expected = JourneyLayout::Look(FJourneyQuery::For(Fixture.Run));
        TestEqual(Where + TEXT(": the view shows the query's look"), Describe(View->JourneyShown()), Describe(Expected));
        int32 Fog = 0, Emblems = 0, Pennants = 0, Broken = 0, Walked = 0;
        for (const FStageLook& Stage : Expected.Stages)
        {
            Fog += Stage.bFog; Emblems += Stage.Emblem != EExpeditionWorld::Unknown; Pennants += Stage.bPennant; Broken += Stage.bBroken; Walked += Stage.bWalkedIn;
        }
        const int32 Forks = Expected.ForkStage != INDEX_NONE ? Expected.Raised.Num() : 0;
        TestEqual(Where + TEXT(": the journey table and map"), Count(*View, TEXT("Table")) + Count(*View, TEXT("Map")), 2);
        TestEqual(Where + TEXT(": five stage tokens"), Count(*View, TEXT("Token")), 5);
        TestEqual(Where + TEXT(": a fog bank over each unreached stage of a run in progress"), Count(*View, TEXT("Fog")), Fog);
        TestEqual(Where + TEXT(": an emblem on each stage with a known world"), Count(*View, TEXT("Emblem")), Emblems);
        TestEqual(Where + TEXT(": a pennant per win"), Count(*View, TEXT("Pennant")), Pennants);
        TestEqual(Where + TEXT(": a broken sword at the loss"), Count(*View, TEXT("Lost")), Broken);
        TestEqual(Where + TEXT(": walked ink to each reached stage, the fork's branches, and to the Vault on a win"), Count(*View, TEXT("Ink")), Walked + Forks + (Expected.bGoalReached ? 1 : 0));
        TestEqual(Where + TEXT(": every ink stroke is walked or dashed (six road segments plus the fork)"), Count(*View, TEXT("Ink")) + Count(*View, TEXT("InkDashed")), 6 + Forks);
        TestEqual(Where + TEXT(": the Vault goal is always shown"), Count(*View, TEXT("Goal")), 1);
        TestEqual(Where + TEXT(": a plinth per raised diorama"), Count(*View, TEXT("Plinth")), Expected.Raised.Num());
        TArray<FName> Dioramas;
        for (const AExpeditionBattleView::FJourneyPart& Part : View->JourneyParts()) { if (Part.Role == TEXT("Diorama")) { Dioramas.Add(Part.Mesh); } }
        if (TestEqual(Where + TEXT(": a diorama per raise"), Dioramas.Num(), Expected.Raised.Num()))
        {
            for (int32 Index = 0; Index < Dioramas.Num(); ++Index) { TestEqual(Where + TEXT(": the r002 diorama of the world"), Dioramas[Index], DioramaName(Expected.Raised[Index].Kind)); }
        }
        // The fog stands where the stage does; the raised dioramas stand on their plinths at the layout's spots, and the fly-in seam reports them.
        for (const AExpeditionBattleView::FJourneyPart& Part : View->JourneyParts())
        {
            if (Part.Role == TEXT("Fog") || Part.Role == TEXT("Token")) { TestTrue(Where + TEXT(": on its stage's socket"), FVector2D(Part.Local.GetLocation()).Equals(StageSpot(Part.Index), 0.01)); }
            if (Part.Role == TEXT("Diorama") && Part.Index != INDEX_NONE)
            {
                FTransform Seam;
                TestTrue(Where + TEXT(": the fly-in seam finds the raised diorama"), View->RaisedDioramaTransform(Part.Index, Seam) && FVector2D(Seam.GetLocation()).Equals(PlinthSpot(Part.Index, Expected.Raised.Num()), 0.5));
            }
        }
        FTransform None;
        TestFalse(Where + TEXT(": no seam for an offer that is not raised"), View->RaisedDioramaTransform(5, None));
        TestTrue(Where + TEXT(": the camera is the journey camera"), View->CameraTransform().GetLocation().Equals(View->JourneyCameraTransform().GetLocation(), 0.5));
    }
    DestroyWorld(World);
    IFileManager::Get().DeleteDirectory(*Directory, false, true);
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJourneyFallbackTest, "Project.Expedition.Presentation.JourneyFallsBackAsAWhole",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FJourneyFallbackTest::RunTest(const FString& Parameters)
{
    using namespace JourneyMapTest;
    const UDiceRollTheme* Shipped = ShippedTheme();
    if (!TestNotNull(TEXT("The shipped theme asset"), Shipped) || !TestTrue(TEXT("Its journey map is on"), Shipped->JourneyComplete())) { return false; }
    const TArray<FFixture> All = Fixtures();
    const FFixture* Mid = All.FindByPredicate([](const FFixture& F) { return F.Name == TEXT("mid"); });
    const FFixture* Won = All.FindByPredicate([](const FFixture& F) { return F.Name == TEXT("victory"); });
    if (!TestTrue(TEXT("The mid-run and victory fixtures"), Mid && Won)) { return false; }
    const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation/JourneyFallback"), FGuid::NewGuid().ToString());
    // Theme copies: the flag off; one piece pointing at a deleted package; one piece unloadable; and (separately) the r002 overview off, which the map needs.
    const TSoftObjectPtr<UStaticMesh> Dangling{FSoftObjectPath(TEXT("/Game/DnDDiceRoll/Theme/Journey/SM_Deleted_Journey_Piece.SM_Deleted_Journey_Piece"))};
    const TSoftObjectPtr<UStaticMesh> Unloadable{FSoftObjectPath(TEXT("/Engine/BasicShapes/Cube.NoSuchJourneyMesh"))};
    struct FCase { const TCHAR* Name; TFunction<void(UDiceRollTheme&)> Break; };
    const TArray<FCase> Cases = {
        {TEXT("journey flag off"), [](UDiceRollTheme& T) { T.bJourneyR001 = false; }},
        {TEXT("a deleted fog package"), [&](UDiceRollTheme& T) { T.JourneyFog = Dangling; }},
        {TEXT("an unloadable plinth"), [&](UDiceRollTheme& T) { T.JourneyPlinth = Unloadable; }},
        {TEXT("the r002 overview off"), [](UDiceRollTheme& T) { T.bOverviewR002 = false; }},
    };
    // The unloadable piece logs the engine's own "failed to find" warning each time the set is loaded; that is the case under test.
    AddExpectedMessage(TEXT("NoSuchJourneyMesh"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 0);
    for (const FCase& Case : Cases)
    {
        TStrongObjectPtr<UDiceRollTheme> Broken(ThemeCopy(Shipped));
        Case.Break(*Broken);
        TestTrue(FString(Case.Name) + TEXT(": the set loads empty or not at all"), !Broken->LoadJourney().IsComplete() || !Broken->OverviewR002Complete());
        UWorld* World = MakeWorld();
        if (!TestNotNull(TEXT("Transient world"), World)) { return false; }
        if (AExpeditionBattleView* View = Board(*this, World, FPaths::Combine(Directory, Case.Name, TEXT("route.json")), Mid->Run, Broken.Get()))
        {
            TestFalse(FString(Case.Name) + TEXT(": no journey set"), View->HasJourneySet());
            TestTrue(FString(Case.Name) + TEXT(": the route screen is today's overview"), View->ShowingOverview() && !View->ShowingJourney());
            TestTrue(FString(Case.Name) + TEXT(": nothing of the journey is drawn"), View->JourneyParts().IsEmpty());
            TestEqual(FString(Case.Name) + TEXT(": today's table and a diorama per offer"), View->OverviewMeshes().Num(), 1 + View->Progress().Offers.Num());
        }
        if (AExpeditionBattleView* View = Board(*this, World, FPaths::Combine(Directory, Case.Name, TEXT("ending.json")), Won->Run, Broken.Get()))
        {
            TestFalse(FString(Case.Name) + TEXT(": the ending keeps the battle board, as today"), View->ShowingOverview());
        }
        DestroyWorld(World);
    }
    // -ExpeditionPrimitives (UsePrimitives): no overview at all, as today; the run state is the same as with the journey map.
    UWorld* World = MakeWorld();
    if (!TestNotNull(TEXT("Transient world"), World)) { return false; }
    FExpeditionProgress Shown[2];
    for (int32 Variant = 0; Variant < 2; ++Variant)
    {
        if (AExpeditionBattleView* View = Board(*this, World, FPaths::Combine(Directory, FString::Printf(TEXT("variant-%d"), Variant), TEXT("current.json")), Mid->Run, nullptr, Variant == 1))
        {
            TestEqual(TEXT("The journey map is drawn except on primitives"), View->ShowingJourney(), Variant == 0);
            if (Variant == 1) { TestFalse(TEXT("Primitives show no overview"), View->ShowingOverview()); }
            Shown[Variant] = View->Progress();
        }
    }
    TestTrue(TEXT("The journey map leaves the run state as primitives do (ADR-0001)"), FExpeditionProgress::StaticStruct()->CompareScriptStruct(&Shown[0], &Shown[1], 0));
    DestroyWorld(World);
    IFileManager::Get().DeleteDirectory(*Directory, false, true);
    return !HasAnyErrors();
}

namespace JourneyMapTest
{
/** A journey at the fork of Stage with the given offer worlds (or, Stage INDEX_NONE, an ending: won, or lost at stage 2 in ShowcaseWorld). */
FJourney Synthetic(int32 Stage, TArray<EExpeditionWorld> Offers, EExpeditionWorld ShowcaseWorld = EExpeditionWorld::Vault, bool bWon = true)
{
    FJourney Journey;
    Journey.Phase = Stage == INDEX_NONE ? (bWon ? EExpeditionPhase::Victory : EExpeditionPhase::Defeat) : EExpeditionPhase::Route;
    Journey.CurrentIndex = Stage;
    for (int32 Index = 0; Index < FExpeditionRun::EncounterCount; ++Index)
    {
        FJourneyStage S;
        S.Index = Index;
        S.Kind = FExpeditionRun::StageKind(Index);
        const bool bDone = Stage == INDEX_NONE ? (bWon || Index <= 2) : Index < Stage;
        if (bDone)
        {
            const bool bLostHere = Stage == INDEX_NONE && !bWon && Index == 2;
            S.State = bLostHere ? EJourneyState::Lost : EJourneyState::Cleared;
            S.bTaken = true;
            S.World = Index == 4 ? EExpeditionWorld::Vault : bLostHere ? ShowcaseWorld : Index % 2 ? EExpeditionWorld::Vigil : EExpeditionWorld::Root;
            S.Taken.World = S.World;
            S.Taken.Name = Index == 4 ? TEXT("Artifact vault: Watchtower road") : TEXT("Watchtower road");
        }
        else if (Index == Stage)
        {
            S.State = EJourneyState::Current;
            for (int32 Offer = 0; Offer < Offers.Num(); ++Offer)
            {
                FJourneyRoute Route;
                Route.Id = Offer + 1; Route.World = Offers[Offer]; Route.bDangerous = Offer == 1; Route.Risk = Offer == 1 ? ERouteRisk::Severe : ERouteRisk::Moderate;
                Route.Name = Offers[Offer] == EExpeditionWorld::Vault ? (Offer ? TEXT("Artifact vault: Watchtower road") : TEXT("Artifact vault: Stream trail")) : (Offer ? TEXT("Watchtower road") : TEXT("Stream trail"));
                S.Offers.Add(Route);
            }
        }
        Journey.Stages.Add(S);
    }
    return Journey;
}

bool Overlap(const ExpeditionLayout::Rect& A, const ExpeditionLayout::Rect& B, double Margin)
{
    return A.Left < B.Right + Margin && A.Right > B.Left - Margin && A.Top < B.Bottom + Margin && A.Bottom > B.Top - Margin;
}

FString RectText(const ExpeditionLayout::Rect& R) { return FString::Printf(TEXT("%.0f,%.0f to %.0f,%.0f"), R.Left, R.Top, R.Right, R.Bottom); }

/** The design-unit box of a small mesh standing at a table point (turned toward the camera, scale 1). */
bool MeshBox(const UStaticMesh& Mesh, const FVector& At, double W, double H, ExpeditionLayout::Rect& Out)
{
    const FBox Local = Mesh.GetBoundingBox();
    const FRotator Turn(0.0f, -90.0f, 0.0f);
    FVector2D Low(1e9, 1e9), High(-1e9, -1e9);
    for (int32 Corner = 0; Corner < 8; ++Corner)
    {
        const FVector Point((Corner & 1) ? Local.Max.X : Local.Min.X, (Corner & 2) ? Local.Max.Y : Local.Min.Y, (Corner & 4) ? Local.Max.Z : Local.Min.Z);
        FVector2D Pixel;
        if (!JourneyLayout::Project(Turn.RotateVector(Point) + At, W, H, Pixel)) { return false; }
        Low = FVector2D(FMath::Min(Low.X, Pixel.X), FMath::Min(Low.Y, Pixel.Y));
        High = FVector2D(FMath::Max(High.X, Pixel.X), FMath::Max(High.Y, Pixel.Y));
    }
    Out = {Low.X, Low.Y, High.X, High.Y};
    return true;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJourneyHudClearTest, "Project.Expedition.Presentation.JourneyDioramasStayClearOfTheHud",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FJourneyHudClearTest::RunTest(const FString& Parameters)
{
    using namespace JourneyMapTest;
    // At 1920x1080, 1280x720, 1600x600 and 2560x1080, on every fork the worlds can offer and on both endings, each raised diorama on its plinth stays inside the viewport, clear of
    // every HUD rectangle (the banner, both route cards or the ending's panel, the party cards, the status strip, the Menu, the feedback line, every stage chip and every plaque,
    // its own included) and of the other diorama, and is at least 190 x 150 design units: a world big enough to read. The stage tokens with their pennants and the Vault keep stay
    // clear of the top bar and the party cards, and the chips do not overlap each other. The meshes are the shipped theme's.
    const UDiceRollTheme* Shipped = ShippedTheme();
    if (!TestNotNull(TEXT("The shipped theme asset"), Shipped) || !TestTrue(TEXT("Its journey map is on"), Shipped->JourneyComplete())) { return false; }
    const FDiceRollOverview Set = Shipped->LoadOverview();
    const FDiceRollJourney Journey = Shipped->LoadJourney();
    if (!TestTrue(TEXT("The r002 dioramas and the journey set load"), Set.bR002 && Journey.IsComplete())) { return false; }
    auto MeshOf = [&](EDiorama Kind) { return Kind == EDiorama::Vault ? Set.Vault : Kind == EDiorama::Vigil ? Set.Vigil : Set.Root; };
    using W_ = EExpeditionWorld;
    struct FScreen { const TCHAR* Name; FJourney Journey; };
    const TArray<FScreen> Screens = {
        {TEXT("first fork (forest, cathedral)"), Synthetic(0, {W_::Root, W_::Vigil})}, {TEXT("two forests"), Synthetic(1, {W_::Root, W_::Root})},
        {TEXT("two cathedrals"), Synthetic(2, {W_::Vigil, W_::Vigil})}, {TEXT("mid fork"), Synthetic(3, {W_::Root, W_::Vigil})},
        {TEXT("finale fork (two Vaults)"), Synthetic(4, {W_::Vault, W_::Vault})}, {TEXT("victory"), Synthetic(INDEX_NONE, {})},
        {TEXT("defeat in the Vigil"), Synthetic(INDEX_NONE, {}, W_::Vigil, false)}, {TEXT("defeat in the forest"), Synthetic(INDEX_NONE, {}, W_::Root, false)}};
    constexpr double Margin = 4.0, EdgeInset = 12.0;
    for (const FIntPoint& Resolution : {FIntPoint(1920, 1080), FIntPoint(1280, 720), FIntPoint(1600, 600), FIntPoint(2560, 1080)})
    {
        const double Scale = ExpeditionLayout::ViewScale(Resolution.X, Resolution.Y);
        const double W = Resolution.X / Scale, H = Resolution.Y / Scale;
        for (const FScreen& Screen : Screens)
        {
            const FJourneyLook Look = JourneyLayout::Look(Screen.Journey);
            const TArray<ExpeditionLayout::Rect> Hud = JourneyLayout::HudRects(Look, W, H);
            const FString Base = FString::Printf(TEXT("%s, %dx%d"), Screen.Name, Resolution.X, Resolution.Y);
            TestEqual(Base + TEXT(": the screen raises the expected dioramas"), Look.Raised.Num(), Look.bEnded ? 1 : 2);
            TArray<ExpeditionLayout::Rect> Placed;
            for (int32 Index = 0; Index < Look.Raised.Num(); ++Index)
            {
                ExpeditionLayout::Rect Box{};
                const FString Label = FString::Printf(TEXT("%s, raised %d"), *Base, Index);
                if (!TestTrue(Label + TEXT(": projects"), RaisedScreenBox(MeshOf(Look.Raised[Index].Kind)->GetBoundingBox(), Journey.Plinth->GetBoundingBox(), Look.Raised[Index], W, H, Box))) { continue; }
                const FString Where = TEXT(" (") + RectText(Box) + FString::Printf(TEXT(" of %.0f x %.0f)"), W, H);
                TestTrue(Label + TEXT(": inside the viewport") + Where, Box.Left >= EdgeInset && Box.Top >= EdgeInset && Box.Right <= W - EdgeInset && Box.Bottom <= H - EdgeInset);
                for (int32 Rect = 0; Rect < Hud.Num(); ++Rect)
                { TestFalse(FString::Printf(TEXT("%s: clear of HUD rect %d (%s)%s"), *Label, Rect, *RectText(Hud[Rect]), *Where), Overlap(Box, Hud[Rect], Margin)); }
                for (const ExpeditionLayout::Rect& Other : Placed) { TestFalse(Label + TEXT(": clear of the other diorama"), Overlap(Box, Other, 0.0)); }
                Placed.Add(Box);
                TestTrue(Label + TEXT(": big enough to read") + Where, Box.Right - Box.Left >= 190.0 && Box.Bottom - Box.Top >= 150.0);
                // Its plaque sits on its plinth (inside its box, by design), inside the viewport and clear of every HUD rectangle and the other dioramas.
                const ExpeditionLayout::Rect Plaque = PlaqueRect(Look.Raised[Index], W, H);
                TestTrue(Label + TEXT(": the plaque is inside the viewport (") + RectText(Plaque) + TEXT(")"), Plaque.Left >= EdgeInset && Plaque.Right <= W - EdgeInset && Plaque.Bottom <= H - EdgeInset);
                for (int32 Rect = 0; Rect < Hud.Num(); ++Rect)
                { TestFalse(FString::Printf(TEXT("%s: the plaque (%s) is clear of HUD rect %d (%s)"), *Label, *RectText(Plaque), Rect, *RectText(Hud[Rect])), Overlap(Plaque, Hud[Rect], 2.0)); }
                for (int32 Other = 0; Other + 1 < Placed.Num(); ++Other) { TestFalse(Label + TEXT(": the plaque is clear of the other diorama"), Overlap(Plaque, Placed[Other], 0.0)); }
                TestTrue(Label + TEXT(": centred: the pair straddles the middle"), Look.Raised.Num() == 1 ? FMath::Abs((Box.Left + Box.Right) * 0.5 - W * 0.5) < 20.0 : (Index == 0 ? Box.Right < W * 0.5 : Box.Left > W * 0.5));
            }
            // The road's tokens (with the pennant's height) and the Vault keep stay below the top bar and right of the party cards; the chips do not overlap.
            // The top band's panels: the status strip, the Menu and (on the route screen) the banner.
            TArray<ExpeditionLayout::Rect> Top = {{8.0, 4.0, 288.0, 50.0}, {W - 130.0, 4.0, W - 4.0, 48.0}};
            if (!Look.bEnded) { Top.Add(BannerRect(W)); }
            auto UnderTop = [&Top](const ExpeditionLayout::Rect& Box) { for (const ExpeditionLayout::Rect& Panel : Top) { if (Overlap(Box, Panel, 0.0)) { return true; } } return false; };
            const ExpeditionLayout::Rect Party = {ExpeditionLayout::PartyCardX, ExpeditionLayout::PartyCardTop, ExpeditionLayout::PartyCardX + ExpeditionLayout::PartyCardWidth, 330.0};
            for (int32 Stage = 0; Stage < 5; ++Stage)
            {
                ExpeditionLayout::Rect Box{};
                if (MeshBox(*Journey.Pennant, StagePoint(Stage), W, H, Box))
                {
                    TestFalse(FString::Printf(TEXT("%s: stage %d is clear of the top band's panels (%s)"), *Base, Stage, *RectText(Box)), UnderTop(Box));
                    if (!Look.bEnded) { TestFalse(FString::Printf(TEXT("%s: stage %d is clear of the party cards"), *Base, Stage), Overlap(Box, Party, 0.0)); }
                }
                for (int32 Other = Stage + 1; Other < 5; ++Other)
                { TestFalse(FString::Printf(TEXT("%s: chips %d and %d"), *Base, Stage, Other), Overlap(StageChipRect(Stage, W, H), StageChipRect(Other, W, H), 2.0)); }
                TestFalse(FString::Printf(TEXT("%s: chip %d and the goal's"), *Base, Stage), Overlap(StageChipRect(Stage, W, H), GoalChipRect(W, H), 2.0));
            }
            ExpeditionLayout::Rect Keep{};
            if (TestTrue(Base + TEXT(": the keep projects"), MeshBox(*Journey.Goal, GoalPoint(), W, H, Keep)))
            {
                TestFalse(Base + TEXT(": the Vault keep is clear of the Menu and the top band (") + RectText(Keep) + TEXT(")"), UnderTop(Keep));
                TestTrue(Base + TEXT(": the Vault keep is on screen"), Keep.Right <= W - EdgeInset);
            }
        }
    }
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJourneyCameraTest, "Project.Expedition.Presentation.JourneyLayoutProjectsLikeTheCamera",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FJourneyCameraTest::RunTest(const FString& Parameters)
{
    using namespace JourneyMapTest;
    // The layout tests project with JourneyLayout::Project; this proves it is the engine's own projection of the view's journey camera at every supported aspect: the engine's
    // FMinimalViewInfo projection under the default MaintainYFOV constraint, with the camera's pose and field of view, lands where Project says.
    const TArray<FFixture> All = Fixtures();
    if (!TestTrue(TEXT("The fresh fixture"), !All.IsEmpty())) { return false; }
    const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation/JourneyCamera"), FGuid::NewGuid().ToString());
    UWorld* World = MakeWorld();
    if (!TestNotNull(TEXT("Transient world"), World)) { return false; }
    if (AExpeditionBattleView* View = Board(*this, World, FPaths::Combine(Directory, TEXT("current.json")), All[0].Run))
    {
        if (TestTrue(TEXT("The journey map is shown"), View->ShowingJourney()))
        {
            const FTransform Camera = View->CameraTransform();
            const FVector Table(0.0, 0.0, 80.0);
            TestTrue(TEXT("The eye is where JourneyLayout says"), Camera.GetLocation().Equals(Table + JourneyLayout::CameraEye(), 0.5));
            TestTrue(TEXT("The aim is where JourneyLayout says"), Camera.GetRotation().GetForwardVector().Equals(JourneyLayout::CameraLook().GetSafeNormal(), 0.001));
            TestEqual(TEXT("The field of view"), static_cast<double>(View->CameraFieldOfView()), JourneyLayout::CameraFieldOfView, 0.001);
            for (const FIntPoint& Resolution : {FIntPoint(1920, 1080), FIntPoint(1280, 720), FIntPoint(1600, 600), FIntPoint(2560, 1080)})
            {
                FMinimalViewInfo Info;
                Info.Location = Camera.GetLocation();
                Info.Rotation = Camera.Rotator();
                Info.FOV = View->CameraFieldOfView();
                Info.AspectRatio = static_cast<float>(JourneyLayout::CameraReferenceAspect);   // the camera component's default aspect, which MaintainYFOV reads the FOV against
                Info.bConstrainAspectRatio = false;
                const FIntRect Rect(0, 0, Resolution.X, Resolution.Y);
                FSceneViewProjectionData Data;
                Data.ViewOrigin = Info.Location;
                Data.SetViewRectangle(Rect);
                Data.ViewRotationMatrix = FInverseRotationMatrix(Info.Rotation) * FMatrix(FPlane(0, 0, 1, 0), FPlane(1, 0, 0, 0), FPlane(0, 1, 0, 0), FPlane(0, 0, 0, 1));
                FMinimalViewInfo::CalculateProjectionMatrixGivenViewRectangle(Info, AspectRatio_MaintainYFOV, Rect, Data);
                const FMatrix ViewProjection = Data.ComputeViewProjectionMatrix();
                for (const FVector& Point : {StagePoint(0), StagePoint(4), GoalPoint(), FVector(40, -100, 30), FVector(-40, -140, 0), FVector(0, -200, 0)})
                {
                    FVector2D Real, Mine;
                    if (!FSceneView::ProjectWorldToScreen(Table + Point, Rect, ViewProjection, Real) || !JourneyLayout::Project(Point, Resolution.X, Resolution.Y, Mine))
                    { AddError(TEXT("A table point does not project")); continue; }
                    TestTrue(FString::Printf(TEXT("%dx%d: projects like the engine (%s: %.1f, %.1f vs %.1f, %.1f)"), Resolution.X, Resolution.Y, *Point.ToString(), Mine.X, Mine.Y, Real.X, Real.Y), Mine.Equals(Real, 1.5));
                }
            }
        }
    }
    DestroyWorld(World);
    IFileManager::Get().DeleteDirectory(*Directory, false, true);
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJourneyDioramaClickTest, "Project.Expedition.Presentation.JourneyDioramaClickIsTheCardCommand",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FJourneyDioramaClickTest::RunTest(const FString& Parameters)
{
    using namespace JourneyMapTest;
    // Through the real HUD and controller: a click at the middle of an offered diorama hits that offer's diorama button and sets out exactly as a click on its card does:
    // the same run state and the same checkpoint bytes. The diorama is never a Tab stop (the card already is), and with the Menu open it takes no click.
    const TArray<FFixture> All = Fixtures();
    const FFixture* Mid = All.FindByPredicate([](const FFixture& F) { return F.Name == TEXT("mid"); });
    if (!TestNotNull(TEXT("The mid-run fixture"), Mid)) { return false; }
    FExpeditionSaveData Data;
    Data.Current = Mid->Run.Snapshot();
    for (int32 Offer = 0; Offer < 2; ++Offer)
    {
        FExpeditionSnapshot After[2];
        TArray<uint8> Bytes[2];
        for (int32 Way = 0; Way < 2; ++Way)   // 0: the card, 1: the diorama
        {
            ExpeditionInputHarness::FHarness Game(*this, TEXT("JourneyClick"), &Data);
            if (!TestTrue(TEXT("Input, board and HUD are ready"), Game.IsReady())) { return false; }
            Game.Frame();
            Game.Press(EKeys::O);
            Game.Settle();
            if (!TestTrue(TEXT("The route screen shows the journey map"), Game.View->ShowingJourney())) { return false; }
            const FName Card(*FString::Printf(TEXT("Route%d"), Offer)), Diorama(*FString::Printf(TEXT("Diorama%d"), Offer));
            const TArray<FName> Tabs = Game.Hud->MenuButtons();
            TestTrue(TEXT("The card is a Tab stop"), Tabs.Contains(Card));
            TestFalse(TEXT("The diorama is not a Tab stop"), Tabs.Contains(Diorama));
            ExpeditionLayout::Rect Box{};
            if (!TestTrue(TEXT("The view reports the raised diorama's box"), Game.View->RaisedScreenBox(Offer, Game.Target.Size.X / ExpeditionLayout::ViewScale(Game.Target.Size.X, Game.Target.Size.Y),
                Game.Target.Size.Y / ExpeditionLayout::ViewScale(Game.Target.Size.X, Game.Target.Size.Y), Box))) { return false; }
            const double S = ExpeditionLayout::ViewScale(Game.Target.Size.X, Game.Target.Size.Y);
            const FVector2D Centre((Box.Left + Box.Right) * 0.5 * S, (Box.Top + Box.Bottom) * 0.5 * S);
            FName Hit;
            if (!TestTrue(TEXT("A click at the diorama's middle hits its button"), Game.Hud->ButtonAt(Centre, Hit) && Hit == Diorama)) { return false; }
            if (Way == 1 && Offer == 0)
            {
                // With the Menu open, the overlay owns every click: the diorama is not offered.
                Game.Press(EKeys::I);   // the Menu key
                FName Under;
                TestTrue(TEXT("The Menu is open"), Game.Player->ShowingMenu());
                TestFalse(TEXT("With the Menu open the diorama takes no click"), Game.Hud->ButtonAt(Centre, Under) && Under == Diorama);
                Game.Press(EKeys::I);
                TestFalse(TEXT("The Menu is closed again"), Game.Player->ShowingMenu());
                Game.Hud->ButtonAt(Centre, Hit);
            }
            Game.Player->HandleButton(Way == 0 ? Card : Hit);
            Game.Frame();
            TestTrue(TEXT("Setting out starts the fight"), Game.View->InCombat());
            After[Way] = Game.Live();
            Bytes[Way] = Game.CheckpointBytes();
        }
        TestTrue(FString::Printf(TEXT("Offer %d: the diorama click leaves the same run as the card (route, fight, both random streams)"), Offer), ExpeditionTestSupport::SameRun(After[0], After[1]));
        TestTrue(FString::Printf(TEXT("Offer %d: the same checkpoint bytes"), Offer), Bytes[0] == Bytes[1] && !Bytes[0].IsEmpty());
        TestEqual(FString::Printf(TEXT("Offer %d: the route taken is that offer"), Offer), After[1].Progress.History.Last().Route.Id, Mid->Run.Progress().Offers[Offer].Id);
    }
    return !HasAnyErrors();
}
#endif
