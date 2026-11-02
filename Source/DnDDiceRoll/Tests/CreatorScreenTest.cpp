#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Party/CreatorScreen.h"
#include "Run/ExpeditionRun.h"

namespace CreatorScreenTestLocal
{
FPartyBuildSelection CreatorScreenSlot(const TCHAR* Build, const TCHAR* Name)
{
    FPartyBuildSelection P; P.Id = 2; P.Name = Name; P.BuildId = Build; return P;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCreatorScreenKeysTest, "Project.Expedition.Creator.ScreenKeyboard",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FCreatorScreenKeysTest::RunTest(const FString& Parameters)
{
    using EKey = FCreatorScreen::EKey; using EResult = FCreatorScreen::EResult;
    FCreatorScreen C;
    TestTrue(TEXT("Closed screen ignores keys"), C.Key(EKey::Enter) == EResult::None);
    C.Open(CreatorScreenTestLocal::CreatorScreenSlot(TEXT("rogue.thief"), TEXT("Nix")), 1);
    TestTrue(TEXT("Open"), C.bOpen);
    TestTrue(TEXT("Starts from the slot's class"), C.Draft.Role == EAdventurerRole::Rogue);
    TestEqual(TEXT("Keeps the slot name"), C.Draft.Name, FString(TEXT("Nix")));
    TestEqual(TEXT("Recommended array spends all 27"), C.Remaining(), 0);
    // Typing only on the name row, with automatic word capitals.
    C.Key(EKey::Backspace); C.Key(EKey::Backspace); C.Key(EKey::Backspace);
    for (TCHAR Ch : FString(TEXT("vESSA tHORN"))) { C.Type(Ch); }
    TestEqual(TEXT("Typed name"), C.Draft.Name, FString(TEXT("Vessa Thorn")));
    C.Type(TEXT('7'));
    TestEqual(TEXT("Digits refused"), C.Draft.Name, FString(TEXT("Vessa Thorn")));
    C.Key(EKey::Down); C.Type(TEXT('x'));
    TestEqual(TEXT("No typing off the name row"), C.Draft.Name, FString(TEXT("Vessa Thorn")));
    // Point buy rows.
    C.Key(EKey::Down); // Strength
    TestEqual(TEXT("Row = STR"), C.Row, FCreatorScreen::RowFirstAbility);
    const int32 Str = C.Draft.Scores.Strength;
    C.Key(EKey::Left);
    TestEqual(TEXT("Left lowers"), C.Draft.Scores.Strength, Str - 1);
    TestTrue(TEXT("Refund shows"), C.Remaining() > 0);
    C.Key(EKey::Down); // DEX already 15
    C.Key(EKey::Right);
    TestEqual(TEXT("15 is the cap"), C.Draft.Scores.Dexterity, 15);
    TestFalse(TEXT("Refusal message"), C.Message.IsEmpty());
    C.Key(EKey::Up); C.Key(EKey::Right);
    TestEqual(TEXT("Raise back"), C.Draft.Scores.Strength, Str);
    // Wraps from the top to Cancel.
    C.Row = FCreatorScreen::RowName; C.Key(EKey::Up);
    TestEqual(TEXT("Up wraps to Cancel"), C.Row, FCreatorScreen::RowCancel);
    TestTrue(TEXT("Escape cancels"), C.Key(EKey::Escape) == EResult::Cancel);
    TestTrue(TEXT("Enter on Cancel cancels"), C.Key(EKey::Enter) == EResult::Cancel);
    C.Key(EKey::Left);
    TestEqual(TEXT("Left/Right swaps Confirm/Cancel"), C.Row, FCreatorScreen::RowConfirm);
    TestTrue(TEXT("Enter on Confirm confirms"), C.Key(EKey::Enter) == EResult::Confirm);
    // Class row resets the array and clears skills.
    C.Row = FCreatorScreen::RowClass; C.Key(EKey::Right);
    TestTrue(TEXT("Next class after Rogue is Wizard (SRD order)"), C.Draft.Role == FCharacterCreator::Classes()[(FCharacterCreator::Classes().IndexOfByKey(EAdventurerRole::Rogue) + 1) % 12]);
    C.Key(EKey::Left);
    TestTrue(TEXT("Back to Rogue"), C.Draft.Role == EAdventurerRole::Rogue);
    // Colour rows.
    C.Row = FCreatorScreen::RowFirstColor + 2; const int32 Accent = C.Draft.Colors[2];
    C.Key(EKey::Right);
    TestEqual(TEXT("Accent cycles"), C.Draft.Colors[2], (Accent + 1) % FCharacterCreator::PaletteSize);
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCreatorScreenSkillsTest, "Project.Expedition.Creator.ScreenSkillsAndConfirm",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FCreatorScreenSkillsTest::RunTest(const FString& Parameters)
{
    using EResult = FCreatorScreen::EResult;
    FCreatorScreen C;
    FPartyBuildSelection Slot = CreatorScreenTestLocal::CreatorScreenSlot(TEXT("rogue.thief"), TEXT("Nix"));
    C.Open(Slot, 1);
    FString Error;
    TestFalse(TEXT("No skills yet"), C.Ready(Error));
    FPartyBuildSelection Before = Slot;
    TestFalse(TEXT("Confirm refused while incomplete"), C.Confirm(Slot, Error));
    TestTrue(TEXT("Slot untouched"), Slot.Sheet.IsEmpty() && Slot.BuildId == Before.BuildId);
    TestTrue(TEXT("Still open after refusal"), C.bOpen);
    const TArray<ESkill> List = FCharacterCreator::SkillList(EAdventurerRole::Rogue);
    auto Click = [&](ESkill Skill) { return C.Button(FString::Printf(TEXT("CrSkill%d"), List.IndexOfByKey(Skill))); };
    Click(ESkill::Stealth);
    TestTrue(TEXT("First click: proficient"), C.Draft.Skills.Contains(ESkill::Stealth) && !C.Draft.Expertise.Contains(ESkill::Stealth));
    Click(ESkill::Stealth);
    TestTrue(TEXT("Second click: Expertise"), C.Draft.Expertise.Contains(ESkill::Stealth));
    Click(ESkill::Stealth);
    TestFalse(TEXT("Third click: cleared"), C.Draft.Skills.Contains(ESkill::Stealth) || C.Draft.Expertise.Contains(ESkill::Stealth));
    for (ESkill S : {ESkill::Stealth, ESkill::Perception, ESkill::Acrobatics, ESkill::Investigation}) { Click(S); }
    Click(ESkill::Deception);
    TestFalse(TEXT("Fifth skill refused"), C.Draft.Skills.Contains(ESkill::Deception));
    TestFalse(TEXT("with a reason"), C.Message.IsEmpty());
    Click(ESkill::Stealth); Click(ESkill::Perception);
    TestTrue(TEXT("Complete rogue is ready"), C.Ready(Error));
    const int32 R0 = C.Remaining();
    C.Button(TEXT("CrMinus1")); const int32 R1 = C.Remaining();
    C.Button(TEXT("CrPlus1")); const int32 R2 = C.Remaining();
    TestTrue(TEXT("Mouse minus refunds"), R1 > R0);
    TestTrue(TEXT("Mouse plus spends"), R2 < R1);
    C.Button(TEXT("CrColor1-"));
    TestEqual(TEXT("Mouse colour step"), C.Row, FCreatorScreen::RowFirstColor + 1);
    TestTrue(TEXT("Confirm button reports confirm"), C.Button(TEXT("CrConfirm")) == EResult::Confirm);
    TestTrue(TEXT("Confirm writes the slot"), C.Confirm(Slot, Error));
    TestFalse(TEXT("Closed after confirm"), C.bOpen);
    TestEqual(TEXT("Slot id kept"), Slot.Id, 2);
    TestTrue(TEXT("Slot holds a custom sheet"), Slot.Sheet.IsCustom());
    TestEqual(TEXT("Skills saved"), Slot.Sheet.ClassSkills.Num(), 4);
    // Reopening re-edits the custom hero.
    FCreatorScreen Again; Again.Open(Slot, 1);
    TestTrue(TEXT("Re-edit loads the sheet"), Again.Draft.Skills.Num() == 4 && Again.Draft.Expertise.Num() == 2);
    TestTrue(TEXT("Cancel button"), Again.Button(TEXT("CrCancel")) == EResult::Cancel);
    // A prebuilt class choice clears the sheet.
    FCreatorScreen::ClearSheet(Slot);
    TestTrue(TEXT("Prebuilt choice clears the custom sheet"), Slot.Sheet.IsEmpty());
    // Class picker by index and an unknown id are safe.
    C.Open(Slot, 1);
    C.Button(TEXT("CrClass0"));
    TestTrue(TEXT("Class by index"), C.Draft.Role == FCharacterCreator::Classes()[0]);
    TestTrue(TEXT("Unknown id ignored"), C.Button(TEXT("CrNope")) == EResult::None && C.Button(TEXT("CrClass99")) == EResult::None);
    return !HasAnyErrors();
}
#endif
