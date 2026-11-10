#!/usr/bin/env python3
"""Measure whether a hit flash blows out its target (#189).

Reads the `impact` shots of a capture gallery run (`python3 tools/dev.py gallery`): for each one it takes the struck
combatant's projected footprint box from the run's manifest (the game projects the footprint ring's square, from the board top
to the figure's head, with the camera the screenshot sees) and measures the PNG inside it:

* peak luminance: the brightest pixel's Rec. 709 luma of the 8-bit sRGB values, 0 to 255
* white fraction: the share of the box's pixels whose red, green and blue are all at least WHITE_LEVEL
* clipped fraction: the share of pixels whose luma is at least CLIP_LEVEL (the same threshold on luma)

A shot passes when the peak luminance is below CLIP_LEVEL (254: a pixel at 255 is clipped) and fewer than MAX_WHITE_FRACTION of the box is pure white. The box
includes a little floor around the figure; the HUD's damage text and bar are drawn inside it and count too, which is deliberate:
the flash must leave the ring and bar readable. Standard library only (a small PNG decoder), so it runs anywhere the helper does.

    python3 tools/impact_flash.py .local/gallery/<run>            # every impact shot under a run
    python3 tools/impact_flash.py path/to/manifest.json --shot impact-yaw000-default --json out.json

Exit 0 when every measured shot passes, 1 when one fails, 2 when nothing could be measured.
"""
import argparse
import json
from pathlib import Path
import struct
import sys
import zlib

WHITE_LEVEL = 250
CLIP_LEVEL = 254.0
MAX_WHITE_FRACTION = 0.02
PNG_SIGNATURE = b"\x89PNG\r\n\x1a\n"
CHANNELS = {2: 3, 6: 4}


def _paeth(a, b, c):
    p = a + b - c
    pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
    return a if pa <= pb and pa <= pc else b if pb <= pc else c


def read_png(path, rows=None, columns=None):
    """Decode an 8-bit RGB or RGBA PNG. Returns (width, height, channels, pixel rows), each row a bytes object of
    `columns * channels` bytes. `rows` and `columns` bound how much is decoded: filters only look up and left, so rows
    below the last wanted one and columns right of the last wanted one are never reconstructed."""
    data = Path(path).read_bytes()
    if data[:8] != PNG_SIGNATURE:
        raise ValueError(f"{path} is not a PNG")
    position, header, compressed = 8, None, []
    while position + 8 <= len(data):
        length, kind = struct.unpack(">I4s", data[position:position + 8])
        body = data[position + 8:position + 8 + length]
        position += 12 + length
        if kind == b"IHDR":
            header = struct.unpack(">IIBBBBB", body)
        elif kind == b"IDAT":
            compressed.append(body)
        elif kind == b"IEND":
            break
    if header is None or not compressed:
        raise ValueError(f"{path} has no image data")
    width, height, depth, color, _, _, interlace = header
    if depth != 8 or color not in CHANNELS or interlace:
        raise ValueError(f"{path}: only 8-bit RGB or RGBA PNGs that are not interlaced are supported")
    channels = CHANNELS[color]
    stride = width * channels
    raw = zlib.decompress(b"".join(compressed))
    if len(raw) < height * (stride + 1):
        raise ValueError(f"{path} is truncated")
    last_row = height if rows is None else min(height, rows)
    keep = stride if columns is None else min(stride, columns * channels)
    image, previous = [], bytearray(keep)
    for y in range(last_row):
        start = y * (stride + 1)
        kind = raw[start]
        line = bytearray(raw[start + 1:start + 1 + keep])
        if kind == 1:
            for i in range(channels, keep):
                line[i] = (line[i] + line[i - channels]) & 255
        elif kind == 2:
            for i in range(keep):
                line[i] = (line[i] + previous[i]) & 255
        elif kind == 3:
            for i in range(keep):
                left = line[i - channels] if i >= channels else 0
                line[i] = (line[i] + ((left + previous[i]) >> 1)) & 255
        elif kind == 4:
            for i in range(keep):
                left = line[i - channels] if i >= channels else 0
                corner = previous[i - channels] if i >= channels else 0
                line[i] = (line[i] + _paeth(left, previous[i], corner)) & 255
        elif kind != 0:
            raise ValueError(f"{path}: row {y} has unknown filter {kind}")
        image.append(bytes(line))
        previous = line
    return width, height, channels, image


def luma(r, g, b):
    return 0.2126 * r + 0.7152 * g + 0.0722 * b


def measure_box(image, channels, box):
    """Statistics of the pixels in box = (x0, y0, x1, y1), x1 and y1 exclusive."""
    x0, y0, x1, y1 = box
    if x0 >= x1 or y0 >= y1:
        raise ValueError(f"The box {box} is empty")
    peak, total, white, clipped, count = 0.0, 0.0, 0, 0, 0
    values = []
    for y in range(y0, y1):
        row = image[y]
        for x in range(x0, x1):
            i = x * channels
            r, g, b = row[i], row[i + 1], row[i + 2]
            y_value = luma(r, g, b)
            values.append(y_value)
            total += y_value
            count += 1
            if y_value > peak:
                peak = y_value
            if min(r, g, b) >= WHITE_LEVEL:
                white += 1
            if y_value >= CLIP_LEVEL:
                clipped += 1
    values.sort()
    return {
        "pixels": count,
        "peakLuma": round(peak, 2),
        "p99Luma": round(values[min(count - 1, int(count * 0.99))], 2),
        "meanLuma": round(total / count, 2),
        "whiteFraction": white / count,
        "clippedFraction": clipped / count,
    }


def verdict(stats):
    """True when the box is neither clipped nor blown out."""
    return stats["peakLuma"] < CLIP_LEVEL and stats["whiteFraction"] < MAX_WHITE_FRACTION


def measure_shot(directory, resolution, shot):
    """Measure one manifest shot entry; returns a result dict, or None when it is not a captured impact shot with a footprint."""
    if shot.get("mode") != "impact" or shot.get("status") != "captured":
        return None
    footprint = shot.get("target", {}).get("footprint")
    if not footprint:
        return None
    box = (footprint["x0"], footprint["y0"], footprint["x1"], footprint["y1"])
    width, height, channels, image = read_png(directory / shot["file"], rows=box[3], columns=box[2])
    if resolution and (width, height) != tuple(resolution):
        raise ValueError(f"{shot['file']} is {width}x{height}, the manifest says {resolution[0]}x{resolution[1]}")
    stats = measure_box(image, channels, box)
    stats.update({"shot": shot["name"], "target": shot["target"].get("name"), "cell": [shot["target"].get("cellX"), shot["target"].get("cellY")],
                  "box": list(box), "size": [width, height], "pass": verdict(stats)})
    return stats


def measure_manifest(manifest_path, names=None):
    manifest_path = Path(manifest_path)
    manifest = json.loads(manifest_path.read_text())
    size = manifest.get("resolution") or {}
    resolution = (size["width"], size["height"]) if "width" in size else None
    results = []
    for shot in manifest.get("shots", []):
        if names and shot.get("name") not in names:
            continue
        result = measure_shot(manifest_path.parent, resolution, shot)
        if result:
            result["manifest"] = str(manifest_path)
            result["fixture"] = manifest.get("fixture", "")
            results.append(result)
    return results


def unmeasured_shots(manifest_path, names=None):
    """Captured impact shots with no footprint box (the struck unit was off screen), as 'fixture WxH shot' labels."""
    manifest = json.loads(Path(manifest_path).read_text())
    size = manifest.get("resolution") or {}
    return [f"{manifest.get('fixture', '-')} {size.get('width', '?')}x{size.get('height', '?')} {shot['name']}" for shot in manifest.get("shots", [])
            if shot.get("mode") == "impact" and shot.get("status") == "captured" and not shot.get("target", {}).get("footprint")
            and (not names or shot["name"] in names)]


def find_manifests(path):
    path = Path(path)
    if path.is_file():
        return [path]
    return sorted(path.rglob("manifest.json"))


def format_row(result):
    return (f"{result['fixture'] or '-':24} {result['size'][0]}x{result['size'][1]:<5} {result['shot']:30} peak {result['peakLuma']:6.1f}"
            f"  p99 {result['p99Luma']:6.1f}  mean {result['meanLuma']:6.1f}  white {result['whiteFraction'] * 100:5.2f}%"
            f"  clipped {result['clippedFraction'] * 100:5.2f}%  {'ok' if result['pass'] else 'BLOWN OUT'}")


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("path", help="a gallery run folder (every manifest.json below it) or one manifest.json")
    parser.add_argument("--shot", action="append", help="measure only this shot name (repeatable)")
    parser.add_argument("--json", metavar="FILE", help="also write every measurement to FILE")
    args = parser.parse_args(argv)
    results, unmeasured = [], []
    for manifest in find_manifests(args.path):
        results.extend(measure_manifest(manifest, set(args.shot or [])))
        unmeasured.extend(unmeasured_shots(manifest, set(args.shot or [])))
    if not results:
        print("No impact shots with a footprint box were found.", file=sys.stderr)
        return 2
    for result in results:
        print(format_row(result))
    for name in unmeasured:
        print(f"{name}: no footprint box (the target is off screen at this zoom), not measured", file=sys.stderr)
    if args.json:
        Path(args.json).write_text(json.dumps({"whiteLevel": WHITE_LEVEL, "clipLevel": CLIP_LEVEL, "maxWhiteFraction": MAX_WHITE_FRACTION, "shots": results}, indent=2) + "\n")
    failed = [result for result in results if not result["pass"]]
    print(f"{len(results) - len(failed)} of {len(results)} impact shots stay below clipping with under {MAX_WHITE_FRACTION * 100:.0f}% pure white")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
