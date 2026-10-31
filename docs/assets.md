# Asset provenance

The sandbox uses Unreal Engine's bundled basic cube. No external content has been imported.

For each imported asset or generated source, record its destination, author/provider, source URL or generation recipe, license, and modifications here. Keep editable source assets in `Art/` when introduced and import the runtime form into `Content/`.

## Tactical battle runtime presentation

`AExpeditionBattleView` builds every board tile, obstacle, themed decoration, character figure, marker and effect at runtime from `/Engine/BasicShapes/Cube`, `Cylinder`, `Sphere`, `Cone` and `/Engine/BasicShapes/BasicShapeMaterial`, plus runtime point lights, height fog and a post-process grade. Provider: Epic Games, bundled Unreal Engine content under the applicable Unreal Engine license. Runtime material instances apply original colors; no imported files or binary asset edits. The existing `/Game/DnDDiceRoll/Maps/L_AgentSandbox` remains the saved fixture. Original character/encounter names are authored in C++; supported SRD math and attribution are documented in [rules reference](rules-reference.md).

## Runtime audio

All sound effects (dice, hits, misses, spells, healing, footsteps, turn chimes, camp, loot, victory and defeat) are synthesized in C++ by `ExpeditionAudio` and played through runtime `USoundWaveProcedural` objects. No audio files are imported or stored.
