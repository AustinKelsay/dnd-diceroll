#include "Presentation/ExpeditionSpike.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

const FExpeditionSpike& FExpeditionSpike::Get()
{
    static const FExpeditionSpike Value = []
    {
        FExpeditionSpike S;
        const TCHAR* Line = FCommandLine::Get();
        S.bWorld = FParse::Param(Line, TEXT("SpikeWorld"));
        S.bGrid = FParse::Param(Line, TEXT("SpikeGrid"));
        S.bLumen = FParse::Param(Line, TEXT("SpikeLumen"));
        float Cell = 0.0f;
        if (FParse::Value(Line, TEXT("SpikeCell="), Cell) && Cell >= 60.0f && Cell <= 300.0f) { S.Cell = Cell; }
        else if (S.bWorld) { S.Cell = 150.0f; }
        FString Text;
        if (FParse::Value(Line, TEXT("SpikeCam="), Text)) { S.Cam = Text == TEXT("b") ? ECam::B : Text == TEXT("zoom") ? ECam::Zoom : ECam::Today; }
        if (FParse::Value(Line, TEXT("SpikeFilter="), Text)) { S.Filter = Text == TEXT("trilinear") ? EFilter::Trilinear : Text == TEXT("sharp") ? EFilter::Sharp : EFilter::Nearest; }
        if (FParse::Value(Line, TEXT("SpikeLight="), Text)) { S.Light = Text == TEXT("key") ? ELight::Key : ELight::Today; }
        FParse::Value(Line, TEXT("SpikeFill="), S.Fill);
        FParse::Value(Line, TEXT("SpikeRim="), S.Rim);
        FParse::Value(Line, TEXT("SpikeGlowK="), S.GlowK);
        return S;
    }();
    return Value;
}

float FExpeditionSpike::DistanceScale() const
{
    // Same framing for FOV 40 as FOV 50: distance grows by tan(25)/tan(20).
    return (Cell / 110.0f) * (Cam == ECam::Today ? 1.0f : 1.2812f);
}

FString FExpeditionSpike::Describe() const
{
    return FString::Printf(TEXT("world=%d cell=%.0f cam=%d filter=%d light=%d grid=%d lumen=%d"), bWorld, Cell, (int32)Cam, (int32)Filter, (int32)Light, bGrid, bLumen);
}
