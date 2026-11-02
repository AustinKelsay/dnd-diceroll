#include "Party/CharacterCreator.h"
#include "Party/BuildCatalog.h"
#include "Party/FeatureKit.h"
#include "Run/ExpeditionRun.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace CharacterCreatorLocal
{
FProficiencyChoice LevelOneChoice(const FHeroSheet& Sheet)
{
    FProficiencyChoice Choice; Choice.bNoBackground = true;
    for (int32 S : Sheet.ClassSkills) { Choice.ClassSkills.Add(static_cast<ESkill>(S)); }
    for (int32 S : Sheet.ExpertiseSkills) { Choice.ExpertiseSkills.Add(static_cast<ESkill>(S)); }
    return Choice;
}
uint32 Bit(ESkill Skill) { return 1u << static_cast<uint32>(Skill); }
}

const TArray<EAdventurerRole>& FCharacterCreator::Classes()
{
    using R = EAdventurerRole;
    static const TArray<EAdventurerRole> Values = {R::Barbarian, R::Bard, R::Cleric, R::Druid, R::Fighter, R::Monk,
        R::Paladin, R::Ranger, R::Rogue, R::Sorcerer, R::Warlock, R::Wizard};
    return Values;
}

const TCHAR* FCharacterCreator::ClassName(EAdventurerRole Role)
{
    switch (Role)
    {
    case EAdventurerRole::Barbarian: return TEXT("Barbarian");
    case EAdventurerRole::Bard: return TEXT("Bard");
    case EAdventurerRole::Cleric: return TEXT("Cleric");
    case EAdventurerRole::Druid: return TEXT("Druid");
    case EAdventurerRole::Fighter: return TEXT("Fighter");
    case EAdventurerRole::Monk: return TEXT("Monk");
    case EAdventurerRole::Paladin: return TEXT("Paladin");
    case EAdventurerRole::Ranger: return TEXT("Ranger");
    case EAdventurerRole::Rogue: return TEXT("Rogue");
    case EAdventurerRole::Sorcerer: return TEXT("Sorcerer");
    case EAdventurerRole::Warlock: return TEXT("Warlock");
    case EAdventurerRole::Wizard: return TEXT("Wizard");
    default: return TEXT("Unknown");
    }
}

int32 FCharacterCreator::HitDie(EAdventurerRole Role)
{
    switch (Role)
    {
    case EAdventurerRole::Barbarian: return 12;
    case EAdventurerRole::Fighter: case EAdventurerRole::Paladin: case EAdventurerRole::Ranger: return 10;
    case EAdventurerRole::Sorcerer: case EAdventurerRole::Wizard: return 6;
    default: return 8;
    }
}

FString FCharacterCreator::FeatureSummary(EAdventurerRole Role)
{
    // SRD 5.2.1 level-1 class features (names only; the kit decides what is modelled in battle).
    switch (Role)
    {
    case EAdventurerRole::Barbarian: return TEXT("Rage, Unarmored Defense, Weapon Mastery. Key: STR.");
    case EAdventurerRole::Bard: return TEXT("Bardic Inspiration, Spellcasting. Key: CHA.");
    case EAdventurerRole::Cleric: return TEXT("Spellcasting, Divine Order. Key: WIS.");
    case EAdventurerRole::Druid: return TEXT("Spellcasting, Druidic, Primal Order. Key: WIS.");
    case EAdventurerRole::Fighter: return TEXT("Fighting Style, Second Wind, Weapon Mastery. Key: STR or DEX.");
    case EAdventurerRole::Monk: return TEXT("Martial Arts, Unarmored Defense. Key: DEX and WIS.");
    case EAdventurerRole::Paladin: return TEXT("Lay On Hands, Spellcasting, Weapon Mastery. Key: STR and CHA.");
    case EAdventurerRole::Ranger: return TEXT("Spellcasting, Favored Enemy, Weapon Mastery. Key: DEX and WIS.");
    case EAdventurerRole::Rogue: return TEXT("Expertise, Sneak Attack, Thieves' Cant, Weapon Mastery. Key: DEX.");
    case EAdventurerRole::Sorcerer: return TEXT("Spellcasting, Innate Sorcery. Key: CHA.");
    case EAdventurerRole::Warlock: return TEXT("Eldritch Invocations, Pact Magic. Key: CHA.");
    case EAdventurerRole::Wizard: return TEXT("Spellcasting, Ritual Adept, Arcane Recovery. Key: INT.");
    default: return FString();
    }
}

FLinearColor FCharacterCreator::PaletteColor(int32 Index)
{
    // Muted, dark high-fantasy dyes (sRGB-ish linear values tuned for the parchment/ink HUD).
    static const FLinearColor Colors[PaletteSize] = {
        FLinearColor(0.42f, 0.05f, 0.04f), FLinearColor(0.70f, 0.48f, 0.12f), FLinearColor(0.08f, 0.22f, 0.12f), FLinearColor(0.07f, 0.12f, 0.30f),
        FLinearColor(0.26f, 0.10f, 0.32f), FLinearColor(0.60f, 0.58f, 0.52f), FLinearColor(0.10f, 0.09f, 0.08f), FLinearColor(0.38f, 0.22f, 0.10f)};
    return Colors[FMath::Clamp(Index, 0, PaletteSize - 1)];
}

const TCHAR* FCharacterCreator::PaletteName(int32 Index)
{
    static const TCHAR* Names[PaletteSize] = {TEXT("Blood"), TEXT("Gilt"), TEXT("Moss"), TEXT("Woad"), TEXT("Nightshade"), TEXT("Bone"), TEXT("Soot"), TEXT("Rust")};
    return Names[FMath::Clamp(Index, 0, PaletteSize - 1)];
}

const TCHAR* FCharacterCreator::SlotName(int32 Slot)
{
    static const TCHAR* Names[ColorSlots] = {TEXT("Primary"), TEXT("Secondary"), TEXT("Accent")};
    return Names[FMath::Clamp(Slot, 0, ColorSlots - 1)];
}

FCharacterDraft FCharacterCreator::NewDraft(EAdventurerRole Role)
{
    FCharacterDraft Draft;
    SetClass(Draft, FPointBuyRules::IsSrdClass(Role) ? Role : EAdventurerRole::Fighter);
    return Draft;
}

void FCharacterCreator::SetClass(FCharacterDraft& Draft, EAdventurerRole Role)
{
    if (!FPointBuyRules::IsSrdClass(Role)) { return; }
    Draft.Role = Role;
    Draft.Scores = FPointBuyRules::Recommended(Role);
    Draft.Skills.Reset();
    Draft.Expertise.Reset();
}

int32 FCharacterCreator::SkillPicks(EAdventurerRole Role) { return FProficiencyRules::ClassProficiencies(Role, 1).SkillPicks; }
int32 FCharacterCreator::ExpertisePicks(EAdventurerRole Role) { return FProficiencyRules::ClassProficiencies(Role, 1).ExpertisePicks; }
TArray<ESkill> FCharacterCreator::SkillList(EAdventurerRole Role) { return FProficiencyRules::ClassProficiencies(Role, 1).SkillList; }

bool FCharacterCreator::ToggleSkill(FCharacterDraft& Draft, ESkill Skill)
{
    if (Draft.Skills.Contains(Skill)) { Draft.Skills.Remove(Skill); Draft.Expertise.Remove(Skill); return true; }
    if (!SkillList(Draft.Role).Contains(Skill) || Draft.Skills.Num() >= SkillPicks(Draft.Role)) { return false; }
    Draft.Skills.Add(Skill);
    return true;
}

bool FCharacterCreator::ToggleExpertise(FCharacterDraft& Draft, ESkill Skill)
{
    if (Draft.Expertise.Contains(Skill)) { Draft.Expertise.Remove(Skill); return true; }
    if (!Draft.Skills.Contains(Skill) || Draft.Expertise.Num() >= ExpertisePicks(Draft.Role)) { return false; }
    Draft.Expertise.Add(Skill);
    return true;
}

void FCharacterCreator::CycleColor(FCharacterDraft& Draft, int32 Slot, int32 Step)
{
    if (Slot < 0 || Slot >= ColorSlots) { return; }
    Draft.Colors[Slot] = ((Draft.Colors[Slot] + Step) % PaletteSize + PaletteSize) % PaletteSize;
}

FString FCharacterCreator::BuildIdFor(EAdventurerRole Role)
{
    if (!FPointBuyRules::IsSrdClass(Role)) { return FString(); }
    for (const auto& Build : FBuildCatalog::All()) { if (Build.Profile.AdventurerRole == Role) { return Build.Id; } }
    return FString();
}

bool FCharacterCreator::ValidateSheet(EAdventurerRole Role, const FHeroSheet& Sheet, FString& Error)
{
    if (!FPointBuyRules::IsSrdClass(Role)) { Error = TEXT("Custom heroes use one of the 12 SRD classes."); return false; }
    FAbilityScores Scores;
    if (!Sheet.Scores(Scores)) { Error = TEXT("A custom hero needs six base scores."); return false; }
    if (!FPointBuyRules::Validate(Scores, Error)) { return false; }
    if (Sheet.Colors.Num() != ColorSlots) { Error = TEXT("Choose three colours."); return false; }
    for (int32 C : Sheet.Colors) { if (C < 0 || C >= PaletteSize) { Error = TEXT("Colour is not on the palette."); return false; } }
    for (int32 S : Sheet.ClassSkills) { if (S < 0 || S >= FProficiencyRules::SkillCount) { Error = TEXT("Unknown skill."); return false; } }
    for (int32 S : Sheet.ExpertiseSkills) { if (S < 0 || S >= FProficiencyRules::SkillCount) { Error = TEXT("Unknown skill."); return false; } }
    FCharacterRules Ignored;
    return FProficiencyRules::BuildCharacter(Role, 1, Scores, CharacterCreatorLocal::LevelOneChoice(Sheet), Ignored, Error);
}

bool FCharacterCreator::Validate(const FCharacterDraft& Draft, FString& Error)
{
    const FString Name = Draft.Name.TrimStartAndEnd();
    if (Name.IsEmpty()) { Error = TEXT("Name your hero."); return false; }
    if (Name.Len() > MaxNameLength) { Error = TEXT("Names are at most 24 characters."); return false; }
    if (BuildIdFor(Draft.Role).IsEmpty()) { Error = TEXT("No kit is published for that class."); return false; }
    if (Draft.Skills.Num() != SkillPicks(Draft.Role)) { Error = FString::Printf(TEXT("Choose %d class skills."), SkillPicks(Draft.Role)); return false; }
    if (Draft.Expertise.Num() != ExpertisePicks(Draft.Role)) { Error = FString::Printf(TEXT("Choose %d Expertise skills."), ExpertisePicks(Draft.Role)); return false; }
    return ValidateSheet(Draft.Role, ToSheet(Draft), Error);
}

FHeroSheet FCharacterCreator::ToSheet(const FCharacterDraft& Draft)
{
    FHeroSheet Sheet;
    for (int32 I = 0; I < FAbilityRules::AbilityCount; ++I) { Sheet.BaseScores.Add(Draft.Scores.Get(static_cast<EAbility>(I))); }
    for (ESkill S : Draft.Skills) { Sheet.ClassSkills.Add(static_cast<int32>(S)); }
    for (ESkill S : Draft.Expertise) { Sheet.ExpertiseSkills.Add(static_cast<int32>(S)); }
    for (int32 C : Draft.Colors) { Sheet.Colors.Add(C); }
    return Sheet;
}

bool FCharacterCreator::ToSelection(const FCharacterDraft& Draft, int32 SlotId, FPartyBuildSelection& Out, FString& Error)
{
    if (!Validate(Draft, Error)) { return false; }
    FPartyBuildSelection Selection;
    Selection.Id = SlotId; Selection.Name = Draft.Name.TrimStartAndEnd(); Selection.BuildId = BuildIdFor(Draft.Role); Selection.Sheet = ToSheet(Draft);
    Out = Selection;
    return true;
}

bool FCharacterCreator::FromSelection(const FPartyBuildSelection& Selection, FCharacterDraft& Out)
{
    const FBuildDefinition* Build = FBuildCatalog::Find(Selection.BuildId);
    FString Error;
    if (!Build || !ValidateSheet(Build->Profile.AdventurerRole, Selection.Sheet, Error)) { return false; }
    FCharacterDraft Draft; Draft.Name = Selection.Name; Draft.Role = Build->Profile.AdventurerRole;
    Selection.Sheet.Scores(Draft.Scores);
    for (int32 S : Selection.Sheet.ClassSkills) { Draft.Skills.Add(static_cast<ESkill>(S)); }
    for (int32 S : Selection.Sheet.ExpertiseSkills) { Draft.Expertise.Add(static_cast<ESkill>(S)); }
    for (int32 I = 0; I < ColorSlots; ++I) { Draft.Colors[I] = Selection.Sheet.Colors[I]; }
    Out = Draft;
    return true;
}

bool FCharacterCreator::Apply(const FHeroSheet& Sheet, FCombatantState& Hero, FString& Error)
{
    if (!ValidateSheet(Hero.AdventurerRole, Sheet, Error)) { return false; }
    FAbilityScores New; Sheet.Scores(New);
    const FAbilityScores Old = FAbilityRules::PrebuiltScores(Hero.AdventurerRole);
    const EAbility Key = FAbilityRules::AttackAbility(Hero.AdventurerRole);
    const int32 Dex = FAbilityRules::Modifier(New.Dexterity) - FAbilityRules::Modifier(Old.Dexterity);
    const int32 KeyShift = FAbilityRules::Modifier(New.Get(Key)) - FAbilityRules::Modifier(Old.Get(Key));
    Hero.Sheet = Sheet;
    Hero.InitiativeBonus += Dex;
    Hero.AttackBonus += KeyShift;
    if (Hero.ArmorWorn < 0) { Hero.ArmorClass += Dex; }
    Hero.MaxHP = Hero.HP = FBuildCatalog::MaxHitPoints(Hero);
    FFeatureKit::Refill(Hero);
    Error.Reset();
    return true;
}

FCharacterRules FCharacterCreator::SheetCharacter(const FCombatantState& Hero)
{
    FCharacterRules Character;
    FAbilityScores Scores;
    if (!FBuildCatalog::HeroScores(Hero, Scores)) { Hero.Sheet.Scores(Scores); }
    FString Error;
    if (!FProficiencyRules::BuildCharacter(Hero.AdventurerRole, 1, Scores, CharacterCreatorLocal::LevelOneChoice(Hero.Sheet), Character, Error))
    {
        Character = FCharacterRules(); Character.Scores = Scores; return Character;
    }
    Character.Level = FMath::Clamp(Hero.Level, 1, 20);
    // Later Expertise (Rogue 6, Bard 2/9, Ranger 2/9, Wizard Scholar 2): fill in class-skill order, deterministic.
    const FClassProficiencies Class = FProficiencyRules::ClassProficiencies(Hero.AdventurerRole, Character.Level);
    int32 Have = FMath::CountBits(Character.ExpertiseMask);
    for (int32 S : Hero.Sheet.ClassSkills)
    {
        if (Have >= Class.ExpertisePicks) { break; }
        const ESkill Skill = static_cast<ESkill>(S);
        if (Character.HasExpertise(Skill) || (!Class.ExpertiseList.IsEmpty() && !Class.ExpertiseList.Contains(Skill))) { continue; }
        Character.ExpertiseMask |= CharacterCreatorLocal::Bit(Skill); ++Have;
    }
    return Character;
}

FString FCharacterCreator::RosterPath() { return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("SaveGames/Expedition/creator-roster-v1.json")); }

FString FCharacterCreator::ToJson(const TArray<FCharacterDraft>& Roster)
{
    TArray<TSharedPtr<FJsonValue>> Entries;
    for (const FCharacterDraft& D : Roster)
    {
        auto O = MakeShared<FJsonObject>();
        auto Ints = [](auto&& Values)
        {
            TArray<TSharedPtr<FJsonValue>> Out;
            for (const auto V : Values) { Out.Add(MakeShared<FJsonValueNumber>(static_cast<int32>(V))); }
            return Out;
        };
        O->SetStringField(TEXT("Name"), D.Name);
        O->SetNumberField(TEXT("Class"), static_cast<int32>(D.Role));
        TArray<int32> Scores; for (int32 I = 0; I < 6; ++I) { Scores.Add(D.Scores.Get(static_cast<EAbility>(I))); }
        O->SetArrayField(TEXT("Scores"), Ints(Scores));
        O->SetArrayField(TEXT("Skills"), Ints(D.Skills));
        O->SetArrayField(TEXT("Expertise"), Ints(D.Expertise));
        O->SetArrayField(TEXT("Colors"), Ints(TArray<int32>{D.Colors[0], D.Colors[1], D.Colors[2]}));
        Entries.Add(MakeShared<FJsonValueObject>(O));
    }
    auto Root = MakeShared<FJsonObject>();
    Root->SetNumberField(TEXT("Version"), 1);
    Root->SetArrayField(TEXT("Heroes"), Entries);
    FString Text;
    auto Writer = TJsonWriterFactory<>::Create(&Text);
    FJsonSerializer::Serialize(Root, Writer);
    return Text;
}

bool FCharacterCreator::FromJson(const FString& Json, TArray<FCharacterDraft>& Out, FString& Error)
{
    TSharedPtr<FJsonObject> Root;
    if (Json.Len() > 256 * 1024 || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Root) || !Root) { Error = TEXT("Roster file is not valid JSON."); return false; }
    int32 Version = 0; const TArray<TSharedPtr<FJsonValue>>* Heroes = nullptr;
    if (!Root->TryGetNumberField(TEXT("Version"), Version) || Version != 1 || !Root->TryGetArrayField(TEXT("Heroes"), Heroes) || Heroes->Num() > MaxRoster)
    { Error = TEXT("Roster file has an unsupported version or too many heroes."); return false; }
    auto ReadInts = [](const TSharedPtr<FJsonObject>& O, const TCHAR* Field, TArray<int32>& Values)
    {
        const TArray<TSharedPtr<FJsonValue>>* Array = nullptr;
        if (!O->TryGetArrayField(Field, Array) || Array->Num() > 18) { return false; }
        for (const auto& V : *Array)
        {
            double N = 0;
            if (!V->TryGetNumber(N) || N != FMath::FloorToDouble(N) || N < -1000 || N > 1000) { return false; }
            Values.Add(static_cast<int32>(N));
        }
        return true;
    };
    TArray<FCharacterDraft> Result;
    for (const auto& Entry : *Heroes)
    {
        const TSharedPtr<FJsonObject>* O = nullptr; int32 Class = 0; FCharacterDraft D;
        TArray<int32> Scores, Skills, Expertise, Colors;
        if (!Entry->TryGetObject(O) || !(*O)->TryGetStringField(TEXT("Name"), D.Name) || !(*O)->TryGetNumberField(TEXT("Class"), Class) ||
            !ReadInts(*O, TEXT("Scores"), Scores) || !ReadInts(*O, TEXT("Skills"), Skills) || !ReadInts(*O, TEXT("Expertise"), Expertise) || !ReadInts(*O, TEXT("Colors"), Colors) ||
            Scores.Num() != 6 || Colors.Num() != ColorSlots || Class < 0 || Class > 255)
        { Error = TEXT("Roster entry is malformed."); return false; }
        D.Role = static_cast<EAdventurerRole>(Class);
        for (int32 I = 0; I < 6; ++I) { FPointBuyRules::Set(D.Scores, static_cast<EAbility>(I), Scores[I]); }
        for (int32 S : Skills) { if (S < 0 || S >= FProficiencyRules::SkillCount) { Error = TEXT("Roster entry is malformed."); return false; } D.Skills.Add(static_cast<ESkill>(S)); }
        for (int32 S : Expertise) { if (S < 0 || S >= FProficiencyRules::SkillCount) { Error = TEXT("Roster entry is malformed."); return false; } D.Expertise.Add(static_cast<ESkill>(S)); }
        for (int32 I = 0; I < ColorSlots; ++I) { D.Colors[I] = Colors[I]; }
        if (!Validate(D, Error)) { return false; }
        Result.Add(D);
    }
    Out = MoveTemp(Result);
    Error.Reset();
    return true;
}

bool FCharacterCreator::SaveRoster(const TArray<FCharacterDraft>& Roster, const FString& Path, FString& Error)
{
    if (Roster.Num() > MaxRoster) { Error = TEXT("Roster is full."); return false; }
    for (const auto& D : Roster) { if (!Validate(D, Error)) { return false; } }
    if (!FFileHelper::SaveStringToFile(ToJson(Roster), *Path)) { Error = TEXT("Could not write the roster file."); return false; }
    Error.Reset();
    return true;
}

bool FCharacterCreator::LoadRoster(const FString& Path, TArray<FCharacterDraft>& Out, FString& Error)
{
    FString Text;
    if (!FFileHelper::LoadFileToString(Text, *Path)) { Error = TEXT("No roster file."); return false; }
    return FromJson(Text, Out, Error);
}
