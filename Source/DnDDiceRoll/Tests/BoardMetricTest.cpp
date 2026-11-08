#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "Components/PrimitiveComponent.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Persistence/ExpeditionSave.h"
#include "Presentation/BoardMetric.h"
#include "Presentation/DiceRollTheme.h"
#include "Presentation/ExpeditionBattleView.h"
#include "ExpeditionTestSupport.h"

namespace
{
UWorld* MakeWorld()
{
    const UWorld::InitializationValues Values = UWorld::InitializationValues()
        .AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false);
    return UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
}

// Writes a checkpoint at the first encounter whose board is Width x Height (the capture fixtures search the same way).
bool WriteBoardCheckpoint(const FString& Path, int32 Width, int32 Height)
{
    for (int32 Seed = 1; Seed < 2000; ++Seed)
    {
        FString Error;
        FExpeditionRun Run;
        Run.NewRun(Seed, Error, FString::Printf(TEXT("metric-%d"), Seed));
        for (int32 Offer = 0; Offer < Run.Progress().Offers.Num(); ++Offer)
        {
            const FExpeditionRoute& Route = Run.Progress().Offers[Offer];
            if (Route.Width != Width || Route.Height != Height) { continue; }
            if (!Run.ChooseRoute(Route.Id, Error)) { continue; }
            FExpeditionSaveData Data;
            Data.Current = Run.Snapshot();
            return FExpeditionSave::Write(Path, Data, Error);
        }
    }
    return false;
}

struct FSeededOutcome
{
    FCombatSnapshot Battle;
    FString Checkpoint;
    int32 Commands = 0;
};

// Plays a fixed, rules-only policy through the view, then reads back the checkpoint it wrote.
// Variant 0 keeps the theme's metric; others change the presentation before the run starts.
FSeededOutcome PlaySeeded(FAutomationTestBase& Test, UWorld* World, const FString& Directory, int32 Variant)
{
    FSeededOutcome Outcome;
    AExpeditionBattleView* View = World->SpawnActor<AExpeditionBattleView>();
    if (!Test.TestNotNull(TEXT("Battle view"), View)) { return Outcome; }
    const FString Path = FPaths::Combine(Directory, FString::FromInt(Variant), TEXT("current.json"));
    View->ConfigureStorage(Path);
    View->DispatchBeginPlay();
    if (Variant == 1) { View->SetBoardMetric(FBoardMetric(100.0f, 170.0f)); }
    else if (Variant == 2) { View->SetBoardMetric(FBoardMetric(220.0f, 185.0f)); }
    else if (Variant == 3) { View->UsePrimitives(); }
    View->NewRun();
    View->ChooseRoute(0);
    for (int32 Step = 0; Step < 80 && View->InCombat(); ++Step)
    {
        const int32 Id = View->Rules().ActiveId();
        const FCombatantState* Active = View->Rules().Find(Id);
        if (!Active) { break; }
        if (Active->Side == ECombatSide::Enemy)
        {
            View->Tick(1.0f);
            View->SkipPresentation();
            continue;
        }
        const TArray<int32> Targets = View->Rules().LegalTargets(Id);
        const TArray<FIntPoint> Moves = View->Rules().LegalMoves(Id);
        if (!Targets.IsEmpty()) { View->Execute({ECombatCommandType::Attack, Id, {}, Targets[0]}); ++Outcome.Commands; }
        else if (!Moves.IsEmpty()) { View->Execute({ECombatCommandType::Move, Id, Moves[0]}); ++Outcome.Commands; }
        View->SkipPresentation();
        if (View->InCombat() && View->Rules().ActiveId() == Id) { View->Execute({ECombatCommandType::EndTurn, Id}); ++Outcome.Commands; View->SkipPresentation(); }
    }
    View->Save();
    Outcome.Battle = View->Rules().Snapshot();
    FFileHelper::LoadFileToString(Outcome.Checkpoint, *Path);
    return Outcome;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBoardMetricSourceTest, "Project.Expedition.Presentation.BoardMetricSingleSource",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FBoardMetricSourceTest::RunTest(const FString& Parameters)
{
    const FBoardMetric Default;
    TestEqual(TEXT("The default cell is 150 cm"), Default.CellCm, 150.0f);
    TestTrue(TEXT("The default figure is 175 to 185 cm"), Default.FigureCm >= 175.0f && Default.FigureCm <= 185.0f);
    TestEqual(TEXT("The theme defaults to the same cell"), GetDefault<UDiceRollTheme>()->CellSize, 150.0f);
    TestTrue(TEXT("The theme figure is 175 to 185 cm"), GetDefault<UDiceRollTheme>()->FigureHeight >= 175.0f && GetDefault<UDiceRollTheme>()->FigureHeight <= 185.0f);
    TestEqual(TEXT("A 110 cm reference length is unchanged at 110 cm"), FBoardMetric(110.0f, 180.0f).Scaled(60.0f), 60.0f);
    TestEqual(TEXT("Reference lengths scale with the cell"), Default.Scaled(110.0f), 150.0f);
    TestEqual(TEXT("The board top keeps its r001 proportion"), Default.BoardTop(), 31.0f * 150.0f / 110.0f);
    TestTrue(TEXT("The pointer plane is above the board top"), Default.CursorPlane() > Default.BoardTop());

    UWorld* World = MakeWorld();
    if (!TestNotNull(TEXT("Transient world"), World)) { return false; }
    const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation/BoardMetric"), FGuid::NewGuid().ToString());
    AExpeditionBattleView* View = World->SpawnActor<AExpeditionBattleView>();
    if (TestNotNull(TEXT("Battle view"), View))
    {
        View->ConfigureStorage(FPaths::Combine(Directory, TEXT("current.json")));
        View->DispatchBeginPlay();
        View->NewRun();
        View->ChooseRoute(0);
        View->Tick(0.016f);
        for (const bool bPrimitives : {false, true})
        {
            if (bPrimitives) { View->UsePrimitives(); }
            else { TestTrue(TEXT("The shipped theme loads, so the themed pass is real"), View->UsesTheme()); }
            const FString Mode = bPrimitives ? TEXT("primitive") : TEXT("themed");
            View->Tick(0.016f);
            TestEqual(*(Mode + TEXT(": the view's metric defaults to 150 cm")), View->Metric().CellCm, 150.0f);
            const float Step = static_cast<float>(View->CellPosition(FIntPoint(4, 2)).X - View->CellPosition(FIntPoint(3, 2)).X);
            TestEqual(*(Mode + TEXT(": cell spacing is the metric's cell size")), Step, View->Metric().CellCm);
            TestEqual(*(Mode + TEXT(": rows are spaced the same")), static_cast<float>(View->CellPosition(FIntPoint(1, 5)).Y - View->CellPosition(FIntPoint(1, 4)).Y), View->Metric().CellCm);
            TestEqual(*(Mode + TEXT(": board positions sit on the board top")), static_cast<float>(View->BoardPosition(FVector2D(2.5f, 1.5f)).Z), View->Metric().BoardTop());
            TestEqual(*(Mode + TEXT(": the pointer plane is the metric's")), static_cast<float>(View->CellPosition(FIntPoint::ZeroValue).Z), View->Metric().CursorPlane());
            for (const FCombatantState& Unit : View->Rules().Snapshot().Encounter.Combatants)
            {
                TestTrue(FString::Printf(TEXT("%s: %s stands on its cell"), *Mode, *Unit.Name),
                    View->UnitPosition(Unit.Id).Equals(View->BoardPosition(FVector2D(Unit.Cell.X, Unit.Cell.Y)), 0.5f));
            }
            // Rig roots stand on their cells; the highest part of each standing figure is the metric's figure height.
            TArray<USceneComponent*> Components;
            View->GetComponents(Components);
            int32 Measured = 0;
            for (const FCombatantState& Unit : View->Rules().Snapshot().Encounter.Combatants)
            {
                if (Unit.Life != ECombatLife::Conscious) { continue; }
                const FVector Cell = View->CellPosition(Unit.Cell);
                for (USceneComponent* Component : Components)
                {
                    if (Component->GetAttachParent() != View->GetRootComponent() || Component->GetFName() == TEXT("Scenery") ||
                        Component->GetNumChildrenComponents() < 2 || !FVector2D(Component->GetRelativeLocation()).Equals(FVector2D(Cell), 0.1f)) { continue; }
                    TArray<USceneComponent*> Children;
                    Component->GetChildrenComponents(true, Children);
                    double Highest = View->Metric().BoardTop();
                    for (USceneComponent* Child : Children)
                    {
                        const UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(Child);
                        if (Primitive && Primitive->IsVisible()) { Highest = FMath::Max(Highest, Primitive->Bounds.GetBox().Max.Z); }
                    }
                    const double Height = Highest - View->Metric().BoardTop();
                    TestTrue(FString::Printf(TEXT("%s: %s stands about 180 cm (measured %.1f)"), *Mode, *Unit.Name, Height), Height >= 170.0f && Height <= 192.0f);
                    ++Measured;
                    break;
                }
            }
            TestTrue(*(Mode + TEXT(": at least one figure was measured")), Measured > 0);
        }
        // The metric is the only place the cell size lives: changing it moves every conversion.
        View->SetBoardMetric(FBoardMetric(200.0f, 180.0f));
        TestEqual(TEXT("A 200 cm metric spaces cells 200 cm apart"), static_cast<float>(View->CellPosition(FIntPoint(4, 2)).X - View->CellPosition(FIntPoint(3, 2)).X), 200.0f);
        TestEqual(TEXT("It moves the board top"), static_cast<float>(View->BoardPosition(FVector2D::ZeroVector).Z), View->Metric().BoardTop());
        FIntPoint Cell;
        const FVector Aim = View->CellPosition(FIntPoint(5, 3));
        TestTrue(TEXT("Picking follows the new metric"), View->CellAtRay(Aim + FVector(30, 10, 600), FVector(-30.0f, -10.0f, -600.0f).GetSafeNormal(), Cell) && Cell == FIntPoint(5, 3));
    }
    World->DestroyWorld(false);
    IFileManager::Get().DeleteDirectory(*Directory, false, true);
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBoardPickingRoundTripTest, "Project.Expedition.Presentation.BoardPickingRoundTrip",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FBoardPickingRoundTripTest::RunTest(const FString& Parameters)
{
    const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation/BoardPicking"), FGuid::NewGuid().ToString());
    for (const FIntPoint Board : {FIntPoint(16, 11), FIntPoint(18, 12)})
    {
        const FString Path = FPaths::Combine(Directory, FString::Printf(TEXT("%dx%d"), Board.X, Board.Y), TEXT("current.json"));
        if (!TestTrue(FString::Printf(TEXT("A seed gives a %dx%d board"), Board.X, Board.Y), WriteBoardCheckpoint(Path, Board.X, Board.Y))) { continue; }
        for (const bool bPrimitives : {false, true})
        {
            UWorld* World = MakeWorld();
            if (!TestNotNull(TEXT("Transient world"), World)) { continue; }
            AExpeditionBattleView* View = World->SpawnActor<AExpeditionBattleView>();
            if (TestNotNull(TEXT("Battle view"), View))
            {
                View->ConfigureStorage(Path);
                View->DispatchBeginPlay();
                View->Resume();
                if (bPrimitives) { View->UsePrimitives(); }
                else { TestTrue(TEXT("The shipped theme loads, so the themed pass is real"), View->UsesTheme()); }
                View->Tick(0.016f);
                const FCombatEncounter& Encounter = View->Rules().Snapshot().Encounter;
                TestEqual(TEXT("Board width"), Encounter.Width, Board.X);
                TestEqual(TEXT("Board height"), Encounter.Height, Board.Y);
                const float Cell = View->Metric().CellCm;
                // Centre, the four corners and the four edge midpoints, each well inside the cell.
                const FVector2D Inside[] = {{0, 0}, {0.45f, 0.45f}, {-0.45f, 0.45f}, {0.45f, -0.45f}, {-0.45f, -0.45f},
                    {0.45f, 0}, {-0.45f, 0}, {0, 0.45f}, {0, -0.45f}};
                for (const float Yaw : {-90.0f, 0.0f, 90.0f, 180.0f})
                {
                    View->SnapCamera(Yaw, View->FramedDistance());
                    const FVector Eye = View->CameraTransform().GetLocation();
                    int32 Wrong = 0, Checked = 0;
                    for (int32 Y = 0; Y < Encounter.Height; ++Y)
                    {
                        for (int32 X = 0; X < Encounter.Width; ++X)
                        {
                            const FIntPoint Expected(X, Y);
                            for (const FVector2D& Offset : Inside)
                            {
                                const FVector Aim = View->CellPosition(Expected) + FVector(Offset.X * Cell, Offset.Y * Cell, 0.0f);
                                FIntPoint Picked;
                                ++Checked;
                                if (!View->CellAtRay(Eye, (Aim - Eye).GetSafeNormal(), Picked) || Picked != Expected) { ++Wrong; }
                            }
                        }
                    }
                    TestEqual(FString::Printf(TEXT("%dx%d %s yaw %.0f: every cell and near-edge point picks its own cell (%d points)"),
                        Board.X, Board.Y, bPrimitives ? TEXT("primitive") : TEXT("themed"), Yaw, Checked), Wrong, 0);
                    // Just outside the board nothing is picked.
                    for (const FIntPoint Outside : {FIntPoint(-1, 0), FIntPoint(Encounter.Width, 0), FIntPoint(0, -1), FIntPoint(0, Encounter.Height)})
                    {
                        FIntPoint Picked;
                        const FVector Aim = View->CellPosition(Outside);
                        TestFalse(TEXT("A point past the edge picks no cell"), View->CellAtRay(Eye, (Aim - Eye).GetSafeNormal(), Picked));
                    }
                    // cell -> world -> cell, also without the camera.
                    TestTrue(TEXT("A cell centre maps back through the metric"), View->Metric().CellAt(View->CellPosition(FIntPoint(Board.X - 1, Board.Y - 1))) == FIntPoint(Board.X - 1, Board.Y - 1));
                }
            }
            World->DestroyWorld(false);
        }
    }
    IFileManager::Get().DeleteDirectory(*Directory, false, true);
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMetricKeepsStateTest, "Project.Expedition.Presentation.BoardMetricLeavesStateUnchanged",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FMetricKeepsStateTest::RunTest(const FString& Parameters)
{
    // The same seeded run, driven only by rules commands, under different presentation metrics and the
    // primitive fallback. Snapshots and checkpoint bytes must be identical: the metric is presentation only.
    UWorld* World = MakeWorld();
    if (!TestNotNull(TEXT("Transient world"), World)) { return false; }
    const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation/BoardMetricState"), FGuid::NewGuid().ToString());
    const FSeededOutcome Reference = PlaySeeded(*this, World, Directory, 0);
    TestTrue(TEXT("The run played commands"), Reference.Commands > 0);
    TestFalse(TEXT("The run wrote a checkpoint"), Reference.Checkpoint.IsEmpty());
    for (const int32 Variant : {1, 2, 3})
    {
        const FSeededOutcome Other = PlaySeeded(*this, World, Directory, Variant);
        TestEqual(FString::Printf(TEXT("Variant %d plays the same number of commands"), Variant), Other.Commands, Reference.Commands);
        TestTrue(FString::Printf(TEXT("Variant %d: battle snapshot is identical"), Variant),
            FCombatSnapshot::StaticStruct()->CompareScriptStruct(&Reference.Battle, &Other.Battle, 0));
        TestTrue(FString::Printf(TEXT("Variant %d: checkpoint bytes are identical"), Variant), Other.Checkpoint == Reference.Checkpoint);
    }
    World->DestroyWorld(false);
    IFileManager::Get().DeleteDirectory(*Directory, false, true);
    return !HasAnyErrors();
}
#endif
