"""Create the starter map once; validate existing output without overwriting it."""
import unreal

MAP = "/Game/DnDDiceRoll/Maps/L_AgentSandbox"
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)


def spawn(actor_class, location, label):
    actor = actors.spawn_actor_from_class(actor_class, location)
    if not actor:
        raise RuntimeError("Unable to spawn " + label)
    actor.set_actor_label(label)
    return actor


def validate():
    current = actors.get_all_level_actors()
    floors = [a for a in current if a.get_actor_label() == "SandboxFloor"]
    if len(floors) != 1 or not isinstance(floors[0], unreal.StaticMeshActor):
        raise RuntimeError("Sandbox is incomplete: expected one StaticMeshActor labeled SandboxFloor. Existing map preserved; repair it explicitly in Unreal.")
    component = floors[0].static_mesh_component
    if not component.static_mesh or component.get_collision_enabled() == unreal.CollisionEnabled.NO_COLLISION:
        raise RuntimeError("SandboxFloor requires a mesh and collision; existing map preserved.")
    for actor_class in (unreal.DirectionalLight, unreal.SkyLight, unreal.PlayerStart):
        if not any(isinstance(a, actor_class) for a in current):
            raise RuntimeError("Sandbox is incomplete: missing " + actor_class.__name__ + "; existing map preserved.")


if unreal.EditorAssetLibrary.does_asset_exist(MAP):
    if not levels.load_level(MAP):
        raise RuntimeError("Unable to load existing " + MAP)
    validate()
    unreal.log("Sandbox already exists: " + MAP + "; validated and preserved.")
else:
    cube = unreal.load_asset("/Engine/BasicShapes/Cube")
    if not cube:
        raise RuntimeError("Engine cube mesh is unavailable")
    if not levels.new_level(MAP):
        raise RuntimeError("Unable to create " + MAP)
    floor = spawn(unreal.StaticMeshActor, unreal.Vector(0, 0, -10), "SandboxFloor")
    if not floor.static_mesh_component.set_static_mesh(cube):
        raise RuntimeError("Unable to assign floor mesh")
    floor.set_actor_scale3d(unreal.Vector(20, 20, 0.2))
    light = spawn(unreal.DirectionalLight, unreal.Vector(0, 0, 500), "SandboxSun")
    light.set_actor_rotation(unreal.Rotator(-45, -30, 0), False)
    spawn(unreal.SkyLight, unreal.Vector(0, 0, 300), "SandboxSky")
    spawn(unreal.PlayerStart, unreal.Vector(0, 0, 120), "SandboxPlayerStart")
    validate()
    if not levels.save_current_level():
        raise RuntimeError("Unable to save " + MAP)
    if unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages():
        raise RuntimeError("Map remains dirty after save")
    unreal.log("Created and saved " + MAP)
unreal.log("STARTER_BOOTSTRAP_VALIDATED " + MAP)
