#include "Party/FeatureKit.h"
#include "Party/BuildCatalog.h"
#include "Rules/WeaponRules.h"
#include "Rules/ClassFeatureRules.h"
#include "Rules/SpellRules.h"

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
    Hero.ChannelUses = ChannelCap(Hero);
    Hero.BardicUses = BardicCap(Hero);
    Hero.WildShapeUses = WildShapeCap(Hero);
    Hero.WildShapeHP = 0;
    Hero.KiPoints = KiCap(Hero);
    Hero.SorceryPoints = SorceryCap(Hero);
    Hero.bArcaneRecovered = false;
}
bool FFeatureKit::HasArcaneRecovery(const FCombatantState& Hero)
{
    return Hero.Side == ECombatSide::Party && FBuildCatalog::ClassLevel(Hero, EAdventurerRole::Wizard) >= 1;
}
int32 FFeatureKit::ArcaneRecoveryPreview(const FCombatantState& Hero)
{
    FCombatantState Copy = Hero;
    return ArcaneRecover(Copy);
}
int32 FFeatureKit::ArcaneRecover(FCombatantState& Hero)
{
    if (!HasArcaneRecovery(Hero) || Hero.bArcaneRecovered) { return 0; }
    int32 Budget = FClassFeatureRules::ArcaneRecoveryLevels(FBuildCatalog::ClassLevel(Hero, EAdventurerRole::Wizard));
    int32 Restored = 0;
    if (FBuildCatalog::SlotCap(Hero) > 0)
    {
        // Real slots: highest levels first (SRD: none of 6th level or higher), level 1 within the build's slot cap.
        for (int32 Level = FMath::Min(5, Budget); Level >= 1; --Level)
        {
            const int32 Cap = Level == 1 ? FMath::Min(FBuildCatalog::SlotCap(Hero), Hero.SlotsByLevel.IsEmpty() ? 2 : FMath::Min(2, FSpellRules::SlotsFor(Hero.AdventurerRole, Hero.Level, 1)))
                : Hero.SlotsByLevel.IsValidIndex(Level - 1) ? FSpellRules::SlotsFor(Hero.AdventurerRole, Hero.Level, Level) : 0;
            if (Cap <= 0 || (Level > 1 && !Hero.SlotsByLevel.IsValidIndex(Level - 1))) { continue; }
            while (Budget >= Level)
            {
                int32& Pool = Level == 1 ? Hero.SpellSlots : Hero.SlotsByLevel[Level - 1];
                if (Pool >= Cap) { break; }
                ++Pool; Budget -= Level; Restored += Level;
            }
        }
        if (!Hero.SlotsByLevel.IsEmpty()) { Hero.SlotsByLevel[0] = Hero.SpellSlots; }
    }
    else if (const auto* Build = FBuildCatalog::Find(Hero.BuildId); Build && Build->Profile.ClassId == TEXT("wizard") && Build->KitCap > 0)
    {
        // Kit wizards spend kit uses as their spell resource: one use per slot level.
        Restored = FMath::Min(Budget, Build->KitCap - Hero.KitUses);
        Hero.KitUses += Restored;
    }
    if (Restored > 0) { Hero.bArcaneRecovered = true; }
    return Restored;
}
void FFeatureKit::ShortRest(FCombatantState& Hero)
{
    Hero.ActionSurgeUses = ActionSurgeCap(Hero);
    Hero.ChannelUses = ChannelCap(Hero);
    Hero.WildShapeUses = WildShapeCap(Hero);
    Hero.KiPoints = KiCap(Hero);
}
int32 FFeatureKit::SorceryCap(const FCombatantState& Hero)
{
    const int32 Sorcerer = FBuildCatalog::ClassLevel(Hero, EAdventurerRole::Sorcerer);
    return Hero.Side == ECombatSide::Party && Sorcerer >= 2 ? Sorcerer : 0;
}
bool FFeatureKit::HasUncannyDodge(const FCombatantState& Hero)
{
    return Hero.Side == ECombatSide::Party && FBuildCatalog::ClassLevel(Hero, EAdventurerRole::Rogue) >= 5;
}
bool FFeatureKit::HasRecklessAttack(const FCombatantState& Hero)
{
    return Hero.Side == ECombatSide::Party && FBuildCatalog::ClassLevel(Hero, EAdventurerRole::Barbarian) >= 2;
}
int32 FFeatureKit::KiCap(const FCombatantState& Hero)
{
    const int32 Monk = FBuildCatalog::ClassLevel(Hero, EAdventurerRole::Monk);
    return Hero.Side == ECombatSide::Party && Monk >= 2 ? Monk : 0;
}
void FFeatureKit::Grant(FCombatantState& Hero, const FCombatantState& Before)
{
    if (HasFightingStyle(Hero) && Style(Hero) == EFightingStyle::None) { Hero.FightingStyle = static_cast<int32>(DefaultStyle(Hero)); }
    const auto Raise = [](int32& Pool, int32 Now, int32 Was) { Pool = FMath::Clamp(Pool + FMath::Max(0, Now - Was), 0, Now); };
    Raise(Hero.ActionSurgeUses, ActionSurgeCap(Hero), ActionSurgeCap(Before));
    Raise(Hero.ChannelUses, ChannelCap(Hero), ChannelCap(Before));
    Raise(Hero.BardicUses, BardicCap(Hero), BardicCap(Before));
    Raise(Hero.WildShapeUses, WildShapeCap(Hero), WildShapeCap(Before));
    Raise(Hero.KiPoints, KiCap(Hero), KiCap(Before));
    Raise(Hero.SorceryPoints, SorceryCap(Hero), SorceryCap(Before));
}
int32 FFeatureKit::ActionSurgeCap(const FCombatantState& Hero)
{
    return Hero.Side == ECombatSide::Party && FBuildCatalog::ClassLevel(Hero, EAdventurerRole::Fighter) >= 2 ? 1 : 0;
}
int32 FFeatureKit::ChannelCap(const FCombatantState& Hero)
{
    return Hero.Side == ECombatSide::Party && FBuildCatalog::ClassLevel(Hero, EAdventurerRole::Cleric) >= 2 ? 1 : 0;
}
int32 FFeatureKit::BardicCap(const FCombatantState& Hero)
{
    if (Hero.Side != ECombatSide::Party || FBuildCatalog::ClassLevel(Hero, EAdventurerRole::Bard) < 1) { return 0; }
    FAbilityScores Scores;
    return FBuildCatalog::HeroScores(Hero, Scores) ? FMath::Max(1, FAbilityRules::Modifier(Scores.Get(EAbility::Charisma))) : 1;
}
int32 FFeatureKit::BardicSides(const FCombatantState& Bard)
{
    return FBuildCatalog::ClassLevel(Bard, EAdventurerRole::Bard) >= 5 ? 8 : 6;
}
int32 FFeatureKit::WildShapeCap(const FCombatantState& Hero)
{
    return Hero.Side == ECombatSide::Party && FBuildCatalog::ClassLevel(Hero, EAdventurerRole::Druid) >= 2 ? 2 : 0;
}
bool FFeatureKit::HasBonusFeature(const FCombatantState& Hero)
{
    return Commands(Hero).ContainsByPredicate([](ECombatCommandType Action) { return Cost(Action) == EActionCost::BonusAction; });
}
bool FFeatureKit::IsUndead(const FCombatantState& Unit)
{
    return Unit.Role == TEXT("Skeleton") || Unit.Role == TEXT("Zombie");
}
const TCHAR* FFeatureKit::CommandName(ECombatCommandType Action)
{
    switch (Action)
    {
    case ECombatCommandType::ActionSurge: return TEXT("Action Surge");
    case ECombatCommandType::TurnUndead: return TEXT("Turn Undead");
    case ECombatCommandType::BardicInspiration: return TEXT("Bardic Inspiration");
    case ECombatCommandType::WildShape: return TEXT("Wild Shape");
    case ECombatCommandType::FlurryOfBlows: return TEXT("Flurry of Blows");
    case ECombatCommandType::PatientDefense: return TEXT("Patient Defense");
    case ECombatCommandType::StepOfTheWind: return TEXT("Step of the Wind");
    case ECombatCommandType::FontOfMagic: return TEXT("Font of Magic");
    case ECombatCommandType::UncannyDodge: return TEXT("Uncanny Dodge");
    case ECombatCommandType::RecklessAttack: return TEXT("Reckless Attack");
    default: return TEXT("Feature");
    }
}
EActionCost FFeatureKit::Cost(ECombatCommandType Action)
{
    switch (Action)
    {
    case ECombatCommandType::ActionSurge: case ECombatCommandType::RecklessAttack: return EActionCost::Free;
    case ECombatCommandType::UncannyDodge: return EActionCost::Reaction;
    case ECombatCommandType::BardicInspiration: case ECombatCommandType::FlurryOfBlows: case ECombatCommandType::PatientDefense: case ECombatCommandType::StepOfTheWind:
    case ECombatCommandType::FontOfMagic:
        return EActionCost::BonusAction;
    default: return EActionCost::Action;
    }
}
FString FFeatureKit::Usage(ECombatCommandType Action, const FCombatantState& Hero)
{
    switch (Action)
    {
    case ECombatCommandType::ActionSurge: return FString::Printf(TEXT("free, %d left"), Hero.ActionSurgeUses);
    case ECombatCommandType::TurnUndead: return FString::Printf(TEXT("action, %d channel"), Hero.ChannelUses);
    case ECombatCommandType::BardicInspiration: return FString::Printf(TEXT("bonus, d%d, %d left"), BardicSides(Hero), Hero.BardicUses);
    case ECombatCommandType::WildShape: return Hero.WildShapeHP > 0 ? FString::Printf(TEXT("wolf, %d form HP"), Hero.WildShapeHP) : FString::Printf(TEXT("action, %d left"), Hero.WildShapeUses);
    case ECombatCommandType::FlurryOfBlows: case ECombatCommandType::PatientDefense: case ECombatCommandType::StepOfTheWind: return FString::Printf(TEXT("bonus, %d ki"), Hero.KiPoints);
    case ECombatCommandType::RecklessAttack: return Hero.bReckless ? TEXT("reckless until your turn") : TEXT("free, before attacking");
    case ECombatCommandType::UncannyDodge: return Hero.bReactionAvailable ? TEXT("auto, reaction ready") : TEXT("auto, reaction spent");
    case ECombatCommandType::FontOfMagic: return FString::Printf(TEXT("bonus, %d points"), Hero.SorceryPoints);
    default: return FString();
    }
}
TArray<ECombatCommandType> FFeatureKit::Commands(const FCombatantState& Hero)
{
    TArray<ECombatCommandType> Out;
    if (ActionSurgeCap(Hero)) { Out.Add(ECombatCommandType::ActionSurge); }
    if (ChannelCap(Hero)) { Out.Add(ECombatCommandType::TurnUndead); }
    if (BardicCap(Hero)) { Out.Add(ECombatCommandType::BardicInspiration); }
    if (WildShapeCap(Hero)) { Out.Add(ECombatCommandType::WildShape); }
    if (HasRecklessAttack(Hero)) { Out.Add(ECombatCommandType::RecklessAttack); }
    if (HasUncannyDodge(Hero)) { Out.Add(ECombatCommandType::UncannyDodge); }
    if (SorceryCap(Hero)) { Out.Add(ECombatCommandType::FontOfMagic); }
    if (KiCap(Hero)) { Out.Append({ECombatCommandType::FlurryOfBlows, ECombatCommandType::PatientDefense, ECombatCommandType::StepOfTheWind}); }
    return Out;
}
bool FFeatureKit::PicksTarget(ECombatCommandType Action)
{
    return Action == ECombatCommandType::BardicInspiration || Action == ECombatCommandType::FlurryOfBlows;
}
bool FFeatureKit::Valid(const FCombatantState& Hero)
{
    return Hero.FightingStyle >= 0 && Hero.FightingStyle < static_cast<int32>(EFightingStyle::Count) && (Hero.FightingStyle == 0 || HasFightingStyle(Hero)) &&
        Hero.ActionSurgeUses >= 0 && Hero.ActionSurgeUses <= ActionSurgeCap(Hero) && Hero.ChannelUses >= 0 && Hero.ChannelUses <= ChannelCap(Hero) &&
        Hero.BardicUses >= 0 && Hero.BardicUses <= BardicCap(Hero) && (Hero.BardicDie == 0 || Hero.BardicDie == 6 || Hero.BardicDie == 8) &&
        Hero.WildShapeUses >= 0 && Hero.WildShapeUses <= WildShapeCap(Hero) && Hero.WildShapeHP >= 0 && Hero.WildShapeHP <= (WildShapeCap(Hero) ? WolfHP : 0) &&
        Hero.KiPoints >= 0 && Hero.KiPoints <= KiCap(Hero) && Hero.SorceryPoints >= 0 && Hero.SorceryPoints <= SorceryCap(Hero) &&
        (!Hero.bReckless || HasRecklessAttack(Hero)) && (!Hero.bArcaneRecovered || HasArcaneRecovery(Hero));
}
TArray<FString> FFeatureKit::RewardSheet(const FCombatantState& Hero)
{
    static const TCHAR* Tags[] = { TEXT("Action"), TEXT("Bonus"), TEXT("Reaction"), TEXT("Free") };
    TArray<FString> Lines;
    for (const ECombatCommandType Action : Commands(Hero))
    {
        Lines.Add(FString::Printf(TEXT("%s [%s]: %s"), CommandName(Action), Tags[static_cast<uint8>(Cost(Action))], *Usage(Action, Hero)));
    }
    if (HasArcaneRecovery(Hero))
    {
        Lines.Add(Hero.bArcaneRecovered ? FString(TEXT("Arcane Recovery [Free]: used until camp")) : FString::Printf(TEXT("Arcane Recovery [Free]: %d slot level(s) at a short rest"), ArcaneRecoveryPreview(Hero)));
    }
    return Lines;
}