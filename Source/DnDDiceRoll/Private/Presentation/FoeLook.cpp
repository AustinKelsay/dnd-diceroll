#include "Presentation/FoeLook.h"

namespace ExpeditionFoe
{
namespace
{
struct FNameRule
{
    const TCHAR* Word;
    EFoeLook Look;
};

// First match wins, so a more specific role comes before the one whose word it contains: "Raider brute" holds "raider".
constexpr FNameRule NameTable[] = {
    {TEXT("warden"), EFoeLook::Warden},
    {TEXT("brute"), EFoeLook::Brute},
    {TEXT("skirmisher"), EFoeLook::Skirmisher},
    {TEXT("raider"), EFoeLook::Raider},
};
}

FName RigKey(EFoeLook Look)
{
    switch (Look)
    {
    case EFoeLook::Raider: return FName(TEXT("Raider"));
    case EFoeLook::Archer: return FName(TEXT("Archer"));
    case EFoeLook::Skirmisher: return FName(TEXT("Skirmisher"));
    case EFoeLook::Brute: return FName(TEXT("Brute"));
    case EFoeLook::Warden: return FName(TEXT("Warden"));
    default: return FName(TEXT("Scout"));
    }
}

bool LookFromSavedRole(const FString& Role, EFoeLook& OutLook)
{
    // Only the roles #188 introduces: "Raider" and "Archer" are what every saved run carries today for scouts, raiders and archers.
    for (const EFoeLook Look : {EFoeLook::Skirmisher, EFoeLook::Brute, EFoeLook::Warden})
    {
        if (Role.Equals(RigKey(Look).ToString(), ESearchCase::IgnoreCase)) { OutLook = Look; return true; }
    }
    return false;
}

EFoeLook LookFromName(const FString& Name, int32 Range)
{
    if (Range > 1) { return EFoeLook::Archer; }
    for (const FNameRule& Rule : NameTable)
    {
        if (Name.Contains(Rule.Word, ESearchCase::IgnoreCase)) { return Rule.Look; }
    }
    return EFoeLook::Scout;
}

EFoeLook LookOf(const FCombatantState& Unit)
{
    EFoeLook Saved;
    if (LookFromSavedRole(Unit.Role, Saved) && Unit.Range <= 1) { return Saved; }
    return LookFromName(Unit.Name, Unit.Range);
}
}
