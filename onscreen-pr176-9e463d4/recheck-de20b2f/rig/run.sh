#!/usr/bin/env bash
R=/home/plebdev/.t3/worktrees/dnd-diceroll-t3/t3-193f04c2-6d221135861cb956; C=$R/.local/pr176b/cap
S=$R/$(cat $R/.local/pr175b/staging-build | tr -d '\n')Linux/DnDDiceRoll/Binaries/Linux/DnDDiceRoll
P=$R/$(cat $R/.local/pr175b/b176 | tr -d '\n')Linux/DnDDiceRoll/Binaries/Linux/DnDDiceRoll
run() { # name bin fixture-json poses res
 out=$C/out/$1; save=$C/save/$1; mkdir -p $out $save; cp $3 $save/current-v1.json; W=${5%x*}; H=${5#*x}
 timeout 500 $2 -game -windowed -ForceRes -ResX=$W -ResY=$H -RenderOffscreen -unattended -nosound -nosplash -UseFixedTimeStep -FPS=30 -corelimit=2 -abslog=$out/game.log -ExpeditionSaveRoot=$save -ExpeditionCapture=$4 -ExpeditionCaptureOut=$out -ExpeditionCaptureFixture=$1 > $out/console.log 2>&1; echo "$1 exit $? shots=$(ls $out/*.png 2>/dev/null|wc -l)"; }
for res in 1920x1080 1280x720; do
 run pr176-vigilpillars-$res $P $R/Saved/CaptureFixtures/board-vigil-pillars/current-v1.json $C/poses.json $res
 run stg-vigilpillars-$res $S $R/Saved/CaptureFixtures/board-vigil-pillars/current-v1.json $C/poses.json $res
 run pr176-root-r002-$res $P $R/Saved/CaptureFixtures/board-root-r002/current-v1.json $C/poses.json $res
done
run pr176-lead-silhouettes-1920x1080 $P $R/.local/pr176b/lead/fixture-current-v1.json $R/.local/pr176b/lead/poses.json 1920x1080
run pr176-lead-silhouettes-1280x720 $P $R/.local/pr176b/lead/fixture-current-v1.json $R/.local/pr176b/lead/poses.json 1280x720
echo ALLDONE
