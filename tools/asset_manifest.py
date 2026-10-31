"""Read-only asset record validation and revision planning. Python 3.11+, no dependencies."""

import argparse
import hashlib
import json
import math
from pathlib import Path, PurePosixPath
import re
import sys


def require(condition, message):
    if not condition:
        raise ValueError(message)


def fields(value, names, label):
    require(isinstance(value, dict) and set(value) == set(names.split()), f"{label}: expected fields {names}")


def positive(value):
    return type(value) is int and value > 0


def words(value):
    return isinstance(value, str) and bool(value.strip())


def unique_object(pairs):
    result = {}
    for key, value in pairs:
        require(key not in result, f"Duplicate JSON key: {key}")
        result[key] = value
    return result


def reject_constant(value):
    raise ValueError(f"Non-finite JSON number: {value}")


def finite_float(value):
    result = float(value)
    require(math.isfinite(result), f"Non-finite JSON number: {value}")
    return result


def inspect_record(path, root):
    """Check the manifest and each file's current bytes, including archived revisions."""
    root = Path(root).resolve()
    try:
        record = json.loads(Path(path).read_text(encoding="utf-8"), object_pairs_hook=unique_object,
                            parse_constant=reject_constant, parse_float=finite_float)
    except (OSError, UnicodeError, json.JSONDecodeError) as exc:
        raise ValueError(f"Cannot read asset record: {exc}") from exc
    fields(record, "schema_version asset_id revision unreal_package issue provenance recipe tools files import_policy validation", "record")
    require(type(record["schema_version"]) is int and record["schema_version"] == 1, "Unsupported schema_version")
    require(isinstance(record["asset_id"], str) and re.fullmatch(r"[a-z][a-z0-9]*(?:-[a-z0-9]+)*", record["asset_id"]), "Invalid asset_id")
    require(positive(record["revision"]), "revision must be a positive integer")
    package = record["unreal_package"]
    require(isinstance(package, str) and re.fullmatch(r"/Game/DnDDiceRoll/(?:[A-Za-z0-9_]+/)*[A-Za-z0-9_]+", package), "Invalid Unreal package under /Game/DnDDiceRoll")
    require(words(record["issue"]), "issue is required")
    fields(record["provenance"], "author license origin", "provenance")
    require(all(words(v) for v in record["provenance"].values()), "Complete provenance is required")
    require(isinstance(record["recipe"], dict) and bool(record["recipe"]), "recipe must describe generation or authored construction")
    require(isinstance(record["tools"], dict) and bool(record["tools"]) and
            all(words(k) and words(v) for k, v in record["tools"].items()), "tools must name tool versions")
    policy = record["import_policy"]
    fields(policy, "name version settings", "import_policy")
    require(words(policy["name"]) and positive(policy["version"]) and isinstance(policy["settings"], dict), "Invalid import policy")
    validation = record["validation"]
    fields(validation, "state evidence", "validation")
    require(validation["state"] in ("pending", "passed", "failed"), "Invalid validation state")
    require(isinstance(validation["evidence"], list) and all(words(v) for v in validation["evidence"]), "Evidence must be a list of references")
    require(validation["state"] != "passed" or bool(validation["evidence"]), "Passed validation needs evidence")
    require(isinstance(record["files"], list) and bool(record["files"]), "files must include source and export")
    seen, roles = set(), set()
    for entry in record["files"]:
        fields(entry, "role path sha256", "file")
        require(entry["role"] in ("source", "export", "reference"), "Unknown file role")
        relative = entry["path"]
        require(isinstance(relative, str) and "\\" not in relative, "Use repository-relative POSIX paths")
        parts = relative.split("/")
        require(len(parts) > 1 and parts[0] == "Art" and all(p not in ("", ".", "..") for p in parts), "Files must be within Art/ without traversal")
        require(relative not in seen, f"Duplicate file path: {relative}")
        seen.add(relative)
        roles.add(entry["role"])
        require(isinstance(entry["sha256"], str) and re.fullmatch(r"[0-9a-f]{64}", entry["sha256"]), "Invalid SHA-256")
        target = root.joinpath(*parts)
        require(target.resolve().is_relative_to(root / "Art"), f"File escapes Art/: {relative}")
        require(not any(root.joinpath(*parts[:i]).is_symlink() for i in range(1, len(parts) + 1)), f"Symlink in file path: {relative}")
        try:
            require(target.is_file(), f"Missing file: {relative}")
            with target.open("rb") as stream:
                require(not stream.read(128).startswith(b"version https://git-lfs.github.com/spec/v1"), f"Hydrate LFS file: {relative}")
                stream.seek(0)
                digest = hashlib.file_digest(stream, "sha256").hexdigest()
        except OSError as exc:
            raise ValueError(f"Cannot read {relative}: {exc}") from exc
        require(digest == entry["sha256"], f"Stale hash or modified source: {relative}")
    require({"source", "export"}.issubset(roles), "Include at least one editable source and export")
    return record


def json_identity(value):
    # Python equality treats True == 1; JSON import settings must preserve types.
    return json.dumps(value, sort_keys=True, separators=(",", ":"), allow_nan=False)


def export_location(path, asset_id):
    parts = PurePosixPath(path).parts
    # Only the documented revision container is relocatable. Keep subfolders so
    # textures with the same basename retain their association with materials.
    if (len(parts) > 3 and parts[:2] == ("Art", asset_id)
            and re.fullmatch(r"r0*[1-9][0-9]*", parts[2])):
        return ("revision", *parts[3:])
    return ("repository", *parts)


def import_inputs(record):
    return json_identity([sorted((export_location(f["path"], record["asset_id"]), f["sha256"])
                                for f in record["files"] if f["role"] == "export"), record["import_policy"]])


def plan_revision(current, previous=None):
    """Plan already inspected records; this never imports or approves an asset."""
    action = "new"
    if previous is None:
        require(current["revision"] == 1, "Supply --previous for an existing asset revision")
    if previous is not None:
        for key in ("asset_id", "unreal_package"):
            require(current[key] == previous[key], f"Revision changes stable {key}; coordinate a separate migration")
        if json_identity(current) == json_identity(previous):
            action = "unchanged"
        else:
            require(current["revision"] == previous["revision"] + 1, "Changed records must advance by one revision")
            old = {f["path"]: f["sha256"] for f in previous["files"]}
            require(all(f["path"] not in old or old[f["path"]] == f["sha256"] for f in current["files"]), "Preserve prior revision bytes at their recorded paths")
            action = "metadata_only" if import_inputs(current) == import_inputs(previous) else "reimport"
    return {"action": action, "asset_id": current["asset_id"], "revision": current["revision"],
            "unreal_package": current["unreal_package"], "validation": current["validation"]["state"]}


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("record", type=Path)
    parser.add_argument("--previous", type=Path, help="Prior immutable record; all its files are checked too")
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1], help="Repository root for Art/ paths")
    args = parser.parse_args(argv)
    if sys.version_info < (3, 11):
        parser.exit(1, "Python 3.11+ required.\n")
    try:
        current = inspect_record(args.record, args.root)
        previous = inspect_record(args.previous, args.root) if args.previous else None
        print(json.dumps(plan_revision(current, previous), indent=2))
    except (ValueError, TypeError, KeyError) as exc:
        parser.exit(1, f"FAIL asset record: {exc}\n")


if __name__ == "__main__":
    main()
