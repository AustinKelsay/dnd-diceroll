"""Read back the project skill in a headless editor; never create/save assets."""
import hashlib
import json
import os
from pathlib import Path
import sys

import unreal


skill_module = sys.modules.get("unreal_starter.skills")
if skill_module is None:
    raise RuntimeError("Project skill was not imported by editor startup")
INSTRUCTIONS_PATH = skill_module.INSTRUCTIONS_PATH
ProjectAuthoringSkill = skill_module.ProjectAuthoringSkill
root = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).resolve()
if INSTRUCTIONS_PATH != root / "docs/agents/editor-authoring.md":
    raise RuntimeError("Skill source belongs to another checkout")
path = ProjectAuthoringSkill.static_class().get_path_name()
# AICallable functions lack ordinary Python glue in the pinned engine.
toolset = unreal.get_default_object(unreal.AgentSkillToolset)
summaries = toolset.call_method("ListSkills")
if path not in summaries or not summaries[path].strip():
    raise RuntimeError("Project skill missing from ListSkills")
details = toolset.call_method("GetSkills", args=([path],))
instructions = INSTRUCTIONS_PATH.read_text(encoding="utf-8")
if path not in details or details[path].instructions != instructions:
    raise RuntimeError("GetSkills did not return the current Markdown")
report = {
    "project_file": str(Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.get_project_file_path())).resolve()),
    "pid": os.getpid(),
    "engine_version": unreal.SystemLibrary.get_engine_version(),
    "skill_path": path,
    "description": summaries[path],
    "instructions_sha256": hashlib.sha256(instructions.encode("utf-8")).hexdigest(),
}
print("PROJECT_SKILL_VERIFIED " + json.dumps(report, sort_keys=True))
