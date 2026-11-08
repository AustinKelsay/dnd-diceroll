import unreal
D = "/Game/DnDDiceRoll/Spike140/"
def log(t): unreal.log("SPIKE_STATS " + t)
for n in ("SM_World_VigilSpike", "SM_World_VigilSpike_Occluders"):
    m = unreal.load_asset(D + n)
    try: log("%s verts=%s" % (n, unreal.EditorStaticMeshLibrary.get_number_verts(m, 0)))
    except Exception as e: log("verts fail %s" % e)
    try: log("%s lod0 tris=%s" % (n, unreal.GeometryScriptLibrary_StaticMeshFunctions.get_mesh_lod_triangle_count(m, unreal.GeometryScriptMeshReadLOD(unreal.GeometryScriptLODType.MAX_AVAILABLE, 0))) if False else "skip")
    except Exception as e: log("tri fail %s" % e)
    ns = m.get_editor_property("nanite_settings"); ns.enabled = False; m.set_editor_property("nanite_settings", ns)
    unreal.EditorStaticMeshLibrary.set_lod_from_static_mesh if False else None
    log("%s nanite now %s" % (n, m.get_editor_property("nanite_settings").enabled))
    try: log("%s tris=%s" % (n, m.get_num_triangles(0) if hasattr(m, "get_num_triangles") else "n/a"))
    except Exception as e: log("tri fail %s" % e)
    unreal.EditorAssetLibrary.save_loaded_asset(m)
for n in ("A_FighterSpike_Idle", "A_FighterSpike_Walk"):
    a = unreal.load_asset(D + n)
    names = unreal.AnimationLibrary.get_animation_track_names(a)
    log("%s len=%.4f frames=%s tracks=%d rate=%s" % (n, a.get_play_length(), unreal.AnimationLibrary.get_num_frames(a), len(names), "n/a"))
sk = unreal.load_asset(D + "SK_Hero_FighterSpike_Skeleton")
try: log("skeleton bones=%d" % len(unreal.AnimationLibrary.get_animation_track_names(unreal.load_asset(D + "A_FighterSpike_Idle"))))
except Exception as e: log("x %s" % e)
