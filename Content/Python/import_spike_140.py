"""#140 spike: import the Blender spike assets into /Game/DnDDiceRoll/Spike140 (throwaway).

Run headless with the editor closed (one UnrealEditor-Cmd process):
  UnrealEditor-Cmd DnDDiceRoll.uproject -run=pythonscript -script=<repo>/Content/Python/import_spike_140.py -unattended -nosplash
Meshes/textures already imported are kept (delete the folder to reimport). Materials are rebuilt every run.
Prints SPIKE_IMPORT lines with bone counts, clip lengths and triangle counts.
"""
import os
import unreal

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
SRC = os.path.join(ROOT, "Art", "spike-140", "exports")
DEST = "/Game/DnDDiceRoll/Spike140"
STAGE = DEST + "/_Import"

tools = unreal.AssetToolsHelpers.get_asset_tools()
library = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary


def log(text):
    unreal.log("SPIKE_IMPORT " + text)


def import_file(path, stage):
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", path)
    task.set_editor_property("destination_path", stage)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("save", False)
    tools.import_asset_tasks([task])
    return [unreal.load_asset(p) for p in task.get_editor_property("imported_object_paths")]


def move(asset, target):
    if not library.rename_asset(asset.get_path_name(), target):
        raise RuntimeError("Unable to move " + asset.get_path_name() + " to " + target)
    return unreal.load_asset(target)


def tune_texture(texture, nearest):
    texture.set_editor_property("filter", unreal.TextureFilter.TF_NEAREST if nearest else unreal.TextureFilter.TF_TRILINEAR)
    texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON)
    texture.set_editor_property("srgb", True)
    texture.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_WORLD)
    texture.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_SIMPLE_AVERAGE)
    texture.set_editor_property("never_stream", True)


def texture_pair(texture, base):
    """Nearest and trilinear copies named T_<base>_Nearest / _Linear."""
    out = {}
    for suffix, nearest in (("Nearest", True), ("Linear", False)):
        path = DEST + "/T_" + base + "_" + suffix
        if library.does_asset_exist(path):
            copy = unreal.load_asset(path)
        else:
            copy = library.duplicate_asset(texture.get_path_name(), path)
        tune_texture(copy, nearest)
        out[suffix] = copy
    return out


def expression(material, kind, x, y, **props):
    node = mel.create_material_expression(material, kind, x, y)
    for key, value in props.items():
        node.set_editor_property(key, value)
    return node


def fresh_material(name, **props):
    path = DEST + "/" + name
    if library.does_asset_exist(path):
        material = unreal.load_asset(path)
        mel.delete_all_material_expressions(material)
    else:
        material = tools.create_asset(name, DEST, unreal.Material, unreal.MaterialFactoryNew())
    for key, value in props.items():
        material.set_editor_property(key, value)
    return material


def rgb(material, source, x, y):
    mask = expression(material, unreal.MaterialExpressionComponentMask, x, y, r=True, g=True, b=True, a=False)
    mel.connect_material_expressions(source, "", mask, "")
    return mask


def mul(material, a, b, x, y, a_out="", b_out=""):
    node = expression(material, unreal.MaterialExpressionMultiply, x, y)
    mel.connect_material_expressions(a, a_out, node, "A")
    mel.connect_material_expressions(b, b_out, node, "B")
    return node


def add(material, a, b, x, y):
    node = expression(material, unreal.MaterialExpressionAdd, x, y)
    mel.connect_material_expressions(a, "", node, "A")
    mel.connect_material_expressions(b, "", node, "B")
    return node


def scalar(material, name, value, x, y):
    return expression(material, unreal.MaterialExpressionScalarParameter, x, y, parameter_name=name, default_value=value)


def overlay_material():
    material = fresh_material("M_SpikeOverlay", blend_mode=unreal.BlendMode.BLEND_TRANSLUCENT,
                              shading_model=unreal.MaterialShadingModel.MSM_UNLIT, two_sided=True)
    color = expression(material, unreal.MaterialExpressionVectorParameter, -500, 0, parameter_name="Color", default_value=unreal.LinearColor(1, 1, 1, 1))
    opacity = scalar(material, "Opacity", 0.5, -500, 200)
    mel.connect_material_property(rgb(material, color, -300, 0), "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.connect_material_property(opacity, "", unreal.MaterialProperty.MP_OPACITY)
    mel.recompile_material(material)
    return material


SHARP_CODE = """
float w; float h;
Tex.GetDimensions(w, h);
float2 size = float2(w, h);
float2 t = UV * size;
float2 box = clamp(abs(ddx(t)) + abs(ddy(t)), 1e-5, 1.0);
float2 tx = t - 0.5 * box;
float2 off = smoothstep(1.0 - box, 1.0, frac(tx));
float2 uv2 = (floor(tx) + 0.5 + off) / size;
return Tex.SampleGrad(TexSampler, uv2, ddx(UV), ddy(UV));
"""


def spike_master(name, sharp):
    material = fresh_material(name, used_with_skeletal_mesh=True)
    white = unreal.load_asset("/Engine/EngineResources/WhiteSquareTexture")
    if sharp:
        tex_object = expression(material, unreal.MaterialExpressionTextureObjectParameter, -1100, -100, parameter_name="Tex", texture=white)
        uv = expression(material, unreal.MaterialExpressionTextureCoordinate, -1100, 100)
        custom = expression(material, unreal.MaterialExpressionCustom, -800, 0, code=SHARP_CODE, description="SharpTexel",
                            output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT4)
        inputs = []
        for input_name in ("Tex", "UV"):
            item = unreal.CustomInput()
            item.set_editor_property("input_name", input_name)
            inputs.append(item)
        custom.set_editor_property("inputs", inputs)
        mel.connect_material_expressions(tex_object, "", custom, "Tex")
        mel.connect_material_expressions(uv, "", custom, "UV")
        texel = rgb(material, custom, -650, 0)
    else:
        tex = expression(material, unreal.MaterialExpressionTextureSampleParameter2D, -900, -100, parameter_name="Tex", texture=white)
        texel = rgb(material, tex, -700, -100)
    vertex = expression(material, unreal.MaterialExpressionVertexColor, -900, 250)
    tint = expression(material, unreal.MaterialExpressionVectorParameter, -900, 450, parameter_name="Color", default_value=unreal.LinearColor(1, 1, 1, 1))
    lit = mul(material, texel, rgb(material, vertex, -700, 250), -500, 0)
    color = mul(material, lit, rgb(material, tint, -700, 450), -300, 100)
    glow = scalar(material, "Glow", 0.0, -300, 450)
    glow_mask = scalar(material, "GlowMask", 0.0, -300, 520)
    fill = scalar(material, "Fill", 0.0, -300, 590)
    rim = scalar(material, "Rim", 0.0, -300, 660)
    # emissive = color * (Glow + vertexAlpha * GlowMask) + (color + 0.04) * (Fill * hemisphere + Rim * fresnel)
    alpha = expression(material, unreal.MaterialExpressionComponentMask, -500, 300, r=False, g=False, b=False, a=True)
    mel.connect_material_expressions(vertex, "", alpha, "")
    masked = mul(material, alpha, glow_mask, -150, 300)
    glow_total = add(material, glow, masked, 0, 350)
    glow_term = mul(material, color, glow_total, 150, 300)
    normal = expression(material, unreal.MaterialExpressionPixelNormalWS, -500, 800)
    nz = expression(material, unreal.MaterialExpressionComponentMask, -350, 800, r=False, g=False, b=True, a=False)
    mel.connect_material_expressions(normal, "", nz, "")
    half = expression(material, unreal.MaterialExpressionMultiply, -200, 800)
    mel.connect_material_expressions(nz, "", half, "A")
    half.set_editor_property("const_b", 0.5)
    hemi = expression(material, unreal.MaterialExpressionAdd, -50, 800)
    mel.connect_material_expressions(half, "", hemi, "A")
    hemi.set_editor_property("const_b", 0.5)
    fill_term = mul(material, hemi, fill, 100, 700)
    fresnel = expression(material, unreal.MaterialExpressionFresnel, -200, 900, exponent=3.0)
    rim_term = mul(material, fresnel, rim, 100, 900)
    ambient = add(material, fill_term, rim_term, 250, 800)
    lifted = expression(material, unreal.MaterialExpressionAdd, 250, 600)
    mel.connect_material_expressions(color, "", lifted, "A")
    lifted.set_editor_property("const_b", 0.04)
    ambient_term = mul(material, lifted, ambient, 400, 700)
    emissive = add(material, glow_term, ambient_term, 550, 400)
    rough = expression(material, unreal.MaterialExpressionConstant, 300, 500, r=0.85)
    spec = expression(material, unreal.MaterialExpressionConstant, 300, 560, r=0.2)
    mel.connect_material_property(color, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(emissive, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(spec, "", unreal.MaterialProperty.MP_SPECULAR)
    mel.recompile_material(material)
    return material


def instance(name, parent, texture, glow):
    path = DEST + "/" + name
    mi = unreal.load_asset(path) if library.does_asset_exist(path) else tools.create_asset(
        name, DEST, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(mi, parent)
    mel.set_material_instance_texture_parameter_value(mi, "Tex", texture)
    mel.set_material_instance_scalar_parameter_value(mi, "Glow", glow)
    mel.update_material_instance(mi)
    return mi


def import_world(name, atlas_png):
    package = DEST + "/" + name
    if library.does_asset_exist(package):
        return unreal.load_asset(package)
    assets = import_file(os.path.join(SRC, name + ".glb"), STAGE + "/" + name)
    meshes = [a for a in assets if isinstance(a, unreal.StaticMesh)]
    if len(meshes) != 1:
        raise RuntimeError("Expected one static mesh in " + name + ", got " + str(len(meshes)))
    mesh = move(meshes[0], package)
    library.delete_directory(STAGE + "/" + name)
    return mesh


def mesh_stats(mesh):
    box = mesh.get_bounds()
    verts = tris = "?"
    try:
        sub = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
        verts = sub.get_number_verts(mesh, 0)
    except Exception as err:  # noqa: BLE001
        verts = "n/a (%s)" % err
    try:
        tris = mesh.get_num_triangles(0) if hasattr(mesh, "get_num_triangles") else unreal.MeshDescriptionLibrary if False else "n/a"
    except Exception as err:  # noqa: BLE001
        tris = "n/a (%s)" % err
    return "extent(cm)=%.0f x %.0f x %.0f origin=(%.0f,%.0f,%.0f) verts=%s tris=%s sections=%d" % (
        box.box_extent.x * 2, box.box_extent.y * 2, box.box_extent.z * 2, box.origin.x, box.origin.y, box.origin.z, verts, tris, mesh.get_num_sections(0))


def import_rig():
    mesh_path = DEST + "/SK_Hero_FighterSpike"
    if library.does_asset_exist(mesh_path):
        return unreal.load_asset(mesh_path), unreal.load_asset(DEST + "/A_FighterSpike_Idle"), unreal.load_asset(DEST + "/A_FighterSpike_Walk")
    stage = STAGE + "/Rig"
    assets = import_file(os.path.join(SRC, "SK_Hero_FighterSpike.glb"), stage)
    for asset in assets:
        log("rig import produced %s %s" % (type(asset).__name__, asset.get_path_name()))
    skeletal = [a for a in assets if isinstance(a, unreal.SkeletalMesh)]
    skeletons = [a for a in assets if isinstance(a, unreal.Skeleton)]
    clips = [a for a in assets if isinstance(a, unreal.AnimSequence)]
    textures = [a for a in assets if isinstance(a, unreal.Texture2D)]
    if len(skeletal) != 1 or len(skeletons) != 1:
        raise RuntimeError("Expected one skeletal mesh and one skeleton, got %d and %d" % (len(skeletal), len(skeletons)))
    mesh = move(skeletal[0], mesh_path)
    move(skeletons[0], DEST + "/SK_Hero_FighterSpike_Skeleton")
    found = {}
    for clip in clips:
        lower = clip.get_name().lower()
        key = "Idle" if "idle" in lower else "Walk" if "walk" in lower else None
        if key:
            found[key] = move(clip, DEST + "/A_FighterSpike_" + key)
        else:
            log("unrecognised clip " + clip.get_name())
    if textures:
        move(textures[0], DEST + "/T_Hero_FighterSpike_Source")
    library.delete_directory(stage)
    return mesh, found.get("Idle"), found.get("Walk")


def rig_report(mesh, idle, walk):
    skeleton = mesh.get_editor_property("skeleton")
    log("skeleton asset " + skeleton.get_path_name())
    box = mesh.get_bounds()
    log("skeletal bounds(cm) %.1f x %.1f x %.1f, origin z %.1f" % (box.box_extent.x * 2, box.box_extent.y * 2, box.box_extent.z * 2, box.origin.z))
    for label, clip in (("Idle", idle), ("Walk", walk)):
        if clip is None:
            log("%s clip MISSING" % label)
            continue
        try:
            names = unreal.AnimationLibrary.get_animation_track_names(clip)
            log("%s: length %.4f s, frames %s, rate %s, tracks(bones animated) %d" % (
                label, clip.get_editor_property("sequence_length") if clip.get_editor_property("sequence_length") else clip.get_play_length(),
                unreal.AnimationLibrary.get_num_frames(clip), unreal.AnimationLibrary.get_frame_rate(clip), len(names)))
            if label == "Idle":
                log("bones: " + ", ".join(str(n) for n in names))
        except Exception as err:  # noqa: BLE001
            log("%s stats failed: %s (play length %.4f)" % (label, err, clip.get_play_length()))
    try:
        sub = unreal.get_editor_subsystem(unreal.SkeletalMeshEditorSubsystem)
        log("skeletal verts=%s tris=%s" % (sub.get_num_verts(mesh, 0), sub.get_num_triangles(mesh, 0) if hasattr(sub, "get_num_triangles") else "n/a"))
    except Exception as err:  # noqa: BLE001
        log("skeletal count failed: " + str(err))
    try:
        log("skeletal materials: %d slots" % len(mesh.get_editor_property("materials")))
    except Exception as err:  # noqa: BLE001
        log("materials: " + str(err))


# ---- run
library.make_directory(DEST)
atlas_path = DEST + "/T_World_VigilSpike_Atlas_Source"
if not library.does_asset_exist(atlas_path):
    imported = import_file(os.path.join(SRC, "T_World_VigilSpike_Atlas.png"), DEST)
    textures = [a for a in imported if isinstance(a, unreal.Texture2D)]
    if len(textures) != 1:
        raise RuntimeError("Atlas import produced %d textures" % len(textures))
    move(textures[0], atlas_path)
atlas = texture_pair(unreal.load_asset(atlas_path), "World_VigilSpike_Atlas")

world = import_world("SM_World_VigilSpike", None)
occluders = import_world("SM_World_VigilSpike_Occluders", None)
rig, idle, walk = import_rig()
fighter_source = unreal.load_asset(DEST + "/T_Hero_FighterSpike_Source") if library.does_asset_exist(DEST + "/T_Hero_FighterSpike_Source") else None
fighter = texture_pair(fighter_source, "Hero_FighterSpike") if fighter_source else atlas

std = spike_master("M_Spike_Std", False)
sharp = spike_master("M_Spike_Sharp", True)
overlay_material()
for kind, tex, glow in (("World", atlas, 0.35), ("Fighter", fighter, 0.6)):
    instance("MI_Spike_%s_Nearest" % kind, std, tex["Nearest"], glow)
    instance("MI_Spike_%s_Trilinear" % kind, std, tex["Linear"], glow)
    instance("MI_Spike_%s_Sharp" % kind, sharp, tex["Linear"], glow)
for mesh in (world, occluders):
    for index in range(mesh.get_num_sections(0)):
        mesh.set_material(index, unreal.load_asset(DEST + "/MI_Spike_World_Nearest"))

log("world: " + mesh_stats(world))
log("occluders: " + mesh_stats(occluders))
rig_report(rig, idle, walk)

if not library.save_directory(DEST, only_if_is_dirty=False, recursive=True):
    raise RuntimeError("Unable to save " + DEST)
dirty = unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()
log("dirty content packages after save: %d %s" % (len(dirty), [p.get_name() for p in dirty][:10]))
log("SPIKE_IMPORT_COMPLETE")
