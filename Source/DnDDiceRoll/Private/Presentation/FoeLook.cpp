#include "Presentation/FoeLook.h"

namespace ExpeditionFoe
{
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
    // The saved roles (#188) are the rig keys of their looks; a scout is saved as a Raider.
    for (const EFoeLook Look : {EFoeLook::Raider, EFoeLook::Archer, EFoeLook::Skirmisher, EFoeLook::Brute, EFoeLook::Warden})
    {
        if (Role.Equals(RigKey(Look).ToString(), ESearchCase::IgnoreCase)) { OutLook = Look; return true; }
    }
    return false;
}

EFoeLook LookOf(const FCombatantState& Unit)
{
    if (Unit.Range > 1) { return EFoeLook::Archer; }
    EFoeLook Look = EFoeLook::Scout;
    if (!LookFromSavedRole(Unit.Role, Look)) { return EFoeLook::Scout; }
    if (Look == EFoeLook::Raider && Unit.Name.Contains(TEXT("scout"), ESearchCase::IgnoreCase)) { return EFoeLook::Scout; }
    // A melee unit saved as an archer has no bow to draw; it is the plain figure.
    return Look == EFoeLook::Archer ? EFoeLook::Scout : Look;
}
}
