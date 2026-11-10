#if WITH_DEV_AUTOMATION_TESTS
#include "BalanceHarness.h"

#include "HAL/PlatformMisc.h"
#include "HAL/PlatformTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/AutomationTest.h"

// The bulk balance baseline (#179). Deliberately outside the Project. prefix: run it explicitly with
// `tools/dev.py --cores 2 test --filter Balance.Baseline.` (or one group), then tools/balance_report.py.
namespace BalanceBaseline
{
/**
 * Plays one party (all seeds) per frame, so the game thread keeps ticking between parties: a single
 * 20-second block made the engine's own pending HTTP checks time out and log warnings.
 */
class FPlayGroupCommand : public IAutomationLatentCommand
{
public:
    FPlayGroupCommand(FAutomationTestBase& InTest, FString InGroup, TArray<BalanceHarness::FPartySpec> InParties)
        : Test(InTest), Group(MoveTemp(InGroup)), Parties(MoveTemp(InParties)) {}

    virtual bool Update() override
    {
        using namespace BalanceHarness;
        if (Next < Parties.Num())
        {
            const double Start = FPlatformTime::Seconds();
            Runs.Append(PlayGroup({Parties[Next++]}));
            Seconds += FPlatformTime::Seconds() - Start;
            return false;
        }
        int32 Errors = 0, Illegal = 0, Passive = 0, Mismatches = 0, Stalls = 0, Wins = 0;
        for (const auto& Run : Runs)
        {
            Errors += Run.PolicyErrors; Illegal += Run.IllegalProposals; Passive += Run.PassiveEnds; Mismatches += Run.ProtectionMismatches;
            Stalls += Run.Outcome == TEXT("Stall") ? 1 : 0; Wins += Run.Outcome == TEXT("Victory") ? 1 : 0;
        }
        FString Error;
        const FString Folder = GroupFolder(Group, Error);
        const bool bWritten = WriteGroup(Folder, Group, Parties, Runs, Seconds, Error);
        Test.TestTrue(TEXT("Group results written to ") + Folder + TEXT(" ") + Error, bWritten);
        Test.TestEqual(TEXT("Every party played every seed"), Runs.Num(), Parties.Num() * SeedCount);
        Test.TestEqual(TEXT("No command was rejected"), Errors, 0);
        Test.TestEqual(TEXT("No illegal proposal"), Illegal, 0);
        Test.TestEqual(TEXT("No passive end"), Passive, 0);
        Test.TestEqual(TEXT("Every committed prevention equals the protection query"), Mismatches, 0);
        // Stalls are findings, reported by the summary, not test failures.
        Test.AddInfo(FString::Printf(TEXT("Balance group %s: %d runs, %d victories, %d stalls, %.1f s -> %s"), *Group, Runs.Num(), Wins, Stalls, Seconds, *Folder));
        return true;
    }

private:
    FAutomationTestBase& Test;
    FString Group;
    TArray<BalanceHarness::FPartySpec> Parties;
    TArray<BalanceHarness::FRunStats> Runs;
    int32 Next = 0;
    double Seconds = 0.0;
};

bool RunGroup(FAutomationTestBase& Test, const FString& Group, const TArray<BalanceHarness::FPartySpec>& Parties)
{
    ADD_LATENT_AUTOMATION_COMMAND(FPlayGroupCommand(Test, Group, Parties));
    return true;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBalanceBaselineFixed, "Balance.Baseline.Fixed", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FBalanceBaselineFixed::RunTest(const FString& Parameters) { return BalanceBaseline::RunGroup(*this, TEXT("Fixed"), BalanceHarness::RepresentativeParties()); }

#define BALANCE_ROTATION_GROUP(N) \
    IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBalanceBaselineRotation##N, "Balance.Baseline.Rotation" #N, EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter) \
    bool FBalanceBaselineRotation##N::RunTest(const FString& Parameters) { return BalanceBaseline::RunGroup(*this, TEXT("Rotation" #N), BalanceHarness::RotationParties(N - 1)); }
BALANCE_ROTATION_GROUP(1)
BALANCE_ROTATION_GROUP(2)
BALANCE_ROTATION_GROUP(3)
BALANCE_ROTATION_GROUP(4)
BALANCE_ROTATION_GROUP(5)
#undef BALANCE_ROTATION_GROUP

// Reproduce one run as a text trace: DND_BALANCE_TRACE=<party>:<seed>:<route pattern>, e.g. familiar:20261116:23.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBalanceTrace, "Balance.Trace", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FBalanceTrace::RunTest(const FString& Parameters)
{
    TArray<FString> Parts;
    FPlatformMisc::GetEnvironmentVariable(TEXT("DND_BALANCE_TRACE")).ParseIntoArray(Parts, TEXT(":"));
    if (!TestEqual(TEXT("DND_BALANCE_TRACE is <party>:<seed>:<route pattern>"), Parts.Num(), 3)) { return false; }
    const BalanceHarness::FPartySpec* Party = BalanceHarness::FindParty(Parts[0]);
    if (!TestNotNull(TEXT("Known party ") + Parts[0], Party)) { return false; }
    FString Error;
    const FString Folder = BalanceHarness::GroupFolder(TEXT("trace"), Error);
    if (!TestTrue(TEXT("Trace folder inside .local/ ") + Error, BalanceHarness::EnsureFolder(Folder, Error))) { return false; }
    const FString Path = FPaths::Combine(Folder, FString::Printf(TEXT("%s-%s-%s.txt"), *Parts[0], *Parts[1], *Parts[2]));
    TestTrue(TEXT("Trace written to ") + Path, FFileHelper::SaveStringToFile(BalanceHarness::TraceRun(*Party, FCString::Atoi(*Parts[1]), FCString::Atoi(*Parts[2])), *Path));
    return !HasAnyErrors();
}
#endif
