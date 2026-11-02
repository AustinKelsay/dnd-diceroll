#include "Rules/SpellTacticsRules.h"
#include "Rules/SpellcastingRules.h"

namespace
{
using namespace SpellZoneTrigger;
// SRD 5.2.1. Radii in feet. Triggers: the cast burst, entering (first time on a turn), ending or starting a turn there, every 5 ft moved inside.
const FSpellZone Zones[] =
{
    { TEXT("spirit-guardians"), Enter | EndTurn, 15, true, true, false },
    { TEXT("moonbeam"), OnCast | Enter | EndTurn, 5, false, false, false },
    { TEXT("spike-growth"), PerStep, 20, false, false, true },
    { TEXT("conjure-animals"), Enter | EndTurn, 10, false, true, false },
    { TEXT("conjure-woodland-beings"), Enter | EndTurn, 10, true, true, false },
    { TEXT("black-tentacles"), OnCast | Enter | EndTurn, 10, false, false, true },
    // Ring form (20-ft diameter): creatures inside or entering take the fire side.
    { TEXT("wall-of-fire"), OnCast | Enter | EndTurn, 10, false, false, false },
};

const TCHAR* const RepeatSave[] =
{
    TEXT("hold-person"), TEXT("hold-monster"), TEXT("slow"), TEXT("sleep"), TEXT("fear"), TEXT("compulsion"),
    TEXT("blindness-deafness"), TEXT("hideous-laughter"), TEXT("ray-of-enfeeblement"), TEXT("phantasmal-killer"), TEXT("confusion"),
};
}

int32 FSpellTacticsRules::DurationRounds(const TCHAR* Duration)
{
    const FString Text = FString(Duration ? Duration : TEXT("")).ToLower();
    if (Text.Contains(TEXT("instantaneous"))) { return 0; }
    if (Text.Contains(TEXT("until dispelled")) || Text.Contains(TEXT("triggered"))) { return -1; }
    int32 Number = 1;
    for (int32 Index = 0; Index < Text.Len(); ++Index)
    {
        if (FChar::IsDigit(Text[Index])) { Number = FCString::Atoi(*Text + Index); break; }
    }
    if (Text.Contains(TEXT("round"))) { return Number; }
    if (Text.Contains(TEXT("minute"))) { return Number * 10; }
    if (Text.Contains(TEXT("hour"))) { return Number * 600; }
    if (Text.Contains(TEXT("day"))) { return Number * 14400; }
    return 0;
}

const FSpellZone* FSpellTacticsRules::Zone(const FString& Id)
{
    for (const FSpellZone& Z : Zones) { if (Id.Equals(Z.Id, ESearchCase::IgnoreCase)) { return &Z; } }
    return nullptr;
}
const FSpellZone* FSpellTacticsRules::Zone(const FSrdSpell& Spell) { return Zone(FString(Spell.Id)); }
int32 FSpellTacticsRules::ZoneCount() { return UE_ARRAY_COUNT(Zones); }
const FSpellZone& FSpellTacticsRules::ZoneAt(int32 Index) { return Zones[Index]; }

bool FSpellTacticsRules::RepeatsSaveAtEndOfTurn(const FSrdSpell& Spell)
{
    for (const TCHAR* Id : RepeatSave) { if (FCString::Stricmp(Id, Spell.Id) == 0) { return true; } }
    return false;
}
