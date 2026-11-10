#include "Presentation/CombatHints.h"
#include "Presentation/ActionText.h"

bool FHintObservation::Holds(ECombatHint Hint) const
{
    switch (Hint)
    {
    case ECombatHint::MoveSelectConfirm: return bAtRest;
    case ECombatHint::ScarceResources: return bAtRest && bResourceCue;
    case ECombatHint::DownedRescue: return bAtRest && bDowned;
    case ECombatHint::EndingTurn: return bAtRest && bOnlyEndTurn;
    case ECombatHint::Count: break;
    }
    return false;
}

void FCombatHints::Arm()
{
    bArmed = true;
    bTipsOff = false;
    for (bool& Flag : Shown) { Flag = false; }
    Active = ECombatHint::Count;
    bVisible = false;
}

void FCombatHints::Disarm()
{
    bArmed = false;
    Active = ECombatHint::Count;
    bVisible = false;
}

void FCombatHints::Replay()
{
    Arm();
}

void FCombatHints::SkipTips()
{
    bTipsOff = true;
    Active = ECombatHint::Count;
    bVisible = false;
}

void FCombatHints::Dismiss()
{
    Active = ECombatHint::Count;
    bVisible = false;
}

bool FCombatHints::Wants() const
{
    if (!bArmed || bTipsOff) { return false; }
    if (Active != ECombatHint::Count) { return true; }
    for (const bool Flag : Shown) { if (!Flag) { return true; } }
    return false;
}

void FCombatHints::Observe(const FHintObservation& Now)
{
    Last = Now;
    if (!bArmed || bTipsOff) { Active = ECombatHint::Count; bVisible = false; return; }
    // Not at rest: an overlay, a playback or an enemy turn. The hint is kept, hidden, and nothing new is raised.
    if (!Now.bAtRest) { bVisible = false; return; }
    // A hint whose cause has passed (the ally was rescued, the hero has options again) is put away.
    if (Active != ECombatHint::Count && !Now.Holds(Active)) { Active = ECombatHint::Count; }
    if (Active == ECombatHint::Count)
    {
        for (int32 Index = 0; Index < int32(ECombatHint::Count); ++Index)
        {
            if (!Shown[Index] && Now.Holds(ECombatHint(Index))) { Active = ECombatHint(Index); Shown[Index] = true; break; }
        }
    }
    bVisible = Active != ECombatHint::Count;
}

bool FCombatHints::Show(ECombatHint Hint, const FHintObservation& Now)
{
    Arm();
    Last = Now;
    if (Hint == ECombatHint::Count || !Now.Holds(Hint)) { return false; }
    Active = Hint;
    Shown[int32(Hint)] = true;
    bVisible = true;
    return true;
}

FHintContent FCombatHints::Content() const
{
    return Active == ECombatHint::Count ? FHintContent() : ContentFor(Active, Last);
}

FHintContent FCombatHints::ContentFor(ECombatHint Hint, const FHintObservation& Now)
{
    FHintContent Content;
    Content.Title = ExpeditionStyle::HintName(Hint);
    switch (Hint)
    {
    case ECombatHint::MoveSelectConfirm:
        Content.Lines.Add(TEXT("Click a glowing tile to move, or a marked enemy to attack."));
        Content.Lines.Add(TEXT("Arrows move the cursor and Enter does the same."));
        break;
    case ECombatHint::ScarceResources:
    {
        const FActionResourceCost& Cost = Now.Resource;
        const FString Name = ExpeditionStyle::ResourceName(Cost.Resource);
        Content.Lines.Add(Cost.Cap > 0
            ? FString::Printf(TEXT("%s %d/%d is what you hold of the most you carry."), *Name, Cost.Held, Cost.Cap)
            : FString::Printf(TEXT("%s %d is what you hold."), *Name, Cost.Held));
        const FString Camp = ExpeditionStyle::CampRecoveryText(Cost);
        if (!Camp.IsEmpty()) { Content.Lines.Add(Camp + TEXT(".")); }
        if (Cost.CampRecovery != 0) { Content.Lines.Add(TEXT("Camp gives back only what the build's recovery says.")); }
        break;
    }
    case ECombatHint::DownedRescue:
        Content.Title = Now.DownedName.IsEmpty() ? FString(TEXT("A hero is Downed")) : FString::Printf(TEXT("%s is Downed"), *Now.DownedName);
        Content.Lines.Add(FString::Printf(TEXT("An adjacent ally can Rescue (R) within %d %s, or they die."), Now.RescueTurnsLeft, Now.RescueTurnsLeft == 1 ? TEXT("turn") : TEXT("turns")));
        break;
    case ECombatHint::EndingTurn:
        Content.Lines.Add(TEXT("Nothing is left but End Turn."));
        Content.Lines.Add(TEXT("Press Space to end it."));
        break;
    case ECombatHint::Count:
        break;
    }
    return Content;
}

namespace ExpeditionStyle
{
FString HintName(ECombatHint Hint)
{
    switch (Hint)
    {
    case ECombatHint::MoveSelectConfirm: return TEXT("Your turn");
    case ECombatHint::ScarceResources: return TEXT("Limited resources");
    case ECombatHint::DownedRescue: return TEXT("Downed ally");
    case ECombatHint::EndingTurn: return TEXT("Ending a turn");
    case ECombatHint::Count: break;
    }
    return FString();
}

FString HintDismissKey() { return TEXT("D"); }
}
