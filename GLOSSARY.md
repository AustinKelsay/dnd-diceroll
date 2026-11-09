# dnd-diceroll

A party survives a generated expedition by resolving encounters, choosing routes, and managing limited resources.

## Language

**dnd-diceroll**: The game and its product name. An expedition is one run within the game.
_Avoid_: Expedition as the game title

**Expedition**: One complete run toward a simple objective, ending in victory or a party wipe.
_Avoid_: campaign when it implies a larger story system

**Party**: The four starting adventurers and their surviving members during an expedition.

**Adventurer**: A persistent party member whose identity, equipment, resources, and fate carry between encounters.
_Avoid_: pawn when discussing game rules

**Encounter**: One generated tactical situation with an objective, terrain, participants, and an outcome.

**Route**: A choice of the next encounter opportunity with partial information about its risks and rewards.

**Downed**: An adventurer at zero health who cannot act normally but can still be rescued before dying.

**Dead**: Permanently lost for the current expedition; recovery cannot return this adventurer to play.

**Recovery**: A limited opportunity to replenish surviving party members between encounters.

**Party wipe**: The end of an expedition when no party member can continue or rescue another.

**Run record**: A historical summary of a completed expedition that grants no permanent combat advantage.

**Seed**: The reproducible input for an expedition's generated opportunities and random outcomes; separate expeditions may share a seed.

**Checkpoint**: The latest committed state of the current expedition, including resolved actions and permanent losses, from which play resumes.

**Milestone level**: An adventurer's earned tier within the current expedition, retained until that expedition ends.

**Fighter subclass**: The Fighter's chosen combat specialization, fixed for an expedition.

**Precision Strike**: A Battle Master attack that spends a limited maneuver use for improved accuracy and damage.

**Build**: A stable catalog identity selecting a class, compatible subclass (or an explicitly interim base kit), immutable profile and compact ability kit. It is independent of an adventurer instance ID/name.

**Catalog revision**: The persisted rules/profile interpretation revision; unsupported revisions fail restore rather than silently reinterpret a build.

**Next party**: Four ordered build selections edited at startup or after an ending, used only by explicit New Run. Resume retains the saved party.

**Return to menu**: Putting an unfinished expedition away and showing the start screen (Backspace or the Menu button; it waits for the party's turn and finished playback, and mid-encounter it asks first). It writes nothing: the checkpoint stays as last committed, Resume continues from it, and New Run still refuses to replace it.

**Special slot**: One of at most two catalog abilities selected with F/M. Passive features do not create an active slot.

**Ruleset**: The rules interpretation selected when an expedition starts, persisted for the whole run and its encounters.

**Compact**: The default authored tactical ruleset with compact class kits and historical seeded behavior; format 4 saves resume in this mode.

**SRD mode**: Opt-in battle rules based on SRD 5.2.1, including death saves, Initiative Disadvantage for surprise and saving-throw-based Grapple/Shove; not a claim of complete tabletop coverage.
