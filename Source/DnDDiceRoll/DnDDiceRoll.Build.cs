using UnrealBuildTool;

public class DnDDiceRoll : ModuleRules
{
    public DnDDiceRoll(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "InputCore", "Json" });
        PrivateDependencyModuleNames.AddRange(new[] { "RenderCore", "RHI" });
    }
}
