#include "Presentation/ProgressionText.h"
#include "Presentation/ActionText.h"

namespace ProgressionText
{
FString Arrow() { return FString(TEXT("→")); }

FString RewardTitle(const FProgressionFacts& Facts)
{
    return FString::Printf(TEXT("VICTORY %d OF %d  ·  SURVIVORS REACH LEVEL %d"), Facts.Victories, FExpeditionRun::EncounterCount, Facts.SurvivorLevel);
}

FString PotionLine(const FProgressionFacts& Facts)
{
    return FString::Printf(TEXT("Every card also gives +%d healing potion%s"), Facts.RewardPotions, Facts.RewardPotions == 1 ? TEXT("") : TEXT("s"));
}

FString CardName(ERewardCard Card)
{
    switch (Card)
    {
    case ERewardCard::Reserve: return TEXT("Reserve");
    case ERewardCard::Honed: return TEXT("Honed");
    case ERewardCard::Gear: break;
    }
    return TEXT("Gear");
}

FString CardRule(ERewardCard Card)
{
    switch (Card)
    {
    case ERewardCard::Reserve: return TEXT("+1 cap and +1 use on each special pool");
    case ERewardCard::Honed: return TEXT("Both specials grow stronger");
    case ERewardCard::Gear: break;
    }
    return TEXT("+1 gear rank: +1 basic damage");
}

FString NoteText(ERewardCardNote Note)
{
    switch (Note)
    {
    case ERewardCardNote::Fallen: return TEXT("Fallen");
    case ERewardCardNote::GearOnlyRun: return TEXT("Gear only in this run");
    case ERewardCardNote::ReserveTaken: return TEXT("Reserve taken");
    case ERewardCardNote::AlreadyHoned: return TEXT("Already honed");
    case ERewardCardNote::GearAtCap: return TEXT("Gear at rank 3: potions only");
    case ERewardCardNote::None: break;
    }
    return FString();
}

FString PoolChangeText(const FPoolChange& Pool)
{
    return FString::Printf(TEXT("%s %d/%d %s %d/%d"), *ExpeditionStyle::ResourceName(Pool.Resource), Pool.HeldBefore, Pool.CapBefore, *Arrow(), Pool.HeldAfter, Pool.CapAfter);
}

namespace
{
FString SpecialChangeText(const FSpecialChange& Special)
{
    switch (Special.Value)
    {
    case ESpecialValue::Range: return FString::Printf(TEXT("%s range %d %s %d"), *Special.Name, Special.Before, *Arrow(), Special.After);
    case ESpecialValue::Modifier: return FString::Printf(TEXT("%s +%d %s +%d"), *Special.Name, Special.Before, *Arrow(), Special.After);
    case ESpecialValue::Amount: break;
    }
    return FString::Printf(TEXT("%s %d %s %d"), *Special.Name, Special.Before, *Arrow(), Special.After);
}
}

TArray<FString> CardLines(const FRewardCardFacts& Card)
{
    TArray<FString> Lines;
    switch (Card.Card)
    {
    case ERewardCard::Gear:
        Lines.Add(FString::Printf(TEXT("Basic damage %d %s %d"), Card.DamageBefore.Total, *Arrow(), Card.DamageAfter.Total));
        Lines.Add(FString::Printf(TEXT("Gear rank %d %s %d"), Card.GearBefore, *Arrow(), Card.GearAfter));
        break;
    case ERewardCard::Reserve:
    {
        TArray<FString> Pools;
        for (const FPoolChange& Pool : Card.Pools) { Pools.Add(PoolChangeText(Pool)); }
        Lines.Add(Pools.IsEmpty() ? FString(TEXT("No special pool")) : FString::Join(Pools, TEXT("  ·  ")));
        break;
    }
    case ERewardCard::Honed:
        for (const FSpecialChange& Special : Card.Specials) { Lines.Add(SpecialChangeText(Special)); }
        break;
    }
    Lines.Add(FString::Printf(TEXT("Potions %d %s %d"), Card.PotionsBefore, *Arrow(), Card.PotionsAfter));
    return Lines;
}

FString ClaimLabel(const FHeroProgressFacts& Hero, const FRewardCardFacts& Card)
{
    if (!Card.bAvailable) { return NoteText(Card.Note); }
    return FString::Printf(TEXT("Give %s to %s"), *CardName(Card.Card), *Hero.Name);
}

FString DamageBreakdown(const FHeroProgressFacts& Hero)
{
    const FBasicDamageParts& D = Hero.Damage;
    FString Text = FString::Printf(TEXT("Basic damage %d = %d build"), D.Total, D.Build);
    if (D.Levels) { Text += FString::Printf(TEXT(" + %d level%s"), D.Levels, D.Levels == 1 ? TEXT("") : TEXT("s")); }
    if (D.Gear) { Text += FString::Printf(TEXT(" + %d gear"), D.Gear); }
    if (D.Rage) { Text += FString::Printf(TEXT(" + %d Rage"), D.Rage); }
    return Text;
}

FString UpgradesLine(const FHeroProgressFacts& Hero)
{
    TArray<FString> Upgrades;
    if (Hero.bReserve) { Upgrades.Add(CardName(ERewardCard::Reserve)); }
    if (Hero.bHoned) { Upgrades.Add(CardName(ERewardCard::Honed)); }
    return Upgrades.IsEmpty() ? FString() : TEXT("Upgrades: ") + FString::Join(Upgrades, TEXT(", "));
}

FString LevelGainLine(const FHeroProgressFacts& Hero)
{
    return Hero.bLevelled ? FString::Printf(TEXT("basic %d %s %d"), Hero.DamageBeforeLevel, *Arrow(), Hero.Damage.Total) : FString();
}

FString FallenLine(const FHeroProgressFacts& Hero) { return FString::Printf(TEXT("fell at Lv%d"), Hero.Level); }

FString CampLine(const FHeroProgressFacts& Hero)
{
    FString Text = FString::Printf(TEXT("%s  HP %d/%d %s %d/%d"), *Hero.Name, Hero.HpBefore, Hero.MaxHP, *Arrow(), Hero.HpAfter, Hero.MaxHP);
    for (const FPoolChange& Pool : Hero.Camp) { Text += TEXT("  ·  ") + PoolChangeText(Pool); }
    return Text;
}
}
