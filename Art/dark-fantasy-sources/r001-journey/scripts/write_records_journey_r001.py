"""Asset records (asset.json) for the #248 journey map: thirteen new asset ids, r001 each, new packages under /Game/DnDDiceRoll/Theme/Journey.

  python3 Art/dark-fantasy-sources/r001-journey/scripts/write_records_journey_r001.py     (after build_journey_r001.py; standard library only)

Records follow docs/agents/asset-manifests.md; hashes are of the files on disk when this runs.  validation.state stays 'pending' until the import and the
on-screen checks are recorded.
"""
import copy
import hashlib
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, '..', '..', '..', '..'))
SRC = os.path.join(ROOT, 'Art', 'dark-fantasy-sources')
JR = os.path.join(SRC, 'r001-journey')
ISSUE = 'https://github.com/AustinKelsay/dnd-diceroll/issues/248'


def sha(path):
    return hashlib.sha256(open(os.path.join(ROOT, path), 'rb').read()).hexdigest()


POLICY = dict(
    name='dnd-diceroll-dark-fantasy-theme', version=2,
    settings=dict(
        importer='Interchange glTF via Content/Python/import_dark_fantasy_theme.py, explicit reviewed import (#248 journey map)',
        material='MI_* child of M_DiceRollTheme: texture x COLOR_0.rgb, glow from COLOR_0.a (the fog bank: MI_* child of M_DiceRollCard, opacity from COLOR_0.a)',
        texture_filter='trilinear with anisotropic 8, mips generated, never nearest',
        texture_compression='default (sRGB, mips); no nearest or editor-icon compression',
        nanite='off, set explicitly on import',
        lods='none: small meshes drawn only on the route screen and the ending (the largest, the table, is 7.6k triangles)',
        collision='none (presentation only)',
        vertex_colour='COLOR_0 VEC4 normalised uint16 (RGB baked light, A glow mask; opacity for the fog card)',
        axes="metres, the asset faces +X (the journey turns it -90 degrees about Z toward the camera), +Y is the viewer's right, Z up (glTF Y-up export); the ink strokes run 1 m along +X from the origin"))

SCRIPTS = [f'Art/dark-fantasy-sources/r001/scripts/{n}' for n in ('tlib.py', 'prims.py', 'render_lib.py')] + \
          [f'Art/dark-fantasy-sources/r002-vault/scripts/{n}' for n in ('r002_lib.py', 'vault_lib.py')] + \
          [f'Art/dark-fantasy-sources/r002-overview/scripts/{n}' for n in ('ov_lib.py', 'build_tabletop_r002.py')] + \
          ['Art/dark-fantasy-sources/r001-journey/scripts/build_journey_r001.py', 'Art/dark-fantasy-sources/r001-journey/journey_layout.json']

METHODS = {
    'SM_Journey_Table': 'A clean 6 x 2.75 m war table built with the r002 table pieces (planks at 256 texels/m, apron, a brass candlestick and a pillar candle at the back corners whose flames carry the glow mask, an hourglass, books, coins, inkwell, scroll and tankard at the far edges), nothing in the middle; a baked warm candle light',
    'SM_Journey_Map': 'A 3.65 x 2.32 m vellum chart painted by script (about 540 texels/m): coast and river, forest, marsh and drowned spires, mountains and the Vault crypt, the pencilled road and a double red socket at each stage, trailhead and goal where JourneyLayout.h places them, compass rose, a ribboned cartouche with no lettering, border, folds, a scorched corner; three stone weights',
    'SM_Journey_Stage': 'A 16 cm rimmed bronze disc with twelve studs on an oak foot, light and neutral so the game tints it per stage state',
    'SM_Journey_Emblem_Root': 'A 10 cm brass-rimmed medallion on a post, tilted back 35 degrees toward the camera: a gilt leaf on green enamel (the Root of the World)',
    'SM_Journey_Emblem_Vigil': 'A 10 cm brass-rimmed medallion on a post, tilted back 35 degrees toward the camera: silver waves under an arch on blue enamel (the Drowned Vigil)',
    'SM_Journey_Emblem_Vault': 'A 10 cm brass-rimmed medallion on a post, tilted back 35 degrees toward the camera: a glowing cyan rune on slate (the Vault); the face carries the glow mask',
    'SM_Journey_Pennant': 'An 18 cm brass pin with a two-sided swallow-tailed red pennant and a gilt trim, beside a cleared token',
    'SM_Journey_Lost': 'A broken sword driven into the token (steel stump with a jagged break, iron guard, leather grip) and its tip lying beside it',
    'SM_Journey_Ink': 'A 1 m walked-ink stroke (red-brown, 2.6 cm, wavering width) along +X from the origin, a hair above the map; stretched between stops',
    'SM_Journey_InkDashed': 'The same stroke, narrower, in dashes of pencil brown: the road not yet walked',
    'SM_Journey_Fog': 'A 48 x 41 cm low fog bank: five layered soft discs and two curved curtains, two-sided, opacity in COLOR_0.a and a cloud-noise texture; hides a stage until it is reached',
    'SM_Journey_Goal': 'The Vault keep at the end of the road: a crenellated slate crypt on a stone base with an iron door toward the camera and a floating cyan Waystone shard carrying the glow mask',
    'SM_Journey_Plinth': 'The riser a raised diorama stands on: an oak block 0.72 x 0.74 x 0.14 m with moulded lips, brass corner caps and a front plate (the route name is drawn by the HUD)',
}


def record(asset, name, m):
    package = f'/Game/DnDDiceRoll/Theme/Journey/{name}'
    glb = m['file']
    params = dict(units='metres', pivot='the middle of the table top' if name in ('SM_Journey_Table', 'SM_Journey_Map') else 'the start of the stroke' if 'Ink' in name else 'the middle of the underside; placed on the map',
                  triangles=m['triangles'], size_m=m['size_m'], atlas_px=m['atlas_px'], glow_triangles=m['glow_tris'], replaces='nothing (new package)')
    return dict(
        schema_version=1, asset_id=asset, revision=1, unreal_package=package, issue=ISSUE,
        provenance=dict(author='dnd-diceroll project agents (Claude Opus 5.5) for the repository owner',
                        license='Project-owned original work; same terms as this repository',
                        origin='Authored by script in Blender 5.2.3 LTS from procedural geometry and textures with the r002 overview helpers; no third-party, kit, AI-service or downloaded content'),
        recipe=dict(method=METHODS[name], approved_reference='#239 decisions (the war table is the journey map, fog of war, the Vault as the goal); #149 overview set look',
                    parameters=params, model_revision='not applicable (procedural Blender scripts)'),
        tools=dict(blender='5.2.3 LTS'),
        files=[dict(role='source', path=p, sha256=sha(p)) for p in SCRIPTS] + [dict(role='export', path=glb, sha256=sha(glb))],
        import_policy=copy.deepcopy(POLICY), validation=dict(state='pending', evidence=[]))


def main():
    metrics = json.load(open(os.path.join(JR, 'journey_metrics.json')))
    for name, m in metrics.items():
        rec = record(m['asset_id'], name, m)
        d = os.path.join(ROOT, 'Art', rec['asset_id'], 'r001')
        os.makedirs(d, exist_ok=True)
        with open(os.path.join(d, 'asset.json'), 'w') as fh:
            json.dump(rec, fh, indent=2)
            fh.write('\n')
        print('RECORD', os.path.relpath(os.path.join(d, 'asset.json'), ROOT))


if __name__ == '__main__':
    main()
