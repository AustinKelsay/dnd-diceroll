#!/bin/bash
# usage: g.sh label args...
cd /home/plebdev/.t3/worktrees/dnd-diceroll-t3/spike-140-look-dev
label=$1; shift
args=(); for a in "$@"; do args+=(--game-arg="$a"); done
timeout 590 python3 tools/dev.py --cores 2 gallery --fixture board-18x12 --res 1920x1080 --poses ${POSES:-tools/capture/poses-spike.json} "${args[@]}" 2>&1 | tail -2
D=$(ls -d .local/gallery/* | tail -1)
echo "$label $D" >> /tmp/claude-1000/runs.txt
grep -E "Failed to compile|Not enough|SPIKE (material|world)" -A1 $D/board-18x12/1920x1080/game.log | head
