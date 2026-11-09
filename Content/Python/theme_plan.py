"""Pure-Python half of the theme import (no `unreal` import, so unit tests can run it): what the Drowned Vigil
world kit r002 imports, which existing assets a run may replace, and how to read an export.

Used by import_dark_fantasy_theme.py (issue #145 phase 2). The rules:
  - Every r002 asset has a table row. A row whose package already exists is SKIPPED, never overwritten.
  - Only the ids in REPLACEABLE (the four revised r001 packages) may be replaced, and only when the run is told so
    explicitly with the environment variable DND_THEME_REIMPORT=<id>[,<id>...] (or `all`, meaning exactly REPLACEABLE).
    An id outside REPLACEABLE is an error, not a silent no-op.
  - Nothing here touches r001 assets.
"""
import hashlib
import json
import os
import re
import struct

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
REIMPORT_ENV = "DND_THEME_REIMPORT"
NANITE_FIX_ENV = "DND_THEME_FIX_R001_NANITE"
REVISION_TAG = "DiceRollAssetRevision"

# Kinds pick the material parent: lit = M_DiceRollTheme, occluder = M_DiceRollOccluder (dithered mask), card = M_DiceRollCard.
LIT, OCCLUDER, CARD = "lit", "occluder", "card"

# (asset id, revision, export name, folder, kind, theme property, index or None, (roughness, rim, fill, glow, self_lit))
# Property + index: an index puts the mesh in that slot of an array property (variants, scatter, mist, shafts).
STONE = (0.92, 0.0, 0.05, 1.0, 0.0)
PILLAR = (0.9, 0.0, 0.05, 1.0, 0.0)
WALL = (0.9, 0.0, 0.05, 1.0, 0.0)
GROUND = (0.9, 0.0, 0.0, 1.0, 0.0)
CARDLOOK = (1.0, 0.0, 0.0, 0.0, 0.0)
R002 = [
    ("world-vigil", 2, "SM_World_Vigil", "Worlds", LIT, "vigil_world", None, GROUND),
    ("world-vigil-occluders", 1, "SM_World_Vigil_Occluders", "Worlds", OCCLUDER, "vigil_occluders", None, GROUND),
    ("prop-pillar", 2, "SM_Prop_Pillar", "Props", LIT, "pillar_variants", 0, PILLAR),
    ("prop-pillar-b", 1, "SM_Prop_Pillar_B", "Props", LIT, "pillar_variants", 1, PILLAR),
    ("prop-pillar-c", 1, "SM_Prop_Pillar_C", "Props", LIT, "pillar_variants", 2, PILLAR),
    ("prop-wall", 2, "SM_Prop_Wall", "Props", LIT, "wall_variants", 0, WALL),
    ("prop-wall-b", 1, "SM_Prop_Wall_B", "Props", LIT, "wall_variants", 1, WALL),
    ("prop-wall-c", 1, "SM_Prop_Wall_C", "Props", LIT, "wall_variants", 2, WALL),
    ("prop-stones", 2, "SM_Prop_Stones", "Props", LIT, "stones_variants", 0, STONE),
    ("prop-stones-b", 1, "SM_Prop_Stones_B", "Props", LIT, "stones_variants", 1, STONE),
    ("prop-stones-c", 1, "SM_Prop_Stones_C", "Props", LIT, "stones_variants", 2, STONE),
    ("prop-arch-2cell", 1, "SM_Prop_Arch2Cell", "Props", LIT, "arch_two_cell", None, WALL),
    ("scatter-rubble", 1, "SM_Scatter_Rubble", "Scatter", LIT, "scatter", 0, (0.92, 0.0, 0.05, 1.0, 0.0)),
    ("scatter-candles", 1, "SM_Scatter_Candles", "Scatter", LIT, "scatter", 1, (0.8, 0.0, 0.0, 1.0, 0.0)),
    ("scatter-bones", 1, "SM_Scatter_Bones", "Scatter", LIT, "scatter", 2, (0.85, 0.0, 0.05, 1.0, 0.0)),
    ("scatter-roots", 1, "SM_Scatter_Roots", "Scatter", LIT, "scatter", 3, (1.0, 0.0, 0.05, 1.0, 0.0)),
    ("scatter-weapons", 1, "SM_Scatter_Weapons", "Scatter", LIT, "scatter", 4, (0.5, 0.0, 0.05, 1.0, 0.0)),
    ("scatter-puddle", 1, "SM_Scatter_Puddle", "Scatter", LIT, "scatter", 5, (0.25, 0.0, 0.0, 1.0, 0.0)),
    ("scatter-urn", 1, "SM_Scatter_Urn", "Scatter", LIT, "scatter", 6, (0.7, 0.0, 0.05, 1.0, 0.0)),
    ("scatter-moss", 1, "SM_Scatter_Moss", "Scatter", LIT, "scatter", 7, (1.0, 0.0, 0.0, 1.0, 0.0)),
    ("fx-mist-card", 1, "SM_Fx_MistCard", "Fx", CARD, "mist_cards", 0, CARDLOOK),
    ("fx-mist-card-b", 1, "SM_Fx_MistCard_B", "Fx", CARD, "mist_cards", 1, CARDLOOK),
    ("fx-mist-card-c", 1, "SM_Fx_MistCard_C", "Fx", CARD, "mist_cards", 2, CARDLOOK),
    ("fx-light-shaft", 1, "SM_Fx_LightShaft", "Fx", CARD, "light_shafts", 0, CARDLOOK),
    ("fx-light-shaft-b", 1, "SM_Fx_LightShaft_B", "Fx", CARD, "light_shafts", 1, CARDLOOK),
]

# The only existing r001 packages a run may replace, each with the reason. They keep their package path (the asset
# records say so: a rename is a separate coordinated migration), so the r002 mesh replaces the r001 mesh in place.
REPLACEABLE = {
    "world-vigil": "r002 world at true metric",
    "prop-pillar": "variant A rebuilt at the 150 cm cell",
    "prop-wall": "variant A rebuilt at the 150 cm cell",
    "prop-stones": "variant A rebuilt at the 150 cm cell",
}
# The kit is switched on (UDiceRollTheme::bMetricKit) only when every one of these carries the r002 tag.
METRIC_GATE = ("world-vigil", "prop-pillar", "prop-wall", "prop-stones")
ARRAY_SIZES = {"pillar_variants": 3, "wall_variants": 3, "stones_variants": 3, "scatter": 8, "mist_cards": 3, "light_shafts": 2}


def export_path(asset_id, revision, name):
    return os.path.join(ROOT, "Art", asset_id, "r%03d" % revision, name + ".glb")


def sha256_file(path):
    digest = hashlib.sha256()
    with open(path, "rb") as handle:
        for block in iter(lambda: handle.read(1 << 20), b""):
            digest.update(block)
    return digest.hexdigest()


def requested_replacements(value):
    """The set of asset ids a run may replace, from DND_THEME_REIMPORT. Raises on an id outside REPLACEABLE."""
    value = (value or "").strip()
    if not value:
        return set()
    if value.lower() == "all":
        return set(REPLACEABLE)
    ids = {part.strip() for part in re.split(r"[,\s]+", value) if part.strip()}
    unknown = sorted(ids - set(REPLACEABLE))
    if unknown:
        raise ValueError("Not replaceable (allow-list is %s): %s" % (", ".join(sorted(REPLACEABLE)), ", ".join(unknown)))
    return ids


def check_import_result(paths, package, filename):
    """Validates what an import task reported (task.imported_object_paths) for a reimport over `package`.

    The original object stays loaded when an import fails, so identity alone proves nothing: the import must have reported the
    expected package, and nothing else. Raises RuntimeError otherwise.
    """
    packages = [str(path).split(".")[0] for path in paths]
    if package not in packages:
        raise RuntimeError("Reimporting {} did not report {} among its imported objects ({}); the import failed".format(filename, package, packages))
    strays = [path for path in packages if path != package]
    if strays:
        raise RuntimeError("Reimporting {} over {} also created {}; remove them and fix the import".format(filename, package, strays))


def lod_exports(asset_id, revision, name):
    """Explicit LOD exports beside an export: <name>_LOD1.glb, <name>_LOD2.glb ... in order. Empty when none are authored."""
    folder = os.path.dirname(export_path(asset_id, revision, name))
    found = {}
    if os.path.isdir(folder):
        for entry in os.listdir(folder):
            match = re.fullmatch(re.escape(name) + r"_LOD(\d+)\.glb", entry)
            if match and int(match.group(1)) >= 1:
                found[int(match.group(1))] = os.path.join(folder, entry)
    return [found[key] for key in sorted(found)]


def record_lods(asset_id, revision):
    """The LOD exports (index, repository path, sha256) an asset record lists, in LOD order; empty for the exempt (cards, small scatter)."""
    with open(os.path.join(ROOT, "Art", asset_id, "r%03d" % revision, "asset.json")) as handle:
        record = json.load(handle)
    found = []
    for entry in record["files"]:
        match = re.search(r"_LOD(\d+)\.glb$", entry["path"])
        if entry["role"] == "export" and match:
            found.append((int(match.group(1)), entry["path"], entry["sha256"]))
    return sorted(found)


def read_glb(path):
    with open(path, "rb") as handle:
        data = handle.read()
    if data[:4] != b"glTF":
        raise ValueError(path + " is not a binary glTF file")
    json_length = struct.unpack("<I", data[12:16])[0]
    header = json.loads(data[20:20 + json_length])
    binary_at = 20 + json_length
    binary = data[binary_at + 8:binary_at + 8 + struct.unpack("<I", data[binary_at:binary_at + 4])[0]]
    return header, binary


_FORMATS = {5126: ("f", 4, 1.0), 5121: ("B", 1, 255.0), 5123: ("H", 2, 65535.0)}


def has_alpha_mask(path):
    """True when the export's COLOR_0 is VEC4 and its alpha really varies (a glow mask or opacity), else False.

    glTF COLOR_0 is optional and often RGB only; Unreal then reads alpha 1 everywhere, which would make a whole mesh
    glow. Only a VEC4 colour with a spread of alpha counts as a mask.
    """
    if not os.path.isfile(path):
        return False
    header, binary = read_glb(path)
    for mesh in header.get("meshes", []):
        for primitive in mesh.get("primitives", []):
            index = primitive.get("attributes", {}).get("COLOR_0")
            if index is None:
                continue
            accessor = header["accessors"][index]
            if accessor["type"] != "VEC4":
                continue
            view = header["bufferViews"][accessor["bufferView"]]
            code, size, divisor = _FORMATS[accessor["componentType"]]
            start = view.get("byteOffset", 0) + accessor.get("byteOffset", 0)
            stride = view.get("byteStride", 4 * size)
            alphas = [struct.unpack_from("<" + code, binary, start + i * stride + 3 * size)[0] / divisor for i in range(accessor["count"])]
            if alphas and max(alphas) - min(alphas) > 0.02 and min(alphas) < 0.99:
                return True
    return False


def triangle_count(path):
    """Triangles in an export (indexed triangle lists only; the exporters write nothing else)."""
    header, _ = read_glb(path)
    total = 0
    for mesh in header.get("meshes", []):
        for primitive in mesh.get("primitives", []):
            if primitive.get("mode", 4) != 4:
                raise ValueError("Unsupported primitive mode in " + path)
            accessor = header["accessors"][primitive["indices"]] if "indices" in primitive else header["accessors"][primitive["attributes"]["POSITION"]]
            total += accessor["count"] // 3
    return total


def package_path(folder, name):
    return "/Game/DnDDiceRoll/Theme/%s/%s" % (folder, name)
