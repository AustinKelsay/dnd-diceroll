"""Root tiles repeat across 200 cells: fill the top-face patch with mirrored moss and a dark rim."""
import bpy, sys, numpy as np
path = sys.argv[sys.argv.index("--") + 1]
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=path)
image = [i for i in bpy.data.images if i.size[0] > 0][0]
w, h = image.size
px = np.array(image.pixels[:], dtype=np.float32).reshape(h, w, 4)[::-1]  # row 0 = top
moss = px[15:82, 55:122].copy()                                           # plain moss, 67 px
tile = np.concatenate([np.concatenate([moss, moss[:, ::-1]], 1), np.concatenate([moss[::-1], moss[::-1, ::-1]], 1)], 0)
patch = tile[:132, :132] * np.array([0.92, 0.95, 0.9, 1.0], dtype=np.float32)
rim = 3
patch[:rim, :, :3] *= 0.45; patch[-rim:, :, :3] *= 0.45; patch[:, :rim, :3] *= 0.45; patch[:, -rim:, :3] *= 0.45
px[2:134, 2:134] = patch
image.pixels[:] = px[::-1].ravel()
image.pack()
mesh = [o for o in bpy.data.objects if o.type == "MESH"][0]
bpy.context.view_layer.objects.active = mesh; mesh.select_set(True)
bpy.ops.export_scene.gltf(filepath=path, export_format="GLB", use_selection=True, export_image_format="AUTO")
print("CALM_TILE_DONE", w, h)
