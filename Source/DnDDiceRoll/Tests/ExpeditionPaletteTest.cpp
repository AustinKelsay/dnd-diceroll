#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "RenderingThread.h"
#include "UObject/GarbageCollection.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Party/BuildCatalog.h"
#include "Persistence/ExpeditionSave.h"
#include "Presentation/ExpeditionBattleView.h"
#include "Presentation/ExpeditionPalette.h"
#include "Run/ExpeditionRun.h"
#include "ExpeditionTestSupport.h"

// Subclass palettes and duplicate-identity accents (#150 batch 4). Presentation only (ADR-0001): a palette is a pure function of the build id and an
// accent of the party order. The first three tests need no world; the last two build boards with the shipped theme and read the figures' materials back.

namespace PaletteTest
{
using namespace ExpeditionPalette;

FCombatantState Hero(int32 Id, const TCHAR* BuildId)
{
    FCombatantState Unit;
    FBuildCatalog::Create(BuildId, Id, FString::Printf(TEXT("Hero %d"), Id), Unit);
    return Unit;
}

bool SameSlot(const FSlot& A, const FSlot& B)
{
    return A.Mask == B.Mask && A.Target.Equals(B.Target, 1.0e-6f) && FMath::IsNearlyEqual(A.Reference, B.Reference, 1.0e-6f);
}

bool SamePalette(const FPalette& A, const FPalette& B)
{
    for (int32 Index = 0; Index < SlotCount; ++Index) { if (!SameSlot(A.Slots[Index], B.Slots[Index])) { return false; } }
    return true;
}

FString Name(const FCombatantState& Unit) { return Unit.BuildId; }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPaletteTableTest, "Project.Expedition.Presentation.PaletteLogic.TableCoversCatalog",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FPaletteTableTest::RunTest(const FString& Parameters)
{
    using namespace ExpeditionPalette;
    using namespace PaletteTest;
    // Every catalog build maps to its own valid palette; the table has no row for a build that does not exist.
    const TArray<FBuildDefinition>& Builds = FBuildCatalog::All();
    TestEqual(TEXT("The catalog has 70 builds"), Builds.Num(), 70);
    const TArray<FString> Ids = BuildIds();
    TestEqual(TEXT("The palette table has a row per build and no other"), Ids.Num(), Builds.Num());
    TSet<FString> Unique(Ids);
    TestEqual(TEXT("Its build ids are unique"), Unique.Num(), Ids.Num());
    TMap<FString, TArray<FPalette>> PerClass;
    for (const FBuildDefinition& Build : Builds)
    {
        const FString Label = Build.Id;
        TestTrue(*(Label + TEXT(" has a table row")), Unique.Contains(Build.Id));
        const FString Class = ClassKeyOf(Build.Profile);
        TestEqual(*(Label + TEXT(" is keyed by its class id")), Class, Build.Profile.ClassId);
        const FPalette Palette = ForBuild(Build.Id, Class);
        TestTrue(*(Label + TEXT(" resolves its own row")), Palette.bBuildRow && Palette.bKnownClass);
        for (int32 Index = 0; Index < SlotCount; ++Index)
        {
            const FSlot& Slot = Palette.Slots[Index];
            if (Index == static_cast<int32>(ESlot::X)) { TestFalse(*(Label + TEXT(": the accent slot is not part of a build palette")), Slot.IsActive()); continue; }
            TestTrue(*FString::Printf(TEXT("%s slot %d has a positive reference luminance"), *Label, Index), Slot.Reference > 0.0f);
            if (!Slot.IsActive()) { continue; }
            TestTrue(*FString::Printf(TEXT("%s slot %d masks only the 13 atlas tiles"), *Label, Index), Slot.Mask != 0 && Slot.Mask < (1 << 13));
            const FLinearColor C = Slot.Target;
            TestTrue(*FString::Printf(TEXT("%s slot %d target is a displayable colour"), *Label, Index),
                C.R >= 0.0f && C.R <= 1.0f && C.G >= 0.0f && C.G <= 1.0f && C.B >= 0.0f && C.B <= 1.0f);
        }
        PerClass.FindOrAdd(Class).Add(Palette);
    }
    TestEqual(TEXT("All thirteen classes are covered"), PerClass.Num(), 13);
    // The variants of a class are distinct from one another (at most one per class is the class's own look, and it is not repeated).
    for (const TPair<FString, TArray<FPalette>>& Entry : PerClass)
    {
        for (int32 A = 0; A < Entry.Value.Num(); ++A)
        {
            for (int32 B = A + 1; B < Entry.Value.Num(); ++B)
            { TestFalse(*FString::Printf(TEXT("%s variants %d and %d differ"), *Entry.Key, A, B), SamePalette(Entry.Value[A], Entry.Value[B])); }
        }
    }
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPaletteFallbackTest, "Project.Expedition.Presentation.PaletteLogic.Fallbacks",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FPaletteFallbackTest::RunTest(const FString& Parameters)
{
    using namespace ExpeditionPalette;
    using namespace PaletteTest;
    // An unknown build of a known class falls back to the class default (not a row, still the class's palette).
    const FPalette Default = ForBuild(TEXT("fighter.no-such-subclass"), TEXT("fighter"));
    TestTrue(TEXT("An unknown fighter build is the class default"), Default.bKnownClass && !Default.bBuildRow);
    TestTrue(TEXT("The fighter's default is the corrected steel (slot A is set)"), Default.Slots[static_cast<int32>(ESlot::A)].IsActive());
    TestTrue(TEXT("The fighter's default equals Champion's look"), SamePalette(Default, ForBuild(TEXT("fighter.champion"), TEXT("fighter"))));
    TestTrue(TEXT("An empty build id is the class default"), SamePalette(ForBuild(FString(), TEXT("fighter")), Default));
    const FPalette Rogue = ForBuild(TEXT("rogue.future"), TEXT("rogue"));
    TestTrue(TEXT("An unknown rogue build is the rogue default, which is the class's own colours"), Rogue.bKnownClass && !Rogue.bBuildRow && Rogue.ActiveSlots() == 0);
    // A build id of another class is never borrowed.
    TestTrue(TEXT("A rogue id on a fighter is the fighter default"), SamePalette(ForBuild(TEXT("rogue.assassin"), TEXT("fighter")), Default));
    // An unknown class has no palette at all, and no accent.
    const FPalette None = ForBuild(TEXT("fighter.champion"), TEXT("jester"));
    TestTrue(TEXT("An unknown class has no palette"), !None.bKnownClass && None.ActiveSlots() == 0);
    FPalette Accented;
    SetAccent(Accented, TEXT("jester"), 1);
    TestTrue(TEXT("An unknown class takes no accent"), !Accented.bKnownClass && Accented.ActiveSlots() == 0);
    // The key is the class id, else the role's name.
    FCombatantState Legacy;
    Legacy.AdventurerRole = EAdventurerRole::Bard;
    TestEqual(TEXT("A hero with no class id is keyed by its role"), ClassKeyOf(Legacy), FString(TEXT("bard")));
    TestEqual(TEXT("A foe-less empty unit has no class"), ClassKeyOf(FCombatantState()), FString());
    TestTrue(TEXT("The bard role resolves its class default"), ForUnit(Legacy).bKnownClass);
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPaletteAccentTest, "Project.Expedition.Presentation.PaletteLogic.Accents",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FPaletteAccentTest::RunTest(const FString& Parameters)
{
    using namespace ExpeditionPalette;
    using namespace PaletteTest;
    // Two fighters (different subclasses), a wizard and a rogue: only the duplicated class is accented, by party position.
    TArray<FCombatantState> Party = {Hero(1, TEXT("fighter.champion")), Hero(2, TEXT("wizard.evocation")), Hero(3, TEXT("fighter.rune-knight")), Hero(4, TEXT("rogue.thief"))};
    TMap<int32, int32> Accents = AccentSlots(Party);
    TestEqual(TEXT("Two heroes are accented"), Accents.Num(), 2);
    TestEqual(TEXT("The first fighter is party slot 0"), Accents.FindRef(1), 0);
    TestEqual(TEXT("The second fighter is party slot 2"), Accents.FindRef(3), 2);
    TestFalse(TEXT("The wizard is not accented"), Accents.Contains(2));
    TestFalse(TEXT("The rogue is not accented"), Accents.Contains(4));
    // The same build twice is accented too, and four of a class take four distinct colours.
    TArray<FCombatantState> Four = {Hero(1, TEXT("cleric.life")), Hero(2, TEXT("cleric.life")), Hero(3, TEXT("cleric.war")), Hero(4, TEXT("cleric.life"))};
    const TMap<int32, int32> Every = AccentSlots(Four);
    TestEqual(TEXT("All four clerics are accented"), Every.Num(), 4);
    TArray<FLinearColor> Colours;
    for (const FCombatantState& Unit : Four)
    {
        FPalette Palette = ForUnit(Unit);
        SetAccent(Palette, ClassKeyOf(Unit), Every.FindRef(Unit.Id));
        const FSlot& X = Palette.Slots[static_cast<int32>(ESlot::X)];
        TestTrue(TEXT("The accent slot is set"), X.IsActive());
        for (const FLinearColor& Seen : Colours) { TestFalse(TEXT("Each party slot has its own accent colour"), Seen.Equals(X.Target, 0.02f)); }
        Colours.Add(X.Target);
        // The subclass palette underneath is untouched by the accent.
        const FPalette Plain = ForUnit(Unit);
        for (int32 Index = 0; Index < BuildSlotCount; ++Index) { TestTrue(TEXT("The build slots are the build's own"), SameSlot(Palette.Slots[Index], Plain.Slots[Index])); }
    }
    // Deterministic: a pure function of the party order, the same on every call, for the same party read back from a copy, and the enemy side never counts.
    TestTrue(TEXT("The slots are stable across calls"), AccentSlots(Party).OrderIndependentCompareEqual(Accents));
    TArray<FCombatantState> Copy = Party;
    Copy.Add(Hero(5, TEXT("fighter.champion")));
    Copy.Last().Side = ECombatSide::Enemy;
    TestTrue(TEXT("A foe of the same class changes no party accent"), AccentSlots(Copy).OrderIndependentCompareEqual(Accents));
    // A slot beyond the four wraps, negative never crashes.
    FPalette Wrapped = ForUnit(Party[0]);
    FPalette First = ForUnit(Party[0]);
    SetAccent(Wrapped, TEXT("fighter"), 4);
    SetAccent(First, TEXT("fighter"), 0);
    TestTrue(TEXT("Slot 4 wraps to slot 0"), SameSlot(Wrapped.Slots[static_cast<int32>(ESlot::X)], First.Slots[static_cast<int32>(ESlot::X)]));
    SetAccent(Wrapped, TEXT("fighter"), -1);
    TestTrue(TEXT("A negative slot wraps"), Wrapped.Slots[static_cast<int32>(ESlot::X)].IsActive());
    // The functions take the party by const reference: nothing here can write a snapshot or draw a random number (the board-level test also compares
    // checkpoint bytes with palettes on and off).
    return !HasAnyErrors();
}

namespace PaletteTest
{
void ReleaseWorld(UWorld* World)
{
    World->DestroyWorld(false);
    FlushRenderingCommands();
    CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
}

AExpeditionBattleView* MakeView(const FString& Path, bool bStatic)
{
    const UWorld::InitializationValues Values = UWorld::InitializationValues()
        .AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
    if (!World) { return nullptr; }
    AExpeditionBattleView* View = World->SpawnActor<AExpeditionBattleView>();
    if (!View) { ReleaseWorld(World); return nullptr; }
    View->ConfigureStorage(Path);
    View->DispatchBeginPlay();
    if (bStatic) { View->UseStaticFigures(); }
    return View;
}

void Release(AExpeditionBattleView* View)
{
    if (View) { ReleaseWorld(View->GetWorld()); }
}

// A fresh run with the given builds on a stream trail board, written as a checkpoint, and the view that resumed it.
AExpeditionBattleView* ResumeParty(FAutomationTestBase& Test, const TArray<const TCHAR*>& Builds, const FString& Path, FExpeditionSaveData& Data, bool bStatic = false)
{
    TArray<FPartyBuildSelection> Party;
    for (int32 Index = 0; Index < 4; ++Index)
    {
        FPartyBuildSelection Slot;
        Slot.Id = Index + 1;
        Slot.Name = FString::Printf(TEXT("Hero %d"), Index + 1);
        Slot.BuildId = Builds[Index];
        Party.Add(Slot);
    }
    FExpeditionRun Run;
    FString Error;
    if (!Test.TestTrue(TEXT("The run starts"), Run.NewRun(11, Party, Error, TEXT("palette-party"))))  { return nullptr; }
    const auto* Route = Run.Progress().Offers.FindByPredicate([](const auto& R) { return !R.bDangerous; });
    if (!Test.TestTrue(TEXT("A trail board is chosen"), Route && Run.ChooseRoute(Route->Id, Error))) { return nullptr; }
    Data.Current = Run.Snapshot();
    if (!Test.TestTrue(TEXT("The checkpoint writes"), FExpeditionSave::Write(Path, Data, Error))) { return nullptr; }
    AExpeditionBattleView* View = MakeView(Path, bStatic);
    if (!Test.TestNotNull(TEXT("Resumed view"), View)) { return nullptr; }
    View->Resume();
    View->Tick(0.016f);
    return View;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPaletteBoardTest, "Project.Expedition.Presentation.PaletteBoard.ReachesFigures",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FPaletteBoardTest::RunTest(const FString& Parameters)
{
    using namespace ExpeditionPalette;
    using namespace PaletteTest;
    const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation/PaletteBoard"), FGuid::NewGuid().ToString());
    // Every catalog build, four to a party, reaches its figure's material as its palette; a class appearing twice also carries the accent.
    TArray<FString> Ids;
    for (const FBuildDefinition& Build : FBuildCatalog::All()) { Ids.Add(Build.Id); }
    int32 Rigged = 0, Checked = 0;
    for (int32 Start = 0; Start < Ids.Num(); Start += 4)
    {
        TArray<const TCHAR*> Builds;
        TArray<FString> Keep;
        for (int32 Index = 0; Index < 4; ++Index) { Keep.Add(Ids[(Start + Index) % Ids.Num()]); }
        for (const FString& Id : Keep) { Builds.Add(*Id); }
        FExpeditionSaveData Data;
        AExpeditionBattleView* View = ResumeParty(*this, Builds, FPaths::Combine(Directory, FString::FromInt(Start), TEXT("current.json")), Data);
        if (!View) { continue; }
        TestTrue(TEXT("The shipped theme loads"), View->UsesTheme());
        const TArray<FCombatantState>& Combatants = View->Rules().Snapshot().Encounter.Combatants;
        const TMap<int32, int32> Accents = AccentSlots(Combatants);
        for (const FCombatantState& Unit : Combatants)
        {
            if (Unit.Side != ECombatSide::Party) { continue; }
            FPalette Held;
            if (!View->UnitAnimation(Unit.Id).bSkeletal) { continue; }   // a class without a rig keeps its static figure and has no palette
            ++Rigged;
            const FString Label = Unit.BuildId;
            if (!TestTrue(*(Label + TEXT(": the figure's material carries the palette parameters (run import_dark_fantasy_theme.py)")), View->UnitPalette(Unit.Id, Held))) { continue; }
            FPalette Expected = ForUnit(Unit);
            if (const int32* Slot = Accents.Find(Unit.Id)) { SetAccent(Expected, ClassKeyOf(Unit), *Slot); }
            TestTrue(*(Label + TEXT(": the figure shows its build's palette")), SamePalette(Held, Expected));
            ++Checked;
        }
        Release(View);
    }
    TestTrue(TEXT("At least the shipped rigged party was checked"), Checked > 0);
    AddInfo(FString::Printf(TEXT("%d rigged heroes checked of %d"), Checked, Rigged));
    IFileManager::Get().DeleteDirectory(*Directory, false, true);
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPaletteDuplicatesTest, "Project.Expedition.Presentation.PaletteBoard.DuplicateAccents",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FPaletteDuplicatesTest::RunTest(const FString& Parameters)
{
    using namespace ExpeditionPalette;
    using namespace PaletteTest;
    const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation/PaletteDuplicates"), FGuid::NewGuid().ToString());
    // Four fighters of the same build: each is a rigged fighter with its own accent, in the party's order; the subclass palette beneath is shared.
    const TArray<const TCHAR*> Same = {TEXT("fighter.champion"), TEXT("fighter.champion"), TEXT("fighter.champion"), TEXT("fighter.champion")};
    // Two of two classes: pairs are accented, nobody else is.
    const TArray<const TCHAR*> Pairs = {TEXT("cleric.life"), TEXT("rogue.thief"), TEXT("cleric.war"), TEXT("rogue.thief")};
    // All different: no accents.
    const TArray<const TCHAR*> Mixed = {TEXT("fighter.champion"), TEXT("rogue.thief"), TEXT("cleric.life"), TEXT("wizard.evocation")};
    struct FCase { const TCHAR* Name; const TArray<const TCHAR*>* Builds; int32 Accented; };
    const FCase Cases[] = {{TEXT("four fighters"), &Same, 4}, {TEXT("two pairs"), &Pairs, 4}, {TEXT("all different"), &Mixed, 0}};
    for (const FCase& Case : Cases)
    {
        const FString Path = FPaths::Combine(Directory, Case.Name, TEXT("current.json"));
        FExpeditionSaveData Data;
        AExpeditionBattleView* View = ResumeParty(*this, *Case.Builds, Path, Data);
        if (!View) { continue; }
        const FString Label = Case.Name;
        TArray<FCombatantState> Party;
        for (const FCombatantState& Unit : View->Rules().Snapshot().Encounter.Combatants) { if (Unit.Side == ECombatSide::Party) { Party.Add(Unit); } }
        TMap<int32, FPalette> Before;
        int32 Accented = 0;
        TArray<FLinearColor> Seen;
        for (const FCombatantState& Unit : Party)
        {
            FPalette Held;
            if (!TestTrue(*(Label + TEXT(": ") + Unit.Name + TEXT(" is a rigged figure with palette parameters")), View->UnitAnimation(Unit.Id).bSkeletal && View->UnitPalette(Unit.Id, Held))) { continue; }
            Before.Add(Unit.Id, Held);
            const FSlot& X = Held.Slots[static_cast<int32>(ESlot::X)];
            if (X.IsActive())
            {
                ++Accented;
                for (const FLinearColor& Other : Seen) { TestFalse(*(Label + TEXT(": the accent colours differ")), Other.Equals(X.Target, 0.02f)); }
                Seen.Add(X.Target);
            }
        }
        TestEqual(*(Label + TEXT(": accented heroes")), Accented, Case.Accented);
        if (Case.Accented == 4 && Party.Num() == 4)
        {
            // Four of one build look different only through the accent: the subclass slots are equal.
            for (int32 Index = 1; Index < 4; ++Index)
            { for (int32 Slot = 0; Slot < BuildSlotCount; ++Slot) { TestTrue(*(Label + TEXT(": the same build shares its palette")), SameSlot(Before[Party[0].Id].Slots[Slot], Before[Party[Index].Id].Slots[Slot])); } }
        }
        // The party snapshot was never touched by building the figures.
        FExpeditionSnapshot Restored;
        Restored.Progress = View->Progress();
        Restored.Battle = View->Rules().Snapshot();
        TestTrue(*(Label + TEXT(": the snapshot equals the checkpoint")), ExpeditionTestSupport::SameRun(Data.Current, Restored));
        Release(View);
        // Resume in a fresh world: the same colours, hero by hero.
        AExpeditionBattleView* Again = MakeView(Path, false);
        if (!TestNotNull(*(Label + TEXT(": a second resumed view")), Again)) { continue; }
        Again->Resume();
        Again->Tick(0.016f);
        for (const FCombatantState& Unit : Party)
        {
            FPalette Held;
            if (!Before.Contains(Unit.Id) || !Again->UnitPalette(Unit.Id, Held)) { continue; }
            TestTrue(*(Label + TEXT(": ") + Unit.Name + TEXT(" shows the same palette after Resume")), SamePalette(Held, Before[Unit.Id]));
        }
        Release(Again);
        // The static figures (no rig) carry no palette and the board is unchanged in rules.
        AExpeditionBattleView* Static = MakeView(Path, true);
        if (TestNotNull(*(Label + TEXT(": a static-figure view")), Static))
        {
            Static->Resume();
            Static->Tick(0.016f);
            FPalette Held;
            for (const FCombatantState& Unit : Party) { TestFalse(*(Label + TEXT(": a static figure has no palette")), Static->UnitPalette(Unit.Id, Held)); }
            FExpeditionSnapshot StaticRestored;
            StaticRestored.Progress = Static->Progress();
            StaticRestored.Battle = Static->Rules().Snapshot();
            TestTrue(*(Label + TEXT(": rigged and static views hold the same snapshot")), ExpeditionTestSupport::SameRun(Restored, StaticRestored));
            Release(Static);
        }
    }
    IFileManager::Get().DeleteDirectory(*Directory, false, true);
    return !HasAnyErrors();
}
#endif
