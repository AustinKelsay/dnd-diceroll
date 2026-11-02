#include "Party/CreatorScreen.h"
#include "Party/BuildCatalog.h"
#include "Run/ExpeditionRun.h"

void FCreatorScreen::Open(const FPartyBuildSelection& Selection, int32 Slot)
{
    SlotIndex = Slot; Row = RowName; SkillCursor = 0; Message.Reset(); bOpen = true;
    if (!Selection.Sheet.IsEmpty() && FCharacterCreator::FromSelection(Selection, Draft)) { return; }
    const FBuildDefinition* Build = FBuildCatalog::Find(Selection.BuildId);
    const EAdventurerRole Role = Build && FPointBuyRules::IsSrdClass(Build->Profile.AdventurerRole) ? Build->Profile.AdventurerRole : EAdventurerRole::Fighter;
    Draft = FCharacterCreator::NewDraft(Role);
    Draft.Name = Selection.Name.Left(FCharacterCreator::MaxNameLength);
}

ESkill FCreatorScreen::CursorSkill() const
{
    const TArray<ESkill> List = FCharacterCreator::SkillList(Draft.Role);
    return List.IsValidIndex(SkillCursor) ? List[SkillCursor] : ESkill::Athletics;
}

void FCreatorScreen::StepClass(int32 Direction)
{
    const TArray<EAdventurerRole>& Classes = FCharacterCreator::Classes();
    const int32 Index = FMath::Max(0, Classes.IndexOfByKey(Draft.Role));
    FCharacterCreator::SetClass(Draft, Classes[(Index + Direction + Classes.Num()) % Classes.Num()]);
    SkillCursor = 0;
}

void FCreatorScreen::CycleSkill()
{
    const ESkill Skill = CursorSkill();
    if (!Draft.Skills.Contains(Skill))
    {
        if (!FCharacterCreator::ToggleSkill(Draft, Skill)) { Message = FString::Printf(TEXT("%s picks only %d skills."), FCharacterCreator::ClassName(Draft.Role), FCharacterCreator::SkillPicks(Draft.Role)); }
        return;
    }
    if (!Draft.Expertise.Contains(Skill) && FCharacterCreator::ExpertisePicks(Draft.Role) > 0 && FCharacterCreator::ToggleExpertise(Draft, Skill)) { return; }
    FCharacterCreator::ToggleSkill(Draft, Skill);
}

void FCreatorScreen::Step(int32 Direction)
{
    Message.Reset();
    if (Row == RowClass) { StepClass(Direction); }
    else if (Row >= RowFirstAbility && Row < RowSkills)
    {
        const EAbility Ability = static_cast<EAbility>(Row - RowFirstAbility);
        const bool bDone = Direction > 0 ? FPointBuyRules::Raise(Draft.Scores, Ability) : FPointBuyRules::Lower(Draft.Scores, Ability);
        if (!bDone) { Message = Direction > 0 ? (Draft.Scores.Get(Ability) >= FPointBuyRules::MaxScore ? TEXT("15 is the point-buy maximum.") : TEXT("Not enough points left.")) : TEXT("8 is the point-buy minimum."); }
    }
    else if (Row == RowSkills)
    {
        const int32 Count = FCharacterCreator::SkillList(Draft.Role).Num();
        if (Count > 0) { SkillCursor = (SkillCursor + Direction + Count) % Count; }
    }
    else if (Row >= RowFirstColor && Row < RowConfirm) { FCharacterCreator::CycleColor(Draft, Row - RowFirstColor, Direction); }
    else if (Row == RowConfirm || Row == RowCancel) { Row = Row == RowConfirm ? RowCancel : RowConfirm; }
}

FCreatorScreen::EResult FCreatorScreen::Key(EKey Key)
{
    if (!bOpen) { return EResult::None; }
    switch (Key)
    {
    case EKey::Up: Row = (Row + RowCount - 1) % RowCount; break;
    case EKey::Down: Row = (Row + 1) % RowCount; break;
    case EKey::Left: Step(-1); break;
    case EKey::Right: Step(1); break;
    case EKey::Backspace: if (Row == RowName && !Draft.Name.IsEmpty()) { Draft.Name.LeftChopInline(1); } break;
    case EKey::Escape: return EResult::Cancel;
    case EKey::Enter:
        if (Row == RowConfirm) { return EResult::Confirm; }
        if (Row == RowCancel) { return EResult::Cancel; }
        if (Row == RowSkills) { CycleSkill(); }
        else if (Row == RowClass) { StepClass(1); }
        else if (Row >= RowFirstColor) { FCharacterCreator::CycleColor(Draft, Row - RowFirstColor, 1); }
        else { Row = Row == RowName ? RowClass : Row + 1; }
        break;
    }
    return EResult::None;
}

void FCreatorScreen::Type(TCHAR Character)
{
    if (!bOpen || Row != RowName || Draft.Name.Len() >= FCharacterCreator::MaxNameLength) { return; }
    const bool bAllowed = FChar::IsAlpha(Character) || ((Character == TEXT(' ') || Character == TEXT('-') || Character == TEXT('\'')) && !Draft.Name.IsEmpty());
    if (!bAllowed) { return; }
    const bool bWordStart = Draft.Name.IsEmpty() || Draft.Name.EndsWith(TEXT(" ")) || Draft.Name.EndsWith(TEXT("-"));
    Draft.Name.AppendChar(bWordStart ? FChar::ToUpper(Character) : FChar::ToLower(Character));
}

FCreatorScreen::EResult FCreatorScreen::Button(const FString& Id)
{
    if (!bOpen) { return EResult::None; }
    Message.Reset();
    if (Id == TEXT("CrConfirm")) { Row = RowConfirm; return EResult::Confirm; }
    if (Id == TEXT("CrCancel")) { Row = RowCancel; return EResult::Cancel; }
    if (Id == TEXT("CrName")) { Row = RowName; return EResult::None; }
    if (Id == TEXT("CrClassNext") || Id == TEXT("CrClassPrev")) { Row = RowClass; StepClass(Id == TEXT("CrClassNext") ? 1 : -1); return EResult::None; }
    if (Id.StartsWith(TEXT("CrClass")))
    {
        const int32 Index = FCString::Atoi(*Id.RightChop(7));
        if (FCharacterCreator::Classes().IsValidIndex(Index)) { Row = RowClass; FCharacterCreator::SetClass(Draft, FCharacterCreator::Classes()[Index]); SkillCursor = 0; }
        return EResult::None;
    }
    if (Id.StartsWith(TEXT("CrPlus")) || Id.StartsWith(TEXT("CrMinus")))
    {
        const bool bPlus = Id.StartsWith(TEXT("CrPlus"));
        const int32 Index = FCString::Atoi(*Id.RightChop(bPlus ? 6 : 7));
        if (Index >= 0 && Index < FAbilityRules::AbilityCount) { Row = RowFirstAbility + Index; Step(bPlus ? 1 : -1); }
        return EResult::None;
    }
    if (Id.StartsWith(TEXT("CrSkill")))
    {
        const int32 Index = FCString::Atoi(*Id.RightChop(7));
        if (FCharacterCreator::SkillList(Draft.Role).IsValidIndex(Index)) { Row = RowSkills; SkillCursor = Index; CycleSkill(); }
        return EResult::None;
    }
    if (Id.StartsWith(TEXT("CrColor")) && Id.Len() >= 9)
    {
        const int32 Slot = Id[7] - TEXT('0');
        if (Slot >= 0 && Slot < FCharacterCreator::ColorSlots) { Row = RowFirstColor + Slot; FCharacterCreator::CycleColor(Draft, Slot, Id[8] == TEXT('-') ? -1 : 1); }
    }
    return EResult::None;
}

bool FCreatorScreen::Confirm(FPartyBuildSelection& Slot, FString& Error)
{
    FPartyBuildSelection Out;
    if (!FCharacterCreator::ToSelection(Draft, Slot.Id, Out, Error)) { Message = Error; return false; }
    Slot = Out;
    Close();
    return true;
}

void FCreatorScreen::ClearSheet(FPartyBuildSelection& Slot) { Slot.Sheet = FHeroSheet(); }
