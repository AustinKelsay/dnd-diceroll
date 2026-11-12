#include "Presentation/ExpeditionPalette.h"

#include "Materials/MaterialInstanceDynamic.h"

namespace
{
struct FPaletteClassRow
{
    const TCHAR* Key;
    uint16 Masks[ExpeditionPalette::SlotCount];
    float Refs[ExpeditionPalette::SlotCount];
    float Accent[ExpeditionPalette::AccentCount][3];
    uint8 DefaultPresent;
    float Default[ExpeditionPalette::BuildSlotCount][3];
};
struct FPaletteBuildRow
{
    const TCHAR* Id;
    uint8 Present;
    float Colour[ExpeditionPalette::BuildSlotCount][3];
};
#include "Presentation/ExpeditionPaletteTable.inl"

const FPaletteClassRow* FindClass(const FString& Key)
{
    for (const FPaletteClassRow& Row : PaletteClassRows) { if (Key.Equals(Row.Key, ESearchCase::IgnoreCase)) { return &Row; } }
    return nullptr;
}

const FPaletteBuildRow* FindBuild(const FString& Id)
{
    for (const FPaletteBuildRow& Row : PaletteBuildRows) { if (Id.Equals(Row.Id, ESearchCase::IgnoreCase)) { return &Row; } }
    return nullptr;
}

FLinearColor Rgb(const float (&Value)[3]) { return FLinearColor(Value[0], Value[1], Value[2]); }

const TCHAR* Letter(ExpeditionPalette::ESlot Slot)
{
    switch (Slot)
    {
    case ExpeditionPalette::ESlot::A: return TEXT("A");
    case ExpeditionPalette::ESlot::B: return TEXT("B");
    case ExpeditionPalette::ESlot::C: return TEXT("C");
    case ExpeditionPalette::ESlot::G: return TEXT("G");
    default: return TEXT("X");
    }
}
}

namespace ExpeditionPalette
{
int32 FPalette::ActiveSlots() const
{
    int32 Count = 0;
    for (const FSlot& Slot : Slots) { Count += Slot.IsActive() ? 1 : 0; }
    return Count;
}

FString ClassKeyOf(const FCombatantState& Unit)
{
    if (!Unit.ClassId.IsEmpty()) { return Unit.ClassId.ToLower(); }
    const UEnum* Enum = StaticEnum<EAdventurerRole>();
    return Unit.AdventurerRole == EAdventurerRole::None || !Enum ? FString() : Enum->GetNameStringByValue(static_cast<int64>(Unit.AdventurerRole)).ToLower();
}

FPalette ForBuild(const FString& BuildId, const FString& ClassKey)
{
    FPalette Result;
    const FPaletteClassRow* Class = FindClass(ClassKey);
    if (!Class) { return Result; }
    Result.bKnownClass = true;
    const FPaletteBuildRow* Build = FindBuild(BuildId);
    // A build row of another class is never used for this class (a corrupt or hand-edited id falls back to the class default).
    if (Build && !FString(Build->Id).StartsWith(ClassKey + TEXT("."), ESearchCase::IgnoreCase)) { Build = nullptr; }
    Result.bBuildRow = Build != nullptr;
    const uint8 Present = Build ? Build->Present : Class->DefaultPresent;
    for (int32 Index = 0; Index < SlotCount; ++Index) { Result.Slots[Index].Reference = Class->Refs[Index]; }
    for (int32 Index = 0; Index < BuildSlotCount; ++Index)
    {
        if (!(Present & (1 << Index))) { continue; }
        Result.Slots[Index].Mask = Class->Masks[Index];
        Result.Slots[Index].Target = Rgb(Build ? Build->Colour[Index] : Class->Default[Index]);
    }
    return Result;
}

FPalette ForUnit(const FCombatantState& Unit) { return ForBuild(Unit.BuildId, ClassKeyOf(Unit)); }

TArray<FString> BuildIds()
{
    TArray<FString> Ids;
    for (const FPaletteBuildRow& Row : PaletteBuildRows) { Ids.Add(Row.Id); }
    return Ids;
}

TMap<int32, int32> AccentSlots(const TArray<FCombatantState>& Combatants)
{
    TMap<int32, int32> Result;
    TMap<FString, int32> PerClass;
    for (const FCombatantState& Unit : Combatants) { if (Unit.Side == ECombatSide::Party) { PerClass.FindOrAdd(ClassKeyOf(Unit)) += 1; } }
    int32 Position = 0;
    for (const FCombatantState& Unit : Combatants)
    {
        if (Unit.Side != ECombatSide::Party) { continue; }
        const int32* Count = PerClass.Find(ClassKeyOf(Unit));
        if (Count && *Count > 1) { Result.Add(Unit.Id, Position); }
        ++Position;
    }
    return Result;
}

void SetAccent(FPalette& Palette, const FString& ClassKey, int32 PartySlot)
{
    const FPaletteClassRow* Class = FindClass(ClassKey);
    if (!Class) { return; }
    const int32 Index = ((PartySlot % AccentCount) + AccentCount) % AccentCount;
    FSlot& Slot = Palette.Slots[static_cast<int32>(ESlot::X)];
    Slot.Mask = Class->Masks[static_cast<int32>(ESlot::X)];
    Slot.Reference = Class->Refs[static_cast<int32>(ESlot::X)];
    Slot.Target = Rgb(Class->Accent[Index]);
    Palette.bKnownClass = true;
}

FName MaskName(ESlot Slot) { return FName(*FString::Printf(TEXT("PalMask%s"), Letter(Slot))); }
FName ColorName(ESlot Slot) { return FName(*FString::Printf(TEXT("Pal%s"), Letter(Slot))); }

void Apply(UMaterialInstanceDynamic* Material, const FPalette& Palette)
{
    if (!Material) { return; }
    for (int32 Index = 0; Index < SlotCount; ++Index)
    {
        const ESlot Slot = static_cast<ESlot>(Index);
        const FSlot& Value = Palette.Slots[Index];
        Material->SetScalarParameterValue(MaskName(Slot), static_cast<float>(Value.Mask));
        Material->SetVectorParameterValue(ColorName(Slot), FLinearColor(Value.Target.R, Value.Target.G, Value.Target.B, Value.Reference));
    }
}

bool Read(const UMaterialInstanceDynamic* Material, FPalette& OutPalette)
{
    if (!Material) { return false; }
    OutPalette = FPalette();
    for (int32 Index = 0; Index < SlotCount; ++Index)
    {
        const ESlot Slot = static_cast<ESlot>(Index);
        float Mask = 0.0f;
        FLinearColor Colour;
        if (!Material->GetScalarParameterValue(FHashedMaterialParameterInfo(MaskName(Slot)), Mask)
            || !Material->GetVectorParameterValue(FHashedMaterialParameterInfo(ColorName(Slot)), Colour)) { return false; }
        OutPalette.Slots[Index].Mask = static_cast<uint16>(FMath::RoundToInt(Mask));
        OutPalette.Slots[Index].Target = FLinearColor(Colour.R, Colour.G, Colour.B);
        OutPalette.Slots[Index].Reference = Colour.A;
    }
    return true;
}
}
