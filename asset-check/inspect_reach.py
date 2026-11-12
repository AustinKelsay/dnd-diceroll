import json, os, unreal
mel = unreal.MaterialEditingLibrary
mat = unreal.load_asset("/Game/DnDDiceRoll/Theme/Materials/M_DiceRollTheme")
exprs = list(mel.get_material_expressions(mat))
by_name = {e.get_name(): e for e in exprs}
seen = set(); stack = []
roots = {}
for prop in ("MP_BASE_COLOR","MP_NORMAL","MP_ROUGHNESS","MP_METALLIC","MP_EMISSIVE_COLOR","MP_OPACITY","MP_OPACITY_MASK","MP_AMBIENT_OCCLUSION","MP_SPECULAR","MP_WORLD_POSITION_OFFSET"):
    try:
        node = mel.get_material_property_input_node(mat, getattr(unreal.MaterialProperty, prop))
    except Exception:
        continue
    if node:
        roots[prop] = node.get_name(); stack.append(node)
while stack:
    n = stack.pop()
    if n.get_name() in seen:
        continue
    seen.add(n.get_name())
    for x in mel.get_inputs_for_material_expression(mat, n):
        if x:
            stack.append(x)
cls = lambda names: sorted({by_name[n].get_class().get_name() for n in names})
unreached = [n for n in by_name if n not in seen]
out = {"total": len(exprs), "reachable": len(seen), "unreachable": len(unreached), "roots": roots,
       "power_reachable": {n: (n in seen) for n, e in by_name.items() if e.get_class().get_name() == "MaterialExpressionPower"},
       "unreachable_by_class": {}}
for n in unreached:
    c = by_name[n].get_class().get_name(); out["unreachable_by_class"][c] = out["unreachable_by_class"].get(c, 0) + 1
json.dump(out, open(os.environ["INSPECT_OUT"], "w"), indent=1, sort_keys=True)
unreal.log("INSPECT_REACH_DONE")
