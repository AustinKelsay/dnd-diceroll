"""UE 5.8.4 editor identity and complete dirty-package status."""
import json
import os
from pathlib import Path
import unreal
import toolset_registry
from toolset_registry.registration import Registration


@unreal.uclass()
class StarterWorkspaceTools(unreal.ToolsetDefinition):
    """Identifies this editor process and reports unsaved content/map packages."""

    @toolset_registry.tool_call
    @staticmethod
    def get_status() -> str:
        """Return JSON with canonical project path, PID, build and unsaved packages."""
        content = unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()
        maps = unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
        return json.dumps({
            "project_file": str(Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.get_project_file_path())).resolve()),
            "pid": os.getpid(),
            "engine_version": unreal.SystemLibrary.get_engine_version(),
            "dirty_packages": sorted({p.get_path_name() for p in [*content, *maps]}),
            "pie_active": unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor(),
        })


registration = Registration([StarterWorkspaceTools])
