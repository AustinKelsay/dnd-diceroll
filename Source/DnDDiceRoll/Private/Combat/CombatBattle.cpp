#include "Combat/CombatBattle.h"
#include "Party/BuildCatalog.h"
#include "Rules/SpellRules.h"
#include "Rules/MovementRules.h"
#include "Rules/OpportunityRules.h"
#include "Rules/ClassFeatureRules.h"
#include "Rules/WeaponRules.h"
#include "Rules/ArmorRules.h"
#include "Rules/ProficiencyRules.h"
#include "Rules/SpellcastingRules.h"
#include "Rules/AreaRules.h"
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
int32 WeaponAttackBonus(const FCombatBattle& Battle, const FCombatantState& Unit)
{
    const FWeaponProfile* Weapon = MainWeapon(Unit);
    if (!Weapon) { return Unit.WildShapeHP > 0 ? FFeatureKit::WolfAttack : Unit.AttackBonus; }
    const FAbilityScores Scores = Battle.AbilityScores(Unit.Id);
    const int32 Archery = Weapon->bRanged && FFeatureKit::Style(Unit) == EFightingStyle::Archery ? 2 : 0;
    return FProficiencyRules::ProficiencyBonus(Unit.Level) + FAbilityRules::Modifier(Scores.Get(FWeaponRules::AttackAbility(*Weapon, Scores))) + Unit.WeaponRank + Archery;
}
int32 WeaponDamageModifier(const FCombatBattle& Battle, const FCombatantState& Unit)
{
    const FWeaponProfile* Weapon = MainWeapon(Unit);
    if (!Weapon) { return Unit.WildShapeHP > 0 ? FFeatureKit::WolfDamageBonus : FBuildCatalog::AttackModifier(Unit); }
    const FAbilityScores Scores = Battle.AbilityScores(Unit.Id);
    const int32 Dueling = !Weapon->bRanged && !Weapon->Has(EWeaponProperty::TwoHanded) && Unit.OffHand < 0 && FFeatureKit::Style(Unit) == EFightingStyle::Dueling ? 2 : 0;
    return FAbilityRules::Modifier(Scores.Get(FWeaponRules::AttackAbility(*Weapon, Scores))) + Unit.WeaponRank + (Unit.RageTurns > 0 ? 2 : 0) + Dueling;
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
            bool bSlotsOk = Unit.SlotsByLevel.Num() == FSpellRules::MaxSpellLevel && Unit.SlotsByLevel[0] == Unit.SpellSlots;
            for (int32 SlotLevel = 1; bSlotsOk && SlotLevel <= Unit.SlotsByLevel.Num(); ++SlotLevel)
            { bSlotsOk = Unit.SlotsByLevel[SlotLevel - 1] >= 0 && Unit.SlotsByLevel[SlotLevel - 1] <= FSpellRules::SlotsFor(Unit.AdventurerRole, Unit.Level, SlotLevel); }
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
            if (static_cast<uint8>(Condition.Condition) > static_cast<uint8>(ECombatCondition::Unconscious) || Seen.Contains(Key) ||
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
    const bool bParty = Encounter.Combatants.ContainsByPredicate([](const auto& U) { return U.Side == ECombatSide::Party && U.Life == ECombatLife::Conscious; });
    const bool bEnemy = Encounter.Combatants.ContainsByPredicate([](const auto& U) { return U.Side == ECombatSide::Enemy && U.Life == ECombatLife::Conscious; });
    if (!bParty || !bEnemy) { Error = TEXT("Both sides need an active combatant."); return false; }
    State = FCombatSnapshot();
    State.Encounter = Encounter;
    State.RandomState = Seed;
    bStarted = true;
    for (FCombatantState& Unit : State.Encounter.Combatants)
    {
        Unit.Initiative = Roll(20) + Unit.InitiativeBonus;
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

int32 FCombatBattle::EffectiveArmorClass(int32 Id) const
{
    const FCombatantState* Unit = Find(Id);
    if (!Unit) { return 0; }
    if (Unit->WildShapeHP > 0) { return FFeatureKit::WolfAC; }
    const int32 Spell = (Unit->bShielded ? 5 : 0) + (Unit->ArmorWorn >= 0 && FFeatureKit::Style(*Unit) == EFightingStyle::Defense ? 1 : 0);
    if (Unit->ArmorWorn < 0) { return Spell + Unit->ArmorClass + (Unit->bShieldEquipped ? FArmorRules::Profile(EArmor::Shield).BaseAC : 0); }
    return Spell + FArmorRules::ArmorClass(static_cast<EArmor>(Unit->ArmorWorn), Unit->bShieldEquipped, AbilityScores(Id).Get(EAbility::Dexterity));
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
        return FString::Printf(TEXT("CURE WOUNDS | Action + 1 slot | ally/self, %d cell | %dd%d+%d healing"), TouchRange, CureDice, CureDie, HealingModifier);
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
    case ECombatCommandType::Dash: return FStandardActionRules::Cost(EStandardAction::Dash, Unit.AdventurerRole, Unit.Level, !bActionAvailable);
    case ECombatCommandType::Disengage: return FStandardActionRules::Cost(EStandardAction::Disengage, Unit.AdventurerRole, Unit.Level, !bActionAvailable);
    case ECombatCommandType::Hide: return FStandardActionRules::Cost(EStandardAction::Hide, Unit.AdventurerRole, Unit.Level, !bActionAvailable);
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
    return State.Encounter.DifficultCells.Contains(Cell) ? 2 : 1;
}

TArray<FIntPoint> FCombatBattle::LegalMoves(int32 Id) const
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
    if ((Action == ECombatCommandType::CureWounds || Action == ECombatCommandType::MagicMissile || CombatSpellSlots::IsAreaSpell(Action)) && ArmorPenaltiesOf(*this, *Unit).bCannotCastSpells) { return nullptr; }
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
            if (A.Command == Action && A.bResourceAvailable && (A.bBonus ? State.bBonusActionAvailable : State.bActionAvailable)) { Ability = &A; break; }
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

TArray<int32> FCombatBattle::LegalTargets(int32 Id, ECombatCommandType Action) const
{
    TArray<int32> Result;
    const FCombatantState* Unit = Find(Id);
    if (!bStarted || State.Phase != EBattlePhase::Fighting || ActiveId() != Id || !Unit || Unit->Life != ECombatLife::Conscious) { return Result; }
    if (FFeatureKit::IsFeature(Action)) { return ActionUser(Id, Action) ? FeatureTargets(*Unit, Action) : Result; }
    if (Action == ECombatCommandType::Special1 || Action == ECombatCommandType::Special2 || (Action == ECombatCommandType::CureWounds && !Unit->BuildId.IsEmpty()))
    {
        for (const auto& A : FBuildCatalog::Abilities(*Unit))
        {
            if (A.Command != Action || !A.bResourceAvailable || !(A.bBonus ? State.bBonusActionAvailable : State.bActionAvailable)) { continue; }
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

TArray<FIntPoint> FCombatBattle::PathTo(int32 Id, FIntPoint Destination) const
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
    return Encounter.bScaledSneakAttack ? FMath::Max(1, FClassFeatureRules::SneakAttackDice(Actor.Level)) : 1;
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
            { Forecast.bHealing = true; Forecast.MinAmount = Forecast.MaxAmount = FMath::Min(A.Amount, Target->MaxHP - Target->HP); if (A.bCureDice) { Forecast.MinAmount = FMath::Min(CureDice + HealingModifier,Target->MaxHP-Target->HP); Forecast.MaxAmount = FMath::Min(CureDice * CureDie + HealingModifier,Target->MaxHP-Target->HP); } }
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
    case ECombatCommandType::CureWounds: Effect(CureDice, CureDie, HealingModifier); break;
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
    // SRD: a ranged attack has disadvantage while a hostile, non-incapacitated creature is within five feet.
    if (RangedAttacker(*Actor))
    {
        for (const FCombatantState& Other : State.Encounter.Combatants)
        {
            if (Other.Side != Actor->Side && Other.Life == ECombatLife::Conscious && GridDistance(Other.Cell, Actor->Cell) <= 1)
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
        const bool bActorSees = FLightRules::CanSee(FLightRules::PerceivedLight(ELight::Darkness, FLightRules::DarkvisionFeet(Actor->Role), Feet));
        const bool bTargetSees = FLightRules::CanSee(FLightRules::PerceivedLight(ELight::Darkness, FLightRules::DarkvisionFeet(Target->Role), Feet));
        if (!bActorSees) { Mode.Disadvantage.Add(TEXT("can't see the target in darkness")); }
        if (!bTargetSees) { Mode.Advantage.Add(TEXT("unseen in darkness")); }
    }
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
    if (Target->bHidden) { Mode.Disadvantage.Add(TEXT("target hidden")); }
    if (const FCombatantState* Helper = Find(Target->HelpedById); Helper && Helper->Id != Actor->Id && Helper->Side == Actor->Side)
    { Mode.Advantage.Add(FString::Printf(TEXT("helped by %s"), *Helper->Name)); }
    Mode.Mode = FCheckRules::CombineMode(Mode.Advantage.Num(), Mode.Disadvantage.Num());
    return Mode;
}

bool FCombatBattle::HasCondition(const FCombatantState& Unit, ECombatCondition Condition)
{
    return Unit.Conditions.ContainsByPredicate([Condition](const FCombatCondition& C) { return C.Condition == Condition; });
}

bool FCombatBattle::IsHelpless(const FCombatantState& Unit)
{
    return HasCondition(Unit, ECombatCondition::Stunned) || HasCondition(Unit, ECombatCondition::Paralyzed) || HasCondition(Unit, ECombatCondition::Unconscious);
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
    return FCheckRules::CombineMode(Adv, Dis);
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
    FCombatantState* Unit = MutableFind(ActiveId());
    if (!Unit) { return; }
    Unit->bReactionAvailable = true;
    Unit->bDodging = false; Unit->bDisengaged = false; Unit->bReadied = false; Unit->bShielded = false;
    for (FCombatantState& Other : State.Encounter.Combatants) { if (Other.HelpedById == Unit->Id) { Other.HelpedById = INDEX_NONE; } }
    State.SneakAttackUsedBy.Reset();
    const bool bHeld = HasCondition(*Unit, ECombatCondition::Grappled) || HasCondition(*Unit, ECombatCondition::Restrained) || IsHelpless(*Unit);
    const int32 TurnSpeed = FMath::Max(0, BaseSpeed(*this, *Unit) - ArmorPenaltiesOf(*this, *Unit).SpeedPenaltyFeet / FWeaponRules::FeetPerCell);
    State.MovementLeft = bHeld ? 0 : TurnSpeed;
    if (!bHeld && HasCondition(*Unit, ECombatCondition::Prone))
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
}

FCombatResult FCombatBattle::Submit(const FCombatCommand& Command)
{
    TArray<ECombatLife> LifeBefore;
    for (const FCombatantState& Unit : State.Encounter.Combatants) { LifeBefore.Add(Unit.Life); }
    FCombatResult Result = Resolve(Command);
    DiceLog = nullptr;
    if (!Result.bAccepted) { return Result; }
    PruneConditions();
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
    Result.AttackBonus = WeaponAttackBonus(*this, *Actor) + ManeuverBonus;
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
    if (Result.bHit && ShieldWouldBlock(*Target, Result.AttackTotal, Result.TargetArmorClass, Result.bCritical))
    {
        SpendSpellSlot(*Target); Target->bReactionAvailable = false; Target->bShielded = true;
        Result.TargetArmorClass += 5; Result.bHit = false; Result.bShieldReaction = true;
    }
    if (State.Encounter.bInspiration && Result.NaturalRoll == 20) { FInspirationRules::Grant(MutableFind(ActorId)->bInspired); }
    // Attacking reveals a hidden attacker; Help is spent by the first ally attack it benefits.
    MutableFind(ActorId)->bHidden = false;
    if (const FCombatantState* Helper = Find(Target->HelpedById); Helper && Helper->Id != ActorId && Helper->Side == Actor->Side) { Target->HelpedById = INDEX_NONE; }
    // SRD: any hit on a paralyzed or unconscious creature from within 5 feet is a critical hit.
    if (Result.bHit && GridDistance(Actor->Cell, Target->Cell) <= 1 && (HasCondition(*Target, ECombatCondition::Paralyzed) || HasCondition(*Target, ECombatCondition::Unconscious))) { Result.bCritical = true; }
    if (Result.bHit)
    {
        DiceLog = &Result.EffectDice;
        // Weapon die, then the critical die; both have the same sides, so totals match earlier builds.
        int32 DiceCount = 1, DiceSides = 0;
        WeaponDice(*Actor, DiceCount, DiceSides);
        // SRD Great Weapon Fighting: reroll each 1 or 2 once on a two-handed (or two-handed versatile) melee weapon.
        const FWeaponProfile* Held = MainWeapon(*Actor);
        const bool bGreat = Held && !Held->bRanged && FFeatureKit::Style(*Actor) == EFightingStyle::GreatWeaponFighting &&
            (Held->Has(EWeaponProperty::TwoHanded) || (Held->Has(EWeaponProperty::Versatile) && !Actor->bShieldEquipped));
        auto WeaponRoll = [&]() { int32 Sum = 0; for (int32 I = 0; I < DiceCount; ++I) { int32 Die = Roll(DiceSides); if (bGreat && Die <= 2) { Die = Roll(DiceSides); } Sum += Die; } return Sum; };
        Result.Damage = WeaponRoll();
        if (Result.bCritical) { Result.Damage += WeaponRoll(); }
        Result.EffectModifier = (bOffHand ? FTwoWeaponRules::OffHandDamageModifier(WeaponDamageModifier(*this, *Actor), FFeatureKit::Style(*Actor) == EFightingStyle::TwoWeaponFighting) : WeaponDamageModifier(*this, *Actor)) + (State.Encounter.bSuperiorityDice ? 0 : ManeuverBonus);
        Result.Damage += Result.EffectModifier;
        Result.bSneakAttack = SneakAttackApplies(*Actor, *Target);
        if (Result.bSneakAttack) { Result.Damage += RollDice((Result.bCritical ? 2 : 1) * SneakDiceCount(State.Encounter, *Actor), SneakDie); State.SneakAttackUsedBy.Add(ActorId); }
        DiceLog = nullptr;
        ApplyDefenses(Result, ActorId, ECombatCommandType::Attack, TargetId);
        const int32 Protection = ProtectionFor(*Target);
        Result.DamagePrevented = FMath::Min(Result.Damage, Protection);
        ApplyDamage(*Target, Result.Damage);
        Result.Damage -= Result.DamagePrevented;
    }
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
            Moving->Cell = Next;
            Result.Path.Add(Next);
            State.MovementLeft -= StepCost(Next);
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
                if (A.bCureDice) { SpendSpellSlot(*User); DiceLog = &Result.EffectDice; Result.EffectModifier = HealingModifier; Amount = RollDice(CureDice,CureDie)+HealingModifier; DiceLog = nullptr; }
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
            if (!A.bBonus && State.bActionAvailable && (A.Effect == EAbilityEffect::Strike || A.Effect == EAbilityEffect::Push))
            { State.AttacksRemaining = FClassFeatureRules::AttacksPerAction(User->AdventurerRole, User->Level) - 1; }
            if (A.bBonus) { State.bBonusActionAvailable = false; } else { State.bActionAvailable = false; }
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
            State.AttacksRemaining = FClassFeatureRules::AttacksPerAction(Actor->AdventurerRole, Actor->Level) - 1;
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
        auto SpendSlot = [User, Slot]() { if (Slot == 1) { SpendSpellSlot(*User); } else { --User->SlotsByLevel[Slot - 1]; } };
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
            else if (Command.Type == ECombatCommandType::CureWounds) { SpendSlot(); Result.EffectModifier = HealingModifier; Amount = RollDice(CureDice + CombatSpellSlots::UpcastExtra(ESpell::CureWounds, Slot), CureDie) + HealingModifier; Ability = Slot == 1 ? FString(TEXT("Cure Wounds (1 slot)")) : FString::Printf(TEXT("Cure Wounds (level %d slot)"), Slot); }
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
        auto ContestDie = [this](const FCombatantState& Unit) { const int32 First = Roll(20); return CheckMode(Unit, ERollMode::Normal) == ERollMode::Disadvantage ? FMath::Min(First, Roll(20)) : First; };
        const int32 ActorRoll = ContestDie(*Actor);
        const int32 DefenderRoll = ContestDie(*Find(Command.TargetId));
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
        const bool bBonus = State.bBonusActionAvailable && FStandardActionRules::CanTakeAsBonusAction(Standard, User->AdventurerRole, User->Level);
        (bBonus ? State.bBonusActionAvailable : State.bActionAvailable) = false;
        FString Effect;
        switch (Command.Type)
        {
        case ECombatCommandType::Dash:
        {
            const bool bHeld = HasCondition(*User, ECombatCondition::Grappled) || HasCondition(*User, ECombatCondition::Restrained);
            const int32 Added = bHeld ? 0 : FMath::Max(0, BaseSpeed(*this, *User) - ArmorPenaltiesOf(*this, *User).SpeedPenaltyFeet / FWeaponRules::FeetPerCell);
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
            const FD20Outcome Check = FCheckRules::Resolve(Character(User->Id).SkillBonus(ESkill::Stealth), FMath::Clamp(Dc, FCheckRules::MinDc, FCheckRules::MaxDc), CheckMode(*User, ERollMode::Normal), State.RandomState);
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
        for (FCombatCondition& Condition : User->Conditions) { if (Condition.RoundsLeft > 0) { --Condition.RoundsLeft; } }
        User->Conditions.RemoveAll([](const FCombatCondition& Condition) { return Condition.RoundsLeft == 0; });
        AdvanceTurn();
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
    default: return Actor->Role == TEXT("Archer") ? EDamageType::Piercing : EDamageType::Slashing;
    }
}

FDamageDefenses FCombatBattle::DamageDefenses(int32 Id) const
{
    const FCombatantState* Unit = Find(Id);
    return Unit ? FDamageRules::ProfileDefenses(Unit->Role) : FDamageDefenses();
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
    const bool bWizard = Target.AdventurerRole == EAdventurerRole::Wizard || (Target.SecondaryLevel > 0 && Target.SecondaryRole == EAdventurerRole::Wizard);
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
    const bool bMassive = State.Encounter.bDeathSaves && FDeathSaveRules::InstantDeath(Real, HPBefore, Target.MaxHP);
    if (Target.bConcentrating && Target.HP > 0 && Real > 0)
    {
        // Rolls only while concentrating, so battles without concentration keep their seeded streams.
        Target.bConcentrating = FCheckRules::Resolve(Character(Target.Id).SaveBonus(EAbility::Constitution), FConcentrationRules::SaveDc(Real),
            SaveMode(Target, EAbility::Constitution, ERollMode::Normal), State.RandomState).bSuccess;
    }
    if (Target.HP == 0)
    {
        Target.GuardTurns = 0; Target.GuardAmount = 0; Target.RageTurns = 0; Target.GuardTargetId = INDEX_NONE; Target.LastSurge = 0;
        for (auto& U : State.Encounter.Combatants) { if (U.GuardTargetId == Target.Id) { U.GuardTargetId = INDEX_NONE; U.GuardTurns = 0; U.GuardAmount = 0; } }
        Target.bDodging = false; Target.bDisengaged = false; Target.bHidden = false; Target.bReadied = false; Target.HelpedById = INDEX_NONE;
        for (auto& U : State.Encounter.Combatants) { if (U.HelpedById == Target.Id) { U.HelpedById = INDEX_NONE; } }
        Target.Life = Target.Side == ECombatSide::Party ? ECombatLife::Downed : ECombatLife::Dead;
        if (bMassive) { Target.Life = ECombatLife::Dead; }
        Target.RescueTurnsLeft = Target.Life == ECombatLife::Downed ? 3 : 0;
        Target.DeathSaveSuccesses = 0; Target.DeathSaveFailures = 0; Target.TemporaryHP = 0; Target.bConcentrating = false;
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
    if (State.Phase != EBattlePhase::Fighting)
    {
        State.AttacksRemaining = 0;
        for (FCombatantState& Unit : State.Encounter.Combatants)
        {
            Unit.GuardTurns = 0; Unit.GuardAmount = 0; Unit.RageTurns = 0; Unit.GuardTargetId = INDEX_NONE; Unit.LastSurge = 0;
            Unit.bReactionAvailable = true;
            Unit.bDodging = false; Unit.bDisengaged = false; Unit.bHidden = false; Unit.bReadied = false; Unit.HelpedById = INDEX_NONE; Unit.bConcentrating = false; Unit.bShielded = false;
            if (Unit.Life == ECombatLife::Downed)
            {
                // With death saves, a stable hero survives a victory on 1 HP; anyone still dying is lost.
                const bool bSurvives = State.Phase == EBattlePhase::Victory && State.Encounter.bDeathSaves && Unit.DeathSaveSuccesses >= 3;
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
        if (--Unit.RescueTurnsLeft == 0) { Unit.Life = ECombatLife::Dead; }
        return;
    }
    if (Unit.DeathSaveSuccesses >= 3) { return; } // Stable: no more saves.
    FDeathSaveTally Tally;
    Tally.Successes = Unit.DeathSaveSuccesses; Tally.Failures = Unit.DeathSaveFailures;
    const EDeathState Outcome = FDeathSaveRules::ApplySave(Tally, Roll(20));
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
    if (LegalTargets(Id).IsEmpty() && State.MovementLeft > 0 && State.bActionAvailable)
    {
        TMap<FIntPoint, FIntPoint> Parents;
        const TMap<FIntPoint, int32> Costs = Distances(Id, &Parents);
        TOptional<FIntPoint> Best;
        int32 BestCost = MAX_int32;
        // Search in fixed cell order for a reachable firing cell. TMap iteration never chooses AI ties.
        for (int32 Y = 0; Y < State.Encounter.Height; ++Y)
        {
            for (int32 X = 0; X < State.Encounter.Width; ++X)
            {
                const FIntPoint Cell(X, Y);
                const int32* Cost = Costs.Find(Cell);
                if (!Cost || *Cost >= BestCost) { continue; }
                for (const FCombatantState& Target : State.Encounter.Combatants)
                {
                    if (CanTargetFrom(*Enemy, Cell, Target))
                    {
                        Best = Cell; BestCost = *Cost; break;
                    }
                }
            }
        }
        if (Best.IsSet() && BestCost > 0)
        {
            FIntPoint Destination = Best.GetValue();
            while (Costs[Destination] > State.MovementLeft) { Destination = Parents[Destination]; }
            Results.Add(Submit({ECombatCommandType::Move, Id, Destination}));
        }
    }
    const TArray<int32> Targets = LegalTargets(Id);
    if (!Targets.IsEmpty()) { Results.Add(Submit({ECombatCommandType::Attack, Id, {}, Targets[0]})); }
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
                Unit.bDodging || Unit.bShielded || Unit.bDisengaged || Unit.bHidden || Unit.bReadied || Unit.HelpedById != INDEX_NONE ||
                Unit.Conditions.ContainsByPredicate([](const FCombatCondition& C) { return C.SourceId != INDEX_NONE || C.Condition == ECombatCondition::Prone; })))) { return Reject(); }
        bParty |= Unit.Side == ECombatSide::Party && Unit.Life == ECombatLife::Conscious;
        bEnemy |= Unit.Side == ECombatSide::Enemy && Unit.Life == ECombatLife::Conscious;
    }
    const auto Expected = !bParty ? EBattlePhase::Defeat : !bEnemy ? EBattlePhase::Victory : EBattlePhase::Fighting;
    const auto* Active = Candidate.Find(Candidate.ActiveId());
    if (Snapshot.Phase != Expected || !Active || Active->Life != ECombatLife::Conscious ||
        Snapshot.MovementLeft < 0 || Snapshot.MovementLeft > Active->Speed * 3 ||
        Snapshot.AttacksRemaining < 0 || Snapshot.AttacksRemaining >= FClassFeatureRules::AttacksPerAction(Active->AdventurerRole, Active->Level) ||
        (Snapshot.AttacksRemaining > 0 && (Snapshot.bActionAvailable || Snapshot.Phase != EBattlePhase::Fighting)) ||
        Snapshot.SneakAttackUsedBy.ContainsByPredicate([&](int32 SneakId) { return !Candidate.Find(SneakId) || Snapshot.SneakAttackUsedBy.FilterByPredicate([SneakId](int32 Other) { return Other == SneakId; }).Num() > 1; }) ||
        (!Snapshot.bBonusActionAvailable && ([&]()
        {
            if (FFeatureKit::HasBonusFeature(*Active)) { return false; } // class features (FFeatureKit) can spend it
            // Cunning Action (#60) can spend the bonus action on Dash, Disengage or Hide.
            if (FStandardActionRules::CanTakeAsBonusAction(EStandardAction::Dash, Active->AdventurerRole, Active->Level)) { return false; }
            const auto* Build = FBuildCatalog::Find(Active->BuildId);
            if (Build && Build->KitCap > 0)
            { return (Snapshot.Phase == EBattlePhase::Fighting && Active->KitUses >= Build->KitCap) || !FBuildCatalog::Abilities(*Active).ContainsByPredicate([](const auto& A) { return A.bBonus; }); }
            if (Active->AdventurerRole == EAdventurerRole::Barbarian)
            { return Snapshot.Phase == EBattlePhase::Fighting && Active->RageTurns == 0; }
            return Active->AdventurerRole != EAdventurerRole::Fighter || (Snapshot.Phase == EBattlePhase::Fighting && Active->SecondWindUses > 1);
        })())) { return Reject(); }
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
    case ECombatCommandType::PatientDefense: case ECombatCommandType::StepOfTheWind: return Unit.KiPoints > 0 && State.bBonusActionAvailable;
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
    if (Action == ECombatCommandType::FlurryOfBlows)
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
