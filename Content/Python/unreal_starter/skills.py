"""Expose versioned project guidance through Epic's editor skill registry."""
from pathlib import Path

import unreal
from toolset_registry.agent_skill import agent_skill


INSTRUCTIONS_PATH = Path(__file__).resolve().parents[3] / "docs/agents/editor-authoring.md"


@agent_skill
class ProjectAuthoringSkill(unreal.AgentSkill):
    """Project conventions for asset authoring and presentation/input verification.
    Read when adding or changing project assets or validating visible game behavior."""

    instructions = INSTRUCTIONS_PATH.read_text(encoding="utf-8")
