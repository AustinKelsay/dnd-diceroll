"""Create the contextual-grid overlay material for the themed battle board (issue #142).

Run headless with the editor closed:
  UnrealEditor DnDDiceRoll.uproject -run=pythonscript -script=<repo>/Content/Python/create_board_overlay_material.py -unattended -nullrhi

Creates /Game/DnDDiceRoll/Theme/Materials/M_BoardOverlay: translucent, unlit, two-sided, emissive "Color" and
opacity "Opacity". The battle view loads it by that path for the reach contour, corner ticks, path ribbon, brackets,
full-grid lines and footprint rings. Without the asset the view still draws, with dimmed opaque lines in place of
translucent ones.

Graph revision (metadata tag DiceRollGraphRevision, as the theme materials): an existing asset whose tag equals
GRAPH_REVISION is left alone; any other (or none) is rebuilt IN PLACE, so the asset, its path and every instance survive.
board-overlay-2 draws with the depth test off, so the reach contour, path ribbon, brackets, blocked crosses and footprint
rings stay visible through tall architecture (pillars, arches). The bars are thin, so drawing over figures is acceptable.
board-overlay-3 (#144) multiplies the emissive colour by PerInstanceCustomData3Vector (slots 0-2, default 1, 1, 1) and the opacity by
PerInstanceCustomData slot 3 (default 1): the footprint rings, the markers and every style of the contextual grid share one instanced
component of a battle board and carry each bar's colour and opacity per instance. A component with no custom data (the dimmed-line fallback
never uses this material, and a loose ring set keeps its own Color and Opacity parameters) reads the defaults and draws exactly as before.
"""
import unreal

THEME = "/Game/DnDDiceRoll/Theme"
PACKAGE = THEME + "/Materials/M_BoardOverlay"
REVISION_TAG = "DiceRollGraphRevision"
GRAPH_REVISION = "board-overlay-3"

tools = unreal.AssetToolsHelpers.get_asset_tools()
library = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary


def expression(material, kind, x, y, **props):
    node = mel.create_material_expression(material, kind, x, y)
    for key, value in props.items():
        node.set_editor_property(key, value)
    return node


def build(material):
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    material.set_editor_property("two_sided", True)
    # Draw over opaque pixels even when behind them (UMaterial::bDisableDepthTest, translucency only): architecture in front
    # of the board no longer hides the overlay.
    material.set_editor_property("disable_depth_test", True)
    # The bars and rings are instanced static meshes; cooked builds need the usage flag set up front.
    material.set_editor_property("used_with_instanced_static_meshes", True)
    color = expression(material, unreal.MaterialExpressionVectorParameter, -500, 0, parameter_name="Color",
                       default_value=unreal.LinearColor(1, 1, 1, 1))
    opacity = expression(material, unreal.MaterialExpressionScalarParameter, -500, 250, parameter_name="Opacity", default_value=1.0)
    mask = expression(material, unreal.MaterialExpressionComponentMask, -250, 0, r=True, g=True, b=True, a=False)
    mel.connect_material_expressions(color, "", mask, "")
    instance_color = expression(material, unreal.MaterialExpressionPerInstanceCustomData3Vector, -500, 120,
                                const_default_value=unreal.LinearColor(1, 1, 1, 1), data_index=0)
    tinted = expression(material, unreal.MaterialExpressionMultiply, -50, 0)
    mel.connect_material_expressions(mask, "", tinted, "A")
    mel.connect_material_expressions(instance_color, "", tinted, "B")
    mel.connect_material_property(tinted, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    instance_opacity = expression(material, unreal.MaterialExpressionPerInstanceCustomData, -500, 400, const_default_value=1.0, data_index=3)
    shown = expression(material, unreal.MaterialExpressionMultiply, -50, 250)
    mel.connect_material_expressions(opacity, "", shown, "A")
    mel.connect_material_expressions(instance_opacity, "", shown, "B")
    mel.connect_material_property(shown, "", unreal.MaterialProperty.MP_OPACITY)
    mel.recompile_material(material)


def main():
    if library.does_asset_exist(PACKAGE):
        material = unreal.load_asset(PACKAGE)
        if library.get_metadata_tag(material, REVISION_TAG) == GRAPH_REVISION:
            unreal.log("M_BoardOverlay is current ({}); left unchanged.".format(GRAPH_REVISION))
            return
        mel.delete_all_material_expressions(material)
        build(material)
        library.set_metadata_tag(material, REVISION_TAG, GRAPH_REVISION)
        material.modify()
        if not library.save_asset(PACKAGE, only_if_is_dirty=False):
            raise RuntimeError("Unable to save " + PACKAGE)
        unreal.log("Rebuilt {} in place as {}".format(PACKAGE, GRAPH_REVISION))
        return
    material = tools.create_asset("M_BoardOverlay", THEME + "/Materials", unreal.Material, unreal.MaterialFactoryNew())
    build(material)
    library.set_metadata_tag(material, REVISION_TAG, GRAPH_REVISION)
    if not library.save_asset(PACKAGE, only_if_is_dirty=False):
        raise RuntimeError("Unable to save " + PACKAGE)
    unreal.log("Created " + PACKAGE)


main()
