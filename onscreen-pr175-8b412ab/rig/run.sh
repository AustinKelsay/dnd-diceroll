#!/usr/bin/env bash
# run.sh : capture the reduced poses for both packages. Runs under the machine lock.
R=/home/plebdev/.t3/worktrees/dnd-diceroll-t3/t3-193f04c2-6d221135861cb956
C=$R/.local/pr175/cap
declare -A BIN=( [staging]=$R/$(cat $R/.local/pr175/staging-build)Linux/DnDDiceRoll/Binaries/Linux/DnDDiceRoll [pr175]=$R/Builds/20261110T042547958778Z/Linux/DnDDiceRoll/Binaries/Linux/DnDDiceRoll )
for fx in default-seed-battle board-vigil-pillars board-vigil-walls board-18x12; do
 for res in 1920x1080 1280x720; do
  for pkg in staging pr175; do
   W=${res%x*}; H=${res#*x}; out=$C/out/$pkg/$fx/$res; save=$C/save/$pkg/$fx/$res; mkdir -p $out $save; cp $R/Saved/CaptureFixtures/$fx/current-v1.json $save/
   echo "== $pkg $fx $res $(date -u +%H:%M:%S)"
   timeout 400 "${BIN[$pkg]}" -game -windowed -ForceRes -ResX=$W -ResY=$H -RenderOffscreen -unattended -nosound -nosplash -UseFixedTimeStep -FPS=30 -corelimit=2 -abslog=$out/game.log -ExpeditionSaveRoot=$save -ExpeditionCapture=$C/poses.json -ExpeditionCaptureOut=$out -ExpeditionCaptureFixture=$fx > $out/console.log 2>&1
   echo "exit $? shots=$(ls $out/*.png 2>/dev/null | wc -l)"
  done
 done
done
echo ALLDONE $(date -u +%H:%M:%S)
