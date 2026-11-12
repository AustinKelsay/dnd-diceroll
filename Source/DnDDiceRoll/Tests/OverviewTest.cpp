#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Persistence/ExpeditionSave.h"
#include "Presentation/DiceRollTheme.h"
#include "Presentation/ExpeditionBattleView.h"
#include "Presentation/OverviewLayout.h"
#include "RenderingThread.h"
#include "SceneView.h"
#include "UObject/GarbageCollection.h"
#include "UObject/StrongObjectPtr.h"
#include "ExpeditionTestSupport.h"

/**
 * The tabletop overview r002 (#149, the last part): which diorama each road gets and where it stands (pure), the theme's all-or-nothing choice between the r002
 * set and the r001 one, the shipped theme's set, the scene the view builds on the title and on the route screens (including the finale's Vault diorama), and that
 * the choice of overview never changes the snapshot or the checkpoint (ADR-0001).
 */
namespace
{
using OverviewLayout::EDiorama;

UWorld* MakeOverviewWorld()
{
    const UWorld::InitializationValues Values = UWorld::InitializationValues()
        .AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false);
    return UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
}

void DestroyOverviewWorld(UWorld* World)
{
    World->DestroyWorld(false);
    FlushRenderingCommands();
    CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
}

const UDiceRollTheme* OverviewShippedTheme()
{
    return LoadObject<UDiceRollTheme>(nullptr, TEXT("/Game/DnDDiceRoll/Theme/DA_DiceRollTheme.DA_DiceRollTheme"), nullptr, LOAD_Quiet | LOAD_NoWarn);
}

UDiceRollTheme* OverviewThemeCopy(const UDiceRollTheme* Shipped)
{
    UDiceRollTheme* Copy = Shipped ? DuplicateObject<UDiceRollTheme>(Shipped, GetTransientPackage()) : nullptr;
    if (Copy) { Copy->ClearFlags(RF_Standalone | RF_Public); Copy->SetFlags(RF_Transient); }
    return Copy;
}

const TSoftObjectPtr<UStaticMesh> CubeMesh{FSoftObjectPath(TEXT("/Engine/BasicShapes/Cube.Cube"))};
const TSoftObjectPtr<UStaticMesh> SphereMesh{FSoftObjectPath(TEXT("/Engine/BasicShapes/Sphere.Sphere"))};
const TSoftObjectPtr<UStaticMesh> Dangling{FSoftObjectPath(TEXT("/Game/DnDDiceRoll/Theme/Overview/SM_Deleted_Overview_Piece.SM_Deleted_Overview_Piece"))};
const TSoftObjectPtr<UStaticMesh> Unloadable{FSoftObjectPath(TEXT("/Engine/BasicShapes/Cube.NoSuchOverviewMesh"))};

/** A theme whose r001 entries are spheres and whose r002 entries are cubes, so which set loaded is visible from the pointers. */
void FillOverview(UDiceRollTheme& Theme)
{
    Theme.Tabletop = SphereMesh; Theme.RootDiorama = SphereMesh; Theme.VigilDiorama = SphereMesh;
    Theme.bOverviewR002 = true;
    Theme.OverviewTabletop = CubeMesh; Theme.OverviewRootDiorama = CubeMesh; Theme.OverviewVigilDiorama = CubeMesh; Theme.OverviewVaultDiorama = CubeMesh;
}

/** A checkpoint on the route screen: the start of a run, or the one before the finale (four fights won). */
bool WriteRouteCheckpoint(const FString& Path, bool bBeforeFinale)
{
    FExpeditionRun Run;
    FString Error;
    for (int32 Seed = 1; Seed < 200; ++Seed)
    {
        Run = FExpeditionRun();
        if (!Run.NewRun(Seed, Error, FString::Printf(TEXT("overview-%d"), Seed))) { continue; }
        bool bOk = true;
        while (bBeforeFinale && bOk && Run.Progress().Phase == EExpeditionPhase::Route && Run.Progress().EncountersCompleted < FExpeditionRun::EncounterCount - 1)
        {
            if (!Run.ChooseRoute(Run.Progress().Offers[0].Id, Error) || !ExpeditionTestSupport::FinishCombat(Run) || Run.Progress().Phase != EExpeditionPhase::Reward) { bOk = false; break; }
            int32 Survivor = INDEX_NONE;
            for (const FCombatantState& Unit : Run.Battle().Snapshot().Encounter.Combatants)
            { if (Unit.Side == ECombatSide::Party && Unit.Life == ECombatLife::Conscious) { Survivor = Unit.Id; break; } }
            if (Survivor == INDEX_NONE || !Run.ClaimReward(Survivor, Error)) { bOk = false; break; }
            if (Run.Progress().Phase == EExpeditionPhase::Recovery && !Run.Recover(true, Error)) { bOk = false; break; }
        }
        if (!bOk || Run.Progress().Phase != EExpeditionPhase::Route) { continue; }
        if (bBeforeFinale && Run.Progress().EncountersCompleted != FExpeditionRun::EncounterCount - 1) { continue; }
        FExpeditionSaveData Data;
        Data.Current = Run.Snapshot();
        return FExpeditionSave::Write(Path, Data, Error);
    }
    return false;
}

AExpeditionBattleView* OverviewBoard(FAutomationTestBase& Test, UWorld* World, const FString& Path)
{
    AExpeditionBattleView* View = World->SpawnActor<AExpeditionBattleView>();
    if (!Test.TestNotNull(TEXT("Battle view"), View)) { return nullptr; }
    View->ConfigureStorage(Path);
    View->DispatchBeginPlay();
    View->Resume();
    View->Tick(0.016f);
    return View;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOverviewLayoutTest, "Project.Expedition.Presentation.OverviewChoosesAndPlacesTheDioramas",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FOverviewLayoutTest::RunTest(const FString& Parameters)
{
    const bool Guarded[] = {false, false};
    const bool Mixed[] = {false, true};
    const bool Only[] = {true};
    // The title shows the first two worlds whatever the flags say.
    TestTrue(TEXT("The title shows the forest and the cathedral"), OverviewLayout::Kinds(false, false, {}, true) == TArray<EDiorama>({EDiorama::Root, EDiorama::Vigil}));
    TestTrue(TEXT("The title does not change without a Vault diorama"), OverviewLayout::Kinds(false, false, {}, false) == TArray<EDiorama>({EDiorama::Root, EDiorama::Vigil}));
    // A route screen: guarded roads the forest, dangerous roads the cathedral, in the order offered.
    TestTrue(TEXT("Two guarded roads are two forests"), OverviewLayout::Kinds(true, false, Guarded, true) == TArray<EDiorama>({EDiorama::Root, EDiorama::Root}));
    TestTrue(TEXT("A guarded and a dangerous road"), OverviewLayout::Kinds(true, false, Mixed, true) == TArray<EDiorama>({EDiorama::Root, EDiorama::Vigil}));
    TestTrue(TEXT("A single dangerous road"), OverviewLayout::Kinds(true, false, Only, false) == TArray<EDiorama>({EDiorama::Vigil}));
    // The finale leads to the Vault whatever its roads, or to the Vigil when the set has no Vault diorama.
    TestTrue(TEXT("The finale draws the Vault for every road"), OverviewLayout::Kinds(true, true, Mixed, true) == TArray<EDiorama>({EDiorama::Vault, EDiorama::Vault}));
    TestTrue(TEXT("Without a Vault diorama the finale draws the Vigil as before"), OverviewLayout::Kinds(true, true, Mixed, false) == TArray<EDiorama>({EDiorama::Vigil, EDiorama::Vigil}));
    TestTrue(TEXT("No offers, no dioramas"), OverviewLayout::Kinds(true, false, {}, true).IsEmpty());

    // Placement on the title and on the r001 set: offer 0 screen-left (+X), offer 1 screen-right, a single road in the middle; the r002 row stands 8 cm further back on its deeper table.
    for (const bool bR002 : {false, true})
    {
        for (const bool bRoute : {false, true})
        {
            if (bR002 && bRoute) { continue; }
            const TArray<OverviewLayout::FPlacement> Two = OverviewLayout::Layout({EDiorama::Root, EDiorama::Vigil}, bR002, bRoute);
            if (!TestEqual(TEXT("Two placements"), Two.Num(), 2)) { return false; }
            TestEqual(TEXT("Offer 0 is screen-left"), Two[0].Offset.X, bR002 ? 50.0 : 52.0);
            TestEqual(TEXT("Offer 1 is screen-right"), Two[1].Offset.X, bR002 ? -34.0 : -52.0);
            TestEqual(TEXT("The row's depth"), Two[0].Offset.Y, bR002 ? 16.0 : 22.0);
            TestEqual(TEXT("Both stand at one depth"), Two[0].Offset.Y, Two[1].Offset.Y);
            TestTrue(TEXT("They turn a little toward each other"), Two[0].Yaw == -10.0f && Two[1].Yaw == 10.0f);
            TestTrue(TEXT("Each keeps its world, at full size"), Two[0].Kind == EDiorama::Root && Two[1].Kind == EDiorama::Vigil && Two[0].Scale == 1.0f && Two[1].Scale == 1.0f);
            const TArray<OverviewLayout::FPlacement> One = OverviewLayout::Layout({EDiorama::Vault}, bR002, bRoute);
            if (!TestEqual(TEXT("One placement"), One.Num(), 1)) { return false; }
            TestEqual(TEXT("A single road stands in the middle"), One[0].Offset.X, 0.0);
        }
    }
    // The r002 route and finale screens: each diorama beside its card on the lower table, scaled, the Vigil smaller; a lone offer takes the left spot.
    {
        const TArray<OverviewLayout::FPlacement> Side = OverviewLayout::Layout({EDiorama::Root, EDiorama::Vigil}, true, true);
        if (!TestEqual(TEXT("Two side placements"), Side.Num(), 2)) { return false; }
        TestTrue(TEXT("Offer 0 is left of the left card, offer 1 right of the right card"), Side[0].Offset.X > 0.0 && Side[1].Offset.X < 0.0 && Side[0].Offset.X == -Side[1].Offset.X);
        TestTrue(TEXT("Both are in front of the table's middle, on the lower table"), Side[0].Offset.Y == Side[1].Offset.Y && Side[0].Offset.Y < -100.0);
        TestTrue(TEXT("They are scaled to the open margin, the Vigil the smallest"), Side[0].Scale == 0.68f && Side[1].Scale == 0.62f && Side[0].Scale < 1.0f);
        TestEqual(TEXT("A lone offer takes the left spot"), OverviewLayout::Layout({EDiorama::Vault}, true, true)[0].Offset.X, 112.0);
        TestEqual(TEXT("The Vault is as large as the forest"), OverviewLayout::SideScale(EDiorama::Vault), OverviewLayout::SideScale(EDiorama::Root));
    }
    // The r001 set keeps its layout on every screen (the route flag changes nothing there).
    {
        const TArray<OverviewLayout::FPlacement> A = OverviewLayout::Layout({EDiorama::Root, EDiorama::Vigil}, false, false);
        const TArray<OverviewLayout::FPlacement> B = OverviewLayout::Layout({EDiorama::Root, EDiorama::Vigil}, false, true);
        TestTrue(TEXT("r001 placement is the same on the route screen"), A.Num() == 2 && B.Num() == 2 && A[0].Offset == B[0].Offset && A[1].Offset == B[1].Offset && A[0].Scale == B[0].Scale && A[1].Yaw == B[1].Yaw);
    }

    // The three candle lights: three of them, on the table, and r001's unchanged.
    const TArray<FVector> Old = OverviewLayout::Candles(false);
    const TArray<FVector> Lights = OverviewLayout::Candles(true);
    TestEqual(TEXT("Three lights on the r001 table"), Old.Num(), 3);
    TestEqual(TEXT("Three lights on the r002 table"), Lights.Num(), 3);
    TestTrue(TEXT("The r001 spots are unchanged"), Old[0] == FVector(-100, 30, 40) && Old[1] == FVector(100, 34, 40) && Old[2] == FVector(0, -70, 70));
    TestTrue(TEXT("The r002 cluster hangs on the candle cluster's flame"), Lights[2] == FVector(-55, -90, 15));
    const TArray<float> OldStrengths = OverviewLayout::CandleStrengths(false);
    const TArray<float> Strengths = OverviewLayout::CandleStrengths(true);
    TestTrue(TEXT("The r001 strengths are unchanged"), OldStrengths == TArray<float>({16.0f, 16.0f, 16.0f}));
    TestTrue(TEXT("One strength per light, the low cluster the dimmest"), Strengths.Num() == Lights.Num() && Strengths[2] == 4.0f && Strengths[0] == 16.0f && Strengths[1] == 16.0f);
    for (const FVector& Light : Lights) { TestTrue(TEXT("A candle light hangs above the table top, within the table"), Light.Z > 0.0f && Light.Z < 60.0f && FMath::Abs(Light.X) < 170.0f && Light.Y > -240.0f && Light.Y < 60.0f); }
    TestTrue(TEXT("The two back candles flank the dioramas"), Lights[0].X < -52.0f - 20.0f && Lights[1].X > 52.0f + 20.0f);
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOverviewSetTest, "Project.Expedition.Presentation.OverviewSetFallsBackAtomically",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FOverviewSetTest::RunTest(const FString& Parameters)
{
    // A fresh theme has no r002 set; an r002 set needs the flag and all four entries.
    UDiceRollTheme* Theme = NewObject<UDiceRollTheme>(GetTransientPackage());
    TestFalse(TEXT("A fresh theme's r002 overview is incomplete"), Theme->OverviewR002Complete());
    FillOverview(*Theme);
    TestTrue(TEXT("Every entry set and the flag on is a complete set"), Theme->OverviewR002Complete());
    FDiceRollOverview Set = Theme->LoadOverview();
    UStaticMesh* Cube = CubeMesh.LoadSynchronous();
    UStaticMesh* Sphere = SphereMesh.LoadSynchronous();
    if (!TestTrue(TEXT("The engine shapes load"), Cube && Sphere)) { return false; }
    TestTrue(TEXT("The r002 set is the one loaded"), Set.bR002 && Set.Table == Cube && Set.Root == Cube && Set.Vigil == Cube && Set.Vault == Cube);

    // Any one entry missing, dangling or not loadable sends the whole overview back to r001: never a mix, and the Vault road then has no diorama of its own.
    auto Falls = [&](const TCHAR* What, TFunctionRef<void(UDiceRollTheme&)> Break, bool bLoadFails)
    {
        UDiceRollTheme* Broken = NewObject<UDiceRollTheme>(GetTransientPackage());
        FillOverview(*Broken);
        Break(*Broken);
        if (bLoadFails) { AddExpectedMessage(TEXT("NoSuchOverviewMesh"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 0); }
        else { TestFalse(FString::Printf(TEXT("Without %s the set is incomplete"), What), Broken->OverviewR002Complete()); }
        const FDiceRollOverview Fallback = Broken->LoadOverview();
        TestFalse(FString::Printf(TEXT("Without %s the overview is r001"), What), Fallback.bR002);
        TestTrue(FString::Printf(TEXT("Without %s every piece is the r001 one"), What), Fallback.Table == Sphere && Fallback.Root == Sphere && Fallback.Vigil == Sphere);
        TestNull(FString::Printf(TEXT("Without %s the Vault has no diorama"), What), Fallback.Vault);
    };
    Falls(TEXT("the flag"), [](UDiceRollTheme& T) { T.bOverviewR002 = false; }, false);
    Falls(TEXT("the table"), [](UDiceRollTheme& T) { T.OverviewTabletop = nullptr; }, false);
    Falls(TEXT("the Root diorama"), [](UDiceRollTheme& T) { T.OverviewRootDiorama = nullptr; }, false);
    Falls(TEXT("the Vigil diorama"), [](UDiceRollTheme& T) { T.OverviewVigilDiorama = nullptr; }, false);
    Falls(TEXT("the Vault diorama"), [](UDiceRollTheme& T) { T.OverviewVaultDiorama = nullptr; }, false);
    Falls(TEXT("a dangling table"), [](UDiceRollTheme& T) { T.OverviewTabletop = Dangling; }, false);
    Falls(TEXT("a dangling Vault diorama"), [](UDiceRollTheme& T) { T.OverviewVaultDiorama = Dangling; }, false);
    // A package that exists but whose object does not load still counts as complete, and the load itself falls back as a set.
    Falls(TEXT("an unloadable Vigil diorama"), [](UDiceRollTheme& T) { T.OverviewVigilDiorama = Unloadable; }, true);

    // With no r001 table either, there is no overview at all.
    UDiceRollTheme* Bare = NewObject<UDiceRollTheme>(GetTransientPackage());
    TestNull(TEXT("No table at all, no overview"), Bare->LoadOverview().Table);

    // The shipped theme: the r002 set is on, complete and loads as itself; the r001 entries it falls back to are still there.
    const UDiceRollTheme* Shipped = OverviewShippedTheme();
    if (TestNotNull(TEXT("The shipped theme asset"), Shipped))
    {
        TestTrue(TEXT("The shipped theme's overview r002 is switched on (run the theme import)"), Shipped->bOverviewR002);
        TestTrue(TEXT("The shipped theme's r002 set is complete and every entry exists"), Shipped->OverviewR002Complete());
        const FDiceRollOverview Real = Shipped->LoadOverview();
        TestTrue(TEXT("It loads as the r002 set"), Real.bR002 && Real.Table && Real.Root && Real.Vigil && Real.Vault);
        if (Real.bR002)
        {
            TestEqual(TEXT("The war table"), Real.Table->GetName(), FString(TEXT("SM_Overview_Tabletop")));
            TestEqual(TEXT("The forest"), Real.Root->GetName(), FString(TEXT("SM_Overview_Diorama_Root")));
            TestEqual(TEXT("The cathedral"), Real.Vigil->GetName(), FString(TEXT("SM_Overview_Diorama_Vigil")));
            TestEqual(TEXT("The vault"), Real.Vault->GetName(), FString(TEXT("SM_Overview_Diorama_Vault")));
        }
        TestTrue(TEXT("The r001 fallback entries are kept"), Shipped->Tabletop.ToSoftObjectPath().ToString().EndsWith(TEXT("SM_Tabletop")) && !Shipped->RootDiorama.IsNull() && !Shipped->VigilDiorama.IsNull());
        UDiceRollTheme* Off = OverviewThemeCopy(Shipped);
        Off->bOverviewR002 = false;
        const FDiceRollOverview Old = Off->LoadOverview();
        TestTrue(TEXT("With the flag off the shipped theme draws the r001 set"), !Old.bR002 && Old.Table && Old.Root && Old.Vigil && !Old.Vault);
        if (Old.Table) { TestEqual(TEXT("The r001 table"), Old.Table->GetName(), FString(TEXT("SM_Tabletop"))); }
    }
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOverviewSceneTest, "Project.Expedition.Presentation.OverviewSceneIsBuiltFromTheSet",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FOverviewSceneTest::RunTest(const FString& Parameters)
{
    const UDiceRollTheme* Shipped = OverviewShippedTheme();
    if (!TestNotNull(TEXT("The shipped theme asset"), Shipped) || !TestTrue(TEXT("Its r002 overview is on (run the theme import)"), Shipped->OverviewR002Complete())) { return false; }
    const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation/OverviewScene"), FGuid::NewGuid().ToString());
    const FString Start = FPaths::Combine(Directory, TEXT("start.json"));
    const FString Finale = FPaths::Combine(Directory, TEXT("finale.json"));
    if (!TestTrue(TEXT("A route checkpoint at the start"), WriteRouteCheckpoint(Start, false))) { return false; }
    if (!TestTrue(TEXT("A route checkpoint before the finale"), WriteRouteCheckpoint(Finale, true))) { return false; }
    UWorld* World = MakeOverviewWorld();
    if (!TestNotNull(TEXT("Transient world"), World)) { return false; }
    const TArray<FName> R002Names = {TEXT("SM_Overview_Tabletop"), TEXT("SM_Overview_Diorama_Root"), TEXT("SM_Overview_Diorama_Vigil"), TEXT("SM_Overview_Diorama_Vault")};
    const FName OldTable = TEXT("SM_Tabletop"), OldRoot = TEXT("SM_Diorama_Root"), OldVigil = TEXT("SM_Diorama_Vigil");

    // The route screen at the start: the war table, a diorama for each offer, the three candle lights on the table's flames.
    if (AExpeditionBattleView* View = OverviewBoard(*this, World, Start))
    {
        if (TestTrue(TEXT("The route screen shows the overview"), View->ShowingOverview() && View->OverviewIsR002()))
        {
            const TArray<FName>& Drawn = View->OverviewMeshes();
            TestTrue(TEXT("The table is the r002 war table"), Drawn.Num() >= 2 && Drawn[0] == R002Names[0]);
            const TArray<bool> Dangerous = [&] { TArray<bool> Out; for (const FExpeditionRoute& Route : View->Progress().Offers) { Out.Add(Route.bDangerous); } return Out; }();
            TestEqual(TEXT("One diorama for each offer"), Drawn.Num(), 1 + Dangerous.Num());
            for (int32 Index = 0; Index < Dangerous.Num() && Index + 1 < Drawn.Num(); ++Index)
            { TestTrue(TEXT("Guarded roads draw the forest, dangerous roads the cathedral"), Drawn[Index + 1] == (Dangerous[Index] ? R002Names[2] : R002Names[1])); }
            TestTrue(TEXT("The candle lights are the r002 flames"), View->OverviewLights() == OverviewLayout::Candles(true));
        }
        // The same board with the r002 flag off: the r001 table and dioramas, the r001 lights; the table is still shown.
        TStrongObjectPtr<UDiceRollTheme> Off(OverviewThemeCopy(Shipped));
        Off->bOverviewR002 = false;
        View->UseTheme(Off.Get());
        View->Tick(0.016f);
        if (TestTrue(TEXT("With r002 off the overview is still shown"), View->ShowingOverview() && !View->OverviewIsR002()))
        {
            const TArray<FName>& Drawn = View->OverviewMeshes();
            TestTrue(TEXT("The r001 table"), Drawn.Num() >= 2 && Drawn[0] == OldTable);
            for (int32 Index = 1; Index < Drawn.Num(); ++Index) { TestTrue(TEXT("Only r001 dioramas"), Drawn[Index] == OldRoot || Drawn[Index] == OldVigil); }
            TestTrue(TEXT("The r001 lights"), View->OverviewLights() == OverviewLayout::Candles(false));
        }
        View->UsePrimitives();
        View->Tick(0.016f);
        TestFalse(TEXT("On primitives there is no overview"), View->ShowingOverview());
    }
    // The route screen before the finale: every offer leads to the Vault, so the Vault diorama stands on the table (the r001 set drew the Vigil one).
    if (AExpeditionBattleView* View = OverviewBoard(*this, World, Finale))
    {
        if (TestTrue(TEXT("The finale's route screen shows the overview"), View->ShowingOverview() && View->OverviewIsR002()))
        {
            const TArray<FName>& Drawn = View->OverviewMeshes();
            TestEqual(TEXT("One diorama for each offer"), Drawn.Num(), 1 + View->Progress().Offers.Num());
            for (int32 Index = 1; Index < Drawn.Num(); ++Index) { TestEqual(TEXT("The finale draws the Vault"), Drawn[Index], R002Names[3]); }
        }
        TStrongObjectPtr<UDiceRollTheme> Off(OverviewThemeCopy(Shipped));
        Off->bOverviewR002 = false;
        View->UseTheme(Off.Get());
        View->Tick(0.016f);
        const TArray<FName>& Drawn = View->OverviewMeshes();
        for (int32 Index = 1; Index < Drawn.Num(); ++Index) { TestEqual(TEXT("Without the r002 set the finale draws the Vigil diorama, as before"), Drawn[Index], OldVigil); }
    }
    DestroyOverviewWorld(World);
    IFileManager::Get().DeleteDirectory(*Directory, false, true);
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOverviewChoiceLeavesStateTest, "Project.Expedition.Presentation.OverviewChoiceLeavesStateUnchanged",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FOverviewChoiceLeavesStateTest::RunTest(const FString& Parameters)
{
    // The route screens drawn from the r002 set, the r001 set and primitives hold the same snapshot and write the same checkpoint bytes (ADR-0001): the overview
    // reads the run's route offers and nothing else, and never writes.
    const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation/OverviewState"), FGuid::NewGuid().ToString());
    const FString Source = FPaths::Combine(Directory, TEXT("finale.json"));
    if (!TestTrue(TEXT("A route checkpoint before the finale"), WriteRouteCheckpoint(Source, true))) { return false; }
    const UDiceRollTheme* Shipped = OverviewShippedTheme();
    if (!TestNotNull(TEXT("The shipped theme asset"), Shipped)) { return false; }
    FString Checkpoints[3];
    FExpeditionProgress Snapshots[3];
    for (int32 Variant = 0; Variant < 3; ++Variant)
    {
        UWorld* World = MakeOverviewWorld();
        if (!TestNotNull(TEXT("Transient world"), World)) { return false; }
        const FString Path = FPaths::Combine(Directory, FString::FromInt(Variant), TEXT("current.json"));
        IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
        IFileManager::Get().Copy(*Path, *Source);
        TStrongObjectPtr<UDiceRollTheme> Off(OverviewThemeCopy(Shipped));
        Off->bOverviewR002 = false;
        AExpeditionBattleView* View = World->SpawnActor<AExpeditionBattleView>();
        if (TestNotNull(TEXT("Battle view"), View))
        {
            View->ConfigureStorage(Path);
            View->DispatchBeginPlay();
            if (Variant == 1) { View->UseTheme(Off.Get()); }
            if (Variant == 2) { View->UsePrimitives(); }
            View->Resume();
            for (int32 Frame = 0; Frame < 5; ++Frame) { View->Tick(0.016f); }
            TestEqual(TEXT("The overview is drawn except on primitives"), View->ShowingOverview(), Variant != 2);
            Snapshots[Variant] = View->Progress();
            View->Save();
            FFileHelper::LoadFileToString(Checkpoints[Variant], *Path);
        }
        DestroyOverviewWorld(World);
    }
    for (int32 Variant = 1; Variant < 3; ++Variant)
    {
        TestTrue(TEXT("The route screen's run snapshot is identical"), FExpeditionProgress::StaticStruct()->CompareScriptStruct(&Snapshots[Variant], &Snapshots[0], 0));
        TestEqual(TEXT("The checkpoint bytes are identical"), Checkpoints[Variant], Checkpoints[0]);
    }
    TestFalse(TEXT("A checkpoint was written"), Checkpoints[0].IsEmpty());
    IFileManager::Get().DeleteDirectory(*Directory, false, true);
    return !HasAnyErrors();
}

namespace
{
/** The projected bounds, in design units, of one diorama as the view places it: the mesh's local box, scaled, turned toward the camera plus the row's yaw, moved to its spot. */
bool DioramaBounds(const UStaticMesh& Mesh, const OverviewLayout::FPlacement& Spot, double Width, double Height, ExpeditionLayout::Rect& Out)
{
    const FBox Local = Mesh.GetBoundingBox();
    const FRotator Turn(0.0f, -90.0f + Spot.Yaw, 0.0f);
    const double Scale = ExpeditionLayout::ViewScale(Width, Height);
    FVector2D Low(1e9, 1e9), High(-1e9, -1e9);
    for (int32 Corner = 0; Corner < 8; ++Corner)
    {
        const FVector Point((Corner & 1) ? Local.Max.X : Local.Min.X, (Corner & 2) ? Local.Max.Y : Local.Min.Y, (Corner & 4) ? Local.Max.Z : Local.Min.Z);
        const FVector OnTable = Turn.RotateVector(Point * Spot.Scale) + FVector(Spot.Offset.X, Spot.Offset.Y, 0.0);
        FVector2D Pixel;
        if (!OverviewLayout::Project(OnTable, Width, Height, Pixel)) { return false; }
        Low = FVector2D(FMath::Min(Low.X, Pixel.X), FMath::Min(Low.Y, Pixel.Y));
        High = FVector2D(FMath::Max(High.X, Pixel.X), FMath::Max(High.Y, Pixel.Y));
    }
    Out = {Low.X / Scale, Low.Y / Scale, High.X / Scale, High.Y / Scale};
    return true;
}

bool OverviewRectsOverlap(const ExpeditionLayout::Rect& A, const ExpeditionLayout::Rect& B, double Margin)
{
    return A.Left < B.Right + Margin && A.Right > B.Left - Margin && A.Top < B.Bottom + Margin && A.Bottom > B.Top - Margin;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOverviewHudClearTest, "Project.Expedition.Presentation.OverviewDioramasStayClearOfTheHud",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FOverviewHudClearTest::RunTest(const FString& Parameters)
{
    // The dioramas are the point of the overview, so on every screen that shows them (the title, the route screen, the finale's route screen with two Vault roads) and at both
    // capture resolutions, each one's projected bounds stay inside the viewport and clear of every HUD rectangle (the banner, both cards, the party cards, a hero's details, the
    // status strip, the Menu, the feedback line; on the title the drawers). The meshes are the shipped theme's r002 set.
    const UDiceRollTheme* Shipped = OverviewShippedTheme();
    if (!TestNotNull(TEXT("The shipped theme asset"), Shipped) || !TestTrue(TEXT("Its r002 overview is on (run the theme import)"), Shipped->OverviewR002Complete())) { return false; }
    const FDiceRollOverview Set = Shipped->LoadOverview();
    if (!TestTrue(TEXT("The r002 set loads"), Set.bR002 && Set.Root && Set.Vigil && Set.Vault)) { return false; }
    auto MeshOf = [&](EDiorama Kind) { return Kind == EDiorama::Vault ? Set.Vault : Kind == EDiorama::Vigil ? Set.Vigil : Set.Root; };
    struct FScreen { const TCHAR* Name; bool bHasRun; bool bFinale; TArray<bool> Dangerous; };
    const TArray<FScreen> Screens = {{TEXT("title"), false, false, {}}, {TEXT("route (guarded and dangerous)"), true, false, {false, true}}, {TEXT("route (two guarded roads)"), true, false, {false, false}},
        {TEXT("route (two dangerous roads)"), true, false, {true, true}}, {TEXT("finale (two Vault roads)"), true, true, {false, true}}};
    constexpr double Margin = 4.0, EdgeInset = 12.0;
    for (const FIntPoint& Resolution : {FIntPoint(1920, 1080), FIntPoint(1280, 720)})
    {
        const double Scale = ExpeditionLayout::ViewScale(Resolution.X, Resolution.Y);
        const double W = Resolution.X / Scale, H = Resolution.Y / Scale;
        for (const FScreen& Screen : Screens)
        {
            const TArray<EDiorama> Kinds = OverviewLayout::Kinds(Screen.bHasRun, Screen.bFinale, Screen.Dangerous, true);
            const TArray<OverviewLayout::FPlacement> Spots = OverviewLayout::Layout(Kinds, true, Screen.bHasRun);
            const TArray<ExpeditionLayout::Rect> Hud = OverviewLayout::HudRects(Screen.bHasRun, W, H);
            TArray<ExpeditionLayout::Rect> Placed;
            for (int32 Index = 0; Index < Spots.Num(); ++Index)
            {
                ExpeditionLayout::Rect Box{};
                const FString Label = FString::Printf(TEXT("%s, %dx%d, offer %d"), Screen.Name, Resolution.X, Resolution.Y, Index);
                if (!TestTrue(Label + TEXT(": the bounds project"), DioramaBounds(*MeshOf(Spots[Index].Kind), Spots[Index], Resolution.X, Resolution.Y, Box))) { continue; }
                const FString Where = FString::Printf(TEXT(" (bounds %.0f,%.0f to %.0f,%.0f of %.0f x %.0f)"), Box.Left, Box.Top, Box.Right, Box.Bottom, W, H);
                TestTrue(Label + TEXT(": inside the viewport with a margin") + Where, Box.Left >= EdgeInset && Box.Top >= EdgeInset && Box.Right <= W - EdgeInset && Box.Bottom <= H - EdgeInset);
                for (int32 Rect = 0; Rect < Hud.Num(); ++Rect)
                { TestFalse(FString::Printf(TEXT("%s: clear of HUD rect %d%s"), *Label, Rect, *Where), OverviewRectsOverlap(Box, Hud[Rect], Margin)); }
                for (const ExpeditionLayout::Rect& Other : Placed) { TestFalse(Label + TEXT(": clear of the other diorama"), OverviewRectsOverlap(Box, Other, 0.0)); }
                Placed.Add(Box);
                // At least 150 x 120 design units: a diorama must still read as its world.
                TestTrue(Label + TEXT(": big enough to read"), Box.Right - Box.Left >= 150.0 && Box.Bottom - Box.Top >= 100.0);
            }
        }
    }
    // On the route and finale screens the war map (centre 1.66 m toward the viewer, 1.05 x 0.63 m) lies between the two dioramas: they do not overlap its box on the table.
    for (const bool bFinale : {false, true})
    {
        const TArray<EDiorama> Kinds = OverviewLayout::Kinds(true, bFinale, {false, true}, true);
        for (const OverviewLayout::FPlacement& Spot : OverviewLayout::Layout(Kinds, true, true))
        {
            const FBox Local = MeshOf(Spot.Kind)->GetBoundingBox();
            const FRotator Turn(0.0f, -90.0f + Spot.Yaw, 0.0f);
            double Nearest = 1e9;
            for (int32 Corner = 0; Corner < 4; ++Corner)
            {
                const FVector Point((Corner & 1) ? Local.Max.X : Local.Min.X, (Corner & 2) ? Local.Max.Y : Local.Min.Y, 0.0);
                const FVector OnTable = Turn.RotateVector(Point * Spot.Scale) + FVector(Spot.Offset.X, Spot.Offset.Y, 0.0);
                Nearest = FMath::Min(Nearest, FMath::Abs(OnTable.X) );   // table-top X is screen-left; the map spans |X| < 52.5 cm (1.05 m wide) about the middle
            }
            TestTrue(TEXT("A diorama's tray stays beside the war map, not on it"), Nearest > 52.5 + 5.0);
        }
    }
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOverviewCameraConstantsTest, "Project.Expedition.Presentation.OverviewLayoutProjectsLikeTheCamera",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FOverviewCameraConstantsTest::RunTest(const FString& Parameters)
{
    // The layout test projects with OverviewLayout::Project; this proves it is the overview camera's own projection: the view's camera pose and field of view are the
    // constants, and Unreal's projection of a few table points lands where Project says (both resolutions).
    const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation/OverviewCamera"), FGuid::NewGuid().ToString());
    const FString Start = FPaths::Combine(Directory, TEXT("start.json"));
    if (!TestTrue(TEXT("A route checkpoint at the start"), WriteRouteCheckpoint(Start, false))) { return false; }
    UWorld* World = MakeOverviewWorld();
    if (!TestNotNull(TEXT("Transient world"), World)) { return false; }
    if (AExpeditionBattleView* View = OverviewBoard(*this, World, Start))
    {
        View->SnapCameraToTargets();
        View->Tick(0.016f);
        if (TestTrue(TEXT("The overview is shown"), View->ShowingOverview()))
        {
            const FTransform Camera = View->CameraTransform();
            const FVector Table(0.0, 0.0, 80.0);
            TestTrue(TEXT("The eye is where OverviewLayout says"), Camera.GetLocation().Equals(Table + OverviewLayout::CameraEye(), 0.5));
            TestTrue(TEXT("The aim is where OverviewLayout says"), Camera.GetRotation().GetForwardVector().Equals(OverviewLayout::CameraLook().GetSafeNormal(), 0.001));
            TestEqual(TEXT("The horizontal field of view"), static_cast<double>(View->CameraFieldOfView()), OverviewLayout::CameraFieldOfView, 0.001);
            for (const FIntPoint& Resolution : {FIntPoint(1920, 1080), FIntPoint(1280, 720)})
            {
                const FMatrix ViewRotation = FInverseRotationMatrix(Camera.Rotator()) * FMatrix(FPlane(0, 0, 1, 0), FPlane(1, 0, 0, 0), FPlane(0, 1, 0, 0), FPlane(0, 0, 0, 1));
                const FMatrix ViewMatrix = FTranslationMatrix(-Camera.GetLocation()) * ViewRotation;
                const FMatrix Projection = FReversedZPerspectiveMatrix(FMath::DegreesToRadians(View->CameraFieldOfView() * 0.5f), static_cast<float>(Resolution.X), static_cast<float>(Resolution.Y), 10.0f);
                for (const FVector& Point : {FVector(0, 0, 0), FVector(112, -166, 20), FVector(-112, -166, 40), FVector(-95, -60, 12), FVector(52, 22, 30)})
                {
                    FVector2D Real, Mine;
                    if (!FSceneView::ProjectWorldToScreen(Table + Point, FIntRect(0, 0, Resolution.X, Resolution.Y), ViewMatrix * Projection, Real) || !OverviewLayout::Project(Point, Resolution.X, Resolution.Y, Mine))
                    { AddError(TEXT("A table point does not project")); continue; }
                    TestTrue(FString::Printf(TEXT("%dx%d: projects like the camera (%s: %.1f, %.1f vs %.1f, %.1f)"), Resolution.X, Resolution.Y, *Point.ToString(), Mine.X, Mine.Y, Real.X, Real.Y), Mine.Equals(Real, 1.5));
                }
            }
        }
    }
    DestroyOverviewWorld(World);
    IFileManager::Get().DeleteDirectory(*Directory, false, true);
    return !HasAnyErrors();
}
#endif
