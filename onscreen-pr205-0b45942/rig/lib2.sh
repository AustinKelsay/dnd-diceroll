source /home/plebdev/.t3/worktrees/dnd-diceroll-t3/t3-641aa874-26d2aed68b01116a/.local/onscreen/lib.sh
# fxscene NAME FIXTURE_JSON_PATH W H [LOOKS|-]: fresh save root with the checkpoint, launch (retrying the SDL race), press O (Resume), wait.
fxscene() { local n=$1 src=$2 w=${3:-1920} h=${4:-1080} looks=${5:--}
  for q in $(ls $P/*/pid 2>/dev/null); do local qn=$(basename $(dirname $q)); ps -p $(cat $q) >/dev/null 2>&1 && closegame $qn; done
  rm -rf $P/$n; mkdir -p $P/$n/saves; cp $src $P/$n/saves/current-v1.json
  local extra=(); [ "$looks" != "-" ] && extra=("-ExpeditionFoeLooks=$looks")
  SAVEROOT=$P/$n/saves game $n $w $h "${extra[@]}" || return 1
  wtype -s 300 o; sleep ${SETTLE:-7}; SAVE=$P/$n/saves/current-v1.json; export SAVE; python3 $O/drv.py $SAVE state 2>/dev/null | cut -c1-300; }
# snap OUT.jpg : full-screen lossy-high snapshot
snap() { grim -o HEADLESS-1 -t jpeg -q 93 "$SHOTS/$1.jpg" && echo "snap $1"; }
looksof() { python3 -c "import json;print(json.load(open('$R/Saved/CaptureFixtures/$1/fixture.json'))['foeLooks'])"; }
