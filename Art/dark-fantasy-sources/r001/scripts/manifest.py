"""Parse every theme-build/*.glb (stdlib only) -> theme-build/manifest.json."""
import json, struct, glob, os, hashlib
B = __import__('os').environ.get('THEME_LOCAL', __import__('os').path.join(__import__('os').getcwd(), '.local')) + '/theme-build'
rows = []
for p in sorted(glob.glob(f'{B}/*.glb')):
    data = open(p, 'rb').read()
    _, _, length = struct.unpack('<III', data[:12])
    jl, jt = struct.unpack('<II', data[12:20]); js = json.loads(data[20:20 + jl])
    bin_off = 20 + jl + 8
    tris, lo, hi, color = 0, [1e9] * 3, [-1e9] * 3, False
    for m in js['meshes']:
        for pr in m['primitives']:
            tris += js['accessors'][pr['indices']]['count'] // 3
            a = js['accessors'][pr['attributes']['POSITION']]
            lo = [min(x, y) for x, y in zip(lo, a['min'])]; hi = [max(x, y) for x, y in zip(hi, a['max'])]
            color |= 'COLOR_0' in pr['attributes']
    # glTF Y-up -> Blender/Unreal-style Z-up metres: (x, -z, y)
    bmin = [lo[0], -hi[2], lo[1]]; bmax = [hi[0], -lo[2], hi[1]]
    sizes = []
    for im in js.get('images', []):
        bv = js['bufferViews'][im['bufferView']]
        png = data[bin_off + bv.get('byteOffset', 0): bin_off + bv.get('byteOffset', 0) + bv['byteLength']]
        w, h = struct.unpack('>II', png[16:24]); sizes.append([w, h])
    samp = [js['samplers'][t['sampler']].get('magFilter') for t in js.get('textures', []) if 'sampler' in t]
    rows.append(dict(file=os.path.basename(p), triangles=tris,
                     bbox_min_m=[round(v, 4) for v in bmin], bbox_max_m=[round(v, 4) for v in bmax],
                     size_m=[round(b - a, 4) for a, b in zip(bmin, bmax)],
                     materials=len(js.get('materials', [])), texture_px=sizes[0] if len(sizes) == 1 else sizes,
                     texture_mag_filter='NEAREST' if samp == [9728] else samp, has_COLOR_0=color,
                     bytes=len(data), sha256=hashlib.sha256(data).hexdigest()))
man = dict(schema=1, units='metres, Z-up (bbox shown in Blender/Unreal axes; file is glTF Y-up)', facing='+X',
           notes={'COLOR_0': 'baked display light (sRGB-encoded multiplier, soft shoulder above 0.8); multiply with baseColor',
                  'characters': 'each scaled to 1.15 m total height (per-asset, so relative sizes are not preserved)'},
           assets=rows, images=dict(contact_sheet='contact_sheet.png', checks=['check_vigil.png', 'check_root.png']))
json.dump(man, open(f'{B}/manifest.json', 'w'), indent=2)
for r in rows:
    print(f"{r['file']:24s} {r['triangles']:6d} {r['size_m']} tex {r['texture_px']} mats {r['materials']} col {r['has_COLOR_0']} {r['texture_mag_filter']}")
