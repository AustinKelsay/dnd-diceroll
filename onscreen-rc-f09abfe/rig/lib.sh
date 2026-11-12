source /home/plebdev/.t3/worktrees/dnd-diceroll-t3/t3-2052f717-f2729e40d44e35c5/.local/rc/rig/env.sh
BIN=$(ls /home/plebdev/.t3/worktrees/dnd-diceroll-t3/t3-2052f717-f2729e40d44e35c5/Builds/*/Linux/DnDDiceRoll/Binaries/Linux/DnDDiceRoll | head -1)
PTR=$R/.local/rc/ptr/ptr2
P=$R/.local/rc/run; SHOTS=$P/shots; mkdir -p $SHOTS
getlock() { FLAG=$P/lockflag; rm -f $FLAG; nohup flock -w 7200 /home/plebdev/.t3/ultra-unreal.lock $O/hold.sh $FLAG > $P/hold.log 2>&1 &
  for i in $(seq 1 110); do [ -e $FLAG ] && break; sleep 5; done; [ -e $FLAG ] && echo "LOCK HELD $(date -u +%H:%M:%SZ)" || echo "NOT YET"; }
droplock() { rm -f $P/lockflag; sleep 5; echo "lock released $(date -u +%H:%M:%SZ)"; }
# game NAME W H [extra args]: launch with own user dir and save root (SAVEROOT overrides the save folder); retry on the SDL startup race
game() { local n=$1 w=$2 h=$3; shift 3; local sv=${SAVEROOT:-$P/$n/saves}
  mkdir -p $P/$n/runtime $sv
  for try in 1 2 3; do
    hyprctl keyword monitor HEADLESS-1,${w}x${h}@60,0x0,1 >/dev/null; sleep 1; echo "$w $h" > $O/dims
    setsid "$BIN" -windowed -ResX=$w -ResY=$h -ForceRes -corelimit=2 -UserDir=$P/$n/runtime -ExpeditionSaveRoot=$sv -abslog=$P/$n/game.log -stdout -FullStdOutLogOutput "$@" > $P/$n/stdout.log 2>&1 &
    sleep 18; local pid=$(pgrep -f "abslog=$P/$n/game.log" | tail -1)
    if [ -n "$pid" ] && ps -p $pid >/dev/null; then echo $pid > $P/$n/pid; echo "up $n pid $pid try $try"; return 0; else echo "startup race try $try"; cp $P/$n/game.log $P/$n/game-crash-try$try.log 2>/dev/null; fi
  done; return 1; }
shot() { sleep ${2:-0.5}; grim -o HEADLESS-1 "$SHOTS/$1.png" && echo "shot $1"; }
burst() { local n=$1 c=${2:-6}; for i in $(seq 1 $c); do grim -o HEADLESS-1 -t jpeg -q 80 "$SHOTS/$n-$i.jpg"; done; echo "burst $n x$c"; }
key() { wtype -s 200 "$@"; }
ptr() { read RW RH < $O/dims; $PTR $RW $RH s 1000 "$@"; }
closegame() { local pid=$(cat $P/$1/pid); local a=$(hyprctl clients -j | python3 -c "import json,sys;p=int('$pid');print(([c['address'] for c in json.load(sys.stdin) if c['pid']==p] or [''])[0])"); hyprctl dispatch closewindow address:$a >/dev/null; sleep 8; ps -p $pid >/dev/null && echo STILL-RUNNING || echo closed; echo "errors: $(grep -c 'Error:' $P/$1/game.log)"; }
# wheel N: the pointer must really move for the game to see the wheel (found 2026-11-11), so jitter it
pw() { local x=$((700+RANDOM%500)) y=$((300+RANDOM%400)); ptr m $x $y w "$@"; }
# bgburst NAME COUNT: background grim burst, jpeg q85, no delay between shots
bgburst() { local n=$1 c=${2:-14}; ( for i in $(seq -w 1 $c); do grim -o HEADLESS-1 -t jpeg -q 85 "$SHOTS/$n-$i.jpg"; done ) >/dev/null 2>&1 & }
sheet() { local n=$1; shift; montage "$@" -tile ${TILE:-4x} -geometry ${GEO:-480x270+2+2} -quality 85 $SHOTS/$n.jpg; }
save_state() { python3 - "$1" <<'PY'
import json,sys
c=json.load(open(sys.argv[1]))['Current']; b=c['Battle']; e=b['Encounter']
g=lambda d,*k:next((d[x] for x in k if x in d),None)
print('round',g(b,'round','Round'),'turn',b['TurnOrder'][b['TurnIndex']],'move',b['MovementLeft'],'act',b['bActionAvailable'],'bonus',b['bBonusActionAvailable'])
print(' '.join(f"{g(x,'ID','Id')}:{x['Role'][:4]}{x['HP']}/{x['MaxHP']}L{x['Life']}@{x['Cell']['X']},{x['Cell']['Y']}" for x in e['Combatants']))
PY
}
# kk NAMED...: named keys (Escape, Return, Left, ...) via -k; key stays for single letters
kk() { local a=(); for k in "$@"; do a+=(-k "$k"); done; wtype -s 250 "${a[@]}"; }
# actburst NAME "x,y WxH" COUNT -- wtype args : fire the key after 0.25 s and capture COUNT cropped frames (grim ~18 ms each)
actburst() { local n=$1 g=$2 c=$3; shift 4; ( sleep 0.25; wtype "$@" ) >/dev/null 2>&1 & 
  for i in $(seq -w 1 $c); do grim -g "$g" -t jpeg -q 90 "$SHOTS/$n-$i.jpg"; done; wait; }
# cs NAME FILES... : contact sheet; every Nth frame
cs() { local n=$1 step=$2 cols=$3; shift 3; ls "$@" | awk "NR%$step==1" | xargs montage -tile ${cols}x -geometry +2+2 -quality 88 $SHOTS/$n.jpg; }
# ppmburst NAME "x,y WxH" COUNT KEY : lossless crop burst; fires one wtype key 0.3 s in
ppmburst() { local n=$1 g=$2 c=$3 k=${4:-Return}; mkdir -p $P/bursts/$n; ( sleep 0.3; wtype -s 200 -k $k ) >/dev/null 2>&1 &
  for i in $(seq -w 1 $c); do grim -g "$g" -t ppm $P/bursts/$n/f$i.ppm; done; wait; echo "burst $n $c"; }
D() { python3 $O/drv.py $SAVE "$@"; }
# sheet2 OUT BURSTDIR STEP COLS [W H] [START]: contact sheet of every STEPth ppm frame
sheet2() { local out=$1 d=$2 step=$3 cols=$4 w=${5:-330} h=${6:-225} st=${7:-1}; local f=$(ls $d/*.ppm | awk "NR%$step==$((st%step))" | head -48); montage $f -tile ${cols}x -geometry ${w}x${h}+2+2 -quality 85 $out; }
# scene NAME FIXTURE W H : (re)launch the game on fixtures/FIXTURE (copied to a fresh save root) and press O (Resume)
scene() { local n=$1 fx=$2 w=${3:-1920} h=${4:-1080}; for q in $(ls $P/*/pid 2>/dev/null); do local qn=$(basename $(dirname $q)); ps -p $(cat $q) >/dev/null 2>&1 && closegame $qn; done
  [ -f $P/$n/pid ] && ps -p $(cat $P/$n/pid) >/dev/null 2>&1 && closegame $n
  rm -rf $P/$n/saves; mkdir -p $P/$n/saves; cp $O/fixtures/$fx/current-v1.json $P/$n/saves/
  SAVEROOT=$P/$n/saves game $n $w $h || return 1; wtype -s 300 o; sleep 7; SAVE=$P/$n/saves/current-v1.json; D state; }
