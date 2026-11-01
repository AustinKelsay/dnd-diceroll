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
        for (const auto* Path : {TEXT("berserker"), TEXT("totem-warrior"), TEXT("beast"), TEXT("wild-magic")})
        {
            auto B = Add(TEXT(""), TEXT("barbarian"), Path, TEXT("Barbarian"), EAdventurerRole::Barbarian, 34, 14, 8, 3, 1, 1);
            B.Id = B.Profile.BuildId = FString(TEXT("barbarian.")) + Path;
            B.RageCap = B.Profile.RageUses = 2; B.RageRecovery = 1;
            B.Specials = {ECombatCommandType::Special1, ECombatCommandType::Special2};
            FAbilityFacts Rage; Rage.ResourceName = TEXT("Rage"); Rage.ResourceCost = 1; Rage.Name = TEXT("Rage"); Rage.Description = TEXT("Bonus + 1 Rage use; self; +2 basic damage, reduce damage by 1 through next own turn");
            B.Abilities.Add(Rage);
            FAbilityFacts A; A.Command = ECombatCommandType::Special2; A.Target = EAbilityTarget::Enemy;
            if (FString(Path) == TEXT("berserker")) { A.Name = TEXT("Frenzy"); A.Range = 1; A.Amount = 4; A.Effect = EAbilityEffect::Strike; A.Description = TEXT("Bonus; raging; enemy range 1; guaranteed 4 damage"); }
            else if (FString(Path) == TEXT("totem-warrior")) { A.Name = TEXT("Shelter"); A.bBonus = false; A.Range = 3; A.Amount = 3; A.Target = EAbilityTarget::Ally; A.Effect = EAbilityEffect::Protect; A.Description = TEXT("Action; raging; other ally range 3; reduce damage by 3 until next own turn"); }
            else if (FString(Path) == TEXT("beast")) { A.Name = TEXT("Long Claw"); A.Range = 2; A.Amount = 6; A.bBonus = false; A.Effect = EAbilityEffect::Strike; A.Description = TEXT("Action; raging; enemy range 2; guaranteed 6 damage"); }
            else { A.Name = TEXT("Wild Bolt"); A.Amount = 4; A.Range = 3; A.bBonus = false; A.Effect = EAbilityEffect::Surge; A.Description = TEXT("Action; raging; enemy range 3; seeded outcome 1: 4 damage or 2: 8 damage"); }
            B.Abilities.Add(A); Result.Add(B);
        }
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
    return B && Hero.SecondWindUses >= 0 && Hero.SecondWindUses <= B->WindCap && Hero.SpellSlots >= 0 && Hero.SpellSlots <= B->SlotCap && Hero.ManeuverUses >= 0 && Hero.ManeuverUses <= B->ManeuverCap && Hero.RageUses >= 0 && Hero.RageUses <= B->RageCap;
}

TArray<FAbilityFacts> FBuildCatalog::Abilities(const FCombatantState& Hero)
{
    const auto* B = Find(Hero.BuildId);
    TArray<FAbilityFacts> Result = B ? B->Abilities : TArray<FAbilityFacts>();
    if (B && Result.IsEmpty())
    {
        for (const auto Command : B->Specials)
        {
            FAbilityFacts A; A.Command = Command; A.Description = FCombatBattle::DescribeAction(Command); A.ResourceCost = 1;
            if (Command == ECombatCommandType::SecondWind) { A.Name = TEXT("Second Wind"); A.Effect = EAbilityEffect::Healing; A.ResourceName = TEXT("Wind"); A.bResourceAvailable = Hero.SecondWindUses > 0; }
            else if (Command == ECombatCommandType::PrecisionStrike) { A.Name = TEXT("Precision Strike"); A.Effect = EAbilityEffect::WeaponAttack; A.ResourceName = TEXT("Maneuver"); A.Target = EAbilityTarget::Enemy; A.Range = Hero.Range; A.bBonus = false; A.bResourceAvailable = Hero.ManeuverUses > 0; }
            else if (Command == ECombatCommandType::CureWounds) { A.Name = TEXT("Cure Wounds"); A.Effect = EAbilityEffect::Healing; A.ResourceName = TEXT("Slot"); A.Target = EAbilityTarget::Ally; A.Range = BuildRules::TouchRange; A.bBonus = false; A.bResourceAvailable = Hero.SpellSlots > 0; }
            else { A.Name = TEXT("Magic Missile"); A.Effect = EAbilityEffect::Missile; A.ResourceName = TEXT("Slot"); A.Target = EAbilityTarget::Enemy; A.Range = BuildRules::MissileRange; A.bBonus = false; A.bResourceAvailable = Hero.SpellSlots > 0; }
            Result.Add(A);
        }
        return Result;
    }
    for (auto& A : Result) { A.bResourceAvailable = A.Effect == EAbilityEffect::Rage ? Hero.RageUses > 0 && Hero.RageTurns == 0 : Hero.RageTurns > 0; }
    return Result;
}

FString FBuildCatalog::DisplayName(const FCombatantState& Hero)
{
    FString Kit = Hero.SubclassId;
    Kit.ReplaceInline(TEXT("-"), TEXT(" "));
    TArray<FString> Words; Kit.ParseIntoArray(Words, TEXT(" "));
    Kit.Reset(); for (auto& Word : Words) { Word[0] = FChar::ToUpper(Word[0]); Kit += (Kit.IsEmpty() ? TEXT("") : TEXT(" ")) + Word; }
    return Hero.Role + TEXT(" / ") + (Kit.IsEmpty() ? TEXT("Interim base kit") : Kit);
}
FString FBuildCatalog::Status(const FCombatantState& Hero)
{
    const auto* B = Find(Hero.BuildId); if (!B) { return FString(); }
    FString Text;
    if (B->WindCap) { Text += FString::Printf(TEXT("Wind %d / "), Hero.SecondWindUses); }
    if (B->SlotCap) { Text += FString::Printf(TEXT("Slots %d / "), Hero.SpellSlots); }
    if (B->ManeuverCap) { Text += FString::Printf(TEXT("Maneuvers %d / "), Hero.ManeuverUses); }
    if (B->RageCap) { Text += FString::Printf(TEXT("Rage %d; %d own turn ends / "), Hero.RageUses, Hero.RageTurns); }
    if (Hero.GuardTargetId != INDEX_NONE) { Text += FString::Printf(TEXT("Shelter -> %d / "), Hero.GuardTargetId); }
    if (Hero.LastSurge) { Text += FString::Printf(TEXT("Last surge %d / "), Hero.LastSurge); }
    return Text;
}
int32 FBuildCatalog::AttackModifier(const FCombatantState& Hero)
{
    return Hero.DamageBonus + Hero.WeaponRank + (Hero.RageTurns > 0 ? 2 : 0);
}
