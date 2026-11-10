#if WITH_DEV_AUTOMATION_TESTS
#include "ExpeditionInputHarness.h"
#include "Presentation/ExpeditionAutoplay.h"
#include "Party/BuildCatalog.h"

namespace AutoplayTest
{
using namespace ExpeditionAutoplay;

constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter;

/** The shipped script the perf run uses; tests read the same file. */
bool LoadShipped(FAutomationTestBase& Test, FAutoplayScript& Script)
{
    const FString Path = FPaths::Combine(FPaths::ProjectDir(), TEXT("tools/autoplay/watchtower-road.json"));
    FString Text, Error;
    if (!Test.TestTrue(TEXT("the shipped autoplay script is readable"), FFileHelper::LoadFileToString(Text, *Path))) { return false; }
    const bool bParsed = ParseScript(Text, Script, Error);
    if (!bParsed) { Test.AddError(Error); }
    return Test.TestTrue(TEXT("the shipped autoplay script parses"), bParsed);
}

bool SameCommand(const FCombatCommand& A, const FCombatCommand& B)
{
    return A.Type == B.Type && A.ActorId == B.ActorId && A.Destination == B.Destination && A.TargetId == B.TargetId && A.ShoveChoice == B.ShoveChoice;
}

/** Lets enemies act until a party hero holds the turn (or the encounter ends). */
void EnemiesAct(FExpeditionRun& Run)
{
    for (int32 Guard = 0; Guard < 100 && Run.Progress().Phase == EExpeditionPhase::Combat; ++Guard)
    {
        const FCombatantState* Active = Run.Battle().Find(Run.Battle().ActiveId());
        if (!Active || Active->Side != ECombatSide::Enemy) { return; }
        Run.PlayEnemyTurn();
    }
}

/** Runs frames until the director settles; false if it never did. */
bool RunDirector(ExpeditionInputHarness::FHarness& Game, FExpeditionAutoplayDirector& Director, int32 MaxFrames)
{
    for (int32 Frame = 0; Frame < MaxFrames && Director.Status() == FExpeditionAutoplayDirector::EStatus::Running; ++Frame) { Game.Frame(); }
    return Director.Status() != FExpeditionAutoplayDirector::EStatus::Running;
}

/** Playback at the instant setting (the view still ticks banners and enemy delays), and a script with short waits. */
FAutoplayScript Quick(FAutoplayScript Script)
{
    Script.BootFrames = 2;
    Script.ThinkFrames = 1;
    Script.SettleFrames = 2;
    return Script;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAutoplayScriptTest, "Project.Expedition.Autoplay.ScriptIsValidated", AutoplayTest::Flags)
bool FAutoplayScriptTest::RunTest(const FString& Parameters)
{
    using namespace ExpeditionAutoplay;
    FAutoplayScript Shipped;
    if (!AutoplayTest::LoadShipped(*this, Shipped)) { return false; }
    TestEqual(TEXT("the script plays the player path's first seed"), Shipped.Seed, FirstRunSeed);
    TestEqual(TEXT("the script names four builds"), Shipped.Builds.Num(), 4);
    TestTrue(TEXT("the script takes the dangerous Watchtower road"), Shipped.bDangerous);
    TestTrue(TEXT("the script lists tactical variants for the planner"), Shipped.Variants.Num() >= 4);
    const TArray<FPartyBuildSelection> Party = PartyFor(Shipped);
    TestEqual(TEXT("the party has four members"), Party.Num(), 4);
    for (int32 Slot = 0; Slot < Party.Num(); ++Slot) { TestEqual(FString::Printf(TEXT("slot %d has id %d"), Slot, Slot + 1), Party[Slot].Id, Slot + 1); }

    // Each bad document is refused with a message that names the problem.
    const FString Good = TEXT("\"version\":1,\"seed\":20261030,\"party\":[\"fighter.champion\",\"rogue.thief\",\"cleric.life\",\"wizard.evocation\"]");
    struct FCase { const TCHAR* Label; FString Json; const TCHAR* Needle; };
    const FCase Cases[] = {
        {TEXT("not JSON"), TEXT("nope"), TEXT("JSON object")},
        {TEXT("no version"), TEXT("{\"seed\":20261030}"), TEXT("version")},
        {TEXT("a seed the player path cannot start"), TEXT("{\"version\":1,\"seed\":7,\"party\":[]}"), TEXT("20261030")},
        {TEXT("three builds"), TEXT("{\"version\":1,\"seed\":20261030,\"party\":[\"fighter.champion\",\"rogue.thief\",\"cleric.life\"]}"), TEXT("four")},
        {TEXT("an unknown build"), TEXT("{\"version\":1,\"seed\":20261030,\"party\":[\"fighter.champion\",\"rogue.thief\",\"cleric.life\",\"bard.nonsense\"]}"), TEXT("catalog")},
        {TEXT("an unknown route"), TEXT("{") + Good + TEXT(",\"route\":\"sideways\"}"), TEXT("route")},
        {TEXT("a string for a frame count"), TEXT("{") + Good + TEXT(",\"thinkFrames\":\"24\"}"), TEXT("thinkFrames")},
        {TEXT("a bait slot out of range"), TEXT("{") + Good + TEXT(",\"variants\":[{\"bait\":9}]}"), TEXT("bait")},
        {TEXT("an empty variant list"), TEXT("{") + Good + TEXT(",\"variants\":[]}"), TEXT("variants")},
    };
    for (const auto& Case : Cases)
    {
        FAutoplayScript Out;
        FString Error;
        TestFalse(FString::Printf(TEXT("%s is refused"), Case.Label), ParseScript(Case.Json, Out, Error));
        TestTrue(FString::Printf(TEXT("%s: the message names the problem (%s)"), Case.Label, *Error), Error.Contains(Case.Needle));
    }
    FAutoplayScript Minimal;
    FString Error;
    TestTrue(TEXT("a minimal script parses with defaults"), ParseScript(TEXT("{") + Good + TEXT("}"), Minimal, Error));
    TestEqual(TEXT("defaults give the planner four baits"), Minimal.Variants.Num(), 4);
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAutoplayPlanTest, "Project.Expedition.Autoplay.PlanReachesEveryBeat", AutoplayTest::Flags)
bool FAutoplayPlanTest::RunTest(const FString& Parameters)
{
    using namespace ExpeditionAutoplay;
    FAutoplayScript Script;
    if (!AutoplayTest::LoadShipped(*this, Script)) { return false; }
    const TArray<FPartyBuildSelection> Party = PartyFor(Script);
    FAutoplayPlan Plan;
    const bool bOk = BuildPlan(Script, Party, TEXT("autoplay-test"), Plan);
    for (const FString& Line : Plan.Report) { AddInfo(Line); }
    if (!TestTrue(TEXT("some variant reaches move, attack, special, downing, rescue and victory headlessly"), bOk)) { return false; }
    AddInfo(FString::Printf(TEXT("seed %d, variant %d, %d commands, round %d, %d survivor(s), unit %d downed"), Script.Seed, Plan.Variant, Plan.Steps.Num(), Plan.Rounds, Plan.Survivors, Plan.DownedId));
    for (const EBeat Beat : {EBeat::Move, EBeat::Attack, EBeat::Special, EBeat::Downing, EBeat::Rescue, EBeat::Victory})
    { TestTrue(FString::Printf(TEXT("the plan reaches %s"), BeatName(Beat)), Plan.Reached(Beat)); }
    TestTrue(TEXT("a party member survives to take the reward"), Plan.Survivors >= 1);

    const FAutoplayStep* Special = Plan.Steps.FindByPredicate([](const FAutoplayStep& Step)
        { return Step.Command.Type == ECombatCommandType::Special1 || Step.Command.Type == ECombatCommandType::Special2; });
    TestNotNull(TEXT("the special is a Special1 or Special2 command"), Special);
    const FAutoplayStep* Rescue = Plan.Steps.FindByPredicate([](const FAutoplayStep& Step) { return Step.Command.Type == ECombatCommandType::Rescue; });
    if (TestNotNull(TEXT("the plan has a Rescue command"), Rescue))
    {
        const FCombatantState* Target = Rescue->Before.Battle.Encounter.Combatants.FindByPredicate([Rescue](const FCombatantState& Unit) { return Unit.Id == Rescue->Command.TargetId; });
        if (TestNotNull(TEXT("the rescued unit is in the encounter"), Target)) { TestTrue(TEXT("the rescued unit was downed when the Rescue was issued"), Target->Life == ECombatLife::Downed); }
    }

    // Every step is a real command: replaying them on a fresh run, with the enemies acting between, meets the recorded states.
    FExpeditionRun Run;
    FString Error;
    if (!TestTrue(TEXT("the run starts at the script's seed"), Run.NewRun(Script.Seed, Party, Error, TEXT("autoplay-test")))) { return false; }
    const FExpeditionRoute* Dangerous = Run.Progress().Offers.FindByPredicate([](const FExpeditionRoute& Route) { return Route.bDangerous; });
    if (!TestNotNull(TEXT("the dangerous route is offered"), Dangerous)) { return false; }
    TestTrue(TEXT("the route is taken"), Run.ChooseRoute(Dangerous->Id, Error));
    TestTrue(TEXT("the opening matches"), SameModuloIdentity(Run.Snapshot(), Plan.Opening));
    for (int32 Index = 0; Index < Plan.Steps.Num(); ++Index)
    {
        AutoplayTest::EnemiesAct(Run);
        const FAutoplayStep& Step = Plan.Steps[Index];
        if (!TestTrue(FString::Printf(TEXT("step %d starts from the recorded state"), Index), SameModuloIdentity(Run.Snapshot(), Step.Before))) { return false; }
        if (!TestTrue(FString::Printf(TEXT("step %d is accepted"), Index), Run.Submit(Step.Command).bAccepted)) { return false; }
    }
    TestEqual(TEXT("the replay ends at the reward"), Run.Progress().Phase, EExpeditionPhase::Reward);
    TestTrue(TEXT("the replay ends in the recorded final state"), SameModuloIdentity(Run.Snapshot(), Plan.Final));

    // The plan is a pure function of the script: planning again gives the same commands.
    FAutoplayPlan Again;
    TestTrue(TEXT("planning again succeeds"), BuildPlan(Script, Party, TEXT("autoplay-test-2"), Again));
    if (TestEqual(TEXT("planning again picks the same variant"), Again.Variant, Plan.Variant) && TestEqual(TEXT("and as many commands"), Again.Steps.Num(), Plan.Steps.Num()))
    {
        for (int32 Index = 0; Index < Plan.Steps.Num(); ++Index) { TestTrue(FString::Printf(TEXT("command %d repeats"), Index), AutoplayTest::SameCommand(Plan.Steps[Index].Command, Again.Steps[Index].Command)); }
    }
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAutoplayLiveTest, "Project.Expedition.Autoplay.LiveRunMatchesDirectCommands", AutoplayTest::Flags)
bool FAutoplayLiveTest::RunTest(const FString& Parameters)
{
    using namespace ExpeditionAutoplay;
    FAutoplayScript Shipped;
    if (!AutoplayTest::LoadShipped(*this, Shipped)) { return false; }
    const FAutoplayScript Script = AutoplayTest::Quick(Shipped);

    // Fresh pass through the real controller and view, in a world with no saved run.
    TArray<uint8> LiveBytes;
    FExpeditionSaveData Saved;
    TArray<FAutoplayStep> Steps;
    {
        ExpeditionInputHarness::FHarness Game(*this, TEXT("AutoplayFresh"));
        if (!TestTrue(TEXT("Input, board and HUD are ready"), Game.IsReady())) { return false; }
        Game.View->CycleSpeed();
        Game.View->CycleSpeed();
        TestEqual(TEXT("playback is instant for the test"), Game.View->Speed(), EPlaybackSpeed::Instant);
        TSharedPtr<FExpeditionAutoplayDirector> Director = Game.Player->StartAutoplay(Script, false, false);
        if (!TestTrue(TEXT("the director starts"), Director.IsValid())) { return false; }
        const bool bSettled = AutoplayTest::RunDirector(Game, *Director, 20000);
        TestTrue(TEXT("autoplay settles within the frame budget"), bSettled);
        TestTrue(FString::Printf(TEXT("autoplay passes (%s)"), *Director->Failure()), Director->Status() == FExpeditionAutoplayDirector::EStatus::Passed);
        if (Director->Status() != FExpeditionAutoplayDirector::EStatus::Passed) { return false; }
        for (const EBeat Beat : {EBeat::Move, EBeat::Attack, EBeat::Special, EBeat::Downing, EBeat::Rescue, EBeat::Victory, EBeat::Quit})
        { TestTrue(FString::Printf(TEXT("the live run met the beat %s"), BeatName(Beat)), Director->Saw(Beat)); }
        for (const TCHAR* Name : {TEXT("plan"), TEXT("start->route"), TEXT("route->battle"), TEXT("battle->reward"), TEXT("quit")})
        { TestTrue(FString::Printf(TEXT("the transition %s began and ended"), Name), Director->Transitions().Contains(FString(Name))); }
        TestFalse(TEXT("quitting put the run away"), Game.View->HasRun());
        TestTrue(TEXT("the checkpoint is kept for a fresh process"), Game.View->HasUnfinishedCheckpoint());
        LiveBytes = Game.CheckpointBytes();
        Steps = Director->Plan().Steps;
        FString Error;
        if (!TestTrue(TEXT("the checkpoint decodes"), FExpeditionSave::Read(Game.Path, Saved, Error))) { return false; }
        TestEqual(TEXT("the checkpoint waits at the reward"), Saved.Current.Progress.Phase, EExpeditionPhase::Reward);
    }

    // The same commands issued straight to a session give the same checkpoint bytes.
    {
        const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation/AutoplayDirect"), FGuid::NewGuid().ToString());
        const FString Path = FPaths::Combine(Directory, TEXT("current.json"));
        FExpeditionSession Direct(Path);
        FString Error;
        if (!TestTrue(TEXT("the direct session starts with the live run's identity"), Direct.NewRun(Script.Seed, PartyFor(Script), Error, Saved.Current.Progress.RunId))) { AddError(Error); return false; }
        const FExpeditionRoute* Dangerous = Direct.Run().Progress().Offers.FindByPredicate([](const FExpeditionRoute& Route) { return Route.bDangerous; });
        if (!TestNotNull(TEXT("the dangerous route is offered"), Dangerous)) { return false; }
        TestTrue(TEXT("the route is taken"), Direct.ChooseRoute(Dangerous->Id, Error));
        for (const FAutoplayStep& Step : Steps)
        {
            for (int32 Guard = 0; Guard < 100 && Direct.Run().Progress().Phase == EExpeditionPhase::Combat; ++Guard)
            {
                const FCombatantState* Active = Direct.Run().Battle().Find(Direct.Run().Battle().ActiveId());
                if (!Active || Active->Side != ECombatSide::Enemy) { break; }
                Direct.PlayEnemyTurn();
            }
            if (!TestTrue(TEXT("a direct command is accepted"), Direct.Submit(Step.Command).bAccepted)) { return false; }
        }
        TArray<uint8> DirectBytes;
        FFileHelper::LoadFileToArray(DirectBytes, *Path, FILEREAD_Silent);
        TestFalse(TEXT("the direct run wrote a checkpoint"), DirectBytes.IsEmpty());
        TestTrue(TEXT("autoplay and direct commands leave identical checkpoint bytes"), DirectBytes == LiveBytes);
        IFileManager::Get().DeleteDirectory(*Directory, false, true);
    }

    // A fresh process resumes that checkpoint, claims the reward and takes the camp.
    {
        ExpeditionInputHarness::FHarness Game(*this, TEXT("AutoplayResume"), &Saved);
        if (!TestTrue(TEXT("Input, board and HUD are ready for the resume pass"), Game.IsReady())) { return false; }
        Game.View->CycleSpeed();
        Game.View->CycleSpeed();
        TSharedPtr<FExpeditionAutoplayDirector> Director = Game.Player->StartAutoplay(Script, true, false);
        if (!TestTrue(TEXT("the resume director starts"), Director.IsValid())) { return false; }
        TestTrue(TEXT("the resume pass settles within the frame budget"), AutoplayTest::RunDirector(Game, *Director, 8000));
        TestTrue(FString::Printf(TEXT("the resume pass passes (%s)"), *Director->Failure()), Director->Status() == FExpeditionAutoplayDirector::EStatus::Passed);
        for (const TCHAR* Name : {TEXT("resume"), TEXT("reward->route")})
        { TestTrue(FString::Printf(TEXT("the transition %s began and ended"), Name), Director->Transitions().Contains(FString(Name))); }
        TestEqual(TEXT("the resumed run reaches the route table"), Game.View->Progress().Phase, EExpeditionPhase::Route);
        TestEqual(TEXT("one encounter is done"), Game.View->Progress().EncountersCompleted, 1);
    }
    return !HasAnyErrors();
}
#endif
