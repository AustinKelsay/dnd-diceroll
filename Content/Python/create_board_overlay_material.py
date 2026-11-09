"""Create the contextual-grid overlay material for the themed battle board (issue #142).

Run headless with the editor closed:
  UnrealEditor DnDDiceRoll.uproject -run=pythonscript -script=<repo>/Content/Python/create_board_overlay_material.py -unattended -nullrhi

Creates /Game/DnDDiceRoll/Theme/Materials/M_BoardOverlay: translucent, unlit, two-sided, emissive "Color" and
opacity "Opacity". The battle view loads it by that path for the reach contour, corner ticks, path ribbon, brackets,
full-grid lines and footprint rings. An existing asset is left alone (reported, never overwritten); without the
asset the view still draws, with dimmed opaque lines in place of translucent ones.
"""
import unreal

THEME = "/Game/DnDDiceRoll/Theme"
PACKAGE = THEME + "/Materials/M_BoardOverlay"

tools = unreal.AssetToolsHelpers.get_asset_tools()
library = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary


def expression(material, kind, x, y, **props):
    node = mel.create_material_expression(material, kind, x, y)
    for key, value in props.items():
        node.set_editor_property(key, value)
    return node


def main():
    if library.does_asset_exist(PACKAGE):
        unreal.log("M_BoardOverlay exists; left unchanged. Delete it deliberately to recreate it.")
        return
    material = tools.create_asset("M_BoardOverlay", THEME + "/Materials", unreal.Material, unreal.MaterialFactoryNew())
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    material.set_editor_property("two_sided", True)
    # The bars and rings are instanced static meshes; cooked builds need the usage flag set up front.
    material.set_editor_property("used_with_instanced_static_meshes", True)
    color = expression(material, unreal.MaterialExpressionVectorParameter, -500, 0, parameter_name="Color",
                       default_value=unreal.LinearColor(1, 1, 1, 1))
    opacity = expression(material, unreal.MaterialExpressionScalarParameter, -500, 250, parameter_name="Opacity", default_value=1.0)
    mask = expression(material, unreal.MaterialExpressionComponentMask, -250, 0, r=True, g=True, b=True, a=False)
    mel.connect_material_expressions(color, "", mask, "")
    mel.connect_material_property(mask, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.connect_material_property(opacity, "", unreal.MaterialProperty.MP_OPACITY)
    mel.recompile_material(material)
    if not library.save_asset(PACKAGE, only_if_is_dirty=False):
        raise RuntimeError("Unable to save " + PACKAGE)
    unreal.log("Created " + PACKAGE)


main()
