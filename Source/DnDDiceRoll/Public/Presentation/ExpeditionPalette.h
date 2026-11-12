#pragma once

#include "CoreMinimal.h"
#include "Combat/CombatBattle.h"

class UMaterialInstanceDynamic;

/**
 * Subclass palettes and duplicate-identity accents for the rigged party (#150 batch 4). Presentation only (ADR-0001): a palette is chosen from a
 * hero's build id and an accent from its position among the party, both pure functions of the committed encounter. Neither reads or writes a
 * snapshot, a checkpoint or a random draw, so Resume, Finish() and skip show the same colours.
 *
 * The mechanism needs no mesh and no atlas of its own. The class rigs share a 1024 atlas of 13 tiles on a 4x4 grid. The master material
 * (M_DiceRollTheme) has five slots, each with a bit mask over those tiles ("PalMask<slot>", bit = tile index = row * 4 + column, row 0 at the top) and
 * a colour ("Pal<slot>": RGB the target, A the reference luminance, the class's own colour of that slot). A masked pixel becomes
 * target x min(luminance(pixel) / reference, 1.7): the tile keeps its folds, bands and highlights and takes the slot's hue. All masks 0 (the
 * default) is the plain material. Slots: A primary garment, B secondary garment, C trim, G glow, X the party-slot accent.
 *
 * The table is Art/rig-pilot/cast/palettes/palettes.json, compiled into ExpeditionPaletteTable.inl by Art/rig-pilot/cast/scripts/palettes.py.
 */
namespace ExpeditionPalette
{
enum class ESlot : uint8 { A, B, C, G, X };
constexpr int32 SlotCount = 5;
constexpr int32 BuildSlotCount = 4;
/** Distinct accent hues; a party slot beyond this count wraps. */
constexpr int32 AccentCount = 4;

struct FSlot
{
    /** Bit i selects atlas tile i. 0 leaves the slot unused (the class's own colours). */
    uint16 Mask = 0;
    /** Linear target colour (the colour a pixel of the class's own slot colour becomes). */
    FLinearColor Target = FLinearColor::Black;
    /** Linear luminance of the class's own colour of the slot. */
    float Reference = 1.0f;
    bool IsActive() const { return Mask != 0; }
};

struct FPalette
{
    FSlot Slots[SlotCount];
    /** True when the table has a row for the build id itself, false for the class default and for an unknown class. */
    bool bBuildRow = false;
    /** True when the class is in the table (false means no palette at all: the plain material). */
    bool bKnownClass = false;
    int32 ActiveSlots() const;
};

/** The class key the table uses: the hero's ClassId ("fighter"), else the role's lower-case name. Empty for none. */
FString ClassKeyOf(const FCombatantState& Unit);
/** The palette of a build: its row, else its class default, else (unknown class) none. A build id of another class never matches. */
FPalette ForBuild(const FString& BuildId, const FString& ClassKey);
FPalette ForUnit(const FCombatantState& Unit);
/** Every build id in the table, in table order (for sweeps). */
TArray<FString> BuildIds();

/**
 * The accent slot of each party member whose class appears more than once among the party: the unit's position (0-based) among the party side in
 * encounter order, so it is the same on every board of a run and after Resume. A hero of a class that appears once has no accent.
 */
TMap<int32, int32> AccentSlots(const TArray<FCombatantState>& Combatants);
/** Turns the X slot on for a class and party slot (the slot wraps at AccentCount). A class not in the table is left alone. */
void SetAccent(FPalette& Palette, const FString& ClassKey, int32 PartySlot);

/** Writes the palette to a figure's material instance (every slot, so an unused one clears). */
void Apply(UMaterialInstanceDynamic* Material, const FPalette& Palette);
/** Reads it back (tests): false when the material does not carry the parameters. */
bool Read(const UMaterialInstanceDynamic* Material, FPalette& OutPalette);
/** The material parameter names, for tools and tests. */
FName MaskName(ESlot Slot);
FName ColorName(ESlot Slot);
}
