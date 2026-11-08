import unreal
D = "/Game/DnDDiceRoll/Spike140/"
for n in ("SM_World_VigilSpike", "SM_World_VigilSpike_Occluders"):
    m = unreal.load_asset(D + n)
    ns = m.get_editor_property("nanite_settings"); ns.enabled = False
    m.set_editor_property("nanite_settings", ns)
    unreal.log("SPIKE_FIX %s before tris=%s" % (n, m.get_num_triangles(0)))
    m.post_edit_change()
    unreal.log("SPIKE_FIX %s after tris=%s verts=%s" % (n, m.get_num_triangles(0), m.get_num_vertices(0) if hasattr(m, "get_num_vertices") else "?"))
    unreal.EditorAssetLibrary.save_loaded_asset(m)
