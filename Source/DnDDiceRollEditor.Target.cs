using UnrealBuildTool;

public class DnDDiceRollEditorTarget : TargetRules
{
    public DnDDiceRollEditorTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Editor;
        DefaultBuildSettings = BuildSettingsVersion.Latest;
        IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
        ExtraModuleNames.Add("DnDDiceRoll");
    }
}
