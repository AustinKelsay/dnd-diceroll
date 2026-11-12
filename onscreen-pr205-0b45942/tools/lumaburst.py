#!/usr/bin/env python3
"""Luma of a box per frame in a burst of P6 PPM frames (grim -t ppm), same definition as tools/impact_flash.py:
Rec.709 weights on 8-bit sRGB codes; white = all channels >= 250; clipped = luma >= 254.
usage: lumaburst.py OUT.csv X Y W H frame1.ppm frame2.ppm ...   (box in the frame's own coordinates; use 0 0 0 0 for whole frame)"""
import sys

def read_ppm(path):
    d = open(path, 'rb').read()
    parts, i = [], 0
    while len(parts) < 4:
        while d[i:i+1].isspace(): i += 1
        if d[i:i+1] == b'#':
            while d[i:i+1] != b'\n': i += 1
            continue
        j = i
        while not d[j:j+1].isspace(): j += 1
        parts.append(d[i:j]); i = j
    i += 1
    return int(parts[1]), int(parts[2]), d[i:]

def measure(path, box):
    w, h, px = read_ppm(path)
    x0, y0, bw, bh = box
    if bw == 0: x0, y0, bw, bh = 0, 0, w, h
    peak = 0.0; tot = 0.0; white = 0; clip = 0; n = bw * bh
    for y in range(y0, y0 + bh):
        row = px[(y * w + x0) * 3:(y * w + x0 + bw) * 3]
        for k in range(0, len(row), 3):
            r, g, b = row[k], row[k+1], row[k+2]
            l = 0.2126*r + 0.7152*g + 0.0722*b
            tot += l
            if l > peak: peak = l
            if r >= 250 and g >= 250 and b >= 250: white += 1
            if l >= 254: clip += 1
    return peak, tot / n, white / n, clip / n

if __name__ == '__main__':
    out = sys.argv[1]; box = tuple(int(v) for v in sys.argv[2:6]); files = sys.argv[6:]
    with open(out, 'w') as f:
        f.write('frame,peak,mean,white_pct,clip_pct\n')
        for p in files:
            pk, mn, wh, cl = measure(p, box)
            f.write(f'{p.rsplit("/",1)[-1]},{pk:.1f},{mn:.1f},{wh*100:.2f},{cl*100:.2f}\n')
    print('wrote', out, len(files))
