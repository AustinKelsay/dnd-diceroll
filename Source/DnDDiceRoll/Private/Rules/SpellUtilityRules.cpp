#include "Rules/SpellUtilityRules.h"
#include "Rules/SpellCatalogRules.h"

namespace SpellUtility
{
    using enum ESpellUtility;
    static const TCHAR* Exploration = TEXT("exploration/social utility: no combat effect on the tactical grid");
    static const FSpellUtility Table[] =
    {
        // ---- Handled ----
        { TEXT("spike-growth"), Engine, TEXT("20-ft zone, difficult terrain, 2d4 per 5 ft moved") },
        { TEXT("conjure-animals"), Engine, TEXT("pack as a 10-ft zone: DEX save on enter/end turn; the pack's own movement is not modelled") },
        { TEXT("conjure-woodland-beings"), Engine, TEXT("10-ft emanation: WIS save on enter/end turn; the Disengage bonus action is not modelled") },
        { TEXT("shield"), Reaction, TEXT("auto: when hit by an attack it turns into a miss (+5 AC until your next turn)") },
        { TEXT("hellish-rebuke"), Reaction, TEXT("auto: after damage from a creature within 60 ft, DEX save 2d10 fire (+1d10/slot), half on success") },
        { TEXT("counterspell"), Reaction, TEXT("auto (FFeatureKit Counterspell): CON save or the enemy spell fails; enemy casters are stat-block cantrips only") },

        // ---- Exceptions: L0 ----
        { TEXT("druidcraft"), Exception, Exploration },
        { TEXT("elementalism"), Exception, Exploration },
        { TEXT("mending"), Exception, TEXT("1-minute cast; repairs objects (no object state)") },
        { TEXT("message"), Exception, Exploration },
        { TEXT("minor-illusion"), Exception, TEXT("illusions need investigation checks and AI belief state (not modelled)") },
        { TEXT("prestidigitation"), Exception, Exploration },
        { TEXT("thaumaturgy"), Exception, Exploration },
        { TEXT("mage-hand"), Exception, TEXT("object interaction only; the hand can't attack and there are no objects on the grid") },
        // ---- Exceptions: L1 ----
        { TEXT("alarm"), Exception, TEXT("1-minute ritual ward for rest scenes; nothing to trigger in a started battle") },
        { TEXT("comprehend-languages"), Exception, Exploration },
        { TEXT("create-or-destroy-water"), Exception, Exploration },
        { TEXT("detect-evil-and-good"), Exception, TEXT("information only: combatants are always known on the grid") },
        { TEXT("detect-magic"), Exception, TEXT("information only: no hidden magic on the grid") },
        { TEXT("detect-poison-and-disease"), Exception, Exploration },
        { TEXT("disguise-self"), Exception, Exploration },
        { TEXT("feather-fall"), Exception, TEXT("no falling or elevation on the grid") },
        { TEXT("find-familiar"), Exception, TEXT("1-hour ritual: can't be cast in combat; a pre-summoned familiar needs encounter setup (future)") },
        { TEXT("floating-disk"), Exception, TEXT("carries objects; no inventory weight on the grid") },
        { TEXT("goodberry"), Exception, TEXT("creates items eaten later; no consumable inventory in battle") },
        { TEXT("identify"), Exception, Exploration },
        { TEXT("illusory-script"), Exception, Exploration },
        { TEXT("jump"), Exception, TEXT("no jumping or elevation on the grid") },
        { TEXT("purify-food-and-drink"), Exception, Exploration },
        { TEXT("silent-image"), Exception, TEXT("illusions need investigation checks and AI belief state (not modelled)") },
        { TEXT("speak-with-animals"), Exception, Exploration },
        { TEXT("unseen-servant"), Exception, TEXT("object chores only; the servant can't attack and there are no objects on the grid") },
        // ---- Exceptions: L2 ----
        { TEXT("alter-self"), Exception, TEXT("shape options are exploration (aquatic, disguise); natural weapons need a weapon swap (future)") },
        { TEXT("animal-messenger"), Exception, Exploration },
        { TEXT("arcane-lock"), Exception, TEXT("no doors or objects on the grid") },
        { TEXT("arcanists-magic-aura"), Exception, Exploration },
        { TEXT("augury"), Exception, TEXT("1-minute ritual divination") },
        { TEXT("detect-thoughts"), Exception, TEXT("information/social; the probe needs AI state (not modelled)") },
        { TEXT("dragons-breath"), Exception, TEXT("grants the target a Magic action command each turn; party commands are class kits (future command)") },
        { TEXT("enthrall"), Exception, TEXT("Perception penalty only; no passive Perception in battle") },
        { TEXT("find-traps"), Exception, Exploration },
        { TEXT("gentle-repose"), Exception, TEXT("affects corpses out of combat") },
        { TEXT("knock"), Exception, TEXT("no doors or locks on the grid") },
        { TEXT("locate-animals-or-plants"), Exception, Exploration },
        { TEXT("locate-object"), Exception, Exploration },
        { TEXT("magic-mouth"), Exception, TEXT("1-minute ritual") },
        { TEXT("pass-without-trace"), Exception, TEXT("+10 Stealth; Hide has no contested Stealth roll in battle") },
        { TEXT("prayer-of-healing"), Exception, TEXT("10-minute cast: out-of-combat healing") },
        { TEXT("rope-trick"), Exception, TEXT("extradimensional refuge for rests; no elevation on the grid") },
        { TEXT("spider-climb"), Exception, TEXT("no walls or ceilings to climb on the grid") },
        { TEXT("zone-of-truth"), Exception, Exploration },
        // ---- Exceptions: L3 ----
        { TEXT("animate-dead"), Exception, TEXT("1-minute cast; undead servants persist between battles (party roster, future)") },
        { TEXT("blink"), Exception, TEXT("needs an Ethereal (untargetable, unseen) state between turns (not modelled)") },
        { TEXT("clairvoyance"), Exception, TEXT("10-minute cast, remote sensing") },
        { TEXT("create-food-and-water"), Exception, Exploration },
        { TEXT("gaseous-form"), Exception, TEXT("squeezing through cracks and flying need terrain features the grid lacks") },
        { TEXT("glyph-of-warding"), Exception, TEXT("1-hour cast: a prepared trap, not a combat action") },
        { TEXT("magic-circle"), Exception, TEXT("1-minute cast") },
        { TEXT("major-image"), Exception, TEXT("illusions need investigation checks and AI belief state (not modelled)") },
        { TEXT("meld-into-stone"), Exception, TEXT("no stone terrain to meld into") },
        { TEXT("nondetection"), Exception, Exploration },
        { TEXT("phantom-steed"), Exception, TEXT("1-minute cast; travel mount") },
        { TEXT("remove-curse"), Exception, TEXT("no curses in the combat model") },
        { TEXT("sending"), Exception, Exploration },
        { TEXT("speak-with-dead"), Exception, Exploration },
        { TEXT("speak-with-plants"), Exception, Exploration },
        { TEXT("tiny-hut"), Exception, TEXT("1-minute ritual shelter for rests") },
        { TEXT("tongues"), Exception, Exploration },
        { TEXT("water-breathing"), Exception, TEXT("no water on the grid") },
        { TEXT("water-walk"), Exception, TEXT("no water on the grid") },
        { TEXT("wind-wall"), Exception, TEXT("needs a wall shape and projectile deflection on lines (future area shape)") },
        // ---- Exceptions: L4 ----
        { TEXT("arcane-eye"), Exception, TEXT("remote sensing") },
        { TEXT("control-water"), Exception, TEXT("no water on the grid") },
        { TEXT("divination"), Exception, TEXT("ritual divination") },
        { TEXT("fabricate"), Exception, TEXT("10-minute crafting") },
        { TEXT("hallucinatory-terrain"), Exception, TEXT("10-minute cast; illusory terrain needs AI belief state") },
        { TEXT("locate-creature"), Exception, Exploration },
        { TEXT("polymorph"), Exception, TEXT("needs a beast stat-block swap with its own HP (future summons/shapes system)") },
        { TEXT("private-sanctum"), Exception, TEXT("10-minute cast warding an area for rests") },
        { TEXT("secret-chest"), Exception, Exploration },
        { TEXT("stone-shape"), Exception, TEXT("reshapes stone: no destructible or editable terrain") },
    };
}

int32 FSpellUtilityRules::Num() { return UE_ARRAY_COUNT(SpellUtility::Table); }
const FSpellUtility& FSpellUtilityRules::At(int32 Index) { return SpellUtility::Table[FMath::Clamp(Index, 0, Num() - 1)]; }

const FSpellUtility* FSpellUtilityRules::Find(const FString& Id)
{
    for (const FSpellUtility& U : SpellUtility::Table) { if (Id.Equals(U.Id, ESearchCase::IgnoreCase)) { return &U; } }
    return nullptr;
}

bool FSpellUtilityRules::Handled(const FString& Id) { const FSpellUtility* U = Find(Id); return U && U->Kind != ESpellUtility::Exception; }
bool FSpellUtilityRules::IsException(const FString& Id) { const FSpellUtility* U = Find(Id); return U && U->Kind == ESpellUtility::Exception; }

int32 FSpellUtilityRules::CantripDice(int32 CharacterLevel) { return CharacterLevel >= 17 ? 4 : CharacterLevel >= 11 ? 3 : CharacterLevel >= 5 ? 2 : 1; }

int32 FSpellUtilityRules::Beams(const FSrdSpell& Spell, int32 CharacterLevel, int32 Slot)
{
    if (FCString::Stricmp(Spell.Id, TEXT("eldritch-blast")) == 0) { return CantripDice(CharacterLevel); }
    if (FCString::Stricmp(Spell.Id, TEXT("scorching-ray")) == 0) { return 3 + FMath::Max(0, Slot - 2); }
    if (FCString::Stricmp(Spell.Id, TEXT("magic-missile")) == 0) { return 3 + FMath::Max(0, Slot - 1); }
    return 1;
}

int32 FSpellUtilityRules::SizeClass(int32 SpellLevel)
{
    return SpellLevel <= 0 ? 0 : SpellLevel <= 2 ? 1 : SpellLevel <= 4 ? 2 : SpellLevel <= 6 ? 3 : 4;
}
