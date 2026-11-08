import unreal
mel = unreal.MaterialEditingLibrary
for n in ("M_SpikeOverlay", "M_Spike_Std"):
    m = unreal.load_asset("/Game/DnDDiceRoll/Spike140/" + n)
    unreal.log("SPIKE_DIAG %s blend=%s shading=%s" % (n, m.get_editor_property("blend_mode"), m.get_editor_property("shading_model")))
    for prop in (unreal.MaterialProperty.MP_OPACITY, unreal.MaterialProperty.MP_EMISSIVE_COLOR, unreal.MaterialProperty.MP_BASE_COLOR):
        node = mel.get_material_property_input_node(m, prop)
        unreal.log("SPIKE_DIAG   %s -> %s" % (prop, node))
    unreal.log("SPIKE_DIAG num expressions %s" % mel.get_num_material_expressions(m))
t = unreal.load_asset("/Game/DnDDiceRoll/Spike140/T_World_VigilSpike_Atlas_Linear")
unreal.log("SPIKE_DIAG atlas %sx%s filter=%s srgb=%s" % (t.blueprint_get_size_x(), t.blueprint_get_size_y(), t.get_editor_property("filter"), t.get_editor_property("srgb")))
