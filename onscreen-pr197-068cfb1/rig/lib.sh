source /home/plebdev/.t3/worktrees/dnd-diceroll-t3/t3-89abfd45-4e2f806269b1eac1/.local/onscreen/env.sh
unset DISPLAY; export SDL_VIDEODRIVER=wayland XDG_SESSION_TYPE=wayland
R=/home/plebdev/.t3/worktrees/dnd-diceroll-t3/t3-89abfd45-4e2f806269b1eac1
BIN=$(cat $O/bin.path)
PTR=$O/ptr/ptr2
P=$O/run; SHOTS=$P/shots; mkdir -p $SHOTS
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
