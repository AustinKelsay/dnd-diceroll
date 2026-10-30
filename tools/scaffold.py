"""Small non-overwriting Unreal class templates for the existing runtime module."""
import re

BASES = {
    "actor": ("A", "AActor", "GameFramework/Actor.h"),
    "component": ("U", "UActorComponent", "Components/ActorComponent.h"),
    "object": ("U", "UObject", "UObject/Object.h"),
}


def new_class(workspace, kind, name):
    if kind not in BASES or not re.fullmatch(r"[A-Z][A-Za-z0-9]{1,63}", name):
        raise ValueError("Use actor/component/object and a PascalCase name without the A/U prefix (2–64 alphanumeric characters)")
    prefix, base, include = BASES[kind]
    module = workspace.settings["module"]
    if not re.fullmatch(r"[A-Za-z][A-Za-z0-9_]*", module):
        raise ValueError("Invalid configured module")
    folder = workspace.root / "Source" / module
    header, source = folder / (name + ".h"), folder / (name + ".cpp")
    # UHT reflected names are module-wide, regardless of directory.
    if any(folder.rglob(name + ".h")) or any(folder.rglob(name + ".cpp")):
        raise ValueError("Class output already exists; refusing to overwrite")
    text = '#pragma once\n\n#include "CoreMinimal.h"\n#include "' + include + '"\n#include "' + name + '.generated.h"\n\nUCLASS(BlueprintType, Blueprintable)\nclass ' + module.upper() + '_API ' + prefix + name + ' : public ' + base + '\n{\n    GENERATED_BODY()\n};\n'
    created = []
    try:
        for path, content in ((header, text), (source, '#include "' + name + '.h"\n')):
            # Exclusive create protects against a race after preflight.
            with path.open("x") as stream:
                created.append(path)
                stream.write(content)
    except Exception:
        for path in created:
            path.unlink(missing_ok=True)
        raise
    return header, source
