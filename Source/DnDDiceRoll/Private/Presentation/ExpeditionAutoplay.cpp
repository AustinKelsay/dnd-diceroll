#include "Presentation/ExpeditionAutoplay.h"
#include "Party/BuildCatalog.h"
#include "Presentation/ExpeditionBattleView.h"
#include "Presentation/ExpeditionPlayerController.h"
#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformTime.h"
#include "Misc/CommandLine.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "ProfilingDebugging/CsvProfiler.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Class.h"

DEFINE_LOG_CATEGORY_STATIC(LogExpeditionAutoplay, Log, All);

using namespace ExpeditionAutoplay;

namespace
{
int32 Steps(FIntPoint A, FIntPoint B) { return FMath::Abs(A.X - B.X) + FMath::Abs(A.Y - B.Y); }

// ---------------------------------------------------------------------------------------------------------------
// Script parsing.

// TryGetNumberField also accepts numeric strings; scripts must hold real numbers.
bool WholeNumber(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field, int32 Min, int32 Max, int32& Value, FString& Error)
{
    const TSharedPtr<FJsonValue> Entry = Object->TryGetField(Field);
    if (!Entry.IsValid()) { return true; }
    double Number = 0.0;
    if (Entry->Type != EJson::Number || !Entry->TryGetNumber(Number) || Number != FMath::FloorToDouble(Number) || Number < Min || Number > Max)
    {
        Error = FString::Printf(TEXT("\"%s\" must be a whole number from %d to %d"), Field, Min, Max);
        return false;
    }
    Value = static_cast<int32>(Number);
    return true;
}

bool Truth(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field, bool& Value, FString& Error)
{
    const TSharedPtr<FJsonValue> Entry = Object->TryGetField(Field);
    if (!Entry.IsValid()) { return true; }
    if (Entry->Type != EJson::Boolean || !Entry->TryGetBool(Value)) { Error = FString::Printf(TEXT("\"%s\" must be true or false"), Field); return false; }
    return true;
}

// ---------------------------------------------------------------------------------------------------------------
// The player policy. It reads only the rules' public queries and returns one party command at a time.

/** What the policy remembers between commands: which beats have happened. */
struct FHistory
{
    bool bMove = false, bAttack = false, bSpecial = false, bDowning = false, bRescue = false;
};

FCombatCommand EndTurn(int32 Id) { return {ECombatCommandType::EndTurn, Id}; }

const FCombatantState* NearestFoe(const FCombatBattle& Battle, FIntPoint From, int32& Distance)
{
    const FCombatantState* Best = nullptr;
    Distance = MAX_int32;
    for (const FCombatantState& Unit : Battle.Snapshot().Encounter.Combatants)
    {
        if (Unit.Side != ECombatSide::Enemy || Unit.Life != ECombatLife::Conscious) { continue; }
        const int32 Here = Steps(From, Unit.Cell);
        if (Here < Distance || (Here == Distance && Best && Unit.Id < Best->Id)) { Best = &Unit; Distance = Here; }
    }
    return Best;
}

const FCombatantState* FirstDowned(const FCombatBattle& Battle)
{
    for (const FCombatantState& Unit : Battle.Snapshot().Encounter.Combatants)
    { if (Unit.Side == ECombatSide::Party && Unit.Life == ECombatLife::Downed) { return &Unit; } }
    return nullptr;
}

/** The legal move with the lowest score that beats Current (ties: lowest row, then column); false when none does. */
template <typename TScore>
bool BestMove(const FCombatBattle& Battle, int32 Id, int32 Current, TScore Score, FIntPoint& Out)
{
    bool bFound = false;
    int32 Best = Current;
    for (const FIntPoint& Cell : Battle.LegalMoves(Id))
    {
        const int32 Here = Score(Cell);
        const bool bTie = bFound && Here == Best && (Cell.Y < Out.Y || (Cell.Y == Out.Y && Cell.X < Out.X));
        if (Here < Best || bTie) { Best = Here; Out = Cell; bFound = true; }
    }
    return bFound;
}

/** A Special1 or Special2 the active hero can use now (a swap would only shuffle the line, so it is skipped). */
bool SpecialFor(const FCombatBattle& Battle, const FCombatantState& Me, FCombatCommand& Out)
{
    for (const FAbilityFacts& Ability : FBuildCatalog::Abilities(Me))
    {
        if ((Ability.Command != ECombatCommandType::Special1 && Ability.Command != ECombatCommandType::Special2) || Ability.Effect == EAbilityEffect::Swap) { continue; }
        const TArray<int32> Targets = Battle.LegalTargets(Me.Id, Ability.Command);
        if (Targets.IsEmpty()) { continue; }
        Out = {Ability.Command, Me.Id, {}, Targets[0]};
        return true;
    }
    return false;
}

int32 WeakestTarget(const FCombatBattle& Battle, const TArray<int32>& Targets)
{
    int32 Pick = Targets[0];
    int32 Lowest = MAX_int32;
    for (const int32 Id : Targets)
    {
        const FCombatantState* Unit = Battle.Find(Id);
        if (Unit && Unit->HP < Lowest) { Lowest = Unit->HP; Pick = Id; }
    }
    return Pick;
}

/**
 * Before any party member has fallen the bait (a front-line hero) walks up, attacks once and then only stands;
 * the others use one special and hold. Once someone is downed, whoever acts walks to them and rescues them.
 * After that the party fights normally: wounded heroes drink and step back, the rest hit the weakest foe.
 */
FCombatCommand Next(const FCombatBattle& Battle, const FTactic& Variant, const FHistory& Memory)
{
    const int32 Id = Battle.ActiveId();
    const FCombatantState* Me = Battle.Find(Id);
    if (!Me || Me->Side != ECombatSide::Party || Me->Life != ECombatLife::Conscious) { return EndTurn(Id); }
    const bool bBait = Id == Variant.Bait + 1;
    int32 FoeDistance = 0;
    const FCombatantState* Foe = NearestFoe(Battle, Me->Cell, FoeDistance);
    FIntPoint Cell = FIntPoint::ZeroValue;
    FCombatCommand Special;

    if (const FCombatantState* Down = FirstDowned(Battle))
    {
        if (Battle.LegalTargets(Id, ECombatCommandType::Rescue).Contains(Down->Id)) { return {ECombatCommandType::Rescue, Id, {}, Down->Id}; }
        const FIntPoint Goal = Down->Cell;
        if (BestMove(Battle, Id, Steps(Me->Cell, Goal), [Goal](FIntPoint Here) { return Steps(Here, Goal); }, Cell)) { return {ECombatCommandType::Move, Id, Cell}; }
        // No way closer: clear the foes that crowd the fallen hero.
        const TArray<int32> Targets = Battle.LegalTargets(Id);
        if (!Targets.IsEmpty()) { return {ECombatCommandType::Attack, Id, {}, WeakestTarget(Battle, Targets)}; }
        return EndTurn(Id);
    }

    if (!Memory.bDowning)
    {
        if (bBait)
        {
            if (Variant.bAdvance && Foe && FoeDistance > 1 &&
                BestMove(Battle, Id, FoeDistance, [&Battle](FIntPoint Here) { int32 Near = 0; NearestFoe(Battle, Here, Near); return Near; }, Cell))
            { return {ECombatCommandType::Move, Id, Cell}; }
            if (!Memory.bAttack)
            {
                const TArray<int32> Targets = Battle.LegalTargets(Id);
                if (!Targets.IsEmpty()) { return {ECombatCommandType::Attack, Id, {}, Targets[0]}; }
            }
        }
        else if (Variant.bFallBack && Foe && FoeDistance <= 3 &&
            BestMove(Battle, Id, -FoeDistance, [&Battle](FIntPoint Here) { int32 Near = 0; NearestFoe(Battle, Here, Near); return -Near; }, Cell))
        { return {ECombatCommandType::Move, Id, Cell}; }
        if (!Memory.bSpecial && SpecialFor(Battle, *Me, Special)) { return Special; }
        return EndTurn(Id);
    }

    if (Me->HP * 2 <= Me->MaxHP)
    {
        if (Battle.LegalTargets(Id, ECombatCommandType::Potion).Contains(Id)) { return {ECombatCommandType::Potion, Id, {}, Id}; }
        if (Battle.LegalTargets(Id, ECombatCommandType::SecondWind).Contains(Id)) { return {ECombatCommandType::SecondWind, Id, {}, Id}; }
        if (Variant.bRetreat && Foe && FoeDistance <= 2 &&
            BestMove(Battle, Id, -FoeDistance, [&Battle](FIntPoint Here) { int32 Near = 0; NearestFoe(Battle, Here, Near); return -Near; }, Cell))
        { return {ECombatCommandType::Move, Id, Cell}; }
    }
    if (!Memory.bSpecial && SpecialFor(Battle, *Me, Special)) { return Special; }
    const TArray<int32> Targets = Battle.LegalTargets(Id);
    if (!Targets.IsEmpty()) { return {ECombatCommandType::Attack, Id, {}, WeakestTarget(Battle, Targets)}; }
    if (Battle.Snapshot().bActionAvailable && Foe &&
        BestMove(Battle, Id, FoeDistance, [&Battle](FIntPoint Here) { int32 Near = 0; NearestFoe(Battle, Here, Near); return Near; }, Cell))
    { return {ECombatCommandType::Move, Id, Cell}; }
    return EndTurn(Id);
}

/** Notes which beat an accepted command is the first to meet. */
void Observe(FHistory& Memory, const FCombatCommand& Command, FAutoplayStep& Step)
{
    auto Meet = [&Step](bool& bFlag, EBeat Beat) { if (!bFlag) { bFlag = true; Step.Beats.Add(Beat); } };
    switch (Command.Type)
    {
    case ECombatCommandType::Move: Meet(Memory.bMove, EBeat::Move); break;
    case ECombatCommandType::Attack:
    case ECombatCommandType::PrecisionStrike: Meet(Memory.bAttack, EBeat::Attack); break;
    case ECombatCommandType::Special1:
    case ECombatCommandType::Special2: Meet(Memory.bSpecial, EBeat::Special); break;
    case ECombatCommandType::Rescue: Meet(Memory.bRescue, EBeat::Rescue); break;
    default: break;
    }
}

int32 CountSurvivors(const FCombatBattle& Battle)
{
    int32 Count = 0;
    for (const FCombatantState& Unit : Battle.Snapshot().Encounter.Combatants)
    { if (Unit.Side == ECombatSide::Party && Unit.Life == ECombatLife::Conscious) { ++Count; } }
    return Count;
}

/** One variant played to its end against the rules. Fills Out; Why says what stopped it when it returns false. */
bool Simulate(const FAutoplayScript& Script, const FTactic& Variant, const TArray<FPartyBuildSelection>& Party, const FString& Identity, FAutoplayPlan& Out, FString& Why)
{
    Out = FAutoplayPlan();
    FExpeditionRun Run;
    FString Error;
    if (!Run.NewRun(Script.Seed, Party, Error, Identity)) { Why = TEXT("the run did not start: ") + Error; return false; }
    int32 OfferId = INDEX_NONE;
    for (const FExpeditionRoute& Route : Run.Progress().Offers) { if (Route.bDangerous == Script.bDangerous) { OfferId = Route.Id; break; } }
    if (OfferId == INDEX_NONE || !Run.ChooseRoute(OfferId, Error)) { Why = TEXT("the route could not be taken: ") + Error; return false; }
    Out.Opening = Run.Snapshot();

    FHistory Memory;
    auto NoteDowning = [&]()
    {
        if (Memory.bDowning) { return; }
        if (const FCombatantState* Down = FirstDowned(Run.Battle()))
        {
            Memory.bDowning = true;
            Out.bReached[static_cast<int32>(EBeat::Downing)] = true;
            Out.DownedId = Down->Id;
        }
    };
    for (int32 Guard = 0; Guard < 4000 && Run.Progress().Phase == EExpeditionPhase::Combat; ++Guard)
    {
        const FCombatBattle& Battle = Run.Battle();
        if (Battle.Snapshot().Round > Variant.MaxRounds) { Why = FString::Printf(TEXT("still fighting at round %d"), Battle.Snapshot().Round); return false; }
        const int32 Id = Battle.ActiveId();
        const FCombatantState* Active = Battle.Find(Id);
        if (!Active) { Why = TEXT("no unit holds the turn"); return false; }
        if (Active->Side == ECombatSide::Enemy)
        {
            if (Run.PlayEnemyTurn().IsEmpty()) { Why = TEXT("an enemy turn did nothing"); return false; }
            NoteDowning();
            continue;
        }
        bool bEnded = false;
        for (int32 Count = 0; Count < 16 && !bEnded; ++Count)
        {
            FAutoplayStep Step;
            Step.Command = Next(Run.Battle(), Variant, Memory);
            Step.Before = Run.Snapshot();
            if (!Run.Submit(Step.Command).bAccepted)
            {
                Why = FString::Printf(TEXT("the rules refused command %d (type %d) of unit %d"), Out.Steps.Num(), static_cast<int32>(Step.Command.Type), Id);
                return false;
            }
            Observe(Memory, Step.Command, Step);
            for (const EBeat Beat : Step.Beats) { Out.bReached[static_cast<int32>(Beat)] = true; }
            Out.Steps.Add(MoveTemp(Step));
            NoteDowning();
            bEnded = Run.Progress().Phase != EExpeditionPhase::Combat || Run.Battle().ActiveId() != Id;
        }
        if (!bEnded) { Why = FString::Printf(TEXT("unit %d never ended its turn"), Id); return false; }
    }
    const EExpeditionPhase Phase = Run.Progress().Phase;
    if (Phase == EExpeditionPhase::Combat) { Why = TEXT("the fight never ended"); return false; }
    if (Phase == EExpeditionPhase::Defeat) { Why = TEXT("the party was defeated"); return false; }
    Out.bReached[static_cast<int32>(EBeat::Victory)] = Phase == EExpeditionPhase::Reward;
    Out.Final = Run.Snapshot();
    Out.Survivors = CountSurvivors(Run.Battle());
    Out.Rounds = Run.Battle().Snapshot().Round;
    FString Missing;
    for (const EBeat Beat : {EBeat::Move, EBeat::Attack, EBeat::Special, EBeat::Downing, EBeat::Rescue, EBeat::Victory})
    { if (!Out.Reached(Beat)) { Missing += (Missing.IsEmpty() ? TEXT("") : TEXT(", ")) + FString(BeatName(Beat)); } }
    if (!Missing.IsEmpty()) { Why = TEXT("never reached: ") + Missing; return false; }
    Out.bComplete = true;
    return true;
}

// ---------------------------------------------------------------------------------------------------------------
// The live driver's helpers.

#if CSV_PROFILER
void LogCsvFinished(const FString& Filename)
{
    UE_LOG(LogExpeditionAutoplay, Display, TEXT("AUTOPLAY_CSV path=%s"), *Filename);
}
#endif

const TCHAR* StageName(int32 Stage)
{
    static const TCHAR* Names[] = {TEXT("boot"), TEXT("builder"), TEXT("starting"), TEXT("routing"), TEXT("choosing"), TEXT("fight"), TEXT("victory"), TEXT("leaving"),
        TEXT("resuming"), TEXT("claiming"), TEXT("camping"), TEXT("flushing"), TEXT("done")};
    return Stage >= 0 && Stage < UE_ARRAY_COUNT(Names) ? Names[Stage] : TEXT("?");
}
}

const TCHAR* ExpeditionAutoplay::BeatName(EBeat Beat)
{
    switch (Beat)
    {
    case EBeat::Move: return TEXT("move");
    case EBeat::Attack: return TEXT("attack");
    case EBeat::Special: return TEXT("special");
    case EBeat::Downing: return TEXT("downing");
    case EBeat::Rescue: return TEXT("rescue");
    case EBeat::Victory: return TEXT("victory");
    case EBeat::Quit: return TEXT("quit");
    }
    return TEXT("?");
}

bool ExpeditionAutoplay::ParseScript(const FString& Json, FAutoplayScript& Out, FString& Error)
{
    Out = FAutoplayScript();
    Error.Reset();
    TSharedPtr<FJsonObject> Root;
    if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Root) || !Root.IsValid()) { Error = TEXT("The autoplay script is not a JSON object"); return false; }
    int32 Version = 0;
    if (!Root->HasField(TEXT("version")) || !WholeNumber(Root, TEXT("version"), 1, 1, Version, Error) || Version != 1)
    { if (Error.IsEmpty()) { Error = TEXT("The autoplay script needs \"version\": 1"); } return false; }
    FString Name;
    if (Root->TryGetStringField(TEXT("name"), Name)) { Out.Name = Name; }
    if (Out.Name.IsEmpty()) { Out.Name = TEXT("autoplay"); }
    if (!WholeNumber(Root, TEXT("seed"), 0, MAX_int32 - 1, Out.Seed, Error)) { return false; }
    if (Out.Seed != FirstRunSeed)
    {
        Error = FString::Printf(TEXT("\"seed\" must be %d: the player path starts every first run there and offers no other seed (vary \"party\" and \"variants\" instead)"), FirstRunSeed);
        return false;
    }
    const TArray<TSharedPtr<FJsonValue>>* Party = nullptr;
    if (!Root->TryGetArrayField(TEXT("party"), Party) || Party->Num() != 4) { Error = TEXT("\"party\" must list exactly four catalog build ids, in slot order"); return false; }
    for (const TSharedPtr<FJsonValue>& Entry : *Party)
    {
        FString Id;
        if (!Entry.IsValid() || Entry->Type != EJson::String || !Entry->TryGetString(Id) || !FBuildCatalog::Find(Id))
        { Error = TEXT("\"party\" holds an entry that is not a catalog build id"); return false; }
        Out.Builds.Add(Id);
    }
    FString Route = TEXT("dangerous");
    Root->TryGetStringField(TEXT("route"), Route);
    if (Route != TEXT("dangerous") && Route != TEXT("guarded")) { Error = TEXT("\"route\" must be \"dangerous\" or \"guarded\""); return false; }
    Out.bDangerous = Route == TEXT("dangerous");
    if (!WholeNumber(Root, TEXT("bootFrames"), 1, 36000, Out.BootFrames, Error) || !WholeNumber(Root, TEXT("thinkFrames"), 0, 600, Out.ThinkFrames, Error) ||
        !WholeNumber(Root, TEXT("settleFrames"), 1, 600, Out.SettleFrames, Error) || !WholeNumber(Root, TEXT("stageSeconds"), 10, 3600, Out.StageSeconds, Error) ||
        !WholeNumber(Root, TEXT("fightSeconds"), 30, 7200, Out.FightSeconds, Error)) { return false; }
    const TArray<TSharedPtr<FJsonValue>>* Variants = nullptr;
    if (Root->HasField(TEXT("variants")))
    {
        if (!Root->TryGetArrayField(TEXT("variants"), Variants) || Variants->IsEmpty()) { Error = TEXT("\"variants\" must be a non-empty array"); return false; }
        for (int32 Index = 0; Index < Variants->Num(); ++Index)
        {
            const TSharedPtr<FJsonObject>* Entry = nullptr;
            if (!(*Variants)[Index].IsValid() || !(*Variants)[Index]->TryGetObject(Entry)) { Error = FString::Printf(TEXT("Variant %d is not an object"), Index); return false; }
            FTactic Variant;
            FString Problem;
            if (!WholeNumber(*Entry, TEXT("bait"), 0, 3, Variant.Bait, Problem) || !Truth(*Entry, TEXT("advance"), Variant.bAdvance, Problem) ||
                !Truth(*Entry, TEXT("fallBack"), Variant.bFallBack, Problem) || !Truth(*Entry, TEXT("retreat"), Variant.bRetreat, Problem) ||
                !WholeNumber(*Entry, TEXT("maxRounds"), 3, 200, Variant.MaxRounds, Problem))
            { Error = FString::Printf(TEXT("Variant %d: %s"), Index, *Problem); return false; }
            Out.Variants.Add(Variant);
        }
    }
    else
    {
        for (int32 Bait = 0; Bait < 4; ++Bait) { FTactic Variant; Variant.Bait = Bait; Out.Variants.Add(Variant); }
    }
    return true;
}

TArray<FPartyBuildSelection> ExpeditionAutoplay::PartyFor(const FAutoplayScript& Script)
{
    // The same names the controller's preset gives, so the party is the one the start screen builds.
    const TCHAR* Names[] = {TEXT("Arden"), TEXT("Nix"), TEXT("Sera"), TEXT("Ivo")};
    TArray<FPartyBuildSelection> Party;
    for (int32 Slot = 0; Slot < 4 && Slot < Script.Builds.Num(); ++Slot)
    {
        FPartyBuildSelection Member;
        Member.Id = Slot + 1;
        Member.Name = Names[Slot];
        Member.BuildId = Script.Builds[Slot];
        Party.Add(Member);
    }
    return Party;
}

bool ExpeditionAutoplay::SameModuloIdentity(const FExpeditionSnapshot& A, const FExpeditionSnapshot& B)
{
    FExpeditionSnapshot Other = B;
    Other.Progress.RunId = A.Progress.RunId;
    return FExpeditionSnapshot::StaticStruct()->CompareScriptStruct(&A, &Other, 0);
}

bool ExpeditionAutoplay::BuildPlan(const FAutoplayScript& Script, const TArray<FPartyBuildSelection>& Party, const FString& Identity, FAutoplayPlan& Plan)
{
    Plan = FAutoplayPlan();
    TArray<FString> Report;
    FAutoplayPlan Best;
    for (int32 Index = 0; Index < Script.Variants.Num(); ++Index)
    {
        const FTactic& Variant = Script.Variants[Index];
        FAutoplayPlan Candidate;
        FString Why;
        const bool bOk = Simulate(Script, Variant, Party, Identity, Candidate, Why);
        const FString Label = FString::Printf(TEXT("variant %d (bait %d, advance %d, fall back %d, retreat %d)"), Index, Variant.Bait, static_cast<int32>(Variant.bAdvance), static_cast<int32>(Variant.bFallBack), static_cast<int32>(Variant.bRetreat));
        if (!bOk) { Report.Add(Label + TEXT(": dropped, ") + Why); continue; }
        Report.Add(FString::Printf(TEXT("%s: every beat, %d command(s), round %d, %d survivor(s), unit %d downed"), *Label, Candidate.Steps.Num(), Candidate.Rounds, Candidate.Survivors, Candidate.DownedId));
        if (!Best.bComplete || Candidate.Survivors > Best.Survivors || (Candidate.Survivors == Best.Survivors && Candidate.Steps.Num() < Best.Steps.Num()))
        { Best = MoveTemp(Candidate); Best.Variant = Index; }
    }
    Plan = MoveTemp(Best);
    Plan.Report = MoveTemp(Report);
    return Plan.bComplete;
}

// ---------------------------------------------------------------------------------------------------------------
// The live driver.

bool FExpeditionAutoplayDirector::Requested(const TCHAR* CommandLine)
{
    FString Path;
    return CommandLine && FParse::Value(CommandLine, TEXT("ExpeditionAutoplay="), Path) && !Path.IsEmpty();
}

TSharedPtr<FExpeditionAutoplayDirector> FExpeditionAutoplayDirector::TryCreate(AExpeditionPlayerController* Controller, AExpeditionBattleView* View)
{
    FString Path;
    if (!FParse::Value(FCommandLine::Get(), TEXT("ExpeditionAutoplay="), Path) || Path.IsEmpty()) { return nullptr; }
    TSharedPtr<FExpeditionAutoplayDirector> Director = MakeShared<FExpeditionAutoplayDirector>();
    Director->Controller = Controller;
    Director->View = View;
    Director->ScriptPath = Path;
    Director->bResumePass = FParse::Param(FCommandLine::Get(), TEXT("ExpeditionAutoplayResume"));
    Director->StartSeconds = FPlatformTime::Seconds();
    FString Text;
    if (FPaths::IsRelative(Path) || !FFileHelper::LoadFileToString(Text, *Path))
    { Director->ScriptError = FString::Printf(TEXT("Cannot read the autoplay script \"%s\" (it must be an absolute path)"), *Path); }
    else { ParseScript(Text, Director->Script, Director->ScriptError); }
#if CSV_PROFILER
    static bool bHooked = false;
    if (!bHooked) { bHooked = true; FCsvProfiler::Get()->OnCSVProfileFinished().AddStatic(&LogCsvFinished); }
#endif
    return Director;
}

TSharedPtr<FExpeditionAutoplayDirector> FExpeditionAutoplayDirector::Create(AExpeditionPlayerController* Controller, AExpeditionBattleView* View, const FAutoplayScript& Script, bool bResume, bool bExitOnFinish)
{
    TSharedPtr<FExpeditionAutoplayDirector> Director = MakeShared<FExpeditionAutoplayDirector>();
    Director->Controller = Controller;
    Director->View = View;
    Director->Script = Script;
    Director->bResumePass = bResume;
    Director->bExit = bExitOnFinish;
    Director->StartSeconds = FPlatformTime::Seconds();
    return Director;
}

bool FExpeditionAutoplayDirector::Calm() const
{
    const AExpeditionBattleView* Board = View.Get();
    if (!Board || Board->IsPresenting()) { return false; }
    const FExpeditionPlayback& Show = Board->Playback();
    const bool bBanner = !Show.Banner().Title.IsEmpty() && Show.Banner().Age <= Show.Banner().Life;
    return !bBanner && Show.Texts().IsEmpty() && Show.Sparkles().IsEmpty();
}

bool FExpeditionAutoplayDirector::TableShown(int32 WaitedFrames) const
{
    const AExpeditionBattleView* Board = View.Get();
    if (!Board || !Board->HasRun() || Board->Progress().Phase != EExpeditionPhase::Route) { return false; }
    if (Board->ShowingOverview() || !Board->UsesTheme()) { return true; }
    // A theme whose tabletop did not load never shows an overview; do not wait for it forever (the measurement then says so).
    if (WaitedFrames > Script.SettleFrames + 300)
    { UE_LOG(LogExpeditionAutoplay, Warning, TEXT("AUTOPLAY_WARN the themed route table never appeared; continuing without it")); return true; }
    return false;
}

void FExpeditionAutoplayDirector::Enter(EStage To)
{
    Stage = To;
    StageFrames = 0;
    ActFrames = 0;
    bPrepared = false;
    StageStarted = FPlatformTime::Seconds();
}

void FExpeditionAutoplayDirector::Mark(const FString& Kind, const FString& Name, const FString& Edge, const FString& Detail)
{
    const double Elapsed = FPlatformTime::Seconds() - StartSeconds;
    FString Line = FString::Printf(TEXT("AUTOPLAY_%s %s"), *Kind, *Name);
    if (!Edge.IsEmpty()) { Line += TEXT(" ") + Edge; }
    Line += FString::Printf(TEXT(" frame=%llu t=%.3f"), static_cast<uint64>(GFrameCounter), Elapsed);
    if (!Detail.IsEmpty()) { Line += TEXT(" ") + Detail; }
    UE_LOG(LogExpeditionAutoplay, Display, TEXT("%s"), *Line);
#if CSV_PROFILER
    // The same marker rides in the CSV's EVENTS column, so a capture slices without matching frame numbers.
    FString Event = FString::Printf(TEXT("AUTOPLAY_%s:%s"), *Kind, *Name);
    if (!Edge.IsEmpty()) { Event += TEXT(":") + Edge; }
    FCsvProfiler::RecordEvent(CSV_CATEGORY_INDEX_GLOBAL, Event);
#endif
}

void FExpeditionAutoplayDirector::Beat(EBeat Which, const FString& Detail)
{
    bool& bFlag = bSeen[static_cast<int32>(Which)];
    if (bFlag) { return; }
    bFlag = true;
    Mark(TEXT("BEAT"), BeatName(Which), FString(), Detail);
}

void FExpeditionAutoplayDirector::BeginTransition(const FString& Name)
{
    if (!OpenTransition.IsEmpty()) { EndTransition(); }
    OpenTransition = Name;
    OpenFrame = GFrameCounter;
    OpenSeconds = FPlatformTime::Seconds();
    Mark(TEXT("TRANSITION"), Name, TEXT("begin"));
}

void FExpeditionAutoplayDirector::EndTransition()
{
    if (OpenTransition.IsEmpty()) { return; }
    Mark(TEXT("TRANSITION"), OpenTransition, TEXT("end"),
        FString::Printf(TEXT("frames=%llu seconds=%.3f"), static_cast<uint64>(GFrameCounter - OpenFrame), FPlatformTime::Seconds() - OpenSeconds));
    Finished.Add(OpenTransition);
    OpenTransition.Reset();
}

void FExpeditionAutoplayDirector::WatchDowning()
{
    const AExpeditionBattleView* Board = View.Get();
    if (bDowningSeen || !Board || !Board->HasRun() || Board->Progress().Phase != EExpeditionPhase::Combat) { return; }
    for (const FCombatantState& Unit : Board->Rules().Snapshot().Encounter.Combatants)
    {
        if (Unit.Side == ECombatSide::Party && Unit.Life == ECombatLife::Downed)
        {
            bDowningSeen = true;
            Beat(EBeat::Downing, FString::Printf(TEXT("unit=%d name=%s"), Unit.Id, *Unit.Name));
            return;
        }
    }
}

void FExpeditionAutoplayDirector::Fail(const FString& Message)
{
    Finish(false, Message);
}

void FExpeditionAutoplayDirector::Finish(bool bOk, const FString& Message)
{
    if (State != EStatus::Running) { return; }
    State = bOk ? EStatus::Passed : EStatus::Failed;
    if (!bOk)
    {
        Failed = Message;
        UE_LOG(LogExpeditionAutoplay, Error, TEXT("AUTOPLAY_FAIL stage=%s %s"), StageName(static_cast<int32>(Stage)), *Message);
    }
    EndTransition();
    FString Seen;
    for (int32 Index = 0; Index < BeatCount; ++Index)
    { if (bSeen[Index]) { Seen += (Seen.IsEmpty() ? TEXT("") : TEXT(",")) + FString(BeatName(static_cast<EBeat>(Index))); } }
    Mark(TEXT("DONE"), bOk ? TEXT("ok") : TEXT("failed"), FString(), FString::Printf(TEXT("pass=%s beats=%s steps=%d"), bResumePass ? TEXT("resume") : TEXT("fresh"), *Seen, NextStep));
    ExitCode = bOk ? 0 : 1;
    if (!bExit) { Stage = EStage::Done; return; }
    Stage = EStage::Flushing;
    FlushStarted = FPlatformTime::Seconds();
}

void FExpeditionAutoplayDirector::Flush()
{
    const double Waited = FPlatformTime::Seconds() - FlushStarted;
#if CSV_PROFILER
    // Close the profiler capture ourselves: a run shorter than -csvCaptureFrames would otherwise exit with the file unwritten.
    if (FCsvProfiler::IsCapturing())
    {
        if (!bFlushRequested) { FCsvProfiler::Get()->EndCapture(); bFlushRequested = true; }
        if (Waited < 30.0) { return; }
    }
    else if (FCsvProfiler::Get()->IsWritingFile() && Waited < 30.0) { return; }
#endif
    Stage = EStage::Done;
    if (ExitCode) { FPlatformMisc::RequestExitWithStatus(false, static_cast<uint8>(ExitCode)); }
    else { FPlatformMisc::RequestExit(false); }
}

bool FExpeditionAutoplayDirector::Prepare(const FAutoplayStep& Step)
{
    AExpeditionPlayerController* Player = Controller.Get();
    const AExpeditionBattleView* Board = View.Get();
    if (!Player || !Board) { return false; }
    const FCombatCommand& Command = Step.Command;
    switch (Command.Type)
    {
    case ECombatCommandType::EndTurn: return true;
    case ECombatCommandType::Move: Player->PreviewAim(true, Command.Destination); return true;
    case ECombatCommandType::Attack:
    {
        const FCombatantState* Target = Board->Rules().Find(Command.TargetId);
        if (!Target) { return false; }
        Player->PreviewAim(true, Target->Cell);
        return true;
    }
    case ECombatCommandType::Rescue: Player->HandleButton(TEXT("ModeRescue")); return true;
    case ECombatCommandType::Potion: Player->HandleButton(TEXT("ModePotion")); return true;
    default:
        // The dock's two ability buttons: the role button is the first ability, the second button the other.
        if (Command.Type == Player->SpecialAction(0)) { Player->HandleButton(TEXT("ModeRole")); return true; }
        if (Command.Type == Player->SpecialAction(1)) { Player->HandleButton(TEXT("Maneuver")); return true; }
        return false;
    }
}

bool FExpeditionAutoplayDirector::Act(const FAutoplayStep& Step)
{
    AExpeditionPlayerController* Player = Controller.Get();
    AExpeditionBattleView* Board = View.Get();
    if (!Player || !Board) { return false; }
    const FCombatCommand& Command = Step.Command;
    if (Command.Type == ECombatCommandType::EndTurn)
    {
        Player->HandleButton(TEXT("EndTurn"));
        return Board->Progress().Phase != EExpeditionPhase::Combat || Board->Rules().ActiveId() != Command.ActorId;
    }
    FIntPoint Cell = Command.Destination;
    if (Command.Type != ECombatCommandType::Move)
    {
        const FCombatantState* Target = Board->Rules().Find(Command.TargetId);
        if (!Target) { return false; }
        Cell = Target->Cell;
    }
    // The click path: the mode the dock set (or the attack mode), then a click on the cell.
    const bool bAccepted = Player->ActAtCell(Cell);
    Player->PreviewAim(false);
    return bAccepted;
}

void FExpeditionAutoplayDirector::Tick()
{
    if (Stage == EStage::Done) { return; }
    if (Stage == EStage::Flushing) { Flush(); return; }
    if (!ScriptError.IsEmpty()) { Fail(ScriptError); return; }
    if (!Controller.IsValid() || !View.IsValid()) { Fail(TEXT("The battle view or the player controller disappeared")); return; }
    ++Frames;
    ++StageFrames;
    const double Now = FPlatformTime::Seconds();
    if (Stage != EStage::Boot && Stage != EStage::Fight && Now - StageStarted > Script.StageSeconds)
    { Fail(FString::Printf(TEXT("The %s stage took more than %d seconds"), StageName(static_cast<int32>(Stage)), Script.StageSeconds)); return; }
    if (Stage == EStage::Fight && Now - FightStarted > Script.FightSeconds)
    { Fail(FString::Printf(TEXT("The fight took more than %d seconds (step %d of %d)"), Script.FightSeconds, NextStep, Planned.Steps.Num())); return; }
    WatchDowning();
    if (bResumePass) { TickResume(); } else { TickFresh(); }
}

void FExpeditionAutoplayDirector::TickFresh()
{
    AExpeditionBattleView* Board = View.Get();
    AExpeditionPlayerController* Player = Controller.Get();
    switch (Stage)
    {
    case EStage::Boot:
        if (Frames < static_cast<uint64>(Script.BootFrames)) { return; }
        Mark(TEXT("BEGIN"), Script.Name, TEXT("fresh"), FString::Printf(TEXT("seed=%d script=%s"), Script.Seed, *ScriptPath));
        Enter(EStage::Builder);
        return;
    case EStage::Builder:
    {
        if (Board->HasRun()) { Fail(TEXT("A run is already in progress; autoplay needs a clean -ExpeditionSaveRoot")); return; }
        if (!bPartyChosen)
        {
            for (int32 Slot = 0; Slot < 4; ++Slot)
            {
                Player->SelectPartyIndex(Slot, false);
                if (!Player->SelectBuild(Script.Builds[Slot])) { Fail(FString::Printf(TEXT("The build picker refused %s for slot %d"), *Script.Builds[Slot], Slot)); return; }
            }
            const TArray<FPartyBuildSelection>& Chosen = Player->NextParty();
            for (int32 Slot = 0; Slot < 4; ++Slot)
            {
                if (!Chosen.IsValidIndex(Slot) || Chosen[Slot].BuildId != Script.Builds[Slot]) { Fail(FString::Printf(TEXT("Slot %d did not take %s"), Slot, *Script.Builds[Slot])); return; }
            }
            bPartyChosen = true;
            StageFrames = 0;
            return;
        }
        if (!bPlanned)
        {
            // Planned on the start screen, before any transition, and marked so the capture can set the frame aside.
            BeginTransition(TEXT("plan"));
            const bool bOk = BuildPlan(Script, Player->NextParty(), TEXT("autoplay-plan"), Planned);
            EndTransition();
            for (const FString& Line : Planned.Report) { UE_LOG(LogExpeditionAutoplay, Display, TEXT("AUTOPLAY_PLAN %s"), *Line); }
            if (!bOk) { Fail(TEXT("No variant of the script reaches every beat (see the AUTOPLAY_PLAN lines)")); return; }
            UE_LOG(LogExpeditionAutoplay, Display, TEXT("AUTOPLAY_PLAN chosen variant=%d commands=%d survivors=%d downed=%d round=%d"),
                Planned.Variant, Planned.Steps.Num(), Planned.Survivors, Planned.DownedId, Planned.Rounds);
            bPlanned = true;
            StageFrames = 0;
            return;
        }
        if (StageFrames < Script.ThinkFrames) { return; }
        BeginTransition(TEXT("start->route"));
        Player->HandleButton(TEXT("New"));
        if (!Board->HasRun()) { Fail(FString::Printf(TEXT("New run was refused: %s"), Board->Messages().IsEmpty() ? TEXT("no message") : *Board->Messages().Last())); return; }
        if (Board->Progress().Seed != Script.Seed) { Fail(FString::Printf(TEXT("The run started at seed %d, not %d"), Board->Progress().Seed, Script.Seed)); return; }
        Enter(EStage::Starting);
        return;
    }
    case EStage::Starting:
        if (TableShown(StageFrames) && Calm()) { if (++ActFrames >= Script.SettleFrames) { EndTransition(); Enter(EStage::Routing); } }
        else { ActFrames = 0; }
        return;
    case EStage::Routing:
    {
        if (StageFrames < Script.ThinkFrames) { return; }
        int32 Index = INDEX_NONE;
        const TArray<FExpeditionRoute>& Offers = Board->Progress().Offers;
        for (int32 Offer = 0; Offer < Offers.Num() && Offer < 2; ++Offer) { if (Offers[Offer].bDangerous == Script.bDangerous) { Index = Offer; break; } }
        if (Index == INDEX_NONE) { Fail(TEXT("No offered route matches the script's route")); return; }
        BeginTransition(TEXT("route->battle"));
        Player->HandleButton(Index == 0 ? FName(TEXT("Route0")) : FName(TEXT("Route1")));
        if (Board->Progress().Phase != EExpeditionPhase::Combat) { Fail(TEXT("The route was refused")); return; }
        Enter(EStage::Choosing);
        return;
    }
    case EStage::Choosing:
        if (Board->InCombat() && !Board->ShowingOverview()) { ++ActFrames; } else { ActFrames = 0; }
        if (ActFrames < Script.SettleFrames) { return; }
        EndTransition();
        if (!SameModuloIdentity(FExpeditionSnapshot{Board->Progress(), Board->Rules().Snapshot()}, Planned.Opening)) { Fail(TEXT("The live encounter differs from the plan's opening")); return; }
        Enter(EStage::Fight);
        FightStarted = StageStarted;
        return;
    case EStage::Fight:
    {
        const EExpeditionPhase Phase = Board->Progress().Phase;
        if (Phase != EExpeditionPhase::Combat)
        {
            if (Phase != EExpeditionPhase::Reward) { Fail(FString::Printf(TEXT("The fight ended in phase %d, not at the reward"), static_cast<int32>(Phase))); return; }
            Beat(EBeat::Victory);
            BeginTransition(TEXT("battle->reward"));
            Enter(EStage::Victory);
            return;
        }
        const FCombatantState* Active = Board->Rules().Find(Board->Rules().ActiveId());
        if (!Active || Active->Side != ECombatSide::Party || Board->IsPresenting()) { bPrepared = false; ActFrames = 0; return; }
        if (!Planned.Steps.IsValidIndex(NextStep)) { Fail(TEXT("The plan ran out while the fight goes on")); return; }
        const FAutoplayStep& Step = Planned.Steps[NextStep];
        if (!bPrepared)
        {
            if (Active->Id != Step.Command.ActorId || !SameModuloIdentity(FExpeditionSnapshot{Board->Progress(), Board->Rules().Snapshot()}, Step.Before))
            {
                Fail(FString::Printf(TEXT("Step %d: the live game differs from the plan (round %d, unit %d holds the turn, the plan expected unit %d)"),
                    NextStep, Board->Rules().Snapshot().Round, Active->Id, Step.Command.ActorId));
                return;
            }
            if (!Prepare(Step)) { Fail(FString::Printf(TEXT("Step %d: no dock control shows command type %d"), NextStep, static_cast<int32>(Step.Command.Type))); return; }
            bPrepared = true;
            ActFrames = 0;
            return;
        }
        if (++ActFrames < Script.ThinkFrames) { return; }
        if (!Act(Step)) { Fail(FString::Printf(TEXT("Step %d: the game refused command type %d of unit %d"), NextStep, static_cast<int32>(Step.Command.Type), Step.Command.ActorId)); return; }
        UE_LOG(LogExpeditionAutoplay, Display, TEXT("AUTOPLAY_STEP %d/%d type=%d actor=%d target=%d frame=%llu"),
            NextStep + 1, Planned.Steps.Num(), static_cast<int32>(Step.Command.Type), Step.Command.ActorId, Step.Command.TargetId, static_cast<uint64>(GFrameCounter));
        for (const EBeat Met : Step.Beats) { Beat(Met, FString::Printf(TEXT("step=%d type=%d actor=%d target=%d"), NextStep + 1, static_cast<int32>(Step.Command.Type), Step.Command.ActorId, Step.Command.TargetId)); }
        ++NextStep;
        bPrepared = false;
        ActFrames = 0;
        return;
    }
    case EStage::Victory:
        if (Calm()) { ++ActFrames; } else { ActFrames = 0; }
        if (ActFrames < Script.SettleFrames) { return; }
        EndTransition();
        if (!SameModuloIdentity(FExpeditionSnapshot{Board->Progress(), Board->Rules().Snapshot()}, Planned.Final)) { Fail(TEXT("The live victory differs from the plan's final state")); return; }
        Enter(EStage::Leaving);
        return;
    case EStage::Leaving:
    {
        if (StageFrames < Script.ThinkFrames) { return; }
        if (!bPrepared)
        {
            BeginTransition(TEXT("quit"));
            Player->HandleButton(TEXT("Menu"));
            bPrepared = true;
            return;
        }
        if (Board->HasRun()) { return; }
        if (++ActFrames < Script.SettleFrames) { return; }
        EndTransition();
        FString Note;
        FString Root;
        if (FParse::Value(FCommandLine::Get(), TEXT("ExpeditionSaveRoot="), Root) && !Root.IsEmpty())
        {
            const FString Checkpoint = FPaths::Combine(Root, TEXT("current-v1.json"));
            Note = FString::Printf(TEXT("checkpoint=%s bytes=%lld md5=%s"), *Checkpoint, IFileManager::Get().FileSize(*Checkpoint), *LexToString(FMD5Hash::HashFile(*Checkpoint)));
        }
        Beat(EBeat::Quit, Note);
        for (int32 Index = 0; Index < BeatCount; ++Index)
        { if (!bSeen[Index]) { Fail(FString::Printf(TEXT("The beat %s was never reached"), BeatName(static_cast<EBeat>(Index)))); return; } }
        Finish(true, FString());
        return;
    }
    default: return;
    }
}

void FExpeditionAutoplayDirector::TickResume()
{
    AExpeditionBattleView* Board = View.Get();
    AExpeditionPlayerController* Player = Controller.Get();
    switch (Stage)
    {
    case EStage::Boot:
        if (Frames < static_cast<uint64>(Script.BootFrames)) { return; }
        Mark(TEXT("BEGIN"), Script.Name, TEXT("resume"), FString::Printf(TEXT("seed=%d script=%s"), Script.Seed, *ScriptPath));
        BeginTransition(TEXT("resume"));
        Player->HandleButton(TEXT("Resume"));
        if (!Board->HasRun()) { Fail(FString::Printf(TEXT("Resume found no checkpoint: %s"), Board->Messages().IsEmpty() ? TEXT("no message") : *Board->Messages().Last())); return; }
        Enter(EStage::Resuming);
        return;
    case EStage::Resuming:
        if (Calm()) { ++ActFrames; } else { ActFrames = 0; }
        if (ActFrames < Script.SettleFrames) { return; }
        EndTransition();
        if (Board->Progress().Seed != Script.Seed || Board->Progress().Phase != EExpeditionPhase::Reward)
        { Fail(FString::Printf(TEXT("The checkpoint resumed at seed %d in phase %d, not the reward of seed %d"), Board->Progress().Seed, static_cast<int32>(Board->Progress().Phase), Script.Seed)); return; }
        Mark(TEXT("BEAT"), TEXT("resume"));
        Enter(EStage::Claiming);
        return;
    case EStage::Claiming:
        if (StageFrames < Script.ThinkFrames) { return; }
        BeginTransition(TEXT("reward->route"));
        Player->HandleButton(TEXT("Claim"));
        if (Board->Progress().Phase != EExpeditionPhase::Recovery) { Fail(TEXT("The reward was not claimed")); return; }
        Mark(TEXT("BEAT"), TEXT("claim"));
        Enter(EStage::Camping);
        return;
    case EStage::Camping:
        if (!bPrepared)
        {
            if (StageFrames < Script.ThinkFrames) { return; }
            Player->HandleButton(TEXT("Camp"));
            if (Board->Progress().Phase != EExpeditionPhase::Route) { Fail(TEXT("Camp was refused")); return; }
            Mark(TEXT("BEAT"), TEXT("camp"));
            bPrepared = true;
            return;
        }
        if (TableShown(StageFrames) && Calm()) { ++ActFrames; } else { ActFrames = 0; }
        if (ActFrames < Script.SettleFrames) { return; }
        EndTransition();
        Finish(true, FString());
        return;
    default: return;
    }
}
