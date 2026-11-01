"""Rendering helpers for verification images (EEVEE, numpy compositing, no Pillow)."""
import math
import sys

sys.path.insert(0, __import__('os').path.dirname(__import__('os').path.abspath(__file__)))
import bpy
import numpy as np
from mathutils import Vector

import tlib as L


def setup_scene(res=(512, 512), bg=(.035, .035, .04)):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    sc = bpy.context.scene
    sc.render.engine = 'BLENDER_EEVEE'
    sc.render.resolution_x, sc.render.resolution_y = res
    sc.render.resolution_percentage = 100
    sc.render.film_transparent = False
    sc.view_settings.view_transform = 'Standard'
    sc.view_settings.look = 'None'
    try:
        sc.eevee.taa_render_samples = 16
    except Exception:
        pass
    w = bpy.data.worlds.new('w')
    w.use_nodes = True
    bgn = w.node_tree.nodes['Background']
    bgn.inputs[0].default_value = (*bg, 1)
    bgn.inputs[1].default_value = 1.0
    sc.world = w
    return sc


def import_glb(path):
    before = set(bpy.data.objects)
    bpy.ops.import_scene.gltf(filepath=path)
    obs = [o for o in bpy.data.objects if o not in before and o.type == 'MESH']
    return obs


def unlit_vertex_color(ob, gain=1.0):
    """Material = texture x COLOR_0 as emission: shows the baked lighting the way it was authored."""
    for slot in ob.material_slots:
        m = slot.material
        nt = m.node_tree
        tex = next(n for n in nt.nodes if n.type == 'TEX_IMAGE')
        out = next(n for n in nt.nodes if n.type == 'OUTPUT_MATERIAL')
        ca = nt.nodes.new('ShaderNodeVertexColor')
        ca.layer_name = ob.data.color_attributes[0].name if len(ob.data.color_attributes) else ''
        mul = nt.nodes.new('ShaderNodeMix')
        mul.data_type = 'RGBA'
        mul.blend_type = 'MULTIPLY'
        mul.inputs['Factor'].default_value = 1.0
        nt.links.new(tex.outputs['Color'], mul.inputs[6])
        nt.links.new(ca.outputs['Color'], mul.inputs[7])
        em = nt.nodes.new('ShaderNodeEmission')
        em.inputs['Strength'].default_value = gain
        nt.links.new(mul.outputs[2], em.inputs['Color'])
        nt.links.new(em.outputs[0], out.inputs['Surface'])
        m.use_backface_culling = True


def unlit_texture(ob, gain=1.0):
    for slot in ob.material_slots:
        m = slot.material
        nt = m.node_tree
        tex = next(n for n in nt.nodes if n.type == 'TEX_IMAGE')
        out = next(n for n in nt.nodes if n.type == 'OUTPUT_MATERIAL')
        em = nt.nodes.new('ShaderNodeEmission')
        em.inputs['Strength'].default_value = gain
        nt.links.new(tex.outputs['Color'], em.inputs['Color'])
        nt.links.new(em.outputs[0], out.inputs['Surface'])
        m.use_backface_culling = True


def lights(sun_dir=(-.5, .35, -.75), sun=3.2, fill=.9):
    sc = bpy.context.scene
    for name, d, e, col in (('sun', sun_dir, sun, (1, .95, .88)), ('fill', (.6, -.7, -.3), fill, (.6, .7, 1.0))):
        ld = bpy.data.lights.new(name, 'SUN')
        ld.energy = e
        ld.color = col
        ld.angle = math.radians(3)
        o = bpy.data.objects.new(name, ld)
        o.rotation_euler = Vector(d).to_track_quat('-Z', 'Y').to_euler()
        sc.collection.objects.link(o)


def bounds(obs):
    pts = [o.matrix_world @ Vector(c) for o in obs for c in o.bound_box]
    lo = Vector((min(p.x for p in pts), min(p.y for p in pts), min(p.z for p in pts)))
    hi = Vector((max(p.x for p in pts), max(p.y for p in pts), max(p.z for p in pts)))
    return lo, hi


def camera_three_quarter(obs, direction=(1.0, -.85, .7), margin=1.18):
    lo, hi = bounds(obs)
    c = (lo + hi) / 2
    d = Vector(direction).normalized()
    cam = bpy.data.cameras.new('cam')
    cam.type = 'ORTHO'
    o = bpy.data.objects.new('cam', cam)
    bpy.context.scene.collection.objects.link(o)
    o.location = c + d * (hi - lo).length * 3 + d
    o.rotation_euler = (-d).to_track_quat('-Z', 'Y').to_euler()
    bpy.context.view_layer.update()
    # fit: project the bbox corners onto the camera plane
    inv = o.matrix_world.inverted()
    corners = [inv @ Vector((x, y, z)) for x in (lo.x, hi.x) for y in (lo.y, hi.y) for z in (lo.z, hi.z)]
    xs = [p.x for p in corners]
    ys = [p.y for p in corners]
    cx, cy = (max(xs) + min(xs)) / 2, (max(ys) + min(ys)) / 2
    o.location = o.matrix_world @ Vector((cx, cy, 0))
    cam.ortho_scale = max(max(xs) - min(xs), max(ys) - min(ys)) * margin
    cam.clip_end = 10000
    bpy.context.scene.camera = o
    return o


def label(cam, text, size=.055, color=(1, .86, .55)):
    """Text glued to the bottom-left of an orthographic camera view."""
    s = cam.data.ortho_scale
    cu = bpy.data.curves.new('lbl', 'FONT')
    cu.body = text
    cu.size = s * size
    t = bpy.data.objects.new('lbl', cu)
    bpy.context.scene.collection.objects.link(t)
    m = bpy.data.materials.new('lblm')
    nt = m.node_tree
    for n in list(nt.nodes):
        if n.type != 'OUTPUT_MATERIAL':
            nt.nodes.remove(n)
    em = nt.nodes.new('ShaderNodeEmission')
    em.inputs['Color'].default_value = (*color, 1)
    nt.links.new(em.outputs[0], nt.nodes['Material Output'].inputs['Surface'])
    cu.materials.append(m)
    t.parent = cam
    t.location = (-s * .47, -s * .47, -1.0)
    return t


def render_to_array(path):
    sc = bpy.context.scene
    sc.render.filepath = path
    sc.render.image_settings.file_format = 'PNG'
    sc.render.image_settings.color_mode = 'RGBA'
    bpy.ops.render.render(write_still=True)
    img = bpy.data.images.load(path)
    a = L.img_array(img)
    bpy.data.images.remove(img)
    return a


def sheet(tiles, cols, gap=6, bg=(.02, .02, .025)):
    h, w = tiles[0].shape[:2]
    rows = (len(tiles) + cols - 1) // cols
    A = np.zeros((rows * h + (rows + 1) * gap, cols * w + (cols + 1) * gap, 4), np.float32)
    A[..., :3] = bg
    A[..., 3] = 1
    for i, t in enumerate(tiles):
        r, c = divmod(i, cols)
        y0, x0 = gap + r * (h + gap), gap + c * (w + gap)
        A[y0:y0 + h, x0:x0 + w] = t
    return A
