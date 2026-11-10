#!/usr/bin/env bash
R=/home/plebdev/.t3/worktrees/dnd-diceroll-t3/t3-193f04c2-6d221135861cb956; C=$R/.local/pr175/cap
BIN=$R/$(cat $R/.local/pr175/staging-build)Linux/DnDDiceRoll/Binaries/Linux/DnDDiceRoll
for fx in board-vigil-pillars default-seed-battle; do out=$C/out/staging2/$fx/1920x1080; save=$C/save/staging2/$fx; mkdir -p $out $save; cp $R/Saved/CaptureFixtures/$fx/current-v1.json $save/
timeout 400 $BIN -game -windowed -ForceRes -ResX=1920 -ResY=1080 -RenderOffscreen -unattended -nosound -nosplash -UseFixedTimeStep -FPS=30 -corelimit=2 -abslog=$out/game.log -ExpeditionSaveRoot=$save -ExpeditionCapture=$C/poses.json -ExpeditionCaptureOut=$out -ExpeditionCaptureFixture=$fx > $out/console.log 2>&1; echo "$fx exit $?"; done
