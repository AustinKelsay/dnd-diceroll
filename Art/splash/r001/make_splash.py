"""Build the Linux game splash, Content/Splash/Splash.bmp, from the project's own tabletop art.

Run from the repository root:  python3 Art/splash/r001/make_splash.py

1. Blender renders Art/tabletop/r001/SM_Tabletop.glb unlit (texture x baked vertex light, the same
   look as the dark-fantasy theme renders) at twice the splash size -> Art/splash/r001/backdrop.png.
2. ImageMagick dims the bookshelf behind the title, adds a vignette, the gilded double frame with
   corner diamonds used by the in-game panels, and the "dnd-diceroll" title, then box-downsamples
   to 720x370 -> Art/splash/r001/Splash.png (preview) and Content/Splash/Splash.bmp.

The packaged Linux game loads the splash with SDL_LoadBMP, so the runtime file must be an
uncompressed BMP (written here as 24-bit BMP3). 720x370 matches the engine's own Splash.bmp.
Tools: Blender 5.2.3 LTS, ImageMagick 7.1.2, font Noto Serif Display (SIL OFL 1.1).
"""
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, '..', '..', '..'))
W, H = 720, 370
SS = 2  # supersampling factor for the render and the composition
FONT = '/usr/share/fonts/noto/NotoSerifDisplay-Bold.ttf'
GOLD = '#d8b46a'
GOLD_HI = '#f3dca0'
GOLD_LO = '#7a5a26'


def render_backdrop(out):
    """Runs inside Blender."""
    import bpy
    from mathutils import Vector
    sys.path.insert(0, os.path.join(ROOT, 'Art/dark-fantasy-sources/r001/scripts'))
    import render_lib as R
    R.setup_scene((W * SS, H * SS), bg=(0, 0, 0))
    for o in R.import_glb(os.path.join(ROOT, 'Art/tabletop/r001/SM_Tabletop.glb')):
        R.unlit_vertex_color(o)
    cam = bpy.data.cameras.new('splash')
    cam.lens = 22
    c = bpy.data.objects.new('splash', cam)
    bpy.context.scene.collection.objects.link(c)
    c.location = Vector((1.55, 0.0, 0.62))
    c.rotation_euler = (Vector((-0.4, 0.0, 0.18)) - c.location).to_track_quat('-Z', 'Y').to_euler()
    bpy.context.scene.camera = c
    sc = bpy.context.scene
    sc.render.image_settings.file_format = 'PNG'
    sc.render.image_settings.color_mode = 'RGB'
    sc.render.filepath = out
    bpy.ops.render.render(write_still=True)


def magick(*args):
    subprocess.run(['magick', *map(str, args)], check=True)


def frame_draw(w, h):
    """Draw primitives for the gilded double frame and its corner diamonds (supersampled px)."""
    s = SS
    o, i = 9 * s, 15 * s  # outer and inner frame insets
    cmds = [
        f'fill none stroke {GOLD_LO} stroke-width {6 * s} rectangle {o},{o} {w - o - 1},{h - o - 1}',
        f'fill none stroke {GOLD} stroke-width {2 * s} rectangle {o},{o} {w - o - 1},{h - o - 1}',
        f'fill none stroke {GOLD} stroke-width {1 * s} rectangle {i},{i} {w - i - 1},{h - i - 1}',
    ]
    d = 6 * s
    for x, y in ((o, o), (w - o - 1, o), (o, h - o - 1), (w - o - 1, h - o - 1)):
        cmds.append(f'fill {GOLD_HI} stroke {GOLD_LO} stroke-width {s} '
                    f'polygon {x},{y - d} {x + d},{y} {x},{y + d} {x - d},{y}')
    return cmds


def compose(backdrop, preview, bmp):
    w, h = W * SS, H * SS
    title_y = 112 * SS  # title baseline region, over the dimmed bookshelf
    rule_y = 150 * SS
    tmp = os.path.join(HERE, '.compose.png')
    args = [backdrop,
            # Dim the bookshelf behind the title; keep the candlelit table at full brightness.
            '(', '-size', f'{w}x{int(h * .62)}', 'gradient:#707070-#ffffff',
            '-size', f'{w}x{h - int(h * .62)}', 'xc:#ffffff', '-append', ')',
            '-compose', 'Multiply', '-composite',
            # Soft vignette toward the frame.
            '(', '-size', f'{w}x{h}', 'radial-gradient:#ffffff-#5a5a5a', '-level', '0%,115%', ')',
            '-compose', 'Multiply', '-composite',
            '-compose', 'Over']
    # Title: a dark halo for legibility, then a vertical gold gradient fill.
    title = 'dnd-diceroll'
    pt = 62 * SS
    args += ['(', '-size', f'{w}x{h}', 'xc:none', '-font', FONT, '-pointsize', pt, '-gravity', 'north',
             '-fill', '#000000', '-annotate', f'+0+{title_y - 52 * SS}', title,
             '-channel', 'A', '-morphology', 'Dilate', f'Disk:{3 * SS}', '-blur', f'0x{6 * SS}',
             '-evaluate', 'multiply', '0.9', '+channel', ')', '-composite']
    # Gold runs light to dark across the letters only (top of the ascenders to the baseline).
    top, band = title_y - 38 * SS, 48 * SS
    args += ['(', '-size', f'{w}x{band}', 'gradient:' + GOLD_HI + '-' + '#a07a38',
             '-background', GOLD_HI, '-gravity', 'north', '-splice', f'0x{top}',
             '-background', '#a07a38', '-gravity', 'north', '-extent', f'{w}x{h}',
             '(', '-size', f'{w}x{h}', 'xc:black', '-font', FONT, '-pointsize', pt, '-gravity', 'north',
             '-fill', 'white', '-annotate', f'+0+{title_y - 52 * SS}', title, ')',
             '-alpha', 'off', '-compose', 'CopyOpacity', '-composite', ')',
             '-compose', 'Over', '-composite']
    # Ornamental rule with a centre diamond under the title.
    cx, rw, d = w // 2, 120 * SS, 5 * SS
    draw = [f'stroke {GOLD} stroke-width {SS} line {cx - rw},{rule_y} {cx - 2 * d},{rule_y}',
            f'stroke {GOLD} stroke-width {SS} line {cx + 2 * d},{rule_y} {cx + rw},{rule_y}',
            f'fill {GOLD_HI} stroke {GOLD_LO} stroke-width {SS} '
            f'polygon {cx},{rule_y - d} {cx + d},{rule_y} {cx},{rule_y + d} {cx - d},{rule_y}']
    draw += frame_draw(w, h)
    for cmd in draw:
        args += ['-draw', cmd]
    args += [tmp]
    magick(*args)
    magick(tmp, '-filter', 'Box', '-resize', f'{W}x{H}!', '-strip', preview)
    os.makedirs(os.path.dirname(bmp), exist_ok=True)
    magick(preview, '-alpha', 'off', '-type', 'TrueColor', 'BMP3:' + bmp)
    os.remove(tmp)


if __name__ == '__main__':
    if '--' in sys.argv:  # inside Blender
        render_backdrop(sys.argv[sys.argv.index('--') + 1])
    else:
        backdrop = os.path.join(HERE, 'backdrop.png')
        subprocess.run(['blender', '-b', '--factory-startup', '--python', os.path.abspath(__file__),
                        '--', backdrop], check=True, stdout=subprocess.DEVNULL)
        compose(backdrop, os.path.join(HERE, 'Splash.png'), os.path.join(ROOT, 'Content/Splash/Splash.bmp'))
        print('wrote', os.path.join(ROOT, 'Content/Splash/Splash.bmp'))
