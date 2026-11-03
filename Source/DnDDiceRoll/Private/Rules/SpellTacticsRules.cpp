#include "Rules/SpellTacticsRules.h"
#include "Rules/HexGrid.h"
#include "Rules/SpellEffectRules.h"
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
    { TEXT("wall-of-fire"), OnCast | Enter | EndTurn, 0, false, false, false, 60 },
    { TEXT("fog-cloud"), 0, 20, false, false, false },
    { TEXT("darkness"), 0, 15, false, false, false },
    { TEXT("light"), 0, 20, false, false, false },
    { TEXT("dancing-lights"), 0, 10, false, false, false },
    { TEXT("continual-flame"), 0, 20, false, false, false },
    { TEXT("daylight"), 0, 60, false, false, false },
    { TEXT("silence"), 0, 20, false, false, false },
    // Illusions as sight-blocking images: Minor Illusion one cell, Silent Image 15-ft cube (radius 1 cell), Major Image 20-ft cube (radius 2).
    { TEXT("minor-illusion"), 0, 0, false, false, false },
    { TEXT("silent-image"), 0, 5, false, false, false },
    { TEXT("major-image"), 0, 10, false, false, false },
    { TEXT("plant-growth"), 0, 100, false, false, true },
    { TEXT("guardian-of-faith"), Enter | StartTurn, 10, false, true, false },
    { TEXT("aura-of-life"), 0, 30, true, true, false },
    { TEXT("conjure-minor-elementals"), 0, 15, true, true, false },
    { TEXT("pass-without-trace"), 0, 30, true, true, false },
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
TArray<FIntPoint> FSpellTacticsRules::WallPath(FIntPoint Aim, const TArray<FIntPoint>& Nodes, int32 MaxFeet)
{
    const int32 MaxCells = FMath::Max(1, MaxFeet / FHexGrid::FeetPerCell);
    TArray<FIntPoint> Out = {Aim};
    FIntPoint From = Aim;
    for (const FIntPoint& Node : Nodes)
    {
        for (const FIntPoint& Cell : FHexGrid::BoardLine(From, Node))
        {
            if (Out.Num() >= MaxCells) { return Out; }
            Out.AddUnique(Cell);
        }
        From = Node;
    }
    return Out;
}
int32 FSpellTacticsRules::ZoneCount() { return UE_ARRAY_COUNT(Zones); }
const FSpellZone& FSpellTacticsRules::ZoneAt(int32 Index) { return Zones[Index]; }

bool FSpellTacticsRules::RepeatsSaveAtEndOfTurn(const FSrdSpell& Spell)
{
    if (const FSpellEffectSpec* Spec = FSpellEffectRules::Find(Spell.Id); Spec && (Spec->Flags & (SpellEffectFlag::RepeatSave | SpellEffectFlag::SleepEscalates))) { return true; }
    for (const TCHAR* Id : RepeatSave) { if (FCString::Stricmp(Id, Spell.Id) == 0) { return true; } }
    return false;
}
