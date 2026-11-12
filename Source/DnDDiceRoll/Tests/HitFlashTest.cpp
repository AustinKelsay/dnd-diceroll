#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "RenderingThread.h"
#include "UObject/GarbageCollection.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Presentation/ExpeditionBattleView.h"
#include "Presentation/ExpeditionPlayback.h"
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
    // The light ramps in from nothing at the caster over the first BoltRampEnd of the path (#150 batch 3), then never rises on the way to the target.
    TestEqual(TEXT("A bolt on the caster's own cell carries no light"), BoltLight(1, 0.0f), 0.0f);
    float Last = BoltLight(1, BoltRampEnd);
    for (int32 Step = 1; Step <= 20; ++Step)
    {
        const float Alpha = Step / 20.0f;
        const float Now = BoltLight(1, Alpha);
        if (Alpha <= BoltRampEnd) { TestTrue(TEXT("The light rises over the ramp"), Now >= BoltLight(1, (Step - 1) / 20.0f) - 0.0001f); }
        else
        {
            TestTrue(TEXT("The light never rises after the ramp, on the way to the target"), Now <= Last + 0.0001f);
            Last = Now;
        }
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

// The production wiring of the bolt lights (#189): UpdateEffects must light each live bolt from ExpeditionFlash::BoltLight, in flight and at landing,
// with a Magic Missile's bolts staggered as AddAttack throws them (one, two and three in flight). Reading the real point lights means a literal 180 cd
// (or 540 for a volley) put back in UpdateEffects fails here, not only in the helper's own test.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHitFlashBoltLightTest, "Project.Expedition.Presentation.HitFlashBoltLights",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FHitFlashBoltLightTest::RunTest(const FString& Parameters)
{
    const UWorld::InitializationValues Values = UWorld::InitializationValues()
        .AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
    if (!TestNotNull(TEXT("Transient world"), World)) { return false; }
    const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation/HitFlashLights"), FGuid::NewGuid().ToString());
    AExpeditionBattleView* View = World->SpawnActor<AExpeditionBattleView>();
    if (TestNotNull(TEXT("Battle view"), View))
    {
        View->ConfigureStorage(FPaths::Combine(Directory, TEXT("current.json")));
        View->DispatchBeginPlay();
        View->NewRun();
        View->ChooseRoute(0);
        View->HoldEnemyTurns(true);
        View->Tick(0.016f);
        int32 TargetId = INDEX_NONE;
        FVector2D From = FVector2D::ZeroVector;
        for (const FCombatantState& Unit : View->Rules().Snapshot().Encounter.Combatants)
        {
            if (Unit.Side == ECombatSide::Enemy && Unit.Life == ECombatLife::Conscious) { TargetId = Unit.Id; From = FVector2D(Unit.Cell.X - 3, Unit.Cell.Y); break; }
        }
        if (TestTrue(TEXT("An enemy to strike"), TargetId != INDEX_NONE))
        {
            struct FCase { const TCHAR* Name; EImpactPreview Effect; EImpactMoment Moment; int32 Live; };
            const FCase Cases[] = {
                {TEXT("fire at release"), EImpactPreview::Fire, EImpactMoment::Release, 1},
                {TEXT("spell at release"), EImpactPreview::Spell, EImpactMoment::Release, 1},
                {TEXT("fire in flight"), EImpactPreview::Fire, EImpactMoment::Flight, 1},
                {TEXT("fire landing"), EImpactPreview::Fire, EImpactMoment::Landing, 1},
                {TEXT("spell in flight"), EImpactPreview::Spell, EImpactMoment::Flight, 1},
                {TEXT("spell landing"), EImpactPreview::Spell, EImpactMoment::Landing, 1},
                {TEXT("missile in flight (three bolts)"), EImpactPreview::Missile, EImpactMoment::Flight, 3},
                {TEXT("missile first landing (three bolts)"), EImpactPreview::Missile, EImpactMoment::FirstLanding, 3},
                {TEXT("missile second landing (two bolts)"), EImpactPreview::Missile, EImpactMoment::SecondLanding, 2},
                {TEXT("missile final landing (one bolt)"), EImpactPreview::Missile, EImpactMoment::Landing, 1},
            };
            for (const FCase& Case : Cases)
            {
                View->ClearImpactPreview();
                View->PreviewImpact(TargetId, Case.Effect, From, Case.Moment);
                View->Tick(0.016f);
                const TArray<FPlaybackProjectile>& Bolts = View->Playback().Projectiles();
                const TArray<float> Lights = View->BoltLightIntensities();
                TestEqual(FString::Printf(TEXT("%s: live bolts"), Case.Name), Bolts.Num(), Case.Live);
                float Total = 0.0f;
                for (int32 Index = 0; Index < Lights.Num(); ++Index)
                {
                    const float Expected = Bolts.IsValidIndex(Index) ? ExpeditionFlash::BoltLight(Bolts.Num(), Bolts[Index].Age / Bolts[Index].Duration) : 0.0f;
                    TestTrue(FString::Printf(TEXT("%s: light %d is %.2f cd, expected %.2f"), Case.Name, Index, Lights[Index], Expected), FMath::IsNearlyEqual(Lights[Index], Expected, 0.01f));
                    Total += Lights[Index];
                }
                // Whatever the volley, the lights together stay within the flight budget (the old 180 cd per bolt, 540 for Magic Missile, is far above it).
                TestTrue(FString::Printf(TEXT("%s: %.2f cd in all is within the %.0f cd budget"), Case.Name, Total, ExpeditionFlash::BoltLightCandelas), Total <= ExpeditionFlash::BoltLightCandelas + 0.01f);
            }
            // Absolute values for a lone bolt, so a change to the constants and the wiring together is still noticed.
            View->ClearImpactPreview();
            View->PreviewImpact(TargetId, EImpactPreview::Fire, From, EImpactMoment::Flight);
            View->Tick(0.016f);
            TestTrue(TEXT("A lone bolt in flight carries 50 cd"), FMath::IsNearlyEqual(View->BoltLightIntensities()[0], 50.0f, 0.01f));
            View->ClearImpactPreview();
            View->PreviewImpact(TargetId, EImpactPreview::Fire, From, EImpactMoment::Landing);
            View->Tick(0.016f);
            TestTrue(TEXT("A lone bolt at its target carries 3.5 cd"), FMath::IsNearlyEqual(View->BoltLightIntensities()[0], 3.5f, 0.01f));
            View->ClearImpactPreview();
            // The light at release (#150 batch 3): a bolt starts on the caster's own cell, and the full 50 cd there blew the caster out (peak 255, up to 1.76% pure white).
            // The light ramps in over the first stretch of flight, so what is left at the release frame is a small part of the flight light, for a lone bolt (the Fire
            // Bolt and the Cleric's radiant bolt) and for a Magic Missile's first bolt alike; it never exceeds that part, and rises with the flight.
            for (const EImpactPreview Effect : {EImpactPreview::Fire, EImpactPreview::Spell, EImpactPreview::Missile})
            {
                View->ClearImpactPreview();
                View->PreviewImpact(TargetId, Effect, From, EImpactMoment::Release);
                View->Tick(0.016f);
                const TArray<float> Released = View->BoltLightIntensities();
                float Total = 0.0f;
                for (const float Light : Released) { Total += Light; }
                TestTrue(FString::Printf(TEXT("A bolt at its release frame carries %.2f cd, at most a fifth of its 50 cd flight light"), Total), Total <= 0.2f * ExpeditionFlash::BoltLightCandelas + 0.01f);
                int32 Left = 0;
                for (const FPlaybackProjectile& Live : View->Playback().Projectiles())
                { if (Live.Age >= 0.0f) { ++Left; TestTrue(TEXT("The bolt that has left is at the start of its flight"), Live.Age / Live.Duration < 0.3f); } }
                TestEqual(TEXT("Only the first bolt has left at release"), Left, 1);
                View->ClearImpactPreview();
            }
            // The batch 3 casters' basic attack bolts (#150): the same light, ramped in the same way, in each class's own tint; the shooter named, as a real attack throws it.
            TArray<FLinearColor> Tints;
            for (const EAdventurerRole Caster : {EAdventurerRole::Wizard, EAdventurerRole::Cleric, EAdventurerRole::Sorcerer, EAdventurerRole::Warlock, EAdventurerRole::Druid})
            {
                for (const EImpactMoment Moment : {EImpactMoment::Release, EImpactMoment::Flight})
                {
                    View->ClearImpactPreview();
                    View->PreviewImpact(TargetId, EImpactPreview::Spell, From, Moment, Caster);
                    View->Tick(0.016f);
                    const TArray<FPlaybackProjectile>& Live = View->Playback().Projectiles();
                    float Light = 0.0f;
                    for (const float Each : View->BoltLightIntensities()) { Light += Each; }
                    if (!TestEqual(FString::Printf(TEXT("%d at moment %d: one bolt"), static_cast<int32>(Caster), static_cast<int32>(Moment)), Live.Num(), 1)) { continue; }
                    TestFalse(TEXT("A caster's attack is a bolt, not an arrow"), Live[0].bArrow);
                    TestTrue(TEXT("It carries its class tint"), Live[0].Color.Equals(FExpeditionPlayback::RangedBolt(From, From, Caster, false).Color, 0.001f));
                    const float Expected = ExpeditionFlash::BoltLight(1, Live[0].Age / Live[0].Duration);
                    TestTrue(FString::Printf(TEXT("Its light is %.2f cd, expected %.2f"), Light, Expected), FMath::IsNearlyEqual(Light, Expected, 0.01f));
                    if (Moment == EImpactMoment::Release)
                    {
                        TestTrue(TEXT("At release it is a fifth of the flight light at most"), Light <= 0.2f * ExpeditionFlash::BoltLightCandelas + 0.01f);
                        Tints.Add(Live[0].Color);
                    }
                    else { TestTrue(TEXT("In flight it is the full flight light"), FMath::IsNearlyEqual(Light, ExpeditionFlash::BoltLightCandelas, 0.01f)); }
                }
            }
            TestEqual(TEXT("Five caster tints"), Tints.Num(), 5);
            for (int32 A = 0; A < Tints.Num(); ++A)
            { for (int32 B = A + 1; B < Tints.Num(); ++B) { TestFalse(FString::Printf(TEXT("Tints %d and %d differ"), A, B), Tints[A].Equals(Tints[B], 0.05f)); } }
            View->ClearImpactPreview();
            TestTrue(TEXT("The light is nothing at the caster"), ExpeditionFlash::BoltLight(1, 0.0f) == 0.0f);
            TestTrue(TEXT("The light is at its full flight value by the end of the ramp"), FMath::IsNearlyEqual(ExpeditionFlash::BoltLight(1, ExpeditionFlash::BoltRampEnd), ExpeditionFlash::BoltLightCandelas, 0.01f));
            float Previous = -1.0f;
            for (int32 Step = 0; Step <= 10; ++Step)
            {
                const float Light = ExpeditionFlash::BoltLight(1, ExpeditionFlash::BoltRampEnd * Step / 10.0f);
                TestTrue(FString::Printf(TEXT("The ramp only rises (step %d: %.2f cd)"), Step, Light), Light >= Previous);
                Previous = Light;
            }
        }
    }
    World->DestroyWorld(false);
    FlushRenderingCommands();
    CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
    IFileManager::Get().DeleteDirectory(*Directory, false, true);
    return !HasAnyErrors();
}

// The preview builds its bolts with the production helper, so the two cannot drift (#189): a volley is three bolts, staggered by MissileStagger.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHitFlashVolleyTest, "Project.Expedition.Presentation.HitFlashVolley",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FHitFlashVolleyTest::RunTest(const FString& Parameters)
{
    const TArray<FPlaybackProjectile> Volley = FExpeditionPlayback::MagicMissileBolts(FVector2D(1, 1), FVector2D(5, 1));
    if (!TestEqual(TEXT("Three missiles"), Volley.Num(), 3)) { return false; }
    for (int32 Index = 0; Index < 3; ++Index)
    {
        TestTrue(FString::Printf(TEXT("Missile %d starts %.2f s late"), Index, FExpeditionPlayback::MissileStagger * Index), FMath::IsNearlyEqual(Volley[Index].Age, -FExpeditionPlayback::MissileStagger * Index));
        TestEqual(TEXT("Every missile takes the same time"), Volley[Index].Duration, FExpeditionPlayback::MissileFlight);
        TestFalse(TEXT("A missile is not an arrow"), Volley[Index].bArrow);
    }
    TestTrue(TEXT("The arcs spread to both sides"), Volley[0].Arc * Volley[2].Arc < 0.0f);
    const FPlaybackProjectile Bolt = FExpeditionPlayback::RangedBolt(FVector2D(1, 1), FVector2D(5, 1), EAdventurerRole::Cleric, false);
    TestTrue(TEXT("A Cleric's bolt is gold"), Bolt.Color.R > Bolt.Color.G && Bolt.Color.G > Bolt.Color.B && !Bolt.bArrow);
    for (const EAdventurerRole Caster : {EAdventurerRole::Sorcerer, EAdventurerRole::Warlock, EAdventurerRole::Druid})
    {
        const FPlaybackProjectile CasterBolt = FExpeditionPlayback::RangedBolt(FVector2D(1, 1), FVector2D(5, 1), Caster, false);
        TestFalse(TEXT("A batch 3 caster's bolt is not an arrow"), CasterBolt.bArrow);
        TestFalse(TEXT("and not the pale arrow tint"), CasterBolt.Color.Equals(FExpeditionPlayback::RangedBolt(FVector2D(1, 1), FVector2D(5, 1), EAdventurerRole::Ranger, false).Color, 0.05f));
    }
    const FPlaybackProjectile Ember = FExpeditionPlayback::RangedBolt(FVector2D(1, 1), FVector2D(5, 1), EAdventurerRole::Sorcerer, false);
    const FPlaybackProjectile Eldritch = FExpeditionPlayback::RangedBolt(FVector2D(1, 1), FVector2D(5, 1), EAdventurerRole::Warlock, false);
    const FPlaybackProjectile Thorn = FExpeditionPlayback::RangedBolt(FVector2D(1, 1), FVector2D(5, 1), EAdventurerRole::Druid, false);
    // A crimson-magenta ember (#150 batch 4: it was an orange-red 20 degrees from the Wizard's Fire Bolt, so the two read as one colour side by side).
    TestTrue(TEXT("The Sorcerer's bolt is a crimson-magenta ember"), Ember.Color.R > 0.9f && Ember.Color.G < 0.15f && Ember.Color.B > 0.25f && Ember.Color.B < 0.5f);
    TestTrue(TEXT("The Warlock's bolt is eldritch green"), Eldritch.Color.G > 0.9f && Eldritch.Color.R < 0.7f && Eldritch.Color.B < 0.5f);
    TestTrue(TEXT("The Druid's bolt is leaf gold"), Thorn.Color.G > 0.9f && Thorn.Color.R > 0.6f && Thorn.Color.B < 0.4f);
    const FPlaybackProjectile Note = FExpeditionPlayback::RangedBolt(FVector2D(1, 1), FVector2D(5, 1), EAdventurerRole::Bard, false);
    const FPlaybackProjectile Cyan = FExpeditionPlayback::RangedBolt(FVector2D(1, 1), FVector2D(5, 1), EAdventurerRole::Artificer, false);
    TestTrue(TEXT("The Bard's bolt is a teal note and not an arrow"), !Note.bArrow && Note.Color.G > 0.9f && Note.Color.B > 0.5f && Note.Color.R < 0.3f);
    TestTrue(TEXT("The Artificer's special bolt is a blue-cyan and not an arrow"), !Cyan.bArrow && Cyan.Color.B > 0.9f && Cyan.Color.G > 0.6f && Cyan.Color.R < 0.4f);
    TestTrue(TEXT("A rogue's arrow is an arrow"), FExpeditionPlayback::RangedBolt(FVector2D(1, 1), FVector2D(5, 1), EAdventurerRole::Rogue, true).bArrow);
    return !HasAnyErrors();
}

// The arrow's lights around a ranged release (#198): an arrow carries no point light at any moment (release, flight, landing), the shooter's figure is
// not tinted by it, and the spell bolt a Ranger used to throw (the same construction in the arrow's colour) carries only the ramped-in part of its light at the shooter at
// release (it was the whole 50 cd, which is what blew the ranger and the floor out). The real point lights are read from the view, so a light put back on an arrow in UpdateEffects fails here.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHitFlashArrowLightTest, "Project.Expedition.Presentation.HitFlashArrowLights",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FHitFlashArrowLightTest::RunTest(const FString& Parameters)
{
    const UWorld::InitializationValues Values = UWorld::InitializationValues()
        .AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
    if (!TestNotNull(TEXT("Transient world"), World)) { return false; }
    const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation/HitFlashArrowLights"), FGuid::NewGuid().ToString());
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
        int32 TargetId = INDEX_NONE, ShooterId = INDEX_NONE;
        FVector2D From = FVector2D::ZeroVector;
        for (const FCombatantState& Unit : View->Rules().Snapshot().Encounter.Combatants)
        {
            if (Unit.Side == ECombatSide::Enemy && Unit.Life == ECombatLife::Conscious && TargetId == INDEX_NONE) { TargetId = Unit.Id; }
            if (Unit.Side == ECombatSide::Party && Unit.Life == ECombatLife::Conscious && ShooterId == INDEX_NONE) { ShooterId = Unit.Id; From = FVector2D(Unit.Cell.X, Unit.Cell.Y); }
        }
        if (TestTrue(TEXT("A shooter and a foe"), TargetId != INDEX_NONE && ShooterId != INDEX_NONE))
        {
            TArray<FLinearColor> Idle;
            FLinearColor IdleRing;
            TestTrue(TEXT("The shooter's figure can be read"), View->UnitShownColors(ShooterId, Idle, IdleRing));
            struct FMoment { const TCHAR* Name; EImpactMoment Moment; };
            const FMoment Moments[] = {{TEXT("release"), EImpactMoment::Release}, {TEXT("flight"), EImpactMoment::Flight}, {TEXT("landing"), EImpactMoment::Landing}};
            for (const FMoment& Moment : Moments)
            {
                View->ClearImpactPreview();
                View->PreviewImpact(TargetId, EImpactPreview::Arrow, From, Moment.Moment);
                View->Tick(0.016f);
                const TArray<FPlaybackProjectile>& Shots = View->Playback().Projectiles();
                const TArray<float> Lights = View->BoltLightIntensities();
                TestTrue(FString::Printf(TEXT("arrow %s: one arrow in the air"), Moment.Name), Shots.Num() == 1 && Shots[0].bArrow);
                float Total = 0.0f;
                for (const float Light : Lights) { Total += Light; }
                TestTrue(FString::Printf(TEXT("arrow %s: %.2f cd in all, none (an arrow carries no light)"), Moment.Name, Total), Total == 0.0f);
                TArray<FLinearColor> Shown;
                FLinearColor Ring;
                if (View->UnitShownColors(ShooterId, Shown, Ring) && TestEqual(*FString::Printf(TEXT("arrow %s: the shooter has the same parts"), Moment.Name), Shown.Num(), Idle.Num()))
                {
                    for (int32 Part = 0; Part < Shown.Num(); ++Part)
                    { TestTrue(*FString::Printf(TEXT("arrow %s: the shooter's part %d is not flashed"), Moment.Name, Part), Shown[Part].Equals(Idle[Part], 0.01f)); }
                    TestTrue(*FString::Printf(TEXT("arrow %s: nor is its ring"), Moment.Name), Ring.Equals(IdleRing, 0.01f));
                }
            }
            // What the Ranger threw before: a spell bolt in the arrow's colour, with its light on the shooter's cell at release.
            View->ClearImpactPreview();
            View->PreviewImpact(TargetId, EImpactPreview::Bolt, From, EImpactMoment::Release);
            View->Tick(0.016f);
            const TArray<float> Old = View->BoltLightIntensities();
            // Before batch 3 this light was the whole 50 cd; the ramp (ExpeditionFlash::BoltRampEnd) now holds it to a fraction at the release frame for every spell bolt.
            TestTrue(TEXT("The old Ranger bolt at release carries only the ramped-in part of its 50 cd flight light"), Old.Num() > 0 && Old[0] > 0.0f && Old[0] <= 0.2f * ExpeditionFlash::BoltLightCandelas);
            View->ClearImpactPreview();
        }
    }
    World->DestroyWorld(false);
    FlushRenderingCommands();
    CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
    IFileManager::Get().DeleteDirectory(*Directory, false, true);
    return !HasAnyErrors();
}
#endif
