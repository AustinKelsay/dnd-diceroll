#!/usr/bin/env bash
# Locked job body: for each variant, swap the asset in, inspect in a fresh headless UnrealEditor-Cmd, restore. Never leaves the tracked asset changed.
set -u
W=<worktree>
cd "$W"
P=Content/DnDDiceRoll/Theme/Materials/M_DiceRollTheme.uasset
cp "$P" .local/assets/_orig.uasset
restore() { cp .local/assets/_orig.uasset "$P"; }
trap restore EXIT
mkdir -p .local/inspect
for v in head staging gate193; do
  cp .local/assets/$v.uasset "$P"
  INSPECT_OUT="$W/.local/inspect/$v.json" ~/Applications/UnrealEngine-5.8.4/Engine/Binaries/Linux/UnrealEditor-Cmd "$W/DnDDiceRoll.uproject" -run=pythonscript -script="$W/.local/tools/inspect_theme.py" -abslog="$W/.local/inspect/$v.log" -unattended -nosplash -RenderOffscreen -corelimit=2 > .local/inspect/$v.console.log 2>&1
  echo "$v exit=$?"
done
