"""Write the versioned asset records (asset.json) for the Waystone set piece r001 exports (#250), from setpiece_metrics.json.

  python3 Art/dark-fantasy-sources/r003-setpiece/scripts/write_records_setpiece.py     (run after build_setpiece_vault.py)

Standard library only. Records follow docs/agents/asset-manifests.md and the Vault kit's records (write_records_vault_r002.py); hashes are
of the files on disk when this runs, so rerun it after any rebuild. Every mesh is under 1k triangles, so none has LOD exports.
"""
import hashlib
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, '..', '..', '..', '..'))
SRC = os.path.join(ROOT, 'Art', 'dark-fantasy-sources', 'r003-setpiece')
ISSUE = 'https://github.com/AustinKelsay/dnd-diceroll/issues/250'


def sha(path):
    return hashlib.sha256(open(os.path.join(ROOT, path), 'rb').read()).hexdigest()


SOURCES = [f'Art/dark-fantasy-sources/r001/scripts/{n}' for n in ('tlib.py', 'prims.py')] + \
          [f'Art/dark-fantasy-sources/r002-vault/scripts/{n}' for n in ('r002_lib.py', 'vault_lib.py')] + \
          ['Art/dark-fantasy-sources/r003-setpiece/scripts/build_setpiece_vault.py']

POLICY = dict(
    name='dnd-diceroll-dark-fantasy-theme', version=2,
    settings=dict(
        importer='Interchange glTF via Content/Python/import_dark_fantasy_theme.py (theme_plan.SETPIECE_R001; new packages only, nothing replaced, no data-asset entry)',
        material='MI_* child of M_DiceRollTheme: texture x COLOR_0.rgb, glow from COLOR_0.a',
        texture_filter='trilinear with anisotropic 8, mips generated, never nearest',
        texture_compression='default (sRGB, mips); no nearest or editor-icon compression',
        nanite='off, set explicitly on import',
        lods='exempt: under ~1k triangles (no LOD exports)',
        collision='none (presentation only; the set piece never blocks anything)',
        vertex_colour='COLOR_0 VEC4 normalised uint16 (RGB light, A glow mask)',
        axes='metres, +X forward, Z up (glTF Y-up export)'))

DESC = {
    'setpiece-vault-seal-upper': ('the upper half of a rune-seal shard: a tapered hexagonal basalt crystal 0.30 m tall with a cold-blue rune channel on its +X face, '
                                  'its bottom the shared jagged crack', 'centre of the crack plane (z = 0); the lower half has the same pivot, so placing both at one transform shows the intact shard'),
    'setpiece-vault-seal-lower': ('the lower half of a rune-seal shard: 0.20 m, pointed tip down, its top the same jagged crack as the upper half', 'centre of the crack plane (z = 0), shared with the upper half'),
    'setpiece-vault-chip': ('a faceted basalt chip about 12 cm across with a rune fleck, thrown when the seal shatters', 'centre of the chip'),
}


def record(name, m):
    aid, rev = m['asset_id'], m['revision']
    what, pivot = DESC[aid]
    return dict(
        schema_version=1, asset_id=aid, revision=rev, unreal_package=m['unreal_package'], issue=ISSUE,
        provenance=dict(author='dnd-diceroll project agents (Claude Opus 5.5) for the repository owner',
                        license='Project-owned original work; same terms as this repository',
                        origin='Authored by script in Blender 5.2.3 LTS for the Vault finale set piece (#250): the rune seal around the Waystone artifact, in the Vault kit r002 look (blue-black basalt, one cold-blue glow accent); all textures and geometry procedural; no third-party, kit, AI-service or downloaded content'),
        recipe=dict(method='Authored in Blender by script (build_setpiece_vault.py): ' + what + '; vertex colour = ray-cast self ambient occlusion x height gradient; glow only via COLOR_0.a on the rune material',
                    approved_reference='The Vault kit r002 (#149, Art/dark-fantasy-sources/r002-vault); #250 scope (Austin, 2026-11-15)',
                    parameters=dict(units='metres at true scale (the Vault is authored at the 150 cm cell and placed unscaled)', axes='+X forward, Z up', pivot=pivot,
                                    triangles=m['triangles'], size_m=m['size_m'], bbox_min_m=m['bbox_min_m'], bbox_max_m=m['bbox_max_m'], atlas_px=m['atlas_px'],
                                    sampling='Linear, 8 px wrapped padding per atlas region', glow_triangles=m['glow_tris'], lods='none (exempt)',
                                    use='finale only: six shards float around the artifact in the apse and fly onto the dais along pre-authored arcs when the finale is won (no physics)'),
                    model_revision='not applicable (procedural Blender scripts)'),
        tools=dict(blender='5.2.3 LTS'),
        files=[dict(role='source', path=p, sha256=sha(p)) for p in SOURCES] + [dict(role='export', path=m['file'], sha256=sha(m['file']))],
        import_policy=json.loads(json.dumps(POLICY)), validation=dict(state='pending', evidence=[]))


def main():
    metrics = json.load(open(os.path.join(SRC, 'setpiece_metrics.json')))
    for name, m in metrics.items():
        if not name.startswith('SM_'):
            continue
        rec = record(name, m)
        d = os.path.join(ROOT, 'Art', rec['asset_id'], f"r{rec['revision']:03d}")
        os.makedirs(d, exist_ok=True)
        with open(os.path.join(d, 'asset.json'), 'w') as fh:
            json.dump(rec, fh, indent=2)
            fh.write('\n')
        print('wrote', os.path.relpath(os.path.join(d, 'asset.json'), ROOT))


main()
