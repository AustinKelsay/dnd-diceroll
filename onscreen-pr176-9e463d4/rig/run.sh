#!/usr/bin/env bash
R=/home/plebdev/.t3/worktrees/dnd-diceroll-t3/t3-193f04c2-6d221135861cb956; C=$R/.local/pr176/cap
BIN=$R/Builds/20261110T044530393668Z/Linux/DnDDiceRoll/Binaries/Linux/DnDDiceRoll
for fx in board-root-r002 board-root-pillars board-vigil-pillars; do for res in 1920x1080 1280x720; do
 W=${res%x*}; H=${res#*x}; out=$C/out/$fx/$res; save=$C/save/$fx/$res; mkdir -p $out $save; cp $R/Saved/CaptureFixtures/$fx/current-v1.json $save/
 echo "== $fx $res $(date -u +%H:%M:%S)"
 timeout 500 $BIN -game -windowed -ForceRes -ResX=$W -ResY=$H -RenderOffscreen -unattended -nosound -nosplash -UseFixedTimeStep -FPS=30 -corelimit=2 -abslog=$out/game.log -ExpeditionSaveRoot=$save -ExpeditionCapture=$R/tools/capture/poses.json -ExpeditionCaptureOut=$out -ExpeditionCaptureFixture=$fx > $out/console.log 2>&1
 echo "exit $? shots=$(ls $out/*.png 2>/dev/null | wc -l)"
done; done; echo ALLDONE
