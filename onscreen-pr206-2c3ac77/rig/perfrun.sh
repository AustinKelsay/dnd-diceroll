#!/usr/bin/env bash
# perfrun.sh NAME SCRIPT_OR_DEFAULT PASS [RUN]: one locked perf pass in my nested compositor
W=/home/plebdev/.t3/worktrees/dnd-diceroll-t3/t3-ca6d63de-1439986ddd5d03a0
source $W/.local/onscreen/env.sh
unset DISPLAY; export SDL_VIDEODRIVER=wayland XDG_SESSION_TYPE=wayland
hyprctl keyword monitor HEADLESS-1,1920x1080@60,0x0,1 >/dev/null
cd $W
S=(); [ "$2" != default ] && S=(--script $2)
R=(); [ -n "${4:-}" ] && R=(--run $4)
flock -w 7200 /home/plebdev/.t3/ultra-unreal.lock python3 tools/dev.py --cores 2 perf --package Builds/20261112T131020135518Z "${S[@]}" --pass $3 "${R[@]}"
