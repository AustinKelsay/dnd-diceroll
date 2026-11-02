#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "ExpeditionHUD.generated.h"

class AExpeditionBattleView;
class AExpeditionPlayerController;
struct FCombatantState;

/**
 * Canvas interface drawn in a 1280-wide design space and scaled to the viewport.
 * Buttons are registered while drawing; the controller asks for the button under a
 * click. Every value shown comes from the session snapshot or the playback display.
 */
UCLASS()
class DNDDICEROLL_API AExpeditionHUD : public AHUD
{
    GENERATED_BODY()
public:
    virtual void DrawHUD() override;
    bool ButtonAt(FVector2D ScreenPosition, FName& Id) const;
    /** Panels that should swallow clicks instead of passing them to the board. */
    bool CoversBoard(FVector2D ScreenPosition) const;
private:
    enum class EAlign : uint8 { Left, Center, Right };
    struct FButtonArea { FName Id; FBox2D Box; bool bEnabled = true; };
    TArray<FButtonArea> Buttons;
    TArray<FBox2D> Blockers;
    float S = 1.0f;
    float W = 1280.0f;
    float H = 720.0f;
    FVector2D Mouse = FVector2D(-1, -1);
    float Clock = 0.0f;
    TWeakObjectPtr<AExpeditionPlayerController> Player;
    TWeakObjectPtr<const AExpeditionBattleView> View;

    // Primitives in design units.
    void Rect(FLinearColor Color, float X, float Y, float Width, float Height);
    void Frame(FLinearColor Color, float X, float Y, float Width, float Height, float Thickness = 1.0f);
    void Panel(float X, float Y, float Width, float Height, FLinearColor Accent = FLinearColor(0.85f, 0.7f, 0.4f), float Opacity = 0.9f, bool bBlock = true);
    FVector2D Measure(const FString& Text, float Scale) const;
    void Text(const FString& Value, float X, float Y, FLinearColor Color = FLinearColor::White, float Scale = 1.0f, EAlign Align = EAlign::Left, float MaxWidth = 0.0f);
    TArray<FString> Wrap(const FString& Value, float Width, float Scale) const;
    void Polygon(FVector2D Center, float Radius, int32 Sides, float Rotation, FLinearColor Color, float Squash = 1.0f);
    void Circle(FVector2D Center, float Radius, FLinearColor Color) { Polygon(Center, Radius, 28, 0.0f, Color); }
    void Line(FVector2D A, FVector2D B, FLinearColor Color, float Thickness = 1.0f);
    void Bar(float X, float Y, float Width, float Height, float Fraction, float Trail, FLinearColor Fill);
    bool Hovered(float X, float Y, float Width, float Height) const;
    bool Button(FName Id, const FString& Label, const FString& Sub, float X, float Y, float Width, float Height, bool bEnabled = true, bool bSelected = false, FLinearColor Accent = FLinearColor(0.95f, 0.78f, 0.4f));
    bool Project(const FVector& World, FVector2D& Screen) const;
    void Die(FVector2D Center, float Radius, int32 Sides, int32 Value, FLinearColor Color, float Spin, float Alpha, bool bLanded);

    // Layers.
    void DrawWorldOverlays();
    void DrawTopBar();
    void DrawInitiative();
    void DrawParty();
    void DrawPartyCard(const FCombatantState& Unit, int32 Index, float Y);
    void DrawActionBar();
    void DrawForecast();
    void DrawDice();
    void DrawLog();
    void DrawBanner();
    void DrawTitle();
    void DrawRoutes();
    void DrawReward();
    void DrawCamp();
    void DrawEnding();
    void DrawRecords();
    void DrawHelp();
};
