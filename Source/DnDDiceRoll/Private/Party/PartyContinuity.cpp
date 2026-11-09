#include "Party/PartyContinuity.h"
#include "Party/BuildCatalog.h"
#include "Rules/ClassFeatureRules.h"
#include "Rules/SpellRules.h"
#include "Rules/ProficiencyRules.h"
#include "Rules/AbilityRules.h"
#include "Rules/MonsterRules.h"
#include "Rules/AdvancementRules.h"


namespace
{
FClassLevels ToRulesLevels(const FCombatClassLevels& C)
{
    FClassLevels Out;
    Out.Fighter = C.Fighter; Out.Rogue = C.Rogue; Out.Cleric = C.Cleric; Out.Wizard = C.Wizard;
    Out.Barbarian = C.Barbarian; Out.Monk = C.Monk; Out.Paladin = C.Paladin; Out.Ranger = C.Ranger;
    Out.Sorcerer = C.Sorcerer; Out.Warlock = C.Warlock; Out.Druid = C.Druid; Out.Bard = C.Bard; Out.Artificer = C.Artificer;
    return Out;
}

void FromRulesLevels(FCombatClassLevels& C, const FClassLevels& In)
{
    C.Fighter = In.Fighter; C.Rogue = In.Rogue; C.Cleric = In.Cleric; C.Wizard = In.Wizard;
    C.Barbarian = In.Barbarian; C.Monk = In.Monk; C.Paladin = In.Paladin; C.Ranger = In.Ranger;
    C.Sorcerer = In.Sorcerer; C.Warlock = In.Warlock; C.Druid = In.Druid; C.Bard = In.Bard; C.Artificer = In.Artificer;
}

/** If ClassLevels are all zero, treat Level as levels in AdventurerRole (legacy / Compact). */
FClassLevels EffectiveLevels(const FCombatantState& Unit)
{
    FClassLevels Levels = ToRulesLevels(Unit.ClassLevels);
    if (Levels.CharacterLevel() == 0 && Unit.Level > 0 && Unit.AdventurerRole != EAdventurerRole::None)
    {
        Levels.Set(Unit.AdventurerRole, Unit.Level);
    }
    return Levels;
}

}

bool FPartyContinuity::Carry(FCombatBattle& Battle, const FCombatEncounter& Layout, int32 Seed, FString& Error)
{
    Error.Reset();
    if (Battle.Snapshot().Phase != EBattlePhase::Victory)
    {
        Error = TEXT("Win the encounter before continuing."); return false;
    }
    FCombatEncounter Next = Layout;
    TSet<int32> CarriedIds;
    for (FCombatantState& Spawn : Next.Combatants)
    {
        if (Spawn.Side != ECombatSide::Party) { continue; }
        const FCombatantState* Previous = Battle.Find(Spawn.Id);
        if (!Previous || Previous->Side != ECombatSide::Party || CarriedIds.Contains(Spawn.Id))
        {
            Error = TEXT("Layout must retain exactly the existing party identities."); return false;
        }
        CarriedIds.Add(Spawn.Id);
        const FIntPoint Cell = Spawn.Cell;
        Spawn = *Previous;
        Spawn.Cell = Cell;
    }
    for (const FCombatantState& Previous : Battle.Snapshot().Encounter.Combatants)
    {
        if (Previous.Side == ECombatSide::Party && !CarriedIds.Contains(Previous.Id))
        {
            Error = TEXT("Layout omitted a persistent adventurer, including a dead member."); return false;
        }
    }
    FCombatBattle Candidate;
    if (!Candidate.Start(Next, Seed, Error)) { return false; }
    Battle = MoveTemp(Candidate);
    return true;
}

bool FPartyContinuity::Reward(FCombatBattle& Battle, int32 RecipientId, int32 Potions, FString& Error)
{
    Error.Reset();
    const FCombatantState* Recipient = Battle.Find(RecipientId);
    if (Battle.Snapshot().Phase != EBattlePhase::Victory || !Recipient || Recipient->Side != ECombatSide::Party ||
        Recipient->Life != ECombatLife::Conscious || Potions < 1 || Potions > 2)
    {
        Error = TEXT("Select a conscious survivor for the victory reward."); return false;
    }
    FCombatantState* Survivor = Battle.MutableFind(RecipientId);
    Survivor->WeaponRank = FMath::Min(3, Survivor->WeaponRank + 1);
    Survivor->Potions = FMath::Min(9, Survivor->Potions + Potions);
    return true;
}

namespace
{
constexpr int32 CampHealing = 10;
constexpr int32 CampResource = 1;
constexpr int32 ResourceCap = 2;
}

FString FPartyContinuity::RecoveryDescription()
{
    return FString::Printf(TEXT("Survivors: +%d HP, +%d Wind/slot/Rage/Kit (caps: max HP/%d). No revival or potions."), CampHealing, CampResource, ResourceCap);
}

bool FPartyContinuity::Recover(FCombatBattle& Battle, FString& Error)
{
    Error.Reset();
    if (Battle.Snapshot().Phase != EBattlePhase::Victory) { Error = TEXT("Recovery requires a completed victory."); return false; }
    for (FCombatantState& Survivor : Battle.State.Encounter.Combatants)
    {
        if (Survivor.Side != ECombatSide::Party || Survivor.Life != ECombatLife::Conscious) { continue; }
        Survivor.HP = FMath::Min(Survivor.MaxHP, Survivor.HP + CampHealing);
        if (const auto* B = FBuildCatalog::Find(Survivor.BuildId))
        {
            Survivor.KitUses = FMath::Min(B->KitCap, Survivor.KitUses + B->KitRecovery);
            Survivor.RageUses = FMath::Min(B->RageCap, Survivor.RageUses + B->RageRecovery);
            Survivor.SecondWindUses = FMath::Min(B->WindCap, Survivor.SecondWindUses + B->WindRecovery);
            if (Battle.Snapshot().Encounter.Ruleset == ERuleset::SRD && Survivor.AdventurerRole == EAdventurerRole::Fighter)
            { Survivor.ActionSurgeUses = FClassFeatureRules::ActionSurgeUses(Survivor.Level); }
            Survivor.SpellSlots = FMath::Min(B->SlotCap, Survivor.SpellSlots + B->SlotRecovery);
            if (Battle.Snapshot().Encounter.Ruleset == ERuleset::SRD)
            {
                Battle.FillSrdSpellSlots(Survivor);
            }
            Survivor.ManeuverUses = FMath::Min(B->ManeuverCap, Survivor.ManeuverUses + B->ManeuverRecovery);
        }
        else
        {
            if (Survivor.AdventurerRole == EAdventurerRole::Fighter)
            {
                Survivor.SecondWindUses = FMath::Min(ResourceCap, Survivor.SecondWindUses + CampResource);
                if (Battle.Snapshot().Encounter.Ruleset == ERuleset::SRD)
                { Survivor.ActionSurgeUses = FClassFeatureRules::ActionSurgeUses(Survivor.Level); }
            }
            if (Survivor.AdventurerRole == EAdventurerRole::Cleric || Survivor.AdventurerRole == EAdventurerRole::Wizard)
            {
                Survivor.SpellSlots = FMath::Min(ResourceCap, Survivor.SpellSlots + CampResource);
                if (Battle.Snapshot().Encounter.Ruleset == ERuleset::SRD)
                {
                    Battle.FillSrdSpellSlots(Survivor);
                }
            }
        }
    }
    return true;
}

bool FPartyContinuity::Advance(FCombatBattle& Battle, const FCombatEncounter& Layout, int32 Seed, int32 RecipientId, FString& Error)
{
    FCombatBattle Candidate = Battle;
    if (!Reward(Candidate, RecipientId, 1, Error) || !Carry(Candidate, Layout, Seed, Error)) { return false; }
    Battle = MoveTemp(Candidate);
    return true;
}

void FPartyContinuity::AwardEncounterExperience(FCombatBattle& Battle)
{
    if (Battle.Snapshot().Encounter.Ruleset != ERuleset::SRD) { return; }
    int32 TotalXp = 0;
    for (const FCombatantState& Unit : Battle.Snapshot().Encounter.Combatants)
    {
        if (Unit.Side == ECombatSide::Enemy)
        {
            TotalXp += FMonsterRules::ExperienceReward(Unit.Role);
        }
    }
    if (TotalXp <= 0) { return; }
    int32 Recipients = 0;
    for (const FCombatantState& Hero : Battle.Snapshot().Encounter.Combatants)
    {
        if (Hero.Side == ECombatSide::Party && Hero.Life == ECombatLife::Conscious) { ++Recipients; }
    }
    if (Recipients <= 0) { return; }
    // SRD: divide encounter XP evenly among the party (floor; remainder discarded).
    const int32 Share = TotalXp / Recipients;
    if (Share <= 0) { return; }
    for (FCombatantState& Hero : Battle.State.Encounter.Combatants)
    {
        if (Hero.Side != ECombatSide::Party || Hero.Life != ECombatLife::Conscious) { continue; }
        Hero.ExperiencePoints += Share;
        Hero.bLevelUpAvailable = FAdvancementRules::PendingLevelUps(Hero.Level, Hero.ExperiencePoints) > 0;
    }
}

bool FPartyContinuity::ApplyLevelUp(FCombatBattle& Battle, int32 AdventurerId, EAdventurerRole ClassToAdvance, int32 HitDieRoll, FString& Error)
{
    Error.Reset();
    if (Battle.Snapshot().Encounter.Ruleset != ERuleset::SRD)
    {
        Error = TEXT("Level Up applies in SRD mode only."); return false;
    }
    if (Battle.Snapshot().Phase == EBattlePhase::Fighting)
    {
        Error = TEXT("Level Up is only available between battles."); return false;
    }
    FCombatantState* Hero = Battle.MutableFind(AdventurerId);
    if (!Hero || Hero->Side != ECombatSide::Party || Hero->Life != ECombatLife::Conscious)
    {
        Error = TEXT("Select a conscious party member to level up."); return false;
    }
    if (FAdvancementRules::PendingLevelUps(Hero->Level, Hero->ExperiencePoints) < 1)
    {
        Error = TEXT("Not enough experience for a level."); return false;
    }
    FClassLevels Levels = EffectiveLevels(*Hero);
    const bool bNewClass = Levels.Get(ClassToAdvance) == 0 && Levels.CharacterLevel() > 0;
    const FAbilityScores Scores = FAbilityRules::PrebuiltScores(
        Hero->AdventurerRole != EAdventurerRole::None ? Hero->AdventurerRole : ClassToAdvance);
    if (bNewClass && !FAdvancementRules::MeetsMulticlassPrerequisite(
            ClassToAdvance, Scores.Strength, Scores.Dexterity, Scores.Constitution,
            Scores.Intelligence, Scores.Wisdom, Scores.Charisma))
    {
        Error = TEXT("Ability scores do not meet the multiclass prerequisite (13)."); return false;
    }
    const int32 NewClassLevel = Levels.Get(ClassToAdvance) + 1;
    const int32 ConMod = FAbilityRules::Modifier(Scores.Constitution);
    const int32 ActualGain = FAdvancementRules::HpGainForClassLevel(ClassToAdvance, HitDieRoll, ConMod);

    Levels.Set(ClassToAdvance, NewClassLevel);
    FromRulesLevels(Hero->ClassLevels, Levels);
    Hero->Level = Levels.CharacterLevel();
    Hero->MaxHP += ActualGain;
    Hero->HP = FMath::Min(Hero->MaxHP, Hero->HP + ActualGain);
    if (const auto* Build = FBuildCatalog::Find(Hero->BuildId))
    {
        Hero->DamageBonus = FBuildCatalog::BasicDamage(*Build, Hero->Level);
    }
    // Multiclass (non-pact) slots + separate Warlock pact pool.
    Hero->SpellSlot1 = FAdvancementRules::MulticlassSlotsFor(Levels, 1);
    Hero->SpellSlot2 = FAdvancementRules::MulticlassSlotsFor(Levels, 2);
    Hero->SpellSlot3 = FAdvancementRules::MulticlassSlotsFor(Levels, 3);
    if (Levels.Warlock > 0)
    {
        Hero->PactSlotLevel = FSpellRules::MaxSlotLevel(EAdventurerRole::Warlock, Levels.Warlock);
        Hero->PactSlots = FSpellRules::SlotsFor(EAdventurerRole::Warlock, Levels.Warlock, Hero->PactSlotLevel);
    }
    else
    {
        Hero->PactSlotLevel = 0;
        Hero->PactSlots = 0;
    }
    if (Levels.Fighter > 0)
    {
        Hero->ActionSurgeUses = FClassFeatureRules::ActionSurgeUses(Levels.Fighter);
        Hero->SecondWindUses = FMath::Max(Hero->SecondWindUses, 1);
    }
    Hero->bLevelUpAvailable = FAdvancementRules::PendingLevelUps(Hero->Level, Hero->ExperiencePoints) > 0;
    return true;
}
