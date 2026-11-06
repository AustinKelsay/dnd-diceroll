using UnrealBuildTool;

public class DnDDiceRoll : ModuleRules
{
    public DnDDiceRoll(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        // PR2 standard-action library touch so UBT rebuilds after adding Rules/*.cpp
        PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "InputCore", "Json" });
        PrivateDependencyModuleNames.AddRange(new[] { "RenderCore" });
    }
}
