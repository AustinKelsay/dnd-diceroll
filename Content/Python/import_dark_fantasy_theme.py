"""Import the dark-fantasy theme from Art/ into /Game/DnDDiceRoll/Theme (issue #38).

Run headless with the editor closed:
  UnrealEditor DnDDiceRoll.uproject -run=pythonscript -script=<repo>/Content/Python/import_dark_fantasy_theme.py -unattended -nullrhi
Existing theme meshes and textures are preserved; a revised export is reimported at its
recorded package through the editor (asset production). The master material (rebuilt in place, so its
asset and instances stay), material instances, texture settings and the data asset are refreshed on every run.

Rigged characters (#147): the same run also imports the fighter, wizard and raider as skeletal meshes on one shared
skeleton, with LOD1 and LOD2, the nine clips and Nanite off (see "Rigged characters" below). Existing rig assets are
never overwritten: set THEME_RIG_REIMPORT to a comma-separated allow-list (mesh names such as SK_Hero_Fighter,
SKEL_Humanoid, animations) to delete and reimport exactly those.
"""
import json
import os
import struct
import unreal
# A headless commandlet does not load the mesh editor modules, so their subsystems would be None.
unreal.load_module("SkeletalMeshEditor")
if unreal.get_editor_subsystem(unreal.SkeletalMeshEditorSubsystem) is None:
    raise RuntimeError("SkeletalMeshEditorSubsystem is unavailable even after loading the SkeletalMeshEditor module")

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
THEME = "/Game/DnDDiceRoll/Theme"
MASTER = THEME + "/Materials/M_DiceRollTheme"
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


MASTER_SCALARS = {"Glow", "SelfLit", "Fill", "Rim", "Roughness", "CameraFade"}
MASTER_VECTORS = {"Color", "FillSky", "FillGround", "RimColor"}
MASTER_TEXTURES = {"Tex"}


def master_is_current(material):
    names = lambda values: {str(v) for v in values}
    return (material.get_editor_property("blend_mode") == unreal.BlendMode.BLEND_MASKED and MASTER_SCALARS <= names(mel.get_scalar_parameter_names(material))
            and MASTER_VECTORS <= names(mel.get_vector_parameter_names(material))
            and MASTER_TEXTURES <= names(mel.get_texture_parameter_names(material)))


def set_if_different(obj, prop, value):
    """Sets an editor property only when it differs, so unchanged assets are never dirtied."""
    if obj.get_editor_property(prop) != value:
        obj.set_editor_property(prop, value)
        return True
    return False


def master_material():
    """Builds (or rebuilds in place) M_DiceRollTheme. The asset, its path and every instance of it survive.

    base     = Tex.rgb x vertexColor.rgb x Color
    emissive = base x (vertexColor.a x Glow + SelfLit)           glow only where the vertex alpha says so
             + base x Fill x hemisphere(world normal Z)           sky/ground fill, so shadowed sides keep form
             + RimColor x Rim x fresnel                           cool rim so dark figures read against dark ground
    roughness = Roughness (per instance)
    """
    if library.does_asset_exist(MASTER):
        material = unreal.load_asset(MASTER)
        # Leave an existing graph alone when it already exposes this graph's parameters, so a rerun never
        # re-saves the master (and every instance) for nothing. Set DND_THEME_REBUILD_MASTER=1 to rebuild it
        # on purpose after changing the graph below.
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

    # Camera fade: scenery closer to the camera than CameraFade x the camera's height is clipped, so a
    # diorama that surrounds the board (r001 Root) never swallows the camera when the HUD-safe fit pulls it
    # back. The radius scales with zoom; the board and figures are always farther away. In shadow passes the
    # camera is the light, so clipped scenery keeps its shadows.
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
    world_pos = expression(material, unreal.MaterialExpressionWorldPosition, -700, 1550)
    camera_pos = expression(material, unreal.MaterialExpressionCameraPositionWS, -700, 1650)
    distance = expression(material, unreal.MaterialExpressionDistance, -500, 1600)
    mel.connect_material_expressions(world_pos, "", distance, "A")
    mel.connect_material_expressions(camera_pos, "", distance, "B")
    camera_z = expression(material, unreal.MaterialExpressionComponentMask, -500, 1700, r=False, g=False, b=True, a=False)
    mel.connect_material_expressions(camera_pos, "", camera_z, "")
    fade = scalar(material, "CameraFade", 0.5, -500, 1800)
    radius = mul(material, camera_z, fade, -300, 1750)
    beyond = expression(material, unreal.MaterialExpressionSubtract, -150, 1650)
    mel.connect_material_expressions(distance, "", beyond, "A")
    mel.connect_material_expressions(radius, "", beyond, "B")
    edge = expression(material, unreal.MaterialExpressionMultiply, 0, 1650, const_b=0.02)
    mel.connect_material_expressions(beyond, "", edge, "A")
    opacity = expression(material, unreal.MaterialExpressionSaturate, 150, 1650)
    mel.connect_material_expressions(edge, "", opacity, "")
    mel.connect_material_property(opacity, "", unreal.MaterialProperty.MP_OPACITY_MASK)
    spec = expression(material, unreal.MaterialExpressionConstant, 100, 600, r=0.2)
    mel.connect_material_property(color, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(emissive, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.connect_material_property(roughness, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(spec, "", unreal.MaterialProperty.MP_SPECULAR)
    mel.recompile_material(material)
    return material


def ensure_skeletal_usage(material):
    """Skinned meshes need their own shader permutation (#147); without the flag a skeletal mesh draws the default material.

    master_is_current() does not look at this flag, so this runs whether the master was kept or rebuilt. It is written
    with set_if_different and recompiled only when it changed, so a rerun leaves the master untouched.
    """
    if set_if_different(material, "used_with_skeletal_mesh", True):
        mel.recompile_material(material)
        unreal.log("THEME_MASTER used_with_skeletal_mesh set")


def has_alpha_mask(asset_id, name):
    """True when the r001 export carries a vertex-colour alpha that actually varies (a real glow mask).

    glTF COLOR_0 is optional and often RGB only; Unreal then reads alpha 1 everywhere, which would make the whole
    mesh glow. Read the export's accessors and treat only a VEC4 colour with a spread of alpha as a mask.
    """
    source = os.path.join(ROOT, "Art", asset_id, "r001", name + ".glb")
    if not os.path.isfile(source):
        return False
    with open(source, "rb") as handle:
        data = handle.read()
    json_length = struct.unpack("<I", data[12:16])[0]
    header = json.loads(data[20:20 + json_length])
    binary_at = 20 + json_length
    binary = data[binary_at + 8:binary_at + 8 + struct.unpack("<I", data[binary_at:binary_at + 4])[0]]
    formats = {5126: ("f", 4, 1.0), 5121: ("B", 1, 255.0), 5123: ("H", 2, 65535.0)}
    for mesh in header.get("meshes", []):
        for primitive in mesh.get("primitives", []):
            index = primitive.get("attributes", {}).get("COLOR_0")
            if index is None:
                continue
            accessor = header["accessors"][index]
            if accessor["type"] != "VEC4":
                continue
            view = header["bufferViews"][accessor["bufferView"]]
            code, size, divisor = formats[accessor["componentType"]]
            start = view.get("byteOffset", 0) + accessor.get("byteOffset", 0)
            stride = view.get("byteStride", 4 * size)
            alphas = [struct.unpack_from("<" + code, binary, start + i * stride + 3 * size)[0] / divisor for i in range(accessor["count"])]
            if alphas and max(alphas) - min(alphas) > 0.02 and min(alphas) < 0.99:
                return True
    return False


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
    library.delete_directory(staging)
    return unreal.load_asset(package), texture


def material_instance(mesh_name, instance_name, texture_name, texture, asset_id, look, master):
    """Creates or refreshes one MI_* child of the master for a mesh and returns it (shared by static and skeletal meshes)."""
    roughness, rim, fill, glow, self_lit = look
    path = THEME + "/Materials/" + instance_name
    instance = unreal.load_asset(path) if library.does_asset_exist(path) else tools.create_asset(
        instance_name, THEME + "/Materials", unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    changed = False
    if instance.get_editor_property("parent") != master:
        mel.set_material_instance_parent(instance, master)
        changed = True
    if texture is None:
        existing = THEME + "/Textures/" + texture_name
        texture = unreal.load_asset(existing) if library.does_asset_exist(existing) else None
    if texture is not None:
        # Trilinear + anisotropic 8 (gate 1); uncompressed colour. TF_TRILINEAR would map straight to a plain trilinear
        # sampler, which never uses r.MaxAnisotropy. TF_DEFAULT defers to the texture group, and Config/DefaultDeviceProfiles.ini
        # makes TEXTUREGROUP_World MinMagFilter=aniso + MipFilter=linear, which is the anisotropic-linear sampler.
        # At 256 texels/m nearest, trilinear and sharp texels looked alike (gate 1), and mips stop shimmer at distance.
        # Each setting is written only when it differs, so a rerun leaves unchanged textures byte-identical.
        set_if_different(texture, "filter", unreal.TextureFilter.TF_DEFAULT)
        set_if_different(texture, "compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON)
        set_if_different(texture, "srgb", True)
        set_if_different(texture, "lod_group", unreal.TextureGroup.TEXTUREGROUP_WORLD)
        set_if_different(texture, "mip_gen_settings", unreal.TextureMipGenSettings.TMGS_SIMPLE_AVERAGE)
        if mel.get_material_instance_texture_parameter_value(instance, "Tex") != texture:
            mel.set_material_instance_texture_parameter_value(instance, "Tex", texture)
            changed = True
    # The glow mask guard: without a varying vertex alpha (every static r001 export today) glow stays 0.
    masked = has_alpha_mask(asset_id, mesh_name)
    for parameter, value in (("Glow", glow if masked else 0.0), ("SelfLit", self_lit), ("Rim", rim), ("Fill", fill), ("Roughness", roughness)):
        if abs(mel.get_material_instance_scalar_parameter_value(instance, parameter) - value) > 1e-6:
            mel.set_material_instance_scalar_parameter_value(instance, parameter, value)
            changed = True
    if changed:
        mel.update_material_instance(instance)
    unreal.log("THEME_MATERIAL {} glow={} mask={}".format(mesh_name, glow if masked else 0.0, masked))
    return instance


def apply_material(mesh, texture, asset_id, name, look, master):
    instance = material_instance(name, "MI_" + name[3:], "T_" + name[3:], texture, asset_id, look, master)
    # Assign only when different, so an unchanged mesh is never dirtied (and never re-saved).
    for index in range(mesh.get_num_sections(0)):
        if mesh.get_material(index) != instance:
            mesh.set_material(index, instance)


def theme_asset():
    if library.does_asset_exist(DATA):
        return unreal.load_asset(DATA)
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", unreal.DiceRollTheme)
    return tools.create_asset("DA_DiceRollTheme", THEME, unreal.DiceRollTheme, factory)


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
# Explicit allow-list of rig assets to delete and reimport: mesh names, SKEL_Humanoid, animations. Empty by default.
REIMPORT = {token.strip() for token in os.environ.get("THEME_RIG_REIMPORT", "").split(",") if token.strip()}


def clip_path(clip):
    return RIG_FOLDER + "/Animations/A_Humanoid_" + clip


def rig_log(kind, path, status, lods=None):
    suffix = "" if lods is None else " lods={}".format(lods)
    unreal.log("THEME_RIG_ASSET {} {} {}{}".format(kind, path, status, suffix))


def lod_count(mesh):
    return unreal.get_editor_subsystem(unreal.SkeletalMeshEditorSubsystem).get_lod_count(mesh)


def delete_for_reimport(path, token):
    if token in REIMPORT and library.does_asset_exist(path):
        if not library.delete_asset(path):
            raise RuntimeError("Unable to delete " + path + " for reimport")
        rig_log("deleted", path, "allow-listed")


def check_allow_list():
    known = {name for _, name, _, _ in RIG} | {"SKEL_Humanoid", "animations"}
    if REIMPORT - known:
        raise RuntimeError("THEME_RIG_REIMPORT names unknown assets: " + ", ".join(sorted(REIMPORT - known)))
    # Everything on the shared skeleton must be rebuilt with it, or the survivors would point at a deleted asset.
    if "SKEL_Humanoid" in REIMPORT and not known <= REIMPORT:
        raise RuntimeError("Reimporting SKEL_Humanoid needs every rig asset allow-listed: " + ", ".join(sorted(known)))


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


def import_lods(mesh, asset_id, name):
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
    count = lod_count(mesh)
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
    lods = import_lods(mesh, asset_id, name)
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
    instance = material_instance(name, "MI_" + name, "T_" + name, texture, asset_id, look, master)
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
    for name in names:
        delete_for_reimport(RIG_FOLDER + "/" + name, name)
        if name in REIMPORT:
            delete_for_reimport(THEME + "/Textures/T_" + name, name)
    delete_for_reimport(SKELETON, "SKEL_Humanoid")
    for clip in CLIPS:
        delete_for_reimport(clip_path(clip), "animations")
    # The skeleton, the clips and the fighter come from one import, so they exist together or not at all.
    bundle = [SKELETON, RIG_FOLDER + "/" + names[0]] + [clip_path(clip) for clip in CLIPS]
    present = [library.does_asset_exist(path) for path in bundle]
    if any(present) and not all(present):
        raise RuntimeError("Incomplete rig bundle, missing: {}. Add them (and the rest of the bundle) to THEME_RIG_REIMPORT to rebuild.".format(
            [path for path, here in zip(bundle, present) if not here]))
    found = 0
    for asset_id, name, prop, look in RIG:
        package = RIG_FOLDER + "/" + name
        staging = None
        if library.does_asset_exist(package):
            mesh, texture = unreal.load_asset(package), None
            lods = lod_count(mesh)
            if lods != 3:
                raise RuntimeError("{} has {} LODs, expected 3; add it to THEME_RIG_REIMPORT to rebuild".format(package, lods))
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
            library.delete_directory(staging)
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


master = master_material()
ensure_skeletal_usage(master)
theme = theme_asset()
set_if_different(theme, "tint_material", master)
found = 0
for asset_id, name, folder, prop, *look in ASSETS:
    mesh, texture = import_mesh(asset_id, name, folder)
    if mesh is None:
        continue
    apply_material(mesh, texture, asset_id, name, look, master)
    set_if_different(theme, prop, mesh)
    found += 1
rigged = import_rig(master, theme)
# Save only what this run changed: unchanged meshes and textures keep their exact bytes.
if not library.save_directory(THEME, only_if_is_dirty=True, recursive=True):
    raise RuntimeError("Unable to save " + THEME)
unreal.log("THEME_IMPORT_COMPLETE {}/{} meshes".format(found, len(ASSETS)))
unreal.log("THEME_RIG_COMPLETE {}/{} characters, {} clips".format(rigged, len(RIG), len(CLIPS)))
