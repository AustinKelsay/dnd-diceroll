#include "Run/ExpeditionRun.h"
#include "Party/PartyContinuity.h"
#include "Party/BuildCatalog.h"
#include "Rules/AbilityRules.h"

namespace
{
struct FEncounterRecipe
{
    const TCHAR* Terrain;
    /** The tactical note revision 9 previews show after the terrain. */
    const TCHAR* Note;
    /** Cells on the smallest 16 x 11 board; a front-to-back recipe gives X as an offset from the centre column Width / 2. */
    TArray<FIntPoint> BlockedCells;
    int32 EnemyColumn;
    int32 FirstEnemyRow;
    /** 0 stones, 1 pillars, 2 walls (FEncounterIdentity::ObstacleKind). */
    int32 ObstacleKind;
    bool bFrontToBack;
};

// Generated grids are about three times the original 8-9 x 7-8 area.
constexpr int32 MinWidth = 16, MaxWidth = 18, MinHeight = 11, MaxHeight = 12;
// The party forms a column here, centred vertically. Each left-to-right recipe puts the enemy line at a
// fixed column 5..7 cells away, so fights open within about a turn at speed 4 and the rest
// of the board stays free for flanking and retreat on both sides.
constexpr int32 PartyColumn = 4;
// The front row of a front-to-back layout (the camera's home side), where the party enters.
constexpr int32 FrontRow = 1;

// The preview and generated geometry read the same bounded recipe definition.
// Obstacles fit the smallest 16 x 11 grid and never touch a spawn: the party uses column 4,
// rows 3..7, and each enemy line uses its own column on rows 3/5/7(/9) or 4/6/8.
// Recipes 0-2 are every revision's; revision 9 Compact runs (#188, docs/runs/gameplay-depth/11-encounter-identity-contract.md)
// draw them for the approach (fights 1-2), 3-5 for the pressure fights (3-4) and 6 for the finale.
const FEncounterRecipe Recipes[] = {
    {TEXT("split stones"), TEXT("open flanks"), {{6, 2}, {7, 2}, {6, 3}, {8, 6}, {8, 7}, {7, 7}, {12, 1}, {13, 1}, {2, 9}, {3, 9}, {13, 5}, {15, 8}}, 10, 3, 0, false},
    {TEXT("ruined pillars"), TEXT("broken sight lines"), {{6, 2}, {6, 8}, {7, 5}, {11, 3}, {11, 7}, {13, 1}, {14, 9}, {1, 1}, {1, 9}, {2, 5}}, 9, 4, 1, false},
    {TEXT("broken wall"), TEXT("narrow gaps"), {{7, 1}, {7, 2}, {7, 3}, {7, 5}, {7, 6}, {7, 8}, {7, 9}, {13, 1}, {14, 1}, {15, 1}, {13, 9}, {14, 9}}, 11, 3, 2, false},
    // Revision 9's layouts keep every open cell beside at least three open cells (a board corner excepted): obstacles stay off the edge ring
    // and never sit one cell apart or diagonally, so no 1-wide gap, edge corridor or inside corner can be sealed by a couple of bodies (#188).
    // Staggered single pillars break sight lines across the middle, which favours closing in over shooting.
    {TEXT("fallen colonnade"), TEXT("pillars block shots"), {{6, 2}, {6, 5}, {6, 8}, {8, 3}, {8, 7}, {11, 2}, {11, 5}, {11, 8}, {13, 4}, {2, 2}, {2, 8}}, 10, 3, 1, false},
    // Two walled lanes and a central wall: four two-cell lanes, a chokepoint fight.
    {TEXT("sunken causeway"), TEXT("two lanes"), {{6, 2}, {7, 2}, {8, 2}, {9, 2}, {10, 2}, {6, 8}, {7, 8}, {8, 8}, {9, 8}, {10, 8}, {7, 5}, {8, 5}, {9, 5}}, 11, 3, 2, false},
    // Scattered cover: ranged duels around the cairns.
    {TEXT("cairn field"), TEXT("scattered cover"), {{6, 2}, {6, 6}, {7, 4}, {7, 8}, {9, 2}, {9, 6}, {12, 4}, {11, 8}, {13, 2}, {13, 7}, {2, 2}, {2, 8}}, 10, 3, 0, false},
    // Pillars line an aisle from the open front to the guardians at the back, by the Vault world's Waystone (#149).
    {TEXT("vault sanctum"), TEXT("an aisle to the warden"), {{-3, 2}, {-3, 5}, {-3, 8}, {3, 2}, {3, 5}, {3, 8}, {-5, 3}, {-5, 7}, {5, 3}, {5, 7}}, 0, 0, 1, true}
};
// Every revision draws a recipe from the first three, so revision 9 keeps revision 8's random numbers (and fights 1-2 their boards).
constexpr int32 DrawnRecipes = 3, PressureRecipe = 3, FinaleRecipe = 6;
constexpr int32 IdentityRevision = 9;
/** Revision 10 (#218): Compact rewards offer Reserve and Honed beside Gear. Generation is revision 9's exactly. */
constexpr int32 UpgradeRevision = 10;

/** The saved FCombatantState::Role values (#188). Revisions 7 and 8 only ever use Raider and Archer. */
enum class EFoeRole : uint8 { Raider, Archer, Skirmisher, Brute, Warden };

const TCHAR* RoleId(EFoeRole Role)
{
    switch (Role)
    {
    case EFoeRole::Archer: return TEXT("Archer");
    case EFoeRole::Skirmisher: return TEXT("Skirmisher");
    case EFoeRole::Brute: return TEXT("Brute");
    case EFoeRole::Warden: return TEXT("Warden");
    default: return TEXT("Raider");
    }
}

/** Revision 9 encounter identity: Compact runs created under revision 9 or later. SRD keeps revision 7 generation at every revision. */
bool HasIdentity(ERuleset Ruleset, int32 Revision) { return Ruleset == ERuleset::Compact && Revision >= IdentityRevision; }

EEncounterStage StageOf(int32 Stage)
{
    return Stage >= FExpeditionRun::EncounterCount - 1 ? EEncounterStage::Finale : Stage >= 2 ? EEncounterStage::Pressure : EEncounterStage::Approach;
}

/** The recipes a fight may use: 0-2 everywhere before revision 9; with identity, 0-2, 3-5 or 6 by stage. */
bool AllowedRecipe(bool bIdentity, int32 Stage, int32 Recipe)
{
    if (!bIdentity) { return Recipe >= 0 && Recipe < DrawnRecipes; }
    switch (StageOf(Stage))
    {
    case EEncounterStage::Approach: return Recipe >= 0 && Recipe < PressureRecipe;
    case EEncounterStage::Pressure: return Recipe >= PressureRecipe && Recipe < FinaleRecipe;
    default: return Recipe == FinaleRecipe;
    }
}

/** The roster in spawn and id order. Before revision 9 (and in SRD): raiders, then the archer. */
TArray<EFoeRole> Roster(bool bIdentity, int32 Stage, bool bDangerous)
{
    using R = EFoeRole;
    if (!bIdentity || StageOf(Stage) == EEncounterStage::Approach) { return bDangerous ? TArray<R>{R::Raider, R::Raider, R::Archer} : TArray<R>{R::Raider, R::Archer}; }
    if (StageOf(Stage) == EEncounterStage::Pressure) { return bDangerous ? TArray<R>{R::Brute, R::Skirmisher, R::Raider, R::Archer} : TArray<R>{R::Skirmisher, R::Raider, R::Archer}; }
    return bDangerous ? TArray<R>{R::Warden, R::Brute, R::Raider, R::Archer} : TArray<R>{R::Warden, R::Raider, R::Archer};
}

/** The preview's word for a role; a Guarded raider is a scout, as its name says. */
const TCHAR* RoleWord(EFoeRole Role, bool bDangerous)
{
    switch (Role)
    {
    case EFoeRole::Archer: return TEXT("archer");
    case EFoeRole::Skirmisher: return TEXT("skirmisher");
    case EFoeRole::Brute: return TEXT("brute");
    case EFoeRole::Warden: return TEXT("warden");
    default: return bDangerous ? TEXT("raider") : TEXT("scout");
    }
}

/** The fixed risk table (contract 11), verified against the measured cost by report 12. */
const TCHAR* RiskWord(int32 Stage, bool bDangerous)
{
    switch (StageOf(Stage))
    {
    case EEncounterStage::Approach: return bDangerous ? TEXT("high") : TEXT("low");
    case EEncounterStage::Pressure: return bDangerous ? TEXT("severe") : TEXT("moderate");
    default: return bDangerous ? TEXT("severe") : TEXT("high");
    }
}

// One profile supplies generated facts, truthful text and restore validation. bUpgradeCards (revision 10 Compact, #218) changes only the
// reward phrase of routes 1-4: a reward card is a choice, no longer a guaranteed +1 gear.
void DescribeRoute(FExpeditionRoute& Route, int32 Stage, int32 Choice, bool bSecuredSupplies, bool bIdentity, bool bUpgradeCards)
{
    Route.Id = Stage * 2 + Choice + 1;
    Route.bDangerous = Choice == 1;
    Route.EnemyCount = Choice == 1 ? 3 : 2;
    Route.RewardPotions = Choice == 1 || bSecuredSupplies ? 2 : 1;
    Route.Name = Choice == 0 ? TEXT("Stream trail") : TEXT("Watchtower road");
    const auto& Recipe = Recipes[Route.Recipe];
    const bool bFinale = Stage == FExpeditionRun::EncounterCount - 1;
    if (bIdentity)
    {
        // Revision 9: the route tag, then one fact per route-card line: roster, terrain with its note, risk, rewards.
        const TArray<EFoeRole> Foes = Roster(true, Stage, Route.bDangerous);
        TArray<FString> Words;
        for (const EFoeRole Role : Foes) { Words.Add(RoleWord(Role, Route.bDangerous)); }
        Route.EnemyCount = Foes.Num();
        if (bFinale) { Route.RewardPotions = 0; Route.Name = TEXT("Artifact vault: ") + Route.Name; }
        const FString Rewards = bFinale ? FString(TEXT("Recover the artifact; no loot")) : FString::Printf(TEXT("%s, +%d potion%s%s"), bUpgradeCards ? TEXT("1 upgrade choice") : TEXT("+1 gear"),
            Route.RewardPotions, Route.RewardPotions == 1 ? TEXT("") : TEXT("s"), Choice == 0 && bSecuredSupplies ? TEXT(" (secured supplies)") : TEXT(""));
        Route.Preview = FString::Printf(TEXT("%s | %d foes: %s | %s: %s | Risk: %s | %s"), Choice == 0 ? TEXT("Guarded") : TEXT("Dangerous"),
            Route.EnemyCount, *FString::Join(Words, TEXT(", ")), Recipe.Terrain, Recipe.Note, RiskWord(Stage, Route.bDangerous), *Rewards);
        return;
    }
    Route.Preview = FString::Printf(TEXT("%s | %d %s | %s | +1 gear (rank 3 cap) | +%d potions%s"),
        Choice == 0 ? TEXT("Guarded") : TEXT("Dangerous"), Route.EnemyCount,
        Choice == 0 ? TEXT("scouts") : TEXT("raiders incl. archer"), Recipe.Terrain, Route.RewardPotions,
        Choice == 0 && bSecuredSupplies ? TEXT(" (secured supplies)") : TEXT(""));
    if (bFinale)
    {
        Route.RewardPotions = 0;
        Route.Name = TEXT("Artifact vault: ") + Route.Name;
        Route.Preview = FString::Printf(TEXT("%s | %d guards incl. archer | %s | recover artifact; no loot"),
            Choice == 0 ? TEXT("Guarded") : TEXT("Dangerous"), Route.EnemyCount, Recipe.Terrain);
    }
}

// Enemy stats for one generated fight, by ruleset, by the catalog revision the run was created under and, from revision 9, by role
// (#184 docs/runs/gameplay-depth/10-balance-tuning-report.md; #188 11-encounter-identity-contract.md and 12-encounter-identity-report.md).
// Revision 7 numbers: every SRD run, and Compact runs created before revision 8, for their whole lifetime. Revision 8 Compact
// numbers: the raiders and archers of every later Compact run. Revision 9 roles adjust that base. False for an unsupported
// ruleset, revision or role, so generation and restore fail instead of guessing.
struct FEnemyStats { int32 HP = 0, ArmorClass = 0, AttackBonus = 0, DamageDie = 0, DamageBonus = 0, Speed = 4; };

/** Revision 9's base for the pressure fights (3-4) and the finale (5): this share of the revision 8 HP, and a damage bonus of 3
 * (revision 8 rises to 4 from fight 4). */
constexpr int32 PressureBaseHpPercent = 70, FinaleBaseHpPercent = 90, LaterBaseDamageBonus = 3;

/** Revision 9 role adjustments to the revision 8 base for the route and stage (contract 11, tuned in report 12). */
struct FRoleTuning { int32 HpPercent; int32 ArmorDelta; int32 DamageDie; int32 DamageDelta; int32 Speed; };
FRoleTuning RoleTuning(EFoeRole Role, int32 BaseDie)
{
    switch (Role)
    {
    case EFoeRole::Skirmisher: return {60, 1, 4, 0, 6};
    case EFoeRole::Brute: return {110, -1, 10, 0, 3};
    case EFoeRole::Warden: return {175, 1, BaseDie, 2, 4};
    default: return {100, 0, BaseDie, 0, 4};
    }
}

bool EnemyStats(ERuleset Ruleset, int32 Revision, EFoeRole Role, bool bDangerous, int32 Stage, FEnemyStats& Stats)
{
    if (Revision < FBuildCatalog::MinimumRevision || Revision > FBuildCatalog::Revision) { return false; }
    bool bTuned = false;
    switch (Ruleset)
    {
    case ERuleset::Compact: bTuned = Revision >= 8; break;
    case ERuleset::SRD: bTuned = false; break;
    default: return false;
    }
    const bool bBaseRole = Role == EFoeRole::Raider || Role == EFoeRole::Archer;
    if (!bTuned)
    {
        Stats.HP = (bDangerous ? 12 : 10) + Stage * 2;
        Stats.ArmorClass = bDangerous ? 12 : 11;
        Stats.AttackBonus = 2;
        Stats.DamageDie = bDangerous ? 6 : 4;
        Stats.DamageBonus = 1;
        return bBaseRole;
    }
    Stats.HP = (bDangerous ? 24 : 20) + Stage * 4;
    Stats.ArmorClass = bDangerous ? 13 : 12;
    Stats.AttackBonus = FMath::Min(5, 4 + Stage / 2);
    Stats.DamageDie = bDangerous ? 8 : 6;
    Stats.DamageBonus = 3 + Stage / 3;
    if (!HasIdentity(Ruleset, Revision)) { return bBaseRole; }
    // Revision 9 pressure and finale fights field one more foe than revision 8's, so their base is lighter (report 12, steps 2 and 4).
    if (StageOf(Stage) != EEncounterStage::Approach)
    {
        Stats.HP = Stats.HP * (StageOf(Stage) == EEncounterStage::Finale ? FinaleBaseHpPercent : PressureBaseHpPercent) / 100;
        Stats.DamageBonus = LaterBaseDamageBonus;
    }
    const FRoleTuning Tuning = RoleTuning(Role, Stats.DamageDie);
    Stats.HP = Stats.HP * Tuning.HpPercent / 100;
    Stats.ArmorClass += Tuning.ArmorDelta;
    Stats.DamageDie = Tuning.DamageDie;
    Stats.DamageBonus += Tuning.DamageDelta;
    Stats.Speed = Tuning.Speed;
    return true;
}

int32 CountSurvivors(const FCombatBattle& Battle)
{
    int32 Count = 0;
    for (const auto& Unit : Battle.Snapshot().Encounter.Combatants)
    { if (Unit.Side == ECombatSide::Party && Unit.Life == ECombatLife::Conscious) { ++Count; } }
    return Count;
}
}

bool FExpeditionRun::NewRun(int32 Seed, FString& Error, const FString& Identity, EFighterSubclass Subclass, ERuleset Ruleset, int32 Revision)
{
    if (Subclass != EFighterSubclass::Champion && Subclass != EFighterSubclass::BattleMaster) { Error = TEXT("Choose a Fighter subclass."); return false; }
    TArray<FPartyBuildSelection> Selection;
    const auto Tutorial = FCombatBattle::TutorialEncounter();
    for (int32 I = 0; I < 4; ++I)
    {
        FPartyBuildSelection Slot;
        Slot.Id = I + 1; Slot.Name = Tutorial.Combatants[I].Name; Slot.BuildId = Tutorial.Combatants[I].BuildId;
        if (I == 0) { Slot.BuildId = Subclass == EFighterSubclass::BattleMaster ? TEXT("fighter.battle-master") : TEXT("fighter.champion"); }
        Selection.Add(Slot);
    }
    return NewRun(Seed, Selection, Error, Identity, Ruleset, Revision);
}

bool FExpeditionRun::NewRun(int32 Seed, const TArray<FPartyBuildSelection>& Selection, FString& Error, const FString& Identity, ERuleset Ruleset, int32 Revision)
{
    Error.Reset();
    if (!State.Party.IsEmpty() && State.Phase != EExpeditionPhase::Victory && State.Phase != EExpeditionPhase::Defeat) { Error = TEXT("Finish this expedition before changing its party."); return false; }
    if (Ruleset != ERuleset::Compact && Ruleset != ERuleset::SRD) { Error = TEXT("Choose a valid ruleset."); return false; }
    if (Selection.Num() != 4) { Error = TEXT("Choose exactly four adventurers."); return false; }
    if (Revision < FBuildCatalog::MinimumRevision || Revision > FBuildCatalog::Revision)
    {
        Error = FString::Printf(TEXT("Rules revision %d is not playable; this build plays revisions %d to %d."), Revision, FBuildCatalog::MinimumRevision, FBuildCatalog::Revision);
        return false;
    }
    FExpeditionRun Candidate;
    Candidate.State.Seed = Seed;
    Candidate.State.CatalogRevision = Revision;
    Candidate.State.Ruleset = Ruleset;
    Candidate.State.RunId = Identity.IsEmpty() ? FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens) : Identity;
    Candidate.State.GenerationRandomState = Seed;
    auto Party = FCombatBattle::TutorialEncounter();
    Party.Ruleset = Ruleset;
    for (int32 I = 0; I < 4; ++I)
    {
        const auto Cell = Party.Combatants[I].Cell;
        const auto& Slot = Selection[I];
        if (Slot.Id != I + 1 || !FBuildCatalog::Create(Slot.BuildId, Slot.Id, Slot.Name, Party.Combatants[I]))
        { Error = TEXT("Choose a valid build and name for each independent slot 1–4."); return false; }
        Party.Combatants[I].Cell = Cell;
    }
    Candidate.State.Party = Selection;
    if (!Candidate.Combat.Start(Party, Seed, Error)) { return false; }
    Candidate.bStarted = true;
    Candidate.OfferRoutes();
    *this = MoveTemp(Candidate);
    return true;
}

void FExpeditionRun::OfferRoutes()
{
    FRandomStream Random(State.GenerationRandomState);
    State.Offers.Reset();
    const bool bSecuredSupplies = !State.History.IsEmpty() && State.History.Last().Route.bDangerous;
    const bool bIdentity = HasIdentity(State.Ruleset, State.CatalogRevision);
    const bool bUpgradeCards = OffersUpgradeCards(State.Ruleset, State.CatalogRevision);
    const EEncounterStage Stage = StageOf(State.EncountersCompleted);
    for (int32 Index = 0; Index < 2; ++Index)
    {
        FExpeditionRoute Route;
        Route.EncounterSeed = Random.RandRange(0, MAX_int32 - 1);
        // Every revision draws the same numbers in the same order; revision 9 maps the drawn 0-2 onto its stage's layouts
        // (the pressure layouts 3-5; the finale draws and ignores it for the vault sanctum).
        Route.Recipe = Random.RandRange(0, DrawnRecipes - 1);
        if (bIdentity && Stage == EEncounterStage::Pressure) { Route.Recipe += PressureRecipe; }
        if (bIdentity && Stage == EEncounterStage::Finale) { Route.Recipe = FinaleRecipe; }
        Route.Width = Random.RandRange(MinWidth, MaxWidth);
        Route.Height = Random.RandRange(MinHeight, MaxHeight);
        DescribeRoute(Route, State.EncountersCompleted, Index, bSecuredSupplies, bIdentity, bUpgradeCards);
        State.Offers.Add(Route);
    }
    State.GenerationRandomState = Random.GetCurrentSeed();
    State.Phase = EExpeditionPhase::Route;
}

bool FExpeditionRun::ChooseRoute(int32 RouteId, FString& Error)
{
    Error.Reset();
    const FExpeditionRoute* Offered = State.Offers.FindByPredicate([RouteId](const FExpeditionRoute& Route) { return Route.Id == RouteId; });
    if (!bStarted || State.Phase != EExpeditionPhase::Route || !Offered)
    {
        Error = TEXT("Choose one of the current route opportunities."); return false;
    }
    FExpeditionRun Candidate = *this;
    const FCombatEncounter Layout = Candidate.Generate(*Offered);
    const bool bReady = State.EncountersCompleted == 0 ? Candidate.Combat.Start(Layout, Offered->EncounterSeed, Error) :
        FPartyContinuity::Carry(Candidate.Combat, Layout, Offered->EncounterSeed, Error);
    if (!bReady) { return false; }
    FExpeditionEncounterRecord Record;
    Record.Route = *Offered;
    Record.Survivors = CountSurvivors(Candidate.Combat);
    Candidate.State.History.Add(Record);
    Candidate.State.Offers.Reset();
    Candidate.State.Phase = EExpeditionPhase::Combat;
    *this = MoveTemp(Candidate);
    return true;
}

FCombatEncounter FExpeditionRun::Generate(const FExpeditionRoute& Route) const
{
    FCombatEncounter Layout;
    // An unsupported recipe, ruleset, revision or role, or a preview whose enemy count disagrees, yields an encounter
    // without enemies, which Start and Restore reject.
    if (!FExpeditionRun::EncounterLayout(Route, State.EncountersCompleted, State.Ruleset, State.CatalogRevision, Layout) || Layout.Combatants.Num() != 4 + Route.EnemyCount)
    {
        Layout.Combatants.SetNum(4);
        return Layout;
    }
    for (int32 I = 0; I < 4; ++I)
    {
        const FIntPoint Spawn = Layout.Combatants[I].Cell;
        if (const auto* Hero = Combat.Find(I + 1)) { Layout.Combatants[I] = *Hero; }
        Layout.Combatants[I].Cell = Spawn;
    }
    return Layout;
}

bool FExpeditionRun::EncounterLayout(const FExpeditionRoute& Route, int32 Stage, ERuleset Ruleset, int32 Revision, FCombatEncounter& Out)
{
    FCombatEncounter Layout = FCombatBattle::TutorialEncounter();
    Layout.Ruleset = Ruleset;
    Layout.Width = Route.Width;
    Layout.Height = Route.Height;
    Layout.Combatants.SetNum(4);
    const bool bIdentity = HasIdentity(Ruleset, Revision);
    if (Stage < 0 || Stage >= EncounterCount || !AllowedRecipe(bIdentity, Stage, Route.Recipe)) { return false; }
    // Central obstacle clusters leave an open perimeter, spawn columns and lanes
    // around each cluster. No retry loop or seed can disconnect the open floor.
    const FEncounterRecipe& Recipe = Recipes[Route.Recipe];
    const int32 Centre = Route.Width / 2;
    Layout.BlockedCells = Recipe.BlockedCells;
    if (Recipe.bFrontToBack) { for (FIntPoint& Cell : Layout.BlockedCells) { Cell.X += Centre; } }
    const int32 FirstPartyRow = (Route.Height - 4) / 2;
    for (int32 Index = 0; Index < 4; ++Index)
    {
        // Front to back (the vault sanctum): the party enters along the front row, (c-2..c+1, 1).
        Layout.Combatants[Index].Cell = Recipe.bFrontToBack ? FIntPoint(Centre - 2 + Index, FrontRow) : FIntPoint(PartyColumn, FirstPartyRow + Index);
    }
    const TArray<EFoeRole> Foes = Roster(bIdentity, Stage, Route.bDangerous);
    const bool bFinale = Stage == EncounterCount - 1;
    int32 Raiders = 0;
    for (int32 Index = 0; Index < Foes.Num(); ++Index)
    {
        const EFoeRole Role = Foes[Index];
        FCombatantState Enemy;
        Enemy.Id = 20 + Index;
        Enemy.Side = ECombatSide::Enemy;
        switch (Role)
        {
        case EFoeRole::Raider: Enemy.Name = FString::Printf(TEXT("%s %d"), Route.bDangerous ? TEXT("Raider") : TEXT("Scout"), ++Raiders); break;
        // Before revision 9 every archer was the "Vault bow"; from revision 9 only the finale's is.
        case EFoeRole::Archer: Enemy.Name = !bIdentity || bFinale ? TEXT("Vault bow") : TEXT("Archer"); break;
        case EFoeRole::Skirmisher: Enemy.Name = TEXT("Skirmisher"); break;
        case EFoeRole::Brute: Enemy.Name = TEXT("Raider brute"); break;
        case EFoeRole::Warden: Enemy.Name = TEXT("Vault warden"); break;
        }
        Enemy.Role = RoleId(Role);
        if (Recipe.bFrontToBack)
        {
            // The guardians wait at the back: the warden at (c, 8), the archer and the other flanker at (c + 3, 7) and (c - 3, 7),
            // and a fourth (the brute) in front of the warden at (c, 6).
            Enemy.Cell = Role == EFoeRole::Warden ? FIntPoint(Centre, 8) : Role == EFoeRole::Archer ? FIntPoint(Centre + 3, 7) :
                Role == EFoeRole::Brute ? FIntPoint(Centre, 6) : FIntPoint(Centre - 3, 7);
        }
        // Up to four enemies use rows 3/5/7/9 or 4/6/8; every grid is at least eleven rows tall.
        else { Enemy.Cell = {Recipe.EnemyColumn, Recipe.FirstEnemyRow + Index * 2}; }
        Enemy.Range = Role == EFoeRole::Archer ? 4 : 1;
        FEnemyStats Stats;
        if (!EnemyStats(Ruleset, Revision, Role, Route.bDangerous, Stage, Stats)) { return false; }
        Enemy.HP = Enemy.MaxHP = Stats.HP;
        Enemy.ArmorClass = Stats.ArmorClass;
        Enemy.AttackBonus = Stats.AttackBonus;
        Enemy.DamageDie = Stats.DamageDie;
        Enemy.DamageBonus = Stats.DamageBonus;
        Enemy.Speed = Stats.Speed;
        Enemy.InitiativeBonus = 1;
        Layout.Combatants.Add(Enemy);
    }
    Out = MoveTemp(Layout);
    return true;
}

FEncounterIdentity FExpeditionRun::Encounter(const FExpeditionProgress& Progress, const FExpeditionRoute& Route)
{
    FEncounterIdentity Identity;
    Identity.Stage = StageOf(FMath::Clamp((Route.Id - 1) / 2, 0, EncounterCount - 1));
    const bool bIdentity = HasIdentity(Progress.Ruleset, Progress.CatalogRevision);
    if (Route.Recipe < 0 || Route.Recipe >= (bIdentity ? static_cast<int32>(UE_ARRAY_COUNT(Recipes)) : DrawnRecipes)) { return Identity; }
    const FEncounterRecipe& Recipe = Recipes[Route.Recipe];
    Identity.ObstacleKind = Recipe.ObstacleKind;
    Identity.Layout = Recipe.Terrain;
    Identity.bFrontToBack = Recipe.bFrontToBack;
    return Identity;
}

FCombatResult FExpeditionRun::Submit(const FCombatCommand& Command)
{
    const FCombatantState* Actor = Combat.Find(Command.ActorId);
    if (!bStarted || State.Phase != EExpeditionPhase::Combat || !Actor || Actor->Side != ECombatSide::Party)
    { return {false, TEXT("Only party commands in an active encounter are allowed.")}; }
    FCombatResult Result = Combat.Submit(Command);
    if (Result.bAccepted) { ResolveCombat(); }
    return Result;
}

TArray<FCombatResult> FExpeditionRun::PlayEnemyTurn()
{
    if (!bStarted || State.Phase != EExpeditionPhase::Combat) { return {}; }
    auto Results = Combat.PlayEnemyTurn();
    ResolveCombat();
    return Results;
}

void FExpeditionRun::ResolveCombat()
{
    const EBattlePhase Outcome = Combat.Snapshot().Phase;
    if (State.Phase != EExpeditionPhase::Combat || Outcome == EBattlePhase::Fighting) { return; }
    FExpeditionEncounterRecord& Record = State.History.Last();
    Record.Outcome = Outcome;
    Record.Survivors = CountSurvivors(Combat);
    if (Outcome == EBattlePhase::Defeat) { State.Phase = EExpeditionPhase::Defeat; return; }
    ++State.EncountersCompleted;
    if (State.EncountersCompleted < EncounterCount)
    {
        for (auto& Unit : Combat.State.Encounter.Combatants)
        {
            if (Unit.Side == ECombatSide::Party && Unit.Life == ECombatLife::Conscious) { ++Unit.Level; Unit.DamageBonus = FBuildCatalog::BasicDamage(*FBuildCatalog::Find(Unit.BuildId), Unit.Level); }
        }
    }
    State.bArtifactRecovered = State.EncountersCompleted == EncounterCount;
    State.Phase = State.bArtifactRecovered ? EExpeditionPhase::Victory : EExpeditionPhase::Reward;
}

bool FExpeditionRun::OffersUpgradeCards(ERuleset Ruleset, int32 Revision) { return Ruleset == ERuleset::Compact && Revision >= UpgradeRevision; }

bool FExpeditionRun::ClaimReward(int32 RecipientId, FString& Error) { return ClaimReward(RecipientId, ERewardCard::Gear, Error); }

bool FExpeditionRun::ClaimReward(int32 RecipientId, ERewardCard Card, FString& Error)
{
    Error.Reset();
    if (!bStarted || State.Phase != EExpeditionPhase::Reward)
    { Error = TEXT("Loot can be claimed once after each of the first four victories."); return false; }
    if (Card != ERewardCard::Gear && Card != ERewardCard::Reserve && Card != ERewardCard::Honed) { Error = TEXT("Choose Gear, Reserve or Honed."); return false; }
    const FCombatantState* Recipient = Combat.Find(RecipientId);
    if (!Recipient || Recipient->Side != ECombatSide::Party || Recipient->Life != ECombatLife::Conscious)
    { Error = TEXT("Select a conscious survivor for the victory reward."); return false; }
    if (Card != ERewardCard::Gear && !OffersUpgradeCards(State.Ruleset, State.CatalogRevision)) { Error = TEXT("Gear only in this run."); return false; }
    // The cards draw no random numbers; a rejection changes nothing. The candidate battle is installed only when the claim is whole.
    FCombatBattle Candidate = Combat;
    if (!FPartyContinuity::Reward(Candidate, RecipientId, State.History.Last().Route.RewardPotions, Card, Error)) { return false; }
    Combat = MoveTemp(Candidate);
    State.History.Last().RewardRecipient = RecipientId;
    State.History.Last().RewardCard = Card;
    if (State.EncountersCompleted == 1) { State.Phase = EExpeditionPhase::Recovery; }
    else { OfferRoutes(); }
    return true;
}

FString FExpeditionRun::RecoveryDescription() { return FPartyContinuity::RecoveryDescription(); }

bool FExpeditionRun::Recover(bool bTakeRecovery, FString& Error)
{
    Error.Reset();
    if (!bStarted || State.Phase != EExpeditionPhase::Recovery || State.bCampResolved)
    { Error = TEXT("Recovery is offered only once, after the first victory reward."); return false; }
    if (bTakeRecovery && !FPartyContinuity::Recover(Combat, Error)) { return false; }
    State.bCampResolved = true;
    State.bCampUsed = bTakeRecovery;
    OfferRoutes();
    return true;
}

bool FExpeditionRun::Restore(const FExpeditionSnapshot& Snapshot, FString& Error)
{
    Error.Reset();
    auto Reject = [&Error]() { Error = TEXT("Invalid saved expedition: phase, party, routes or history disagree."); return false; };
    const auto& P = Snapshot.Progress;
    if (P.CatalogRevision < FBuildCatalog::MinimumRevision || P.CatalogRevision > FBuildCatalog::Revision)
    {
        // An unknown rules interpretation is refused by name rather than reinterpreted; the file itself stays on disk.
        Error = FString::Printf(TEXT("This expedition was saved under rules revision %d; this build plays revisions %d to %d, so it cannot be resumed."),
            P.CatalogRevision, FBuildCatalog::MinimumRevision, FBuildCatalog::Revision);
        return false;
    }
    if ((P.Ruleset != ERuleset::Compact && P.Ruleset != ERuleset::SRD) || Snapshot.Battle.Encounter.Ruleset != P.Ruleset ||
        P.Party.Num() != 4 || P.RunId.IsEmpty() || P.RunId.Len() > 80 || static_cast<uint8>(P.Phase) > static_cast<uint8>(EExpeditionPhase::Defeat) ||
        P.EncountersCompleted < 0 || P.EncountersCompleted > EncounterCount || P.History.Num() > EncounterCount ||
        (P.bCampUsed && !P.bCampResolved) || P.bArtifactRecovered != (P.Phase == EExpeditionPhase::Victory)) { return Reject(); }
    for (int32 I = 0; I < 4; ++I)
    {
        const auto& Selection = P.Party[I];
        if (Selection.Id != I + 1 || Selection.Name.IsEmpty() || Selection.Name.Len() > 80 || !FBuildCatalog::Find(Selection.BuildId)) { return Reject(); }
    }
    FExpeditionRun Candidate;
    if (!Candidate.Combat.Restore(Snapshot.Battle, Error)) { return false; }
    Candidate.State = P;
    Candidate.bStarted = true;
    const bool bRoute = P.Phase == EExpeditionPhase::Route;
    const bool bCombat = P.Phase == EExpeditionPhase::Combat;
    const bool bDefeat = P.Phase == EExpeditionPhase::Defeat;
    const bool bReward = P.Phase == EExpeditionPhase::Reward;
    const bool bRecovery = P.Phase == EExpeditionPhase::Recovery;
    const bool bVictory = P.Phase == EExpeditionPhase::Victory;
    if ((bVictory ? P.EncountersCompleted != EncounterCount : P.EncountersCompleted >= EncounterCount) ||
        (bRecovery && P.EncountersCompleted != 1) || (bReward && P.EncountersCompleted < 1) ||
        P.History.Num() != P.EncountersCompleted + ((bCombat || bDefeat) ? 1 : 0) ||
        P.Offers.Num() != (bRoute ? 2 : 0) ||
        P.bCampResolved != (P.EncountersCompleted >= 1 && !bRecovery && !(bReward && P.EncountersCompleted == 1))) { return Reject(); }
    // Every retained route uses the same bounded recipe/preview facts as generation, under the run's own revision.
    const bool bIdentity = HasIdentity(P.Ruleset, P.CatalogRevision);
    const bool bUpgradeCards = OffersUpgradeCards(P.Ruleset, P.CatalogRevision);
    auto ValidRoute = [&P, bIdentity, bUpgradeCards](const FExpeditionRoute& R, int32 Stage, int32 Choice)
    {
        const bool bSupplies = Stage > 0 && P.History[Stage - 1].Route.bDangerous;
        if (R.EncounterSeed < 0 || R.EncounterSeed >= MAX_int32 || !AllowedRecipe(bIdentity, Stage, R.Recipe) ||
            R.Width < MinWidth || R.Width > MaxWidth || R.Height < MinHeight || R.Height > MaxHeight) { return false; }
        auto Expected = R;
        DescribeRoute(Expected, Stage, Choice, bSupplies, bIdentity, bUpgradeCards);
        return FExpeditionRoute::StaticStruct()->CompareScriptStruct(&R, &Expected, 0);
    };
    for (int32 I = 0; I < P.Offers.Num(); ++I) { if (!ValidRoute(P.Offers[I], P.EncountersCompleted, I)) { return Reject(); } }
    int32 PreviousSurvivors = 4;
    TArray<int32> DeathsByLevel;
    DeathsByLevel.Init(0, EncounterCount + 1);
    for (int32 I = 0; I < P.History.Num(); ++I)
    {
        const auto& H = P.History[I];
        const bool bUnfinished = I == P.History.Num() - 1 && bCombat;
        const bool bLost = I == P.History.Num() - 1 && bDefeat;
        const bool bLootPending = I == P.History.Num() - 1 && bReward;
        const bool bHasReward = I < EncounterCount - 1 && !bUnfinished && !bLost && !bLootPending;
        if (!ValidRoute(H.Route, I, H.Route.bDangerous ? 1 : 0) ||
            H.Outcome != (bUnfinished ? EBattlePhase::Fighting : bLost ? EBattlePhase::Defeat : EBattlePhase::Victory) ||
            H.Survivors < (bLost ? 0 : 1) || H.Survivors > PreviousSurvivors || (bLost && H.Survivors != 0) ||
            (bUnfinished && H.Survivors != PreviousSurvivors) ||
            (bHasReward ? H.RewardRecipient < 1 || H.RewardRecipient > 4 : H.RewardRecipient != INDEX_NONE) ||
            // A non-Gear card only on a rewarded record of a run that offers the cards (#218); a record without a reward holds Gear.
            (H.RewardCard != ERewardCard::Gear && (!bHasReward || !bUpgradeCards || (H.RewardCard != ERewardCard::Reserve && H.RewardCard != ERewardCard::Honed)))) { return Reject(); }
        if (!bUnfinished) { DeathsByLevel[I + 1] = PreviousSurvivors - H.Survivors; }
        PreviousSurvivors = H.Survivors;
    }
    const auto ExpectedPhase = bCombat || P.History.IsEmpty() ? EBattlePhase::Fighting : bDefeat ? EBattlePhase::Defeat : EBattlePhase::Victory;
    if (Snapshot.Battle.Phase != ExpectedPhase || (!P.History.IsEmpty() && !bCombat && CountSurvivors(Candidate.Combat) != P.History.Last().Survivors)) { return Reject(); }
    // Match immutable roster/recipe facts, while retaining exact live placement, fate,
    // resources, initiative and random state. No Start/NewRun is used to hydrate.
    FCombatEncounter Layout;
    if (P.History.IsEmpty()) { Layout = FCombatBattle::TutorialEncounter(); }
    else
    {
        Candidate.State.EncountersCompleted = P.History.Num() - 1;
        Layout = Candidate.Generate(P.History.Last().Route);
        Candidate.State.EncountersCompleted = P.EncountersCompleted;
    }
    const auto& Saved = Snapshot.Battle.Encounter;
    if (Saved.Width != Layout.Width || Saved.Height != Layout.Height || Saved.BlockedCells != Layout.BlockedCells || Saved.Combatants.Num() != Layout.Combatants.Num()) { return Reject(); }
    if (bCombat)
    {
        int32 Remaining = 0;
        for (const auto& U : Saved.Combatants) { if (U.Side == ECombatSide::Party && U.Life != ECombatLife::Dead) { ++Remaining; } }
        if (Remaining > P.History.Last().Survivors) { return Reject(); }
        DeathsByLevel[P.History.Num()] = P.History.Last().Survivors - Remaining;
    }
    for (int32 I = 0; I < Layout.Combatants.Num(); ++I)
    {
        const auto& U = Saved.Combatants[I]; auto Base = Layout.Combatants[I];
        if (U.Side != (I < 4 ? ECombatSide::Party : ECombatSide::Enemy)) { return Reject(); }
        if (U.Side == ECombatSide::Party)
        {
            const auto* Build = FBuildCatalog::Find(U.BuildId);
            if (!Build || !P.Party.IsValidIndex(I) || U.Id != P.Party[I].Id || U.Name != P.Party[I].Name || U.BuildId != P.Party[I].BuildId || U.ClassId != Build->Profile.ClassId || U.SubclassId != Build->Profile.SubclassId ||
                U.FighterSubclass != Build->Profile.FighterSubclass || !FBuildCatalog::ValidResources(U)) { return Reject(); }
            const auto Spawn = Base.Cell;
            Base = Build->Profile; Base.Id = U.Id; Base.Name = U.Name; Base.Cell = Spawn;
            // Names are instance values; detached combat validation bounds them.
            const int32 ExpectedLevel = FMath::Min(5, P.EncountersCompleted + 1);
            int32 MinimumLevel = 1;
            for (int32 Stage = 0; Stage < P.History.Num(); ++Stage)
            {
                const auto& Record = P.History[Stage];
                // Four survivors prove everyone reached this stage. A named loot
                // recipient also proves that adventurer earned its victory level.
                if (Record.Survivors == 4 || Record.RewardRecipient == U.Id)
                {
                    const int32 EarnedLevel = Stage + 1 + (Record.Outcome == EBattlePhase::Victory ? 1 : 0);
                    MinimumLevel = FMath::Max(MinimumLevel, FMath::Min(5, EarnedLevel));
                }
            }
            if (U.Level < MinimumLevel || U.Level > ExpectedLevel || (U.Life == ECombatLife::Dead && U.Level > FMath::Max(1, P.EncountersCompleted + ((bCombat || bDefeat) ? 1 : 0))) || (U.Life != ECombatLife::Dead && U.Level != ExpectedLevel)) { return Reject(); }
            // Each permanent loss belongs to the level of the encounter where it
            // occurred; matching the distribution also bounds unidentified deaths.
            if (U.Life == ECombatLife::Dead && --DeathsByLevel[U.Level] < 0) { return Reject(); }
            Base.DamageBonus = FBuildCatalog::BasicDamage(*Build, U.Level);
        }
        // SRD Start replaces the profile initiative bonus with the DEX modifier.
        if (Snapshot.Battle.Encounter.Ruleset == ERuleset::SRD)
        {
            const FAbilityScores Scores = Base.AdventurerRole != EAdventurerRole::None ? FAbilityRules::PrebuiltScores(Base.AdventurerRole) : FAbilityRules::EnemyScores(Base.Role);
            Base.InitiativeBonus = FAbilityRules::Modifier(Scores.Dexterity);
        }
        if (U.Id != Base.Id || U.Side != Base.Side || U.AdventurerRole != Base.AdventurerRole || U.Name != Base.Name || U.Role != Base.Role ||
            U.MaxHP != Base.MaxHP || U.ArmorClass != Base.ArmorClass || U.AttackBonus != Base.AttackBonus || U.DamageDie != Base.DamageDie ||
            U.DamageBonus != Base.DamageBonus || U.Range != Base.Range || U.Speed != Base.Speed || U.InitiativeBonus != Base.InitiativeBonus || U.bSneakWeapon != Base.bSneakWeapon ||
            (U.AdventurerRole != EAdventurerRole::Fighter && U.SecondWindUses != 0) ||
            (U.AdventurerRole != EAdventurerRole::Cleric && U.AdventurerRole != EAdventurerRole::Wizard && U.SpellSlots != 0) ||
            (U.Side == ECombatSide::Enemy && (!U.BuildId.IsEmpty() || !U.ClassId.IsEmpty() || !U.SubclassId.IsEmpty() || U.Level != 1 || U.FighterSubclass != EFighterSubclass::None || U.Potions != 0 || U.WeaponRank != 0))) { return Reject(); }
        if (U.Side == ECombatSide::Party)
        {
            // Every derived reward fact against history (#218): the gear rank counts Gear claims; Reserve and Honed are held exactly when
            // history holds that claim, at most once each. Dead heroes keep theirs. Every card supplies the route's potions.
            int32 Gear = 0, Reserves = 0, Honings = 0, SuppliedPotions = 1;
            for (const auto& H : P.History)
            {
                if (H.RewardRecipient != U.Id) { continue; }
                SuppliedPotions += H.Route.RewardPotions;
                Gear += H.RewardCard == ERewardCard::Gear ? 1 : 0; Reserves += H.RewardCard == ERewardCard::Reserve ? 1 : 0; Honings += H.RewardCard == ERewardCard::Honed ? 1 : 0;
            }
            if (U.WeaponRank != FMath::Min(3, Gear) || U.Potions > SuppliedPotions || Reserves > 1 || Honings > 1 || U.bReserve != (Reserves == 1) || U.bHoned != (Honings == 1)) { return Reject(); }
        }
        else if (U.bReserve || U.bHoned) { return Reject(); }
        if (P.History.IsEmpty() && (U.Cell != Base.Cell || U.HP != Base.HP || U.Life != Base.Life || U.Potions != Base.Potions ||
            U.KitUses != Base.KitUses || U.GuardTurns != 0 || U.GuardAmount != 0 || U.RageUses != Base.RageUses || U.RageTurns != 0 || U.GuardTargetId != INDEX_NONE || U.LastSurge != 0 ||
            U.ManeuverUses != (U.FighterSubclass == EFighterSubclass::BattleMaster ? 2 : 0) || U.WeaponRank != 0 || U.SpellSlots != Base.SpellSlots || U.SecondWindUses != Base.SecondWindUses)) { return Reject(); }
    }
    for (int32 Deaths : DeathsByLevel) { if (Deaths != 0) { return Reject(); } }
    if (P.History.IsEmpty() && (Snapshot.Battle.Round != 1 || !Snapshot.Battle.bActionAvailable || !Snapshot.Battle.bBonusActionAvailable ||
        Snapshot.Battle.TurnIndex != 0 || Snapshot.Battle.MovementLeft != Candidate.Combat.Find(Candidate.Combat.ActiveId())->Speed)) { return Reject(); }
    *this = MoveTemp(Candidate);
    return true;
}
