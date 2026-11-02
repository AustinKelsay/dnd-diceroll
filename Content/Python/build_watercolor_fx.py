"""Build the watercolor spell-effect material (/Game/DnDDiceRoll/FX/M_WatercolorFX).

Run headless with the editor closed:
  UnrealEditor-Cmd DnDDiceRoll.uproject -run=pythonscript -script=<repo>/Content/Python/build_watercolor_fx.py -unattended -nullrhi

The material is procedural: no textures are imported. It is an unlit "wash":
  * stepped (banded) shading from a 3-step Fresnel term, like layered glazes;
  * darker pigment pooling toward silhouette edges;
  * paper grain from a world-space noise that breaks up the flat colour.
Parameters: Color (vector, the wash tint, may exceed 1 for glow) and Grain (scalar 0..1).
AExpeditionBattleView uses it only for spell effects (Fire Bolt, Magic Missile, Cure Wounds)
and falls back to the engine BasicShapeMaterial when the asset is missing.
The asset is rebuilt from scratch on every run so this script stays the source of truth.
"""
import unreal

FOLDER = "/Game/DnDDiceRoll/FX"
NAME = "M_WatercolorFX"
PATH = FOLDER + "/" + NAME

tools = unreal.AssetToolsHelpers.get_asset_tools()
library = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary


def node(material, kind, x, y, **props):
    expression = mel.create_material_expression(material, kind, x, y)
    for key, value in props.items():
        expression.set_editor_property(key, value)
    return expression


def link(source, output, target, pin):
    if not mel.connect_material_expressions(source, output, target, pin):
        raise RuntimeError("WATERCOLOR_LINK_FAILED %s.%s -> %s.%s" % (source.get_name(), output, target.get_name(), pin))


if library.does_asset_exist(PATH):
    library.delete_asset(PATH)
material = tools.create_asset(NAME, FOLDER, unreal.Material, unreal.MaterialFactoryNew())
material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
material.set_editor_property("two_sided", True)

color = node(material, unreal.MaterialExpressionVectorParameter, -1200, -200, parameter_name="Color",
             default_value=unreal.LinearColor(1, 1, 1, 1))
grain_amount = node(material, unreal.MaterialExpressionScalarParameter, -1200, 400, parameter_name="Grain", default_value=0.35)
tint = node(material, unreal.MaterialExpressionComponentMask, -1000, -200, r=True, g=True, b=True, a=False)
link(color, "", tint, "")

# Stepped edge term: floor(fresnel * 3) / 3 gives three flat glaze bands.
fresnel = node(material, unreal.MaterialExpressionFresnel, -1200, 100, exponent=1.6, base_reflect_fraction=0.0)
bands = node(material, unreal.MaterialExpressionMultiply, -1000, 100, const_b=3.0)
link(fresnel, "", bands, "A")
floor = node(material, unreal.MaterialExpressionFloor, -850, 100)
link(bands, "", floor, "")
stepped = node(material, unreal.MaterialExpressionDivide, -700, 100, const_b=3.0)
link(floor, "", stepped, "A")
# Pigment pools at the rim: full tint in the middle, 35% at the darkest band.
pooling = node(material, unreal.MaterialExpressionLinearInterpolate, -550, 100, const_a=1.0, const_b=0.35)
link(stepped, "", pooling, "Alpha")

# Paper grain: world-space value noise, mapped to a gentle 0.8..1.05 brightness wobble.
noise = node(material, unreal.MaterialExpressionNoise, -1000, 350, scale=0.08, levels=3,
             output_min=0.0, output_max=1.0)
wobble = node(material, unreal.MaterialExpressionLinearInterpolate, -800, 350, const_a=0.8, const_b=1.05)
link(noise, "", wobble, "Alpha")
paper = node(material, unreal.MaterialExpressionLinearInterpolate, -600, 350, const_a=1.0)
link(wobble, "", paper, "B")
link(grain_amount, "", paper, "Alpha")

washed = node(material, unreal.MaterialExpressionMultiply, -350, -50)
link(tint, "", washed, "A")
link(pooling, "", washed, "B")
final = node(material, unreal.MaterialExpressionMultiply, -200, 100)
link(washed, "", final, "A")
link(paper, "", final, "B")
mel.connect_material_property(final, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
mel.recompile_material(material)
if not library.save_asset(PATH, only_if_is_dirty=False):
    raise RuntimeError("Unable to save " + PATH)
unreal.log("WATERCOLOR_FX_OK " + PATH)