"""Build the spell VFX trial assets under /Game/DnDDiceRoll/Theme/SpellFX (candlelit arcane direction).

Run headless with the editor closed:
  UnrealEditor-Cmd DnDDiceRoll.uproject -run=pythonscript -script=<repo>/Content/Python/build_spell_fx.py -unattended -nullrhi
Creates (or rebuilds) M_SpellFX, the single parent material for every spell effect, and its
MI_SpellFX_Evocation instance, then imports the low-poly meshes from Art/spellfx-evocation/r001
(Nanite off, no collision). Re-running replaces the material graph so look tweaks stay in one place.
"""
import os
import unreal

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
FOLDER = "/Game/DnDDiceRoll/Theme/SpellFX"
MASTER = FOLDER + "/M_SpellFX"
EVOCATION = FOLDER + "/MI_SpellFX_Evocation"
MESHES = ["SM_SpellFX_Ico", "SM_SpellFX_Shard", "SM_SpellFX_CellFrame", "SM_SpellFX_Burst"]
SOURCE = os.path.join(ROOT, "Art", "spellfx-evocation", "r001")

tools = unreal.AssetToolsHelpers.get_asset_tools()
library = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary

# Matte stepped colour, blocky grit, ink shell on back faces, ragged break on the leading edge only.
SHADER = r"""
// Matte late-PS1 (Gauntlet Dark Legacy grit): hard 3-step bands, nearest-neighbour 6 cm texels, no glow, ink rim.
float3 N = normalize(Nw);
float3 V = normalize(Cam);
float F = 1.0 - saturate(abs(dot(N, V)));
float band = floor(saturate(F * 1.8) * 3.0) / 2.0;
band = saturate(band);
float front = Sign > 0.0 ? 1.0 : 0.0;
// Blocky texel noise: one value per 6 cm cell, no interpolation (point-sampled low-res look).
float3 t = floor(Wp / 6.0);
float g = frac(sin(dot(t, float3(127.1, 311.7, 74.7))) * 43758.5453);
float grit = 0.62 + floor(g * 3.0) * 0.19;
// Coarser 18 cm blocks for the ragged leading edge (hard pixel steps, not a smooth wash).
float3 e = floor(Wp / 18.0);
float n = frac(sin(dot(e, float3(269.5, 183.3, 246.1))) * 43758.5453);
float3 L = Lead;
float lead = Radial > 0.5 ? saturate(F * 2.0 - 1.2) : saturate(dot(N, normalize(L + float3(0.0001, 0.0, 0.0))));
float edge = step(0.5, saturate((n - lead * Edge) * 8.0 + 0.6));
if (InkShell > 0.5) { return float4(Ink, Opacity * (1.0 - front) * step(0.34, F)); }
float rim = step(0.8, F);
float3 col = lerp(Core, Hue, band) * grit;
// Muddy: pull a quarter toward grey, never above authored brightness.
float lum = dot(col, float3(0.3, 0.59, 0.11));
col = lerp(col, lum.xxx, 0.25) * min(Glow, 1.0);
return float4(lerp(col, Ink, rim), Opacity * front * edge);
"""


def expression(material, kind, x, y, **props):
    node = mel.create_material_expression(material, kind, x, y)
    for key, value in props.items():
        node.set_editor_property(key, value)
    return node


def master_material():
    if library.does_asset_exist(MASTER):
        material = unreal.load_asset(MASTER)
        mel.delete_all_material_expressions(material)
    else:
        material = tools.create_asset("M_SpellFX", FOLDER, unreal.Material, unreal.MaterialFactoryNew())
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    material.set_editor_property("two_sided", True)
    material.set_editor_property("used_with_static_lighting", False)
    vectors = [("Core", (1.0, 0.93, 0.62)), ("Hue", (1.0, 0.42, 0.08)), ("Ink", (0.035, 0.02, 0.03)), ("Lead", (1.0, 0.0, 0.0))]
    scalars = [("Opacity", 1.0), ("Glow", 1.0), ("InkShell", 0.0), ("Radial", 0.0), ("Edge", 0.6)]
    custom = expression(material, unreal.MaterialExpressionCustom, -200, 0, code=SHADER, output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT4, description="SpellFX")
    inputs = []
    names = [v[0] for v in vectors] + [s[0] for s in scalars] + ["Nw", "Cam", "Wp", "Sign"]
    for name in names:
        item = unreal.CustomInput()
        item.set_editor_property("input_name", name)
        inputs.append(item)
    custom.set_editor_property("inputs", inputs)
    y = -400
    for name, value in vectors:
        node = expression(material, unreal.MaterialExpressionVectorParameter, -700, y, parameter_name=name, default_value=unreal.LinearColor(value[0], value[1], value[2], 1.0))
        mask = expression(material, unreal.MaterialExpressionComponentMask, -450, y, r=True, g=True, b=True, a=False)
        mel.connect_material_expressions(node, "", mask, "")
        mel.connect_material_expressions(mask, "", custom, name)
        y += 120
    for name, value in scalars:
        node = expression(material, unreal.MaterialExpressionScalarParameter, -700, y, parameter_name=name, default_value=value)
        mel.connect_material_expressions(node, "", custom, name)
        y += 80
    normal = expression(material, unreal.MaterialExpressionVertexNormalWS, -700, y)
    camera = expression(material, unreal.MaterialExpressionCameraVectorWS, -700, y + 80)
    world = expression(material, unreal.MaterialExpressionWorldPosition, -700, y + 160)
    sign = expression(material, unreal.MaterialExpressionTwoSidedSign, -700, y + 240)
    mel.connect_material_expressions(normal, "", custom, "Nw")
    mel.connect_material_expressions(camera, "", custom, "Cam")
    mel.connect_material_expressions(world, "", custom, "Wp")
    mel.connect_material_expressions(sign, "", custom, "Sign")
    rgb = expression(material, unreal.MaterialExpressionComponentMask, 100, -50, r=True, g=True, b=True, a=False)
    alpha = expression(material, unreal.MaterialExpressionComponentMask, 100, 80, r=False, g=False, b=False, a=True)
    mel.connect_material_expressions(custom, "", rgb, "")
    mel.connect_material_expressions(custom, "", alpha, "")
    # Cancel auto-exposure so the flat colours read the same on dark (Vault) and bright boards
    # instead of blowing out to white; Glow then means display brightness (1 = as authored).
    inverse = expression(material, unreal.MaterialExpressionEyeAdaptationInverse, 100, -200)
    emissive = expression(material, unreal.MaterialExpressionMultiply, 300, -100)
    mel.connect_material_expressions(rgb, "", emissive, "A")
    mel.connect_material_expressions(inverse, "", emissive, "B")
    mel.connect_material_property(emissive, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.connect_material_property(alpha, "", unreal.MaterialProperty.MP_OPACITY)
    mel.recompile_material(material)
    library.save_asset(MASTER)
    return material


def evocation_instance(parent):
    if library.does_asset_exist(EVOCATION):
        instance = unreal.load_asset(EVOCATION)
    else:
        instance = tools.create_asset("MI_SpellFX_Evocation", FOLDER, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(instance, parent)
    # Evocation: white-hot core; the element hue (fire: ember orange) is set per spell at runtime.
    mel.set_material_instance_vector_parameter_value(instance, "Core", unreal.LinearColor(1.0, 0.96, 0.82, 1.0))
    mel.set_material_instance_vector_parameter_value(instance, "Hue", unreal.LinearColor(1.0, 0.42, 0.08, 1.0))
    mel.set_material_instance_scalar_parameter_value(instance, "Glow", 1.0)
    library.save_asset(EVOCATION)


def import_mesh(name):
    package = FOLDER + "/" + name
    source = os.path.join(SOURCE, name + ".obj")
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", source)
    task.set_editor_property("destination_path", FOLDER)
    task.set_editor_property("destination_name", name)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("save", False)
    tools.import_asset_tasks([task])
    mesh = unreal.load_asset(package)
    if not isinstance(mesh, unreal.StaticMesh):
        found = [unreal.load_asset(p) for p in task.get_editor_property("imported_object_paths")]
        found = [a for a in found if isinstance(a, unreal.StaticMesh)]
        if len(found) != 1:
            raise RuntimeError("SPELLFX_IMPORT_FAILED " + source)
        library.rename_asset(found[0].get_path_name(), package)
        mesh = unreal.load_asset(package)
    settings = mesh.get_editor_property("nanite_settings")
    settings.set_editor_property("enabled", False)
    mesh.set_editor_property("nanite_settings", settings)
    body = mesh.get_editor_property("body_setup")
    if body:
        body.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_SIMPLE_AS_COMPLEX)
    library.save_asset(package)
    # Interchange may drop material/texture side assets beside the mesh; spell meshes use M_SpellFX only.
    for path in library.list_assets(FOLDER, recursive=False):
        asset_name = path.split("/")[-1].split(".")[0]
        if asset_name not in MESHES + ["M_SpellFX", "MI_SpellFX_Evocation"]:
            library.delete_asset(path)
    unreal.log("SPELLFX_MESH " + package)


parent = master_material()
evocation_instance(parent)
for mesh_name in MESHES:
    import_mesh(mesh_name)
unreal.log("SPELLFX_BUILD_OK")
