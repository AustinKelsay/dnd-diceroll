export R=/home/plebdev/.t3/worktrees/dnd-diceroll-t3/t3-1b8cd600-4a7fd846e50f779f; export P=$R/.local/run221; export O=$P/rig215
[ -f $O/sig ] && export HYPRLAND_INSTANCE_SIGNATURE=$(cat $O/sig); [ -f $O/sock ] && export WAYLAND_DISPLAY=$(cat $O/sock); unset DISPLAY; export SDL_VIDEODRIVER=wayland XDG_SESSION_TYPE=wayland
BIN=/home/plebdev/.t3/worktrees/dnd-diceroll-t3/t3-2905ef20-e2a9dadb4b1ad478/Builds/20261112T232931721425Z/Linux/DnDDiceRoll/Binaries/Linux/DnDDiceRoll
PTR=$O/ptr2; SHOTS=$P/shots
# safety: abort unless my instance shows only HEADLESS
safe() { local m=$(hyprctl monitors 2>&1 | grep -E '^Monitor' ); echo "$m"; echo "$m" | grep -q HDMI && { echo ABORT-HDMI; return 1; }; echo "$m" | grep -q 'DP-' && { echo ABORT-DP; return 1; }; echo "$m" | grep -q HEADLESS || { echo ABORT-NOHEADLESS; return 1; }; echo SAFE sig=$HYPRLAND_INSTANCE_SIGNATURE disp=$WAYLAND_DISPLAY; }
game() { local n=$1 w=$2 h=$3; shift 3; local sv=${SAVEROOT:-$P/$n/saves}
  mkdir -p $P/$n/runtime $sv
  for try in 1 2 3; do
    hyprctl keyword monitor HEADLESS-1,${w}x${h}@60,0x0,1 >/dev/null; sleep 1; echo "$w $h" > $O/dims
    setsid "$BIN" -windowed -ResX=$w -ResY=$h -ForceRes -corelimit=2 -UserDir=$P/$n/runtime -ExpeditionSaveRoot=$sv -abslog=$P/$n/game.log -stdout -FullStdOutLogOutput "$@" > $P/$n/stdout.log 2>&1 &
    sleep 18; local pid=$(pgrep -f "abslog=$P/$n/game.log" | tail -1)
    if [ -n "$pid" ] && ps -p $pid >/dev/null; then echo $pid > $P/$n/pid; echo "up $n pid $pid try $try"; return 0; else echo "startup race try $try"; cp $P/$n/game.log $P/$n/game-crash-try$try.log 2>/dev/null; fi
  done; return 1; }
shot() { sleep ${2:-0.5}; grim -o HEADLESS-1 "$SHOTS/$1.png" && echo "shot $1"; }
key() { wtype -s 200 "$@"; }
kk() { local a=(); for k in "$@"; do a+=(-k "$k"); done; wtype -s 250 "${a[@]}"; }
ptr() { read RW RH < $O/dims; $PTR $RW $RH s 300 "$@"; }
closegame() { local pid=$(cat $P/$1/pid); local a=$(hyprctl clients -j | python3 -c "import json,sys;p=int('$pid');print(([c['address'] for c in json.load(sys.stdin) if c['pid']==p] or [''])[0])"); hyprctl dispatch closewindow address:$a >/dev/null; sleep 8; ps -p $pid >/dev/null && echo STILL-RUNNING || echo closed; echo "errors: $(grep -c 'Error:' $P/$1/game.log)"; }
D() { python3 $O/drv.py $SAVE "$@"; }
scene() { local n=$1 fx=$2 w=${3:-1920} h=${4:-1080}; [ -f $P/$n/pid ] && ps -p $(cat $P/$n/pid) >/dev/null 2>&1 && closegame $n
  rm -rf $P/$n/saves; mkdir -p $P/$n/saves; cp $O/fixtures/$fx/current-v1.json $P/$n/saves/
  SAVEROOT=$P/$n/saves game $n $w $h || return 1; wtype -s 300 o; sleep 7; SAVE=$P/$n/saves/current-v1.json; D state; }
# trial LABEL X Y "PTR OPS AFTER PRESS (between bd and bu)" [HOLD_MS]
# wiggle then settle so the game has a hover preview, press, do ops, release; shots + state before/after
trial() { local l=$1 x=$2 y=$3 ops=$4 hold=${5:-200}
  echo "== $l"; echo -n "before: "; D state; grim $SHOTS/$l-a-before.png
  ptr m $((x+15)) $((y+10)) m $x $y s 700 bd 272 s 150 $ops s $hold bu 272 s 1800
  grim $SHOTS/$l-b-after.png; echo -n "after:  "; D state; }
