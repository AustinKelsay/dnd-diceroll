"""Register the project status toolset when Unreal's registry is available."""
from unreal_starter.status import registration

registration.register()

# Importing defines an editor-only skill; its instructions stay in repo Markdown.
from unreal_starter import skills
