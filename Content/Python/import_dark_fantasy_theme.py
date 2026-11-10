"""Import the dark-fantasy theme from Art/ into /Game/DnDDiceRoll/Theme (issue #38).

Run headless with the editor closed:
  UnrealEditor DnDDiceRoll.uproject -run=pythonscript -script=<repo>/Content/Python/import_dark_fantasy_theme.py -unattended -nullrhi
Existing theme meshes and textures are preserved. The master material (rebuilt in place, so its asset and instances
stay), material instances, texture settings and the data asset are refreshed on every run.

Drowned Vigil world kit r002 (issue #145 phase 2) -- the reviewed reimport path, see theme_plan.py for the table:
  - New r002 assets (new package names) are imported when absent and left alone when present.
  - The four revised r001 packages (world-vigil, prop-pillar, prop-wall, prop-stones) are replaced ONLY when the run is
    told so: DND_THEME_REIMPORT=world-vigil,prop-pillar,prop-wall,prop-stones (or `all`). Any other id is an error.
    Replacing is all or nothing for those four, because the board scales r001 art and places r002 art at true metric.
  - Every r002 mesh gets Nanite switched OFF, no collision, its explicit LODs (<name>_LOD1.glb and _LOD2.glb beside the
    export, for meshes over ~1k triangles; cards, shafts and small scatter are exempt and have none) and trilinear textures. Existing r001 meshes are only reported, unless
    DND_THEME_FIX_R001_NANITE=1 asks for their Nanite flag to be switched off too (that changes their bytes).
  - Two materials are created (never overwritten): M_DiceRollOccluder (dithered opacity mask for the near architecture)
    and M_DiceRollCard (translucent, for mist and light shafts).
  - One THEME_ASSET line per asset, THEME_FILE lines for every Theme file whose SHA-256 changed, and the run saves only
    dirty packages and raises if a save fails.

Root of the World world kit r002 (issue #149 phase 2) goes through the same code path with its own table (theme_plan.ROOT_R002):
  - The Root kit has 25 rows: 24 new packages (SM_World_Root_Occluders, SM_Prop_Root_* variants and arch, SM_Scatter_Root_*,
    SM_Fx_Root_*) and the revised SM_World_Root, whose r001 package is replaced ONLY with DND_THEME_REIMPORT=world-root. Its meshes
    are tagged DiceRollAssetRevision=root-kit-r002 and UDiceRollTheme::bRootMetricKit is set only when SM_World_Root carries it.
  - INSTANCE_TINTS in theme_plan writes the Color tint of a kit instance (the lever for the paving value); it is written only
    when it differs.

Rigged characters (#147): the same run also imports the fighter, wizard and raider as skeletal meshes on one shared
skeleton, with LOD1 and LOD2, the nine clips and Nanite off (see "Rigged characters" below). Existing rig assets are
never overwritten and this script never deletes them (a force delete nulls references other assets hold). A
THEME_RIG_REIMPORT request is validated and then refused: delete the rig assets deliberately in the editor, rerun.
"""
import json
import os
import sys
import unreal

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import theme_plan

# A headless commandlet does not load the mesh editor modules, so their subsystems would be None.
unreal.load_module("SkeletalMeshEditor")
if unreal.get_editor_subsystem(unreal.SkeletalMeshEditorSubsystem) is None:
    raise RuntimeError("SkeletalMeshEditorSubsystem is unavailable even after loading the SkeletalMeshEditor module")

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
THEME = "/Game/DnDDiceRoll/Theme"
MASTER = THEME + "/Materials/M_DiceRollTheme"
OCCLUDER_MATERIAL = "M_DiceRollOccluder"
CARD_MATERIAL = "M_DiceRollCard"
DATA = THEME + "/DA_DiceRollTheme"

# Surface looks (#143). Per mesh: asset id, export name, folder, data-asset property, then
#   roughness  - stone, cloth and ground high; characters and metal lower
#   rim        - Fresnel rim strength; characters only, so dark figures read against dark ground
#   fill       - hemisphere fill strength; strong on characters, none or a trace elsewhere
#   glow       - emissive intensity applied to the vertex-alpha mask (flames, runes, glass); used only when the
#                mesh really has a varying vertex alpha, see has_alpha_mask()
#   self_lit   - a constant emissive share for objects that are themselves a light source (the flame) and for the
#                tabletop overview, which stays as it was. Zero everywhere else: there is no blanket glow any more.
ASSETS = [
    ("hero-fighter", "SM_Hero_Fighter", "Characters", "fighter", 0.55, 1.0, 1.0, 1.0, 0.0),
    ("hero-rogue", "SM_Hero_Rogue", "Characters", "rogue", 0.6, 1.0, 1.0, 1.0, 0.0),
    ("hero-cleric", "SM_Hero_Cleric", "Characters", "cleric", 0.6, 1.0, 1.0, 1.0, 0.0),
    ("hero-wizard", "SM_Hero_Wizard", "Characters", "wizard", 0.65, 1.0, 1.0, 1.0, 0.0),
    ("foe-raider", "SM_Foe_Raider", "Characters", "raider", 0.6, 1.0, 1.0, 1.0, 0.0),
    ("foe-scout", "SM_Foe_Scout", "Characters", "scout", 0.6, 1.0, 1.0, 1.0, 0.0),
    ("foe-archer", "SM_Foe_Archer", "Characters", "archer", 0.6, 1.0, 1.0, 1.0, 0.0),
    ("world-root", "SM_World_Root", "Worlds", "root_world", 0.9, 0.0, 0.0, 1.0, 0.0),
    ("world-vigil", "SM_World_Vigil", "Worlds", "vigil_world", 0.9, 0.0, 0.0, 1.0, 0.0),
    ("tile-root", "SM_Tile_Root", "Worlds", "root_tile", 0.9, 0.0, 0.0, 1.0, 0.0),
    ("tile-vigil", "SM_Tile_Vigil", "Worlds", "vigil_tile", 0.9, 0.0, 0.0, 1.0, 0.0),
    ("prop-stones", "SM_Prop_Stones", "Props", "stones", 0.92, 0.0, 0.05, 1.0, 0.0),
    ("prop-pillar", "SM_Prop_Pillar", "Props", "pillar", 0.9, 0.0, 0.05, 1.0, 0.0),
    ("prop-wall", "SM_Prop_Wall", "Props", "wall", 0.9, 0.0, 0.05, 1.0, 0.0),
    ("prop-brazier", "SM_Prop_Brazier", "Props", "brazier", 0.5, 0.0, 0.05, 1.0, 0.0),
    ("prop-flame", "SM_Prop_Flame", "Props", "flame", 1.0, 0.0, 0.0, 1.0, 1.4),
    ("prop-banner", "SM_Prop_Banner", "Props", "banner", 0.95, 0.0, 0.05, 1.0, 0.0),
    ("prop-cage", "SM_Prop_Cage", "Props", "cage", 0.5, 0.0, 0.05, 1.0, 0.0),
    ("tabletop", "SM_Tabletop", "Overview", "tabletop", 0.8, 0.0, 0.0, 1.0, 0.3),
    ("diorama-root", "SM_Diorama_Root", "Overview", "root_diorama", 0.9, 0.0, 0.0, 1.0, 0.35),
    ("diorama-vigil", "SM_Diorama_Vigil", "Overview", "vigil_diorama", 0.9, 0.0, 0.0, 1.0, 0.35),
]

tools = unreal.AssetToolsHelpers.get_asset_tools()
library = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary


def expression(material, kind, x, y, **props):
    node = mel.create_material_expression(material, kind, x, y)
    for key, value in props.items():
        node.set_editor_property(key, value)
    return node


def rgb(material, source, output, x, y):
    mask = expression(material, unreal.MaterialExpressionComponentMask, x, y, r=True, g=True, b=True, a=False)
    mel.connect_material_expressions(source, output, mask, "")
    return mask


def vector(material, name, value, x, y):
    return expression(material, unreal.MaterialExpressionVectorParameter, x, y, parameter_name=name, default_value=value)


def scalar(material, name, value, x, y):
    return expression(material, unreal.MaterialExpressionScalarParameter, x, y, parameter_name=name, default_value=value)


def mul(material, a, b, x, y, a_out="", b_out=""):
    node = expression(material, unreal.MaterialExpressionMultiply, x, y)
    mel.connect_material_expressions(a, a_out, node, "A")
    mel.connect_material_expressions(b, b_out, node, "B")
    return node


def add(material, a, b, x, y):
    node = expression(material, unreal.MaterialExpressionAdd, x, y)
    mel.connect_material_expressions(a, "", node, "A")
    mel.connect_material_expressions(b, "", node, "B")
    return node


# Bump this whenever the graph in master_material() changes: an existing master is kept only while its stored
# revision equals it, so a structural change (not just a new parameter name) is picked up on the next import.
MASTER_GRAPH_REVISION = "dice-theme-4"
MASTER_REVISION_TAG = "DiceRollGraphRevision"


def master_is_current(material):
    return unreal.EditorAssetLibrary.get_metadata_tag(material, MASTER_REVISION_TAG) == MASTER_GRAPH_REVISION


def set_if_different(obj, prop, value):
    """Sets an editor property only when it differs, so unchanged assets are never dirtied."""
    if obj.get_editor_property(prop) != value:
        obj.set_editor_property(prop, value)
        return True
    return False


def camera_clip(material, x, y):
    """Opacity mask (0 .. 1) that clips scenery closer to the camera than CameraFade x the camera's height (#143).

    A diorama that surrounds the board (r001 Root) would swallow the camera when the HUD-safe fit pulls it back; the radius
    scales with zoom and the board and figures are always farther away. In shadow passes the camera is the light, so clipped
    scenery keeps its shadows. Shared by M_DiceRollTheme and M_DiceRollOccluder (which multiplies it with its sight-line dither).
    """
    world_pos = expression(material, unreal.MaterialExpressionWorldPosition, x, y)
    camera_pos = expression(material, unreal.MaterialExpressionCameraPositionWS, x, y + 100)
    distance = expression(material, unreal.MaterialExpressionDistance, x + 200, y + 50)
    mel.connect_material_expressions(world_pos, "", distance, "A")
    mel.connect_material_expressions(camera_pos, "", distance, "B")
    camera_z = expression(material, unreal.MaterialExpressionComponentMask, x + 200, y + 150, r=False, g=False, b=True, a=False)
    mel.connect_material_expressions(camera_pos, "", camera_z, "")
    fade = scalar(material, "CameraFade", 0.5, x + 200, y + 250)
    radius = mul(material, camera_z, fade, x + 400, y + 200)
    beyond = expression(material, unreal.MaterialExpressionSubtract, x + 550, y + 100)
    mel.connect_material_expressions(distance, "", beyond, "A")
    mel.connect_material_expressions(radius, "", beyond, "B")
    edge = expression(material, unreal.MaterialExpressionMultiply, x + 700, y + 100, const_b=0.02)
    mel.connect_material_expressions(beyond, "", edge, "A")
    opacity = expression(material, unreal.MaterialExpressionSaturate, x + 850, y + 100)
    mel.connect_material_expressions(edge, "", opacity, "")
    return opacity


def shadow_safe(material, mask, x, y):
    """ShadowPassSwitch: the camera-dependent mask in the main pass, opaque (1) in shadow passes, so scenery clipped or
    dithered near the camera still casts its shadow (a shadow depth pass keeps the player camera's origin)."""
    one = expression(material, unreal.MaterialExpressionConstant, x - 150, y + 100, r=1.0)
    switch = expression(material, unreal.MaterialExpressionShadowReplace, x, y)
    mel.connect_material_expressions(mask, "", switch, "Default")
    mel.connect_material_expressions(one, "", switch, "Shadow")
    return switch


def master_material():
    """Builds (or rebuilds in place) M_DiceRollTheme. The asset, its path and every instance of it survive.

    base     = Tex.rgb x vertexColor.rgb x Color
    emissive = base x (vertexColor.a x Glow + SelfLit)           glow only where the vertex alpha says so
             + base x Fill x hemisphere(world normal Z)           sky/ground fill, so shadowed sides keep form
             + RimColor x Rim x fresnel                           cool rim so dark figures read against dark ground
    roughness = Roughness (per instance)
    opacity mask = ShadowPassSwitch(Default: camera clip, Shadow: 1)   the camera clip never removes shadows
    """
    if library.does_asset_exist(MASTER):
        material = unreal.load_asset(MASTER)
        # Leave an existing graph alone when it carries this graph revision, so a rerun never
        # re-saves the master (and every instance) for nothing. Set DND_THEME_REBUILD_MASTER=1 to rebuild it
        # on purpose; otherwise it is rebuilt only when its stored graph revision differs from MASTER_GRAPH_REVISION.
        if os.environ.get("DND_THEME_REBUILD_MASTER") != "1" and master_is_current(material):
            unreal.log("THEME_MASTER kept (graph current)")
            return material
        mel.delete_all_material_expressions(material)
        unreal.log("THEME_MASTER rebuilt")
    else:
        material = tools.create_asset("M_DiceRollTheme", THEME + "/Materials", unreal.Material, unreal.MaterialFactoryNew())
    white = unreal.load_asset("/Engine/EngineResources/WhiteSquareTexture")
    tex = expression(material, unreal.MaterialExpressionTextureSampleParameter2D, -1100, -100, parameter_name="Tex", texture=white)
    vertex = expression(material, unreal.MaterialExpressionVertexColor, -1100, 200)
    tint = vector(material, "Color", unreal.LinearColor(1, 1, 1, 1), -1100, 400)
    lit = mul(material, tex, rgb(material, vertex, "", -900, 200), -700, 0, "RGB", "")
    color = mul(material, lit, rgb(material, tint, "", -900, 400), -500, 100)

    # Glow: vertex alpha is the mask. A mesh without a varying alpha reads as 1 everywhere, so such meshes get Glow 0.
    glow = scalar(material, "Glow", 0.0, -900, 560)
    self_lit = scalar(material, "SelfLit", 0.0, -900, 640)
    masked = expression(material, unreal.MaterialExpressionMultiply, -700, 560)
    mel.connect_material_expressions(vertex, "A", masked, "A")
    mel.connect_material_expressions(glow, "", masked, "B")
    glow_term = mul(material, color, add(material, masked, self_lit, -500, 560), -300, 400)

    # Hemisphere fill: lerp(ground, sky, normalZ * 0.5 + 0.5) times the base colour.
    normal = expression(material, unreal.MaterialExpressionPixelNormalWS, -1100, 800)
    up = expression(material, unreal.MaterialExpressionComponentMask, -950, 800, r=False, g=False, b=True, a=False)
    mel.connect_material_expressions(normal, "", up, "")
    half = expression(material, unreal.MaterialExpressionMultiply, -800, 800, const_b=0.5)
    mel.connect_material_expressions(up, "", half, "A")
    blend = expression(material, unreal.MaterialExpressionAdd, -650, 800, const_b=0.5)
    mel.connect_material_expressions(half, "", blend, "A")
    sky = vector(material, "FillSky", unreal.LinearColor(0.3, 0.38, 0.55, 1), -800, 900)
    ground = vector(material, "FillGround", unreal.LinearColor(0.12, 0.1, 0.09, 1), -800, 1000)
    hemi = expression(material, unreal.MaterialExpressionLinearInterpolate, -450, 900)
    mel.connect_material_expressions(rgb(material, ground, "", -600, 1000), "", hemi, "A")
    mel.connect_material_expressions(rgb(material, sky, "", -600, 900), "", hemi, "B")
    mel.connect_material_expressions(blend, "", hemi, "Alpha")
    fill = scalar(material, "Fill", 0.0, -450, 1100)
    fill_term = mul(material, mul(material, hemi, color, -250, 900), fill, -100, 900)

    # Rim: fresnel (exponent 3, no base reflection) in a cool tint.
    fresnel = expression(material, unreal.MaterialExpressionFresnel, -450, 1250, exponent=3.0, base_reflect_fraction=0.0)
    rim_color = vector(material, "RimColor", unreal.LinearColor(0.45, 0.55, 0.85, 1), -700, 1350)
    rim = scalar(material, "Rim", 0.0, -450, 1350)
    rim_term = mul(material, mul(material, fresnel, rgb(material, rim_color, "", -550, 1350), -250, 1250), rim, -100, 1250)

    emissive = add(material, add(material, glow_term, fill_term, 100, 600), rim_term, 250, 700)
    roughness = scalar(material, "Roughness", 0.85, 100, 500)
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
    mel.connect_material_property(shadow_safe(material, camera_clip(material, -700, 1550), 300, 1650), "", unreal.MaterialProperty.MP_OPACITY_MASK)
    spec = expression(material, unreal.MaterialExpressionConstant, 100, 600, r=0.2)
    mel.connect_material_property(color, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(emissive, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.connect_material_property(roughness, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(spec, "", unreal.MaterialProperty.MP_SPECULAR)
    mel.recompile_material(material)
    unreal.EditorAssetLibrary.set_metadata_tag(material, MASTER_REVISION_TAG, MASTER_GRAPH_REVISION)
    material.modify()
    return material


def ensure_skeletal_usage(material):
    """Skinned meshes need their own shader permutation (#147); without the flag a skeletal mesh draws the default material.

    master_is_current() does not look at this flag, so this runs whether the master was kept or rebuilt. It is written
    with set_if_different and recompiled only when it changed, so a rerun leaves the master untouched.
    """
    if set_if_different(material, "used_with_skeletal_mesh", True):
        mel.recompile_material(material)
        unreal.log("THEME_MASTER used_with_skeletal_mesh set")


def remove_staging(staging):
    """Deletes a staging folder if it is still there. Moving the last asset out already unregisters an emptied folder, and deleting
    a directory that does not exist logs 'Could not find directory ..., deleting '' instead'."""
    if library.does_directory_exist(staging):
        library.delete_directory(staging)


def import_mesh(asset_id, name, folder):
    package = THEME + "/" + folder + "/" + name
    if library.does_asset_exist(package):
        return unreal.load_asset(package), None
    source = os.path.join(ROOT, "Art", asset_id, "r001", name + ".glb")
    if not os.path.isfile(source):
        unreal.log_warning("THEME_MISSING_EXPORT " + source)
        return None, None
    staging = THEME + "/_Import/" + name
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", source)
    task.set_editor_property("destination_path", staging)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("save", False)
    tools.import_asset_tasks([task])
    imported = [unreal.load_asset(path) for path in task.get_editor_property("imported_object_paths")]
    meshes = [asset for asset in imported if isinstance(asset, unreal.StaticMesh)]
    textures = [asset for asset in imported if isinstance(asset, unreal.Texture2D)]
    if len(meshes) != 1:
        raise RuntimeError("Expected one static mesh from " + source + ", got " + str(len(meshes)))
    if not library.rename_asset(meshes[0].get_path_name(), package):
        raise RuntimeError("Unable to move mesh to " + package)
    texture = None
    if textures:
        target = THEME + "/Textures/T_" + name[3:]
        if not library.rename_asset(textures[0].get_path_name(), target):
            raise RuntimeError("Unable to move texture to " + target)
        texture = unreal.load_asset(target)
    remove_staging(staging)
    return unreal.load_asset(package), texture


KIT_TAG_VALUE = theme_plan.VIGIL_TAG
# The world kits this script imports through one code path: name, revision tag, table, metric gate, theme array sizes and the
# data-asset flag the gate sets. Plain data, so nothing here touches the engine before main().
KITS = [
    {"name": "vigil", "tag": theme_plan.VIGIL_TAG, "rows": theme_plan.R002, "gate": theme_plan.METRIC_GATE,
     "sizes": theme_plan.ARRAY_SIZES, "flag": "metric_kit"},
    {"name": "root", "tag": theme_plan.ROOT_TAG, "rows": theme_plan.ROOT_R002, "gate": theme_plan.ROOT_METRIC_GATE,
     "sizes": theme_plan.ROOT_ARRAY_SIZES, "flag": "root_metric_kit"},
]
# A headless commandlet does not load the static mesh editor module, so its subsystem would be None.
unreal.load_module("StaticMeshEditor")
mesh_tools = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
if mesh_tools is None:
    raise RuntimeError("StaticMeshEditorSubsystem is unavailable even after loading the StaticMeshEditor module")


def node(material, kind, x, y, **props):
    return expression(material, kind, x, y, **props)


def binary(material, kind, a, b, x, y, a_out="", b_out="", **props):
    result = node(material, kind, x, y, **props)
    if a is not None:
        mel.connect_material_expressions(a, a_out, result, "A")
    if b is not None:
        mel.connect_material_expressions(b, b_out, result, "B")
    return result


def unary(material, kind, source, x, y, source_out=""):
    result = node(material, kind, x, y)
    mel.connect_material_expressions(source, source_out, result, "")
    return result


def sight_fade(material, world_position, eye, target_name, radius, fade_min, x, y):
    """Opacity (FadeMin .. 1) of a pixel for the sight line from `eye` to the vector parameter `target_name`.

    Mirrors OccluderFade::Opacity in C++: a pixel inside the cylinder around the line between the eye and the target is
    faded; a pixel before the eye or beyond the target (the line parameter outside 0..1) stays opaque.
    """
    target = rgb(material, vector(material, target_name, unreal.LinearColor(0, 0, 0, 0), x, y), "", x + 150, y)
    ray = binary(material, unreal.MaterialExpressionSubtract, target, eye, x + 300, y)
    rel = binary(material, unreal.MaterialExpressionSubtract, world_position, eye, x + 300, y + 120)
    ray_len = binary(material, unreal.MaterialExpressionDotProduct, ray, ray, x + 450, y)
    denominator = binary(material, unreal.MaterialExpressionMax, ray_len, None, x + 600, y, const_b=1.0)
    along = binary(material, unreal.MaterialExpressionDotProduct, rel, ray, x + 450, y + 120)
    t = binary(material, unreal.MaterialExpressionDivide, along, denominator, x + 750, y + 60)
    clamped = unary(material, unreal.MaterialExpressionSaturate, t, x + 900, y + 60)
    closest = binary(material, unreal.MaterialExpressionAdd, eye, binary(material, unreal.MaterialExpressionMultiply, ray, clamped, x + 900, y + 200), x + 1050, y + 200)
    distance = binary(material, unreal.MaterialExpressionDistance, world_position, closest, x + 1200, y + 200)
    inner = binary(material, unreal.MaterialExpressionMultiply, radius, None, x + 1200, y + 320, const_b=0.5)
    ramp = unary(material, unreal.MaterialExpressionSaturate,
                 binary(material, unreal.MaterialExpressionSubtract, binary(material, unreal.MaterialExpressionDivide, distance, inner, x + 1350, y + 260), None, x + 1500, y + 260, const_b=1.0),
                 x + 1650, y + 260)
    opacity = node(material, unreal.MaterialExpressionLinearInterpolate, x + 1800, y + 260, const_b=1.0)
    mel.connect_material_expressions(fade_min, "", opacity, "A")
    mel.connect_material_expressions(ramp, "", opacity, "Alpha")
    in_range = unary(material, unreal.MaterialExpressionOneMinus,
                     unary(material, unreal.MaterialExpressionCeil,
                           unary(material, unreal.MaterialExpressionAbs, binary(material, unreal.MaterialExpressionSubtract, t, clamped, x + 1050, y + 400), x + 1200, y + 400),
                           x + 1350, y + 400), x + 1500, y + 400)
    result = node(material, unreal.MaterialExpressionLinearInterpolate, x + 1950, y + 300, const_a=1.0)
    mel.connect_material_expressions(opacity, "", result, "B")
    mel.connect_material_expressions(in_range, "", result, "Alpha")
    return result


def board_fade(material, world_position, eye, fade_min, x, y):
    """Opacity (FadeMin .. 1) of a pixel for the board test: does it cover the board? Mirrors OccluderFade::Board in C++.

    The ray from the camera through the pixel is intersected with the board-top plane (BoardZ). When the hit point lies in
    the board rectangle (BoardRect = min X, min Y, max X, max Y, already grown by the margin) and the pixel is closer to the
    camera than the hit (t > 1), it is faded; outside the rectangle it eases back to opaque over BoardRamp cm. A pixel at or
    above the camera's height, or at or below the plane, is opaque. The default rectangle is empty and so far away that everything stays opaque.
    """
    def pick(source, r, g, b, a, px, py, output=""):
        mask = node(material, unreal.MaterialExpressionComponentMask, px, py, r=r, g=g, b=b, a=a)
        mel.connect_material_expressions(source, output, mask, "")
        return mask

    rect = vector(material, "BoardRect", unreal.LinearColor(1e7, 1e7, -1e7, -1e7), x, y)
    plane = scalar(material, "BoardZ", 0.0, x, y + 150)
    ramp_cm = scalar(material, "BoardRamp", 75.0, x, y + 250)
    ray = binary(material, unreal.MaterialExpressionSubtract, world_position, eye, x + 150, y + 350)
    ray_z = pick(ray, False, False, True, False, x + 300, y + 350)
    eye_z = pick(eye, False, False, True, False, x + 300, y + 450)
    # 1 when the ray points down (Ray.Z < 0), else 0; the divisor is clamped so a level ray never divides by zero.
    downward = unary(material, unreal.MaterialExpressionCeil,
                     unary(material, unreal.MaterialExpressionSaturate,
                           binary(material, unreal.MaterialExpressionMultiply, ray_z, None, x + 450, y + 350, const_b=-1.0), x + 600, y + 350),
                     x + 750, y + 350)
    safe_z = binary(material, unreal.MaterialExpressionMin, ray_z, None, x + 450, y + 450, const_b=-0.001)
    t = binary(material, unreal.MaterialExpressionDivide, binary(material, unreal.MaterialExpressionSubtract, plane, eye_z, x + 450, y + 550), safe_z, x + 750, y + 500)
    # 1 when the pixel is before the plane hit (t > 1), else 0.
    before = unary(material, unreal.MaterialExpressionCeil,
                   unary(material, unreal.MaterialExpressionSaturate,
                         binary(material, unreal.MaterialExpressionSubtract, t, None, x + 900, y + 500, const_b=1.0), x + 1050, y + 500),
                   x + 1200, y + 500)
    valid = binary(material, unreal.MaterialExpressionMultiply, downward, before, x + 1350, y + 420)
    hit = binary(material, unreal.MaterialExpressionAdd, eye, binary(material, unreal.MaterialExpressionMultiply, ray, t, x + 900, y + 650), x + 1050, y + 650)
    hit_xy = pick(hit, True, True, False, False, x + 1200, y + 650)
    # A vector parameter's default output is RGB; BoardRect packs four numbers, so read its RGBA output.
    low = pick(rect, True, True, False, False, x + 300, y, "RGBA")
    high = pick(rect, False, False, True, True, x + 300, y + 100, "RGBA")
    outside_low = binary(material, unreal.MaterialExpressionSubtract, low, hit_xy, x + 1350, y + 0)
    outside_high = binary(material, unreal.MaterialExpressionSubtract, hit_xy, high, x + 1350, y + 150)
    outside_xy = binary(material, unreal.MaterialExpressionMax, outside_low, outside_high, x + 1500, y + 80)
    outside = binary(material, unreal.MaterialExpressionMax, pick(outside_xy, True, False, False, False, x + 1650, y + 40),
                     pick(outside_xy, False, True, False, False, x + 1650, y + 140), x + 1800, y + 80)
    ease = unary(material, unreal.MaterialExpressionSaturate, binary(material, unreal.MaterialExpressionDivide, outside, ramp_cm, x + 1950, y + 80), x + 2100, y + 80)
    opacity = node(material, unreal.MaterialExpressionLinearInterpolate, x + 2250, y + 80, const_b=1.0)
    mel.connect_material_expressions(fade_min, "", opacity, "A")
    mel.connect_material_expressions(ease, "", opacity, "Alpha")
    result = node(material, unreal.MaterialExpressionLinearInterpolate, x + 2400, y + 300, const_a=1.0)
    mel.connect_material_expressions(opacity, "", result, "B")
    mel.connect_material_expressions(valid, "", result, "Alpha")
    return result


OCCLUDER_SCALARS = {"Glow", "Roughness", "CameraFade", "FadeRadius", "FadeMin", "BoardZ", "BoardRamp"}
OCCLUDER_VECTORS = {"Color", "SightEye", "SightFocus", "SightUnit", "BoardRect"}


# Bump when build_occluder_material changes; the stored tag decides whether an existing asset is rebuilt.
OCCLUDER_GRAPH_REVISION = "dice-occluder-3"


def occluder_is_current(material):
    return library.get_metadata_tag(material, MASTER_REVISION_TAG) == OCCLUDER_GRAPH_REVISION


def build_occluder_material(material):
    """Dithered opacity mask for the near architecture (#145): base look of the world, faded where it covers the board.

    Parameters set at run time by AExpeditionBattleView::UpdateOccluder: BoardRect (world XY min and max, grown by half a cell),
    BoardZ and BoardRamp (cm) for the board test, SightEye, SightFocus, SightUnit (world cm), FadeRadius (cm) and FadeMin for
    the sight-line test, plus CameraFade (the master's camera clip, #143). Defaults leave every pixel opaque away from the camera. The mask compares the faded opacity with an
    interleaved-gradient noise of the pixel position, so the cut-out is a dither, never a hard edge or a hide.
    """
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
    material.set_editor_property("opacity_mask_clip_value", 0.3333)
    white = unreal.load_asset("/Engine/EngineResources/WhiteSquareTexture")
    tex = node(material, unreal.MaterialExpressionTextureSampleParameter2D, -1100, -100, parameter_name="Tex", texture=white)
    vertex = node(material, unreal.MaterialExpressionVertexColor, -1100, 200)
    tint = vector(material, "Color", unreal.LinearColor(1, 1, 1, 1), -1100, 400)
    lit = mul(material, tex, rgb(material, vertex, "", -900, 200), -700, 0, "RGB", "")
    color = mul(material, lit, rgb(material, tint, "", -900, 400), -500, 100)
    glow = scalar(material, "Glow", 0.0, -900, 560)
    masked = node(material, unreal.MaterialExpressionMultiply, -700, 560)
    mel.connect_material_expressions(vertex, "A", masked, "A")
    mel.connect_material_expressions(glow, "", masked, "B")
    emissive = mul(material, color, masked, -300, 400)
    roughness = scalar(material, "Roughness", 0.9, 100, 500)
    mel.connect_material_property(color, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(emissive, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.connect_material_property(roughness, "", unreal.MaterialProperty.MP_ROUGHNESS)

    world_position = node(material, unreal.MaterialExpressionWorldPosition, -1100, 1000)
    eye = rgb(material, vector(material, "SightEye", unreal.LinearColor(0, 0, 0, 0), -1100, 1200), "", -950, 1200)
    radius = scalar(material, "FadeRadius", 300.0, -1100, 1400)
    fade_min = scalar(material, "FadeMin", 0.15, -1100, 1500)
    focus = sight_fade(material, world_position, eye, "SightFocus", radius, fade_min, -800, 1000)
    unit = sight_fade(material, world_position, eye, "SightUnit", radius, fade_min, -800, 1800)
    sight = binary(material, unreal.MaterialExpressionMin, focus, unit, 1300, 1400)
    # The board test is the main fade; the sight lines stay for a figure at the board's far edge (see OccluderFade.h).
    board = board_fade(material, world_position, eye, fade_min, -800, 2600)
    faded = binary(material, unreal.MaterialExpressionMin, sight, board, 1400, 1500)
    # Interleaved gradient noise of the pixel position: frac(52.98 * frac(dot(pixel, (0.0671, 0.00584)))).
    screen = node(material, unreal.MaterialExpressionScreenPosition, 300, 2200)
    size = node(material, unreal.MaterialExpressionViewProperty, 300, 2350)
    size.set_editor_property("property", unreal.MaterialExposedViewProperty.MEVP_VIEW_SIZE)
    pixel = mul(material, screen, size, 500, 2250)
    weights = node(material, unreal.MaterialExpressionConstant2Vector, 500, 2400, r=0.06711056, g=0.00583715)
    spread = unary(material, unreal.MaterialExpressionFrac, binary(material, unreal.MaterialExpressionDotProduct, pixel, weights, 700, 2300), 850, 2300)
    noise = unary(material, unreal.MaterialExpressionFrac, binary(material, unreal.MaterialExpressionMultiply, spread, None, 1000, 2300, const_b=52.9829189), 1150, 2300)
    keep = unary(material, unreal.MaterialExpressionSaturate,
                 binary(material, unreal.MaterialExpressionAdd,
                        binary(material, unreal.MaterialExpressionMultiply, binary(material, unreal.MaterialExpressionSubtract, faded, noise, 1450, 2000), None, 1600, 2000, const_b=1000.0),
                        None, 1750, 2000, const_b=0.5), 1900, 2000)
    # Both fades apply, neither overrides the other: a pixel is kept only if the sight-line dither keeps it AND it is outside
    # the camera clip (#143), so geometry around the camera never swallows it and what blocks the view dithers.
    clipped = binary(material, unreal.MaterialExpressionMin, keep, camera_clip(material, 300, 2600), 2100, 2000)
    mel.connect_material_property(shadow_safe(material, clipped, 2300, 2000), "", unreal.MaterialProperty.MP_OPACITY_MASK)


def build_card_material(material):
    """Translucent, unlit, two-sided card for mist and light shafts: Tex x COLOR_0.rgb x Color, opacity COLOR_0.a x Opacity."""
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    material.set_editor_property("two_sided", True)
    white = unreal.load_asset("/Engine/EngineResources/WhiteSquareTexture")
    tex = node(material, unreal.MaterialExpressionTextureSampleParameter2D, -900, -100, parameter_name="Tex", texture=white)
    vertex = node(material, unreal.MaterialExpressionVertexColor, -900, 200)
    tint = vector(material, "Color", unreal.LinearColor(1, 1, 1, 1), -900, 400)
    lit = mul(material, tex, rgb(material, vertex, "", -700, 200), -500, 0, "RGB", "")
    color = mul(material, lit, rgb(material, tint, "", -700, 400), -300, 100)
    opacity_scale = scalar(material, "Opacity", 1.0, -700, 560)
    opacity = node(material, unreal.MaterialExpressionMultiply, -300, 560)
    mel.connect_material_expressions(vertex, "A", opacity, "A")
    mel.connect_material_expressions(opacity_scale, "", opacity, "B")
    mel.connect_material_property(color, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.connect_material_property(opacity, "", unreal.MaterialProperty.MP_OPACITY)


FIGURE_SILHOUETTE_MATERIAL = "M_FigureSilhouette"
# Bump when build_figure_silhouette_material changes.
FIGURE_SILHOUETTE_REVISION = "figure-silhouette-3"


def figure_silhouette_is_current(material):
    return library.get_metadata_tag(material, MASTER_REVISION_TAG) == FIGURE_SILHOUETTE_REVISION


def build_figure_silhouette_material(material):
    """Post-process material that draws a hidden figure's silhouette in its side's colour (#145 review, tall kit obstacles).

    Figures render into custom depth with stencil 1 (party) or 2 (enemy). Where CustomDepth is farther from the camera than SceneDepth by
    more than `Tolerance` cm the figure is behind some other geometry, and the pixel is mixed toward the side colour with that side's
    alpha (below).
    Where the figure is the nearest thing (custom depth equals scene depth) or no figure is there (stencil 0) the pixel is unchanged.

    Blendable Location is After Tonemapping: the fill is then the exact HUD colour, untouched by auto-exposure, bloom, vignette and
    the grade (before the tonemapper it would also need the pre-exposure scale). Inputs and output are in display (sRGB) space there,
    so the linear HUD palette (`PartyColor` = Sky 0.4/0.85/1.0, `EnemyColor` = Danger 1.0/0.4/0.3) is encoded with pow(c, 1/2.2).
    Side reads without colour: party (stencil 1) is a solid, brighter fill (PartyAlpha 0.7, lifted toward white by PartyLift);
    enemy (stencil 2) is diagonal hatching, screen-space stripes of (x + y) with a StripePx period of 6 px at 1080p scaled by the
    view height, alternating EnemyStripeAlpha (0.7) with a faint EnemyGapAlpha (0.15). In greyscale a solid blob and a striped
    one differ by pattern and by luminance.
    Needs r.CustomDepth=3 (Config/DefaultEngine.ini); the stencil read is zero otherwise and the material then does nothing.
    """
    material.set_editor_property("material_domain", unreal.MaterialDomain.MD_POST_PROCESS)
    material.set_editor_property("blendable_location", unreal.BlendableLocation.BL_SCENE_COLOR_AFTER_TONEMAPPING)
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_OPAQUE)

    def scene(texture_id, x, y):
        return node(material, unreal.MaterialExpressionSceneTexture, x, y, scene_texture_id=texture_id)

    def red(source, x, y):
        mask = node(material, unreal.MaterialExpressionComponentMask, x, y, r=True, g=False, b=False, a=False)
        mel.connect_material_expressions(source, "", mask, "")
        return mask

    custom_depth = red(scene(unreal.SceneTextureId.PPI_CUSTOM_DEPTH, -1400, 0), -1150, 0)
    scene_depth = red(scene(unreal.SceneTextureId.PPI_SCENE_DEPTH, -1400, 200), -1150, 200)
    stencil = red(scene(unreal.SceneTextureId.PPI_CUSTOM_STENCIL, -1400, 400), -1150, 400)
    input_color = scene(unreal.SceneTextureId.PPI_POST_PROCESS_INPUT0, -1400, 700)

    tolerance = scalar(material, "Tolerance", 3.0, -1150, 560)
    slope = scalar(material, "Slope", 0.5, -1150, 640)
    # A hidden figure is FARTHER than the geometry covering it: its custom depth exceeds the scene depth there.
    gap = binary(material, unreal.MaterialExpressionSubtract, custom_depth, scene_depth, -900, 100)
    over = binary(material, unreal.MaterialExpressionSubtract, gap, tolerance, -700, 100)
    hidden = unary(material, unreal.MaterialExpressionSaturate, binary(material, unreal.MaterialExpressionMultiply, over, slope, -500, 100), -300, 100)
    # Stencil 0 is "no figure"; 1 is party, 2 is enemy.
    is_figure = unary(material, unreal.MaterialExpressionSaturate, stencil, -900, 400)
    is_enemy = unary(material, unreal.MaterialExpressionSaturate, binary(material, unreal.MaterialExpressionSubtract, stencil, None, -1000, 500, const_b=1.0), -700, 500)

    def encoded(name, value, y):
        parameter = rgb(material, vector(material, name, value, -1150, y), "", -950, y)
        gamma = node(material, unreal.MaterialExpressionPower, -750, y, const_exponent=1.0 / 2.2)
        mel.connect_material_expressions(parameter, "", gamma, "Base")
        return gamma

    party = encoded("PartyColor", unreal.LinearColor(0.4, 0.85, 1.0, 1), 900)
    enemy = encoded("EnemyColor", unreal.LinearColor(1.0, 0.4, 0.3, 1), 1050)
    lift = scalar(material, "PartyLift", 0.2, -750, 800)
    bright = node(material, unreal.MaterialExpressionLinearInterpolate, -600, 900, const_b=1.0)
    mel.connect_material_expressions(party, "", bright, "A")
    mel.connect_material_expressions(lift, "", bright, "Alpha")
    side = node(material, unreal.MaterialExpressionLinearInterpolate, -450, 950)
    mel.connect_material_expressions(bright, "", side, "A")
    mel.connect_material_expressions(enemy, "", side, "B")
    mel.connect_material_expressions(is_enemy, "", side, "Alpha")
    # Hatch: stripes along (x + y) in pixels. ViewportUV x ViewSize gives pixels; the period is StripePx at 1080p, so it scales with the view height.
    uv = node(material, unreal.MaterialExpressionScreenPosition, -1400, 1300)
    size = node(material, unreal.MaterialExpressionViewSize, -1400, 1450)
    pixels = binary(material, unreal.MaterialExpressionMultiply, uv, size, -1150, 1350, a_out="ViewportUV")

    def channel(source, r, g, x, y):
        mask = node(material, unreal.MaterialExpressionComponentMask, x, y, r=r, g=g, b=False, a=False)
        mel.connect_material_expressions(source, "", mask, "")
        return mask

    x_plus_y = binary(material, unreal.MaterialExpressionAdd, channel(pixels, True, False, -950, 1300), channel(pixels, False, True, -950, 1400), -750, 1350)
    stripe_px = scalar(material, "StripePx", 6.0, -1150, 1550)
    height_scale = binary(material, unreal.MaterialExpressionDivide, channel(size, False, True, -1150, 1650), None, -950, 1600, const_b=1080.0)
    period = binary(material, unreal.MaterialExpressionMultiply, stripe_px, height_scale, -550, 1500)
    phase = unary(material, unreal.MaterialExpressionFrac, binary(material, unreal.MaterialExpressionDivide, x_plus_y, period, -350, 1400), -150, 1400)
    stripe = unary(material, unreal.MaterialExpressionCeil, unary(material, unreal.MaterialExpressionSaturate,
                   binary(material, unreal.MaterialExpressionSubtract, phase, None, -50, 1400, const_b=0.5), 100, 1400), 250, 1400)
    party_alpha = scalar(material, "PartyAlpha", 0.7, -450, 1150)
    gap_alpha = scalar(material, "EnemyGapAlpha", 0.15, -450, 1250)
    stripe_alpha = scalar(material, "EnemyStripeAlpha", 0.7, -450, 1350)
    enemy_alpha = node(material, unreal.MaterialExpressionLinearInterpolate, 400, 1300)
    mel.connect_material_expressions(gap_alpha, "", enemy_alpha, "A")
    mel.connect_material_expressions(stripe_alpha, "", enemy_alpha, "B")
    mel.connect_material_expressions(stripe, "", enemy_alpha, "Alpha")
    side_alpha = node(material, unreal.MaterialExpressionLinearInterpolate, 550, 1200)
    mel.connect_material_expressions(party_alpha, "", side_alpha, "A")
    mel.connect_material_expressions(enemy_alpha, "", side_alpha, "B")
    mel.connect_material_expressions(is_enemy, "", side_alpha, "Alpha")
    alpha = binary(material, unreal.MaterialExpressionMultiply, binary(material, unreal.MaterialExpressionMultiply, hidden, is_figure, -100, 300), side_alpha, 100, 300)
    result = node(material, unreal.MaterialExpressionLinearInterpolate, 300, 500)
    mel.connect_material_expressions(rgb(material, input_color, "", -100, 700), "", result, "A")
    mel.connect_material_expressions(side, "", result, "B")
    mel.connect_material_expressions(alpha, "", result, "Alpha")
    mel.connect_material_property(result, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)


def ensure_material(name, build, is_current=None, revision=None):
    """Creates a material once. An existing asset is kept; when `is_current` says its graph lacks this script's parameters
    it is rebuilt in place, so the asset, its path and every instance of it survive."""
    path = THEME + "/Materials/" + name
    if library.does_asset_exist(path):
        material = unreal.load_asset(path)
        if is_current is None or is_current(material):
            unreal.log("THEME_MATERIAL_KEPT " + path)
            return material
        mel.delete_all_material_expressions(material)
        build(material)
        mel.recompile_material(material)
        if revision:
            library.set_metadata_tag(material, MASTER_REVISION_TAG, revision)
            material.modify()
        unreal.log("THEME_MATERIAL_REBUILT " + path)
        return material
    material = tools.create_asset(name, THEME + "/Materials", unreal.Material, unreal.MaterialFactoryNew())
    build(material)
    mel.recompile_material(material)
    if revision:
        library.set_metadata_tag(material, MASTER_REVISION_TAG, revision)
    unreal.log("THEME_MATERIAL_CREATED " + path)
    return material


def r001_source(asset_id, name):
    return os.path.join(ROOT, "Art", asset_id, "r001", name + ".glb")


def kit_tag(mesh):
    """The kit revision tag a mesh carries (vigil-kit-r002 or root-kit-r002), or None for an r001 mesh."""
    if mesh is None:
        return None
    tag = library.get_metadata_tag(mesh, theme_plan.REVISION_TAG)
    return tag if tag in theme_plan.KIT_TAGS else None


def kit_tagged(mesh, tag=None):
    """True when the mesh carries a kit tag: this one when `tag` is given, either kit's otherwise."""
    found = kit_tag(mesh)
    return found is not None and (tag is None or found == tag)


def theme_hashes():
    """SHA-256 of every packaged Theme file on disk, keyed by repository-relative path."""
    base = os.path.join(ROOT, "Content", "DnDDiceRoll", "Theme")
    hashes = {}
    for folder, _, names in os.walk(base):
        for filename in names:
            if filename.endswith((".uasset", ".uexp", ".ubulk")):
                full = os.path.join(folder, filename)
                hashes[os.path.relpath(full, ROOT).replace(os.sep, "/")] = theme_plan.sha256_file(full)
    return hashes


def nanite_enabled(mesh):
    return bool(mesh.get_editor_property("nanite_settings").get_editor_property("enabled"))


def force_nanite_off(mesh):
    """UE's glTF importer enables Nanite silently; themed meshes are small and need explicit LODs, so it is switched off."""
    if not nanite_enabled(mesh):
        return False
    settings = mesh.get_editor_property("nanite_settings")
    settings.set_editor_property("enabled", False)
    mesh_tools.set_nanite_settings(mesh, settings, True)
    if nanite_enabled(mesh):
        raise RuntimeError("Nanite stayed enabled on " + mesh.get_path_name())
    return True


def collision_shapes(mesh):
    """Simple collision shapes on a mesh, or None when the property layout cannot be read."""
    try:
        body = mesh.get_editor_property("body_setup")
        if body is None:
            return 0
        geometry = body.get_editor_property("agg_geom")
        return sum(len(geometry.get_editor_property(field)) for field in ("convex_elems", "sphere_elems", "box_elems", "sphyl_elems", "tapered_capsule_elems"))
    except Exception as error:  # reported in the log line; the lead checks the asset in the editor
        unreal.log("THEME_COLLISION_UNREADABLE {} {}".format(mesh.get_path_name(), error))
        return None


def import_lods(mesh, asset_id, revision, name):
    """Imports the explicit LODs (<name>_LOD1.glb, <name>_LOD2.glb) beside the export. The asset record decides which are expected:
    meshes over ~1k triangles have two, cards, shafts and small scatter none. The files must match the record's hashes and the
    mesh must end with exactly 1 + that many LODs."""
    expected = theme_plan.record_lods(asset_id, revision)
    exports = theme_plan.lod_exports(asset_id, revision, name)
    if [os.path.relpath(path, ROOT).replace(os.sep, "/") for path in exports] != [entry[1] for entry in expected]:
        raise RuntimeError("LOD exports beside {} do not match its asset record: {} vs {}".format(name, exports, expected))
    for (index, _, digest), path in zip(expected, exports):
        if theme_plan.sha256_file(path) != digest:
            raise RuntimeError("LOD{} of {} differs from its asset record; rerun write_records_r002.py (write_records_root_r002.py for the Root)".format(index, name))
        result = mesh_tools.import_lod(mesh, index, path)
        if result < 0:
            raise RuntimeError("Unable to import LOD {} of {} from {}".format(index, name, path))
        unreal.log("THEME_LOD {} lod={} tris={} source_sha256={}".format(name, index, theme_plan.triangle_count(path), theme_plan.sha256_file(path)))
    check_lod_count(mesh, asset_id, revision)
    return len(exports)


def check_lod_count(mesh, asset_id, revision):
    wanted = 1 + len(theme_plan.record_lods(asset_id, revision))
    if mesh_tools.get_lod_count(mesh) != wanted:
        raise RuntimeError("{} has {} LODs, its asset record expects {}".format(mesh.get_path_name(), mesh_tools.get_lod_count(mesh), wanted))


def lod_slots(mesh):
    """The material slot each section of each LOD uses: [[slot, ...] for lod 0, [..] for lod 1, ...]."""
    return [[mesh_tools.get_lod_material_slot(mesh, lod, section) for section in range(mesh.get_num_sections(lod))]
            for lod in range(mesh_tools.get_lod_count(mesh))]


def repair_lod_slots(mesh, name):
    """Maps every section of every LOD to material slot 0 (the one MI). The extra slots the LOD imports added are kept, unused.

    Each LOD glb names its own material (LOD1_SM_X ...), so import_lod appends a slot per LOD and its sections point there; only
    LOD0 got the instance, and LOD1 and LOD2 rendered with the default grey. A kit mesh has one atlas and one instance, so every
    section belongs in slot 0. Slots are never trimmed: trimming them preceded a startup crash in the first native run (#163), so
    only the section mapping changes. Idempotent: a mesh whose sections already all use slot 0 is not touched, so a plain run
    repairs wrong existing meshes in place (a modify, not a reimport) and leaves right ones byte-identical.
    """
    before = lod_slots(mesh)
    fixed = 0
    for lod, sections in enumerate(before):
        for section, slot in enumerate(sections):
            if slot != 0:
                mesh_tools.set_lod_material_slot(mesh, 0, lod, section)
                fixed += 1
    after = lod_slots(mesh)
    if any(slot != 0 for sections in after for slot in sections):
        raise RuntimeError("{} still has LOD sections outside material slot 0: {}".format(name, after))
    # The extra material slots are left in place. Writing static_materials directly drops slots that the built
    # render data still references (the game then crashed in mesh draw-command setup); unused slots are harmless.
    trimmed = False
    if fixed or trimmed:
        mesh.modify()
    unreal.log("THEME_LOD_SLOTS {} before={} after={} sections_fixed={} slots={} trimmed={}".format(
        name, before, after, fixed, len(list(mesh.get_editor_property("static_materials"))), trimmed))
    return fixed or trimmed


def stage_import(source, name):
    staging = THEME + "/_Import/" + name
    if library.does_directory_exist(staging):
        library.delete_directory(staging)
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", source)
    task.set_editor_property("destination_path", staging)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("save", False)
    tools.import_asset_tasks([task])
    imported = [unreal.load_asset(path) for path in task.get_editor_property("imported_object_paths")]
    meshes = [asset for asset in imported if isinstance(asset, unreal.StaticMesh)]
    textures = [asset for asset in imported if isinstance(asset, unreal.Texture2D)]
    if len(meshes) != 1:
        raise RuntimeError("Expected one static mesh from " + source + ", got " + str(len(meshes)))
    if len(textures) != 1:
        raise RuntimeError("Expected one atlas texture from " + source + ", got " + str(len(textures)))
    return meshes[0], textures[0], staging


def import_kit_asset(asset_id, revision, name, folder, replace):
    """Imports one r002 export to its package. Replaces an existing package only when `replace` (allow-listed and requested)."""
    source = theme_plan.export_path(asset_id, revision, name)
    if not os.path.isfile(source):
        raise RuntimeError("Missing r002 export " + source)
    record_path = os.path.join(os.path.dirname(source), "asset.json")
    with open(record_path) as handle:
        record = json.load(handle)
    # Records list every export (LOD0 and any authored LODs); check the LOD0 file this import reads.
    export = [entry for entry in record["files"] if entry["role"] == "export" and os.path.basename(entry["path"]) == os.path.basename(source)]
    if record["unreal_package"] != theme_plan.package_path(folder, name) or len(export) != 1 or export[0]["sha256"] != theme_plan.sha256_file(source):
        raise RuntimeError("Asset record " + record_path + " does not match the export or the package; rerun write_records_r002.py (write_records_root_r002.py for the Root)")
    package = theme_plan.package_path(folder, name)
    target_texture = THEME + "/Textures/T_" + name[3:]
    if library.does_asset_exist(package):
        if not replace:
            raise RuntimeError("Refusing to overwrite " + package)
        return reimport_in_place(source, package, target_texture, name)
    if library.does_asset_exist(target_texture):
        raise RuntimeError("A texture already exists at " + target_texture + " but the mesh does not; remove it deliberately")
    mesh, texture, staging = stage_import(source, name)
    if not library.rename_asset(mesh.get_path_name(), package):
        raise RuntimeError("Unable to move mesh to " + package)
    if not library.rename_asset(texture.get_path_name(), target_texture):
        raise RuntimeError("Unable to move texture to " + target_texture)
    remove_staging(staging)
    return unreal.load_asset(package), unreal.load_asset(target_texture)


def import_over(filename, package, name):
    """Imports `filename` over the existing asset at `package` and returns that same object.

    destination_path is the asset's own folder and destination_name its name, with replace_existing=True. Interchange then turns the
    import into a reimport of the asset already there (UInterchangeManager::ImportAssetAsync, bConvertToReimport): the UObject, its
    package and every reference to it survive, which a delete-and-recreate (ForceDeleteObjects nulls references in loaded assets) does
    not. In a reimport only the factory node that matches the existing object is applied (FTaskImportObject_GameThread skips the rest),
    so a mesh file reimports the mesh alone and nothing else is created. Anything else that comes back is an error.
    """
    before = unreal.load_asset(package)
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", filename)
    task.set_editor_property("destination_path", package.rsplit("/", 1)[0])
    task.set_editor_property("destination_name", name)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("save", False)
    tools.import_asset_tasks([task])
    paths = [str(path) for path in task.get_editor_property("imported_object_paths")]
    # A failed import leaves the original object loaded, so identity alone proves nothing: the task must report the package.
    theme_plan.check_import_result(paths, package, filename)
    after = unreal.load_asset(package)
    if after is None or after != before:
        raise RuntimeError("Reimporting {} did not keep the asset at {} (got {})".format(filename, package, after))
    return after


def reimport_in_place(source, package, target_texture, name):
    """Replaces a revised r001 package by reimporting its r002 export in place: the mesh from the glb, the atlas from the PNG
    embedded in it (the glb's own texture node is not applied by a reimport). Both keep their object identity."""
    mesh = import_over(source, package, name)
    header, binary = theme_plan.read_glb(source)
    images = header.get("images", [])
    if len(images) != 1 or "bufferView" not in images[0]:
        raise RuntimeError(source + " must embed exactly one atlas image")
    view = header["bufferViews"][images[0]["bufferView"]]
    start = view.get("byteOffset", 0)
    scratch = os.path.join(ROOT, "Saved", "ThemeImport")
    os.makedirs(scratch, exist_ok=True)
    atlas = os.path.join(scratch, "T_" + name[3:] + ".png")
    with open(atlas, "wb") as handle:
        handle.write(binary[start:start + view["byteLength"]])
    if library.does_asset_exist(target_texture):
        texture = import_over(atlas, target_texture, "T_" + name[3:])
    else:
        folder = target_texture.rsplit("/", 1)[0]
        task = unreal.AssetImportTask()
        task.set_editor_property("filename", atlas)
        task.set_editor_property("destination_path", folder)
        task.set_editor_property("destination_name", "T_" + name[3:])
        task.set_editor_property("automated", True)
        task.set_editor_property("replace_existing", True)
        task.set_editor_property("save", False)
        tools.import_asset_tasks([task])
        texture = unreal.load_asset(target_texture)
        if texture is None:
            raise RuntimeError("Unable to import the atlas to " + target_texture)
    return mesh, texture


def set_texture(texture, revision):
    # Trilinear + anisotropic 8 (gate 1). TF_TRILINEAR maps straight to a plain trilinear sampler, which never uses
    # r.MaxAnisotropy; TF_DEFAULT defers to the texture group, and Config/DefaultDeviceProfiles.ini makes TEXTUREGROUP_World
    # MinMagFilter=aniso + MipFilter=linear, the anisotropic-linear sampler. Mips stop shimmer at distance.
    # r001 stays uncompressed (as imported in #38/#143); r002 uses the default sRGB compression its records state.
    compression = unreal.TextureCompressionSettings.TC_DEFAULT if revision >= 2 else unreal.TextureCompressionSettings.TC_EDITOR_ICON
    for prop, value in (("filter", unreal.TextureFilter.TF_DEFAULT), ("compression_settings", compression), ("srgb", True),
                        ("lod_group", unreal.TextureGroup.TEXTUREGROUP_WORLD), ("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_SIMPLE_AVERAGE)):
        if texture.get_editor_property(prop) != value:
            texture.set_editor_property(prop, value)


def material_instance(mesh_name, instance_name, texture_name, texture, asset_id, look, parents, kind=theme_plan.LIT, revision=1, source=None, tint=None):
    """Creates or refreshes one MI_* child of the matching parent for a mesh and returns it (shared by static and skeletal meshes).

    `tint` is an optional (r, g, b) written to the instance's `Color` parameter (the master's and the occluder's tint); None leaves it alone."""
    roughness, rim, fill, glow, self_lit = look
    path = THEME + "/Materials/" + instance_name
    instance = unreal.load_asset(path) if library.does_asset_exist(path) else tools.create_asset(
        instance_name, THEME + "/Materials", unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    changed = False
    if instance.get_editor_property("parent") != parents[kind]:
        mel.set_material_instance_parent(instance, parents[kind])
        changed = True
    if texture is None:
        existing = THEME + "/Textures/" + texture_name
        texture = unreal.load_asset(existing) if library.does_asset_exist(existing) else None
    if texture is not None:
        set_texture(texture, revision)
        if mel.get_material_instance_texture_parameter_value(instance, "Tex") != texture:
            mel.set_material_instance_texture_parameter_value(instance, "Tex", texture)
            changed = True
    # The glow mask guard: without a varying vertex alpha glow stays 0.
    masked = theme_plan.has_alpha_mask(source or r001_source(asset_id, mesh_name))
    if kind == theme_plan.LIT:
        scalars = (("Glow", glow if masked else 0.0), ("SelfLit", self_lit), ("Rim", rim), ("Fill", fill), ("Roughness", roughness))
    elif kind == theme_plan.OCCLUDER:
        scalars = (("Glow", glow if masked else 0.0), ("Roughness", roughness))
    else:
        scalars = ()
    for parameter, value in scalars:
        if abs(mel.get_material_instance_scalar_parameter_value(instance, parameter) - value) > 1e-6:
            mel.set_material_instance_scalar_parameter_value(instance, parameter, value)
            changed = True
    if tint is not None and kind != theme_plan.CARD:
        wanted = unreal.LinearColor(tint[0], tint[1], tint[2], 1.0)
        current = mel.get_material_instance_vector_parameter_value(instance, "Color")
        if max(abs(current.r - wanted.r), abs(current.g - wanted.g), abs(current.b - wanted.b), abs(current.a - wanted.a)) > 1e-6:
            mel.set_material_instance_vector_parameter_value(instance, "Color", wanted)
            changed = True
    if changed:
        mel.update_material_instance(instance)
    unreal.log("THEME_MATERIAL {} glow={} mask={}".format(mesh_name, glow if masked and kind != theme_plan.CARD else 0.0, masked))
    return instance


def apply_material(mesh, texture, asset_id, name, look, parents, kind=theme_plan.LIT, revision=1, source=None, tint=None):
    instance = material_instance(name, "MI_" + name[3:], "T_" + name[3:], texture, asset_id, look, parents, kind, revision, source, tint)
    # Assign only when different, so an unchanged mesh is never dirtied (and never re-saved).
    for index in range(mesh.get_num_sections(0)):
        if mesh.get_material(index) != instance:
            mesh.set_material(index, instance)


# ---------------------------------------------------------------------------------------------------------------------
# Rigged characters (#147). Art/rig-pilot/r001 holds the sources; each character record is Art/<asset-id>/r001/.
# One skeleton (SKEL_Humanoid, from the fighter's import) serves all three characters; the nine clips are imported once,
# from the fighter's LOD0 glb (every clip is shared across characters). Nothing existing is overwritten.

RIG_FOLDER = THEME + "/Characters"
SKELETON = RIG_FOLDER + "/SKEL_Humanoid"
CLIPS = ["Idle", "Walk", "Melee", "Ranged", "Cast", "Hit", "Downed", "Death", "Recover"]
# asset id, mesh name, data-asset property, look (roughness, rim, fill, glow, self_lit): characters get Rim and Fill 1.
RIG = [
    ("sk-hero-fighter", "SK_Hero_Fighter", "fighter_rig", (0.55, 1.0, 1.0, 1.0, 0.0)),
    ("sk-hero-wizard", "SK_Hero_Wizard", "wizard_rig", (0.65, 1.0, 1.0, 1.0, 0.0)),
    ("sk-foe-raider", "SK_Foe_Raider", "raider_rig", (0.6, 1.0, 1.0, 1.0, 0.0)),
]
# Replacement is refused (see refuse_reimport): the allow-list is only validated so the refusal can name the problem.
REIMPORT = {token.strip() for token in os.environ.get("THEME_RIG_REIMPORT", "").split(",") if token.strip()}


def clip_path(clip):
    return RIG_FOLDER + "/Animations/A_Humanoid_" + clip


def rig_log(kind, path, status, lods=None):
    suffix = "" if lods is None else " lods={}".format(lods)
    unreal.log("THEME_RIG_ASSET {} {} {}{}".format(kind, path, status, suffix))


def rig_lod_count(mesh):
    return unreal.get_editor_subsystem(unreal.SkeletalMeshEditorSubsystem).get_lod_count(mesh)


def check_allow_list():
    known = {name for _, name, _, _ in RIG} | {"SKEL_Humanoid", "animations"}
    if REIMPORT - known:
        raise RuntimeError("THEME_RIG_REIMPORT names unknown assets: " + ", ".join(sorted(REIMPORT - known)))
    # Everything on the shared skeleton belongs to one bundle, so a partial request is rejected before anything else.
    if REIMPORT and not known <= REIMPORT:
        raise RuntimeError("THEME_RIG_REIMPORT must name the whole rig bundle: " + ", ".join(sorted(known)))
    for asset_id, name, _, _ in RIG:
        for suffix in ("", "_LOD1", "_LOD2"):
            if not os.path.isfile(source_glb(asset_id, name + suffix)):
                raise RuntimeError("THEME_RIG_REIMPORT: missing source " + source_glb(asset_id, name + suffix))


def refuse_reimport():
    """No side effects. In-place skeletal replacement (Interchange reimport with the shared-skeleton override pipeline
    and two LOD imports) cannot be made reliable from source alone, and delete_asset would null every reference held by
    other loaded assets (the theme data asset, maps, other meshes), which recreating the package does not restore."""
    raise RuntimeError("THEME_RIG_REIMPORT plan is valid, but this script never deletes or replaces rig assets: delete the "
                       "rig assets deliberately in the editor (fix up references), then rerun without THEME_RIG_REIMPORT.")


def source_glb(asset_id, name):
    return os.path.join(ROOT, "Art", asset_id, "r001", name + ".glb")


def import_glb(source, staging, skeleton):
    if skeleton is None:
        task = unreal.AssetImportTask()
        task.set_editor_property("filename", source)
        task.set_editor_property("destination_path", staging)
        task.set_editor_property("automated", True)
        task.set_editor_property("replace_existing", True)
        task.set_editor_property("save", False)
        tools.import_asset_tasks([task])
        return [unreal.load_asset(path) for path in task.get_editor_property("imported_object_paths")]
    # Reusing the shared skeleton needs an override pipeline, which only the Interchange manager accepts (an
    # AssetImportTask ignores it, and SkeletalMesh.skeleton is read-only from Python). Verified by a headless probe.
    pipeline = unreal.new_object(unreal.InterchangeGenericAssetsPipeline)
    common = pipeline.get_editor_property("common_skeletal_meshes_and_animations_properties")
    common.set_editor_property("skeleton", skeleton)
    params = unreal.ImportAssetParameters()
    params.set_editor_property("is_automated", True)
    params.set_editor_property("override_pipelines", [unreal.SoftObjectPath(pipeline.get_path_name())])
    source_data = unreal.InterchangeManager.create_source_data(source)
    unreal.InterchangeManager.get_interchange_manager_scripted().import_asset(staging, source_data, params)
    return [unreal.load_asset(path) for path in library.list_assets(staging, recursive=True)]


def clip_of(asset):
    name = asset.get_name().lower().replace("|", "_")
    for clip in CLIPS:
        # Interchange names clips <mesh><Clip> (SK_Hero_FighterIdle), no separator; no clip name ends another.
        if name == clip.lower() or name.endswith(clip.lower()):
            return clip
    return None


def disable_nanite(mesh):
    """The glTF importer enables Nanite silently; characters are skinned and low-poly, so switch it off explicitly."""
    try:
        settings = mesh.get_editor_property("nanite_settings")
    except Exception:  # noqa: BLE001
        unreal.log_warning("THEME_RIG_NANITE {} has no nanite_settings property in this build; check it in the editor".format(mesh.get_name()))
        return
    if settings.get_editor_property("enabled"):
        settings.set_editor_property("enabled", False)
        mesh.set_editor_property("nanite_settings", settings)
    if mesh.get_editor_property("nanite_settings").get_editor_property("enabled"):
        raise RuntimeError("Unable to turn Nanite off for " + mesh.get_name())


def import_rig_lods(mesh, asset_id, name):
    """Imports LOD1 and LOD2 and returns the LOD count, which must be 3.

    On the Interchange (glb) path import_lod returns as soon as the import is scheduled, so a failed LOD import does
    not return -1: the LOD count read back afterwards is the real check. Both checks are kept.
    """
    subsystem = unreal.get_editor_subsystem(unreal.SkeletalMeshEditorSubsystem)
    for lod, suffix in ((1, "_LOD1"), (2, "_LOD2")):
        source = os.path.join(ROOT, "Art", asset_id, "r001", name + suffix + ".glb")
        if not os.path.isfile(source):
            raise RuntimeError("Missing LOD export " + source)
        if subsystem.import_lod(mesh, lod, source) < 0:
            raise RuntimeError("Unable to import " + source + " as LOD" + str(lod) + " of " + name)
    count = rig_lod_count(mesh)
    if count != 3:
        raise RuntimeError("{} has {} LODs after importing LOD1 and LOD2, expected 3 (an Interchange LOD import can fail "
                           "after being scheduled; check the import log for {}_LOD1.glb and {}_LOD2.glb)".format(name, count, name, name))
    return count


def import_rig_mesh(asset_id, name, shared, first):
    """Imports one character into staging and moves its assets to their final packages.

    The first (fighter) import also yields the shared skeleton and the nine clips. Returns (mesh, texture, staging); the
    caller applies the material and then removes staging, because the imported mesh still points at staging materials.
    """
    source = source_glb(asset_id, name)
    if not os.path.isfile(source):
        unreal.log_warning("THEME_MISSING_EXPORT " + source)
        return None, None, None, None
    if not first and shared is None:
        raise RuntimeError("Cannot import " + name + ": " + SKELETON + " does not exist yet")
    staging = THEME + "/_Import/" + name
    imported = import_glb(source, staging, None if first else shared)
    meshes = [asset for asset in imported if isinstance(asset, unreal.SkeletalMesh)]
    textures = [asset for asset in imported if isinstance(asset, unreal.Texture2D)]
    if len(meshes) != 1:
        raise RuntimeError("Expected one skeletal mesh from " + source + ", got " + str(len(meshes)))
    mesh = meshes[0]
    if first:
        skeletons = [asset for asset in imported if isinstance(asset, unreal.Skeleton)]
        if len(skeletons) != 1:
            raise RuntimeError("Expected one skeleton from " + source + ", got " + str(len(skeletons)))
        found = {}
        for asset in imported:
            if isinstance(asset, unreal.AnimSequence) and clip_of(asset):
                found.setdefault(clip_of(asset), []).append(asset)
        if sorted(found) != sorted(CLIPS) or any(len(group) != 1 for group in found.values()):
            raise RuntimeError("Expected exactly the nine clips {} from {}, got {}".format(
                CLIPS, source, {clip: [a.get_name() for a in group] for clip, group in found.items()}))
        if not library.rename_asset(skeletons[0].get_path_name(), SKELETON):
            raise RuntimeError("Unable to move skeleton to " + SKELETON)
        for clip in CLIPS:
            if not library.rename_asset(found[clip][0].get_path_name(), clip_path(clip)):
                raise RuntimeError("Unable to move clip to " + clip_path(clip))
    skeleton = unreal.load_asset(SKELETON)
    if mesh.get_editor_property("skeleton") != skeleton:
        try:
            mesh.set_editor_property("skeleton", skeleton)
        except Exception as error:  # noqa: BLE001
            raise RuntimeError("{} is not on {}: {}".format(name, SKELETON, error))
        if mesh.get_editor_property("skeleton") != skeleton:
            raise RuntimeError(name + " is not on " + SKELETON)
    try:
        mesh.set_editor_property("physics_asset", None)  # not required; an importer-made one lives in staging
    except Exception:  # noqa: BLE001
        pass
    disable_nanite(mesh)
    lods = import_rig_lods(mesh, asset_id, name)
    package = RIG_FOLDER + "/" + name
    if not library.rename_asset(mesh.get_path_name(), package):
        raise RuntimeError("Unable to move mesh to " + package)
    texture = None
    if textures:
        target = THEME + "/Textures/T_" + name
        if not library.rename_asset(textures[0].get_path_name(), target):
            raise RuntimeError("Unable to move texture to " + target)
        texture = unreal.load_asset(target)
    return unreal.load_asset(package), texture, staging, lods


def apply_rig_material(mesh, texture, asset_id, name, look, master):
    instance = material_instance(name, "MI_" + name, "T_" + name, texture, asset_id, look, {theme_plan.LIT: master})
    slots = list(mesh.get_editor_property("materials"))
    changed = False
    for slot in slots:
        if slot.get_editor_property("material_interface") != instance:
            slot.set_editor_property("material_interface", instance)
            changed = True
    if changed:
        mesh.set_editor_property("materials", slots)


def path_of(asset):
    if asset is None:
        return None
    return asset.get_path_name() if hasattr(asset, "get_path_name") else str(asset)


def rig_definition(theme, prop, name):
    """The data-asset entry for a character, or None when it already matches (so an unchanged theme is not dirtied)."""
    current = theme.get_editor_property(prop)
    mesh = unreal.load_asset(RIG_FOLDER + "/" + name)
    clips = {clip: unreal.load_asset(clip_path(clip)) for clip in CLIPS}
    animations = current.get_editor_property("animations")
    if path_of(current.get_editor_property("mesh")) == path_of(mesh) and all(
            path_of(animations.get_editor_property(clip.lower())) == path_of(asset) for clip, asset in clips.items()):
        return None
    definition = unreal.DiceRollCharacter()
    definition.set_editor_property("mesh", mesh)
    definition.set_editor_property("yaw_offset", current.get_editor_property("yaw_offset"))
    animation_set = unreal.DiceRollAnimSet()
    for clip, asset in clips.items():
        animation_set.set_editor_property(clip.lower(), asset)
    definition.set_editor_property("animations", animation_set)
    return definition


def import_rig(master, theme):
    if not os.path.isdir(os.path.join(ROOT, "Art", "rig-pilot")):
        unreal.log_warning("THEME_RIG_SKIPPED Art/rig-pilot is not in this checkout")
        return 0
    check_allow_list()
    names = [name for _, name, _, _ in RIG]
    # The skeleton, the clips and the fighter come from one import, so they exist together or not at all.
    bundle = [SKELETON, RIG_FOLDER + "/" + names[0]] + [clip_path(clip) for clip in CLIPS]
    present = [library.does_asset_exist(path) for path in bundle]
    if any(present) and not all(present):
        raise RuntimeError("Incomplete rig bundle, missing: {}. Delete the survivors deliberately in the editor and rerun.".format(
            [path for path, here in zip(bundle, present) if not here]))
    found = 0
    for asset_id, name, prop, look in RIG:
        package = RIG_FOLDER + "/" + name
        staging = None
        if library.does_asset_exist(package):
            mesh, texture = unreal.load_asset(package), None
            lods = rig_lod_count(mesh)
            if lods != 3:
                raise RuntimeError("{} has {} LODs, expected 3; delete it deliberately in the editor and rerun".format(package, lods))
            rig_log("mesh", package, "kept", lods)
        else:
            mesh, texture, staging, lods = import_rig_mesh(asset_id, name, unreal.load_asset(SKELETON) if library.does_asset_exist(SKELETON) else None, not all(present) and name == names[0])
            if mesh is None:
                continue
            rig_log("mesh", package, "imported", lods)
            if name == names[0]:
                rig_log("skeleton", SKELETON, "imported")
                for clip in CLIPS:
                    rig_log("clip", clip_path(clip), "imported")
        apply_rig_material(mesh, texture, asset_id, name, look, master)
        if staging:
            remove_staging(staging)
        definition = rig_definition(theme, prop, name)
        if definition is not None:
            theme.set_editor_property(prop, definition)
            rig_log("theme", prop, "assigned")
        found += 1
    for clip in CLIPS:
        if all(present):
            rig_log("clip", clip_path(clip), "kept")
    if all(present):
        rig_log("skeleton", SKELETON, "kept")
    return found


def theme_asset():
    if library.does_asset_exist(DATA):
        return unreal.load_asset(DATA)
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", unreal.DiceRollTheme)
    return tools.create_asset("DA_DiceRollTheme", THEME, unreal.DiceRollTheme, factory)


def asset_line(asset_id, package, action, source, mesh, extra=""):
    tris = theme_plan.triangle_count(source) if source and os.path.isfile(source) else "n/a"
    digest = theme_plan.sha256_file(source) if source and os.path.isfile(source) else "n/a"
    nanite = "n/a" if mesh is None else ("on" if nanite_enabled(mesh) else "off")
    lods = "n/a" if mesh is None else mesh_tools.get_lod_count(mesh)
    shapes = "n/a" if mesh is None else collision_shapes(mesh)
    kit = "n/a" if mesh is None else (kit_tag(mesh) or "r001")
    unreal.log("THEME_ASSET id={} package={} kit={} action={} source_tris={} source_sha256={} nanite={} lods={} collision_shapes={}{}".format(
        asset_id, package, kit, action, tris, digest, nanite, lods, shapes, (" " + extra) if extra else ""))


def set_list(theme, prop, items):
    """Sets an array property only when it differs, so an unchanged theme is not dirtied."""
    def name(item):
        return "" if not item else item.get_path_name() if hasattr(item, "get_path_name") else str(item)
    current = [name(item) for item in theme.get_editor_property(prop)]
    wanted = [name(item) for item in items]
    if current != wanted:
        set_if_different(theme, prop, items)


def check_metric_gate(kit, replacements):
    """Raises unless a kit's metric-gate packages are all (or none) tagged or being replaced in this run."""
    gate = {}
    for row in kit["rows"]:
        if row[0] in kit["gate"]:
            package = theme_plan.package_path(row[3], row[2])
            gate[row[0]] = (library.does_asset_exist(package) and kit_tagged(unreal.load_asset(package), kit["tag"])) or row[0] in replacements
    if any(gate.values()) and not all(gate.values()):
        raise RuntimeError("Replace the {} metric gate together: {} (now {})".format(kit["name"], ", ".join(kit["gate"]), gate))


def import_kit(kit, theme, parents, replacements):
    """Imports one world kit's rows (the Vigil's, then the Root's) and returns (meshes loaded, whether its metric gate is met).

    A row whose package holds a mesh with this kit's tag is left alone ("present"); a row whose package exists as r001 is skipped
    unless its id is allow-listed and requested; everything else is imported, stripped of collision and Nanite, given its explicit
    LODs and tagged. The theme's array properties and the kit's metric flag are written only when they differ."""
    slots = {prop: [None] * count for prop, count in kit["sizes"].items()}
    loaded = 0
    for asset_id, revision, name, folder, kind, prop, index, look in kit["rows"]:
        package = theme_plan.package_path(folder, name)
        source = theme_plan.export_path(asset_id, revision, name)
        exists = library.does_asset_exist(package)
        replace = asset_id in replacements
        if exists and kit_tagged(unreal.load_asset(package), kit["tag"]):
            mesh, texture, action = unreal.load_asset(package), None, "present"
        elif exists and not replace:
            reason = "r001 package kept (replace with {}={})".format(theme_plan.REIMPORT_ENV, asset_id) if asset_id in theme_plan.ALL_REPLACEABLE else "exists and is not tagged as the kit; remove it deliberately to import"
            asset_line(asset_id, package, "skipped", source, unreal.load_asset(package), "reason=\"" + reason + "\"")
            continue
        else:
            mesh, texture = import_kit_asset(asset_id, revision, name, folder, replace)
            action = "replaced" if exists else "created"
            mesh_tools.remove_collisions(mesh)
            shapes = collision_shapes(mesh)
            if shapes:
                raise RuntimeError("{} still has {} simple collision shapes".format(package, shapes))
            import_lods(mesh, asset_id, revision, name)
            library.set_metadata_tag(mesh, theme_plan.REVISION_TAG, kit["tag"])
            mesh.modify()  # marks the package dirty so the import is saved
        force_nanite_off(mesh)
        check_lod_count(mesh, asset_id, revision)
        if theme_plan.record_lods(asset_id, revision):
            repair_lod_slots(mesh, name)
        apply_material(mesh, texture, asset_id, name, look, parents, kind=kind, revision=2, source=source, tint=theme_plan.INSTANCE_TINTS.get(asset_id))
        asset_line(asset_id, package, action, source, mesh, "lod_exports={}".format(len(theme_plan.lod_exports(asset_id, revision, name))))
        if index is None:
            set_if_different(theme, prop, mesh)
        else:
            slots[prop][index] = mesh
        loaded += 1
    for prop, items in slots.items():
        set_list(theme, prop, items)
    metric = all(kit_tagged(unreal.load_asset(theme_plan.package_path(row[3], row[2])), kit["tag"]) for row in kit["rows"] if row[0] in kit["gate"])
    if bool(theme.get_editor_property(kit["flag"])) != metric:
        set_if_different(theme, kit["flag"], metric)
    return loaded, metric


def main():
    # A rig replacement request is validated and refused before anything is touched: no master rebuild, no settings, no imports.
    if REIMPORT:
        check_allow_list()
        refuse_reimport()
    before = theme_hashes()
    replacements = theme_plan.requested_replacements(os.environ.get(theme_plan.REIMPORT_ENV))
    fix_r001_nanite = os.environ.get(theme_plan.NANITE_FIX_ENV) == "1"
    unreal.log("THEME_PLAN replace={} fix_r001_nanite={}".format(sorted(replacements) or "none", fix_r001_nanite))
    master = master_material()
    ensure_skeletal_usage(master)
    occluder_parent = ensure_material(OCCLUDER_MATERIAL, build_occluder_material, occluder_is_current, OCCLUDER_GRAPH_REVISION)
    card_parent = ensure_material(CARD_MATERIAL, build_card_material)
    ensure_material(FIGURE_SILHOUETTE_MATERIAL, build_figure_silhouette_material, figure_silhouette_is_current, FIGURE_SILHOUETTE_REVISION)
    parents = {theme_plan.LIT: master, theme_plan.OCCLUDER: occluder_parent, theme_plan.CARD: card_parent}
    theme = theme_asset()
    set_if_different(theme, "tint_material", master)

    # A kit's true-metric world (and, for the Vigil, props) replaces its r001 packages together or not at all: the board scales
    # r001 art and places r002 art unscaled, so a mix would draw one of them at the wrong size.
    for kit in KITS:
        check_metric_gate(kit, replacements)

    found = 0
    for asset_id, name, folder, prop, *look in ASSETS:
        mesh, texture = import_mesh(asset_id, name, folder)
        if mesh is None:
            continue
        package = theme_plan.package_path(folder, name)
        if kit_tagged(mesh):
            # Already replaced by r002: its look is applied below, never reset from the r001 export.
            set_if_different(theme, prop, mesh)
            found += 1
            continue
        apply_material(mesh, texture, asset_id, name, look, parents)
        set_if_different(theme, prop, mesh)
        if nanite_enabled(mesh):
            if fix_r001_nanite:
                force_nanite_off(mesh)
            else:
                unreal.log("THEME_NANITE_ON_R001 {} (set {}=1 to switch it off; that changes the asset's bytes)".format(package, theme_plan.NANITE_FIX_ENV))
        asset_line(asset_id, package, "kept-r001", r001_source(asset_id, name), mesh)
        found += 1

    kit_loaded, kit_metric = {}, {}
    for kit in KITS:
        kit_loaded[kit["name"]], kit_metric[kit["name"]] = import_kit(kit, theme, parents, replacements)

    rigged = import_rig(master, theme)

    # Save only what this run changed: unchanged meshes and textures keep their exact bytes.
    if not library.save_directory(THEME, only_if_is_dirty=True, recursive=True):
        raise RuntimeError("Unable to save " + THEME)
    after = theme_hashes()
    changed = sorted(path for path in set(before) | set(after) if before.get(path) != after.get(path))
    for path in changed:
        state = "added" if path not in before else "removed" if path not in after else "changed"
        unreal.log("THEME_FILE {} {} sha256={}".format(state, path, after.get(path, "-")))
    unreal.log("THEME_FILES_CHANGED {}".format(len(changed)))
    unreal.log("THEME_IMPORT_COMPLETE {}/{} meshes, kit {}/{} meshes, metric_kit={}".format(
        found, len(ASSETS), kit_loaded["vigil"], len(theme_plan.R002), kit_metric["vigil"]))
    unreal.log("THEME_ROOT_COMPLETE root kit {}/{} meshes, root_metric_kit={}".format(kit_loaded["root"], len(theme_plan.ROOT_R002), kit_metric["root"]))
    unreal.log("THEME_RIG_COMPLETE {}/{} characters, {} clips".format(rigged, len(RIG), len(CLIPS)))


main()
