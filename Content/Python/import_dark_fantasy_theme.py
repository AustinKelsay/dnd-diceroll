"""Import the dark-fantasy theme from Art/ into /Game/DnDDiceRoll/Theme (issue #38).

Run headless with the editor closed:
  UnrealEditor DnDDiceRoll.uproject -run=pythonscript -script=<repo>/Content/Python/import_dark_fantasy_theme.py -unattended -nullrhi
Existing theme meshes and textures are preserved; a revised export is reimported at its
recorded package through the editor (asset production). Material instances, texture settings
and the data asset are refreshed on every run.
"""
import os
import unreal

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
THEME = "/Game/DnDDiceRoll/Theme"
MASTER = THEME + "/Materials/M_DiceRollTheme"
DATA = THEME + "/DA_DiceRollTheme"

# asset id, export name, folder, data-asset property, glow (emissive share of the texture)
ASSETS = [
    ("hero-fighter", "SM_Hero_Fighter", "Characters", "fighter", 0.6),
    ("hero-rogue", "SM_Hero_Rogue", "Characters", "rogue", 0.6),
    ("hero-cleric", "SM_Hero_Cleric", "Characters", "cleric", 0.6),
    ("hero-wizard", "SM_Hero_Wizard", "Characters", "wizard", 0.6),
    ("foe-raider", "SM_Foe_Raider", "Characters", "raider", 0.5),
    ("foe-scout", "SM_Foe_Scout", "Characters", "scout", 0.5),
    ("foe-archer", "SM_Foe_Archer", "Characters", "archer", 0.5),
    ("world-root", "SM_World_Root", "Worlds", "root_world", 0.35),
    ("world-vigil", "SM_World_Vigil", "Worlds", "vigil_world", 0.35),
    ("tile-root", "SM_Tile_Root", "Worlds", "root_tile", 0.2),
    ("tile-vigil", "SM_Tile_Vigil", "Worlds", "vigil_tile", 0.2),
    ("prop-stones", "SM_Prop_Stones", "Props", "stones", 0.12),
    ("prop-pillar", "SM_Prop_Pillar", "Props", "pillar", 0.12),
    ("prop-wall", "SM_Prop_Wall", "Props", "wall", 0.12),
    ("prop-brazier", "SM_Prop_Brazier", "Props", "brazier", 0.08),
    ("prop-flame", "SM_Prop_Flame", "Props", "flame", 1.4),
    ("prop-banner", "SM_Prop_Banner", "Props", "banner", 0.15),
    ("prop-cage", "SM_Prop_Cage", "Props", "cage", 0.12),
    ("tabletop", "SM_Tabletop", "Overview", "tabletop", 0.3),
    ("diorama-root", "SM_Diorama_Root", "Overview", "root_diorama", 0.35),
    ("diorama-vigil", "SM_Diorama_Vigil", "Overview", "vigil_diorama", 0.35),
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


def master_material():
    if library.does_asset_exist(MASTER):
        return unreal.load_asset(MASTER)
    material = tools.create_asset("M_DiceRollTheme", THEME + "/Materials", unreal.Material, unreal.MaterialFactoryNew())
    white = unreal.load_asset("/Engine/EngineResources/WhiteSquareTexture")
    tex = expression(material, unreal.MaterialExpressionTextureSampleParameter2D, -900, -100, parameter_name="Tex", texture=white)
    vertex = expression(material, unreal.MaterialExpressionVertexColor, -900, 200)
    tint = expression(material, unreal.MaterialExpressionVectorParameter, -900, 400, parameter_name="Color", default_value=unreal.LinearColor(1, 1, 1, 1))
    glow = expression(material, unreal.MaterialExpressionScalarParameter, -400, 400, parameter_name="Glow", default_value=0.0)
    lit = expression(material, unreal.MaterialExpressionMultiply, -500, 0)
    mel.connect_material_expressions(tex, "RGB", lit, "A")
    mel.connect_material_expressions(rgb(material, vertex, "", -700, 200), "", lit, "B")
    color = expression(material, unreal.MaterialExpressionMultiply, -300, 100)
    mel.connect_material_expressions(lit, "", color, "A")
    mel.connect_material_expressions(rgb(material, tint, "", -700, 400), "", color, "B")
    emissive = expression(material, unreal.MaterialExpressionMultiply, -150, 300)
    mel.connect_material_expressions(color, "", emissive, "A")
    mel.connect_material_expressions(glow, "", emissive, "B")
    rough = expression(material, unreal.MaterialExpressionConstant, -150, 500, r=0.85)
    spec = expression(material, unreal.MaterialExpressionConstant, -150, 600, r=0.2)
    mel.connect_material_property(color, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(emissive, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(spec, "", unreal.MaterialProperty.MP_SPECULAR)
    mel.recompile_material(material)
    return material


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


def apply_material(mesh, texture, name, glow, master):
    path = THEME + "/Materials/MI_" + name[3:]
    instance = unreal.load_asset(path) if library.does_asset_exist(path) else tools.create_asset(
        "MI_" + name[3:], THEME + "/Materials", unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(instance, master)
    if texture is None:
        existing = THEME + "/Textures/T_" + name[3:]
        texture = unreal.load_asset(existing) if library.does_asset_exist(existing) else None
    if texture is not None:
        # Crisp pixel art: nearest filtering, uncompressed colour.
        texture.set_editor_property("filter", unreal.TextureFilter.TF_NEAREST)
        texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON)
        texture.set_editor_property("srgb", True)
        # Mips stop distant pixel textures from shimmering; nearest keeps them crisp up close.
        texture.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_WORLD)
        texture.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_SIMPLE_AVERAGE)
        mel.set_material_instance_texture_parameter_value(instance, "Tex", texture)
    mel.set_material_instance_scalar_parameter_value(instance, "Glow", glow)
    mel.update_material_instance(instance)
    for index in range(mesh.get_num_sections(0)):
        mesh.set_material(index, instance)


def theme_asset():
    if library.does_asset_exist(DATA):
        return unreal.load_asset(DATA)
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", unreal.DiceRollTheme)
    return tools.create_asset("DA_DiceRollTheme", THEME, unreal.DiceRollTheme, factory)


master = master_material()
theme = theme_asset()
theme.set_editor_property("tint_material", master)
found = 0
for asset_id, name, folder, prop, glow in ASSETS:
    mesh, texture = import_mesh(asset_id, name, folder)
    if mesh is None:
        continue
    apply_material(mesh, texture, name, glow, master)
    theme.set_editor_property(prop, mesh)
    found += 1
if not library.save_directory(THEME, only_if_is_dirty=False, recursive=True):
    raise RuntimeError("Unable to save " + THEME)
unreal.log("THEME_IMPORT_COMPLETE {}/{} meshes".format(found, len(ASSETS)))
