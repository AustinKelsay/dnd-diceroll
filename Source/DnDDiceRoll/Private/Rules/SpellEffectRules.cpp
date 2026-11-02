#include "Rules/SpellEffectRules.h"

// Named (not anonymous) so unity builds never merge its short helpers (C, T, D, Foe) with other files.
namespace SpellEffectTable
{
using namespace SpellEffectFlag;
using namespace SpellEffectExtra;
constexpr uint32 C(ESpellCondition Condition) { return 1u << static_cast<uint8>(Condition); }
constexpr uint16 T(ECreatureType Type) { return static_cast<uint16>(1u << static_cast<uint8>(Type)); }
constexpr uint16 D(EDamageType Type) { return static_cast<uint16>(1u << static_cast<uint8>(Type)); }
constexpr ESpellEffectTarget Foe = ESpellEffectTarget::Foe, Ally = ESpellEffectTarget::Ally, Self = ESpellEffectTarget::Self;

// SRD 5.2.1 spell text, one row per spell; fields left out keep their defaults. Spells not here resolve from the catalog row alone.
const FSpellEffectSpec Specs[] = {
    // ---- Phase 3: conditions ----
    { .Id = TEXT("charm-person"), .Flags = EndsOnCasterSideDamage | AdvantageInCombat, .OnlyTypes = T(ECreatureType::Humanoid), .Targets = 1, .TargetsPerSlot = 1 },
    { .Id = TEXT("charm-monster"), .Flags = EndsOnCasterSideDamage | AdvantageInCombat, .Targets = 1, .TargetsPerSlot = 1 },
    { .Id = TEXT("animal-friendship"), .Flags = EndsOnCasterSideDamage, .OnlyTypes = T(ECreatureType::Beast), .Targets = 1, .TargetsPerSlot = 1 },
    // Dominate: it fights the caster's foes and never the caster's side; each time it takes damage it repeats the save.
    { .Id = TEXT("dominate-beast"), .Flags = AdvantageInCombat | SaveOnDamage, .OnlyTypes = T(ECreatureType::Beast), .Extra = Dominated },
    { .Id = TEXT("dominate-person"), .Flags = AdvantageInCombat | SaveOnDamage, .OnlyTypes = T(ECreatureType::Humanoid), .Extra = Dominated },
    // Suggestion: the tactical course is "stop fighting": it takes no actions until you or your allies damage it.
    { .Id = TEXT("suggestion"), .Flags = EndsOnCasterSideDamage, .Extra = Pacified },
    // Command: Choice 0 Grovel (Prone + loses its next turn), 1 Halt (loses its next turn), 2 Approach/Flee/Drop are not modelled; 1 round.
    { .Id = TEXT("command"), .Conditions = C(ESpellCondition::Prone) | C(ESpellCondition::Incapacitated), .Targets = 1, .TargetsPerSlot = 1, .ConditionRounds = 1 },
    { .Id = TEXT("color-spray"), .Conditions = C(ESpellCondition::Blinded), .ConditionRounds = 1 },
    { .Id = TEXT("sleep"), .Flags = EndsOnAnyDamage | SleepEscalates | SparesAllies, .Conditions = C(ESpellCondition::Incapacitated) },
    { .Id = TEXT("hold-person"), .OnlyTypes = T(ECreatureType::Humanoid), .Targets = 1, .TargetsPerSlot = 1 },
    { .Id = TEXT("hold-monster"), .Targets = 1, .TargetsPerSlot = 1 },
    { .Id = TEXT("hypnotic-pattern"), .Flags = EndsOnAnyDamage | SpeedZero },
    { .Id = TEXT("hideous-laughter"), .Flags = CantStand | SaveOnDamage, .Conditions = C(ESpellCondition::Prone) | C(ESpellCondition::Incapacitated) },
    // Blindness/Deafness: Choice 1 deafens instead.
    { .Id = TEXT("blindness-deafness"), .Conditions = C(ESpellCondition::Blinded), .Targets = 1, .TargetsPerSlot = 1 },
    // Escaping (an action and a STR check) is approximated as the repeat save at the end of its turns.
    { .Id = TEXT("entangle"), .Flags = RepeatSave },
    { .Id = TEXT("web"), .Flags = RepeatSave | NoCastDamage },
    { .Id = TEXT("stinking-cloud"), .Flags = RepeatSave },
    // ---- Phase 4: buffs, debuffs and riders ----
    { .Id = TEXT("bless"), .Target = Ally, .Flags = Buff, .Targets = 3, .TargetsPerSlot = 1, .RollDie = 4 },
    { .Id = TEXT("bane"), .Flags = Buff, .Targets = 3, .TargetsPerSlot = 1, .RollDie = -4 },
    { .Id = TEXT("shield-of-faith"), .Target = Ally, .Flags = Buff, .AcBonus = 2 },
    { .Id = TEXT("mage-armor"), .Target = Ally, .Flags = Buff, .AcBase = 13 },
    { .Id = TEXT("heroism"), .Target = Ally, .Flags = Buff | TempHpPerTurn, .Targets = 1, .TargetsPerSlot = 1, .ConditionImmunity = C(ESpellCondition::Frightened) },
    { .Id = TEXT("longstrider"), .Target = Ally, .Flags = Buff, .Targets = 1, .TargetsPerSlot = 1, .SpeedFeet = 10 },
    { .Id = TEXT("expeditious-retreat"), .Target = Self, .Flags = Buff, .SpeedPercent = 200 },
    { .Id = TEXT("divine-favor"), .Target = Self, .Flags = Buff | OwnWeaponHit | NoCastDamage, .HitDice = 1, .HitSides = 4, .HitType = EDamageType::Radiant },
    { .Id = TEXT("hex"), .Flags = Buff | MarkOnHit | NoCastDamage, .HitDice = 1, .HitSides = 6, .HitType = EDamageType::Necrotic, .Extra = CheckDisadvantage },
    { .Id = TEXT("hunters-mark"), .Flags = Buff | MarkOnHit | NoCastDamage, .HitDice = 1, .HitSides = 6, .HitType = EDamageType::Force },
    { .Id = TEXT("protection-from-evil-and-good"), .Target = Ally, .Flags = Buff | WardEvilGood },
    { .Id = TEXT("sanctuary"), .Target = Ally, .Flags = Buff | Sanctuary | EndsOnHostileAct },
    { .Id = TEXT("faerie-fire"), .Flags = Buff | AttackersAdvantage },
    { .Id = TEXT("false-life"), .Target = Self, .Flags = Buff, .TempDice = 2, .TempSides = 4, .TempFlat = 4, .TempPerSlot = 5 },
    { .Id = TEXT("aid"), .Target = Ally, .Flags = Buff, .Targets = 3, .MaxHpPerSlot = 5 },
    { .Id = TEXT("barkskin"), .Target = Ally, .Flags = Buff, .AcFloor = 17 },
    { .Id = TEXT("blur"), .Target = Self, .Flags = Buff | AttackersDisadvantage },
    { .Id = TEXT("mirror-image"), .Target = Self, .Flags = Buff | MirrorImages, .Conditions = 0 },
    { .Id = TEXT("invisibility"), .Target = Ally, .Flags = Buff | Invisible | EndsOnHostileAct, .Targets = 1, .TargetsPerSlot = 1 },
    { .Id = TEXT("greater-invisibility"), .Target = Ally, .Flags = Buff | Invisible },
    // Enlarge (Choice 0): +1d4 weapon damage, advantage on STR checks and saves. Reduce is not modelled.
    // Choice 1 (or any foe, which makes a CON save) Reduce: -1d4 weapon damage, disadvantage on STR checks and saves.
    { .Id = TEXT("enlarge-reduce"), .Target = Ally, .Flags = Buff | OwnWeaponHit | StrSaveAdvantage, .bWeaponTyped = true, .HitDice = 1, .HitSides = 4, .Extra = BothSides },
    { .Id = TEXT("magic-weapon"), .Target = Ally, .Flags = Buff, .WeaponBonus = 1 },
    { .Id = TEXT("warding-bond"), .Target = Ally, .Flags = Buff, .AcBonus = 1, .SaveBonus = 1,
      .Resist = static_cast<uint16>((1u << static_cast<uint8>(EDamageType::Thunder) << 1) - 1), .Extra = ShareDamage },
    { .Id = TEXT("protection-from-poison"), .Target = Ally, .Flags = Buff, .Resist = D(EDamageType::Poison) },
    { .Id = TEXT("haste"), .Target = Ally, .Flags = Buff | DexSaveAdvantage | Lethargy, .AcBonus = 2, .SpeedPercent = 200 },
    { .Id = TEXT("slow"), .Flags = Buff | NoReactions | SparesAllies, .Targets = 6, .AcBonus = -2, .SpeedPercent = 50, .DexSaveBonus = -2 },
    { .Id = TEXT("protection-from-energy"), .Target = Ally, .Flags = Buff | ResistChoice },
    { .Id = TEXT("stoneskin"), .Target = Ally, .Flags = Buff, .Resist = static_cast<uint16>(D(EDamageType::Bludgeoning) | D(EDamageType::Piercing) | D(EDamageType::Slashing)) },
    { .Id = TEXT("fire-shield"), .Target = Self, .Flags = Buff | FireShield | NoCastDamage, .HitDice = 2, .HitSides = 8, .HitType = EDamageType::Fire },
    { .Id = TEXT("death-ward"), .Target = Ally, .Flags = Buff | DeathWard },
    { .Id = TEXT("freedom-of-movement"), .Target = Ally, .Flags = Buff, .ConditionImmunity = C(ESpellCondition::Paralyzed) | C(ESpellCondition::Restrained) },
    // Smites: bonus action right after a melee weapon hit this turn; the catalog dice are the smite damage.
    { .Id = TEXT("divine-smite"), .Flags = Smite, .BonusVsTypes = static_cast<uint16>(T(ECreatureType::Fiend) | T(ECreatureType::Undead)), .HitDice = 1, .HitSides = 8, .HitType = EDamageType::Radiant },
    { .Id = TEXT("searing-smite"), .Flags = Smite | StartTurnDamage, .HitDice = 1, .HitSides = 6, .HitType = EDamageType::Fire, .HitDicePerSlot = 1 },
    { .Id = TEXT("shining-smite"), .Flags = Smite | AttackersAdvantage },
    // Ensnaring Strike: STR save or Restrained; 1d6 piercing (+1d6 per slot) at the start of each of its turns, then the save again (escape check approximated).
    { .Id = TEXT("ensnaring-strike"), .Flags = Smite | StartTurnDamage, .Conditions = C(ESpellCondition::Restrained), .HitDice = 1, .HitSides = 6, .HitType = EDamageType::Piercing, .HitDicePerSlot = 1, .Extra = SmiteSave },
    // ---- Phase 6: utility spells as tactical equivalents ----
    { .Id = TEXT("guidance"), .Target = Ally, .Flags = Buff, .Extra = CheckDie },
    { .Id = TEXT("resistance"), .Target = Ally, .Flags = Buff, .Extra = ReduceChosen },
    { .Id = TEXT("calm-emotions"), .Flags = EndsOnAnyDamage | SparesAllies, .Conditions = C(ESpellCondition::Incapacitated), .OnlyTypes = T(ECreatureType::Humanoid) },
    { .Id = TEXT("darkvision"), .Target = Ally, .Flags = Buff, .Extra = Darkvision },
    { .Id = TEXT("enhance-ability"), .Target = Ally, .Flags = Buff, .Targets = 1, .TargetsPerSlot = 1, .Extra = CheckAdvantage },
    { .Id = TEXT("levitate"), .Target = Ally, .Flags = Buff | SpeedZero, .Extra = Aloft | BothSides },
    { .Id = TEXT("ray-of-enfeeblement"), .Flags = Buff, .Extra = Enfeebled },
    { .Id = TEXT("blink"), .Target = Self, .Flags = Buff, .Extra = Blink },
    { .Id = TEXT("see-invisibility"), .Target = Self, .Flags = Buff, .Extra = SeeInvisible },
    { .Id = TEXT("beacon-of-hope"), .Target = Ally, .Flags = Buff, .Targets = 12, .Extra = MaxHealing | WisDeathAdvantage | AllInRange },
    { .Id = TEXT("fly"), .Target = Ally, .Flags = Buff, .Targets = 1, .TargetsPerSlot = 1, .Extra = FlySpeed },
    { .Id = TEXT("confusion"), .Flags = Buff, .Extra = Confused },
    { .Id = TEXT("resilient-sphere"), .Target = Ally, .Flags = Buff | SpeedZero, .Extra = Sphere | BothSides },
};

struct FCreatureRow { const TCHAR* Role; ECreatureType Type; uint32 Immune; bool bNoExhaustion; };
// SRD 5.2.1 stat blocks (Condition Immunities line); roles not listed are Humanoid with none.
const FCreatureRow Creatures[] = {
    { TEXT("Skeleton"), ECreatureType::Undead, C(ESpellCondition::Poisoned), true },
    { TEXT("Zombie"), ECreatureType::Undead, C(ESpellCondition::Poisoned), true },
    { TEXT("Ghoul"), ECreatureType::Undead, C(ESpellCondition::Charmed) | C(ESpellCondition::Poisoned), true },
    { TEXT("Ice Mephit"), ECreatureType::Elemental, C(ESpellCondition::Poisoned), true },
    { TEXT("Fire Elemental"), ECreatureType::Elemental, C(ESpellCondition::Grappled) | C(ESpellCondition::Paralyzed) | C(ESpellCondition::Petrified) |
        C(ESpellCondition::Poisoned) | C(ESpellCondition::Prone) | C(ESpellCondition::Restrained) | C(ESpellCondition::Unconscious), true },
    { TEXT("Wolf"), ECreatureType::Beast, 0, false },
    { TEXT("Dire Wolf"), ECreatureType::Beast, 0, false },
    { TEXT("Giant Spider"), ECreatureType::Beast, 0, false },
    { TEXT("Goblin"), ECreatureType::Fey, 0, false },
    { TEXT("Imp"), ECreatureType::Fiend, C(ESpellCondition::Poisoned), false },
    { TEXT("Ogre"), ECreatureType::Giant, 0, false },
    { TEXT("Owlbear"), ECreatureType::Monstrosity, 0, false },
    { TEXT("Animated Armor"), ECreatureType::Construct, C(ESpellCondition::Blinded) | C(ESpellCondition::Charmed) | C(ESpellCondition::Deafened) |
        C(ESpellCondition::Frightened) | C(ESpellCondition::Paralyzed) | C(ESpellCondition::Petrified) | C(ESpellCondition::Poisoned), true },
};
const FCreatureRow* Row(const FString& Role)
{
    for (const FCreatureRow& R : Creatures) { if (Role == R.Role) { return &R; } }
    return nullptr;
}
}

ECreatureType FCreatureRules::TypeOf(const FString& Role) { const SpellEffectTable::FCreatureRow* R = SpellEffectTable::Row(Role); return R ? R->Type : ECreatureType::Humanoid; }
uint32 FCreatureRules::ConditionImmunities(const FString& Role) { const SpellEffectTable::FCreatureRow* R = SpellEffectTable::Row(Role); return R ? R->Immune : 0; }
bool FCreatureRules::ImmuneToExhaustion(const FString& Role) { const SpellEffectTable::FCreatureRow* R = SpellEffectTable::Row(Role); return R && R->bNoExhaustion; }
const TCHAR* FCreatureRules::TypeName(ECreatureType Type)
{
    static const TCHAR* Names[] = { TEXT("Aberration"), TEXT("Beast"), TEXT("Celestial"), TEXT("Construct"), TEXT("Dragon"), TEXT("Elemental"), TEXT("Fey"),
        TEXT("Fiend"), TEXT("Giant"), TEXT("Humanoid"), TEXT("Monstrosity"), TEXT("Ooze"), TEXT("Plant"), TEXT("Undead") };
    return Type < ECreatureType::Count ? Names[static_cast<uint8>(Type)] : TEXT("?");
}

int32 FSpellEffectRules::Num() { return UE_ARRAY_COUNT(SpellEffectTable::Specs); }
const FSpellEffectSpec& FSpellEffectRules::At(int32 Index) { return SpellEffectTable::Specs[Index]; }
const FSpellEffectSpec* FSpellEffectRules::Find(const FString& Id)
{
    for (const FSpellEffectSpec& S : SpellEffectTable::Specs) { if (Id.Equals(S.Id, ESearchCase::IgnoreCase)) { return &S; } }
    return nullptr;
}
bool FSpellEffectRules::IsBuff(const FSpellEffectSpec& Spec) { return (Spec.Flags & SpellEffectFlag::Buff) != 0; }
bool FSpellEffectRules::CanAffect(const FSpellEffectSpec& Spec, const FString& Role)
{
    return Spec.OnlyTypes == 0 || (Spec.OnlyTypes & FCreatureRules::Bit(FCreatureRules::TypeOf(Role))) != 0;
}
int32 FSpellEffectRules::MaxTargets(const FSpellEffectSpec& Spec, int32 SpellLevel, int32 Slot)
{
    return FMath::Max(1, Spec.Targets + Spec.TargetsPerSlot * FMath::Max(0, Slot - SpellLevel));
}
EDamageType FSpellEffectRules::EnergyChoice(int32 Choice)
{
    static const EDamageType Types[] = { EDamageType::Fire, EDamageType::Cold, EDamageType::Lightning, EDamageType::Acid, EDamageType::Thunder };
    return Types[FMath::Clamp(Choice, 0, 4)];
}
