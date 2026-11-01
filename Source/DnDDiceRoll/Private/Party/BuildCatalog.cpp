#include "Party/BuildCatalog.h"

namespace
{
const TArray<FBuildDefinition>& Definitions()
{
    static const TArray<FBuildDefinition> Values = []
    {
        TArray<FBuildDefinition> Result;
        const TCHAR* Ids[] = {TEXT("fighter.champion"), TEXT("fighter.battle-master"), TEXT("rogue.base"), TEXT("cleric.base"), TEXT("wizard.base")};
        const TCHAR* Classes[] = {TEXT("fighter"), TEXT("fighter"), TEXT("rogue"), TEXT("cleric"), TEXT("wizard")};
        const TCHAR* Subclasses[] = {TEXT("champion"), TEXT("battle-master"), TEXT(""), TEXT(""), TEXT("")};
        const TCHAR* Roles[] = {TEXT("Fighter"), TEXT("Fighter"), TEXT("Rogue"), TEXT("Cleric"), TEXT("Wizard")};
        for (int32 I = 0; I < 5; ++I)
        {
            FBuildDefinition B;
            B.Id = Ids[I];
            auto& P = B.Profile;
            P.BuildId = B.Id; P.ClassId = Classes[I]; P.SubclassId = Subclasses[I];
            P.Role = Roles[I]; P.AdventurerRole = I < 2 ? EAdventurerRole::Fighter : static_cast<EAdventurerRole>(I);
            P.MaxHP = P.HP = I < 2 ? 30 : 22;
            P.ArmorClass = I < 2 ? 16 : 13;
            P.DamageDie = I == 4 ? 10 : 6; P.DamageBonus = I == 4 ? 0 : 3;
            P.Range = I < 2 ? 1 : I == 4 ? 4 : 3; P.InitiativeBonus = I == 2 ? 4 : 1;
            P.Potions = 1; P.bSneakWeapon = I == 2;
            P.FighterSubclass = I == 0 ? EFighterSubclass::Champion : I == 1 ? EFighterSubclass::BattleMaster : EFighterSubclass::None;
            if (I < 2) { B.WindCap = P.SecondWindUses = 2; B.WindRecovery = 1; B.Specials.Add(ECombatCommandType::SecondWind); }
            if (I == 1) { B.ManeuverCap = P.ManeuverUses = 2; B.Specials.Add(ECombatCommandType::PrecisionStrike); }
            if (I >= 3) { B.SlotCap = P.SpellSlots = 2; B.SlotRecovery = 1; B.Specials.Add(I == 3 ? ECombatCommandType::CureWounds : ECombatCommandType::MagicMissile); }
            B.CriticalThreshold = I == 0 ? 19 : 20; B.SneakDie = I == 2 ? 6 : 0;
            Result.Add(B);
        }
        return Result;
    }();
    return Values;
}
}
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
