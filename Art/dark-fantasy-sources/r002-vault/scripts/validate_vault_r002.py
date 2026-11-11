"""Validate the Vault r002 world from the exported glb files, in a fresh Blender process (issue #149).

  blender -b --python Art/dark-fantasy-sources/r002-vault/scripts/validate_vault_r002.py

Checks, with the numbers written to Art/dark-fantasy-sources/r002-vault/camera_check.json:
  1. file integrity of every export: UVs inside 0..1, Linear sampling, COLOR_0 RGBA, glow alpha only on glow meshes, kit pieces inside
     their cell and not below the floor pivot, world and occluder atlases identical;
  2. FOOTPRINT: no triangle of the world or the occluders above 10 cm inside the 27 x 18 m board footprint;
  3. SIGHT VOLUME (camera_volume.py): no vertex, centroid or edge midpoint of the world above 15 cm inside the hull of footprint + every
     camera eye, and no triangle of the world intersecting the hull surface (BVH overlap); the occluders do enter it (that is why they
     are occluders) and the number is reported;
  4. EYE SPACE: the camera is never inside geometry: every eye of the envelope (5 degree yaw steps) is at least 1.5 m from every triangle
     of the world and the occluders, and not behind a surface;
  5. RAYS: 72 cameras (arms 15 / 35 / 55 m x pitch 35 / 45 / 60 x 8 yaws) x 864 floor targets (36 x 24 at 75 cm, 5 cm above the floor):
     no front face of the world is hit before a target; hits on occluders are counted (they are dithered in game), and the same
     test over the full eye curve at 3 m target spacing.
Prints 'VAULT VALIDATE OK' or 'VAULT VALIDATE FAIL'.
"""
import hashlib
import json
import math
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import r002_lib as R
import lod_r002 as D
import camera_volume as CV
from r002_lib import bpy, np, ROOT, Vector, make_bvh, tri_normals
from mathutils.bvhtree import BVHTree

OUT = os.path.join(ROOT, 'Art', 'dark-fantasy-sources', 'r002-vault')
fails = []
REPORT = {}


def check(cond, msg):
    if not cond:
        fails.append(msg)
        print('FAIL', msg)


def load(path):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.gltf(filepath=path)
    ob = next(o for o in bpy.data.objects if o.type == 'MESH')
    me = ob.data
    uv = np.empty(len(me.loops) * 2)
    me.uv_layers[0].data.foreach_get('uv', uv)
    ca = me.color_attributes[0]
    c = np.empty(len(ca.data) * 4, np.float32)
    ca.data.foreach_get('color', c)
    co = np.empty(len(me.vertices) * 3)
    me.vertices.foreach_get('co', co)
    mat = me.materials[0].node_tree
    img = next((n.image for n in mat.nodes if n.type == 'TEX_IMAGE'), None)
    px = None
    if img is not None:
        a = np.empty(img.size[0] * img.size[1] * 4, np.float32)
        img.pixels.foreach_get(a)
        px = hashlib.sha256((a * 255).round().astype(np.uint8).tobytes()).hexdigest()[:16]
    return dict(tris=len(me.polygons), uv=uv.reshape(-1, 2), col=c.reshape(-1, 4), co=co.reshape(-1, 3), px=px, interp=[n.interpolation for n in mat.nodes if n.type == 'TEX_IMAGE'])


def cameras_72():
    out = []
    for arm in (15.0, 35.0, 55.0):
        for pitch in (35.0, 45.0, 60.0):
            for yaw in range(0, 360, 45):
                r, p = math.radians(yaw), math.radians(pitch)
                out.append((math.cos(r) * math.cos(p) * arm, math.sin(r) * math.cos(p) * arm, math.sin(p) * arm + CV.BOARD_TOP))
    return out


def targets(step):
    xs = np.arange(-CV.FX + step / 2, CV.FX, step)
    ys = np.arange(-CV.FY + step / 2, CV.FY, step)
    return [Vector((x, y, .05)) for x in xs for y in ys]


def blocked(bvh, nrm, comp, eyes, tgts):
    """Rays eye -> target; a front-face hit before the target counts (back faces are culled in game).  Returns per-class counts."""
    hits = {}
    for eye in eyes:
        e = Vector(eye)
        for tv in tgts:
            d = tv - e
            dist = d.length
            d.normalize()
            o = e.copy()
            remaining = dist
            for _ in range(40):
                loc, _n, idx, h = bvh.ray_cast(o, d, remaining - .03)
                if loc is None:
                    break
                if nrm[idx] @ np.array(d) >= 0:
                    o = loc + d * 1e-3
                    remaining -= h + 1e-3
                    continue
                hits[comp[idx]] = hits.get(comp[idx], 0) + 1
                break
    return hits


def main():
    paths = [os.path.join(ROOT, 'Art', 'world-vault', 'r001', 'SM_World_Vault.glb'), os.path.join(ROOT, 'Art', 'world-vault-occluders', 'r001', 'SM_World_Vault_Occluders.glb')]
    km = json.load(open(os.path.join(OUT, 'kit_metrics.json')))
    paths += [os.path.join(ROOT, v['file']) for v in km.values()]
    atl = {}
    for p in paths:
        rel = os.path.relpath(p, ROOT)
        d = load(p)
        uv, col, co = d['uv'], d['col'], d['co']
        check(uv.min() >= -1e-4 and uv.max() <= 1 + 1e-4, f'{rel}: UV outside 0..1 {uv.min():.4f} {uv.max():.4f}')
        check(d['interp'] == ['Linear'], f'{rel}: image sampling {d["interp"]}')
        check(col.shape[1] == 4, f'{rel}: COLOR_0 not RGBA')
        a = col[:, 3]
        print(f'{rel:78s} tris {d["tris"]:7d} alpha[{a.min():.2f},{a.max():.2f}] zmin {co[:, 2].min():+.3f} m')
        if '/world-vault' in rel:
            atl[rel] = d['px']
            check(a.max() == 1.0, f'{rel}: no glow alpha')            # the world has rune inlays and lancets, the occluders rune lines, crystals and the Waystone
        kind = rel.split('/')[1]
        if kind.startswith(('prop-', 'scatter-')):
            check(co[:, 2].min() > -.03, f'{rel}: geometry below the floor pivot ({co[:, 2].min():.3f})')
            check(abs(co[:, 0]).max() < .95 and (abs(co[:, 1]).max() < .95 or 'arch' in rel), f'{rel}: footprint beyond the cell (|x| {abs(co[:, 0]).max():.2f}, |y| {abs(co[:, 1]).max():.2f})')
            if 'arch' in rel:
                check(abs(co[:, 1]).max() < 1.6 and abs(co[:, 0]).max() < .95, f'{rel}: arch beyond its two cells')
        if kind.startswith('scatter-'):
            check(co[:, 2].max() < .6, f'{rel}: scatter taller than 60 cm')
        if kind.startswith('fx-'):
            check(a.min() >= 0 and a.max() <= .8, f'{rel}: opacity alpha outside 0..0.8')
    check(len(set(atl.values())) == 1, f'world and occluder atlases differ: {atl}')

    # ------------------------------------------------------------------ geometry checks on the world
    Pw, _, Cw, _ = D.read_glb(paths[0])
    Po, _, Co, _ = D.read_glb(paths[1])
    REPORT['world_triangles'] = len(Pw)
    REPORT['occluder_triangles'] = len(Po)
    for nm, P in (('world', Pw), ('occluders', Po)):
        pts = np.concatenate([P.reshape(-1, 3), P.mean(1)])
        inside = (np.abs(pts[:, 0]) < CV.FX) & (np.abs(pts[:, 1]) < CV.FY)
        hi = float(pts[inside, 2].max()) if inside.any() else 0.0
        REPORT[f'{nm}_max_height_inside_footprint_m'] = round(hi, 4)
        check(hi <= .10, f'{nm}: geometry {hi:.3f} m above the floor inside the board footprint')
        print(f'{nm}: highest point inside the 27 x 18 m footprint {hi * 100:.1f} cm')
    vol = CV.Volume(margin=.5)
    REPORT['hull_planes'] = int(len(vol.planes))
    for nm, P in (('world', Pw), ('occluders', Po)):
        pts = np.concatenate([P.reshape(-1, 3), P.mean(1), ((P + P[:, [1, 2, 0]]) / 2).reshape(-1, 3)])
        n_in = int((vol.contains(pts) & (pts[:, 2] > .15)).sum())
        REPORT[f'{nm}_points_inside_sight_volume_margin_0p5m'] = n_in
        print(f'{nm}: {n_in} of {len(pts)} sample points inside the sight volume (+0.5 m)')
        if nm == 'world':
            check(n_in == 0, f'world enters the sight volume ({n_in} sample points)')
    # surface intersections with the (uninflated) hull
    hull = BVHTree.FromPolygons([Vector(v) for v in vol.V], [tuple(map(int, f)) for f in vol.F], all_triangles=True)
    bw = make_bvh(Pw)
    bo = make_bvh(Po)
    ov_w = bw.overlap(hull)
    ov_o = bo.overlap(hull)
    # the hull's bottom face is the footprint lifted to 12 cm; ground triangles are below it, so only genuine intersections count
    REPORT['world_triangle_hull_overlaps'] = len(ov_w)
    REPORT['occluder_triangle_hull_overlaps'] = len(ov_o)
    print(f'hull surface overlaps: world {len(ov_w)}, occluders {len(ov_o)}')
    check(len(ov_w) == 0, f'world triangles intersect the sight volume surface ({len(ov_w)})')

    # ------------------------------------------------------------------ eye space
    Pall = np.concatenate([Pw, Po])
    ball = make_bvh(Pall)
    nall = tri_normals(Pall)
    eyes = CV.eye_positions(5.0)
    REPORT['eye_positions'] = int(len(eyes))
    worst = 1e9
    inside_eyes = 0
    for e in eyes:
        loc, nrm, idx, dist = ball.find_nearest(Vector(e), CV.EYE_RADIUS + .5)
        if loc is None:
            continue
        worst = min(worst, dist)
        if dist < CV.EYE_RADIUS:
            fails.append(f'eye {tuple(np.round(e, 1))} is {dist:.2f} m from geometry')
        if (Vector(e) - loc).dot(Vector(nall[idx])) < 0:
            inside_eyes += 1
    REPORT['eyes_closer_than_1p5m'] = sum(1 for f in fails if f.startswith('eye '))
    REPORT['eyes_behind_a_surface'] = inside_eyes
    REPORT['nearest_geometry_to_any_eye_m'] = round(worst, 2) if worst < 1e8 else '>2.0'
    check(inside_eyes == 0, f'{inside_eyes} eyes are behind a surface (inside geometry)')
    print(f'eye space: {len(eyes)} eyes, nearest geometry {REPORT["nearest_geometry_to_any_eye_m"]} m, {inside_eyes} inside')

    # ------------------------------------------------------------------ rays
    nw = tri_normals(Pw)
    comp_all = np.concatenate([np.zeros(len(Pw), int), np.ones(len(Po), int)])
    cams = cameras_72()
    tg = targets(.75)
    h_w = blocked(bw, nw, np.zeros(len(Pw), int), cams, tg)
    h_all = blocked(ball, nall, comp_all, cams, tg)
    REPORT['rays_72_cameras'] = dict(cameras=len(cams), targets=len(tg), total_rays=len(cams) * len(tg), blocked_by_world_alone=int(sum(h_w.values())),
                                      blocked_by_world_with_occluders=int(h_all.get(0, 0)), blocked_by_occluders=int(h_all.get(1, 0)))
    print('72-camera rays:', REPORT['rays_72_cameras'])
    check(sum(h_w.values()) == 0, f'world blocks {sum(h_w.values())} sight lines')
    check(h_all.get(0, 0) == 0, 'world blocks sight lines once occluders are present')
    curve = np.array(CV.eye_curve())
    ext = [(r * math.cos(math.radians(yaw)), r * math.sin(math.radians(yaw)), z) for r, z in curve[::3] for yaw in range(0, 360, 45)]
    tg3 = targets(3.0)
    h_ext = blocked(bw, nw, np.zeros(len(Pw), int), ext, tg3)
    REPORT['rays_envelope_sweep'] = dict(cameras=len(ext), targets=len(tg3), total_rays=len(ext) * len(tg3), blocked_by_world=int(sum(h_ext.values())))
    print('envelope sweep rays:', REPORT['rays_envelope_sweep'])
    check(sum(h_ext.values()) == 0, f'world blocks {sum(h_ext.values())} sight lines in the envelope sweep')
    REPORT['validate'] = 'FAIL' if fails else 'OK'
    with open(os.path.join(OUT, 'camera_check.json'), 'w') as fh:
        json.dump(REPORT, fh, indent=1)
    print('VAULT VALIDATE', 'FAIL' if fails else 'OK', len(fails))


main()
