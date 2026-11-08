#pragma once

#include "CoreMinimal.h"

class AExpeditionBattleView;
class AExpeditionPlayerController;

/** What a capture shot shows. Battle and Targeting need an encounter; Overview needs the tabletop; Start leaves the run. */
enum class ECaptureMode : uint8 { Battle, Targeting, Overview, Start };

/** One fixed pose from the poses file. A Distance of zero means the board's framed default. */
struct DNDDICEROLL_API FCaptureShot
{
    FString Name;
    ECaptureMode Mode = ECaptureMode::Battle;
    bool bHasCamera = false;
    float Yaw = 0.0f;
    float Distance = 0.0f;
};

struct DNDDICEROLL_API FCapturePoses
{
    /** Rendered frames to wait after each pose change before the screenshot. */
    int32 SettleFrames = 60;
    TArray<FCaptureShot> Shots;
};

namespace ExpeditionCapture
{
/** The camera distance range a battle allows; poses outside it are rejected. */
constexpr float MinDistance = 900.0f;
constexpr float MaxDistance = 3200.0f;
constexpr int32 MaxSettleFrames = 1200;

DNDDICEROLL_API const TCHAR* ModeName(ECaptureMode Mode);
/** Pure: validates a poses document (version 1) and fills Out, or fails with a message naming the problem. */
DNDDICEROLL_API bool ParsePoses(const FString& Json, FCapturePoses& Out, FString& Error);
}

/**
 * Presentation-only capture run, active only when the command line has -ExpeditionCapture=<poses.json>.
 * It resumes the checkpoint under -ExpeditionSaveRoot, then for each pose sets the mode and camera,
 * waits a fixed number of frames, saves a screenshot under -ExpeditionCaptureOut and exits. It never
 * submits a combat command and never writes the checkpoint.
 */
class DNDDICEROLL_API FExpeditionCaptureDirector
{
public:
    /** Null unless the command line asks for a capture run. */
    static TSharedPtr<FExpeditionCaptureDirector> TryCreate(AExpeditionPlayerController* Controller, AExpeditionBattleView* View);
    /** Once per rendered frame, from the player controller. */
    void Tick();

private:
    enum class EStage : uint8 { Loading, Next, Settling, Saving, Done };
    struct FResult
    {
        FCaptureShot Shot;
        FString File, Status, Reason;
        float Yaw = 0.0f, Distance = 0.0f;
        FVector Location = FVector::ZeroVector;
        FRotator Rotation = FRotator::ZeroRotator;
        float Fov = 0.0f;
    };

    TWeakObjectPtr<AExpeditionPlayerController> Controller;
    TWeakObjectPtr<AExpeditionBattleView> View;
    FCapturePoses Poses;
    FString OutputDir, PosesPath, Fixture;
    EStage Stage = EStage::Loading;
    int32 NextShot = 0;
    int32 Frames = 0;
    int32 StageFrames = 0;
    bool bResumed = false;
    bool bFailed = false;
    FString Failure;
    FIntPoint Resolution = FIntPoint::ZeroValue;
    /** Read once after the resume; the start screen later leaves the run. */
    int32 Seed = 0;
    FIntPoint Layout = FIntPoint::ZeroValue;
    TArray<FResult> Results;
    FResult Pending;

    bool Idle() const;
    bool Apply(const FCaptureShot& Shot, FString& Skip);
    void Fail(const FString& Message);
    void Finish();
    void WriteManifest() const;
};
