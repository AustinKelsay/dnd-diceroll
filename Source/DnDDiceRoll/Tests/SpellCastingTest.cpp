#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Combat/CombatBattle.h"
#include "Rules/SpellCatalogRules.h"
#include "Rules/SpellEffectRules.h"
#include "Party/BuildCatalog.h"
#include "Rules/SpellUtilityRules.h"
#include "Algo/Find.h"
#include "Rules/WeaponRules.h"

namespace SpellCastingTest
{
FCombatEncounter Caster(EAdventurerRole Role, int32 Level, FIntPoint FoeCell)
{
    FCombatEncounter Encounter;
    Encounter.Width = Encounter.Height = 12;
    FCombatantState Me;
    if (static_cast<uint8>(Role) >= static_cast<uint8>(EAdventurerRole::Monk))
    {
        // Newer classes start from their catalog build.
        for (const FBuildDefinition& Build : FBuildCatalog::All())
        {
            if (Build.Profile.AdventurerRole == Role && FBuildCatalog::Create(Build.Id, 10, TEXT("Caster"), Me)) { break; }
        }
    }
    Me.Id = 10; Me.Name = TEXT("Caster"); Me.Role = TEXT("Raider"); Me.AdventurerRole = Role; Me.Level = Level; Me.Cell = {0, 0}; Me.InitiativeBonus = 30;
    Me.MaxHP = Me.HP = 300; Me.SlotsByLevel.SetNum(FSpellRules::MaxSpellLevel);
    for (int32 L = 1; L <= FSpellRules::MaxSpellLevel; ++L) { Me.SlotsByLevel[L - 1] = FMath::Min(2, FBuildCatalog::SlotMax(Me, L)); }
    if (FBuildCatalog::Find(Me.BuildId)) { Me.SlotsByLevel[0] = FMath::Min(Me.SlotsByLevel[0], FBuildCatalog::SlotCap(Me)); }
    Me.SpellSlots = Me.SlotsByLevel[0];
    Me.KitUses = FMath::Min(Me.KitUses, Me.SpellSlots);
    Encounter.Combatants.Add(Me);
    FCombatantState Foe;
    Foe.Id = 20; Foe.Name = TEXT("Foe"); Foe.Role = TEXT("Raider"); Foe.Side = ECombatSide::Enemy; Foe.Cell = FoeCell; Foe.InitiativeBonus = -10; Foe.MaxHP = Foe.HP = 500;
    Encounter.Combatants.Add(Foe);
    return Encounter;
}
FCombatCommand Cast(const TCHAR* Id, int32 Target, int32 Slot = 0)
{
    FCombatCommand C{ECombatCommandType::CastSpell, 10, {}, Target}; C.SpellId = Id; C.SlotLevel = Slot; return C;
}
int32 Sum(const TArray<FDieRoll>& Dice) { int32 S = 0; for (const FDieRoll& D : Dice) { S += D.Value; } return S; }
}
using namespace SpellCastingTest;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSpellCastingTableTest, "Project.Expedition.Spells.EveryCombatSpellCasts",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSpellCastingTableTest::RunTest(const FString& Parameters)
{
    const EAdventurerRole Roles[] = {EAdventurerRole::Bard, EAdventurerRole::Cleric, EAdventurerRole::Druid, EAdventurerRole::Paladin,
        EAdventurerRole::Ranger, EAdventurerRole::Sorcerer, EAdventurerRole::Warlock, EAdventurerRole::Wizard};
    int32 ByLevel[10] = {};
    FString Names[10];
    for (int32 Index = 0; Index < FSpellCatalogRules::Num(); ++Index)
    {
        const FSrdSpell& Spell = FSpellCatalogRules::At(Index);
        if (!FSpellCatalogRules::CombatResolvable(Spell)) { continue; }
        const FSpellEffectSpec* Spec = FSpellEffectRules::Find(Spell.Id);
        bool bCastBySomeone = false;
        if (Spec && (Spec->Flags & SpellEffectFlag::Smite)) { bCastBySomeone = true; ++ByLevel[Spell.Level]; Names[Spell.Level] += FString(Spell.Name) + TEXT(", "); continue; } // smites: SpellEffectsTest
        // Situational: need a downed/dead/afflicted/enchanted creature first (SpellUtilityTest).
        static const TCHAR* const Situational[] = {TEXT("spare-the-dying"), TEXT("revivify"), TEXT("lesser-restoration"), TEXT("dispel-magic"), TEXT("remove-curse")};
        if (Algo::FindByPredicate(Situational, [&](const TCHAR* Id) { return FCString::Stricmp(Id, Spell.Id) == 0; })) { ++ByLevel[Spell.Level]; Names[Spell.Level] += FString(Spell.Name) + TEXT(", "); continue; }
        if (Spell.CastTime == ESpellCastTime::Reaction) { ++ByLevel[Spell.Level]; Names[Spell.Level] += FString(Spell.Name) + TEXT(", "); continue; } // reactions fire automatically: SpellReactionTest
        bCastBySomeone = false;
        for (const EAdventurerRole Role : Roles)
        {
            if (!FSpellCatalogRules::CanLearn(Role, 5, Spell)) { continue; }
            FString Error; FCombatBattle Battle;
            FCombatEncounter Encounter = Caster(Role, 5, {1, 0});
            if (Spec && (Spec->OnlyTypes & FCreatureRules::Bit(ECreatureType::Beast))) { Encounter.Combatants[1].Role = TEXT("Wolf"); }
            if (!Battle.Start(Encounter, 3, Error)) { continue; } // kit-only classes need a kit build to start
            TestTrue(FString::Printf(TEXT("%s listed castable"), Spell.Id), Battle.CastableSpells(10).Contains(Spell.Id));
            int32 Target = (Spec ? Spec->Target != ESpellEffectTarget::Foe : Spell.Effect == ESpellEffect::Healing) ? 10 : 20;
            const TArray<int32> Legal = Battle.SpellTargets(10, Spell.Id);
            if (!Legal.Contains(Target) && FSpellUtilityRules::Handled(Spell.Id) && !Legal.IsEmpty()) { Target = Legal[0]; } // self-centred utility spells
            TestTrue(FString::Printf(TEXT("%s has a target"), Spell.Id), Legal.Contains(Target));
            FCombatCommand Command = Cast(Spell.Id, Target); Command.Destination = {2, 2};
            const FCombatResult R = Battle.Submit(Command);
            TestTrue(FString::Printf(TEXT("%s resolves for a level 5 caster: %s"), Spell.Id, *R.Feedback), R.bAccepted);
            if (Spell.Level > 0 && R.bAccepted)
            {
                int32 Left = 0, Start = 0; for (int32 L = 0; L < FSpellRules::MaxSpellLevel; ++L) { Left += Battle.Find(10)->SlotsByLevel[L]; Start += Caster(Role, 5, {1, 0}).Combatants[0].SlotsByLevel[L]; }
                TestEqual(FString::Printf(TEXT("%s spends one slot"), Spell.Id), Left, Start - 1);
            }
            TestFalse(FString::Printf(TEXT("%s cannot be cast again with the same action"), Spell.Id), Battle.Submit(Cast(Spell.Id, Target)).bAccepted);
            bCastBySomeone = true;
            break;
        }
        if (Spell.Level <= 3) { TestTrue(FString::Printf(TEXT("%s is castable by some level 5 class"), Spell.Id), bCastBySomeone); }
        if (bCastBySomeone) { ++ByLevel[Spell.Level]; Names[Spell.Level] += FString(Spell.Name) + TEXT(", "); }
    }
    AddInfo(FString::Printf(TEXT("Combat-castable spells by level: 0=%d 1=%d 2=%d 3=%d 4=%d 5=%d 6=%d 7=%d 8=%d 9=%d"),
        ByLevel[0], ByLevel[1], ByLevel[2], ByLevel[3], ByLevel[4], ByLevel[5], ByLevel[6], ByLevel[7], ByLevel[8], ByLevel[9]));
    for (int32 L = 0; L <= 3; ++L) { AddInfo(FString::Printf(TEXT("Castable level %d: %s"), L, *Names[L])); }
    TestTrue(TEXT("Cantrips wired"), ByLevel[0] >= 8);
    // Class tags gate casting: a wizard cannot cast Sacred Flame, a fighter casts nothing, a level 1 wizard cannot cast Fireball.
    FString Error; FCombatBattle Battle;
    Battle.Start(Caster(EAdventurerRole::Wizard, 1, {1, 0}), 3, Error);
    TestFalse(TEXT("Sacred Flame is not on the wizard list"), Battle.Submit(Cast(TEXT("sacred-flame"), 20)).bAccepted);
    TestFalse(TEXT("Fireball needs wizard level 5"), Battle.Submit(Cast(TEXT("fireball"), 20)).bAccepted);
    TestFalse(TEXT("A fighter learns no Fire Bolt"), FSpellCatalogRules::CanLearn(EAdventurerRole::Fighter, 5, *FSpellCatalogRules::Find(TEXT("fire-bolt"))));
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSpellFireBoltTest, "Project.Expedition.Spells.FireBoltEndToEnd",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSpellFireBoltTest::RunTest(const FString& Parameters)
{
    bool bHit = false, bMiss = false, bCrit = false;
    for (int32 Seed = 1; Seed < 400 && !(bHit && bMiss && bCrit); ++Seed)
    {
        FString Error; FCombatBattle Battle;
        if (!Battle.Start(Caster(EAdventurerRole::Wizard, 5, {4, 0}), Seed, Error)) { AddError(Error); return false; }
        TestTrue(TEXT("Fire Bolt reaches 4 cells"), Battle.SpellTargets(10, TEXT("fire-bolt")).Contains(20));
        const FCombatResult R = Battle.Submit(Cast(TEXT("fire-bolt"), 20));
        if (!TestTrue(TEXT("Fire Bolt resolves"), R.bAccepted)) { return false; }
        TestEqual(TEXT("Fire"), R.DamageType, EDamageType::Fire);
        TestEqual(TEXT("Cantrip spends no slot"), Battle.Find(10)->SpellSlots, 2);
        TestFalse(TEXT("Costs the action"), Battle.Snapshot().bActionAvailable);
        TestEqual(TEXT("One attack roll"), R.AttackRolls.Num(), 1);
        if (R.bCritical) { bCrit = true; TestEqual(TEXT("Crit doubles 2d10 to 4d10"), R.EffectDice.Num(), 4); TestEqual(TEXT("Crit damage"), 500 - Battle.Find(20)->HP, Sum(R.EffectDice)); }
        else if (R.bHit) { bHit = true; TestEqual(TEXT("Level 5 tier: 2d10"), R.EffectDice.Num(), 2); TestEqual(TEXT("Hit damage"), 500 - Battle.Find(20)->HP, Sum(R.EffectDice)); }
        else { bMiss = true; TestEqual(TEXT("A miss deals nothing"), Battle.Find(20)->HP, 500); }
    }
    TestTrue(TEXT("Saw a hit"), bHit); TestTrue(TEXT("Saw a miss"), bMiss); TestTrue(TEXT("Saw a critical"), bCrit);
    FString Error; FCombatBattle Low;
    Low.Start(Caster(EAdventurerRole::Wizard, 1, {4, 0}), 5, Error);
    const FCombatResult R = Low.Submit(Cast(TEXT("fire-bolt"), 20));
    if (R.bHit && !R.bCritical) { TestEqual(TEXT("Level 1: 1d10"), R.EffectDice.Num(), 1); }
    FCombatBattle Far;
    Far.Start(Caster(EAdventurerRole::Wizard, 5, {11, 11}), 5, Error);
    TestTrue(TEXT("Within 120 ft"), Far.SpellTargets(10, TEXT("fire-bolt")).Contains(20));
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSpellFireballTest, "Project.Expedition.Spells.FireballAreaSaveUpcast",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSpellFireballTest::RunTest(const FString& Parameters)
{
    auto Setup = []()
    {
        FCombatEncounter E = Caster(EAdventurerRole::Wizard, 5, {6, 0});
        FCombatantState Ally = E.Combatants[0]; Ally.Id = 11; Ally.Name = TEXT("Ally"); Ally.Cell = {7, 1}; Ally.AdventurerRole = EAdventurerRole::Cleric; Ally.SpellSlots = 0; Ally.SlotsByLevel.Reset(); Ally.InitiativeBonus = 0;
        E.Combatants.Add(Ally);
        FCombatantState Far = E.Combatants[1]; Far.Id = 21; Far.Name = TEXT("Far"); Far.Cell = {6, 9};
        E.Combatants.Add(Far);
        return E;
    };
    bool bSaved = false, bFailed = false;
    for (int32 Seed = 1; Seed < 200 && !(bSaved && bFailed); ++Seed)
    {
        FString Error; FCombatBattle Battle;
        if (!Battle.Start(Setup(), Seed, Error)) { AddError(Error); return false; }
        const FCombatResult R = Battle.Submit(Cast(TEXT("fireball"), 20));
        if (!TestTrue(TEXT("Fireball resolves"), R.bAccepted)) { AddError(R.Feedback); return false; }
        TestTrue(TEXT("DEX save spell"), R.bSaveSpell);
        TestEqual(TEXT("8d6 at 3rd level"), R.EffectDice.Num(), 8);
        TestEqual(TEXT("3rd-level slot spent"), Battle.Find(10)->SlotsByLevel[2], 1);
        TestEqual(TEXT("Far foe outside 20 ft"), Battle.Find(21)->HP, 500);
        TestTrue(TEXT("Ally in the burst is burned"), Battle.Find(11)->HP < 300);
        TestEqual(TEXT("Caster is outside the burst"), Battle.Find(10)->HP, 300);
        const int32 Full = Sum(R.EffectDice), Lost = 500 - Battle.Find(20)->HP;
        if (Lost == Full) { bFailed = true; }
        else { TestEqual(TEXT("Half on a save, rounded down"), Lost, Full / 2); bSaved = true; }
    }
    TestTrue(TEXT("Saw a failed save"), bFailed); TestTrue(TEXT("Saw a successful save"), bSaved);
    // Upcast: +1d6 per slot above 3rd (SRD). Heroes cap at level 5 (3rd-level slots), so check the rule table for higher slots.
    const FSrdSpell* Fireball = FSpellCatalogRules::Find(TEXT("fireball"));
    for (int32 Slot = 3; Slot <= 9; ++Slot) { TestEqual(FString::Printf(TEXT("Fireball dice at slot %d"), Slot), FSpellCatalogRules::DiceCount(*Fireball, 5, Slot), 5 + Slot); }
    FString Error;
    FCombatBattle Low;
    Low.Start(Setup(), 9, Error);
    TestFalse(TEXT("Cannot cast at 2nd level"), Low.Submit(Cast(TEXT("fireball"), 20, 2)).bAccepted);
    FCombatEncounter Empty = Setup(); Empty.Combatants[0].SlotsByLevel[2] = 0;
    FCombatBattle None; None.Start(Empty, 9, Error);
    TestFalse(TEXT("No 3rd-level or higher slot"), None.Submit(Cast(TEXT("fireball"), 20)).bAccepted);
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSpellConcentrationTest, "Project.Expedition.Spells.ConcentrationEndsConditions",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSpellConcentrationTest::RunTest(const FString& Parameters)
{
    bool bHeld = false, bBroken = false, bReplaced = false;
    for (int32 Seed = 1; Seed < 300 && !(bBroken && bReplaced); ++Seed)
    {
        FCombatEncounter E = Caster(EAdventurerRole::Wizard, 5, {2, 0});
        E.Combatants[0].MaxHP = E.Combatants[0].HP = 200;
        FCombatantState Brute = E.Combatants[1]; Brute.Id = 21; Brute.Name = TEXT("Brute"); Brute.Cell = {0, 1}; Brute.InitiativeBonus = 0;
        E.Combatants.Add(Brute);
        FString Error; FCombatBattle Battle;
        if (!Battle.Start(E, Seed, Error)) { AddError(Error); return false; }
        const FCombatResult R = Battle.Submit(Cast(TEXT("hold-person"), 20));
        if (!TestTrue(TEXT("Hold Person resolves"), R.bAccepted)) { AddError(R.Feedback); return false; }
        auto Held = [&Battle]() { return Battle.Find(20)->Conditions.ContainsByPredicate([](const FCombatCondition& C) { return C.Condition == ECombatCondition::Paralyzed && C.SourceId == 10; }); };
        if (!R.bHit) { TestFalse(TEXT("No hold, no concentration"), Battle.Find(10)->bConcentrating); continue; }
        bHeld = true;
        TestTrue(TEXT("Concentrating"), Battle.Find(10)->bConcentrating);
        TestTrue(TEXT("Paralyzed"), Held());
        if (!bReplaced && (Seed % 2) == 0)
        {
            // Next turn: a new concentration spell ends the old one.
            Battle.Submit({ECombatCommandType::EndTurn, 10});
            for (int32 Guard = 0; Guard < 4 && Battle.ActiveId() != 10; ++Guard) { Battle.Submit({ECombatCommandType::EndTurn, Battle.ActiveId()}); }
            if (Battle.ActiveId() != 10 || !Held()) { continue; }
            const FCombatResult Again = Battle.Submit(Cast(TEXT("hold-person"), 21));
            if (Again.bAccepted) { TestFalse(TEXT("Recasting concentration ends the first hold"), Held()); bReplaced = true; }
            continue;
        }
        Battle.Submit({ECombatCommandType::EndTurn, 10});
        for (int32 Guard = 0; Guard < 4 && Battle.ActiveId() != 21; ++Guard) { Battle.Submit({ECombatCommandType::EndTurn, Battle.ActiveId()}); }
        if (Battle.ActiveId() != 21) { continue; }
        const int32 Before = Battle.Find(10)->HP;
        Battle.Submit({ECombatCommandType::Attack, 21, {}, 10});
        if (Battle.Find(10)->HP == Before) { continue; }
        if (!Battle.Find(10)->bConcentrating) { TestFalse(TEXT("Broken concentration frees the target"), Held()); bBroken = true; }
        else { TestTrue(TEXT("Kept concentration keeps the hold"), Held()); }
    }
    TestTrue(TEXT("Saw a hold"), bHeld); TestTrue(TEXT("Saw damage break concentration"), bBroken); TestTrue(TEXT("Saw a recast replace it"), bReplaced);
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSpellTwoDamageTypesTest, "Project.Expedition.Spells.TwoDamageTypes",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSpellTwoDamageTypesTest::RunTest(const FString& Parameters)
{
    const FSrdSpell* Storm = FSpellCatalogRules::Find(TEXT("ice-storm"));
    if (!TestNotNull(TEXT("Ice Storm"), Storm)) { return false; }
    TestEqual(TEXT("Bludgeoning first"), Storm->DamageType, EDamageType::Bludgeoning);
    TestEqual(TEXT("Cold second"), Storm->Damage2Type, EDamageType::Cold);
    TestEqual(TEXT("4d6 cold"), Storm->Dice2[4], 4);
    // Ice Knife (level 1): piercing on the attack plus its cold burst; upcast adds cold dice.
    bool bSeen = false, bUp = false;
    for (int32 Seed = 1; Seed < 200 && !(bSeen && bUp); ++Seed)
    {
        FString Error; FCombatBattle Battle;
        Battle.Start(Caster(EAdventurerRole::Wizard, 5, {4, 0}), Seed, Error);
        const int32 Slot = (Seed % 2) ? 1 : 2;
        const FCombatResult R = Battle.Submit(Cast(TEXT("ice-knife"), 20, Slot));
        if (!TestTrue(TEXT("Ice Knife resolves"), R.bAccepted)) { AddError(R.Feedback); return false; }
        // SRD 5.2.1: the cold burst always rolls (hit or miss); the target takes it only on a failed DEX save.
        if (R.bCritical) { continue; }
        const bool bPierce = R.Feedback.Contains(TEXT("piercing"));
        TestEqual(TEXT("1d10 piercing on a hit + 2d6 cold (+1d6 per slot)"), R.EffectDice.Num(), (bPierce ? 1 : 0) + (Slot == 1 ? 2 : 3));
        const bool bFailed = R.Feedback.Contains(TEXT("fails"));
        if (bPierce && bFailed) { (Slot == 1 ? bSeen : bUp) = true; TestEqual(TEXT("Both types dealt"), 500 - Battle.Find(20)->HP, Sum(R.EffectDice)); }
    }
    TestTrue(TEXT("Saw an Ice Knife hit"), bSeen); TestTrue(TEXT("Saw an upcast Ice Knife hit"), bUp);
    for (const TCHAR* Id : {TEXT("flame-strike"), TEXT("meteor-swarm"), TEXT("ice-knife")})
    {
        const FSrdSpell* S = FSpellCatalogRules::Find(Id);
        TestTrue(FString::Printf(TEXT("%s keeps a second type"), Id), S && S->Dice2Sides > 0);
    }
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSpellUtilityPhaseTest, "Project.Expedition.Spells.UtilityPhase",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSpellUtilityPhaseTest::RunTest(const FString& Parameters)
{
    auto WithAlly = [](EAdventurerRole Role, ECombatLife Life)
    {
        FCombatEncounter E = Caster(Role, 5, {4, 0}); E.bTwoRoundDowned = false;
        FCombatantState Ally = E.Combatants[0];
        Ally.Id = 11; Ally.Name = TEXT("Ally"); Ally.Cell = {0, 1}; Ally.InitiativeBonus = 0; Ally.Life = Life; Ally.HP = Life == ECombatLife::Conscious ? 30 : 0; Ally.RescueTurnsLeft = Life == ECombatLife::Downed ? 3 : 0;
        Ally.SlotsByLevel.Init(0, FSpellRules::MaxSpellLevel); Ally.SpellSlots = 0;
        E.Combatants.Add(Ally);
        return E;
    };
    {
        // Spare the Dying stabilizes a downed ally.
        FString Error; FCombatBattle Battle;
        if (TestTrue(TEXT("start downed"), Battle.Start(WithAlly(EAdventurerRole::Cleric, ECombatLife::Downed), 3, Error) || (AddError(Error), false)))
        {
            TestTrue(TEXT("downed ally targetable"), Battle.SpellTargets(10, TEXT("spare-the-dying")).Contains(11));
            const FCombatResult R = Battle.Submit(Cast(TEXT("spare-the-dying"), 11));
            TestTrue(FString::Printf(TEXT("Spare the Dying resolves: %s"), *R.Feedback), R.bAccepted);
            TestEqual(TEXT("stable"), Battle.Find(11)->DeathSaveSuccesses, 3);
        }
    }
    {
        // Revivify: a dead ally returns with 1 HP.
        FString Error; FCombatBattle Battle;
        if (TestTrue(TEXT("start dead"), Battle.Start(WithAlly(EAdventurerRole::Cleric, ECombatLife::Dead), 3, Error)))
        {
            const FCombatResult R = Battle.Submit(Cast(TEXT("revivify"), 11, 3));
            TestTrue(FString::Printf(TEXT("Revivify resolves: %s"), *R.Feedback), R.bAccepted);
            TestEqual(TEXT("1 HP"), Battle.Find(11)->HP, 1);
            TestEqual(TEXT("alive"), Battle.Find(11)->Life, ECombatLife::Conscious);
        }
    }
    {
        // Misty Step teleports to an open cell within 30 ft, not beyond.
        FString Error; FCombatBattle Battle;
        Battle.Start(Caster(EAdventurerRole::Wizard, 5, {4, 0}), 3, Error);
        FCombatCommand Far = Cast(TEXT("misty-step"), 10, 2); Far.Destination = {9, 9};
        TestFalse(TEXT("too far"), Battle.Submit(Far).bAccepted);
        FCombatCommand Near = Cast(TEXT("misty-step"), 10, 2); Near.Destination = {3, 3};
        const FCombatResult R = Battle.Submit(Near);
        TestTrue(FString::Printf(TEXT("Misty Step: %s"), *R.Feedback), R.bAccepted);
        TestEqual(TEXT("moved"), Battle.Find(10)->Cell, FIntPoint(3, 3));
    }
    {
        // Fog Cloud: a foe inside can't be targeted by a single-target spell.
        FString Error; FCombatBattle Battle;
        Battle.Start(Caster(EAdventurerRole::Wizard, 5, {6, 0}), 3, Error);
        TestTrue(TEXT("bolt before fog"), Battle.SpellTargets(10, TEXT("fire-bolt")).Contains(20));
        const FCombatResult R = Battle.Submit(Cast(TEXT("fog-cloud"), 20, 1));
        TestTrue(FString::Printf(TEXT("Fog Cloud: %s"), *R.Feedback), R.bAccepted);
        TestFalse(TEXT("no sight into fog"), Battle.SpellTargets(10, TEXT("fire-bolt")).Contains(20));
    }
    {
        // Illusions are sight-blocking images: Major Image over the foe hides it like a fog.
        FString Error; FCombatBattle Battle;
        Battle.Start(Caster(EAdventurerRole::Wizard, 5, {6, 0}), 3, Error);
        const FCombatResult R = Battle.Submit(Cast(TEXT("major-image"), 20, 3));
        TestTrue(FString::Printf(TEXT("Major Image: %s"), *R.Feedback), R.bAccepted);
        TestFalse(TEXT("no sight through the image"), Battle.SpellTargets(10, TEXT("fire-bolt")).Contains(20));
        TestTrue(TEXT("concentration on the image"), Battle.Find(10)->bConcentrating);
    }
    {
        // Remove Curse ends Hex on the creature.
        FString Error; FCombatBattle Battle;
        if (TestTrue(TEXT("start warlock"), Battle.Start(Caster(EAdventurerRole::Warlock, 5, {1, 0}), 3, Error) || (AddError(Error), false)))
        {
            const FCombatResult H = Battle.Submit(Cast(TEXT("hex"), 20));
            TestTrue(FString::Printf(TEXT("Hex: %s"), *H.Feedback), H.bAccepted);
            Battle.Submit(FCombatCommand{ECombatCommandType::EndTurn, 10}); Battle.Submit(FCombatCommand{ECombatCommandType::EndTurn, 20});
            TestTrue(TEXT("hexed foe is a Remove Curse target"), Battle.SpellTargets(10, TEXT("remove-curse")).Contains(20));
            const FCombatResult R = Battle.Submit(Cast(TEXT("remove-curse"), 20));
            TestTrue(FString::Printf(TEXT("Remove Curse: %s"), *R.Feedback), R.bAccepted);
            TestFalse(TEXT("Hex ended"), Battle.ActiveSpells().ContainsByPredicate([](const FActiveSpell& A) { return A.SpellId == TEXT("hex") && A.TargetId == 20; }));
        }
    }
    {
        // Blink: 1d6 at the end of the caster's turn; on 4-6 it is Ethereal (no foe can target it) until its next turn.
        bool bAway = false, bStayed = false;
        for (int32 Seed = 1; Seed <= 40 && !(bAway && bStayed); ++Seed)
        {
            FString Error; FCombatBattle Battle;
            Battle.Start(Caster(EAdventurerRole::Wizard, 5, {1, 0}), Seed, Error);
            if (!TestTrue(TEXT("Blink cast"), Battle.Submit(Cast(TEXT("blink"), 10, 3)).bAccepted)) { break; }
            Battle.Submit(FCombatCommand{ECombatCommandType::EndTurn, 10});
            const FActiveSpell* B = Battle.ActiveSpells().FindByPredicate([](const FActiveSpell& A) { return A.SpellId == TEXT("blink"); });
            if (!TestNotNull(TEXT("Blink lasts"), B)) { break; }
            const bool bTargetable = Battle.LegalTargets(20).Contains(10);
            if (B->Value == 1) { bAway = true; TestFalse(TEXT("Ethereal caster untargetable"), bTargetable); }
            else { bStayed = true; TestTrue(TEXT("present caster targetable"), bTargetable); }
        }
        TestTrue(TEXT("Blink vanishes on some rolls"), bAway);
        TestTrue(TEXT("Blink stays on others"), bStayed);
    }
    {
        // Silence: no verbal spells inside.
        FString Error; FCombatBattle Battle;
        Battle.Start(Caster(EAdventurerRole::Cleric, 5, {2, 0}), 3, Error);
        const FCombatResult R = Battle.Submit(Cast(TEXT("silence"), 10, 2));
        TestTrue(FString::Printf(TEXT("Silence: %s"), *R.Feedback), R.bAccepted);
        TestFalse(TEXT("sacred flame silenced"), Battle.CastableSpells(10).Contains(TEXT("sacred-flame")));
    }
    {
        // Chromatic Orb deals the chosen type; Magic Missile darts split between targets.
        bool bFire = false;
        for (int32 Seed = 1; Seed < 60 && !bFire; ++Seed)
        {
            FString Error; FCombatBattle Battle;
            Battle.Start(Caster(EAdventurerRole::Wizard, 5, {3, 0}), Seed, Error);
            FCombatCommand C = Cast(TEXT("chromatic-orb"), 20, 1); C.Choice = static_cast<int32>(EDamageType::Fire);
            const FCombatResult R = Battle.Submit(C);
            TestTrue(TEXT("orb resolves"), R.bAccepted);
            if (R.bHit) { bFire = true; TestEqual(TEXT("fire"), R.DamageType, EDamageType::Fire); TestTrue(TEXT("3d8+"), R.EffectDice.Num() >= 3); }
        }
        TestTrue(TEXT("saw an orb hit"), bFire);
        FCombatEncounter E = Caster(EAdventurerRole::Wizard, 5, {3, 0});
        FCombatantState Second = E.Combatants[1]; Second.Id = 21; Second.Name = TEXT("Foe2"); Second.Cell = {0, 3}; E.Combatants.Add(Second);
        FString Error; FCombatBattle Battle; TestTrue(FString::Printf(TEXT("two foes start: %s"), *Error), Battle.Start(E, 3, Error));
        TestTrue(TEXT("second foe legal"), Battle.SpellTargets(10, TEXT("magic-missile"), 1).Contains(21));
        FCombatCommand C = Cast(TEXT("magic-missile"), 20, 1); C.ExtraTargetIds = {21, 21};
        const FCombatResult R = Battle.Submit(C);
        TestTrue(FString::Printf(TEXT("split missiles: %s"), *R.Feedback), R.bAccepted);
        TestTrue(TEXT("first hurt"), Battle.Find(20)->HP < 500);
        TestTrue(FString::Printf(TEXT("second hurt: %s"), *R.Feedback), Battle.Find(21)->HP < 500);
    }
    {
        // Gust of Wind pushes a failed saver 15 ft along the line.
        bool bPushed = false;
        for (int32 Seed = 1; Seed < 60 && !bPushed; ++Seed)
        {
            FString Error; FCombatBattle Battle;
            Battle.Start(Caster(EAdventurerRole::Wizard, 5, {2, 0}), Seed, Error);
            const FCombatResult R = Battle.Submit(Cast(TEXT("gust-of-wind"), 20, 2));
            TestTrue(FString::Printf(TEXT("gust: %s"), *R.Feedback), R.bAccepted);
            if (Battle.Find(20)->Cell == FIntPoint(5, 0)) { bPushed = true; }
        }
        TestTrue(TEXT("saw a push"), bPushed);
    }
    {
        // Shillelagh: the druid's quarterstaff uses a d10 at level 5.
        FCombatEncounter E = Caster(EAdventurerRole::Druid, 5, {1, 0});
        E.Combatants[0].MainHand = static_cast<int32>(EWeapon::Quarterstaff);
        FString Error; FCombatBattle Battle; Battle.Start(E, 3, Error);
        const FCombatResult R = Battle.Submit(Cast(TEXT("shillelagh"), 10));
        TestTrue(FString::Printf(TEXT("shillelagh: %s"), *R.Feedback), R.bAccepted && R.Feedback.Contains(TEXT("empowered")));
    }
    return !HasAnyErrors();
}
#endif
