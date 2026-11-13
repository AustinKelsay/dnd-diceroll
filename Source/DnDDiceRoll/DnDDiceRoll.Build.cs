using System.IO;
using UnrealBuildTool;

public class DnDDiceRoll : ModuleRules
{
    public DnDDiceRoll(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "InputCore", "Json" });
        PrivateDependencyModuleNames.AddRange(new[] { "RenderCore", "Slate", "SlateCore" });
        if (Target.IsInPlatformGroup(UnrealPlatformGroup.Linux) && Target.bCompileAgainstApplicationCore)
        {
            // The Linux platform application keeps which window holds SDL keyboard focus; a left press that outlives it is spent (see AExpeditionPlayerController::KeyboardFocusLost).
            // Only its header is used (an inline accessor), so only SDL's include path is added, never the SDL library a second time.
            PrivateDependencyModuleNames.Add("ApplicationCore");
            PrivateIncludePaths.Add(Path.Combine(EngineDirectory, "Source", "ThirdParty", "SDL3", "SDL-gui-backend", "include"));
            PrivateDefinitions.Add("SDL_WITH_EPIC_EXTENSIONS=1");
        }
    }
}
