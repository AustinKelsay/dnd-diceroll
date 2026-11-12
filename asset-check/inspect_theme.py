"""Read-only inspection of M_DiceRollTheme. Run inside UnrealEditor-Cmd (-run=pythonscript). Writes JSON to $INSPECT_OUT."""
import json
import os
import unreal

mel = unreal.MaterialEditingLibrary
PATH = "/Game/DnDDiceRoll/Theme/Materials/M_DiceRollTheme"
OUT = os.environ["INSPECT_OUT"]
res = {"path": PATH, "errors": []}


def safe(label, fn):
    try:
        return fn()
    except Exception as exc:  # report, never hide
        res["errors"].append("{}: {}".format(label, exc))
        return None


mat = unreal.load_asset(PATH)
res["class"] = mat.get_class().get_name()
res["revision_tag"] = unreal.EditorAssetLibrary.get_metadata_tag(mat, "DiceRollGraphRevision")
res["usage"] = {}
for name in ("MATUSAGE_NANITE", "MATUSAGE_SKELETAL_MESH", "MATUSAGE_MORPH_TARGETS", "MATUSAGE_INSTANCED_STATIC_MESHES",
             "MATUSAGE_STATIC_LIGHTING", "MATUSAGE_PARTICLE_SPRITES", "MATUSAGE_MESH_PARTICLES", "MATUSAGE_SPLINE_MESH", "MATUSAGE_CLOTH"):
    usage = getattr(unreal.MaterialUsage, name, None)
    if usage is not None:
        res["usage"][name] = safe(name, lambda u=usage: bool(mel.has_material_usage(mat, u)))
res["props"] = {}
for prop in ("used_with_nanite", "used_with_skeletal_mesh", "used_with_morph_targets", "used_with_instanced_static_meshes",
             "two_sided", "blend_mode", "shading_model", "material_domain", "used_with_static_lighting"):
    res["props"][prop] = safe(prop, lambda p=prop: str(mat.get_editor_property(p)))

# expressions
exprs = None
n = safe("count", lambda: mel.get_num_material_expressions(mat))
res["num_expressions"] = n
for getter in (lambda: list(mel.get_material_expressions(mat)),
               lambda: list(mat.get_editor_property("expression_collection").get_editor_property("expressions")),
               lambda: list(mat.get_editor_property("expressions"))):
    exprs = safe("enumerate", getter)
    if exprs:
        break
res["enumerated"] = len(exprs) if exprs else 0
nodes = []
power_nodes = []
if exprs:
    index = {id(e): i for i, e in enumerate(exprs)}
    for e in exprs:
        item = {"class": e.get_class().get_name()}
        for prop in ("parameter_name", "group", "default_value", "r", "const_exponent", "const_a", "const_b", "desc"):
            v = safe(prop, lambda p=prop: e.get_editor_property(p)) if prop in ("parameter_name", "default_value", "r", "const_exponent", "desc") else None
            if v is not None and (not isinstance(v, str) or v):
                item[prop] = str(v)
        ins = safe("inputs", lambda e=e: mel.get_inputs_for_material_expression(mat, e)) or []
        item["inputs_wired"] = [ (x.get_class().get_name() if x else None) for x in ins]
        item["inputs_connected"] = sum(1 for x in ins if x)
        names = safe("input_names", lambda e=e: list(mel.get_material_expression_input_names(e)))
        if names:
            item["input_names"] = [str(x) for x in names]
        item["x"] = safe("x", lambda e=e: e.get_editor_property("material_expression_editor_x"))
        item["y"] = safe("y", lambda e=e: e.get_editor_property("material_expression_editor_y"))
        nodes.append(item)
        if item["class"] == "MaterialExpressionPower":
            power_nodes.append(item)
res["power_nodes"] = power_nodes
# Stable, order-independent signature of the graph for A/B comparison
res["graph_signature"] = sorted(json.dumps({k: v for k, v in n.items() if k not in ("x", "y")}, sort_keys=True) for n in nodes)
res["param_names"] = sorted(n.get("parameter_name") for n in nodes if n.get("parameter_name"))
res["node_class_counts"] = {}
for n in nodes:
    res["node_class_counts"][n["class"]] = res["node_class_counts"].get(n["class"], 0) + 1

# material property connections
res["property_inputs"] = {}
for prop in ("MP_BASE_COLOR", "MP_NORMAL", "MP_ROUGHNESS", "MP_METALLIC", "MP_EMISSIVE_COLOR", "MP_OPACITY", "MP_OPACITY_MASK",
             "MP_AMBIENT_OCCLUSION", "MP_SPECULAR", "MP_WORLD_POSITION_OFFSET"):
    p = getattr(unreal.MaterialProperty, prop, None)
    if p is not None:
        node = safe(prop, lambda p=p: mel.get_material_property_input_node(mat, p))
        res["property_inputs"][prop] = node.get_class().get_name() if node else None

# every MI that inherits the master: effective Nanite / skeletal
res["children"] = {}
ar = unreal.AssetRegistryHelpers.get_asset_registry()
for a in ar.get_assets_by_path("/Game/DnDDiceRoll", recursive=True):
    if str(a.asset_class_path.asset_name) == "MaterialInstanceConstant":
        mi = safe("load " + str(a.asset_name), lambda a=a: a.get_asset())
        if mi is None:
            continue
        parent = mi.get_editor_property("parent")
        if parent and parent.get_path_name().startswith(PATH):
            res["children"][str(a.asset_name)] = {
                "nanite": safe("mi nanite", lambda mi=mi: bool(mel.has_material_usage(mi.get_base_material(), unreal.MaterialUsage.MATUSAGE_NANITE))),
            }
res["child_count"] = len(res["children"])
with open(OUT, "w") as fh:
    json.dump(res, fh, indent=1, sort_keys=True)
unreal.log("INSPECT_THEME_DONE " + OUT)
