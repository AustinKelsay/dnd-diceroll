#!/usr/bin/env bash
# usage: perfchain.sh NAME [script]  — separate locked warmup, measured, resume, then the report
cd /home/plebdev/.t3/worktrees/dnd-diceroll-t3/t3-641aa874-26d2aed68b01116a
source .local/onscreen/env.sh
unset DISPLAY; export SDL_VIDEODRIVER=wayland XDG_SESSION_TYPE=wayland
N=$1; S=${2:-}
PK=Builds/20261112T131827785360Z
L="flock -w 7200 /home/plebdev/.t3/ultra-unreal.lock"
mkdir -p .local/perfout/$N
SA=(); [ -n "$S" ] && SA=(--script "$S")
hyprctl keyword monitor HEADLESS-1,1920x1080@60,0x0,1 >/dev/null
$L python3 tools/dev.py --cores 2 perf --package $PK "${SA[@]}" --pass warmup > .local/perfout/$N/warmup.log 2>&1; echo "EXIT $?" >> .local/perfout/$N/warmup.log
RUN=$(grep -o 'PERF_RUN [0-9TZ]*' .local/perfout/$N/warmup.log | awk '{print $2}'); echo $RUN > .local/perfout/$N/run-id.txt
$L python3 tools/dev.py --cores 2 perf --package $PK "${SA[@]}" --pass measured --run $RUN > .local/perfout/$N/measured.log 2>&1; echo "EXIT $?" >> .local/perfout/$N/measured.log
$L python3 tools/dev.py --cores 2 perf --package $PK "${SA[@]}" --pass resume --run $RUN > .local/perfout/$N/resume.log 2>&1; echo "EXIT $?" >> .local/perfout/$N/resume.log
python3 tools/dev.py perf --pass report --run $RUN > .local/perfout/$N/report.log 2>&1; echo "EXIT $?" >> .local/perfout/$N/report.log
