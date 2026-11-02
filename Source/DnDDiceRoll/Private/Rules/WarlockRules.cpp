#include "Rules/WarlockRules.h"

namespace
{
using I = EInvocation; using C = EActionCost;
// SRD 5.2.1 "Eldritch Invocation Options" (Warlock class). Index = EInvocation.
const FInvocationFacts InvocationTable[] = {
    {TEXT("None")},
    {TEXT("Agonizing Blast"), 2, I::None, true, false, true, C::Free, true, TEXT("add CHA to one damaging Warlock cantrip's damage")},
    {TEXT("Armor of Shadows"), 1, I::None, false, false, false, C::Free, true, TEXT("Mage Armor on yourself at will: AC 13 + DEX with no armor")},
    {TEXT("Ascendant Step"), 5, I::None, false, false, false, C::Free, false, TEXT("Levitate on yourself at will")},
    {TEXT("Devil's Sight"), 2, I::None, false, false, false, C::Free, false, TEXT("see normally in Dim Light and Darkness within 120 ft")},
    {TEXT("Devouring Blade"), 12, I::ThirstingBlade, false, false, false, C::Free, false, TEXT("Thirsting Blade gives two extra attacks")},
    {TEXT("Eldritch Mind"), 1, I::None, false, false, false, C::Free, true, TEXT("advantage on Constitution saves to keep Concentration")},
    {TEXT("Eldritch Smite"), 5, I::PactOfTheBlade, false, false, false, C::Free, false, TEXT("spend a Pact slot on a pact weapon hit: +1d8 force per slot level +1d8 and knock Prone")},
    {TEXT("Eldritch Spear"), 2, I::None, true, false, true, C::Free, false, TEXT("one damaging cantrip's range becomes 30 ft x warlock level")},
    {TEXT("Fiendish Vigor"), 2, I::None, false, false, false, C::Free, false, TEXT("False Life on yourself at will, taking the highest roll")},
    {TEXT("Gaze of Two Minds"), 5, I::None, false, false, false, C::BonusAction, false, TEXT("perceive through a willing creature's senses")},
    {TEXT("Gift of the Depths"), 5, I::None, false, false, false, C::Free, false, TEXT("breathe underwater, swim speed; Water Breathing once per Long Rest")},
    {TEXT("Gift of the Protectors"), 9, I::PactOfTheTome, false, false, false, C::Free, false, TEXT("a creature named in the Book of Shadows drops to 1 HP instead of 0, once per Long Rest")},
    {TEXT("Investment of the Chain Master"), 5, I::PactOfTheChain, false, false, false, C::BonusAction, false, TEXT("improved familiar: fly or swim 40 ft, bonus-action attack, your save DC")},
    {TEXT("Lessons of the First Ones"), 2, I::None, false, false, true, C::Free, false, TEXT("gain one Origin feat")},
    {TEXT("Lifedrinker"), 9, I::PactOfTheBlade, false, false, false, C::Free, false, TEXT("pact weapon hit: +1d6 necrotic, psychic or radiant; spend a Hit Die to heal")},
    {TEXT("Mask of Many Faces"), 2, I::None, false, false, false, C::Free, false, TEXT("Disguise Self at will")},
    {TEXT("Master of Myriad Forms"), 5, I::None, false, false, false, C::Free, false, TEXT("Alter Self at will")},
    {TEXT("Misty Visions"), 2, I::None, false, false, false, C::Free, false, TEXT("Silent Image at will")},
    {TEXT("One with Shadows"), 5, I::None, false, false, false, C::Free, false, TEXT("Invisibility on yourself in Dim Light or Darkness")},
    {TEXT("Otherworldly Leap"), 2, I::None, false, false, false, C::Free, false, TEXT("Jump on yourself at will")},
    {TEXT("Pact of the Blade"), 1, I::None, false, false, false, C::BonusAction, true, TEXT("pact weapon: use CHA instead of STR or DEX for its attack and damage")},
    {TEXT("Pact of the Chain"), 1, I::None, false, false, false, C::Free, false, TEXT("Find Familiar (special forms); the familiar can attack with your Reaction")},
    {TEXT("Pact of the Tome"), 1, I::None, false, false, false, C::Free, false, TEXT("Book of Shadows: three extra cantrips and two Ritual spells")},
    {TEXT("Repelling Blast"), 2, I::None, false, true, true, C::Free, false, TEXT("a cantrip attack hit pushes a Large or smaller target 10 ft")},
    {TEXT("Thirsting Blade"), 5, I::PactOfTheBlade, false, false, false, C::Free, false, TEXT("Extra Attack with the pact weapon")},
    {TEXT("Visions of Distant Realms"), 9, I::None, false, false, false, C::Free, false, TEXT("Arcane Eye without a slot")},
    {TEXT("Whispers of the Grave"), 7, I::None, false, false, false, C::Free, false, TEXT("Speak with Dead without a slot")},
    {TEXT("Witch Sight"), 15, I::None, false, false, false, C::Free, false, TEXT("Truesight 30 ft")},
};
static_assert(UE_ARRAY_COUNT(InvocationTable) == static_cast<int32>(EInvocation::Count), "one row per invocation");
int32 WarlockLevel20(int32 Level) { return FMath::Clamp(Level, 0, 20); }
}

const FInvocationFacts& FWarlockRules::Facts(EInvocation Invocation)
{
    const int32 Index = static_cast<int32>(Invocation);
    return InvocationTable[Index > 0 && Index < static_cast<int32>(EInvocation::Count) ? Index : 0];
}
int32 FWarlockRules::InvocationsKnown(int32 WarlockLevel)
{
    static const int32 Known[] = {0, 1, 3, 3, 3, 5, 5, 6, 6, 7, 7, 7, 8, 8, 8, 9, 9, 9, 10, 10, 10};
    return Known[WarlockLevel20(WarlockLevel)];
}
int32 FWarlockRules::PactSlots(int32 WarlockLevel)
{
    const int32 L = WarlockLevel20(WarlockLevel);
    return L >= 17 ? 4 : L >= 11 ? 3 : L >= 2 ? 2 : L;
}
int32 FWarlockRules::PactSlotLevel(int32 WarlockLevel)
{
    const int32 L = WarlockLevel20(WarlockLevel);
    return L == 0 ? 0 : FMath::Min(5, (L + 1) / 2);
}
int32 FWarlockRules::MagicalCunningSlots(int32 WarlockLevel)
{
    if (WarlockLevel < 2) { return 0; }
    return WarlockLevel >= 20 ? PactSlots(WarlockLevel) : (PactSlots(WarlockLevel) + 1) / 2;
}
TArray<int32> FWarlockRules::MysticArcanumLevels(int32 WarlockLevel)
{
    TArray<int32> Out;
    if (WarlockLevel >= 11) { Out.Add(6); }
    if (WarlockLevel >= 13) { Out.Add(7); }
    if (WarlockLevel >= 15) { Out.Add(8); }
    if (WarlockLevel >= 17) { Out.Add(9); }
    return Out;
}
int32 FWarlockRules::DarkOnesBlessing(int32 CharismaModifier, int32 WarlockLevel)
{
    return WarlockLevel >= 3 ? FMath::Max(1, CharismaModifier + WarlockLevel) : 0;
}
TArray<FString> FWarlockRules::FiendSpells(int32 WarlockLevel)
{
    TArray<FString> Out;
    if (WarlockLevel >= 3) { Out.Append({TEXT("Burning Hands"), TEXT("Command"), TEXT("Scorching Ray"), TEXT("Suggestion")}); }
    if (WarlockLevel >= 5) { Out.Append({TEXT("Fireball"), TEXT("Stinking Cloud")}); }
    if (WarlockLevel >= 7) { Out.Append({TEXT("Fire Shield"), TEXT("Wall of Fire")}); }
    if (WarlockLevel >= 9) { Out.Append({TEXT("Geas"), TEXT("Insect Plague")}); }
    return Out;
}
bool FWarlockRules::Has(const TArray<int32>& Held, EInvocation Invocation) { return Held.Contains(static_cast<int32>(Invocation)); }
bool FWarlockRules::CanTake(EInvocation Invocation, int32 WarlockLevel, const TArray<int32>& Held, bool bDamageCantrip, bool bAttackCantrip, FString& Why)
{
    if (Invocation <= EInvocation::None || Invocation >= EInvocation::Count) { Why = TEXT("Unknown invocation."); return false; }
    const FInvocationFacts& F = Facts(Invocation);
    if (WarlockLevel < 1) { Why = TEXT("Eldritch Invocations need Warlock 1."); return false; }
    if (WarlockLevel < F.MinLevel) { Why = FString::Printf(TEXT("%s needs Warlock %d."), F.Name, F.MinLevel); return false; }
    if (F.Requires != EInvocation::None && !Has(Held, F.Requires)) { Why = FString::Printf(TEXT("%s needs %s."), F.Name, Facts(F.Requires).Name); return false; }
    if (F.bNeedsDamageCantrip && !bDamageCantrip) { Why = FString::Printf(TEXT("%s needs a Warlock cantrip that deals damage."), F.Name); return false; }
    if (F.bNeedsAttackCantrip && !bAttackCantrip) { Why = FString::Printf(TEXT("%s needs a Warlock cantrip that makes an attack roll."), F.Name); return false; }
    // The game knows one qualifying cantrip (Eldritch Blast), so a repeatable invocation can still be taken only once.
    if (Has(Held, Invocation)) { Why = FString::Printf(TEXT("%s is already known%s."), F.Name, F.bRepeatable ? TEXT(" (repeats need a different cantrip)") : TEXT("")); return false; }
    if (Held.Num() >= InvocationsKnown(WarlockLevel)) { Why = FString::Printf(TEXT("Warlock %d knows at most %d invocation(s)."), WarlockLevel, InvocationsKnown(WarlockLevel)); return false; }
    return true;
}
bool FWarlockRules::ValidSet(const TArray<int32>& Held, int32 WarlockLevel, bool bDamageCantrip, bool bAttackCantrip, FString& Why)
{
    if (Held.Num() > InvocationsKnown(WarlockLevel)) { Why = FString::Printf(TEXT("Warlock %d knows at most %d invocation(s)."), WarlockLevel, InvocationsKnown(WarlockLevel)); return false; }
    for (int32 Index = 0; Index < Held.Num(); ++Index)
    {
        // Each must be takeable given all the others (prerequisites may be any other held invocation).
        TArray<int32> Others = Held; Others.RemoveAt(Index);
        if (!CanTake(static_cast<EInvocation>(FMath::Clamp(Held[Index], 0, 255)), WarlockLevel, Others, bDamageCantrip, bAttackCantrip, Why)) { return false; }
    }
    return true;
}
TArray<int32> FWarlockRules::DefaultSet(int32 WarlockLevel)
{
    // Combat-first order: the playable picks, then SRD utility picks, each only once its prerequisites hold.
    static const EInvocation Order[] = { I::ArmorOfShadows, I::AgonizingBlast, I::EldritchMind, I::PactOfTheBlade, I::DevilsSight, I::RepellingBlast,
        I::ThirstingBlade, I::FiendishVigor, I::MaskOfManyFaces, I::EldritchSmite, I::OneWithShadows, I::AscendantStep, I::WhispersOfTheGrave,
        I::Lifedrinker, I::VisionsOfDistantRealms, I::MistyVisions, I::DevouringBlade, I::WitchSight };
    TArray<int32> Out; FString Why;
    for (const EInvocation Pick : Order) { if (CanTake(Pick, WarlockLevel, Out, true, true, Why)) { Out.Add(static_cast<int32>(Pick)); } }
    return Out;
}