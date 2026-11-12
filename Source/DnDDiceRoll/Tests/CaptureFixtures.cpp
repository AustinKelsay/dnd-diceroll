#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Combat/ActionFacts.h"
#include "Party/BuildCatalog.h"
#include "Persistence/ExpeditionSave.h"
#include "ExpeditionTestSupport.h"

/**
 * Developer tool, not a behavior test: writes real checkpoints at each expedition phase
 * under Saved/CaptureFixtures/<phase>/current-v1.json so visual QA can resume straight into
 * that screen with -ExpeditionSaveRoot=<that folder>. It also writes the gallery fixtures that
 * `tools/dev.py gallery` renders (default-seed-route/-battle/-enemy-turn, board-16x11, board-18x12, board-vigil-pillars,
 * board-vigil-walls, board-root-r002 and board-root-pillars (the Stream trail forest boards with the broken-wall and ruined-pillars
 * recipes, which show the Root of the World kit r002 once it is imported), board-vault and board-vault-pillars (the finale with the same two
 * recipes, which show the Vault kit r002 once it is imported), the action dock's kit-barbarian-battle, kit-wizard-battle,
 * kit-barbarian-raging, kit-wizard-all-spent, sight-blocked-battle, the hint fixtures end-turn-only-battle and downed-ally-battle, and the batch 2 cast
 * fixtures cast2-default-seed-battle, cast2-board-vigil-pillars, cast2-board-root-r002 and cast2-board-vault with a party of barbarian, monk, paladin and ranger, the same four with `cast3-` for a sorcerer, warlock, druid and wizard and with `cast4-` for a bard, artificer, fighter and wizard, and
 * board-vault-raiders, the dangerous Vault finale against rigged raiders, and the batch 4 palette sweep pal-NN-vigil|root|vault (every catalog build on the three
 * cast boards, four distinct classes to a party; palette-parties.json lists them) with dup-four-fighters-* and dup-two-pairs-*, the duplicate-accent parties),
 * each with a
 * fixture.json sidecar naming the seed and board that were found. Run it explicitly:
 *   tools/dev.py test --filter Project.Tools.CaptureFixtures
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FExpeditionCaptureFixtures, "Project.Tools.CaptureFixtures",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FExpeditionCaptureFixtures::RunTest(const FString& Parameters)
{
    const FString Root = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("CaptureFixtures"));
    auto Write = [&](const TCHAR* Name, const FExpeditionRun& Run)
    {
        FExpeditionSaveData Data;
        Data.Current = Run.Snapshot();
        const EExpeditionPhase Phase = Run.Progress().Phase;
        if (Phase == EExpeditionPhase::Victory || Phase == EExpeditionPhase::Defeat) { Data.Records.Add(FExpeditionSave::Ending(Run)); }
        FString Error;
        const bool bWritten = FExpeditionSave::Write(FPaths::Combine(Root, Name, TEXT("current-v1.json")), Data, Error);
        TestTrue(FString::Printf(TEXT("Fixture %s writes %s"), Name, *Error), bWritten);
    };
    auto Survivor = [](const FExpeditionRun& Run) -> int32
    {
        for (const FCombatantState& Unit : Run.Battle().Snapshot().Encounter.Combatants)
        { if (Unit.Side == ECombatSide::Party && Unit.Life == ECombatLife::Conscious) { return Unit.Id; } }
        return INDEX_NONE;
    };
    // Gallery fixtures: first encounters with known seeds and board sizes, party to act first.
    auto Describe = [&](const TCHAR* Name, const FExpeditionRun& Run, int32 Seed, int32 Offer)
    {
        const FCombatEncounter& Encounter = Run.Battle().Snapshot().Encounter;
        const int32 Width = Run.Progress().Phase == EExpeditionPhase::Combat ? Encounter.Width : 0;
        const int32 Height = Run.Progress().Phase == EExpeditionPhase::Combat ? Encounter.Height : 0;
        const FString Info = FString::Printf(TEXT("{\n  \"seed\": %d,\n  \"offer\": %d,\n  \"phase\": \"%s\",\n  \"boardWidth\": %d,\n  \"boardHeight\": %d\n}\n"),
            Seed, Offer, Run.Progress().Phase == EExpeditionPhase::Combat ? TEXT("combat") : TEXT("route"), Width, Height);
        TestTrue(FString::Printf(TEXT("Fixture %s writes its sidecar"), Name), FFileHelper::SaveStringToFile(Info, *FPaths::Combine(Root, Name, TEXT("fixture.json"))));
    };
    constexpr int32 DefaultSeed = 20261030;
    {
        FString Error;
        FExpeditionRun Run;
        Run.NewRun(DefaultSeed, Error, FString::Printf(TEXT("capture-%d"), DefaultSeed));
        Write(TEXT("default-seed-route"), Run);
        Describe(TEXT("default-seed-route"), Run, DefaultSeed, -1);
        TestTrue(TEXT("The default seed offers a route"), Run.ChooseRoute(Run.Progress().Offers[0].Id, Error));
        Write(TEXT("default-seed-battle"), Run);
        Describe(TEXT("default-seed-battle"), Run, DefaultSeed, 0);
        // Enemy turn: the party ends its turns until an enemy is active. Fixture preparation may play
        // legal commands; capture mode only resumes this checkpoint and holds the enemy's turn.
        for (int32 Step = 0; Step < 8 && Run.Progress().Phase == EExpeditionPhase::Combat; ++Step)
        {
            const FCombatantState* Active = Run.Battle().Find(Run.Battle().ActiveId());
            if (!Active || Active->Side != ECombatSide::Party) { break; }
            Run.Submit({ECombatCommandType::EndTurn, Active->Id});
        }
        const FCombatantState* Enemy = Run.Battle().Find(Run.Battle().ActiveId());
        if (TestTrue(TEXT("The default seed reaches an enemy turn"), Run.Progress().Phase == EExpeditionPhase::Combat && Enemy && Enemy->Side == ECombatSide::Enemy))
        {
            Write(TEXT("default-seed-enemy-turn"), Run);
            Describe(TEXT("default-seed-enemy-turn"), Run, DefaultSeed, 0);
        }
    }
    auto FindBoard = [&](const TCHAR* Name, int32 Width, int32 Height)
    {
        for (int32 Seed = 1; Seed < 2000; ++Seed)
        {
            for (int32 Offer = 0; Offer < 2; ++Offer)
            {
                FString Error;
                FExpeditionRun Run;
                Run.NewRun(Seed, Error, FString::Printf(TEXT("capture-%d"), Seed));
                const FExpeditionRoute& Route = Run.Progress().Offers[Offer];
                if (Route.Width != Width || Route.Height != Height || !Run.ChooseRoute(Route.Id, Error)) { continue; }
                const FCombatantState* Active = Run.Battle().Find(Run.Battle().ActiveId());
                if (!Active || Active->Side != ECombatSide::Party) { continue; }
                Write(Name, Run);
                Describe(Name, Run, Seed, Offer);
                return;
            }
        }
        AddError(FString::Printf(TEXT("No seed below 2000 gives a %dx%d board with the party first"), Width, Height));
    };
    FindBoard(TEXT("board-16x11"), 16, 11);
    FindBoard(TEXT("board-18x12"), 18, 12);
    // Drowned Vigil boards (the dangerous Watchtower road) with the architectural recipes: ruined pillars (1) and broken wall (2).
    // The wall recipe is the one whose adjacent blocked cells get the 2-cell arch, so these cover the pillar and wall variants and the arch.
    auto FindVigilRecipe = [&](const TCHAR* Name, int32 Recipe)
    {
        for (int32 Seed = 1; Seed < 2000; ++Seed)
        {
            FString Error;
            FExpeditionRun Run;
            Run.NewRun(Seed, Error, FString::Printf(TEXT("capture-%d"), Seed));
            const FExpeditionRoute& Route = Run.Progress().Offers[1];
            if (!Route.bDangerous || Route.Recipe != Recipe || !Run.ChooseRoute(Route.Id, Error)) { continue; }
            const FCombatantState* Active = Run.Battle().Find(Run.Battle().ActiveId());
            if (!Active || Active->Side != ECombatSide::Party) { continue; }
            Write(Name, Run);
            Describe(Name, Run, Seed, 1);
            return;
        }
        AddError(FString::Printf(TEXT("No seed below 2000 gives a dangerous route with recipe %d and the party first"), Recipe));
    };
    FindVigilRecipe(TEXT("board-vigil-pillars"), 1);
    FindVigilRecipe(TEXT("board-vigil-walls"), 2);
    // Root of the World boards (the Stream trail, route offer 0) with the same architectural recipes (#149): the broken wall is the
    // one whose adjacent blocked cells get the Root's 2-cell arch (`board-root-r002`, in the default gallery set); the ruined pillars
    // show the spire and stump variants (`board-root-pillars`, render it with `gallery --fixture board-root-pillars`).
    auto FindRootRecipe = [&](const TCHAR* Name, int32 Recipe)
    {
        for (int32 Seed = 1; Seed < 2000; ++Seed)
        {
            FString Error;
            FExpeditionRun Run;
            Run.NewRun(Seed, Error, FString::Printf(TEXT("capture-%d"), Seed));
            const FExpeditionRoute& Route = Run.Progress().Offers[0];
            if (Route.bDangerous || Route.Recipe != Recipe || !Run.ChooseRoute(Route.Id, Error)) { continue; }
            const FCombatantState* Active = Run.Battle().Find(Run.Battle().ActiveId());
            if (!Active || Active->Side != ECombatSide::Party) { continue; }
            Write(Name, Run);
            Describe(Name, Run, Seed, 0);
            return;
        }
        AddError(FString::Printf(TEXT("No seed below 2000 gives a forest route with recipe %d and the party first"), Recipe));
    };
    FindRootRecipe(TEXT("board-root-r002"), 2);
    FindRootRecipe(TEXT("board-root-pillars"), 1);
    // Rigged cast batches 2 and 3 (#150): the same three boards the cast is judged on (the default seed, Vigil pillars, Root recipe 2) and the Vault finale with a
    // party of the batch's classes, so those classes can be captured and measured on them: `cast2-default-seed-battle`, `cast2-board-vigil-pillars`,
    // `cast2-board-root-r002`, `cast2-board-vault` (barbarian, monk, paladin, ranger) and the same four names with `cast3-` (sorcerer, warlock, druid and a wizard,
    // whose Fire Bolt is the spell bolt the release is measured on). Render with `gallery --fixture <name>`; the pose sheets are cast2-poses.json and cast3-poses.json.
    const auto WriteCastFixtures = [&](const TCHAR* Prefix, const TArray<const TCHAR*>& Builds, const TCHAR* Label)
    {
        TArray<FPartyBuildSelection> Cast;
        for (int32 Index = 0; Index < Builds.Num(); ++Index)
        {
            FPartyBuildSelection Slot;
            Slot.Id = Index + 1; Slot.Name = FString::Printf(TEXT("Hero %d"), Index + 1); Slot.BuildId = Builds[Index];
            Cast.Add(Slot);
        }
        const FString Group = Prefix;
        {
            FString Error;
            FExpeditionRun Run;
            if (TestTrue(*FString::Printf(TEXT("The default seed starts with the %s party"), Label), Run.NewRun(DefaultSeed, Cast, Error, FString::Printf(TEXT("capture-%s-%d"), Prefix, DefaultSeed)) &&
                Run.ChooseRoute(Run.Progress().Offers[0].Id, Error)))
            {
                Write(*(Group + TEXT("-default-seed-battle")), Run);
                Describe(*(Group + TEXT("-default-seed-battle")), Run, DefaultSeed, 0);
            }
        }
        const auto FindCast = [&](const FString& Name, int32 Offer, bool bDangerous, int32 Recipe)
        {
            for (int32 Seed = 1; Seed < 2000; ++Seed)
            {
                FString Error;
                FExpeditionRun Run;
                if (!Run.NewRun(Seed, Cast, Error, FString::Printf(TEXT("capture-%s-%d"), Prefix, Seed))) { continue; }
                const FExpeditionRoute& Route = Run.Progress().Offers[Offer];
                if (Route.bDangerous != bDangerous || Route.Recipe != Recipe || !Run.ChooseRoute(Route.Id, Error)) { continue; }
                const FCombatantState* Active = Run.Battle().Find(Run.Battle().ActiveId());
                if (!Active || Active->Side != ECombatSide::Party) { continue; }
                Write(*Name, Run);
                Describe(*Name, Run, Seed, Offer);
                return;
            }
            AddError(FString::Printf(TEXT("No seed below 2000 gives %s with the %s party first"), *Name, Label));
        };
        FindCast(Group + TEXT("-board-vigil-pillars"), 1, true, 1);
        FindCast(Group + TEXT("-board-root-r002"), 0, false, 2);
        // The Vault finale (the same scripted player as board-vault) with the batch's party, the board the cast is also judged on (#149).
        bool bVault = false;
        for (int32 Seed = 1; Seed < 400 && !bVault; ++Seed)
        {
            FExpeditionRun Run;
            if (!ExpeditionTestSupport::ReachFinale(Run, Seed, 2, true, &Cast)) { continue; }
            Write(*(Group + TEXT("-board-vault")), Run);
            Describe(*(Group + TEXT("-board-vault")), Run, Seed, 0);
            bVault = true;
        }
        if (!bVault) { AddError(FString::Printf(TEXT("No seed below 400 reaches the broken-wall finale with the %s party first"), Label)); }
    };
    WriteCastFixtures(TEXT("cast2"), {TEXT("barbarian.berserker"), TEXT("monk.open-hand"), TEXT("paladin.devotion"), TEXT("ranger.hunter")}, TEXT("batch 2"));
    WriteCastFixtures(TEXT("cast3"), {TEXT("sorcerer.draconic-bloodline"), TEXT("warlock.fiend"), TEXT("druid.land"), TEXT("wizard.evocation")}, TEXT("batch 3"));
    WriteCastFixtures(TEXT("cast4"), {TEXT("bard.valor"), TEXT("artificer.battle-smith"), TEXT("fighter.champion"), TEXT("wizard.evocation")}, TEXT("batch 4"));
    // The Vault finale (encounter 5, #149): the first seeds that reach the fifth fight with the party acting first, found by the scripted
    // player. `board-vault` is the broken-wall finale (adjacent blocked cells get the Vault's 2-cell crypt arch; in the default gallery set),
    // `board-vault-pillars` the ruined-pillar finale (render it with `gallery --fixture board-vault-pillars`).
    auto FindFinale = [&](const TCHAR* Name, int32 Recipe)
    {
        for (int32 Seed = 1; Seed < 400; ++Seed)
        {
            FExpeditionRun Run;
            if (!ExpeditionTestSupport::ReachFinale(Run, Seed, Recipe)) { continue; }
            Write(Name, Run);
            Describe(Name, Run, Seed, 0);
            return;
        }
        AddError(FString::Printf(TEXT("No seed below 400 reaches the finale with recipe %d and the party first"), Recipe));
    };
    FindFinale(TEXT("board-vault"), 2);
    FindFinale(TEXT("board-vault-pillars"), 1);
    // The dangerous finale (#150): the default party against raiders on the Vault, so the rigged raider can be measured there too (`board-vault-raiders`).
    {
        bool bFound = false;
        for (int32 Seed = 1; Seed < 400 && !bFound; ++Seed)
        {
            FExpeditionRun Run;
            if (!ExpeditionTestSupport::ReachFinale(Run, Seed, 2, true, nullptr, true)) { continue; }
            Write(TEXT("board-vault-raiders"), Run);
            Describe(TEXT("board-vault-raiders"), Run, Seed, 1);
            bFound = true;
        }
        if (!bFound) { AddError(TEXT("No seed below 400 reaches the dangerous broken-wall finale with the party first")); }
    }
    // Action dock fixtures (#58): the first seed whose first turn belongs to the named kit, then a script of legal commands
    // (capture mode itself still only resumes and looks) that puts the hero in the state the tooltips explain.
    auto CloseIn = [](FExpeditionRun& Run, int32 Id)
    {
        FIntPoint Best = Run.Battle().Find(Id)->Cell;
        int32 BestSteps = MAX_int32;
        for (const FIntPoint& Cell : Run.Battle().LegalMoves(Id))
        {
            for (const FCombatantState& Enemy : Run.Battle().Snapshot().Encounter.Combatants)
            {
                const int32 Steps = ExpeditionTestSupport::Distance(Cell, Enemy.Cell);
                if (Enemy.Side == ECombatSide::Enemy && Enemy.HP > 0 && Steps < BestSteps) { BestSteps = Steps; Best = Cell; }
            }
        }
        return BestSteps != MAX_int32 && Run.Submit({ECombatCommandType::Move, Id, Best}).bAccepted;
    };
    auto FindActing = [&](const TCHAR* Name, const TArray<FString>& Builds, const FString& Acting, const TFunction<bool(FExpeditionRun&, int32)>& Script)
    {
        for (int32 Seed = 1; Seed < 400; ++Seed)
        {
            TArray<FPartyBuildSelection> Party;
            for (int32 Index = 0; Index < Builds.Num(); ++Index)
            {
                FPartyBuildSelection Slot;
                Slot.Id = Index + 1; Slot.Name = FString::Printf(TEXT("Hero %d"), Index + 1); Slot.BuildId = Builds[Index];
                Party.Add(Slot);
            }
            FString Error;
            FExpeditionRun Run;
            if (!Run.NewRun(Seed, Party, Error, FString::Printf(TEXT("capture-kit-%d"), Seed)) || !Run.ChooseRoute(Run.Progress().Offers[0].Id, Error)) { continue; }
            const FCombatantState* Active = Run.Battle().Find(Run.Battle().ActiveId());
            if (!Active || Active->Side != ECombatSide::Party || Active->BuildId != Acting) { continue; }
            const int32 Id = Active->Id;
            if (Script && (!Script(Run, Id) || Run.Battle().ActiveId() != Id)) { continue; }
            Write(Name, Run);
            Describe(Name, Run, Seed, 0);
            return;
        }
        AddError(FString::Printf(TEXT("No seed below 400 gives %s the first turn in the state %s needs"), *Acting, Name));
    };
    FindActing(TEXT("kit-barbarian-battle"), {TEXT("barbarian.berserker"), TEXT("wizard.evocation"), TEXT("cleric.life"), TEXT("rogue.thief")}, TEXT("barbarian.berserker"), nullptr);
    FindActing(TEXT("kit-wizard-battle"), {TEXT("wizard.evocation"), TEXT("fighter.battle-master"), TEXT("cleric.life"), TEXT("rogue.thief")}, TEXT("wizard.evocation"), nullptr);
    // Already raging: the Rage bonus action was spent, so Rage says "Already raging" and the Bonus action is gone.
    FindActing(TEXT("kit-barbarian-raging"), {TEXT("barbarian.berserker"), TEXT("wizard.evocation"), TEXT("cleric.life"), TEXT("rogue.thief")}, TEXT("barbarian.berserker"),
        [](FExpeditionRun& Run, int32 Id)
        {
            return Run.Submit({ECombatCommandType::Special1, Id, {}, Id}).bAccepted && Run.Battle().Find(Id)->RageTurns > 0;
        });
    // Everything spent: the Wizard closes in, bursts (Bonus, a Kit) and fires a missile (Action, a Kit and a Slot), leaving no Kit.
    const auto SpendEverything = [&CloseIn](FExpeditionRun& Run, int32 Id)
    {
        for (int32 Step = 0; Step < 4; ++Step)
        {
            const TArray<int32> Burst = Run.Battle().LegalTargets(Id, ECombatCommandType::Special2);
            const TArray<int32> Missile = Run.Battle().LegalTargets(Id, ECombatCommandType::MagicMissile);
            if (!Burst.IsEmpty() && !Missile.IsEmpty())
            {
                return Run.Submit({ECombatCommandType::Special2, Id, {}, Burst[0]}).bAccepted && Run.Submit({ECombatCommandType::MagicMissile, Id, {}, Missile[0]}).bAccepted &&
                    Run.Battle().Find(Id)->KitUses == 0 && !Run.Battle().Snapshot().bActionAvailable && !Run.Battle().Snapshot().bBonusActionAvailable;
            }
            if (!CloseIn(Run, Id)) { return false; }
        }
        return false;
    };
    FindActing(TEXT("kit-wizard-all-spent"), {TEXT("wizard.evocation"), TEXT("fighter.battle-master"), TEXT("cleric.life"), TEXT("rogue.thief")}, TEXT("wizard.evocation"), SpendEverything);
    // Nothing but End Turn (#158): the same Wizard, then every step of movement walked, so no action in the bar is available.
    FindActing(TEXT("end-turn-only-battle"), {TEXT("wizard.evocation"), TEXT("fighter.battle-master"), TEXT("cleric.life"), TEXT("rogue.thief")}, TEXT("wizard.evocation"),
        [&SpendEverything](FExpeditionRun& Run, int32 Id)
        {
            if (!SpendEverything(Run, Id)) { return false; }
            for (int32 Step = 0; Step < 30 && Run.Battle().Snapshot().MovementLeft > 0; ++Step)
            {
                const TArray<FIntPoint> Moves = Run.Battle().LegalMoves(Id);
                if (Moves.IsEmpty() || !Run.Submit({ECombatCommandType::Move, Id, Moves.Last()}).bAccepted) { break; }
            }
            for (const ECombatCommandType Command : FActionFactsQuery::BarFor(Run.Battle(), Id))
            { if (Command != ECombatCommandType::EndTurn && FActionFactsQuery::For(Run.Battle(), Id, Command).bAvailable) { return false; } }
            return true;
        });
    // A Downed ally (#158): the default-seed battle with its second hero Downed, put through the snapshot's validation like any restored checkpoint.
    {
        FString Error;
        FExpeditionRun Run;
        Run.NewRun(DefaultSeed, Error, FString::Printf(TEXT("capture-%d"), DefaultSeed));
        Run.ChooseRoute(Run.Progress().Offers[0].Id, Error);
        FExpeditionSnapshot Snapshot = Run.Snapshot();
        const int32 ActiveId = Run.Battle().ActiveId();
        bool bDowned = false;
        for (FCombatantState& Unit : Snapshot.Battle.Encounter.Combatants)
        {
            if (!bDowned && Unit.Side == ECombatSide::Party && Unit.Id != ActiveId) { Unit.HP = 0; Unit.Life = ECombatLife::Downed; Unit.RescueTurnsLeft = 2; bDowned = true; }
        }
        FExpeditionRun Downed;
        if (TestTrue(TEXT("A Downed ally restores as a valid checkpoint"), bDowned && Downed.Restore(Snapshot, Error)))
        {
            Write(TEXT("downed-ally-battle"), Downed);
            Describe(TEXT("downed-ally-battle"), Downed, DefaultSeed, 0);
        }
    }
    // Blocked sight: a first turn where some attack or ability finds every target in range but behind an obstacle.
    {
        bool bFound = false;
        for (int32 Seed = 1; Seed < 3000 && !bFound; ++Seed)
        {
            for (int32 Offer = 0; Offer < 2 && !bFound; ++Offer)
            {
                TArray<FPartyBuildSelection> Party;
                const TCHAR* Builds[] = {TEXT("wizard.evocation"), TEXT("ranger.hunter"), TEXT("fighter.battle-master"), TEXT("cleric.life")};
                for (int32 Index = 0; Index < 4; ++Index)
                {
                    FPartyBuildSelection Slot;
                    Slot.Id = Index + 1; Slot.Name = FString::Printf(TEXT("Hero %d"), Index + 1); Slot.BuildId = Builds[Index];
                    Party.Add(Slot);
                }
                FString Error;
                FExpeditionRun Run;
                if (!Run.NewRun(Seed, Party, Error, FString::Printf(TEXT("capture-sight-%d"), Seed))) { continue; }
                const FExpeditionRoute& Route = Run.Progress().Offers[Offer];
                if (!Run.ChooseRoute(Route.Id, Error)) { continue; }
                const FCombatantState* Active = Run.Battle().Find(Run.Battle().ActiveId());
                if (!Active || Active->Side != ECombatSide::Party) { continue; }
                TArray<ECombatCommandType> Commands = {ECombatCommandType::Attack};
                for (const FAbilityFacts& Ability : FBuildCatalog::Abilities(*Active)) { Commands.Add(Ability.Command); }
                for (const ECombatCommandType Command : Commands)
                {
                    if (!FActionFactsQuery::For(Run.Battle(), Active->Id, Command).Blocks(EActionBlocker::SightBlocked)) { continue; }
                    Write(TEXT("sight-blocked-battle"), Run);
                    Describe(TEXT("sight-blocked-battle"), Run, Seed, Offer);
                    bFound = true;
                    break;
                }
            }
        }
        TestTrue(TEXT("A first turn exists where an action's only targets are behind blocked sight"), bFound);
    }
    // Subclass palettes and duplicate accents (#150 batch 4). Every catalog build is captured on the three boards the cast is judged on (Vigil pillars,
    // Root recipe 2, the plain-road Vault finale). Each party starts a run at the seed the default party's board uses and takes the first seed from there
    // that gives the same recipe with its own heroes acting first, so most parties share the board and every board has the same recipe and size class.
    // Parties are four distinct classes (no accent), formed greedily from the classes with the most builds left; a short last party is filled with builds
    // that were already covered. Fixtures: `pal-NN-vigil|root|vault`, palette-parties.json naming each party, and `dup-four-fighters-*` and
    // `dup-two-pairs-*` for the duplicate accents.
    {
        const TCHAR* BoardNames[3] = {TEXT("vigil"), TEXT("root"), TEXT("vault")};
        const auto Selection = [](const TArray<FString>& Builds)
        {
            TArray<FPartyBuildSelection> Party;
            for (int32 Index = 0; Index < Builds.Num(); ++Index)
            {
                FPartyBuildSelection Slot;
                Slot.Id = Index + 1; Slot.Name = FString::Printf(TEXT("Hero %d"), Index + 1); Slot.BuildId = Builds[Index];
                Party.Add(Slot);
            }
            return Party;
        };
        // The base seeds (the default party's): where each party's search starts.
        const auto BaseSeed = [](int32 Offer, bool bDangerous, int32 Recipe) -> int32
        {
            for (int32 Seed = 1; Seed < 2000; ++Seed)
            {
                FString Error;
                FExpeditionRun Run;
                if (!Run.NewRun(Seed, Error, FString::Printf(TEXT("capture-%d"), Seed))) { continue; }
                const FExpeditionRoute& Route = Run.Progress().Offers[Offer];
                if (Route.bDangerous != bDangerous || Route.Recipe != Recipe || !Run.ChooseRoute(Route.Id, Error)) { continue; }
                const FCombatantState* Active = Run.Battle().Find(Run.Battle().ActiveId());
                if (Active && Active->Side == ECombatSide::Party) { return Seed; }
            }
            return INDEX_NONE;
        };
        int32 VaultBase = INDEX_NONE;
        for (int32 Seed = 1; Seed < 400 && VaultBase == INDEX_NONE; ++Seed) { FExpeditionRun Run; if (ExpeditionTestSupport::ReachFinale(Run, Seed, 2, true)) { VaultBase = Seed; } }
        const int32 Bases[3] = {BaseSeed(1, true, 1), BaseSeed(0, false, 2), VaultBase};
        if (!TestTrue(TEXT("The three palette boards are found with the default party first"), Bases[0] != INDEX_NONE && Bases[1] != INDEX_NONE && Bases[2] != INDEX_NONE)) { return false; }
        // One fixture: the party's own run on the board (0 Vigil pillars, 1 Root recipe 2, 2 the Vault finale), from the base seed upward.
        const auto WriteBoard = [&](const FString& Name, const TArray<FString>& Builds, int32 Board)
        {
            const TArray<FPartyBuildSelection> Party = Selection(Builds);
            for (int32 Step = 0; Step < (Board == 2 ? 400 : 2000); ++Step)
            {
                const int32 Seed = 1 + ((Bases[Board] - 1 + Step) % (Board == 2 ? 399 : 1999));
                FString Error;
                FExpeditionRun Run;
                if (Board == 2)
                {
                    if (!ExpeditionTestSupport::ReachFinale(Run, Seed, 2, true, &Party)) { continue; }
                }
                else
                {
                    if (!Run.NewRun(Seed, Party, Error, FString::Printf(TEXT("capture-pal-%d"), Seed))) { continue; }
                    const FExpeditionRoute& Route = Run.Progress().Offers[Board == 0 ? 1 : 0];
                    if (Route.bDangerous != (Board == 0) || Route.Recipe != (Board == 0 ? 1 : 2) || !Run.ChooseRoute(Route.Id, Error)) { continue; }
                    const FCombatantState* Active = Run.Battle().Find(Run.Battle().ActiveId());
                    if (!Active || Active->Side != ECombatSide::Party) { continue; }
                }
                if (Board == 2)
                {
                    // The scripted player reaches the finale with heroes Downed, lying under their neighbours; a look at a build wants every hero standing, so the
                    // Downed are stood up at full health (the same kind of edit as downed-ally-battle, validated by the restore). A seed that reaches the finale with a hero Dead is skipped (a permanent loss cannot be undone).
                    FExpeditionSnapshot Snapshot = Run.Snapshot();
                    if (Snapshot.Battle.Encounter.Combatants.ContainsByPredicate([](const FCombatantState& Unit) { return Unit.Side == ECombatSide::Party && Unit.Life == ECombatLife::Dead; })) { continue; }
                    for (FCombatantState& Unit : Snapshot.Battle.Encounter.Combatants)
                    {
                        if (Unit.Side == ECombatSide::Party && Unit.Life == ECombatLife::Downed)
                        { Unit.Life = ECombatLife::Conscious; Unit.HP = Unit.MaxHP; Unit.RescueTurnsLeft = 0; Unit.DeathSaveSuccesses = 0; Unit.DeathSaveFailures = 0; }
                    }
                    FExpeditionRun Healed;
                    if (!Healed.Restore(Snapshot, Error)) { continue; }
                    Run = MoveTemp(Healed);
                }
                Write(*Name, Run);
                Describe(*Name, Run, Seed, Board == 0 ? 1 : 0);
                return;
            }
            AddError(FString::Printf(TEXT("No seed gives %s with its party first"), *Name));
        };
        // Greedy parties of four distinct classes over every catalog build.
        TMap<FString, TArray<FString>> Left;
        TArray<FString> ClassOrder;
        for (const FBuildDefinition& Build : FBuildCatalog::All())
        {
            if (!Left.Contains(Build.Profile.ClassId)) { ClassOrder.Add(Build.Profile.ClassId); }
            Left.FindOrAdd(Build.Profile.ClassId).Add(Build.Id);
        }
        TMap<FString, FString> First;
        for (const FString& Class : ClassOrder) { First.Add(Class, Left[Class][0]); }
        TArray<TArray<FString>> Parties;
        TArray<TArray<FString>> Fillers;
        for (;;)
        {
            TArray<FString> Open = ClassOrder.FilterByPredicate([&Left](const FString& Class) { return Left[Class].Num() > 0; });
            if (Open.IsEmpty()) { break; }
            Open.StableSort([&Left](const FString& A, const FString& B) { return Left[A].Num() > Left[B].Num(); });
            TArray<FString> Party, Filler;
            for (int32 Index = 0; Index < Open.Num() && Party.Num() < 4; ++Index) { Party.Add(Left[Open[Index]][0]); Left[Open[Index]].RemoveAt(0); }
            for (const FString& Class : ClassOrder)
            {
                if (Party.Num() >= 4) { break; }
                if (!Party.ContainsByPredicate([&Class](const FString& Id) { return Id.StartsWith(Class + TEXT(".")); })) { Party.Add(First[Class]); Filler.Add(First[Class]); }
            }
            Parties.Add(Party);
            Fillers.Add(Filler);
        }
        FString Manifest = TEXT("{\n  \"parties\": [\n");
        for (int32 PartyIndex = 0; PartyIndex < Parties.Num(); ++PartyIndex)
        {
            const FString Id = FString::Printf(TEXT("pal-%02d"), PartyIndex);
            const FString Filler = Fillers[PartyIndex].IsEmpty() ? FString() : FString::Printf(TEXT("\"%s\""), *FString::Join(Fillers[PartyIndex], TEXT("\", \"")));
            Manifest += FString::Printf(TEXT("    {\"id\": \"%s\", \"builds\": [\"%s\"], \"filler\": [%s]}%s\n"), *Id, *FString::Join(Parties[PartyIndex], TEXT("\", \"")),
                *Filler, PartyIndex + 1 < Parties.Num() ? TEXT(",") : TEXT(""));
            for (int32 Board = 0; Board < 3; ++Board) { WriteBoard(FString::Printf(TEXT("%s-%s"), *Id, BoardNames[Board]), Parties[PartyIndex], Board); }
        }
        Manifest += TEXT("  ]\n}\n");
        TestTrue(TEXT("The palette party manifest writes"), FFileHelper::SaveStringToFile(Manifest, *FPaths::Combine(Root, TEXT("palette-parties.json"))));
        // Duplicate parties: four of one build, and two pairs of two classes; on Vigil, Root and the Vault.
        TArray<TPair<FString, TArray<FString>>> Duplicates;
        Duplicates.Emplace(TEXT("dup-four-fighters"), TArray<FString>{TEXT("fighter.champion"), TEXT("fighter.champion"), TEXT("fighter.champion"), TEXT("fighter.champion")});
        Duplicates.Emplace(TEXT("dup-two-pairs"), TArray<FString>{TEXT("cleric.life"), TEXT("rogue.thief"), TEXT("cleric.war"), TEXT("rogue.thief")});
        for (const TPair<FString, TArray<FString>>& Duplicate : Duplicates)
        { for (int32 Board = 0; Board < 3; ++Board) { WriteBoard(FString::Printf(TEXT("%s-%s"), *Duplicate.Key, BoardNames[Board]), Duplicate.Value, Board); } }
    }
    bool bVictoryWritten = false;
    for (int32 Seed = 1; Seed < 60 && !bVictoryWritten; ++Seed)
    {
        FString Error;
        FExpeditionRun Run;
        Run.NewRun(Seed, Error, FString::Printf(TEXT("capture-%d"), Seed));
        Run.ChooseRoute(Run.Progress().Offers[0].Id, Error);
        Write(TEXT("first-battle"), Run);
        if (!ExpeditionTestSupport::FinishCombat(Run) || Run.Progress().Phase != EExpeditionPhase::Reward) { continue; }
        Write(TEXT("reward"), Run);
        Run.ClaimReward(Survivor(Run), Error);
        Write(TEXT("camp"), Run);
        Run.Recover(true, Error);
        Run.ChooseRoute(Run.Progress().Offers[1].Id, Error);
        if (!ExpeditionTestSupport::FinishCombat(Run) || Run.Progress().Phase != EExpeditionPhase::Reward) { continue; }
        Run.ClaimReward(Survivor(Run), Error);
        while (Run.Progress().Phase == EExpeditionPhase::Route && Run.Progress().EncountersCompleted < 4)
        {
            if (!Run.ChooseRoute(Run.Progress().Offers[0].Id, Error) || !ExpeditionTestSupport::FinishCombat(Run) || Run.Progress().Phase != EExpeditionPhase::Reward) { break; }
            if (Run.Progress().EncountersCompleted == 4) { Write(TEXT("fourth-reward"), Run); }
            if (!Run.ClaimReward(Survivor(Run), Error)) { break; }
        }
        if (Run.Progress().Phase != EExpeditionPhase::Route || Run.Progress().EncountersCompleted != 4) { continue; }
        Write(TEXT("final-route"), Run);
        Run.ChooseRoute(Run.Progress().Offers[0].Id, Error);
        Write(TEXT("vault"), Run);
        if (!ExpeditionTestSupport::FinishCombat(Run) || Run.Progress().Phase != EExpeditionPhase::Victory) { continue; }
        Write(TEXT("victory"), Run);
        bVictoryWritten = true;
    }
    TestTrue(TEXT("A seed reaches victory"), bVictoryWritten);
    FString Error;
    FExpeditionRun Lost;
    Lost.NewRun(3, Error, TEXT("capture-defeat"));
    Lost.ChooseRoute(Lost.Progress().Offers[1].Id, Error);
    ExpeditionTestSupport::FinishCombat(Lost, false);
    TestEqual(TEXT("A passive party is defeated"), Lost.Progress().Phase, EExpeditionPhase::Defeat);
    Write(TEXT("defeat"), Lost);
    return !HasAnyErrors();
}
#endif
