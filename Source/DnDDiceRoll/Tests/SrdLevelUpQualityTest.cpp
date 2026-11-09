#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Combat/CombatBattle.h"
#include "Party/BuildCatalog.h"
#include "Party/PartyContinuity.h"
#include "Rules/MonsterRules.h"
#include "Rules/AdvancementRules.h"
#include "Rules/SpellRules.h"
#include "Run/ExpeditionRun.h"
#include "Persistence/ExpeditionSave.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "ExpeditionTestSupport.h"

namespace
{
FCombatantState MakeHero(int32 Id, const FString& Name, FIntPoint Cell)
{
    const FBuildDefinition* Build = FBuildCatalog::Find(TEXT("fighter.champion"));
    FCombatantState Hero = Build->Profile;
    Hero.Id = Id;
    Hero.Name = Name;
    Hero.Side = ECombatSide::Party;
    Hero.Cell = Cell;
    Hero.Level = 1;
    Hero.ExperiencePoints = 0;
    Hero.ClassLevels.Fighter = 1;
    Hero.bLevelUpAvailable = false;
    Hero.Life = ECombatLife::Conscious;
    return Hero;
}

FCombatantState MakeBandit(int32 Id, FIntPoint Cell)
{
    FCombatantState Enemy;
    Enemy.Id = Id;
    Enemy.Name = TEXT("Bandit");
    Enemy.Role = TEXT("Bandit");
    Enemy.Side = ECombatSide::Enemy;
    Enemy.Cell = Cell;
    Enemy.MaxHP = 11;
    Enemy.HP = 11;
    Enemy.ArmorClass = 12;
    Enemy.AttackBonus = 3;
    Enemy.DamageDie = 6;
    Enemy.DamageBonus = 1;
    Enemy.Speed = 4;
    Enemy.Level = 1;
    return Enemy;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSrdXpSplitExcludesDownedTest, "Project.Expedition.Rules.SrdXpSplitExcludesDowned",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSrdXpSplitExcludesDownedTest::RunTest(const FString& Parameters)
{
    // 25 XP / 2 conscious = floor 12; downed member receives nothing and does not inflate the divisor.
    FCombatEncounter Encounter;
    Encounter.Ruleset = ERuleset::SRD;
    Encounter.Width = 8;
    Encounter.Height = 8;
    FCombatantState A = MakeHero(1, TEXT("AliveA"), FIntPoint(1, 1));
    FCombatantState B = MakeHero(2, TEXT("DownedB"), FIntPoint(1, 2));
    B.Life = ECombatLife::Downed;
    B.HP = 0;
    FCombatantState C = MakeHero(3, TEXT("AliveC"), FIntPoint(1, 3));
    Encounter.Combatants = { A, B, C, MakeBandit(10, FIntPoint(4, 1)) };

    FCombatBattle Battle;
    FString Error;
    TestTrue(TEXT("Start"), Battle.Start(Encounter, 11, Error));
    Battle.State.Phase = EBattlePhase::Victory;
    FPartyContinuity::AwardEncounterExperience(Battle);

    TestEqual(TEXT("AliveA share 12"), Battle.Find(1)->ExperiencePoints, 12);
    TestEqual(TEXT("DownedB gets 0"), Battle.Find(2)->ExperiencePoints, 0);
    TestEqual(TEXT("AliveC share 12"), Battle.Find(3)->ExperiencePoints, 12);
    TestFalse(TEXT("Downed no level-up flag"), Battle.Find(2)->bLevelUpAvailable);
    TestFalse(TEXT("12 XP not enough to level"), Battle.Find(1)->bLevelUpAvailable);

    // Three conscious: 25/3 = floor 8.
    FCombatEncounter Trio = Encounter;
    Trio.Combatants[1].Life = ECombatLife::Conscious;
    Trio.Combatants[1].HP = 10;
    FCombatBattle TrioBattle;
    TestTrue(TEXT("Start trio"), TrioBattle.Start(Trio, 12, Error));
    TrioBattle.State.Phase = EBattlePhase::Victory;
    FPartyContinuity::AwardEncounterExperience(TrioBattle);
    TestEqual(TEXT("Trio floor 8"), TrioBattle.Find(1)->ExperiencePoints, 8);
    TestEqual(TEXT("Trio equal B"), TrioBattle.Find(2)->ExperiencePoints, 8);
    TestEqual(TEXT("Trio equal C"), TrioBattle.Find(3)->ExperiencePoints, 8);
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSrdPactSlotsSeparateFromMulticlassTest, "Project.Expedition.Rules.SrdPactSlotsSeparateFromMulticlass",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSrdPactSlotsSeparateFromMulticlassTest::RunTest(const FString& Parameters)
{
    // Warlock levels never feed MulticlassSpellcasterLevel; pact pool is separate.
    FClassLevels WizOnly;
    WizOnly.Set(EAdventurerRole::Wizard, 3);
    FClassLevels Mix = WizOnly;
    Mix.Set(EAdventurerRole::Warlock, 2);
    TestEqual(TEXT("Wizard3 caster level 3"), FAdvancementRules::MulticlassSpellcasterLevel(WizOnly), 3);
    TestEqual(TEXT("Wizard3+Warlock2 still caster 3"), FAdvancementRules::MulticlassSpellcasterLevel(Mix), 3);
    TestEqual(TEXT("Mix first slots = Wizard3"), FAdvancementRules::MulticlassSlotsFor(Mix, 1), 4);
    TestEqual(TEXT("Mix second slots = Wizard3"), FAdvancementRules::MulticlassSlotsFor(Mix, 2), 2);
    TestEqual(TEXT("Mix third slots 0"), FAdvancementRules::MulticlassSlotsFor(Mix, 3), 0);

    FClassLevels PactOnly;
    PactOnly.Set(EAdventurerRole::Warlock, 5);
    TestEqual(TEXT("Warlock alone caster 0"), FAdvancementRules::MulticlassSpellcasterLevel(PactOnly), 0);
    TestEqual(TEXT("Warlock alone no multiclass 1st"), FAdvancementRules::MulticlassSlotsFor(PactOnly, 1), 0);

    FCombatantState Lock;
    Lock.Id = 1;
    Lock.Name = TEXT("Hexblade");
    Lock.Side = ECombatSide::Party;
    Lock.Role = TEXT("Warlock");
    Lock.AdventurerRole = EAdventurerRole::Warlock;
    Lock.Level = 5;
    Lock.ClassLevels.Wizard = 3;
    Lock.ClassLevels.Warlock = 2;
    Lock.MaxHP = 28;
    Lock.HP = 28;
    Lock.ArmorClass = 13;
    Lock.Speed = 4;
    Lock.Cell = FIntPoint(1, 1);

    FCombatBattle Battle;
    Battle.State.Encounter.Ruleset = ERuleset::SRD;
    Battle.FillSrdSpellSlots(Lock);

    TestEqual(TEXT("Multiclass 1st from Wizard3"), Lock.SpellSlot1, 4);
    TestEqual(TEXT("Multiclass 2nd from Wizard3"), Lock.SpellSlot2, 2);
    TestEqual(TEXT("Multiclass 3rd none"), Lock.SpellSlot3, 0);
    TestEqual(TEXT("Pact level at WL2 is 1st"), Lock.PactSlotLevel, 1);
    TestEqual(TEXT("Pact slots at WL2 is 2"), Lock.PactSlots, 2);
    TestEqual(TEXT("Remaining 1st = multi+pact"), Battle.SpellSlotsRemaining(Lock, 1), 6);
    TestEqual(TEXT("Remaining 2nd = multi only"), Battle.SpellSlotsRemaining(Lock, 2), 2);

    // Spending prefers multiclass slots before pact.
    TestTrue(TEXT("Spend 1st from multi"), Battle.SpendSpellSlot(Lock, 1));
    TestEqual(TEXT("Multi 1st now 3"), Lock.SpellSlot1, 3);
    TestEqual(TEXT("Pact untouched"), Lock.PactSlots, 2);
    Lock.SpellSlot1 = 0;
    TestTrue(TEXT("Spend 1st from pact"), Battle.SpendSpellSlot(Lock, 1));
    TestEqual(TEXT("Pact now 1"), Lock.PactSlots, 1);
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSrdSchema7WithoutPactMigratesTest, "Project.Expedition.Persistence.SrdSchema7WithoutPactMigrates",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSrdSchema7WithoutPactMigratesTest::RunTest(const FString& Parameters)
{
    FExpeditionRun Run;
    FString Error, Json;
    TestTrue(TEXT("Start SRD"), Run.NewRun(42, Error, TEXT("schema7-no-pact"), EFighterSubclass::Champion, ERuleset::SRD));
    FExpeditionSaveData Data;
    Data.Current = Run.Snapshot();
    TestTrue(TEXT("Encode"), FExpeditionSave::Encode(Data, Json, Error));

    TSharedPtr<FJsonObject> Object;
    TestTrue(TEXT("Parse"), FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Object) && Object.IsValid());
    Object->SetNumberField(TEXT("Version"), 7);
    auto StripPact = [](const TSharedPtr<FJsonObject>& Unit)
    {
        if (!Unit.IsValid()) { return; }
        Unit->RemoveField(TEXT("PactSlots"));
        Unit->RemoveField(TEXT("PactSlotLevel"));
    };
    if (TSharedPtr<FJsonObject> Current = Object->GetObjectField(TEXT("Current")))
    {
        if (TSharedPtr<FJsonObject> Battle = Current->GetObjectField(TEXT("Battle")))
        {
            if (TSharedPtr<FJsonObject> Encounter = Battle->GetObjectField(TEXT("Encounter")))
            {
                const TArray<TSharedPtr<FJsonValue>>* Combatants = nullptr;
                if (Encounter->TryGetArrayField(TEXT("Combatants"), Combatants) && Combatants)
                {
                    for (const TSharedPtr<FJsonValue>& Value : *Combatants)
                    {
                        if (Value.IsValid() && Value->Type == EJson::Object) { StripPact(Value->AsObject()); }
                    }
                }
            }
        }
    }
    FString Stripped;
    FJsonSerializer::Serialize(Object.ToSharedRef(), TJsonWriterFactory<>::Create(&Stripped));
    TestFalse(TEXT("Stripped JSON omits PactSlots key"), Stripped.Contains(TEXT("\"PactSlots\"")));

    FExpeditionSaveData Loaded;
    TestTrue(TEXT("Decode injects"), FExpeditionSave::Decode(Stripped, Loaded, Error));
    TestEqual(TEXT("Still version 7"), Loaded.Version, 7);
    TestTrue(TEXT("Has combatants"), Loaded.Current.Battle.Encounter.Combatants.Num() > 0);
    for (const FCombatantState& Unit : Loaded.Current.Battle.Encounter.Combatants)
    {
        if (Unit.Side != ECombatSide::Party) { continue; }
        TestEqual(TEXT("Injected PactSlots 0"), Unit.PactSlots, 0);
        TestEqual(TEXT("Injected PactSlotLevel 0"), Unit.PactSlotLevel, 0);
    }
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSrdLevelUpAvailableFlagTest, "Project.Expedition.Rules.SrdLevelUpAvailableFlag",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSrdLevelUpAvailableFlagTest::RunTest(const FString& Parameters)
{
    FCombatEncounter Encounter;
    Encounter.Ruleset = ERuleset::SRD;
    Encounter.Width = 8;
    Encounter.Height = 8;
    Encounter.Combatants = { MakeHero(1, TEXT("Hero"), FIntPoint(1, 1)), MakeBandit(10, FIntPoint(4, 1)) };

    FCombatBattle Battle;
    FString Error;
    TestTrue(TEXT("Start"), Battle.Start(Encounter, 21, Error));
    Battle.State.Phase = EBattlePhase::Victory;

    // Below threshold: flag stays false.
    FPartyContinuity::AwardEncounterExperience(Battle);
    TestEqual(TEXT("Bandit XP 25"), Battle.Find(1)->ExperiencePoints, 25);
    TestFalse(TEXT("Flag clear under L2"), Battle.Find(1)->bLevelUpAvailable);

    // Reach L2 threshold via Award: hero at 250 + Thug 100 = 350.
    Battle.State.Encounter.Combatants[0].ExperiencePoints = 250;
    Battle.State.Encounter.Combatants[0].bLevelUpAvailable = false;
    Battle.State.Encounter.Combatants[1].Role = TEXT("Thug");
    FPartyContinuity::AwardEncounterExperience(Battle);
    TestEqual(TEXT("250+100 = 350"), Battle.Find(1)->ExperiencePoints, 350);
    TestTrue(TEXT("Flag set at 350 XP"), Battle.Find(1)->bLevelUpAvailable);

    TestTrue(TEXT("Apply L2"), FPartyContinuity::ApplyLevelUp(Battle, 1, EAdventurerRole::Fighter, 7, Error));
    TestEqual(TEXT("Now L2"), Battle.Find(1)->Level, 2);
    TestFalse(TEXT("Flag cleared after exactly one pending"), Battle.Find(1)->bLevelUpAvailable);

    // Multiple pending: flag remains after one ApplyLevelUp.
    Battle.State.Encounter.Combatants[0].ExperiencePoints = 6500;
    Battle.State.Encounter.Combatants[0].bLevelUpAvailable =
        FAdvancementRules::PendingLevelUps(Battle.Find(1)->Level, Battle.Find(1)->ExperiencePoints) > 0;
    TestTrue(TEXT("Flag set with multi pending"), Battle.Find(1)->bLevelUpAvailable);
    TestTrue(TEXT("Apply L3"), FPartyContinuity::ApplyLevelUp(Battle, 1, EAdventurerRole::Fighter, 5, Error));
    TestEqual(TEXT("Now L3"), Battle.Find(1)->Level, 3);
    TestTrue(TEXT("Flag still set with remaining pending"), Battle.Find(1)->bLevelUpAvailable);
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSrdApplyLevelUpBetweenBattlesTest, "Project.Expedition.Rules.SrdApplyLevelUpBetweenBattles",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSrdApplyLevelUpBetweenBattlesTest::RunTest(const FString& Parameters)
{
    FCombatEncounter Encounter;
    Encounter.Ruleset = ERuleset::SRD;
    Encounter.Width = 8;
    Encounter.Height = 8;
    FCombatantState Hero = MakeHero(1, TEXT("Hero"), FIntPoint(1, 1));
    Hero.ExperiencePoints = 300;
    Hero.bLevelUpAvailable = true;
    Encounter.Combatants = { Hero, MakeBandit(10, FIntPoint(4, 1)) };

    FCombatBattle Battle;
    FString Error;
    TestTrue(TEXT("Start"), Battle.Start(Encounter, 31, Error));
    Battle.State.Encounter.Combatants[0].ExperiencePoints = 300;
    Battle.State.Encounter.Combatants[0].bLevelUpAvailable = true;

    Battle.State.Phase = EBattlePhase::Fighting;
    TestFalse(TEXT("Reject during Fighting"), FPartyContinuity::ApplyLevelUp(Battle, 1, EAdventurerRole::Fighter, 6, Error));
    TestTrue(TEXT("Error names between battles"), Error.Contains(TEXT("between battles")));
    TestEqual(TEXT("Level unchanged in fight"), Battle.Find(1)->Level, 1);

    Battle.State.Phase = EBattlePhase::Victory;
    TestTrue(TEXT("Accept in Victory"), FPartyContinuity::ApplyLevelUp(Battle, 1, EAdventurerRole::Fighter, 6, Error));
    TestEqual(TEXT("Leveled in Victory"), Battle.Find(1)->Level, 2);

    // Defeat is also between battles (not Fighting).
    Battle.State.Encounter.Combatants[0].ExperiencePoints = 900;
    Battle.State.Encounter.Combatants[0].bLevelUpAvailable = true;
    Battle.State.Phase = EBattlePhase::Defeat;
    TestTrue(TEXT("Accept in Defeat"), FPartyContinuity::ApplyLevelUp(Battle, 1, EAdventurerRole::Fighter, 4, Error));
    TestEqual(TEXT("Leveled in Defeat"), Battle.Find(1)->Level, 3);
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSrdRewardSaveRoundtripTest, "Project.Expedition.Persistence.SrdRewardSaveRoundtrip",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSrdRewardSaveRoundtripTest::RunTest(const FString& Parameters)
{
    FString Error;
    FExpeditionRun Run;
    TestTrue(TEXT("SRD NewRun"), Run.NewRun(42, Error, TEXT("srd-reward-rt"), EFighterSubclass::Champion, ERuleset::SRD));
    TestTrue(TEXT("Choose"), Run.ChooseRoute(Run.Progress().Offers[0].Id, Error));
    TestTrue(TEXT("Finish"), ExpeditionTestSupport::FinishCombat(Run));
    TestEqual(TEXT("Reward"), static_cast<int32>(Run.Progress().Phase), static_cast<int32>(EExpeditionPhase::Reward));
    FExpeditionSaveData Data;
    Data.Current = Run.Snapshot();
    FString Json;
    TestTrue(FString::Printf(TEXT("Encode: %s"), *Error), FExpeditionSave::Encode(Data, Json, Error));
    FExpeditionSaveData Loaded;
    TestTrue(FString::Printf(TEXT("Decode: %s"), *Error), FExpeditionSave::Decode(Json, Loaded, Error));
    TestTrue(TEXT("Ruleset SRD"), Loaded.Current.Progress.Ruleset == ERuleset::SRD);
    TestEqual(TEXT("Phase Reward"), static_cast<int32>(Loaded.Current.Progress.Phase), static_cast<int32>(EExpeditionPhase::Reward));
    FExpeditionRun Restored;
    TestTrue(FString::Printf(TEXT("Restore: %s"), *Error), Restored.Restore(Loaded.Current, Error));
    return !HasAnyErrors();
}
#endif
