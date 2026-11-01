#include "Party/BuildCatalog.h"

namespace
{
const TArray<FBuildDefinition>& Definitions()
{
    static const TArray<FBuildDefinition> Values = []
    {
        TArray<FBuildDefinition> Result;
        auto Add = [&](const TCHAR* Id, const TCHAR* Class, const TCHAR* Subclass, const TCHAR* Role,
            EAdventurerRole AdventurerRole, int32 HP, int32 Armor, int32 Die, int32 Bonus, int32 Range, int32 Initiative)
        {
            FBuildDefinition B; B.Id = Id;
            auto& P = B.Profile;
            P.BuildId = Id; P.ClassId = Class; P.SubclassId = Subclass; P.Role = Role;
            P.AdventurerRole = AdventurerRole; P.MaxHP = P.HP = HP; P.ArmorClass = Armor;
            P.DamageDie = Die; P.DamageBonus = Bonus; P.Range = Range; P.InitiativeBonus = Initiative; P.Potions = 1;
            return B;
        };
        auto Champion = Add(TEXT("fighter.champion"), TEXT("fighter"), TEXT("champion"), TEXT("Fighter"), EAdventurerRole::Fighter, 30, 16, 6, 3, 1, 1);
        Champion.Profile.FighterSubclass = EFighterSubclass::Champion;
        Champion.WindCap = Champion.Profile.SecondWindUses = 2; Champion.WindRecovery = 1;
        Champion.Specials = {ECombatCommandType::SecondWind}; Champion.CriticalThreshold = 19;
        Result.Add(Champion);
        auto Master = Champion;
        Master.Id = Master.Profile.BuildId = TEXT("fighter.battle-master"); Master.Profile.SubclassId = TEXT("battle-master");
        Master.Profile.FighterSubclass = EFighterSubclass::BattleMaster; Master.CriticalThreshold = 20;
        Master.ManeuverCap = Master.Profile.ManeuverUses = 2; Master.Specials.Add(ECombatCommandType::PrecisionStrike);
        Result.Add(Master);
        auto Rogue = Add(TEXT("rogue.base"), TEXT("rogue"), TEXT(""), TEXT("Rogue"), EAdventurerRole::Rogue, 22, 13, 6, 3, 3, 4);
        Rogue.Profile.bSneakWeapon = true; Rogue.SneakDie = 6; Result.Add(Rogue);
        auto Cleric = Add(TEXT("cleric.base"), TEXT("cleric"), TEXT(""), TEXT("Cleric"), EAdventurerRole::Cleric, 22, 13, 6, 3, 3, 1);
        Cleric.SlotCap = Cleric.Profile.SpellSlots = 2; Cleric.SlotRecovery = 1; Cleric.Specials = {ECombatCommandType::CureWounds}; Result.Add(Cleric);
        auto Wizard = Add(TEXT("wizard.base"), TEXT("wizard"), TEXT(""), TEXT("Wizard"), EAdventurerRole::Wizard, 22, 13, 10, 0, 4, 1);
        Wizard.SlotCap = Wizard.Profile.SpellSlots = 2; Wizard.SlotRecovery = 1; Wizard.Specials = {ECombatCommandType::MagicMissile}; Result.Add(Wizard);
        return Result;
    }();
    return Values;
}
}
const TArray<FBuildDefinition>& FBuildCatalog::All() { return Definitions(); }
const FBuildDefinition* FBuildCatalog::Find(const FString& Id)
{ return Definitions().FindByPredicate([&](const auto& B) { return B.Id == Id; }); }
bool FBuildCatalog::Create(const FString& BuildId, int32 Id, const FString& Name, FCombatantState& Out)
{
    const auto* B = Find(BuildId);
    if (!B || Id < 1 || Name.IsEmpty() || Name.Len() > 80) { return false; }
    Out = B->Profile; Out.Id = Id; Out.Name = Name; return true;
}
bool FBuildCatalog::HasSpecial(const FCombatantState& Hero, ECombatCommandType Action)
{
    if (const auto* B = Find(Hero.BuildId)) { return B->Specials.Contains(Action); }
    // Standalone combat fixtures may intentionally use custom profiles without catalog identity.
    return Hero.BuildId.IsEmpty() && ((Action == ECombatCommandType::SecondWind && Hero.AdventurerRole == EAdventurerRole::Fighter) ||
        (Action == ECombatCommandType::PrecisionStrike && Hero.FighterSubclass == EFighterSubclass::BattleMaster) ||
        (Action == ECombatCommandType::CureWounds && Hero.AdventurerRole == EAdventurerRole::Cleric) ||
        (Action == ECombatCommandType::MagicMissile && Hero.AdventurerRole == EAdventurerRole::Wizard));
}
int32 FBuildCatalog::BasicDamage(const FBuildDefinition& Build, int32 Level) { return Build.Profile.DamageBonus + Level - 1; }
bool FBuildCatalog::ValidResources(const FCombatantState& Hero)
{
    const auto* B = Find(Hero.BuildId);
    return B && Hero.SecondWindUses >= 0 && Hero.SecondWindUses <= B->WindCap && Hero.SpellSlots >= 0 && Hero.SpellSlots <= B->SlotCap && Hero.ManeuverUses >= 0 && Hero.ManeuverUses <= B->ManeuverCap;
}
