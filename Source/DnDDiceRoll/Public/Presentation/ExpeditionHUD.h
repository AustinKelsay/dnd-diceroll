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
    TArray<FName> MenuButtons() const;
    /**
     * Targeting the last DrawHUD asked to place: one per target chip and per reach or blocked-sight
     * marker. Counted before screen projection, so a headless test without a viewport sees them.
     * Both are zero whenever an overlay owns the screen.
     */
    int32 TargetChipsRequested() const { return ChipsRequested; }
    int32 ReachMarkersRequested() const { return ReachRequested; }
    /** Every registered button, in viewport pixels, as the last DrawHUD placed them (#146). */
    TArray<FBox2D> ButtonRects() const;
    FVector2D ViewportSize() const { return FVector2D(W * S, H * S); }
    /**
     * Fraction of the viewport the last frame's persistent panels and buttons cover, overlaps counted once.
     * Transient layers (tooltips, forecast, dice, banner, target chips, hover detail) are excluded.
     */
    double PersistentCoverage() const;
    /** Whether the last draw requested keyboard inspection brackets, before projection. */
    bool KeyboardCursorRequested() const { return bCursorRequested; }
    /** The combatant whose full status panel the last draw showed, or INDEX_NONE. */
    int32 PartyDetailUnit() const { return DetailUnitId; }
private:
    int32 DetailUnitId = INDEX_NONE;
    int32 ChipsRequested = 0;
    int32 ReachRequested = 0;
    bool bCursorRequested = false;
    enum class EAlign : uint8 { Left, Center, Right };
    struct FButtonArea { FName Id; FBox2D Box; bool bEnabled = true; bool bTransient = false; };
    bool bTransientButtons = false; // Set while the open Menu draws: its buttons are excluded from the persistent budget.
    TArray<FButtonArea> Buttons;
    TArray<FBox2D> Blockers;
    TArray<FBox2D> Persistent; // Always-on panels in design units, for the coverage budget (#146).
    TArray<FBox2D> ChipExclusions; // Visible HUD panels in design units, independent of input blocking.
    float S = 1.0f;
    float W = 1280.0f;
    float H = 720.0f;
    FVector2D Mouse = FVector2D(-1, -1);
    float Clock = 0.0f;
    // Combat log fade (#146): the log shows for LogLingerSeconds after its last change or hover.
    int32 LogCount = -1;
    FString LogLast;
    float LogChangedAt = -100.0f;
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
    bool Button(FName Id, const FString& Label, const FString& Sub, float X, float Y, float Width, float Height, bool bEnabled = true, bool bSelected = false, FLinearColor Accent = FLinearColor(0.95f, 0.78f, 0.4f), float LabelScale = 0.92f);
    void Persist(float X, float Y, float Width, float Height) { Persistent.Add(FBox2D(FVector2D(X, Y), FVector2D(X + Width, Y + Height))); }
    float StatusStripWidth() const;
    bool Project(const FVector& World, FVector2D& Screen) const;
    void Die(FVector2D Center, float Radius, int32 Sides, int32 Value, FLinearColor Color, float Spin, float Alpha, bool bLanded);

    // Layers.
    void DrawWorldOverlays();
    void DrawTargetChips();
    void DrawTopBar();
    void DrawMenuPanel();
    void DrawInitiative();
    void DrawParty();
    void DrawPartyCard(const FCombatantState& Unit, int32 Index, float Y);
    void DrawPartyDetail(const FCombatantState& Unit, float X, float Y);
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
    void DrawReturnConfirm();
};
