#include "Party/FeatureKit.h"
#include "Party/BuildCatalog.h"
#include "Rules/WeaponRules.h"

const TCHAR* FFeatureKit::StyleName(EFightingStyle Style)
{
    switch (Style)
    {
    case EFightingStyle::Archery: return TEXT("Archery");
    case EFightingStyle::Defense: return TEXT("Defense");
    case EFightingStyle::Dueling: return TEXT("Dueling");
    case EFightingStyle::GreatWeaponFighting: return TEXT("Great Weapon Fighting");
    case EFightingStyle::Protection: return TEXT("Protection");
    case EFightingStyle::TwoWeaponFighting: return TEXT("Two-Weapon Fighting");
    default: return TEXT("None");
    }
}
const TCHAR* FFeatureKit::StyleEffect(EFightingStyle Style)
{
    switch (Style)
    {
    case EFightingStyle::Archery: return TEXT("+2 to hit with ranged weapons");
    case EFightingStyle::Defense: return TEXT("+1 AC while wearing armor");
    case EFightingStyle::Dueling: return TEXT("+2 damage with a one-handed melee weapon and no other weapon");
    case EFightingStyle::GreatWeaponFighting: return TEXT("reroll 1s and 2s on two-handed weapon damage dice");
    case EFightingStyle::Protection: return TEXT("reaction with a shield: disadvantage on an attack against an adjacent ally");
    case EFightingStyle::TwoWeaponFighting: return TEXT("add your ability modifier to off-hand damage");
    default: return TEXT("");
    }
}
bool FFeatureKit::HasFightingStyle(const FCombatantState& Hero)
{
    return Hero.Side == ECombatSide::Party && (FBuildCatalog::ClassLevel(Hero, EAdventurerRole::Fighter) >= 1 ||
        FBuildCatalog::ClassLevel(Hero, EAdventurerRole::Paladin) >= 2 || FBuildCatalog::ClassLevel(Hero, EAdventurerRole::Ranger) >= 2);
}
EFightingStyle FFeatureKit::Style(const FCombatantState& Hero)
{
    return HasFightingStyle(Hero) && Hero.FightingStyle > 0 && Hero.FightingStyle < static_cast<int32>(EFightingStyle::Count) ? static_cast<EFightingStyle>(Hero.FightingStyle) : EFightingStyle::None;
}
EFightingStyle FFeatureKit::DefaultStyle(const FCombatantState& Hero)
{
    if (Hero.MainHand >= 0 && Hero.MainHand < static_cast<int32>(EWeapon::Count))
    {
        const FWeaponProfile& Weapon = FWeaponRules::Profile(static_cast<EWeapon>(Hero.MainHand));
        if (Weapon.bRanged) { return EFightingStyle::Archery; }
        if (Weapon.Has(EWeaponProperty::TwoHanded)) { return EFightingStyle::GreatWeaponFighting; }
        if (Hero.OffHand >= 0) { return EFightingStyle::TwoWeaponFighting; }
    }
    return Hero.ArmorWorn >= 0 ? EFightingStyle::Defense : EFightingStyle::Dueling;
}
bool FFeatureKit::ChooseStyle(FCombatantState& Hero, EFightingStyle Choice, FString& Error)
{
    if (!HasFightingStyle(Hero)) { Error = TEXT("Fighting Style needs Fighter 1, Paladin 2 or Ranger 2."); return false; }
    if (Choice == EFightingStyle::None || Choice >= EFightingStyle::Count) { Error = TEXT("Unknown fighting style."); return false; }
    Hero.FightingStyle = static_cast<int32>(Choice);
    return true;
}
void FFeatureKit::Refill(FCombatantState& Hero)
{
    if (HasFightingStyle(Hero) && Style(Hero) == EFightingStyle::None) { Hero.FightingStyle = static_cast<int32>(DefaultStyle(Hero)); }
    Hero.ActionSurgeUses = ActionSurgeCap(Hero);
}
void FFeatureKit::Grant(FCombatantState& Hero, const FCombatantState& Before)
{
    if (HasFightingStyle(Hero) && Style(Hero) == EFightingStyle::None) { Hero.FightingStyle = static_cast<int32>(DefaultStyle(Hero)); }
    const auto Raise = [](int32& Pool, int32 Now, int32 Was) { Pool = FMath::Clamp(Pool + FMath::Max(0, Now - Was), 0, Now); };
    Raise(Hero.ActionSurgeUses, ActionSurgeCap(Hero), ActionSurgeCap(Before));
}
int32 FFeatureKit::ActionSurgeCap(const FCombatantState& Hero)
{
    return Hero.Side == ECombatSide::Party && FBuildCatalog::ClassLevel(Hero, EAdventurerRole::Fighter) >= 2 ? 1 : 0;
}
const TCHAR* FFeatureKit::CommandName(ECombatCommandType Action)
{
    switch (Action)
    {
    case ECombatCommandType::ActionSurge: return TEXT("Action Surge");
    default: return TEXT("Feature");
    }
}
EActionCost FFeatureKit::Cost(ECombatCommandType Action)
{
    switch (Action)
    {
    case ECombatCommandType::ActionSurge: return EActionCost::Free;
    default: return EActionCost::Action;
    }
}
FString FFeatureKit::Usage(ECombatCommandType Action, const FCombatantState& Hero)
{
    switch (Action)
    {
    case ECombatCommandType::ActionSurge: return FString::Printf(TEXT("free, %d left"), Hero.ActionSurgeUses);
    default: return FString();
    }
}
TArray<ECombatCommandType> FFeatureKit::Commands(const FCombatantState& Hero)
{
    TArray<ECombatCommandType> Out;
    if (ActionSurgeCap(Hero)) { Out.Add(ECombatCommandType::ActionSurge); }
    return Out;
}
bool FFeatureKit::PicksTarget(ECombatCommandType Action)
{
    return false;
}
bool FFeatureKit::Valid(const FCombatantState& Hero)
{
    return Hero.FightingStyle >= 0 && Hero.FightingStyle < static_cast<int32>(EFightingStyle::Count) && (Hero.FightingStyle == 0 || HasFightingStyle(Hero)) &&
        Hero.ActionSurgeUses >= 0 && Hero.ActionSurgeUses <= ActionSurgeCap(Hero);
}