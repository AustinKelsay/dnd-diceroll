#if WITH_DEV_AUTOMATION_TESTS
#include "ExpeditionInputHarness.h"
#include "Presentation/CameraRig.h"
#include "Presentation/TargetChipLayout.h"
#include "Party/BuildCatalog.h"
#include "Run/ExpeditionRun.h"

/**
 * #190: the d20 roll card must not cover the attacker, the target or their footprints. The camera frames the board inside
 * ExpeditionLayout::BoardBounds, so the card belongs in the screen space outside it; the test projects the two combatants'
 * footprints and bodies through the real camera and compares them with the card's rectangle.
 */
namespace RollCardTest
{
using ExpeditionInputHarness::FHarness;

/** The first party turn of the first encounter, in the real input path. */
bool FirstPartyTurn(FHarness& Game)
{
    Game.Frame();
    Game.Press(EKeys::Enter);
    Game.Settle();
    Game.Press(EKeys::Enter);
    if (!Game.View->InCombat()) { return false; }
    Game.PartyTurn();
    return Game.View->InCombat();
}

/** Plays turns, closing in on the enemy as needed, until a hero's attack starts; returns who attacked whom. */
bool StartAttack(FHarness& Game, int32& AttackerId, int32& TargetId)
{
    for (int32 Turn = 0; Turn < 30 && Game.View->InCombat(); ++Turn)
    {
        Game.PartyTurn();
        if (!Game.View->InCombat()) { return false; }
        const FCombatBattle& Rules = Game.View->Rules();
        const int32 Id = Rules.ActiveId();
        TArray<int32> Targets = Rules.LegalTargets(Id);
        if (Targets.IsEmpty())
        {
            FIntPoint Best = Rules.Find(Id)->Cell;
            int32 BestDistance = 1000;
            for (const FIntPoint Cell : Rules.LegalMoves(Id))
            {
                for (const FCombatantState& Enemy : Rules.Snapshot().Encounter.Combatants)
                {
                    if (Enemy.Side != ECombatSide::Enemy || Enemy.Life == ECombatLife::Dead) { continue; }
                    const int32 Distance = FMath::Max(FMath::Abs(Cell.X - Enemy.Cell.X), FMath::Abs(Cell.Y - Enemy.Cell.Y));
                    if (Distance < BestDistance) { BestDistance = Distance; Best = Cell; }
                }
            }
            if (Best != Rules.Find(Id)->Cell) { Game.View->Execute({ECombatCommandType::Move, Id, Best}); Game.Settle(); }
            if (!Game.View->InCombat() || Game.View->Rules().ActiveId() != Id) { continue; }
            Targets = Game.View->Rules().LegalTargets(Id);
        }
        if (!Targets.IsEmpty())
        {
            AttackerId = Id; TargetId = Targets[0];
            return Game.View->Execute({ECombatCommandType::Attack, Id, {}, TargetId});
        }
        if (Game.View->Rules().ActiveId() == Id) { Game.Press(EKeys::SpaceBar); }
    }
    return false;
}

/** The screen rectangle (viewport pixels) a combatant covers: its cell footprint and its body, through the real camera. */
bool UnitRect(const FHarness& Game, int32 Id, FBox2D& Rect)
{
    const FCombatantState* Unit = Game.View->Rules().Find(Id);
    if (!Unit) { return false; }
    const FTransform Pose = Game.View->CameraTransform();
    const FBoardMetric& Metric = Game.View->Metric();
    const float Aspect = float(Game.Target.Size.X) / float(Game.Target.Size.Y);
    TArray<FVector> Points;
    const float Half = Metric.CellCm * 0.5f;
    for (const FVector2D Corner : {FVector2D(-1, -1), FVector2D(1, -1), FVector2D(1, 1), FVector2D(-1, 1)})
    {
        const FVector Base = Game.View->BoardPosition(FVector2D(Unit->Cell.X, Unit->Cell.Y), 0.0f);
        Points.Add(Base + FVector(Corner.X * Half, Corner.Y * Half, 0.0f));
    }
    Points.Add(Game.View->UnitPosition(Id, Metric.Figure(1.0f)));
    Points.Add(Game.View->UnitPosition(Id, 0.0f));
    Rect = FBox2D(ForceInit);
    for (const FVector& Point : Points)
    {
        FVector2D Screen;
        if (!ExpeditionCameraRig::Project(Point, Pose.GetLocation(), Pose.GetRotation().GetForwardVector(), Aspect, Screen)) { return false; }
        Rect += FVector2D(Screen.X * Game.Target.Size.X, Screen.Y * Game.Target.Size.Y);
    }
    return true;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRollCardClearTest, "Project.Expedition.Presentation.RollCardClearOfCombatants",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRollCardClearTest::RunTest(const FString& Parameters)
{
    using namespace RollCardTest;
    FHarness Game(*this, TEXT("RollCardClear"));
    if (!TestTrue(TEXT("Input, board and HUD are ready"), Game.IsReady())) { return false; }
    if (!TestTrue(TEXT("The party has the first turn"), FirstPartyTurn(Game))) { return false; }
    int32 Attacker = INDEX_NONE, Target = INDEX_NONE;
    if (!TestTrue(TEXT("A hero attacks an enemy"), StartAttack(Game, Attacker, Target))) { return false; }
    // Let the d20 tumble in until the card is drawn, then hold time still so every size and yaw sees the same frame.
    for (int32 Index = 0; Index < 12 && !Game.Hud->DiceCardRequested(); ++Index) { Game.Frame(0.05f); }
    if (!TestNotNull(TEXT("The roll card is up"), Game.Hud->DiceCardRequested())) { return false; }

    int32 Checked = 0;
    for (const FIntPoint Size : {FIntPoint(1920, 1080), FIntPoint(1280, 720), FIntPoint(1600, 600)})
    {
        Game.Target.Size = Size;
        const double Scale = ExpeditionLayout::ViewScale(Size.X, Size.Y);
        for (const float Yaw : {-90.0f, 0.0f, 90.0f, 180.0f})
        {
            const FString Where = FString::Printf(TEXT("%dx%d yaw %.0f"), Size.X, Size.Y, Yaw);
            Game.View->SnapCamera(Yaw, Game.View->FramedDistance(Yaw));
            Game.Frame(0.0f);
            Game.Frame(0.0f);
            const FBox2D* Card = Game.Hud->DiceCardRequested();
            if (!TestNotNull(Where + TEXT(": the card is up"), Card)) { continue; }
            const FBox2D Box = *Card;
            TestTrue(Where + TEXT(": the card is inside the viewport"), Box.Min.X >= 0 && Box.Min.Y >= 0 && Box.Max.X <= Size.X && Box.Max.Y <= Size.Y);
            // The card sits outside the area the camera frames the board in.
            const ExpeditionLayout::Rect Bounds = ExpeditionLayout::BoardBounds(Size.X / Scale, Size.Y / Scale);
            const FBox2D Board(FVector2D(Bounds.Left, Bounds.Top) * Scale, FVector2D(Bounds.Right, Bounds.Bottom) * Scale);
            TestFalse(Where + TEXT(": the card is outside the camera's board area"), Box.Intersect(Board));
            // ... and so clear of the two combatants, their footprints and bodies, projected through the real camera.
            for (const int32 Id : {Attacker, Target})
            {
                FBox2D Unit;
                if (!TestTrue(Where + TEXT(": a combatant projects"), UnitRect(Game, Id, Unit))) { continue; }
                TestFalse(Where + (Id == Attacker ? TEXT(": the card does not cover the attacker") : TEXT(": the card does not cover the target")), Box.Intersect(Unit));
                ++Checked;
            }
            // ... and clear of every button, party card and the compact HUD's other panels.
            for (const FBox2D& Button : Game.Hud->ButtonRects()) { TestFalse(Where + TEXT(": the card is not on a button or party card"), Box.Intersect(Button)); }
            for (const FBox2D& Card2 : Game.Hud->PartyDetailRects()) { TestFalse(Where + TEXT(": the card is not on a status panel"), Box.Intersect(Card2)); }
            const ExpeditionLayout::Rect Log = ExpeditionLayout::LogRect(Size.X / Scale, Size.Y / Scale);
            TestFalse(Where + TEXT(": the card is not on the combat log"), Box.Intersect(FBox2D(FVector2D(Log.Left, Log.Top) * Scale, FVector2D(Log.Right, Log.Bottom) * Scale)));
            // It is a transient layer: the persistent budget does not count it.
            TestTrue(Where + TEXT(": the persistent HUD stays within its budget with the card up"), Game.Hud->PersistentCoverage() <= 0.18);
        }
    }
    TestTrue(TEXT("Both combatants were checked at every size and yaw"), Checked == 3 * 4 * 2);
    return !HasAnyErrors();
}

namespace RollCardTest
{
FDiceTray MakeTray(const TCHAR* Title, int32 D20, int32 Extra, int32 ExtraSides)
{
    FDiceTray Tray;
    Tray.Title = Title;
    Tray.Dice.Add({20, D20, 0.0f, FLinearColor(0.95f, 0.92f, 0.84f)});
    for (int32 Index = 0; Index < Extra; ++Index) { Tray.Dice.Add({ExtraSides, 1 + Index % ExtraSides, 0.06f * (Index + 1), FLinearColor(1.0f, 0.55f, 0.45f)}); }
    Tray.Verdict = D20 == 20 ? TEXT("CRITICAL HIT!") : D20 == 1 ? TEXT("NATURAL 1 - MISS") : TEXT("HIT");
    Tray.VerdictColor = FLinearColor(1.0f, 0.55f, 0.35f);
    Tray.Detail = TEXT("d20 17 + 5 = 22  vs  AC 12   -   damage 1d8+3 = 9 - 3 protection = 6, with a second line of arithmetic that has to wrap");
    return Tray;
}
bool Holds(const FBox2D& Outer, const FBox2D& Inner, float Slack = 0.01f)
{
    return Inner.Min.X >= Outer.Min.X - Slack && Inner.Min.Y >= Outer.Min.Y - Slack && Inner.Max.X <= Outer.Max.X + Slack && Inner.Max.Y <= Outer.Max.Y + Slack;
}
}


namespace RollCardTest
{
/** A run at the first route's encounter for a party of the given builds (slots 1-4). */
bool PartyRun(const TArray<FString>& Builds, int32 Seed, FExpeditionRun& Run)
{
    TArray<FPartyBuildSelection> Party;
    for (int32 Index = 0; Index < Builds.Num(); ++Index)
    {
        FPartyBuildSelection Slot;
        Slot.Id = Index + 1; Slot.Name = FString::Printf(TEXT("Hero %d"), Index + 1); Slot.BuildId = Builds[Index];
        Party.Add(Slot);
    }
    FString Error;
    if (!Run.NewRun(Seed, Party, Error, FString::Printf(TEXT("roll-card-%d"), Seed))) { return false; }
    return !Run.Progress().Offers.IsEmpty() && Run.ChooseRoute(Run.Progress().Offers[0].Id, Error);
}
/** The catalog's builds with the longest full-status text (the tallest hover panels), longest first. */
TArray<FString> TallestStatusBuilds(int32 Count)
{
    TArray<TPair<int32, FString>> Lengths;
    for (const auto& Build : FBuildCatalog::All())
    {
        FExpeditionRun Run;
        if (!PartyRun({Build.Id, Build.Id, Build.Id, Build.Id}, 5, Run)) { continue; }
        for (const FCombatantState& Unit : Run.Battle().Snapshot().Encounter.Combatants)
        { if (Unit.Side == ECombatSide::Party) { Lengths.Add({FBuildCatalog::Status(Unit).Len() + FBuildCatalog::DisplayName(Unit).Len(), Build.Id}); break; } }
    }
    Lengths.Sort([](const TPair<int32, FString>& A, const TPair<int32, FString>& B) { return A.Key == B.Key ? A.Value < B.Value : A.Key > B.Key; });
    TArray<FString> Result;
    for (int32 Index = 0; Index < Lengths.Num() && Result.Num() < Count; ++Index) { Result.Add(Lengths[Index].Value); }
    return Result;
}
}

/** #190 review: tumbling, landing and glowing dice stay inside the card's dice row, below the title, over the whole animation, at every size. */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRollCardDiceInsideTest, "Project.Expedition.Presentation.RollCardDiceStayInsideCard",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRollCardDiceInsideTest::RunTest(const FString& Parameters)
{
    using namespace RollCardTest;
    FHarness Game(*this, TEXT("RollCardDice"));
    if (!TestTrue(TEXT("Input, board and HUD are ready"), Game.IsReady())) { return false; }
    if (!TestTrue(TEXT("The party has the first turn"), FirstPartyTurn(Game))) { return false; }
    const FDiceTray Trays[] = {MakeTray(TEXT("Arden attacks Raider 2"), 17, 1, 8), MakeTray(TEXT("Nix attacks Scout"), 20, 2, 6), MakeTray(TEXT("Ivo casts a burst"), 1, 8, 6), MakeTray(TEXT("Sera attacks"), 20, 0, 6)};
    int32 Frames = 0;
    for (const FIntPoint Size : {FIntPoint(1920, 1080), FIntPoint(1280, 720), FIntPoint(1600, 600)})
    {
        Game.Target.Size = Size;
        const double Scale = ExpeditionLayout::ViewScale(Size.X, Size.Y);
        for (const FDiceTray& Tray : Trays)
        {
            for (float Age = 0.02f; Age < 1.4f; Age += 0.03f)
            {
                Game.View->PreviewRoll(Tray, Age);
                Game.Frame(0.0f);
                const FString Where = FString::Printf(TEXT("%dx%d %s age %.2f"), Size.X, Size.Y, *Tray.Title, Age);
                const FBox2D* Card = Game.Hud->DiceCardRequested();
                if (!TestNotNull(Where + TEXT(": the card is up"), Card)) { break; }
                const FBox2D Row = Game.Hud->DiceRowRect(), Dice = Game.Hud->DiceBounds();
                TestTrue(Where + TEXT(": the dice row is inside the card"), Holds(*Card, Row));
                TestTrue(Where + TEXT(": the dice row is below the title"), Row.Min.Y >= Card->Min.Y + ExpeditionLayout::RollCardRowTop * Scale - 0.01);
                if (!TestTrue(Where + TEXT(": dice were drawn"), Dice.bIsValid)) { continue; }
                TestTrue(Where + TEXT(": every die, its shadow and glow stay inside the dice row"), Holds(Row, Dice));
                ++Frames;
            }
        }
    }
    TestTrue(TEXT("Many animation frames were checked"), Frames >= 500);
    Game.View->ClearRollPreview();
    return !HasAnyErrors();
}

/** #190 review: a party card's status panel keeps clear of the roll card, for every hero, with the details explicitly open. */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRollCardDetailPanelTest, "Project.Expedition.Presentation.RollCardKeepsStatusPanelsClear",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRollCardDetailPanelTest::RunTest(const FString& Parameters)
{
    using namespace RollCardTest;
    // The four builds with the longest status text, so every card opens the tallest panel the catalog can produce.
    const TArray<FString> Tall = TallestStatusBuilds(4);
    if (!TestEqual(TEXT("Four tall builds were found"), Tall.Num(), 4)) { return false; }
    FExpeditionRun Run;
    if (!TestTrue(TEXT("A party of them starts the first encounter"), PartyRun(Tall, 7, Run))) { return false; }
    FExpeditionSaveData Data;
    Data.Current = Run.Snapshot();
    FHarness Game(*this, TEXT("RollCardDetails"), &Data);
    if (!TestTrue(TEXT("Input, board and HUD are ready"), Game.IsReady())) { return false; }
    Game.Frame();
    Game.Press(EKeys::O);
    if (!TestTrue(TEXT("The saved encounter resumes"), Game.View->HasRun() && Game.View->InCombat())) { return false; }
    Game.Settle();
    Game.PartyTurn();
    // The tallest real panel is five lines (title, health, a Protect line, the resources over two); ask for six.
    Game.Hud->PreviewDetailLines(6);
    Game.View->PreviewRoll(MakeTray(TEXT("Arden attacks Raider 2"), 17, 1, 8));
    int32 Opened = 0;
    double Tallest = 0;
    for (const FIntPoint Size : {FIntPoint(1920, 1080), FIntPoint(1280, 720), FIntPoint(1600, 600)})
    {
        Game.Target.Size = Size;
        const double Scale = ExpeditionLayout::ViewScale(Size.X, Size.Y);
        for (int32 Card = 0; Card < 4; ++Card)
        {
            const FString Where = FString::Printf(TEXT("%dx%d card %d"), Size.X, Size.Y, Card + 1);
            Game.Player->SimulatePointer(FVector2D(108.0, ExpeditionLayout::PartyCardTop + Card * ExpeditionLayout::PartyCardPitch + 32.0) * Scale, FIntPoint(0, 0));
            Game.Frame(0.0f);
            Game.Frame(0.0f);
            const FBox2D* RollCard = Game.Hud->DiceCardRequested();
            if (!TestNotNull(Where + TEXT(": the roll card is up"), RollCard)) { continue; }
            // The details are explicitly open: hovering a card opens its status panel.
            if (!TestFalse(Where + TEXT(": hovering opens a status panel"), Game.Hud->PartyDetailRects().IsEmpty())) { continue; }
            ++Opened;
            for (const FBox2D& Panel : Game.Hud->PartyDetailRects())
            {
                // Without the avoidance the tallest panels of the lowest cards reach down into the card.
                Tallest = FMath::Max(Tallest, Panel.GetSize().Y / Scale);
                TestFalse(Where + TEXT(": the status panel does not overlap the roll card"), Panel.Intersect(*RollCard));
                TestTrue(Where + TEXT(": the status panel stays on screen"), Panel.Min.X >= 0 && Panel.Min.Y >= 0 && Panel.Max.X <= Size.X && Panel.Max.Y <= Size.Y);
            }
        }
    }
    TestTrue(TEXT("A status panel opened for every hero at every size"), Opened == 12);
    AddInfo(FString::Printf(TEXT("Tallest status panel checked: %.0f design units"), Tallest));
    TestTrue(TEXT("The panels were tall enough to reach the roll card unless moved (the lowest card's panel starts at 258)"), 258.0 + Tallest > ExpeditionLayout::PartyCardTop + 3 * ExpeditionLayout::PartyCardPitch + ExpeditionLayout::PartyCardHeight + ExpeditionLayout::RollCardGap);
    Game.Hud->PreviewDetailLines(0);
    Game.Player->ReleaseSimulatedPointer();
    Game.View->ClearRollPreview();
    return !HasAnyErrors();
}

/** #190 review: the combat hint and the roll card share the left margin, so the hint waits until the card has gone (after playback too). */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRollCardHintTest, "Project.Expedition.Presentation.HintWaitsForRollCard",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRollCardHintTest::RunTest(const FString& Parameters)
{
    using namespace RollCardTest;
    FHarness Game(*this, TEXT("RollCardHint"));
    if (!TestTrue(TEXT("Input, board and HUD are ready"), Game.IsReady())) { return false; }
    if (!TestTrue(TEXT("The party has the first turn"), FirstPartyTurn(Game))) { return false; }
    for (int32 Index = 0; Index < 6; ++Index) { Game.Frame(0.05f); }
    if (!TestTrue(TEXT("The first hint is up, undismissed"), Game.Player->VisibleHint() != ECombatHint::Count && Game.Hud->HintRequested() != nullptr)) { return false; }
    const ECombatHint Hint = Game.Player->VisibleHint();

    // A roll card is up: the hint is not drawn and its keys do nothing.
    Game.View->PreviewRoll(MakeTray(TEXT("Arden attacks Raider 2"), 17, 1, 8));
    Game.Frame(0.0f);
    TestNotNull(TEXT("The roll card is up"), Game.Hud->DiceCardRequested());
    TestNull(TEXT("The hint is not drawn over it"), Game.Hud->HintRequested());
    TestTrue(TEXT("The hint is held back, not dismissed"), Game.Player->VisibleHint() == ECombatHint::Count);
    // Once the card has gone the same hint returns.
    Game.View->ClearRollPreview();
    Game.Frame(0.05f);
    TestNull(TEXT("The card is gone"), Game.Hud->DiceCardRequested());
    TestTrue(TEXT("The hint returns, the same one"), Game.Player->VisibleHint() == Hint && Game.Hud->HintRequested() != nullptr);

    // A real attack: when playback ends the card still lingers and the hint comes back at once; it must not be drawn over the card.
    int32 Attacker = INDEX_NONE, Target = INDEX_NONE;
    if (!TestTrue(TEXT("A hero attacks an enemy"), StartAttack(Game, Attacker, Target))) { return false; }
    for (int32 Index = 0; Index < 400 && Game.View->IsPresenting(); ++Index) { Game.Frame(0.05f); }
    if (!TestTrue(TEXT("Playback is over"), !Game.View->IsPresenting())) { return false; }
    if (!TestTrue(TEXT("The roll card still lingers after playback"), Game.View->RollCardUp())) { return false; }
    FString Why;
    if (!TestTrue(TEXT("The hint returns at the end of playback"), Game.Player->PreviewHint(Hint, Why))) { return false; }
    int32 Both = 0, CardFrames = 0, HintFrames = 0;
    for (int32 Index = 0; Index < 240; ++Index)
    {
        Game.Frame(0.05f);
        const bool bCard = Game.Hud->DiceCardRequested() != nullptr, bHint = Game.Hud->HintRequested() != nullptr;
        if (bCard) { ++CardFrames; }
        if (bHint) { ++HintFrames; }
        if (bCard && bHint) { ++Both; }
    }
    TestTrue(TEXT("The attack's roll card was drawn after playback"), CardFrames > 0);
    TestEqual(TEXT("The hint and the roll card are never drawn together after playback"), Both, 0);
    TestTrue(TEXT("The hint is drawn once the card has gone"), HintFrames > 0);
    AddInfo(FString::Printf(TEXT("After playback: card frames %d, hint frames %d"), CardFrames, HintFrames));
    return !HasAnyErrors();
}
#endif
