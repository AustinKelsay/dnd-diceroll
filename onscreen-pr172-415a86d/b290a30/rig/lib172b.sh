source /home/plebdev/.t3/worktrees/dnd-diceroll-t3/t3-193f04c2-6d221135861cb956/.local/onscreen/lib.sh
R=/home/plebdev/.t3/worktrees/dnd-diceroll-t3/t3-193f04c2-6d221135861cb956
BIN=$R/Builds/20261110T031754837811Z/Linux/DnDDiceRoll/Binaries/Linux/DnDDiceRoll
P=$O/pr172b; SHOTS=$P/shots; mkdir -p $SHOTS
# fx NAME FIXTURE W H [extra args] : fresh save folder seeded with a fixture ('none' = empty), launch, retry once on the SDL startup race; resume with O if a fixture
fx() { local n=$1 f=$2 w=$3 h=$4; shift 4
  rm -rf $P/$n; mkdir -p $P/$n/saves; [ "$f" != none ] && cp $R/Saved/CaptureFixtures/$f/current-v1.json $P/$n/saves/
  export SAVEDIR=$P/$n/saves; O_SAVE=$O; 
  for try in 1 2 3; do
    hyprctl keyword monitor HEADLESS-1,${w}x${h}@60,0x0,1 >/dev/null; sleep 1
    echo "$w $h" > $O/dims
    mkdir -p $P/$n/runtime
    setsid "$BIN" -windowed -ResX=$w -ResY=$h -ForceRes -corelimit=2 -UserDir=$P/$n/runtime -ExpeditionSaveRoot=$P/$n/saves -abslog=$P/$n/game.log -stdout -FullStdOutLogOutput "$@" > $P/$n/stdout.log 2>&1 &
    sleep 18
    local pid=$(pgrep -f "abslog=$P/$n/game.log" | tail -1)
    if [ -n "$pid" ] && ps -p $pid >/dev/null; then echo $pid > $P/$n/pid; echo "up $n pid $pid try $try"; break; else echo "startup race, retry $try"; cp $P/$n/game.log $P/$n/game-crash-try$try.log 2>/dev/null; fi
  done
  [ "$f" != none ] && { key -k o; sleep 4; }
}
pclose() { local pid=$(cat $P/$1/pid); local a=$(hyprctl clients -j | python3 -c "import json,sys;p=int('$pid');print(([c['address'] for c in json.load(sys.stdin) if c['pid']==p] or [''])[0])"); hyprctl dispatch closewindow address:$a; sleep 8; ps -p $pid >/dev/null && echo STILL-RUNNING || echo closed; echo "errors: $(grep -c 'Error:' $P/$1/game.log)"; }
hshot2 () 
{ 
    local n=$1 x=$2 y=$3;
    ptr m $((x-8)) $((y-5)) s 150 m $x $y s 900 sh "grim $SHOTS/$n.png";
    echo hshot $n
}
getlock() { FLAG=$P/lockflag; rm -f $FLAG; nohup flock -w 7200 /home/plebdev/.t3/ultra-unreal.lock $P/hold.sh $FLAG > $P/hold.log 2>&1 &
  for i in $(seq 1 110); do [ -e $FLAG ] && break; sleep 5; done; [ -e $FLAG ] && echo "LOCK HELD $(date -u +%H:%M:%SZ)" || echo "NOT YET"; }
droplock() { rm -f $P/lockflag; sleep 5; echo "lock released $(date -u +%H:%M:%SZ)"; }
BIN143=$R/Builds/20261109T225039957106Z/Linux/DnDDiceRoll/Binaries/Linux/DnDDiceRoll
# fxraw NAME W H [args]: like fx but keeps whatever is already in $P/NAME/saves
fxraw() { local n=$1 w=$2 h=$3; shift 3; export SAVEDIR=$P/$n/saves
  for try in 1 2 3; do hyprctl keyword monitor HEADLESS-1,${w}x${h}@60,0x0,1 >/dev/null; sleep 1; echo "$w $h" > $O/dims; mkdir -p $P/$n/runtime
    setsid "$BIN" -windowed -ResX=$w -ResY=$h -ForceRes -corelimit=2 -UserDir=$P/$n/runtime -ExpeditionSaveRoot=$P/$n/saves -abslog=$P/$n/game.log -stdout -FullStdOutLogOutput "$@" > $P/$n/stdout.log 2>&1 &
    sleep 18; local pid=$(pgrep -f "abslog=$P/$n/game.log" | tail -1)
    if [ -n "$pid" ] && ps -p $pid >/dev/null; then echo $pid > $P/$n/pid; echo "up $n pid $pid try $try"; break; else echo retry; cp $P/$n/game.log $P/$n/game-crash-try$try.log; fi; done; }
# bg_ptr OPS... : start a pointer process that connects now and waits for "go" (touch $P/go) before running OPS
bg_ptr() { rm -f $P/go; read RW RH < $O/dims; (setsid $PTR $RW $RH wf $P/go "$@" >/dev/null 2>&1 < /dev/null &); sleep 2.2; }
go() { touch $P/go; }
