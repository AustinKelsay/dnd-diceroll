#include "Presentation/ExpeditionPlayback.h"
#include "Party/BuildCatalog.h"

namespace
{
const FLinearColor PlaybackGold(1.0f, 0.78f, 0.25f);
const FLinearColor Blood(1.0f, 0.32f, 0.22f);
const FLinearColor Mint(0.35f, 1.0f, 0.55f);
const FLinearColor Ash(0.75f, 0.78f, 0.82f);

FVector2D CellOf(FIntPoint Cell) { return FVector2D(Cell.X, Cell.Y); }
float EaseInOut(float Alpha) { return Alpha * Alpha * (3.0f - 2.0f * Alpha); }
float Bump(float Alpha) { return FMath::Sin(Alpha * PI); }
FVector2D SafeDirection(FVector2D From, FVector2D To)
{
    const FVector2D Delta = To - From;
    return Delta.IsNearlyZero() ? FVector2D(1, 0) : Delta.GetSafeNormal();
}
int32 DiceSum(const TArray<FDieRoll>& Dice, int32 Modifier)
{
    int32 Sum = Modifier;
    for (const FDieRoll& Die : Dice) { Sum += Die.Value; }
    return Sum;
}
FString DiceText(const TArray<FDieRoll>& Dice, int32 Modifier)
{
    FString Text;
    for (const FDieRoll& Die : Dice) { Text += (Text.IsEmpty() ? TEXT("") : TEXT(" + ")) + FString::FromInt(Die.Value); }
    if (Modifier) { Text += FString::Printf(TEXT(" %s %d"), Modifier > 0 ? TEXT("+") : TEXT("-"), FMath::Abs(Modifier)); }
    return Text;
}
FString TurnTitle(const FCombatantState& Unit)
{
    return Unit.Side == ECombatSide::Party ? Unit.Name.ToUpper() + TEXT("'S TURN") : Unit.Name.ToUpper();
}
}

namespace ExpeditionStyle
{
FLinearColor PartyColor(int32 Id)
{
    const FLinearColor Colors[] = {{0.25f, 0.6f, 1.0f}, {0.35f, 0.9f, 0.55f}, {1.0f, 0.82f, 0.3f}, {0.75f, 0.5f, 1.0f}};
    return Colors[FMath::Clamp(Id - 1, 0, 3)];
}
FLinearColor UnitColor(const FCombatantState& Unit)
{
    return Unit.Side == ECombatSide::Party ? PartyColor(Unit.Id) : FLinearColor(1.0f, 0.42f, 0.32f);
}
bool IsFriendlyAction(ECombatCommandType Action, const FCombatantState* Actor)
{
    if (Actor)
    {
        for (const auto& Ability : FBuildCatalog::Abilities(*Actor))
        { if (Ability.Command == Action) { return Ability.Target != EAbilityTarget::Enemy; } }
    }
    return Action == ECombatCommandType::SecondWind || Action == ECombatCommandType::CureWounds ||
        Action == ECombatCommandType::Rescue || Action == ECombatCommandType::Potion;
}
FString TargetChip(const FCombatBattle& Rules, int32 ActorId, ECombatCommandType Action, int32 TargetId)
{
    const auto F = Rules.Forecast(ActorId, Action, TargetId);
    const auto* Target = Rules.Find(TargetId);
    const auto* Actor = Rules.Find(ActorId);
    if (!F.bValid || !Target || !Actor) { return FString(); }
    for (const auto& A : FBuildCatalog::Abilities(*Actor))
    {
        if (A.Command != Action) { continue; }
        // Rage's +2 applies to basic attacks only (the catalog says "+2 basic damage").
        if (A.Effect == EAbilityEffect::Rage) { return TEXT("Rage: +2 basic damage, -1 taken"); }
        if (A.Effect == EAbilityEffect::Swap) { return TEXT("Swap positions"); }
        if (A.Effect == EAbilityEffect::Protect) { return FString::Printf(TEXT("Protect -%d / next turn"), A.Amount); }
    }
    FString Amount = F.MinAmount == F.MaxAmount ? FString::FromInt(F.MinAmount)
        : F.bDiscreteAmounts ? FString::Printf(TEXT("%d or %d"), F.MinAmount, F.MaxAmount)
        : FString::Printf(TEXT("%d-%d"), F.MinAmount, F.MaxAmount);
    FString Label = F.bHealing ? TEXT("Heal +") + Amount
        : ((F.bAttackRoll || F.bSavingThrow) ? FString::Printf(TEXT("%d%% | "), F.HitChancePercent) : FString(TEXT("Auto | "))) + Amount;
    for (const auto& A : FBuildCatalog::Abilities(*Actor))
    { if (A.Command == Action && A.Effect == EAbilityEffect::Push && F.MinAmount < Target->HP) { Label += TEXT(" + push if open"); } }
    if (F.bCanDrop) { Label += F.MaxAmount >= Target->HP ? TEXT(" | can drop") : TEXT(" | crit drop"); }
    return Label;
}

FString ChipLabel(const FString& Name, const FString& Detail, float MaxWidth, TFunctionRef<float(const FString&)> Measure)
{
    if (Name.IsEmpty()) { return Detail; }
    const FString Tail = TEXT(": ") + Detail;
    if (Measure(Name + Tail) <= MaxWidth) { return Name + Tail; }
    // Keep the longest name prefix that still fits beside the whole detail.
    for (int32 Keep = Name.Len() - 1; Keep >= 1; --Keep)
    {
        const FString Candidate = Name.Left(Keep).TrimEnd() + TEXT("...") + Tail;
        if (Measure(Candidate) <= MaxWidth) { return Candidate; }
    }
    return Detail;
}

FString ForecastAmount(const FActionForecast& Forecast)
{
    if (Forecast.MaxAmount == 0) { return Forecast.Formula; }
    const TCHAR* Verb = Forecast.bHealing ? TEXT("Heals") : TEXT("Deals");
    if (Forecast.MinAmount == Forecast.MaxAmount) { return FString::Printf(TEXT("%s %d  (%s)"), Verb, Forecast.MinAmount, *Forecast.Formula); }
    return Forecast.bDiscreteAmounts
        ? FString::Printf(TEXT("%s %d or %d  (%s)"), Verb, Forecast.MinAmount, Forecast.MaxAmount, *Forecast.Formula)
        : FString::Printf(TEXT("%s %d-%d  (%s)"), Verb, Forecast.MinAmount, Forecast.MaxAmount, *Forecast.Formula);
}
FString ActionName(ECombatCommandType Action)
{
    switch (Action)
    {
    case ECombatCommandType::SecondWind: return TEXT("Second Wind");
    case ECombatCommandType::CureWounds: return TEXT("Cure Wounds");
    case ECombatCommandType::MagicMissile: return TEXT("Magic Missile");
    case ECombatCommandType::Rescue: return TEXT("Rescue");
    case ECombatCommandType::Potion: return TEXT("Healing Potion");
    case ECombatCommandType::Move: return TEXT("Move");
    case ECombatCommandType::EndTurn: return TEXT("End Turn");
    case ECombatCommandType::PrecisionStrike: return TEXT("Precision Strike");
    default: return TEXT("Attack");
    }
}
const FCombatantState* FindUnit(const FCombatSnapshot& Snapshot, int32 Id)
{
    return Snapshot.Encounter.Combatants.FindByPredicate([Id](const FCombatantState& Unit) { return Unit.Id == Id; });
}
}

void FExpeditionPlayback::Reset(const FCombatSnapshot& Snapshot)
{
    Steps.Reset(); Current = 0; StepTime = 0; bStepBegun = false;
    Floaters.Reset(); Shots.Reset(); Glitter.Reset();
    Dice = FDiceTray(); TurnBanner = FTurnBanner(); Focus = INDEX_NONE;
    Displays.Reset();
    Sync(Snapshot);
    for (auto& Pair : Displays) { Pair.Value.ShownHP = Pair.Value.TrailHP = Pair.Value.HP; }
}

void FExpeditionPlayback::Sync(const FCombatSnapshot& Snapshot)
{
    TSet<int32> Present;
    for (const FCombatantState& Unit : Snapshot.Encounter.Combatants)
    {
        Present.Add(Unit.Id);
        FUnitDisplay* Existing = Displays.Find(Unit.Id);
        FUnitDisplay& Display = Existing ? *Existing : Displays.Add(Unit.Id);
        if (!Existing)
        {
            Display.Facing = Unit.Side == ECombatSide::Party ? FVector2D(1, 0) : FVector2D(-1, 0);
            Display.ShownHP = Display.TrailHP = Unit.HP;
        }
        Display.Id = Unit.Id;
        Display.Position = CellOf(Unit.Cell);
        Display.Lean = FVector2D::ZeroVector;
        Display.Hop = 0;
        Display.HP = Unit.HP;
        Display.MaxHP = FMath::Max(1, Unit.MaxHP);
        Display.Life = Unit.Life;
        Display.Tilt = Unit.Life == ECombatLife::Conscious ? 0.0f : 1.0f;
        if (!Existing) { Display.Pose = ExpeditionPose::Rest(Display.Life); Display.bPoseSnap = true; ++Display.PoseSerial; }
        else { Converge(Display, false); }
    }
    for (auto It = Displays.CreateIterator(); It; ++It) { if (!Present.Contains(It.Key())) { It.RemoveCurrent(); } }
}

void FExpeditionPlayback::SetPose(FUnitDisplay& Unit, EUnitPose Pose)
{
    if (Unit.Pose == Pose && !Unit.bPoseSnap) { return; }
    Unit.Pose = Pose;
    Unit.bPoseSnap = false;
    ++Unit.PoseSerial;
}

void FExpeditionPlayback::Converge(FUnitDisplay& Unit, bool bSnap)
{
    const EUnitPose Rest = ExpeditionPose::Rest(Unit.Life);
    if (Unit.Pose == Rest && (!bSnap || Unit.bPoseSnap)) { return; }
    Unit.Pose = Rest;
    Unit.bPoseSnap = bSnap;
    ++Unit.PoseSerial;
}

void FExpeditionPlayback::Add(float Duration, TFunction<void()> Begin, TFunction<void(float)> Update)
{
    Steps.Add({Duration, MoveTemp(Begin), MoveTemp(Update)});
}

void FExpeditionPlayback::Cue(EExpeditionCue Value) const
{
    if (!bSkipping && OnCue) { OnCue(Value); }
}

void FExpeditionPlayback::Shake(float Amount) const
{
    if (!bSkipping && OnShake) { OnShake(Amount); }
}

void FExpeditionPlayback::ShowBanner(const FString& Title, const FString& Subtitle, FLinearColor Color, float Life)
{
    TurnBanner = {Title, Subtitle, Color, 0.0f, Life};
}

void FExpeditionPlayback::ShowText(FVector2D Cell, const FString& Text, FLinearColor Color, float Scale, float Life)
{
    // Stack texts that share a cell so simultaneous results stay readable.
    int32 Overlap = 0;
    for (const FFloatingText& Existing : Floaters) { Overlap += Existing.Cell.Equals(Cell, 0.6f) && Existing.Age < 0.35f ? 1 : 0; }
    Floaters.Add({Text, Color, Cell, -0.12f * Overlap, Life, Scale});
}

void FExpeditionPlayback::ShowSparkles(FVector2D Cell, FLinearColor Color, int32 Count)
{
    for (int32 Index = 0; Index < Count; ++Index)
    {
        Glitter.Add({Cell, Color, -0.05f * Index, 0.9f, Index * (2.0f * PI / Count)});
    }
}

TArray<FPlaybackProjectile> FExpeditionPlayback::MagicMissileBolts(FVector2D From, FVector2D To)
{
    TArray<FPlaybackProjectile> Bolts;
    for (int32 Index = 0; Index < 3; ++Index)
    {
        Bolts.Add({From, To, -MissileStagger * Index, MissileFlight, (Index - 1) * -70.0f, 0.13f, FLinearColor(0.55f, 0.3f, 1.0f), false});
    }
    return Bolts;
}

FPlaybackProjectile FExpeditionPlayback::RangedBolt(FVector2D From, FVector2D To, EAdventurerRole Role, bool bArrow)
{
    // A caster's bolt takes the colour of its magic (#150 batch 3 adds the last three): the Wizard's orange fire, the Cleric's amber, the Sorcerer's crimson-magenta ember (moved off orange-red in batch 4, so it never reads as the Wizard's Fire Bolt beside it), the
    // Warlock's eldritch green (its glow-mask colour) and the Druid's leaf-gold thorn flame; batch 4 adds the Bard's teal dissonant note and the Artificer's cyan
    // (its spell specials only: its basic attack is a crossbow bolt, an arrow). Any other shooter's arrow is the pale arrow colour.
    const FLinearColor Color = Role == EAdventurerRole::Wizard ? FLinearColor(1.0f, 0.45f, 0.1f) :
        Role == EAdventurerRole::Cleric ? FLinearColor(1.0f, 0.68f, 0.2f) :
        Role == EAdventurerRole::Sorcerer ? FLinearColor(0.95f, 0.07f, 0.38f) :
        Role == EAdventurerRole::Warlock ? FLinearColor(0.35f, 1.0f, 0.3f) :
        Role == EAdventurerRole::Druid ? FLinearColor(0.75f, 0.95f, 0.22f) :
        Role == EAdventurerRole::Bard ? FLinearColor(0.15f, 0.95f, 0.65f) :
        Role == EAdventurerRole::Artificer ? FLinearColor(0.25f, 0.75f, 1.0f) : FLinearColor(0.85f, 0.75f, 0.6f);
    return {From, To, 0.0f, 0.3f, bArrow ? 40.0f : 20.0f, bArrow ? 0.1f : 0.2f, Color, bArrow};
}

void FExpeditionPlayback::PreviewImpact(int32 TargetId, EImpactPreview Effect, FVector2D From, EImpactMoment Moment, EAdventurerRole Shooter)
{
    FUnitDisplay* Target = Displays.Find(TargetId);
    if (!Target) { return; }
    const bool bHit = Moment == EImpactMoment::Landing;
    bHeldFlash = bHit;
    if (bHit)
    {
        Target->Flash = 1.0f;
        if (Target->Life == ECombatLife::Conscious) { SetPose(*Target, ExpeditionPose::ForDamage()); }
        ShowText(Target->Position, TEXT("-7"), Blood, 1.25f);
    }
    // The bolts AddAttack throws, aged as production would have them: Flight half way along the first bolt's path, a landing just before the
    // bolt in question arrives (the last bolt for the hit, the impact step begins while it is still arriving). Bolts that production would already have removed are gone.
    HeldShots.Reset();
    const FVector2D To = Target->Position;
    if (Effect == EImpactPreview::Missile) { HeldShots = MagicMissileBolts(From, To); }
    else if (Effect == EImpactPreview::Arrow) { HeldShots.Add(RangedBolt(From, To, EAdventurerRole::Ranger, true)); }
    else if (Effect == EImpactPreview::Bolt) { HeldShots.Add(RangedBolt(From, To, EAdventurerRole::Ranger, false)); }
    else if (Effect != EImpactPreview::Hit)
    {
        // A named shooter's bolt has its class colour; without one the effect says which (Fire is the Wizard's, Spell the Cleric's).
        const EAdventurerRole Role = Shooter != EAdventurerRole::None ? Shooter : Effect == EImpactPreview::Fire ? EAdventurerRole::Wizard : EAdventurerRole::Cleric;
        HeldShots.Add(RangedBolt(From, To, Role, false));
    }
    const float Duration = HeldShots.IsEmpty() ? 0.0f : HeldShots[0].Duration;
    const int32 Landing = Moment == EImpactMoment::FirstLanding ? 0 : Moment == EImpactMoment::SecondLanding ? 1 : Effect == EImpactPreview::Missile ? 2 : 0;
    // Seconds since the first bolt left.
    const float Time = Moment == EImpactMoment::Release ? 0.03f : Moment == EImpactMoment::Flight ? 0.5f * Duration : 0.92f * Duration + MissileStagger * (Effect == EImpactPreview::Missile ? Landing : 0);
    for (FPlaybackProjectile& Shot : HeldShots) { Shot.Age += Time; }
    HeldShots.RemoveAll([](const FPlaybackProjectile& Shot) { return Shot.Age > Shot.Duration; });
    Shots = HeldShots;
}

void FExpeditionPlayback::HoldImpact(int32 UnitId)
{
    if (FUnitDisplay* Display = Displays.Find(UnitId); Display && bHeldFlash) { Display->Flash = 1.0f; }
    if (!HeldShots.IsEmpty()) { Shots = HeldShots; }
}

void FExpeditionPlayback::ReleaseImpact(int32 TargetId)
{
    bHeldFlash = false;
    if (!HeldShots.IsEmpty()) { HeldShots.Reset(); Shots.Reset(); }
    if (FUnitDisplay* Target = Displays.Find(TargetId)) { Converge(*Target, false); }
}

void FExpeditionPlayback::Enqueue(const TArray<FCombatResult>& Results, const FCombatSnapshot& After)
{
    if (!Results.ContainsByPredicate([](const auto& R) { return R.bAccepted; })) { return; }
    bool bTurnEnded = false;
    for (const FCombatResult& Result : Results)
    {
        if (!Result.bAccepted) { continue; }
        AddResult(Result, After);
        bTurnEnded |= Result.Type == ECombatCommandType::EndTurn;
    }
    // Announce what the committed state means once its causes have played.
    Add(0.0f, [this, After, bTurnEnded]()
    {
        Focus = INDEX_NONE;
        if (After.Phase == EBattlePhase::Victory)
        {
            ShowBanner(TEXT("VICTORY"), TEXT("The battle is won"), PlaybackGold, 2.4f); Cue(EExpeditionCue::Victory);
        }
        else if (After.Phase == EBattlePhase::Defeat)
        {
            ShowBanner(TEXT("DEFEAT"), TEXT("No adventurer can continue"), Blood, 2.8f); Cue(EExpeditionCue::Defeat);
        }
        else if (bTurnEnded && After.TurnOrder.IsValidIndex(After.TurnIndex))
        {
            if (const FCombatantState* Next = ExpeditionStyle::FindUnit(After, After.TurnOrder[After.TurnIndex]))
            {
                const bool bParty = Next->Side == ECombatSide::Party;
                ShowBanner(TurnTitle(*Next), FString::Printf(TEXT("%s  ·  Round %d"), bParty ? *Next->Role : TEXT("Enemy turn"), After.Round),
                    ExpeditionStyle::UnitColor(*Next), 1.2f);
                Cue(bParty ? EExpeditionCue::PartyTurn : EExpeditionCue::EnemyTurn);
            }
        }
    });
    Add(0.0f, [this, After]() { Sync(After); });
}

void FExpeditionPlayback::AddResult(const FCombatResult& Result, const FCombatSnapshot& After)
{
    if (Result.Type == ECombatCommandType::Special1 || Result.Type == ECombatCommandType::Special2)
    {
        const FCombatantState* SpecialActor = ExpeditionStyle::FindUnit(After, Result.ActorId);
        bool bSpell = false;
        if (SpecialActor)
        {
            for (const FAbilityFacts& Ability : FBuildCatalog::Abilities(*SpecialActor)) { if (Ability.Command == Result.Type) { bSpell = Ability.bSpell; } }
        }
        bool bBoltThrown = false;
        for (const auto& Effect : Result.Effects)
        {
            const bool bFar = SpecialActor && FMath::Abs(SpecialActor->Cell.X - Effect.Cell.X) + FMath::Abs(SpecialActor->Cell.Y - Effect.Cell.Y) > 1;
            // A Mitigation entry is the "Prevented N" note on a victim of a damaging special, not the actor's own effect: the ability's own entry poses the actor.
            const EUnitPose ActorPose = Effect.Effect == EAbilityEffect::Mitigation ? EUnitPose::Idle : ExpeditionPose::ForEffect(Effect.Effect, bFar, bSpell);
            // The artificer's spell special aimed at an enemy from range throws a cyan bolt (#150 batch 4), once per special, at the Cast clip's release frame, before the
            // hit lands; every other special is as before. The target must be a standing enemy and the effect one that damages it.
            const FCombatantState* BoltTarget = ExpeditionStyle::FindUnit(After, Effect.TargetId);
            const bool bDamages = Effect.Effect == EAbilityEffect::Strike || Effect.Effect == EAbilityEffect::Area || Effect.Effect == EAbilityEffect::Push ||
                Effect.Effect == EAbilityEffect::Surge || Effect.Effect == EAbilityEffect::Missile;
            if (!bBoltThrown && bSpell && bFar && bDamages && SpecialActor && SpecialActor->AdventurerRole == EAdventurerRole::Artificer && BoltTarget && BoltTarget->Side == ECombatSide::Enemy)
            {
                bBoltThrown = true;
                const int32 BoltActorId = Result.ActorId;
                const FVector2D BoltFrom = CellOf(SpecialActor->Cell), BoltTo = CellOf(Effect.Cell);
                Add(CastReleaseSeconds, [this, BoltActorId, ActorPose]() { if (FUnitDisplay* Actor = Displays.Find(BoltActorId)) { SetPose(*Actor, ActorPose); } });
                Add(0.3f, [this, BoltFrom, BoltTo]()
                {
                    Cue(EExpeditionCue::Spell);
                    Shots.Add(RangedBolt(BoltFrom, BoltTo, EAdventurerRole::Artificer, false));
                });
            }
            Add(0.45f, [this, Effect, Result, ActorPose]()
            {
                auto* Unit = Displays.Find(Effect.TargetId); if (!Unit) { return; }
                if (ActorPose != EUnitPose::Idle) { if (FUnitDisplay* Actor = Displays.Find(Result.ActorId)) { SetPose(*Actor, ActorPose); } }
                Focus = Result.ActorId;
                Dice = FDiceTray();
                if (Effect.Effect == EAbilityEffect::Rage)
                {
                    Unit->Glow = 1.0f;
                    ShowText(CellOf(Effect.Cell), FString::Printf(TEXT("Rage: %d own turn ends"), Effect.Amount), PlaybackGold);
                    Cue(EExpeditionCue::PartyTurn);
                }
                else if (Effect.Effect == EAbilityEffect::Protect)
                {
                    Unit->Glow = 1.0f;
                    ShowText(CellOf(Effect.Cell), FString::Printf(TEXT("Shelter: -%d incoming"), Effect.Amount), PlaybackGold);
                    Cue(EExpeditionCue::Spell);
                }
                else if (Effect.Effect == EAbilityEffect::Mitigation)
                { ShowText(CellOf(Effect.Cell),FString::Printf(TEXT("Prevented %d"),Effect.Amount),PlaybackGold); }
                else if (Effect.Effect == EAbilityEffect::Swap || Effect.Effect == EAbilityEffect::Push)
                { Unit->Position = CellOf(Effect.Cell); ShowText(CellOf(Effect.Cell), TEXT("Repositioned"), PlaybackGold); Cue(EExpeditionCue::Step); }
                else if (Effect.Effect == EAbilityEffect::Healing)
                { Unit->HP += Effect.Amount; ShowText(CellOf(Effect.Cell), FString::Printf(TEXT("+%d"), Effect.Amount), Mint); Cue(EExpeditionCue::Spell); }
                else
                {
                    Unit->HP = FMath::Max(0, Unit->HP - Effect.Amount); Unit->Flash = 1.0f;
                    if (Unit->HP > 0 && Unit->Life == ECombatLife::Conscious) { SetPose(*Unit, ExpeditionPose::ForDamage()); }
                    ShowText(CellOf(Effect.Cell), FString::Printf(TEXT("-%d"), Effect.Amount), Blood);
                    if (Effect.Effect == EAbilityEffect::Surge) { ShowText(CellOf(Effect.Cell), FString::Printf(TEXT("Wild Bolt outcome %d"), Result.SurgeOutcome), PlaybackGold, 0.75f); }
                    Cue(EExpeditionCue::Hit);
                }
            });
        }
        AddFalls(Result, After);
        return;
    }
    switch (Result.Type)
    {
    case ECombatCommandType::Move: AddMove(Result); break;
    case ECombatCommandType::PrecisionStrike:
    case ECombatCommandType::Attack:
    case ECombatCommandType::MagicMissile: AddAttack(Result, After); break;
    case ECombatCommandType::SecondWind:
    case ECombatCommandType::CureWounds:
    case ECombatCommandType::Potion:
    case ECombatCommandType::Rescue: AddHealing(Result, After); break;
    case ECombatCommandType::EndTurn: AddFalls(Result, After); break;
    default: break;
    }
}

void FExpeditionPlayback::AddMove(const FCombatResult& Result)
{
    const int32 Id = Result.ActorId;
    for (const FIntPoint Cell : Result.Path)
    {
        TSharedRef<FVector2D> From = MakeShared<FVector2D>();
        Add(0.15f, [this, Id, Cell, From]()
        {
            FUnitDisplay* Unit = Displays.Find(Id);
            if (!Unit) { return; }
            Focus = Id;
            *From = Unit->Position;
            Unit->Facing = SafeDirection(*From, CellOf(Cell));
            SetPose(*Unit, ExpeditionPose::ForCommand(ECombatCommandType::Move, false));
            Cue(EExpeditionCue::Step);
        }, [this, Id, Cell, From](float Alpha)
        {
            if (FUnitDisplay* Unit = Displays.Find(Id))
            {
                Unit->Position = FMath::Lerp(*From, CellOf(Cell), EaseInOut(Alpha));
                Unit->Hop = Bump(Alpha) * 16.0f;
            }
        });
    }
}

void FExpeditionPlayback::AddAttack(const FCombatResult& Result, const FCombatSnapshot& After)
{
    const FCombatantState* Actor = ExpeditionStyle::FindUnit(After, Result.ActorId);
    const FCombatantState* Target = ExpeditionStyle::FindUnit(After, Result.TargetId);
    if (!Actor || !Target) { return; }
    const bool bMissile = Result.Type == ECombatCommandType::MagicMissile;
    const int32 ActorId = Actor->Id, TargetId = Target->Id;
    const FVector2D From = CellOf(Actor->Cell), To = CellOf(Target->Cell);
    const FVector2D Direction = SafeDirection(From, To);
    const bool bRanged = FMath::Abs(Actor->Cell.X - Target->Cell.X) + FMath::Abs(Actor->Cell.Y - Target->Cell.Y) > 1;
    const FString ActorName = Actor->Name, TargetName = Target->Name;
    const EAdventurerRole Role = Actor->AdventurerRole;
    const EUnitPose ActorPose = ExpeditionPose::ForCommand(Result.Type, bRanged, ExpeditionPose::IsSpellAttack(Result.Type, Role, bRanged));
    // Every ranged attack that is not a spell is an arrow (the Ranger's bow included): it carries no light and nothing flashes the shooter.
    // A spell bolt throws a point light from the shooter's cell, which blew the whole ranger and the floor out at release (#198); it now leaves at the
    // Cast clip's release frame and its light ramps in over the first stretch of flight (ExpeditionFlash::BoltRampEnd, #150 batch 3).
    const bool bArrow = !ExpeditionPose::IsSpellAttack(Result.Type, Role, bRanged);

    // Wind up while the d20 (or spell dice) tumble.
    Add(bMissile ? 0.3f : FDiceTray::TumbleSeconds + 0.05f, [=, this]()
    {
        Focus = ActorId;
        if (FUnitDisplay* Unit = Displays.Find(ActorId)) { Unit->Facing = Direction; if (bMissile) { Unit->Glow = 1.0f; } }
        Dice = FDiceTray();
        Dice.bVisible = true;
        Dice.Title = bMissile ? FString::Printf(TEXT("%s casts Magic Missile at %s"), *ActorName, *TargetName)
            : FString::Printf(TEXT("%s attacks %s"), *ActorName, *TargetName);
        if (bMissile)
        {
            for (int32 Index = 0; Index < Result.EffectDice.Num(); ++Index)
            { Dice.Dice.Add({Result.EffectDice[Index].Sides, Result.EffectDice[Index].Value, 0.06f * Index, FLinearColor(0.75f, 0.6f, 1.0f)}); }
            Dice.Detail = FString::Printf(TEXT("%s = %d damage  ·  never misses"), *DiceText(Result.EffectDice, Result.EffectModifier), Result.Damage);
            Cue(EExpeditionCue::Spell);
        }
        else
        {
            Dice.Dice.Add({20, Result.NaturalRoll, 0.0f, FLinearColor(0.95f, 0.92f, 0.84f)});
            Dice.Detail = FString::Printf(TEXT("Rolling d20 %+d against AC %d"), Result.AttackBonus, Result.TargetArmorClass);
        }
        Cue(EExpeditionCue::DiceRoll);
    }, [=, this](float Alpha)
    {
        if (FUnitDisplay* Unit = Displays.Find(ActorId)) { Unit->Lean = -Direction * 0.12f * EaseInOut(Alpha); }
    });

    if (!bMissile)
    {
        Add(0.32f, [=, this]()
        {
            Cue(EExpeditionCue::DiceLand);
            Dice.Detail = FString::Printf(TEXT("d20 %d %s %d = %d  vs  AC %d"), Result.NaturalRoll, Result.AttackBonus >= 0 ? TEXT("+") : TEXT("-"),
                FMath::Abs(Result.AttackBonus), Result.AttackTotal, Result.TargetArmorClass);
            if (Result.bCritical) { Dice.Verdict = TEXT("CRITICAL HIT!"); Dice.VerdictColor = PlaybackGold; Cue(EExpeditionCue::Critical); Shake(0.5f); }
            else if (Result.NaturalRoll == 1) { Dice.Verdict = TEXT("NATURAL 1 — MISS"); Dice.VerdictColor = Blood; }
            else if (Result.bHit) { Dice.Verdict = TEXT("HIT"); Dice.VerdictColor = FLinearColor(1.0f, 0.55f, 0.35f); }
            else { Dice.Verdict = TEXT("MISS"); Dice.VerdictColor = Ash; }
        });
    }

    // Deliver the blow: a lunge in melee, a projectile at range.
    if (bMissile)
    {
        // The cast plays out before the bolts leave: the clip's release frame is the thrust, with the focus pointing at the target.
        Add(CastReleaseSeconds, [=, this]()
        {
            if (FUnitDisplay* Unit = Displays.Find(ActorId)) { SetPose(*Unit, ActorPose); }
        });
        Add(0.5f, [=, this]()
        {
            Cue(EExpeditionCue::Spell);
            Shots.Append(MagicMissileBolts(From, To));
        });
    }
    else if (bRanged)
    {
        // A bow draw plays out before the arrow leaves: the clip's release frame is when the string lets go (the Ranged clips key it at frame 26).
        const bool bDraw = ActorPose == EUnitPose::Ranged;
        const bool bCast = ActorPose == EUnitPose::Cast;
        if (bDraw || bCast)
        {
            Add(bCast ? CastReleaseSeconds : RangedReleaseSeconds, [=, this]()
            {
                if (FUnitDisplay* Unit = Displays.Find(ActorId)) { SetPose(*Unit, ActorPose); }
            });
        }
        Add(0.3f, [=, this]()
        {
            if (!bDraw && !bCast) { if (FUnitDisplay* Unit = Displays.Find(ActorId)) { SetPose(*Unit, ActorPose); } }
            Cue(bArrow ? EExpeditionCue::Bow : EExpeditionCue::Spell);
            Shots.Add(RangedBolt(From, To, Role, bArrow));
        });
    }
    else
    {
        Add(0.16f, [=, this]()
        {
            if (FUnitDisplay* Unit = Displays.Find(ActorId)) { SetPose(*Unit, ActorPose); }
            Cue(EExpeditionCue::Swing);
        }, [=, this](float Alpha)
        {
            if (FUnitDisplay* Unit = Displays.Find(ActorId)) { Unit->Lean = Direction * FMath::Lerp(-0.12f, 0.42f, EaseInOut(Alpha)); }
        });
    }
    AddImpact(Result, Direction);
    Add(0.14f, nullptr, [=, this](float Alpha)
    {
        if (FUnitDisplay* Unit = Displays.Find(ActorId)) { Unit->Lean = Unit->Lean * (1.0f - Alpha); }
    });
    AddFalls(Result, After);
}

void FExpeditionPlayback::AddImpact(const FCombatResult& Result, FVector2D Direction)
{
    const int32 TargetId = Result.TargetId;
    const bool bMissile = Result.Type == ECombatCommandType::MagicMissile;
    Add(0.38f, [=, this]()
    {
        FUnitDisplay* Target = Displays.Find(TargetId);
        if (!Target) { return; }
        if (!Result.bHit)
        {
            ShowText(Target->Position, TEXT("MISS"), Ash, 0.9f);
            Cue(EExpeditionCue::Miss);
            return;
        }
        Target->HP = FMath::Max(0, Target->HP - Result.Damage);
        Target->Flash = 1.0f;
        // A unit that stays up flinches; one that falls goes straight to its fall clip in AddFalls.
        if (Target->HP > 0 && Target->Life == ECombatLife::Conscious) { SetPose(*Target, ExpeditionPose::ForDamage()); }
        ShowText(Target->Position, FString::Printf(TEXT("-%d%s"), Result.Damage, Result.bCritical ? TEXT("!") : TEXT("")),
            Result.bCritical ? PlaybackGold : Blood, Result.bCritical ? 1.6f : 1.25f);
        if (Result.bSneakAttack) { ShowText(Target->Position, TEXT("SNEAK ATTACK"), FLinearColor(0.4f, 1.0f, 0.6f), 0.75f); }
        Cue(Result.bCritical ? EExpeditionCue::Critical : EExpeditionCue::Hit);
        Shake(Result.bCritical ? 0.45f : 0.2f);
        if (!bMissile)
        {
            for (int32 Index = 0; Index < Result.EffectDice.Num(); ++Index)
            {
                Dice.Dice.Add({Result.EffectDice[Index].Sides, Result.EffectDice[Index].Value, Dice.Age + 0.05f * Index, FLinearColor(1.0f, 0.55f, 0.45f)});
            }
            const FString Protection = Result.DamagePrevented > 0 ? FString::Printf(TEXT(" - %d protection"), Result.DamagePrevented) : FString();
            Dice.Detail += FString::Printf(TEXT("   ·   damage %s%s = %d"), *DiceText(Result.EffectDice, Result.EffectModifier), *Protection, Result.Damage);
        }
    }, [=, this](float Alpha)
    {
        if (FUnitDisplay* Target = Displays.Find(TargetId))
        {
            const FVector2D Side(-Direction.Y, Direction.X);
            Target->Lean = Result.bHit ? Direction * 0.16f * Bump(Alpha) : Side * 0.28f * Bump(Alpha);
        }
    });
}

void FExpeditionPlayback::AddFalls(const FCombatResult& Result, const FCombatSnapshot& After)
{
    TArray<int32> Fallen = Result.Downed;
    for (int32 Id : Result.Died) { Fallen.AddUnique(Id); }
    for (int32 Id : Fallen)
    {
        const FCombatantState* Unit = ExpeditionStyle::FindUnit(After, Id);
        if (!Unit) { continue; }
        const bool bDead = Result.Died.Contains(Id);
        const bool bEnemy = Unit->Side == ECombatSide::Enemy;
        const FString Name = Unit->Name;
        TSharedRef<float> StartTilt = MakeShared<float>(0.0f);
        Add(0.5f, [=, this]()
        {
            FUnitDisplay* Display = Displays.Find(Id);
            if (!Display) { return; }
            *StartTilt = Display->Tilt;
            Display->Life = bDead ? ECombatLife::Dead : ECombatLife::Downed;
            Display->HP = 0;
            SetPose(*Display, ExpeditionPose::ForFall(bDead));
            Focus = Id;
            if (bEnemy) { ShowText(Display->Position, TEXT("SLAIN"), Ash, 1.0f, 1.6f); }
            else if (bDead) { ShowText(Display->Position, Name.ToUpper() + TEXT(" HAS DIED"), Blood, 1.15f, 2.2f); }
            else { ShowText(Display->Position, TEXT("DOWNED"), FLinearColor(1.0f, 0.6f, 0.3f), 1.0f, 1.8f); }
            Cue(bDead ? EExpeditionCue::Death : EExpeditionCue::Downed);
        }, [=, this](float Alpha)
        {
            if (FUnitDisplay* Display = Displays.Find(Id))
            {
                Display->Tilt = FMath::Lerp(*StartTilt, 1.0f, EaseInOut(Alpha));
            }
        });
    }
}

void FExpeditionPlayback::AddHealing(const FCombatResult& Result, const FCombatSnapshot& After)
{
    const FCombatantState* Actor = ExpeditionStyle::FindUnit(After, Result.ActorId);
    const FCombatantState* Target = ExpeditionStyle::FindUnit(After, Result.TargetId);
    if (!Actor || !Target) { return; }
    const int32 ActorId = Actor->Id, TargetId = Target->Id;
    const FString Ability = ExpeditionStyle::ActionName(Result.Type);
    const FString Title = ActorId == TargetId ? FString::Printf(TEXT("%s uses %s"), *Actor->Name, *Ability)
        : FString::Printf(TEXT("%s uses %s on %s"), *Actor->Name, *Ability, *Target->Name);
    const FVector2D Direction = SafeDirection(CellOf(Actor->Cell), CellOf(Target->Cell));
    const FLinearColor Color = Result.Type == ECombatCommandType::SecondWind ? PlaybackGold : Mint;
    Add(Result.EffectDice.IsEmpty() ? 0.3f : FDiceTray::TumbleSeconds, [=, this]()
    {
        Focus = ActorId;
        if (FUnitDisplay* Unit = Displays.Find(ActorId))
        {
            Unit->Glow = 1.0f;
            if (ActorId != TargetId) { Unit->Facing = Direction; }
            if (const EUnitPose Pose = ExpeditionPose::ForCommand(Result.Type, false); Pose != EUnitPose::Idle) { SetPose(*Unit, Pose); }
        }
        Dice = FDiceTray();
        Dice.bVisible = true;
        Dice.Title = Title;
        for (int32 Index = 0; Index < Result.EffectDice.Num(); ++Index)
        { Dice.Dice.Add({Result.EffectDice[Index].Sides, Result.EffectDice[Index].Value, 0.06f * Index, FLinearColor(0.55f, 1.0f, 0.65f)}); }
        const int32 Raw = DiceSum(Result.EffectDice, Result.EffectModifier);
        Dice.Detail = Result.EffectDice.IsEmpty() ? FString::Printf(TEXT("Restores %d HP"), Result.Healing)
            : Raw == Result.Healing ? FString::Printf(TEXT("%s = %d healing"), *DiceText(Result.EffectDice, Result.EffectModifier), Raw)
            : FString::Printf(TEXT("%s = %d  ·  +%d HP (full health)"), *DiceText(Result.EffectDice, Result.EffectModifier), Raw, Result.Healing);
        Cue(Result.EffectDice.IsEmpty() ? EExpeditionCue::Click : EExpeditionCue::DiceRoll);
    }, [=, this](float Alpha)
    {
        if (FUnitDisplay* Unit = Displays.Find(ActorId))
        {
            Unit->Hop = Bump(Alpha) * 10.0f;
            if (ActorId != TargetId) { Unit->Lean = Direction * 0.2f * Bump(Alpha); }
        }
    });
    TSharedRef<float> StartTilt = MakeShared<float>(0.0f);
    Add(0.55f, [=, this]()
    {
        FUnitDisplay* Unit = Displays.Find(TargetId);
        if (!Unit) { return; }
        Focus = TargetId;
        const bool bWasDown = Unit->Life == ECombatLife::Downed;
        *StartTilt = Unit->Tilt;
        Unit->HP = FMath::Min(Unit->MaxHP, Unit->HP + Result.Healing);
        Unit->Life = ECombatLife::Conscious;
        Unit->Glow = 1.0f;
        if (bWasDown) { SetPose(*Unit, ExpeditionPose::ForRecovery()); }
        ShowSparkles(Unit->Position, Color);
        ShowText(Unit->Position, FString::Printf(TEXT("+%d"), Result.Healing), Color, 1.25f);
        if (bWasDown) { ShowText(Unit->Position, TEXT("BACK ON THEIR FEET"), Mint, 0.8f, 1.8f); }
        Cue(EExpeditionCue::Heal);
    }, [=, this](float Alpha)
    {
        if (FUnitDisplay* Unit = Displays.Find(TargetId)) { Unit->Tilt = FMath::Lerp(*StartTilt, 0.0f, EaseInOut(Alpha)); }
    });
    AddFalls(Result, After);
}

void FExpeditionPlayback::Tick(float DeltaSeconds)
{
    const float Seconds = FMath::Max(0.0f, DeltaSeconds) * Speed;
    Advance(Seconds);
    StepRunner(Seconds);
}

void FExpeditionPlayback::Advance(float Seconds)
{
    for (FFloatingText& Text : Floaters) { Text.Age += Seconds; }
    Floaters.RemoveAll([](const FFloatingText& Text) { return Text.Age > Text.Life; });
    for (FPlaybackProjectile& Shot : Shots) { Shot.Age += Seconds; }
    Shots.RemoveAll([](const FPlaybackProjectile& Shot) { return Shot.Age > Shot.Duration; });
    for (FPlaybackSparkle& Sparkle : Glitter) { Sparkle.Age += Seconds; }
    Glitter.RemoveAll([](const FPlaybackSparkle& Sparkle) { return Sparkle.Age > Sparkle.Life; });
    if (Dice.bVisible && !Dice.bHeld)
    {
        Dice.Age += Seconds;
        if (Dice.Age > FDiceTray::LingerSeconds + FDiceTray::TumbleSeconds && !IsBusy()) { Dice.bVisible = false; }
    }
    TurnBanner.Age += Seconds;
    for (auto& Pair : Displays)
    {
        FUnitDisplay& Unit = Pair.Value;
        Unit.Flash = FMath::Max(0.0f, Unit.Flash - Seconds * 3.5f);
        Unit.Glow = FMath::Max(0.0f, Unit.Glow - Seconds * 1.5f);
        Unit.ShownHP = FMath::FInterpTo(Unit.ShownHP, Unit.HP, Seconds, 10.0f);
        Unit.TrailHP = Unit.TrailHP > Unit.ShownHP ? FMath::FInterpTo(Unit.TrailHP, Unit.ShownHP, Seconds, 2.5f) : Unit.ShownHP;
    }
}

void FExpeditionPlayback::StepRunner(float Seconds)
{
    float Remaining = Seconds;
    while (Current < Steps.Num())
    {
        FStep& Step = Steps[Current];
        if (!bStepBegun)
        {
            bStepBegun = true;
            StepTime = 0.0f;
            if (Step.Begin) { Step.Begin(); }
        }
        const float Needed = Step.Duration - StepTime;
        if (Remaining < Needed)
        {
            StepTime += Remaining;
            if (Step.Update) { Step.Update(FMath::Clamp(StepTime / Step.Duration, 0.0f, 1.0f)); }
            return;
        }
        Remaining -= FMath::Max(0.0f, Needed);
        if (Steps[Current].Update) { Steps[Current].Update(1.0f); }
        ++Current;
        bStepBegun = false;
    }
    Steps.Reset();
    Current = 0;
}

void FExpeditionPlayback::Finish()
{
    // Poses converge even when no step is queued, so a clip still playing from an earlier result ends on the committed pose.
    if (IsBusy())
    {
        bSkipping = true;
        StepRunner(TNumericLimits<float>::Max());
        bSkipping = false;
        Shots.Reset();
        Glitter.Reset();
        for (auto& Pair : Displays) { Pair.Value.ShownHP = Pair.Value.TrailHP = Pair.Value.HP; }
        Dice.Age = FMath::Max(Dice.Age, FDiceTray::TumbleSeconds + 0.5f);
    }
    for (auto& Pair : Displays) { Converge(Pair.Value, true); }
}
