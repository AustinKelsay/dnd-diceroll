import json, os, unreal
mel = unreal.MaterialEditingLibrary
mat = unreal.load_asset("/Game/DnDDiceRoll/Theme/Materials/M_DiceRollTheme")
exprs = list(mel.get_material_expressions(mat))
powers = {e.get_name(): {"wired_inputs": [x.get_class().get_name() if x else None for x in mel.get_inputs_for_material_expression(mat, e)], "consumers": []} for e in exprs if e.get_class().get_name() == "MaterialExpressionPower"}
for e in exprs:
    for x in mel.get_inputs_for_material_expression(mat, e):
        if x and x.get_name() in powers:
            powers[x.get_name()]["consumers"].append(e.get_class().get_name() + ":" + e.get_name())
json.dump(powers, open(os.environ["INSPECT_OUT"], "w"), indent=1, sort_keys=True)
unreal.log("INSPECT_POWER_DONE")
