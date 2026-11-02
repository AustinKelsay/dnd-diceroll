#include "Combat/CombatBattle.h"
#include "Rules/SpellEffectRules.h"
#include "Rules/SpellTacticsRules.h"
#include "Rules/SpellUtilityRules.h"
#include "Party/BuildCatalog.h"
#include "Rules/SpellRules.h"
#include "Rules/MovementRules.h"
#include "Rules/OpportunityRules.h"
#include "Rules/ClassFeatureRules.h"
#include "Rules/WeaponRules.h"
#include "Rules/ArmorRules.h"
#include "Rules/MonsterRules.h"
#include "Rules/ProficiencyRules.h"
#include "Rules/SpellcastingRules.h"
#include "Rules/AreaRules.h"
#include "Rules/SpellCatalogRules.h"
#include "Rules/CheckRules.h"
#include "Rules/StandardActionRules.h"
#include "Rules/DeathSaveRules.h"
#include "Rules/ConcentrationRules.h"
#include "Rules/CantripRules.h"
#include "Rules/TwoWeaponRules.h"
#include "Rules/GrappleRules.h"
#include "Rules/SuperiorityRules.h"
#include "Rules/StealthRules.h"
#include "Rules/InspirationRules.h"
#include "Rules/LightRules.h"
#include "Rules/EncumbranceRules.h"
#include "Rules/CoinRules.h"
#include "Party/FeatureKit.h"
#include "Rules/WarlockRules.h"

namespace
{
int32 GridDistance(FIntPoint A, FIntPoint B)
{
    return FMath::Abs(A.X - B.X) + FMath::Abs(A.Y - B.Y);
}
using namespace BuildRules;
constexpr int32 PotionDice = 2;
constexpr int32 PotionDie = 4;
constexpr int32 PotionBonus = 2;
int32 CriticalThreshold(const FCombatantState& Hero)
{
    if (const auto* B = FBuildCatalog::Find(Hero.BuildId)) { return B->CriticalThreshold; }
    return Hero.FighterSubclass == EFighterSubclass::Champion ? 19 : 20;
}
const FIntPoint Neighbors[] = {{1, 0}, {0, 1}, {-1, 0}, {0, -1}};
int32 WeaponMaxDice(const FCombatantState& Unit)
{
    int32 Count = 1, Sides = 0;
    FCombatBattle::WeaponDice(Unit, Count, Sides);
    return Count * Sides;
}
const FWeaponProfile* MainWeapon(const FCombatantState& Unit)
{
    if (Unit.WildShapeHP > 0) { return nullptr; } // beast form: natural weapons only
    return Unit.MainHand >= 0 && Unit.MainHand < static_cast<int32>(EWeapon::Count) ? &FWeaponRules::Profile(static_cast<EWeapon>(Unit.MainHand)) : nullptr;
}
bool RangedAttacker(const FCombatantState& Unit)
{
    const FWeaponProfile* Weapon = MainWeapon(Unit);
    return Weapon ? Weapon->bRanged : Unit.Range > 1;
}
bool NeedsAmmunition(const FCombatantState& Unit)
{
    const FWeaponProfile* Weapon = MainWeapon(Unit);
    return Weapon && Weapon->Has(EWeaponProperty::Ammunition);
}
// Armor penalties apply to party adventurers wearing armor or a shield their class is not trained for.
int32 BaseSpeed(const FCombatBattle& Battle, const FCombatantState& Unit)
{
    if (!Battle.Snapshot().Encounter.bEncumbrance) { return Unit.Speed; }
    const int32 Pounds = Unit.CarriedPounds + Unit.Potions * FEncumbranceRules::Weight(EShopItem::PotionOfHealing) + (Unit.bShieldEquipped ? FEncumbranceRules::Weight(EShopItem::Shield) : 0);
    return FEncumbranceRules::SpeedCells(Unit.Speed, Pounds, Battle.AbilityScores(Unit.Id).Strength, true);
}

FArmorPenalties ArmorPenaltiesOf(const FCombatBattle& Battle, const FCombatantState& Unit)
{
    if (Unit.Side != ECombatSide::Party || Unit.AdventurerRole == EAdventurerRole::None || (Unit.ArmorWorn < 0 && !Unit.bShieldEquipped)) { return FArmorPenalties(); }
    return FArmorRules::Penalties(Unit.AdventurerRole, Unit.ArmorWorn < 0 ? EArmor::None : static_cast<EArmor>(Unit.ArmorWorn), Unit.bShieldEquipped,
        Battle.AbilityScores(Unit.Id).Get(EAbility::Strength));
}
// With a weapon: proficiency + the weapon's ability (finesse picks the better of STR/DEX) + rank. Legacy: AttackBonus.
/** SRD Extra Attack for heroes; Multiattack from the stat block for enemies. */
int32 AttacksPerAction(const FCombatantState& Unit)
{
    if (Unit.Side == ECombatSide::Enemy) { return FMonsterRules::AttacksPerAction(Unit.Role); }
    // SRD multiclassing: Extra Attack comes from class level and never stacks across classes.
    return FMath::Max(FClassFeatureRules::AttacksPerAction(Unit.AdventurerRole, FBuildCatalog::ClassLevel(Unit, Unit.AdventurerRole)),
        FClassFeatureRules::AttacksPerAction(Unit.SecondaryRole, FBuildCatalog::ClassLevel(Unit, Unit.SecondaryRole)));
}
/** Sorcerer Quickened Spell: a pending quicken turns the next kit spell into a bonus action. */
bool KitBonus(const FCombatantState& Unit, const FAbilityFacts& A) { return A.bBonus || (Unit.QuickenState == 1 && A.bKitResource); }
/** Pact of the Blade (SRD 5.2.1 invocation): the pact weapon may use CHA for attack and damage rolls; modelled for melee weapons. */
int32 PactAbilityModifier(const FCombatantState& Unit, const FWeaponProfile& Weapon, const FAbilityScores& Scores)
{
    const int32 Normal = FAbilityRules::Modifier(Scores.Get(FWeaponRules::AttackAbility(Weapon, Scores)));
    return !Weapon.bRanged && FFeatureKit::HasInvocation(Unit, EInvocation::PactOfTheBlade) ? FMath::Max(Normal, FAbilityRules::Modifier(Scores.Charisma)) : Normal;
}
int32 WeaponAttackBonus(const FCombatBattle& Battle, const FCombatantState& Unit)
{
    const FWeaponProfile* Weapon = MainWeapon(Unit);
    const int32 Sacred = Unit.SacredWeaponTurns > 0 ? FMath::Max(1, FAbilityRules::Modifier(Battle.AbilityScores(Unit.Id).Charisma)) : 0;
    if (!Weapon) { return (Unit.WildShapeHP > 0 ? FFeatureKit::WolfAttack : Unit.AttackBonus) + Sacred; }
    const FAbilityScores Scores = Battle.AbilityScores(Unit.Id);
    const int32 Archery = Weapon->bRanged && FFeatureKit::Style(Unit) == EFightingStyle::Archery ? 2 : 0;
    return FProficiencyRules::ProficiencyBonus(Unit.Level) + PactAbilityModifier(Unit, *Weapon, Scores) + Unit.WeaponRank + Archery + Sacred;
}
int32 WeaponDamageModifier(const FCombatBattle& Battle, const FCombatantState& Unit)
{
    const FWeaponProfile* Weapon = MainWeapon(Unit);
    if (!Weapon) { return Unit.WildShapeHP > 0 ? FFeatureKit::WolfDamageBonus : FBuildCatalog::AttackModifier(Unit); }
    const FAbilityScores Scores = Battle.AbilityScores(Unit.Id);
    const int32 Dueling = !Weapon->bRanged && !Weapon->Has(EWeaponProperty::TwoHanded) && Unit.OffHand < 0 && FFeatureKit::Style(Unit) == EFightingStyle::Dueling ? 2 : 0;
    return PactAbilityModifier(Unit, *Weapon, Scores) + Unit.WeaponRank + (Unit.RageTurns > 0 ? 2 : 0) + Dueling;
}
}

bool FCombatBattle::ValidateEncounter(const FCombatEncounter& Encounter, FString& Error)
{
    Error.Reset();
    auto Reject = [&Error](const TCHAR* Reason) { Error = Reason; return false; };
    if (Encounter.Width < 2 || Encounter.Height < 2 || Encounter.Width > 24 || Encounter.Height > 24 ||
        Encounter.Combatants.Num() < 2 || Encounter.Combatants.Num() > 32)
    {
        return Reject(TEXT("Encounter must have a 2..24 grid and 2..32 combatants."));
    }
    auto Inside = [&Encounter](FIntPoint Cell)
    {
        return Cell.X >= 0 && Cell.Y >= 0 && Cell.X < Encounter.Width && Cell.Y < Encounter.Height;
    };
    TSet<FIntPoint> Occupied;
    for (FIntPoint Cell : Encounter.BlockedCells)
    {
        if (!Inside(Cell) || Occupied.Contains(Cell)) { return Reject(TEXT("Invalid blocked cell.")); }
        Occupied.Add(Cell);
    }
    TSet<FIntPoint> Difficult;
    for (FIntPoint Cell : Encounter.DifficultCells)
    {
        if (!Inside(Cell) || Occupied.Contains(Cell) || Difficult.Contains(Cell)) { return Reject(TEXT("Invalid difficult terrain cell.")); }
        Difficult.Add(Cell);
    }
    TSet<int32> Ids;
    for (const FCombatantState& Unit : Encounter.Combatants)
    {
        if (Unit.Id < 0 || Ids.Contains(Unit.Id) || !Inside(Unit.Cell) || Occupied.Contains(Unit.Cell) ||
            Unit.Level < 1 || Unit.Level > 5 || Unit.ManeuverUses < 0 || Unit.ManeuverUses > 4 ||
            static_cast<uint8>(Unit.FighterSubclass) > static_cast<uint8>(EFighterSubclass::BattleMaster) ||
            (Unit.FighterSubclass != EFighterSubclass::BattleMaster && Unit.ManeuverUses != 0) ||
            (Unit.AdventurerRole != EAdventurerRole::Fighter && Unit.FighterSubclass != EFighterSubclass::None) ||
            Unit.MaxHP < 1 || Unit.MaxHP > 1000 || Unit.HP < 0 || Unit.HP > Unit.MaxHP ||
            Unit.ArmorClass < 1 || Unit.ArmorClass > 40 || Unit.AttackBonus < -10 || Unit.AttackBonus > 30 ||
            Unit.DamageDie < 1 || Unit.DamageDie > 100 || Unit.DamageBonus < 0 || Unit.DamageBonus > 100 ||
            Unit.Range < 1 || Unit.Range > 32 || Unit.Speed < 0 || Unit.Speed > 32 ||
            Unit.InitiativeBonus < -10 || Unit.InitiativeBonus > 30 ||
            static_cast<uint8>(Unit.AdventurerRole) > static_cast<uint8>(EAdventurerRole::Artificer) ||
            static_cast<uint8>(Unit.Life) > static_cast<uint8>(ECombatLife::Dead) ||
            (Unit.Life == ECombatLife::Conscious ? Unit.HP <= 0 || Unit.RescueTurnsLeft != 0 : Unit.HP != 0) ||
            (Unit.Life == ECombatLife::Downed && (Unit.Side != ECombatSide::Party || Unit.RescueTurnsLeft < 1 || Unit.RescueTurnsLeft > 3)) ||
            (Unit.Life == ECombatLife::Dead && Unit.RescueTurnsLeft != 0) ||
            Unit.DeathSaveSuccesses < 0 || Unit.DeathSaveSuccesses > 3 || Unit.DeathSaveFailures < 0 || Unit.DeathSaveFailures > 2 ||
            (Unit.Life != ECombatLife::Downed && (Unit.DeathSaveSuccesses != 0 || Unit.DeathSaveFailures != 0)) ||
            Unit.TemporaryHP < 0 || Unit.TemporaryHP > 999 || Unit.HitDiceSpent < 0 || Unit.HitDiceSpent > Unit.Level ||
            (Unit.bConcentrating && Unit.Life != ECombatLife::Conscious) ||
            Unit.SecondWindUses < 0 || Unit.SecondWindUses > 2 || Unit.SpellSlots < 0 || Unit.SpellSlots > 2 ||
            Unit.Potions < 0 || Unit.Potions > 9 || Unit.WeaponRank < 0 || Unit.WeaponRank > 3 ||
            (Unit.Side != ECombatSide::Party && Unit.Side != ECombatSide::Enemy))
        {
            return Reject(TEXT("Invalid combatant stats, ID, or occupied spawn."));
        }
        const auto* Build = FBuildCatalog::Find(Unit.BuildId);
        if (Build && !FBuildCatalog::ValidResources(Unit))
        { return Reject(TEXT("Resources do not belong to the selected build.")); }
        if (!FFeatureKit::Valid(Unit)) { return Reject(TEXT("Class feature state is not available to this hero.")); }
        if (!Unit.SlotsByLevel.IsEmpty())
        {
            bool bSlotsOk = (Unit.SlotsByLevel.Num() == 3 || Unit.SlotsByLevel.Num() == FSpellRules::MaxSpellLevel) && Unit.SlotsByLevel[0] == Unit.SpellSlots;
            for (int32 SlotLevel = 1; bSlotsOk && SlotLevel <= Unit.SlotsByLevel.Num(); ++SlotLevel)
            { bSlotsOk = Unit.SlotsByLevel[SlotLevel - 1] >= 0 && Unit.SlotsByLevel[SlotLevel - 1] <= FBuildCatalog::SlotMax(Unit, SlotLevel); }
            if (!bSlotsOk) { return Reject(TEXT("Spell slots do not match the per-level slot table.")); }
        }
        if (Unit.MainHand < -1 || Unit.MainHand >= static_cast<int32>(EWeapon::Count) || Unit.ArmorWorn < -1 || Unit.ArmorWorn >= static_cast<int32>(EArmor::Shield) ||
            (Unit.MainHand >= 0 && !FWeaponRules::CanWield(FWeaponRules::Profile(static_cast<EWeapon>(Unit.MainHand)),
                FWeaponRules::Profile(static_cast<EWeapon>(Unit.MainHand)).Has(EWeaponProperty::TwoHanded), Unit.bShieldEquipped)))
        { return Reject(TEXT("Invalid or unwieldable equipment.")); }
        if (Unit.Ammunition < 0 || Unit.Ammunition > 99) { return Reject(TEXT("Ammunition must be 0-99.")); }
        if (Unit.OffHand < -1 || Unit.OffHand >= static_cast<int32>(EWeapon::Count) || (Unit.OffHand >= 0 && (Unit.bShieldEquipped ||
            !FWeaponRules::Profile(static_cast<EWeapon>(Unit.OffHand)).Has(EWeaponProperty::Light))))
        { return Reject(TEXT("Off-hand weapon must be light and needs a free hand.")); }
        if (Unit.Size < 0 || Unit.Size > static_cast<int32>(ECreatureSize::Gargantuan)) { return Reject(TEXT("Invalid creature size.")); }
        const bool bBarbarian = Unit.AdventurerRole == EAdventurerRole::Barbarian && Unit.Side == ECombatSide::Party && Build && Build->Profile.AdventurerRole == EAdventurerRole::Barbarian;
        if (Build && Build->KitCap > 0 && (Unit.AdventurerRole != Build->Profile.AdventurerRole || Unit.Side != ECombatSide::Party))
        { return Reject(TEXT("Kit resources require their compatible party build.")); }
        if (static_cast<uint8>(Unit.AdventurerRole) >= static_cast<uint8>(EAdventurerRole::Monk) &&
            (!Build || Build->Profile.AdventurerRole != Unit.AdventurerRole || Unit.Side != ECombatSide::Party))
        { return Reject(TEXT("Martial role requires a compatible build.")); }
        if (Unit.KitUses < 0 || Unit.KitUses > (Build ? Build->KitCap : 0) || Unit.GuardTurns < 0 || Unit.GuardTurns > 1 || Unit.GuardAmount < 0 || Unit.GuardAmount > 4 ||
            ((Unit.GuardTurns > 0) != (Unit.GuardAmount > 0)) ||
            (Unit.GuardTurns > 0 && (Unit.Life != ECombatLife::Conscious || Unit.GuardTargetId == INDEX_NONE || !Build || Build->KitCap == 0 || Unit.KitUses == Build->KitCap)))
        { return Reject(TEXT("Invalid kit resource or protection duration.")); }
        if (Unit.GuardTurns > 0)
        {
            bool Compatible = false;
            for (const auto& A : FBuildCatalog::Abilities(Unit))
            { Compatible |= A.Effect == EAbilityEffect::Protect && A.Amount == Unit.GuardAmount && (A.Target == EAbilityTarget::Self ? Unit.GuardTargetId == Unit.Id : Unit.GuardTargetId != Unit.Id); }
            if (!Compatible) { return Reject(TEXT("Protection does not belong to build.")); }
        }
        if (Unit.AdventurerRole == EAdventurerRole::Barbarian && !bBarbarian) { return Reject(TEXT("Barbarian requires a compatible build.")); }
        if (Unit.RageUses < 0 || Unit.RageUses > (bBarbarian ? Build->RageCap : 0) || Unit.RageTurns < 0 || Unit.RageTurns > (bBarbarian ? 2 : 0) ||
            Unit.LastSurge < 0 || Unit.LastSurge > FBuildCatalog::SurgeCap(Unit) ||
            (Unit.RageTurns > 0 && (Unit.RageUses == Build->RageCap || Unit.Life != ECombatLife::Conscious)) ||
            (Unit.LastSurge > 0 && Unit.Life != ECombatLife::Conscious) ||
            (Unit.GuardTargetId != INDEX_NONE && Unit.GuardTurns == 0 && (Unit.BuildId != TEXT("barbarian.totem-warrior") || Unit.RageTurns == 0)))
        { return Reject(TEXT("Invalid owned ability state.")); }
        if ((Unit.Life != ECombatLife::Conscious && (Unit.bDodging || Unit.bDisengaged || Unit.bHidden || Unit.bReadied || Unit.HelpedById != INDEX_NONE)) ||
            (Unit.HelpedById != INDEX_NONE && !Encounter.Combatants.ContainsByPredicate([&](const auto& T) { return T.Id == Unit.HelpedById && T.Side != Unit.Side && T.Life == ECombatLife::Conscious; })))
        { return Reject(TEXT("Invalid standard action state.")); }
        if (Unit.GuardTargetId != INDEX_NONE)
        {
            const auto* Target = Encounter.Combatants.FindByPredicate([&](const auto& T) { return T.Id == Unit.GuardTargetId; });
            if (!Target || (Target->Id == Unit.Id && Unit.GuardTurns == 0) || Target->Side != Unit.Side || Target->Life != ECombatLife::Conscious) { return Reject(TEXT("Invalid protected ally.")); }
        }
        Ids.Add(Unit.Id);
        Occupied.Add(Unit.Cell);
    }
    for (const FCombatantState& Unit : Encounter.Combatants)
    {
        if (Unit.Conditions.Num() > 8 || (Unit.Life == ECombatLife::Dead && !Unit.Conditions.IsEmpty())) { return Reject(TEXT("Invalid condition list.")); }
        TSet<TPair<uint8, int32>> Seen;
        for (const FCombatCondition& Condition : Unit.Conditions)
        {
            const FCombatantState* Source = Condition.SourceId == INDEX_NONE ? nullptr :
                Encounter.Combatants.FindByPredicate([&Condition](const FCombatantState& S) { return S.Id == Condition.SourceId; });
            const TPair<uint8, int32> Key(static_cast<uint8>(Condition.Condition), Condition.SourceId);
            if (static_cast<uint8>(Condition.Condition) > static_cast<uint8>(ECombatCondition::Petrified) || Seen.Contains(Key) ||
                (Condition.SourceId != INDEX_NONE && (!Source || Source->Id == Unit.Id || Source->Life == ECombatLife::Dead)) ||
                (Condition.RoundsLeft != -1 && (Condition.RoundsLeft < 1 || Condition.RoundsLeft > 10)) ||
                (Condition.Condition == ECombatCondition::Grappled && (!Source || Source->Life != ECombatLife::Conscious || GridDistance(Source->Cell, Unit.Cell) > 1)))
            { return Reject(TEXT("Invalid condition, source, or duration.")); }
            Seen.Add(Key);
        }
    }
    return true;
}

bool FCombatBattle::Start(const FCombatEncounter& Encounter, int32 Seed, FString& Error)
{
    if (!ValidateEncounter(Encounter, Error)) { return false; }
    {
        FCombatBattle Probe; Probe.State.Encounter = Encounter;
        for (const FCombatantState& Unit : Encounter.Combatants) { if (!Probe.ValidPreparedSpells(Unit, Error)) { return false; } }
    }
    const bool bParty = Encounter.Combatants.ContainsByPredicate([](const auto& U) { return U.Side == ECombatSide::Party && U.Life == ECombatLife::Conscious; });
    const bool bEnemy = Encounter.Combatants.ContainsByPredicate([](const auto& U) { return U.Side == ECombatSide::Enemy && U.Life == ECombatLife::Conscious; });
    if (!bParty || !bEnemy) { Error = TEXT("Both sides need an active combatant."); return false; }
    State = FCombatSnapshot();
    State.Encounter = Encounter;
    State.RandomState = Seed;
    bStarted = true;
    for (FCombatantState& Unit : State.Encounter.Combatants)
    {
        Unit.Initiative = Roll(20) + Unit.InitiativeBonus + FFeatureKit::JackOfAllTrades(Unit);
        if (Unit.Life != ECombatLife::Dead) { State.TurnOrder.Add(Unit.Id); }
    }
    State.TurnOrder.Sort([this](int32 A, int32 B)
    {
        const int32 IA = Find(A)->Initiative;
        const int32 IB = Find(B)->Initiative;
        return IA == IB ? A < B : IA > IB;
    });
    if (State.Encounter.bPartyAmbush)
    {
        TArray<int32> Stealth;
        for (const FCombatantState& Unit : State.Encounter.Combatants)
        {
            if (Unit.Side == ECombatSide::Party && Unit.Life == ECombatLife::Conscious) { Stealth.Add(Roll(20) + Character(Unit.Id).SkillBonus(ESkill::Stealth)); }
        }
        for (FCombatantState& Unit : State.Encounter.Combatants)
        {
            Unit.bSurprised = Unit.Side == ECombatSide::Enemy && Unit.Life == ECombatLife::Conscious &&
                FStealthRules::IsSurprised(Stealth, FStealthRules::PassiveScore(Character(Unit.Id).SkillBonus(ESkill::Perception), ERollMode::Normal));
        }
    }
    if (FCombatantState* Opener = MutableFind(ActiveId()); Opener->bSurprised)
    {
        Opener->bSurprised = false;
        AdvanceTurn();
    }
    else if (FCombatantState* First = MutableFind(ActiveId()); First->Life == ECombatLife::Downed)
    {
        DownedTurn(*First);
        AdvanceTurn();
    }
    else { BeginTurn(); }
    PruneConditions();
    return true;
}

const FCombatantState* FCombatBattle::Find(int32 Id) const
{
    return State.Encounter.Combatants.FindByPredicate([Id](const FCombatantState& Unit) { return Unit.Id == Id; });
}

int32 FCombatBattle::ActiveId() const
{
    return bStarted && State.TurnOrder.IsValidIndex(State.TurnIndex) ? State.TurnOrder[State.TurnIndex] : INDEX_NONE;
}

int32 FCombatBattle::Roll(int32 Sides)
{
    FRandomStream Random(State.RandomState);
    const int32 Value = Random.RandRange(1, Sides);
    State.RandomState = Random.GetCurrentSeed();
    if (DiceLog) { DiceLog->Add({Sides, Value}); }
    return Value;
}

void FCombatBattle::WeaponDice(const FCombatantState& Unit, int32& OutCount, int32& OutSides)
{
    OutCount = 1; OutSides = Unit.DamageDie;
    if (Unit.WildShapeHP > 0) { OutCount = 2; OutSides = 4; return; } // wolf bite
    if (Unit.MainHand < 0 || Unit.MainHand >= static_cast<int32>(EWeapon::Count))
    {
        // #72: the Wizard's basic attack is the Fire Bolt cantrip, whose dice grow at character level 5.
        if (Unit.AdventurerRole == EAdventurerRole::Wizard && OutSides == FSpellRules::Facts(ESpell::FireBolt).DieSides)
        { OutCount = FCantripRules::DiceCount(ESpell::FireBolt, Unit.Level); }
        return;
    }
    const FWeaponProfile& Weapon = FWeaponRules::Profile(static_cast<EWeapon>(Unit.MainHand));
    FWeaponRules::DamageDice(Weapon, Weapon.Has(EWeaponProperty::TwoHanded) || (Weapon.Has(EWeaponProperty::Versatile) && !Unit.bShieldEquipped), false, OutCount, OutSides);
}

int32 FCombatBattle::ArmorClassBeforeSpells(int32 Id) const
{
    const FCombatantState* Unit = Find(Id);
    if (!Unit) { return 0; }
    if (Unit->WildShapeHP > 0) { return FFeatureKit::WolfAC; }
    const int32 Spell = (Unit->bShielded ? 5 : 0) + (Unit->ArmorWorn >= 0 && FFeatureKit::Style(*Unit) == EFightingStyle::Defense ? 1 : 0);
    // Mage Armor on a creature with a flat stat-block AC: 13 + DEX if better.
    if (Unit->ArmorWorn < 0 && SpellUnarmoredBonus(Id) > 0) { return Spell + FMath::Max(Unit->ArmorClass, SpellUnarmoredBonus(Id) + AbilityModifier(Id, EAbility::Dexterity)) + (Unit->bShieldEquipped ? FArmorRules::Profile(EArmor::Shield).BaseAC : 0); }
    if (Unit->ArmorWorn < 0) { return Spell + Unit->ArmorClass + (Unit->bShieldEquipped ? FArmorRules::Profile(EArmor::Shield).BaseAC : 0); }
    // SRD Unarmored Defense: Barbarian 1 adds CON with no armor (shield allowed); Monk 1 adds WIS with no armor and no shield.
    const FAbilityScores Scores = AbilityScores(Id);
    int32 Unarmored = 0;
    if (Unit->Side == ECombatSide::Party && Unit->ArmorWorn == static_cast<int32>(EArmor::None))
    {
        if (FBuildCatalog::ClassLevel(*Unit, EAdventurerRole::Barbarian) > 0) { Unarmored = FMath::Max(Unarmored, FAbilityRules::Modifier(Scores.Constitution)); }
        // Armor of Shadows: Mage Armor on self at will (13 + DEX unarmored).
        if (FFeatureKit::HasInvocation(*Unit, EInvocation::ArmorOfShadows)) { Unarmored = FMath::Max(Unarmored, 3); }
        Unarmored = FMath::Max(Unarmored, SpellUnarmoredBonus(Id) > 0 ? 3 : 0); // Mage Armor: 13 + DEX
        if (FBuildCatalog::ClassLevel(*Unit, EAdventurerRole::Monk) > 0 && !Unit->bShieldEquipped) { Unarmored = FMath::Max(Unarmored, FAbilityRules::Modifier(Scores.Wisdom)); }
    }
    return Spell + Unarmored + FArmorRules::ArmorClass(static_cast<EArmor>(Unit->ArmorWorn), Unit->bShieldEquipped, Scores.Get(EAbility::Dexterity));
}

namespace CombatSpellSlots
{
/** Requested level (0 = lowest open) if a slot of it is left, else 0. */
int32 CastSlotLevel(const FCombatantState& Unit, int32 Requested)
{
    for (int32 Level = FMath::Max(1, Requested); Level <= FSpellRules::MaxSpellLevel; ++Level)
    {
        const int32 Left = Level == 1 ? Unit.SpellSlots : Unit.SlotsByLevel.IsValidIndex(Level - 1) ? Unit.SlotsByLevel[Level - 1] : 0;
        if (Left > 0) { return Level; }
        if (Requested > 0) { return 0; }
    }
    return 0;
}
/** Extra dice an upcast adds over the spell's base count. */
int32 UpcastExtra(ESpell Spell, int32 Slot) { return FSpellRules::UpcastDiceCount(Spell, Slot) - FSpellRules::UpcastDiceCount(Spell, FSpellRules::Facts(Spell).Level); }
bool IsAreaSpell(ECombatCommandType Action) { return Action == ECombatCommandType::BurningHands || Action == ECombatCommandType::Thunderwave; }
ESpell AreaSpell(ECombatCommandType Action) { return Action == ECombatCommandType::BurningHands ? ESpell::BurningHands : ESpell::Thunderwave; }
/** Burning Hands: 15-ft cone; Thunderwave: 15-ft cube. Both start at the caster and aim at the chosen target. */
FAreaTemplate SpellArea(ECombatCommandType Action, FIntPoint Caster, FIntPoint Aim)
{
    FAreaTemplate Area;
    Area.Shape = Action == ECombatCommandType::BurningHands ? EAreaShape::Cone : EAreaShape::Cube;
    Area.Origin = Caster; Area.Direction = Aim - Caster; Area.SizeFeet = 15;
    return Area;
}
}

void FCombatBattle::SpendSpellSlot(FCombatantState& Unit)
{
    if (Unit.SpellSlots <= 0) { return; }
    --Unit.SpellSlots;
    if (!Unit.SlotsByLevel.IsEmpty()) { Unit.SlotsByLevel[0] = Unit.SpellSlots; }
}

int32 FCombatBattle::RollDice(int32 Count, int32 Sides)
{
    int32 Total = 0;
    for (int32 Index = 0; Index < Count; ++Index) { Total += Roll(Sides); }
    return Total;
}

FString FCombatBattle::DescribeAction(ECombatCommandType Action)
{
    switch (Action)
    {
    case ECombatCommandType::PrecisionStrike:
        return TEXT("SUPERIORITY DIE | Free with an attack + 1 die | Precision Attack: add a d8 superiority die to the attack roll; dice return on a rest");
    case ECombatCommandType::SecondWind:
        return FString::Printf(TEXT("SECOND WIND | Bonus + 1 use | self | d%d+fighter level healing | target self"), WindDie);
    case ECombatCommandType::CureWounds:
        return FString::Printf(TEXT("CURE WOUNDS | Action + 1 slot | ally/self, %d cell | %dd%d + WIS modifier healing (prebuilt cleric +%d)"), TouchRange, CureDice, CureDie, HealingModifier);
    case ECombatCommandType::MagicMissile:
        return FString::Printf(TEXT("MAGIC MISSILE | Action + 1 slot | enemy, %d cells + sight | %dd%d+%d auto-hit"), MissileRange, MissileDarts, MissileDie, MissileDarts * DartBonus);
    case ECombatCommandType::Rescue:
        return FString::Printf(TEXT("RESCUE | Action, no item | downed ally, %d cell | restore 1 HP"), TouchRange);
    case ECombatCommandType::Potion:
        return FString::Printf(TEXT("POTION (Use an Object) | Action + 1 bottle | ally/self, %d cell | %dd%d+%d healing"), TouchRange, PotionDice, PotionDie, PotionBonus);
    case ECombatCommandType::BurningHands:
        return TEXT("BURNING HANDS | Action + 1 slot | 15-ft cone from you | 3d6 fire, DEX save halves; +1d6 per slot above 1st");
    case ECombatCommandType::Thunderwave:
        return TEXT("THUNDERWAVE | Action + 1 slot | 15-ft cube from you | 2d8 thunder, CON save halves; fail pushes 10 ft; +1d8 per slot above 1st");
    case ECombatCommandType::Shove:
        return TEXT("SHOVE | Action | adjacent foe | Athletics vs Athletics/Acrobatics; win knocks prone");
    case ECombatCommandType::Grapple:
        return TEXT("GRAPPLE | Action | adjacent foe | Athletics vs Athletics/Acrobatics; win sets speed 0");
    case ECombatCommandType::Dash: return TEXT("DASH | Action (Rogue 2+: bonus) | self | adds your speed to this turn's movement");
    case ECombatCommandType::Disengage: return TEXT("DISENGAGE | Action (Rogue 2+: bonus) | self | your movement provokes no opportunity attacks this turn");
    case ECombatCommandType::Dodge: return TEXT("DODGE | Action | self | attacks you see have disadvantage; advantage on DEX saves until your next turn");
    case ECombatCommandType::Help: return TEXT("HELP | Action | adjacent foe | an ally's next attack on it has advantage, until your next turn");
    case ECombatCommandType::Hide: return TEXT("HIDE | Action (Rogue 2+: bonus) | self | Stealth vs best enemy passive Perception; hidden attacks with advantage");
    case ECombatCommandType::Ready: return TEXT("READY | Action | self | reaction: attack the first foe that ends a move in your range");
    case ECombatCommandType::OffHandAttack: return TEXT("OFF-HAND | Bonus | adjacent foe | after the Attack action with two light weapons; no ability bonus to damage");
    case ECombatCommandType::ActionSurge: return TEXT("ACTION SURGE | Free + 1 use | self | after spending your action, take one more action this turn; 1 use per rest");
    case ECombatCommandType::BardicInspiration: return TEXT("BARDIC INSPIRATION | Bonus + 1 use | ally within 60 ft | the ally holds a d6 (d8 at Bard 5), added to its next attack roll that would miss");
    case ECombatCommandType::WildShape: return TEXT("WILD SHAPE | Action + 1 use | self | become a wolf: 11 form HP take damage first, AC 13, bite +4 for 2d4+2, no spells; 2 uses per rest");
    case ECombatCommandType::FlurryOfBlows: return TEXT("FLURRY OF BLOWS | Bonus + 1 ki | adjacent foe | right after the Attack action, two more strikes");
    case ECombatCommandType::PatientDefense: return TEXT("PATIENT DEFENSE | Bonus + 1 ki | self | take the Dodge action");
    case ECombatCommandType::StepOfTheWind: return TEXT("STEP OF THE WIND | Bonus + 1 ki | self | take the Disengage action");
    case ECombatCommandType::Counterspell: return TEXT("COUNTERSPELL | Reaction + 1 spell slot | enemy caster within 60 ft in sight | automatic: an enemy spell (Sacred Flame) fails");
    case ECombatCommandType::DeflectMissiles: return TEXT("DEFLECT MISSILES | Reaction | self | automatic: when a ranged weapon attack hits you, reduce the damage by 1d10 + DEX + monk level");
    case ECombatCommandType::QuickenedSpell: return TEXT("QUICKENED SPELL | Free + 2 sorcery points | self | the next class spell you cast this turn costs your bonus action instead of your action");
    case ECombatCommandType::FlexibleCasting: return TEXT("FLEXIBLE CASTING | Bonus + 1 spell use | self | convert an expended spell slot into 2 sorcery points (up to your sorcerer level)");
    case ECombatCommandType::SacredWeapon: return TEXT("SACRED WEAPON | Action + Channel Divinity | self | for 1 minute (10 rounds) add your CHA modifier (min +1) to weapon attack rolls; 1 use per rest");
    case ECombatCommandType::MartialArts: return TEXT("MARTIAL ARTS | Bonus | adjacent foe | right after the Attack action, one unarmed strike (no ki)");
    case ECombatCommandType::RecklessAttack: return TEXT("RECKLESS ATTACK | Free | self | before your first attack this turn: advantage on melee weapon attacks this turn, but attacks against you have advantage until your next turn");
    case ECombatCommandType::UncannyDodge: return TEXT("UNCANNY DODGE | Reaction | self | automatic: when an attacker you can see hits you, halve the damage");
    case ECombatCommandType::FontOfMagic: return TEXT("FONT OF MAGIC | Bonus + 2 sorcery points | self | regain one spell use (a 1st-level slot); points = Sorcerer level per long rest");
    case ECombatCommandType::TurnUndead: return TEXT("TURN UNDEAD | Action + Channel Divinity | undead within 30 ft | WIS save vs your spell DC or frightened of you for 1 minute; 1 use per rest");
    default:
        return FString::Printf(TEXT("ATTACK / MOVE | Attack costs Action | Rogue: +d%d with conscious ally by target; Wizard: Fire Bolt"), SneakDie);
    }
}

FString FCombatBattle::CostTag(ECombatCommandType Action)
{
    TArray<FString> Fields;
    DescribeAction(Action).ParseIntoArray(Fields, TEXT(" | "));
    return Fields.Num() > 1 ? Fields[1] : FString();
}

FString FCombatBattle::UsageTag(ECombatCommandType Action, const FCombatantState& Unit)
{
    switch (Action)
    {
    case ECombatCommandType::SecondWind:
        return FString::Printf(TEXT("bonus, %d left"), Unit.SecondWindUses);
    case ECombatCommandType::CureWounds:
    case ECombatCommandType::MagicMissile:
    case ECombatCommandType::BurningHands:
    case ECombatCommandType::Thunderwave:
        return FString::Printf(TEXT("%d slot%s left"), Unit.SpellSlots, Unit.SpellSlots == 1 ? TEXT("") : TEXT("s"));
    case ECombatCommandType::Potion:
        return FString::Printf(TEXT("%d left"), Unit.Potions);
    case ECombatCommandType::PrecisionStrike:
        return FString::Printf(TEXT("+d8 to hit, %d dice left"), Unit.ManeuverUses);
    default:
        return FFeatureKit::IsFeature(Action) ? FFeatureKit::Usage(Action, Unit) : FString();
    }
}

EActionCost FCombatBattle::ActionCost(ECombatCommandType Action, const FCombatantState& Unit, bool bActionAvailable)
{
    switch (Action)
    {
    case ECombatCommandType::Move:
    case ECombatCommandType::EndTurn:
    case ECombatCommandType::PrecisionStrike: return EActionCost::Free; // movement, ending a turn; a maneuver rides on an attack
    case ECombatCommandType::SecondWind:
    case ECombatCommandType::OffHandAttack: return EActionCost::BonusAction;
    case ECombatCommandType::Special1:
    case ECombatCommandType::Special2:
        for (const auto& A : FBuildCatalog::Abilities(Unit)) { if (A.Command == Action) { return A.Cost(); } }
        return EActionCost::Action;
    case ECombatCommandType::Dash: return FStandardActionRules::Cost(EStandardAction::Dash, EAdventurerRole::Rogue, FBuildCatalog::ClassLevel(Unit, EAdventurerRole::Rogue), !bActionAvailable);
    case ECombatCommandType::Disengage: return FStandardActionRules::Cost(EStandardAction::Disengage, EAdventurerRole::Rogue, FBuildCatalog::ClassLevel(Unit, EAdventurerRole::Rogue), !bActionAvailable);
    case ECombatCommandType::Hide: return FStandardActionRules::Cost(EStandardAction::Hide, EAdventurerRole::Rogue, FBuildCatalog::ClassLevel(Unit, EAdventurerRole::Rogue), !bActionAvailable);
    default: return FFeatureKit::IsFeature(Action) ? FFeatureKit::Cost(Action) : EActionCost::Action;
    }
}

FString FCombatBattle::CostPrefix(ECombatCommandType Action, const FCombatantState& Unit, bool bActionAvailable)
{
    return FString::Printf(TEXT("[%s] "), FStandardActionRules::CostLabel(ActionCost(Action, Unit, bActionAvailable)));
}

FString FCombatBattle::SpellTag(ECombatCommandType Action)
{
    switch (Action)
    {
    case ECombatCommandType::CureWounds: return FSpellRules::Summary(ESpell::CureWounds);
    case ECombatCommandType::MagicMissile: return FSpellRules::Summary(ESpell::MagicMissile);
    case ECombatCommandType::BurningHands: return FSpellRules::Summary(ESpell::BurningHands);
    case ECombatCommandType::Thunderwave: return FSpellRules::Summary(ESpell::Thunderwave);
    default: return FString();
    }
}

bool FCombatBattle::IsInside(FIntPoint Cell) const
{
    return Cell.X >= 0 && Cell.Y >= 0 && Cell.X < State.Encounter.Width && Cell.Y < State.Encounter.Height;
}

bool FCombatBattle::IsOpen(FIntPoint Cell, int32 MovingId) const
{
    return IsInside(Cell) && !State.Encounter.BlockedCells.Contains(Cell) &&
        !State.Encounter.Combatants.ContainsByPredicate([Cell, MovingId](const FCombatantState& Unit)
        {
            // Downed and dead bodies occupy their cells until the encounter transition.
            return Unit.Id != MovingId && Unit.Cell == Cell;
        });
}

TMap<FIntPoint, int32> FCombatBattle::Distances(int32 Id, TMap<FIntPoint, FIntPoint>* Parents) const
{
    TMap<FIntPoint, int32> Result;
    const FCombatantState* Unit = Find(Id);
    if (!Unit) { return Result; }
    // Deterministic Dijkstra with FIFO cost buckets. With uniform step costs it visits cells and picks
    // parents in exactly the order of the earlier breadth-first search, so existing routes are unchanged.
    TArray<TArray<FIntPoint>> Buckets;
    Buckets.SetNum(1);
    Buckets[0].Add(Unit->Cell);
    Result.Add(Unit->Cell, 0);
    // SRD Frightened: no willing step closer to a visible source of fear.
    TArray<FIntPoint> Fears;
    for (const FCombatCondition& C : Unit->Conditions)
    {
        const FCombatantState* Source = Find(C.SourceId);
        if (C.Condition == ECombatCondition::Frightened && Source && Source->Life == ECombatLife::Conscious && HasLineOfSight(Unit->Cell, Source->Cell)) { Fears.Add(Source->Cell); }
    }
    for (int32 Cost = 0; Cost < Buckets.Num(); ++Cost)
    {
        for (int32 Index = 0; Index < Buckets[Cost].Num(); ++Index)
        {
            const FIntPoint Cell = Buckets[Cost][Index];
            if (Result[Cell] != Cost) { continue; }
            for (FIntPoint Offset : Neighbors)
            {
                const FIntPoint Next = Cell + Offset;
                if (!IsOpen(Next, Id)) { continue; }
                if (Fears.ContainsByPredicate([&](const FIntPoint& F) { return GridDistance(Next, F) < GridDistance(Cell, F); })) { continue; }
                const int32 NextCost = Cost + StepCost(Next);
                if (const int32* Known = Result.Find(Next); Known && *Known <= NextCost) { continue; }
                Result.Add(Next, NextCost);
                if (Parents) { Parents->Add(Next, Cell); }
                if (Buckets.Num() <= NextCost) { Buckets.SetNum(NextCost + 1); }
                Buckets[NextCost].Add(Next);
            }
        }
    }
    return Result;
}

TArray<int32> FCombatBattle::OpportunityThreats(int32 MoverId, const TArray<FIntPoint>& Path) const
{
    TArray<int32> Threats;
    const FCombatantState* Mover = Find(MoverId);
    if (!Mover) { return Threats; }
    FIntPoint From = Mover->Cell;
    for (const FIntPoint& Next : Path)
    {
        for (const FCombatantState& Reactor : State.Encounter.Combatants)
        {
            if (Reactor.Side == Mover->Side || Reactor.Life != ECombatLife::Conscious || IsIncapacitated(Reactor) || RangedAttacker(Reactor) || Threats.Contains(Reactor.Id) ||
                !FOpportunityRules::StepProvokes(Reactor.Cell, ActionRange(Reactor, ECombatCommandType::Attack), From, Next, Mover->bDisengaged, false, Reactor.bReactionAvailable)) { continue; }
            Threats.Add(Reactor.Id);
        }
        From = Next;
    }
    return Threats;
}

const TCHAR* FCombatBattle::ConditionName(ECombatCondition Condition)
{
    switch (Condition)
    {
    case ECombatCondition::Prone: return TEXT("Prone");
    case ECombatCondition::Poisoned: return TEXT("Poisoned");
    case ECombatCondition::Blinded: return TEXT("Blinded");
    case ECombatCondition::Frightened: return TEXT("Frightened");
    case ECombatCondition::Restrained: return TEXT("Restrained");
    case ECombatCondition::Grappled: return TEXT("Grappled");
    case ECombatCondition::Incapacitated: return TEXT("Incapacitated");
    case ECombatCondition::Stunned: return TEXT("Stunned");
    case ECombatCondition::Paralyzed: return TEXT("Paralyzed");
    case ECombatCondition::Unconscious: return TEXT("Unconscious");
    case ECombatCondition::Charmed: return TEXT("Charmed");
    case ECombatCondition::Deafened: return TEXT("Deafened");
    case ECombatCondition::Invisible: return TEXT("Invisible");
    case ECombatCondition::Petrified: return TEXT("Petrified");
    default: return TEXT("Condition");
    }
}

FString FCombatBattle::StatusText(int32 Id) const
{
    TArray<FString> Parts;
    if (const FCombatantState* Unit = Find(Id))
    {
        for (const FCombatCondition& C : Unit->Conditions)
        {
            Parts.Add(C.RoundsLeft > 0 ? FString::Printf(TEXT("%s (%d round%s)"), ConditionName(C.Condition), C.RoundsLeft, C.RoundsLeft == 1 ? TEXT("") : TEXT("s")) : FString(ConditionName(C.Condition)));
        }
    }
    return FString::Join(Parts, TEXT(", "));
}

FString FCombatBattle::ArmorClassText(int32 Id) const
{
    const FCombatantState* Unit = Find(Id);
    if (!Unit) { return FString(); }
    TArray<FString> Sources;
    if (Unit->ArmorWorn > static_cast<int32>(EArmor::None) && Unit->ArmorWorn < static_cast<int32>(EArmor::Shield)) { Sources.Add(FArmorRules::Profile(static_cast<EArmor>(Unit->ArmorWorn)).Name); }
    if (Unit->bShieldEquipped) { Sources.Add(TEXT("shield")); }
    const FString Base = FString::Printf(TEXT("AC %d"), EffectiveArmorClass(Id));
    return Sources.IsEmpty() ? Base : FString::Printf(TEXT("%s (%s)"), *Base, *FString::Join(Sources, TEXT(", ")));
}

int32 FCombatBattle::StepCost(FIntPoint Cell) const
{
    if (State.Encounter.DifficultCells.Contains(Cell)) { return 2; }
    // Spike Growth, Black Tentacles: the zone is difficult terrain while it lasts.
    for (const FActiveSpell& A : State.ActiveSpells)
    {
        const FSpellZone* Zone = FSpellTacticsRules::Zone(A.SpellId);
        if (Zone && Zone->bDifficult && ZoneCovers(A, Cell)) { return A.SpellId.Equals(TEXT("plant-growth"), ESearchCase::IgnoreCase) ? 4 : 2; }
    }
    return 1;
}

TArray<FIntPoint> FCombatBattle::LegalMovesRaw(int32 Id) const
{
    TArray<FIntPoint> Result;
    if (!bStarted || State.Phase != EBattlePhase::Fighting || ActiveId() != Id) { return Result; }
    for (const auto& Entry : Distances(Id))
    {
        if (Entry.Value > 0 && Entry.Value <= State.MovementLeft) { Result.Add(Entry.Key); }
    }
    return Result;
}

bool FCombatBattle::HasLineOfSight(FIntPoint From, FIntPoint To) const
{
    // Supercover grid traversal. At a corner both touching cells must be clear.
    const FIntPoint Delta = To - From;
    const int32 NX = FMath::Abs(Delta.X);
    const int32 NY = FMath::Abs(Delta.Y);
    const int32 SX = FMath::Sign(Delta.X);
    const int32 SY = FMath::Sign(Delta.Y);
    int32 IX = 0;
    int32 IY = 0;
    auto Blocked = [this](FIntPoint Cell) { return State.Encounter.BlockedCells.Contains(Cell); };
    while (IX < NX || IY < NY)
    {
        const int32 Decision = (1 + 2 * IX) * NY - (1 + 2 * IY) * NX;
        if (Decision == 0)
        {
            if (Blocked(From + FIntPoint(SX, 0)) || Blocked(From + FIntPoint(0, SY))) { return false; }
            From += FIntPoint(SX, SY);
            ++IX; ++IY;
        }
        else if (Decision < 0) { From.X += SX; ++IX; }
        else { From.Y += SY; ++IY; }
        if (Blocked(From)) { return false; }
    }
    return true;
}

bool FCombatBattle::CanTargetFrom(const FCombatantState& Attacker, FIntPoint Origin, const FCombatantState& Target) const
{
    return Target.Side != Attacker.Side && Target.HP > 0 && GridDistance(Origin, Target.Cell) <= ActionRange(Attacker, ECombatCommandType::Attack) &&
        HasLineOfSight(Origin, Target.Cell);
}

namespace
{
/** Battle command to SRD standard action (#60). Ready maps to Attack, which is never a bonus action. */
bool StandardActionOf(ECombatCommandType Action, EStandardAction& Out)
{
    switch (Action)
    {
    case ECombatCommandType::Dash: Out = EStandardAction::Dash; return true;
    case ECombatCommandType::Disengage: Out = EStandardAction::Disengage; return true;
    case ECombatCommandType::Dodge: Out = EStandardAction::Dodge; return true;
    case ECombatCommandType::Help: Out = EStandardAction::Help; return true;
    case ECombatCommandType::Hide: Out = EStandardAction::Hide; return true;
    case ECombatCommandType::Ready: Out = EStandardAction::Attack; return true;
    default: return false;
    }
}
}

const FCombatantState* FCombatBattle::ActionUser(int32 Id, ECombatCommandType Action) const
{
    const FCombatantState* Unit = Find(Id);
    if (!bStarted || State.Phase != EBattlePhase::Fighting || ActiveId() != Id || !Unit || Unit->Life != ECombatLife::Conscious) { return nullptr; }
    // Extra Attack (#61): after the Attack action, banked attacks still allow Attack and Precision Strike.
    const bool bWeaponAttack = Action == ECombatCommandType::Attack || Action == ECombatCommandType::PrecisionStrike;
    if (IsIncapacitated(*Unit)) { return nullptr; }
    if (FFeatureKit::IsFeature(Action)) { return FeatureReady(*Unit, Action) ? Unit : nullptr; }
    if (Unit->WildShapeHP > 0 && !SpellTag(Action).IsEmpty()) { return nullptr; } // beasts cannot cast spells
    if (Action == ECombatCommandType::OffHandAttack)
    {
        const FWeaponProfile* Main = MainWeapon(*Unit);
        const bool bOffHand = Unit->OffHand >= 0 && Unit->OffHand < static_cast<int32>(EWeapon::Count);
        return Main && bOffHand && FTwoWeaponRules::CanOffHandAttack(*Main, FWeaponRules::Profile(static_cast<EWeapon>(Unit->OffHand)), State.bAttackActionTaken, !State.bBonusActionAvailable) ? Unit : nullptr;
    }
    if (EStandardAction Standard; StandardActionOf(Action, Standard))
    {
        return State.bActionAvailable || (State.bBonusActionAvailable && FStandardActionRules::CanTakeAsBonusAction(Standard, Unit->AdventurerRole, Unit->Level)) ? Unit : nullptr;
    }
    if (!State.bActionAvailable && !(bWeaponAttack && State.AttacksRemaining > 0)) { return nullptr; }
    if (Action == ECombatCommandType::PrecisionStrike && (!FBuildCatalog::HasSpecial(*Unit, Action) || Unit->ManeuverUses <= 0)) { return nullptr; }
    if (Action == ECombatCommandType::CureWounds && (!FBuildCatalog::HasSpecial(*Unit, Action) || CombatSpellSlots::CastSlotLevel(*Unit, 0) == 0)) { return nullptr; }
    if (Action == ECombatCommandType::MagicMissile && (!FBuildCatalog::HasSpecial(*Unit, Action) || CombatSpellSlots::CastSlotLevel(*Unit, 0) == 0)) { return nullptr; }
    if (CombatSpellSlots::IsAreaSpell(Action) && (Unit->Side != ECombatSide::Party || !FSpellRules::OnClassList(Unit->AdventurerRole, CombatSpellSlots::AreaSpell(Action)) || CombatSpellSlots::CastSlotLevel(*Unit, 0) == 0)) { return nullptr; }
    if (Action == ECombatCommandType::MagicMissile) { if (const auto* B = FBuildCatalog::Find(Unit->BuildId); B && Unit->KitUses < B->MissileKitCost) { return nullptr; } }
    if (Action == ECombatCommandType::Potion && Unit->Potions <= 0) { return nullptr; }
    if ((Action == ECombatCommandType::Shove || Action == ECombatCommandType::Grapple) && Unit->Side != ECombatSide::Party) { return nullptr; }
    if (bWeaponAttack && NeedsAmmunition(*Unit) && Unit->Ammunition <= 0) { return nullptr; }
    if ((Action == ECombatCommandType::CureWounds || Action == ECombatCommandType::MagicMissile || CombatSpellSlots::IsAreaSpell(Action)) && (ArmorPenaltiesOf(*this, *Unit).bCannotCastSpells || State.bSlotSpentThisTurn)) { return nullptr; }
    return Unit;
}

int32 FCombatBattle::ActionRange(const FCombatantState& Unit, ECombatCommandType Action)
{
    switch (Action)
    {
    case ECombatCommandType::Attack: case ECombatCommandType::PrecisionStrike:
        if (const FWeaponProfile* Weapon = MainWeapon(Unit)) { return Weapon->bRanged ? Weapon->LongFeet / FWeaponRules::FeetPerCell : FWeaponRules::ReachCells(*Weapon); }
        return Unit.WildShapeHP > 0 ? 1 : Unit.Range;
    case ECombatCommandType::MagicMissile: return MissileRange;
    case ECombatCommandType::CureWounds: case ECombatCommandType::Rescue: case ECombatCommandType::Potion: return Unit.Side == ECombatSide::Party ? TouchRange : 0;
    default: return 0;
    }
}

FActionReach FCombatBattle::Reach(int32 Id, ECombatCommandType Action) const
{
    FActionReach Result;
    const FCombatantState* Unit = Find(Id);
    if (!Unit) { return Result; }
    int32 Range = 0;
    bool bSelf = false;
    const auto Abilities = FBuildCatalog::Abilities(*Unit);
    const bool bCatalog = Action == ECombatCommandType::Special1 || Action == ECombatCommandType::Special2
        || (!Unit->BuildId.IsEmpty() && Abilities.ContainsByPredicate([Action](const FAbilityFacts& A) { return A.Command == Action; }));
    if (bCatalog)
    {
        // Catalog abilities carry their own budget, range and target kind (same gates as LegalTargets).
        if (!bStarted || State.Phase != EBattlePhase::Fighting || ActiveId() != Id || Unit->Life != ECombatLife::Conscious) { return Result; }
        const FAbilityFacts* Ability = nullptr;
        for (const auto& A : Abilities)
        {
            if (A.Command == Action && A.bResourceAvailable && (KitBonus(*Unit, A) ? State.bBonusActionAvailable : State.bActionAvailable)) { Ability = &A; break; }
        }
        if (!Ability) { return Result; }
        if (Ability->Target == EAbilityTarget::Self) { Result.InSight.Add(Unit->Cell); return Result; }
        Range = Ability->Range;
        bSelf = Ability->Target == EAbilityTarget::Friendly;
    }
    else
    {
        if (!ActionUser(Id, Action)) { return Result; }
        Range = ActionRange(*Unit, Action);
        // Friendly touch actions may target the user's own cell; hostile ones never can.
        bSelf = Action == ECombatCommandType::CureWounds || Action == ECombatCommandType::Rescue || Action == ECombatCommandType::Potion;
    }
    if (Range <= 0) { return Result; }
    for (int32 Y = 0; Y < State.Encounter.Height; ++Y)
    {
        for (int32 X = 0; X < State.Encounter.Width; ++X)
        {
            const FIntPoint Cell(X, Y);
            const int32 Steps = GridDistance(Unit->Cell, Cell);
            if (Steps > Range || (Steps == 0 && !bSelf)) { continue; }
            (!State.Encounter.BlockedCells.Contains(Cell) && HasLineOfSight(Unit->Cell, Cell) ? Result.InSight : Result.Obscured).Add(Cell);
        }
    }
    return Result;
}

TArray<int32> FCombatBattle::LegalTargetsRaw(int32 Id, ECombatCommandType Action) const
{
    TArray<int32> Result;
    const FCombatantState* Unit = Find(Id);
    if (!bStarted || State.Phase != EBattlePhase::Fighting || ActiveId() != Id || !Unit || Unit->Life != ECombatLife::Conscious) { return Result; }
    if (FFeatureKit::IsFeature(Action)) { return ActionUser(Id, Action) ? FeatureTargets(*Unit, Action) : Result; }
    if (Action == ECombatCommandType::Special1 || Action == ECombatCommandType::Special2 || (Action == ECombatCommandType::CureWounds && !Unit->BuildId.IsEmpty()))
    {
        for (const auto& A : FBuildCatalog::Abilities(*Unit))
        {
            if (A.Command != Action || !A.bResourceAvailable || !(KitBonus(*Unit, A) ? State.bBonusActionAvailable : State.bActionAvailable)) { continue; }
            for (const auto& T : State.Encounter.Combatants)
            {
                const bool Kind = A.Target == EAbilityTarget::Self ? T.Id == Id : A.Target == EAbilityTarget::Enemy ? T.Side != Unit->Side : T.Side == Unit->Side && (A.Target == EAbilityTarget::Friendly || T.Id != Id);
                if (Kind && (A.Effect != EAbilityEffect::Healing || T.HP < T.MaxHP) && T.Life == ECombatLife::Conscious && GridDistance(Unit->Cell, T.Cell) <= A.Range && HasLineOfSight(Unit->Cell, T.Cell)) { Result.Add(T.Id); }
            }
        }
        return Result;
    }
    if (Action == ECombatCommandType::SecondWind)
    {
        if (FBuildCatalog::HasSpecial(*Unit, Action) && Unit->SecondWindUses > 0 && State.bBonusActionAvailable && Unit->HP < Unit->MaxHP) { Result.Add(Id); }
        return Result;
    }
    if (!ActionUser(Id, Action)) { return Result; }
    for (const FCombatantState& Target : State.Encounter.Combatants)
    {
        bool bLegal = false;
        if ((Action == ECombatCommandType::Attack || Action == ECombatCommandType::PrecisionStrike)) { bLegal = CanTargetFrom(*Unit, Unit->Cell, Target); }
        else if (Action == ECombatCommandType::OffHandAttack) { bLegal = CanTargetFrom(*Unit, Unit->Cell, Target) && GridDistance(Unit->Cell, Target.Cell) <= 1; }
        else if (Action == ECombatCommandType::MagicMissile)
        {
            bLegal = Target.Side != Unit->Side && Target.Life == ECombatLife::Conscious && GridDistance(Unit->Cell, Target.Cell) <= MissileRange && HasLineOfSight(Unit->Cell, Target.Cell);
        }
        else if (Action == ECombatCommandType::Shove || Action == ECombatCommandType::Grapple)
        {
            const ECombatCondition Applies = Action == ECombatCommandType::Shove ? ECombatCondition::Prone : ECombatCondition::Grappled;
            bLegal = Target.Side != Unit->Side && Target.Life == ECombatLife::Conscious &&
                FGrappleRules::SizeAllows(static_cast<ECreatureSize>(Unit->Size), static_cast<ECreatureSize>(Target.Size)) &&
                FMath::Max(FMath::Abs(Target.Cell.X - Unit->Cell.X), FMath::Abs(Target.Cell.Y - Unit->Cell.Y)) <= 1 &&
                !Target.Conditions.ContainsByPredicate([Applies](const FCombatCondition& C) { return C.Condition == Applies; });
        }
        else if (EStandardAction Standard; StandardActionOf(Action, Standard))
        {
            bLegal = Action == ECombatCommandType::Help ? Target.Side != Unit->Side && Target.Life == ECombatLife::Conscious && GridDistance(Unit->Cell, Target.Cell) <= 1 : Target.Id == Unit->Id;
        }
        else if (CombatSpellSlots::IsAreaSpell(Action))
        {
            bLegal = Target.Side != Unit->Side && Target.Life == ECombatLife::Conscious && FAreaRules::Covers(CombatSpellSlots::SpellArea(Action, Unit->Cell, Target.Cell), Target.Cell) && HasLineOfSight(Unit->Cell, Target.Cell);
        }
        else if (Action == ECombatCommandType::CureWounds || Action == ECombatCommandType::Rescue || Action == ECombatCommandType::Potion)
        {
            bLegal = Unit->Side == ECombatSide::Party && Target.Side == Unit->Side && Target.Life != ECombatLife::Dead && Target.HP < Target.MaxHP &&
                GridDistance(Unit->Cell, Target.Cell) <= TouchRange && HasLineOfSight(Unit->Cell, Target.Cell) &&
                (Action != ECombatCommandType::Rescue || Target.Life == ECombatLife::Downed) &&
                (Action != ECombatCommandType::CureWounds || Target.Life == ECombatLife::Conscious);
        }
        if (bLegal) { Result.Add(Target.Id); }
    }
    return Result;
}

TArray<FIntPoint> FCombatBattle::PathToRaw(int32 Id, FIntPoint Destination) const
{
    // Same legality as LegalMoves, with one search that also yields the route.
    TArray<FIntPoint> Path;
    const FCombatantState* Unit = Find(Id);
    if (!bStarted || State.Phase != EBattlePhase::Fighting || ActiveId() != Id || !Unit) { return Path; }
    TMap<FIntPoint, FIntPoint> Parents;
    const TMap<FIntPoint, int32> Costs = Distances(Id, &Parents);
    const int32* Cost = Costs.Find(Destination);
    if (!Cost || *Cost <= 0 || *Cost > State.MovementLeft) { return Path; }
    for (FIntPoint Cell = Destination; Cell != Unit->Cell; Cell = Parents[Cell]) { Path.Insert(Cell, 0); }
    return Path;
}

static int32 SneakDiceCount(const FCombatEncounter& Encounter, const FCombatantState& Actor)
{
    return Encounter.bScaledSneakAttack ? FMath::Max(1, FClassFeatureRules::SneakAttackDice(FBuildCatalog::ClassLevel(Actor, EAdventurerRole::Rogue))) : 1;
}

bool FCombatBattle::SneakAttackApplies(const FCombatantState& Actor, const FCombatantState& Target) const
{
    const bool bSneakBuild = FBuildCatalog::Find(Actor.BuildId) ? FBuildCatalog::Find(Actor.BuildId)->SneakDie > 0 : Actor.AdventurerRole == EAdventurerRole::Rogue;
    if (!bSneakBuild || !Actor.bSneakWeapon || State.SneakAttackUsedBy.Contains(Actor.Id)) { return false; }
    const ERollMode Mode = AttackRollMode(Actor.Id, Target.Id).Mode;
    return Mode != ERollMode::Disadvantage && (Mode == ERollMode::Advantage ||
        State.Encounter.Combatants.ContainsByPredicate([&Actor, &Target](const FCombatantState& Ally)
        {
            return Ally.Id != Actor.Id && Ally.Side == Actor.Side && Ally.Life == ECombatLife::Conscious && GridDistance(Ally.Cell, Target.Cell) <= 1;
        }));
}

int32 FCombatBattle::NextSlotLevel(int32 Id, int32 Current) const
{
    const FCombatantState* Unit = Find(Id);
    if (!Unit) { return 0; }
    for (int32 Level = FMath::Max(0, Current) + 1; Level <= FSpellRules::MaxSpellLevel; ++Level)
    {
        if (CombatSpellSlots::CastSlotLevel(*Unit, Level) == Level) { return Level; }
    }
    return 0;
}

FActionForecast FCombatBattle::Forecast(int32 ActorId, ECombatCommandType Action, int32 TargetId, int32 SlotLevel) const
{
    FActionForecast Forecast;
    const FCombatantState* Actor = Find(ActorId);
    const FCombatantState* Target = Find(TargetId);
    if (!Actor || !Target || !LegalTargets(ActorId, Action).Contains(TargetId)) { return Forecast; }
    Forecast.bValid = true;
    if (Action == ECombatCommandType::Special1 || Action == ECombatCommandType::Special2 || (Action == ECombatCommandType::CureWounds && !Actor->BuildId.IsEmpty()))
    {
        for (const auto& A : FBuildCatalog::Abilities(*Actor))
        {
            if (A.Command != Action) { continue; }
            Forecast.Formula = A.Description;
            if (A.Effect == EAbilityEffect::Area)
            {
                for (int32 Id : OrderedAreaTargets(*Actor,*Target))
                {
                    const auto& Other = *Find(Id);
                    const int32 Prevented=FMath::Min(A.Amount,ProtectionFor(Other));
                    const int32 Lost=FMath::Min(Other.HP,FMath::Max(0,A.Amount-Prevented));
                    Forecast.Formula += TEXT(" | ") + Other.Name + FString::Printf(TEXT(" loses %d HP"),Lost);
                    if(Prevented) { Forecast.Effects.Add({Id,EAbilityEffect::Mitigation,Prevented,Other.Cell}); }
                    Forecast.Effects.Add({Id,EAbilityEffect::Strike,Lost,Other.Cell});
                }
            }
            if (A.Effect == EAbilityEffect::Healing)
            { Forecast.bHealing = true; Forecast.MinAmount = Forecast.MaxAmount = FMath::Min(A.Amount, Target->MaxHP - Target->HP); if (A.bCureDice) { const int32 Mod = CureModifier(ActorId); Forecast.MinAmount = FMath::Clamp(CureDice + Mod,0,Target->MaxHP-Target->HP); Forecast.MaxAmount = FMath::Clamp(CureDice * CureDie + Mod,0,Target->MaxHP-Target->HP); } }
            Forecast.bDiscreteAmounts = A.Effect == EAbilityEffect::Surge;
            if (A.Effect == EAbilityEffect::Strike || A.Effect == EAbilityEffect::Surge || A.Effect == EAbilityEffect::Push || A.Effect == EAbilityEffect::Area)
            {
                Forecast.HitChancePercent = 100;
                Forecast.MinAmount = A.Amount;
                Forecast.MaxAmount = A.Effect == EAbilityEffect::Surge ? A.Amount * 2 : A.Amount;
                auto Reduce = [&](int32 Amount)
                {
                    const int32 Protection = ProtectionFor(*Target);
                    return FMath::Min(Target->HP, FMath::Max(0, Amount - Protection));
                };
                Forecast.MinAmount = Reduce(Forecast.MinAmount); Forecast.MaxAmount = Reduce(Forecast.MaxAmount);
                Forecast.bCanDrop = Forecast.MaxAmount >= Target->HP;
            }
        }
        return Forecast;
    }
    auto Effect = [&Forecast](int32 Count, int32 Sides, int32 Modifier)
    {
        Forecast.MinAmount += Count + Modifier;
        Forecast.MaxAmount += Count * Sides + Modifier;
        Forecast.Formula = Modifier ? FString::Printf(TEXT("%dd%d%+d"), Count, Sides, Modifier) : FString::Printf(TEXT("%dd%d"), Count, Sides);
    };
    switch (Action)
    {
    case ECombatCommandType::PrecisionStrike:
    case ECombatCommandType::Attack:
    {
        Forecast.bAttackRoll = true;
        Forecast.CoverBonus = CoverBonus(ActorId, TargetId);
        Forecast.TargetArmorClass = EffectiveArmorClass(TargetId) + Forecast.CoverBonus;
        int32 Hits = 0;
        for (int32 Natural = 1; Natural <= 20; ++Natural)
        { Hits += Natural >= CriticalThreshold(*Actor) || (Natural != 1 && Natural + WeaponAttackBonus(*this, *Actor) + (Action == ECombatCommandType::PrecisionStrike ? PrecisionBonus : 0) >= Forecast.TargetArmorClass) ? 1 : 0; }
        const FAttackRollMode Mode = AttackRollMode(ActorId, TargetId);
        const bool bDie = Action == ECombatCommandType::PrecisionStrike && State.Encounter.bSuperiorityDice;
        if (bDie) { Hits = 0; for (int32 Natural = 1; Natural <= 20; ++Natural) { Hits += Natural >= CriticalThreshold(*Actor) || (Natural != 1 && Natural + WeaponAttackBonus(*this, *Actor) >= Forecast.TargetArmorClass) ? 1 : 0; } }
        Forecast.RollMode = Mode.Mode;
        Forecast.AdvantageSources = Mode.Advantage;
        Forecast.DisadvantageSources = Mode.Disadvantage;
        // Hitting faces are a top run of the d20, so two-die chances are exact squares.
        Forecast.HitChanceOutOf400 = Mode.Mode == ERollMode::Advantage ? 400 - (20 - Hits) * (20 - Hits) :
            Mode.Mode == ERollMode::Disadvantage ? Hits * Hits : Hits * 20;
        if (bDie)
        {
            const int32 Sides = FSuperiorityRules::DieSides(FBuildCatalog::ClassLevel(*Actor, EAdventurerRole::Fighter));
            Forecast.HitChanceOutOf400 = (FSuperiorityRules::PrecisionHitOutOf(WeaponAttackBonus(*this, *Actor), Forecast.TargetArmorClass, Mode.Mode, Sides) + Sides / 2) / Sides;
        }
        Forecast.HitChancePercent = (Forecast.HitChanceOutOf400 + 2) / 4;
        int32 WeaponCount = 1, WeaponSides = 0;
        WeaponDice(*Actor, WeaponCount, WeaponSides);
        Effect(WeaponCount, WeaponSides, WeaponDamageModifier(*this, *Actor) + (Action == ECombatCommandType::PrecisionStrike && !bDie ? PrecisionBonus : 0));
        Forecast.bSneakAttack = SneakAttackApplies(*Actor, *Target);
        if (Forecast.bSneakAttack) { const int32 N = SneakDiceCount(State.Encounter, *Actor); Forecast.MinAmount += N; Forecast.MaxAmount += N * SneakDie; Forecast.Formula += FString::Printf(TEXT(" +%dd%d sneak"), N, SneakDie); }
        break;
    }
    case ECombatCommandType::MagicMissile:
    {
        const int32 Darts = MissileDarts + CombatSpellSlots::UpcastExtra(ESpell::MagicMissile, FMath::Max(1, CombatSpellSlots::CastSlotLevel(*Actor, SlotLevel)));
        Forecast.HitChancePercent = 100; Forecast.HitChanceOutOf400 = 400; Effect(Darts, MissileDie, Darts * DartBonus); break;
    }
    case ECombatCommandType::BurningHands:
    case ECombatCommandType::Thunderwave:
    {
        // Chance shown is the target failing its save (full damage); a save halves it. Effects list everyone the area covers.
        const int32 Slot = CombatSpellSlots::CastSlotLevel(*Actor, SlotLevel);
        if (Slot == 0) { Forecast.bValid = false; return Forecast; }
        const ESpell Spell = CombatSpellSlots::AreaSpell(Action);
        const FSpellDelivery Delivery = FSpellcastingRules::Delivery(Spell);
        const int32 Dc = FSpellcastingRules::SaveDc(FProficiencyRules::ProficiencyBonus(Actor->Level), AbilityModifier(ActorId, FSpellcastingRules::SpellcastingAbility(Actor->AdventurerRole)));
        Forecast.HitChanceOutOf400 = 400 - SavingThrowChance(TargetId, Delivery.SaveAbility, Dc, ERollMode::Normal);
        Forecast.HitChancePercent = (Forecast.HitChanceOutOf400 + 2) / 4;
        Effect(FSpellRules::UpcastDiceCount(Spell, Slot), FSpellRules::Facts(Spell).DieSides, 0);
        Forecast.Formula += FString::Printf(TEXT(" %s, level %d slot | %s save DC %d halves%s"), Action == ECombatCommandType::BurningHands ? TEXT("fire") : TEXT("thunder"), Slot,
            Delivery.SaveAbility == EAbility::Dexterity ? TEXT("DEX") : TEXT("CON"), Dc, Action == ECombatCommandType::Thunderwave ? TEXT("; fail pushes 2 cells") : TEXT(""));
        const FAreaTemplate Area = CombatSpellSlots::SpellArea(Action, Actor->Cell, Target->Cell);
        for (const FCombatantState& Unit : State.Encounter.Combatants)
        {
            if (Unit.Id != ActorId && Unit.Life != ECombatLife::Dead && FAreaRules::Covers(Area, Unit.Cell) && HasLineOfSight(Area.Origin, Unit.Cell)) { Forecast.Effects.Add({Unit.Id, EAbilityEffect::Area, Forecast.MaxAmount, Unit.Cell}); }
        }
        break;
    }
    case ECombatCommandType::Shove:
    case ECombatCommandType::Grapple:
    {
        // Exact contest odds, with disadvantage for poisoned or frightened sides, matching Submit.
        const FCharacterRules Defender = Character(TargetId);
        const int32 ActorModifier = Character(ActorId).SkillBonus(ESkill::Athletics);
        const int32 DefenderModifier = FMath::Max(Defender.SkillBonus(ESkill::Athletics), Defender.SkillBonus(ESkill::Acrobatics));
        const bool bActorWorse = CheckMode(*Actor, ERollMode::Normal) == ERollMode::Disadvantage;
        const bool bDefenderWorse = CheckMode(*Target, ERollMode::Normal) == ERollMode::Disadvantage;
        auto Weight = [](int32 Face, bool bWorse) { return bWorse ? 2 * (20 - Face) + 1 : 20; };
        int64 Wins = 0;
        for (int32 A = 1; A <= 20; ++A) { for (int32 D = 1; D <= 20; ++D) { if (FCheckRules::ActorWinsContest(A + ActorModifier, D + DefenderModifier)) { Wins += int64(Weight(A, bActorWorse)) * Weight(D, bDefenderWorse); } } }
        Forecast.HitChanceOutOf400 = int32((Wins + 200) / 400);
        Forecast.HitChancePercent = (Forecast.HitChanceOutOf400 + 2) / 4;
        Forecast.Formula = FString::Printf(TEXT("Athletics %+d vs %+d%s | win: %s"), ActorModifier, DefenderModifier, bActorWorse ? TEXT(" (disadvantage)") : TEXT(""),
            Action == ECombatCommandType::Shove ? TEXT("Prone") : TEXT("Grappled (speed 0)"));
        return Forecast;
    }
    case ECombatCommandType::SecondWind: Effect(1, WindDie, FMath::Max(1, FBuildCatalog::ClassLevel(*Actor, EAdventurerRole::Fighter))); break;
    case ECombatCommandType::CureWounds: Effect(CureDice, CureDie, CureModifier(ActorId)); break;
    case ECombatCommandType::Potion: Effect(PotionDice, PotionDie, PotionBonus); break;
    case ECombatCommandType::Rescue: Forecast.MinAmount = Forecast.MaxAmount = 1; Forecast.Formula = TEXT("1"); break;
    default: Forecast.bValid = false; return Forecast;
    }
    Forecast.bHealing = Action != ECombatCommandType::Attack && Action != ECombatCommandType::PrecisionStrike && Action != ECombatCommandType::MagicMissile && !CombatSpellSlots::IsAreaSpell(Action);
    if (Forecast.bHealing)
    {
        const int32 Missing = Target->MaxHP - Target->HP;
        Forecast.MinAmount = FMath::Min(Forecast.MinAmount, Missing);
        Forecast.MaxAmount = FMath::Min(Forecast.MaxAmount, Missing);
    }
    else
    {
        // A critical doubles the weapon and Sneak Attack dice, so warn on that best case too.
        const int32 CriticalExtra = Forecast.bAttackRoll ? WeaponMaxDice(*Actor) + (Forecast.bSneakAttack ? SneakDie * SneakDiceCount(State.Encounter, *Actor) : 0) : 0;
        // Defenses apply after every other modifier, so the shown range is the adjusted one.
        Forecast.DamageType = DamageTypeOf(ActorId, Action);
        const FDamageDefenses Defenses = DamageDefenses(TargetId);
        Forecast.DamageResponse = FDamageRules::Response(Forecast.DamageType, Defenses);
        const int32 BestCase = FDamageRules::Adjust(Forecast.MaxAmount + CriticalExtra, Forecast.DamageType, Defenses);
        Forecast.MinAmount = FDamageRules::Adjust(Forecast.MinAmount, Forecast.DamageType, Defenses);
        Forecast.MaxAmount = FDamageRules::Adjust(Forecast.MaxAmount, Forecast.DamageType, Defenses);
        if (Forecast.DamageResponse != EDamageResponse::Normal)
        { Forecast.Formula += FString::Printf(TEXT(" %s, %s"), FDamageRules::TypeName(Forecast.DamageType), FDamageRules::ResponseName(Forecast.DamageResponse)); }
        Forecast.bCanDrop = BestCase >= Target->HP;
    }
    if (!Forecast.bHealing)
    {
        const int32 Protection = ProtectionFor(*Target);
        const int32 CriticalExtra = Forecast.bAttackRoll ? WeaponMaxDice(*Actor) + (Forecast.bSneakAttack ? SneakDie * SneakDiceCount(State.Encounter, *Actor) : 0) : 0;
        Forecast.bCanDrop = FMath::Max(0, Forecast.MaxAmount + CriticalExtra - Protection) >= Target->HP;
        Forecast.MinAmount = FMath::Max(0, Forecast.MinAmount - Protection);
        Forecast.MaxAmount = FMath::Max(0, Forecast.MaxAmount - Protection);
        if (Protection > 0) { Forecast.Formula += FString::Printf(TEXT(" minus %d protection (minimum 0)"), Protection); }
    }
    return Forecast;
}

FAbilityScores FCombatBattle::AbilityScores(int32 Id) const
{
    const FCombatantState* Unit = Find(Id);
    if (!Unit) { return FAbilityScores(); }
    if (Unit->Side == ECombatSide::Party && Unit->AdventurerRole != EAdventurerRole::None)
    {
        FAbilityScores Scores; FBuildCatalog::HeroScores(*Unit, Scores); return Scores; // #79: ASIs apply
    }
    return FAbilityRules::EnemyScores(Unit->Role);
}

int32 FCombatBattle::AbilityModifier(int32 Id, EAbility Ability) const
{
    return Find(Id) ? FAbilityRules::Modifier(AbilityScores(Id).Get(Ability)) : 0;
}

int32 FCombatBattle::CureModifier(int32 Id) const
{
    const FCombatantState* Unit = Find(Id);
    return Unit && Unit->Side == ECombatSide::Party && !Unit->BuildId.IsEmpty() ? AbilityModifier(Id, EAbility::Wisdom) : HealingModifier;
}

FCharacterRules FCombatBattle::Character(int32 Id) const
{
    const FCombatantState* Unit = Find(Id);
    if (!Unit) { return FCharacterRules(); }
    return Unit->Side == ECombatSide::Party && Unit->AdventurerRole != EAdventurerRole::None ?
        FProficiencyRules::PrebuiltCharacter(Unit->AdventurerRole, Unit->Level) : FProficiencyRules::EnemyCharacter(Unit->Role);
}

int32 FCombatBattle::SkillCheckChance(int32 Id, ESkill Skill, int32 Dc, ERollMode Mode) const
{
    return Find(Id) ? FCheckRules::SuccessOutOf400(Character(Id).SkillBonus(Skill), Dc, CheckMode(*Find(Id), Mode)) : 0;
}

int32 FCombatBattle::SavingThrowChance(int32 Id, EAbility Ability, int32 Dc, ERollMode Mode) const
{
    return Find(Id) ? (IsHelpless(*Find(Id)) && (Ability == EAbility::Strength || Ability == EAbility::Dexterity) ? 0 : FCheckRules::SuccessOutOf400(Character(Id).SaveBonus(Ability), Dc, SaveMode(*Find(Id), Ability, Mode))) : 0;
}

namespace
{
bool ValidateD20Test(bool bStarted, const FCombatantState* Unit, int32 Dc, FString& Error)
{
    if (!bStarted) { Error = TEXT("No battle in progress."); return false; }
    if (!Unit) { Error = TEXT("Unknown combatant."); return false; }
    if (Unit->Life != ECombatLife::Conscious) { Error = TEXT("Only conscious combatants roll checks and saves."); return false; }
    if (Dc < FCheckRules::MinDc || Dc > FCheckRules::MaxDc) { Error = TEXT("DC must be 1..30."); return false; }
    return true;
}
}

bool FCombatBattle::SkillCheck(int32 Id, ESkill Skill, int32 Dc, ERollMode Mode, FD20Outcome& Out, FString& Error)
{
    if (!ValidateD20Test(bStarted, Find(Id), Dc, Error)) { return false; }
    Out = FCheckRules::Resolve(Character(Id).SkillBonus(Skill), Dc, CheckMode(*Find(Id), Mode), State.RandomState);
    Error.Reset();
    return true;
}

bool FCombatBattle::SavingThrow(int32 Id, EAbility Ability, int32 Dc, ERollMode Mode, FD20Outcome& Out, FString& Error)
{
    if (!ValidateD20Test(bStarted, Find(Id), Dc, Error)) { return false; }
    Out = FCheckRules::Resolve(Character(Id).SaveBonus(Ability), Dc, SaveMode(*Find(Id), Ability, Mode), State.RandomState);
    if (IsHelpless(*Find(Id)) && (Ability == EAbility::Strength || Ability == EAbility::Dexterity)) { Out.bSuccess = false; }
    Error.Reset();
    return true;
}

const TCHAR* FCombatBattle::CoverName(int32 Bonus)
{
    return Bonus >= 5 ? TEXT("three-quarters cover") : Bonus > 0 ? TEXT("half cover") : TEXT("no cover");
}

int32 FCombatBattle::CoverBonus(int32 ActorId, int32 TargetId) const
{
    const FCombatantState* Actor = Find(ActorId);
    const FCombatantState* Target = Find(TargetId);
    if (!Actor || !Target) { return 0; }
    TArray<FIntPoint> Creatures;
    if (State.Encounter.bCreatureCover)
    {
        for (const FCombatantState& Other : State.Encounter.Combatants)
        {
            if (Other.Id != ActorId && Other.Id != TargetId && Other.Life == ECombatLife::Conscious) { Creatures.Add(Other.Cell); }
        }
    }
    if (State.Encounter.BlockedCells.IsEmpty() && Creatures.IsEmpty()) { return 0; }
    // SRD/DMG grid cover: from the attacker's best cell corner, trace a line to each corner of the target's cell.
    // A line is blocked when it passes through the interior of a blocked cell (edges and corners do not block).
    auto Crosses = [](FVector2D A, FVector2D B, FIntPoint Cell)
    {
        double Enter = 0.0, Exit = 1.0;
        const double Start[2] = {A.X, A.Y}, Delta[2] = {B.X - A.X, B.Y - A.Y}, Low[2] = {(double)Cell.X, (double)Cell.Y};
        for (int32 Axis = 0; Axis < 2; ++Axis)
        {
            if (Delta[Axis] == 0.0)
            {
                if (Start[Axis] <= Low[Axis] || Start[Axis] >= Low[Axis] + 1.0) { return false; }
                continue;
            }
            double T0 = (Low[Axis] - Start[Axis]) / Delta[Axis], T1 = (Low[Axis] + 1.0 - Start[Axis]) / Delta[Axis];
            if (T0 > T1) { Swap(T0, T1); }
            Enter = FMath::Max(Enter, T0); Exit = FMath::Min(Exit, T1);
        }
        return Enter < Exit;
    };
    auto Corner = [](FIntPoint Cell, int32 Index) { return FVector2D(Cell.X + (Index & 1), Cell.Y + (Index >> 1)); };
    auto BestLines = [&](const TArray<FIntPoint>& Cells)
    {
        int32 Best = 4;
        for (int32 From = 0; From < 4 && Best > 0; ++From)
        {
            int32 Lines = 0;
            for (int32 To = 0; To < 4; ++To)
            {
                for (FIntPoint Cell : Cells)
                {
                    if (Crosses(Corner(Actor->Cell, From), Corner(Target->Cell, To), Cell)) { ++Lines; break; }
                }
            }
            Best = FMath::Min(Best, Lines);
        }
        return Best;
    };
    const int32 Walls = State.Encounter.BlockedCells.IsEmpty() ? 0 : BestLines(State.Encounter.BlockedCells);
    const int32 WallBonus = Walls >= 3 ? 5 : Walls > 0 ? 2 : 0;
    // Creatures never give more than half cover.
    return FMath::Max(WallBonus, !Creatures.IsEmpty() && BestLines(Creatures) > 0 ? 2 : 0);
}

FAttackRollMode FCombatBattle::AttackRollMode(int32 ActorId, int32 TargetId) const
{
    FAttackRollMode Mode;
    const FCombatantState* Actor = Find(ActorId);
    if (!Actor || !Find(TargetId)) { return Mode; }
    // SRD 5.2.1: a ranged attack has disadvantage while within 5 ft of an enemy who can see you and isn't Incapacitated.
    if (RangedAttacker(*Actor))
    {
        for (const FCombatantState& Other : State.Encounter.Combatants)
        {
            if (Other.Side != Actor->Side && Other.Life == ECombatLife::Conscious && GridDistance(Other.Cell, Actor->Cell) <= 1 &&
                !IsIncapacitated(Other) && NearbyFoeSees(Other, *Actor))
            {
                Mode.Disadvantage.Add(FString::Printf(TEXT("ranged attack with %s adjacent"), *Other.Name));
            }
        }
    }
    const FCombatantState* Target = Find(TargetId);
    if (const FWeaponProfile* Weapon = MainWeapon(*Actor); Weapon && Weapon->bRanged &&
        FWeaponRules::Band(*Weapon, GridDistance(Actor->Cell, Target->Cell), false) == ERangeBand::Long)
    { Mode.Disadvantage.Add(TEXT("long range")); }
    if (ArmorPenaltiesOf(*this, *Actor).bStrDexDisadvantage) { Mode.Disadvantage.Add(TEXT("unproficient armor")); }
    if (State.Encounter.bDarkness)
    {
        // #88: darkness everywhere; darkvision within range turns it into dim light, which still lets a creature see.
        const int32 Feet = GridDistance(Actor->Cell, Target->Cell) * 5;
        // Light spells (Light, Daylight...) brighten their cells; the Darkvision spell grants 150 ft.
        const ELight AtTarget = SpellZoneAt(Target->Cell, 1) ? ELight::Bright : ELight::Darkness;
        const ELight AtActor = SpellZoneAt(Actor->Cell, 1) ? ELight::Bright : ELight::Darkness;
        const int32 ActorDv = SpellExtraOn(Actor->Id, SpellEffectExtra::Darkvision) ? FMath::Max(150, FLightRules::DarkvisionFeet(Actor->Role)) : FLightRules::DarkvisionFeet(Actor->Role);
        const int32 TargetDv = SpellExtraOn(Target->Id, SpellEffectExtra::Darkvision) ? FMath::Max(150, FLightRules::DarkvisionFeet(Target->Role)) : FLightRules::DarkvisionFeet(Target->Role);
        const bool bActorSees = FLightRules::CanSee(FLightRules::PerceivedLight(AtTarget, ActorDv, Feet));
        const bool bTargetSees = FLightRules::CanSee(FLightRules::PerceivedLight(AtActor, TargetDv, Feet));
        if (!bActorSees) { Mode.Disadvantage.Add(TEXT("can't see the target in darkness")); }
        if (!bTargetSees) { Mode.Advantage.Add(TEXT("unseen in darkness")); }
    }
    // Fog Cloud and Darkness: a heavily obscured cell blocks sight both ways.
    if (!SpellCanSee(Actor->Id, TargetId, false)) { Mode.Disadvantage.Add(TEXT("can't see the target (obscured)")); }
    if (!SpellCanSee(TargetId, Actor->Id, false)) { Mode.Advantage.Add(TEXT("unseen (obscured)")); }
    for (const FCombatCondition& Condition : Actor->Conditions)
    {
        const FCombatantState* Source = Find(Condition.SourceId);
        switch (Condition.Condition)
        {
        case ECombatCondition::Poisoned: Mode.Disadvantage.Add(TEXT("attacker poisoned")); break;
        case ECombatCondition::Blinded: Mode.Disadvantage.Add(TEXT("attacker blinded")); break;
        case ECombatCondition::Restrained: Mode.Disadvantage.Add(TEXT("attacker restrained")); break;
        case ECombatCondition::Prone: Mode.Disadvantage.Add(TEXT("attacker prone")); break;
        case ECombatCondition::Frightened:
            if (!Source || (Source->Life == ECombatLife::Conscious && HasLineOfSight(Actor->Cell, Source->Cell))) { Mode.Disadvantage.Add(TEXT("attacker frightened")); }
            break;
        default: break;
        }
    }
    if (HasCondition(*Target, ECombatCondition::Blinded)) { Mode.Advantage.Add(TEXT("target blinded")); }
    if (HasCondition(*Target, ECombatCondition::Restrained)) { Mode.Advantage.Add(TEXT("target restrained")); }
    for (const ECombatCondition Helpless : {ECombatCondition::Stunned, ECombatCondition::Paralyzed, ECombatCondition::Unconscious})
    {
        if (HasCondition(*Target, Helpless)) { Mode.Advantage.Add(FString::Printf(TEXT("target %s"), *FString(ConditionName(Helpless)).ToLower())); }
    }
    if (HasCondition(*Target, ECombatCondition::Prone))
    {
        (FMovementRules::AttackAgainstProne(GridDistance(Actor->Cell, Target->Cell)) == ERollMode::Advantage ? Mode.Advantage : Mode.Disadvantage).Add(TEXT("target prone"));
    }
    // Standard actions (#60): Dodge, Hide and Help.
    if (FStandardActionRules::AttackAgainstDodger(Target->bDodging && !IsIncapacitated(*Target), !Actor->bHidden && HasLineOfSight(Target->Cell, Actor->Cell)) == ERollMode::Disadvantage)
    { Mode.Disadvantage.Add(TEXT("target dodging")); }
    if (Actor->bHidden) { Mode.Advantage.Add(TEXT("attacker hidden")); }
    if (Target->bReckless) { Mode.Advantage.Add(TEXT("target reckless")); }
    if (Actor->bReckless && Actor->Id == ActiveId())
    {
        const FWeaponProfile* Held = MainWeapon(*Actor);
        if (Held ? !Held->bRanged : GridDistance(Actor->Cell, Target->Cell) <= 1) { Mode.Advantage.Add(TEXT("reckless attack")); }
    }
    if (Target->bHidden) { Mode.Disadvantage.Add(TEXT("target hidden")); }
    if (const FCombatantState* Helper = Find(Target->HelpedById); Helper && Helper->Id != Actor->Id && Helper->Side == Actor->Side)
    { Mode.Advantage.Add(FString::Printf(TEXT("helped by %s"), *Helper->Name)); }
    SpellAttackModes(*Actor, *Target, Mode);
    Mode.Mode = FCheckRules::CombineMode(Mode.Advantage.Num(), Mode.Disadvantage.Num());
    return Mode;
}

bool FCombatBattle::HasCondition(const FCombatantState& Unit, ECombatCondition Condition)
{
    return Unit.Conditions.ContainsByPredicate([Condition](const FCombatCondition& C) { return C.Condition == Condition; });
}

bool FCombatBattle::NearbyFoeSees(const FCombatantState& Viewer, const FCombatantState& Target) const
{
    if (HasCondition(Viewer, ECombatCondition::Blinded) || !SpellCanSee(Viewer.Id, Target.Id, true)) { return false; }
    if (!State.Encounter.bDarkness) { return true; }
    // #88 darkness: the viewer needs light at the target's cell or darkvision reaching it.
    const ELight AtTarget = SpellZoneAt(Target.Cell, 1) ? ELight::Bright : ELight::Darkness;
    const int32 Dv = SpellExtraOn(Viewer.Id, SpellEffectExtra::Darkvision) ? FMath::Max(150, FLightRules::DarkvisionFeet(Viewer.Role)) : FLightRules::DarkvisionFeet(Viewer.Role);
    return FLightRules::CanSee(FLightRules::PerceivedLight(AtTarget, Dv, GridDistance(Viewer.Cell, Target.Cell) * 5));
}

bool FCombatBattle::IsHelpless(const FCombatantState& Unit)
{
    return HasCondition(Unit, ECombatCondition::Stunned) || HasCondition(Unit, ECombatCondition::Paralyzed) || HasCondition(Unit, ECombatCondition::Unconscious) ||
        HasCondition(Unit, ECombatCondition::Petrified);
}

bool FCombatBattle::IsIncapacitated(const FCombatantState& Unit)
{
    return IsHelpless(Unit) || HasCondition(Unit, ECombatCondition::Incapacitated);
}

ERollMode FCombatBattle::SaveMode(const FCombatantState& Unit, EAbility Ability, ERollMode Requested) const
{
    const int32 Adv = (Requested == ERollMode::Advantage ? 1 : 0) +
        (Ability == EAbility::Dexterity && FStandardActionRules::DexSaveWhileDodging(Unit.bDodging && !IsIncapacitated(Unit)) == ERollMode::Advantage ? 1 : 0);
    const int32 Dis = (Requested == ERollMode::Disadvantage ? 1 : 0) + (Ability == EAbility::Dexterity && HasCondition(Unit, ECombatCondition::Restrained) ? 1 : 0);
    int32 SpellAdv = 0, SpellDis = 0;
    SpellSaveModes(Unit, Ability, SpellAdv, SpellDis);
    return FCheckRules::CombineMode(Adv + SpellAdv, Dis + SpellDis);
}

ERollMode FCombatBattle::CheckMode(const FCombatantState& Unit, ERollMode Requested) const
{
    int32 Dis = Requested == ERollMode::Disadvantage ? 1 : 0;
    if (HasCondition(Unit, ECombatCondition::Poisoned)) { ++Dis; }
    for (const FCombatCondition& C : Unit.Conditions)
    {
        const FCombatantState* Source = Find(C.SourceId);
        if (C.Condition == ECombatCondition::Frightened && (!Source || (Source->Life == ECombatLife::Conscious && HasLineOfSight(Unit.Cell, Source->Cell)))) { ++Dis; break; }
    }
    return FCheckRules::CombineMode(Requested == ERollMode::Advantage ? 1 : 0, Dis);
}

void FCombatBattle::BeginTurn()
{
    if (!MutableFind(ActiveId())) { return; }
    State.bSlotSpentThisTurn = false;
    SpellsOnTurnStart(ActiveId());
    FCombatantState* Unit = MutableFind(ActiveId());
    Unit->bReactionAvailable = true;
    Unit->bDodging = false; Unit->bDisengaged = false; Unit->bReadied = false; Unit->bShielded = false; Unit->bReckless = false; Unit->QuickenState = 0;
    for (FCombatantState& Other : State.Encounter.Combatants) { if (Other.HelpedById == Unit->Id) { Other.HelpedById = INDEX_NONE; } }
    State.SneakAttackUsedBy.Reset();
    State.MeleeHitThisTurn.Reset();
    State.MeleeCritThisTurn.Reset();
    const bool bHeld = HasCondition(*Unit, ECombatCondition::Grappled) || HasCondition(*Unit, ECombatCondition::Restrained) || IsHelpless(*Unit);
    const int32 TurnSpeed = SpellSpeed(Unit->Id, FMath::Max(0, BaseSpeed(*this, *Unit) - ArmorPenaltiesOf(*this, *Unit).SpeedPenaltyFeet / FWeaponRules::FeetPerCell));
    State.MovementLeft = bHeld ? 0 : TurnSpeed;
    if (!bHeld && HasCondition(*Unit, ECombatCondition::Prone) && !SpellKeepsProne(Unit->Id))
    {
        Unit->Conditions.RemoveAll([](const FCombatCondition& C) { return C.Condition == ECombatCondition::Prone; });
        State.MovementLeft = FMath::Max(0, TurnSpeed - FMovementRules::StandUpCost(Unit->Speed));
    }
    if (IsIncapacitated(*Unit))
    {
        State.bActionAvailable = false;
        State.bBonusActionAvailable = false;
        Unit->bReactionAvailable = false;
    }
    if (SpellBlocksReactions(Unit->Id)) { Unit->bReactionAvailable = false; }
    SpellEffectsOnTurnStart(Unit->Id);
}

void FCombatBattle::PruneConditions()
{
    const bool bFighting = State.Phase == EBattlePhase::Fighting;
    for (FCombatantState& Unit : State.Encounter.Combatants)
    {
        if (Unit.Life == ECombatLife::Dead) { Unit.Conditions.Reset(); continue; }
        Unit.Conditions.RemoveAll([&](const FCombatCondition& C)
        {
            if (C.SourceId == INDEX_NONE) { return !bFighting && C.Condition == ECombatCondition::Prone; }
            const FCombatantState* Source = Find(C.SourceId);
            return !bFighting || !Source || Source->Life == ECombatLife::Dead ||
                (C.Condition == ECombatCondition::Grappled && (Source->Life != ECombatLife::Conscious || IsIncapacitated(*Source) || GridDistance(Source->Cell, Unit.Cell) > 1));
        });
    }
    if (!bFighting) { const TArray<FActiveSpell> Ended = MoveTemp(State.ActiveSpells); State.ActiveSpells.Reset(); for (const FActiveSpell& A : Ended) { EndSpellEffects(A); } return; }
    State.ActiveSpells.RemoveAll([this](const FActiveSpell& A)
    {
        const FCombatantState* Caster = Find(A.CasterId);
        const FCombatantState* Target = A.TargetId == INDEX_NONE ? nullptr : Find(A.TargetId);
        return !Caster || Caster->Life == ECombatLife::Dead || (A.TargetId != INDEX_NONE && (!Target || Target->Life == ECombatLife::Dead));
    });
}

FCombatResult FCombatBattle::Submit(const FCombatCommand& Command)
{
    TArray<ECombatLife> LifeBefore;
    for (const FCombatantState& Unit : State.Encounter.Combatants) { LifeBefore.Add(Unit.Life); }
    FCombatResult Result = Resolve(Command);
    DiceLog = nullptr;
    if (!Result.bAccepted) { PendingSpellEvents.Reset(); return Result; }
    PruneConditions();
    Result.SpellEvents = MoveTemp(PendingSpellEvents);
    if (OnSpellEvent) { for (const FSpellCastEvent& E : Result.SpellEvents) { OnSpellEvent(E); } }
    PendingSpellEvents.Reset();
    if (Result.DamagePrevented > 0) { Result.Feedback += FString::Printf(TEXT(" | protection prevented %d damage"),Result.DamagePrevented); }
    Result.Type = Command.Type;
    Result.ActorId = Command.ActorId;
    Result.TargetId = Command.TargetId;
    for (int32 Index = 0; Index < State.Encounter.Combatants.Num() && Index < LifeBefore.Num(); ++Index)
    {
        const FCombatantState& Unit = State.Encounter.Combatants[Index];
        if (Unit.Life == LifeBefore[Index]) { continue; }
        if (Unit.Life == ECombatLife::Downed) { Result.Downed.Add(Unit.Id); }
        if (Unit.Life == ECombatLife::Dead) { Result.Died.Add(Unit.Id); }
    }
    return Result;
}

void FCombatBattle::Strike(int32 ActorId, int32 TargetId, int32 ManeuverBonus, FCombatResult& Result, bool bOffHand)
{
    // The off-hand swing (#62) uses the off-hand weapon's profile and drops a positive ability modifier from damage.
    FCombatantState Hand = *Find(ActorId);
    if (bOffHand) { Hand.MainHand = Hand.OffHand; }
    const FCombatantState* Actor = &Hand;
    FCombatantState* Target = MutableFind(TargetId);
    if (const FMonsterStatBlock* Block = Actor->Side == ECombatSide::Enemy ? FMonsterRules::Find(Actor->Role) : nullptr; Block && Block->Kind == EMonsterAttack::SaveCantrip)
    {
        // SRD Sacred Flame (cantrip): no attack roll and no cover; the target makes a DEX save against the caster's DC or takes 1d8 radiant.
        Result.bSaveSpell = true;
        MutableFind(ActorId)->bHidden = false;
        const int32 Dc = FMonsterRules::SpellSaveDc(*Block);
        Result.TargetArmorClass = Dc;
        for (FCombatantState& Counter : State.Encounter.Combatants)
        {
            if (Counter.Side == Actor->Side || Counter.Life != ECombatLife::Conscious || IsIncapacitated(Counter) || !Counter.bReactionAvailable ||
                !FFeatureKit::HasCounterspell(Counter) || GridDistance(Counter.Cell, Actor->Cell) > FFeatureKit::CounterspellRange || !HasLineOfSight(Counter.Cell, Actor->Cell) ||
                Counter.SpellSlots <= 0) { continue; }
            // SRD Counterspell: a cantrip (level 0) is stopped automatically, no check.
            Counter.bReactionAvailable = false; SpendSpellSlot(Counter);
            Result.bCounterspelled = true;
            Result.RollModeSources.Add(FString::Printf(TEXT("%s Counterspell (reaction, 1 slot): the spell fails"), *Counter.Name));
            Result.AttackRolls.Add(0);
            return;
        }
        Result.AttackRolls.Add(Roll(20)); Result.NaturalRoll = Result.AttackRolls[0];
        Result.AttackBonus = Character(TargetId).SaveBonus(Block->SaveAbility);
        Result.AttackTotal = Result.NaturalRoll + Result.AttackBonus;
        Result.bHit = Result.AttackTotal < Dc;
        Result.RollModeSources.Add(FString::Printf(TEXT("Sacred Flame: DEX save DC %d"), Dc));
        if (Result.bHit)
        {
            DiceLog = &Result.EffectDice; Result.Damage = Roll(Block->DamageDieSides); DiceLog = nullptr;
            const int32 Protection = ProtectionFor(*Target);
            Result.DamagePrevented = FMath::Min(Result.Damage, Protection);
            ApplyDamage(*Target, Result.Damage);
            Result.Damage -= Result.DamagePrevented;
        }
        return;
    }
    if (SanctuaryStops(ActorId, TargetId, Result)) { return; }
    FAttackRollMode Mode = AttackRollMode(ActorId, TargetId);
    // SRD Protection style: an adjacent ally with a shield spends its reaction to impose disadvantage.
    for (FCombatantState& Guard : State.Encounter.Combatants)
    {
        if (Guard.Id == TargetId || Guard.Side != Target->Side || Guard.Life != ECombatLife::Conscious || IsIncapacitated(Guard) || !Guard.bReactionAvailable ||
            !Guard.bShieldEquipped || FFeatureKit::Style(Guard) != EFightingStyle::Protection || GridDistance(Guard.Cell, Target->Cell) > 1) { continue; }
        Guard.bReactionAvailable = false;
        Mode.Disadvantage.Add(FString::Printf(TEXT("%s Protection (reaction)"), *Guard.Name));
        Mode.Mode = FCheckRules::CombineMode(Mode.Advantage.Num(), Mode.Disadvantage.Num());
        Result.bProtectionReaction = true;
        break;
    }
    if (State.Encounter.bInspiration && Mode.Mode != ERollMode::Advantage && MutableFind(ActorId)->bInspired)
    {
        FInspirationRules::Spend(MutableFind(ActorId)->bInspired);
        Mode.Mode = FInspirationRules::ModeAfterSpending(Mode.Mode, true);
        Mode.Advantage.Add(TEXT("inspiration"));
    }
    Result.RollMode = Mode.Mode;
    Result.RollModeSources = Mode.Advantage;
    Result.RollModeSources.Append(Mode.Disadvantage);
    Result.AttackRolls.Add(Roll(20));
    if (Mode.Mode != ERollMode::Normal) { Result.AttackRolls.Add(Roll(20)); }
    Result.NaturalRoll = Mode.Mode == ERollMode::Advantage ? FMath::Max(Result.AttackRolls[0], Result.AttackRolls.Last()) :
        FMath::Min(Result.AttackRolls[0], Result.AttackRolls.Last());
    if (NeedsAmmunition(*Actor)) { --MutableFind(ActorId)->Ammunition; }
    Result.AttackBonus = WeaponAttackBonus(*this, *Actor) + ManeuverBonus + SpellAttackBonus(ActorId, true, Result) + StrikeAbilitySwap(*Actor);
    Result.AttackTotal = Result.NaturalRoll + Result.AttackBonus;
    Result.CoverBonus = CoverBonus(ActorId, TargetId);
    Result.TargetArmorClass = EffectiveArmorClass(TargetId) + Result.CoverBonus;
    Result.bCritical = Result.NaturalRoll >= CriticalThreshold(*Actor);
    Result.bHit = Result.bCritical || (Result.NaturalRoll != 1 && Result.AttackTotal >= Result.TargetArmorClass);
    // SRD Bardic Inspiration: the held die is added after the roll, here only when it could still turn a miss into a hit.
    if (FCombatantState* Holder = MutableFind(ActorId); !Result.bHit && Result.NaturalRoll != 1 && Holder->BardicDie > 0 && Result.AttackTotal + Holder->BardicDie >= Result.TargetArmorClass)
    {
        Result.BardicRoll = Roll(Holder->BardicDie);
        Holder->BardicDie = 0;
        Result.AttackBonus += Result.BardicRoll; Result.AttackTotal += Result.BardicRoll;
        Result.RollModeSources.Add(FString::Printf(TEXT("Bardic Inspiration +%d"), Result.BardicRoll));
        Result.bHit = Result.AttackTotal >= Result.TargetArmorClass;
    }
    // SRD 5.2.1 College of Lore 3, Cutting Words (reaction, 60 ft, seen): a Lore bard spends a Bardic Inspiration use to subtract the die
    // from a foe's attack roll; the engine spends it only when the die could turn the hit into a miss.
    if (Result.bHit && !Result.bCritical && Actor->Side == ECombatSide::Enemy)
    {
        for (FCombatantState& Bard : State.Encounter.Combatants)
        {
            if (Bard.Side != ECombatSide::Party || Bard.Life != ECombatLife::Conscious || Bard.BuildId != TEXT("bard.lore") || !Bard.bReactionAvailable || Bard.BardicUses <= 0 ||
                FBuildCatalog::ClassLevel(Bard, EAdventurerRole::Bard) < 3 || IsIncapacitated(Bard) || GridDistance(Bard.Cell, Actor->Cell) > 12 || !HasLineOfSight(Bard.Cell, Actor->Cell)) { continue; }
            const int32 Sides = FFeatureKit::BardicSides(Bard);
            if (Result.AttackTotal - Sides >= Result.TargetArmorClass) { continue; }
            --Bard.BardicUses; Bard.bReactionAvailable = false;
            Result.CuttingRoll = Roll(Sides);
            Result.AttackBonus -= Result.CuttingRoll; Result.AttackTotal -= Result.CuttingRoll;
            Result.RollModeSources.Add(FString::Printf(TEXT("Cutting Words -%d (%s)"), Result.CuttingRoll, *Bard.Name));
            Result.bHit = Result.AttackTotal >= Result.TargetArmorClass;
            break;
        }
    }
    if (Result.bHit && ShieldWouldBlock(*Target, Result.AttackTotal, Result.TargetArmorClass, Result.bCritical))
    {
        SpendSpellSlot(*Target); Target->bReactionAvailable = false; Target->bShielded = true;
        Result.TargetArmorClass += 5; Result.bHit = false; Result.bShieldReaction = true;
    }
    if (State.Encounter.bInspiration && Result.NaturalRoll == 20) { FInspirationRules::Grant(MutableFind(ActorId)->bInspired); }
    // Attacking reveals a hidden attacker; Help is spent by the first ally attack it benefits.
    if (Result.bHit && MirrorImageTakes(TargetId, Result)) { Result.bHit = false; }
    MutableFind(ActorId)->bHidden = false;
    EndOnHostileAct(ActorId);
    if (const FCombatantState* Helper = Find(Target->HelpedById); Helper && Helper->Id != ActorId && Helper->Side == Actor->Side) { Target->HelpedById = INDEX_NONE; }
    // SRD: any hit on a paralyzed or unconscious creature from within 5 feet is a critical hit.
    if (Result.bHit && GridDistance(Actor->Cell, Target->Cell) <= 1 && (HasCondition(*Target, ECombatCondition::Paralyzed) || HasCondition(*Target, ECombatCondition::Unconscious))) { Result.bCritical = true; }
    if (Result.bHit)
    {
        DiceLog = &Result.EffectDice;
        // Weapon die, then the critical die; both have the same sides, so totals match earlier builds.
        int32 DiceCount = 1, DiceSides = 0;
        WeaponDice(*Actor, DiceCount, DiceSides);
        ShillelaghDie(*Actor, DiceCount, DiceSides);
        // SRD Great Weapon Fighting: reroll each 1 or 2 once on a two-handed (or two-handed versatile) melee weapon.
        const FWeaponProfile* Held = MainWeapon(*Actor);
        const bool bGreat = Held && !Held->bRanged && FFeatureKit::Style(*Actor) == EFightingStyle::GreatWeaponFighting &&
            (Held->Has(EWeaponProperty::TwoHanded) || (Held->Has(EWeaponProperty::Versatile) && !Actor->bShieldEquipped));
        auto WeaponRoll = [&]() { int32 Sum = 0; for (int32 I = 0; I < DiceCount; ++I) { int32 Die = Roll(DiceSides); if (bGreat && Die <= 2) { Die = Roll(DiceSides); } Sum += Die; } return Sum; };
        Result.Damage = WeaponRoll();
        if (Result.bCritical) { Result.Damage += WeaponRoll(); }
        Result.EffectModifier = (bOffHand ? FTwoWeaponRules::OffHandDamageModifier(WeaponDamageModifier(*this, *Actor), FFeatureKit::Style(*Actor) == EFightingStyle::TwoWeaponFighting) : WeaponDamageModifier(*this, *Actor)) + (State.Encounter.bSuperiorityDice ? 0 : ManeuverBonus);
        Result.EffectModifier += StrikeAbilitySwap(*Actor);
        Result.Damage += Result.EffectModifier;
        Result.bSneakAttack = SneakAttackApplies(*Actor, *Target);
        if (Result.bSneakAttack) { Result.Damage += RollDice((Result.bCritical ? 2 : 1) * SneakDiceCount(State.Encounter, *Actor), SneakDie); State.SneakAttackUsedBy.Add(ActorId); }
        DiceLog = nullptr;
        ApplyDefenses(Result, ActorId, ECombatCommandType::Attack, TargetId);
        Result.Damage += SpellHitDamage(ActorId, TargetId, Result.bCritical, true, Result.DamageType, Result);
        Result.Damage = FMath::Max(0, Result.Damage);
        if (Result.Damage > 1 && FFeatureKit::HasUncannyDodge(*Target) && Target->Life == ECombatLife::Conscious && Target->bReactionAvailable && !IsIncapacitated(*Target) && HasLineOfSight(Target->Cell, Actor->Cell))
        {
            // SRD Uncanny Dodge: the rogue's reaction halves the hit's damage (rounded down).
            Target->bReactionAvailable = false;
            Result.Damage /= 2; Result.bUncannyDodge = true;
            Result.RollModeSources.Add(TEXT("Uncanny Dodge: damage halved"));
        }
        if (Result.Damage > 0 && RangedAttacker(*Actor) && FFeatureKit::HasDeflectMissiles(*Target) && Target->Life == ECombatLife::Conscious && Target->bReactionAvailable && !IsIncapacitated(*Target))
        {
            // SRD Deflect Missiles: reaction, damage reduced by 1d10 + DEX modifier + monk level (catching the missile is not modelled).
            Target->bReactionAvailable = false;
            const int32 Cut = Roll(10) + AbilityModifier(TargetId, EAbility::Dexterity) + FBuildCatalog::ClassLevel(*Target, EAdventurerRole::Monk);
            Result.Damage = FMath::Max(0, Result.Damage - FMath::Max(0, Cut));
            Result.RollModeSources.Add(FString::Printf(TEXT("Deflect Missiles: -%d"), FMath::Max(0, Cut)));
        }
        if (const FActiveSpell* Weak = SpellExtraOn(ActorId, SpellEffectExtra::Enfeebled))
        {
            // SRD 5.2.1 Ray of Enfeeblement: the target's attacks deal 1d8 less damage.
            const int32 Cut = Roll(8);
            Result.Damage = FMath::Max(0, Result.Damage - Cut);
            Result.RollModeSources.Add(FString::Printf(TEXT("Ray of Enfeeblement -%d"), Cut));
        }
        Result.Damage = ResistanceCut(TargetId, Result.DamageType, Result.Damage);
        const int32 Protection = ProtectionFor(*Target);
        Result.DamagePrevented = FMath::Min(Result.Damage, Protection);
        ApplyDamage(*Target, Result.Damage);
        Result.Damage -= Result.DamagePrevented;
        if (GridDistance(Actor->Cell, Target->Cell) <= 1 && !RangedAttacker(*Actor))
        {
            State.MeleeHitThisTurn.AddUnique(TargetId);
            if (Result.bCritical) { State.MeleeCritThisTurn.AddUnique(TargetId); }
            SpellRetaliation(ActorId, TargetId, Result);
        }
        if (Result.Damage > 0) { SpellReactionsOnDamaged(ActorId, TargetId, Result); }
    }
}

int32 FCombatBattle::StrikeAbilitySwap(const FCombatantState& Actor) const
{
    // SRD 5.2.1 True Strike (this attack) and Shillelagh (club/quarterstaff): the spellcasting modifier replaces STR/DEX.
    const FWeaponProfile* Weapon = MainWeapon(Actor);
    int32 Dice = 1, Sides = 0;
    if (!Weapon || !(bTrueStrike || ShillelaghDie(Actor, Dice, Sides))) { return 0; }
    const FAbilityScores Scores = AbilityScores(Actor.Id);
    const int32 Normal = PactAbilityModifier(Actor, *Weapon, Scores);
    int32 Spell = AbilityModifier(Actor.Id, FSpellcastingRules::SpellcastingAbility(Actor.AdventurerRole));
    if (Actor.SecondaryLevel > 0) { Spell = FMath::Max(Spell, AbilityModifier(Actor.Id, FSpellcastingRules::SpellcastingAbility(Actor.SecondaryRole))); }
    return FMath::Max(0, Spell - Normal);
}

bool FCombatBattle::ShillelaghDie(const FCombatantState& Actor, int32& Count, int32& Sides) const
{
    if (Actor.MainHand != static_cast<int32>(EWeapon::Club) && Actor.MainHand != static_cast<int32>(EWeapon::Quarterstaff)) { return false; }
    if (!SpellEffectOn(Actor.Id, TEXT("shillelagh")) || Actor.WildShapeHP > 0) { return false; }
    // d8, d10 at level 5, d12 at 11, 2d6 at 17.
    Count = Actor.Level >= 17 ? 2 : 1;
    Sides = Actor.Level >= 17 ? 6 : Actor.Level >= 11 ? 12 : Actor.Level >= 5 ? 10 : 8;
    return true;
}

FCombatResult FCombatBattle::Resolve(const FCombatCommand& Command)
{
    FCombatResult Result;
    auto Reject = [&Result](const TCHAR* Reason) { Result.Feedback = Reason; return Result; };
    const FCombatantState* Actor = Find(Command.ActorId);
    if (!bStarted || State.Phase != EBattlePhase::Fighting) { return Reject(TEXT("Battle is not active.")); }
    if (!Actor || Actor->Life != ECombatLife::Conscious || ActiveId() != Command.ActorId) { return Reject(TEXT("Only the active combatant can act.")); }
    if (Command.Type == ECombatCommandType::Move)
    {
        Result.Path = PathTo(Command.ActorId, Command.Destination);
        if (Result.Path.IsEmpty()) { return Reject(TEXT("Cell is occupied, blocked, or beyond remaining movement.")); }
        // Each step costs StepCost (two into difficult terrain). Each step out of a melee enemy's reach may provoke its reaction (#59).
        FCombatantState* Moving = MutableFind(Command.ActorId);
        const FString MoverName = Moving->Name;
        const TArray<FIntPoint> Planned = Result.Path;
        Result.Path.Reset();
        FString Strikes;
        for (const FIntPoint& Next : Planned)
        {
            for (FCombatantState& Reactor : State.Encounter.Combatants)
            {
                if (Moving->Life != ECombatLife::Conscious) { break; }
                if (Reactor.Side == Moving->Side || Reactor.Life != ECombatLife::Conscious || IsIncapacitated(Reactor) || RangedAttacker(Reactor) ||
                    !FOpportunityRules::StepProvokes(Reactor.Cell, ActionRange(Reactor, ECombatCommandType::Attack), Moving->Cell, Next, Moving->bDisengaged, false, Reactor.bReactionAvailable)) { continue; }
                Reactor.bReactionAvailable = false;
                FCombatResult Swing;
                Strike(Reactor.Id, Moving->Id, 0, Swing);
                Result.Reactions.Add({Reactor.Id, Moving->Cell, Swing.NaturalRoll, Swing.AttackTotal, Swing.TargetArmorClass, Swing.bHit, Swing.bCritical, Swing.Damage});
                Strikes += FString::Printf(TEXT(" | %s opportunity attack: d20 %d = %d vs AC %d, %s%s"), *Reactor.Name, Swing.NaturalRoll, Swing.AttackTotal,
                    Swing.TargetArmorClass, Swing.bCritical ? TEXT("CRITICAL ") : TEXT(""), Swing.bHit ? *FString::Printf(TEXT("HIT %d"), Swing.Damage) : TEXT("MISS"));
            }
            if (Moving->Life != ECombatLife::Conscious) { break; }
            const FIntPoint From = Moving->Cell;
            Moving->Cell = Next;
            Result.Path.Add(Next);
            State.MovementLeft -= StepCost(Next);
            SpellsOnStep(Moving->Id, From, Next);
            if (Moving->Life != ECombatLife::Conscious) { break; }
        }
        // Ready (#60): a readied foe strikes when the mover ends its move within reach or range.
        for (FCombatantState& Reactor : State.Encounter.Combatants)
        {
            if (Result.Path.IsEmpty() || Moving->Life != ECombatLife::Conscious) { break; }
            if (!Reactor.bReadied || Reactor.Side == Moving->Side || Reactor.Life != ECombatLife::Conscious || IsIncapacitated(Reactor) || !Reactor.bReactionAvailable ||
                !CanTargetFrom(Reactor, Reactor.Cell, *Moving) || (NeedsAmmunition(Reactor) && Reactor.Ammunition <= 0)) { continue; }
            Reactor.bReadied = false;
            Reactor.bReactionAvailable = false;
            FCombatResult Swing;
            Strike(Reactor.Id, Moving->Id, 0, Swing);
            Result.Reactions.Add({Reactor.Id, Moving->Cell, Swing.NaturalRoll, Swing.AttackTotal, Swing.TargetArmorClass, Swing.bHit, Swing.bCritical, Swing.Damage});
            Strikes += FString::Printf(TEXT(" | %s readied attack: d20 %d = %d vs AC %d, %s%s"), *Reactor.Name, Swing.NaturalRoll, Swing.AttackTotal,
                Swing.TargetArmorClass, Swing.bCritical ? TEXT("CRITICAL ") : TEXT(""), Swing.bHit ? *FString::Printf(TEXT("HIT %d"), Swing.Damage) : TEXT("MISS"));
        }
        CheckOutcome();
        const int32 Steps = Result.Path.Num();
        // Presentation always receives a final cell, even when the first step was cut short.
        if (Result.Path.IsEmpty()) { Result.Path.Add(Moving->Cell); }
        Result.Feedback = FString::Printf(TEXT("%s moves %d cells (%d left)."), *MoverName, Steps, State.MovementLeft) + Strikes;
    }
    else if (FFeatureKit::IsFeature(Command.Type))
    {
        if (!LegalTargets(Command.ActorId, Command.Type).Contains(Command.TargetId)) { return Reject(TEXT("Class feature unavailable: uses, action budget, timing or target.")); }
        Result.Feedback = ResolveFeature(Command, Result);
    }
    else if (Command.Type == ECombatCommandType::Special1 || Command.Type == ECombatCommandType::Special2 || (Command.Type == ECombatCommandType::CureWounds && !Actor->BuildId.IsEmpty()))
    {
        if (!LegalTargets(Command.ActorId, Command.Type).Contains(Command.TargetId)) { return Reject(TEXT("Special unavailable: budget, stance, resource, target, range or sight.")); }
        for (const auto& A : FBuildCatalog::Abilities(*Actor))
        {
            if (A.Command != Command.Type) { continue; }
            auto* User = MutableFind(Command.ActorId); auto* Target = MutableFind(Command.TargetId);
            if (A.bKitResource) { User->KitUses -= A.ResourceCost; }
            if (A.Effect == EAbilityEffect::Swap)
            {
                const auto Cell = User->Cell; User->Cell = Target->Cell; Target->Cell = Cell;
                Result.Effects.Add({User->Id, A.Effect, 0, User->Cell});
                Result.Effects.Add({Target->Id, A.Effect, 0, Target->Cell});
            }
            else if (A.Effect == EAbilityEffect::Healing)
            {
                int32 Amount = A.Amount;
                if (A.bCureDice) { SpendSpellSlot(*User); State.bSlotSpentThisTurn = true; DiceLog = &Result.EffectDice; Result.EffectModifier = CureModifier(User->Id); Amount = FMath::Max(1, RollDice(CureDice,CureDie)+Result.EffectModifier); DiceLog = nullptr; }
                Result.Healing = FMath::Min(Amount, Target->MaxHP - Target->HP); Target->HP += Result.Healing;
                Result.Effects.Add({Target->Id, A.Effect, Result.Healing, Target->Cell});
            }
            else if (A.Effect == EAbilityEffect::Rage) { --User->RageUses; User->RageTurns = 2; }
            else if (A.Effect == EAbilityEffect::Protect)
            {
                User->GuardTargetId = Target->Id;
                if (A.bKitResource) { User->GuardAmount = A.Amount; User->GuardTurns = 1; }
            }
            else
            {
                int32 Amount = A.Amount;
                if (A.Effect == EAbilityEffect::Surge) { User->LastSurge = Result.SurgeOutcome = Roll(2); Amount = Result.SurgeOutcome * A.Amount; }
                TArray<int32> Targets = {Target->Id};
                if (A.Effect == EAbilityEffect::Area) { Targets = OrderedAreaTargets(*User,*Target); }
                for (int32 Id : Targets)
                {
                    auto* Victim = MutableFind(Id); const int32 Before = Victim->HP; const int32 Prevented = FMath::Min(Amount, ProtectionFor(*Victim));
                    ApplyDamage(*Victim, Amount); const int32 Lost = Before - Victim->HP;
                    Result.Damage += Lost; Result.bHit = true;
                    if (Prevented) { Result.Effects.Add({Id,EAbilityEffect::Mitigation,Prevented,Victim->Cell}); }
                    Result.Effects.Add({Id, A.Effect == EAbilityEffect::Surge ? EAbilityEffect::Surge : EAbilityEffect::Strike, Lost, Victim->Cell});
                    Result.DamagePrevented += Prevented;
                    if (A.Effect == EAbilityEffect::Push && Victim->Life == ECombatLife::Conscious)
                    {
                        const auto Delta = Victim->Cell - User->Cell;
                        const FIntPoint Step = FMath::Abs(Delta.X) >= FMath::Abs(Delta.Y) ? FIntPoint(FMath::Sign(Delta.X),0) : FIntPoint(0,FMath::Sign(Delta.Y));
                        if (IsOpen(Victim->Cell + Step, Id)) { Victim->Cell += Step; Result.Effects.Add({Id,EAbilityEffect::Push,0,Victim->Cell}); }
                    }
                }
            }
            if (A.Effect == EAbilityEffect::Protect || A.Effect == EAbilityEffect::Rage)
            { Result.Effects.Add({Target->Id, A.Effect, A.Effect == EAbilityEffect::Protect ? (A.Amount > 0 ? A.Amount : 3) : 2, Target->Cell}); }
            // An action strike is this turn's Attack action, so Extra Attack banks the remaining weapon attacks (#61).
            const bool bQuick = KitBonus(*User, A);
            if (!bQuick && State.bActionAvailable && (A.Effect == EAbilityEffect::Strike || A.Effect == EAbilityEffect::Push))
            { State.AttacksRemaining = AttacksPerAction(*User) - 1; }
            if (bQuick) { State.bBonusActionAvailable = false; if (!A.bBonus) { User->QuickenState = 2; } } else { State.bActionAvailable = false; }
            Result.Feedback = A.Name + TEXT(": ") + A.Description + FString::Printf(TEXT(" | committed HP loss %d, heal %d, prevented %d"), Result.Damage, Result.Healing, Result.DamagePrevented);
        }
        CheckOutcome();
    }
    else if (Command.Type == ECombatCommandType::OffHandAttack)
    {
        if (!LegalTargets(Command.ActorId, Command.Type).Contains(Command.TargetId)) { return Reject(TEXT("Off-hand attack needs the Attack action, two light weapons, a bonus action and an adjacent foe.")); }
        Strike(Command.ActorId, Command.TargetId, 0, Result, true);
        State.bBonusActionAvailable = false;
        CheckOutcome();
        Result.Feedback = FString::Printf(TEXT("%s off-hand > %s | %s | damage %d%s"), *Actor->Name, *Find(Command.TargetId)->Name, Result.bHit ? TEXT("HIT") : TEXT("MISS"), Result.Damage, *DefenseText(Result));
    }
    else if (Command.Type == ECombatCommandType::CastSpell)
    {
        if (!ResolveCatalogSpell(Command, Result)) { return Result; }
    }
    else if (Command.Type == ECombatCommandType::Attack || Command.Type == ECombatCommandType::PrecisionStrike)
    {
        if (!LegalTargets(Command.ActorId, Command.Type).Contains(Command.TargetId)) { return Reject(TEXT("No attack available, or target is inactive, friendly, out of range, or obscured.")); }
        FCombatantState* Target = State.Encounter.Combatants.FindByPredicate([&Command](const FCombatantState& Unit) { return Unit.Id == Command.TargetId; });
        const int32 ManeuverBonus = Command.Type != ECombatCommandType::PrecisionStrike ? 0 :
            State.Encounter.bSuperiorityDice ? Roll(FSuperiorityRules::DieSides(FBuildCatalog::ClassLevel(*Actor, EAdventurerRole::Fighter))) : PrecisionBonus;
        if (ManeuverBonus) { --MutableFind(Command.ActorId)->ManeuverUses; }
        Strike(Command.ActorId, Command.TargetId, ManeuverBonus, Result);
        if (State.bActionAvailable)
        {
            State.bActionAvailable = false;
            State.bAttackActionTaken = true;
            State.AttacksRemaining = AttacksPerAction(*Actor) - 1;
        }
        else { --State.AttacksRemaining; }
        CheckOutcome();
        FString TargetStatus = Target->Life == ECombatLife::Downed ? (State.Encounter.bDeathSaves ? FString(TEXT(" DOWNED (death saves)")) : FString::Printf(TEXT(" DOWNED (%d rescue turns)"), Target->RescueTurnsLeft)) :
            Target->Life == ECombatLife::Dead ? TEXT(" DEAD") : TEXT("");
        if (Result.bSneakAttack) { TargetStatus += FString::Printf(TEXT(" SNEAK +d%d"), SneakDie); }
        const FString ModeText = Result.AttackRolls.Num() > 1 ?
            FString::Printf(TEXT(" (%s %d/%d)"), FCheckRules::ModeName(Result.RollMode), Result.AttackRolls[0], Result.AttackRolls[1]) : FString();
        Result.Feedback = FString::Printf(TEXT("%s > %s: d20 %d%s + %d = %d vs AC %d%s | %s | %d damage%s. HP %d/%d%s"),
            *Actor->Name, *Target->Name, Result.NaturalRoll, *ModeText, WeaponAttackBonus(*this, *Actor), Result.AttackTotal, Result.TargetArmorClass,
            Result.CoverBonus ? *FString::Printf(TEXT(" (%s)"), CoverName(Result.CoverBonus)) : TEXT(""),
            Result.bCritical ? TEXT("CRITICAL") : Result.bHit ? TEXT("HIT") : TEXT("MISS"), Result.Damage, *DefenseText(Result),
            Target->HP, Target->MaxHP, *TargetStatus);
        if (Result.bSaveSpell)
        {
            Result.Feedback = Result.bCounterspelled ? FString::Printf(TEXT("%s > %s: Sacred Flame | %s"), *Actor->Name, *Target->Name, *Result.RollModeSources.Last()) :
                FString::Printf(TEXT("%s > %s: Sacred Flame | DEX save d20 %d + %d = %d vs DC %d | %s | %d radiant damage. HP %d/%d%s"), *Actor->Name, *Target->Name,
                    Result.NaturalRoll, Result.AttackBonus, Result.AttackTotal, Result.TargetArmorClass, Result.bHit ? TEXT("FAILED") : TEXT("SAVED"), Result.Damage, Target->HP, Target->MaxHP, *TargetStatus);
        }
    }
    else if (Command.Type == ECombatCommandType::SecondWind || Command.Type == ECombatCommandType::CureWounds ||
        Command.Type == ECombatCommandType::MagicMissile || Command.Type == ECombatCommandType::Rescue || Command.Type == ECombatCommandType::Potion)
    {
        if (!LegalTargets(Command.ActorId, Command.Type).Contains(Command.TargetId)) { return Reject(TEXT("Ability unavailable: check role, budget, uses, living target, range and sight.")); }
        FCombatantState* User = MutableFind(Command.ActorId);
        FCombatantState* Target = MutableFind(Command.TargetId);
        FString Ability = TEXT("Rescue");
        int32 Slot = 1;
        if (Command.Type == ECombatCommandType::MagicMissile || Command.Type == ECombatCommandType::CureWounds)
        {
            Slot = CombatSpellSlots::CastSlotLevel(*User, Command.SlotLevel);
            if (Slot == 0) { return Reject(TEXT("No spell slot of that level left.")); }
        }
        auto SpendSlot = [this, User, Slot]() { if (Slot == 1) { SpendSpellSlot(*User); } else { --User->SlotsByLevel[Slot - 1]; } State.bSlotSpentThisTurn = true; };
        if (Command.Type == ECombatCommandType::MagicMissile)
        {
            SpendSlot();
            if (const auto* B = FBuildCatalog::Find(User->BuildId)) { User->KitUses -= B->MissileKitCost; }
            const int32 Darts = MissileDarts + CombatSpellSlots::UpcastExtra(ESpell::MagicMissile, Slot);
            DiceLog = &Result.EffectDice;
            Result.EffectModifier = Darts * DartBonus;
            Result.Damage = RollDice(Darts, MissileDie) + Result.EffectModifier;
            DiceLog = nullptr;
            Result.bHit = true;
            ApplyDefenses(Result, Command.ActorId, Command.Type, Command.TargetId);
            const int32 Protection = ProtectionFor(*Target);
            Result.DamagePrevented = FMath::Min(Result.Damage, Protection);
            ApplyDamage(*Target, Result.Damage);
            Result.Damage -= Result.DamagePrevented;
            Ability = Slot == 1 ? FString::Printf(TEXT("Magic Missile (%d darts, 1 slot)"), Darts) : FString::Printf(TEXT("Magic Missile (%d darts, level %d slot)"), Darts, Slot);
        }
        else
        {
            int32 Amount = 1;
            DiceLog = &Result.EffectDice;
            if (Command.Type == ECombatCommandType::SecondWind) { --User->SecondWindUses; Result.EffectModifier = FMath::Max(1, FBuildCatalog::ClassLevel(*User, EAdventurerRole::Fighter)); Amount = Roll(WindDie) + Result.EffectModifier; Ability = TEXT("Second Wind (bonus, 1 use)"); }
            else if (Command.Type == ECombatCommandType::CureWounds) { SpendSlot(); Result.EffectModifier = CureModifier(User->Id); Amount = FMath::Max(1, RollDice(CureDice + CombatSpellSlots::UpcastExtra(ESpell::CureWounds, Slot), CureDie) + Result.EffectModifier); Ability = Slot == 1 ? FString(TEXT("Cure Wounds (1 slot)")) : FString::Printf(TEXT("Cure Wounds (level %d slot)"), Slot); }
            else if (Command.Type == ECombatCommandType::Potion) { --User->Potions; Result.EffectModifier = PotionBonus; Amount = RollDice(PotionDice, PotionDie) + PotionBonus; Ability = TEXT("Potion (1 bottle)"); }
            else { Result.EffectModifier = 1; }
            DiceLog = nullptr;
            Result.Healing = FMath::Min(Amount, Target->MaxHP - Target->HP);
            Target->HP += Result.Healing;
            Target->Life = ECombatLife::Conscious;
            Target->RescueTurnsLeft = 0; Target->DeathSaveSuccesses = 0; Target->DeathSaveFailures = 0;
        }
        if (Command.Type == ECombatCommandType::SecondWind) { State.bBonusActionAvailable = false; }
        else { State.bActionAvailable = false; }
        Result.Feedback = FString::Printf(TEXT("%s: %s > %s | heal %d, damage %d%s | HP %d/%d"), *User->Name, *Ability, *Target->Name, Result.Healing, Result.Damage, *DefenseText(Result), Target->HP, Target->MaxHP);
        CheckOutcome();
    }
    else if (CombatSpellSlots::IsAreaSpell(Command.Type))
    {
        if (!LegalTargets(Command.ActorId, Command.Type).Contains(Command.TargetId)) { return Reject(TEXT("Spell unavailable: check class list, slots, armor, and that the target is in the area.")); }
        FCombatantState* User = MutableFind(Command.ActorId);
        const int32 Slot = CombatSpellSlots::CastSlotLevel(*User, Command.SlotLevel);
        if (Slot == 0) { return Reject(TEXT("No spell slot of that level left.")); }
        const ESpell Spell = CombatSpellSlots::AreaSpell(Command.Type);
        const FSpellDelivery Delivery = FSpellcastingRules::Delivery(Spell);
        const FAreaTemplate Area = CombatSpellSlots::SpellArea(Command.Type, User->Cell, Find(Command.TargetId)->Cell);
        const int32 Dc = FSpellcastingRules::SaveDc(FProficiencyRules::ProficiencyBonus(User->Level), AbilityModifier(User->Id, FSpellcastingRules::SpellcastingAbility(User->AdventurerRole)));
        if (Slot == 1) { SpendSpellSlot(*User); } else { --User->SlotsByLevel[Slot - 1]; }
        State.bSlotSpentThisTurn = true;
        // One damage roll for the whole area, then a save per creature in it (allies too), in roster order.
        DiceLog = &Result.EffectDice;
        const int32 Rolled = RollDice(FSpellRules::UpcastDiceCount(Spell, Slot), FSpellRules::Facts(Spell).DieSides);
        DiceLog = nullptr;
        TArray<int32> Victims;
        for (const FCombatantState& Unit : State.Encounter.Combatants) { if (Unit.Id != User->Id && Unit.Life != ECombatLife::Dead && FAreaRules::Covers(Area, Unit.Cell) && HasLineOfSight(Area.Origin, Unit.Cell)) { Victims.Add(Unit.Id); } }
        FString Lines;
        for (const int32 VictimId : Victims)
        {
            const FCombatantState& Saver = *Find(VictimId);
            const bool bRolled = FCheckRules::Resolve(Character(VictimId).SaveBonus(Delivery.SaveAbility), Dc, SaveMode(Saver, Delivery.SaveAbility, ERollMode::Normal), State.RandomState).bSuccess;
            const bool bSaved = bRolled && !(IsHelpless(Saver) && (Delivery.SaveAbility == EAbility::Strength || Delivery.SaveAbility == EAbility::Dexterity));
            FCombatResult Part;
            Part.Damage = FSpellcastingRules::DamageAfterSave(Rolled, bSaved, Delivery.bHalfOnSave);
            ApplyDefenses(Part, User->Id, Command.Type, VictimId);
            FCombatantState* Victim = MutableFind(VictimId);
            const int32 Prevented = FMath::Min(Part.Damage, ProtectionFor(*Victim));
            ApplyDamage(*Victim, Part.Damage);
            const int32 Dealt = Part.Damage - Prevented;
            Result.Damage += Dealt; Result.DamagePrevented += Prevented;
            Result.Effects.Add({VictimId, EAbilityEffect::Area, Dealt, Victim->Cell});
            // Thunderwave: a failed save pushes the creature 10 feet straight away from the caster.
            if (Command.Type == ECombatCommandType::Thunderwave && !bSaved && Victim->Life != ECombatLife::Dead)
            {
                const FIntPoint Delta = Victim->Cell - User->Cell;
                const bool bAlongX = FMath::Abs(Delta.X) >= FMath::Abs(Delta.Y);
                const FIntPoint Step(bAlongX ? FMath::Sign(Delta.X) : 0, bAlongX ? 0 : FMath::Sign(Delta.Y));
                for (int32 Index = 0; Index < 2 && IsInside(Victim->Cell + Step) && IsOpen(Victim->Cell + Step, VictimId); ++Index) { Victim->Cell += Step; }
                Result.Effects.Add({VictimId, EAbilityEffect::Push, 0, Victim->Cell});
            }
            Lines += FString::Printf(TEXT(" | %s %s %d"), *Victim->Name, bSaved ? TEXT("saves,") : TEXT("fails,"), Dealt);
        }
        Result.bHit = true;
        Result.DamageType = FSpellRules::Facts(Spell).DamageType;
        State.bActionAvailable = false;
        Result.Feedback = FString::Printf(TEXT("%s: %s (level %d slot, DC %d)%s"), *User->Name, FSpellRules::Facts(Spell).Name, Slot, Dc, *Lines);
        CheckOutcome();
    }
    else if (Command.Type == ECombatCommandType::Shove || Command.Type == ECombatCommandType::Grapple)
    {
        if (!LegalTargets(Command.ActorId, Command.Type).Contains(Command.TargetId)) { return Reject(TEXT("Shove and grapple need an adjacent, conscious foe not already under that condition.")); }
        // SRD contest: attacker's Athletics against the target's better of Athletics or Acrobatics; ties hold.
        const bool bShove = Command.Type == ECombatCommandType::Shove;
        const FCharacterRules Defender = Character(Command.TargetId);
        const int32 ActorModifier = Character(Command.ActorId).SkillBonus(ESkill::Athletics);
        const int32 DefenderModifier = FMath::Max(Defender.SkillBonus(ESkill::Athletics), Defender.SkillBonus(ESkill::Acrobatics));
        // Contests are ability checks: poisoned or frightened sides roll twice and keep the lower die.
        auto ContestDie = [this](const FCombatantState& Unit, EAbility Ability)
        {
            ERollMode Mode = CheckMode(Unit, ERollMode::Normal);
            const int32 Bonus = SpellCheckBonus(Unit, Ability, Mode);
            const int32 First = Roll(20);
            return (Mode == ERollMode::Disadvantage ? FMath::Min(First, Roll(20)) : Mode == ERollMode::Advantage ? FMath::Max(First, Roll(20)) : First) + Bonus;
        };
        const int32 ActorRoll = ContestDie(*Actor, EAbility::Strength);
        const int32 DefenderRoll = ContestDie(*Find(Command.TargetId), Defender.SkillBonus(ESkill::Athletics) >= Defender.SkillBonus(ESkill::Acrobatics) ? EAbility::Strength : EAbility::Dexterity);
        const bool bWon = FCheckRules::ActorWinsContest(ActorRoll + ActorModifier, DefenderRoll + DefenderModifier);
        FCombatantState* Victim = MutableFind(Command.TargetId);
        if (bWon)
        {
            FCombatCondition Applied;
            Applied.Condition = bShove ? ECombatCondition::Prone : ECombatCondition::Grappled;
            Applied.SourceId = bShove ? INDEX_NONE : Command.ActorId;
            Victim->Conditions.Add(Applied);
        }
        State.bActionAvailable = false;
        Result.bHit = bWon;
        Result.Feedback = FString::Printf(TEXT("%s: %s > %s | Athletics %d vs %d | %s"), *Actor->Name, bShove ? TEXT("Shove") : TEXT("Grapple"), *Victim->Name,
            ActorRoll + ActorModifier, DefenderRoll + DefenderModifier, bWon ? (bShove ? TEXT("knocked prone") : TEXT("grappled")) : TEXT("resisted"));
    }
    else if (EStandardAction Standard; StandardActionOf(Command.Type, Standard))
    {
        if (!LegalTargets(Command.ActorId, Command.Type).Contains(Command.TargetId)) { return Reject(TEXT("Standard action unavailable: check action budget and target.")); }
        FCombatantState* User = MutableFind(Command.ActorId);
        // Cunning Action keeps the action free when the bonus action can pay instead.
        const bool bBonus = State.bBonusActionAvailable && FStandardActionRules::CanTakeAsBonusAction(Standard, EAdventurerRole::Rogue, FBuildCatalog::ClassLevel(*User, EAdventurerRole::Rogue));
        (bBonus ? State.bBonusActionAvailable : State.bActionAvailable) = false;
        FString Effect;
        switch (Command.Type)
        {
        case ECombatCommandType::Dash:
        {
            const bool bHeld = HasCondition(*User, ECombatCondition::Grappled) || HasCondition(*User, ECombatCondition::Restrained);
            const int32 Added = bHeld ? 0 : SpellSpeed(User->Id, FMath::Max(0, BaseSpeed(*this, *User) - ArmorPenaltiesOf(*this, *User).SpeedPenaltyFeet / FWeaponRules::FeetPerCell));
            State.MovementLeft += Added;
            Effect = FString::Printf(TEXT("dashes (+%d movement, %d left)"), Added, State.MovementLeft);
            break;
        }
        case ECombatCommandType::Disengage: User->bDisengaged = true; Effect = TEXT("disengages"); break;
        case ECombatCommandType::Dodge: User->bDodging = true; Effect = TEXT("dodges"); break;
        case ECombatCommandType::Ready: User->bReadied = true; Effect = TEXT("readies an attack"); break;
        case ECombatCommandType::Help:
        {
            FCombatantState* Target = MutableFind(Command.TargetId);
            Target->HelpedById = User->Id;
            Effect = FString::Printf(TEXT("helps against %s"), *Target->Name);
            break;
        }
        default:
        {
            // Hide: one seeded Stealth check against the best passive Perception among conscious foes.
            int32 Dc = 10;
            for (const FCombatantState& Other : State.Encounter.Combatants)
            {
                if (Other.Side != User->Side && Other.Life == ECombatLife::Conscious) { Dc = FMath::Max(Dc, 10 + Character(Other.Id).SkillBonus(ESkill::Perception)); }
            }
            ERollMode HideMode = CheckMode(*User, ERollMode::Normal);
            const int32 HideBonus = SpellCheckBonus(*User, EAbility::Dexterity, HideMode, true);
            const FD20Outcome Check = FCheckRules::Resolve(Character(User->Id).SkillBonus(ESkill::Stealth) + HideBonus, FMath::Clamp(Dc, FCheckRules::MinDc, FCheckRules::MaxDc), HideMode, State.RandomState);
            User->bHidden = Check.bSuccess;
            Result.bHit = Check.bSuccess;
            Result.NaturalRoll = Check.Kept;
            Effect = FString::Printf(TEXT("Stealth %d vs passive Perception %d: %s"), Check.Total, Dc, Check.bSuccess ? TEXT("hidden") : TEXT("seen"));
            break;
        }
        }
        Result.Feedback = FString::Printf(TEXT("%s %s%s."), *User->Name, *Effect, bBonus ? TEXT(" (bonus action)") : TEXT(""));
    }
    else if (Command.Type == ECombatCommandType::EndTurn)
    {
        Result.Feedback = Actor->Name + TEXT(" ends turn.");
        auto* User = MutableFind(Command.ActorId);
        if (User->RageTurns > 0 && --User->RageTurns == 0) { User->GuardTargetId = INDEX_NONE; }
        if (User->SacredWeaponTurns > 0) { --User->SacredWeaponTurns; }
        for (FCombatCondition& Condition : User->Conditions) { if (Condition.RoundsLeft > 0) { --Condition.RoundsLeft; } }
        User->Conditions.RemoveAll([](const FCombatCondition& Condition) { return Condition.RoundsLeft == 0; });
        SpellsOnTurnEnd(User->Id);
        CheckOutcome();
        if (State.Phase == EBattlePhase::Fighting) { AdvanceTurn(); }
    }
    else { return Reject(TEXT("Unknown command.")); }
    Result.bAccepted = true;
    return Result;
}

EDamageType FCombatBattle::DamageTypeOf(int32 ActorId, ECombatCommandType Action) const
{
    if (Action == ECombatCommandType::MagicMissile) { return EDamageType::Force; }
    if (Action == ECombatCommandType::BurningHands) { return EDamageType::Fire; }
    if (Action == ECombatCommandType::Thunderwave) { return EDamageType::Thunder; }
    const FCombatantState* Actor = Find(ActorId);
    if (!Actor) { return EDamageType::Slashing; }
    // Prebuilt profiles: longsword, shortbow, the cleric's radiant ranged profile, Fire Bolt; enemies by role.
    switch (Actor->AdventurerRole)
    {
    case EAdventurerRole::Fighter: return EDamageType::Slashing;
    case EAdventurerRole::Rogue: return EDamageType::Piercing;
    case EAdventurerRole::Cleric: return EDamageType::Radiant;
    case EAdventurerRole::Wizard: return EDamageType::Fire;
    default: return Actor->Role == TEXT("Archer") || Actor->Range > 1 ? EDamageType::Piercing : Actor->Role == TEXT("Thug") ? EDamageType::Bludgeoning : EDamageType::Slashing; // SRD: Scout longbow, Thug mace, Bandit scimitar
    }
}

FDamageDefenses FCombatBattle::DamageDefenses(int32 Id) const
{
    const FCombatantState* Unit = Find(Id);
    FDamageDefenses Defenses = Unit ? FDamageRules::ProfileDefenses(Unit->Role) : FDamageDefenses();
    if (Unit) { Defenses.Resistances |= SpellResistances(Id); }
    // SRD 5.2.1 Silence: creatures inside are immune to thunder damage.
    if (Unit && SpellZoneAt(Unit->Cell, 2)) { Defenses.Immunities |= static_cast<uint16>(1u << static_cast<uint8>(EDamageType::Thunder)); }
    return Defenses;
}

void FCombatBattle::ApplyDefenses(FCombatResult& Result, int32 ActorId, ECombatCommandType Action, int32 TargetId) const
{
    Result.DamageType = DamageTypeOf(ActorId, Action);
    const FDamageDefenses Defenses = DamageDefenses(TargetId);
    Result.DamageBeforeDefenses = Result.Damage;
    Result.DamageResponse = FDamageRules::Response(Result.DamageType, Defenses);
    Result.Damage = FDamageRules::Adjust(Result.Damage, Result.DamageType, Defenses);
}

bool FCombatBattle::ShieldWouldBlock(const FCombatantState& Target, int32 AttackTotal, int32 ArmorClass, bool bCritical)
{
    // Shield is on the Sorcerer and Wizard lists; a prepared list (if any) must hold it for a sorcerer.
    auto Has = [&](EAdventurerRole R) { return Target.AdventurerRole == R || (Target.SecondaryLevel > 0 && Target.SecondaryRole == R); };
    const bool bWizard = Has(EAdventurerRole::Wizard) || (Has(EAdventurerRole::Sorcerer) && (Target.PreparedSpells.IsEmpty() || Target.PreparedSpells.Contains(TEXT("shield"))));
    return !bCritical && bWizard && Target.Side == ECombatSide::Party && Target.Life == ECombatLife::Conscious && Target.bReactionAvailable &&
        !Target.bShielded && Target.SpellSlots > 0 && AttackTotal < ArmorClass + 5 &&
        FBuildCatalog::ValidResources([&] { FCombatantState After = Target; --After.SpellSlots; return After; }()); // never spend a slot a kit has reserved
}

FString FCombatBattle::DefenseText(const FCombatResult& Result)
{
    if (Result.bShieldReaction) { return TEXT(" | Shield (reaction, 1 slot): +5 AC"); }
    return Result.DamageResponse == EDamageResponse::Normal ? FString() :
        FString::Printf(TEXT(" %s (%s from %d)"), FDamageRules::TypeName(Result.DamageType), FDamageRules::ResponseName(Result.DamageResponse), Result.DamageBeforeDefenses);
}

FCombatantState* FCombatBattle::MutableFind(int32 Id)
{
    return State.Encounter.Combatants.FindByPredicate([Id](const FCombatantState& Unit) { return Unit.Id == Id; });
}

int32 FCombatBattle::ProtectionFor(const FCombatantState& Target) const
{
    int32 Protection = Target.RageTurns > 0 ? 1 : 0;
    for (const auto& U : State.Encounter.Combatants)
    { if (U.GuardTargetId == Target.Id) { Protection = FMath::Max(Protection, U.GuardAmount > 0 ? U.GuardAmount : 3); } }
    return Protection;
}

void FCombatBattle::ApplyDamage(FCombatantState& Target, int32 Damage)
{
    Damage = SpellDamageIntake(Target, Damage);
    if (Target.WildShapeHP > 0 && Damage > 0)
    {
        // SRD Wild Shape: the beast form takes the damage; at 0 the druid reverts and the excess carries over.
        const int32 Absorbed = FMath::Min(Target.WildShapeHP, Damage);
        Target.WildShapeHP -= Absorbed; Damage -= Absorbed;
    }
    const int32 Protection = ProtectionFor(Target);
    const int32 HPBefore = Target.HP;
    const int32 Real = FDeathSaveRules::AbsorbWithTemporary(Target.TemporaryHP, FMath::Max(0, Damage - Protection));
    Target.HP = FMath::Max(0, Target.HP - Real);
    if (Target.HP == 0 && HPBefore > 0) { DeathWardSaves(Target); }
    const bool bMassive = State.Encounter.bDeathSaves && FDeathSaveRules::InstantDeath(Real, HPBefore, Target.MaxHP);
    if (Target.bConcentrating && Target.HP > 0 && Real > 0)
    {
        // Rolls only while concentrating, so battles without concentration keep their seeded streams.
        if (!FCheckRules::Resolve(Character(Target.Id).SaveBonus(EAbility::Constitution), FConcentrationRules::SaveDc(Real),
            SaveMode(Target, EAbility::Constitution, FFeatureKit::HasInvocation(Target, EInvocation::EldritchMind) ? ERollMode::Advantage : ERollMode::Normal), State.RandomState).bSuccess) { EndConcentration(Target); }
    }
    if (Real > 0) { SpellEffectsOnDamaged(Target.Id, ActiveId()); SpellDamageShared(Target.Id, Real); }
    if (Target.HP == 0 && HPBefore > 0 && Target.Side == ECombatSide::Enemy)
    {
        // SRD 5.2.1 Fiend Patron 3, Dark One's Blessing: a fiend warlock who drops a foe, or sees one drop within 10 ft, gains CHA + Warlock level temp HP (min 1).
        const int32 Killer = ActiveId();
        for (FCombatantState& Lock : State.Encounter.Combatants)
        {
            if (Lock.Side != ECombatSide::Party || Lock.Life != ECombatLife::Conscious || Lock.BuildId != TEXT("warlock.fiend")) { continue; }
            const int32 WarlockLevel = FBuildCatalog::ClassLevel(Lock, EAdventurerRole::Warlock);
            const FIntPoint Gap = Lock.Cell - Target.Cell;
            if (WarlockLevel < 3 || (Lock.Id != Killer && FMath::Max(FMath::Abs(Gap.X), FMath::Abs(Gap.Y)) > 2)) { continue; }
            Lock.TemporaryHP = FMath::Max(Lock.TemporaryHP, FWarlockRules::DarkOnesBlessing(AbilityModifier(Lock.Id, EAbility::Charisma), WarlockLevel));
        }
    }
    if (Target.HP == 0)
    {
        Target.GuardTurns = 0; Target.GuardAmount = 0; Target.RageTurns = 0; Target.GuardTargetId = INDEX_NONE; Target.LastSurge = 0; Target.SacredWeaponTurns = 0;
        for (auto& U : State.Encounter.Combatants) { if (U.GuardTargetId == Target.Id) { U.GuardTargetId = INDEX_NONE; U.GuardTurns = 0; U.GuardAmount = 0; } }
        Target.bDodging = false; Target.bDisengaged = false; Target.bHidden = false; Target.bReadied = false; Target.HelpedById = INDEX_NONE;
        for (auto& U : State.Encounter.Combatants) { if (U.HelpedById == Target.Id) { U.HelpedById = INDEX_NONE; } }
        Target.Life = Target.Side == ECombatSide::Party ? ECombatLife::Downed : ECombatLife::Dead;
        if (bMassive) { Target.Life = ECombatLife::Dead; }
        Target.RescueTurnsLeft = Target.Life == ECombatLife::Downed ? 3 : 0;
        Target.DeathSaveSuccesses = 0; Target.DeathSaveFailures = 0; Target.TemporaryHP = 0; EndConcentration(Target);
    }
}

void FCombatBattle::CheckOutcome()
{
    bool bParty = false;
    bool bEnemy = false;
    for (const FCombatantState& Unit : State.Encounter.Combatants)
    {
        bParty |= Unit.Side == ECombatSide::Party && Unit.Life == ECombatLife::Conscious;
        bEnemy |= Unit.Side == ECombatSide::Enemy && Unit.Life == ECombatLife::Conscious;
    }
    State.Phase = !bParty ? EBattlePhase::Defeat : !bEnemy ? EBattlePhase::Victory : EBattlePhase::Fighting;
    // Rescue must happen during combat. A wipe or an unresolved victory deadline is permanent.
    if (State.Phase != EBattlePhase::Fighting) { const TArray<FActiveSpell> Ended = MoveTemp(State.ActiveSpells); State.ActiveSpells.Reset(); for (const FActiveSpell& A : Ended) { EndSpellEffects(A); } State.bSlotSpentThisTurn = false; }
    if (State.Phase != EBattlePhase::Fighting)
    {
        State.AttacksRemaining = 0;
        for (FCombatantState& Unit : State.Encounter.Combatants)
        {
            Unit.GuardTurns = 0; Unit.GuardAmount = 0; Unit.RageTurns = 0; Unit.GuardTargetId = INDEX_NONE; Unit.LastSurge = 0; Unit.SacredWeaponTurns = 0;
            Unit.bReactionAvailable = true;
            Unit.bDodging = false; Unit.bDisengaged = false; Unit.bHidden = false; Unit.bReadied = false; Unit.HelpedById = INDEX_NONE; Unit.bConcentrating = false; Unit.bShielded = false;
            if (Unit.Life == ECombatLife::Downed)
            {
                // With death saves, a stable hero survives a victory on 1 HP; anyone still dying is lost.
                const bool bSurvives = State.Phase == EBattlePhase::Victory && Unit.DeathSaveSuccesses >= 3;
                Unit.Life = bSurvives ? ECombatLife::Conscious : ECombatLife::Dead; Unit.HP = bSurvives ? 1 : 0;
                Unit.RescueTurnsLeft = 0; Unit.DeathSaveSuccesses = 0; Unit.DeathSaveFailures = 0;
            }
        }
    }
}

void FCombatBattle::DownedTurn(FCombatantState& Unit)
{
    if (!State.Encounter.bDeathSaves)
    {
        if (Unit.DeathSaveSuccesses >= 3) { return; } // Spare the Dying: stable
        if (--Unit.RescueTurnsLeft == 0) { Unit.Life = ECombatLife::Dead; }
        return;
    }
    if (Unit.DeathSaveSuccesses >= 3) { return; } // Stable: no more saves.
    FDeathSaveTally Tally;
    Tally.Successes = Unit.DeathSaveSuccesses; Tally.Failures = Unit.DeathSaveFailures;
    int32 DeathRoll = Roll(20);
    if (SpellExtraOn(Unit.Id, SpellEffectExtra::WisDeathAdvantage)) { DeathRoll = FMath::Max(DeathRoll, Roll(20)); } // Beacon of Hope
    const EDeathState Outcome = FDeathSaveRules::ApplySave(Tally, DeathRoll);
    Unit.DeathSaveSuccesses = Tally.Successes; Unit.DeathSaveFailures = Tally.Failures;
    if (Outcome == EDeathState::Dead) { Unit.Life = ECombatLife::Dead; Unit.RescueTurnsLeft = 0; Unit.DeathSaveSuccesses = 0; Unit.DeathSaveFailures = 0; }
    else if (Outcome == EDeathState::Revived) { Unit.Life = ECombatLife::Conscious; Unit.HP = 1; Unit.RescueTurnsLeft = 0; Unit.DeathSaveSuccesses = 0; Unit.DeathSaveFailures = 0; }
    else if (Outcome == EDeathState::Stable) { Unit.DeathSaveSuccesses = 3; }
}

void FCombatBattle::AdvanceTurn()
{
    for (int32 Count = 0; Count < State.TurnOrder.Num(); ++Count)
    {
        State.TurnIndex = (State.TurnIndex + 1) % State.TurnOrder.Num();
        if (State.TurnIndex == 0) { ++State.Round; }
        FCombatantState* Next = MutableFind(ActiveId());
        if (State.Round > 1) { for (FCombatantState& Unit : State.Encounter.Combatants) { Unit.bSurprised = false; } }
        if (Next->Life == ECombatLife::Downed) { AuraOfLifeRevives(*Next); }
        if (Next->Life == ECombatLife::Downed) { DownedTurn(*Next); }
        else if (Next->bSurprised) { Next->bSurprised = false; }
        else if (Next->Life == ECombatLife::Conscious)
        {
            Next->GuardTargetId = INDEX_NONE; Next->GuardAmount = 0; Next->GuardTurns = 0;
            State.bActionAvailable = true;
            State.bAttackActionTaken = false;
            State.AttacksRemaining = 0;
            State.bBonusActionAvailable = true;
            BeginTurn();
            return;
        }
    }
}

TArray<FCombatResult> FCombatBattle::PlayEnemyTurn()
{
    TArray<FCombatResult> Results;
    const int32 Id = ActiveId();
    const FCombatantState* Enemy = Find(Id);
    if (!bStarted || State.Phase != EBattlePhase::Fighting || !Enemy || Enemy->Side != ECombatSide::Enemy) { return Results; }
    const FMonsterStatBlock* Block = FMonsterRules::Find(Enemy->Role);
    auto SelfAction = [&](ECombatCommandType Type)
    {
        if (!State.bActionAvailable || !LegalTargets(Id, Type).Contains(Id)) { return false; }
        Results.Add(Submit({Type, Id, {}, Id}));
        return true;
    };
    // Fixed cell order search for the cheapest reachable firing cell; TMap iteration never chooses AI ties.
    auto FindFiringCell = [&](const TMap<FIntPoint, int32>& Costs) -> TOptional<FIntPoint>
    {
        TOptional<FIntPoint> Best;
        int32 BestCost = MAX_int32;
        for (int32 Y = 0; Y < State.Encounter.Height; ++Y)
        {
            for (int32 X = 0; X < State.Encounter.Width; ++X)
            {
                const FIntPoint Cell(X, Y);
                const int32* Cost = Costs.Find(Cell);
                if (!Cost || *Cost >= BestCost) { continue; }
                for (const FCombatantState& Target : State.Encounter.Combatants)
                {
                    if (CanTargetFrom(*Find(Id), Cell, Target)) { Best = Cell; BestCost = *Cost; break; }
                }
            }
        }
        return Best;
    };
    auto StepToward = [&]()
    {
        TMap<FIntPoint, FIntPoint> Parents;
        const TMap<FIntPoint, int32> Costs = Distances(Id, &Parents);
        const TOptional<FIntPoint> Best = FindFiringCell(Costs);
        if (!Best.IsSet() || Costs[Best.GetValue()] == 0 || State.MovementLeft <= 0) { return Best.IsSet(); }
        FIntPoint Destination = Best.GetValue();
        while (Costs[Destination] > State.MovementLeft) { Destination = Parents[Destination]; }
        if (Destination != Find(Id)->Cell) { Results.Add(Submit({ECombatCommandType::Move, Id, Destination})); }
        return true;
    };
    auto NextToHero = [&]()
    {
        const FCombatantState& Self = *Find(Id);
        return State.Encounter.Combatants.ContainsByPredicate([&](const FCombatantState& U)
            { return U.Side != Self.Side && U.Life == ECombatLife::Conscious && GridDistance(U.Cell, Self.Cell) <= 1; });
    };
    // Skirmishers (Scouts, archers, the Acolyte) that are bloodied in melee Disengage and fall back instead of provoking.
    if (Block && Block->bSkirmisher && Enemy->HP * 2 < Enemy->MaxHP && NextToHero() && SelfAction(ECombatCommandType::Disengage))
    {
        const TMap<FIntPoint, int32> Costs = Distances(Id);
        TOptional<FIntPoint> Retreat; int32 BestGap = 1;
        for (int32 Y = 0; Y < State.Encounter.Height; ++Y)
        {
            for (int32 X = 0; X < State.Encounter.Width; ++X)
            {
                const int32* Cost = Costs.Find({X, Y});
                if (!Cost || *Cost > State.MovementLeft) { continue; }
                int32 Gap = MAX_int32;
                for (const FCombatantState& U : State.Encounter.Combatants)
                {
                    if (U.Side == ECombatSide::Party && U.Life == ECombatLife::Conscious) { Gap = FMath::Min(Gap, GridDistance(U.Cell, FIntPoint(X, Y))); }
                }
                if (Gap > BestGap) { BestGap = Gap; Retreat = FIntPoint(X, Y); }
            }
        }
        if (Retreat.IsSet()) { Results.Add(Submit({ECombatCommandType::Move, Id, Retreat.GetValue()})); }
        if (State.Phase == EBattlePhase::Fighting) { Results.Add(Submit({ECombatCommandType::EndTurn, Id})); }
        return Results;
    }
    bool bReachable = true;
    if (LegalTargets(Id).IsEmpty() && State.MovementLeft > 0 && State.bActionAvailable) { bReachable = StepToward(); }
    // Still out of reach: Dash toward the nearest firing cell.
    if (bReachable && LegalTargets(Id).IsEmpty() && State.Phase == EBattlePhase::Fighting && SelfAction(ECombatCommandType::Dash)) { StepToward(); }
    TArray<int32> Targets = LegalTargets(Id);
    // Multiattack: a stat block's attacks per Attack action (Thug, Scout); ordinary foes swing once.
    for (int32 Swing = 0; !Targets.IsEmpty() && State.Phase == EBattlePhase::Fighting; ++Swing)
    {
        if (Swing > 0 && State.AttacksRemaining <= 0) { break; }
        const FCombatResult Swung = Submit({ECombatCommandType::Attack, Id, {}, Targets[0]});
        Results.Add(Swung);
        if (!Swung.bAccepted) { break; }
        Targets = LegalTargets(Id);
    }
    // No hero in reach at all, even dashing: ranged foes Hide, melee foes Dodge.
    if (!bReachable && State.Phase == EBattlePhase::Fighting && !SelfAction(Find(Id)->Range > 1 ? ECombatCommandType::Hide : ECombatCommandType::Dodge)) { SelfAction(ECombatCommandType::Dodge); }
    if (State.Phase == EBattlePhase::Fighting) { Results.Add(Submit({ECombatCommandType::EndTurn, Id})); }
    return Results;
}
FCombatEncounter FCombatBattle::TutorialEncounter()
{
    FCombatEncounter Encounter;
    Encounter.BlockedCells = {{3, 3}, {4, 3}, {3, 4}};
    const TCHAR* Names[] = {TEXT("Arden"), TEXT("Nix"), TEXT("Sera"), TEXT("Ivo"), TEXT("Briar raider"), TEXT("Ash raider"), TEXT("Moss archer")};
    const TCHAR* Roles[] = {TEXT("Fighter"), TEXT("Rogue"), TEXT("Cleric"), TEXT("Wizard"), TEXT("Raider"), TEXT("Raider"), TEXT("Archer")};
    const FIntPoint Cells[] = {{1, 1}, {2, 1}, {1, 2}, {2, 2}, {5, 5}, {6, 5}, {5, 6}};
    for (int32 Index = 0; Index < 7; ++Index)
    {
        FCombatantState Unit;
        Unit.Id = Index + 1;
        Unit.Name = Names[Index]; Unit.Role = Roles[Index]; Unit.Cell = Cells[Index];
        Unit.Side = Index < 4 ? ECombatSide::Party : ECombatSide::Enemy;
        Unit.MaxHP = Unit.HP = Index == 0 ? 30 : Index < 4 ? 22 : 16;
        if (Index < 4)
        {
            const TCHAR* Builds[] = {TEXT("fighter.champion"), TEXT("rogue.thief"), TEXT("cleric.life"), TEXT("wizard.evocation")};
            FBuildCatalog::Create(Builds[Index], Index + 1, Names[Index], Unit);
            Unit.Cell = Cells[Index];
            // Historical standalone fixture is a base Fighter; expedition chooses a subclass.
            if (Index == 0) { Unit.BuildId.Reset(); Unit.ClassId.Reset(); Unit.SubclassId.Reset(); Unit.FighterSubclass = EFighterSubclass::None; }
        }
        else
        {
            Unit.ArmorClass = 12; Unit.Range = Index == 6 ? 4 : 1;
            Unit.InitiativeBonus = 1; Unit.AttackBonus = 3; Unit.DamageBonus = 1;
        }
        Encounter.Combatants.Add(Unit);
    }
    return Encounter;
}

bool FCombatBattle::Restore(const FCombatSnapshot& Snapshot, FString& Error)
{
    if (!ValidateEncounter(Snapshot.Encounter, Error)) { return false; }
    auto Reject = [&Error]() { Error = TEXT("Invalid saved combat phase, initiative, turn or budget."); return false; };
    if (static_cast<uint8>(Snapshot.Phase) > static_cast<uint8>(EBattlePhase::Defeat) ||
        Snapshot.Round < 1 || Snapshot.Round > 1000000 || Snapshot.TurnOrder.IsEmpty() ||
        Snapshot.TurnOrder.Num() > Snapshot.Encounter.Combatants.Num() || !Snapshot.TurnOrder.IsValidIndex(Snapshot.TurnIndex) ||
        (Snapshot.bAttackActionTaken && Snapshot.bActionAvailable)) { return Reject(); }
    FCombatBattle Candidate;
    Candidate.State = Snapshot;
    Candidate.bStarted = true;
    TSet<int32> Ordered;
    const FCombatantState* Previous = nullptr;
    for (int32 Id : Snapshot.TurnOrder)
    {
        const auto* Unit = Candidate.Find(Id);
        if (!Unit || Ordered.Contains(Id) || (Previous && (Previous->Initiative < Unit->Initiative ||
            (Previous->Initiative == Unit->Initiative && Previous->Id > Unit->Id)))) { return Reject(); }
        Ordered.Add(Id); Previous = Unit;
    }
    bool bParty = false, bEnemy = false;
    for (const auto& Unit : Snapshot.Encounter.Combatants)
    {
        if (Unit.Name.IsEmpty() || Unit.Name.Len() > 80 || Unit.Role.IsEmpty() || Unit.Role.Len() > 80 ||
            Unit.Initiative < 1 + Unit.InitiativeBonus || Unit.Initiative > 20 + Unit.InitiativeBonus ||
            (Unit.Life != ECombatLife::Dead && !Ordered.Contains(Unit.Id)) ||
            (Unit.Side == ECombatSide::Enemy && (Unit.AdventurerRole != EAdventurerRole::None || Unit.Life == ECombatLife::Downed)) ||
            (Snapshot.Phase != EBattlePhase::Fighting && (Unit.Life == ECombatLife::Downed || Unit.RageTurns != 0 || Unit.GuardTargetId != INDEX_NONE || Unit.LastSurge != 0 || Unit.GuardTurns != 0 || Unit.GuardAmount != 0 || !Unit.bReactionAvailable ||
                Unit.bDodging || Unit.bReckless || Unit.SacredWeaponTurns != 0 || Unit.QuickenState != 0 || Unit.bShielded || Unit.bDisengaged || Unit.bHidden || Unit.bReadied || Unit.HelpedById != INDEX_NONE ||
                Unit.Conditions.ContainsByPredicate([](const FCombatCondition& C) { return C.SourceId != INDEX_NONE || C.Condition == ECombatCondition::Prone; })))) { return Reject(); }
        bParty |= Unit.Side == ECombatSide::Party && Unit.Life == ECombatLife::Conscious;
        bEnemy |= Unit.Side == ECombatSide::Enemy && Unit.Life == ECombatLife::Conscious;
    }
    const auto Expected = !bParty ? EBattlePhase::Defeat : !bEnemy ? EBattlePhase::Victory : EBattlePhase::Fighting;
    const auto* Active = Candidate.Find(Candidate.ActiveId());
    // A battle can end on the turn of a foe that died to a reaction (opportunity attack), so only a live fight needs a conscious active unit.
    if (Snapshot.Phase != Expected || !Active || (Snapshot.Phase == EBattlePhase::Fighting && Active->Life != ECombatLife::Conscious) ||
        Snapshot.MovementLeft < 0 || Snapshot.MovementLeft > Active->Speed * 3 ||
        Snapshot.AttacksRemaining < 0 || Snapshot.AttacksRemaining >= AttacksPerAction(*Active) ||
        (Snapshot.AttacksRemaining > 0 && (Snapshot.bActionAvailable || Snapshot.Phase != EBattlePhase::Fighting)) ||
        Snapshot.SneakAttackUsedBy.ContainsByPredicate([&](int32 SneakId) { return !Candidate.Find(SneakId) || Snapshot.SneakAttackUsedBy.FilterByPredicate([SneakId](int32 Other) { return Other == SneakId; }).Num() > 1; }) ||
        (!Snapshot.bBonusActionAvailable && ([&]()
        {
            if (FFeatureKit::HasBonusFeature(*Active)) { return false; } // class features (FFeatureKit) can spend it
            // Cunning Action (#60) can spend the bonus action on Dash, Disengage or Hide.
            if (FStandardActionRules::CanTakeAsBonusAction(EStandardAction::Dash, Active->AdventurerRole, Active->Level)) { return false; }
            const auto* Build = FBuildCatalog::Find(Active->BuildId);
            if (Build && Build->KitCap > 0)
            { return (Snapshot.Phase == EBattlePhase::Fighting && Active->KitUses >= Build->KitCap) || Active->QuickenState == 2 || !FBuildCatalog::Abilities(*Active).ContainsByPredicate([](const auto& A) { return A.bBonus; }); }
            if (Active->AdventurerRole == EAdventurerRole::Barbarian)
            { return Snapshot.Phase == EBattlePhase::Fighting && Active->RageTurns == 0; }
            return Active->AdventurerRole != EAdventurerRole::Fighter || (Snapshot.Phase == EBattlePhase::Fighting && Active->SecondWindUses > 1);
        })())) { return Reject(); }
    for (const auto& Unit : Snapshot.Encounter.Combatants) { if (!Candidate.ValidPreparedSpells(Unit, Error)) { return false; } }
    if (Snapshot.Phase != EBattlePhase::Fighting && (!Snapshot.ActiveSpells.IsEmpty() || Snapshot.bSlotSpentThisTurn)) { return Reject(); }
    TSet<int32> Serials;
    for (const FActiveSpell& A : Snapshot.ActiveSpells)
    {
        const FSrdSpell* S = FSpellCatalogRules::Find(A.SpellId);
        const FCombatantState* Caster = Candidate.Find(A.CasterId);
        if (!S || A.SpellId != S->Id || !Caster || Caster->Life == ECombatLife::Dead || (A.TargetId != INDEX_NONE && !Candidate.Find(A.TargetId)) ||
            A.SlotLevel < 0 || A.SlotLevel > 9 || A.RoundsLeft < -1 || A.RoundsLeft == 0 || A.RoundsLeft > 144000 || A.RadiusCells < 0 || A.RadiusCells > 64 ||
            A.Serial < 1 || A.Serial >= Snapshot.NextSpellSerial || Serials.Contains(A.Serial) || A.bConcentration != S->bConcentration ||
            (A.bConcentration && !Caster->bConcentrating) || A.TriggeredThisTurn.ContainsByPredicate([&](int32 Id) { return !Candidate.Find(Id); })) { return Reject(); }
        Serials.Add(A.Serial);
    }
    if (Snapshot.NextSpellSerial < 1) { return Reject(); }
    *this = MoveTemp(Candidate);
    Error.Reset(); return true;
}

bool FCombatBattle::FeatureReady(const FCombatantState& Unit, ECombatCommandType Action) const
{
    if (!FFeatureKit::Commands(Unit).Contains(Action)) { return false; }
    switch (Action)
    {
    case ECombatCommandType::ActionSurge: return Unit.ActionSurgeUses > 0 && !State.bActionAvailable && State.AttacksRemaining == 0;
    case ECombatCommandType::TurnUndead: return Unit.ChannelUses > 0 && State.bActionAvailable && State.AttacksRemaining == 0;
    case ECombatCommandType::BardicInspiration: return Unit.BardicUses > 0 && State.bBonusActionAvailable;
    case ECombatCommandType::WildShape: return Unit.WildShapeUses > 0 && Unit.WildShapeHP == 0 && State.bActionAvailable && State.AttacksRemaining == 0;
    case ECombatCommandType::FlurryOfBlows: return Unit.KiPoints > 0 && State.bBonusActionAvailable && State.bAttackActionTaken;
    case ECombatCommandType::MartialArts: return State.bBonusActionAvailable && State.bAttackActionTaken;
    case ECombatCommandType::QuickenedSpell: return Unit.QuickenState == 0 && Unit.SorceryPoints >= FFeatureKit::QuickenCost && State.bBonusActionAvailable && Unit.KitUses > 0;
    case ECombatCommandType::FlexibleCasting: return Unit.KitUses > 0 && Unit.SorceryPoints + FFeatureKit::FontCost <= FFeatureKit::SorceryCap(Unit) && State.bBonusActionAvailable;
    case ECombatCommandType::SacredWeapon: return Unit.ChannelUses > 0 && Unit.SacredWeaponTurns == 0 && State.bActionAvailable && State.AttacksRemaining == 0;
    case ECombatCommandType::PatientDefense: case ECombatCommandType::StepOfTheWind: return Unit.KiPoints > 0 && State.bBonusActionAvailable;
    case ECombatCommandType::RecklessAttack: return !Unit.bReckless && State.bActionAvailable && !State.bAttackActionTaken && State.AttacksRemaining == 0;
    case ECombatCommandType::FontOfMagic:
    {
        const auto* Build = FBuildCatalog::Find(Unit.BuildId);
        return Unit.SorceryPoints >= FFeatureKit::FontCost && State.bBonusActionAvailable && Build && Unit.KitUses < Build->KitCap;
    }
    default: return false;
    }
}

TArray<int32> FCombatBattle::FeatureTargets(const FCombatantState& Unit, ECombatCommandType Action) const
{
    if (Action == ECombatCommandType::BardicInspiration)
    {
        // SRD: one creature other than you within 60 feet who can hear you; one die held at a time.
        TArray<int32> Allies;
        for (const FCombatantState& Other : State.Encounter.Combatants)
        {
            if (Other.Id != Unit.Id && Other.Side == Unit.Side && Other.Life == ECombatLife::Conscious && Other.BardicDie == 0 && GridDistance(Unit.Cell, Other.Cell) <= FFeatureKit::BardicCells) { Allies.Add(Other.Id); }
        }
        return Allies;
    }
    if (Action == ECombatCommandType::FlurryOfBlows || Action == ECombatCommandType::MartialArts)
    {
        TArray<int32> Foes;
        for (const FCombatantState& Other : State.Encounter.Combatants)
        {
            if (Other.Side != Unit.Side && Other.Life == ECombatLife::Conscious && GridDistance(Unit.Cell, Other.Cell) <= 1 && HasLineOfSight(Unit.Cell, Other.Cell)) { Foes.Add(Other.Id); }
        }
        return Foes;
    }
    return {Unit.Id};
}

FString FCombatBattle::ResolveFeature(const FCombatCommand& Command, FCombatResult& Result)
{
    FCombatantState* User = MutableFind(Command.ActorId);
    switch (Command.Type)
    {
    case ECombatCommandType::ActionSurge:
        --User->ActionSurgeUses;
        State.bActionAvailable = true;
        State.bAttackActionTaken = false;
        return FString::Printf(TEXT("%s uses Action Surge: one more action this turn (%d left)."), *User->Name, User->ActionSurgeUses);
    case ECombatCommandType::WildShape:
        --User->WildShapeUses;
        State.bActionAvailable = false;
        User->WildShapeHP = FFeatureKit::WolfHP;
        return FString::Printf(TEXT("%s takes the shape of a wolf (%d form HP, AC %d; %d uses left)."), *User->Name, User->WildShapeHP, FFeatureKit::WolfAC, User->WildShapeUses);
    case ECombatCommandType::RecklessAttack:
        User->bReckless = true;
        return FString::Printf(TEXT("%s attacks recklessly: advantage on melee attacks this turn, attacks against %s have advantage until its next turn."), *User->Name, *User->Name);
    case ECombatCommandType::FontOfMagic:
        User->SorceryPoints -= FFeatureKit::FontCost;
        State.bBonusActionAvailable = false;
        ++User->KitUses;
        return FString::Printf(TEXT("%s turns %d sorcery points into a spell use (%d uses, %d points left) (bonus action)."), *User->Name, FFeatureKit::FontCost, User->KitUses, User->SorceryPoints);
    case ECombatCommandType::PatientDefense:
    case ECombatCommandType::StepOfTheWind:
        --User->KiPoints;
        State.bBonusActionAvailable = false;
        (Command.Type == ECombatCommandType::PatientDefense ? User->bDodging : User->bDisengaged) = true;
        return FString::Printf(TEXT("%s spends 1 ki and %s (%d ki left) (bonus action)."), *User->Name, Command.Type == ECombatCommandType::PatientDefense ? TEXT("dodges") : TEXT("disengages"), User->KiPoints);
    case ECombatCommandType::QuickenedSpell:
        User->SorceryPoints -= FFeatureKit::QuickenCost;
        User->QuickenState = 1;
        return FString::Printf(TEXT("%s quickens the next spell: it costs the bonus action (%d points left)."), *User->Name, User->SorceryPoints);
    case ECombatCommandType::FlexibleCasting:
        --User->KitUses;
        User->SorceryPoints += FFeatureKit::FontCost;
        State.bBonusActionAvailable = false;
        return FString::Printf(TEXT("%s converts a spell use into %d sorcery points (%d uses, %d points left) (bonus action)."), *User->Name, FFeatureKit::FontCost, User->KitUses, User->SorceryPoints);
    case ECombatCommandType::SacredWeapon:
        --User->ChannelUses;
        State.bActionAvailable = false;
        User->SacredWeaponTurns = FFeatureKit::SacredWeaponRounds;
        return FString::Printf(TEXT("%s channels divinity into a Sacred Weapon: +%d to weapon attacks for %d rounds (%d channel left)."), *User->Name, FMath::Max(1, AbilityModifier(User->Id, EAbility::Charisma)), User->SacredWeaponTurns, User->ChannelUses);
    case ECombatCommandType::MartialArts:
    {
        State.bBonusActionAvailable = false;
        FCombatResult Swing; Swing.bAccepted = true;
        Strike(Command.ActorId, Command.TargetId, 0, Swing);
        Result.bHit = Swing.bHit; Result.Damage = Swing.bHit ? Swing.Damage : 0;
        return FString::Printf(TEXT("%s follows up with a Martial Arts strike (bonus action): %s %d."), *User->Name, Swing.bHit ? TEXT("hit") : TEXT("miss"), Result.Damage);
    }
    case ECombatCommandType::FlurryOfBlows:
    {
        --User->KiPoints;
        State.bBonusActionAvailable = false;
        FString Text = FString::Printf(TEXT("%s spends 1 ki on a Flurry of Blows (%d ki left) (bonus action):"), *User->Name, User->KiPoints);
        int32 Total = 0, Hits = 0;
        for (int32 Blow = 0; Blow < 2; ++Blow)
        {
            const FCombatantState* Target = Find(Command.TargetId);
            if (!Target || Target->Life != ECombatLife::Conscious) { break; }
            FCombatResult Swing; Swing.bAccepted = true;
            Strike(Command.ActorId, Command.TargetId, 0, Swing);
            Text += FString::Printf(TEXT(" %s %d"), Swing.bHit ? TEXT("hit") : TEXT("miss"), Swing.Damage);
            if (Swing.bHit) { ++Hits; Total += Swing.Damage; }
        }
        Result.bHit = Hits > 0; Result.Damage = Total;
        return Text + TEXT(".");
    }
    case ECombatCommandType::BardicInspiration:
    {
        --User->BardicUses;
        State.bBonusActionAvailable = false;
        FCombatantState* Ally = MutableFind(Command.TargetId);
        Ally->BardicDie = FFeatureKit::BardicSides(*User);
        return FString::Printf(TEXT("%s inspires %s (d%d held; %d left) (bonus action)."), *User->Name, *Ally->Name, Ally->BardicDie, User->BardicUses);
    }
    case ECombatCommandType::TurnUndead:
    {
        --User->ChannelUses;
        State.bActionAvailable = false;
        const int32 Dc = FSpellcastingRules::SaveDc(FProficiencyRules::ProficiencyBonus(User->Level), AbilityModifier(User->Id, EAbility::Wisdom));
        int32 Turned = 0, Saved = 0;
        for (FCombatantState& Other : State.Encounter.Combatants)
        {
            if (Other.Side == User->Side || Other.Life != ECombatLife::Conscious || !FFeatureKit::IsUndead(Other)) { continue; }
            if (GridDistance(User->Cell, Other.Cell) > FFeatureKit::TurnUndeadCells || !HasLineOfSight(User->Cell, Other.Cell)) { continue; }
            const bool bSaved = FCheckRules::Resolve(Character(Other.Id).SaveBonus(EAbility::Wisdom), Dc, SaveMode(Other, EAbility::Wisdom, ERollMode::Normal), State.RandomState).bSuccess;
            if (bSaved) { ++Saved; continue; }
            FCombatCondition Fear; Fear.Condition = ECombatCondition::Frightened; Fear.SourceId = User->Id; Fear.RoundsLeft = FFeatureKit::TurnUndeadRounds;
            Other.Conditions.Add(Fear); ++Turned;
        }
        Result.bHit = Turned > 0;
        return FString::Printf(TEXT("%s presents a holy symbol (DC %d): %d undead turned, %d resisted (%d channel left)."), *User->Name, Dc, Turned, Saved, User->ChannelUses);
    }
    default: return FString();
    }
}

TArray<int32> FCombatBattle::OrderedAreaTargets(const FCombatantState& Actor, const FCombatantState& Target) const
{
    TArray<int32> Targets={Target.Id};
    for(const auto& Other:State.Encounter.Combatants)
    {
        if(Other.Id!=Target.Id && Other.Side!=Actor.Side && Other.Life==ECombatLife::Conscious && GridDistance(Target.Cell,Other.Cell)<=1 && HasLineOfSight(Target.Cell,Other.Cell)) { Targets.Add(Other.Id); }
    }
    return Targets;
}

namespace CombatSpellSlots
{
/** Catalog spell area on the grid: cones, lines and self cubes start at the caster and point at the aim; spheres and cylinders centre on the aim (or the caster for Self); a cube cast at range is treated as a sphere of half its side around the aim. */
bool CatalogArea(const FSrdSpell& Spell, FIntPoint Caster, FIntPoint Aim, FAreaTemplate& Out)
{
    if (Spell.Area == ESpellArea::None || Spell.AreaFeet <= 0) { return false; }
    const bool bSelf = Spell.RangeFeet == 0;
    Out.SizeFeet = Spell.AreaFeet;
    switch (Spell.Area)
    {
    case ESpellArea::Cone: Out.Shape = EAreaShape::Cone; break;
    case ESpellArea::Line: Out.Shape = EAreaShape::Line; break;
    case ESpellArea::Cube: Out.Shape = bSelf ? EAreaShape::Cube : EAreaShape::Sphere; if (!bSelf) { Out.SizeFeet = FMath::Max(FAreaRules::FeetPerCell, Spell.AreaFeet / 2); } break;
    case ESpellArea::Cylinder: Out.Shape = EAreaShape::Cylinder; break;
    default: Out.Shape = EAreaShape::Sphere; break;
    }
    if (Out.Shape == EAreaShape::Cone || Out.Shape == EAreaShape::Line || Out.Shape == EAreaShape::Cube)
    {
        Out.Origin = Caster;
        Out.Direction = Aim == Caster ? FIntPoint(1, 0) : Aim - Caster;
    }
    else { Out.Origin = bSelf ? Caster : Aim; }
    return true;
}
bool ToCombatCondition(ESpellCondition In, ECombatCondition& Out)
{
    switch (In)
    {
    case ESpellCondition::Blinded: Out = ECombatCondition::Blinded; return true;
    case ESpellCondition::Frightened: Out = ECombatCondition::Frightened; return true;
    case ESpellCondition::Grappled: Out = ECombatCondition::Grappled; return true;
    case ESpellCondition::Incapacitated: Out = ECombatCondition::Incapacitated; return true;
    case ESpellCondition::Paralyzed: Out = ECombatCondition::Paralyzed; return true;
    case ESpellCondition::Poisoned: Out = ECombatCondition::Poisoned; return true;
    case ESpellCondition::Prone: Out = ECombatCondition::Prone; return true;
    case ESpellCondition::Restrained: Out = ECombatCondition::Restrained; return true;
    case ESpellCondition::Stunned: Out = ECombatCondition::Stunned; return true;
    case ESpellCondition::Unconscious: Out = ECombatCondition::Unconscious; return true;
    case ESpellCondition::Charmed: Out = ECombatCondition::Charmed; return true;
    case ESpellCondition::Deafened: Out = ECombatCondition::Deafened; return true;
    case ESpellCondition::Invisible: Out = ECombatCondition::Invisible; return true;
    case ESpellCondition::Petrified: Out = ECombatCondition::Petrified; return true;
    default: return false;
    }
}
}

const FSrdSpell* FCombatBattle::UsableSpell(const FCombatantState& Unit, const FString& SpellId, int32 Requested, int32& OutSlot, EAdventurerRole& OutClass) const
{
    OutSlot = 0;
    const FSrdSpell* Spell = FSpellCatalogRules::Find(SpellId);
    if (!Spell || !bStarted || State.Phase != EBattlePhase::Fighting || Unit.Side != ECombatSide::Party || Unit.Life != ECombatLife::Conscious ||
        IsIncapacitated(Unit) || ActiveId() != Unit.Id || !FSpellCatalogRules::CombatResolvable(*Spell)) { return nullptr; }
    // SRD: each spell is cast through a class whose list has it and whose level grants that spell level.
    const int32 PrimaryLevels = Unit.Level - Unit.SecondaryLevel;
    if (FSpellCatalogRules::CanLearn(Unit.AdventurerRole, PrimaryLevels, *Spell)) { OutClass = Unit.AdventurerRole; }
    else if (Unit.SecondaryLevel > 0 && FSpellCatalogRules::CanLearn(Unit.SecondaryRole, Unit.SecondaryLevel, *Spell)) { OutClass = Unit.SecondaryRole; }
    else { return nullptr; }
    if (ArmorPenaltiesOf(*this, Unit).bCannotCastSpells) { return nullptr; }
    if (Spell->bVerbal && SpellZoneAt(Unit.Cell, 2)) { return nullptr; } // Silence: no verbal components
    if (Spell->CastTime == ESpellCastTime::Reaction) { return nullptr; } // reactions fire automatically (Shield, Hellish Rebuke, Counterspell)
    if (Spell->CastTime == ESpellCastTime::BonusAction ? !State.bBonusActionAvailable : !State.bActionAvailable) { return nullptr; }
    if (!Unit.PreparedSpells.IsEmpty() && !Unit.PreparedSpells.Contains(FString(Spell->Id))) { return nullptr; }
    if (Spell->Level == 0) { return Spell; }
    if (State.bSlotSpentThisTurn) { return nullptr; } // SRD 5.2.1: one spell slot per turn
    if (Requested > 0 && Requested < Spell->Level) { return nullptr; }
    for (int32 Level = FMath::Max(Spell->Level, Requested); Level <= FSpellRules::MaxSpellLevel; ++Level)
    {
        const int32 Left = Level == 1 ? Unit.SpellSlots : Unit.SlotsByLevel.IsValidIndex(Level - 1) ? Unit.SlotsByLevel[Level - 1] : 0;
        bool bOpen = Left > 0;
        if (bOpen && Level == 1 && FBuildCatalog::Find(Unit.BuildId))
        {
            FCombatantState After = Unit; SpendSpellSlot(After);
            bOpen = FBuildCatalog::ValidResources(After); // never spend a slot a kit has reserved
        }
        if (bOpen) { OutSlot = Level; return Spell; }
        if (Requested > 0) { return nullptr; }
    }
    return nullptr;
}

TArray<FString> FCombatBattle::CastableSpells(int32 Id) const
{
    TArray<FString> Out;
    const FCombatantState* Unit = Find(Id);
    if (!Unit) { return Out; }
    for (int32 Index = 0; Index < FSpellCatalogRules::Num(); ++Index)
    {
        int32 Slot = 0; EAdventurerRole Class = EAdventurerRole::None;
        const FSrdSpell& Spell = FSpellCatalogRules::At(Index);
        if (UsableSpell(*Unit, Spell.Id, 0, Slot, Class)) { Out.Add(Spell.Id); }
    }
    return Out;
}

TArray<int32> FCombatBattle::SpellTargetsRaw(int32 Id, const FString& SpellId, int32 SlotLevel) const
{
    TArray<int32> Out;
    const FCombatantState* Unit = Find(Id);
    int32 Slot = 0; EAdventurerRole Class = EAdventurerRole::None;
    const FSrdSpell* Spell = Unit ? UsableSpell(*Unit, SpellId, SlotLevel, Slot, Class) : nullptr;
    if (!Spell) { return Out; }
    const bool bHeal = Spell->Effect == ESpellEffect::Healing;
    // Range in cells: Touch is 5 ft (1 cell); Self is 0 and aims the area instead.
    // Self-range spells with no area (Vampiric Touch) reach like a touch.
    const int32 Range = Spell->RangeFeet == 0 ? (Spell->Area == ESpellArea::None && !bHeal ? 1 : 0) : FMath::Max(1, Spell->RangeFeet / FAreaRules::FeetPerCell);
    for (const FCombatantState& Target : State.Encounter.Combatants)
    {
        const bool bSide = bHeal ? Target.Side == Unit->Side && Target.Life != ECombatLife::Dead : Target.Side != Unit->Side && Target.Life == ECombatLife::Conscious;
        if (!bSide) { continue; }
        if (Range > 0)
        {
            if (GridDistance(Unit->Cell, Target.Cell) > Range || (Target.Id != Id && !HasLineOfSight(Unit->Cell, Target.Cell))) { continue; }
        }
        else if (bHeal) { if (Target.Id != Id) { continue; } }
        else
        {
            FAreaTemplate Area;
            if (!CombatSpellSlots::CatalogArea(*Spell, Unit->Cell, Target.Cell, Area) || !FAreaRules::Covers(Area, Target.Cell) || !HasLineOfSight(Unit->Cell, Target.Cell)) { continue; }
        }
        Out.Add(Target.Id);
    }
    return Out;
}

void FCombatBattle::EndConcentration(FCombatantState& Caster)
{
    if (!Caster.bConcentrating) { return; }
    Caster.bConcentrating = false;
    const int32 CasterId = Caster.Id;
    for (int32 Index = State.ActiveSpells.Num() - 1; Index >= 0; --Index)
    {
        if (State.ActiveSpells[Index].CasterId == CasterId && State.ActiveSpells[Index].bConcentration) { RemoveActiveSpell(Index); }
    }
    // Conditions a concentration spell imposed are tagged RoundsLeft -1 with the caster as source; they end with it. Grapples are not spells.
    const int32 Id = Caster.Id;
    for (FCombatantState& Unit : State.Encounter.Combatants)
    {
        // Conditions a non-concentration spell of the caster still holds on this creature (Charm Person) stay.
        const int32 UnitId = Unit.Id;
        if (State.ActiveSpells.ContainsByPredicate([&](const FActiveSpell& A) { return A.CasterId == Id && A.TargetId == UnitId; })) { continue; }
        Unit.Conditions.RemoveAll([Id](const FCombatCondition& C) { return C.SourceId == Id && C.RoundsLeft == -1 && C.Condition != ECombatCondition::Grappled; });
    }
}

bool FCombatBattle::ResolveCatalogSpell(const FCombatCommand& Command, FCombatResult& Result)
{
    FCombatantState* User = MutableFind(Command.ActorId);
    int32 Slot = 0; EAdventurerRole Class = EAdventurerRole::None;
    const FSrdSpell* Spell = User ? UsableSpell(*User, Command.SpellId, Command.SlotLevel, Slot, Class) : nullptr;
    if (!Spell) { Result.Feedback = TEXT("Spell unavailable: check class list and level, combat support, action budget, slot level and armor."); return false; }
    const TArray<int32> Legal = SpellTargets(Command.ActorId, Command.SpellId, Command.SlotLevel);
    if (!Legal.Contains(Command.TargetId)) { Result.Feedback = TEXT("Spell target is out of range, out of sight, outside the area, or on the wrong side."); return false; }
    const FSrdSpell& S = *Spell;
    if (FString Why; UtilityCastBlocked(Command, S, Why)) { Result.Feedback = Why; return false; }
    const int32 Prof = FProficiencyRules::ProficiencyBonus(User->Level);
    const int32 Mod = AbilityModifier(User->Id, FSpellcastingRules::SpellcastingAbility(Class));
    const int32 Dc = FSpellcastingRules::SaveDc(Prof, Mod);
    const int32 AttackBonus = FSpellcastingRules::AttackBonus(Prof, Mod);

    // Pay: one slot of the cast level (cantrips are free) and the action or bonus action.
    if (Slot == 1) { SpendSpellSlot(*User); } else if (Slot > 1) { --User->SlotsByLevel[Slot - 1]; }
    if (Slot > 0) { State.bSlotSpentThisTurn = true; } // SRD 5.2.1: one spell slot per turn
    if (S.CastTime == ESpellCastTime::BonusAction) { State.bBonusActionAvailable = false; } else { State.bActionAvailable = false; }
    User->bHidden = false;
    if (S.bConcentration) { EndConcentration(*User); User->bConcentrating = true; }
    EndOnHostileAct(User->Id); // casting a spell ends Invisibility and Sanctuary on the caster
    if (ResolveUtilitySpell(Command, S, Slot, Dc, AttackBonus, Mod, Result)) { CheckOutcome(); return true; }
    const FSpellEffectSpec* Spec = FSpellEffectRules::Find(S.Id);
    if (Spec && FSpellEffectRules::IsBuff(*Spec)) { return ResolveEffectSpell(Command, S, *Spec, Slot, Dc, AttackBonus, Mod, Legal, Result); }

    const int32 Count = FSpellCatalogRules::DiceCount(S, User->Level, Slot);
    const bool bMissile = FCString::Stricmp(S.Id, TEXT("magic-missile")) == 0;
    // Agonizing Blast (invocation): + spellcasting modifier to Eldritch Blast damage.
    const int32 Agonizing = FCString::Stricmp(S.Id, TEXT("eldritch-blast")) == 0 && FFeatureKit::HasInvocation(*User, EInvocation::AgonizingBlast) ? Mod : 0;
    const int32 Flat = (S.bAddModifier ? Mod : bMissile ? Count : S.FlatBonus) + Agonizing; // Magic Missile: +1 per dart
    const int32 Count2 = S.Level > 0 && Slot >= 1 && Slot <= 9 ? S.Dice2[Slot] : 0;
    Result.EffectModifier = Flat;
    Result.DamageType = S.DamageType;
    Result.bSaveSpell = S.Resolution == ESpellResolution::Save;
    const FString SlotText = Slot == 0 ? FString(TEXT("cantrip")) : FString::Printf(TEXT("level %d slot"), Slot);

    if (S.Effect == ESpellEffect::Healing)
    {
        FCombatantState* Target = MutableFind(Command.TargetId);
        DiceLog = &Result.EffectDice;
        const bool bMax = SpellExtraOn(Target->Id, SpellEffectExtra::MaxHealing) != nullptr; // Beacon of Hope: maximum healing
        const int32 Amount = FMath::Max(1, (S.DieSides == 0 ? Count : bMax ? Count * S.DieSides : RollDice(Count, S.DieSides)) + Flat);
        DiceLog = nullptr;
        Result.Healing = FMath::Min(Amount, Target->MaxHP - Target->HP);
        Target->HP += Result.Healing;
        if (Target->HP > 0) { Target->Life = ECombatLife::Conscious; Target->RescueTurnsLeft = 0; Target->DeathSaveSuccesses = 0; Target->DeathSaveFailures = 0; }
        Result.Effects.Add({Target->Id, EAbilityEffect::Healing, Result.Healing, Target->Cell});
        Result.bHit = true;
        Result.Feedback = FString::Printf(TEXT("%s: %s (%s) > %s | heal %d | HP %d/%d"), *User->Name, S.Name, *SlotText, *Target->Name, Result.Healing, Target->HP, Target->MaxHP);
        CheckOutcome();
        return true;
    }

    // Victims: every creature in the area but the caster (allies too, per SRD), or the one target.
    // Lasting zones (Spirit Guardians, Moonbeam, Spike Growth...) reach every creature in their radius, but only hurt on casting when the SRD says so.
    const FSpellZone* Zone = FSpellTacticsRules::Zone(S);
    FAreaTemplate Area;
    const bool bArea = Zone || CombatSpellSlots::CatalogArea(S, User->Cell, Find(Command.TargetId)->Cell, Area);
    TArray<FIntPoint> CastCells;
    TArray<int32> Victims = SpellVictims(*User, S, Command.TargetId, &CastCells);
    if (Spec)
    {
        // Spells phase 3: several chosen targets (Hold Person upcast), creature types, "creatures of your choice".
        if (Spec->Targets > 0 && !bArea) { Victims = SpellTargetList(Command, S, *Spec, Slot, Legal); }
        Victims.RemoveAll([&](int32 V) { const FCombatantState* U = Find(V); return !U || !FSpellEffectRules::CanAffect(*Spec, U->Role) || ((Spec->Flags & SpellEffectFlag::SparesAllies) && U->Side == User->Side); });
    }
    const bool bCastDamage = !(Spec && (Spec->Flags & SpellEffectFlag::NoCastDamage));
    const bool bTracked = S.bConcentration || FSpellTacticsRules::RepeatsSaveAtEndOfTurn(S) || (Spec && Spec->ConditionRounds == 0);
    const int32 UntrackedRounds = Spec && Spec->ConditionRounds > 0 ? Spec->ConditionRounds : 1;
    const TArray<int32> Reached = Victims;
    if (Zone && !(Zone->Triggers & SpellZoneTrigger::OnCast)) { Victims.Reset(); }
    int32 ZoneSerial = INDEX_NONE;
    if (Zone)
    {
        FActiveSpell& Entry = AddActiveSpell(S, User->Id, INDEX_NONE, Slot, Dc, AttackBonus);
        Entry.Origin = Zone->bFollowsCaster || S.RangeFeet == 0 ? User->Cell : Find(Command.TargetId)->Cell;
        Entry.RadiusCells = FMath::Max(0, Zone->RadiusFeet / FAreaRules::FeetPerCell);
        Entry.TriggeredThisTurn = Victims;
        ZoneSerial = Entry.Serial;
        ZoneCastExtras(ZoneSerial, Command);
    }
    const int32 CasterId = User->Id;
    TArray<int32> Held;

    // Attack-roll spells: one d20 against the target's AC; a natural 20 doubles the dice.
    bool bAttackHit = true, bCritical = false;
    if (S.Resolution == ESpellResolution::MeleeAttack || S.Resolution == ESpellResolution::RangedAttack)
    {
        const FAttackRollMode Mode = AttackRollMode(User->Id, Command.TargetId);
        bool bCrowded = false;
        if (S.Resolution == ESpellResolution::RangedAttack && !RangedAttacker(*User))
        {
            for (const FCombatantState& Other : State.Encounter.Combatants) { bCrowded |= Other.Side != User->Side && Other.Life == ECombatLife::Conscious && !IsIncapacitated(Other) && GridDistance(Other.Cell, User->Cell) <= 1; }
        }
        Result.RollMode = FCheckRules::CombineMode(Mode.Advantage.Num(), Mode.Disadvantage.Num() + (bCrowded ? 1 : 0));
        Result.AttackRolls.Add(Roll(20));
        if (Result.RollMode != ERollMode::Normal) { Result.AttackRolls.Add(Roll(20)); }
        Result.NaturalRoll = Result.AttackRolls.Num() == 1 ? Result.AttackRolls[0] :
            Result.RollMode == ERollMode::Advantage ? FMath::Max(Result.AttackRolls[0], Result.AttackRolls[1]) : FMath::Min(Result.AttackRolls[0], Result.AttackRolls[1]);
        Result.AttackBonus = AttackBonus + SpellAttackBonus(User->Id, false, Result);
        Result.AttackTotal = Result.NaturalRoll + Result.AttackBonus;
        Result.TargetArmorClass = EffectiveArmorClass(Command.TargetId);
        bCritical = Result.NaturalRoll == 20;
        bAttackHit = bCritical || (Result.NaturalRoll != 1 && Result.AttackTotal >= Result.TargetArmorClass);
        Result.bCritical = bCritical;
    }
    else if (Result.bSaveSpell) { Result.TargetArmorClass = Dc; }

    // One damage roll for the whole spell (both types when it deals two), then each victim's save.
    int32 Rolled = 0, Rolled2 = 0;
    if (S.Effect == ESpellEffect::Damage && bAttackHit && bCastDamage)
    {
        DiceLog = &Result.EffectDice;
        const int32 Dice = Count * (bCritical ? 2 : 1);
        Rolled = (S.DieSides == 0 ? Count : RollDice(Dice, S.DieSides)) + Flat;
        if (Count2 > 0 && S.Dice2Sides > 0) { Rolled2 = RollDice(Count2 * (bCritical ? 2 : 1), S.Dice2Sides); }
        DiceLog = nullptr;
    }
    FString Lines;
    bool bAnyFailed = false;
    for (const int32 VictimId : Victims)
    {
        FCombatantState* Victim = MutableFind(VictimId);
        bool bSaved = false;
        if (Result.bSaveSpell)
        {
            const bool bRolled = RollSpellSave(VictimId, S.SaveAbility, Dc, Spec, Spec && (Spec->Flags & SpellEffectFlag::AdvantageInCombat));
            bSaved = bRolled && !(IsHelpless(*Victim) && (S.SaveAbility == EAbility::Strength || S.SaveAbility == EAbility::Dexterity));
        }
        const bool bAffected = Result.bSaveSpell ? !bSaved : bAttackHit;
        bAnyFailed |= bAffected;
        int32 Dealt = 0;
        if (S.Effect == ESpellEffect::Damage && bCastDamage && (bAffected || (bSaved && S.bHalfOnSave)))
        {
            const FDamageDefenses Defenses = DamageDefenses(VictimId);
            const int32 First = FDamageRules::Adjust(FSpellcastingRules::DamageAfterSave(Rolled, bSaved, S.bHalfOnSave), S.DamageType, Defenses);
            const int32 Second = Rolled2 > 0 ? FDamageRules::Adjust(FSpellcastingRules::DamageAfterSave(Rolled2, bSaved, S.bHalfOnSave), S.Damage2Type, Defenses) : 0;
            const int32 Total = First + Second;
            const int32 Prevented = FMath::Min(Total, ProtectionFor(*Victim));
            ApplyDamage(*Victim, Total);
            Dealt = Total - Prevented;
            Result.Damage += Dealt; Result.DamagePrevented += Prevented;
        }
        // Failed save (or hit): the spell's conditions. Concentration spells hold them until concentration ends (RoundsLeft -1); others last one round.
        FString CondText;
        const uint32 Conds = Spec && Spec->Conditions ? Spec->Conditions : S.Conditions;
        if (bAffected && Conds != 0 && Victim->Life == ECombatLife::Conscious)
        {
            for (int32 Bit = 0; Bit < static_cast<int32>(ESpellCondition::Count); ++Bit)
            {
                ECombatCondition Kind;
                if (!(Conds & (1u << Bit)) || !CombatSpellSlots::ToCombatCondition(static_cast<ESpellCondition>(Bit), Kind)) { continue; }
                if (Victim->Conditions.ContainsByPredicate([&](const FCombatCondition& C) { return C.Condition == Kind && C.SourceId == CasterId; })) { continue; }
                if (!AddSpellCondition(*Victim, Kind, CasterId, bTracked ? -1 : UntrackedRounds)) { continue; }
                CondText += FString::Printf(TEXT(" %s"), ConditionName(Kind));
            }
            if (!CondText.IsEmpty()) { Held.Add(VictimId); }
        }
        Result.Effects.Add({VictimId, bArea ? EAbilityEffect::Area : EAbilityEffect::Strike, Dealt, Victim->Cell});
        Lines += FString::Printf(TEXT(" | %s %s %d%s"), *Victim->Name, Result.bSaveSpell ? (bSaved ? TEXT("saves,") : TEXT("fails,")) : (bCritical ? TEXT("CRITICAL") : bAttackHit ? TEXT("hit,") : TEXT("miss,")), Dealt, *CondText);
    }
    // A concentration spell that took hold of nobody ends at once.
    User = MutableFind(CasterId);
    // Track what lasts: held conditions per victim (repeat saves, duration), or one entry so the duration ends concentration.
    if (!Zone && bTracked)
    {
        if (!Held.IsEmpty()) { for (const int32 Id : Held) { AddActiveSpell(S, CasterId, Id, Slot, Dc, AttackBonus); } }
        else if (S.bConcentration && S.Effect != ESpellEffect::Condition && User->bConcentrating) { AddActiveSpell(S, CasterId, Command.TargetId, Slot, Dc, AttackBonus); }
    }
    if (S.bConcentration && S.Effect == ESpellEffect::Condition && !bAnyFailed) { User->bConcentrating = false; }
    if (Zone && S.bConcentration && !User->bConcentrating) { if (const int32 I = IndexOfSpell(ZoneSerial); I != INDEX_NONE) { State.ActiveSpells.RemoveAt(I); } }
    {
        FActiveSpell Probe; Probe.SpellId = S.Id; Probe.CasterId = CasterId; Probe.SlotLevel = Slot; Probe.Serial = ZoneSerial;
        Probe.RoundsLeft = FSpellTacticsRules::DurationRounds(S);
        if (const int32 I = IndexOfSpell(ZoneSerial); I != INDEX_NONE) { Probe = State.ActiveSpells[I]; }
        EmitSpellEvent(ESpellEventKind::Cast, Probe, Zone ? Reached : Victims.IsEmpty() ? TArray<int32>{Command.TargetId} : Victims);
        FSpellCastEvent& Cast = PendingSpellEvents.Last();
        Cast.Tier = User->Level >= 17 ? 3 : User->Level >= 11 ? 2 : User->Level >= 5 ? 1 : 0;
        if (!Zone) { Cast.Cells = CastCells; }
    }
    Result.bHit = bAnyFailed;
    const FString Check = Result.bSaveSpell ? FString::Printf(TEXT("%s save DC %d"), *FString(FAbilityRules::AbilityName(S.SaveAbility)).Left(3).ToUpper(), Dc) :
        Result.AttackRolls.IsEmpty() ? FString(TEXT("auto-hit")) : FString::Printf(TEXT("d20 %d + %d = %d vs AC %d"), Result.NaturalRoll, AttackBonus, Result.AttackTotal, Result.TargetArmorClass);
    Result.Feedback = FString::Printf(TEXT("%s: %s (%s, %s)%s"), *User->Name, S.Name, *SlotText, *Check, *Lines);
    CheckOutcome();
    return true;
}

// ---- Lasting spells: SRD 5.2.1 durations, zones, repeat saves, forecasts (spells phase 2) ----

int32 FCombatBattle::IndexOfSpell(int32 Serial) const
{
    return State.ActiveSpells.IndexOfByPredicate([Serial](const FActiveSpell& A) { return A.Serial == Serial; });
}

FActiveSpell& FCombatBattle::AddActiveSpell(const FSrdSpell& Spell, int32 CasterId, int32 TargetId, int32 Slot, int32 Dc, int32 AttackBonus)
{
    FActiveSpell& A = State.ActiveSpells.AddDefaulted_GetRef();
    A.SpellId = Spell.Id; A.CasterId = CasterId; A.TargetId = TargetId; A.SlotLevel = Slot;
    const int32 Rounds = FSpellTacticsRules::DurationRounds(Spell);
    A.RoundsLeft = Rounds == 0 ? 1 : Rounds;
    A.bConcentration = Spell.bConcentration; A.Dc = Dc; A.AttackBonus = AttackBonus;
    A.Serial = State.NextSpellSerial++;
    return A;
}

bool FCombatBattle::ZoneCovers(const FActiveSpell& Spell, FIntPoint Cell) const
{
    const FSpellZone* Zone = FSpellTacticsRules::Zone(Spell.SpellId);
    if (!Zone) { return false; }
    FIntPoint Origin = Spell.Origin;
    if (Zone->bFollowsCaster) { if (const FCombatantState* Caster = Find(Spell.CasterId)) { Origin = Caster->Cell; } }
    return IsInside(Cell) && GridDistance(Origin, Cell) <= Spell.RadiusCells;
}

TArray<FIntPoint> FCombatBattle::ZoneCells(const FActiveSpell& Spell) const
{
    TArray<FIntPoint> Out;
    for (int32 Y = 0; Y < State.Encounter.Height; ++Y)
    {
        for (int32 X = 0; X < State.Encounter.Width; ++X) { if (ZoneCovers(Spell, FIntPoint(X, Y))) { Out.Add(FIntPoint(X, Y)); } }
    }
    return Out;
}

FSpellCastEvent FCombatBattle::MakeSpellEvent(ESpellEventKind Kind, const FActiveSpell& A, const TArray<int32>& Targets) const
{
    FSpellCastEvent E;
    E.Kind = Kind; E.SpellId = A.SpellId; E.SlotLevel = A.SlotLevel; E.CasterId = A.CasterId; E.Serial = A.Serial; E.DurationRounds = A.RoundsLeft;
    if (const FSrdSpell* S = FSpellCatalogRules::Find(A.SpellId))
    {
        E.School = static_cast<uint8>(S->School); E.SpellLevel = S->Level; E.AreaShape = static_cast<uint8>(S->Area); E.AreaFeet = S->AreaFeet;
    }
    E.SizeClass = FSpellUtilityRules::SizeClass(E.SpellLevel);
    if (const FSpellZone* Zone = FSpellTacticsRules::Zone(A.SpellId)) { E.AreaFeet = Zone->RadiusFeet; E.Cells = ZoneCells(A); }
    if (const FCombatantState* Caster = Find(A.CasterId)) { E.CasterCell = Caster->Cell; }
    for (const int32 Id : Targets) { if (const FCombatantState* T = Find(Id)) { E.TargetIds.Add(Id); E.TargetCells.Add(T->Cell); } }
    E.bPersist = Kind != ESpellEventKind::End && State.ActiveSpells.ContainsByPredicate([&](const FActiveSpell& X) { return X.Serial == A.Serial; });
    return E;
}

void FCombatBattle::EmitSpellEvent(ESpellEventKind Kind, const FActiveSpell& A, const TArray<int32>& Targets)
{
    PendingSpellEvents.Add(MakeSpellEvent(Kind, A, Targets));
}

TArray<FSpellCastEvent> FCombatBattle::SpellVisuals() const
{
    TArray<FSpellCastEvent> Out;
    for (const FActiveSpell& A : State.ActiveSpells)
    {
        TArray<int32> Targets;
        if (A.TargetId != INDEX_NONE) { Targets.Add(A.TargetId); }
        FSpellCastEvent E = MakeSpellEvent(ESpellEventKind::Persist, A, Targets);
        E.bPersist = true;
        Out.Add(MoveTemp(E));
    }
    return Out;
}

void FCombatBattle::RemoveActiveSpell(int32 Index, bool bEvent)
{
    if (!State.ActiveSpells.IsValidIndex(Index)) { return; }
    const FActiveSpell Ended = State.ActiveSpells[Index];
    State.ActiveSpells.RemoveAt(Index);
    if (bEvent) { EmitSpellEvent(ESpellEventKind::End, Ended, {}); }
    EndSpellEffects(Ended);
    // A concentration spell with nothing left in effect ends the concentration too.
    if (Ended.bConcentration && !State.ActiveSpells.ContainsByPredicate([&](const FActiveSpell& A) { return A.CasterId == Ended.CasterId && A.bConcentration; }))
    {
        if (FCombatantState* Caster = MutableFind(Ended.CasterId)) { EndConcentration(*Caster); }
    }
}

TArray<int32> FCombatBattle::SpellVictims(const FCombatantState& User, const FSrdSpell& S, int32 TargetId, TArray<FIntPoint>* OutCells) const
{
    TArray<int32> Victims;
    const FCombatantState* Aim = Find(TargetId);
    if (!Aim) { return Victims; }
    if (const FSpellZone* Zone = FSpellTacticsRules::Zone(S))
    {
        FActiveSpell Probe; Probe.SpellId = S.Id; Probe.CasterId = User.Id;
        Probe.Origin = Zone->bFollowsCaster || S.RangeFeet == 0 ? User.Cell : Aim->Cell;
        Probe.RadiusCells = FMath::Max(0, Zone->RadiusFeet / FAreaRules::FeetPerCell);
        if (OutCells) { *OutCells = ZoneCells(Probe); }
        for (const FCombatantState& Unit : State.Encounter.Combatants)
        {
            if (Unit.Life == ECombatLife::Dead || Unit.Id == User.Id || !ZoneCovers(Probe, Unit.Cell) || (Zone->bSparesAllies && Unit.Side == User.Side)) { continue; }
            Victims.Add(Unit.Id);
        }
        return Victims;
    }
    FAreaTemplate Area;
    if (CombatSpellSlots::CatalogArea(S, User.Cell, Aim->Cell, Area))
    {
        for (const FCombatantState& Unit : State.Encounter.Combatants)
        {
            if (Unit.Id != User.Id && Unit.Life != ECombatLife::Dead && FAreaRules::Covers(Area, Unit.Cell) && HasLineOfSight(Area.Origin, Unit.Cell)) { Victims.Add(Unit.Id); }
        }
        Victims.AddUnique(TargetId);
        if (OutCells) { for (const FIntPoint& Cell : FAreaRules::Cells(Area)) { if (IsInside(Cell)) { OutCells->Add(Cell); } } }
    }
    else { Victims.Add(TargetId); }
    return Victims;
}

void FCombatBattle::TriggerZone(int32 Index, int32 VictimId)
{
    if (!State.ActiveSpells.IsValidIndex(Index)) { return; }
    FActiveSpell& A = State.ActiveSpells[Index];
    const FSpellZone* Zone = FSpellTacticsRules::Zone(A.SpellId);
    const FSrdSpell* S = FSpellCatalogRules::Find(A.SpellId);
    FCombatantState* Victim = MutableFind(VictimId);
    const FCombatantState* Caster = Find(A.CasterId);
    if (!Zone || !S || !Victim || !Caster || Victim->Life == ECombatLife::Dead || (Victim->Id == A.CasterId && (Zone->bFollowsCaster || Zone->bSparesAllies)) ||
        (Zone->bSparesAllies && Victim->Side == Caster->Side)) { return; }
    if (!(Zone->Triggers & SpellZoneTrigger::PerStep))
    {
        if (A.TriggeredThisTurn.Contains(VictimId)) { return; }
        A.TriggeredThisTurn.Add(VictimId);
    }
    if (A.SpellId == TEXT("guardian-of-faith"))
    {
        // SRD 5.2.1 Guardian of Faith: DEX save or 20 radiant (half on success); it vanishes after dealing 60 total.
        const bool bSaved = FCheckRules::Resolve(Character(VictimId).SaveBonus(EAbility::Dexterity), A.Dc, SaveMode(*Victim, EAbility::Dexterity, ERollMode::Normal), State.RandomState).bSuccess;
        const int32 Dealt = FDamageRules::Adjust(bSaved ? 10 : 20, EDamageType::Radiant, DamageDefenses(VictimId));
        A.Value += Dealt;
        const FActiveSpell Guardian = A;
        ApplyDamage(*Victim, Dealt);
        EmitSpellEvent(ESpellEventKind::Trigger, Guardian, {VictimId});
        if (Guardian.Value >= 60) { if (const int32 I = IndexOfSpell(Guardian.Serial); I != INDEX_NONE) { RemoveActiveSpell(I); } }
        return;
    }
    // Copy what the trigger needs: damage can end concentration and remove this entry.
    const FActiveSpell Copy = A;
    const int32 Count = FSpellCatalogRules::DiceCount(*S, Caster->Level, Copy.SlotLevel);
    const int32 Rolled = S->DieSides > 0 ? RollDice(Count, S->DieSides) : 0;
    bool bSaved = false;
    if (S->Resolution == ESpellResolution::Save)
    {
        const bool bRolled = FCheckRules::Resolve(Character(VictimId).SaveBonus(S->SaveAbility), Copy.Dc, SaveMode(*Victim, S->SaveAbility, ERollMode::Normal), State.RandomState).bSuccess;
        bSaved = bRolled && !(IsHelpless(*Victim) && (S->SaveAbility == EAbility::Strength || S->SaveAbility == EAbility::Dexterity));
    }
    if (!bSaved && S->Conditions != 0 && Victim->Life == ECombatLife::Conscious)
    {
        for (int32 Bit = 0; Bit < static_cast<int32>(ESpellCondition::Count); ++Bit)
        {
            ECombatCondition Kind;
            if (!(S->Conditions & (1u << Bit)) || !CombatSpellSlots::ToCombatCondition(static_cast<ESpellCondition>(Bit), Kind)) { continue; }
            if (Victim->Conditions.ContainsByPredicate([&](const FCombatCondition& C) { return C.Condition == Kind && C.SourceId == Copy.CasterId; })) { continue; }
            FCombatCondition Applied; Applied.Condition = Kind; Applied.SourceId = Copy.CasterId; Applied.RoundsLeft = Copy.bConcentration ? -1 : 1;
            Victim->Conditions.Add(Applied);
        }
    }
    if (S->Effect == ESpellEffect::Damage && Rolled > 0 && (!bSaved || S->bHalfOnSave))
    {
        const int32 Dealt = FDamageRules::Adjust(FSpellcastingRules::DamageAfterSave(Rolled, bSaved, S->bHalfOnSave), S->DamageType, DamageDefenses(VictimId));
        ApplyDamage(*Victim, Dealt);
    }
    EmitSpellEvent(ESpellEventKind::Trigger, Copy, {VictimId});
}

void FCombatBattle::SpellsOnStep(int32 MoverId, FIntPoint From, FIntPoint To)
{
    TArray<int32> Serials;
    for (const FActiveSpell& A : State.ActiveSpells) { if (FSpellTacticsRules::Zone(A.SpellId)) { Serials.Add(A.Serial); } }
    for (const int32 Serial : Serials)
    {
        const FCombatantState* Mover = Find(MoverId);
        if (!Mover || Mover->Life != ECombatLife::Conscious) { return; }
        int32 Index = IndexOfSpell(Serial);
        if (Index == INDEX_NONE) { continue; }
        const FSpellZone* Zone = FSpellTacticsRules::Zone(State.ActiveSpells[Index].SpellId);
        const bool bNow = ZoneCovers(State.ActiveSpells[Index], To);
        if ((Zone->Triggers & SpellZoneTrigger::PerStep) && bNow) { TriggerZone(Index, MoverId); continue; }
        if (!(Zone->Triggers & SpellZoneTrigger::Enter)) { continue; }
        if (Zone->bFollowsCaster && State.ActiveSpells[Index].CasterId == MoverId)
        {
            // The emanation moved with its caster: creatures it newly reaches are entered.
            FActiveSpell Before = State.ActiveSpells[Index];
            Before.Origin = From;
            const FSpellZone Fixed = *Zone;
            TArray<int32> Reached;
            for (const FCombatantState& Unit : State.Encounter.Combatants)
            {
                if (Unit.Id != MoverId && Unit.Life != ECombatLife::Dead && ZoneCovers(State.ActiveSpells[Index], Unit.Cell) && GridDistance(From, Unit.Cell) > Before.RadiusCells) { Reached.Add(Unit.Id); }
            }
            for (const int32 Id : Reached) { Index = IndexOfSpell(Serial); if (Index == INDEX_NONE) { break; } TriggerZone(Index, Id); }
            continue;
        }
        FActiveSpell Before = State.ActiveSpells[Index];
        const bool bWas = Zone->bFollowsCaster ? GridDistance(Find(Before.CasterId) ? Find(Before.CasterId)->Cell : Before.Origin, From) <= Before.RadiusCells : ZoneCovers(Before, From);
        if (bNow && !bWas) { TriggerZone(Index, MoverId); }
    }
}

void FCombatBattle::SpellsOnTurnStart(int32 UnitId)
{
    for (FActiveSpell& A : State.ActiveSpells) { A.TriggeredThisTurn.Reset(); }
    // Blink: the caster returns from the Ethereal Plane at the start of its turn.
    for (FActiveSpell& A : State.ActiveSpells) { if (A.TargetId == UnitId && A.SpellId.Equals(TEXT("blink"), ESearchCase::IgnoreCase)) { A.Value = 0; } }
    // The caster's spells count down at the start of each of its turns (1 minute = 10 rounds).
    TArray<int32> Serials;
    for (const FActiveSpell& A : State.ActiveSpells) { if (A.CasterId == UnitId && A.RoundsLeft > 0) { Serials.Add(A.Serial); } }
    for (const int32 Serial : Serials)
    {
        const int32 Index = IndexOfSpell(Serial);
        if (Index == INDEX_NONE) { continue; }
        if (--State.ActiveSpells[Index].RoundsLeft > 0) { continue; }
        if (State.ActiveSpells[Index].bConcentration)
        {
            if (FCombatantState* Caster = MutableFind(UnitId)) { EndConcentration(*Caster); }
        }
        else { RemoveActiveSpell(Index); }
    }
    Serials.Reset();
    for (const FActiveSpell& A : State.ActiveSpells)
    {
        const FSpellZone* Zone = FSpellTacticsRules::Zone(A.SpellId);
        if (Zone && (Zone->Triggers & SpellZoneTrigger::StartTurn)) { Serials.Add(A.Serial); }
    }
    for (const int32 Serial : Serials)
    {
        const int32 Index = IndexOfSpell(Serial);
        const FCombatantState* Unit = Find(UnitId);
        if (Index != INDEX_NONE && Unit && ZoneCovers(State.ActiveSpells[Index], Unit->Cell)) { TriggerZone(Index, UnitId); }
    }
    SummonsOnTurnStart(UnitId);
}

void FCombatBattle::SpellsOnTurnEnd(int32 UnitId)
{
    // Blink: at the end of each of the caster's turns, 1d6 of 4-6 vanishes it to the Ethereal Plane until its next turn.
    for (int32 I = 0; I < State.ActiveSpells.Num(); ++I)
    {
        if (State.ActiveSpells[I].TargetId == UnitId && State.ActiveSpells[I].SpellId.Equals(TEXT("blink"), ESearchCase::IgnoreCase))
        {
            State.ActiveSpells[I].Value = RollDice(1, 6) >= 4 ? 1 : 0;
            if (State.ActiveSpells[I].Value == 1) { EmitSpellEvent(ESpellEventKind::Trigger, State.ActiveSpells[I], {UnitId}); }
        }
    }
    TArray<int32> Serials;
    for (const FActiveSpell& A : State.ActiveSpells) { Serials.Add(A.Serial); }
    for (const int32 Serial : Serials)
    {
        const int32 Index = IndexOfSpell(Serial);
        FCombatantState* Unit = MutableFind(UnitId);
        if (Index == INDEX_NONE || !Unit || Unit->Life != ECombatLife::Conscious) { continue; }
        const FActiveSpell A = State.ActiveSpells[Index];
        const FSpellZone* Zone = FSpellTacticsRules::Zone(A.SpellId);
        if (Zone)
        {
            if ((Zone->Triggers & SpellZoneTrigger::EndTurn) && ZoneCovers(A, Unit->Cell)) { TriggerZone(Index, UnitId); }
            continue;
        }
        const FSrdSpell* S = FSpellCatalogRules::Find(A.SpellId);
        if (A.TargetId != UnitId || !S || !FSpellTacticsRules::RepeatsSaveAtEndOfTurn(*S) || S->Resolution != ESpellResolution::Save) { continue; }
        // Fear: only a target that cannot see the caster repeats the save.
        const FCombatantState* Caster = Find(A.CasterId);
        if (FCString::Stricmp(S->Id, TEXT("fear")) == 0 && Caster && HasLineOfSight(Caster->Cell, Unit->Cell)) { continue; }
        // Sleep: a creature that failed its repeat save is Unconscious for the duration, no further saves.
        const FSpellEffectSpec* RSpec = FSpellEffectRules::Find(A.SpellId);
        const bool bSleep = RSpec && (RSpec->Flags & SpellEffectFlag::SleepEscalates);
        if (bSleep && A.Value > 0) { continue; }
        const bool bRolled = RollSpellSave(UnitId, S->SaveAbility, A.Dc, RSpec);
        const bool bSaved = bRolled && !(IsHelpless(*Unit) && (S->SaveAbility == EAbility::Strength || S->SaveAbility == EAbility::Dexterity));
        if (!bSaved)
        {
            if (bSleep)
            {
                State.ActiveSpells[Index].Value = 1;
                AddSpellCondition(*Unit, ECombatCondition::Unconscious, A.CasterId, -1);
                AddSpellCondition(*Unit, ECombatCondition::Prone, INDEX_NONE, -1);
            }
            continue;
        }
        const int32 CasterId = A.CasterId;
        Unit->Conditions.RemoveAll([CasterId](const FCombatCondition& C) { return C.SourceId == CasterId && C.RoundsLeft == -1 && C.Condition != ECombatCondition::Grappled; });
        RemoveActiveSpell(Index);
    }
}

bool FCombatBattle::ValidPreparedSpells(const FCombatantState& Unit, FString& Error) const
{
    if (Unit.PreparedSpells.IsEmpty()) { return true; }
    const int32 PrimaryLevels = Unit.Level - Unit.SecondaryLevel;
    int32 Cantrips = 0, Leveled = 0;
    TSet<FString> Seen;
    for (const FString& Id : Unit.PreparedSpells)
    {
        const FSrdSpell* Spell = FSpellCatalogRules::Find(Id);
        if (!Spell || Id != Spell->Id || Seen.Contains(Id) ||
            !FSpellCatalogRules::HeroCanLearn(Unit.AdventurerRole, PrimaryLevels, Unit.SecondaryRole, Unit.SecondaryLevel, *Spell))
        { Error = FString::Printf(TEXT("%s cannot prepare '%s' (unknown, duplicate, or not on a class list at that level)."), *Unit.Name, *Id); return false; }
        Seen.Add(Id);
        ++(Spell->Level == 0 ? Cantrips : Leveled);
    }
    auto Mod = [&](EAdventurerRole Role) { return Role == EAdventurerRole::None ? 0 : AbilityModifier(Unit.Id, FSpellcastingRules::SpellcastingAbility(Role)); };
    const int32 MaxCantrips = FSpellCatalogRules::CantripsKnown(Unit.AdventurerRole, PrimaryLevels) +
        (Unit.SecondaryLevel > 0 ? FSpellCatalogRules::CantripsKnown(Unit.SecondaryRole, Unit.SecondaryLevel) : 0);
    const int32 MaxLeveled = FSpellCatalogRules::SpellsKnownOrPrepared(Unit.AdventurerRole, PrimaryLevels, Mod(Unit.AdventurerRole)) +
        (Unit.SecondaryLevel > 0 ? FSpellCatalogRules::SpellsKnownOrPrepared(Unit.SecondaryRole, Unit.SecondaryLevel, Mod(Unit.SecondaryRole)) : 0);
    if (Cantrips > MaxCantrips || Leveled > MaxLeveled)
    { Error = FString::Printf(TEXT("%s prepares %d cantrips and %d spells; the SRD allows %d and %d."), *Unit.Name, Cantrips, Leveled, MaxCantrips, MaxLeveled); return false; }
    return true;
}

FSpellForecast FCombatBattle::ForecastSpell(int32 ActorId, const FString& SpellId, int32 TargetId, int32 SlotLevel) const
{
    FSpellForecast F;
    const FCombatantState* User = Find(ActorId);
    int32 Slot = 0; EAdventurerRole Class = EAdventurerRole::None;
    const FSrdSpell* Spell = User ? UsableSpell(*User, SpellId, SlotLevel, Slot, Class) : nullptr;
    if (!Spell || !SpellTargets(ActorId, SpellId, SlotLevel).Contains(TargetId)) { return F; }
    const FSrdSpell& S = *Spell;
    const FCombatantState* Target = Find(TargetId);
    const int32 Prof = FProficiencyRules::ProficiencyBonus(User->Level);
    const int32 Mod = AbilityModifier(User->Id, FSpellcastingRules::SpellcastingAbility(Class));
    const int32 AttackBonus = FSpellcastingRules::AttackBonus(Prof, Mod);
    F.bValid = true; F.SpellId = S.Id; F.SlotLevel = Slot;
    F.SaveDc = FSpellcastingRules::SaveDc(Prof, Mod);
    F.DurationRounds = FSpellTacticsRules::DurationRounds(S);
    F.bConcentration = S.bConcentration;
    F.bEndsConcentration = S.bConcentration && User->bConcentrating;
    F.bHealing = S.Effect == ESpellEffect::Healing;
    const int32 Count = FSpellCatalogRules::DiceCount(S, User->Level, Slot);
    const bool bMissile = FCString::Stricmp(S.Id, TEXT("magic-missile")) == 0;
    const int32 Agonizing = FCString::Stricmp(S.Id, TEXT("eldritch-blast")) == 0 && FFeatureKit::HasInvocation(*User, EInvocation::AgonizingBlast) ? Mod : 0;
    const int32 Flat = (S.bAddModifier ? Mod : bMissile ? Count : S.FlatBonus) + Agonizing;
    const int32 Count2 = S.Level > 0 && Slot >= 1 && Slot <= 9 ? S.Dice2[Slot] : 0;
    if (S.Effect == ESpellEffect::Damage || F.bHealing)
    {
        F.MinAmount = (S.DieSides == 0 ? Count : Count) + Flat + (S.Dice2Sides > 0 ? Count2 : 0);
        F.MaxAmount = (S.DieSides == 0 ? Count : Count * S.DieSides) + Flat + Count2 * S.Dice2Sides;
        if (F.bHealing) { F.MinAmount = FMath::Max(1, F.MinAmount); F.MaxAmount = FMath::Max(1, F.MaxAmount); }
        if (FSpellTacticsRules::Zone(S) && !(FSpellTacticsRules::Zone(S)->Triggers & SpellZoneTrigger::OnCast)) { F.MinAmount = F.MaxAmount = 0; }
    }
    if (F.bHealing) { F.AffectedIds.Add(TargetId); F.ChanceOutOf400 = 400; return F; }
    F.AffectedIds = SpellVictims(*User, S, TargetId, &F.Cells);
    if (S.Resolution == ESpellResolution::MeleeAttack || S.Resolution == ESpellResolution::RangedAttack)
    {
        const FAttackRollMode Mode = AttackRollMode(User->Id, TargetId);
        bool bCrowded = false;
        if (S.Resolution == ESpellResolution::RangedAttack && !RangedAttacker(*User))
        {
            for (const FCombatantState& Other : State.Encounter.Combatants) { bCrowded |= Other.Side != User->Side && Other.Life == ECombatLife::Conscious && !IsIncapacitated(Other) && GridDistance(Other.Cell, User->Cell) <= 1; }
        }
        F.bAttackRoll = true;
        F.ChanceOutOf400 = FSpellcastingRules::AttackHitOutOf400(AttackBonus, EffectiveArmorClass(TargetId), FCheckRules::CombineMode(Mode.Advantage.Num(), Mode.Disadvantage.Num() + (bCrowded ? 1 : 0)));
    }
    else if (S.Resolution == ESpellResolution::Save)
    {
        F.bSave = true;
        const bool bAutoFail = IsHelpless(*Target) && (S.SaveAbility == EAbility::Strength || S.SaveAbility == EAbility::Dexterity);
        F.ChanceOutOf400 = bAutoFail ? 400 : FSpellcastingRules::SaveFailOutOf400(Character(TargetId).SaveBonus(S.SaveAbility), F.SaveDc, SaveMode(*Target, S.SaveAbility, ERollMode::Normal));
    }
    else { F.ChanceOutOf400 = 400; }
    return F;
}