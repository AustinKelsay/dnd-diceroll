#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Party/CharacterCreator.h"
#include "Party/BuildCatalog.h"
#include "Run/ExpeditionRun.h"
#include "Persistence/ExpeditionSave.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace CreatorTestLocal
{
FCharacterDraft CreatorRogue()
{
    FCharacterDraft D = FCharacterCreator::NewDraft(EAdventurerRole::Rogue);
    D.Name = TEXT("Vessa Thorn");
    FCharacterCreator::ToggleSkill(D, ESkill::Stealth); FCharacterCreator::ToggleSkill(D, ESkill::Perception);
    FCharacterCreator::ToggleSkill(D, ESkill::Acrobatics); FCharacterCreator::ToggleSkill(D, ESkill::Investigation);
    FCharacterCreator::ToggleExpertise(D, ESkill::Stealth); FCharacterCreator::ToggleExpertise(D, ESkill::Perception);
    D.Colors[0] = 6; D.Colors[1] = 0; D.Colors[2] = 1;
    return D;
}
TArray<FPartyBuildSelection> CreatorParty(const FPartyBuildSelection& First)
{
    TArray<FPartyBuildSelection> Party; Party.Add(First);
    const TCHAR* Builds[] = {TEXT("fighter.champion"), TEXT("cleric.life"), TEXT("wizard.evocation")};
    for (int32 I = 0; I < 3; ++I) { FPartyBuildSelection P; P.Id = I + 2; P.Name = FString::Printf(TEXT("Hero %d"), I + 2); P.BuildId = Builds[I]; Party.Add(P); }
    return Party;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCreatorDraftTest, "Project.Expedition.Creator.DraftValidation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FCreatorDraftTest::RunTest(const FString& Parameters)
{
    TestEqual(TEXT("12 SRD classes offered"), FCharacterCreator::Classes().Num(), 12);
    TestFalse(TEXT("No Artificer"), FCharacterCreator::Classes().Contains(EAdventurerRole::Artificer));
    for (EAdventurerRole Role : FCharacterCreator::Classes())
    {
        const FBuildDefinition* Build = FBuildCatalog::Find(FCharacterCreator::BuildIdFor(Role));
        TestTrue(FString::Printf(TEXT("%s has a kit"), FCharacterCreator::ClassName(Role)), Build && Build->Profile.AdventurerRole == Role);
        TestFalse(TEXT("Feature summary"), FCharacterCreator::FeatureSummary(Role).IsEmpty());
        TestTrue(TEXT("Skill picks"), FCharacterCreator::SkillPicks(Role) >= 2);
    }
    FString Error;
    FCharacterDraft D = FCharacterCreator::NewDraft(EAdventurerRole::Rogue);
    TestFalse(TEXT("Unnamed draft rejected"), FCharacterCreator::Validate(D, Error));
    D = CreatorTestLocal::CreatorRogue();
    TestTrue(TEXT("Complete rogue validates"), FCharacterCreator::Validate(D, Error));
    TestFalse(TEXT("Fifth rogue skill refused"), FCharacterCreator::ToggleSkill(D, ESkill::Deception));
    TestFalse(TEXT("Off-list skill refused"), FCharacterCreator::ToggleSkill(D, ESkill::Arcana));
    TestFalse(TEXT("Third Expertise refused"), FCharacterCreator::ToggleExpertise(D, ESkill::Acrobatics));
    TestTrue(TEXT("Dropping a skill"), FCharacterCreator::ToggleSkill(D, ESkill::Stealth));
    TestFalse(TEXT("drops its Expertise"), D.Expertise.Contains(ESkill::Stealth));
    TestFalse(TEXT("Incomplete skills rejected"), FCharacterCreator::Validate(D, Error));
    D = CreatorTestLocal::CreatorRogue();
    D.Scores.Dexterity = 16;
    TestFalse(TEXT("Score above 15 rejected"), FCharacterCreator::Validate(D, Error));
    D = CreatorTestLocal::CreatorRogue(); D.Scores = FPointBuyRules::Baseline(); D.Scores.Strength = 15; D.Scores.Dexterity = 15; D.Scores.Constitution = 15; D.Scores.Wisdom = 9;
    TestFalse(TEXT("Overspent rejected"), FCharacterCreator::Validate(D, Error));
    D = CreatorTestLocal::CreatorRogue(); D.Colors[2] = 8;
    TestFalse(TEXT("Off-palette colour rejected"), FCharacterCreator::Validate(D, Error));
    D = CreatorTestLocal::CreatorRogue(); FCharacterCreator::SetClass(D, EAdventurerRole::Wizard);
    TestTrue(TEXT("Class change clears skills"), D.Skills.IsEmpty() && D.Expertise.IsEmpty());
    TestEqual(TEXT("Class change loads that class's array"), D.Scores.Intelligence, 15);
    TestEqual(TEXT("Wizard takes no Expertise at level 1"), FCharacterCreator::ExpertisePicks(EAdventurerRole::Wizard), 0);
    D.Colors[0] = 0; FCharacterCreator::CycleColor(D, 0, -1);
    TestEqual(TEXT("Colour wraps"), D.Colors[0], FCharacterCreator::PaletteSize - 1);
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCreatorHeroTest, "Project.Expedition.Creator.CustomHeroInRun",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FCreatorHeroTest::RunTest(const FString& Parameters)
{
    using namespace CreatorTestLocal;
    FString Error;
    FCharacterDraft D = CreatorRogue();
    D.Scores = FPointBuyRules::Baseline(); D.Scores.Dexterity = 15; D.Scores.Constitution = 15; D.Scores.Wisdom = 15; // 27 points
    FPartyBuildSelection Slot;
    if (!TestTrue(TEXT("Selection"), FCharacterCreator::ToSelection(D, 1, Slot, Error))) { AddError(Error); return false; }
    TestEqual(TEXT("Name trimmed and kept"), Slot.Name, FString(TEXT("Vessa Thorn")));
    FCombatantState Prebuilt; FBuildCatalog::Create(Slot.BuildId, 1, Slot.Name, Prebuilt);
    FExpeditionRun Run;
    if (!TestTrue(TEXT("Custom hero starts a run"), Run.NewRun(7, CreatorParty(Slot), Error))) { AddError(Error); return false; }
    const FAbilityScores Scores = Run.Battle().AbilityScores(1);
    TestEqual(TEXT("STR from point buy"), Scores.Strength, 8);
    TestEqual(TEXT("WIS from point buy"), Scores.Wisdom, 15);
    const FCombatantState* Hero = Run.Battle().Find(1);
    if (!TestNotNull(TEXT("Hero"), Hero)) { return false; }
    TestEqual(TEXT("HP = d8 + CON +2"), Hero->MaxHP, 10);
    const int32 DexShift = FAbilityRules::Modifier(15) - FAbilityRules::Modifier(FAbilityRules::PrebuiltScores(EAdventurerRole::Rogue).Dexterity);
    TestEqual(TEXT("Initiative follows DEX"), Hero->InitiativeBonus, Prebuilt.InitiativeBonus + DexShift);
    const FCharacterRules Sheet = Run.Battle().Character(1);
    TestTrue(TEXT("Picked Perception"), Sheet.IsProficient(ESkill::Perception));
    TestTrue(TEXT("Expertise on Perception"), Sheet.HasExpertise(ESkill::Perception));
    TestFalse(TEXT("No background skills"), Sheet.IsProficient(ESkill::Insight));
    TestTrue(TEXT("Rogue DEX save"), Sheet.IsSaveProficient(EAbility::Dexterity));
    TestEqual(TEXT("Perception = WIS +2 + 2x2"), Sheet.SkillBonus(ESkill::Perception), 6);
    // Save codec round trip keeps the sheet and validates it on restore.
    FExpeditionSaveData Data; Data.Current = Run.Snapshot(); FString Json;
    if (!TestTrue(TEXT("Encode"), FExpeditionSave::Encode(Data, Json, Error))) { AddError(Error); return false; }
    FExpeditionSaveData Back;
    if (!TestTrue(TEXT("Decode"), FExpeditionSave::Decode(Json, Back, Error))) { AddError(Error); return false; }
    TestTrue(TEXT("Sheet survives the save"), Back.Current.Progress.Party[0].Sheet.SameAs(Slot.Sheet));
    // Tampering with the sheet in the run selection is rejected by Restore.
    FExpeditionSnapshot Bad = Run.Snapshot(); Bad.Progress.Party[0].Sheet.BaseScores[0] = 15;
    FExpeditionRun Restored;
    TestFalse(TEXT("Tampered sheet rejected"), Restored.Restore(Bad, Error));
    FExpeditionSnapshot Mismatch = Run.Snapshot(); Mismatch.Progress.Party[0].Sheet.Colors[0] = 2;
    TestFalse(TEXT("Selection and unit sheets must match"), Restored.Restore(Mismatch, Error));
    TestTrue(TEXT("Untampered restores"), Restored.Restore(Run.Snapshot(), Error));
    // An older v5 save without Sheet fields still decodes (additive field).
    TSharedPtr<FJsonObject> Root; FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Root);
    TFunction<void(const TSharedPtr<FJsonObject>&)> Strip = [&Strip](const TSharedPtr<FJsonObject>& O)
    {
        O->RemoveField(TEXT("Sheet"));
        for (auto& Pair : O->Values)
        {
            if (Pair.Value->Type == EJson::Object) { Strip(Pair.Value->AsObject()); }
            else if (Pair.Value->Type == EJson::Array) { for (auto& V : Pair.Value->AsArray()) { if (V->Type == EJson::Object) { Strip(V->AsObject()); } } }
        }
    };
    FExpeditionSaveData Prebuilts; FExpeditionRun Plain; FPartyBuildSelection Rogue; Rogue.Id = 1; Rogue.Name = TEXT("Plain"); Rogue.BuildId = TEXT("rogue.thief");
    Plain.NewRun(7, CreatorParty(Rogue), Error); Prebuilts.Current = Plain.Snapshot();
    FString PlainJson; FExpeditionSave::Encode(Prebuilts, PlainJson, Error);
    FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(PlainJson), Root); Strip(Root);
    FString Legacy; FJsonSerializer::Serialize(Root.ToSharedRef(), TJsonWriterFactory<>::Create(&Legacy));
    TestTrue(TEXT("Legacy v5 save without Sheet decodes"), FExpeditionSave::Decode(Legacy, Back, Error));
    // Invalid sheet at NewRun.
    FPartyBuildSelection Broken = Slot; Broken.Sheet.BaseScores[1] = 16;
    FExpeditionRun Refused;
    TestFalse(TEXT("Illegal point buy refused at NewRun"), Refused.NewRun(7, CreatorParty(Broken), Error));
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCreatorRosterTest, "Project.Expedition.Creator.Roster",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FCreatorRosterTest::RunTest(const FString& Parameters)
{
    FString Error;
    TArray<FCharacterDraft> Roster = {CreatorTestLocal::CreatorRogue()};
    FCharacterDraft Monk = FCharacterCreator::NewDraft(EAdventurerRole::Monk); Monk.Name = TEXT("Ash");
    FCharacterCreator::ToggleSkill(Monk, ESkill::Acrobatics); FCharacterCreator::ToggleSkill(Monk, ESkill::Insight);
    Roster.Add(Monk);
    TArray<FCharacterDraft> Back;
    TestTrue(TEXT("Roster JSON round trip"), FCharacterCreator::FromJson(FCharacterCreator::ToJson(Roster), Back, Error));
    TestEqual(TEXT("Two heroes"), Back.Num(), 2);
    if (Back.Num() == 2)
    {
        TestEqual(TEXT("Name"), Back[1].Name, FString(TEXT("Ash")));
        TestTrue(TEXT("Class"), Back[1].Role == EAdventurerRole::Monk);
        TestEqual(TEXT("Scores"), Back[0].Scores.Dexterity, Roster[0].Scores.Dexterity);
        TestEqual(TEXT("Colours"), Back[0].Colors[0], 6);
    }
    FString Tampered = FCharacterCreator::ToJson(Roster).Replace(TEXT("\"Ash\""), TEXT("\"\""));
    TArray<FCharacterDraft> Untouched = Back;
    TestFalse(TEXT("Invalid entry rejects the file"), FCharacterCreator::FromJson(Tampered, Untouched, Error));
    TestEqual(TEXT("Output untouched"), Untouched.Num(), 2);
    const FString Path = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation/creator-roster-test.json"));
    TestTrue(TEXT("Save roster"), FCharacterCreator::SaveRoster(Roster, Path, Error));
    TArray<FCharacterDraft> Loaded;
    TestTrue(TEXT("Load roster"), FCharacterCreator::LoadRoster(Path, Loaded, Error));
    TestEqual(TEXT("Loaded two"), Loaded.Num(), 2);
    IFileManager::Get().Delete(*Path);
    FPartyBuildSelection Selection; FCharacterDraft Again;
    TestTrue(TEXT("Selection"), FCharacterCreator::ToSelection(Monk, 2, Selection, Error));
    TestTrue(TEXT("Re-edit from selection"), FCharacterCreator::FromSelection(Selection, Again) && Again.Name == TEXT("Ash") && Again.Skills.Num() == 2);
    return !HasAnyErrors();
}
#endif
