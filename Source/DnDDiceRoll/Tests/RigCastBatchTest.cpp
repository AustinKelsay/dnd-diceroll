#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Animation/AnimSequence.h"
#include "Engine/SkeletalMesh.h"
#include "Party/BuildCatalog.h"
#include "Presentation/DiceRollTheme.h"
#include "Presentation/UnitPose.h"

// The full rigged cast (#150): the class lookup that later batches fill by data, the shipped rigs of batches 0 to 3 (fighter, wizard,
// rogue, cleric, barbarian, monk, paladin, ranger, sorcerer, warlock, druid), and the clips their abilities play. Board-level convergence, heights and resume for these rigs are asserted in
// UnitPoseTest.cpp, whose figure checks cover every class the shipped theme rigs.

namespace
{
FDiceRollCharacter Definition(const TCHAR* MeshPath)
{
    FDiceRollCharacter Entry;
    Entry.Mesh = TSoftObjectPtr<USkeletalMesh>(FSoftObjectPath(MeshPath));
    return Entry;
}

const TCHAR* ThemePath = TEXT("/Game/DnDDiceRoll/Theme/DA_DiceRollTheme.DA_DiceRollTheme");
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRigLookupTest, "Project.Expedition.Presentation.RigLookupByClass",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRigLookupTest::RunTest(const FString& Parameters)
{
    constexpr EAdventurerRole Roles[] = {EAdventurerRole::None, EAdventurerRole::Fighter, EAdventurerRole::Rogue, EAdventurerRole::Cleric, EAdventurerRole::Wizard,
        EAdventurerRole::Barbarian, EAdventurerRole::Monk, EAdventurerRole::Paladin, EAdventurerRole::Ranger, EAdventurerRole::Sorcerer, EAdventurerRole::Warlock,
        EAdventurerRole::Druid, EAdventurerRole::Bard, EAdventurerRole::Artificer};

    // Keys are the enum names, so a later batch adds a class by data alone; None has none.
    TestEqual(TEXT("Rogue key"), UDiceRollTheme::ClassKey(EAdventurerRole::Rogue), FName(TEXT("Rogue")));
    TestEqual(TEXT("Cleric key"), UDiceRollTheme::ClassKey(EAdventurerRole::Cleric), FName(TEXT("Cleric")));
    TestTrue(TEXT("None has no key"), UDiceRollTheme::ClassKey(EAdventurerRole::None).IsNone());
    TSet<FName> Keys;
    for (const EAdventurerRole Role : Roles) { if (Role != EAdventurerRole::None) { Keys.Add(UDiceRollTheme::ClassKey(Role)); } }
    TestEqual(TEXT("Every class has its own key"), Keys.Num(), 13);

    UDiceRollTheme* Theme = NewObject<UDiceRollTheme>(GetTransientPackage());
    if (!TestNotNull(TEXT("A transient theme"), Theme)) { return false; }

    // An empty theme resolves nothing: every class falls back to its static mesh, then primitives.
    for (const EAdventurerRole Role : Roles) { TestNull(TEXT("An empty theme has no class rig"), Theme->RigForClass(Role)); }
    TestNull(TEXT("An empty theme has no raider rig"), Theme->RigForFoe(FName(TEXT("Raider"))));

    // Entries are looked up by class and never leak to another class.
    Theme->ClassRigs.Add(FName(TEXT("Rogue")), Definition(TEXT("/Game/DnDDiceRoll/Theme/Characters/SK_Hero_Rogue")));
    const FDiceRollCharacter* Rogue = Theme->RigForClass(EAdventurerRole::Rogue);
    if (TestNotNull(TEXT("The rogue entry resolves"), Rogue)) { TestTrue(TEXT("It is the rogue's own definition"), Rogue->Mesh.ToString().EndsWith(TEXT("SK_Hero_Rogue"))); }
    for (const EAdventurerRole Role : Roles) { if (Role != EAdventurerRole::Rogue) { TestNull(TEXT("Another class stays unrigged"), Theme->RigForClass(Role)); } }

    // An entry with no mesh is the same as no entry (the lookup does not hand the caller a definition it cannot draw).
    Theme->ClassRigs.Add(FName(TEXT("Cleric")), FDiceRollCharacter());
    TestNull(TEXT("A meshless entry falls back"), Theme->RigForClass(EAdventurerRole::Cleric));
    // A later batch fills a class by adding a map entry only.
    Theme->ClassRigs.Add(FName(TEXT("Barbarian")), Definition(TEXT("/Game/DnDDiceRoll/Theme/Characters/SK_Hero_Barbarian")));
    TestNotNull(TEXT("A data-only entry resolves"), Theme->RigForClass(EAdventurerRole::Barbarian));

    // A theme saved before #150 holds the pilot fields: they still resolve, and a map entry takes precedence over them.
    Theme->FighterRig = Definition(TEXT("/Game/DnDDiceRoll/Theme/Characters/SK_Hero_Fighter"));
    Theme->WizardRig = Definition(TEXT("/Game/DnDDiceRoll/Theme/Characters/SK_Hero_Wizard"));
    Theme->RaiderRig = Definition(TEXT("/Game/DnDDiceRoll/Theme/Characters/SK_Foe_Raider"));
    TestTrue(TEXT("The pilot fighter field resolves"), Theme->RigForClass(EAdventurerRole::Fighter) == &Theme->FighterRig);
    TestTrue(TEXT("The pilot wizard field resolves"), Theme->RigForClass(EAdventurerRole::Wizard) == &Theme->WizardRig);
    TestTrue(TEXT("The pilot raider field resolves"), Theme->RigForFoe(FName(TEXT("Raider"))) == &Theme->RaiderRig);
    TestNull(TEXT("The pilot raider is not another foe"), Theme->RigForFoe(FName(TEXT("Scout"))));
    Theme->ClassRigs.Add(FName(TEXT("Wizard")), Definition(TEXT("/Game/DnDDiceRoll/Theme/Characters/SK_Hero_Wizard_Alt")));
    const FDiceRollCharacter* Wizard = Theme->RigForClass(EAdventurerRole::Wizard);
    TestTrue(TEXT("The map entry wins over the pilot field"), Wizard && Wizard != &Theme->WizardRig && Wizard->Mesh.ToString().EndsWith(TEXT("_Alt")));
    Theme->FoeRigs.Add(FName(TEXT("Raider")), Definition(TEXT("/Game/DnDDiceRoll/Theme/Characters/SK_Foe_Raider_Alt")));
    const FDiceRollCharacter* Raider = Theme->RigForFoe(FName(TEXT("Raider")));
    TestTrue(TEXT("The foe map entry wins over the pilot field"), Raider && Raider != &Theme->RaiderRig);

    // The shipped theme: the eleven rigged classes resolve to their own rig (never a static or primitive fallback) with a
    // complete clip set and three LODs; the foe raider too. Run import_dark_fantasy_theme.py if this fails.
    const UDiceRollTheme* Shipped = TSoftObjectPtr<UDiceRollTheme>(FSoftObjectPath(ThemePath)).LoadSynchronous();
    if (!TestNotNull(TEXT("The shipped theme loads"), Shipped)) { return false; }
    const TPair<EAdventurerRole, const TCHAR*> Cast[] = {{EAdventurerRole::Fighter, TEXT("SK_Hero_Fighter")}, {EAdventurerRole::Rogue, TEXT("SK_Hero_Rogue")},
        {EAdventurerRole::Cleric, TEXT("SK_Hero_Cleric")}, {EAdventurerRole::Wizard, TEXT("SK_Hero_Wizard")}, {EAdventurerRole::Barbarian, TEXT("SK_Hero_Barbarian")},
        {EAdventurerRole::Monk, TEXT("SK_Hero_Monk")}, {EAdventurerRole::Paladin, TEXT("SK_Hero_Paladin")}, {EAdventurerRole::Ranger, TEXT("SK_Hero_Ranger")},
        {EAdventurerRole::Sorcerer, TEXT("SK_Hero_Sorcerer")}, {EAdventurerRole::Warlock, TEXT("SK_Hero_Warlock")}, {EAdventurerRole::Druid, TEXT("SK_Hero_Druid")}};
    TSet<const USkeleton*> Skeletons;
    for (const TPair<EAdventurerRole, const TCHAR*>& Entry : Cast)
    {
        const FString Label = Entry.Value;
        const FDiceRollCharacter* Rig = Shipped->RigForClass(Entry.Key);
        if (!TestNotNull(*(Label + TEXT(" has a shipped rig")), Rig)) { continue; }
        USkeletalMesh* Mesh = nullptr;
        TArray<UAnimSequence*> Clips;
        if (!TestTrue(*(Label + TEXT(" loads its mesh and all ten clips")), Rig->Load(Mesh, Clips)) || !Mesh) { continue; }
        TestTrue(*(Label + TEXT(" is its own mesh")), Mesh->GetName() == Label);
        TestEqual(*(Label + TEXT(" has ten clips")), Clips.Num(), ExpeditionPose::Count);
        TestEqual(*(Label + TEXT(" has three LODs")), Mesh->GetLODNum(), 3);
        TestEqual(*(Label + TEXT(" has one material slot")), Mesh->GetMaterials().Num(), 1);
        if (Mesh->GetMaterials().Num() == 1) { TestNotNull(*(Label + TEXT(" has its material")), Mesh->GetMaterials()[0].MaterialInterface.Get()); }
        Skeletons.Add(Mesh->GetSkeleton());
        for (const UAnimSequence* Clip : Clips) { TestTrue(*(Label + TEXT(" clip is on the shared skeleton")), Clip && Clip->GetSkeleton() == Mesh->GetSkeleton()); }
    }
    TestEqual(TEXT("The whole cast shares one skeleton"), Skeletons.Num(), 1);
    TestNotNull(TEXT("The shipped raider still resolves"), Shipped->RigForFoe(FName(TEXT("Raider"))));
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCastBatchClipsTest, "Project.Expedition.Presentation.CastBatchClips",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FCastBatchClipsTest::RunTest(const FString& Parameters)
{
    using ExpeditionPose::ForEffect;
    using ExpeditionPose::IsSpellAttack;
    const auto Basic = [](EAdventurerRole Role, bool bFar)
    { return ExpeditionPose::ForCommand(ECombatCommandType::Attack, bFar, IsSpellAttack(ECombatCommandType::Attack, Role, bFar)); };

    // Basic attacks: the cleric swings the mace up close and casts Sacred Flame at range; the rogue never casts.
    TestEqual(TEXT("cleric mace, adjacent"), Basic(EAdventurerRole::Cleric, false), EUnitPose::Melee);
    TestEqual(TEXT("cleric Sacred Flame, distant"), Basic(EAdventurerRole::Cleric, true), EUnitPose::Cast);
    TestEqual(TEXT("rogue dagger, adjacent"), Basic(EAdventurerRole::Rogue, false), EUnitPose::Melee);
    TestEqual(TEXT("rogue, distant"), Basic(EAdventurerRole::Rogue, true), EUnitPose::Ranged);
    TestFalse(TEXT("A rogue's attack is never a spell"), IsSpellAttack(ECombatCommandType::Attack, EAdventurerRole::Rogue, true) || IsSpellAttack(ECombatCommandType::Attack, EAdventurerRole::Rogue, false));

    int32 Clerics = 0, Rogues = 0, Weapons = 0;
    for (const FBuildDefinition& Build : FBuildCatalog::All())
    {
        const EAdventurerRole Role = Build.Profile.AdventurerRole;
        if (Role != EAdventurerRole::Cleric && Role != EAdventurerRole::Rogue) { continue; }
        (Role == EAdventurerRole::Cleric ? Clerics : Rogues) += 1;
        // The arcane trickster is the one rogue with magic; its kit abilities are spells like a caster's.
        const bool bMagic = Role == EAdventurerRole::Cleric || Build.Id == TEXT("rogue.arcane-trickster");
        for (const FAbilityFacts& Ability : Build.Abilities)
        {
            const FString Label = FString::Printf(TEXT("%s %s"), *Build.Id, *Ability.Name);
            if (Ability.bKitResource) { TestEqual(*(Label + TEXT(" is a spell exactly when the build casts")), Ability.bSpell, bMagic); }
            for (const bool bFar : {false, true})
            {
                const EUnitPose Pose = ForEffect(Ability.Effect, bFar, Ability.bSpell);
                if (Ability.bSpell)
                {
                    // A spell casts whatever its normalized effect, a swap included (#150 batch 2: the spell check comes first).
                    TestEqual(*(Label + TEXT(" casts")), Pose, EUnitPose::Cast);
                }
                else if (Ability.Effect == EAbilityEffect::Strike || Ability.Effect == EAbilityEffect::WeaponAttack || Ability.Effect == EAbilityEffect::Area || Ability.Effect == EAbilityEffect::Push)
                {
                    // A weapon ability swings or shoots; only a spell ever plays Cast for it.
                    TestEqual(*(Label + TEXT(" uses a weapon clip")), Pose, bFar ? EUnitPose::Ranged : EUnitPose::Melee);
                    ++Weapons;
                }
            }
        }
    }
    // The rogue's non-spell Protect and Healing specials play the Brace rally (#150 batch 2), not the arms-raised Cast gesture. Both abilities
    // must have been found, so this cannot pass vacuously.
    int32 Gestures = 0;
    for (const TCHAR* Id : {TEXT("rogue.thief"), TEXT("rogue.phantom")})
    {
        const FBuildDefinition* Build = FBuildCatalog::Find(Id);
        if (!TestNotNull(FString::Printf(TEXT("%s is in the catalog"), Id), Build)) { continue; }
        for (const FAbilityFacts& Ability : Build->Abilities)
        {
            const bool bFastHands = Build->Id == TEXT("rogue.thief") && Ability.Name == TEXT("Fast Hands");
            const bool bGhostShelter = Build->Id == TEXT("rogue.phantom") && Ability.Name == TEXT("Ghost Shelter");
            if (!bFastHands && !bGhostShelter) { continue; }
            TestFalse(FString::Printf(TEXT("%s is not a spell"), *Ability.Name), Ability.bSpell);
            TestEqual(FString::Printf(TEXT("%s is %s"), *Ability.Name, bFastHands ? TEXT("Healing") : TEXT("Protect")), Ability.Effect, bFastHands ? EAbilityEffect::Healing : EAbilityEffect::Protect);
            for (const bool bFar : {false, true})
            { TestEqual(FString::Printf(TEXT("%s plays Brace"), *Ability.Name), ForEffect(Ability.Effect, bFar, Ability.bSpell), EUnitPose::Brace); }
            ++Gestures;
        }
    }
    TestEqual(TEXT("Fast Hands and Ghost Shelter were both checked"), Gestures, 2);
    TestTrue(TEXT("The catalog has cleric builds"), Clerics > 0);
    TestTrue(TEXT("The catalog has rogue builds"), Rogues > 0);
    TestTrue(TEXT("Rogue weapon abilities were exercised"), Weapons > 0);
    return !HasAnyErrors();
}
#endif
