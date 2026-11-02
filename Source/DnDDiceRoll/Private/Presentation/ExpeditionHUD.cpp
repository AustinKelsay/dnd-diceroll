#include "Presentation/ExpeditionHUD.h"
#include "Presentation/ExpeditionBattleView.h"
#include "Presentation/ExpeditionPlayerController.h"
#include "CanvasItem.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "GlobalRenderResources.h"
#include "Party/BuildCatalog.h"
#include "Party/FeatureKit.h"
#include "Rules/CoinRules.h"

namespace
{
constexpr float DesignWidth = 1280.0f;
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

bool AExpeditionHUD::Button(FName Id, const FString& Label, const FString& Sub, float X, float Y, float Width, float Height, bool bEnabled, bool bSelected, FLinearColor Accent)
{
    const bool bHover = bEnabled && Hovered(X, Y, Width, Height);
    const float Lift = bHover ? -1.5f : 0.0f;
    Rect(FLinearColor(0, 0, 0, 0.4f), X + 2, Y + 3, Width, Height);
    const FLinearColor Base = bSelected ? FLinearColor(Accent.R * 0.12f, Accent.G * 0.12f, Accent.B * 0.12f, 0.97f)
        : bHover ? FLinearColor(0.05f, 0.035f, 0.025f, 0.97f) : FLinearColor(0.016f, 0.012f, 0.01f, 0.94f);
    Rect(Base, X, Y + Lift, Width, Height);
    Rect(FLinearColor(1, 1, 1, bHover ? 0.07f : 0.035f), X, Y + Lift, Width, Height * 0.45f);
    Frame(WithAlpha(Accent, bSelected ? 0.95f : bHover ? 0.7f : bEnabled ? 0.35f : 0.12f), X, Y + Lift, Width, Height, bSelected ? 2.0f : 1.0f);
    const FLinearColor LabelColor = bEnabled ? (bSelected ? Accent : Parchment) : FLinearColor(0.45f, 0.48f, 0.52f);
    const float LabelY = Sub.IsEmpty() ? Y + Lift + Height * 0.5f - 10 : Y + Lift + Height * 0.5f - 19;
    Text(Label, X + Width * 0.5f, LabelY, LabelColor, 0.92f, EAlign::Center, Width - 12);
    if (!Sub.IsEmpty()) { Text(Sub, X + Width * 0.5f, Y + Lift + Height * 0.5f + 2, bEnabled ? Muted : FLinearColor(0.38f, 0.4f, 0.44f), 0.68f, EAlign::Center, Width - 10); }
    Buttons.Add({Id, FBox2D(FVector2D(X, Y) * S, FVector2D(X + Width, Y + Height) * S), bEnabled});
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
    Player = Cast<AExpeditionPlayerController>(GetOwningPlayerController());
    View = Player.IsValid() ? Player->GetBattleView() : nullptr;
    if (!Player.IsValid() || !View.IsValid() || !Canvas) { return; }
    S = FMath::Max(0.5f, Canvas->SizeX / DesignWidth);
    W = Canvas->SizeX / S;
    H = Canvas->SizeY / S;
    Clock = GetWorld() ? GetWorld()->GetRealTimeSeconds() : 0.0f;
    float MouseX = -1, MouseY = -1;
    Mouse = Player->GetMousePosition(MouseX, MouseY) ? FVector2D(MouseX, MouseY) / S : FVector2D(-1, -1);

    // The tabletop overview has no board: unit bars and labels belong to battles only.
    if (View->HasRun() && !View->ShowingOverview()) { DrawWorldOverlays(); }
    DrawTopBar();
    if (!View->HasRun()) { DrawTitle(); }
    else
    {
        DrawParty();
        DrawLog();
        if (View->InCombat())
        {
            DrawInitiative();
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
    DrawBanner();
    // Overlays own every click: buttons hidden beneath them must not fire.
    if (View->ShowingRecords() || Player->ShowingHelp()) { Buttons.Reset(); }
    if (View->ShowingRecords()) { DrawRecords(); }
    if (Player->ShowingHelp()) { DrawHelp(); }
}

// ---------------------------------------------------------------------------------------
// World-anchored overlays: health bars, floating numbers, the move path.

void AExpeditionHUD::DrawWorldOverlays()
{
    const FCombatBattle& Rules = View->Rules();
    const FExpeditionPlayback& Show = View->Playback();
    const bool bCombat = View->InCombat();
    const int32 Active = bCombat ? Rules.ActiveId() : INDEX_NONE;

    // Path preview: a dotted trail from the adventurer to the hovered cell.
    const TArray<FIntPoint>& Path = Player->PreviewPath();
    if (bCombat && !Path.IsEmpty() && !View->IsPresenting())
    {
        FVector2D Previous;
        bool bHasPrevious = Project(View->UnitPosition(Player->Actor(), 4.0f), Previous);
        for (int32 Index = 0; Index < Path.Num(); ++Index)
        {
            FVector2D Point;
            if (!Project(View->BoardPosition(FVector2D(Path[Index].X, Path[Index].Y), 4.0f), Point)) { continue; }
            if (bHasPrevious) { Line(Previous, Point, FLinearColor(0.5f, 0.95f, 1.0f, 0.9f), 3.0f); }
            Circle(Point, Index == Path.Num() - 1 ? 9.0f : 4.0f, FLinearColor(0.55f, 0.95f, 1.0f, 0.95f));
            Previous = Point; bHasPrevious = true;
        }
        if (bHasPrevious) { Text(FString::FromInt(Path.Num()), Previous.X, Previous.Y - 9, Ink, 0.8f, EAlign::Center); }
    }

    // Keyboard cursor: bright corner brackets around the cell.
    if (bCombat && Player->UsingKeyboardCursor())
    {
        const FIntPoint Cell = Player->KeyboardCursorCell();
        const FVector Center = View->BoardPosition(FVector2D(Cell.X, Cell.Y), 2.0f);
        const FVector Offsets[] = {{-52, -52, 0}, {52, -52, 0}, {52, 52, 0}, {-52, 52, 0}};
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

    // Action reach: a dot on every cell the action can strike, a cross where range is blocked or out of sight.
    if (bCombat && !View->IsPresenting())
    {
        const FActionReach Span = Rules.Reach(Player->Actor(), Player->Action());
        const FLinearColor Strike = ExpeditionStyle::IsFriendlyAction(Player->Action(), Rules.Find(Player->Actor())) ? Safe : Danger;
        for (const FIntPoint Cell : Span.InSight)
        {
            FVector2D Point;
            if (Project(View->BoardPosition(FVector2D(Cell.X, Cell.Y), 3.0f), Point)) { Polygon(Point, 3.5f, 4, 0.0f, Strike * FLinearColor(1, 1, 1, 0.7f)); }
        }
        for (const FIntPoint Cell : Span.Obscured)
        {
            FVector2D Point;
            if (!Project(View->BoardPosition(FVector2D(Cell.X, Cell.Y), 3.0f), Point)) { continue; }
            const FLinearColor Veil(0.75f, 0.75f, 0.8f, 0.55f);
            Line(Point + FVector2D(-5, -5), Point + FVector2D(5, 5), Veil, 2.0f);
            Line(Point + FVector2D(-5, 5), Point + FVector2D(5, -5), Veil, 2.0f);
        }
    }

    // Legal targets get a pulsing marker above their heads and a forecast chip.
    if (bCombat && !View->IsPresenting())
    {
        const bool bFriendly = ExpeditionStyle::IsFriendlyAction(Player->Action(), Rules.Find(Player->Actor()));
        for (int32 Id : Player->Targets())
        {
            const FActionForecast Odds = Rules.Forecast(Player->Actor(), Player->Action(), Id, Player->SlotChoice());
            FVector2D Point;
            if (!Odds.bValid || !Project(View->UnitPosition(Id, 215.0f), Point)) { continue; }
            const FString Amount = Odds.MinAmount == Odds.MaxAmount ? FString::FromInt(Odds.MinAmount) : FString::Printf(TEXT("%d-%d"), Odds.MinAmount, Odds.MaxAmount);
            const FString Label = Odds.bHealing ? FString::Printf(TEXT("Heals %s"), *Amount)
                : FString::Printf(TEXT("%s \u00B7 %s%s"), Odds.bAttackRoll ? *FString::Printf(TEXT("%d%%"), Odds.HitChancePercent) : TEXT("Never misses"), *Amount, Odds.bCanDrop ? TEXT(" \u00B7 can drop") : TEXT(""));
            const FLinearColor Edge = Odds.bCanDrop ? Danger : Odds.bHealing ? Safe : !Odds.bAttackRoll || Odds.HitChancePercent >= 65 ? Safe : Odds.HitChancePercent >= 40 ? Gold : Danger;
            const float Width = Measure(Label, 0.62f).X + 14.0f;
            Rect(FLinearColor(0.03f, 0.03f, 0.04f, 0.82f), Point.X - Width * 0.5f, Point.Y - 22, Width, 18);
            Frame(Edge, Point.X - Width * 0.5f, Point.Y - 22, Width, 18, Odds.bCanDrop ? 2.0f : 1.0f);
            Text(Label, Point.X, Point.Y - 20, Odds.bCanDrop ? Danger : Parchment, 0.62f, EAlign::Center);
        }
        for (int32 Id : Player->Targets())
        {
            FVector2D Point;
            if (!Project(View->UnitPosition(Id, 185.0f), Point)) { continue; }
            const bool bHover = Id == Player->HoveredTarget();
            const float Bob = 4.0f * FMath::Sin(Clock * 5.0f);
            const FLinearColor Color = bFriendly ? Safe : Danger;
            Polygon(FVector2D(Point.X, Point.Y + Bob), bHover ? 12.0f : 9.0f, 3, PI / 2, FLinearColor(0, 0, 0, 0.5f));
            Polygon(FVector2D(Point.X, Point.Y + Bob - 1), bHover ? 10.0f : 7.5f, 3, PI / 2, Color);
        }
    }

    for (const FCombatantState& Unit : Rules.Snapshot().Encounter.Combatants)
    {
        const FUnitDisplay* Display = Show.Unit(Unit.Id);
        if (!Display) { continue; }
        FVector2D Point;
        if (!Project(View->UnitPosition(Unit.Id, Display->Tilt > 0.5f ? 70.0f : 122.0f), Point)) { continue; }
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
        if (!Project(View->BoardPosition(Floater.Cell, 105.0f + Floater.Age * 55.0f), Point)) { continue; }
        const float Life = Floater.Age / Floater.Life;
        const float Alpha = Life < 0.7f ? 1.0f : 1.0f - (Life - 0.7f) / 0.3f;
        const float Pop = 1.0f + 0.6f * FMath::Max(0.0f, 1.0f - Floater.Age / 0.14f);
        Text(Floater.Text, Point.X, Point.Y - 30, WithAlpha(Floater.Color, Alpha), 1.05f * Floater.Scale * Pop, EAlign::Center);
    }
}

// ---------------------------------------------------------------------------------------
// Persistent frame: location, initiative, system buttons, party, log.

void AExpeditionHUD::DrawTopBar()
{
    if (View->HasRun())
    {
        const FExpeditionProgress& Run = View->Progress();
        Panel(12, 12, 250, 74);
        Text(FString::Printf(TEXT("dnd-diceroll  ·  SEED %d"), Run.Seed), 24, 20, Gold, 0.72f, EAlign::Left, 226);
        Text(View->LocationName(), 24, 36, Parchment, 1.08f, EAlign::Left, 226);
        const FString Status = View->InCombat() ? FString::Printf(TEXT("Encounter %d of %d  ·  Round %d"), Run.History.Num(), FExpeditionRun::EncounterCount, View->Rules().Snapshot().Round) : PhaseLine(Run);
        Text(Status, 24, 62, Muted, 0.72f);
    }
    const float Top = 14, Size = 30, Gap = 6;
    float X = W - 12;
    auto System = [&](FName Id, const FString& Label, float Width, bool bEnabled = true)
    {
        X -= Width;
        Button(Id, Label, FString(), X, Top, Width, Size, bEnabled, false, Sky);
        X -= Gap;
    };
    System(TEXT("Help"), TEXT("? Help"), 74);
    const FString Speed = View->Speed() == EPlaybackSpeed::Normal ? TEXT("1x") : View->Speed() == EPlaybackSpeed::Fast ? TEXT("2x") : TEXT("Instant");
    System(TEXT("Speed"), TEXT("T  Speed ") + Speed, 128);
    System(TEXT("Records"), TEXT("H  Records"), 104);
    System(TEXT("Resume"), TEXT("O  Resume"), 98);
    System(TEXT("Save"), TEXT("K  Save"), 82, View->HasRun());
}

void AExpeditionHUD::DrawInitiative()
{
    const FCombatBattle& Rules = View->Rules();
    const FCombatSnapshot& Snapshot = Rules.Snapshot();
    const int32 Count = Snapshot.TurnOrder.Num();
    if (Count == 0) { return; }
    const float Spacing = 44.0f;
    const float Width = Count * Spacing + 96;
    const float Left = 274.0f;
    Panel(Left, 12, Width, 56, Gold, 0.82f);
    Text(TEXT("ROUND"), Left + 14, 20, Muted, 0.62f);
    Text(FString::FromInt(Snapshot.Round), Left + 30, 34, Gold, 1.15f, EAlign::Center);
    // While results play, the strip follows whoever is shown acting, not the committed turn.
    const int32 Focus = View->Playback().FocusId();
    const int32 Active = View->IsPresenting() && Focus != INDEX_NONE ? Focus : Rules.ActiveId();
    for (int32 Index = 0; Index < Count; ++Index)
    {
        const FCombatantState* Unit = Rules.Find(Snapshot.TurnOrder[Index]);
        if (!Unit) { continue; }
        const FUnitDisplay* Display = View->Playback().Unit(Unit->Id);
        const ECombatLife Life = Display ? Display->Life : Unit->Life;
        const FVector2D Center(Left + 74 + Index * Spacing, 40);
        const bool bActive = Unit->Id == Active;
        const float Radius = bActive ? 19.0f : 15.0f;
        const FLinearColor Color = Life == ECombatLife::Dead ? FLinearColor(0.3f, 0.3f, 0.32f) : ExpeditionStyle::UnitColor(*Unit);
        if (bActive) { Circle(Center, Radius + 3 + FMath::Sin(Clock * 5.0f), Gold); }
        Circle(Center, Radius, Color * 0.5f);
        Circle(Center, Radius - 2.5f, Color * (Unit->Side == ECombatSide::Party ? 0.85f : 0.65f));
        if (Life == ECombatLife::Downed) { Circle(Center, Radius - 2.5f, FLinearColor(1.0f, 0.55f, 0.2f, 0.55f)); }
        Text(Initials(*Unit), Center.X, Center.Y - 10, Ink, 0.85f, EAlign::Center);
        if (Life == ECombatLife::Dead)
        {
            Line(Center - FVector2D(10, 10), Center + FVector2D(10, 10), Danger, 2.5f);
            Line(Center + FVector2D(-10, 10), Center + FVector2D(10, -10), Danger, 2.5f);
        }
        if (FVector2D::Distance(Mouse, Center) < Radius)
        {
            Panel(Center.X - 80, 74, 160, 26, Gold, 0.95f, false);
            Text(FString::Printf(TEXT("%s  ·  initiative %d"), *Unit->Name, Unit->Initiative), Center.X, 79, Parchment, 0.7f, EAlign::Center, 150);
        }
    }
}

void AExpeditionHUD::DrawParty()
{
    int32 Index = 0;
    for (const FCombatantState& Unit : View->Rules().Snapshot().Encounter.Combatants)
    {
        if (Unit.Side != ECombatSide::Party) { continue; }
        DrawPartyCard(Unit, Index, 96 + Index * 116.0f);
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
    const float X = 12, Width = 250, Height = 112;
    const bool bHover = Hovered(X, Y, Width, Height);
    Panel(X, Y, Width, Height, bActive ? Gold : Color, bSelected || bHover ? 0.97f : 0.86f);
    if (bActive) { Frame(WithAlpha(Gold, 0.6f + 0.3f * FMath::Sin(Clock * 4.0f)), X, Y, Width, Height, 2.0f); }
    else if (bSelected) { Frame(FLinearColor(0.9f, 0.95f, 1.0f, 0.7f), X, Y, Width, Height, 1.5f); }
    Rect(WithAlpha(Color, Life == ECombatLife::Dead ? 0.25f : 0.9f), X, Y, 4, Height);
    // Portrait medallion.
    const FVector2D Portrait(X + 34, Y + 34);
    Circle(Portrait, 22, Life == ECombatLife::Dead ? FLinearColor(0.2f, 0.2f, 0.22f) : Color * 0.45f);
    Circle(Portrait, 19, Life == ECombatLife::Dead ? FLinearColor(0.32f, 0.32f, 0.34f) : Color * 0.9f);
    Text(Unit.Role.Left(1), Portrait.X, Portrait.Y - 13, Ink, 1.1f, EAlign::Center);
    Text(FString::FromInt(Index + 1), X + 12, Y + 8, Muted, 0.6f);
    Text(Unit.Name, X + 64, Y + 8, Life == ECombatLife::Dead ? Muted : Color, 0.98f);
    Text(FString::Printf(TEXT("Lv%d %s"), Unit.Level, *FBuildCatalog::DisplayName(Unit)), X + 64, Y + 22, Muted, 0.52f, EAlign::Left, 176);
    if (Life == ECombatLife::Dead)
    {
        Text(TEXT("Fallen  ·  lost for this expedition"), X + 64, Y + 34, FLinearColor(0.75f, 0.45f, 0.4f), 0.72f);
    }
    else if (Life == ECombatLife::Downed)
    {
        Text(FString::Printf(TEXT("DOWNED  ·  %d turns to rescue"), Unit.RescueTurnsLeft), X + 64, Y + 32, FLinearColor(1.0f, 0.6f, 0.3f), 0.74f);
    }
    else
    {
        const float Shown = Display ? Display->ShownHP : Unit.HP;
        const float Trail = Display ? Display->TrailHP : Unit.HP;
        const int32 HP = Display ? Display->HP : Unit.HP;
        Bar(X + 64, Y + 34, 128, 9, Shown / FMath::Max(1, Unit.MaxHP), Trail / FMath::Max(1, Unit.MaxHP), HealthColor(Shown / FMath::Max(1, Unit.MaxHP)));
        Text(FString::Printf(TEXT("%d/%d"), HP, Unit.MaxHP), X + Width - 12, Y + 30, Parchment, 0.72f, EAlign::Right);
    }
    // Resources as pips: the things that actually run out.
    const float Row = Y + 56;
    TArray<FString> Parts;
    FBuildCatalog::Status(Unit).ParseIntoArray(Parts, TEXT(" / "));
    FString Resources;
    TArray<FString> StatusLines;
    for (const auto& Part : Parts)
    {
        if (Part.StartsWith(TEXT("Protection")))
        {
            const auto* Target = View->Rules().Find(Unit.GuardTargetId);
            const FString Label = Target ? FString::Printf(TEXT("%s [%d]"), *Target->Name, Target->Id) : TEXT("unavailable target");
            StatusLines.Append(Wrap(FString::Printf(TEXT("Protect %s: -%d damage until next own turn"), *Label, Unit.GuardAmount > 0 ? Unit.GuardAmount : 3), Width - 24, 0.65f));
        }
        else { Resources += (Resources.IsEmpty() ? TEXT("") : TEXT(" / ")) + Part; }
    }
    if (const EFightingStyle Style = FFeatureKit::Style(Unit); Style != EFightingStyle::None) // SRD Fighting Style: passive [Free], Protection uses the reaction
    { StatusLines.Append(Wrap(FString::Printf(TEXT("Style %s: %s"), FFeatureKit::StyleName(Style), FFeatureKit::StyleEffect(Style)), Width - 24, 0.65f)); }
    Resources += FString::Printf(TEXT(" / Potion %d / Gear +%d"), Unit.Potions, Unit.WeaponRank);
    auto ResourceLines = Wrap(Resources, Width - 24, 0.65f);
    ResourceLines.Append(StatusLines);
    for (int32 I = 0; I < ResourceLines.Num(); ++I) { Text(ResourceLines[I], X + 12, Row - 6 + I * 13, Parchment, 0.65f); }
    Buttons.Add({FName(*FString::Printf(TEXT("Party%d"), Index)), FBox2D(FVector2D(X, Y) * S, FVector2D(X + Width, Y + Height) * S), true});
}

void AExpeditionHUD::DrawLog()
{
    const TArray<FString>& Messages = View->Messages();
    if (Messages.IsEmpty()) { return; }
    TArray<FString> Lines;
    for (int32 Index = FMath::Max(0, Messages.Num() - 2); Index < Messages.Num(); ++Index)
    { Lines.Append(Wrap(Messages[Index], 416, 0.7f)); }
    const float Height = 16 + Lines.Num() * 18.0f;
    const float Top = H - 128 - Height;
    Panel(274, Top, 436, Height, Muted, 0.86f, false);
    for (int32 Index = 0; Index < Lines.Num(); ++Index)
    { Text(Lines[Index], 284, Top + 8 + Index * 18.0f, Parchment, 0.7f); }
}

// ---------------------------------------------------------------------------------------
// Combat controls

void AExpeditionHUD::DrawActionBar()
{
    const FCombatBattle& Rules = View->Rules();
    const FCombatSnapshot& Snapshot = Rules.Snapshot();
    const FCombatantState* Active = Rules.Find(Rules.ActiveId());
    const float Y = H - 82;
    if (View->IsPresenting())
    {
        Button(TEXT("Skip"), TEXT("Skip  >>"), TEXT("Tab"), W * 0.5f - 60, Y + 10, 120, 44, true, false, Sky);
        return;
    }
    if (!Active || Snapshot.Phase != EBattlePhase::Fighting) { return; }
    if (Active->Side == ECombatSide::Enemy)
    {
        Panel(W * 0.5f - 150, Y + 14, 300, 38, Danger, 0.85f);
        const int32 Dots = 1 + static_cast<int32>(Clock * 3.0f) % 3;
        Text(FString::Printf(TEXT("%s is deciding%s"), *Active->Name, *FString::ChrN(Dots, TEXT('.'))), W * 0.5f, Y + 23, Danger, 0.85f, EAlign::Center);
        return;
    }
    const int32 Id = Active->Id;
    Text(FString::Printf(TEXT("%s's turn"), *Active->Name), W * 0.5f + 190, Y - 26, ExpeditionStyle::UnitColor(*Active), 0.78f);
    // Budget chips.
    const float Chips = Y - 26;
    auto Chip = [&](const FString& Label, bool bReady, float X)
    {
        Circle(FVector2D(X, Chips + 9), 5, bReady ? Safe : FLinearColor(0.35f, 0.35f, 0.38f));
        Text(Label, X + 10, Chips, bReady ? Parchment : Muted, 0.7f);
    };
    const float ChipLeft = W * 0.5f - 160;
    Chip(FString::Printf(TEXT("Move %d"), Snapshot.MovementLeft), Snapshot.MovementLeft > 0, ChipLeft);
    Chip(TEXT("Action"), Snapshot.bActionAvailable, ChipLeft + 110);
    Chip(TEXT("Bonus action"), Snapshot.bBonusActionAvailable, ChipLeft + 200);
    Chip(TEXT("Reaction"), Active->bReactionAvailable, ChipLeft + 330); // #79: opportunity attacks and Shield spend it

    const auto Abilities = FBuildCatalog::Abilities(*Active);
    const float Width = 128, Gap = 6, Height = 54;
    const ECombatCommandType Mode = Player->Action();
    auto Ready = [&](ECombatCommandType Action) { return !Rules.LegalTargets(Id, Action).IsEmpty(); };
    // SRD actions beyond the build kit get a button only while usable (or selected), so the bar stays short.
    struct FExtra { ECombatCommandType Action; const TCHAR* Id; const TCHAR* Label; const TCHAR* Key; };
    TArray<FExtra> Extras;
    for (const FExtra& E : {FExtra{ECombatCommandType::BurningHands, TEXT("ModeBurningHands"), TEXT("Burning Hands"), TEXT("Y")}, FExtra{ECombatCommandType::Thunderwave, TEXT("ModeThunderwave"), TEXT("Thunderwave"), TEXT("U")},
        FExtra{ECombatCommandType::Shove, TEXT("ModeShove"), TEXT("Shove"), TEXT("I")}, FExtra{ECombatCommandType::Grapple, TEXT("ModeGrapple"), TEXT("Grapple"), TEXT("W")}})
    { if (Ready(E.Action) || Mode == E.Action) { Extras.Add(E); } }
    const float Left = W * 0.5f - ((4 + Abilities.Num() + Extras.Num()) * (Width + Gap) - Gap) * 0.5f;
    TArray<TPair<ECombatCommandType, bool>> Hover;
    Hover.Add({ECombatCommandType::Attack, Button(TEXT("ModeAttack"), TEXT("Attack / Move"), FString::Printf(TEXT("A / d%d%+d"), Active->DamageDie, FBuildCatalog::AttackModifier(*Active)), Left, Y, Width, Height,
        Ready(ECombatCommandType::Attack) || !Rules.LegalMoves(Id).IsEmpty(), Mode == ECombatCommandType::Attack)});
    int32 Column = 1;
    for (const auto& Ability : Abilities)
    {
        const FString Cost = FString::Printf(TEXT("%s %s / %d %s%s"), Column == 1 ? TEXT("F") : TEXT("M"), Ability.bBonus ? TEXT("Bonus") : TEXT("Action"), Ability.ResourceCost, *Ability.ResourceName, Ability.bResourceAvailable ? TEXT("") : TEXT(" unavailable"));
        Hover.Add({Ability.Command, Button(Column == 1 ? TEXT("ModeRole") : TEXT("Maneuver"), Ability.Name, Cost, Left + Column * (Width + Gap), Y, Width, Height, Ready(Ability.Command), Mode == Ability.Command, Sky)});
        ++Column;
    }
    Hover.Add({ECombatCommandType::Rescue, Button(TEXT("ModeRescue"), TEXT("Rescue"), TEXT("R / adjacent downed ally"), Left + Column++ * (Width + Gap), Y, Width, Height, Ready(ECombatCommandType::Rescue), Mode == ECombatCommandType::Rescue, Safe)});
    Hover.Add({ECombatCommandType::Potion, Button(TEXT("ModePotion"), TEXT("Potion"), FString(TEXT("P / ")) + FCombatBattle::UsageTag(ECombatCommandType::Potion, *Active), Left + Column++ * (Width + Gap), Y, Width, Height, Ready(ECombatCommandType::Potion), Mode == ECombatCommandType::Potion, Safe)});
    for (const FExtra& E : Extras)
    {
        const FString Usage = FCombatBattle::UsageTag(E.Action, *Active);
        Hover.Add({E.Action, Button(E.Id, E.Label, FString(E.Key) + (Usage.IsEmpty() ? TEXT(" / Action") : TEXT(" / ") + Usage), Left + Column++ * (Width + Gap), Y, Width, Height, Ready(E.Action), Mode == E.Action, Sky)});
    }
    Button(TEXT("EndTurn"), TEXT("End Turn"), TEXT("Space"), Left + Column * (Width + Gap), Y, Width, Height, true, false, Gold);
    // SRD standard actions (#60): a compact row above the action bar. Help picks a foe; the rest act at once.
    {
        struct FStd { ECombatCommandType Action; const TCHAR* Id; const TCHAR* Label; };
        const FStd Standard[] = {{ECombatCommandType::Dash, TEXT("StdDash"), TEXT("Dash")}, {ECombatCommandType::Disengage, TEXT("StdDisengage"), TEXT("Disengage")},
            {ECombatCommandType::Dodge, TEXT("StdDodge"), TEXT("Dodge")}, {ECombatCommandType::Help, TEXT("StdHelp"), TEXT("Help")},
            {ECombatCommandType::Hide, TEXT("StdHide"), TEXT("Hide")}, {ECombatCommandType::Ready, TEXT("StdReady"), TEXT("Ready")}, {ECombatCommandType::OffHandAttack, TEXT("StdOffHand"), TEXT("Off-hand")}};
        const float StdWidth = 92, StdHeight = 30, StdLeft = W * 0.5f - (7 * (StdWidth + Gap) - Gap) * 0.5f;
        for (int32 Index = 0; Index < 7; ++Index)
        {
            Hover.Add({Standard[Index].Action, Button(Standard[Index].Id, Standard[Index].Label, FCombatBattle::CostTag(Standard[Index].Action),
                StdLeft + Index * (StdWidth + Gap), Y - 72, StdWidth, StdHeight, Ready(Standard[Index].Action), Mode == Standard[Index].Action, Sky)});
        }
    }
    if (Active->SpellSlots > 0 || Active->SlotsByLevel.ContainsByPredicate([](int32 N) { return N > 0; }))
    {
        const FString Slot = Player->SlotChoice() ? FString::Printf(TEXT("Spell slot: level %d  (= to change)"), Player->SlotChoice()) : FString(TEXT("Spell slot: lowest open  (= to upcast)"));
        Button(TEXT("Upcast"), TEXT("Upcast"), Slot, ChipLeft + 440, Chips - 6, 230, 30, true, Player->SlotChoice() > 0, Sky);
    }
    if (const auto* Build = FBuildCatalog::Find(Active->BuildId); Build && Build->SneakDie) { Text(TEXT("Sneak Attack: automatic with adjacent ally"), W * 0.5f, Y - 52, Safe, 0.7f, EAlign::Center); }
    for (const auto& Pair : Hover)
    {
        if (!Pair.Value) { continue; }
        FString Help = FCombatBattle::DescribeAction(Pair.Key);
        for (const auto& A : Abilities) { if (A.Command == Pair.Key) { Help = A.Name + TEXT(": ") + A.Description; } }
        Help = FCombatBattle::CostPrefix(Pair.Key, *Active, Snapshot.bActionAvailable) + Help; // #79 economy tag
        const float TipWidth = FMath::Min(640.0f, Measure(Help, 0.72f).X + 24);
        Panel(W * 0.5f - TipWidth * 0.5f, Y - 66, TipWidth, 30, Sky, 0.95f, false);
        Text(Help, W * 0.5f, Y - 60, Parchment, 0.72f, EAlign::Center, TipWidth - 16);
        const FString Spell = FCombatBattle::SpellTag(Pair.Key);
        if (!Spell.IsEmpty())
        {
            const float SpellWidth = FMath::Min(640.0f, Measure(Spell, 0.66f).X + 24);
            Panel(W * 0.5f - SpellWidth * 0.5f, Y - 96, SpellWidth, 26, Sky, 0.9f, false);
            Text(Spell, W * 0.5f, Y - 91, Muted, 0.66f, EAlign::Center, SpellWidth - 16);
        }
    }
    Text(TEXT("Right-drag or Q/E: rotate  ·  Wheel: zoom  ·  Tab: skip  ·  T: speed  ·  ?: help"), W * 0.5f, H - 20, WithAlpha(Muted, 0.8f), 0.6f, EAlign::Center);
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
        const FActionForecast Forecast = Rules.Forecast(Player->Actor(), Player->Action(), TargetId, Player->SlotChoice());
        if (!Target || !Forecast.bValid) { return; }
        Lines.Add({FString::Printf(TEXT("%s  ·  %s  ·  HP %d/%d"), *Target->Name, *Rules.ArmorClassText(TargetId), Target->HP, Target->MaxHP), ExpeditionStyle::UnitColor(*Target)});
        if (const FString Status = Rules.StatusText(TargetId); !Status.IsEmpty()) { Lines.Add({Status, Gold}); }
        if (Forecast.bAttackRoll)
        {
            const FLinearColor Chance = Forecast.HitChancePercent >= 65 ? Safe : Forecast.HitChancePercent >= 40 ? Gold : Danger;
            Lines.Add({FString::Printf(TEXT("%d%% to hit%s"), Forecast.HitChancePercent, Forecast.RollMode == ERollMode::Normal ? TEXT("") :
                Forecast.RollMode == ERollMode::Advantage ? TEXT("  (advantage)") : TEXT("  (disadvantage)")), Chance});
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
        int32 Cost = 0, Difficult = 0;
        for (const FIntPoint& Cell : Player->PreviewPath()) { const bool bRough = Rules.IsDifficultTerrain(Cell); Cost += bRough ? 2 : 1; Difficult += bRough ? 1 : 0; }
        Lines.Add({FString::Printf(TEXT("Move %d square%s  ·  %d left after"), Steps, Steps == 1 ? TEXT("") : TEXT("s"), Rules.Snapshot().MovementLeft - Cost), Sky});
        if (Difficult > 0) { Lines.Add({FString::Printf(TEXT("Difficult terrain: %d square%s cost double"), Difficult, Difficult == 1 ? TEXT("") : TEXT("s")), Gold}); }
        for (const int32 ThreatId : Rules.OpportunityThreats(Player->Actor(), Player->PreviewPath()))
        {
            if (const FCombatantState* Threat = Rules.Find(ThreatId)) { Lines.Add({FString::Printf(TEXT("Provokes an opportunity attack from %s"), *Threat->Name), Danger}); }
        }
    }
    else if (const FCombatantState* Unit = Rules.Find(Player->HoveredUnit()))
    {
        if (Unit->Life == ECombatLife::Dead) { return; }
        Lines.Add({FString::Printf(TEXT("%s  ·  %s"), *Unit->Name, *Unit->Role), ExpeditionStyle::UnitColor(*Unit)});
        Lines.Add({FString::Printf(TEXT("HP %d/%d  ·  %s  ·  reach %d"), Unit->HP, Unit->MaxHP, *Rules.ArmorClassText(Unit->Id), FCombatBattle::ActionRange(*Unit, ECombatCommandType::Attack)), Parchment});
        if (const FString Status = Rules.StatusText(Unit->Id); !Status.IsEmpty()) { Lines.Add({Status, Gold}); }
        if (Rules.IsDifficultTerrain(Unit->Cell)) { Lines.Add({TEXT("Standing in difficult terrain"), Muted}); }
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
    const float Width = 700, Height = 570;
    const float X = W * 0.5f - Width * 0.5f, Y = H * 0.5f - Height * 0.5f;
    Panel(X, Y, Width, Height, Gold, 0.98f);
    Text(View->HasRun() ? (View->Progress().Phase == EExpeditionPhase::Victory ? TEXT("VICTORY / NEXT PARTY") : TEXT("DEFEAT / NEXT PARTY")) : TEXT("NEXT EXPEDITION / PARTY BUILDER"), W * 0.5f, Y + 18, Gold, 1.2f, EAlign::Center);
    Text(TEXT("1-4 select slot / Up-Down class / Left-Right kit / G familiar preset"), W * 0.5f, Y + 52, Muted, 0.7f, EAlign::Center);
    for (int32 I = 0; I < Player->NextParty().Num(); ++I)
    {
        const auto& P = Player->NextParty()[I];
        FCombatantState Preview; FBuildCatalog::Create(P.BuildId, P.Id, P.Name, Preview);
        const float CardX = X + 18 + I * 166;
        Button(FName(*FString::Printf(TEXT("Party%d"), I)), TEXT(""), TEXT(""), CardX, Y + 84, 160, 78, true, Player->BuilderSlot() == I, Sky);
        Text(P.Name + TEXT(" / ") + Preview.Role, CardX + 80, Y + 92, Parchment, 0.72f, EAlign::Center, 148);
        float LabelY = Y + 117;
        for (const auto& Line : Wrap(FBuildCatalog::DisplayName(Preview).RightChop(Preview.Role.Len() + 3), 148, 0.72f))
        { Text(Line, CardX + 80, LabelY, Muted, 0.72f, EAlign::Center); LabelY += 18; }
    }
    const auto& Choice = Player->NextParty()[Player->BuilderSlot()];
    FCombatantState Hero; FBuildCatalog::Create(Choice.BuildId, Choice.Id, Choice.Name, Hero);
    Text(FBuildCatalog::DisplayName(Hero), X + 24, Y + 170, Parchment, 0.95f);
    float Row = Y + 208;
    for (const auto& A : FBuildCatalog::Abilities(Hero))
    {
        Text(A.Name, X + 24, Row, Gold, 0.82f); Row += 21;
        for (const auto& Line : Wrap(A.Description, Width - 48, 0.72f)) { Text(Line, X + 24, Row, Parchment, 0.72f); Row += 19; }
        Row += 12;
    }
    const auto* Build = FBuildCatalog::Find(Hero.BuildId);
    if (Build->SneakDie) { Text(TEXT("Passive: Sneak Attack with adjacent ally"), X + 24, Row, Safe, 0.75f); }
    if (Build->CriticalThreshold == 19) { Text(TEXT("Passive: critical hits on 19-20"), X + 24, Row, Safe, 0.75f); }
    Button(TEXT("ClassNext"), TEXT("Next class"), TEXT("Up / Down"), X + 24, Y + 370, 200, 44, true);
    Button(TEXT("BuildNext"), TEXT("Next compatible kit"), TEXT("Left / Right"), X + 240, Y + 370, 220, 44, true);
    Button(TEXT("Preset"), TEXT("Familiar party"), TEXT("G"), X + 476, Y + 370, 200, 44, true);
    Button(TEXT("New"), TEXT("New Expedition"), TEXT("N"), X + 24, Y + 434, 200, 54, true, false, Gold);
    Button(TEXT("Resume"), TEXT("Continue saved party"), TEXT("O"), X + 240, Y + 434, 220, 54, true, false, Sky);
    Button(TEXT("Records"), TEXT("Past Expeditions"), TEXT("H"), X + 476, Y + 434, 200, 54, true);
    const auto& Messages = View->Messages();
    if (!Messages.IsEmpty()) { Text(Messages.Last(), X + 24, Y + 510, Muted, 0.7f, EAlign::Left, Width - 48); }

}

void AExpeditionHUD::DrawRoutes()
{
    const FExpeditionProgress& Run = View->Progress();
    const float CardWidth = 340, CardHeight = 250, Gap = 30;
    const float Top = H * 0.5f - CardHeight * 0.5f - 10;
    Text(TEXT("CHOOSE YOUR PATH"), W * 0.5f, Top - 66, Gold, 1.5f, EAlign::Center);
    Text(FString::Printf(TEXT("Encounter %d of %d  ·  Injuries and losses carry forward  ·  No automatic rest"), Run.EncountersCompleted + 1, FExpeditionRun::EncounterCount), W * 0.5f, Top - 30, Muted, 0.76f, EAlign::Center);
    for (int32 Index = 0; Index < Run.Offers.Num() && Index < 2; ++Index)
    {
        const FExpeditionRoute& Route = Run.Offers[Index];
        const float X = W * 0.5f + (Index == 0 ? -CardWidth - Gap * 0.5f : Gap * 0.5f);
        const FLinearColor Accent = Route.bDangerous ? Danger : Safe;
        const bool bHover = Hovered(X, Top, CardWidth, CardHeight);
        Panel(X, Top - (bHover ? 4 : 0), CardWidth, CardHeight, Accent, bHover ? 0.98f : 0.9f);
        const float Y = Top - (bHover ? 4 : 0);
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
        Buttons.Add({FName(*FString::Printf(TEXT("Route%d"), Index)), FBox2D(FVector2D(X, Top) * S, FVector2D(X + CardWidth, Top + CardHeight) * S), true});
        Text(bHover ? TEXT("Click to set out") : TEXT(""), X + CardWidth * 0.5f, Y + CardHeight - 30, Accent, 0.8f, EAlign::Center);
    }
}

void AExpeditionHUD::DrawReward()
{
    const FExpeditionProgress& Run = View->Progress();
    const FCombatBattle& Rules = View->Rules();
    const float Width = 520, Height = 210;
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
    Button(TEXT("Claim"), bReady ? FString::Printf(TEXT("Give to %s"), *Selected->Name) : TEXT("Select a survivor"), TEXT("L"), W * 0.5f - 250, Y + 140, 240, 52, bReady, false, Gold);
    const bool bCanRest = bReady && Selected->HitDiceSpent < Selected->Level && Selected->HP < Selected->MaxHP;
    Button(TEXT("ShortRest"), TEXT("Short rest"), bReady ? FString::Printf(TEXT("spend a hit die (%d left)"), Selected->Level - Selected->HitDiceSpent) : TEXT("hit dice"),
        W * 0.5f + 10, Y + 140, 240, 52, bCanRest, false, Sky);
    Text(FString::Printf(TEXT("Purse: %s"), *FCoinRules::Format(Run.Coins)), X + Width - 16, Y + 16, Gold, 0.74f, EAlign::Right);
    // Rest-point activities: one try each, by the selected survivor.
    const int32 Used = Run.History.IsEmpty() ? 0 : Run.History.Last().ActivitiesUsed;
    const int32 Count = static_cast<int32>(ECampActivity::Count);
    const int32 PerRow = 5;
    const float ActWidth = 160, ActLeft = W * 0.5f - (FMath::Min(Count, PerRow) * (ActWidth + 8) - 8) * 0.5f;
    for (int32 Index = 0; Index < Count; ++Index)
    {
        const bool bOpen = bReady && !(Used & (1 << Index));
        Button(FName(*FString::Printf(TEXT("Camp_%d"), Index)), FExpeditionRun::CampActivityName(static_cast<ECampActivity>(Index)), bOpen ? TEXT("once per rest") : TEXT("done"),
            ActLeft + (Index % PerRow) * (ActWidth + 8), Y + Height + 8 + (Index / PerRow) * 48, ActWidth, 40, bOpen, false, Sky);
    }
}

void AExpeditionHUD::DrawCamp()
{
    const FCombatBattle& Rules = View->Rules();
    const float Width = 560, Height = 250;
    const float X = W * 0.5f - Width * 0.5f, Y = H * 0.5f - Height * 0.5f - 30;
    Panel(X, Y, Width, Height, FLinearColor(1.0f, 0.6f, 0.3f), 0.94f);
    Text(TEXT("MAKE CAMP?"), W * 0.5f, Y + 16, FLinearColor(1.0f, 0.7f, 0.4f), 1.4f, EAlign::Center);
    Text(TEXT("Once per expedition. If you press on, the chance is gone."), W * 0.5f, Y + 56, Muted, 0.76f, EAlign::Center);
    Text(FExpeditionRun::RecoveryDescription(), W * 0.5f, Y + 82, Parchment, 0.74f, EAlign::Center, Width - 40);
    float Row = Y + 112;
    FString Status;
    for (const FCombatantState& Unit : Rules.Snapshot().Encounter.Combatants)
    {
        if (Unit.Side != ECombatSide::Party) { continue; }
        Status += (Status.IsEmpty() ? TEXT("") : TEXT("    ")) + (Unit.Life == ECombatLife::Dead ? Unit.Name + TEXT(" (fallen)") : FString::Printf(TEXT("%s %d/%d"), *Unit.Name, Unit.HP, Unit.MaxHP));
    }
    Text(Status, W * 0.5f, Row, Parchment, 0.76f, EAlign::Center, Width - 40);
    Button(TEXT("Camp"), TEXT("Rest by the fire"), TEXT("C"), W * 0.5f - 250, Y + 160, 240, 56, true, false, FLinearColor(1.0f, 0.6f, 0.3f));
    Button(TEXT("SkipCamp"), TEXT("Press on"), TEXT("V  ·  forfeit the rest"), W * 0.5f + 10, Y + 160, 240, 56, true, false, Muted);
}

void AExpeditionHUD::DrawEnding()
{
    DrawTitle();

}

void AExpeditionHUD::DrawRecords()
{
    const float Width = 680, Height = 330;
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
            Rect(FLinearColor(1, 1, 1, 0.04f), X + 20, Row - 4, Width - 40, 38);
            Text(bWon ? TEXT("RECOVERED") : TEXT("LOST"), X + 32, Row + 4, bWon ? Gold : Danger, 0.78f);
            Text(FString::Printf(TEXT("Seed %d  ·  %s"), Record.Seed, *Record.Summary), X + 150, Row + 4, Parchment, 0.74f, EAlign::Left, Width - 190);
            Row += 44;
        }
    }
    Button(TEXT("Older"), TEXT("Older"), TEXT("Page Up"), X + 24, Y + Height - 64, 130, 46);
    Button(TEXT("Newer"), TEXT("Newer"), TEXT("Page Down"), X + 164, Y + Height - 64, 130, 46);
    Button(TEXT("Close"), TEXT("Close"), TEXT("H / Esc"), X + Width - 154, Y + Height - 64, 130, 46, true, false, Gold);
    Blockers.Add(FBox2D(FVector2D(0, 0), FVector2D(W, H) * S));
}

void AExpeditionHUD::DrawHelp()
{
    const float Width = 700, Height = 480;
    const float X = W * 0.5f - Width * 0.5f, Y = H * 0.5f - Height * 0.5f;
    Rect(FLinearColor(0, 0, 0, 0.5f), 0, 0, W, H);
    Panel(X, Y, Width, Height, Sky, 0.98f);
    Text(TEXT("HOW TO PLAY"), X + 24, Y + 18, Sky, 1.2f);
    const TCHAR* Left[] = {
        TEXT("Click a glowing tile to move"), TEXT("Click a marked enemy to attack"), TEXT("A  Attack / move mode"), TEXT("F / M  First / second catalog special"),
        TEXT("R  Rescue a downed ally"), TEXT("P  Drink or give a potion"), TEXT("Y / U  Burning Hands / Thunderwave"), TEXT("I / W  Shove / Grapple"), TEXT("Space  End turn"), TEXT("1-4  Select adventurer"),
        TEXT("Arrows + Enter  Keyboard cursor")};
    const TCHAR* Right[] = {
        TEXT("Right-drag or Q/E  Rotate"), TEXT("Wheel  Zoom"), TEXT("Tab  Skip the current animation"), TEXT("T  Animation speed"),
        TEXT("=  Spell slot level (upcast)"), TEXT("Z / X  Choose route"), TEXT("L  Give loot to the selected survivor"), TEXT("C / V  Camp or press on"), TEXT("K / O  Save / resume"),
        TEXT("H  Past expeditions  ·  ?  This help")};
    for (int32 Index = 0; Index < FMath::Max<int32>(UE_ARRAY_COUNT(Left), UE_ARRAY_COUNT(Right)); ++Index)
    {
        if (Index < UE_ARRAY_COUNT(Left)) { Text(Left[Index], X + 32, Y + 66 + Index * 28.0f, Parchment, 0.8f); }
        if (Index < UE_ARRAY_COUNT(Right)) { Text(Right[Index], X + Width * 0.5f + 12, Y + 66 + Index * 28.0f, Parchment, 0.8f); }
    }
    Text(TEXT("Downed allies die after 3 of their turns unless rescued. Rescue before the last enemy falls."), W * 0.5f, Y + Height - 82, Gold, 0.72f, EAlign::Center, Width - 40);
    Button(TEXT("Close"), TEXT("Close"), TEXT("Esc"), X + Width - 154, Y + Height - 60, 130, 44, true, false, Sky);
    Blockers.Add(FBox2D(FVector2D(0, 0), FVector2D(W, H) * S));
}
