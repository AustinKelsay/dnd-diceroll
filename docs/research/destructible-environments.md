# Destructible environments and objects: research note

Exploratory research, 2026-11-01. Nothing here is scheduled or approved; it maps what Unreal Engine 5.8 offers, what the project's rules and presentation seams allow, and which product questions need the owner. Every engine claim cites Epic documentation or the installed engine source under `/Users/Shared/Epic Games/UE_5.8/Engine` (the pin in `tools/versions.json`: 5.8.4, CL 58902052, verified 2026-10-30). Web pages were fetched on 2026-11-01; see [Sources](#sources).

Spec drafted from this note: [Destructible terrain with Chaos Destruction](../specs/destructible-terrain.md) (draft, 2026-11-01).

## Project facts this note is grounded in

- Engine 5.8.4 on Apple Silicon/Metal is the validated target; Live Coding is Win64 only, so structural C++ changes use `status` → `close` → `build` → `editor`; changing the engine pin repeats build, bootstrap, automation, identity/discovery, PIE, capture and packaged startup checks (`docs/development.md`).
- `DnDDiceRoll.uproject` enables only editor plugins: `ModelContextProtocol`, `AllToolsets`, `PythonScriptPlugin`, `EditorScriptingUtilities`, each with `TargetAllowList: ["Editor"]`. One runtime module `DnDDiceRoll` depends on `Core`, `CoreUObject`, `Engine`, `InputCore`, `Json` (public) and `RenderCore` (private) (`Source/DnDDiceRoll/DnDDiceRoll.Build.cs`).
- Rules are plain C++ and authoritative. `FCombatEncounter` is `Width`, `Height`, `TArray<FIntPoint> BlockedCells`, `TArray<FCombatantState> Combatants`; `FCombatSnapshot` adds `RandomState`, turn order and budgets (`Source/DnDDiceRoll/Public/Combat/CombatBattle.h`). `ValidateEncounter` accepts a 2..24 grid, blocked cells inside and unique, and no combatant on a blocked cell (`Source/DnDDiceRoll/Private/Combat/CombatBattle.cpp` lines 30-60). The class comment still says "Limits: 16x16 cells"; the validator is the truth.
- Blocked cells block movement in `IsOpen` (`CombatBattle.cpp` line 157) and sight in `HasLineOfSight` (line 203), a supercover traversal where a ray crossing a corner needs both touching cells clear. There are no cover modifiers (`docs/rules-reference.md`, "Tactical adaptations and omissions").
- The three bounded obstacle recipes are a static table in `Source/DnDDiceRoll/Private/Run/ExpeditionRun.cpp` lines 24-28: `split stones` (12 cells), `ruined pillars` (10 cells) and `broken wall` (12 cells), each with an enemy column and first row. `Generate` copies `Recipe.BlockedCells` into the layout (line 128); `Route.Recipe` is drawn from the run stream (line 87). Restore rejects a saved encounter whose `BlockedCells` differ from the recipe (line 288). Saves are version-3 value-only JSON; other versions are rejected (`Source/DnDDiceRoll/Private/Persistence/ExpeditionSave.cpp` line 108; `docs/architecture.md`, "Expedition persistence"; `docs/adr/0001-authoritative-state.md`).
- Presentation replays `FCombatResult` facts (actor, target, path, dice, downed, died) through `FExpeditionPlayback`; `Reset(Snapshot)` rebuilds display state and `Finish()` converges on the committed snapshot (`Source/DnDDiceRoll/Public/Presentation/ExpeditionPlayback.h` lines 95-111). Rules never read presentation (`CombatBattle.h`, `FCombatResult` comment).
- `AExpeditionBattleView::RebuildBoard` destroys every spawned component and rebuilds (`ExpeditionBattleView.cpp` lines 221-255). Each blocked cell becomes one `UStaticMeshComponent` with a `UMaterialInstanceDynamic` via `MakePart` (lines 135-160): themed `Pillar`, `Wall` or `Stones` meshes from `UDiceRollTheme` when present (line 1153), otherwise `BuildObstacle` assembles engine primitives (plinth/shaft/capital, boulder cluster, wall segment; lines 334-380). Components are individual, not instanced. Theme meshes are original Blender exports with per-asset `Art/<asset-id>/r001/asset.json` records and an import script (`docs/assets.md`).
- Grids are 16-18 by 11-12 cells, cardinal movement, five feet per cell (`docs/game-brief.md`, `docs/rules-reference.md`). The look is dark high fantasy with a low-poly PS1 influence; engine primitives are the fallback (`docs/game-brief.md`, "Scope"). Decisions that change product intent need owner input (`CONTEXT.md`, "Engineering direction").

## 1. Unreal's destruction landscape in 5.8

### Plugins and modules in the installed engine

| Plugin (`.uplugin`) | Path under `Engine/Plugins/` | EnabledByDefault | Flag | Modules |
| --- | --- | --- | --- | --- |
| Chaos Solver | `Experimental/ChaosSolverPlugin` | true | Beta | `ChaosSolverEditor` (Editor) |
| Geometry (GeometryCollectionPlugin) | `Experimental/GeometryCollectionPlugin` | false | Beta | `GeometryCollectionEditor`, `GeometryCollectionTracks`, `GeometryCollectionSequencer`, `GeometryCollectionNodes`, `GeometryCollectionDepNodes` |
| Fracture | `Experimental/Fracture` | false | Beta | `FractureEngine` (Runtime) |
| ChaosEditor ("Destruction Tools") | `Experimental/ChaosEditor` | true | Beta | `FractureEditor` (Editor); depends on `GeometryCollectionPlugin`, `Fracture`, `PlanarCut`, `MeshModelingToolsetExp` |
| Chaos Niagara | `Experimental/ChaosNiagara` | true | Beta | `ChaosNiagara` (Runtime) |
| ChaosCaching | `Experimental/ChaosCaching` | false | Experimental | `ChaosCaching` (Runtime), `ChaosCachingEditor` |
| Editor DataflowGraph | `Dataflow` | true | none | `DataflowEditor`, `DataflowEnginePlugin` (Runtime), `DataflowNodes` (Runtime) |
| Geometry Script | `Runtime/GeometryScripting` | unset | none in descriptor; docs say Beta | `GeometryScriptingCore` (Runtime), `GeometryScriptingEditor` |
| Niagara | `FX/Niagara` | true | none | runtime and editor modules |
| Apex Destruction | `Runtime/ApexDestruction` | false | none | `ApexDestruction` (Runtime), `ApexDestructionEditor` |

Flags read from each descriptor on 2026-11-01. An engine plugin with no `EnabledByDefault` key is not enabled by default (`Engine/Source/Runtime/Projects/Private/PluginManager.cpp`, `FPlugin::IsEnabledByDefault`, lines 421-435). A plugin's `Plugins` array enables its dependencies, so a default project that keeps `ChaosEditor` on also gets `GeometryCollectionPlugin` and `Fracture`; this was read from descriptors, not observed in a running editor.

The runtime classes are engine modules, not plugin modules: `Engine/Source/Runtime/Experimental/GeometryCollectionEngine` (`UGeometryCollection`, `UGeometryCollectionComponent`), `Engine/Source/Runtime/Experimental/FieldSystem/Source/FieldSystemEngine` (`UFieldSystemComponent`, field nodes), `Engine/Source/Runtime/Experimental/ChaosSolverEngine` (`AChaosSolverActor`, `UChaosSolverSettings`; `Engine/Config/BaseEngine.ini` line 3721 sets `DefaultChaosSolverActorClass`). A game module can therefore link `GeometryCollectionEngine`, `FieldSystemEngine` and `ChaosSolverEngine` without enabling any plugin; the plugins supply asset creation, Fracture Mode, Sequencer tracks and Dataflow nodes. None of these modules carries a `PlatformAllowList`, so Mac is not excluded by descriptor; packaged Mac behaviour still needs the project's own verification.

### Chaos Destruction (Geometry Collections)

What it is: a Geometry Collection is the container asset for destruction geometry and simulation properties, created from static or skeletal meshes; pieces are clustered into one rigid body, connected to neighbours with strain-limited constraints, and separate when impulses from collisions or fields exceed those limits; fields can also lower strain to weaken a structure; a cache system replays complex destruction cheaply; Niagara and the audio mixer can react to simulation events ([Destruction Overview](https://dev.epicgames.com/documentation/en-us/unreal-engine/destruction-overview)).

Maturity: Epic's 5.4 release notes state "Chaos Destruction is now Production Ready with Unreal Engine 5.4" under "Simulation and VFX > Chaos Destruction", and that per-instance geometry collection memory was reduced ([5.4 release notes](https://dev.epicgames.com/documentation/unreal-engine/unreal-engine-5.4-release-notes?application_version=5.4)). The 5.8 descriptors still mark the plugins Beta (table above); the 5.8 release notes list only incremental Chaos Destruction items: a default Dataflow template when creating an asset, render settings for geometry collection renderable types, a `GyroscopicTorque` option, and a collision-response fix ([5.8 release notes](https://dev.epicgames.com/documentation/unreal-engine/unreal-engine-5-8-release-notes?lang=en-US), "Chaos Destruction").

Authoring (Fracture Mode, `ChaosEditor` plugin): select static mesh actors, switch the editor mode to Fracture (Shift-6), click New under Generate, name and save; the actors are replaced by the Geometry Collection. Source meshes should be watertight and non-intersecting ([Geometry Collections User Guide](https://dev.epicgames.com/documentation/en-us/unreal-engine/geometry-collections-user-guide)). Fracture tools: Uniform, Cluster, Radial, Planar, Slice, Brick (labelled experimental), Mesh (a static mesh as the cutter) and Custom; common options are Random Seed (-1 for a new seed each time), Chance to Fracture, Group Fracture, Grout and noise settings; Fracture Level in View Settings selects the hierarchy level shown ([Fracturing Geometry Collections User Guide](https://dev.epicgames.com/documentation/en-us/unreal-engine/fracturing-geometry-collections-user-guide)). The installed `FractureEditor` module also ships AutoCluster, ClusterMagnet, Convex, Embed, FixTinyGeo, Materials, Proximity, Resample, SplitIslands and UV tools (`Engine/Plugins/Experimental/ChaosEditor/Source/FractureEditor/Public/FractureTool*.h`).

Asset settings (`GeometryCollectionEngine/Public/GeometryCollection/GeometryCollectionObject.h`, lines 553-600 and 786-822):
- Clustering: `EnableClustering`, `ClusterGroupIndex` (collections sharing an index simulate as one cluster), `MaxClusterLevel` ("Maximum level for cluster breaks"), `ClusterConnectionType`, `ConnectionGraphBoundsFilteringMargin`.
- Damage: `DamageModel`, `TArray<float> DamageThreshold` ("Damage threshold for clusters at different levels"), `bUseSizeSpecificDamageThreshold` (per `SizeSpecificData` entry, each with `MaxSize`, `CollisionShapes`, `DamageThreshold`), `bUseMaterialDamageModifiers`, `PerClusterOnlyDamageThreshold` (compatibility), `DamagePropagationData`.
- Removal: `bRemoveOnMaxSleep` ("Remove on Sleep"), `MaximumSleepTime`, `RemovalDuration`, `bAutomaticCrumblePartialClusters`, `bSlowMovingAsSleeping`, `bScaleOnRemoval` (shrink while removed, on by default). Remove-on-break is configured per fracture level in the Fracture tools and gated per instance by the component's `bAllowRemovalOnBreak`; the older "remove pieces on fracture" setting is deprecated in favour of it ([Python API, GeometryCollection 5.4](https://dev.epicgames.com/documentation/en-us/unreal-engine/python-api/class/GeometryCollection?application_version=5.4), secondary to the header).
- Collision and mass: `PhysicsMaterial`, `bDensityFromPhysicsMaterial`, collision type/implicit type per size band.
- Nanite: `EnableNanite`, `bEnableNaniteFallback`, "Nanite Minimum Residency" (lines 648-666). `DataflowAsset` links a procedural fracture graph (line 885).

Component API (`GeometryCollectionEngine/Public/GeometryCollection/GeometryCollectionComponent.h`): `BlueprintCallable` functions include `SetRestCollection`, `ApplyExternalStrain`, `ApplyInternalStrain`, `ApplyLinearVelocity`, `ApplyAngularVelocity`, `ApplyBreakingLinearVelocity`, `ApplyBreakingAngularVelocity`, `ApplyKinematicField`, `ApplyPhysicsField`, `CrumbleCluster`, `CrumbleActiveClusters`, `SetAnchoredByIndex`, `SetAnchoredByBox`, `SetAnchoredByTransformedBox`, `RemoveAllAnchors`, `SetDamageThreshold`, `SetDamageModel`, `SetDamagePropagationData`, `SetEnableDamageFromCollision`, `SetUseMaterialDamageModifiers`, `SetNotifyBreaks`/`Removals`/`Crumblings`, `IsRootBroken`, `GetRootCurrentTransform`, `ResetState`. Events: `OnChaosBreakEvent`, `OnChaosRemovalEvent`, `OnChaosCrumblingEvent`, `OnRootBreakEvent`, `FOnGeometryCollectionFullyDecayedEvent`, `FOnGeometryCollectionRootMovedEvent` (lines 53-61, 1269-1278). Per-instance switches: `bAllowRemovalOnSleep`, `bAllowRemovalOnBreak` (lines 1061-1066), `bNotifyBreaks`, `bNotifyRemovals`, `bNotifyCrumblings` (1446-1462), `bEnableReplication` (1538, irrelevant for this single-player game), and a `Cache` category for recording breaking and trailing data (lines 89-149).

Cost profile: each collection is a rigid-body cluster hierarchy on the Chaos physics thread plus a custom renderer; cost grows with piece count, cluster depth and simultaneously awake pieces. `MaxClusterLevel`, remove-on-break/sleep and `DamageThreshold` per level are the tuning levers in the headers above; no Epic figures for this hardware were found. Chaos Destruction has no Mac-specific statement in Epic's pages fetched here; the engine descriptors do not exclude Mac.

Nanite for geometry collections: Epic lists Geometry Collections among Nanite-capable mesh types and documents the "Enable Nanite" checkbox in the Geometry Collections editor; the "Supported Platforms" text names DirectX 12 / Shader Model 6 and "current console and desktop platforms" without mentioning macOS or Metal ([Nanite Virtualized Geometry](https://dev.epicgames.com/documentation/unreal-engine/nanite-virtualized-geometry-in-unreal-engine)). Locally, `Engine/Config/Mac/DataDrivenPlatformInfo.ini` sets `bSupportsNanite=false` for `[ShaderPlatform METAL_SM5]` and `true` for `[ShaderPlatform METAL_SM6]`. `Config/DefaultEngine.ini` sets no Mac shader platform, so the engine default applies; which one was not verified. Nanite is irrelevant to the low-poly look anyway.

Known limitations relevant here: physics-driven piece motion is not deterministic by default (section 2); the simulation runs on wall-clock physics ticks, so instant playback (`EPlaybackSpeed::Instant`) cannot fast-forward it; fractured pieces have their own collision, which can push figures or block picking; rebuilds must destroy and respawn collections (the current `RebuildBoard` pattern already does this for static mesh components).

### Fields

A `UFieldSystemComponent` applies field commands to the solver: `ApplyLinearForce`, `ApplyStayDynamicField`, `ApplyRadialForce`, `ApplyRadialVectorFalloffForce`, `ApplyUniformVectorFalloffForce`, `ApplyStrainField` ("Apply External Strain"), `ApplyPhysicsField` ("Add Transient Field"), `AddPersistentField`, `RemovePersistentFields`, `AddFieldCommand`, `ResetFieldSystem` (`FieldSystemEngine/Public/Field/FieldSystemComponent.h` lines 95-271). Field node classes: `URadialFalloff`, `UPlaneFalloff`, `UBoxFalloff`, `UNoiseField`, `UUniformVector`, `URadialVector`, `URotatedRadialVector`, `URandomVector`, `UOperatorField`, `UCullingField`, `UUniformScalar`, `UWaveScalar`, `URadialIntMask`, `UUniformInteger`, `UToIntegerField`, `UToFloatField`, `UReturnResultsTerminal`, plus metadata nodes (`FieldSystemObjects.h`). Physics targets include `Field_DynamicState`, `Field_LinearForce`, `Field_ExternalClusterStrain`, `Field_InternalClusterStrain`, `Field_Kill`, `Field_LinearVelocity`, `Field_DisableThreshold`, `Field_SleepingThreshold` (`Chaos/Public/Field/FieldSystemTypes.h` lines 151-160).

Epic's pre-built fields live in `Engine/Content/EditorResources/FieldNodes`: `FS_AnchorField_Generic` (assigned under the collection's Initialization Fields; makes bones inside the volume static; box, sphere or plane falloff), `FS_MasterField` (activation type Delay, OnTick, OnTickWithDelay or Trigger, where Trigger is "activated by a Blueprint directly"; external strain with magnitude and falloff, internal strain decay, radial or directional velocity, noise), and sleep/disable/kill fields ([Chaos Fields User Guide](https://dev.epicgames.com/documentation/en-us/unreal-engine/chaos-fields-user-guide-in-unreal-engine)). The quick start triggers destruction by spawning `FS_MasterField` at a hit location with Activation Type Trigger from a projectile Blueprint ([Destruction Quick Start](https://dev.epicgames.com/documentation/en-us/unreal-engine/destruction-quick-start)). From C++ the equivalent is `UGeometryCollectionComponent::ApplyExternalStrain` or `CrumbleCluster` on a specific component, which avoids a world field touching neighbours.

### Cache and pre-simulated destruction

`AChaosCacheManager` (ChaosCaching plugin, Experimental, off by default) has `ECacheMode` {`None` ("Static Pose"), `Play`, `Record`} and `EStartMode` {`Timed`, `Triggered`} (`Engine/Plugins/Experimental/ChaosCaching/Source/ChaosCaching/Public/Chaos/CacheManagerActor.h` lines 24-37). Epic's API reference describes `trigger_all` / `trigger_component`, a `cache_collection` asset and the Start node for Static Pose or Play modes ([Python API, ChaosCacheManager 5.5](https://dev.epicgames.com/documentation/en-us/unreal-engine/python-api/class/ChaosCacheManager?application_version=5.5)); `UChaosCache` stores per-particle transform and curve tracks ([UChaosCache](https://dev.epicgames.com/documentation/unreal-engine/API/Plugins/ChaosCaching/UChaosCache?lang=en-US)). A recorded cache replays the same motion every time, which makes it the only Chaos path with reproducible visuals; it costs a recording session per asset and a plugin dependency.

### Niagara coupling

`UNiagaraDataInterfaceChaosDestruction` ("Chaos Destruction Data", ChaosNiagara plugin, on by default) samples solver events with `DataSourceType` of Collision, Breaking or Trailing, reads a `ChaosSolverActorSet`, and spawns particles per event with `SpawnMultiplierMinMax`, `SpawnChance`, `DataProcessFrequency` and `MaxNumberOfDataEntriesToSpawn` ([API reference](https://dev.epicgames.com/documentation/unreal-engine/API/Plugins/ChaosNiagara/UNiagaraDataInterfaceChaosDestru-?lang=en-US); enum values in `Engine/Plugins/Experimental/ChaosNiagara/Source/ChaosNiagara/Classes/NiagaraDataInterfaceChaosDestruction.h` lines 297-299). Fallback without Chaos: a plain Niagara burst at a cell, or the project's existing primitive sparks (`SparkMeshes`, `ExpeditionBattleView.cpp` lines 634, 776-786), which already need no plugin.

### Dataflow

Dataflow is Epic's node-based system for procedural generation of physics-based assets ([5.8 release notes](https://dev.epicgames.com/documentation/unreal-engine/unreal-engine-5-8-release-notes?lang=en-US), "Chaos Dataflow Editor"). For destruction it replaces Fracture Mode: creating a Geometry Collection with a DataFlowAsset yields a graph of `Static Mesh To Collection`, `Bounding Box`, `Uniform Scatter Points`, `Voronoi Fracture`, `Auto Cluster` and `Geometry Collection Terminal`, where the terminal's Damage section holds the per-level thresholds ([Dataflow for Destruction Quickstart](https://dev.epicgames.com/documentation/en-us/unreal-engine/dataflow-for-destruction-quickstart)). The `Dataflow` plugin is on by default; the fracture nodes come from `GeometryCollectionNodes` in the Geometry plugin (table above). Useful if many props share one fracture recipe; otherwise Fracture Mode is enough.

### Out of scope

Chaos Flesh (`Experimental/ChaosFlesh`, Experimental, off) and Chaos Cloth Asset (`ChaosClothAsset`, off) are deformable simulations, not destruction.

Apex Destruction: the plugin folder still exists in 5.8 but is off by default, its `Build.cs` has the PhysX/APEX dependencies commented out, and `UDestructibleComponent` carries `UE_DEPRECATED(4.26, "APEX is deprecated. Destruction in future will be supported using Chaos Destruction.")` with 5.1 deprecations pointing to `UGeometryCollection` (`Engine/Plugins/Runtime/ApexDestruction/Source/ApexDestruction/Public/DestructibleComponent.h` lines 29, 81-85; [API: ApexDestruction](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Plugins/ApexDestruction)). Do not consider it.

### Non-physics alternatives

| Approach | Determinism | Runtime cost | Authoring | Fit with the low-poly PS1 look |
| --- | --- | --- | --- | --- |
| Mesh swap: intact mesh replaced by a broken variant (or hidden) when the rules say so | Exact; it is a function of rules state | One component swap; same as today's `MakePart` | One extra Blender export per breakable prop, recorded like the theme set (`docs/assets.md`) | Best fit; chunky hand-made rubble reads as PS1 |
| Hand-authored chunks animated by timeline or code (tween, drop, fade) | Exact if driven by playback time, like the existing lunges and falls in `FExpeditionPlayback` | A few extra components per event | Chunk meshes or reuse of existing stones | Good; no physics jitter |
| Hand-authored chunks with simple physics (`UStaticMeshComponent::SetSimulatePhysics`) | Not reproducible; visual only | Small rigid bodies on the Chaos solver | Chunk meshes | Acceptable if few pieces |
| Material-only damage: `Color`/`Glow` tint on the existing `M_DiceRollTheme`, decals, vertex colour, scorch planes | Exact | Negligible | Material parameters; the theme material already exposes `Color` and `Glow` (`docs/assets.md`) | Good for cracked or scorched states |
| Geometry Script / Dynamic Mesh: boolean cuts at runtime | Deterministic for the same inputs, but geometry is not a rules fact | Game-thread mesh ops; `UDynamicMeshComponent` lacks LOD and instancing; Nanite and Lumen unsupported | No art, but code; plugin is labelled Beta with "use caution when shipping" | Clean cuts look CAD-like, not hand-made |
| Skeletal or animated collapse | Exact | Animation playback | Rigged Blender asset | Good; the most "authored" look |

Geometry Script facts: the plugin is "Geometry Script", documented as Beta; `UDynamicMeshComponent` is backed by a per-component `UDynamicMesh` stored in the level rather than a shared asset; some functions are editor-only; Blueprint execution is on the game thread ([Geometry Scripting User Guide](https://dev.epicgames.com/documentation/en-us/unreal-engine/geometry-scripting-users-guide-in-unreal-engine)). The subtraction example there (temporary primitive subtracted from a mesh) is the boolean path. `ProceduralMeshComponent` (`Engine/Plugins/Runtime/ProceduralMeshComponent`, on by default) is the older raw-triangle route.

## 2. Determinism and authority

What Epic says: the solver has a determinism request, `FPBDRigidsSolver::SetIsDeterministic(const bool)`, whose remark reads "Even if set to false, the sim may still be deterministic if some other feature is enabled and requires it" ([API: SetIsDeterministic](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Chaos/FPBDRigidsSolver_1/SetIsDeterministic); `Engine/Source/Runtime/Experimental/Chaos/Public/PBDRigidsSolver.h` lines 222-227). The console variable `p.Chaos.Solver.Deterministic` is documented in source as "Override determinism. 0: disabled; 1: enabled; -1: use config" with default -1; `IsDetemerministic()` returns true when the flag was requested, rewind data exists, the cvar is 1, or network physics prediction is on (`Engine/Source/Runtime/Experimental/Chaos/Private/PBDRigidsSolver.cpp` lines 345-346, 3621-3624). The evolution's `bIsDeterministic` defaults to false (`Chaos/Private/Chaos/PBDRigidsEvolutionGBF.cpp` line 1377). Deterministic mode orders islands and constraints consistently for a given sequence of inputs on one machine; it does not make results independent of frame timing, tick rate or hardware, and Epic documents no cross-machine guarantee in the pages fetched. No Epic page describing the cvar beyond the source string was found.

Conclusion for this project: destruction state is a rules fact or it does not exist. A wall cell becoming floor, an object's HP, and "this brazier is broken" belong in `FCombatEncounter`/`FCombatSnapshot` and are produced only by accepted commands through `FCombatBattle::Submit`, consuming the seeded stream in a documented order. Chaos pieces, dust and rubble are presentation, exactly like floating damage text: triggered by `FCombatResult` facts, never read back, never gating a result (`docs/adr/0001-authoritative-state.md`; `docs/architecture.md`, "Expedition presentation").

Save/restore implications:
- The committed snapshot must carry the post-destruction board. Today `BlockedCells` is copied from a static recipe and restore rejects any difference (`ExpeditionRun.cpp` line 288). Breakable cells need either a persisted "destroyed cells" list validated against the recipe (cells must be a subset of the recipe's blocked set) or a persisted current `BlockedCells` plus the recipe for preview text. Either way the save schema moves to version 4 with explicit rejection of version 3, following the version 1/2 policy (`docs/rules-reference.md`, "Save, resume and ending records").
- Restore never re-simulates. `RebuildBoard` builds obstacles from the snapshot, so a destroyed cell restores as floor (or as a static "rubble" mesh) with no animation; a preserved object with 3 of 10 HP restores with its damaged material state. Playback `Finish` must converge on the committed snapshot: an in-flight collapse animation ends by snapping to the final mesh state, which rules out Chaos simulation as the thing that decides what is visible at the end.
- Forecast and `PathTo` stay const queries; a breakable cell shows up as a target through `LegalTargets`, not through geometry.
- Enemy pathing changes: `Distances` and `IsOpen` already read `BlockedCells`, so removing a cell opens routes for both sides without extra code, but enemy decisions then depend on the destroyed set, which must be identical after resume.

## 3. Candidate later slices

SRD 5.2.1 object rules, for reference: "Breaking Objects" in the Playing the Game chapter lets a creature "automatically break or otherwise destroy a fragile, nonmagical object" as an action and defers sturdier objects to the Rules Glossary (PDF p.12). The glossary entry (pp.177-178) gives Object Armor Class by substance (cloth/paper/rope 11, crystal/glass/ice 13, wood 15, stone 17, iron/steel 19, mithral 21, adamantine 23), Object Hit Points by size (Tiny 2 fragile / 5 resilient, Small 3 / 10, Medium such as a barrel 4 / 18, Large such as a cart 5 / 27), destruction at 0 HP, immunity to Poison and Psychic damage, and the GM deciding whether destroying part of an object collapses the whole ([SRD 5.2.1 PDF](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf); [online verification](https://www.dndbeyond.com/sources/dnd/br-2024/rules-glossary)). Any adoption follows the attribution and "document the adaptation" policy in `docs/srd-source-notes.md` and `docs/rules-reference.md`.

### A. Breakable obstacle cells

Rules: a blocked cell gains AC and HP (for example stone 17 / 27 resilient Large, or a scaled-down arena value); `Attack` and `Fire Bolt` accept a cell target; at 0 HP the cell leaves `BlockedCells`, opening movement and sight. Needs a new target kind in `FCombatCommand` (cell rather than `TargetId`), `LegalTargets` and `Forecast` support, `FCombatResult` facts (which cell, remaining HP, destroyed), validation and schema v4. Magic Missile targets creatures only (`docs/rules-reference.md`), so it would stay excluded unless the owner adapts it.
Presentation: mesh swap from `Pillar`/`Wall`/`Stones` to a rubble variant or tile, with the existing sparks and a camera shake; later optionally a Chaos crumble for the swap moment.
Assets: one rubble mesh per obstacle family, Blender-sourced with records.
Open questions: should obstacles be attackable at all, given the brief's "blocked terrain obscures sight" simplicity? Which obstacles (all three recipes, or only "broken wall")? Does spending an Action on a wall fit the five-fight pacing? Should enemies attack walls (AI change) or never?

### B. Destructible props as targets or hazards

Rules: a new combatant-like entity (barrel, crate, brazier) with AC/HP from the SRD tables, occupying a cell, no turns. A brazier could deal fire damage to adjacent cells when destroyed (a dnd-diceroll adaptation; the SRD has no brazier rule). Needs an object list in `FCombatEncounter`, validation, targeting, outcome facts and schema v4.
Presentation: intact and broken meshes; the theme already has `Brazier` and `Flame`.
Open questions: do props exist in recipes (changes the three-recipe generator), are they loot, do they give cover (no cover modifiers in V1)?

### C. Area effects knocking down walls

Rules: no area spell exists in the supported subset; Fire Bolt is single-target. Adding one is a new ability with a slot cost and balance work, so this is a product decision before any destruction work.
Presentation: multi-cell mesh swap in one playback step; Chaos radial field as an optional flourish.

### D. Scripted collapse or hazard events per recipe

Rules: the encounter recipe schedules a deterministic event (round N: cells X become blocked or unblocked, or units in cells Y take damage), resolved inside `AdvanceTurn` with the seeded stream. Fits the "bounded recipe" generator and preview disclosure ("terrain" text already names the recipe).
Presentation: pre-authored collapse animation; Chaos Cache playback is the one Chaos path that stays reproducible, if ever used.
Open questions: do hazards fit the "partial risk/reward information" route previews? Should the preview warn?

### E. Cosmetic battle damage

No rules impact. Scorch decals or tint changes on hit cells, rubble sprinkles on kills, shattering figures on enemy death (the existing fall animation already exists). Pure presentation keyed to `FCombatResult` facts, safe under `Finish`, no save impact. Cheapest visible win.

### F. Finale and the Waystone vault

The vault scenery builds three wall runs, torches, banners and the Waystone (`ExpeditionBattleView.cpp` lines 487-510, 656). A finale-specific scripted collapse (D) or a victory shatter (E) around the Waystone pedestal would be presentation only unless the design wants the vault walls to be breakable (A).
Open questions: is the finale meant to feel different mechanically or only visually?

## 4. Recommended path

1. Cheapest meaningful step: slice E (cosmetic damage) proves the presentation seam with zero rules or save change, then slice A limited to one recipe ("broken wall") with rules-level HP, mesh-swap presentation and schema v4. Both run on the current module dependencies; no plugin, no Build.cs change, no Mac re-validation beyond the normal build, automation and package evidence. Tests follow the existing seams: `Combat.*` for targeting and outcome facts, `Presentation.PlaybackConvergesOnCommittedState` for the swap, persistence tests for v4 rejection of v3.
2. What a Chaos layer would add: real tumbling rubble and dust at the swap moment. What it would cost: enabling `GeometryCollectionPlugin`/`Fracture` (or relying on `ChaosEditor` pulling them in), linking `GeometryCollectionEngine`, `FieldSystemEngine` and `ChaosSolverEngine`, authoring one Geometry Collection per breakable mesh in Fracture Mode from the Blender export (with provenance records), rebuilding and re-verifying on Mac, cooked content growth from fractured geometry, and a presentation that still ends in a deterministic mesh swap because pieces cannot be the committed state. It is a flourish on top of step 1, never a replacement.
3. Prototype first, in a throwaway branch and sandbox map, to answer the unknowns: (a) does a Geometry Collection built from a theme mesh look right at PS1 polygon counts, or does Voronoi rubble break the style; (b) does `-nullrhi` automation and the Mac package still pass with the runtime modules linked; (c) does an instant-speed playback snap cleanly when a crumble is mid-flight; (d) how much the cooked package grows per fractured asset.

Checklist of what enabling Chaos destruction would change in this repo:
- `DnDDiceRoll.uproject`: add `GeometryCollectionPlugin` and `Fracture` (both Beta, off by default) with `TargetAllowList: ["Editor"]`, matching the project's existing entries: the Geometry Collection plugin depends on `EditorScriptingUtilities` (scoped to Editor here) and both ship runtime modules the game does not need, and the runtime classes live in engine modules anyway. `ChaosEditor`, `ChaosSolverPlugin`, `ChaosNiagara`, `Dataflow` and `Niagara` are already on by default; `Dataflow` also enables `ChaosCaching`, so say "no cache assets used" rather than "off". (Corrected 2026-11-01 after council review; an earlier draft said editor-only entries were not possible.)
- `Source/DnDDiceRoll/DnDDiceRoll.Build.cs`: add `GeometryCollectionEngine`, which already depends publicly on `ChaosSolverEngine` and `FieldSystemEngine` (`GeometryCollectionEngine.Build.cs` lines 17-23); add `Chaos` only if solver types are used directly. `Niagara` is a plugin module (`Engine/Plugins/FX/Niagara`, `EnabledByDefault: true`), not an engine module, and any Niagara spawn needs it, not only the data-interface path. (Corrected 2026-11-01 after council review.)
- Build loop: structural change, so `status` → `close` → `build` → `editor` (`docs/development.md`).
- Validation gate: the AGENTS.md rule for upgrades (build, bootstrap, automation, MCP/PIE, capture, package) applies in practice because plugin and module changes alter the module manifest and cooked content; record the evidence in a run record.
- Assets: Geometry Collections are binary assets created through Unreal, saved explicitly under `/Game/DnDDiceRoll`, with source and recipe recorded per `docs/assets.md` and `docs/agents/asset-production.md`.
- Tests: no rules test may depend on physics; presentation tests keep asserting convergence on the snapshot.
- Package size and startup: measure with `python3 tools/dev.py package`; packaging only proves `-nullrhi` startup, so rendered rubble needs a capture.

## Sources

Fetched or read on 2026-11-01.

Epic documentation (dev.epicgames.com), all showing "Unreal Engine 5.8 Documentation" unless noted:
- [Destruction Overview](https://dev.epicgames.com/documentation/en-us/unreal-engine/destruction-overview)
- [Chaos Destruction landing page](https://dev.epicgames.com/documentation/en-us/unreal-engine/chaos-destruction-in-unreal-engine) (links only)
- [Geometry Collections User Guide](https://dev.epicgames.com/documentation/en-us/unreal-engine/geometry-collections-user-guide)
- [Fracturing Geometry Collections User Guide](https://dev.epicgames.com/documentation/en-us/unreal-engine/fracturing-geometry-collections-user-guide)
- [Chaos Fields User Guide](https://dev.epicgames.com/documentation/en-us/unreal-engine/chaos-fields-user-guide-in-unreal-engine)
- [Physics Fields](https://dev.epicgames.com/documentation/en-us/unreal-engine/physics-fields-in-unreal-engine) (introduction only)
- [Destruction Quick Start](https://dev.epicgames.com/documentation/en-us/unreal-engine/destruction-quick-start)
- [Dataflow for Destruction Quickstart](https://dev.epicgames.com/documentation/en-us/unreal-engine/dataflow-for-destruction-quickstart)
- [Geometry Scripting User Guide](https://dev.epicgames.com/documentation/en-us/unreal-engine/geometry-scripting-users-guide-in-unreal-engine)
- [Nanite Virtualized Geometry](https://dev.epicgames.com/documentation/unreal-engine/nanite-virtualized-geometry-in-unreal-engine) (header 5.8, some in-page links 5.5)
- [UNiagaraDataInterfaceChaosDestruction API](https://dev.epicgames.com/documentation/unreal-engine/API/Plugins/ChaosNiagara/UNiagaraDataInterfaceChaosDestru-?lang=en-US)
- [FPBDRigidsSolver::SetIsDeterministic API](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Chaos/FPBDRigidsSolver_1/SetIsDeterministic)
- [ApexDestruction API](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Plugins/ApexDestruction)
- [UChaosCache API](https://dev.epicgames.com/documentation/unreal-engine/API/Plugins/ChaosCaching/UChaosCache?lang=en-US) and [Python API: ChaosCacheManager 5.5](https://dev.epicgames.com/documentation/en-us/unreal-engine/python-api/class/ChaosCacheManager?application_version=5.5) (seen through search summaries; secondary)
- [Python API: GeometryCollection 5.4](https://dev.epicgames.com/documentation/en-us/unreal-engine/python-api/class/GeometryCollection?application_version=5.4) (search summary; secondary)
- [Unreal Engine 5.4 Release Notes](https://dev.epicgames.com/documentation/unreal-engine/unreal-engine-5.4-release-notes?application_version=5.4)
- [Unreal Engine 5.8 Release Notes](https://dev.epicgames.com/documentation/unreal-engine/unreal-engine-5-8-release-notes?lang=en-US)

Installed engine (`/Users/Shared/Epic Games/UE_5.8/Engine`):
- `Plugins/Experimental/{ChaosSolverPlugin,GeometryCollectionPlugin,Fracture,ChaosEditor,ChaosNiagara,ChaosCaching,ChaosFlesh}/*.uplugin`, `Plugins/Dataflow/Dataflow.uplugin`, `Plugins/Runtime/{GeometryScripting,ApexDestruction,ProceduralMeshComponent}/*.uplugin`, `Plugins/FX/Niagara/Niagara.uplugin`, `Plugins/ChaosClothAsset/ChaosClothAsset.uplugin`
- `Source/Runtime/Experimental/GeometryCollectionEngine/Public/GeometryCollection/GeometryCollectionComponent.h`, `GeometryCollectionObject.h`
- `Source/Runtime/Experimental/FieldSystem/Source/FieldSystemEngine/Public/Field/FieldSystemComponent.h`, `FieldSystemObjects.h`; `Source/Runtime/Experimental/Chaos/Public/Field/FieldSystemTypes.h`
- `Source/Runtime/Experimental/Chaos/Private/PBDRigidsSolver.cpp`, `Public/PBDRigidsSolver.h`, `Private/Chaos/PBDRigidsEvolutionGBF.cpp`
- `Plugins/Experimental/ChaosNiagara/Source/ChaosNiagara/Classes/NiagaraDataInterfaceChaosDestruction.h`
- `Plugins/Experimental/ChaosCaching/Source/ChaosCaching/Public/Chaos/CacheManagerActor.h`
- `Plugins/Experimental/ChaosEditor/Source/FractureEditor/Public/FractureTool*.h`
- `Plugins/Runtime/ApexDestruction/Source/ApexDestruction/ApexDestruction.Build.cs`, `Public/DestructibleComponent.h`
- `Source/Runtime/Projects/Private/PluginManager.cpp`
- `Config/BaseEngine.ini`, `Config/Mac/DataDrivenPlatformInfo.ini`

SRD:
- [SRD 5.2.1 PDF](https://media.dndbeyond.com/compendium-images/srd/5.2/SRD_CC_v5.2.1.pdf), p.12 "Breaking Objects", pp.177-178 Rules Glossary "Breaking Objects" (text extracted with pdftotext from the downloaded file); online verification at [Playing the Game](https://www.dndbeyond.com/sources/dnd/br-2024/playing-the-game) and [Rules Glossary](https://www.dndbeyond.com/sources/dnd/br-2024/rules-glossary)

Repository: `tools/versions.json`, `DnDDiceRoll.uproject`, `Config/DefaultEngine.ini`, `Source/DnDDiceRoll/DnDDiceRoll.Build.cs`, `Source/DnDDiceRoll/Public/Combat/CombatBattle.h`, `Source/DnDDiceRoll/Private/Combat/CombatBattle.cpp`, `Source/DnDDiceRoll/Private/Run/ExpeditionRun.cpp`, `Source/DnDDiceRoll/Private/Persistence/ExpeditionSave.cpp`, `Source/DnDDiceRoll/Public/Presentation/{ExpeditionBattleView.h,ExpeditionPlayback.h,DiceRollTheme.h}`, `Source/DnDDiceRoll/Private/Presentation/ExpeditionBattleView.cpp`, `docs/development.md`, `docs/architecture.md`, `docs/adr/0001-authoritative-state.md`, `docs/game-brief.md`, `docs/rules-reference.md`, `docs/srd-source-notes.md`, `docs/assets.md`, `docs/runs/expedition-polish/01-presentation-polish.md`, `docs/runs/expedition-polish/03-checkpoint.md`, `CONTEXT.md`, `GLOSSARY.md`.
