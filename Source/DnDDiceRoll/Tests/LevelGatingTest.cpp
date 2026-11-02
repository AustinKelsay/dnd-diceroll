#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Rules/ClassFeatureRules.h"
#include "Rules/SpellRules.h"
#include "Rules/SpellcastingRules.h"
#include "Rules/RestRules.h"
#include "Rules/LevelRules.h"
#include "Rules/CantripRules.h"
#include "Rules/ProficiencyRules.h"
#include "Rules/StandardActionRules.h"
#include "Party/BuildCatalog.h"
#include "Combat/CombatBattle.h"

namespace LevelGating
{
struct FGate { EAdventurerRole Role; const TCHAR* Name; int32 Level; };
using R = EAdventurerRole;
// SRD 5.1 class tables, levels 1-5 (subclass = the SRD archetype; Superiority Dice is Landon's Battle Master directive).
static const FGate Gates[] = {
    {R::Fighter, TEXT("Fighting Style"), 1}, {R::Fighter, TEXT("Second Wind"), 1}, {R::Fighter, TEXT("Action Surge"), 2},
    {R::Fighter, TEXT("Champion: Improved Critical"), 3}, {R::Fighter, TEXT("Superiority Dice"), 3}, {R::Fighter, TEXT("Ability Score Improvement"), 4}, {R::Fighter, TEXT("Extra Attack"), 5},
    {R::Rogue, TEXT("Expertise"), 1}, {R::Rogue, TEXT("Sneak Attack"), 1}, {R::Rogue, TEXT("Cunning Action"), 2}, {R::Rogue, TEXT("Thief: Fast Hands"), 3}, {R::Rogue, TEXT("Uncanny Dodge"), 5},
    {R::Cleric, TEXT("Spellcasting"), 1}, {R::Cleric, TEXT("Life: Disciple of Life"), 1}, {R::Cleric, TEXT("Channel Divinity"), 2}, {R::Cleric, TEXT("Destroy Undead"), 5},
    {R::Wizard, TEXT("Spellcasting"), 1}, {R::Wizard, TEXT("Arcane Recovery"), 1}, {R::Wizard, TEXT("Evocation: Sculpt Spells"), 2}, {R::Wizard, TEXT("Counterspell"), 5},
    {R::Barbarian, TEXT("Rage"), 1}, {R::Barbarian, TEXT("Unarmored Defense"), 1}, {R::Barbarian, TEXT("Reckless Attack"), 2}, {R::Barbarian, TEXT("Danger Sense"), 2},
    {R::Barbarian, TEXT("Berserker: Frenzy"), 3}, {R::Barbarian, TEXT("Extra Attack"), 5}, {R::Barbarian, TEXT("Fast Movement"), 5},
    {R::Monk, TEXT("Martial Arts"), 1}, {R::Monk, TEXT("Unarmored Defense"), 1}, {R::Monk, TEXT("Ki"), 2}, {R::Monk, TEXT("Unarmored Movement"), 2},
    {R::Monk, TEXT("Deflect Missiles"), 3}, {R::Monk, TEXT("Open Hand Technique"), 3}, {R::Monk, TEXT("Slow Fall"), 4}, {R::Monk, TEXT("Extra Attack"), 5}, {R::Monk, TEXT("Stunning Strike"), 5},
    {R::Paladin, TEXT("Divine Sense"), 1}, {R::Paladin, TEXT("Lay on Hands"), 1}, {R::Paladin, TEXT("Fighting Style"), 2}, {R::Paladin, TEXT("Spellcasting"), 1},
    {R::Paladin, TEXT("Divine Smite"), 2}, {R::Paladin, TEXT("Divine Health"), 3}, {R::Paladin, TEXT("Channel Divinity"), 3}, {R::Paladin, TEXT("Extra Attack"), 5},
    {R::Ranger, TEXT("Favored Enemy"), 1}, {R::Ranger, TEXT("Natural Explorer"), 1}, {R::Ranger, TEXT("Fighting Style"), 2}, {R::Ranger, TEXT("Spellcasting"), 1},
    {R::Ranger, TEXT("Hunter: Colossus Slayer"), 3}, {R::Ranger, TEXT("Primeval Awareness"), 3}, {R::Ranger, TEXT("Extra Attack"), 5},
    {R::Sorcerer, TEXT("Spellcasting"), 1}, {R::Sorcerer, TEXT("Draconic: Dragon Resilience"), 1}, {R::Sorcerer, TEXT("Sorcery Points"), 2},
    {R::Sorcerer, TEXT("Metamagic: Quickened Spell"), 3}, {R::Sorcerer, TEXT("Counterspell"), 5},
    // Warlock and Bard: SRD 5.2.1 class tables (Pact Boon = Pact invocations; no Song of Rest).
    {R::Warlock, TEXT("Pact Magic"), 1}, {R::Warlock, TEXT("Eldritch Invocations"), 1}, {R::Warlock, TEXT("Magical Cunning"), 2},
    {R::Warlock, TEXT("Fiend: Dark One's Blessing"), 3}, {R::Warlock, TEXT("Fiend: Fiend Spells"), 3},
    {R::Druid, TEXT("Spellcasting"), 1}, {R::Druid, TEXT("Wild Shape"), 2}, {R::Druid, TEXT("Land: Natural Recovery"), 2},
    {R::Bard, TEXT("Spellcasting"), 1}, {R::Bard, TEXT("Bardic Inspiration"), 1}, {R::Bard, TEXT("Expertise"), 2}, {R::Bard, TEXT("Jack of All Trades"), 2},
    {R::Bard, TEXT("Lore: Bonus Proficiencies"), 3}, {R::Bard, TEXT("Lore: Cutting Words"), 3}, {R::Bard, TEXT("Font of Inspiration"), 5},
};
// SRD 5.2.1 Warlock and Bard features past the engine cap: data only, gated by class level in DataFeaturesThrough.
static const FGate DataGates[] = {
    {R::Warlock, TEXT("Fiend: Dark One's Own Luck"), 6}, {R::Warlock, TEXT("Contact Patron"), 9}, {R::Warlock, TEXT("Fiend: Fiendish Resilience"), 10},
    {R::Warlock, TEXT("Mystic Arcanum (level 6 spell)"), 11}, {R::Warlock, TEXT("Mystic Arcanum (level 7 spell)"), 13}, {R::Warlock, TEXT("Fiend: Hurl Through Hell"), 14},
    {R::Warlock, TEXT("Mystic Arcanum (level 8 spell)"), 15}, {R::Warlock, TEXT("Mystic Arcanum (level 9 spell)"), 17}, {R::Warlock, TEXT("Epic Boon"), 19},
    {R::Warlock, TEXT("Eldritch Master"), 20},
    {R::Bard, TEXT("Lore: Magical Discoveries"), 6}, {R::Bard, TEXT("Countercharm"), 7}, {R::Bard, TEXT("Expertise (two more skills)"), 9},
    {R::Bard, TEXT("Magical Secrets"), 10}, {R::Bard, TEXT("Lore: Peerless Skill"), 14}, {R::Bard, TEXT("Superior Inspiration"), 18},
    {R::Bard, TEXT("Epic Boon"), 19}, {R::Bard, TEXT("Words of Creation"), 20},
};
bool HasData(EAdventurerRole Role, int32 Level, const TCHAR* Name)
{
    return FClassFeatureRules::DataFeaturesThrough(Role, Level).ContainsByPredicate([Name](const FClassFeature& F) { return FCString::Strcmp(F.Name, Name) == 0; });
}
bool Has(EAdventurerRole Role, int32 Level, const TCHAR* Name)
{
    return FClassFeatureRules::FeaturesThrough(Role, Level).ContainsByPredicate([Name](const FClassFeature& F) { return FCString::Strcmp(F.Name, Name) == 0; });
}
FCombatantState Hero(EAdventurerRole Primary, int32 PrimaryLevels, EAdventurerRole Secondary = EAdventurerRole::None, int32 SecondaryLevels = 0)
{
    FCombatantState H; H.Side = ECombatSide::Party; H.AdventurerRole = Primary; H.SecondaryRole = Secondary; H.SecondaryLevel = SecondaryLevels; H.Level = PrimaryLevels + SecondaryLevels;
    return H;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLevelGatingTest, "Project.Expedition.Rules.LevelGating",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FLevelGatingTest::RunTest(const FString& Parameters)
{
    using namespace LevelGating;
    // Every SRD feature: absent the level before, present at its SRD level.
    for (const FGate& G : Gates)
    {
        if (G.Level > 1) { TestFalse(FString::Printf(TEXT("%s not before %d"), G.Name, G.Level), Has(G.Role, G.Level - 1, G.Name)); }
        TestTrue(FString::Printf(TEXT("%s at %d"), G.Name, G.Level), Has(G.Role, G.Level, G.Name));
    }
    // Every row in the code table is covered by the SRD gate table (no feature at an unaudited level).
    for (int32 Role = 1; Role <= static_cast<int32>(EAdventurerRole::Bard); ++Role)
    {
        const EAdventurerRole Class = static_cast<EAdventurerRole>(Role);
        for (const FClassFeature& F : FClassFeatureRules::FeaturesThrough(Class, 5))
        {
            if (FCString::Strcmp(F.Name, TEXT("Ability Score Improvement")) == 0) { TestEqual(TEXT("ASI at 4"), F.Level, 4); continue; }
            if (FCString::Strcmp(F.Name, TEXT("Shield")) == 0) { TestEqual(TEXT("Shield spell at 1"), F.Level, 1); continue; }
            bool bAudited = false;
            for (const FGate& G : Gates) { bAudited |= G.Role == Class && FCString::Strcmp(G.Name, F.Name) == 0 && G.Level == F.Level; }
            TestTrue(FString::Printf(TEXT("%s level is SRD-audited"), F.Name), bAudited);
        }
    }
    // Past the engine cap: each data feature appears exactly at its SRD level and never in the playable (capped) list.
    for (const FGate& G : DataGates)
    {
        TestFalse(FString::Printf(TEXT("%s (data) not before %d"), G.Name, G.Level), HasData(G.Role, G.Level - 1, G.Name));
        TestTrue(FString::Printf(TEXT("%s (data) at %d"), G.Name, G.Level), HasData(G.Role, G.Level, G.Name));
        TestFalse(FString::Printf(TEXT("%s is past the engine cap"), G.Name), Has(G.Role, 20, G.Name));
        TestTrue(FString::Printf(TEXT("%s is above level %d"), G.Name, FClassFeatureRules::EngineLevelCap), G.Level > FClassFeatureRules::EngineLevelCap);
    }
    for (const EAdventurerRole Class : {R::Warlock, R::Bard})
    {
        for (const FClassFeature& F : FClassFeatureRules::DataFeaturesThrough(Class, 20))
        {
            bool bAudited = FCString::Strcmp(F.Name, TEXT("Ability Score Improvement")) == 0 && (F.Level == 4 || F.Level == 8 || F.Level == 12 || F.Level == 16);
            for (const FGate& G : Gates) { bAudited |= G.Role == Class && FCString::Strcmp(G.Name, F.Name) == 0 && G.Level == F.Level; }
            for (const FGate& G : DataGates) { bAudited |= G.Role == Class && FCString::Strcmp(G.Name, F.Name) == 0 && G.Level == F.Level; }
            TestTrue(FString::Printf(TEXT("%s %d (data) is SRD 5.2.1-audited"), F.Name, F.Level), bAudited);
        }
    }
    TestFalse(TEXT("SRD 5.2.1 has no Song of Rest"), HasData(R::Bard, 20, TEXT("Song of Rest")));
    TestFalse(TEXT("SRD 5.2.1 has no separate Pact Boon"), HasData(R::Warlock, 20, TEXT("Pact Boon")));
    // Multiclass-safe: features gate on the class's own level, not total level (Fighter 4 / Warlock 1 has no Magical Cunning).
    const FCombatantState FW = Hero(R::Fighter, 4, R::Warlock, 1);
    TestFalse(TEXT("Fighter 4 / Warlock 1: no Magical Cunning"), Has(R::Warlock, FBuildCatalog::ClassLevel(FW, R::Warlock), TEXT("Magical Cunning")));
    TestFalse(TEXT("Fighter 3 / Bard 2: no Cutting Words"), Has(R::Bard, FBuildCatalog::ClassLevel(Hero(R::Fighter, 3, R::Bard, 2), R::Bard), TEXT("Lore: Cutting Words")));
    TestTrue(TEXT("Fighter 3 / Bard 2: Jack of All Trades"), Has(R::Bard, FBuildCatalog::ClassLevel(Hero(R::Fighter, 3, R::Bard, 2), R::Bard), TEXT("Jack of All Trades")));
    // Spell slots: no spell level before the class table allows it.
    TestEqual(TEXT("Wizard 4: no 3rd"), FSpellRules::SlotsFor(R::Wizard, 4, 3), 0);
    TestEqual(TEXT("Sorcerer 5: two 3rd"), FSpellRules::SlotsFor(R::Sorcerer, 5, 3), 2);
    TestEqual(TEXT("Bard 3: two 2nd"), FSpellRules::SlotsFor(R::Bard, 3, 2), 2);
    TestEqual(TEXT("Druid 1: two 1st"), FSpellRules::SlotsFor(R::Druid, 1, 1), 2);
    TestEqual(TEXT("Paladin 1: two 1st (SRD 5.2.1)"), FSpellRules::SlotsFor(R::Paladin, 1, 1), 2);
    TestEqual(TEXT("Paladin 2: two 1st"), FSpellRules::SlotsFor(R::Paladin, 2, 1), 2);
    TestEqual(TEXT("Ranger 4: three 1st"), FSpellRules::SlotsFor(R::Ranger, 4, 1), 3);
    TestEqual(TEXT("Ranger 4: no 2nd"), FSpellRules::SlotsFor(R::Ranger, 4, 2), 0);
    TestEqual(TEXT("Paladin 5: two 2nd"), FSpellRules::SlotsFor(R::Paladin, 5, 2), 2);
    TestEqual(TEXT("Warlock 1: one 1st"), FSpellRules::SlotsFor(R::Warlock, 1, 1), 1);
    TestEqual(TEXT("Warlock 3: two 2nd, no 1st"), FSpellRules::SlotsFor(R::Warlock, 3, 2) * 10 + FSpellRules::SlotsFor(R::Warlock, 3, 1), 20);
    TestEqual(TEXT("Warlock 5: two 3rd"), FSpellRules::SlotsFor(R::Warlock, 5, 3), 2);
    TestEqual(TEXT("Fighter: none"), FSpellRules::SlotsFor(R::Fighter, 5, 1), 0);
    // Multiclass: class levels decide; two casters use the multiclass table.
    TestEqual(TEXT("Wizard 3 / Fighter 2: no 3rd"), FBuildCatalog::SlotMax(Hero(R::Wizard, 3, R::Fighter, 2), 3), 0);
    TestEqual(TEXT("Wizard 3 / Fighter 2: two 2nd"), FBuildCatalog::SlotMax(Hero(R::Wizard, 3, R::Fighter, 2), 2), 2);
    TestEqual(TEXT("Fighter 4 / Cleric 1: two 1st only"), FBuildCatalog::SlotMax(Hero(R::Fighter, 4, R::Cleric, 1), 1) * 10 + FBuildCatalog::SlotMax(Hero(R::Fighter, 4, R::Cleric, 1), 2), 20);
    TestEqual(TEXT("Wizard 3 / Cleric 2: caster 5, two 3rd"), FBuildCatalog::SlotMax(Hero(R::Wizard, 3, R::Cleric, 2), 3), 2);
    TestEqual(TEXT("Paladin 2 / Wizard 3: caster 4, no 3rd"), FBuildCatalog::SlotMax(Hero(R::Paladin, 2, R::Wizard, 3), 3), 0);
    TestEqual(TEXT("Paladin 1 / Wizard 1: caster 2 (5.2.1 rounds half casters up)"), FBuildCatalog::SlotMax(Hero(R::Paladin, 1, R::Wizard, 1), 1), 3);
    // Class level, not character level, gates class features.
    const FCombatantState FR = Hero(R::Fighter, 3, R::Rogue, 2);
    TestEqual(TEXT("Fighter 3 / Rogue 2: Fighter 3"), FBuildCatalog::ClassLevel(FR, R::Fighter), 3);
    TestEqual(TEXT("Fighter 3: no Extra Attack"), FClassFeatureRules::AttacksPerAction(R::Fighter, FBuildCatalog::ClassLevel(FR, R::Fighter)), 1);
    TestEqual(TEXT("Rogue 2: 1d6 sneak"), FClassFeatureRules::SneakAttackDice(FBuildCatalog::ClassLevel(FR, R::Rogue)), 1);
    TestTrue(TEXT("Rogue 2: Cunning Action"), FStandardActionRules::CanTakeAsBonusAction(EStandardAction::Dash, R::Rogue, FBuildCatalog::ClassLevel(FR, R::Rogue)));
    TestFalse(TEXT("Fighter 4 / Rogue 1: no Cunning Action"), FStandardActionRules::CanTakeAsBonusAction(EStandardAction::Dash, R::Rogue, FBuildCatalog::ClassLevel(Hero(R::Fighter, 4, R::Rogue, 1), R::Rogue)));
    TestEqual(TEXT("Extra Attack does not stack (Fighter 5 + Ranger 5 still 2)"),
        FMath::Max(FClassFeatureRules::AttacksPerAction(R::Fighter, 5), FClassFeatureRules::AttacksPerAction(R::Ranger, 5)), 2);
    TestEqual(TEXT("Proficiency from total level (3+2 = +3)"), FProficiencyRules::ProficiencyBonus(FR.Level), 3);
    TestEqual(TEXT("Cantrips scale on character level"), FCantripRules::ScalingTier(FR.Level), 2);
    // Spellcasting ability and hit dice for every SRD class.
    TestTrue(TEXT("Sorcerer casts with CHA"), FSpellcastingRules::SpellcastingAbility(R::Sorcerer) == EAbility::Charisma);
    TestTrue(TEXT("Druid casts with WIS"), FSpellcastingRules::SpellcastingAbility(R::Druid) == EAbility::Wisdom);
    TestTrue(TEXT("Paladin casts with CHA"), FSpellcastingRules::SpellcastingAbility(R::Paladin) == EAbility::Charisma);
    TestTrue(TEXT("Bard is a caster"), FSpellcastingRules::IsSpellcaster(R::Bard) && !FSpellcastingRules::IsSpellcaster(R::Monk));
    TestEqual(TEXT("Barbarian rests on d12"), FRestRules::HitDieSides(R::Barbarian), 12);
    TestEqual(TEXT("Sorcerer rests on d6"), FRestRules::HitDieSides(R::Sorcerer), 6);
    TestEqual(TEXT("Monk rests on d8"), FRestRules::HitDieSides(R::Monk), 8);
    return true;
}
#endif
