#pragma once

#include "CoreMinimal.h"
#include "Party/CharacterCreator.h"

struct FPartyBuildSelection;

/**
 * Overlay state for the character creator (LOCAL PROPOSAL, see CharacterCreator.h). Plain data with no UObject so
 * keyboard/mouse routing is unit-tested; the HUD only draws it and the controller only forwards input.
 * Rows top to bottom: Name, Class, the six abilities, Skills, three colour slots, Confirm, Cancel.
 */
struct DNDDICEROLL_API FCreatorScreen
{
    enum class EKey : uint8 { Up, Down, Left, Right, Enter, Backspace, Escape };
    enum class EResult : uint8 { None, Confirm, Cancel };
    static constexpr int32 RowName = 0;
    static constexpr int32 RowClass = 1;
    static constexpr int32 RowFirstAbility = 2;
    static constexpr int32 RowSkills = 8;
    static constexpr int32 RowFirstColor = 9;
    static constexpr int32 RowConfirm = 12;
    static constexpr int32 RowCancel = 13;
    static constexpr int32 RowCount = 14;

    bool bOpen = false;
    int32 SlotIndex = 0;
    int32 Row = RowName;
    int32 SkillCursor = 0;
    FCharacterDraft Draft;
    /** Last refusal shown under the panel (empty when fine). */
    FString Message;

    /** Opens on a party slot: re-edits a custom hero, else starts from the slot's class with its recommended array. */
    void Open(const FPartyBuildSelection& Selection, int32 Slot);
    void Close() { bOpen = false; Message.Reset(); }
    EResult Key(EKey Key);
    /** Typed character for the name row; ignored on other rows. */
    void Type(TCHAR Character);
    /** HUD button ids starting with "Cr". Mouse focus follows the clicked row. */
    EResult Button(const FString& Id);
    /** Validates and writes the hero into Slot (keeps its Id). Closes on success; Slot untouched on failure. */
    bool Confirm(FPartyBuildSelection& Slot, FString& Error);
    bool Ready(FString& Error) const { return FCharacterCreator::Validate(Draft, Error); }
    int32 Remaining() const { return FPointBuyRules::Remaining(Draft.Scores); }
    ESkill CursorSkill() const;
    /** Choosing a prebuilt class/preset drops any custom sheet from the slot. */
    static void ClearSheet(FPartyBuildSelection& Slot);
private:
    void Step(int32 Direction);
    void CycleSkill();
    void StepClass(int32 Direction);
};
