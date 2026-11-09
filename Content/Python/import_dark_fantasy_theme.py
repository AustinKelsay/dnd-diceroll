"""Import the dark-fantasy theme from Art/ into /Game/DnDDiceRoll/Theme (issue #38).

Run headless with the editor closed:
  UnrealEditor DnDDiceRoll.uproject -run=pythonscript -script=<repo>/Content/Python/import_dark_fantasy_theme.py -unattended -nullrhi
Existing theme meshes and textures are preserved; a revised export is reimported at its
recorded package through the editor (asset production). The master material (rebuilt in place, so its
asset and instances stay), material instances, texture settings and the data asset are refreshed on every run.
"""
import json
import os
import struct
import unreal

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


MASTER_SCALARS = {"Glow", "SelfLit", "Fill", "Rim", "Roughness"}
MASTER_VECTORS = {"Color", "FillSky", "FillGround", "RimColor"}
MASTER_TEXTURES = {"Tex"}


def master_is_current(material):
    names = lambda values: {str(v) for v in values}
    return (MASTER_SCALARS <= names(mel.get_scalar_parameter_names(material))
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
    spec = expression(material, unreal.MaterialExpressionConstant, 100, 600, r=0.2)
    mel.connect_material_property(color, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(emissive, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.connect_material_property(roughness, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(spec, "", unreal.MaterialProperty.MP_SPECULAR)
    mel.recompile_material(material)
    return material


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


def apply_material(mesh, texture, asset_id, name, look, master):
    roughness, rim, fill, glow, self_lit = look
    path = THEME + "/Materials/MI_" + name[3:]
    instance = unreal.load_asset(path) if library.does_asset_exist(path) else tools.create_asset(
        "MI_" + name[3:], THEME + "/Materials", unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    changed = False
    if instance.get_editor_property("parent") != master:
        mel.set_material_instance_parent(instance, master)
        changed = True
    if texture is None:
        existing = THEME + "/Textures/T_" + name[3:]
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
    # The glow mask guard: without a varying vertex alpha (every r001 export today) glow stays 0.
    masked = has_alpha_mask(asset_id, name)
    for parameter, value in (("Glow", glow if masked else 0.0), ("SelfLit", self_lit), ("Rim", rim), ("Fill", fill), ("Roughness", roughness)):
        if abs(mel.get_material_instance_scalar_parameter_value(instance, parameter) - value) > 1e-6:
            mel.set_material_instance_scalar_parameter_value(instance, parameter, value)
            changed = True
    if changed:
        mel.update_material_instance(instance)
    # Assign only when different, so an unchanged mesh is never dirtied (and never re-saved).
    for index in range(mesh.get_num_sections(0)):
        if mesh.get_material(index) != instance:
            mesh.set_material(index, instance)
    unreal.log("THEME_MATERIAL {} glow={} mask={}".format(name, glow if masked else 0.0, masked))


def theme_asset():
    if library.does_asset_exist(DATA):
        return unreal.load_asset(DATA)
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", unreal.DiceRollTheme)
    return tools.create_asset("DA_DiceRollTheme", THEME, unreal.DiceRollTheme, factory)


master = master_material()
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
# Save only what this run changed: unchanged meshes and textures keep their exact bytes.
if not library.save_directory(THEME, only_if_is_dirty=True, recursive=True):
    raise RuntimeError("Unable to save " + THEME)
unreal.log("THEME_IMPORT_COMPLETE {}/{} meshes".format(found, len(ASSETS)))
