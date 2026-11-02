#include "Rules/MonsterRules.h"

namespace
{
FAbilityScores Make(int32 Str, int32 Dex, int32 Con, int32 Int, int32 Wis, int32 Cha)
{
    FAbilityScores S; S.Strength = Str; S.Dexterity = Dex; S.Constitution = Con; S.Intelligence = Int; S.Wisdom = Wis; S.Charisma = Cha; return S;
}
const TArray<FMonsterStatBlock>& Blocks()
{
    static const TArray<FMonsterStatBlock> All = []()
    {
        TArray<FMonsterStatBlock> Out;
        // Bandit: AC 12 (leather), 11 (2d8+2), scimitar +3 1d6+1.
        { FMonsterStatBlock B; B.Name = TEXT("Bandit"); B.ArmorClass = 12; B.HitDiceCount = 2; B.Scores = Make(11, 12, 12, 10, 10, 10); B.AttackAbility = EAbility::Dexterity; B.DamageDieSides = 6; B.bWearsLeather = true; Out.Add(B); }
        // Bandit with its SRD light crossbow (+3, 1d8+1, range 80/320 ft): the early-route archer.
        { FMonsterStatBlock B; B.Name = TEXT("Bandit Archer"); B.ArmorClass = 12; B.HitDiceCount = 2; B.Scores = Make(11, 12, 12, 10, 10, 10); B.AttackAbility = EAbility::Dexterity; B.Range = 4; B.DamageDieSides = 8; B.bWearsLeather = true; B.bSkirmisher = true; Out.Add(B); }
        // Thug: AC 11 (leather), 32 (5d8+10), Multiattack: two mace attacks +4 1d6+2.
        { FMonsterStatBlock B; B.Name = TEXT("Thug"); B.ArmorClass = 11; B.HitDiceCount = 5; B.Scores = Make(15, 11, 14, 10, 10, 11); B.Attacks = 2; B.DamageDieSides = 6; B.bWearsLeather = true; Out.Add(B); }
        // Scout: AC 13 (leather), 16 (3d8+3), Multiattack: two longbow attacks +4 1d8+2.
        { FMonsterStatBlock B; B.Name = TEXT("Scout"); B.ArmorClass = 13; B.HitDiceCount = 3; B.Scores = Make(11, 14, 12, 11, 13, 11); B.AttackAbility = EAbility::Dexterity; B.Attacks = 2; B.Range = 4; B.DamageDieSides = 8; B.bWearsLeather = true; B.bSkirmisher = true; Out.Add(B); }
        // Acolyte: AC 10, 9 (2d8), spellcaster (WIS, DC 12): Sacred Flame 1d8 radiant, DEX save.
        { FMonsterStatBlock B; B.Name = TEXT("Acolyte"); B.ArmorClass = 10; B.HitDiceCount = 2; B.Scores = Make(10, 10, 10, 10, 14, 11); B.Range = 4; B.DamageDieSides = 8; B.Kind = EMonsterAttack::SaveCantrip; B.bSkirmisher = true; Out.Add(B); }
        return Out;
    }();
    return All;
}
}

const FMonsterStatBlock* FMonsterRules::Find(const FString& Role)
{
    return Blocks().FindByPredicate([&Role](const FMonsterStatBlock& B) { return Role == B.Name; });
}
int32 FMonsterRules::HitPoints(const FMonsterStatBlock& Block)
{
    return FMath::Max(1, Block.HitDiceCount * (Block.HitDieSides + 1) / 2 + Block.HitDiceCount * FAbilityRules::Modifier(Block.Scores.Constitution));
}
int32 FMonsterRules::AttackBonus(const FMonsterStatBlock& Block) { return Block.ProficiencyBonus + FAbilityRules::Modifier(Block.Scores.Get(Block.AttackAbility)); }
int32 FMonsterRules::DamageBonus(const FMonsterStatBlock& Block) { return Block.Kind == EMonsterAttack::SaveCantrip ? 0 : FAbilityRules::Modifier(Block.Scores.Get(Block.AttackAbility)); }
int32 FMonsterRules::SpellSaveDc(const FMonsterStatBlock& Block) { return 8 + Block.ProficiencyBonus + FAbilityRules::Modifier(Block.Scores.Get(Block.SpellAbility)); }
int32 FMonsterRules::AttacksPerAction(const FString& Role) { const auto* B = Find(Role); return B ? B->Attacks : 1; }