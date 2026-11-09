#include "Presentation/ExpeditionHUD.h"
#include "Presentation/ExpeditionBattleView.h"
#include "Presentation/ExpeditionPlayerController.h"
#include "CanvasItem.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "GlobalRenderResources.h"
#include "Party/BuildCatalog.h"
#include "Presentation/HudCoverage.h"
#include "Presentation/TargetChipLayout.h"

namespace
{
// Dark-fantasy palette: warm near-black panels, parchment text and darkened gilt.
const FLinearColor Ink(0.009f, 0.007f, 0.006f);
const FLinearColor Parchment(0.93f, 0.86f, 0.72f);
const FLinearColor Muted(0.66f, 0.6f, 0.52f);
const FLinearColor Gold(0.95f, 0.72f, 0.32f);
const FLinearColor Danger(1.0f, 0.4f, 0.3f);
const FLinearColor Safe(0.45f, 0.95f, 0.55f);
const FLinearColor Sky(0.4f, 0.85f, 1.0f);

FLinearColor WithAlpha(FLinearColor Color, float Alpha) { Color.A *= Alpha; return Color; }
FLinearColor HealthColor(float Fraction)
{
    return Fraction > 0.5f ? FMath::Lerp(FLinearColor(0.95f, 0.8f, 0.25f), FLinearColor(0.35f, 0.85f, 0.4f), (Fraction - 0.5f) * 2.0f)
        : FMath::Lerp(FLinearColor(0.9f, 0.25f, 0.2f), FLinearColor(0.95f, 0.8f, 0.25f), Fraction * 2.0f);
}
FString Initials(const FCombatantState& Unit)
{
    if (Unit.Side == ECombatSide::Party) { return Unit.Name.Left(1); }
    const TCHAR Last = Unit.Name.IsEmpty() ? TEXT(' ') : Unit.Name[Unit.Name.Len() - 1];
    return FChar::IsDigit(Last) ? Unit.Name.Left(1) + Last : Unit.Name.Left(1);
}
FString PhaseLine(const FExpeditionProgress& Run)
{
    switch (Run.Phase)
    {
    case EExpeditionPhase::Route: return FString::Printf(TEXT("Choosing the road  ·  %d of %d won"), Run.EncountersCompleted, FExpeditionRun::EncounterCount);
    case EExpeditionPhase::Reward: return TEXT("Dividing the spoils");
    case EExpeditionPhase::Recovery: return TEXT("A chance to rest");
    case EExpeditionPhase::Victory: return TEXT("Expedition complete");
    case EExpeditionPhase::Defeat: return TEXT("Expedition lost");
    default: return FString();
    }
}
uint32 Scramble(uint32 Value)
{
    Value ^= Value >> 16; Value *= 0x7feb352d; Value ^= Value >> 15; Value *= 0x846ca68b; Value ^= Value >> 16;
    return Value;
}
}

// ---------------------------------------------------------------------------------------
// Primitives

void AExpeditionHUD::Rect(FLinearColor Color, float X, float Y, float Width, float Height)
{
    FCanvasTileItem Tile(FVector2D(X * S, Y * S), FVector2D(Width * S, Height * S), Color);
    Tile.BlendMode = SE_BLEND_Translucent;
    Canvas->DrawItem(Tile);
}

void AExpeditionHUD::Frame(FLinearColor Color, float X, float Y, float Width, float Height, float Thickness)
{
    Rect(Color, X, Y, Width, Thickness);
    Rect(Color, X, Y + Height - Thickness, Width, Thickness);
    Rect(Color, X, Y, Thickness, Height);
    Rect(Color, X + Width - Thickness, Y, Thickness, Height);
}

void AExpeditionHUD::Panel(float X, float Y, float Width, float Height, FLinearColor Accent, float Opacity, bool bBlock)
{
    ChipExclusions.Add(FBox2D(FVector2D(X, Y), FVector2D(X + Width, Y + Height)));
    Rect(FLinearColor(0, 0, 0, 0.35f * Opacity), X + 3, Y + 4, Width, Height);
    Rect(FLinearColor(Ink.R, Ink.G, Ink.B, Opacity), X, Y, Width, Height);
    Rect(FLinearColor(1, 1, 1, 0.035f * Opacity), X, Y, Width, Height * 0.45f);
    // Gilded double frame with corner studs.
    Frame(WithAlpha(Accent, 0.45f * Opacity), X, Y, Width, Height);
    Frame(WithAlpha(Accent, 0.14f * Opacity), X + 4, Y + 4, Width - 8, Height - 8);
    Rect(WithAlpha(Accent, 0.8f * Opacity), X, Y, Width, 2);
    for (const FVector2D Corner : {FVector2D(X, Y), FVector2D(X + Width, Y), FVector2D(X, Y + Height), FVector2D(X + Width, Y + Height)})
    {
        Polygon(Corner, 5.0f, 4, 0.0f, WithAlpha(Accent, 0.9f * Opacity));
    }
    if (bBlock) { Blockers.Add(FBox2D(FVector2D(X, Y) * S, FVector2D(X + Width, Y + Height) * S)); }
}

FVector2D AExpeditionHUD::Measure(const FString& Value, float Scale) const
{
    float Width = 0, Height = 0;
    const_cast<AExpeditionHUD*>(this)->GetTextSize(Value, Width, Height, GEngine->GetMediumFont(), Scale);
    return FVector2D(Width, Height);
}

TArray<FString> AExpeditionHUD::Wrap(const FString& Value, float Width, float Scale) const
{
    TArray<FString> Words, Lines; Value.ParseIntoArrayWS(Words);
    FString Line;
    for (const auto& Word : Words)
    {
        const FString Candidate = Line.IsEmpty() ? Word : Line + TEXT(" ") + Word;
        if (!Line.IsEmpty() && Measure(Candidate, Scale).X > Width) { Lines.Add(Line); Line = Word; }
        else { Line = Candidate; }
    }
    if (!Line.IsEmpty()) { Lines.Add(Line); }
    return Lines;
}

void AExpeditionHUD::Text(const FString& Value, float X, float Y, FLinearColor Color, float Scale, EAlign Align, float MaxWidth)
{
    if (Value.IsEmpty() || Color.A <= 0.01f) { return; }
    FVector2D Size = Measure(Value, Scale);
    if (MaxWidth > 0 && Size.X > MaxWidth) { Scale *= MaxWidth / Size.X; Size = Measure(Value, Scale); }
    const float Left = Align == EAlign::Center ? X - Size.X * 0.5f : Align == EAlign::Right ? X - Size.X : X;
    FCanvasTextItem Item(FVector2D(Left * S, Y * S), FText::FromString(Value), GEngine->GetMediumFont(), Color);
    Item.Scale = FVector2D(Scale * S);
    Item.BlendMode = SE_BLEND_Translucent;
    Item.EnableShadow(FLinearColor(0, 0, 0, 0.75f * Color.A), FVector2D(1.5f, 1.5f));
    Canvas->DrawItem(Item);
}

void AExpeditionHUD::Polygon(FVector2D Center, float Radius, int32 Sides, float Rotation, FLinearColor Color, float Squash)
{
    if (Sides < 3 || Color.A <= 0.01f) { return; }
    TArray<FCanvasUVTri> Triangles;
    const FVector2D Middle = Center * S;
    auto Corner = [&](int32 Index)
    {
        const float Angle = Rotation + Index * 2.0f * PI / Sides;
        return Middle + FVector2D(FMath::Cos(Angle) * Radius, FMath::Sin(Angle) * Radius * Squash) * S;
    };
    for (int32 Index = 0; Index < Sides; ++Index)
    {
        FCanvasUVTri Triangle;
        Triangle.V0_Pos = Middle; Triangle.V1_Pos = Corner(Index); Triangle.V2_Pos = Corner(Index + 1);
        Triangle.V0_Color = Triangle.V1_Color = Triangle.V2_Color = Color;
        Triangles.Add(Triangle);
    }
    FCanvasTriangleItem Item(Triangles, GWhiteTexture);
    Item.BlendMode = SE_BLEND_Translucent;
    Canvas->DrawItem(Item);
}

void AExpeditionHUD::Line(FVector2D A, FVector2D B, FLinearColor Color, float Thickness)
{
    FCanvasLineItem Item(A * S, B * S);
    Item.SetColor(Color);
    Item.LineThickness = Thickness * S;
    Item.BlendMode = SE_BLEND_Translucent;
    Canvas->DrawItem(Item);
}

void AExpeditionHUD::Bar(float X, float Y, float Width, float Height, float Fraction, float Trail, FLinearColor Fill)
{
    Rect(FLinearColor(0, 0, 0, 0.65f), X - 1, Y - 1, Width + 2, Height + 2);
    Rect(FLinearColor(0.12f, 0.12f, 0.14f, 0.9f), X, Y, Width, Height);
    if (Trail > Fraction) { Rect(FLinearColor(1.0f, 0.85f, 0.6f, 0.85f), X + Width * Fraction, Y, Width * (Trail - Fraction), Height); }
    Rect(Fill, X, Y, Width * FMath::Clamp(Fraction, 0.0f, 1.0f), Height);
    Rect(FLinearColor(1, 1, 1, 0.18f), X, Y, Width * FMath::Clamp(Fraction, 0.0f, 1.0f), Height * 0.4f);
}

bool AExpeditionHUD::Hovered(float X, float Y, float Width, float Height) const
{
    return Mouse.X >= X && Mouse.X < X + Width && Mouse.Y >= Y && Mouse.Y < Y + Height;
}

bool AExpeditionHUD::Button(FName Id, const FString& Label, const FString& Sub, float X, float Y, float Width, float Height, bool bEnabled, bool bSelected, FLinearColor Accent, float LabelScale)
{
    const bool bHover = bEnabled && Hovered(X, Y, Width, Height);
    const float Lift = 0.0f;
    const bool bFocused = bEnabled && Player.IsValid() && Player->MenuFocus() == Id;
    Rect(FLinearColor(0, 0, 0, 0.4f), X + 2, Y + 3, Width, Height);
    const FLinearColor Base = bSelected ? FLinearColor(Accent.R * 0.12f, Accent.G * 0.12f, Accent.B * 0.12f, 0.97f)
        : bHover ? FLinearColor(0.05f, 0.035f, 0.025f, 0.97f) : FLinearColor(0.016f, 0.012f, 0.01f, 0.94f);
    Rect(Base, X, Y + Lift, Width, Height);
    Rect(FLinearColor(1, 1, 1, bHover ? 0.07f : 0.035f), X, Y + Lift, Width, Height * 0.45f);
    Frame(WithAlpha(Accent, bSelected ? 0.95f : bHover ? 0.7f : bEnabled ? 0.35f : 0.12f), X, Y + Lift, Width, Height, bSelected ? 2.0f : 1.0f);
    if (bFocused) { Frame(Parchment, X - 3, Y - 3, Width + 6, Height + 6, 2); }
    const FLinearColor LabelColor = bEnabled ? (bSelected ? Accent : Parchment) : FLinearColor(0.45f, 0.48f, 0.52f);
    const float Ratio = LabelScale / 0.92f;
    const float LabelY = Sub.IsEmpty() ? Y + Lift + Height * 0.5f - 10 * Ratio : Y + Lift + Height * 0.5f - 19 * Ratio;
    Text(Label, X + Width * 0.5f, LabelY, LabelColor, LabelScale, EAlign::Center, Width - 12);
    if (!Sub.IsEmpty()) { Text(Sub, X + Width * 0.5f, Y + Lift + Height * 0.5f + 2 * Ratio, bEnabled ? Muted : FLinearColor(0.38f, 0.4f, 0.44f), 0.68f * Ratio, EAlign::Center, Width - 10); }
    Buttons.Add({Id, FBox2D(FVector2D(X, Y) * S, FVector2D(X + Width, Y + Height) * S), bEnabled, bTransientButtons});
    ChipExclusions.Add(FBox2D(FVector2D(X, Y), FVector2D(X + Width, Y + Height)));
    return bHover;
}

bool AExpeditionHUD::Project(const FVector& World, FVector2D& Screen) const
{
    if (!Player.IsValid() || !Player->ProjectWorldLocationToScreen(World, Screen)) { return false; }
    Screen /= S;
    return true;
}

bool AExpeditionHUD::ButtonAt(FVector2D ScreenPosition, FName& Id) const
{
    for (int32 Index = Buttons.Num() - 1; Index >= 0; --Index)
    {
        if (Buttons[Index].Box.IsInside(ScreenPosition)) { if (!Buttons[Index].bEnabled) { return false; } Id = Buttons[Index].Id; return true; }
    }
    return false;
}

bool AExpeditionHUD::CoversBoard(FVector2D ScreenPosition) const
{
    for (const FButtonArea& Area : Buttons) { if (Area.Box.IsInside(ScreenPosition)) { return true; } }
    for (const FBox2D& Box : Blockers) { if (Box.IsInside(ScreenPosition)) { return true; } }
    return false;
}

void AExpeditionHUD::Die(FVector2D Center, float Radius, int32 Sides, int32 Value, FLinearColor Color, float Spin, float Alpha, bool bLanded)
{
    int32 Corners = 6; float Rotation = -PI / 2; float Squash = 1.0f;
    switch (Sides)
    {
    case 4: Corners = 3; Rotation = -PI / 2; break;
    case 6: Corners = 4; Rotation = PI / 4; Radius *= 1.05f; break;
    case 8: Corners = 4; Rotation = 0; Squash = 1.2f; break;
    case 10: case 12: Corners = 5; Rotation = -PI / 2; break;
    default: break;
    }
    Rotation += Spin;
    Polygon(Center + FVector2D(3, 5), Radius, Corners, Rotation, FLinearColor(0, 0, 0, 0.35f * Alpha), Squash);
    Polygon(Center, Radius, Corners, Rotation, WithAlpha(Color * 0.55f, Alpha), Squash);
    Polygon(Center, Radius * 0.84f, Corners, Rotation, WithAlpha(Color, Alpha), Squash);
    if (Sides == 20)
    {
        // A facet triangle gives the d20 its familiar face.
        Polygon(Center, Radius * 0.55f, 3, Rotation, WithAlpha(Color * 1.12f, Alpha));
        for (int32 Index = 0; Index < 3; ++Index)
        {
            const float Inner = Rotation + Index * 2.0f * PI / 3.0f;
            const FVector2D From = Center + FVector2D(FMath::Cos(Inner), FMath::Sin(Inner)) * Radius * 0.55f;
            for (int32 Edge = 0; Edge < 2; ++Edge)
            {
                const float Outer = Rotation + (Index * 2 + Edge) * 2.0f * PI / 6.0f;
                Line(From, Center + FVector2D(FMath::Cos(Outer), FMath::Sin(Outer)) * Radius * 0.84f, WithAlpha(Color * 0.7f, Alpha), 1.0f);
            }
        }
    }
    const float Scale = Radius / (Sides == 20 ? 24.0f : 20.0f) * (Value >= 10 ? 0.9f : 1.0f);
    const FVector2D Size = Measure(FString::FromInt(Value), Scale);
    Text(FString::FromInt(Value), Center.X, Center.Y - Size.Y * 0.5f, FLinearColor(0.08f, 0.06f, 0.05f, Alpha * (bLanded ? 1.0f : 0.75f)), Scale, EAlign::Center);
}

// ---------------------------------------------------------------------------------------
// Frame

void AExpeditionHUD::DrawHUD()
{
    Super::DrawHUD();
    Buttons.Reset();
    Blockers.Reset();
    Persistent.Reset();
    ChipExclusions.Reset();
    ChipsRequested = 0;
    ReachRequested = 0;
    DetailUnitId = INDEX_NONE;
    bCursorRequested = false;
    bTransientButtons = false;
    Player = Cast<AExpeditionPlayerController>(GetOwningPlayerController());
    View = Player.IsValid() ? Player->GetBattleView() : nullptr;
    if (!Player.IsValid() || !View.IsValid() || !Canvas) { return; }
    S = ExpeditionLayout::ViewScale(Canvas->SizeX, Canvas->SizeY);
    W = Canvas->SizeX / S;
    H = Canvas->SizeY / S;
    Clock = GetWorld() ? GetWorld()->GetRealTimeSeconds() : 0.0f;
    float MouseX = -1, MouseY = -1;
    // A capture run ignores the physical pointer so hover never changes a shot.
    Mouse = !Player->IsCapturing() && Player->GetMousePosition(MouseX, MouseY) ? FVector2D(MouseX, MouseY) / S : FVector2D(-1, -1);

    // The tabletop overview has no board: unit bars and labels belong to battles only.
    if (View->HasRun() && !View->ShowingOverview()) { DrawWorldOverlays(); }
    DrawTopBar();
    if (!View->HasRun()) { DrawTitle(); }
    else
    {
        if (View->InCombat() || View->Progress().Phase == EExpeditionPhase::Route || View->Progress().Phase == EExpeditionPhase::Reward || View->Progress().Phase == EExpeditionPhase::Recovery) { DrawParty(); }
        if (View->InCombat()) { DrawLog(); }
        if (View->InCombat())
        {
            DrawDice();
            DrawActionBar();
            DrawForecast();
        }
        else if (!View->IsPresenting() && !View->ShowingRecords())
        {
            switch (View->Progress().Phase)
            {
            case EExpeditionPhase::Route: DrawRoutes(); break;
            case EExpeditionPhase::Reward: DrawReward(); break;
            case EExpeditionPhase::Recovery: DrawCamp(); break;
            case EExpeditionPhase::Victory: case EExpeditionPhase::Defeat: DrawEnding(); break;
            default: break;
            }
        }
        else { DrawDice(); }
    }
    if (View->InCombat()) { DrawBanner(); }
    DrawTargetChips(); // All foreground panel bounds are known now, including the hover forecast.
    if (!View->InCombat() && !Player->ShowingBuilder() && !View->Messages().IsEmpty())
    {
        float FeedbackY=H-64;
        for (const auto& Line : Wrap(View->Messages().Last(),900,0.85f)) { Text(Line,(W-900)/2,FeedbackY,Parchment,0.85f); FeedbackY+=23; }
    }
    // Overlays own every click: buttons hidden beneath them must not fire.
    if (Player->OverlayOpen()) { Buttons.Reset(); }
    if (Player->ShowingMenu()) { Buttons.Reset(); DrawMenuPanel(); }
    if (View->ShowingRecords()) { DrawRecords(); }
    if (Player->ShowingHelp()) { DrawHelp(); }
    if (Player->ConfirmingReturn()) { DrawReturnConfirm(); }
    Player->UpdateMenuFocus();
}

// ---------------------------------------------------------------------------------------
// World-anchored overlays: health bars, floating numbers, the move path.

void AExpeditionHUD::DrawWorldOverlays()
{
    const FCombatBattle& Rules = View->Rules();
    const FExpeditionPlayback& Show = View->Playback();
    const bool bCombat = View->InCombat() && !Player->OverlayOpen();
    const int32 Active = bCombat ? Rules.ActiveId() : INDEX_NONE;

    // Boards with the contextual grid (#142) draw reach, path, blocked sight and victims on the ground itself;
    // the HUD then keeps only what needs text. Primitive boards keep the dotted trail and markers below.
    const bool bGroundGrid = View->DrawsContextualGrid();

    // Path preview: a dotted trail from the adventurer to the hovered cell (the path length is always labelled).
    const TArray<FIntPoint>& Path = Player->PreviewPath();
    if (Player->ShowsTargeting() && !Path.IsEmpty())
    {
        FVector2D Previous;
        bool bHasPrevious = Project(View->UnitPosition(Player->Actor(), View->Metric().Scaled(4.0f)), Previous);
        for (int32 Index = 0; Index < Path.Num(); ++Index)
        {
            FVector2D Point;
            if (!Project(View->BoardPosition(FVector2D(Path[Index].X, Path[Index].Y), View->Metric().Scaled(4.0f)), Point)) { continue; }
            if (!bGroundGrid)
            {
                if (bHasPrevious) { Line(Previous, Point, FLinearColor(0.5f, 0.95f, 1.0f, 0.9f), 3.0f); }
                Circle(Point, Index == Path.Num() - 1 ? 9.0f : 4.0f, FLinearColor(0.55f, 0.95f, 1.0f, 0.95f));
            }
            Previous = Point; bHasPrevious = true;
        }
        if (bHasPrevious)
        {
            const FString Cost = FString::FromInt(Path.Num());
            if (bGroundGrid)
            {
                // No light endpoint disc under the number here, so it gets its own dark plate with a trail-coloured edge
                // and light text: readable on any themed ground.
                const FVector2D Size = Measure(Cost, 0.8f);
                const float PlateW = FMath::Max(Size.X + 14.0f, Size.Y + 6.0f), PlateH = Size.Y + 6.0f;
                const float PlateX = Previous.X - PlateW * 0.5f, PlateY = Previous.Y - 30.0f - 3.0f;
                Rect(FLinearColor(Ink.R, Ink.G, Ink.B, 0.82f), PlateX, PlateY, PlateW, PlateH);
                Frame(WithAlpha(Sky, 0.9f), PlateX, PlateY, PlateW, PlateH, 1.5f);
                Text(Cost, Previous.X, Previous.Y - 30.0f, Parchment, 0.8f, EAlign::Center);
            }
            else { Text(Cost, Previous.X, Previous.Y - 9, Ink, 0.8f, EAlign::Center); }
        }
    }

    // Inspection stays available during enemy turns and playback, just like arrow input.
    if (bCombat && Player->UsingKeyboardCursor())
    {
        bCursorRequested = true;
        const FIntPoint Cell = Player->KeyboardCursorCell();
        const FBoardMetric& Metric = View->Metric();
        const FVector Center = View->BoardPosition(FVector2D(Cell.X, Cell.Y), Metric.Scaled(2.0f));
        const float Half = Metric.CellCm * 0.47f;
        const FVector Offsets[] = {{-Half, -Half, 0}, {Half, -Half, 0}, {Half, Half, 0}, {-Half, Half, 0}};
        FVector2D Corners[4];
        bool bVisible = true;
        for (int32 Index = 0; Index < 4; ++Index) { bVisible &= Project(Center + Offsets[Index], Corners[Index]); }
        if (bVisible)
        {
            const FLinearColor Cursor(1.0f, 0.65f, 0.15f, 0.75f + 0.25f * FMath::Sin(Clock * 6.0f));
            for (int32 Index = 0; Index < 4; ++Index)
            {
                const FVector2D A = Corners[Index], B = Corners[(Index + 1) % 4], C = Corners[(Index + 3) % 4];
                Line(A, FMath::Lerp(A, B, 0.3f), Cursor, 3.5f);
                Line(A, FMath::Lerp(A, C, 0.3f), Cursor, 3.5f);
            }
        }
    }

    // Geometry is independent of target eligibility and available budgets.
    if (Player->ShowsTargeting())
    {
        Text(bGroundGrid ? TEXT("Outline Reach  /  x Blocked sight  /  Ring Legal target  /  AREA Victim")
            : TEXT("o Reach  /  x Blocked sight  /  # Legal target  /  AREA Victim"), W*0.5f, 133, Parchment, 0.65f, EAlign::Center);
        const auto& Reach = Player->Reach();
        auto DrawCells = [&](const TArray<FIntPoint>& Cells, bool bBlocked)
        {
            for (const auto Cell : Cells)
            {
                ++ReachRequested;
                if (bGroundGrid) { continue; }
                FVector2D P;
                if (!Project(View->BoardPosition(FVector2D(Cell.X, Cell.Y), View->Metric().Scaled(5.0f)), P)) { continue; }
                const auto Color = bBlocked ? Danger : Sky;
                if (bBlocked) { Line(P-FVector2D(4,4), P+FVector2D(4,4), Color, 1); Line(P+FVector2D(-4,4), P+FVector2D(4,-4), Color, 1); }
                else { Circle(P, 4, Color); }
            }
        };
        DrawCells(Reach.Visible, false); DrawCells(Reach.Obstructed, true);
        const auto Preview = Rules.Forecast(Player->Actor(), Player->Action(), Player->HoveredTarget());
        for (const auto& Effect : Preview.Effects)
        {
            if (Effect.Effect != EAbilityEffect::Strike) { continue; }
            FVector2D P;
            if (Project(View->UnitPosition(Effect.TargetId, View->Metric().Figure(1.1f)), P))
            { Frame(Gold, P.X-20, P.Y-12, 40, 24, 2); Text(TEXT("AREA"), P.X, P.Y-8, Gold, 0.65f, EAlign::Center); }
        }
    }

    for (const FCombatantState& Unit : Rules.Snapshot().Encounter.Combatants)
    {
        const FUnitDisplay* Display = Show.Unit(Unit.Id);
        if (!Display) { continue; }
        FVector2D Point;
        if (!Project(View->UnitPosition(Unit.Id, Display->Tilt > 0.5f ? View->Metric().Figure(0.4f) : View->Metric().Figure(0.9f)), Point)) { continue; }
        const bool bFocus = Unit.Id == Active || Unit.Id == Player->HoveredUnit() || Unit.Id == Player->HoveredTarget() || Unit.Id == Player->Selection();
        if (Display->Life == ECombatLife::Dead)
        {
            if (Unit.Side == ECombatSide::Party) { Text(Unit.Name + TEXT("  ·  fallen"), Point.X, Point.Y - 8, FLinearColor(0.6f, 0.6f, 0.62f, 0.85f), 0.7f, EAlign::Center); }
            continue;
        }
        if (Display->Life == ECombatLife::Downed)
        {
            Text(TEXT("DOWNED"), Point.X, Point.Y - 26, FLinearColor(1.0f, 0.6f, 0.3f), 0.72f, EAlign::Center);
            for (int32 Pip = 0; Pip < 3; ++Pip)
            {
                const bool bLeft = Pip < Unit.RescueTurnsLeft;
                Circle(FVector2D(Point.X - 14 + Pip * 14, Point.Y - 2), 5.0f, bLeft ? FLinearColor(1.0f, 0.6f, 0.3f) : FLinearColor(0.25f, 0.2f, 0.2f, 0.8f));
            }
            if (bFocus) { Text(FString::Printf(TEXT("%s · rescue within %d turns"), *Unit.Name, Unit.RescueTurnsLeft), Point.X, Point.Y + 8, Parchment, 0.68f, EAlign::Center); }
            continue;
        }
        const float Fraction = Display->ShownHP / Display->MaxHP;
        const float Width = Unit.Side == ECombatSide::Party ? 56.0f : 50.0f;
        const FLinearColor Fill = Unit.Side == ECombatSide::Party ? HealthColor(Fraction) : FLinearColor(0.9f, 0.25f, 0.2f);
        Bar(Point.X - Width * 0.5f, Point.Y, Width, 6, Fraction, Display->TrailHP / Display->MaxHP, Fill);
        if (bFocus)
        {
            Text(FString::Printf(TEXT("%s  %d/%d"), *Unit.Name, Display->HP, Display->MaxHP), Point.X, Point.Y - 20,
                ExpeditionStyle::UnitColor(Unit), 0.74f, EAlign::Center);
        }
    }

    for (const FFloatingText& Floater : Show.Texts())
    {
        if (Floater.Age < 0) { continue; }
        FVector2D Point;
        if (!Project(View->BoardPosition(Floater.Cell, View->Metric().Scaled(105.0f + Floater.Age * 55.0f)), Point)) { continue; }
        const float Life = Floater.Age / Floater.Life;
        const float Alpha = Life < 0.7f ? 1.0f : 1.0f - (Life - 0.7f) / 0.3f;
        const float Pop = 1.0f + 0.6f * FMath::Max(0.0f, 1.0f - Floater.Age / 0.14f);
        Text(Floater.Text, Point.X, Point.Y - 30, WithAlpha(Floater.Color, Alpha), 1.05f * Floater.Scale * Pop, EAlign::Center);
    }
}

// ---------------------------------------------------------------------------------------
// Persistent frame: location, initiative, system buttons, party, log.

void AExpeditionHUD::DrawTargetChips()
{
    if (!Player->ShowsTargeting()) { return; }
    const auto& Rules = View->Rules();
    std::vector<ExpeditionLayout::Rect> Occupied;
    for (const auto& Box : ChipExclusions)
    { Occupied.push_back({Box.Min.X, Box.Min.Y, Box.Max.X, Box.Max.Y}); }
    struct FDrawChip { ExpeditionLayout::Chip Geometry; FString Label; float Width; };
    TArray<FDrawChip> Chips;
    const ExpeditionLayout::Rect Bounds = ExpeditionLayout::ChipBounds(W, H);
    constexpr float LabelScale = 0.72f;
    // Text() shrinks a label that exceeds its panel, so fit the name here and keep the forecast at full size.
    const float LabelFit = float(ExpeditionLayout::ChipMaxWidth - ExpeditionLayout::ChipTextPadding);
    auto MeasureLabel = [this](const FString& Value) { return Measure(Value, LabelScale).X; };
    int32 Overflow = 0;
    for (const int32 Id : Player->Targets())
    {
        ++ChipsRequested;
        FVector2D Anchor;
        if (!Project(View->UnitPosition(Id, View->Metric().Figure(1.3f)), Anchor)) { ++Overflow; continue; }
        const FCombatantState* Unit = Rules.Find(Id);
        const FString Label = ExpeditionStyle::ChipLabel(Unit ? Unit->Name : FString(),
            ExpeditionStyle::TargetChip(Rules, Player->Actor(), Player->Action(), Id), LabelFit, MeasureLabel);
        const float Width = FMath::Min(float(ExpeditionLayout::ChipMaxWidth), Measure(Label, LabelScale).X + float(ExpeditionLayout::ChipTextPadding));
        ExpeditionLayout::Chip Chip;
        if (!ExpeditionLayout::PlaceChip({Anchor.X, Anchor.Y}, Width, ExpeditionLayout::ChipHeight, Bounds, Occupied, Chip)) { ++Overflow; continue; }
        Occupied.push_back(Chip.Panel);
        Chips.Add({Chip, Label, Width});
    }
    // Every leader sees every chip, including ones drawn later. Draw all clipped
    // segments before chip text so connector order cannot obscure a label.
    ExpeditionLayout::ExcludeHUDFrame({0, 0, W, H}, Bounds, Occupied);
    for (const auto& Draw : Chips)
    {
        const auto& Chip = Draw.Geometry;
        const ExpeditionLayout::Point Edge = {std::clamp(Chip.Anchor.X, Chip.Panel.Left, Chip.Panel.Right),
            std::clamp(Chip.Anchor.Y, Chip.Panel.Top, Chip.Panel.Bottom)};
        for (const auto& Segment : ExpeditionLayout::VisibleLeader(Chip.Anchor, Edge, Occupied))
        { Line(FVector2D(Segment.From.X, Segment.From.Y), FVector2D(Segment.To.X, Segment.To.Y), Gold, 1); }
    }
    for (const auto& Draw : Chips)
    {
        const auto& PanelBounds = Draw.Geometry.Panel;
        Panel(PanelBounds.Left, PanelBounds.Top, Draw.Width, ExpeditionLayout::ChipHeight, Gold, 0.96f, false);
        Text(Draw.Label, PanelBounds.Left + Draw.Width / 2, PanelBounds.Top + 3, Parchment, LabelScale, EAlign::Center, Draw.Width - 12);
    }
    if (Overflow > 0)
    { Text(FString::Printf(TEXT("%d more targets: use arrows to inspect"), Overflow), W * 0.5f, H - 116, Gold, 0.7f, EAlign::Center); }
}

void AExpeditionHUD::DrawTopBar()
{
    if (Player->ShowingBuilder() || (View->HasRun() && (View->Progress().Phase == EExpeditionPhase::Victory || View->Progress().Phase == EExpeditionPhase::Defeat))) { return; }
    constexpr float Top = 8, StripHeight = 38, ButtonHeight = 28;
    if (View->HasRun())
    {
        // One strip: where we are, the round and seed, and (in combat) the initiative order.
        const FExpeditionProgress& Run = View->Progress();
        const float Width = StatusStripWidth();
        Panel(8, Top, Width, StripHeight, Gold, 0.86f);
        Persist(8, Top, Width, StripHeight);
        Text(View->LocationName(), 18, Top + 4, Parchment, 0.82f, EAlign::Left, 170);
        const FString Status = View->InCombat() ? FString::Printf(TEXT("Enc %d of %d  ·  Round %d"), Run.History.Num(), FExpeditionRun::EncounterCount, View->Rules().Snapshot().Round) : PhaseLine(Run);
        Text(FString::Printf(TEXT("%s  ·  Seed %d"), *Status, Run.Seed), 18, Top + 22, Muted, 0.54f, EAlign::Left, View->InCombat() ? 170.0f : 240.0f);
        if (View->InCombat()) { DrawInitiative(); }
    }
    // One Menu button holds the secondary controls (#146). Its panel lists them while it is open.
    const bool bWaiting = Player->ReturnPending();
    const float MenuWidth = bWaiting ? 112.0f : 76.0f;
    const FString Label = bWaiting ? TEXT("I  Menu: waiting") : TEXT("I  Menu");
    const float Total = MenuWidth + 12.0f;
    Panel(W - 8 - Total, Top, Total, StripHeight, Sky, 0.86f);
    Persist(W - 8 - Total, Top, Total, StripHeight);
    Button(TEXT("MenuToggle"), Label, FString(), W - 8 - 6 - MenuWidth, Top + (StripHeight - ButtonHeight) * 0.5f, MenuWidth, ButtonHeight, true, bWaiting || Player->ShowingMenu(), Sky, 0.8f);
}

void AExpeditionHUD::DrawMenuPanel()
{
    // The open Menu is a transient layer: a light scrim, the toggle (to close) and one row per control.
    // Every drawn button is registered with its drawn rectangle; the controls beneath are not live.
    constexpr float Top = 8, StripHeight = 38, ButtonHeight = 28, RowHeight = 34, RowGap = 6, PanelWidth = 232, Pad = 12;
    Rect(FLinearColor(0, 0, 0, 0.35f), 0, 0, W, H);
    bTransientButtons = true;
    const bool bWaiting = Player->ReturnPending();
    const float ToggleWidth = bWaiting ? 112.0f : 76.0f;
    Button(TEXT("MenuToggle"), bWaiting ? TEXT("I  Menu: waiting") : TEXT("I  Menu"), FString(), W - 8 - 6 - ToggleWidth, Top + (StripHeight - ButtonHeight) * 0.5f, ToggleWidth, ButtonHeight, true, true, Sky, 0.8f);
    struct FRow { FName Id; FString Label; bool bEnabled; };
    const FString Speed = View->Speed() == EPlaybackSpeed::Normal ? TEXT("1x") : View->Speed() == EPlaybackSpeed::Fast ? TEXT("2x") : TEXT("Instant");
    TArray<FRow> Rows;
    Rows.Add({TEXT("Save"), TEXT("K  Save"), View->HasRun()});
    Rows.Add({TEXT("Resume"), TEXT("O  Resume"), true});
    Rows.Add({TEXT("Records"), TEXT("H  Records"), true});
    Rows.Add({TEXT("Speed"), TEXT("T  Speed ") + Speed, true});
    Rows.Add({TEXT("Help"), TEXT("?  Help"), true});
    Rows.Add({TEXT("Menu"), bWaiting ? TEXT("Bksp  Return: waiting") : TEXT("Bksp  Return to start"), View->CanReturnToMenu()});
    const float Height = Pad * 2 + Rows.Num() * RowHeight + (Rows.Num() - 1) * RowGap;
    const float X = W - 8 - PanelWidth, Y = Top + StripHeight + 6;
    Panel(X, Y, PanelWidth, Height, Sky, 0.98f);
    float RowY = Y + Pad;
    for (const FRow& Row : Rows)
    {
        Button(Row.Id, Row.Label, FString(), X + Pad, RowY, PanelWidth - Pad * 2, RowHeight, Row.bEnabled, false, Sky, 0.8f);
        RowY += RowHeight + RowGap;
    }
    bTransientButtons = false;
}

float AExpeditionHUD::StatusStripWidth() const
{
    if (!View->InCombat()) { return 270.0f; }
    return 188.0f + FMath::Max(1, View->Rules().Snapshot().TurnOrder.Num()) * 26.0f + 10.0f;
}

void AExpeditionHUD::DrawInitiative()
{
    const FCombatBattle& Rules = View->Rules();
    const FCombatSnapshot& Snapshot = Rules.Snapshot();
    const int32 Count = Snapshot.TurnOrder.Num();
    if (Count == 0) { return; }
    constexpr float Spacing = 26.0f, Top = 8, StripHeight = 38;
    const float Left = 8.0f + 188.0f;
    Rect(WithAlpha(Gold, 0.25f), Left - 6, Top + 6, 1, StripHeight - 12);
    // While results play, the strip follows whoever is shown acting, not the committed turn.
    const int32 Focus = View->Playback().FocusId();
    const int32 Active = View->IsPresenting() && Focus != INDEX_NONE ? Focus : Rules.ActiveId();
    for (int32 Index = 0; Index < Count; ++Index)
    {
        const FCombatantState* Unit = Rules.Find(Snapshot.TurnOrder[Index]);
        if (!Unit) { continue; }
        const FUnitDisplay* Display = View->Playback().Unit(Unit->Id);
        const ECombatLife Life = Display ? Display->Life : Unit->Life;
        const FVector2D Center(Left + 12 + Index * Spacing, Top + StripHeight * 0.5f);
        const bool bActive = Unit->Id == Active;
        const float Radius = bActive ? 13.0f : 10.5f;
        const FLinearColor Color = Life == ECombatLife::Dead ? FLinearColor(0.3f, 0.3f, 0.32f) : ExpeditionStyle::UnitColor(*Unit);
        if (bActive) { Circle(Center, Radius + 2 + FMath::Sin(Clock * 5.0f) * 0.8f, Gold); }
        Circle(Center, Radius, Color * 0.5f);
        Circle(Center, Radius - 2.0f, Color * (Unit->Side == ECombatSide::Party ? 0.85f : 0.65f));
        if (Life == ECombatLife::Downed) { Circle(Center, Radius - 2.0f, FLinearColor(1.0f, 0.55f, 0.2f, 0.55f)); }
        Text(Initials(*Unit), Center.X, Center.Y - 7, Ink, 0.6f, EAlign::Center);
        if (Life == ECombatLife::Dead)
        {
            Line(Center - FVector2D(7, 7), Center + FVector2D(7, 7), Danger, 2.0f);
            Line(Center + FVector2D(-7, 7), Center + FVector2D(7, -7), Danger, 2.0f);
        }
        if (FVector2D::Distance(Mouse, Center) < Radius + 1)
        {
            Panel(Center.X - 80, Top + StripHeight + 6, 160, 26, Gold, 0.95f, false);
            Text(FString::Printf(TEXT("%s  ·  initiative %d"), *Unit->Name, Unit->Initiative), Center.X, Top + StripHeight + 11, Parchment, 0.7f, EAlign::Center, 150);
        }
    }
}

namespace
{
constexpr float PartyCardX = 8, PartyCardTop = 54, PartyCardWidth = 200, PartyCardHeight = 64, PartyCardPitch = 68;
}

void AExpeditionHUD::DrawParty()
{
    int32 Index = 0;
    for (const FCombatantState& Unit : View->Rules().Snapshot().Encounter.Combatants)
    {
        if (Unit.Side != ECombatSide::Party) { continue; }
        DrawPartyCard(Unit, Index, PartyCardTop + Index * PartyCardPitch);
        ++Index;
    }
}

void AExpeditionHUD::DrawPartyCard(const FCombatantState& Unit, int32 Index, float Y)
{
    const FUnitDisplay* Display = View->Playback().Unit(Unit.Id);
    const ECombatLife Life = Display ? Display->Life : Unit.Life;
    const bool bActive = View->InCombat() && View->Rules().ActiveId() == Unit.Id;
    const bool bSelected = Player->Selection() == Unit.Id;
    const FLinearColor Color = ExpeditionStyle::UnitColor(Unit);
    const float X = PartyCardX, Width = PartyCardWidth, Height = PartyCardHeight;
    const bool bHover = Hovered(X, Y, Width, Height);
    Panel(X, Y, Width, Height, bActive ? Gold : Color, bSelected || bHover ? 0.97f : 0.86f);
    Persist(X, Y, Width, Height);
    if (bActive) { Frame(WithAlpha(Gold, 0.6f + 0.3f * FMath::Sin(Clock * 4.0f)), X, Y, Width, Height, 2.0f); }
    else if (bSelected) { Frame(FLinearColor(0.9f, 0.95f, 1.0f, 0.7f), X, Y, Width, Height, 1.5f); }
    if (Player->MenuFocus() == FName(*FString::Printf(TEXT("Party%d"), Index))) { Frame(Parchment, X - 3, Y - 3, Width + 6, Height + 6, 2); }
    Rect(WithAlpha(Color, Life == ECombatLife::Dead ? 0.25f : 0.9f), X, Y, 4, Height);
    // Role emblem: a medallion with the class initial.
    const FVector2D Portrait(X + 26, Y + 35);
    Circle(Portrait, 16, Life == ECombatLife::Dead ? FLinearColor(0.2f, 0.2f, 0.22f) : Color * 0.45f);
    Circle(Portrait, 13.5f, Life == ECombatLife::Dead ? FLinearColor(0.32f, 0.32f, 0.34f) : Color * 0.9f);
    Text(Unit.Role.Left(1), Portrait.X, Portrait.Y - 9, Ink, 0.85f, EAlign::Center);
    Text(FString::FromInt(Index + 1), X + 9, Y + 4, Muted, 0.5f);
    Text(Unit.Name, X + 48, Y + 4, Life == ECombatLife::Dead ? Muted : Color, 0.8f, EAlign::Left, 100);
    Text(FString::Printf(TEXT("Lv%d"), Unit.Level), X + Width - 8, Y + 6, Muted, 0.52f, EAlign::Right);
    if (Life == ECombatLife::Dead)
    {
        Text(TEXT("Fallen  ·  lost"), X + 48, Y + 21, FLinearColor(0.75f, 0.45f, 0.4f), 0.62f, EAlign::Left, 144);
    }
    else if (Life == ECombatLife::Downed)
    {
        Text(FString::Printf(TEXT("DOWNED  ·  %d turns to rescue"), Unit.RescueTurnsLeft), X + 48, Y + 21, FLinearColor(1.0f, 0.6f, 0.3f), 0.62f, EAlign::Left, 144);
    }
    else
    {
        const float Shown = Display ? Display->ShownHP : Unit.HP;
        const float Trail = Display ? Display->TrailHP : Unit.HP;
        const int32 HP = Display ? Display->HP : Unit.HP;
        Bar(X + 48, Y + 23, 100, 8, Shown / FMath::Max(1, Unit.MaxHP), Trail / FMath::Max(1, Unit.MaxHP), HealthColor(Shown / FMath::Max(1, Unit.MaxHP)));
        Text(FString::Printf(TEXT("%d/%d"), HP, Unit.MaxHP), X + Width - 8, Y + 20, Parchment, 0.62f, EAlign::Right);
    }
    // Resources as pips: the things that actually run out. Full wording shows in the hover detail.
    const auto* Build = FBuildCatalog::Find(Unit.BuildId);
    float PipX = X + 48;
    const float PipY = Y + 37;
    auto Group = [&](const TCHAR* Tag, int32 Have, int32 Cap, FLinearColor Tint)
    {
        if (Cap <= 0) { return; }
        Text(Tag, PipX, PipY - 5, Tint, 0.5f);
        PipX += 9;
        if (Cap > 6)
        {
            const FString Count = FString::Printf(TEXT("%d/%d"), Have, Cap);
            Text(Count, PipX, PipY - 5, Parchment, 0.5f);
            PipX += Measure(Count, 0.5f).X + 6;
            return;
        }
        for (int32 Pip = 0; Pip < Cap; ++Pip) { Circle(FVector2D(PipX + 3 + Pip * 7.5f, PipY + 1), 2.8f, Pip < Have ? Tint : FLinearColor(0.3f, 0.3f, 0.33f)); }
        PipX += Cap * 7.5f + 5;
    };
    if (Build && Life != ECombatLife::Dead)
    {
        Group(TEXT("K"), Unit.KitUses, Build->KitCap, Gold);
        Group(TEXT("W"), Unit.SecondWindUses, Build->WindCap, Safe);
        Group(TEXT("S"), Unit.SpellSlots, Build->SlotCap, Sky);
        Group(TEXT("M"), Unit.ManeuverUses, Build->ManeuverCap, Gold);
        Group(TEXT("R"), Unit.RageUses, Build->RageCap, Danger);
    }
    FString Gear = FString::Printf(TEXT("Potion %d  ·  Gear +%d"), Unit.Potions, Unit.WeaponRank);
    if (Unit.RageTurns > 0) { Gear += TEXT("  ·  Raging"); }
    if (Unit.GuardTargetId != INDEX_NONE) { Gear += TEXT("  ·  Guarding"); }
    Text(Gear, X + 48, Y + 50, Muted, 0.5f, EAlign::Left, Width - 54);
    Buttons.Add({FName(*FString::Printf(TEXT("Party%d"), Index)), FBox2D(FVector2D(X, Y) * S, FVector2D(X + Width, Y + Height) * S), true});
    // Full status prose on hover, on selecting a hero who is not the one acting, or when the player
    // inspects the acting hero by choosing them again (number key or card), so it is keyboard reachable.
    if (bHover || (bSelected && (!bActive || Player->Inspected() == Unit.Id))) { DrawPartyDetail(Unit, X + Width + 8, Y); }
}

void AExpeditionHUD::DrawPartyDetail(const FCombatantState& Unit, float X, float Y)
{
    const float Width = 320;
    DetailUnitId = Unit.Id;
    TArray<TPair<FString, FLinearColor>> Lines;
    Lines.Add({FString::Printf(TEXT("%s  ·  Lv%d %s"), *Unit.Name, Unit.Level, *FBuildCatalog::DisplayName(Unit)), ExpeditionStyle::UnitColor(Unit)});
    if (Unit.Life == ECombatLife::Dead) { Lines.Add({TEXT("Fallen  ·  lost for this expedition"), FLinearColor(0.75f, 0.45f, 0.4f)}); }
    else if (Unit.Life == ECombatLife::Downed) { Lines.Add({FString::Printf(TEXT("DOWNED  ·  %d turns to rescue"), Unit.RescueTurnsLeft), FLinearColor(1.0f, 0.6f, 0.3f)}); }
    else { Lines.Add({FString::Printf(TEXT("HP %d/%d  ·  AC %d  ·  reach %d"), Unit.HP, Unit.MaxHP, Unit.ArmorClass, Unit.Range), Parchment}); }
    TArray<FString> Parts;
    FBuildCatalog::Status(Unit).ParseIntoArray(Parts, TEXT(" / "));
    FString Resources;
    for (const auto& Part : Parts)
    {
        if (Part.StartsWith(TEXT("Protection")))
        {
            const auto* Target = View->Rules().Find(Unit.GuardTargetId);
            const FString Label = Target ? FString::Printf(TEXT("%s [%d]"), *Target->Name, Target->Id) : TEXT("unavailable target");
            Lines.Add({FString::Printf(TEXT("Protect %s: -%d damage until next own turn"), *Label, Unit.GuardAmount > 0 ? Unit.GuardAmount : 3), Sky});
        }
        else { Resources += (Resources.IsEmpty() ? TEXT("") : TEXT(" / ")) + Part; }
    }
    Resources += FString::Printf(TEXT("%sPotion %d / Gear +%d"), Resources.IsEmpty() ? TEXT("") : TEXT(" / "), Unit.Potions, Unit.WeaponRank);
    Lines.Add({Resources, Parchment});
    TArray<TPair<FString, FLinearColor>> Wrapped;
    for (const auto& Entry : Lines) { for (const auto& Line : Wrap(Entry.Key, Width - 24, 0.68f)) { Wrapped.Add({Line, Entry.Value}); } }
    const float Height = 14 + Wrapped.Num() * 17.0f;
    X = FMath::Min(X, W - Width - 8);
    Y = FMath::Min(Y, H - Height - 90);
    Panel(X, Y, Width, Height, Wrapped[0].Value, 0.97f, false);
    for (int32 Index = 0; Index < Wrapped.Num(); ++Index) { Text(Wrapped[Index].Key, X + 12, Y + 7 + Index * 17.0f, Wrapped[Index].Value, 0.68f); }
}

void AExpeditionHUD::DrawLog()
{
    const TArray<FString>& Messages = View->Messages();
    if (Messages.IsEmpty()) { return; }
    // The log fades after LingerSeconds without a new entry and returns on a new entry or hover.
    constexpr float LingerSeconds = 4.0f, FadeSeconds = 0.6f;
    const FString& Last = Messages.Last();
    if (Messages.Num() != LogCount || Last != LogLast) { LogCount = Messages.Num(); LogLast = Last; LogChangedAt = Clock; }
    TArray<FString> Lines;
    for (int32 Index = FMath::Max(0, Messages.Num() - 2); Index < Messages.Num(); ++Index)
    { Lines.Append(Wrap(Messages[Index], 260, 0.66f)); }
    if (Lines.Num() > 4) { Lines.RemoveAt(0, Lines.Num() - 4); }
    const float Height = 14 + Lines.Num() * 16.0f;
    constexpr float Width = 280, X = 8;
    const float Top = H - 8 - Height;
    const bool bHover = Hovered(X, Top, Width, Height) || Hovered(X, H - 26, 56, 18);
    // A capture never fades the log, so a gallery shot always shows it.
    const float Idle = Clock - LogChangedAt - LingerSeconds;
    const float Alpha = Player->IsCapturing() || bHover ? 1.0f : FMath::Clamp(1.0f - Idle / FadeSeconds, 0.0f, 1.0f);
    if (Alpha <= 0.01f)
    {
        // A small tab keeps the log discoverable; hovering it brings the log back.
        Rect(FLinearColor(Ink.R, Ink.G, Ink.B, 0.6f), X, H - 26, 56, 18);
        Text(TEXT("Log"), X + 28, H - 24, Muted, 0.6f, EAlign::Center);
        Persist(X, H - 26, 56, 18);
        return;
    }
    Panel(X, Top, Width, Height, WithAlpha(Muted, Alpha), 0.86f * Alpha, false);
    Persist(X, Top, Width, Height);
    for (int32 Index = 0; Index < Lines.Num(); ++Index)
    { Text(Lines[Index], X + 10, Top + 7 + Index * 16.0f, WithAlpha(Parchment, Alpha), 0.66f); }
}

// ---------------------------------------------------------------------------------------
// Combat controls

namespace
{
// Compact action dock: a thin bezel for the turn's budgets above one row of buttons.
// Room is kept in each button's lower band for #58's cost/usage tags.
constexpr float DockButtonWidth = 104, DockButtonHeight = 42, DockGap = 4, DockBezel = 20, DockPad = 8, DockFoot = 4, DockMargin = 8;
}

void AExpeditionHUD::DrawActionBar()
{
    const FCombatBattle& Rules = View->Rules();
    const FCombatSnapshot& Snapshot = Rules.Snapshot();
    const FCombatantState* Active = Rules.Find(Rules.ActiveId());
    const float DockHeight = DockBezel + DockButtonHeight + DockFoot;
    const float DockY = H - DockMargin - DockHeight;
    if (View->IsPresenting())
    {
        Button(TEXT("Skip"), TEXT("Skip  >>"), TEXT("Tab"), W * 0.5f - 52, H - DockMargin - DockButtonHeight, 104, DockButtonHeight, true, false, Sky);
        return;
    }
    if (!Active || Snapshot.Phase != EBattlePhase::Fighting) { return; }
    if (Active->Side == ECombatSide::Enemy)
    {
        Panel(W * 0.5f - 130, H - DockMargin - 34, 260, 34, Danger, 0.85f);
        Persist(W * 0.5f - 130, H - DockMargin - 34, 260, 34);
        const int32 Dots = 1 + static_cast<int32>(Clock * 3.0f) % 3;
        Text(FString::Printf(TEXT("%s is deciding%s"), *Active->Name, *FString::ChrN(Dots, TEXT('.'))), W * 0.5f, H - DockMargin - 26, Danger, 0.85f, EAlign::Center, 244);
        return;
    }
    const int32 Id = Active->Id;
    const auto Abilities = FBuildCatalog::Abilities(*Active);
    const int32 Count = 4 + Abilities.Num();
    const float RowWidth = Count * (DockButtonWidth + DockGap) - DockGap;
    const float DockWidth = RowWidth + 2 * DockPad;
    const float DockX = W * 0.5f - DockWidth * 0.5f;
    Panel(DockX, DockY, DockWidth, DockHeight, ExpeditionStyle::UnitColor(*Active), 0.9f);
    Persist(DockX, DockY, DockWidth, DockHeight);
    // Thin top bezel: the turn's budget chips on the left, whose turn it is on the right.
    auto Chip = [&](const FString& Label, bool bReady, float X)
    {
        Circle(FVector2D(X, DockY + 11), 4, bReady ? Safe : FLinearColor(0.35f, 0.35f, 0.38f));
        Text(Label, X + 9, DockY + 5, bReady ? Parchment : Muted, 0.62f);
    };
    Chip(FString::Printf(TEXT("Move %d"), Snapshot.MovementLeft), Snapshot.MovementLeft > 0, DockX + 16);
    Chip(TEXT("Action"), Snapshot.bActionAvailable, DockX + 90);
    Chip(TEXT("Bonus action"), Snapshot.bBonusActionAvailable, DockX + 152);
    Text(FString::Printf(TEXT("%s's turn"), *Active->Name), DockX + DockWidth - 12, DockY + 5, ExpeditionStyle::UnitColor(*Active), 0.66f, EAlign::Right, 150);
    if (const auto* Kit = FBuildCatalog::Find(Active->BuildId); Kit && Kit->SneakDie)
    { Text(TEXT("Sneak Attack: with adjacent ally"), DockX + (250 + DockWidth - 100) * 0.5f, DockY + 5, Safe, 0.6f, EAlign::Center, DockWidth - 350); }

    const float Y = DockY + DockBezel;
    const float Left = DockX + DockPad;
    const float Step = DockButtonWidth + DockGap;
    const ECombatCommandType Mode = Player->Action();
    auto Ready = [&](ECombatCommandType Action) { return !Rules.LegalTargets(Id, Action).IsEmpty(); };
    auto Dock = [&](FName ButtonId, const FString& Label, const FString& Sub, int32 Column, bool bEnabled, bool bSelected, FLinearColor Accent)
    { return Button(ButtonId, Label, Sub, Left + Column * Step, Y, DockButtonWidth, DockButtonHeight, bEnabled, bSelected, Accent, 0.78f); };
    TArray<TPair<ECombatCommandType, bool>> Hover;
    Hover.Add({ECombatCommandType::Attack, Dock(TEXT("ModeAttack"), TEXT("Attack / Move"), FString::Printf(TEXT("A / d%d%+d"), Active->DamageDie, FBuildCatalog::AttackModifier(*Active)), 0,
        Ready(ECombatCommandType::Attack) || !Rules.LegalMoves(Id).IsEmpty(), Mode == ECombatCommandType::Attack, FLinearColor(0.95f, 0.78f, 0.4f))});
    int32 Column = 1;
    for (const auto& Ability : Abilities)
    {
        const FString Cost = FString::Printf(TEXT("%s %s / %d %s%s"), Column == 1 ? TEXT("F") : TEXT("M"), Ability.bBonus ? TEXT("Bonus") : TEXT("Action"), Ability.ResourceCost, *Ability.ResourceName, Ability.bResourceAvailable ? TEXT("") : TEXT(" none"));
        Hover.Add({Ability.Command, Dock(Column == 1 ? TEXT("ModeRole") : TEXT("Maneuver"), Ability.Name, Cost, Column, Ready(Ability.Command), Mode == Ability.Command, Sky)});
        ++Column;
    }
    Hover.Add({ECombatCommandType::Rescue, Dock(TEXT("ModeRescue"), TEXT("Rescue"), TEXT("R / adjacent ally"), Column++, Ready(ECombatCommandType::Rescue), Mode == ECombatCommandType::Rescue, Safe)});
    Hover.Add({ECombatCommandType::Potion, Dock(TEXT("ModePotion"), TEXT("Potion"), FString::Printf(TEXT("P / %d left"), Active->Potions), Column++, Ready(ECombatCommandType::Potion), Mode == ECombatCommandType::Potion, Safe)});
    Dock(TEXT("EndTurn"), TEXT("End Turn"), TEXT("Space"), Column, true, false, Gold);
    for (const auto& Pair : Hover)
    {
        if (!Pair.Value) { continue; }
        FString Help = FCombatBattle::DescribeAction(Pair.Key);
        for (const auto& A : Abilities) { if (A.Command == Pair.Key) { Help = A.Name + TEXT(": ") + A.Description; } }
        const float TipWidth = FMath::Min(640.0f, Measure(Help, 0.72f).X + 24);
        Panel(W * 0.5f - TipWidth * 0.5f, DockY - 36, TipWidth, 30, Sky, 0.95f, false);
        Text(Help, W * 0.5f, DockY - 30, Parchment, 0.72f, EAlign::Center, TipWidth - 16);
    }
}


void AExpeditionHUD::DrawForecast()
{
    FVector2D Anchor = Mouse;
    if (Player->UsingKeyboardCursor())
    {
        const FIntPoint Cell = Player->KeyboardCursorCell();
        if (!Project(View->BoardPosition(FVector2D(Cell.X, Cell.Y), 0.0f), Anchor)) { return; }
        Anchor += FVector2D(30, 10);
    }
    if (View->IsPresenting() || Anchor.X < 0) { return; }
    const FCombatBattle& Rules = View->Rules();
    TArray<TPair<FString, FLinearColor>> Lines;
    const int32 TargetId = Player->HoveredTarget();
    if (TargetId != INDEX_NONE)
    {
        const FCombatantState* Target = Rules.Find(TargetId);
        const FActionForecast Forecast = Rules.Forecast(Player->Actor(), Player->Action(), TargetId);
        if (!Target || !Forecast.bValid) { return; }
        Lines.Add({FString::Printf(TEXT("%s  ·  AC %d  ·  HP %d/%d"), *Target->Name, Target->ArmorClass, Target->HP, Target->MaxHP), ExpeditionStyle::UnitColor(*Target)});
        if (Forecast.bAttackRoll)
        {
            const FLinearColor Chance = Forecast.HitChancePercent >= 65 ? Safe : Forecast.HitChancePercent >= 40 ? Gold : Danger;
            Lines.Add({FString::Printf(TEXT("%d%% to hit"), Forecast.HitChancePercent), Chance});
        }
        else if (!Forecast.bHealing && Forecast.MaxAmount > 0) { Lines.Add({TEXT("Never misses"), Safe}); }
        Lines.Add({ExpeditionStyle::ForecastAmount(Forecast), Parchment});
        if (Forecast.bSneakAttack) { Lines.Add({TEXT("Sneak Attack: an ally is beside them"), Safe}); }
        if (Forecast.bCanDrop) { Lines.Add({Forecast.MaxAmount >= Target->HP ? TEXT("A strong hit can drop them") : TEXT("A critical hit can drop them"), Danger}); }
        if (Forecast.bAttackRoll) { const auto* Actor = Rules.Find(Player->Actor()); const auto* Build = Actor ? FBuildCatalog::Find(Actor->BuildId) : nullptr; Lines.Add({FString::Printf(TEXT("Critical %d-20 doubles damage dice"), Build ? Build->CriticalThreshold : 20), Muted}); }
    }
    else if (!Player->PreviewPath().IsEmpty())
    {
        const int32 Steps = Player->PreviewPath().Num();
        Lines.Add({FString::Printf(TEXT("Move %d square%s  ·  %d left after"), Steps, Steps == 1 ? TEXT("") : TEXT("s"), Rules.Snapshot().MovementLeft - Steps), Sky});
    }
    else if (const FCombatantState* Unit = Rules.Find(Player->HoveredUnit()))
    {
        if (Unit->Life == ECombatLife::Dead) { return; }
        Lines.Add({FString::Printf(TEXT("%s  ·  %s"), *Unit->Name, *Unit->Role), ExpeditionStyle::UnitColor(*Unit)});
        Lines.Add({FString::Printf(TEXT("HP %d/%d  ·  AC %d  ·  reach %d"), Unit->HP, Unit->MaxHP, Unit->ArmorClass, Unit->Range), Parchment});
        if (Unit->Side == ECombatSide::Enemy && Player->Actor() == Rules.ActiveId()) { Lines.Add({TEXT("Not a legal target from here"), Muted}); }
    }
    if (Lines.IsEmpty()) { return; }
    TArray<TPair<FString, FLinearColor>> Wrapped;
    for (const auto& Entry : Lines) { for (const auto& Line : Wrap(Entry.Key, 376, 0.74f)) { Wrapped.Add({Line, Entry.Value}); } }
    Lines = MoveTemp(Wrapped);
    const float Width = 400;
    const float Height = 12 + Lines.Num() * 20.0f;
    const float X = FMath::Min(Anchor.X + 20, W - Width - 8);
    const float Y = FMath::Min(Anchor.Y + 18, H - Height - 100);
    Panel(X, Y, Width, Height, Lines[0].Value, 0.96f, false);
    for (int32 Index = 0; Index < Lines.Num(); ++Index) { Text(Lines[Index].Key, X + 12, Y + 6 + Index * 20.0f, Lines[Index].Value, 0.74f); }
}

void AExpeditionHUD::DrawDice()
{
    const FDiceTray& Tray = View->Playback().Tray();
    if (!Tray.bVisible || Tray.Dice.IsEmpty()) { return; }
    const float End = FDiceTray::TumbleSeconds + FDiceTray::LingerSeconds;
    const float Alpha = FMath::Clamp(1.0f - (Tray.Age - End) / 0.4f, 0.0f, 1.0f) * FMath::Clamp(Tray.Age / 0.12f, 0.0f, 1.0f);
    if (Alpha <= 0.01f) { return; }
    const float Width = 440, Height = 128;
    const float X = W - 12 - Width, Y = 54;
    ChipExclusions.Add(FBox2D(FVector2D(X, Y), FVector2D(X + Width, Y + Height)));
    Rect(FLinearColor(0, 0, 0, 0.25f * Alpha), X + 3, Y + 4, Width, Height);
    Rect(FLinearColor(Ink.R, Ink.G, Ink.B, 0.86f * Alpha), X, Y, Width, Height);
    Rect(WithAlpha(Gold, 0.7f * Alpha), X, Y, Width, 2);
    const float Middle = X + Width * 0.5f;
    Text(Tray.Title, Middle, Y + 8, WithAlpha(Gold, Alpha), 0.82f, EAlign::Center, Width - 20);
    // Dice row.
    float RowWidth = 0;
    for (const FShownDie& Shown : Tray.Dice) { RowWidth += Shown.Sides == 20 ? 74 : 54; }
    float Cursor = Middle - RowWidth * 0.5f - (Tray.Verdict.IsEmpty() ? 0 : 60);
    bool bAllLanded = true;
    for (int32 Index = 0; Index < Tray.Dice.Num(); ++Index)
    {
        const FShownDie& Shown = Tray.Dice[Index];
        const float Radius = Shown.Sides == 20 ? 30.0f : 21.0f;
        const float Slot = Shown.Sides == 20 ? 74 : 54;
        const float Time = Tray.Age - Shown.Delay;
        Cursor += Slot;
        if (Time < 0) { bAllLanded = false; continue; }
        const float Progress = FMath::Clamp(Time / FDiceTray::TumbleSeconds, 0.0f, 1.0f);
        const bool bLanded = Time >= FDiceTray::TumbleSeconds;
        bAllLanded &= bLanded;
        const float Ease = (1.0f - Progress) * (1.0f - Progress);
        const FVector2D Center(Cursor - Slot * 0.5f - 90.0f * Ease, Y + 64 - FMath::Abs(FMath::Sin(Progress * PI * 2.5f)) * 34.0f * (1.0f - Progress));
        const int32 Value = bLanded ? Shown.Value : 1 + Scramble(static_cast<uint32>(Time * 22.0f) * 31u + Index * 977u) % FMath::Max(1, Shown.Sides);
        const float Punch = bLanded ? 1.0f + 0.3f * FMath::Exp(-(Time - FDiceTray::TumbleSeconds) * 9.0f) : 1.0f;
        FLinearColor Color = Shown.Color;
        if (bLanded && Shown.Sides == 20 && Shown.Value == 20) { Circle(Center, Radius * 1.45f, WithAlpha(Gold, (0.35f + 0.15f * FMath::Sin(Clock * 8.0f)) * Alpha)); Color = FLinearColor(1.0f, 0.86f, 0.45f); }
        if (bLanded && Shown.Sides == 20 && Shown.Value == 1) { Color = FLinearColor(0.75f, 0.35f, 0.32f); }
        Die(Center, Radius * Punch, Shown.Sides, Value, Color, Ease * 9.0f, Alpha, bLanded);
    }
    if (!Tray.Verdict.IsEmpty() && bAllLanded)
    {
        const float Pop = 1.0f + 0.25f * FMath::Exp(-Tray.Age * 2.0f);
        Text(Tray.Verdict, X + Width - 18, Y + 46, WithAlpha(Tray.VerdictColor, Alpha), 1.3f * Pop, EAlign::Right, 150);
    }
    Text(Tray.Detail, Middle, Y + Height - 26, WithAlpha(Parchment, Alpha), 0.74f, EAlign::Center, Width - 24);
}

void AExpeditionHUD::DrawBanner()
{
    const FTurnBanner& Banner = View->Playback().Banner();
    if (Banner.Title.IsEmpty() || Banner.Age > Banner.Life) { return; }
    const float In = FMath::Clamp(Banner.Age / 0.2f, 0.0f, 1.0f);
    const float Out = FMath::Clamp((Banner.Life - Banner.Age) / 0.4f, 0.0f, 1.0f);
    const float Alpha = FMath::Min(In, Out);
    const float Slide = (1.0f - In) * -60.0f + (1.0f - Out) * 60.0f;
    const float Y = H * 0.24f;
    ChipExclusions.Add(FBox2D(FVector2D(0, Y), FVector2D(W, Y + 82)));
    Rect(FLinearColor(0, 0, 0, 0.55f * Alpha), 0, Y, W, 82);
    Rect(WithAlpha(Banner.Color, 0.85f * Alpha), W * 0.5f - 220 + Slide, Y, 440, 2);
    Rect(WithAlpha(Banner.Color, 0.85f * Alpha), W * 0.5f - 220 - Slide, Y + 80, 440, 2);
    Text(Banner.Title, W * 0.5f + Slide, Y + 10, WithAlpha(Banner.Color, Alpha), 2.0f, EAlign::Center, W - 80);
    Text(Banner.Subtitle, W * 0.5f - Slide, Y + 54, WithAlpha(Parchment, Alpha), 0.85f, EAlign::Center, W - 80);
}

// ---------------------------------------------------------------------------------------
// Between battles

void AExpeditionHUD::DrawTitle()
{
    // Two side drawers leave the candlelit tabletop visible between them (#146).
    // Left: party slots, class and subclass. Right: build details and the actions.
    // Buttons register in the original order (Close, slots, class/subclass, then actions) so Tab keeps its sequence.
    constexpr float Margin = 12, LeftWidth = 300, RightWidth = 380;
    const float DrawerY = Margin, DrawerHeight = H - 2 * Margin;
    const float LeftX = Margin, RightX = W - Margin - RightWidth;
    Panel(LeftX, DrawerY, LeftWidth, DrawerHeight, Gold, 0.94f);
    Text(TEXT("dnd-diceroll"), LeftX + 14, DrawerY + 8, Gold, 1.25f);
    if (View->HasRun()) { Button(TEXT("Close"), TEXT("Back to result"), TEXT("Esc"), LeftX + LeftWidth - 138, DrawerY + 8, 126, 40, true, false, Sky, 0.8f); }
    Text(TEXT("Four heroes / five battles / levels 1-5"), LeftX + 14, DrawerY + 42, Muted, 0.62f, EAlign::Left, LeftWidth - 28);
    const float SlotY = DrawerY + 62;
    for (int32 I = 0; I < Player->NextParty().Num(); ++I)
    {
        const auto& P = Player->NextParty()[I];
        FCombatantState Hero; FBuildCatalog::Create(P.BuildId, P.Id, P.Name, Hero);
        Button(FName(*FString::Printf(TEXT("Party%d"), I)), FString::Printf(TEXT("%d  %s"), I + 1, *P.Name), FBuildCatalog::DisplayName(Hero), LeftX + 12, SlotY + I * 50, LeftWidth - 24, 44, true, Player->BuilderSlot() == I, Sky, 0.85f);
    }
    const auto& Choice = Player->NextParty()[Player->BuilderSlot()];
    FCombatantState Hero; FBuildCatalog::Create(Choice.BuildId, Choice.Id, Choice.Name, Hero);
    const float ClassLabelY = SlotY + 4 * 50 + 4;
    Text(TEXT("CLASS"), LeftX + 14, ClassLabelY, Gold, 0.8f);
    TArray<FString> Classes;
    int32 ClassIndex = 0, BuildIndex = 0;
    const float ClassY = ClassLabelY + 22;
    TSet<FString> Distinct;
    for (const auto& B : FBuildCatalog::All()) { Distinct.Add(B.Profile.ClassId); }
    const float ClassRows = FMath::CeilToFloat(Distinct.Num() / 2.0f);
    const float SubclassLabelY = ClassY + ClassRows * 34 + 6;
    // Long subclass lists (ten for cleric and wizard) switch to two columns and shorter rows, so
    // every choice stays inside the drawer and Tab never reaches a control drawn off screen.
    int32 BuildCount = 0;
    for (const auto& B : FBuildCatalog::All()) { BuildCount += B.Profile.ClassId == Hero.ClassId ? 1 : 0; }
    const float ListTop = SubclassLabelY + 22, ListBottom = DrawerY + DrawerHeight - 10;
    const int32 BuildColumns = ListTop + BuildCount * 28 <= ListBottom ? 1 : 2;
    const int32 BuildRows = FMath::Max(1, FMath::DivideAndRoundUp(BuildCount, BuildColumns));
    const float BuildRow = FMath::Min(28.0f, (ListBottom - ListTop) / BuildRows);
    const float BuildWidth = (LeftWidth - 24 - (BuildColumns - 1) * 6) / BuildColumns;
    for (const auto& B : FBuildCatalog::All())
    {
        if (!Classes.Contains(B.Profile.ClassId))
        {
            Classes.Add(B.Profile.ClassId);
            Button(FName(*(TEXT("Class:") + B.Profile.ClassId)), B.Profile.Role, TEXT(""), LeftX + 12 + (ClassIndex % 2) * 140, ClassY + (ClassIndex / 2) * 34, 134, 30, true, B.Profile.ClassId == Hero.ClassId, FLinearColor(0.95f, 0.78f, 0.4f), 0.8f);
            ++ClassIndex;
        }
        if (B.Profile.ClassId == Hero.ClassId)
        {
            const FString Label = FBuildCatalog::DisplayName(B.Profile).RightChop(B.Profile.Role.Len() + 3);
            Button(FName(*(TEXT("Build:") + B.Id)), Label, TEXT(""), LeftX + 12 + (BuildIndex % BuildColumns) * (BuildWidth + 6), ListTop + (BuildIndex / BuildColumns) * BuildRow,
                BuildWidth, BuildRow - 3, true, B.Id == Hero.BuildId, Sky, BuildColumns == 1 ? 0.8f : 0.7f);
            ++BuildIndex;
        }
    }
    Text(TEXT("SUBCLASS"), LeftX + 14, SubclassLabelY, Gold, 0.8f);

    // The right side is two panels sized to their content, details on top and actions at the
    // foot, so the tabletop shows between them instead of behind an empty drawer.
    const float InnerX = RightX + 14, InnerWidth = RightWidth - 28;
    const auto* Build = FBuildCatalog::Find(Hero.BuildId);
    float DetailsEnd = DrawerY + 66;
    for (const auto& Ability : FBuildCatalog::Abilities(Hero)) { DetailsEnd += 22 + Wrap(Ability.Description, InnerWidth, 0.72f).Num() * 18.0f + 8; }
    if (Build->SneakDie) { DetailsEnd += 20; }
    if (Build->CriticalThreshold == 19) { DetailsEnd += 20; }
    Panel(RightX, DrawerY, RightWidth, DetailsEnd - DrawerY + 6, Gold, 0.94f);
    Text(FBuildCatalog::DisplayName(Hero), InnerX, DrawerY + 12, Parchment, 1.0f, EAlign::Left, InnerWidth);
    Text(FString::Printf(TEXT("HP %d / AC %d / reach %d"), Hero.MaxHP, Hero.ArmorClass, Hero.Range), InnerX, DrawerY + 38, Muted, 0.8f);
    float Row = DrawerY + 66;
    for (const auto& Ability : FBuildCatalog::Abilities(Hero))
    {
        Text(Ability.Name, InnerX, Row, Gold, 0.85f); Row += 22;
        for (const auto& Line : Wrap(Ability.Description, InnerWidth, 0.72f)) { Text(Line, InnerX, Row, Parchment, 0.72f); Row += 18; }
        Row += 8;
    }
    if (Build->SneakDie) { Text(TEXT("Passive: Sneak Attack with adjacent ally"), InnerX, Row, Safe, 0.75f); Row += 20; }
    if (Build->CriticalThreshold == 19) { Text(TEXT("Passive: critical hits on 19-20"), InnerX, Row, Safe, 0.75f); }
    // Actions sit at the foot of the drawer.
    const float Bottom = DrawerY + DrawerHeight - 12;
    const float SmallY = Bottom - 40, BigY = SmallY - 6 - 48;
    const float Half = (InnerWidth - 6) * 0.5f, Third = (InnerWidth - 12) / 3.0f;
    const TArray<FString> Hint = Wrap(TEXT("1-4 slot / Up-Down class / Left-Right subclass / Tab or Shift-Tab focus / Enter activate"), InnerWidth, 0.6f);
    const float ActionsTop = BigY - 8 - Hint.Num() * 15.0f - 10;
    Panel(RightX, ActionsTop, RightWidth, Bottom + 12 - ActionsTop, Gold, 0.94f);
    Button(TEXT("New"), TEXT("Begin expedition"), TEXT("N"), InnerX, BigY, Half, 48, true, false, FLinearColor(0.95f, 0.78f, 0.4f), 0.85f);
    Button(TEXT("Resume"), TEXT("Resume saved party"), TEXT("O"), InnerX + Half + 6, BigY, Half, 48, true, false, FLinearColor(0.95f, 0.78f, 0.4f), 0.85f);
    Button(TEXT("Preset"), TEXT("Familiar party"), TEXT("G"), InnerX, SmallY, Third, 40, true, false, FLinearColor(0.95f, 0.78f, 0.4f), 0.8f);
    Button(TEXT("Records"), TEXT("Records"), TEXT("H"), InnerX + Third + 6, SmallY, Third, 40, true, false, FLinearColor(0.95f, 0.78f, 0.4f), 0.8f);
    Button(TEXT("Help"), TEXT("Help"), TEXT("?"), InnerX + 2 * (Third + 6), SmallY, Third, 40, true, false, FLinearColor(0.95f, 0.78f, 0.4f), 0.8f);
    float HintY = BigY - 8 - Hint.Num() * 15.0f;
    for (const auto& Line : Hint) { Text(Line, InnerX, HintY, Muted, 0.6f); HintY += 15; }
    if (!View->Messages().IsEmpty())
    {
        // Feedback sits between the drawers, over the tabletop.
        const float MessageX = LeftX + LeftWidth + 16, MessageWidth = RightX - 16 - MessageX;
        const TArray<FString> Lines = Wrap(View->Messages().Last(), MessageWidth - 24, 0.8f);
        const float Height = 14 + Lines.Num() * 21.0f;
        Panel(MessageX, H - Margin - Height, MessageWidth, Height, Muted, 0.9f, false);
        for (int32 Index = 0; Index < Lines.Num(); ++Index) { Text(Lines[Index], MessageX + 12, H - Margin - Height + 7 + Index * 21.0f, Parchment, 0.8f); }
    }
}


void AExpeditionHUD::DrawRoutes()
{
    const FExpeditionProgress& Run = View->Progress();
    const float CardWidth = 340, CardHeight = 250, Gap = 30;
    const float Top = H * 0.5f - CardHeight * 0.5f - 10;
    Panel(W * 0.5f - 370, Top - 84, 740, 76, Gold, 0.95f);
    Text(TEXT("CHOOSE YOUR PATH"), W * 0.5f, Top - 66, Gold, 1.5f, EAlign::Center);
    Text(FString::Printf(TEXT("Encounter %d of %d  ·  Injuries and losses carry forward  ·  No automatic rest"), Run.EncountersCompleted + 1, FExpeditionRun::EncounterCount), W * 0.5f, Top - 30, Muted, 0.76f, EAlign::Center);
    for (int32 Index = 0; Index < Run.Offers.Num() && Index < 2; ++Index)
    {
        const FExpeditionRoute& Route = Run.Offers[Index];
        const float X = W * 0.5f + (Index == 0 ? -CardWidth - Gap * 0.5f : Gap * 0.5f);
        const FLinearColor Accent = Route.bDangerous ? Danger : Safe;
        const bool bHover = Hovered(X, Top, CardWidth, CardHeight);
        const FName Id(*FString::Printf(TEXT("Route%d"), Index));
        const bool bFocused = Player->MenuFocus() == Id;
        Panel(X, Top, CardWidth, CardHeight, Accent, bHover || bFocused ? 0.98f : 0.9f);
        if (bFocused) { Frame(Parchment, X - 3, Top - 3, CardWidth + 6, CardHeight + 6, 2); }
        const float Y = Top;
        Rect(WithAlpha(Accent, 0.22f), X + 16, Y + 16, Measure(Route.bDangerous ? TEXT("DANGEROUS") : TEXT("GUARDED"), 0.7f).X + 16, 22);
        Text(Route.bDangerous ? TEXT("DANGEROUS") : TEXT("GUARDED"), X + 24, Y + 18, Accent, 0.7f);
        Text(Index == 0 ? TEXT("Z") : TEXT("X"), X + CardWidth - 18, Y + 16, Muted, 0.9f, EAlign::Right);
        Text(Route.Name, X + 16, Y + 46, Parchment, 1.25f, EAlign::Left, CardWidth - 32);
        // Enemy tokens.
        for (int32 Enemy = 0; Enemy < Route.EnemyCount; ++Enemy)
        {
            const FVector2D Center(X + 30 + Enemy * 30, Y + 104);
            Circle(Center, 12, FLinearColor(0.4f, 0.1f, 0.08f));
            Circle(Center, 10, FLinearColor(1.0f, 0.42f, 0.32f));
        }
        TArray<FString> Facts;
        Route.Preview.ParseIntoArray(Facts, TEXT(" | "));
        float FactY = Y + 126;
        for (int32 Fact = 1; Fact < Facts.Num(); ++Fact)
        {
            const bool bReward = Facts[Fact].Contains(TEXT("gear")) || Facts[Fact].Contains(TEXT("artifact"));
            Text(Facts[Fact], X + 16, FactY, bReward ? Gold : Parchment, 0.8f, EAlign::Left, CardWidth - 32);
            FactY += 24;
        }
        Buttons.Add({Id, FBox2D(FVector2D(X, Top) * S, FVector2D(X + CardWidth, Top + CardHeight) * S), true});
        Text(bFocused ? TEXT("Enter to set out") : bHover ? TEXT("Click to set out") : TEXT(""), X + CardWidth * 0.5f, Y + CardHeight - 30, Accent, 0.8f, EAlign::Center);
    }
}

void AExpeditionHUD::DrawReward()
{
    const FExpeditionProgress& Run = View->Progress();
    const FCombatBattle& Rules = View->Rules();
    const float Width = 720, Height = 240;
    const float X = W * 0.5f - Width * 0.5f, Y = H * 0.5f - Height * 0.5f - 30;
    Panel(X, Y, Width, Height, Gold, 0.95f);
    Text(TEXT("VICTORY: SURVIVORS GAINED A LEVEL"), W * 0.5f, Y + 16, Gold, 1.05f, EAlign::Center, Width - 32);
    const int32 Potions = Run.History.IsEmpty() ? 0 : Run.History.Last().Route.RewardPotions;
    const FCombatantState* Selected = Rules.Find(Player->Selection());
    const bool bReady = Selected && Selected->Life == ECombatLife::Conscious;
    const bool bGearCapped = bReady && Selected->WeaponRank >= 3;
    const FString Gear = bGearCapped ? TEXT("Gear rank 3 cap: no extra damage") : TEXT("+1 gear rank (+1 damage; max rank 3)");
    Text(FString::Printf(TEXT("%s   ·   +%d healing potion%s"), *Gear, Potions, Potions == 1 ? TEXT("") : TEXT("s")), W * 0.5f, Y + 58, Parchment, 0.86f, EAlign::Center, Width - 32);
    TArray<FString> Lost;
    for (const FCombatantState& Unit : Rules.Snapshot().Encounter.Combatants) { if (Unit.Side == ECombatSide::Party && Unit.Life == ECombatLife::Dead) { Lost.Add(Unit.Name); } }
    if (!Lost.IsEmpty()) { Text(TEXT("Fallen: ") + FString::Join(Lost, TEXT(", ")), W * 0.5f, Y + 84, FLinearColor(0.85f, 0.5f, 0.45f), 0.76f, EAlign::Center); }
    Text(TEXT("Choose who carries it  —  click a survivor's card or press 1-4"), W * 0.5f, Y + 110, Muted, 0.74f, EAlign::Center);
    Button(TEXT("Claim"), bReady ? FString::Printf(TEXT("Give to %s"), *Selected->Name) : TEXT("Select a survivor"), TEXT("L"), W * 0.5f - 130, Y + 170, 260, 52, bReady, false, Gold);
}

void AExpeditionHUD::DrawCamp()
{
    const FCombatBattle& Rules = View->Rules();
    const float Width = 720, Height = 300;
    const float X = W * 0.5f - Width * 0.5f, Y = H * 0.5f - Height * 0.5f - 30;
    Panel(X, Y, Width, Height, FLinearColor(1.0f, 0.6f, 0.3f), 0.94f);
    Text(TEXT("MAKE CAMP?"), W * 0.5f, Y + 16, FLinearColor(1.0f, 0.7f, 0.4f), 1.4f, EAlign::Center);
    Text(TEXT("Once per expedition. If you press on, the chance is gone."), W * 0.5f, Y + 56, Muted, 0.76f, EAlign::Center);
    float DescriptionY=Y+82;
    for (const auto& Line : Wrap(FExpeditionRun::RecoveryDescription(),Width-48,0.88f)) { Text(Line,X+24,DescriptionY,Parchment,0.88f); DescriptionY+=24; }
    float Row = Y + 162;
    FString Status;
    for (const FCombatantState& Unit : Rules.Snapshot().Encounter.Combatants)
    {
        if (Unit.Side != ECombatSide::Party) { continue; }
        Status += (Status.IsEmpty() ? TEXT("") : TEXT("    ")) + (Unit.Life == ECombatLife::Dead ? Unit.Name + TEXT(" (fallen)") : FString::Printf(TEXT("%s %d/%d"), *Unit.Name, Unit.HP, Unit.MaxHP));
    }
    Text(Status, W * 0.5f, Row, Parchment, 0.76f, EAlign::Center, Width - 40);
    Button(TEXT("Camp"), TEXT("Rest by the fire"), TEXT("C"), W * 0.5f - 250, Y + 220, 240, 56, true, false, FLinearColor(1.0f, 0.6f, 0.3f));
    Button(TEXT("SkipCamp"), TEXT("Press on"), TEXT("V  ·  forfeit the rest"), W * 0.5f + 10, Y + 220, 240, 56, true, false, Muted);
}

void AExpeditionHUD::DrawEnding()
{
    if (Player->ShowingBuilder()) { DrawTitle(); return; }
    const auto& Run = View->Progress();
    const float X=(W-880)/2, Y=(H-500)/2;
    Panel(X,Y,880,500,Gold,0.98f);
    Text(Run.Phase==EExpeditionPhase::Victory ? TEXT("ARTIFACT RECOVERED") : TEXT("EXPEDITION LOST"), X+32,Y+24,Gold,1.5f);
    Text(FString::Printf(TEXT("%d / 5 battles won / Seed %d"), Run.EncountersCompleted, Run.Seed), X+32,Y+72,Parchment,1.0f);
    float Row=Y+122;
    for (const auto& Unit : View->Rules().Snapshot().Encounter.Combatants)
    {
        if (Unit.Side!=ECombatSide::Party) { continue; }
        Text(Unit.Name+TEXT(" / ")+FBuildCatalog::DisplayName(Unit)+ (Unit.Life==ECombatLife::Dead ? TEXT(" / Fallen") : TEXT(" / Survived")), X+32,Row,Unit.Life==ECombatLife::Dead ? Danger : Parchment,0.9f); Row+=36;
    }
    Text(TEXT("History is kept. The next expedition starts with fresh heroes."),X+32,Y+308,Muted,0.85f);
    Button(TEXT("Builder"),TEXT("Choose next party"),TEXT("Tab / Enter"),X+32,Y+362,390,64);
    Button(TEXT("Records"),TEXT("View records"),TEXT("H"),X+450,Y+362,398,64);
}

void AExpeditionHUD::DrawRecords()
{
    const float Width = 1000, Height = 500;
    const float X = W * 0.5f - Width * 0.5f, Y = H * 0.5f - Height * 0.5f - 10;
    Rect(FLinearColor(0, 0, 0, 0.5f), 0, 0, W, H);
    Panel(X, Y, Width, Height, Gold, 0.98f);
    Text(TEXT("PAST EXPEDITIONS"), X + 24, Y + 18, Gold, 1.2f);
    Text(TEXT("Records grant no power to new runs."), X + Width - 24, Y + 26, Muted, 0.7f, EAlign::Right);
    const TArray<FExpeditionEnding>& Records = View->Records();
    if (Records.IsEmpty()) { Text(TEXT("No completed expeditions yet."), X + 24, Y + 70, Parchment, 0.85f); }
    else
    {
        Text(FString::Printf(TEXT("%d completed  ·  showing %d-%d, newest first"), Records.Num(), View->RecordOffset() + 1,
            FMath::Min(Records.Num(), View->RecordOffset() + View->RecordsPerPage)), X + 24, Y + 60, Muted, 0.74f);
        float Row = Y + 92;
        for (int32 Index = Records.Num() - 1 - View->RecordOffset(); Index >= FMath::Max(0, Records.Num() - View->RecordOffset() - View->RecordsPerPage); --Index)
        {
            const FExpeditionEnding& Record = Records[Index];
            const bool bWon = Record.Outcome == EExpeditionPhase::Victory;
            Rect(FLinearColor(1, 1, 1, 0.04f), X + 20, Row - 4, Width - 40, 72);
            Text(bWon ? TEXT("RECOVERED") : TEXT("LOST"), X + 32, Row + 4, bWon ? Gold : Danger, 0.78f);
            float DetailY=Row;
            for (const auto& Line : Wrap(FString::Printf(TEXT("Seed %d / %s"), Record.Seed, *Record.Summary), Width-190, 0.85f)) { Text(Line,X+150,DetailY,Parchment,0.85f); DetailY+=22; }
            Row += 76;
        }
    }
    Button(TEXT("Older"), TEXT("Older"), TEXT("Page Up"), X + 24, Y + Height - 64, 130, 46, View->RecordOffset() + View->RecordsPerPage < Records.Num());
    Button(TEXT("Newer"), TEXT("Newer"), TEXT("Page Down"), X + 164, Y + Height - 64, 130, 46, View->RecordOffset() > 0);
    Button(TEXT("Close"), TEXT("Close"), TEXT("H / Esc"), X + Width - 154, Y + Height - 64, 130, 46, true, false, Gold);
    Blockers.Add(FBox2D(FVector2D(0, 0), FVector2D(W, H) * S));
}

void AExpeditionHUD::DrawHelp()
{
    const float Width = 700, Height = View->InCombat() ? 430.0f : 400.0f;
    const float X = W * 0.5f - Width * 0.5f, Y = H * 0.5f - Height * 0.5f;
    Rect(FLinearColor(0, 0, 0, 0.5f), 0, 0, W, H);
    Panel(X, Y, Width, Height, Sky, 0.98f);
    Text(TEXT("HOW TO PLAY"), X + 24, Y + 18, Sky, 1.2f);
    if (!View->InCombat())
    {
        const TArray<FString> Lines = Player->ShowingBuilder() ? TArray<FString>{
            TEXT("Choose four independent heroes. Duplicates are welcome."),
            TEXT("Click a slot (1-4), then a class and compatible subclass."),
            TEXT("Up / Down changes class. Left / Right changes subclass."),
            TEXT("G restores the familiar party. N begins the expedition."),
            TEXT("O resumes the exact saved party, keeping its spent resources."),
            TEXT("Backspace during an expedition returns here; Resume continues it."),
            TEXT("An unfinished saved expedition must be resumed before starting anew.")
        } : TArray<FString>{
            TEXT("Z / X chooses a route. Compare danger and promised rewards."),
            TEXT("1-4 chooses a reward recipient; L confirms. Fallen heroes cannot receive it."),
            TEXT("Gear caps at rank 3. Potions remain useful at the gear cap."),
            TEXT("C takes the one recovery opportunity; V forfeits it."),
            TEXT("Backspace or Menu > Return to start leaves; O resumes from there."),
            TEXT("Deaths are permanent. Recovery never resurrects fallen heroes."),
            TEXT("The ending keeps a record; choose next party to begin again.")};
        float Row=Y+70;
        for (const auto& Line : Lines) { for (const auto& Part : Wrap(Line,Width-64,0.9f)) { Text(Part,X+32,Row,Parchment,0.9f); Row+=27; } }
        Button(TEXT("Close"),TEXT("Back"),TEXT("Esc / ?"),X+Width-154,Y+Height-60,130,44);
        return;
    }
    const TCHAR* Left[] = {
        TEXT("Click a highlighted cell to move"), TEXT("Click a marked enemy to attack"), TEXT("A  Attack / move mode"), TEXT("F / M  First / second catalog special"),
        TEXT("R  Rescue a downed ally"), TEXT("P  Drink or give a potion"), TEXT("Space  End turn"), TEXT("1-4  Select adventurer"),
        TEXT("Arrows + Enter  Keyboard cursor"), TEXT("Hold Alt  Show the whole grid")};
    const TCHAR* Right[] = {
        TEXT("Right-drag or Q/E  Rotate"), TEXT("Wheel  Zoom"), TEXT("Tab  Skip the current animation"), TEXT("T  Animation speed"),
        TEXT("Z / X  Choose route"), TEXT("L  Give loot to the selected survivor"), TEXT("C / V  Camp or press on"), TEXT("K / O  Save / resume"),
        TEXT("H  Records  ·  ?  Help  ·  I  Menu"), TEXT("Esc  Close Help, Records or the Menu")};
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(Left); ++Index)
    {
        Text(Left[Index], X + 32, Y + 66 + Index * 28.0f, Parchment, 0.8f);
        Text(Right[Index], X + Width * 0.5f + 12, Y + 66 + Index * 28.0f, Parchment, 0.8f);
    }
    Text(TEXT("Downed allies die after 3 of their turns unless rescued. Rescue before the last enemy falls."), W * 0.5f, Y + Height - 82, Gold, 0.72f, EAlign::Center, Width - 40);
    float NoteY = Y + Height - 58;
    for (const auto& Line : Wrap(TEXT("Hover a hero card for full status. The combat log fades after a few seconds; hover the bottom-left corner to bring it back."), Width - 200, 0.62f)) { Text(Line, X + 32, NoteY, Muted, 0.62f); NoteY += 15; }
    Button(TEXT("Close"), TEXT("Close"), TEXT("Esc"), X + Width - 154, Y + Height - 60, 130, 44, true, false, Sky);
    Blockers.Add(FBox2D(FVector2D(0, 0), FVector2D(W, H) * S));
}

void AExpeditionHUD::DrawReturnConfirm()
{
    const float Width = 620, Height = 230;
    const float X = W * 0.5f - Width * 0.5f, Y = H * 0.5f - Height * 0.5f;
    Rect(FLinearColor(0, 0, 0, 0.5f), 0, 0, W, H);
    Panel(X, Y, Width, Height, Gold, 0.98f);
    Text(TEXT("RETURN TO MENU?"), X + 24, Y + 18, Gold, 1.2f);
    float Row = Y + 62;
    for (const TCHAR* Line : {TEXT("Every resolved action is saved and has played out. Only a move you have not finished is dropped."),
        TEXT("Nothing happens while this is open. Resume on the start screen brings you back to this turn.")})
    {
        for (const auto& Part : Wrap(Line, Width - 48, 0.85f)) { Text(Part, X + 24, Row, Parchment, 0.85f); Row += 24; }
    }
    Button(TEXT("ReturnConfirm"), TEXT("Return to menu"), TEXT("Enter"), X + 24, Y + Height - 70, 270, 50, true, false, Gold);
    Button(TEXT("KeepPlaying"), TEXT("Keep playing"), TEXT("Esc"), X + Width - 294, Y + Height - 70, 270, 50, true, false, Sky);
    Blockers.Add(FBox2D(FVector2D(0, 0), FVector2D(W, H) * S));
}

TArray<FBox2D> AExpeditionHUD::ButtonRects() const
{
    TArray<FBox2D> Result;
    for (const FButtonArea& Area : Buttons) { Result.Add(Area.Box); }
    return Result;
}

double AExpeditionHUD::PersistentCoverage() const
{
    std::vector<ExpeditionLayout::Rect> Boxes;
    for (const FBox2D& Box : Persistent) { Boxes.push_back({Box.Min.X * S, Box.Min.Y * S, Box.Max.X * S, Box.Max.Y * S}); }
    for (const FButtonArea& Area : Buttons) { if (!Area.bTransient) { Boxes.push_back({Area.Box.Min.X, Area.Box.Min.Y, Area.Box.Max.X, Area.Box.Max.Y}); } }
    const double Width = W * S, Height = H * S;
    return Width > 0 && Height > 0 ? ExpeditionLayout::UnionArea(Boxes, {0, 0, Width, Height}) / (Width * Height) : 0.0;
}

TArray<FName> AExpeditionHUD::MenuButtons() const
{
    TArray<FName> Result;
    for (const auto& Button : Buttons) { if (Button.bEnabled) { Result.AddUnique(Button.Id); } }
    return Result;
}
