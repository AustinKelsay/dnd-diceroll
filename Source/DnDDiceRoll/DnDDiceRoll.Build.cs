using UnrealBuildTool;

public class DnDDiceRoll : ModuleRules
{
    public DnDDiceRoll(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        // PR3 ruleset schema5 touch so UBT rebuilds after adding Rules/*.cpp
        PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "InputCore", "Json" });
        PrivateDependencyModuleNames.AddRange(new[] { "RenderCore" });
    }
}
