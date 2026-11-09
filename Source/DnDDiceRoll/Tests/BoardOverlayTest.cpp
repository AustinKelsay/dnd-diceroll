#if WITH_DEV_AUTOMATION_TESTS
#include "ExpeditionInputHarness.h"
#include "Presentation/BoardMetric.h"
#include "Presentation/BoardOverlay.h"
#include "Party/BuildCatalog.h"

/**
 * The contextual grid and footprint rings (#142). The mask the board draws from must equal the authoritative
 * queries behind #50's targeting feedback, themed boards must not place per-cell tiles, the full-grid key must
 * touch presentation only, and the rings must follow the board metric.
 */
namespace BoardOverlayTests
{
using BoardOverlay::EStyle;

bool ReachFirstPartyTurn(ExpeditionInputHarness::FHarness& Game)
{
    Game.Frame();
    Game.Press(EKeys::Enter);
    Game.Settle();
    Game.Press(EKeys::Enter);
    if (!Game.View->InCombat()) { return false; }
    Game.PartyTurn();
    return Game.View->InCombat();
}

bool SameCells(const TArray<FIntPoint>& A, const TArray<FIntPoint>& B)
{
    const TSet<FIntPoint> SetA(A), SetB(B);
    return SetA.Num() == SetB.Num() && SetA.Includes(SetB);
}

int32 Count(const TArray<BoardOverlay::FBar>& Bars, EStyle Style)
{
    int32 Result = 0;
    for (const BoardOverlay::FBar& Bar : Bars) { if (Bar.Style == Style) { ++Result; } }
    return Result;
}

/** The cells the forecast says an action would strike (what the HUD labels AREA). */
TArray<FIntPoint> ForecastVictims(const FCombatBattle& Rules, int32 Actor, ECombatCommandType Action, int32 Hover)
{
    TArray<FIntPoint> Cells;
    for (const FTargetEffect& Effect : Rules.Forecast(Actor, Action, Hover).Effects)
    {
        if (Effect.Effect != EAbilityEffect::Strike) { continue; }
        if (const FCombatantState* Unit = Rules.Find(Effect.TargetId)) { Cells.AddUnique(Unit->Cell); }
    }
    return Cells;
}

void CheckMaskMatchesRules(FAutomationTestBase& Test, const ExpeditionInputHarness::FHarness& Game, const FString& Context)
{
    const FCombatBattle& Rules = Game.View->Rules();
    const int32 Actor = Game.Player->Actor();
    const ECombatCommandType Action = Game.Player->Action();
    const BoardOverlay::FMask& Mask = Game.View->OverlayMask();
    const FActionReach Expected = Rules.ActionReach(Actor, Action);
    Test.TestTrue(Context + TEXT(": reach cells equal the authoritative visible reach"), SameCells(Mask.Cells(BoardOverlay::Reach), Expected.Visible));
    Test.TestTrue(Context + TEXT(": obstructed cells equal the authoritative blocked sight"), SameCells(Mask.Cells(BoardOverlay::Obstructed), Expected.Obstructed));
    const TArray<FIntPoint> Moves = Action == ECombatCommandType::Attack ? Rules.LegalMoves(Actor) : TArray<FIntPoint>();
    Test.TestTrue(Context + TEXT(": move cells equal the legal moves"), SameCells(Mask.Cells(BoardOverlay::Move), Moves));
    int32 HoverTarget = INDEX_NONE;
    FIntPoint Hover;
    if (Game.Player->HoverCell(Hover))
    {
        for (const FCombatantState& Unit : Rules.Snapshot().Encounter.Combatants)
        { if (Unit.Cell == Hover && Game.Player->Targets().Contains(Unit.Id)) { HoverTarget = Unit.Id; } }
    }
    Test.TestTrue(Context + TEXT(": victim cells equal the forecast's strike targets"),
        SameCells(Mask.Cells(BoardOverlay::Victim), ForecastVictims(Rules, Actor, Action, HoverTarget)));
    if (Game.View->DrawsContextualGrid())
    {
        Test.TestTrue(Context + TEXT(": a move region has a contour"), Moves.IsEmpty() || Game.View->OverlayBars(EStyle::MoveContour) > 0);
        Test.TestEqual(Context + TEXT(": one cross (two bars) per blocked-sight cell"), Game.View->OverlayBars(EStyle::Blocked), 2 * Expected.Obstructed.Num());
    }
}

/** What the every-action test saw: which build/key pairs it checked and how many area cases had victims. */
struct FCoverage
{
    TSet<FString> Actions;
    int32 AreaCases = 0;
    int32 AreaCasesWithVictims = 0;
    int32 MostVictims = 0;
};

/** A checkpoint at the first route's encounter for a party of the given builds (slots 1-4, in order). */
bool PartyCheckpoint(const TArray<FString>& Builds, int32 Seed, FExpeditionSaveData& Data)
{
    TArray<FPartyBuildSelection> Party;
    for (int32 Index = 0; Index < Builds.Num(); ++Index)
    {
        FPartyBuildSelection Slot;
        Slot.Id = Index + 1;
        Slot.Name = FString::Printf(TEXT("Hero %d"), Index + 1);
        Slot.BuildId = Builds[Index];
        Party.Add(Slot);
    }
    FString Error;
    FExpeditionRun Run;
    if (!Run.NewRun(Seed, Party, Error, FString::Printf(TEXT("grid-catalog-%d"), Seed))) { return false; }
    if (Run.Progress().Offers.IsEmpty() || !Run.ChooseRoute(Run.Progress().Offers[0].Id, Error)) { return false; }
    Data.Current = Run.Snapshot();
    return true;
}

bool IsAreaAction(const FCombatantState& Hero, ECombatCommandType Action)
{
    for (const FAbilityFacts& Ability : FBuildCatalog::Abilities(Hero))
    { if (Ability.Command == Action && Ability.Effect == EAbilityEffect::Area) { return true; } }
    return false;
}

/**
 * The active hero's turn: close in on the enemies with a real Move (so area actions have targets in reach), then every
 * mode key, each alone and with the cursor on each legal target, must draw exactly what the authoritative queries say.
 * The checks themselves must change no rules state.
 */
void ExerciseHeroTurn(FAutomationTestBase& Test, ExpeditionInputHarness::FHarness& Game, FCoverage& Coverage)
{
    const int32 ActorId = Game.Player->Actor();
    {
        const FCombatBattle& Rules = Game.View->Rules();
        const FCombatantState* Hero = Rules.Find(ActorId);
        if (!Hero) { return; }
        auto Nearest = [&Rules](FIntPoint Cell)
        {
            int32 Best = 1000;
            for (const FCombatantState& Enemy : Rules.Snapshot().Encounter.Combatants)
            { if (Enemy.Side == ECombatSide::Enemy && Enemy.HP > 0) { Best = FMath::Min(Best, ExpeditionTestSupport::Distance(Cell, Enemy.Cell)); } }
            return Best;
        };
        if (Nearest(Hero->Cell) > 2)
        {
            bool bFound = false;
            FIntPoint BestCell = Hero->Cell;
            for (const FIntPoint& Cell : Rules.LegalMoves(ActorId))
            { if (!bFound || Nearest(Cell) < Nearest(BestCell)) { BestCell = Cell; bFound = true; } }
            if (bFound) { Game.View->Execute({ECombatCommandType::Move, ActorId, BestCell}); }
        }
    }
    Game.Settle();
    if (!Game.View->InCombat() || Game.View->Rules().ActiveId() != ActorId) { return; }
    const FCombatantState* Hero = Game.View->Rules().Find(ActorId);
    if (!Hero) { return; }
    const FString BuildId = Hero->BuildId;
    const FExpeditionSnapshot Before = Game.Live();

    for (const FKey& Key : {EKeys::A, EKeys::F, EKeys::M, EKeys::R, EKeys::P})
    {
        Game.Player->PreviewAim(false);
        Game.Press(Key);
        const FString Label = FString::Printf(TEXT("%s, key %s"), *BuildId, *Key.ToString());
        Coverage.Actions.Add(Label);
        CheckMaskMatchesRules(Test, Game, Label);
        const TArray<int32> Targets = Game.Player->Targets();
        for (const int32 Id : Targets)
        {
            const FCombatantState* Unit = Game.View->Rules().Find(Id);
            if (!Unit) { continue; }
            const FIntPoint Cell = Unit->Cell;
            const FString Where = FString::Printf(TEXT("%s, cursor on %s"), *Label, *Unit->Name);
            Game.Player->PreviewAim(true, Cell);
            Game.Press(Key);
            CheckMaskMatchesRules(Test, Game, Where);
            const FCombatantState* Actor = Game.View->Rules().Find(ActorId);
            if (Actor && IsAreaAction(*Actor, Game.Player->Action()))
            {
                const int32 Victims = Game.View->OverlayMask().Cells(BoardOverlay::Victim).Num();
                ++Coverage.AreaCases;
                if (Victims > 0) { ++Coverage.AreaCasesWithVictims; }
                Coverage.MostVictims = FMath::Max(Coverage.MostVictims, Victims);
            }
        }
    }
    Game.Player->PreviewAim(false);
    Game.Press(EKeys::A);
    Test.TestTrue(BuildId + TEXT(": drawing the grid for every action changed no rules state"), ExpeditionTestSupport::SameRun(Before, Game.Live()));
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBoardOverlayMatchesRulesTest, "Project.Expedition.Presentation.ContextualGridMatchesRules",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FBoardOverlayMatchesRulesTest::RunTest(const FString& Parameters)
{
    ExpeditionInputHarness::FHarness Game(*this, TEXT("ContextualGrid"));
    if (!TestTrue(TEXT("Input, board and HUD are ready"), Game.IsReady())) { return false; }
    if (!TestTrue(TEXT("The party has the first turn"), BoardOverlayTests::ReachFirstPartyTurn(Game))) { return false; }

    // Every mode key #50's tests use, so every action type the actor has: reach, blocked sight and moves follow the rules.
    for (const FKey& Key : {EKeys::A, EKeys::F, EKeys::M, EKeys::B, EKeys::J, EKeys::R, EKeys::P, EKeys::A})
    {
        Game.Press(Key);
        BoardOverlayTests::CheckMaskMatchesRules(*this, Game, FString::Printf(TEXT("Action key %s"), *Key.ToString()));
    }

    // Put the keyboard cursor on each legal target of each action: the hover and the forecast's victims are marked.
    for (const FKey& Key : {EKeys::A, EKeys::F, EKeys::M, EKeys::B, EKeys::J, EKeys::R, EKeys::P})
    {
        Game.Player->PreviewAim(false);
        Game.Press(Key);
        const TArray<int32> Targets = Game.Player->Targets();
        for (const int32 Id : Targets)
        {
            const FCombatantState* Unit = Game.View->Rules().Find(Id);
            if (!Unit) { continue; }
            const FIntPoint Cell = Unit->Cell;
            Game.Player->PreviewAim(true, Cell);
            Game.Press(Key);
            BoardOverlayTests::CheckMaskMatchesRules(*this, Game, FString::Printf(TEXT("Cursor on %s under key %s"), *Unit->Name, *Key.ToString()));
            TestTrue(TEXT("The cursor cell is marked as hovered"), Game.View->OverlayMask().Has(Cell, BoardOverlay::Hover));
        }
    }
    Game.Player->PreviewAim(false);
    Game.Press(EKeys::A);

    // Nothing is marked once a playback or an overlay owns the screen.
    Game.Press(EKeys::Slash);
    TestTrue(TEXT("Help opens"), Game.Player->OverlayOpen());
    TestTrue(TEXT("Under Help the board marks nothing"), Game.View->OverlayMask().Cells(static_cast<uint8>(BoardOverlay::Move | BoardOverlay::Reach | BoardOverlay::Obstructed)).IsEmpty());
    Game.Press(EKeys::Escape);
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBoardOverlayEveryActionTest, "Project.Expedition.Presentation.ContextualGridMatchesEveryCatalogAction",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FBoardOverlayEveryActionTest::RunTest(const FString& Parameters)
{
    // Parties chosen so the catalog's effect kinds all appear: area (Wails, Volley, Sculpted Burst, Cinder Wave), strike,
    // push, swap, protect, healing, surge, missiles, maneuvers, rage and the artificer kit. Every party has area users.
    const TArray<TArray<FString>> Parties = {
        {TEXT("fighter.battle-master"), TEXT("rogue.phantom"), TEXT("wizard.evocation"), TEXT("cleric.life")},
        {TEXT("ranger.hunter"), TEXT("monk.mercy"), TEXT("paladin.watchers"), TEXT("sorcerer.wild-magic")},
        {TEXT("barbarian.wild-magic"), TEXT("warlock.fiend"), TEXT("artificer.armorer"), TEXT("monk.four-elements")},
    };
    BoardOverlayTests::FCoverage Coverage;
    for (int32 PartyIndex = 0; PartyIndex < Parties.Num(); ++PartyIndex)
    {
        FExpeditionSaveData Data;
        if (!TestTrue(FString::Printf(TEXT("Party %d starts the first route"), PartyIndex), BoardOverlayTests::PartyCheckpoint(Parties[PartyIndex], 11 + PartyIndex, Data))) { continue; }
        ExpeditionInputHarness::FHarness Game(*this, TEXT("GridEveryAction"), &Data);
        if (!TestTrue(TEXT("Input, board and HUD are ready"), Game.IsReady())) { continue; }
        Game.Frame();
        Game.Press(EKeys::O);
        if (!TestTrue(FString::Printf(TEXT("Party %d resumes in combat"), PartyIndex), Game.View->HasRun() && Game.View->InCombat())) { continue; }
        Game.Settle();
        // Six party turns: every hero acts at least once, and the later turns find enemies closer.
        for (int32 Turn = 0; Turn < 6 && Game.View->InCombat(); ++Turn)
        {
            Game.PartyTurn();
            if (!Game.View->InCombat()) { break; }
            BoardOverlayTests::ExerciseHeroTurn(*this, Game, Coverage);
            if (!Game.View->InCombat()) { break; }
            Game.Press(EKeys::SpaceBar);
        }
    }
    AddInfo(FString::Printf(TEXT("Checked %d build and key pairs; %d area cases, %d with victims, at most %d victims"),
        Coverage.Actions.Num(), Coverage.AreaCases, Coverage.AreaCasesWithVictims, Coverage.MostVictims));
    TestTrue(TEXT("Many build and action pairs were checked"), Coverage.Actions.Num() >= 12);
    TestTrue(TEXT("Some area action was aimed at a legal target"), Coverage.AreaCases > 0);
    TestTrue(TEXT("At least one area case has a nonempty victim set"), Coverage.AreaCasesWithVictims > 0);
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBoardOverlayGeometryTest, "Project.Expedition.Presentation.ContextualGridGeometry",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FBoardOverlayGeometryTest::RunTest(const FString& Parameters)
{
    TArray<BoardOverlay::FBar> Bars;
    FBoardHighlights Highlights;

    // One cell: four contour edges. Two side by side: still four (runs merge). An L of three: six.
    Highlights.Moves = {FIntPoint(1, 1)};
    BoardOverlay::BuildBars(BoardOverlay::BuildMask(5, 5, Highlights, {}), Highlights, false, Bars);
    TestEqual(TEXT("A single cell has a four-sided contour"), BoardOverlayTests::Count(Bars, BoardOverlay::EStyle::MoveContour), 4);
    TestEqual(TEXT("A single cell has two ticks at each of its four corners"), BoardOverlayTests::Count(Bars, BoardOverlay::EStyle::Tick), 8);
    Highlights.Moves = {FIntPoint(1, 1), FIntPoint(2, 1)};
    BoardOverlay::BuildBars(BoardOverlay::BuildMask(5, 5, Highlights, {}), Highlights, false, Bars);
    TestEqual(TEXT("Two neighbours merge into one rectangle contour"), BoardOverlayTests::Count(Bars, BoardOverlay::EStyle::MoveContour), 4);
    Highlights.Moves = {FIntPoint(1, 1), FIntPoint(2, 1), FIntPoint(1, 2)};
    BoardOverlay::BuildBars(BoardOverlay::BuildMask(5, 5, Highlights, {}), Highlights, false, Bars);
    TestEqual(TEXT("An L-shaped region has six contour runs"), BoardOverlayTests::Count(Bars, BoardOverlay::EStyle::MoveContour), 6);
    for (const BoardOverlay::FBar& Bar : Bars)
    {
        if (Bar.Style != BoardOverlay::EStyle::MoveContour) { continue; }
        TestTrue(TEXT("Contours are axis aligned"), FMath::IsNearlyEqual(Bar.A.X, Bar.B.X) || FMath::IsNearlyEqual(Bar.A.Y, Bar.B.Y));
    }

    // A region on the board edge is outlined along the edge too.
    Highlights.Moves = {FIntPoint(0, 0)};
    BoardOverlay::BuildBars(BoardOverlay::BuildMask(3, 3, Highlights, {}), Highlights, false, Bars);
    TestEqual(TEXT("A corner cell is outlined on all four sides"), BoardOverlayTests::Count(Bars, BoardOverlay::EStyle::MoveContour), 4);

    // The path ribbon: cardinal legs, one cost tick per step, a destination bracket.
    Highlights = FBoardHighlights();
    Highlights.bHasOrigin = true;
    Highlights.Origin = FIntPoint(1, 1);
    Highlights.Path = {FIntPoint(2, 1), FIntPoint(2, 2), FIntPoint(3, 2)};
    BoardOverlay::BuildBars(BoardOverlay::BuildMask(6, 6, Highlights, {}), Highlights, false, Bars);
    TestEqual(TEXT("A three-step path has three legs and three cost ticks"), BoardOverlayTests::Count(Bars, BoardOverlay::EStyle::Ribbon), 6);
    TestEqual(TEXT("The destination has four corner brackets (two arms each)"), BoardOverlayTests::Count(Bars, BoardOverlay::EStyle::PathBracket), 8);
    for (const BoardOverlay::FBar& Bar : Bars)
    {
        if (Bar.Style != BoardOverlay::EStyle::Ribbon) { continue; }
        TestTrue(TEXT("Ribbon bars are axis aligned: exact cardinal corners"), FMath::IsNearlyEqual(Bar.A.X, Bar.B.X) || FMath::IsNearlyEqual(Bar.A.Y, Bar.B.Y));
    }

    // Blocked sight, victims, hover and the full grid.
    Highlights = FBoardHighlights();
    Highlights.Reach.Obstructed = {FIntPoint(1, 1), FIntPoint(2, 2)};
    Highlights.bHasHover = true;
    Highlights.Hover = FIntPoint(3, 3);
    const TArray<FIntPoint> Victims = {FIntPoint(0, 3)};
    BoardOverlay::BuildBars(BoardOverlay::BuildMask(5, 4, Highlights, Victims), Highlights, true, Bars);
    TestEqual(TEXT("Each blocked cell has a two-bar cross"), BoardOverlayTests::Count(Bars, BoardOverlay::EStyle::Blocked), 4);
    TestEqual(TEXT("A victim has a four-bar frame"), BoardOverlayTests::Count(Bars, BoardOverlay::EStyle::Victim), 4);
    TestEqual(TEXT("The hover cell has corner brackets"), BoardOverlayTests::Count(Bars, BoardOverlay::EStyle::HoverBracket), 8);
    TestEqual(TEXT("The full grid is one line per cell edge of the board"), BoardOverlayTests::Count(Bars, BoardOverlay::EStyle::Grid), (5 + 1) + (4 + 1));
    BoardOverlay::BuildBars(BoardOverlay::BuildMask(5, 4, Highlights, Victims), Highlights, false, Bars);
    TestEqual(TEXT("Without the key there is no grid"), BoardOverlayTests::Count(Bars, BoardOverlay::EStyle::Grid), 0);
    // Cells off the board are ignored.
    Highlights = FBoardHighlights();
    Highlights.Moves = {FIntPoint(-1, 0), FIntPoint(9, 9)};
    TestTrue(TEXT("Off-board cells leave the mask empty"), BoardOverlay::BuildMask(3, 3, Highlights, {}).Cells(BoardOverlay::Move).IsEmpty());
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FThemedBoardHasNoTilesTest, "Project.Expedition.Presentation.ThemedBoardHasNoCellTiles",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FThemedBoardHasNoTilesTest::RunTest(const FString& Parameters)
{
    ExpeditionInputHarness::FHarness Game(*this, TEXT("ThemedNoTiles"));
    if (!TestTrue(TEXT("Input, board and HUD are ready"), Game.IsReady())) { return false; }
    if (!TestTrue(TEXT("The party has the first turn"), BoardOverlayTests::ReachFirstPartyTurn(Game))) { return false; }
    const FCombatEncounter& Encounter = Game.View->Rules().Snapshot().Encounter;
    if (Game.View->UsesTheme())
    {
        TestEqual(TEXT("A themed board places no per-cell tile meshes"), Game.View->TileCount(), 0);
        TestTrue(TEXT("A themed board draws the contextual grid"), Game.View->DrawsContextualGrid());
    }
    else { AddInfo(TEXT("No theme is loaded here; the themed half of this check was skipped.")); }
    Game.View->UsePrimitives();
    Game.Frame();
    const FCombatEncounter& Primitive = Game.View->Rules().Snapshot().Encounter;
    TestEqual(TEXT("A primitive board keeps one cube tile per cell"), Game.View->TileCount(), Primitive.Width * Primitive.Height);
    TestFalse(TEXT("A primitive board has no contextual grid"), Game.View->DrawsContextualGrid());
    for (const FCombatantState& Unit : Primitive.Combatants)
    { TestEqual(FString::Printf(TEXT("%s has no footprint ring in the primitive look (it keeps its base disc)"), *Unit.Name), Game.View->FootprintRingDiameterCm(Unit.Id), 0.0f); }
    TestTrue(TEXT("The board is the same size either way"), Primitive.Width == Encounter.Width && Primitive.Height == Encounter.Height);
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFullGridKeyTest, "Project.Expedition.Presentation.FullGridKeyIsPresentationOnly",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FFullGridKeyTest::RunTest(const FString& Parameters)
{
    ExpeditionInputHarness::FHarness Game(*this, TEXT("FullGridKey"));
    if (!TestTrue(TEXT("Input, board and HUD are ready"), Game.IsReady())) { return false; }
    if (!TestTrue(TEXT("The party has the first turn"), BoardOverlayTests::ReachFirstPartyTurn(Game))) { return false; }
    const FExpeditionSnapshot Before = Game.Live();
    const TArray<uint8> CheckpointBefore = Game.CheckpointBytes();
    const FCombatEncounter& Encounter = Game.View->Rules().Snapshot().Encounter;
    TestFalse(TEXT("The grid is hidden at rest"), Game.View->ShowingFullGrid());
    TestEqual(TEXT("No grid lines at rest"), Game.View->OverlayBars(BoardOverlay::EStyle::Grid), 0);

    Game.Player->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::LeftAlt, IE_Pressed, 1.0f));
    Game.Frame();
    TestTrue(TEXT("Holding Left Alt shows the full grid"), Game.View->ShowingFullGrid() && Game.Player->ShowsFullGrid());
    if (Game.View->DrawsContextualGrid())
    { TestEqual(TEXT("One line per cell edge across the board"), Game.View->OverlayBars(BoardOverlay::EStyle::Grid), (Encounter.Width + 1) + (Encounter.Height + 1)); }
    TestTrue(TEXT("Holding the key changes no rules state"), ExpeditionTestSupport::SameRun(Before, Game.Live()));
    TestTrue(TEXT("Holding the key writes no checkpoint"), CheckpointBefore == Game.CheckpointBytes());

    // Help owns the screen: the grid hides under it and returns after.
    Game.Press(EKeys::Slash);
    TestFalse(TEXT("The grid hides under Help"), Game.View->ShowingFullGrid());
    Game.Press(EKeys::Escape);
    Game.Frame();
    TestTrue(TEXT("The grid returns while the key is still down"), Game.View->ShowingFullGrid());

    Game.Player->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::LeftAlt, IE_Released, 0.0f));
    Game.Frame();
    TestFalse(TEXT("Releasing Left Alt hides the grid again"), Game.View->ShowingFullGrid());
    TestEqual(TEXT("The grid lines are gone"), Game.View->OverlayBars(BoardOverlay::EStyle::Grid), 0);
    TestTrue(TEXT("The round trip changes no rules state"), ExpeditionTestSupport::SameRun(Before, Game.Live()));
    TestTrue(TEXT("The round trip writes no checkpoint"), CheckpointBefore == Game.CheckpointBytes());
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFootprintRingTest, "Project.Expedition.Presentation.FootprintRingFollowsMetric",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FFootprintRingTest::RunTest(const FString& Parameters)
{
    ExpeditionInputHarness::FHarness Game(*this, TEXT("FootprintRing"));
    if (!TestTrue(TEXT("Input, board and HUD are ready"), Game.IsReady())) { return false; }
    if (!TestTrue(TEXT("The party has the first turn"), BoardOverlayTests::ReachFirstPartyTurn(Game))) { return false; }
    TestEqual(TEXT("The default 150 cm cell gives a 56 cm ring"), FBoardMetric(150.0f, 180.0f).FootprintDiameter(), 56.0f);
    if (!Game.View->UsesTheme())
    {
        // Rings belong to the themed look; the primitive look keeps its opaque base discs.
        AddInfo(TEXT("No theme is loaded here; the footprint rings are themed only, so the per-figure ring check was skipped."));
        return !HasAnyErrors();
    }

    for (const FBoardMetric& Metric : {Game.View->Metric(), FBoardMetric(100.0f, 170.0f), FBoardMetric(220.0f, 185.0f)})
    {
        Game.View->SetBoardMetric(Metric);
        Game.Frame();
        const float Expected = 56.0f * Metric.CellCm / 150.0f;
        int32 Rings = 0;
        for (const FCombatantState& Unit : Game.View->Rules().Snapshot().Encounter.Combatants)
        {
            const float Diameter = Game.View->FootprintRingDiameterCm(Unit.Id);
            TestTrue(FString::Printf(TEXT("%s has a footprint ring at %.0f cm"), *Unit.Name, Metric.CellCm), Diameter > 0.0f);
            TestTrue(FString::Printf(TEXT("%s ring is %.1f cm across at a %.0f cm cell (got %.2f)"), *Unit.Name, Expected, Metric.CellCm, Diameter),
                FMath::IsNearlyEqual(Diameter, Expected, 0.01f));
            ++Rings;
        }
        TestTrue(TEXT("The board has figures to ring"), Rings > 0);
    }
    return !HasAnyErrors();
}
#endif
