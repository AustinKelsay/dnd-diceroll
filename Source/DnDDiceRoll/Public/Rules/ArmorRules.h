#pragma once

#include "CoreMinimal.h"
#include "Rules/AbilityRules.h"

enum class EAdventurerRole : uint8;

enum class EArmorCategory : uint8 { None, Light, Medium, Heavy, Shield };

/** SRD 5.2.1 armor and the shield. */
enum class EArmor : uint8
{
    None, Padded, Leather, StuddedLeather, Hide, ChainShirt, ScaleMail, Breastplate, HalfPlate,
    RingMail, ChainMail, Splint, Plate, Shield, Count
};

struct DNDDICEROLL_API FArmorProfile
{
    const TCHAR* Name = TEXT("");
    EArmorCategory Category = EArmorCategory::None;
    /** Base AC for armor; AC bonus for the shield. */
    int32 BaseAC = 10;
    /** Minimum Strength to avoid the 10 ft speed penalty; 0 when none. */
    int32 StrengthRequirement = 0;
    bool bStealthDisadvantage = false;
};

/** What an unproficient wearer suffers (SRD 5.2.1 Armor Proficiency). */
struct DNDDICEROLL_API FArmorPenalties
{
    bool bStrDexDisadvantage = false;
    bool bCannotCastSpells = false;
    bool bStealthDisadvantage = false;
    int32 SpeedPenaltyFeet = 0;
};

struct DNDDICEROLL_API FArmorRules
{
    static const FArmorProfile& Profile(EArmor Armor);
    /** Dex cap for a category: unlimited (-1) for none/light, 2 for medium, 0 for heavy. */
    static int32 DexCap(EArmorCategory Category);
    /** Armor Class from worn armor and Dexterity; a shield adds AC only with shield training. */
    static int32 ArmorClass(EAdventurerRole Role, EArmor Armor, bool bShield, int32 Dexterity);
    static bool IsProficient(EAdventurerRole Role, EArmorCategory Category);
    /** Shield training affects AC, not the restrictions for wearing untrained armor. */
    static FArmorPenalties Penalties(EAdventurerRole Role, EArmor Armor, int32 Strength);
};
