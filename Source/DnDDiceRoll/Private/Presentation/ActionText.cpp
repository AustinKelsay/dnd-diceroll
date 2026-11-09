#include "Presentation/ActionText.h"
#include "Presentation/ExpeditionPlayback.h"

namespace ExpeditionStyle
{
FString CostName(EActionCostKind Cost)
{
    switch (Cost)
    {
    case EActionCostKind::Action: return TEXT("Action");
    case EActionCostKind::BonusAction: return TEXT("Bonus action");
    case EActionCostKind::Movement: return TEXT("Move");
    case EActionCostKind::None: return TEXT("No cost");
    }
    return FString();
}

FString ResourceName(EActionResource Resource)
{
    switch (Resource)
    {
    case EActionResource::Kit: return TEXT("Kit");
    case EActionResource::Slot: return TEXT("Slot");
    case EActionResource::Wind: return TEXT("Wind");
    case EActionResource::Maneuver: return TEXT("Maneuver");
    case EActionResource::Rage: return TEXT("Rage");
    case EActionResource::Potion: return TEXT("Potion");
    case EActionResource::None: return TEXT("No resource");
    }
    return FString();
}

FString CostPillText(const FActionFacts& Facts)
{
    switch (Facts.Cost)
    {
    case EActionCostKind::Action: return TEXT("Action");
    case EActionCostKind::BonusAction: return TEXT("Bonus");
    case EActionCostKind::Movement: return TEXT("Move");
    case EActionCostKind::None: return FString();
    }
    return FString();
}

FString UsagePillText(const FActionFacts& Facts)
{
    if (Facts.Cost == EActionCostKind::None) { return FString(); }
    TArray<FString> Parts;
    for (const FActionResourceCost& Cost : Facts.Resources)
    {
        FString Part = FString::Printf(TEXT("%d %s"), Cost.Amount, *ResourceName(Cost.Resource));
        Part += Cost.Cap > 0 ? FString::Printf(TEXT(" · %d/%d"), Cost.Held, Cost.Cap) : FString::Printf(TEXT(" · %d"), Cost.Held);
        Parts.Add(Part);
    }
    FString Text = Parts.IsEmpty() ? FString() : FString::Join(Parts, TEXT(" + "));
    if (Facts.bRequiresRage) { Text += Text.IsEmpty() ? TEXT("Needs Rage") : TEXT(" | Needs Rage"); }
    if (Facts.bForbidsRage) { Text += Text.IsEmpty() ? TEXT("Not while raging") : TEXT(" | Not while raging"); }
    return Text.IsEmpty() ? FString(TEXT("At will")) : Text;
}

FString CampRecoveryText(const FActionResourceCost& Cost)
{
    if (Cost.CampRecovery == 0) { return TEXT("Not refilled by camp"); }
    if (Cost.CampRecovery < 0) { return FString(); }
    if (Cost.Cap > 0 && Cost.CampRecovery >= Cost.Cap) { return TEXT("Camp restores all"); }
    return FString::Printf(TEXT("Camp restores %d"), Cost.CampRecovery);
}

FString BlockerText(const FActionBlocker& Blocker)
{
    switch (Blocker.Reason)
    {
    case EActionBlocker::BattleInactive: return TEXT("The battle is over");
    case EActionBlocker::NotYourTurn: return TEXT("Not this unit's turn");
    case EActionBlocker::ActorDown: return TEXT("Down and cannot act");
    case EActionBlocker::NotInKit: return TEXT("Not in this kit");
    case EActionBlocker::ActionSpent: return TEXT("Action already used");
    case EActionBlocker::BonusSpent: return TEXT("Bonus action already used");
    case EActionBlocker::NoMovementLeft: return TEXT("No movement left");
    case EActionBlocker::NoOpenCell: return TEXT("No open cell to move to");
    case EActionBlocker::ResourceShort:
        return FString::Printf(TEXT("Need %d %s, have %d"), Blocker.Needed, *ResourceName(Blocker.Resource), Blocker.Held);
    case EActionBlocker::StanceRequired: return TEXT("Needs Rage active");
    case EActionBlocker::StanceBlocks: return TEXT("Already raging");
    case EActionBlocker::NoWoundedTarget: return TEXT("No wounded target");
    case EActionBlocker::NoDownedTarget: return TEXT("No downed ally");
    case EActionBlocker::OutOfRange: return FString::Printf(TEXT("Out of range (%d)"), Blocker.Range);
    case EActionBlocker::SightBlocked: return TEXT("Sight is blocked");
    case EActionBlocker::NoEligibleTarget: return TEXT("No eligible target");
    case EActionBlocker::Count: break;
    }
    return FString();
}

FString TooltipHeadline(const FActionFacts& Facts)
{
    const FString Name = Facts.Name.IsEmpty() ? ActionName(Facts.Command) : Facts.Name;
    return Facts.Cost == EActionCostKind::None ? Name : FString::Printf(TEXT("%s · %s"), *Name, *CostName(Facts.Cost));
}
}
