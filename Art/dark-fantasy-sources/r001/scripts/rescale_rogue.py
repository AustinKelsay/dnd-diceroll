"""Nix crouches: scale by the fighter's height factor instead of to full 1.15 m height."""
import bpy, sys
src, factor = sys.argv[sys.argv.index("--") + 1], float(sys.argv[sys.argv.index("--") + 2])
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=src)
mesh = [o for o in bpy.data.objects if o.type == "MESH"][0]
mesh.scale = [s * factor for s in mesh.scale]
bpy.context.view_layer.objects.active = mesh
mesh.select_set(True)
bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
dims = mesh.dimensions
print("ROGUE_DIMS", round(dims.x, 3), round(dims.y, 3), round(dims.z, 3))
bpy.ops.export_scene.gltf(filepath=src, export_format="GLB", use_selection=True, export_image_format="AUTO")
