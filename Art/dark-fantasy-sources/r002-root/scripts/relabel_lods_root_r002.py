"""Rename the mesh, node and material inside every Root r002 LOD glb to Unreal Interchange's LOD pattern 'LODxx_Name' (issue #149; adapted from #145's relabel_lods_r002.py).

  python3 Art/dark-fantasy-sources/r002-root/scripts/relabel_lods_root_r002.py        (standard library only; safe to rerun)

Interchange (UInterchangeMeshLODContainerNode::CheckForLODPattern, read from the glTF *node* name by
FindNamedLodGroup) warns "Possible LOD pattern mismatch. Expecting 'LODxx_Name'" for a name that contains 'LOD' without starting with it, such as
SM_Prop_Pillar_LOD1.  build_lods_root_r002.py names new LODs LOD1_SM_Prop_Pillar; this rewrites the names of LODs it exported before
without touching any geometry, UV, colour or image byte: only the JSON chunk changes (nodes[].name, meshes[].name, materials[].name),
re-padded to 4 bytes, plus the two glb length fields.  Then lod_metrics.json gets the new hashes.  Rerun write_records_root_r002.py afterwards.
The LOD0 glbs are not read or written.
"""
import hashlib
import json
import os
import re
import struct

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, '..', '..', '..', '..'))
METRICS = os.path.join(ROOT, 'Art', 'dark-fantasy-sources', 'r002-root', 'lod_metrics.json')
OLD = re.compile(r'^(M_)?(.+)_LOD(\d+)(?:\.\d{3})?$')  # Blender may add a .001 duplicate suffix


NEW = re.compile(r'^(M_)?LOD(\d+)_(.+?)(?:\.\d{3})?$')   # already LOD<n>_<name>, possibly with Blender's duplicate suffix


def renamed(value):
    match = OLD.match(value)
    if match:
        return f'{match.group(1) or ""}LOD{match.group(3)}_{match.group(2)}'
    match = NEW.match(value)
    return f'{match.group(1) or ""}LOD{match.group(2)}_{match.group(3)}' if match else value


def relabel(path):
    data = open(path, 'rb').read()
    json_length = struct.unpack('<I', data[12:16])[0]
    head = json.loads(data[20:20 + json_length])
    changed = False
    for key in ('nodes', 'meshes', 'materials'):
        for item in head.get(key, []):
            if 'name' in item and renamed(item['name']) != item['name']:
                item['name'] = renamed(item['name'])
                changed = True
    if not changed:
        return False
    text = json.dumps(head, separators=(',', ':')).encode()
    text += b' ' * (-len(text) % 4)
    rest = data[20 + json_length:]
    out = b'glTF' + struct.pack('<II', 2, 12 + 8 + len(text) + len(rest)) + struct.pack('<I', len(text)) + b'JSON' + text + rest
    open(path, 'wb').write(out)
    return True


def main():
    metrics = json.load(open(METRICS))
    count = 0
    for entry in metrics.values():
        for row in entry['lods']:
            path = os.path.join(ROOT, row['file'])
            count += relabel(path)
            row['sha256'] = hashlib.sha256(open(path, 'rb').read()).hexdigest()
            row['bytes'] = os.path.getsize(path)
    with open(METRICS, 'w') as handle:
        json.dump(metrics, handle, indent=2)
        handle.write('\n')
    print(f'relabelled {count} LOD glbs')


main()
