"""Shared helpers for the Vigil rose patch (issue #149): glb read/write, PNG write, the hi-res rose texture, the old atlas layout.

Runs inside Blender's Python (numpy + bpy); the glb and PNG code is standard library only.
"""
import json
import math
import os
import struct
import sys
import zlib

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, '..', '..', '..', '..'))
SRC = os.path.join(ROOT, 'Art', 'dark-fantasy-sources')
OUT_SRC = os.path.join(SRC, 'r002-rose')
sys.path.insert(0, os.path.join(SRC, 'r001', 'scripts'))

ATLAS = 2048                   # the old atlas edge (the old pixels keep their place in the left half of the new one)
ATLAS_W, ATLAS_H = 4096, 2048  # the new atlas: the old 2048 square on the left, the rose on the right
PAD = 16
NEW_PAD = 24                   # wrapped padding around the new region (mip-safe to mip 4 and beyond)
ROSE_PX = 2000                 # new rose region edge (old: 128): the rose is about 9 m across, so 2000 px is about 220 texels/m (the old region was 14)
NEW_X0, NEW_Y0 = ATLAS + NEW_PAD, NEW_PAD   # top-down pixel origin of the new region in the new atlas

# (source glb, output glb) for every Vigil export the rose touches. The rose is in the occluder mesh only: SM_World_Vigil has no vertex in
# the rose region (the patch script asserts it for the world files listed in UNTOUCHED), so the world and its LODs stay as they are.
SOURCES = [(f'Art/world-vigil-occluders/r001/SM_World_Vigil_Occluders{lod}.glb', f'Art/world-vigil-occluders/r002/SM_World_Vigil_Occluders{lod}.glb')
           for lod in ('', '_LOD1', '_LOD2')]
UNTOUCHED = [f'Art/world-vigil/r002/SM_World_Vigil{lod}.glb' for lod in ('', '_LOD1', '_LOD2')]


def read_glb(path):
    data = open(path, 'rb').read()
    assert data[:4] == b'glTF', path
    jl = struct.unpack('<I', data[12:16])[0]
    js = json.loads(data[20:20 + jl])
    bin_off = 20 + jl + 8
    bl = struct.unpack('<I', data[20 + jl:24 + jl])[0]
    return js, bytearray(data[bin_off:bin_off + bl])


def write_glb(path, js, binary):
    head = json.dumps(js, separators=(',', ':')).encode()
    head += b' ' * (-len(head) % 4)
    binary = bytes(binary) + b'\0' * (-len(binary) % 4)
    total = 12 + 8 + len(head) + 8 + len(binary)
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, 'wb') as f:
        f.write(b'glTF' + struct.pack('<II', 2, total))
        f.write(struct.pack('<I', len(head)) + b'JSON' + head)
        f.write(struct.pack('<I', len(binary)) + b'BIN\0' + binary)


def view_bytes(js, binary, index):
    bv = js['bufferViews'][index]
    o = bv.get('byteOffset', 0)
    return binary[o:o + bv['byteLength']]


def accessor_array(js, binary, index):
    """Tightly packed float32 / uint accessor as a numpy array (no interleaving expected from Blender)."""
    ac = js['accessors'][index]
    bv = js['bufferViews'][ac['bufferView']]
    assert 'byteStride' not in bv, 'interleaved view'
    comps = {'SCALAR': 1, 'VEC2': 2, 'VEC3': 3, 'VEC4': 4}[ac['type']]
    dt = {5126: '<f4', 5125: '<u4', 5123: '<u2', 5121: 'u1'}[ac['componentType']]
    o = bv.get('byteOffset', 0) + ac.get('byteOffset', 0)
    return np.frombuffer(bytes(binary[o:o + ac['count'] * comps * np.dtype(dt).itemsize]), dtype=dt).reshape(ac['count'], comps)


def write_accessor(js, binary, index, arr):
    ac = js['accessors'][index]
    bv = js['bufferViews'][ac['bufferView']]
    o = bv.get('byteOffset', 0) + ac.get('byteOffset', 0)
    raw = np.ascontiguousarray(arr, dtype='<f4').tobytes()
    binary[o:o + len(raw)] = raw


def png_bytes(rgba8):
    """8-bit RGBA PNG (filter 0), stdlib zlib."""
    h, w = rgba8.shape[:2]
    raw = b''.join(b'\0' + rgba8[y].tobytes() for y in range(h))
    def chunk(tag, body):
        return struct.pack('>I', len(body)) + tag + body + struct.pack('>I', zlib.crc32(tag + body) & 0xffffffff)
    return b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 6, 0, 0, 0)) + chunk(b'IDAT', zlib.compress(raw, 9)) + chunk(b'IEND', b'')


def png_decode(png):
    """8-bit RGB/RGBA non-interlaced PNG -> (h, w, 4) uint8 (A = 255 for RGB)."""
    assert png[:8] == b'\x89PNG\r\n\x1a\n'
    pos, idat, ihdr = 8, b'', None
    while pos < len(png):
        n = struct.unpack('>I', png[pos:pos + 4])[0]
        tag = png[pos + 4:pos + 8]
        body = png[pos + 8:pos + 8 + n]
        if tag == b'IHDR':
            ihdr = struct.unpack('>IIBBBBB', body)
        elif tag == b'IDAT':
            idat += body
        pos += 12 + n
    w, h, depth, ctype, _, _, inter = ihdr
    assert depth == 8 and inter == 0 and ctype in (2, 6), ihdr
    bpp = 3 if ctype == 2 else 4
    raw = np.frombuffer(zlib.decompress(idat), dtype=np.uint8).reshape(h, 1 + w * bpp)
    out = np.zeros((h, w * bpp), np.int32)
    prev = np.zeros(w * bpp, np.int32)
    for y in range(h):
        f, line = raw[y, 0], raw[y, 1:].astype(np.int32)
        if f == 0:
            cur = line
        elif f == 1:
            cur = line.copy()
            for i in range(bpp, len(cur)):
                cur[i] = (cur[i] + cur[i - bpp]) & 255
        elif f == 2:
            cur = (line + prev) & 255
        elif f == 3:
            cur = line.copy()
            for i in range(len(cur)):
                left = cur[i - bpp] if i >= bpp else 0
                cur[i] = (cur[i] + ((left + prev[i]) >> 1)) & 255
        elif f == 4:
            cur = line.copy()
            for i in range(len(cur)):
                a = cur[i - bpp] if i >= bpp else 0
                b = prev[i]
                c = prev[i - bpp] if i >= bpp else 0
                p = a + b - c
                pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                pr = a if (pa <= pb and pa <= pc) else (b if pb <= pc else c)
                cur[i] = (cur[i] + pr) & 255
        else:
            raise ValueError(f)
        out[y] = cur
        prev = cur
    img = out.reshape(h, w, bpp).astype(np.uint8)
    if bpp == 3:
        img = np.concatenate([img, np.full((h, w, 1), 255, np.uint8)], 2)
    return img


def atlas_of(path):
    js, binary = read_glb(path)
    return png_decode(bytes(view_bytes(js, binary, js['images'][0]['bufferView'])))


def old_layout(metrics_path):
    """Replays tlib.build_atlas's skyline packing for the r002 Vigil regions (their sizes are in world_metrics.json)."""
    m = json.load(open(metrics_path))['atlas']
    sky = np.zeros(m['size'], int)
    place = {}
    for k, (w, h) in m['regions_px'].items():
        W, H = w + 2 * m['pad_px'], h + 2 * m['pad_px']
        best = None
        for x in range(0, m['size'] - W + 1, 4):
            y = sky[x:x + W].max()
            if y + H <= m['size'] and (best is None or y < best[1]):
                best = (x, y)
        x, y = best
        sky[x:x + W] = y + H
        place[k] = (x + m['pad_px'], y + m['pad_px'], w, h)
    return place


def rose_hires(n=ROSE_PX, ss=1):
    """glass.rose_window's design (same palettes, 12 petals, lead rings, sector colours) sampled at n x n, supersampled ss x ss.
    The r001 texture is 128 x 128: its pixel-frequency shimmer terms are rescaled, so the design, not the pixel pattern, carries over."""
    import glass
    N = n * ss
    ref = 128.0
    yy, xx = np.mgrid[0:N, 0:N].astype(np.float64)
    px, py = (xx + .5) * ref / N - .5, (yy + .5) * ref / N - .5          # coordinates in the 128 px grid
    c = (ref - 1) / 2
    d, a = np.hypot(px - c, py - c), np.arctan2(py - c, px - c)
    r = d / (ref / 2)
    pals = (("#3a0612", "#a01428", "#ff4a4a", "#ffb0a0"), ("#06103a", "#1438b0", "#3a8cff", "#b0e0ff"),
            ("#3a2406", "#c88a14", "#ffd84a", "#fff8c0"), ("#20063a", "#6a1ab0", "#b45aff", "#ecc0ff"),
            ("#063a30", "#14a088", "#4affd0", "#c0fff0"))
    petals = 12
    sector = np.floor((a + math.pi) / (2 * math.pi) * petals).astype(int) % petals
    local = ((a + math.pi) / (2 * math.pi) * petals) % 1
    band = np.select([r < .16, r < .42, r < .72, r < .94], [0, 1, 2, 3], 4)
    idx = (band * 2 + sector * (band > 0) * ((band % 2) + 1)) % 5
    f = np.clip(.75 - r * .35 + .12 * np.sin(px * .31) * np.sin(py * .27), 0, 1)
    out = np.zeros((N, N, 3), np.float64)
    for i in range(5):
        s = glass._shade(f, pals[i])
        out[idx == i] = s[idx == i]
    lead = (np.abs(local - .5) > .46) & (band > 0) & (band < 4)
    for b in (.16, .42, .72, .94):
        lead |= np.abs(r - b) < .018
    lead |= np.abs(r - (.57 + .15 * np.cos((local - .5) * 2 * math.pi))) < .016
    out[lead] = glass.LEAD
    out[r > .99] = glass.LEAD                      # outside the disc: lead, never black, so mips and cut edges stay dark glass-lead
    out = out.reshape(n, ss, n, ss, 3).mean((1, 3))
    return out


def to8(x):
    return np.clip(np.rint(np.asarray(x) * 255), 0, 255).astype(np.uint8)
