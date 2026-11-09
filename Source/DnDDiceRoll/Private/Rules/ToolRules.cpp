#include "Rules/ToolRules.h"

FToolCheck FToolRules::Check(int32 AbilityModifier, int32 ProficiencyBonus, bool bHasTool, bool bToolProficient, bool bSkillProficient, bool bRequiresTool)
{
    FToolCheck Result;
    if (bRequiresTool && !bHasTool) { return Result; }
    Result.bAllowed = true;
    const bool bUsingTool = bHasTool && bToolProficient;
    Result.Modifier = AbilityModifier + (bUsingTool ? ProficiencyBonus : 0);
    Result.Mode = bUsingTool && bSkillProficient ? ERollMode::Advantage : ERollMode::Normal;
    return Result;
}

bool FToolRules::HealersKitStabilize(int32& UsesLeft)
{
    if (UsesLeft <= 0) { return false; }
    --UsesLeft;
    return true;
}

bool FToolRules::CanBrewHealing(bool bHerbalismProficient, bool bHasKit) { return bHerbalismProficient && bHasKit; }
