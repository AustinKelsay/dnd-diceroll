#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "RenderingThread.h"
#include "UObject/GarbageCollection.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Presentation/ExpeditionBattleView.h"
#include "Presentation/HitFlash.h"

// Hit and spell flashes (#189): bounded tints for figure parts and skin slots, a bounded rim, and a bolt light that does not clip the floor.

namespace
{
float MaxChannel(const FLinearColor& Color) { return FMath::Max3(Color.R, Color.G, Color.B); }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHitFlashTintTest, "Project.Expedition.Presentation.HitFlashTint",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FHitFlashTintTest::RunTest(const FString& Parameters)
{
    using namespace ExpeditionFlash;
    const FLinearColor Bases[] = {FLinearColor::Black, FLinearColor::White, FLinearColor(0.2f, 0.3f, 0.5f), FLinearColor(1.0f, 0.2f, 0.1f), FLinearColor(2.0f, 1.5f, 1.0f)};
    for (const FLinearColor& Base : Bases)
    {
        TestTrue(TEXT("No flash and no glow leave the base alone"), Tint(Base, 0.0f, 0.0f).Equals(Base, 0.0001f));
        for (int32 Step = 0; Step <= 10; ++Step)
        {
            for (int32 GlowStep = 0; GlowStep <= 10; ++GlowStep)
            {
                const FLinearColor Shown = Tint(Base, GlowStep / 10.0f, Step / 10.0f);
                const float Limit = FMath::Max(MaxChannel(Base), Ceiling) + 0.0001f;
                TestTrue(FString::Printf(TEXT("Flash %d glow %d stays within the ceiling"), Step, GlowStep), MaxChannel(Shown) <= Limit);
                TestTrue(TEXT("Alpha is kept"), FMath::IsNearlyEqual(Shown.A, Base.A));
            }
        }
        // A full flash on any ordinary base (at most 1) never exceeds the ceiling, and out-of-range inputs are clamped.
        if (MaxChannel(Base) <= 1.0f) { TestTrue(TEXT("A full flash stays within the ceiling"), MaxChannel(Tint(Base, 1.0f, 1.0f)) <= Ceiling + 0.0001f); }
        TestTrue(TEXT("Flash is clamped"), Tint(Base, 0.0f, 5.0f).Equals(Tint(Base, 0.0f, 1.0f), 0.0001f));
    }
    TestTrue(TEXT("A flash brightens a dark part"), MaxChannel(Tint(FLinearColor(0.2f, 0.2f, 0.2f), 0.0f, 1.0f)) > 0.5f);
    TestTrue(TEXT("The old HDR flash was far over the ceiling"), 3.0f > Ceiling * 1.9f);

    // The rim: Rim * RimColor stays within the ceiling unless the base look already exceeded it.
    for (const float Rim : {0.0f, 0.8f, 1.6f, 3.0f})
    {
        const FLinearColor BaseRim(0.45f, 0.55f, 0.85f);
        TestTrue(TEXT("No flash keeps the rim"), RimColor(BaseRim, Rim, 0.0f).Equals(BaseRim, 0.0001f));
        for (int32 Step = 0; Step <= 10; ++Step)
        {
            const FLinearColor Shown = RimColor(BaseRim, Rim, Step / 10.0f);
            TestTrue(FString::Printf(TEXT("Rim %.1f at flash %d is bounded"), Rim, Step), Rim * MaxChannel(Shown) <= FMath::Max(Rim * MaxChannel(BaseRim), Ceiling) + 0.0001f);
        }
        TestTrue(TEXT("A full flash turns the rim warm"), RimColor(BaseRim, Rim, 1.0f).R > RimColor(BaseRim, Rim, 1.0f).B);
    }

    // The bolt light: full in flight, faded to a small landing value at the target, shared between a Magic Missile's bolts.
    TestEqual(TEXT("A lone bolt mid-flight carries the flight light"), BoltLight(1, 0.5f), BoltLightCandelas);
    TestEqual(TEXT("A lone bolt at its target carries the landing light"), BoltLight(1, 1.0f), BoltLandingCandelas);
    TestEqual(TEXT("Just before it lands it is still at the landing light"), BoltLight(1, 0.92f), BoltLandingCandelas);
    float Last = BoltLight(1, 0.0f);
    for (int32 Step = 1; Step <= 20; ++Step)
    {
        const float Now = BoltLight(1, Step / 20.0f);
        TestTrue(TEXT("The light never rises on the way to the target"), Now <= Last + 0.0001f);
        Last = Now;
    }
    TestTrue(TEXT("Three bolts carry less than one"), 3.0f * BoltLight(3, 0.5f) < BoltLight(1, 0.5f));
    TestTrue(TEXT("The flight light is far under the old 180 cd"), BoltLightCandelas < 60.0f);
    TestTrue(TEXT("The landing light is a small share of the flight light"), BoltLandingCandelas < 0.25f * BoltLightCandelas);
    TestTrue(TEXT("Bolt and spark colour multiples are not HDR"), BoltEmissive <= 1.0f && SparkEmissive <= 1.0f);
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHitFlashBoardTest, "Project.Expedition.Presentation.HitFlashBoard",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FHitFlashBoardTest::RunTest(const FString& Parameters)
{
    const UWorld::InitializationValues Values = UWorld::InitializationValues()
        .AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
    if (!TestNotNull(TEXT("Transient world"), World)) { return false; }
    const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation/HitFlash"), FGuid::NewGuid().ToString());
    AExpeditionBattleView* View = World->SpawnActor<AExpeditionBattleView>();
    if (TestNotNull(TEXT("Battle view"), View))
    {
        View->ConfigureStorage(FPaths::Combine(Directory, TEXT("current.json")));
        View->DispatchBeginPlay();
        View->NewRun();
        View->ChooseRoute(0);
        View->HoldEnemyTurns(true);
        View->Tick(0.016f);
        View->Tick(0.016f);
        int32 Checked = 0, Skinned = 0;
        for (const FCombatantState& Unit : View->Rules().Snapshot().Encounter.Combatants)
        {
            TArray<FLinearColor> Before, During;
            FLinearColor RingBefore, RingDuring;
            if (!View->UnitShownColors(Unit.Id, Before, RingBefore)) { continue; }
            View->PreviewImpact(Unit.Id, EImpactPreview::Fire, FVector2D(Unit.Cell.X - 2, Unit.Cell.Y));
            View->Tick(0.016f);
            View->UnitShownColors(Unit.Id, During, RingDuring);
            if (!TestEqual(TEXT("Same parts under a flash"), During.Num(), Before.Num())) { continue; }
            const bool bSkinned = View->UnitAnimation(Unit.Id).bSkeletal;
            Skinned += bSkinned ? 1 : 0;
            for (int32 Part = 0; Part < During.Num(); ++Part)
            {
                // Parts and skin slots alike: never brighter than the ceiling, or than they were without the flash if that was already over it.
                TestTrue(FString::Printf(TEXT("%s part %d%s is within the flash ceiling"), *Unit.Name, Part, bSkinned ? TEXT(" (skin)") : TEXT("")),
                    MaxChannel(During[Part]) <= FMath::Max(MaxChannel(Before[Part]), ExpeditionFlash::Ceiling) + 0.01f);
            }
            TestTrue(TEXT("The ring does not take the hit flash"), RingDuring.Equals(RingBefore, 0.01f));
            View->ClearImpactPreview();
            ++Checked;
        }
        TestTrue(TEXT("Some figures were checked"), Checked > 0);
        AddInfo(FString::Printf(TEXT("%d figures checked, %d of them rigged"), Checked, Skinned));
    }
    World->DestroyWorld(false);
    FlushRenderingCommands();
    CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
    IFileManager::Get().DeleteDirectory(*Directory, false, true);
    return !HasAnyErrors();
}
#endif
