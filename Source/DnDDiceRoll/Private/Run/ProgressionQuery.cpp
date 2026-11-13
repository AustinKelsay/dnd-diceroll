#include "Run/ProgressionQuery.h"
#include "Party/BuildCatalog.h"
#include "Party/PartyContinuity.h"

namespace
{
using namespace BuildRules;

TArray<FPoolFacts> PoolsOf(const FCombatantState& Hero)
{
    TArray<FPoolFacts> Pools;
    if (!FBuildCatalog::Find(Hero.BuildId)) { return Pools; }
    const FResourceCaps Caps = FBuildCatalog::Caps(Hero);
    // The party card's order: Kit, Wind, Slot, Maneuver, Rage. Only the pools the build has.
    auto Add = [&Pools](EActionResource Resource, int32 Held, int32 Cap) { if (Cap > 0) { FPoolFacts Pool; Pool.Resource = Resource; Pool.Held = Held; Pool.Cap = Cap; Pools.Add(Pool); } };
    Add(EActionResource::Kit, Hero.KitUses, Caps.Kit);
    Add(EActionResource::Wind, Hero.SecondWindUses, Caps.Wind);
    Add(EActionResource::Slot, Hero.SpellSlots, Caps.Slot);
    Add(EActionResource::Maneuver, Hero.ManeuverUses, Caps.Maneuver);
    Add(EActionResource::Rage, Hero.RageUses, Caps.Rage);
    return Pools;
}

TArray<FPoolChange> PoolChanges(const FCombatantState& Before, const FCombatantState& After)
{
    TArray<FPoolChange> Changes;
    const TArray<FPoolFacts> From = PoolsOf(Before), To = PoolsOf(After);
    for (int32 I = 0; I < From.Num() && I < To.Num(); ++I)
    {
        FPoolChange Change;
        Change.Resource = From[I].Resource; Change.HeldBefore = From[I].Held; Change.CapBefore = From[I].Cap; Change.HeldAfter = To[I].Held; Change.CapAfter = To[I].Cap;
        Changes.Add(Change);
    }
    return Changes;
}

/** The number a special's before -> after line shows: Swap's range; for the specials whose numbers are constants, the modifier they add;
 * otherwise the ability's amount. */
void ValueOf(const FCombatantState& Hero, const FAbilityFacts& A, ESpecialValue& Kind, int32& Value)
{
    const int32 Bonus = FBuildCatalog::HonedBonus(Hero, A.Command);
    Kind = ESpecialValue::Modifier;
    if (A.Effect == EAbilityEffect::Swap) { Kind = ESpecialValue::Range; Value = A.Range; }
    else if (A.bCureDice || A.Command == ECombatCommandType::CureWounds) { Value = HealingModifier + Bonus; }
    else if (A.Effect == EAbilityEffect::Missile) { Value = MissileDarts * DartBonus + Bonus; }
    else if (A.Effect == EAbilityEffect::Rage) { Value = RageBonus + Bonus; }
    else if (A.Command == ECombatCommandType::SecondWind) { Value = FighterLevel + Bonus; }
    else if (A.Command == ECombatCommandType::PrecisionStrike) { Value = PrecisionBonus + Bonus; }
    else { Kind = ESpecialValue::Amount; Value = A.Amount; }
}

TArray<FSpecialChange> SpecialChanges(const FCombatantState& Before, const FCombatantState& After)
{
    TArray<FSpecialChange> Changes;
    const TArray<FAbilityFacts> From = FBuildCatalog::Abilities(Before), To = FBuildCatalog::Abilities(After);
    for (int32 I = 0; I < From.Num() && I < To.Num(); ++I)
    {
        FSpecialChange Change;
        Change.Command = From[I].Command; Change.Name = From[I].Name;
        ESpecialValue AfterKind = ESpecialValue::Amount;
        ValueOf(Before, From[I], Change.Value, Change.Before);
        ValueOf(After, To[I], AfterKind, Change.After);
        Changes.Add(Change);
    }
    return Changes;
}

}

FBasicDamageParts FProgressionQuery::BasicDamage(const FCombatantState& Hero)
{
    FBasicDamageParts Parts;
    const FBuildDefinition* Build = FBuildCatalog::Find(Hero.BuildId);
    Parts.Total = FBuildCatalog::AttackModifier(Hero);
    Parts.Build = Build ? Build->Profile.DamageBonus : Hero.DamageBonus;
    Parts.Levels = Hero.DamageBonus - Parts.Build;
    Parts.Gear = Hero.WeaponRank;
    Parts.Rage = Parts.Total - Hero.DamageBonus - Hero.WeaponRank;
    return Parts;
}

FProgressionFacts FProgressionQuery::For(const FExpeditionRun& Run)
{
    FProgressionFacts Facts;
    const FExpeditionProgress& P = Run.Progress();
    Facts.Phase = P.Phase;
    Facts.Victories = P.EncountersCompleted;
    Facts.bUpgradeCards = FExpeditionRun::OffersUpgradeCards(P.Ruleset, P.CatalogRevision);
    const bool bReward = P.Phase == EExpeditionPhase::Reward && !P.History.IsEmpty();
    const bool bCamp = P.Phase == EExpeditionPhase::Recovery && !P.bCampResolved;
    // Victories 1-4 level their survivors; the gain is "the last" until the next fight starts.
    const bool bAfterLevelling = P.EncountersCompleted >= 1 && P.EncountersCompleted < FExpeditionRun::EncounterCount &&
        (P.Phase == EExpeditionPhase::Reward || P.Phase == EExpeditionPhase::Recovery || P.Phase == EExpeditionPhase::Route);
    if (bReward) { Facts.RewardPotions = P.History.Last().Route.RewardPotions; Facts.SurvivorLevel = FMath::Min(5, P.EncountersCompleted + 1); }
    const FCombatBattle& Battle = Run.Battle();
    FCombatBattle Camped = Battle;
    FString Error;
    const bool bCamped = bCamp && FPartyContinuity::Recover(Camped, Error);
    for (const FCombatantState& Hero : Battle.Snapshot().Encounter.Combatants)
    {
        if (Hero.Side != ECombatSide::Party) { continue; }
        FHeroProgressFacts H;
        H.Id = Hero.Id; H.Name = Hero.Name; H.Life = Hero.Life; H.Level = Hero.Level;
        H.bLevelled = bAfterLevelling && Hero.Life != ECombatLife::Dead && Hero.Level == FMath::Min(5, P.EncountersCompleted + 1);
        H.LevelBefore = H.bLevelled ? Hero.Level - 1 : Hero.Level;
        H.Damage = BasicDamage(Hero);
        const FBuildDefinition* Build = FBuildCatalog::Find(Hero.BuildId);
        H.DamageBeforeLevel = H.Damage.Total - (H.bLevelled && Build ? Hero.DamageBonus - FBuildCatalog::BasicDamage(*Build, H.LevelBefore) : 0);
        H.GearRank = Hero.WeaponRank; H.bReserve = Hero.bReserve; H.bHoned = Hero.bHoned;
        H.Caps = FBuildCatalog::Caps(Hero);
        H.Pools = PoolsOf(Hero);
        H.MaxHP = Hero.MaxHP; H.HpBefore = H.HpAfter = Hero.HP;
        if (bReward)
        {
            for (const ERewardCard Card : {ERewardCard::Gear, ERewardCard::Reserve, ERewardCard::Honed})
            {
                // The reward rule itself on a copy of the battle, after the run's own checks in FExpeditionRun::ClaimReward's order:
                // the facts are what the claim would commit. No random numbers are drawn (the claim's route offer is not part of the card).
                FRewardCardFacts C;
                C.Card = Card;
                FCombatBattle Claimed = Battle;
                FString Refused;
                if (Hero.Life != ECombatLife::Conscious) { C.Note = ERewardCardNote::Fallen; }
                else if (Card != ERewardCard::Gear && !Facts.bUpgradeCards) { C.Note = ERewardCardNote::GearOnlyRun; }
                else if (!FPartyContinuity::Reward(Claimed, Hero.Id, Facts.RewardPotions, Card, Refused)) { C.Note = Card == ERewardCard::Reserve ? ERewardCardNote::ReserveTaken : ERewardCardNote::AlreadyHoned; }
                else { C.bAvailable = true; C.Note = Card == ERewardCard::Gear && Hero.WeaponRank >= 3 ? ERewardCardNote::GearAtCap : ERewardCardNote::None; }
                const FCombatantState& After = C.bAvailable ? *Claimed.Find(Hero.Id) : Hero;
                C.DamageBefore = H.Damage; C.DamageAfter = BasicDamage(After);
                C.GearBefore = Hero.WeaponRank; C.GearAfter = After.WeaponRank;
                C.Pools = PoolChanges(Hero, After);
                C.Specials = SpecialChanges(Hero, After);
                C.PotionsBefore = Hero.Potions; C.PotionsAfter = After.Potions;
                H.Cards.Add(C);
            }
        }
        if (bCamped && Hero.Life == ECombatLife::Conscious)
        {
            const FCombatantState& After = *Camped.Find(Hero.Id);
            H.bCamp = true; H.HpAfter = After.HP;
            H.Camp = PoolChanges(Hero, After);
        }
        Facts.Heroes.Add(H);
    }
    return Facts;
}
