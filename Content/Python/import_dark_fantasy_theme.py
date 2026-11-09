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
"""
import json
import os
import sys
import unreal

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import theme_plan

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
        mel.delete_all_material_expressions(material)
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


KIT_TAG_VALUE = "vigil-kit-r002"
mesh_tools = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)


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


def build_occluder_material(material):
    """Dithered opacity mask for the near architecture (#145): base look of the world, faded around the sight lines.

    Parameters set at run time by AExpeditionBattleView::UpdateOccluder: SightEye, SightFocus, SightUnit (world cm),
    FadeRadius (cm) and FadeMin. Defaults leave every pixel opaque. The mask compares the faded opacity with an
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
    faded = binary(material, unreal.MaterialExpressionMin, focus, unit, 1300, 1400)
    # Interleaved gradient noise of the pixel position: frac(52.98 * frac(dot(pixel, (0.0671, 0.00584)))).
    screen = node(material, unreal.MaterialExpressionScreenPosition, 300, 2200)
    size = node(material, unreal.MaterialExpressionViewProperty, 300, 2350)
    size.set_editor_property("property", unreal.MaterialExposedViewProperty.MEVP_VIEWPORT_SIZE)
    pixel = mul(material, screen, size, 500, 2250)
    weights = node(material, unreal.MaterialExpressionConstant2Vector, 500, 2400, r=0.06711056, g=0.00583715)
    spread = unary(material, unreal.MaterialExpressionFrac, binary(material, unreal.MaterialExpressionDotProduct, pixel, weights, 700, 2300), 850, 2300)
    noise = unary(material, unreal.MaterialExpressionFrac, binary(material, unreal.MaterialExpressionMultiply, spread, None, 1000, 2300, const_b=52.9829189), 1150, 2300)
    keep = unary(material, unreal.MaterialExpressionSaturate,
                 binary(material, unreal.MaterialExpressionAdd,
                        binary(material, unreal.MaterialExpressionMultiply, binary(material, unreal.MaterialExpressionSubtract, faded, noise, 1450, 2000), None, 1600, 2000, const_b=1000.0),
                        None, 1750, 2000, const_b=0.5), 1900, 2000)
    mel.connect_material_property(keep, "", unreal.MaterialProperty.MP_OPACITY_MASK)


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


def ensure_material(name, build):
    """Creates a material once; an existing asset is reported and never rebuilt or overwritten."""
    path = THEME + "/Materials/" + name
    if library.does_asset_exist(path):
        unreal.log("THEME_MATERIAL_KEPT " + path)
        return unreal.load_asset(path)
    material = tools.create_asset(name, THEME + "/Materials", unreal.Material, unreal.MaterialFactoryNew())
    build(material)
    mel.recompile_material(material)
    unreal.log("THEME_MATERIAL_CREATED " + path)
    return material


def r001_source(asset_id, name):
    return os.path.join(ROOT, "Art", asset_id, "r001", name + ".glb")


def kit_tagged(mesh):
    return mesh is not None and library.get_metadata_tag(mesh, theme_plan.REVISION_TAG) == KIT_TAG_VALUE


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
            raise RuntimeError("LOD{} of {} differs from its asset record; rerun write_records_r002.py".format(index, name))
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
    export = [entry for entry in record["files"] if entry["role"] == "export"]
    if record["unreal_package"] != theme_plan.package_path(folder, name) or len(export) != 1 or export[0]["sha256"] != theme_plan.sha256_file(source):
        raise RuntimeError("Asset record " + record_path + " does not match the export or the package; rerun write_records_r002.py")
    package = theme_plan.package_path(folder, name)
    target_texture = THEME + "/Textures/T_" + name[3:]
    mesh, texture, staging = stage_import(source, name)
    if library.does_asset_exist(package):
        if not replace:
            raise RuntimeError("Refusing to overwrite " + package)
        if not library.delete_asset(package):
            raise RuntimeError("Unable to remove the replaced mesh " + package)
        if library.does_asset_exist(target_texture) and not library.delete_asset(target_texture):
            raise RuntimeError("Unable to remove the replaced texture " + target_texture)
    elif library.does_asset_exist(target_texture):
        raise RuntimeError("A texture already exists at " + target_texture + " but the mesh does not; remove it deliberately")
    if not library.rename_asset(mesh.get_path_name(), package):
        raise RuntimeError("Unable to move mesh to " + package)
    if not library.rename_asset(texture.get_path_name(), target_texture):
        raise RuntimeError("Unable to move texture to " + target_texture)
    library.delete_directory(staging)
    return unreal.load_asset(package), unreal.load_asset(target_texture)


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


def apply_material(mesh, texture, asset_id, name, look, parents, kind=theme_plan.LIT, revision=1, source=None):
    roughness, rim, fill, glow, self_lit = look
    path = THEME + "/Materials/MI_" + name[3:]
    instance = unreal.load_asset(path) if library.does_asset_exist(path) else tools.create_asset(
        "MI_" + name[3:], THEME + "/Materials", unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(instance, parents[kind])
    if texture is None:
        existing = THEME + "/Textures/T_" + name[3:]
        texture = unreal.load_asset(existing) if library.does_asset_exist(existing) else None
    if texture is not None:
        set_texture(texture, revision)
        mel.set_material_instance_texture_parameter_value(instance, "Tex", texture)
    # The glow mask guard: without a varying vertex alpha glow stays 0.
    masked = theme_plan.has_alpha_mask(source or r001_source(asset_id, name))
    if kind == theme_plan.LIT:
        mel.set_material_instance_scalar_parameter_value(instance, "Glow", glow if masked else 0.0)
        mel.set_material_instance_scalar_parameter_value(instance, "SelfLit", self_lit)
        mel.set_material_instance_scalar_parameter_value(instance, "Rim", rim)
        mel.set_material_instance_scalar_parameter_value(instance, "Fill", fill)
        mel.set_material_instance_scalar_parameter_value(instance, "Roughness", roughness)
    elif kind == theme_plan.OCCLUDER:
        mel.set_material_instance_scalar_parameter_value(instance, "Glow", glow if masked else 0.0)
        mel.set_material_instance_scalar_parameter_value(instance, "Roughness", roughness)
    mel.update_material_instance(instance)
    # Assign only when different, so an unchanged mesh is never dirtied (and never re-saved).
    for index in range(mesh.get_num_sections(0)):
        if mesh.get_material(index) != instance:
            mesh.set_material(index, instance)
    unreal.log("THEME_MATERIAL {} glow={} mask={}".format(name, glow if masked and kind != theme_plan.CARD else 0.0, masked))


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
    kit = "n/a" if mesh is None else (KIT_TAG_VALUE if kit_tagged(mesh) else "r001")
    unreal.log("THEME_ASSET id={} package={} kit={} action={} source_tris={} source_sha256={} nanite={} lods={} collision_shapes={}{}".format(
        asset_id, package, kit, action, tris, digest, nanite, lods, shapes, (" " + extra) if extra else ""))


def set_list(theme, prop, items):
    """Sets an array property only when it differs, so an unchanged theme is not dirtied."""
    def name(item):
        return "" if not item else item.get_path_name() if hasattr(item, "get_path_name") else str(item)
    current = [name(item) for item in theme.get_editor_property(prop)]
    wanted = [name(item) for item in items]
    if current != wanted:
        theme.set_editor_property(prop, items)


def main():
    before = theme_hashes()
    replacements = theme_plan.requested_replacements(os.environ.get(theme_plan.REIMPORT_ENV))
    fix_r001_nanite = os.environ.get(theme_plan.NANITE_FIX_ENV) == "1"
    unreal.log("THEME_PLAN replace={} fix_r001_nanite={}".format(sorted(replacements) or "none", fix_r001_nanite))
    master = master_material()
    occluder_parent = ensure_material(OCCLUDER_MATERIAL, build_occluder_material)
    card_parent = ensure_material(CARD_MATERIAL, build_card_material)
    parents = {theme_plan.LIT: master, theme_plan.OCCLUDER: occluder_parent, theme_plan.CARD: card_parent}
    theme = theme_asset()
    theme.set_editor_property("tint_material", master)

    # The kit's true-metric world and props replace their r001 packages together or not at all: the board scales r001 art
    # and places r002 art unscaled, so a mix would draw one of them at the wrong size.
    gate = {}
    for row in theme_plan.R002:
        if row[0] in theme_plan.METRIC_GATE:
            package = theme_plan.package_path(row[3], row[2])
            gate[row[0]] = (library.does_asset_exist(package) and kit_tagged(unreal.load_asset(package))) or row[0] in replacements
    if any(gate.values()) and not all(gate.values()):
        raise RuntimeError("Replace the metric gate together: {} (now {})".format(", ".join(theme_plan.METRIC_GATE), gate))

    found = 0
    for asset_id, name, folder, prop, *look in ASSETS:
        mesh, texture = import_mesh(asset_id, name, folder)
        if mesh is None:
            continue
        package = theme_plan.package_path(folder, name)
        if kit_tagged(mesh):
            # Already replaced by r002: its look is applied below, never reset from the r001 export.
            theme.set_editor_property(prop, mesh)
            found += 1
            continue
        apply_material(mesh, texture, asset_id, name, look, parents)
        theme.set_editor_property(prop, mesh)
        if nanite_enabled(mesh):
            if fix_r001_nanite:
                force_nanite_off(mesh)
            else:
                unreal.log("THEME_NANITE_ON_R001 {} (set {}=1 to switch it off; that changes the asset's bytes)".format(package, theme_plan.NANITE_FIX_ENV))
        asset_line(asset_id, package, "kept-r001", r001_source(asset_id, name), mesh)
        found += 1

    slots = {prop: [None] * count for prop, count in theme_plan.ARRAY_SIZES.items()}
    kit_loaded = 0
    for asset_id, revision, name, folder, kind, prop, index, look in theme_plan.R002:
        package = theme_plan.package_path(folder, name)
        source = theme_plan.export_path(asset_id, revision, name)
        exists = library.does_asset_exist(package)
        replace = asset_id in replacements
        if exists and kit_tagged(unreal.load_asset(package)):
            mesh, texture, action = unreal.load_asset(package), None, "present"
        elif exists and not replace:
            reason = "r001 package kept (replace with {}={})".format(theme_plan.REIMPORT_ENV, asset_id) if asset_id in theme_plan.REPLACEABLE else "exists and is not tagged as the kit; remove it deliberately to import"
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
            library.set_metadata_tag(mesh, theme_plan.REVISION_TAG, KIT_TAG_VALUE)
            mesh.mark_package_dirty()
        force_nanite_off(mesh)
        check_lod_count(mesh, asset_id, revision)
        apply_material(mesh, texture, asset_id, name, look, parents, kind=kind, revision=2, source=source)
        asset_line(asset_id, package, action, source, mesh, "lod_exports={}".format(len(theme_plan.lod_exports(asset_id, revision, name))))
        if index is None:
            theme.set_editor_property(prop, mesh)
        else:
            slots[prop][index] = mesh
        kit_loaded += 1
    for prop, items in slots.items():
        set_list(theme, prop, items)
    metric = all(kit_tagged(unreal.load_asset(theme_plan.package_path(row[3], row[2]))) for row in theme_plan.R002 if row[0] in theme_plan.METRIC_GATE)
    if bool(theme.get_editor_property("metric_kit")) != metric:
        theme.set_editor_property("metric_kit", metric)

    # Save only what this run changed: unchanged meshes and textures keep their exact bytes.
    if not library.save_directory(THEME, only_if_is_dirty=True, recursive=True):
        raise RuntimeError("Unable to save " + THEME)
    after = theme_hashes()
    changed = sorted(path for path in set(before) | set(after) if before.get(path) != after.get(path))
    for path in changed:
        state = "added" if path not in before else "removed" if path not in after else "changed"
        unreal.log("THEME_FILE {} {} sha256={}".format(state, path, after.get(path, "-")))
    unreal.log("THEME_FILES_CHANGED {}".format(len(changed)))
    unreal.log("THEME_IMPORT_COMPLETE {}/{} meshes, kit {}/{} meshes, metric_kit={}".format(found, len(ASSETS), kit_loaded, len(theme_plan.R002), metric))


main()
