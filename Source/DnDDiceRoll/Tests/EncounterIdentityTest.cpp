#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Persistence/ExpeditionSave.h"
#include "Run/ExpeditionRun.h"

#include "ExpeditionTestSupport.h"
using namespace ExpeditionTestSupport;

// Encounter identity and pacing (#188, catalog revision 9): docs/runs/gameplay-depth/11-encounter-identity-contract.md, with the
// tuned numbers recorded in its as-built section and measured in 12-encounter-identity-report.md.
namespace EncounterIdentityTest
{
struct FRole { const TCHAR* Role; const TCHAR* Name; int32 Range; int32 Speed; };

/** The roster each stage and route generates under revision 9, in id order. */
TArray<FRole> ExpectedRoster(int32 Stage, bool bDangerous)
{
    const FRole Raider1{TEXT("Raider"), bDangerous ? TEXT("Raider 1") : TEXT("Scout 1"), 1, 4};
    const FRole Raider2{TEXT("Raider"), bDangerous ? TEXT("Raider 2") : TEXT("Scout 2"), 1, 4};
    const FRole Archer{TEXT("Archer"), Stage == 4 ? TEXT("Vault bow") : TEXT("Archer"), 4, 4};
    const FRole Skirmisher{TEXT("Skirmisher"), TEXT("Skirmisher"), 1, 6};
    const FRole Brute{TEXT("Brute"), TEXT("Raider brute"), 1, 3};
    const FRole Warden{TEXT("Warden"), TEXT("Vault warden"), 1, 4};
    if (Stage < 2) { return bDangerous ? TArray<FRole>{Raider1, Raider2, Archer} : TArray<FRole>{Raider1, Archer}; }
    if (Stage < 4) { return bDangerous ? TArray<FRole>{Brute, Skirmisher, Raider1, Archer} : TArray<FRole>{Skirmisher, Raider1, Archer}; }
    return bDangerous ? TArray<FRole>{Warden, Brute, Raider1, Archer} : TArray<FRole>{Warden, Raider1, Archer};
}

/** The recipes a stage draws under revision 9: 0-2 for the approach, 3-5 for the pressure fights, 6 for the finale. */
TArray<int32> StageRecipes(int32 Stage) { return Stage < 2 ? TArray<int32>{0, 1, 2} : Stage < 4 ? TArray<int32>{3, 4, 5} : TArray<int32>{6}; }

FExpeditionRoute MakeRoute(int32 Stage, bool bDangerous, int32 Recipe, int32 Width, int32 Height)
{
    FExpeditionRoute Route;
    Route.Id = Stage * 2 + (bDangerous ? 2 : 1);
    Route.bDangerous = bDangerous;
    Route.Recipe = Recipe;
    Route.Width = Width;
    Route.Height = Height;
    return Route;
}

/** Plays the scripted player (FinishCombat, first conscious survivor rewarded, camp taken) taking route Choice(stage) to the route phase of Stage. */
bool ReachStage(FExpeditionRun& Run, int32 Seed, int32 Stage, int32 Revision, TFunctionRef<int32(int32)> Choice)
{
    FString Error;
    if (!Run.NewRun(Seed, Error, FString::Printf(TEXT("identity-%d"), Seed), EFighterSubclass::Champion, ERuleset::Compact, Revision)) { return false; }
    for (int32 Guard = 0; Guard < 64; ++Guard)
    {
        const EExpeditionPhase Phase = Run.Progress().Phase;
        if (Phase == EExpeditionPhase::Route && Run.Progress().EncountersCompleted == Stage) { return true; }
        switch (Phase)
        {
        case EExpeditionPhase::Route: if (!Run.ChooseRoute(Run.Progress().Offers[Choice(Run.Progress().EncountersCompleted)].Id, Error)) { return false; } break;
        case EExpeditionPhase::Combat: if (!FinishCombat(Run)) { return false; } break;
        case EExpeditionPhase::Reward:
        {
            int32 Survivor = 1;
            while (Survivor <= 4 && Run.Battle().Find(Survivor)->Life != ECombatLife::Conscious) { ++Survivor; }
            if (Survivor > 4 || !Run.ClaimReward(Survivor, Error)) { return false; }
            break;
        }
        case EExpeditionPhase::Recovery: if (!Run.Recover(true, Error)) { return false; } break;
        default: return false;
        }
    }
    return false;
}

/** Walking reach from From over open, unoccupied cells (other units block). */
TArray<FIntPoint> Reach(const FCombatEncounter& Layout, const FCombatantState& Unit)
{
    TArray<FIntPoint> Queue = {Unit.Cell};
    TSet<FIntPoint> Seen = {Unit.Cell};
    for (int32 I = 0; I < Queue.Num(); ++I)
    {
        for (const FIntPoint Offset : {FIntPoint(1, 0), FIntPoint(-1, 0), FIntPoint(0, 1), FIntPoint(0, -1)})
        {
            const FIntPoint Next = Queue[I] + Offset;
            const bool bOccupied = Layout.Combatants.ContainsByPredicate([&](const FCombatantState& Other) { return Other.Id != Unit.Id && Other.Cell == Next; });
            if (Next.X >= 0 && Next.Y >= 0 && Next.X < Layout.Width && Next.Y < Layout.Height && !bOccupied && !Layout.BlockedCells.Contains(Next) && !Seen.Contains(Next))
            { Seen.Add(Next); Queue.Add(Next); }
        }
    }
    return Queue;
}
}
using namespace EncounterIdentityTest;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEncounterIdentityLayoutsTest, "Project.Expedition.Run.EncounterIdentityLayoutsValidEverywhere",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FEncounterIdentityLayoutsTest::RunTest(const FString& Parameters)
{
    // Every layout at every board size, route and stage, for revision 9 (its stage recipes and rosters) and revision 8 (recipes 0-2 at every
    // stage): spawns unique, open and inside, all open floor connected, every spawn able to reach a cell next to every opponent with the
    // other bodies blocking, the 5-7 cell opening gap (closest hero to closest enemy) and obstacles inside the board.
    int32 Checked = 0;
    for (const int32 Revision : {8, 9})
    {
        for (int32 Stage = 0; Stage < FExpeditionRun::EncounterCount; ++Stage)
        {
            const TArray<int32> Recipes = Revision >= 9 ? StageRecipes(Stage) : TArray<int32>{0, 1, 2};
            for (const int32 Recipe : Recipes)
            {
                for (const bool bDangerous : {false, true})
                {
                    for (int32 Width = 16; Width <= 18; ++Width)
                    {
                        for (int32 Height = 11; Height <= 12; ++Height)
                        {
                            const FString Where = FString::Printf(TEXT("revision %d fight %d recipe %d %s %dx%d"), Revision, Stage + 1, Recipe, bDangerous ? TEXT("dangerous") : TEXT("guarded"), Width, Height);
                            FCombatEncounter Layout;
                            if (!TestTrue(Where + TEXT(": generates"), FExpeditionRun::EncounterLayout(MakeRoute(Stage, bDangerous, Recipe, Width, Height), Stage, ERuleset::Compact, Revision, Layout))) { continue; }
                            ++Checked;
                            TestEqual(Where + TEXT(": board width"), Layout.Width, Width);
                            TestEqual(Where + TEXT(": board height"), Layout.Height, Height);
                            for (const FIntPoint& Cell : Layout.BlockedCells)
                            { TestTrue(Where + TEXT(": obstacle on the board"), Cell.X >= 0 && Cell.Y >= 0 && Cell.X < Width && Cell.Y < Height); }
                            TestTrue(Where + TEXT(": obstacles scale with the board"), Layout.BlockedCells.Num() >= 8);
                            const int32 Enemies = Layout.Combatants.Num() - 4;
                            TestEqual(Where + TEXT(": roster size"), Enemies, Revision >= 9 ? ExpectedRoster(Stage, bDangerous).Num() : (bDangerous ? 3 : 2));
                            TSet<FIntPoint> Spawns;
                            int32 Closest = MAX_int32;
                            for (const FCombatantState& Unit : Layout.Combatants)
                            {
                                TestFalse(Where + TEXT(": unique spawn"), Spawns.Contains(Unit.Cell)); Spawns.Add(Unit.Cell);
                                TestFalse(Where + TEXT(": spawn on open floor"), Layout.BlockedCells.Contains(Unit.Cell));
                                TestTrue(Where + TEXT(": spawn inside"), Unit.Cell.X >= 0 && Unit.Cell.Y >= 0 && Unit.Cell.X < Width && Unit.Cell.Y < Height);
                                const TArray<FIntPoint> Reached = Reach(Layout, Unit);
                                for (const FCombatantState& Target : Layout.Combatants)
                                {
                                    if (Target.Side == Unit.Side) { continue; }
                                    if (Unit.Side == ECombatSide::Party) { Closest = FMath::Min(Closest, Distance(Unit.Cell, Target.Cell)); }
                                    TestTrue(Where + TEXT(": every spawn reaches a cell next to every opponent"), Reached.ContainsByPredicate([&](FIntPoint Cell) { return Distance(Cell, Target.Cell) == 1; }));
                                }
                            }
                            TestTrue(FString::Printf(TEXT("%s: opening gap 5..7 (got %d)"), *Where, Closest), Closest >= 5 && Closest <= 7);
                            TArray<FIntPoint> Floor;
                            for (int32 Y = 0; Y < Height && Floor.IsEmpty(); ++Y) { for (int32 X = 0; X < Width && Floor.IsEmpty(); ++X) { if (!Layout.BlockedCells.Contains(FIntPoint(X, Y))) { Floor.Add({X, Y}); } } }
                            TSet<FIntPoint> Seen(Floor);
                            for (int32 I = 0; I < Floor.Num(); ++I)
                            {
                                for (const FIntPoint Offset : {FIntPoint(1, 0), FIntPoint(-1, 0), FIntPoint(0, 1), FIntPoint(0, -1)})
                                {
                                    const FIntPoint Next = Floor[I] + Offset;
                                    if (Next.X >= 0 && Next.Y >= 0 && Next.X < Width && Next.Y < Height && !Layout.BlockedCells.Contains(Next) && !Seen.Contains(Next)) { Seen.Add(Next); Floor.Add(Next); }
                                }
                            }
                            TestEqual(Where + TEXT(": all open floor connected"), Floor.Num(), Width * Height - Layout.BlockedCells.Num());
                            if (Recipe == 6)
                            {
                                // Front to back: the party along the front row, the guardians at the back by the Waystone.
                                const int32 C = Width / 2;
                                for (int32 I = 0; I < 4; ++I) { TestEqual(Where + TEXT(": the party enters at the front"), Layout.Combatants[I].Cell, FIntPoint(C - 2 + I, 1)); }
                                for (int32 I = 4; I < Layout.Combatants.Num(); ++I) { TestTrue(Where + TEXT(": the guardians wait at the back"), Layout.Combatants[I].Cell.Y >= 6); }
                                TestEqual(Where + TEXT(": the warden stands at the back of the aisle"), Layout.Combatants[4].Cell, FIntPoint(C, 8));
                            }
                        }
                    }
                }
            }
        }
    }
    TestEqual(TEXT("Every revision 9 layout (approach 3 x 2 fights, pressure 3 x 2, finale 1) and revision 8 layout (3 x 5) at 6 sizes and 2 routes"), Checked, (3 * 2 + 3 * 2 + 1 + 3 * 5) * 6 * 2);
    // A stage only uses its own layouts, and revision 8 only the first three; anything else is refused.
    FCombatEncounter Unused;
    for (int32 Stage = 0; Stage < FExpeditionRun::EncounterCount; ++Stage)
    {
        for (int32 Recipe = -1; Recipe <= 7; ++Recipe)
        {
            const bool bRevision9 = StageRecipes(Stage).Contains(Recipe);
            TestEqual(FString::Printf(TEXT("Revision 9 fight %d recipe %d"), Stage + 1, Recipe), FExpeditionRun::EncounterLayout(MakeRoute(Stage, false, Recipe, 16, 11), Stage, ERuleset::Compact, 9, Unused), bRevision9);
            TestEqual(FString::Printf(TEXT("Revision 8 fight %d recipe %d"), Stage + 1, Recipe), FExpeditionRun::EncounterLayout(MakeRoute(Stage, false, Recipe, 16, 11), Stage, ERuleset::Compact, 8, Unused), Recipe >= 0 && Recipe <= 2);
            TestEqual(FString::Printf(TEXT("SRD revision 9 fight %d recipe %d keeps revision 7 generation"), Stage + 1, Recipe), FExpeditionRun::EncounterLayout(MakeRoute(Stage, false, Recipe, 16, 11), Stage, ERuleset::SRD, 9, Unused), Recipe >= 0 && Recipe <= 2);
        }
    }
    for (const int32 Revision : {6, 10}) { TestFalse(FString::Printf(TEXT("Revision %d generates nothing"), Revision), FExpeditionRun::EncounterLayout(MakeRoute(0, false, 0, 16, 11), 0, ERuleset::Compact, Revision, Unused)); }
    TestFalse(TEXT("Fight 6 does not exist"), FExpeditionRun::EncounterLayout(MakeRoute(5, false, 6, 16, 11), 5, ERuleset::Compact, 9, Unused));
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEncounterIdentityRolesTest, "Project.Expedition.Run.EncounterIdentityRolesAndStats",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FEncounterIdentityRolesTest::RunTest(const FString& Parameters)
{
    // Revision 9 rosters, names, saved roles, ranges and speeds by stage and route, and the stats as built (contract 11 section 6): the
    // revision 8 base for the route and stage (HP 20/24 + 4s, armour 12/13, attack min(5, 4 + s/2), die d6/d8, damage bonus 3 + s/3), with
    // 75% of that HP in fights 3-4 and 90% in fight 5 and a damage bonus of 3 there; then the role: skirmisher 60% HP, +1 armour, d4,
    // speed 6; brute 110% HP, -1 armour, d10, speed 3; warden 175% HP, +1 armour, +2 damage.
    struct FTuning { int32 HpPercent, Armor, Die, Damage; };
    auto Tuning = [](const FString& Role, int32 BaseDie) -> FTuning
    {
        if (Role == TEXT("Skirmisher")) { return {60, 1, 4, 0}; }
        if (Role == TEXT("Brute")) { return {110, -1, 10, 0}; }
        if (Role == TEXT("Warden")) { return {175, 1, BaseDie, 2}; }
        return {100, 0, BaseDie, 0};
    };
    for (int32 Stage = 0; Stage < FExpeditionRun::EncounterCount; ++Stage)
    {
        for (const bool bDangerous : {false, true})
        {
            const FString Where = FString::Printf(TEXT("fight %d %s"), Stage + 1, bDangerous ? TEXT("dangerous") : TEXT("guarded"));
            FCombatEncounter Layout;
            if (!TestTrue(Where + TEXT(": generates"), FExpeditionRun::EncounterLayout(MakeRoute(Stage, bDangerous, StageRecipes(Stage)[0], 17, 12), Stage, ERuleset::Compact, 9, Layout))) { continue; }
            const TArray<FRole> Roster = ExpectedRoster(Stage, bDangerous);
            if (!TestEqual(Where + TEXT(": roster size"), Layout.Combatants.Num() - 4, Roster.Num())) { continue; }
            const int32 Revision8HP = (bDangerous ? 24 : 20) + 4 * Stage, BaseArmor = bDangerous ? 13 : 12, BaseDie = bDangerous ? 8 : 6;
            const int32 BaseHP = Stage < 2 ? Revision8HP : Revision8HP * (Stage == 4 ? 90 : 75) / 100;
            const int32 BaseDamage = Stage < 2 ? 3 + Stage / 3 : 3;
            for (int32 I = 0; I < Roster.Num(); ++I)
            {
                const FCombatantState& U = Layout.Combatants[4 + I];
                const FTuning T = Tuning(Roster[I].Role, BaseDie);
                TestEqual(Where + TEXT(": id"), U.Id, 20 + I);
                TestEqual(Where + TEXT(": saved role"), U.Role, FString(Roster[I].Role));
                TestEqual(Where + TEXT(": name"), U.Name, FString(Roster[I].Name));
                TestEqual(Where + TEXT(": range"), U.Range, Roster[I].Range);
                TestEqual(Where + TEXT(": speed"), U.Speed, Roster[I].Speed);
                TestEqual(Where + TEXT(": HP"), U.MaxHP, BaseHP * T.HpPercent / 100);
                TestEqual(Where + TEXT(": HP full"), U.HP, U.MaxHP);
                TestEqual(Where + TEXT(": armour"), U.ArmorClass, BaseArmor + T.Armor);
                TestEqual(Where + TEXT(": attack"), U.AttackBonus, FMath::Min(5, 4 + Stage / 2));
                TestEqual(Where + TEXT(": damage die"), U.DamageDie, T.Die);
                TestEqual(Where + TEXT(": damage bonus"), U.DamageBonus, BaseDamage + T.Damage);
            }
        }
    }
    // Revision 8 and SRD revision 9 keep the raiders and the "Vault bow" archer of their own revision at every stage.
    for (const ERuleset Ruleset : {ERuleset::Compact, ERuleset::SRD})
    {
        const int32 Revision = Ruleset == ERuleset::Compact ? 8 : 9;
        for (int32 Stage = 0; Stage < FExpeditionRun::EncounterCount; ++Stage)
        {
            FCombatEncounter Layout;
            if (!TestTrue(TEXT("Older generation"), FExpeditionRun::EncounterLayout(MakeRoute(Stage, true, 2, 16, 11), Stage, Ruleset, Revision, Layout))) { continue; }
            TestEqual(TEXT("Three enemies"), Layout.Combatants.Num(), 7);
            for (int32 I = 4; I < Layout.Combatants.Num(); ++I)
            {
                const FCombatantState& U = Layout.Combatants[I];
                const bool bArcher = I == Layout.Combatants.Num() - 1;
                TestEqual(TEXT("Older role"), U.Role, FString(bArcher ? TEXT("Archer") : TEXT("Raider")));
                TestEqual(TEXT("Older name"), U.Name, bArcher ? FString(TEXT("Vault bow")) : FString::Printf(TEXT("Raider %d"), I - 3));
                TestEqual(TEXT("Older speed"), U.Speed, 4);
                TestEqual(TEXT("Older HP"), U.MaxHP, Ruleset == ERuleset::Compact ? 24 + 4 * Stage : 12 + 2 * Stage);
            }
        }
    }
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEncounterIdentityPreviewTest, "Project.Expedition.Run.EncounterIdentityPreviewsAreTruthful",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FEncounterIdentityPreviewTest::RunTest(const FString& Parameters)
{
    // Through the public run: at every stage both offers carry the route tag and four facts (roster, terrain and note, risk, rewards); the roster
    // and terrain match what choosing the route generates and FExpeditionRun::Encounter reports; the risk is the fixed table; queries never roll.
    const TCHAR* Risk[2][3] = {{TEXT("low"), TEXT("moderate"), TEXT("high")}, {TEXT("high"), TEXT("severe"), TEXT("severe")}};
    for (const int32 Pattern : {0, 31, 0b10101, 0b01010})
    {
        auto Choice = [Pattern](int32 Stage) { return (Pattern >> Stage) & 1; };
        for (int32 Stage = 0; Stage < FExpeditionRun::EncounterCount; ++Stage)
        {
            const int32 Seed = FirstSeedWhere([&](int32 Candidate) { FExpeditionRun Probe; return ReachStage(Probe, Candidate, Stage, 9, Choice); }, 42, 200);
            if (!TestTrue(FString::Printf(TEXT("A seed reaches fight %d on route pattern %d"), Stage + 1, Pattern), Seed != INDEX_NONE)) { continue; }
            FExpeditionRun Run;
            ReachStage(Run, Seed, Stage, 9, Choice);
            const FExpeditionSnapshot BeforeQueries = Run.Snapshot();
            for (int32 Offer = 0; Offer < 2; ++Offer)
            {
                const FExpeditionRoute Route = Run.Progress().Offers[Offer];
                const FString Where = FString::Printf(TEXT("seed %d fight %d offer %d"), Seed, Stage + 1, Offer);
                const FEncounterIdentity Identity = FExpeditionRun::Encounter(Run.Progress(), Route);
                TestTrue(Where + TEXT(": the encounter query never rolls"), SameRun(BeforeQueries, Run.Snapshot()));
                TArray<FString> Facts;
                Route.Preview.ParseIntoArray(Facts, TEXT(" | "));
                if (!TestEqual(Where + TEXT(": the route tag and four facts: ") + Route.Preview, Facts.Num(), 5)) { continue; }
                TestEqual(Where + TEXT(": route tag"), Facts[0], FString(Route.bDangerous ? TEXT("Dangerous") : TEXT("Guarded")));
                const TArray<FRole> Roster = ExpectedRoster(Stage, Route.bDangerous);
                TArray<FString> Expected;
                for (const FRole& Role : Roster) { Expected.Add(FString(Role.Role) == TEXT("Raider") && !Route.bDangerous ? FString(TEXT("scout")) : FString(Role.Role).ToLower()); }
                TestEqual(Where + TEXT(": roster line"), Facts[1], FString::Printf(TEXT("%d foes: %s"), Roster.Num(), *FString::Join(Expected, TEXT(", "))));
                TestEqual(Where + TEXT(": enemy count"), Route.EnemyCount, Roster.Num());
                TestTrue(Where + TEXT(": terrain line names the layout and a note"), Facts[2].StartsWith(Identity.Layout + TEXT(": ")) && Facts[2].Len() > Identity.Layout.Len() + 2);
                const int32 Kind = Stage < 2 ? 0 : Stage < 4 ? 1 : 2;
                TestEqual(Where + TEXT(": risk line"), Facts[3], FString(TEXT("Risk: ")) + Risk[Route.bDangerous ? 1 : 0][Kind]);
                if (Stage == 4) { TestEqual(Where + TEXT(": finale rewards"), Facts[4], FString(TEXT("Recover the artifact; no loot"))); }
                else
                {
                    const bool bSupplies = !Run.Progress().History.IsEmpty() && Run.Progress().History.Last().Route.bDangerous;
                    TestEqual(Where + TEXT(": rewards line"), Facts[4], FString::Printf(TEXT("+1 gear, +%d potion%s%s"), Route.RewardPotions, Route.RewardPotions == 1 ? TEXT("") : TEXT("s"),
                        !Route.bDangerous && bSupplies ? TEXT(" (secured supplies)") : TEXT("")));
                }
                TestTrue(Where + TEXT(": the identity's stage"), static_cast<int32>(Identity.Stage) == Kind);
                TestEqual(Where + TEXT(": front to back only in the finale"), Identity.bFrontToBack, Stage == 4);
                TestTrue(Where + TEXT(": the stage's own layouts"), StageRecipes(Stage).Contains(Route.Recipe));
                // Choosing the route generates exactly the previewed roster on the previewed layout.
                FExpeditionRun Chosen = Run;
                FString Error;
                if (!TestTrue(Where + TEXT(": the route starts its fight"), Chosen.ChooseRoute(Route.Id, Error))) { continue; }
                const FCombatEncounter& Board = Chosen.Battle().Snapshot().Encounter;
                FCombatEncounter Layout;
                FExpeditionRun::EncounterLayout(Route, Stage, ERuleset::Compact, 9, Layout);
                TestTrue(Where + TEXT(": the previewed layout's obstacles"), Board.BlockedCells == Layout.BlockedCells);
                int32 Index = 0;
                for (const FCombatantState& U : Board.Combatants)
                {
                    if (U.Side != ECombatSide::Enemy) { continue; }
                    TestTrue(Where + TEXT(": the previewed roles in order"), Roster.IsValidIndex(Index) && U.Role == Roster[Index].Role);
                    ++Index;
                }
                TestEqual(Where + TEXT(": as many enemies as previewed"), Index, Roster.Num());
            }
        }
    }
    // Revision 8 runs keep the revision 8 preview wording, recipes and archer name; the identity of their finale is the Vault by index.
    FExpeditionRun Old; FString Error;
    TestTrue(TEXT("Revision 8 run"), Old.NewRun(42, Error, TEXT("identity-8"), EFighterSubclass::Champion, ERuleset::Compact, 8));
    TestEqual(TEXT("Revision 8 Guarded preview"), Old.Progress().Offers[0].Preview.Left(17), FString(TEXT("Guarded | 2 scout")));
    TestTrue(TEXT("Revision 8 Dangerous preview"), Old.Progress().Offers[1].Preview.StartsWith(TEXT("Dangerous | 3 raiders incl. archer | ")) && Old.Progress().Offers[1].Preview.EndsWith(TEXT(" | +1 gear (rank 3 cap) | +2 potions")));
    FExpeditionRoute Finale = Old.Progress().Offers[0]; Finale.Id = 9;
    const FEncounterIdentity OldFinale = FExpeditionRun::Encounter(Old.Progress(), Finale);
    TestTrue(TEXT("Revision 8 finale is the Finale stage, left to right"), OldFinale.Stage == EEncounterStage::Finale && !OldFinale.bFrontToBack && OldFinale.ObstacleKind == Finale.Recipe);
    // The identity query by recipe: obstacle kinds and names.
    const TCHAR* Names[] = {TEXT("split stones"), TEXT("ruined pillars"), TEXT("broken wall"), TEXT("fallen colonnade"), TEXT("sunken causeway"), TEXT("cairn field"), TEXT("vault sanctum")};
    const int32 Kinds[] = {0, 1, 2, 1, 2, 0, 1};
    FExpeditionRun Current; Current.NewRun(42, Error, TEXT("identity-9"));
    for (int32 Recipe = 0; Recipe < 7; ++Recipe)
    {
        FExpeditionRoute Route = Current.Progress().Offers[0]; Route.Recipe = Recipe; Route.Id = Recipe < 3 ? 1 : Recipe < 6 ? 5 : 9;
        const FEncounterIdentity Identity = FExpeditionRun::Encounter(Current.Progress(), Route);
        TestEqual(FString::Printf(TEXT("Recipe %d name"), Recipe), Identity.Layout, FString(Names[Recipe]));
        TestEqual(FString::Printf(TEXT("Recipe %d obstacle kind"), Recipe), Identity.ObstacleKind, Kinds[Recipe]);
        TestEqual(FString::Printf(TEXT("Recipe %d front to back"), Recipe), Identity.bFrontToBack, Recipe == 6);
    }
    FExpeditionRoute Unknown = Current.Progress().Offers[0]; Unknown.Recipe = 7;
    TestTrue(TEXT("An unknown recipe has no layout name"), FExpeditionRun::Encounter(Current.Progress(), Unknown).Layout.IsEmpty());
    Unknown.Recipe = 3;
    TestTrue(TEXT("Revision 8 has no recipe 3"), FExpeditionRun::Encounter(Old.Progress(), Unknown).Layout.IsEmpty());
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEncounterIdentityRevisionTest, "Project.Expedition.Persistence.OlderRevisionsResumeWithTheirOwnEnemies",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FEncounterIdentityRevisionTest::RunTest(const FString& Parameters)
{
    // Checkpoints of revision 8 and revision 9 runs, at a route choice, mid-battle and between battles in the pressure stage, resume through the
    // session exactly and continue exactly as the uninterrupted run, keeping their revision: a revision 8 run keeps recipes 0-2, raiders and the
    // Vault bow at fights 3-5. Tampered revisions, recipes, roles and speeds are refused and change nothing. A revision outside 7..9 is refused
    // by NewRun.
    const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation/EncounterIdentity"), FGuid::NewGuid().ToString());
    auto Pattern = [](int32 Stage) { return Stage % 2; };
    for (const int32 Revision : {8, 9})
    {
        const int32 Seed = FirstSeedWhere([&](int32 Candidate) { FExpeditionRun Probe; return ReachStage(Probe, Candidate, 3, Revision, Pattern); }, 42, 200);
        if (!TestTrue(FString::Printf(TEXT("Revision %d: a seed reaches fight 4"), Revision), Seed != INDEX_NONE)) { continue; }
        FExpeditionRun AtRoute;
        ReachStage(AtRoute, Seed, 2, Revision, Pattern);
        FString Error;
        FExpeditionRun MidBattle = AtRoute;
        TestTrue(TEXT("Fight 3 starts"), MidBattle.ChooseRoute(MidBattle.Progress().Offers[1].Id, Error));
        while (MidBattle.Progress().Phase == EExpeditionPhase::Combat && MidBattle.Battle().Find(MidBattle.Battle().ActiveId())->Side == ECombatSide::Enemy) { MidBattle.PlayEnemyTurn(); }
        FExpeditionRun Between;
        ReachStage(Between, Seed, 3, Revision, Pattern);
        for (const FCombatantState& U : MidBattle.Battle().Snapshot().Encounter.Combatants)
        {
            if (U.Side != ECombatSide::Enemy) { continue; }
            TestTrue(FString::Printf(TEXT("Revision %d fight 3 roles"), Revision), Revision == 8 ? (U.Role == TEXT("Raider") || U.Role == TEXT("Archer")) && U.Speed == 4 : true);
        }
        TestTrue(FString::Printf(TEXT("Revision %d fight 3 recipe"), Revision), Revision == 8 ? MidBattle.Progress().History.Last().Route.Recipe <= 2 : MidBattle.Progress().History.Last().Route.Recipe >= 3);
        int32 Index = 0;
        for (FExpeditionRun* Original : {&AtRoute, &MidBattle, &Between})
        {
            const FString Where = FString::Printf(TEXT("revision %d checkpoint %d"), Revision, Index);
            const FString Path = FPaths::Combine(Directory, FString::Printf(TEXT("%d-%d"), Revision, Index++), TEXT("current-v1.json"));
            FExpeditionSaveData Data; Data.Current = Original->Snapshot();
            if (!TestTrue(Where + TEXT(": written ") + Error, FExpeditionSave::Write(Path, Data, Error))) { continue; }
            FExpeditionSession Session(Path);
            if (!TestTrue(Where + TEXT(": resumes ") + Error, Session.Resume(Error))) { continue; }
            TestEqual(Where + TEXT(": keeps its revision"), Session.Run().Progress().CatalogRevision, Revision);
            TestTrue(Where + TEXT(": resumed exactly, RNG included"), SameRun(Data.Current, Session.Run().Snapshot()));
            FExpeditionRun Resumed = Session.Run();
            FExpeditionRun Uninterrupted = *Original;
            FinishRunScripted(Uninterrupted); FinishRunScripted(Resumed);
            TestTrue(Where + TEXT(": continues exactly as the uninterrupted run"), SameRun(Uninterrupted.Snapshot(), Resumed.Snapshot()));
            TestEqual(Where + TEXT(": still its revision at the end"), Resumed.Progress().CatalogRevision, Revision);
            for (const FExpeditionEncounterRecord& Record : Resumed.Progress().History)
            {
                const int32 Stage = (Record.Route.Id - 1) / 2;
                TestTrue(Where + TEXT(": every fought layout is its revision's"), Revision == 8 ? Record.Route.Recipe <= 2 : StageRecipes(Stage).Contains(Record.Route.Recipe));
            }
        }
        // Tampering: a mid-battle pressure checkpoint claiming another revision, layout, role or speed is refused and leaves the run untouched.
        const FExpeditionSnapshot Saved = MidBattle.Snapshot();
        FExpeditionRun Target = MidBattle;
        auto Refused = [&](const TCHAR* What, TFunctionRef<void(FExpeditionSnapshot&)> Change)
        {
            FExpeditionSnapshot Bad = Saved; Change(Bad);
            FString Why;
            TestFalse(FString::Printf(TEXT("Revision %d: %s is refused"), Revision, What), Target.Restore(Bad, Why));
            TestTrue(FString::Printf(TEXT("Revision %d: refusing %s changes nothing"), Revision, What), SameRun(Saved, Target.Snapshot()));
        };
        Refused(TEXT("the other revision"), [Revision](FExpeditionSnapshot& S) { S.Progress.CatalogRevision = Revision == 8 ? 9 : 8; });
        Refused(TEXT("another stage's layout"), [Revision](FExpeditionSnapshot& S) { S.Progress.History.Last().Route.Recipe = Revision == 8 ? 3 : 0; });
        Refused(TEXT("a changed role"), [](FExpeditionSnapshot& S) { S.Battle.Encounter.Combatants[4].Role = TEXT("Warden"); });
        Refused(TEXT("a changed speed"), [](FExpeditionSnapshot& S) { S.Battle.Encounter.Combatants[4].Speed = 5; });
        Refused(TEXT("a changed preview"), [](FExpeditionSnapshot& S) { S.Progress.History.Last().Route.Preview += TEXT(" | Risk: low"); });
    }
    IFileManager::Get().DeleteDirectory(*Directory, false, true);
    // NewRun refuses an unsupported revision by name and keeps the installed run.
    FExpeditionRun Run; FString Error;
    TestTrue(TEXT("A current run"), Run.NewRun(42, Error, TEXT("revision-refusal")));
    TestTrue(TEXT("Finish it"), FinishRunScripted(Run) || Run.Progress().Phase == EExpeditionPhase::Defeat);
    const FExpeditionSnapshot Ended = Run.Snapshot();
    for (const int32 Revision : {6, 10})
    {
        TestFalse(FString::Printf(TEXT("NewRun refuses revision %d"), Revision), Run.NewRun(42, Error, TEXT("revision-refusal"), EFighterSubclass::Champion, ERuleset::Compact, Revision));
        TestTrue(TEXT("The refusal names the revision: ") + Error, Error.Contains(FString::Printf(TEXT("revision %d"), Revision)));
        TestTrue(TEXT("The refusal changes nothing"), SameRun(Ended, Run.Snapshot()));
    }
    for (const int32 Revision : {7, 8, 9})
    {
        FExpeditionRun Older;
        TestTrue(FString::Printf(TEXT("NewRun accepts revision %d"), Revision), Older.NewRun(42, Error, TEXT("revision-accepted"), EFighterSubclass::Champion, ERuleset::Compact, Revision));
        TestEqual(TEXT("and records it"), Older.Progress().CatalogRevision, Revision);
    }
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCompactRevision9FingerprintTest, "Project.Expedition.Run.CompactRevision9SeedFingerprint",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FCompactRevision9FingerprintTest::RunTest(const FString& Parameters)
{
    // Seed 42 under revision 9 with the scripted player (first offer, reward to the first survivor, camp): fights 1-2 keep revision 8's boards
    // and seeds (the same random numbers in the same order), and the whole run is pinned to a golden captured on this build. Twin runs match.
    FExpeditionRun Run, Old, Twin; FString Error;
    TestTrue(TEXT("Revision 9"), Run.NewRun(42, Error, TEXT("seeded-fixture")));
    TestTrue(TEXT("Revision 8"), Old.NewRun(42, Error, TEXT("seeded-fixture"), EFighterSubclass::Champion, ERuleset::Compact, 8));
    TestTrue(TEXT("Twin"), Twin.NewRun(42, Error, TEXT("seeded-fixture")));
    for (int32 Offer = 0; Offer < 2; ++Offer)
    {
        const FExpeditionRoute& A = Run.Progress().Offers[Offer];
        const FExpeditionRoute& B = Old.Progress().Offers[Offer];
        TestTrue(TEXT("Fight 1 offers keep their seed, layout and board"), A.EncounterSeed == B.EncounterSeed && A.Recipe == B.Recipe && A.Width == B.Width && A.Height == B.Height);
    }
    TestEqual(TEXT("The generation stream is the same"), Run.Progress().GenerationRandomState, Old.Progress().GenerationRandomState);
    FinishRunScripted(Run); FinishRunScripted(Twin);
    TestTrue(TEXT("Twin runs are identical"), SameRun(Run.Snapshot(), Twin.Snapshot()));
    int32 PartyHp = 0, Alive = 0;
    for (const FCombatantState& U : Run.Battle().Snapshot().Encounter.Combatants) { if (U.Side == ECombatSide::Party) { PartyHp += U.HP; Alive += U.Life == ECombatLife::Conscious ? 1 : 0; } }
    const FString Fingerprint = FString::Printf(TEXT("rng=%d|gen=%d|partyHp=%d|alive=%d|phase=%d|fights=%d|rounds=%d|turn=%d"), Run.Battle().Snapshot().RandomState,
        Run.Progress().GenerationRandomState, PartyHp, Alive, static_cast<int32>(Run.Progress().Phase), Run.Progress().History.Num(), Run.Battle().Snapshot().Round, Run.Battle().Snapshot().TurnIndex);
    AddInfo(TEXT("Revision 9 seed 42 fingerprint: ") + Fingerprint);
    // Captured on the #188 head: the scripted party wins fights 1-4 and falls in the vault sanctum (phase 5, Defeat) in round 8.
    TestEqual(TEXT("Revision 9 seed 42 golden"), Fingerprint, FString(TEXT("rng=-1266537499|gen=-1390404750|partyHp=0|alive=0|phase=5|fights=5|rounds=8|turn=5")));
    return !HasAnyErrors();
}
#endif
