"""Height maps of the r002 worlds (max z per 1 m cell, world + occluders) to choose the diorama windows: previews/survey_<name>.png, 1 px per 0.5 m, +-48 m.

  blender -b --python-exit-code 1 --python Art/dark-fantasy-sources/r002-overview/scripts/survey_worlds.py      (under the machine lock)
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import ov_lib as O
from ov_lib import L, np
from build_dioramas_r002 import SPECS

def main():
    for key, spec in SPECS.items():
        zmap = np.zeros((192, 192))
        cnt = np.zeros((192, 192))
        for tag, path, down in spec['sources']:
            P, UV, COL, img = O.read_glb_arrays(path)
            c = P.reshape(-1, 3)
            ix = np.clip(((c[:, 0] + 48) / .5).astype(int), 0, 191)
            iy = np.clip(((c[:, 1] + 48) / .5).astype(int), 0, 191)
            np.maximum.at(zmap, (iy, ix), c[:, 2])
            np.add.at(cnt, (iy, ix), 1)
            print(key, tag, 'tris', len(P), 'bbox', c.min(0).round(1), c.max(0).round(1), flush=True)
        a = np.zeros((192, 192, 4), np.float32)
        v = np.clip(zmap / 12.0, 0, 1)
        a[..., 0], a[..., 1], a[..., 2], a[..., 3] = v, np.where(cnt > 0, .25 + .5 * v, 0), np.where(cnt > 0, .2, 0), 1
        x0, x1, y0, y1 = spec['window']
        for x in range(int((x0 + 48) / .5), int((x1 + 48) / .5)):           # the window in blue
            for y in (int((y0 + 48) / .5), int((y1 + 48) / .5)):
                a[min(191, y), x, :3] = (0, .6, 1)
        for y in range(int((y0 + 48) / .5), int((y1 + 48) / .5)):
            for x in (int((x0 + 48) / .5), int((x1 + 48) / .5)):
                a[y, min(191, x), :3] = (0, .6, 1)
        a = np.repeat(np.repeat(a[::-1], 4, 0), 4, 1)                        # +Y up on the picture, 4 x
        L.save_png(a[::-1][::-1], os.path.join(O.OUT_SRC, 'previews', f'survey_{key}.png'))


def run_guarded(entry):
    """Blender exits 0 when a script raises; every way out of these scripts but success must be non-zero, so an unexpected exception is printed and exits 1.
    (Run Blender with --python-exit-code 1 as well: it covers an error before this point, such as a failed import.)"""
    try:
        entry()
    except SystemExit:
        raise
    except BaseException:
        import traceback
        traceback.print_exc()
        sys.exit(1)


if __name__ == '__main__':
    run_guarded(main)
