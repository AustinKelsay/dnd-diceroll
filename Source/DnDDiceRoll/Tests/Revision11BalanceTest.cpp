#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Combat/CombatBattle.h"
#include "ExpeditionTestSupport.h"
#include "Party/BuildCatalog.h"
#include "Persistence/ExpeditionSave.h"
#include "Presentation/ProgressionText.h"
#include "Run/ExpeditionRun.h"
#include "Run/ProgressionQuery.h"

/**
 * Catalog revision 11's balance pass (#241, docs/runs/journey-checkpoint/M2-balance-contract.md): enemies go for the nearest hero rather than
 * the first in roster order, Rescue restores a third of max HP, and the foes are scaled by stage. Compact runs from revision 11 only;
 * revisions 7-10 and SRD keep the older rules.
 */
namespace Revision11BalanceTests
{
FCombatantState Unit(int32 Id, ECombatSide Side, FIntPoint Cell, int32 HP, int32 Range = 1)
{
    FCombatantState U;
    U.Id = Id; U.Side = Side; U.Cell = Cell; U.MaxHP = U.HP = HP; U.Range = Range;
    U.Name = FString::Printf(TEXT("Unit %d"), Id); U.Role = Side == ECombatSide::Party ? TEXT("Fighter") : TEXT("Raider");
    U.Speed = 5; U.InitiativeBonus = -10;
    return U;
}

FCombatEncounter Open(bool bRevision11)
{
    FCombatEncounter E;
    E.Width = E.Height = 9;
    E.bNearestTarget = E.bSturdyRescue = bRevision11;
    return E;
}

/** The first combatant in the list acts first. */
bool StartWithFirstActing(FAutomationTestBase& Test, FCombatBattle& Battle, FCombatEncounter E)
{
    E.Combatants[0].InitiativeBonus = 30;
    FString Error;
    return Test.TestTrue(TEXT("The encounter starts: ") + Error, Battle.Start(E, 42, Error)) && Test.TestEqual(TEXT("The first unit acts"), Battle.ActiveId(), E.Combatants[0].Id);
}

/** The target of the enemy turn's attack, or INDEX_NONE when it did not attack. */
int32 Attacked(const TArray<FCombatResult>& Results)
{
    const FCombatResult* Attack = Results.FindByPredicate([](const FCombatResult& R) { return R.bAccepted && R.Type == ECombatCommandType::Attack; });
    return Attack ? Attack->TargetId : INDEX_NONE;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRevision11TargetChoiceTest, "Project.Expedition.Combat.Revision11EnemiesGoForTheNearestHero",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRevision11TargetChoiceTest::RunTest(const FString& Parameters)
{
    using namespace Revision11BalanceTests;
    for (const bool bRevision11 : {true, false})
    {
        const FString Rule = bRevision11 ? TEXT("Revision 11: ") : TEXT("Older rule: ");
        // Two heroes beside a raider: slot 1 (hero 10, 20 HP) and hero 11 (8 HP). Revision 11 hits the weaker; the older rule hits slot 1.
        {
            FCombatEncounter E = Open(bRevision11);
            E.Combatants = {Unit(20, ECombatSide::Enemy, {4, 4}, 12), Unit(10, ECombatSide::Party, {4, 3}, 20), Unit(11, ECombatSide::Party, {5, 4}, 8)};
            FCombatBattle Battle;
            if (!StartWithFirstActing(*this, Battle, E)) { return false; }
            TestEqual(Rule + TEXT("equally near, the lower HP is attacked"), Attacked(Battle.PlayEnemyTurn()), bRevision11 ? 11 : 10);
        }
        // An archer with both heroes in range: hero 10 (slot 1, 5 HP) three cells away, hero 11 (20 HP) two cells away. The nearest comes first.
        {
            FCombatEncounter E = Open(bRevision11);
            E.Combatants = {Unit(20, ECombatSide::Enemy, {0, 4}, 12, 4), Unit(10, ECombatSide::Party, {3, 4}, 5), Unit(11, ECombatSide::Party, {0, 6}, 20)};
            FCombatBattle Battle;
            if (!StartWithFirstActing(*this, Battle, E)) { return false; }
            TestEqual(Rule + TEXT("the nearest hero in reach comes before the weaker one"), Attacked(Battle.PlayEnemyTurn()), bRevision11 ? 11 : 10);
        }
        // Out of reach of both, equally far: the raider walks to the weaker hero (11, 5 HP) and hits it. The older rule takes the first firing
        // cell in cell order, beside slot 1.
        {
            FCombatEncounter E = Open(bRevision11);
            E.Combatants = {Unit(20, ECombatSide::Enemy, {4, 8}, 12), Unit(10, ECombatSide::Party, {2, 4}, 20), Unit(11, ECombatSide::Party, {6, 4}, 5)};
            FCombatBattle Battle;
            if (!StartWithFirstActing(*this, Battle, E)) { return false; }
            const FCombatSnapshot Before = Battle.Snapshot();
            const TArray<FCombatResult> Results = Battle.PlayEnemyTurn();
            TestEqual(Rule + TEXT("it attacks the hero it walked to"), Attacked(Results), bRevision11 ? 11 : 10);
            // Deterministic: the same position gives the same choice.
            FCombatBattle Again; FString Error;
            TestTrue(TEXT("The position restores"), Again.Restore(Before, Error));
            TestEqual(Rule + TEXT("the same position gives the same choice"), Attacked(Again.PlayEnemyTurn()), Attacked(Results));
        }
    }
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRevision11RescueTest, "Project.Expedition.Combat.Revision11RescueRestoresAThird",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRevision11RescueTest::RunTest(const FString& Parameters)
{
    using namespace Revision11BalanceTests;
    for (const bool bRevision11 : {true, false})
    {
        for (const int32 MaxHP : {31, 2})
        {
            const FString Rule = FString::Printf(TEXT("%s max HP %d: "), bRevision11 ? TEXT("Revision 11") : TEXT("Older rule"), MaxHP);
            FCombatEncounter E = Open(bRevision11);
            FCombatantState Downed = Unit(11, ECombatSide::Party, {2, 1}, MaxHP);
            Downed.HP = 0; Downed.Life = ECombatLife::Downed; Downed.RescueTurnsLeft = 2;
            E.Combatants = {Unit(10, ECombatSide::Party, {1, 1}, 20), Downed, Unit(20, ECombatSide::Enemy, {8, 8}, 12)};
            FCombatBattle Battle;
            if (!StartWithFirstActing(*this, Battle, E)) { return false; }
            const int32 Expected = bRevision11 ? FMath::Max(1, MaxHP / Revision11Rules::RescueShare) : 1;
            TestEqual(Rule + TEXT("the amount"), Battle.RescueAmount(*Battle.Find(11)), Expected);
            const FActionForecast Forecast = Battle.Forecast(10, ECombatCommandType::Rescue, 11);
            TestTrue(Rule + TEXT("the forecast is exact"), Forecast.bValid && Forecast.bHealing && Forecast.MinAmount == Expected && Forecast.MaxAmount == Expected);
            const FCombatResult Result = Battle.Submit({ECombatCommandType::Rescue, 10, {}, 11});
            TestTrue(Rule + TEXT("the rescue is accepted"), Result.bAccepted);
            TestEqual(Rule + TEXT("the committed healing equals the forecast"), Result.Healing, Expected);
            TestEqual(Rule + TEXT("the hero stands with that HP"), Battle.Find(11)->HP, Expected);
            TestEqual(Rule + TEXT("and is conscious"), Battle.Find(11)->Life, ECombatLife::Conscious);
            TestEqual(Rule + TEXT("the text says what it does"), Battle.Describe(ECombatCommandType::Rescue),
                bRevision11 ? FString(TEXT("Downed ally, 1 cell; restore a third of its max HP")) : FCombatBattle::DescribeAction(ECombatCommandType::Rescue));
        }
    }
    TestEqual(TEXT("The older text is unchanged"), FCombatBattle::DescribeAction(ECombatCommandType::Rescue), FString(TEXT("Downed ally, 1 cell; restore 1 HP")));
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRevision11CardsRefillTest, "Project.Expedition.Run.Revision11CardsRefillPools",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRevision11CardsRefillTest::RunTest(const FString& Parameters)
{
    // After fight 1 a survivor with a spent pool takes Reserve or Honed. Revision 10: Reserve adds its cap and one use (Kit 1/2 -> 2/3), Honed
    // changes no pool. Revision 11: Reserve refills each pool to its new cap (1/2 -> 3/3), Honed to the hero's caps (1/2 -> 2/2). The query's
    // after-values and the card's rule say exactly what the claim does, and the claimed run restores.
    for (const int32 Revision : {10, 11})
    {
        for (const ERewardCard Card : {ERewardCard::Reserve, ERewardCard::Honed})
        {
            const FString Name = Card == ERewardCard::Reserve ? TEXT("Reserve") : TEXT("Honed");
            bool bChecked = false;
            for (int32 Seed = 20261201; Seed < 20261241 && !bChecked; ++Seed)
            {
                FExpeditionRun Run; FString Error;
                if (!Run.NewRun(Seed, Error, TEXT("cards-refill"), EFighterSubclass::BattleMaster, ERuleset::Compact, Revision) ||
                    !Run.ChooseRoute(Run.Progress().Offers[0].Id, Error) || !ExpeditionTestSupport::FinishCombat(Run) || Run.Progress().Phase != EExpeditionPhase::Reward) { continue; }
                const FCombatantState* Pick = nullptr;
                for (const FCombatantState& U : Run.Battle().Snapshot().Encounter.Combatants)
                {
                    const FResourceCaps C = FBuildCatalog::Caps(U);
                    if (U.Side == ECombatSide::Party && U.Life == ECombatLife::Conscious && ((C.Kit > 0 && U.KitUses < C.Kit) || (C.Wind > 0 && U.SecondWindUses < C.Wind)))
                    { Pick = &U; break; }
                }
                if (!Pick) { continue; }
                bChecked = true;
                const FCombatantState Before = *Pick;
                const FProgressionFacts Facts = FProgressionQuery::For(Run);
                const FString Where = FString::Printf(TEXT("Revision %d, %s on %s: "), Revision, *Name, *Before.BuildId);
                TestEqual(Where + TEXT("the facts say whether the cards refill"), Facts.bCardsRefill, Revision >= 11);
                const FHeroProgressFacts* Hero = Facts.Heroes.FindByPredicate([&Before](const FHeroProgressFacts& H) { return H.Id == Before.Id; });
                const FRewardCardFacts* Offer = Hero ? Hero->Cards.FindByPredicate([Card](const FRewardCardFacts& C) { return C.Card == Card; }) : nullptr;
                if (!TestNotNull(Where + TEXT("the card is offered"), Offer)) { return false; }
                const FString Rule = Card == ERewardCard::Reserve
                    ? FString(Revision >= 11 ? TEXT("+1 cap on each special pool, all refilled") : TEXT("+1 cap and +1 use on each special pool"))
                    : FString(Revision >= 11 ? TEXT("Both specials grow stronger; pools refilled") : TEXT("Both specials grow stronger"));
                TestEqual(Where + TEXT("the card's rule"), ProgressionText::CardRule(Card, Facts.bCardsRefill), Rule);
                if (!TestTrue(Where + TEXT("the card is claimed: ") + Error, Run.ClaimReward(Before.Id, Card, Error))) { return false; }
                const FCombatantState& After = *Run.Battle().Find(Before.Id);
                const FResourceCaps Cap = FBuildCatalog::Caps(After);
                auto Expect = [Revision, Card](int32 Held, int32 NewCap)
                { return NewCap == 0 ? Held : Revision >= 11 ? NewCap : Card == ERewardCard::Reserve ? Held + 1 : Held; };
                TestEqual(Where + TEXT("Kit"), After.KitUses, Expect(Before.KitUses, Cap.Kit));
                TestEqual(Where + TEXT("Slot"), After.SpellSlots, Expect(Before.SpellSlots, Cap.Slot));
                TestEqual(Where + TEXT("Wind"), After.SecondWindUses, Expect(Before.SecondWindUses, Cap.Wind));
                TestEqual(Where + TEXT("Maneuver"), After.ManeuverUses, Expect(Before.ManeuverUses, Cap.Maneuver));
                TestEqual(Where + TEXT("Rage"), After.RageUses, Expect(Before.RageUses, Cap.Rage));
                TestTrue(Where + TEXT("within its caps"), FBuildCatalog::ValidResources(After));
                for (const FPoolChange& Pool : Offer->Pools)
                {
                    const int32 Held = Pool.Resource == EActionResource::Kit ? After.KitUses : Pool.Resource == EActionResource::Slot ? After.SpellSlots :
                        Pool.Resource == EActionResource::Wind ? After.SecondWindUses : Pool.Resource == EActionResource::Maneuver ? After.ManeuverUses : After.RageUses;
                    TestEqual(Where + TEXT("the card's after-value is the claim's"), Pool.HeldAfter, Held);
                }
                const bool bPoolLine = ProgressionText::CardLines(*Offer).ContainsByPredicate([](const FString& L) { return L.Contains(TEXT("→")) && (L.StartsWith(TEXT("Kit")) || L.StartsWith(TEXT("Wind"))); });
                if (Card == ERewardCard::Honed) { TestEqual(Where + TEXT("the Honed card lists the pools it refills"), bPoolLine, Revision >= 11); }
                FExpeditionRun Resumed;
                TestTrue(Where + TEXT("the claimed run restores: ") + Error, Resumed.Restore(Run.Snapshot(), Error));
            }
            TestTrue(FString::Printf(TEXT("Revision %d, %s: a seed reached a claim on a spent pool"), Revision, *Name), bChecked);
        }
    }
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRevision11ProfilesTest, "Project.Expedition.Run.Revision11MonkAndPaladinProfiles",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRevision11ProfilesTest::RunTest(const FString& Parameters)
{
    using namespace Revision11Rules;
    const TArray<FString> Builds = {TEXT("monk.open-hand"), TEXT("paladin.devotion"), TEXT("fighter.champion"), TEXT("monk.mercy")};
    TArray<FPartyBuildSelection> Party;
    for (int32 I = 0; I < Builds.Num(); ++I) { FPartyBuildSelection Slot; Slot.Id = I + 1; Slot.Name = FString::Printf(TEXT("Hero %d"), I + 1); Slot.BuildId = Builds[I]; Party.Add(Slot); }
    for (const int32 Revision : {10, 11})
    {
        FExpeditionRun Run; FString Error;
        if (!TestTrue(TEXT("A run starts: ") + Error, Run.NewRun(42, Party, Error, TEXT("profiles"), ERuleset::Compact, Revision))) { return false; }
        TestTrue(TEXT("It chooses a route"), Run.ChooseRoute(Run.Progress().Offers[0].Id, Error));
        for (int32 I = 0; I < Builds.Num(); ++I)
        {
            const FCombatantState& Hero = *Run.Battle().Find(I + 1);
            const FCombatantState& Profile = FBuildCatalog::Find(Builds[I])->Profile;
            const bool bMonk = Hero.AdventurerRole == EAdventurerRole::Monk, bPaladin = Hero.AdventurerRole == EAdventurerRole::Paladin;
            const int32 Hp = Revision < 11 ? 0 : bMonk ? MonkHpDelta : bPaladin ? PaladinHpDelta : 0;
            const int32 Armor = Revision < 11 ? 0 : bMonk ? MonkArmorDelta : bPaladin ? PaladinArmorDelta : 0;
            const FString Where = FString::Printf(TEXT("Revision %d, %s: "), Revision, *Builds[I]);
            TestEqual(Where + TEXT("max HP"), Hero.MaxHP, Profile.MaxHP + Hp);
            TestEqual(Where + TEXT("starts at full HP"), Hero.HP, Profile.MaxHP + Hp);
            TestEqual(Where + TEXT("armour"), Hero.ArmorClass, Profile.ArmorClass + Armor);
        }
        FExpeditionRun Resumed;
        TestTrue(FString::Printf(TEXT("Revision %d: the checkpoint restores: "), Revision) + Error, Resumed.Restore(Run.Snapshot(), Error));
        if (Revision == 11)
        {
            // A revision 11 Monk saved with the catalog's (revision 10) numbers does not belong to its run.
            FExpeditionSnapshot Forged = Run.Snapshot();
            FCombatantState& Monk = Forged.Battle.Encounter.Combatants[0];
            Monk.MaxHP -= MonkHpDelta; Monk.HP = FMath::Min(Monk.HP, Monk.MaxHP); Monk.ArmorClass -= MonkArmorDelta;
            TestFalse(TEXT("A revision 11 Monk with revision 10's numbers is refused"), Resumed.Restore(Forged, Error));
        }
    }
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRevision11CreateHeroTest, "Project.Expedition.Run.CreateHeroIsTheHeroNewRunCreates",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRevision11CreateHeroTest::RunTest(const FString& Parameters)
{
    // The builder previews FExpeditionRun::CreateHero (#241 review): for revision 11, revision 10 and SRD it is the hero NewRun starts with.
    const TArray<FString> Builds = {TEXT("monk.open-hand"), TEXT("paladin.devotion"), TEXT("fighter.champion"), TEXT("wizard.evocation")};
    TArray<FPartyBuildSelection> Party;
    for (int32 I = 0; I < Builds.Num(); ++I) { FPartyBuildSelection Slot; Slot.Id = I + 1; Slot.Name = FString::Printf(TEXT("Hero %d"), I + 1); Slot.BuildId = Builds[I]; Party.Add(Slot); }
    struct FCase { ERuleset Ruleset; int32 Revision; const TCHAR* Name; };
    for (const FCase& Case : {FCase{ERuleset::Compact, 11, TEXT("Compact 11")}, FCase{ERuleset::Compact, 10, TEXT("Compact 10")}, FCase{ERuleset::SRD, 11, TEXT("SRD 11")}})
    {
        FExpeditionRun Run; FString Error;
        if (!TestTrue(FString(Case.Name) + TEXT(": a run starts: ") + Error, Run.NewRun(42, Party, Error, TEXT("create-hero"), Case.Ruleset, Case.Revision))) { continue; }
        for (int32 I = 0; I < Builds.Num(); ++I)
        {
            FCombatantState Preview;
            const FString Where = FString::Printf(TEXT("%s, %s: "), Case.Name, *Builds[I]);
            if (!TestTrue(Where + TEXT("the preview is created"), FExpeditionRun::CreateHero(Builds[I], I + 1, Party[I].Name, Case.Ruleset, Case.Revision, Preview))) { continue; }
            const FCombatantState& Started = *Run.Battle().Find(I + 1);
            TestTrue(Where + TEXT("max HP, HP, armour and reach are the started hero's"), Preview.MaxHP == Started.MaxHP && Preview.HP == Started.HP &&
                Preview.ArmorClass == Started.ArmorClass && Preview.Range == Started.Range && Preview.AttackBonus == Started.AttackBonus &&
                Preview.DamageDie == Started.DamageDie && Preview.DamageBonus == Started.DamageBonus && Preview.Speed == Started.Speed);
            const FCombatantState& Catalog = FBuildCatalog::Find(Builds[I])->Profile;
            if (Case.Ruleset != ERuleset::Compact || Case.Revision < 11)
            { TestTrue(Where + TEXT("the catalog profile, unchanged"), Preview.MaxHP == Catalog.MaxHP && Preview.ArmorClass == Catalog.ArmorClass); }
        }
    }
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRevision11BalanceRevisionTest, "Project.Expedition.Run.Revision11BalanceFollowsTheRevision",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRevision11BalanceRevisionTest::RunTest(const FString& Parameters)
{
    for (int32 Revision = FBuildCatalog::MinimumRevision; Revision <= FBuildCatalog::Revision; ++Revision)
    {
        TestEqual(FString::Printf(TEXT("Compact revision %d"), Revision), FExpeditionRun::PlaysRevision11Balance(ERuleset::Compact, Revision), Revision >= 11);
        TestFalse(FString::Printf(TEXT("SRD revision %d keeps the older rules"), Revision), FExpeditionRun::PlaysRevision11Balance(ERuleset::SRD, Revision));
    }
    // Every route and stage, from the generator itself: revision 11's foes are revision 10's, scaled by that cell of the lever, and nothing else.
    int32 Cells = 0;
    for (int32 Stage = 0; Stage < FExpeditionRun::EncounterCount; ++Stage)
    {
        for (const bool bDangerous : {false, true})
        {
            FExpeditionRoute Route; Route.bDangerous = bDangerous; Route.Width = 16; Route.Height = 11;
            Route.Recipe = Stage == FExpeditionRun::EncounterCount - 1 ? 6 : Stage >= 2 ? 3 : 0;
            FCombatEncounter Old, New;
            const FString Where = FString::Printf(TEXT("Stage %d %s: "), Stage + 1, bDangerous ? TEXT("Dangerous") : TEXT("Guarded"));
            if (!TestTrue(Where + TEXT("both revisions generate it"), FExpeditionRun::EncounterLayout(Route, Stage, ERuleset::Compact, 10, Old) &&
                FExpeditionRun::EncounterLayout(Route, Stage, ERuleset::Compact, 11, New)) || !TestEqual(Where + TEXT("the same roster"), New.Combatants.Num(), Old.Combatants.Num())) { continue; }
            const int32 Kind = static_cast<int32>(FExpeditionRun::StageKind(Stage)), Way = bDangerous ? 1 : 0;
            for (int32 U = 4; U < Old.Combatants.Num(); ++U)
            {
                const FCombatantState& A = Old.Combatants[U];
                const FCombatantState& B = New.Combatants[U];
                TestEqual(Where + B.Role + TEXT(" HP"), B.MaxHP, A.MaxHP * Revision11Rules::EnemyHpPercent[Way][Kind] / 100);
                TestEqual(Where + B.Role + TEXT(" damage"), B.DamageBonus, A.DamageBonus + Revision11Rules::EnemyDamageDelta[Way][Kind]);
                TestTrue(Where + B.Role + TEXT(" keeps every other number"), A.Role == B.Role && A.Cell == B.Cell && A.ArmorClass == B.ArmorClass &&
                    A.AttackBonus == B.AttackBonus && A.DamageDie == B.DamageDie && A.Speed == B.Speed && A.Range == B.Range);
            }
            ++Cells;
        }
    }
    TestEqual(TEXT("Every route of every stage was compared"), Cells, 2 * FExpeditionRun::EncounterCount);
    // The same seed under revisions 10 and 11 offers the same routes (generation is revision 10's); only the foes' numbers and the rules differ.
    FExpeditionRun Runs[2]; FString Error;
    int32 Played = 0;
    for (int32 I = 0; I < 2; ++I)
    {
        TestTrue(TEXT("A run starts"), Runs[I].NewRun(42, Error, TEXT("balance-11"), EFighterSubclass::Champion, ERuleset::Compact, 10 + I));
        const FCombatEncounter& Opening = Runs[I].Battle().Snapshot().Encounter;
        TestTrue(FString::Printf(TEXT("Revision %d: the opening encounter's rules"), 10 + I), Opening.bNearestTarget == (I == 1) && Opening.bSturdyRescue == (I == 1));
    }
    for (int32 Fight = 0; Fight < FExpeditionRun::EncounterCount; ++Fight)
    {
        const FString Where = FString::Printf(TEXT("Fight %d: "), Fight + 1);
        const int32 Route = Fight % 2;
        if (!TestTrue(Where + TEXT("both runs choose a route"), Runs[0].ChooseRoute(Runs[0].Progress().Offers[Route].Id, Error) && Runs[1].ChooseRoute(Runs[1].Progress().Offers[Route].Id, Error)))
        { break; }
        ++Played;
        const FCombatEncounter& Old = Runs[0].Battle().Snapshot().Encounter;
        const FCombatEncounter& New = Runs[1].Battle().Snapshot().Encounter;
        TestTrue(Where + TEXT("each fight carries its run's rules"), !Old.bNearestTarget && !Old.bSturdyRescue && New.bNearestTarget && New.bSturdyRescue);
        TestTrue(Where + TEXT("the same board"), Old.Width == New.Width && Old.Height == New.Height && Old.BlockedCells == New.BlockedCells && Old.Combatants.Num() == New.Combatants.Num());
        const int32 Kind = static_cast<int32>(FExpeditionRun::StageKind(Fight)), Way = Route;
        for (int32 U = 4; U < Old.Combatants.Num() && U < New.Combatants.Num(); ++U)
        {
            const FCombatantState& A = Old.Combatants[U];
            const FCombatantState& B = New.Combatants[U];
            TestEqual(Where + B.Role + TEXT(" HP is revision 10's, scaled by stage"), B.MaxHP, A.MaxHP * Revision11Rules::EnemyHpPercent[Way][Kind] / 100);
            TestEqual(Where + B.Role + TEXT(" damage is revision 10's plus the stage's step"), B.DamageBonus, A.DamageBonus + Revision11Rules::EnemyDamageDelta[Way][Kind]);
            TestTrue(Where + B.Role + TEXT(" keeps every other number"), A.Role == B.Role && A.Cell == B.Cell && A.ArmorClass == B.ArmorClass && A.AttackBonus == B.AttackBonus &&
                A.DamageDie == B.DamageDie && A.Speed == B.Speed && A.Range == B.Range);
        }
        if (Fight == 0)
        {
            // The real save round trip has no such fields; Restore derives them from the run's revision, whatever the snapshot says.
            FExpeditionSaveData Data; Data.Current = Runs[1].Snapshot();
            FString Json; FExpeditionSaveData Loaded;
            TestTrue(TEXT("The checkpoint encodes"), FExpeditionSave::Encode(Data, Json, Error));
            TestFalse(TEXT("No balance field is saved"), Json.Contains(TEXT("NearestTarget")) || Json.Contains(TEXT("SturdyRescue")));
            TestTrue(TEXT("The checkpoint decodes"), FExpeditionSave::Decode(Json, Loaded, Error));
            FExpeditionRun Resumed;
            TestTrue(TEXT("It restores"), Resumed.Restore(Loaded.Current, Error));
            TestTrue(TEXT("Restore derives the rules from revision 11"), Resumed.Battle().Snapshot().Encounter.bNearestTarget && Resumed.Battle().Snapshot().Encounter.bSturdyRescue);
            FExpeditionSnapshot Forged = Runs[0].Snapshot(); Forged.Battle.Encounter.bNearestTarget = Forged.Battle.Encounter.bSturdyRescue = true;
            TestTrue(TEXT("A revision 10 snapshot whose flags disagree restores"), Resumed.Restore(Forged, Error));
            TestTrue(TEXT("and plays revision 10's rules"), !Resumed.Battle().Snapshot().Encounter.bNearestTarget && !Resumed.Battle().Snapshot().Encounter.bSturdyRescue);
        }
        bool bBoth = true;
        for (FExpeditionRun& Run : Runs)
        {
            bBoth &= ExpeditionTestSupport::FinishCombat(Run);
            if (Run.Progress().Phase == EExpeditionPhase::Reward && !Run.ClaimReward(1, Error) && !Run.ClaimReward(2, Error) && !Run.ClaimReward(3, Error)) { Run.ClaimReward(4, Error); }
            if (Run.Progress().Phase == EExpeditionPhase::Recovery) { Run.Recover(true, Error); }
            bBoth &= Run.Progress().Phase == EExpeditionPhase::Route;
        }
        if (!bBoth) { break; }
    }
    // The played runs must at least reach fight 2, past the save round trip of fight 1 (the stage table above covers every cell).
    TestTrue(FString::Printf(TEXT("Both played runs reached fight 2 (%d played)"), Played), Played >= 2);
    FExpeditionRun Srd;
    TestTrue(TEXT("An SRD run starts"), Srd.NewRun(42, Error, TEXT("balance-11-srd"), EFighterSubclass::Champion, ERuleset::SRD));
    TestTrue(TEXT("It chooses a route"), Srd.ChooseRoute(Srd.Progress().Offers[0].Id, Error));
    TestTrue(TEXT("SRD keeps the older rules at revision 11"), !Srd.Battle().Snapshot().Encounter.bNearestTarget && !Srd.Battle().Snapshot().Encounter.bSturdyRescue);
    return !HasAnyErrors();
}
#endif
