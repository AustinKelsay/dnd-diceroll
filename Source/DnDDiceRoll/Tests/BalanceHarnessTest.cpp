#if WITH_DEV_AUTOMATION_TESTS
#include "BalanceHarness.h"

#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Party/BuildCatalog.h"
#include "Persistence/ExpeditionSave.h"

#if PLATFORM_UNIX
#include <unistd.h>
#endif

// Small public-seam checks of the balance harness (#179). The bulk baseline is Balance.Baseline.*,
// outside the Project. prefix, so ordinary gates never run it.
namespace BalanceHarnessTest
{
using namespace BalanceHarness;

/** Six parties that between them use every kind of kit effect, the legacy commands and duplicates. */
TArray<FPartySpec> SampleParties()
{
    TArray<FPartySpec> Parties = RepresentativeParties();
    Parties.SetNum(4); // familiar, melee, ranged, support-poor
    Parties.Add(RepresentativeParties()[5]); // mixed
    Parties.Add(RotationParties(2)[7]);
    return Parties;
}

TArray<FRunStats> SampleRuns()
{
    TArray<FRunStats> Runs;
    for (const auto& Party : SampleParties())
    {
        for (int32 I = 0; I < 2; ++I) { Runs.Add(PlayRun(Party, BaseSeed + 7 * I, 0b10110 ^ I)); }
    }
    return Runs;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBalancePolicyLegalTest, "Project.Balance.PolicySubmitsOnlyLegalCommands",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FBalancePolicyLegalTest::RunTest(const FString& Parameters)
{
    int32 Commands = 0, Specials = 0;
    for (const auto& Run : BalanceHarnessTest::SampleRuns())
    {
        const FString Where = FString::Printf(TEXT("%s seed %d"), *Run.PartyId, Run.Seed);
        TestEqual(Where + TEXT(": every proposal was in LegalTargets/LegalMoves"), Run.IllegalProposals, 0);
        TestEqual(Where + TEXT(": the run accepted every submitted command"), Run.PolicyErrors, 0);
        TestEqual(Where + TEXT(": every committed prevention equals FCombatBattle::Protection"), Run.ProtectionMismatches, 0);
        Commands += Run.Commands; Specials += Run.SpecialsUsed;
    }
    TestTrue(TEXT("The sample submitted real commands"), Commands > 200);
    TestTrue(TEXT("The sample used kit specials"), Specials > 10);
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBalancePolicyActsTest, "Project.Balance.PolicyNeverPassesALegalAction",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FBalancePolicyActsTest::RunTest(const FString& Parameters)
{
    int32 Encounters = 0;
    for (const auto& Run : BalanceHarnessTest::SampleRuns())
    {
        const FString Where = FString::Printf(TEXT("%s seed %d"), *Run.PartyId, Run.Seed);
        TestEqual(Where + TEXT(": no End Turn while an Attack or a closer move was legal"), Run.PassiveEnds, 0);
        TestTrue(Where + TEXT(": the run ended in Victory or Defeat, not a stall (") + Run.StallReason + TEXT(")"), Run.Outcome == TEXT("Victory") || Run.Outcome == TEXT("Defeat"));
        TestTrue(Where + TEXT(": a victory is five won fights"), (Run.Outcome == TEXT("Victory")) == (Run.EncountersWon == FExpeditionRun::EncounterCount));
        for (const auto& Encounter : Run.Encounters)
        {
            ++Encounters;
            TestTrue(Where + TEXT(": every fight finished below the round cap"), Encounter.Rounds > 0 && Encounter.Rounds < BalanceHarness::MaxRounds);
        }
    }
    TestTrue(TEXT("The sample played many fights"), Encounters >= 12);
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBalanceDeterminismTest, "Project.Balance.SameSeedSameResults",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FBalanceDeterminismTest::RunTest(const FString& Parameters)
{
    using namespace BalanceHarness;
    const TArray<FPartySpec> Parties = {RepresentativeParties()[0], RotationParties(4)[3]};
    for (const auto& Party : Parties)
    {
        for (const int32 Seed : {BaseSeed, BaseSeed + 31})
        {
            FExpeditionSnapshot FirstEnd, SecondEnd;
            const TArray<FRunStats> First = {PlayRun(Party, Seed, Seed & 31, &FirstEnd)};
            const TArray<FRunStats> Second = {PlayRun(Party, Seed, Seed & 31, &SecondEnd)};
            const FString Where = FString::Printf(TEXT("%s seed %d"), *Party.Id, Seed);
            TestEqual(Where + TEXT(": identical run rows"), RunsCsv(First), RunsCsv(Second));
            TestEqual(Where + TEXT(": identical encounter rows"), EncountersCsv(First), EncountersCsv(Second));
            TestEqual(Where + TEXT(": identical hero rows"), HeroesCsv(First), HeroesCsv(Second));
            TestTrue(Where + TEXT(": identical final expedition snapshot, RNG included"),
                FExpeditionSnapshot::StaticStruct()->CompareScriptStruct(&FirstEnd, &SecondEnd, 0));
        }
        const TArray<FRunStats> A = {PlayRun(Party, BaseSeed, 0)};
        const TArray<FRunStats> B = {PlayRun(Party, BaseSeed + 1, 0)};
        TestNotEqual(Party.Id + TEXT(": a different seed plays a different run"), EncountersCsv(A), EncountersCsv(B));
    }
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBalanceNoCheckpointTest, "Project.Balance.HarnessWritesNoCheckpoint",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FBalanceNoCheckpointTest::RunTest(const FString& Parameters)
{
    const FString Folder = FPaths::GetPath(FExpeditionSave::DefaultPath());
    auto Capture = [&Folder]()
    {
        TMap<FString, TArray<uint8>> Files;
        TArray<FString> Names;
        IFileManager::Get().FindFilesRecursive(Names, *Folder, TEXT("*"), true, false);
        Names.Sort();
        for (const FString& Name : Names) { FFileHelper::LoadFileToArray(Files.Add(Name), *Name); }
        return Files;
    };
    const TMap<FString, TArray<uint8>> Before = Capture();
    BalanceHarness::PlayGroup({BalanceHarness::RepresentativeParties()[0]}, 2);
    const TMap<FString, TArray<uint8>> After = Capture();
    TestEqual(TEXT("The save folder holds the same files"), After.Num(), Before.Num());
    for (const auto& Entry : Before)
    {
        const TArray<uint8>* Now = After.Find(Entry.Key);
        TestTrue(Entry.Key + TEXT(" is byte-identical after harness runs"), Now && *Now == Entry.Value);
    }
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBalanceCoverageTest, "Project.Balance.PartySetCoversEveryKit",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FBalanceCoverageTest::RunTest(const FString& Parameters)
{
    using namespace BalanceHarness;
    const auto& Kits = FBuildCatalog::All();
    TestEqual(TEXT("The approved roster has 70 kits"), Kits.Num(), 70);
    TMap<FString, int32> Appearances;
    TSet<FString> Ids;
    for (int32 Pass = 0; Pass < RotationPasses; ++Pass)
    {
        TSet<FString> Classes;
        const TArray<FPartySpec> Parties = RotationParties(Pass);
        TestEqual(FString::Printf(TEXT("Pass %d has 18 parties"), Pass + 1), Parties.Num(), 18);
        for (const auto& Party : Parties)
        {
            TestFalse(Party.Id + TEXT(" is a unique party id"), Ids.Contains(Party.Id));
            Ids.Add(Party.Id);
            TestEqual(Party.Id + TEXT(" has four slots"), Party.Builds.Num(), 4);
            TSet<FString> Distinct(Party.Builds);
            TestEqual(Party.Id + TEXT(" repeats no kit"), Distinct.Num(), 4);
            for (const auto& Build : Party.Builds)
            {
                const FBuildDefinition* Definition = FBuildCatalog::Find(Build);
                if (!TestNotNull(Build + TEXT(" is a catalog build"), Definition)) { continue; }
                ++Appearances.FindOrAdd(Build);
                Classes.Add(Definition->Profile.ClassId);
            }
        }
        TestEqual(FString::Printf(TEXT("Pass %d includes all 13 classes"), Pass + 1), Classes.Num(), 13);
    }
    for (const auto& Kit : Kits)
    {
        const int32 Count = Appearances.FindRef(Kit.Id);
        TestTrue(Kit.Id + TEXT(" appears in five or six rotation parties"), Count >= 5 && Count <= 6);
    }
    TSet<FString> Archetypes;
    for (const auto& Party : RepresentativeParties())
    {
        TestFalse(Party.Id + TEXT(" is a unique party id"), Ids.Contains(Party.Id));
        Ids.Add(Party.Id);
        Archetypes.Add(Party.Archetype);
        for (const auto& Build : Party.Builds) { TestNotNull(Build + TEXT(" is a catalog build"), FBuildCatalog::Find(Build)); }
    }
    for (const TCHAR* Archetype : {TEXT("melee"), TEXT("ranged"), TEXT("support-poor"), TEXT("mixed"), TEXT("duplicate")})
    { TestTrue(FString(TEXT("Representative set has a ")) + Archetype + TEXT(" party"), Archetypes.Contains(Archetype)); }
    return !HasAnyErrors();
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBalanceOutputTest, "Project.Balance.OutputStaysUnderLocal",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FBalanceOutputTest::RunTest(const FString& Parameters)
{
    using BalanceHarness::ResolveOutputRoot;
    FString Local = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT(".local")));
    FPaths::NormalizeDirectoryName(Local);
    FString Root, Error;
    TestTrue(TEXT("Default root is accepted"), ResolveOutputRoot(FString(), Root, Error));
    TestTrue(TEXT("Default root is .local/balance/<stamp>"), Root.StartsWith(Local + TEXT("/balance/")));
    TestTrue(TEXT("Relative root inside .local/ is accepted"), ResolveOutputRoot(TEXT(".local/balance/check"), Root, Error));
    TestEqual(TEXT("Relative root resolves against the project"), Root, Local + TEXT("/balance/check"));
    TestTrue(TEXT("Absolute root inside .local/ is accepted"), ResolveOutputRoot(Local + TEXT("/balance/abs"), Root, Error));
    for (const FString& Outside : {FString(TEXT("../outside")), FString(TEXT("/tmp/balance")), FString(TEXT(".local/../Saved/balance")),
        FString(TEXT(".local")), FString(TEXT(".localx/balance")), FString(TEXT("Saved/balance"))})
    {
        Error.Reset();
        TestFalse(TEXT("Refused outside .local/: ") + Outside, ResolveOutputRoot(Outside, Root, Error));
        TestTrue(TEXT("Refusal leaves no root and says why: ") + Outside, Root.IsEmpty() && Error.Contains(TEXT(".local")));
    }
#if PLATFORM_UNIX
    // Real symlinks inside .local/: one to a folder outside it, one dangling, one back inside it.
    const FString Target = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("BalanceLinkTarget")));
    const FString OutLink = Local + TEXT("/balance-link-out-test"), Dangling = Local + TEXT("/balance-link-dangling-test"), InLink = Local + TEXT("/balance-link-in-test");
    IFileManager::Get().MakeDirectory(*Target, true);
    IFileManager::Get().MakeDirectory(*(Local + TEXT("/balance")), true);
    for (const FString& Link : {OutLink, Dangling, InLink}) { unlink(TCHAR_TO_UTF8(*Link)); }
    const bool bLinked = symlink(TCHAR_TO_UTF8(*Target), TCHAR_TO_UTF8(*OutLink)) == 0 &&
        symlink(TCHAR_TO_UTF8(*(Target + TEXT("-missing"))), TCHAR_TO_UTF8(*Dangling)) == 0 &&
        symlink(TCHAR_TO_UTF8(*(Local + TEXT("/balance"))), TCHAR_TO_UTF8(*InLink)) == 0;
    if (TestTrue(TEXT("Created the test symlinks"), bLinked))
    {
        Error.Reset();
        TestFalse(TEXT("Root through a link to outside .local/ is refused"), ResolveOutputRoot(TEXT(".local/balance-link-out-test/run"), Root, Error));
        TestTrue(TEXT("The refusal names the real destination"), Error.Contains(TEXT("BalanceLinkTarget")));
        TestFalse(TEXT("Root through a dangling link is refused"), ResolveOutputRoot(TEXT(".local/balance-link-dangling-test/run"), Root, Error));
        Error.Reset();
        TestTrue(TEXT("A group that climbs into the outside link is refused"),
            BalanceHarness::GroupFolderFor(TEXT(".local/balance/ok"), TEXT("../../balance-link-out-test/group"), Error).IsEmpty() && !Error.IsEmpty());
        Error.Reset();
        TestFalse(TEXT("EnsureFolder refuses the outside link"), BalanceHarness::EnsureFolder(OutLink + TEXT("/group"), Error));
        TestFalse(TEXT("Nothing was created behind the outside link"), IFileManager::Get().DirectoryExists(*(Target + TEXT("/group"))));
        Error.Reset();
        TestTrue(TEXT("A link that stays inside .local/ is accepted"), ResolveOutputRoot(TEXT(".local/balance-link-in-test/run"), Root, Error));
    }
    for (const FString& Link : {OutLink, Dangling, InLink}) { unlink(TCHAR_TO_UTF8(*Link)); }
    IFileManager::Get().DeleteDirectory(*Target, false, true);
#else
    AddInfo(TEXT("Physical (symlink) containment is checked on Unix only; this platform has the lexical check."));
#endif
    return !HasAnyErrors();
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBalanceDeadlockTest, "Project.Balance.DeadlockedStallIsRecognised",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FBalanceDeadlockTest::RunTest(const FString& Parameters)
{
    // The soft lock of PR #214's first head (trace rot2-03:20261119:10, fight 4): the last hero at (7, 8) with terrain at (7, 9), bodies at (7, 7),
    // (6, 8) and (8, 8), the brute at (6, 6). Nothing can ever reach anything: a deadlock. Open the terrain cell, or turn a body into a live ally,
    // and a path exists again. The sweep (Balance.Baseline.*) fails on any deadlocked run.
    FCombatSnapshot S;
    S.Encounter.Width = 16; S.Encounter.Height = 11;
    S.Encounter.BlockedCells = {FIntPoint(7, 9)};
    auto Add = [&S](int32 Id, ECombatSide Side, FIntPoint Cell, ECombatLife Life, int32 Range)
    {
        FCombatantState U; U.Id = Id; U.Side = Side; U.Cell = Cell; U.Life = Life; U.Range = Range; U.HP = Life == ECombatLife::Conscious ? 10 : 0;
        S.Encounter.Combatants.Add(U);
    };
    Add(4, ECombatSide::Party, {7, 8}, ECombatLife::Conscious, 1);
    Add(21, ECombatSide::Enemy, {7, 7}, ECombatLife::Dead, 1);
    Add(22, ECombatSide::Enemy, {6, 8}, ECombatLife::Dead, 1);
    Add(23, ECombatSide::Enemy, {8, 8}, ECombatLife::Dead, 4);
    Add(20, ECombatSide::Enemy, {6, 6}, ECombatLife::Conscious, 1);
    TestTrue(TEXT("The traced configuration is a deadlock"), BalanceHarness::Deadlocked(S));
    FCombatSnapshot Open = S; Open.Encounter.BlockedCells.Reset();
    TestFalse(TEXT("Without the terrain cell the hero can walk out"), BalanceHarness::Deadlocked(Open));
    FCombatSnapshot Live = S; Live.Encounter.Combatants[1].Life = ECombatLife::Conscious; Live.Encounter.Combatants[1].HP = 5; Live.Encounter.Combatants[1].Side = ECombatSide::Party;
    TestFalse(TEXT("A live unit is not a wall"), BalanceHarness::Deadlocked(Live));
    FCombatSnapshot Ranged = S; Ranged.Encounter.Combatants[0].Range = 3;
    TestFalse(TEXT("A ranged hero reaches the brute from where it stands"), BalanceHarness::Deadlocked(Ranged));
    return !HasAnyErrors();
}
#endif
