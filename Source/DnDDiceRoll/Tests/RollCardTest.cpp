#if WITH_DEV_AUTOMATION_TESTS
#include "ExpeditionInputHarness.h"
#include "Presentation/CameraRig.h"
#include "Presentation/TargetChipLayout.h"

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
#endif
