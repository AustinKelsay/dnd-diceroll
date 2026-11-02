#pragma once

#include "CoreMinimal.h"
#include "Rules/StandardActionRules.h"

/** SRD 5.2.1 Eldritch Invocations (Warlock 1+). Order is the save format: append only. */
enum class EInvocation : uint8
{
    None,
    AgonizingBlast, ArmorOfShadows, AscendantStep, DevilsSight, DevouringBlade, EldritchMind, EldritchSmite, EldritchSpear,
    FiendishVigor, GazeOfTwoMinds, GiftOfTheDepths, GiftOfTheProtectors, InvestmentOfTheChainMaster, LessonsOfTheFirstOnes,
    Lifedrinker, MaskOfManyFaces, MasterOfMyriadForms, MistyVisions, OneWithShadows, OtherworldlyLeap,
    PactOfTheBlade, PactOfTheChain, PactOfTheTome, RepellingBlast, ThirstingBlade, VisionsOfDistantRealms, WhispersOfTheGrave, WitchSight,
    Count
};

/** One invocation's SRD prerequisites and how the game uses it. */
struct FInvocationFacts
{
    const TCHAR* Name = TEXT("");
    /** Minimum Warlock level (SRD "Prerequisite: Level N+ Warlock"); 1 = no level prerequisite. */
    int32 MinLevel = 1;
    /** Another invocation the warlock must already have (Pact of the Blade, Chain or Tome, or Thirsting Blade). */
    EInvocation Requires = EInvocation::None;
    /** Needs a Warlock cantrip that deals damage (Agonizing Blast, Eldritch Spear) or makes an attack roll (Repelling Blast). */
    bool bNeedsDamageCantrip = false;
    bool bNeedsAttackCantrip = false;
    /** "You can gain this invocation more than once" (each time with a different cantrip). */
    bool bRepeatable = false;
    EActionCost Cost = EActionCost::Free;
    /** True when the battle engine resolves it; false = listed with its SRD text, not modelled. */
    bool bPlayable = false;
    const TCHAR* Effect = TEXT("");
};

/** Warlock class tables (SRD 5.2.1 levels 1-20) as pure data; the engine plays levels 1-5 (FClassFeatureRules::EngineLevelCap). */
struct DNDDICEROLL_API FWarlockRules
{
    static const FInvocationFacts& Facts(EInvocation Invocation);
    /** Warlock table "Eldritch Invocations": 1,3,3,3,5,5,6,6,7,7,7,8,8,8,9,9,9,10,10,10. */
    static int32 InvocationsKnown(int32 WarlockLevel);
    /** Pact Magic: 1 slot at 1, 2 at 2-10, 3 at 11-16, 4 at 17-20. */
    static int32 PactSlots(int32 WarlockLevel);
    /** Pact Magic slot level: 1,1,2,2,3,3,4,4,5 then 5. */
    static int32 PactSlotLevel(int32 WarlockLevel);
    /** Magical Cunning (2+): regain expended Pact slots up to half the maximum, rounded up; Eldritch Master (20) regains all. Once per Long Rest. */
    static int32 MagicalCunningSlots(int32 WarlockLevel);
    /** Mystic Arcanum spell levels known: 6 at 11, 7 at 13, 8 at 15, 9 at 17 (each once per Long Rest, no slot). */
    static TArray<int32> MysticArcanumLevels(int32 WarlockLevel);
    /** Fiend Patron 3+: Dark One's Blessing temporary HP = CHA modifier + warlock level, minimum 1. */
    static int32 DarkOnesBlessing(int32 CharismaModifier, int32 WarlockLevel);
    /** Fiend Patron always-prepared spells by warlock level (3: Burning Hands, Command, Scorching Ray, Suggestion; 5; 7; 9). */
    static TArray<FString> FiendSpells(int32 WarlockLevel);
    /** Can this warlock take this invocation now, given the ones already held? Why is set when not. */
    static bool CanTake(EInvocation Invocation, int32 WarlockLevel, const TArray<int32>& Held, bool bDamageCantrip, bool bAttackCantrip, FString& Why);
    /** Whole-set check: count within InvocationsKnown, every prerequisite met, repeats only for repeatable invocations. */
    static bool ValidSet(const TArray<int32>& Held, int32 WarlockLevel, bool bDamageCantrip, bool bAttackCantrip, FString& Why);
    /** Deterministic default picks for a warlock of this level (all prerequisites met, fills InvocationsKnown). */
    static TArray<int32> DefaultSet(int32 WarlockLevel);
    static bool Has(const TArray<int32>& Held, EInvocation Invocation);
};