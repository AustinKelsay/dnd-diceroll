"""Build the expedition's bus hierarchy in /Game/DnDDiceRoll/Audio (issue #246).

Run headless with the editor closed (under the Ultra machine lock), after the cue (#242), ambience (#244) and music (#245) imports:
  UnrealEditor-Cmd DnDDiceRoll.uproject -run=pythonscript -script=<repo>/Content/Python/setup_expedition_mix.py -unattended -nullrhi -corelimit=2

  - creates /Game/DnDDiceRoll/Audio/SC_Master, the parent of every bus;
  - parents SC_Effects (#242), SC_Ambience (#244) and SC_Music (#245) under it: each child's parent is SC_Master and SC_Master lists the
    three children, so a volume on Master scales them all (the game pushes one sound mix: Master with its children, then each bus).
Nothing else changes; the cue, bed and music assets keep their classes. Reruns change nothing once the hierarchy is in place (properties
are written only when they differ, and only dirty packages are saved; a failed save raises). The last line is MIX_SETUP_COMPLETE.
"""
import unreal

AUDIO = "/Game/DnDDiceRoll/Audio"
MASTER = AUDIO + "/SC_Master"
BUSES = [AUDIO + "/SC_Effects", AUDIO + "/SC_Ambience", AUDIO + "/SC_Music"]

library = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()


def save(path):
    if not library.save_asset(path, only_if_is_dirty=True):
        raise RuntimeError("Unable to save " + path)


def master_class():
    if library.does_asset_exist(MASTER):
        unreal.log("MIX_ASSET kept " + MASTER)
        return unreal.load_asset(MASTER)
    created = tools.create_asset("SC_Master", AUDIO, unreal.SoundClass, unreal.SoundClassFactory())
    if not created:
        raise RuntimeError("Unable to create " + MASTER)
    save(MASTER)
    unreal.log("MIX_ASSET created " + MASTER)
    return created


def main():
    master = master_class()
    children = []
    for path in BUSES:
        if not library.does_asset_exist(path):
            raise RuntimeError("Missing bus {}; run its import first".format(path))
        bus = unreal.load_asset(path)
        children.append(bus)
        if bus.get_editor_property("parent_class") != master:
            bus.set_editor_property("parent_class", master)
            bus.modify()
            unreal.log("MIX_ASSET parented {} under {}".format(path, MASTER))
        else:
            unreal.log("MIX_ASSET kept {} under {}".format(path, MASTER))
        save(path)
    listed = list(master.get_editor_property("child_classes"))
    if [child.get_path_name() for child in listed if child] != [child.get_path_name() for child in children]:
        master.set_editor_property("child_classes", children)
        master.modify()
        unreal.log("MIX_ASSET listed {} children on {}".format(len(children), MASTER))
    save(MASTER)
    unreal.log("MIX_SETUP_COMPLETE")


main()
